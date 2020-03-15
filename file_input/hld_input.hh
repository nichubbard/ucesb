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

#ifndef __HLD_INPUT_HH__
#define __HLD_INPUT_HH__

#include "input_buffer.hh"
#include "decompress.hh"
#include "input_event.hh"
#include "hex_dump_mark.hh"

#include "typedef.hh"
#include <stdlib.h>

#ifdef USE_HLD_INPUT

#include "swapping.hh"

#include "hld_event.hh"

// See http://www-hades.gsi.de/daq/raw_data_format16.html

// The HADES raw event data (termed hld here due to file endings) does
// in contrast to other file formats not contain any buffers or
// records.  The events are located directly in the files.  Some
// carefulness is thus required as the events (and headers) may span
// different input chunks.



#define HLD_EVENT_DECODING_SWAP_A  0xff000000
#define HLD_EVENT_DECODING_SWAP_B  0x000000ff


struct hld_subevent
{
  hld_subevent_header _header;

  char      *_data;   // ptr to data in contigous memory
  buf_chunk *_frag;   // where data is (fragmented)
  size_t     _offset; // offset into fragment

  // size_t     _length; not needed, used verbatim from _header

  bool       _swapping;
};


#define HLD_EVENT_LOCATE_SUBEVENTS_ATTEMPT  0x0004
#define HLD_EVENT_SUBEVENTS_LEFTOVERS       0x0008 // could not read subevents to end...



struct hld_subevent_extra
{

public:
  size_t subevent_length(const hld_subevent_header *header) const
  {
    return header->_size - sizeof(hld_subevent_header);
  }

  size_t subevent_alignment_unused(hld_event_header* header,size_t data_size) const
  {
    size_t alignment = 1 << header->_decoding._align;

    size_t unused = (-data_size) & (alignment - 1);

    return unused;
  }

  void subevent_incomplete_ERROR(const hld_subevent_header *header,
				 size_t missing) const
  {
    ERROR("Subevent (id %d, decoding 0x%08x), length %d bytes, "
	  "not complete before end of event, %d bytes missing",
	  header->_id,
	  header->_decoding.u32,
	  header->_size,
	  (int) missing);
  }
};

struct hld_event_hint // see lmd_event_hint (do the copy-paste...)
{
};

struct hld_event
  : public input_event
{
  hld_event_header _header;

  int        _status;

  size_t     _unused;

  /* Whenever someone has gone through the trouble to locate the
   * subevents (all _will_ be done at once, we get a pointer to that
   * array of information.
   */

  hld_subevent *_subevents;
  int           _nsubevents; // not valid until _subevents has been set

  /* HLD events do not span multiple buffers, maximum is 2 chunks, due
   * to wrapping of input buffers.
   */

  buf_chunk *_chunk_end;   // the current end of chunks
  buf_chunk *_chunk_cur;   // the current end of chunks
  size_t     _offset_cur;

#ifndef USE_THREADING
public:
  keep_buffer_many _defrag_event_many;

public:
  void release()
  {
    input_event::release();
    _defrag_event_many.release();
  }
#endif

  // As opposed to LMD events, this must not necessarily be the last
  // members, as we at most have two chunks

  buf_chunk  _chunks[2];

public:
  void locate_subevents(hld_event_hint *hints);
  void get_subevent_data_src(hld_subevent *subevent_info,
			     char *&start,char *&end);
  void print_event(int data,hex_dump_mark_buf *unpack_fail) const;
};

#define FILE_INPUT_EVENT hld_event

#ifndef USE_THREADING
extern FILE_INPUT_EVENT _file_event;
#endif

class hld_source :
  public data_input_source
{
public:
  hld_source();
  ~hld_source() { }

public:
  void new_file();

public:
  int                _last_seq_no;

  /*
public:
  buf_chunk  _chunks[2];
  buf_chunk *_chunk_cur;
  buf_chunk *_chunk_end;
  */

public:
  hld_event *get_event(/*hld_event *dest*/);

public:
  void print_file_header();
  //void print_buffer_header();

public:
  bool read_record();
  bool skip_record();
};

#endif//USE_HLD_INPUT

#endif//__HLD_INPUT_HH__

