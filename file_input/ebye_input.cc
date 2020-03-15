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

#include "ebye_input.hh"

#include "error.hh"
#include "config.hh"
#include "hex_dump.hh"

#include "swapping.hh"

#include "worker_thread.hh"
#include "thread_buffer.hh"

ebye_source::ebye_source()
{
  _last_seq_no = 0;
}

void ebye_source::new_file()
{
  _chunk_cur = _chunk_end = NULL;
}

/*
void ebye_source::open(const char *filename)
{
  data_input_source::open(filename);

  _last_seq_no = -1;

  read_record();
}
*/

bool ebye_source::read_record()
{
  // Read the next record of the file

  // we (schedule) an release of the previous record
  _input.release_to(_input._cur);

 read_header:
  if (!_input.read_range(&_record_header,sizeof(_record_header)))
    return false;

  if (memcmp(_record_header._id,EBYE_RECORD_ID,8) != 0)
    {
      // Now, what we found was not a record header.

      // This is a ugly hack

      // The filler seems to be 0x5e, so if not, we cause an error
      // (endianess does not matter, all bytes 0x5e)

      if (_record_header._id[0] != 0x5e)
	ERROR("Record ID not %s.",EBYE_RECORD_ID);

      // Attempt to search forward to next aligned 1024 bytes.
      // 1024 is a guess.

      // The input buffer keeps track of where we are

      off_t header_start = _input._cur - sizeof(_record_header);
      off_t next_try = (header_start & ~((off_t) 0x3ff)) + 0x400;

      // So move the input there

      _input._cur = next_try;

      // And try again

      goto read_header;
    }

  switch (_record_header._endian_tape)
    {
    case 0x0001: // our endianess, nothing to do
      break;
    case 0x0100: // opposite endianess
      _record_header._sequence    = bswap_32(_record_header._sequence);
      _record_header._stream      = bswap_16(_record_header._stream);
      _record_header._tape        = bswap_16(_record_header._tape);
      _record_header._data_length = bswap_32(_record_header._data_length);
      break;
    default:
      ERROR("Unexpected tape endianess (0x%04x).",_record_header._endian_tape);
    }

  if (_conf._print_buffer)
    print_buffer_header(&_record_header);

  if (_record_header._sequence !=          (_last_seq_no+1) &&
      _record_header._sequence != (uint16) (_last_seq_no+1))
    {
      WARNING("Record sequence number mismatch (%d, prev %d).",
	      _record_header._sequence,_last_seq_no);
    }
  _last_seq_no = _record_header._sequence;

  // The stream number should be 1<=no<=4, but apparently also 0
  // occurs...
  if (_record_header._stream > 4)
    ERROR("Unexpected stream number (%d).",_record_header._stream);

  if (_record_header._tape != 1)
    ERROR("Unexpected tape (%d).",_record_header._tape);

  if (_record_header._data_length > 0x40000000)
    ERROR("Data length (0x%08x) very large, refusing.",
	  _record_header._data_length);

  _chunk_cur = _chunk_end = _chunks;

  // read the record itself

  int data_size = _record_header._data_length;

  /////////////////////////////////////////////

  int chunks = 0;

  chunks = _input.map_range(data_size,_chunks);

  if (!chunks)
    {
      ERROR("Error while reading record data.");
      return false;
    }

  _chunk_end += chunks;

  _swapping = false;

  switch (_record_header._endian_data)
    {
    case 0x0001: // our endianess, nothing to do
      break;
    case 0x0100: // opposite endianess
      // do_swap32(_record_buf,data_size);
      _swapping = true;
      break;
    default:
      ERROR("Unexpected data endianess (0x%04x).",_record_header._endian_tape);
    }

  return true;
}

bool ebye_source::skip_record()
{
  // TODO: Also handle the case when the filler got momentarily wrong...
  // (might give many spurios errors until things resyncronise...)

  _input._cur = (_input._cur & ~0x3ff) + 0x400;

  //

  _chunk_cur = _chunk_end;

  return true;
}

#ifndef USE_THREADING
FILE_INPUT_EVENT _file_event;
#endif

