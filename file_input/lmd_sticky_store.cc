
#include "lmd_sticky_store.hh"
#include "lmd_output.hh"
#include "error.hh"

#include <string.h>

/* In order to deliver a full set of sticky subevents on request, we
 * need to keep track of them.  Since however the usual delivery is
 * incrementally, as they occur, this additional book-keeping is just
 * done to satisfy very spurious requests.  It should therefore be of
 * low cost (and preferably take the hit when the request comes.
 *
 * We store the sticky events as they arrive in a plain data array.
 * New sticky events are added at the end.  In order to keep track of
 * the contents of the array, we keep an additional array with just
 * the meta-data, i.e. identifiers, lengths and offsets of the events
 * and subevents.  Thus it is easy to in that metadata remove (by
 * marking, not moving) entries as they are superceeded.  Further, to
 * easily find entries in the array, we also keep a hash-table of all
 * identifiers, with offsets to the last (i.e. active) entry.
 *
 * Thus insertation is cheap, the main cost is a single copy of the
 * data into the big array.  The meta-data update is constant time.
 *
 * When we need to request the contents, it is also linear in size, as
 * it only has to traverse the meta-data array and copy out any active
 * items.
 * 
 * To avoid this store to eat infinite amounts of memory, we keep
 * track of the amount of discarded data.  When a reallocation is
 * necessary, it is first checked if there is more than half empty
 * data.  If that is the case, we compact the array instead of
 * allocating more memory.  This will of course take time proportional
 * to the array size, but as it at most happens when we reach the end
 * of the array, its occurance goes inversely proportional to the
 * array size, and thus overall scales with the copy cost.
 *
 */

#define DEBUG_LMD_STICKY_STORE        1
#define DEBUG_LMD_STICKY_STORE_PRINT  0

lmd_sticky_store::lmd_sticky_store()
{
  _data = NULL;
  _data_end = _data_alloc = 0; // bytes
  _data_revoked = 0;

  _meta = NULL;
  _meta_end = _meta_alloc = 0; // bytes
  _meta_revoked = 0;

  _hash = NULL;
  _hash_used = _hash_size = 0; // items
}

lmd_sticky_store::~lmd_sticky_store()
{
  free(_data);
  free(_meta);
  free(_hash);
}

uint32_t sticky_subevent_hash(const lmd_subevent_10_1_host *subevent_header)
{
  const uint32_t *p = (const uint32_t *) subevent_header;

  uint32_t d1 = p[1];
  uint32_t d2 = p[2];

  uint32_t hash = d1;

  hash ^= (hash << 6);
  hash ^= (hash << 21);
  hash ^= (hash << 7);

  hash ^= d2;

  hash ^= (hash << 6);
  hash ^= (hash << 21);
  hash ^= (hash << 7);

  return hash;
}

bool sticky_subevent_same_id(const lmd_subevent_10_1_host *header_a,
			     const lmd_subevent_10_1_host *header_b)
{
  const uint32_t *p_a = (const uint32_t *) header_a;
  const uint32_t *p_b = (const uint32_t *) header_b;

  if (p_a[1] == p_b[1] &&
      p_a[2] == p_b[2])
    return true;
  return false;
}

