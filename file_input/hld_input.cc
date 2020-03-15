/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include "hld_input.hh"

#include "error.hh"

#include "util.hh"
#include "config.hh"
#include "hex_dump.hh"

#include <time.h>

#include "worker_thread.hh"
#include "thread_buffer.hh"

hld_source::hld_source()
{
}


void hld_source::new_file()
{
  _last_seq_no = 0;

  // _chunk_cur = _chunk_end = NULL;
}




template<typename T_header>
bool swap_hld_header(T_header *header)
{
  uint32 high_byte = header->_decoding.u32 & 0xff000000;
  uint32 low_byte  = header->_decoding.u32 & 0x000000ff;

  if (!high_byte && low_byte)
    return false; // no swapping needed

  if (high_byte && !low_byte)
    {
      // swapping needed
      byteswap ((uint32*) header,sizeof (*header));
      return true;
    }
  ERROR("%s decoding (%08x) marker broken, cannot determine endianess.",
	sizeof (*header) == sizeof(hld_event_header) ? "Event" : "Subevent",
	header->_decoding.u32);
}




#if !USE_THREADING && !USE_MERGING
FILE_INPUT_EVENT _file_event;
#endif

hld_event *hld_source::get_event()
{
  hld_event *dest;

#ifdef USE_THREADING
  // Even if we blow up with an error, the reclaim item will be in the
  // reclaim list, so the memory wont leak
  dest = (hld_event *) _wt._defrag_buffer->allocate_reclaim(sizeof (hld_event));
#else
  _file_event.release();
  dest = &_file_event;
#endif

  dest->_status = 0;
  dest->_subevents = NULL;

  _input.release_to(_input._cur);

  if (!_input.read_range(&dest->_header,sizeof(dest->_header)))
    return NULL;

  swap_hld_header(&dest->_header);

  // So header is safely retrieved.

  // Now check that it makes sense

  //printf ("pad:   %d\n",dest->_header._pad);
  //printf ("size:  %d\n",dest->_header._size);
  //printf ("align: %d\n",dest->_header._decoding._align);

  if (UNLIKELY(dest->_header._decoding._align > 4))
    ERROR("Strange alignment, > 4 (which would be 128 bits)");

  // A rather stringent test that we really found a header is to check
  // that the date specification really specifies a date (note that
  // seconds are to be allowed to be 60)

  // Lets still be a bit lazy...  We simply check against a bitmap

  // year - 0-255 : allow: 0xff, month 0-11: 0x0f, day 1-31: 0x1f
  if (UNLIKELY(dest->_header._date & ~(0x00ff0f1f)))
    ERROR("Date specification (%08x) invalid.",dest->_header._date);

  // hour - 0-23 : allow: 0x1f, minute 0-59: 0x3f, second 0-60: 0x3f
  if (UNLIKELY(dest->_header._time & ~(0x001f3f3f)))
    ERROR("Time specification (%08x) invalid.",dest->_header._time);

  /*
	   ((_header._date >> 16) & 0xff) + 1900,
	   ((_header._date >>  8) & 0xff),
	   ((_header._date      ) & 0xff),
	   ((_header._time >> 16) & 0xff),
	   ((_header._time >>  8) & 0xff),
	   ((_header._time      ) & 0xff));
  */

  if (UNLIKELY(dest->_header._size > 0x40000000))
    ERROR("Data length (0x%08x) very large, refusing.",
	  dest->_header._size);

  // And then retrieve the data itself.

  dest->_chunk_cur = dest->_chunk_end = dest->_chunks;
  dest->_offset_cur = 0;

  size_t alignment = 1 << dest->_header._decoding._align;

  dest->_unused = (-dest->_header._size) & (alignment - 1);

  // Get the data along with the alignment part, then discard the alignment

  size_t data_size = dest->_header._size - sizeof(dest->_header);

  data_size += dest->_unused;

  // printf ("unused:%d  (total: %d)\n",dest->_unused,data_size);

  // Do not retrieve any data if the data size is 0 (or locate_subevents will
  // try to find a first header...

  if (LIKELY(data_size))
    {
      int chunks = 0;

      chunks = _input.map_range(data_size,dest->_chunks);

      if (UNLIKELY(!chunks))
	{
	  ERROR("Error while reading subevent data.");
	  return NULL;
	}

      dest->_chunk_end += chunks;
    }

  // Do not discard the alignment padding at the end of the subevent
  // data, instead, after locating all subevents, check that the
  // padded amount of data was also padding in the last subevent

  // ok, so data for event is in the chunks

  return dest;
}














