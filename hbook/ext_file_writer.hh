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

#ifndef __EXT_FILE_WRITER_HH__
#define __EXT_FILE_WRITER_HH__

#include "ext_data_proto.h"

struct external_writer_shm_control
{
  union
  {
    struct
    {
      uint32_t        _magic;
      volatile size_t _len;
    };
    char   dummy1[64]; // align the next items
  };
  union // written by producer
  {
    struct
    {
      volatile size_t _avail;
      volatile size_t _need_producer_wakeup;
      volatile size_t _wakeup_done;
    };
    char   dummy2[64]; // get it in its own cache line
  };
  union // written by consumer
  {
    struct
    {
      volatile size_t _done;
      volatile size_t _need_consumer_wakeup;
      volatile size_t _wakeup_avail;
    };
    char   dummy3[64]; // get it in its own cache line
  };
};

#ifdef DO_EXT_NET_DECL

struct shared_mem_circular;

struct ext_write_config_comm
{
  const char *_input;
  int         _shm_fd;
  int         _pipe_in;
  int         _pipe_out;
  int         _server_fd;

  shared_mem_circular *_shmc;    // The associated memory

  ext_write_config_comm *_next;

  uint32_t   *_sort_u32_raw; // For next event in buffer
  uint32_t    _keep_alive_event;

  uint32_t   *_raw_ptr;
  uint32_t    _raw_words;
};

struct ext_write_config
{
  ext_write_config_comm *_comms;
  bool        _forked;

#if USING_CERNLIB || USING_ROOT
  const char *_outfile;
  const char *_infile;
  const char *_ftitle;
  const char *_id;
  const char *_title;

  int _timeslice;
  int _timeslice_subdir;
#endif
#if USING_ROOT
  int _autosave;
#endif

#ifdef STRUCT_WRITER
  const char *_insrc;
  const char *_header;
  const char *_header_id;
  const char *_header_id_orig;
  int         _debug_header;
  int         _port;
  int         _stdout;

#define EXT_WRITER_DUMP_FORMAT_NORMAL 1
#define EXT_WRITER_DUMP_FORMAT_COMPACT_JSON   2
#define EXT_WRITER_DUMP_FORMAT_HUMAN_JSON   3    
  int         _dump;
#endif
};

extern ext_write_config _config;

struct ext_file_net_stat
{
  uint64_t _committed_size;
  uint64_t _sent_size;
  uint32_t _cur_clients;
};

extern ext_file_net_stat _net_stat;

void ext_net_io_server_bind(int port);

bool ext_net_io_select_clients(int read_fd,int write_fd,
			       bool shutdown,bool zero_timeout);

struct send_item_chunk;

char *ext_net_io_reserve_chunk(size_t length,bool init_chunk,
			       send_item_chunk **chunk);

void ext_net_io_commit_chunk(size_t length,send_item_chunk *chunk);

void ext_net_io_server_close();

#endif/*DO_EXT_NET_DECL*/

#endif/*__EXT_FILE_WRITER_HH__*/