void lmd_sticky_store::insert(const lmd_event_out *event,
			      bool discard_revoke)
{
#if DEBUG_LMD_STICKY_STORE
  verify_meta();
#endif

  // How much additional does this event contribute (worst case,
  // i.e. without considering any stuff that it revokes)

  // The event that we get to handle comes via lmd_event_out, which
  // means it is broken down into chunks, and subevents headers etc
  // not are clearly marked any long.  (It may even be malformed, if
  // the user has copied badly...)

  // The easiest approach is that we copy the entire data (which we
  // anyhow would do) into the data buffer, and then from there sort
  // out the subevents again.

  size_t ev_data_len = event->get_length();
  size_t ev_len = ev_data_len + sizeof(lmd_event_header_host);

  if (_data_end + ev_len > _data_alloc)
    {
      // Is compactification due?

      if (_data_revoked > _data_end / 2)
	compact_data();

      // Problem may still not be solved...

      size_t sz = _data_alloc;

      if (sz < 1024)
	sz = 1024;

      while (_data_end + ev_len > sz)
	sz *= 2;

      if (sz > _data_alloc)
	{
	  _data = (char *) realloc (_data, sz);
	  if (!_data)
	    ERROR("Memory allocation failure.");
	  
	  _data_alloc = sz;
	}
    } 

  // Now that we have the needed data array secured, copy the event

  size_t ev_data_off = _data_end;
  _data_end += ev_len;
  char *raw_ev = _data + ev_data_off;
  
  event->write(raw_ev);

#if DEBUG_LMD_STICKY_STORE
# if DEBUG_LMD_STICKY_STORE_PRINT
  for (size_t i = 0; i < ev_len; i += sizeof (uint32_t))
    fprintf (stderr, "%3zx: %08x\n", i, *((uint32_t *) (raw_ev+i)));
# endif
#endif

  // And figure out how many subevents it contains

  // Some minor checks of the event header.  That the size is correct,
  // and that it has the right type/subtype.

  lmd_event_10_1_host *ev_header = (lmd_event_10_1_host *) raw_ev;

  size_t ev_size =
    EVENT_DATA_LENGTH_FROM_DLEN((size_t) ev_header->_header.l_dlen);

  if (ev_size != ev_data_len)
    ERROR("Trying to store sticky event with wrong data size "
	  "(in header data %zd != total data %zd).",
	  ev_size, ev_len);

  if (ev_size + sizeof(lmd_event_header_host) < sizeof (lmd_event_10_1_host))
    ERROR("Trying to store sticky event with small size "
	  "(%zd < 1 header).",
	  ev_size + sizeof(lmd_event_header_host));

  if (!(ev_header->_header.i_type    == LMD_EVENT_STICKY_TYPE &&
	ev_header->_header.i_subtype == LMD_EVENT_STICKY_SUBTYPE))
    ERROR("Trying to store sticky event with wrong type (%d=0x%04x/%d=0x%04x)",
	  (uint16_t) ev_header->_header.i_type,
	  (uint16_t) ev_header->_header.i_type,
	  (uint16_t) ev_header->_header.i_subtype,
	  (uint16_t) ev_header->_header.i_subtype);

  size_t ev_raw_data_len = sizeof (lmd_event_10_1_host);

  // Now count the subevents.

  uint32_t num_sev = 0;

  for (size_t sev_data_off = ev_raw_data_len; sev_data_off != ev_len; )
    {
      if (sev_data_off + sizeof (lmd_subevent_10_1_host) > ev_len)
	ERROR("Trying to store sticky event with "
	      "subevent header overflowing event (%zd bytes).",
	      sev_data_off + sizeof (lmd_subevent_10_1_host) -
	      ev_len);
      
      lmd_subevent_10_1_host *sev_header =
	(lmd_subevent_10_1_host *) (raw_ev + sev_data_off);

      size_t sev_payload_size;
      if (sev_header->_header.l_dlen == LMD_SUBEVENT_STICKY_DLEN_REVOKE)
	sev_payload_size = 0;
      else
	sev_payload_size =
	  SUBEVENT_DATA_LENGTH_FROM_DLEN((size_t) sev_header->_header.l_dlen);
      size_t sev_next_off =
	sev_data_off + (sizeof (lmd_subevent_10_1_host) +
			sev_payload_size);

      if (sev_next_off > ev_len)
	{
	  assert (false);
	ERROR("Trying to store sticky event with "
	      "subevent payload overflowing event (%zd bytes).",
	      sev_next_off - ev_len);
	}

      if (sev_header->_header.i_type    == LMD_SUBEVENT_STICKY_TSTAMP_TYPE &&
	  sev_header->_header.i_subtype == LMD_SUBEVENT_STICKY_TSTAMP_SUBTYPE)
	{
	  if (num_sev)
	    ERROR("Trying to store sticky event with "
		  "tstamp subevent (%d/%d) as non-fist subevent.",
		  sev_header->_header.i_type,
		  sev_header->_header.i_subtype);
	  else
	    {
	      // We (locally) account this together with the event
	      // header, such that this subevent is kept whatever happens
	      // to the other subevents.
	      ev_raw_data_len +=
		(sizeof (lmd_subevent_10_1_host) + sev_payload_size);
	    }
	}
      else
	num_sev++;

      sev_data_off = sev_next_off;
    }
  
  // Reallocations?

  size_t meta_add =
    sizeof (lmd_sticky_meta_event) +
    num_sev * sizeof (lmd_sticky_meta_subevent);
  size_t hash_add = num_sev;

  bool recalc_hash = false;

  if (_meta_end + meta_add > _meta_alloc)
    {
      // Is compactification due?

      if (_meta_revoked > _meta_end / 2)
	{
	  compact_meta();
	  // Since the offsets change, we just recalculate the hash table
	  recalc_hash = true;
	}

      // Problem may still not be solved...

      size_t sz = _meta_alloc;

      if (sz < 256)
	sz = 256;

      while (_meta_end + meta_add > sz)
	sz *= 2;

      if (sz > _meta_alloc || sz < _meta_alloc / 2)
	{
	  _meta = (char *) realloc (_meta, sz);
	  if (!_meta)
	    ERROR("Memory allocation failure.");
	  
	  _meta_alloc = sz;
	}
    }

  if (_hash_used + hash_add > _hash_size / 2)
    {
      size_t sz = _hash_size;

      if (sz < 16)
	sz = 16;

      while (_hash_used + hash_add > sz / 2)
	sz *= 2;

      if (sz > _hash_size ||
	  sz < _hash_size / 4) // if needs have decreased dramatically...
	{
	  _hash = (lmd_sticky_hash_subevent *)
	    realloc (_hash, sz * sizeof (_hash[0]));
	  if (!_hash)
	    ERROR("Memory allocation failure.");
	  
	  _hash_size = sz;

	  // We simply repopulate the hash table
	  recalc_hash = true;
	}
    }

  if (recalc_hash)
    populate_hash();

  // Ok, so now we also have space to store the meta-data about this event...

  size_t dest_ev_offset = _meta_end;
  
  lmd_sticky_meta_event *ev =
    (lmd_sticky_meta_event *) (_meta + _meta_end);
  _meta_end += sizeof (lmd_sticky_meta_event);

  ev->_data_offset = ev_data_off;
  ev->_data_length = ev_raw_data_len;
  ev->_num_sub  = num_sev;
  ev->_live_sub = num_sev;
  
  for (size_t sev_data_off = ev_raw_data_len; sev_data_off != ev_len; )
    {
      lmd_subevent_10_1_host *sev_header =
	(lmd_subevent_10_1_host *) (raw_ev + sev_data_off);

      size_t sev_payload_size;
      if (sev_header->_header.l_dlen == LMD_SUBEVENT_STICKY_DLEN_REVOKE)
	sev_payload_size = 0;
      else
	sev_payload_size =
	  SUBEVENT_DATA_LENGTH_FROM_DLEN((size_t) sev_header->_header.l_dlen);
      size_t sev_next_off =
	sev_data_off + (sizeof (lmd_subevent_10_1_host) +
			sev_payload_size);

# if DEBUG_LMD_STICKY_STORE_PRINT
      fprintf(stderr, "Consider sev (l_dlen=%d).\n",
	      sev_header->_header.l_dlen);
# endif

	  size_t dest_sev_offset = _meta_end;

      if (!discard_revoke ||
	  sev_header->_header.l_dlen != LMD_SUBEVENT_STICKY_DLEN_REVOKE)
	{      
	  // Insert the subevent metadata

	  lmd_sticky_meta_subevent *sev =
	    (lmd_sticky_meta_subevent *) (_meta + _meta_end);
	  _meta_end += sizeof (lmd_sticky_meta_subevent);

	  sev->_ev_offset = dest_ev_offset;
	  sev->_data_offset = ev_data_off + sev_data_off;
	  sev->_data_length = (sizeof (lmd_subevent_10_1_host) +
			       sev_payload_size);
	}
      else
	{
	  ev->_num_sub--;
	  ev->_live_sub--;
	}

      // And into the hash table Perhaps it is known before?  Note: We
      // cannot remove revoke events.  Since our purpose is to be used
      // for replays, also the revoke events are important!

      // For file replay, we do not want the revoke events however.

      uint32_t hash = sticky_subevent_hash(sev_header);

      size_t entry = hash & (_hash_size - 1);

      for ( ; _hash[entry]._sub_offset;
	    entry = (entry + 1) & (_hash_size - 1))
	{
	  if (_hash[entry]._sub_offset == -1)
	    continue; // discarded

	  // Investigate if this is the same id as ours

	  lmd_sticky_meta_subevent *check_sev =
	    (lmd_sticky_meta_subevent *) (_meta + _hash[entry]._sub_offset);

	  if (check_sev->_data_offset == -1)
	    continue; // discarded

	  char *check_raw = _data + check_sev->_data_offset;

	  lmd_subevent_10_1_host *check_sev_header =
	    (lmd_subevent_10_1_host *) check_raw;	  

	  if (sticky_subevent_same_id(sev_header,
				      check_sev_header))
	    {
	      // Remove the subevent from the event it belongs to.
	      // Actual removal happens later.  Just decrease the
	      // number of live subevents.

	      check_sev->_data_offset = -1;

	      lmd_sticky_meta_event *check_ev =
		(lmd_sticky_meta_event *) (_meta + check_sev->_ev_offset);

	      // When data (or meta) gets compacted, this subevent
	      // will not be retained
	      _data_revoked += check_sev->_data_length;
	      _meta_revoked += sizeof (lmd_sticky_meta_subevent);
	      check_ev->_live_sub--;
	      if (check_ev->_live_sub == 0)
		{
		  // Upon compacting, this event will not be retained.
		  _data_revoked += check_ev->_data_length;
		  _meta_revoked += sizeof (lmd_sticky_meta_event);
		}
	      // May be moved into earlier discardred slot, so kill this one.
	      _hash[entry]._sub_offset = -1;
	      goto hash_found;
	    }
	}
      _hash_used++;
    hash_found:
      if (!discard_revoke ||
	  sev_header->_header.l_dlen != LMD_SUBEVENT_STICKY_DLEN_REVOKE)
	{
	  // Since we may have discarded slots, do the search again
	  entry = hash & (_hash_size - 1);
	  while (_hash[entry]._sub_offset &&
		 _hash[entry]._sub_offset != -1)
	    {
	      entry = (entry + 1) & (_hash_size - 1);
	    }
	  // We have found either an empty slot, or we will overwrite
	  // the previous.
	  _hash[entry]._sub_offset = dest_sev_offset;
#if DEBUG_LMD_STICKY_STORE
# if DEBUG_LMD_STICKY_STORE_PRINT
	  fprintf(stderr, "Inserted sev at hash %zd.\n", entry);
# endif
#endif
	}
      sev_data_off = sev_next_off;
    }

#if DEBUG_LMD_STICKY_STORE
  verify_meta();
#endif
}

