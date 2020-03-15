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

#ifndef __DECOMPRESS_HH__
#define __DECOMPRESS_HH__

#include <unistd.h>

#include "forked_child.hh"

#include "input_buffer.hh"

#define INPUT_TYPE_FILE      1
#define INPUT_TYPE_FILE_SRM  2
#define INPUT_TYPE_RFIO      3
#define INPUT_TYPE_EXTERNAL  4 // ntuple / struct
#define INPUT_TYPE_LAST      (INPUT_TYPE_EXTERNAL)

//#ifdef USE_PTHREAD
//#include "pthread_buffer.hh"
//#endif

class decompressor
{
public:
  decompressor();
  ~decompressor();

public:
  forked_child _fork;

  // For buffering

  //#ifdef USE_PTHREAD
  //  pthread_buffer _buffer;
  //#endif

public:
  void start(int *fd_in,
	     const char *method,
	     const char *const *args,
	     const char *filename,
	     int *fd_out,
	     int fd_src);

  // void join();
  void reap();

};

#define PEEK_MAGIC_BYTES 4

struct drt_info
{
  int _fd_read;
  int _fd_write;

  unsigned char _magic[PEEK_MAGIC_BYTES];

  pthread_t _thread;
};

class data_input_source
{
public:
  data_input_source();
  ~data_input_source()
  {
    assert(!_input._input); // want to get rid of the close below
    close();
  }

public:
  input_buffer_cont  _input;
  forked_child      *_remote_cat;
  decompressor      *_decompressor;

  drt_info          *_relay_info;

public:
  void open(const char *filename
#ifdef USE_PTHREAD
	    ,thread_block *block_reader
#endif
#ifdef USE_THREADING
	    ,thread_block *blocked_next_file
	    ,int           wakeup_next_file
#endif
	    ,bool no_mmap
	    ,bool rfioinput
	    ,bool srminput);
#if 0 /* unused, see comment in decompress.cc */
  void open_rfio(const char *filename
#ifdef USE_PTHREAD
	    ,thread_block *block_reader
#endif
#ifdef USE_THREADING
	    ,thread_block *blocked_next_file
	    ,int           wakeup_next_file
#endif
	    );
#endif
  void connect(const char *name,int type
#ifdef USE_PTHREAD
	       ,thread_block *block_reader
#endif
#ifdef USE_THREADING
	       ,thread_block *blocked_next_file
	       ,int           wakeup_next_file
#endif
	       );
  void close();

public:
  void take_over(data_input_source &src);

};

void decompress(decompressor **handler,
		drt_info     **relay_info,
		int *fd,
		const char *filename,
		unsigned char *push_magic,size_t *push_magic_len);

ssize_t retry_read(int fd,void *buf,size_t count);

size_t full_read(int fd,void *buf,size_t count,bool eof_at_start = true);

#endif//__DECOMPRESS_HH__
