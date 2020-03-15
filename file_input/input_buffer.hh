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

#ifndef __INPUT_BUFFER_HH__
#define __INPUT_BUFFER_HH__

#include "thread_block.hh"

#include "error.hh"

#include "optimise.hh"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

struct buf_chunk
{
  char   *_ptr;    // could be void*, char* to allow arithmetics
  size_t  _length;
};

// Copy the chunk info for one or two chunks, and return the number of
// bytes retrieved.  we are _not_ moving src, since it will be
// overwritten!

inline size_t copy_range(buf_chunk *&dest,
			 buf_chunk *src,buf_chunk *end)
{
  if (src >= end)
    return 0;

  buf_chunk *next = src + 1;

  if (next >= end)
    {
      // only one to copy

      *(dest++) = src[0];
      return src[0]._length;
    }

  assert (next+1 == end);

  *(dest++) = src[0];
  *(dest++) = src[1];

  return src[0]._length + src[1]._length;
}

// Get data from one or two chunks into a buffer, chunks are
// updated/moved to reflect that data was used

inline bool get_range(char *dest,
		      buf_chunk *&src,buf_chunk *end,size_t length)
{
  // First copy as many bytes as we have in the first chunk

  if (src >= end)
    return false;

  if (LIKELY(length < src->_length))
    {
      memcpy(dest,src->_ptr,length);
      src->_ptr    += length;
      src->_length -= length;
      return true;
    }

  memcpy(dest,src->_ptr,src->_length);
  dest += src->_length;
  size_t remain = length - src->_length;
  src++;

  if (!remain)
    return true;

  if (src >= end)
    return false;

  if (UNLIKELY(remain > src->_length))
    return false;

  memcpy(dest,src->_ptr,remain);
  src->_ptr    += remain;
  src->_length -= remain;

  return true;
}

// Get data from one or several chunks into a buffer, src chunks are
// _not_ updated/moved to reflect that data was used, but the src
// pointers are updated

inline bool get_range_many(char *dest,
			   buf_chunk *&src,size_t &offset,
			   buf_chunk *end,size_t length)
{
  // First copy as many bytes as we have in the first chunk

  if (src >= end)
    return false;

  if (LIKELY(offset+length < src->_length))
    {
      memcpy(dest,src->_ptr + offset,length);
      offset += length;
      return true;
    }

  // Data was not available in the first chunk (completely)
  // copy whatever data was available

  size_t avail = src->_length - offset;

  memcpy(dest,src->_ptr + offset,avail);
  dest += avail;
  offset = 0;
  length -= avail;
  src++;

  if (UNLIKELY(!length))
    return true; // we got all that we want

  if (UNLIKELY(src >= end))
    return false; // no more chunks, and we did not get all

  while (UNLIKELY(length >= src->_length))
    {
      // We still want more data than chunk holds.  get it entirely

      memcpy(dest,src->_ptr,src->_length);
      dest += src->_length;
      length -= src->_length;
      src++;

      if (UNLIKELY(!length))
	return true; // we got all that we want

      if (UNLIKELY(src >= end))
	return false; // no more chunks, and we did not get all
    }

  // So, rest data is available in the last chunk (and it wont eat up
  // till the end)

  memcpy(dest,src->_ptr,length);
  offset = length;

  return true;
}

// Get data from one or two chunks into a buffer, if the source data
// is continous, or two chunks, src chunks are updated/moved to reflect
// that data was used

inline bool get_range(buf_chunk dest[2],char **dest_continous,
		      buf_chunk *&src,buf_chunk *end,size_t length)
{
  // First copy as many bytes as we have in the first chunk

  if (src >= end)
    return false;

  //printf ("get_range: %d : %d\n",src->_length,length);
  //fflush(stdout);

  if (LIKELY(length < src->_length))
    {
      *dest_continous = src->_ptr;
      dest[0]._length = length;
      src->_ptr    += length;
      src->_length -= length;
      return true;
    }

  dest[0]._ptr    = src->_ptr;
  dest[0]._length = src->_length;

  size_t remain = length - src->_length;
  src++;

  //printf ("get_range: %d : %d | %d\n",src->_length,length,remain);
  //fflush(stdout);

  if (UNLIKELY(!remain))
    {
      // We actually delivered all data in the first chunk
      *dest_continous = dest[0]._ptr;
      return true;
    }

  *dest_continous = NULL;

  if (src >= end)
    return false;

  if (UNLIKELY(remain > src->_length))
    return false;

  dest[1]._ptr    = src->_ptr;
  dest[1]._length = remain;

  if (UNLIKELY(remain == src->_length))
    {
      // We exactly exhausted the last chunk
      src++;
      return true;
    }

  src->_ptr    += remain;
  src->_length -= remain;

  return true;
}