// Event-----------:----------  Id:-------- Size-------- xxxx-xx-xx  xx:xx:xx

// Event-----------:----------  Id:--------(-/----) Align-- Type----- Size--------
//                              xxxx-xx-xx xx:xx:xx
//   SubEv     Trig:----------  Id:--------(-/----) WSize-- Type----- Size--------

void hld_event::print_event(int data,hex_dump_mark_buf *unpack_fail) const
{
  printf("Event%s%11u%s:%s%10u%s  Align%s%2d%s Type%s%5d%s "
	 "Id:%s%08x%s(%s%d%s/%s%4d%s) Size%s%8d%s\n",
	 CT_OUT(BOLD_BLUE),_header._file_no,CT_OUT(NORM_DEF_COL),
	 CT_OUT(BOLD_BLUE),_header._seq_no,CT_OUT(NORM_DEF_COL),
	 CT_OUT(BOLD_BLUE),_header._decoding._align,CT_OUT(NORM_DEF_COL),
	 CT_OUT(BOLD_BLUE),_header._decoding._type,CT_OUT(NORM_DEF_COL),
	 CT_OUT(BOLD_BLUE),_header._id,CT_OUT(NORM_DEF_COL),
	 CT_OUT(BOLD_BLUE),_header._id >> 31,CT_OUT(NORM_DEF_COL),
	 CT_OUT(BOLD_BLUE),_header._id & 0x7fff,CT_OUT(NORM_DEF_COL), // 0-8191
	 CT_OUT(BOLD_BLUE),_header._size,CT_OUT(NORM_DEF_COL));

  printf("                             %s%04d-%02d-%02d %02d:%02d:%02d%s\n",
	 CT_OUT(BOLD_BLUE),
	 ((_header._date >> 16) & 0xff) + 1900,
	 ((_header._date >>  8) & 0xff) + 1,
	 ((_header._date      ) & 0xff),
	 ((_header._time >> 16) & 0xff),
	 ((_header._time >>  8) & 0xff),
	 ((_header._time      ) & 0xff),
	 CT_OUT(NORM_DEF_COL));

  // Subevents

  for (int subevent = 0; subevent < _nsubevents; subevent++)
    {
      hld_subevent *subevent_info = &_subevents[subevent];

      bool subevent_error = (unpack_fail &&
			     unpack_fail->_next == &subevent_info->_header);

      printf("  %sSubEv%s     Trig:%s%10u%s  "
	     "WSize%s%2d%s Type%s%5d%s "
	     "Id:%s%08x%s(%s%d%s/%s%4d%s) Size%s%8d%s\n",
	     subevent_error ? CT_OUT(BOLD_RED) : "",
	     subevent_error ? CT_OUT(NORM_DEF_COL) : "",
	     CT_OUT(BOLD_MAGENTA),
	     subevent_info->_header._trig_no,
	     CT_OUT(NORM_DEF_COL),
	     CT_OUT(BOLD_MAGENTA),
	     subevent_info->_header._decoding._align,
	     CT_OUT(NORM_DEF_COL),
	     CT_OUT(BOLD_MAGENTA),
	     subevent_info->_header._decoding._type,
	     CT_OUT(NORM_DEF_COL),
	     CT_OUT(BOLD_MAGENTA),
	     subevent_info->_header._id,
	     CT_OUT(NORM_DEF_COL),
	     CT_OUT(BOLD_MAGENTA),
	     subevent_info->_header._id >> 31,
	     CT_OUT(NORM_DEF_COL),
	     CT_OUT(BOLD_MAGENTA),
	     subevent_info->_header._id & 0x7fff,
	     CT_OUT(NORM_DEF_COL),
	     CT_OUT(BOLD_MAGENTA),
	     subevent_info->_header._size,
	     CT_OUT(NORM_DEF_COL));

      if (data)
	{
	  hex_dump_buf buf;

	  if (subevent_info->_data)
	    {
	      uint32 data_length = subevent_info->_header._size;

	      if (subevent_info->_header._decoding._align >= 2 &&
		  (data_length & 3) == 0) // length is divisible by 4
		hex_dump(stdout,
			 (uint32*) subevent_info->_data,
			 subevent_info->_data + data_length,subevent_info->_swapping,
			 "%c%08x",9,buf,unpack_fail);
	      else if (subevent_info->_header._decoding._align >= 1 &&
		       (data_length & 1) == 0) // length is divisible by 2
		hex_dump(stdout,
			 (uint16*) subevent_info->_data,
			 subevent_info->_data + data_length,subevent_info->_swapping,
			 "%c%04x",5,buf,unpack_fail);
	      else
		hex_dump(stdout,
			 (uint8*) subevent_info->_data,
			 subevent_info->_data + data_length,subevent_info->_swapping,
			 "%c%02x",3,buf,unpack_fail);
	    }
	}
    }

  // Is there any remaining data, that could not be assigned as a subevent?

  print_remaining_event(_chunk_cur,_chunk_end,_offset_cur,0/*subevent_info->_swapping*/);

}










