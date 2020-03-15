/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
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

#include "pax_input.hh"

#include "error.hh"

#include "swapping.hh"
#include "config.hh"
#include "hex_dump.hh"

#include "worker_thread.hh"
#include "thread_buffer.hh"

pax_source::pax_source()
{
  _last_seq_no = 0;
}

void pax_source::new_file()
{
  _chunk_cur = _chunk_end = NULL;
}

/*
void pax_source::open(const char *filename)
{
  data_input_source::open(filename);

  read_record();
}
*/
bool pax_source::read_record()
{
  // Read the next record of the file

  // we (schedule) an release of the previous record
  _input.release_to(_input._cur);

  if (!_input.read_range(&_record_header,sizeof(_record_header)))
    return false;

  /*
  n = full_read(_fd,&_record_header,sizeof(_record_header),true);

  if (n == 0)
    return false;

  if (n != sizeof(_record_header))
    {
      perror("read");
      ERROR("Error while reading record header.");
    }
  */

  _swapping = false;

  switch (_record_header._type)
    {
    case PAX_RECORD_TYPE_UNDEF:
    default:
      ERROR("Unknown/unhandled record type (0x%04x).",_record_header._type);
      break;
    case PAX_RECORD_TYPE_STD:
      break;
    case PAX_RECORD_TYPE_STD_SWAP:
      _swapping = true;
      break;
    }
  /*
  printf ("record_header: %04x %04x %04x %04x\n",
	  _record_header._length,
	  _record_header._type,
	  _record_header._seqno,
	  _record_header._off_nonfrag);
  */
  // now, if swapping is active, swap the header
  if (_swapping)
    byteswap((uint16 *) &_record_header,sizeof(_record_header));

  if (_conf._print_buffer)
    print_buffer_header(); // &_record_header

  if (_record_header._length > PAX_RECORD_MAX_LENGTH)
    ERROR("Record length (%d) too large.",_record_header._length);

  if (_record_header._seqno != (uint16) (_last_seq_no+1))
    {
      WARNING("Record sequence number mismatch (%d, prev %d).",
	      _record_header._seqno,_last_seq_no);
    }
  _last_seq_no = _record_header._seqno;

  _chunk_cur = _chunk_end = _chunks;

  // read the record itself

  size_t data_size = _record_header._length - sizeof(_record_header);

  /////////////////////////////////////////////

  int chunks = 0;

  chunks = _input.map_range(data_size,_chunks);

  if (!chunks)
    {
      ERROR("Error while reading record data.");
      return false;
    }

  // _record_avail = 0; // If we blow up, try next record :-)

  _chunk_end += chunks;

  // Find out where the next non-spanned event starts

  if (_record_header._off_nonfrag > _record_header._length)
    ERROR("Non-frag pointer outside record (%d>%d).  (too large)",
	  _record_header._off_nonfrag,_record_header._length);
  if (_record_header._off_nonfrag < sizeof(_record_header))
    ERROR("Non-frag pointer outside record (%d).  (too small)",
	  _record_header._off_nonfrag);

  size_t skip = _record_header._off_nonfrag - sizeof(_record_header);

  if (!skip_range(_chunk_cur,_chunk_end,skip))
    ERROR("Internal error.");  // should have been caught above

  return true;
}

// This routine is called when we have failed to do an get_event
// Discared the current record and try to continue reading the file If
// the failure happened while trying to read a record itself (e.g. due
// to a malformed record header) it is sane to try again.
// If the failure was due to running out of data while reading the
// record header, we'll simply get a double fault :-)   Hmm, no, since
// that returns gracefully.

bool pax_source::skip_record()
{
  // Hmm, in case the record header was malformed, we should possibly
  // try to reset the read pointer at some sane location, since if the
  // record header was bad, it may have caused us to go too far ahead
  // in the file

  // TODO: the read routines need to be protected against requesting
  // more data than our buffers can hold!!!

  _chunk_cur = _chunk_end;

  return true;
}

#ifndef USE_THREADING
FILE_INPUT_EVENT _file_event;
#endif

