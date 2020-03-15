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

#include "genf_input.hh"

#include "error.hh"

#include "swapping.hh"
#include "hex_dump.hh"

#include "worker_thread.hh"
#include "thread_buffer.hh"

genf_source::genf_source()
{
}

void genf_source::new_file()
{
  _chunk_cur = _chunk_end = NULL;
}

bool genf_source::read_record()
{
  // Read the next record of the file

  // we (schedule) an release of the previous record
  _input.release_to(_input._cur);

  if (!_input.read_range(&_record_header,sizeof(_record_header)))
    return false;

  _swapping = false;

  // now, if swapping is active, swap the header
  if (_swapping)
    _record_header._length = bswap_16(_record_header._length);

  _chunk_cur = _chunk_end = _chunks;

  // read the record itself

  size_t data_size = ((size_t) _record_header._length) * sizeof (uint16);

  /////////////////////////////////////////////

  int chunks = 0;

  chunks = _input.map_range(data_size,_chunks);

  if (!chunks)
    {
      ERROR("Error while reading record data.");
      return false;
    }

  _chunk_end += chunks;

  return true;
}

bool genf_source::skip_record()
{
  // The genf event format has NO chance to resynchronise
  return false;
}

#ifndef USE_THREADING
FILE_INPUT_EVENT _file_event;
#endif

genf_event *genf_source::get_event()
{
  genf_event *dest;

#ifdef USE_THREADING
  // Even if we blow up with an error, the reclaim item will be in the
  // reclaim list, so the memory wont leak
  dest = (genf_event *) _wt._defrag_buffer->allocate_reclaim(sizeof (genf_event));
#else
  _file_event.release();
  dest = &_file_event;
#endif

  for ( ; ; )
    {
      if (_chunk_cur == _chunk_end)
	{
	  //printf ("get record...\n");
	  if (!read_record())
	    return NULL;
	  continue; // We read a record, just make sure we're not immediately at it's end
	}

      //printf ("get ev header...\n");

      genf_event_header *event_header = &dest->_header;

      *event_header = _record_header;

      if (!event_header->_length)
	{
	  _chunk_cur = _chunk_end;
	  continue; // next record
	}

      if (!get_range(dest->_chunks,&dest->_data,
		     _chunk_cur,_chunk_end,
		     event_header->_length * sizeof (uint16)))
	ERROR("Event larger than record. %d",event_header->_length);

      dest->_swapping = _swapping;

      return dest;
    }
}
/*
bool genf_source::get_event()
{
  return get_event(&_cur_event) != NULL;
}
*/
void genf_source::print_file_header()
{
}

void genf_source::print_buffer_header()
{
  printf ("Record header:\n");
  printf ("Length %s%04x%s - %s%d%s\n",
	  CT_OUT(BOLD),_record_header._length,CT_OUT(NORM),
	  CT_OUT(BOLD),_record_header._length,CT_OUT(NORM));
}

void genf_event::print_event(int data,hex_dump_mark_buf *unpack_fail) const
{
  printf (" Length %s%04x%s - %s%d%s\n",
	  CT_OUT(BOLD_BLUE),_header._length,CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD_BLUE),_header._length,CT_OUT(NORM_DEF_COL));

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

void genf_event::get_data_src(uint16 *&start,uint16 *&end)
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