void hld_event::locate_subevents(hld_event_hint */*hints*/)
{
  if (_status & HLD_EVENT_LOCATE_SUBEVENTS_ATTEMPT)
    return;

  _status |= HLD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;

  assert(!_subevents); // we have already been called!

  // get some space, for at least so many subevents as we have seen at
  // most before...

#define MAX_SUBEVENTS 100 // TODO: fix this!!!

#ifdef USE_THREADING
  _subevents = (hld_subevent *)
    _wt._defrag_buffer->allocate_reclaim(sizeof (hld_subevent) * MAX_SUBEVENTS);
#else
  _subevents = (hld_subevent *)
    _defrag_event.allocate(sizeof (hld_subevent) * MAX_SUBEVENTS);
#endif

  _nsubevents = 0;

  size_t alignment_mask = (1 << _header._decoding._align) - 1;

  // Then, let's chew over the event...

  buf_chunk *chunk_cur  = _chunk_cur;
  size_t     offset_cur = _offset_cur;

  // size_t total_size = 0, total_unused = 0;

  size_t unused = 0;

  //void *start = fragment->_ptr;
  //size_t left;

  while (chunk_cur < _chunk_end)
    {
      // First of all, do we have space to dump this subevent?
      /*
      printf ("%x / %x (%d)\n",
	      offset_cur,chunk_cur->_length,
	      _chunk_end - chunk_cur);
      fflush(stdout);
      */

      // We have more data to read.
      // Does this fragment have enough data for the subevent header?

      hld_subevent             *subevent_info = &_subevents[_nsubevents];
      hld_subevent_header      *subevent_header = &subevent_info->_header;

      if (!get_range_many((char *) subevent_header,
			  chunk_cur,offset_cur,_chunk_end,
			  sizeof(hld_subevent_header)))
	{
	  // We're simply out of data.
	  ERROR("Subevent header not complete before end of event.");
	}

      // Ok, we have gotten the subevent header.  Byteswap it (if needed)!

      subevent_info->_swapping = swap_hld_header(subevent_header);

      if (subevent_header->_decoding._align > 3)
	ERROR("Strange subevent data size, > 3 (which would be 64 bits)");

      // So, subevent header is there, now remember where we got the
      // data

      size_t data_length = subevent_header->_size - sizeof(hld_subevent_header);

      // Is there any padding data to be skipped?

      unused = (-subevent_header->_size) & alignment_mask;

      // total_size   += subevent_header->_size;
      // total_unused += unused;

      //printf ("size: %d  ... unused:%d  trig: %d\n",
      //        data_length,unused,subevent_header->_trig_no);

      // Did the data come exclusively from this buffer?

      size_t chunk_left = chunk_cur < _chunk_end ?
	chunk_cur->_length - offset_cur : 0;

      if (LIKELY(data_length + unused <= chunk_left))
	{
	  // likely, since most subevents smaller than buffer size

	  if (UNLIKELY(!data_length))
	    {
	      // if the data size is zero, and we're the last
	      // subevent, then chunk_cur will not be valid, and we
	      // should not give such a pointer away (i.e. we may not
	      // dereference chunk_cur, so we cannot give it away)

	      // But we want to give an invalid (but non-null) pointer
	      // (since the length is zero, noone should _ever_ look
	      // at it...

	      subevent_info->_data = (char*)16;
	    }
	  else
	    {
	      subevent_info->_data = chunk_cur->_ptr + offset_cur;
	    }

	  data_length += unused;

	  offset_cur += data_length;
      	}
      else
	{
	  data_length += unused;

	  // We have some fun, first of all, the data is not stored
	  // continous yet

	  subevent_info->_data   = NULL;

	  // This is where to get to the data

	  subevent_info->_frag   = chunk_cur;
	  subevent_info->_offset = offset_cur;

	  // Second of all, we must make sure that one cat get to
	  // the data at all!  (we must anyhow read past it to get to
	  // the next subevent in the buffer)

	  // Each time we come into the loop, the current fragment
	  // has some data, but not enough

	  data_length -= chunk_left;

	  // So, go get next fragment

	  chunk_cur++;

	  if (chunk_cur >= _chunk_end ||
	      data_length > chunk_cur->_length)
	    {
	      ERROR("Subevent (id %d, decoding 0x%08x) length %d bytes, "
		    "not complete before end of event, %d bytes missing",
		    subevent_info->_header._id,
		    subevent_info->_header._decoding.u32,
		    subevent_info->_header._size,
		    (int) data_length);
	    }

	  chunk_left = chunk_cur->_length;

	  offset_cur = data_length;

	  // Account for how much data was used in ending fragment
	}

      // One more subevent fitted in the event...

      _nsubevents++;

      // Now, we might exactly have reached a fragment boundary
      // This we need not consider, since it would be handled at
      // the beginning of the loop as an fragmented subevent header!
      // But then, we'll at some times activate the branch for
      // fragmented headers.  Better let it be handled here

      if (chunk_left == data_length)
	{
	  chunk_cur++;
	  offset_cur = 0;
	}

      // Subevent was successfully parsed, note that...

      _chunk_cur = chunk_cur;
      _offset_cur = offset_cur;
    }

  // Check that padding was padding.  Hmm, it seems for some files,
  // the total data size of the event does not include the last
  // subevent's padding.  For other files, it includes the padding,
  // and thus the event is not itself padded...  In any case, the
  // subevent extraction above has never gone into non-event data for
  // the real data
  /*
  printf ("event: %d+%d   subev_tot:  %d+%d=%d  +32=%d\n",
	  _header._size,_unused,
	  total_size,total_unused,
	  total_size+total_unused,
	  total_size+total_unused+32);
  */
  if (unused != (size_t) _unused && _unused != 0)
    ERROR("Last subevent padding (%zd) does not match event padding (%zd).",
	  unused,_unused);

  //printf ("===> %d\n",_nsubevents);
  //  fflush(stdout);

  // Aha, so we have successfully gotten all subevent data
}





