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

#ifndef __LMD_OUTPUT_TCP_HH__
#define __LMD_OUTPUT_TCP_HH__

#ifdef USE_LMD_OUTPUT_TCP
#ifndef USE_PTHREAD
#error pthreads needed
#endif

#include <netinet/in.h>

#include "typedef.hh"
#include "lmd_event.hh"

#include "lmd_output.hh"

#include "thread_block.hh"

#define LMD_OUTPUT_DEFAULT_BUFFER_SIZE    0x80000
#define LMD_OUTPUT_DEFAULT_BUF_PER_STREAM      8 // each chunk is 8x512k=4M

#define LMD_OUTPUT_DEFAULT_MAX_BUF      0x800000 // 8 MB of buffer
#define LMD_OUTPUT_MAX_CLIENTS                40

#define LMD_OUTPUT_FREE_STREAMS               16
#define LMD_OUTPUT_FILLED_STREAMS              8

/*
struct lmd_output_buffer
{
  s_bufhe_host _buffer_header;
  char         _data[LMD_OUTPUT_BUFFER_SIZE - sizeof(s_bufhe_host)];
};
*/

// Stream is first sticky event replay part:
#define LOS_FLAGS_STICKY_RECOVERY_FIRST     0x01
// Stream is (part of) sticky event replay:
#define LOS_FLAGS_STICKY_RECOVERY_MORE      0x02
#define LOS_FLAGS_STICKY_RECOVERY_MASK     (LOS_FLAGS_STICKY_RECOVERY_FIRST | \
					    LOS_FLAGS_STICKY_RECOVERY_MORE)
// This stream contains sticky events
#define LOS_FLAGS_HAS_STICKY_EVENT          0x04
// If a client is about to go beyond this stream, them it will
// loose sticky events, so it needs to find a recovery stream. 
#define LOS_FLAGS_STICKY_LOST_AFTER         0x08

struct lmd_output_stream
{
  lmd_output_stream *_next;
  lmd_output_stream *_prev;

  int    _alloc_stream_no; // only for debugging, used to keep track...

  int    _flags;
  size_t _filled;   // only increased by the filling routine
  size_t _max_fill; // more than this will never be filled (may only
		    // decrease while active filling)

  int    _clients;  // number of clients sending from this buffer

  // lmd_output_buffer _buffers[LMD_OUTPUT_BUF_PER_STREAM];
  char  *_bufs;
};

struct lmd_output_state
{
  // To keep the zero-copy approach as far as possible, and support
  // several reader processes at the same time, with varying amounts
  // of backlog, we operate (a la MBS) with a set of streams.  Each
  // stream consist of several buffers, but this is just a packaging
  // detail.  The streams are what is managed.

  // We have a pool of streams.  All incoming data is added (filled)
  // into the next stream available.  When that one has become full
  // (or a flush is forced somehow) it's added to the end of streams
  // that are to be sent to the clients.

  // Each client start (either at the last stream available, or with
  // the oldest one (that still has a continous path to the newest
  // one, plus 1 for margin) and sends data from there.  (we assume
  // the clients to be faster than the DAQ, but this is not necessary).

  // A client then marks the stream it serves from as active (data is
  // being sent from there).  This stream is then not allowed to be
  // reused.  The client will be given data from this stream until
  // it's used up, at which time it selects the next stream in the
  // queue.

  // If a client does not eat data fast enough, it may be that the
  // stream it holds is the oldest on the list.  If this is the case,
  // then it is not allowed to reuse that one, and rather the next
  // oldest will be reused.  When the client eventually resumes data
  // reading, it will then no longer have access to the one that was
  // already reused and a jump will occur (it will not get all data).



  // All sending will be operated by a separate thread, which handle
  // all client connections (via select).  We can at most accept as
  // many clients that we have, minus 2 streams.  This is in case they
  // all become stuck on (different) streams that then (temporarily)
  // become unavailable for.  The 2 streams are needed for the
  // progress of the stream filling.