// Get data from one or two chunks into one or two chunk, chunks are
// updated/moved to reflect that data was used

inline bool get_range(buf_chunk dest[2],buf_chunk **dest_end,
		      buf_chunk *&src,buf_chunk *end,size_t length)
{
  // First copy as many bytes as we have in the first chunk

  if (src >= end)
    return false;

  dest[0]._ptr    = src->_ptr;

  if (LIKELY(length < src->_length))
    {
      dest[0]._length = length;
      src->_ptr    += length;
      src->_length -= length;
      *dest_end = &dest[1];
      return true;
    }

  dest[0]._length = src->_length;

  size_t remain = length - src->_length;
  src++;

  if (UNLIKELY(!remain))
    {
      // We actually delivered all data in the first chunk
      *dest_end = &dest[1];
      return true;
    }

  *dest_end = &dest[0]; // for safety

  if (src >= end)
    return false;

  if (UNLIKELY(remain > src->_length))
    return false;

  dest[1]._ptr    = src->_ptr;
  dest[1]._length = remain;

  *dest_end = &dest[2];

  if (UNLIKELY(remain == src->_length))
    {
      // We exactly exhausted the last chunk
      src++;
      return true;
    }

  src->_ptr    += remain;
  src->_length -= remain;

  return true;
}

inline bool skip_range(buf_chunk *&src,buf_chunk *end,size_t length)
{
  // First copy as many bytes as we have in the first chunk

  if (src >= end)
    return false;

  if (LIKELY(length < src->_length))
    {
      src->_ptr    += length;
      src->_length -= length;
      return true;
    }

  size_t remain = length - src->_length;
  src++;

  if (!remain)
    return true;

  if (src >= end)
    return false;

  if (UNLIKELY(remain > src->_length))
    return false;

  src->_ptr    += remain;
  src->_length -= remain;

  return true;
}


class input_buffer
{
public:
  input_buffer();
  virtual ~input_buffer();

public:
#ifdef USE_CURSES
  // Only used for diagnostics (--progress)
  const char *_filename;
#endif
  void set_filename(const char *filename);


public:
#ifdef USE_THREADING
  thread_block *_blocked_next_file;
  int           _wakeup_next_file;

  void request_next_file();
  void set_next_file(thread_block *blocked_next_file,
		     int           wakeup_next_file);
#endif

public:
  virtual void close() = 0;

public:
  virtual int map_range(off_t start,off_t end,buf_chunk chunks[2]) = 0;
  virtual void release_to(off_t end) = 0;

#ifdef USE_THREADING
  virtual void arrange_release_to(off_t end) = 0;
#endif

public:
  virtual size_t buffer_size() = 0;
  virtual size_t max_item_length() = 0;

public:
  bool read_range(void *dest,off_t start,size_t length);

};

// This class encapsulates an continous input file.  Note, after an
// error (marked by either throwing an error, or failure to read data,
// one must not read more, since _cur has been speculatively moved
// forward!  (optimisation)

class input_buffer_cont
{
public:
  input_buffer_cont()
  {
    _input = NULL;
    _cur   = -1;
  }

public:
  input_buffer *_input;
  off_t         _cur;

public:
  int map_range(size_t length,buf_chunk chunks[2])
  {
    off_t start = _cur;
    _cur += length;
    return _input->map_range(start,_cur,chunks);
  }

  int read_range(void *dest,size_t length)
  {
    off_t start = _cur;
    _cur += length;
    return _input->read_range(dest,start,length);
  }

  void release_to(off_t end)
  {
#ifdef USE_THREADING
    _input->arrange_release_to(end);
#else
    _input->release_to(end);
#endif
  }

public:
  void close();
  void destroy();

  void take_over(input_buffer_cont &src);

};

void print_remaining_event(buf_chunk *chunk_cur,
			   buf_chunk *chunk_end,
			   size_t offset_cur,
			   bool swapping);

#endif//__INPUT_BUFFER_HH__
