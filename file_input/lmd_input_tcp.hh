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

#ifndef __LMD_INPUT_TCP_HH__
#define __LMD_INPUT_TCP_HH__

#include "typedef.hh"
#include "endian.hh"

#include <stdlib.h>
#include <stdint.h>

#define LMD_TCP_PORT_TRANS          6000
#define LMD_TCP_PORT_STREAM         6002
#define LMD_TCP_PORT_EVENT          6003

#define LMD_TCP_PORT_TRANS_MAP_ADD  1234

#define LMD_TCP_INFO_BUFSIZE_NODATA     -1
#define LMD_TCP_INFO_BUFSIZE_MAXCLIENTS -2

#define LMD_PORT_MAP_MARK         0x50540000
#define LMD_PORT_MAP_MARK_MASK    0xffff0000
#define LMD_PORT_MAP_PORT_MASK    0x0000ffff

struct ltcp_filter_opcode
{
#if __BYTE_ORDER == __BIG_ENDIAN
  uint8   length;        // length of filter
  uint8   next_filter_block; //
  uint8   filter_spec;
  uint8   link_f2   : 1;
  uint8   link_f1   : 1; // 1=and 0=or
  uint8   opc       : 3;
  uint8   selwrite  : 1; // select write of event/subevent
  uint8   selfilter : 1; // select filter of event/subevent
  uint8   ev_subev  : 1; // 1:event 0:subev active for selection
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint8   ev_subev  : 1; // 1:event 0:subev active for selection
  uint8   selfilter : 1; // select filter of event/subevent
  uint8   selwrite  : 1; // select write of event/subevent
  uint8   opc       : 3;
  uint8   link_f1   : 1; // 1=and 0=or
  uint8   link_f2   : 1;
  uint8   filter_spec;
  uint8   next_filter_block; //
  uint8   length;        // length of filter
#endif
};

struct ltcp_filter_item
{
  uint32  pattern;
  sint32   offset;
  ltcp_filter_opcode opcode;
};

#define LTCP_MAX_FILTER_ITEMS   32

struct ltcp_base_filter_struct
{
  uint32  testbit;       // 0x00000001
  uint32  endian;        // 0x0 for little endian,
                         // 0xffffffff for big endian (stupid!!)
  sint32  numev;         // number of events to send (-1 for many...)
  sint32  sample;        // downscale factor
  sint32  flush;         // flush rate (seconds) (default: 3 FLUSH__TIME)
  ltcp_filter_item items[LTCP_MAX_FILTER_ITEMS];
};

#define LTCP_MAX_FILTER_DESCR 16

/*
// This structure is not swapping resistant...
struct ltcp_filter_descr
{
  uint8   write_descr;
  uint8   filter_descr;
  uint8   filter_block_begin; // index into filter structure
  uint8   filter_block_end;   // index into filter structure
  uint8   next_descr;         // index into filter_descr structure
  uint8   dummy;
  uint16  num_descriptors;
};
*/

/*
// This structure is not swapping resistant...
struct ltcp_client_filter_struct
{
  ltcp_base_filter_struct base;

  ltcp_filter_descr descr[LTCP_MAX_FILTER_DESCR];
  uint16  filter_ev;     // filter on event
  uint16  filter_subev;  // filter on subevent
  uint16  write_ev;      // write whole event
  uint16  write_subev;   // write subevents
};
*/

struct ltcp_event_client_struct
{
  uint32  testbit;
  uint32  endian;        // sender endian

  sint32  dlen;          // data length (bytes)
  sint32  free;          // free length (bytes)
  sint32  num_events;    // number of events in buffer
  sint32  max_buf_size;  // maximum buffer size
  uint32  send_bytes;    // bytes sent (changed to uint32 for program safety)
  sint32  send_buffers;  // number of buffers to send
  sint32  con_clients;   // currently connect(ed?) clients
  uint32  buffer_type;   // 1:data 2:message 4:flush 8:last 16:first (inclusive/or)
  uint32  message_type;  // 1:info 2:warn 4:error 8:fatal(?)

  char    message[256];

  uint32  read_count;    // read buffers
  uint32  read_count_ok; // good read buffers
  uint32  skip_count;    // skipped buffers
  uint32  byte_read_count; // read bytes
  uint32  count_bufs;    // processed buffers
  uint32  count_events;  // processed events

  uint32  sent_events;
  uint32  sent_bytes;
  uint32  send_buffer;
  uint32  con_proc_events; // processed events since connect
  uint32  filter_matches;  // filter matched events

  // char        output_buffer[16384] // data(?)
};

struct ltcp_event_client_ack_struct
{
  uint32  buffers;         // same as send_buffers
  uint32  bytes;           // same as send_bytes
  uint32  client_status;   // 1=success#
};

struct ltcp_stream_trans_open_info
{
  uint32   testbit;
  uint32   bufsize;
  uint32   bufs_per_stream;
  uint32   streams;
};




class lmd_input_tcp
{
public:
  lmd_input_tcp();
  virtual ~lmd_input_tcp();

public:
  int _fd;

protected:
  bool parse_connection(const char *server,
			struct sockaddr_in *p_serv_addr,
			uint16_t *port,
			uint16_t default_port);
  bool open_connection(const struct sockaddr_in *p_serv_addr,
		       uint16_t port, bool error_on_failure);
  void close_connection();

  void do_read(void *buf,size_t count,int timeout = -1);
  void do_write(void *buf,size_t count,int timeout = -1);

public:
  virtual size_t connect(const char *server) = 0;
  virtual void close() = 0;

  virtual size_t get_buffer(void *buf,size_t count) = 0;

  virtual size_t preferred_min_buffer_size() = 0;

public:
  void create_dummy_buffer(void *buf, size_t count,
			   int buffer_no, bool mark_dummy);
};


class lmd_input_tcp_buffer :
  public lmd_input_tcp
{
public:
  ltcp_stream_trans_open_info _info;

protected:
  size_t read_info(int *data_port);

  size_t read_buffer(void *buf,size_t count,int *nbufs);

protected:
  size_t do_map_connect(const char *server,
			int port_map_add, uint16_t default_port);

};


class lmd_input_tcp_transport :
  public lmd_input_tcp_buffer
{
public:
  virtual ~lmd_input_tcp_transport() { }

public:
  virtual size_t connect(const char *server);
  virtual void close();

  virtual size_t get_buffer(void *buf,size_t count);

  virtual size_t preferred_min_buffer_size();
};


class lmd_input_tcp_stream :
  public lmd_input_tcp_buffer
{
public:
  lmd_input_tcp_stream();
  virtual ~lmd_input_tcp_stream() { }

public:
  int _buffers_to_read;

public:
  virtual size_t connect(const char *server);
  virtual void close();

  virtual size_t get_buffer(void *buf,size_t count);

  virtual size_t preferred_min_buffer_size();
};


class lmd_input_tcp_event :
  public lmd_input_tcp
{
public:
  lmd_input_tcp_event();
  virtual ~lmd_input_tcp_event() { }

public:
  ltcp_event_client_struct _header;

  size_t _bytes_left;
  size_t _data_left;

  uint32 _buffer_no;

  bool   _swapping;

protected:
  size_t read_client_header();
  void send_acknowledge();

public:
  virtual size_t connect(const char *server);
  virtual void close();

  virtual size_t get_buffer(void *buf,size_t count);

  virtual size_t preferred_min_buffer_size();
};


#endif//__LMD_INPUT_TCP_HH__