pax_event *pax_source::get_event()
{
  // Search for the next event

  // _event_header   = NULL;
  // _event_data     = NULL;
  // _event_data_end = NULL;

  //printf ("get event...\n");

  pax_event *dest;

#ifdef USE_THREADING
  // Even if we blow up with an error, the reclaim item will be in the
  // reclaim list, so the memory wont leak
  dest = (pax_event *) _wt._defrag_buffer->allocate_reclaim(sizeof (pax_event));
#else
  _file_event.release();
  dest = &_file_event;
#endif

  for ( ; ; )
    {
      //printf ("get record?...\n");
      if (_chunk_cur == _chunk_end)
	{
	  //printf ("get record...\n");
	  if (!read_record())
	    return NULL;
	  continue; // We read a record, just make sure we're not immediately at it's end
	}

      //printf ("get ev header...\n");

      pax_event_header *event_header = &dest->_header;

      if (!get_range((char *) event_header,
		     _chunk_cur,_chunk_end,
		     sizeof(pax_event_header)))
	{
	  // Now, if the fault was due to us being out of buffer,
	  // then we gracefully allow it

	  if (_chunk_cur == _chunk_end)
	    continue;

	  ERROR("Failure reading event header.");
	}

      /*
      _event_header = (pax_event_header*) _record_cur;
      */

      if (_swapping)
	byteswap((uint16 *) event_header,sizeof(pax_event_header));

      if (!event_header->_length)
	{
	  _chunk_cur = _chunk_end; // not needed
	  continue; // next record
	}

      //printf ("got ev header...\n");

      if (event_header->_length < sizeof (pax_event_header))
	ERROR("Event smaller than header.");

      if (!get_range(dest->_chunks,&dest->_data,
		     _chunk_cur,_chunk_end,
		     event_header->_length - sizeof (pax_event_header)))
	ERROR("Event larger than record.  (spanned events not supported).");

      /*
      printf ("Event: %d-%d=%d\n",
	      event_header->_length , sizeof (pax_event_header),
	      event_header->_length - sizeof (pax_event_header));
      */

      dest->_swapping = _swapping;

      // _event_data = (uint16*) (_record_cur + sizeof (pax_event_header));
      // _event_data_end = _event_data + event_header->_length / sizeof(uint16);

      // _record_cur += _event_header->_length;

      return dest;
    }
}
/*
bool pax_source::get_event()
{
  return get_event(&_cur_event) != NULL;
}
*/
void pax_source::print_file_header()
{
}

void pax_source::print_buffer_header()
{
  printf("Record header:\n");
  printf("Length %s%d%s  Type %s%d%s  "
	 "SeqNo %s%d%s  OffNonFrag %s%d%s\n",
	 CT_OUT(BOLD),_record_header._length,CT_OUT(NORM),
	 CT_OUT(BOLD),_record_header._type,CT_OUT(NORM),
	 CT_OUT(BOLD),_record_header._seqno,CT_OUT(NORM),
	 CT_OUT(BOLD),_record_header._off_nonfrag,CT_OUT(NORM));
}

void pax_event::print_event(int data,hex_dump_mark_buf *unpack_fail) const
{
  printf(" Length %sd%d%s  Type %s%d%s\n",
	 CT_OUT(BOLD_BLUE),_header._length,CT_OUT(NORM_DEF_COL),
	 CT_OUT(BOLD_BLUE),_header._type,CT_OUT(NORM_DEF_COL));

  if (data)
    {
      // We do not use get_data_src, since that causes the need for
      // a reclaim item (which must be inserted properly into the lists)
      // such that it gets properly deallocated

      hex_dump_buf buf;

      if (_data)
	hex_dump(stdout,
		 (uint16*) _data,_data + _chunks[0]._length,_swapping,
		 "%c%04x",5,buf,unpack_fail);
      else
	{
	  hex_dump(stdout,
		   (uint16*) _chunks[0]._ptr,
		   _chunks[0]._ptr + _chunks[0]._length,_swapping,
		   "%c%04x",5,buf,unpack_fail);
	  hex_dump(stdout,
		   (uint16*) _chunks[1]._ptr,
		   _chunks[1]._ptr + _chunks[1]._length,_swapping,
		   "%c%04x",5,buf,unpack_fail);
	}
    }
}
/*
void pax_source::print_event(int data)
{
  _cur_event.print_event(data);
}
*/


void pax_event::get_data_src(uint16 *&start,uint16 *&end)
{
  if (UNLIKELY(!_data))
    {
      // We need to get the data from the chunks.  Since pointer has not
      // been set, we know it is two chunks (or more).  (Currently: only
      // two possible, since we do not support fragmented events)

      // The data need to be handled with a defragment buffer.

      void   *dest;

#ifdef USE_THREADING
      dest = _wt._defrag_buffer->allocate_reclaim(_chunks[0]._length +
						  _chunks[1]._length);
#else
      dest = _defrag_event.allocate(_chunks[0]._length +
				    _chunks[1]._length);
#endif

      _data = (char*) dest;

      memcpy(_data,_chunks[0]._ptr,_chunks[0]._length);
      memcpy(_data + _chunks[0]._length,
	     _chunks[1]._ptr,_chunks[1]._length);

      _chunks[0]._length += _chunks[1]._length;
    }

  start = (uint16*)  _data;
  end   = (uint16*) (_data + _chunks[0]._length);

  // printf ("Event_get_data: %d\n",end-start);
}