void lmd_sticky_store::compact_data()
{
#if DEBUG_LMD_STICKY_STORE
  verify_meta();
# if DEBUG_LMD_STICKY_STORE_PRINT
  fprintf(stderr,"Compact data!\n");
# endif
#endif
  // Go trough the meta-data, and move all event data up ahead.

  size_t dest_offset = 0;
  size_t iter_ev_offset;

  for (iter_ev_offset = 0; iter_ev_offset < _meta_end; )
    {
      lmd_sticky_meta_event *ev =
	(lmd_sticky_meta_event *) (_meta + iter_ev_offset);
      iter_ev_offset +=
	sizeof (lmd_sticky_meta_event) +
	ev->_num_sub * sizeof (lmd_sticky_meta_subevent);

      if (!ev->_live_sub)
	{
	  // There are no live subevents left in this event.
	  // Forget about its data.
	  continue;
	}

      char *src_ptr = _data + ev->_data_offset;
      char *dest_ptr = _data + (ev->_data_offset = dest_offset);
      dest_offset += ev->_data_length;

      // Move (if needed) the event to its new position
      assert (dest_ptr <= src_ptr);
      memmove(dest_ptr, src_ptr, ev->_data_length);

      // Get pointer to the subevents
      lmd_sticky_meta_subevent *sev =
	(lmd_sticky_meta_subevent *) (ev + 1);

      for (uint32_t i = 0; i < ev->_num_sub; i++, sev++)
	{
	  if (sev->_data_offset == -1)
	    continue;

	  char *src_ptr = _data + sev->_data_offset;
	  char *dest_ptr = _data + (sev->_data_offset = dest_offset);
	  dest_offset += sev->_data_length;

	  // Move (if needed) to its new position
	  assert (dest_ptr <= src_ptr);
	  memmove(dest_ptr, src_ptr, sev->_data_length);
	}
    }
  assert(iter_ev_offset == _meta_end);
  // New end of meta-data:
  _data_end = dest_offset;
  _data_revoked = 0;

#if DEBUG_LMD_STICKY_STORE
  verify_meta();
#endif
}