  // Another limit is of course the available bandwidth to the
  // outside.  We'll also implement a strangulation method such, that
  // when the bandwidth used (connected client x data flow) becomes
  // too large, then it will start to skip streams for all (non-local)
  // clients.  The skip ratio will be adjusted dynamically.  (the
  // effect will be that the clients faster reach the newest buffer,
  // and once there, they cannot send data until more becomes
  // available).


  lmd_output_stream *_stream_first; // oldest stream
  lmd_output_stream *_stream_last;  // newest stream

  int _num_streams; // number of streams in use
  int _max_streams;

  // A circular buffer of free streams (only consumed by the producer
  // (buffer filler), only added by the server thread

  lmd_output_stream *volatile _free_streams[LMD_OUTPUT_FREE_STREAMS];

  volatile int _free_streams_avail;
  volatile int _free_streams_used;

  // A circular buffer of streams that have been filled by the producer

  lmd_output_stream *volatile _filled_streams[LMD_OUTPUT_FILLED_STREAMS];

  volatile int _filled_streams_avail;
  volatile int _filled_streams_used;

  // The stream currently being filled

  lmd_output_stream *_fill_stream;

  // lmd_output_stream _streams[LMD_OUTPUT_STREAMS];

  // Each buffer (stream) is only sent to one receiver, useful for
  // fanout to multiple processing instances.
  bool _sendonce;

public:
  uint32 _buf_size;
  uint32 _stream_bufs;

public:
  lmd_output_state();
  ~lmd_output_state();

public:
  lmd_output_stream *get_free_stream(); // used by the buffer filler
  void add_free_stream(lmd_output_stream *stream); // from the server thread

  void unlink_stream(lmd_output_stream *stream);

  bool create_free_stream();

  void free_oldest_unused();

  lmd_output_stream *deque_filled_stream();

public:
  void free_client_stream(lmd_output_stream *stream);

  lmd_output_stream *get_next_client_stream(lmd_output_stream *stream,
					    int *need_recovery_stream);

  lmd_output_stream *get_last_client_stream();

  void add_client_stream(lmd_output_stream *stream);

  int streams_to_send();

public:
  void dump_state();

};


#define LMD_OUTPUT_STREAM_SERVER  1
#define LMD_OUTPUT_TRANS_SERVER   2

#define LOCC_STATE_SEND_INFO      1  // send initial info to client
#define LOCC_STATE_REQUEST_WAIT   2  // waiting for request
#define LOCC_STATE_STREAM_WAIT    3  // waiting for data in a stream
#define LOCC_STATE_BUFFER_WAIT    4  // waiting for data in a buffer
#define LOCC_STATE_SEND_WAIT      5
#define LOCC_STATE_CLOSE_WAIT     6  // wait (timeout) for othe end to close


class lmd_output_tcp;
class lmd_output_server_con;

class lmd_output_client_con
{
public:
  lmd_output_client_con(int fd, lmd_output_server_con *server_con,
			lmd_output_state *data);

public:
  int _fd;

  int _state;

  struct request
  {
    size_t _got;
    char   _msg[12+1]; // +1 for 0 termination
  } _request;

public:
  lmd_output_stream *_current; // current stream we send from
  size_t             _offset;  // how much has been sent

  int                _need_recovery_stream;
  lmd_output_stream *_pending; // stream to send from

  lmd_output_state  *_data;

  struct sockaddr_in _cliAddr;

  lmd_output_server_con *_server_con;

  struct timeval _close_beginwait;

protected:
  void next_stream(lmd_output_tcp *tcp_server);

public:
  int setup_select(int nfd,
		   fd_set *readfds,fd_set *writefds);

  bool after_select(fd_set *readfds,fd_set *writefds,
		    lmd_output_tcp *tcp_server);

public:
  bool stream_is_available(lmd_output_stream *stream,
			   bool sticky_events_seen,
			   lmd_output_tcp *tcp_server);

public:
  void close();

public:
  void dump_state();
};

