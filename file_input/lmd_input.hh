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

#ifndef __LMD_INPUT_HH__
#define __LMD_INPUT_HH__

#include "input_buffer.hh"
#include "decompress.hh"
#include "input_event.hh"
#include "hex_dump_mark.hh"

#include "typedef.hh"
#include <stdlib.h>

#ifdef USE_LMD_INPUT

#include "lmd_event.hh"

#define LMD_INPUT_TYPE_STREAM  (INPUT_TYPE_LAST+1)
#define LMD_INPUT_TYPE_TRANS   (INPUT_TYPE_LAST+2)
#define LMD_INPUT_TYPE_EVENT   (INPUT_TYPE_LAST+3)

// After we have gotten all the records needed for an event (none yet
// released), but before we have investigated the subevent headers, we
// keep the information of where to get all the data by keeping
// pointers into the raw record data (which we now 'own', from first
// pointer to last, so that we can copy around there as we please)

// When we have all the fragments, and the code has decided that the
// event may be of interest, we'll locate all subevents and fetch
// their headers, and remember the locations of the data.  But we will
// NOT make sure that the data is in contigous memory.  That will only
// be done per subevent when the data is requested!

struct lmd_subevent
{
  lmd_subevent_10_1_host _header;

  char      *_data;   // ptr to data in contigous memory
  buf_chunk *_frag;   // where data is (fragmented)
  size_t     _offset; // offset into fragment
  // size_t     _length;
};

#define LMD_EVENT_GET_10_1_INFO_ATTEMPT     0x0001
#define LMD_EVENT_HAS_10_1_INFO             0x0002
#define LMD_EVENT_IS_STICKY                 0x0004
#define LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT  0x0008
#define LMD_EVENT_SUBEVENTS_LEFTOVERS       0x0010 // could not read subevents to end...
#define LMD_EVENT_FIRST_BUFFER_HAS_STICKY   0x0020

// This structure holds information to hint the unpacking at reasonable
// allocation sizes.  Since also the preunpacking is within separate
// threads, this should be thread-local.
struct lmd_event_hint
{
public:
  lmd_event_hint()
  {
    _max_subevents = 1;
    _max_subevents_since_decrease = 0;
    _hist_req_realloc = 0;
  }

public:
  int _max_subevents;
  int _max_subevents_since_decrease;
  uint _hist_req_realloc;
};

struct lmd_event
  : public input_event
{
  lmd_event_10_1_host _header;

  /*
  // The data
  char      *_data;
  // If it was subdivided, and no data pointer gotten yet
  buf_chunk  _chunks[2];
  // Hmm, can this be handled more elegantly?
  */

  int        _status;

  bool       _swapping;

  /* Whenever someone has gone through the trouble to locate the
   * subevents (all _will_ be done at once, we get a pointer to that
   * array of information.
   */

  lmd_subevent *_subevents;
  int           _nsubevents; // not valid until _subevents has been set

  /* Since lmd events may be spanning many buffers (for us, have
   * multiple chunks, we must allow for that.
   */

  buf_chunk *_chunk_end;   // the current end of chunks
  buf_chunk *_chunk_alloc; // how many are allocated?

  buf_chunk *_chunk_cur;   // the current use end of chunks
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

  buf_chunk *_chunks_ptr;
  
#ifdef USE_INPUTFILTER
  bool _aida_extra;
  bool _aida_implant;
  int64_t _aida_length;
#endif

public:
  void get_10_1_info();
  void locate_subevents(lmd_event_hint *hints);
  void get_subevent_data_src(lmd_subevent *subevent_info,
			     char *&start,char *&end);
  void print_event(int data,hex_dump_mark_buf *unpack_fail) const;
  bool is_sticky() const { return _status & LMD_EVENT_IS_STICKY; }
  bool is_subevent_sticky_revoke(lmd_subevent *subevent_info) const
  {
    return ((_status & LMD_EVENT_IS_STICKY) &&
	    subevent_info->_header._header.l_dlen ==
	    LMD_SUBEVENT_STICKY_DLEN_REVOKE);
  }
};

#define FILE_INPUT_EVENT lmd_event

#if !USE_THREADING && !USE_MERGING
extern FILE_INPUT_EVENT _file_event;
#endif

class lmd_source :
  public data_input_source
{
public:
  lmd_source();
  ~lmd_source() { }

public:
  void new_file();

public:
  s_bufhe_host       _buffer_header;
  bool               _swapping;
  uint32_t           _last_buffer_no;
  bool               _expect_file_header;
  uint               _events_left;

  bool               _skip_record_try_again;
  off_t              _prev_record_release_to;
  off_t              _prev_record_release_to2;

  bool               _buffers_maybe_missing;
  bool               _close_is_error;

  s_filhe_extra_host _file_header;
  bool               _file_header_seen;

  int                _first_buf_status;

public:
  buf_chunk  _chunks[2];
  buf_chunk *_chunk_cur;
  buf_chunk *_chunk_end;

public:
  void release_events();

  virtual lmd_event *get_event(/*pax_event *dest*/);

#if USE_MERGING
  lmd_event _file_event;
#endif

public:
  void print_file_header(const s_filhe_extra_host *header);
  void print_buffer_header(const s_bufhe_host *header);

public:
  bool read_record(bool expect_fragment = false);
  bool skip_record();

};

#endif//USE_LMD_INPUT

#endif//__LMD_INPUT_HH__