void hld_event::get_subevent_data_src(hld_subevent *subevent_info,
				      char *&start,char *&end)
{
  // Now, if we would be running on a Cell SPU, we would be
  // wanting to get the data from the main memory in any case,
  // with DMA, so we can handle the fragmented cases just as well
  // as the non-fragmented.  It is just a few more DMA operations.
  // So that one would use a special function, and not this one.
  // This one is for use by normal processors.

  // We have a few cases.  Either the data was not fragmented:

  size_t data_length = subevent_info->_header._size - sizeof(hld_subevent_header);

  if (UNLIKELY(subevent_info->_data == NULL))
    {
      // We need to copy the subevent to an defragment buffer.
      // The trouble with this buffer is that it needs to be
      // deallocated or marked free somehow also...

      // Defragmentation buffers.  Need to survive until we have
      // retired the event (retire = ALL operations done).  This
      // is since we may be requested to do a data dump of an
      // (broken) event (or a normal event dump).  However, since
      // different processes might be responsible for allocating
      // the defrag buffer (either the main file reading one (due
      // to event printout), or the unpacking one (due to unpack)
      // or the retirement one (due to debug printout), we need to
      // have one defrag buffer pool per such process, since
      // allocating should not need to wait for anyone else.  The
      // pointer supplied will be stable for the rest of the event
      // life-time, i.e. can be used by other threads on the same
      // machine.

#ifdef USE_THREADING
      char *defrag = (char *)
	_wt._defrag_buffer->allocate_reclaim(data_length);
#else
      char *defrag = (char *)
	_defrag_event_many.allocate(data_length);
#endif

      subevent_info->_data = defrag;

      buf_chunk *frag = subevent_info->_frag;

      size_t size0 = frag->_length - subevent_info->_offset;

      // First copy data from first fragmented buffer

      memcpy(defrag,
	     frag->_ptr + subevent_info->_offset,
	     size0);

      defrag += size0;
      size_t length = data_length - size0;

      frag++;

      // And copy last one

      assert(length <= frag->_length);

      memcpy(defrag,frag->_ptr,length);
      /*
	for (uint i = 0; i < subevent_info->_length; i++)
	printf ("%02x ",(uint8) subevent_info->_data[i]);
	printf ("\n");
	fflush(stdout);
      */
    }

  start = subevent_info->_data;
  end   = subevent_info->_data + data_length;

  return;
}