ebye_event *ebye_source::get_event()
{
  // Search for the next event

  ebye_event *dest;

#ifdef USE_THREADING
  // Even if we blow up with an error, the reclaim item will be in the
  // reclaim list, so the memory wont leak
  dest = (ebye_event *) _wt._defrag_buffer->allocate_reclaim(sizeof (ebye_event));
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

      ebye_event_header *event_header = &dest->_header;

      if (!get_range((char *) event_header,
		     _chunk_cur,_chunk_end,
		     sizeof(ebye_event_header)))
	{
	  ERROR("Failure reading event header.");
	}

      if (_swapping)
	{
#ifdef USE_EBYE_INPUT_16
	  event_header->_start_end_token =
	    bswap_16(event_header->_start_end_token);
	  event_header->_length =
	    bswap_16(event_header->_length);
#endif
#ifdef USE_EBYE_INPUT_32
	  event_header->_start_end_token_length =
	    bswap_32(event_header->_start_end_token_length);
#endif
	}

#ifdef USE_EBYE_INPUT_16
      uint16 start_end_token = event_header->_start_end_token;
      uint16 length          = event_header->_length;
#endif
#ifdef USE_EBYE_INPUT_32
      uint16 start_end_token =
	(uint16) (event_header->_start_end_token_length >> 16);
      uint16 length = event_header->_start_end_token_length & 0x0000ffff;
#endif

      if (start_end_token != 0xffff)
	{
	  // Discard rest of record
	  _chunk_cur = _chunk_end;
#ifdef USE_EBYE_INPUT_16
  	  ERROR("Expected start event/end block token, got 0x%04x.",
		event_header->_start_end_token);
#endif
#ifdef USE_EBYE_INPUT_32
  	  ERROR("Expected start event/end block token, got 0x%08x.",
		event_header->_start_end_token_length);
#endif
	}

      if (!length)
	{
	  if (_chunk_cur != _chunk_end)
	    WARNING("Unused data in record, >0 bytes.");

	  _chunk_cur = _chunk_end; // not needed
	  continue; // next record
	}

      length = (uint16) (length - sizeof(ebye_event_header));

      //printf ("got ev header...\n");

      if (length & 3)
	{
	  _chunk_cur = _chunk_end;
	  ERROR("Event length not 32 bit aligned.");
	}

      if (!get_range(dest->_chunks,&dest->_data,
		     _chunk_cur,_chunk_end,
		     length))
	ERROR("Event longer than data in record.");

      dest->_swapping = _swapping;

      return dest;
    }
}
/*
bool ebye_source::get_event()
{
  return get_event(&_cur_event) != NULL;
}
*/
void ebye_source::print_file_header()
{
}

void ebye_source::print_buffer_header(const ebye_record_header *header)
{
  printf("Record header:\n");
  printf("Length %s%d%s  SeqNo %s%d%s  Stream %s%d%s  Tape %s%d%s\n",
	 CT_OUT(BOLD),header->_data_length,CT_OUT(NORM),
	 CT_OUT(BOLD),header->_sequence,CT_OUT(NORM),
	 CT_OUT(BOLD),header->_stream,CT_OUT(NORM),
	 CT_OUT(BOLD),header->_tape,CT_OUT(NORM));
}

void ebye_event::print_event(int data,hex_dump_mark_buf *unpack_fail) const
{
#ifdef USE_EBYE_INPUT_16
  printf(" Length %s%d%s\n",
	 CT_OUT(BOLD_BLUE),_header._length,CT_OUT(NORM_DEF_COL));
#endif
#ifdef USE_EBYE_INPUT_32
  printf(" Length %s%d%s\n",
	 CT_OUT(BOLD_BLUE),
	 _header._start_end_token_length & 0x0000ffff,
	 CT_OUT(NORM_DEF_COL));
#endif

  if (data)
    {
      // We do not use get_data_src, since that causes the need for
      // a reclaim item (which must be inserted properly into the lists)
      // such that it gets properly deallocated

      hex_dump_buf buf;

#ifdef USE_EBYE_INPUT_16
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
#endif
#ifdef USE_EBYE_INPUT_32
      if (_data)
	hex_dump(stdout,
		 (uint32*) _data,_data + _chunks[0]._length,_swapping,
		 "%c%08x",9,buf,unpack_fail);
      else
	{
	  hex_dump(stdout,
		   (uint32*) _chunks[0]._ptr,
		   _chunks[0]._ptr + _chunks[0]._length,_swapping,
		   "%c%08x",9,buf,unpack_fail);
	  hex_dump(stdout,
		   (uint32*) _chunks[1]._ptr,
		   _chunks[1]._ptr + _chunks[1]._length,_swapping,
		   "%c%08x",9,buf,unpack_fail);
	}
#endif
    }
}

/*
void ebye_source::print_event(int data)
{
  _cur_event.print_event(data);
}
*/


void ebye_event::get_data_src(ebye_data_t *&start,ebye_data_t *&end)
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

  start = (ebye_data_t*)  _data;
  end   = (ebye_data_t*) (_data + _chunks[0]._length);

  // printf ("Event_get_data: %d\n",end-start);
}