void lmd_sticky_store::compact_meta()
{
#if DEBUG_LMD_STICKY_STORE
  verify_meta();
# if DEBUG_LMD_STICKY_STORE_PRINT
  fprintf(stderr,"Compact meta!\n");
# endif
#endif
  // Go trough the meta-data, and squeeze out all subevents that have
  // vanished.

  size_t dest_offset = 0;
  size_t iter_ev_offset;

  for (iter_ev_offset = 0; iter_ev_offset < _meta_end; )
    {
      lmd_sticky_meta_event *ev =
	(lmd_sticky_meta_event *) (_meta + iter_ev_offset);
      iter_ev_offset +=
	sizeof (lmd_sticky_meta_event) +
	ev->_num_sub * sizeof (lmd_sticky_meta_subevent);

      if (!ev->_live_sub)
	{
	  // There are no live subevents left in this event.
	  // Completely forget about it.
	  continue;
	}

      // Get pointer to the subevents (source)
      lmd_sticky_meta_subevent *sev =
	(lmd_sticky_meta_subevent *) (ev + 1);

      size_t dest_ev_offset = dest_offset;
      lmd_sticky_meta_event *dev =
	(lmd_sticky_meta_event *) (_meta + dest_offset);
      dest_offset += sizeof (lmd_sticky_meta_event);

      // Move (if needed) the event to its new position
      assert (dev <= ev);
      memmove(dev, ev, sizeof (lmd_sticky_meta_event));
      ev = dev;

      for (uint32_t i = 0; i < ev->_num_sub; i++, sev++)
	{
	  if (sev->_data_offset == -1)
	    continue;

	  // Subevent is still live.  Calculate its new position.
	  lmd_sticky_meta_subevent *dsev =
	    (lmd_sticky_meta_subevent *) (_meta + dest_offset);
	  dest_offset += sizeof (lmd_sticky_meta_subevent);

	  // Move (if needed) to its new position
	  assert (dsev <= sev);
	  memmove(dsev, sev, sizeof (lmd_sticky_meta_event));
	  // Update the pointer to its event.
	  dsev->_ev_offset = dest_ev_offset;
	}
      ev->_num_sub = ev->_live_sub;
    }
  assert(iter_ev_offset == _meta_end);
  // New end of meta-data:
  _meta_end = dest_offset;
  _meta_revoked = 0;
#if DEBUG_LMD_STICKY_STORE
  verify_meta();
#endif
}