class lmd_output_server_con
{
public:
  lmd_output_server_con();

public:
  void bind(int mode, int port, int port_range_last);

  void close();

public:
  int _socket;

public:
  int _mode;
  int _data_port;
  bool _allow_data;

public:
  int setup_select(int nfd,
		   fd_set *readfds,fd_set *writefds);

  bool after_select(fd_set *readfds,fd_set *writefds,
		    lmd_output_tcp *tcp_server);

public:
  void accept_server();

};


#include <vector>

typedef std::vector<lmd_output_client_con*> lmd_output_client_con_vect;
typedef std::vector<lmd_output_server_con*> lmd_output_server_con_vect;

#define LOT_TOKEN_NEED_FREE_STREAM              1 // producer -> server
#define LOT_TOKEN_HAVE_FREE_STREAM              2 // server -> producer

#define LOT_TOKEN_FILLED_STREAM_QUEUE_FULL      3 // producer -> server
#define LOT_TOKEN_FILLED_STREAM_QUEUE_SLOT_FREE 4 // server -> producer

#define LOT_TOKEN_FILLED_BUFFER                 5 // producer -> server
#define LOT_TOKEN_FILLED_STREAM                 6 // producer -> server

#define LOT_TOKEN_SHUTDOWN                      7 // master -> server

class lmd_output_tcp :
  public lmd_output_buffered
{
public:
  lmd_output_tcp()
  {
    _active = false;

    _tell_fill_stream = 0;
    _tell_fill_buffer = 0;
    _need_recovery_stream = 0;
    _sticky_events_seen = false;

    _mark_replay_stream = 0;

    _need_free_stream = 0;
    _shutdown_streams_to_send = 0;

    _next_search_client_i = 0;

    _hold = false;
    _state._sendonce = false;

    _flush_interval = 10; // flush every 10s by default
  }
  virtual ~lmd_output_tcp()
  {
    close();
  }

public:
  static void *server_thread(void *us);
  void *server();

public:
  void init();

public:
  lmd_output_server_con_vect _servers;
  lmd_output_client_con_vect _clients;

  lmd_output_state  _state;

public:
  // Communication between the data producer and the server itself
  volatile int  _tell_fill_stream;
  volatile int  _tell_fill_buffer;
  volatile int  _need_recovery_stream;
  volatile bool _sticky_events_seen;

public:
  pthread_t _thread;
  bool _active;

public:
  thread_block _block_server;
  thread_block _block_producer;

public:
  int _mark_replay_stream;
  
  bool _need_free_stream;

  size_t _next_search_client_i;

  int  _shutdown_streams_to_send;
  struct timeval _shutdown_lasttime;

  struct timeval _last_stream_enqueued;
  int  _flush_interval;

public:
  volatile uint64_t _total_sent;
  uint64_t _last_sent;

public:
  bool _hold; // no buffer should be discarded, all must be sent somewhere
  // no guarantee however, as we may not notice a client disconnecting
  // until some future buffer, i.e. a few streams may be sent into
  // the big vacuum when a client disconnects

protected:
  lmd_output_server_con *create_server(int mode, int port, int port_range_last,
				       bool allow_data);
public:
  void create_server(int mode, int port,
		     int dataport, int dataport_last,
		     bool allow_data_on_map_port);

protected:
  void send_last_buffer();

public:
  virtual void close();

  virtual void print_status(double elapsed);

protected:
  virtual void write_buffer(size_t count, bool has_sticky);
  virtual void get_buffer();
  virtual bool flush_buffer();

  virtual bool do_sticky_replay();
  virtual void mark_replay_stream(bool replay);

};

lmd_output_tcp *parse_open_lmd_server(const char *);

#endif//USE_LMD_OUTPUT_TCP

#endif//__LMD_OUTPUT_TCP_HH__
