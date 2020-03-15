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

#ifndef __LMD_OUTPUT_HH__
#define __LMD_OUTPUT_HH__

#ifdef USE_LMD_INPUT

#include "forked_child.hh"

#include "lmd_input.hh"
#include "lmd_sticky_store.hh"

#include "limit_file_size.hh"

#include "select_event.hh"

#include "logfile.hh"

struct buf_chunk_swap
{
  const char *_ptr;      // could be void*, char* to allow arithmetics
  size_t      _length;
  bool        _swapping; // true if chunk not in host byte order
};

struct free_ptr
{
  char *_ptr;
  free_ptr *_next;
};

class lmd_event_out
{
public:
  lmd_event_out();
  ~lmd_event_out();

public:
  lmd_event_header_host _header;

  lmd_event_info_host   _info; // will be pointed to by the first chunk

  // note: first chunk starts with the second part of the header,
  // lmd_event_info_host

  buf_chunk_swap *_chunk_start;
  buf_chunk_swap *_chunk_end;
  buf_chunk_swap *_chunk_alloc;

  char *_buf_start;
  char *_buf_end;
  char *_buf_alloc;

  free_ptr *_free_buf;

protected:
  void realloc_chunks();
  void realloc_buf(size_t more);

public:
  void add_chunk(const void *ptr,size_t len,bool swapping);

  void clear();
  bool is_clear() const { return _chunk_end == _chunk_start; }
  bool has_subevents() const;

  size_t get_length() const;

  void write(void *dest) const;

  void dump_debug();

public:
  void copy_header(const lmd_event *event,
		   bool combine_event);
  void copy_all(const lmd_event *event,
		bool combine_event);
  void copy(const lmd_event *event,
	    const select_event *select,
	    bool combine_event,
	    bool emit_header = true);
};

void copy_to_buffer(void *dest, const void *src,
		    size_t length, bool swap_32);

class lmd_output_buffered
{
public:
  lmd_output_buffered();
  virtual ~lmd_output_buffered() { }


public:
  virtual void write_event(const lmd_event_out *event,
			   bool sticky_replay = false,
			   bool discard_revoke = false);
  virtual void event_no_seen(sint32 eventno) { }

  virtual void set_file_header(const s_filhe_extra_host *file_header_extra,
			       const char *add_comment) { }

  virtual void close() = 0;

  void new_buffer(size_t lie_about_used_when_large_dlen = 0);
  void send_buffer(size_t lie_about_used_when_large_dlen = 0);
  void mark_close_buffer();

  virtual void print_status(double elapsed) = 0;

protected:
  virtual void write_buffer(size_t count, bool has_sticky) = 0;
  virtual void get_buffer() = 0;
  virtual bool flush_buffer() { return false; }

  virtual bool do_sticky_replay() { return false; }
  virtual void mark_replay_stream(bool replay) { UNUSED(replay); }

protected:
  void copy_to_buffer(const void *src,size_t length,bool swap_32);

protected:
  s_bufhe_host       _buffer_header;

  uint8*             _cur_buf_start;
  size_t             _cur_buf_length;

  uint8*             _cur_buf_ptr;
  size_t             _cur_buf_left;
  size_t             _cur_buf_usable;

  size_t             _stream_left;
  size_t             _stream_left_max;

public:
  bool               _write_native;
  bool               _compact_buffers;

public:
  select_event       _select;

public:
  uint32_t           _last_bufno;

public:
  uint32_t           _replays;
  uint32_t           _last_replays;

protected:
  lmd_sticky_store   _sticky_store;
};

void lmd_out_common_options();

bool parse_lmd_out_common(char *request,
			  bool allow_selections,
			  bool allow_compact,
			  lmd_output_buffered *out);

class lmd_output_file :
  public lmd_output_buffered,
  public limit_file_size
{
public:
  lmd_output_file();
  virtual ~lmd_output_file();

public:
  void open_stdout();

  void open_file(const char* filename);
  void close_file();

  virtual void new_file(const char *filename);

public:
  virtual void write_event(const lmd_event_out *event,
			   bool sticky_replay = false,
			   bool discard_revoke = false);
  virtual void event_no_seen(sint32 eventno);

  virtual void set_file_header(const s_filhe_extra_host *file_header_extra,
			       const char *add_comment);

  virtual void close();

  virtual void print_status(double elapsed);

public:
  void write_file_header(const s_filhe_extra_host *file_header_extra);

  void report_open_close(bool open);

protected:
  virtual void write_buffer(size_t count, bool has_sticky);
  virtual void get_buffer();

public:
  int                _fd_handle;
  int                _fd_write;

  char*              _cur_filename;
  logfile*           _lmd_log;

public:
  s_filhe_extra_host _file_header_extra;
  bool               _has_file_header;

public:
  uint32             _event_cut;
  uint32             _last_eventno;

  uint32             _buf_size;

  bool               _write_protect;

public:
  uint64_t           _total_written;
  uint64_t           _last_written;

public:
  // When compressing the output on the fly
  forked_child       _compressor;
  uint32             _compression_level;
};

void lmd_out_common_options();

lmd_output_file *parse_open_lmd_file(const char *,
				     bool allow_selections = true);

#endif//USE_LMD_INPUT

#endif//__LMD_OUTPUT_HH__