void lmd_sticky_store::populate_hash()
{
#if DEBUG_LMD_STICKY_STORE
# if DEBUG_LMD_STICKY_STORE_PRINT
  fprintf(stderr,"Populating hash!\n");
# endif
#endif

  // Initialize all items empty

  _hash_used = 0;
  for (size_t i = 0; i < _hash_size; i++)
    _hash[i]._sub_offset = 0;
  
  // Go trough the meta-data, and reconstruct the hash table
  size_t iter_ev_offset;

  for (iter_ev_offset = 0; iter_ev_offset < _meta_end; )
    {
      lmd_sticky_meta_event *ev =
	(lmd_sticky_meta_event *) (_meta + iter_ev_offset);
      iter_ev_offset +=
	sizeof (lmd_sticky_meta_event) +
	ev->_num_sub * sizeof (lmd_sticky_meta_subevent);

      if (!ev->_live_sub)
	continue;

      // Get pointer to the subevents
      lmd_sticky_meta_subevent *sev =
	(lmd_sticky_meta_subevent *) (ev + 1);

      for (uint32_t i = 0; i < ev->_num_sub; i++, sev++)
	{
	  if (sev->_data_offset == -1)
	    continue;

	  // Subevent is live, insert into hash table
	  // insert_hash((char *) sev - _meta);
	  insert_hash(sev);
	}
    }
  assert(iter_ev_offset == _meta_end);
#if DEBUG_LMD_STICKY_STORE
  verify_meta();
#endif
}

