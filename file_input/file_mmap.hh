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

#ifndef __FILE_MMAP_HH__
#define __FILE_MMAP_HH__

#include "sig_mmap.hh"

#include "reclaim.hh"

#include "input_buffer.hh"
#include "thread_buffer.hh"

class file_mmap;

#ifdef USE_THREADING
struct fmm_reclaim
  : public tb_reclaim
{
  file_mmap *_mm;
  off_t      _end;
};
#endif

struct file_mmap_chunk
{
  // What parts of the file do we map
  off_t _start;
  off_t _end;
  // Where is our mapping
  char *_base;
  // Neighbour chunk
  file_mmap_chunk *_prev;
  file_mmap_chunk *_next;
  // Info for the SIGBUS catcher
  sig_mmap_info _mmap_info;
};

class file_mmap
  : public input_buffer
{
public:
  file_mmap();
  virtual ~file_mmap();

public:
  int _fd;

public:
  size_t _page_mask;

public:
  volatile off_t _avail;
  volatile off_t _done;

  // These two members are for monitoring (only) (optimize away?)
  off_t _front; // how far have we read
  off_t _back;  // how far have we released?

  bool _last_has_eof;

public:
  file_mmap_chunk         _ends;
  /*static*/ file_mmap_chunk *_free;

public:
  void init(int fd);
  virtual void close();

public:
  virtual int map_range(off_t start,off_t end,buf_chunk chunks[2]);
  virtual void release_to(off_t end);

#ifdef USE_THREADING
  virtual void arrange_release_to(off_t end);
#endif

public:
  virtual size_t buffer_size();
  virtual size_t max_item_length();

  void consistency_check();
};

#endif//__FILE_MMAP_HH