void lmd_sticky_store::insert_hash(lmd_sticky_meta_subevent *sev)
{
#if DEBUG_LMD_STICKY_STORE
# if DEBUG_LMD_STICKY_STORE_PRINT
  fprintf(stderr,"Insert hash!\n");
# endif
#endif

  // can never be 0, as an event always is before the subevent
  ssize_t offset_sev = (char *) sev - _meta;

  char *sev_raw = _data + sev->_data_offset;

  lmd_subevent_10_1_host *subevent_header =
    (lmd_subevent_10_1_host *) sev_raw;

  uint32_t hash = sticky_subevent_hash(subevent_header);

  size_t entry = hash & (_hash_size - 1);

  while (_hash[entry]._sub_offset &&
	 _hash[entry]._sub_offset != -1) // we can insert at discarded position
    {
      entry = (entry + 1) & (_hash_size - 1);
    }
  _hash[entry]._sub_offset = offset_sev;
  _hash_used++;
}

#if DEBUG_LMD_STICKY_STORE
void lmd_sticky_store::verify_meta()
{
  // This function is used to check the integrity of the meta-data
  // structures

  size_t iter_ev_offset;

# if DEBUG_LMD_STICKY_STORE_PRINT
  fprintf (stderr,"--- DATA: [%05zx/%05zx] r:%05zx ---\n",
	   _data_end, _data_alloc, _data_revoked);
  fprintf (stderr,"--- META: [%05zx/%05zx] r:%05zx ---\n",
	   _meta_end, _meta_alloc, _meta_revoked);
# endif
  for (iter_ev_offset = 0; iter_ev_offset < _meta_end; )
    {
      assert (iter_ev_offset + sizeof (lmd_sticky_meta_event) <= _meta_end);

      size_t this_ev_offset = iter_ev_offset;
      lmd_sticky_meta_event *ev =
	(lmd_sticky_meta_event *) (_meta + iter_ev_offset);
# if DEBUG_LMD_STICKY_STORE_PRINT
      fprintf (stderr,
	       "@%04zx  EV   d@=%05zx:%02zx  live:%d/%d\n",
	       iter_ev_offset,
	       ev->_data_offset, ev->_data_length,
	       ev->_live_sub, ev->_num_sub);
# endif
      iter_ev_offset +=
	sizeof (lmd_sticky_meta_event) +
	ev->_num_sub * sizeof (lmd_sticky_meta_subevent);
      assert(iter_ev_offset <= _meta_end);

      assert(!ev->_live_sub ||
	     ev->_data_offset + ev->_data_length <= _data_end);

      uint32_t live_sub = 0;
      
      // Get pointer to the subevents
      lmd_sticky_meta_subevent *sev =
	(lmd_sticky_meta_subevent *) (ev + 1);

      for (uint32_t i = 0; i < ev->_num_sub; i++, sev++)
	{
	  assert((char *) (sev+1) <= _meta + _meta_end);
	  
# if DEBUG_LMD_STICKY_STORE_PRINT
	  fprintf (stderr,
		   " @%04zx  SEV d@=%05zx:%02zx  (EV@%04zx)\n",
		   ((char *) sev) - _meta,
		   sev->_data_offset, sev->_data_length,
		   sev->_ev_offset);
# endif
	  
	  if (sev->_data_offset == -1)
	    continue;

	  live_sub++;

	  assert(sev->_ev_offset == this_ev_offset);
	  assert(sev->_data_offset > 0);
	  assert(sev->_data_offset + sev->_data_length <= _data_end);
	}
      assert(live_sub == ev->_live_sub);
    }
  assert(iter_ev_offset == _meta_end);
  
# if DEBUG_LMD_STICKY_STORE_PRINT
  fprintf (stderr,"--- HASH: [%zx/%zx] ---\n",
	   _hash_used,_hash_size);
  for (size_t i = 0; i < _hash_size; i++)
    {
      if (_hash[i]._sub_offset &&
	  _hash[i]._sub_offset != -1)
	{
	  fprintf (stderr,
		   " [%04zx] SEV@%04zx\n",
		   i,
		   _hash[i]._sub_offset);	  
	}
    }
  fprintf (stderr,"-------------\n");
# endif
}
#endif

void lmd_sticky_store::write_events(lmd_output_buffered *dest)
{
#if DEBUG_LMD_STICKY_STORE
  verify_meta();
# if DEBUG_LMD_STICKY_STORE_PRINT
  fprintf(stderr,"Write events!\n");
# endif
#endif

  // Go trough the meta-data, and prepare all non-empty events
  // for writing, and send the write call.

  // TODO: when writing for a file, we should not eject sticky
  // 'revoke' events.  The file is new, and does not need any
  // revoking.

  lmd_event_out event;

  size_t iter_ev_offset;

  for (iter_ev_offset = 0; iter_ev_offset < _meta_end; )
    {
      lmd_sticky_meta_event *ev =
	(lmd_sticky_meta_event *) (_meta + iter_ev_offset);
      iter_ev_offset +=
	sizeof (lmd_sticky_meta_event) +
	ev->_num_sub * sizeof (lmd_sticky_meta_subevent);

      if (!ev->_live_sub)
	{
	  // There are no live subevents left in this event.
	  continue;
	}

      event.clear();

      const char *src_ptr = _data + ev->_data_offset;
      const lmd_event_10_1_host *header_ptr =
	(lmd_event_10_1_host *) src_ptr;

      // The first part of the header
      event._header = header_ptr->_header;
      // The second part of the header
      event._info = header_ptr->_info;
      // Which is handled as a chunk
      event.add_chunk(&event._info, sizeof (event._info), false/*native*/);

      assert(ev->_data_length >= sizeof (lmd_event_10_1_host));
      const char *more_ptr = src_ptr + sizeof (lmd_event_10_1_host);
      size_t more_length = ev->_data_length - sizeof (lmd_event_10_1_host);
      // Any additional data
      if (more_length)
	event.add_chunk(more_ptr, more_length, false/*native*/);
      /*
      printf ("%zd = %zd - %zd\n",
	      more_length,ev->_data_length,sizeof (lmd_event_10_1_host));
      */
      // Get pointer to the subevents
      lmd_sticky_meta_subevent *sev =
	(lmd_sticky_meta_subevent *) (ev + 1);

      for (uint32_t i = 0; i < ev->_num_sub; i++, sev++)
	{
	  if (sev->_data_offset == -1)
	    continue;

	  const char *src_ptr = _data + sev->_data_offset;

	  event.add_chunk(src_ptr, sev->_data_length, false/*native*/);
	}

      // event.dump_debug();
      dest->write_event(&event, true);
    }
  assert(iter_ev_offset == _meta_end);
}
