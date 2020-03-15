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

#include "lmd_output_tcp.hh"
#include "lmd_input_tcp.hh"
#include "parse_util.hh"

#include "optimise.hh"
#include "error.hh"
#include "colourtext.hh"
#include "set_thread_name.hh"

#include "../common/strndup.hh"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/select.h>


/*
 */



lmd_output_state::lmd_output_state()
{
  _stream_first = NULL;
  _stream_last = 0;

  _num_streams = 0;
  _max_streams = LMD_OUTPUT_DEFAULT_MAX_BUF /
    (LMD_OUTPUT_DEFAULT_BUFFER_SIZE * LMD_OUTPUT_DEFAULT_BUF_PER_STREAM);
  _buf_size = LMD_OUTPUT_DEFAULT_BUFFER_SIZE;
  _stream_bufs = LMD_OUTPUT_DEFAULT_BUF_PER_STREAM;

  _free_streams_avail = 0;
  _free_streams_used = 0;

  _filled_streams_avail = 0;
  _filled_streams_used = 0;

  _fill_stream = 0;
}

lmd_output_state::~lmd_output_state()
{
  // Kill the streams in the free buffer

  while (_free_streams_avail - _free_streams_used > 0)
    {
      free (_free_streams[_free_streams_used % LMD_OUTPUT_FREE_STREAMS]->_bufs);
      delete _free_streams[_free_streams_used % LMD_OUTPUT_FREE_STREAMS];
      _free_streams_used++;
    }

  while (_filled_streams_avail - _filled_streams_used > 0)
    {
      free (_filled_streams[_filled_streams_used % LMD_OUTPUT_FILLED_STREAMS]->_bufs);
      delete _filled_streams[_filled_streams_used % LMD_OUTPUT_FILLED_STREAMS];
      _filled_streams_used++;
    }

  if (_fill_stream)
    {
      free (_fill_stream->_bufs);
      delete _fill_stream;
    }

  // Kill the streams that were active

  lmd_output_stream *stream = _stream_first;

  while (stream)
    {
      lmd_output_stream *kill_stream = stream;
      stream = stream->_next;
      free (kill_stream->_bufs);
      delete kill_stream;
    }
}

#if 0 // unused, but should be
lmd_output_stream *lmd_output_state::get_free_stream()
{
  int filled = _free_streams_avail - _free_streams_used;

  if (filled <= 0)
    return NULL; // no free stream available

  lmd_output_stream *stream =
    _free_streams[_free_streams_used % LMD_OUTPUT_FREE_STREAMS];
   MFENCE; // make sure we do not reorder
  _free_streams_used++;

  // DBGprintf ("get_free_stream: give %2d\n",stream->_alloc_stream_no);

  return stream;
}
#endif


void lmd_output_state::add_free_stream(lmd_output_stream *stream)
{
  // Is there space in the circular buffer for this?

  int filled = _free_streams_avail - _free_streams_used;

  if (filled >= LMD_OUTPUT_FREE_STREAMS)
    {
      // No space available.  Delete the stream.

      // DGBprintf ("add_free_stream: delete %2d\n",stream->_alloc_stream_no);

      free (stream->_bufs);
      delete stream;
      _num_streams--;
      return;
    }

  // DGBprintf ("add_free_stream: add %2d\n",stream->_alloc_stream_no);

  _free_streams[_free_streams_avail % LMD_OUTPUT_FREE_STREAMS] = stream;
  MFENCE; // make sure we do not reorder
  _free_streams_avail++;
}

bool lmd_output_state::create_free_stream()
{
  // We'd like there to be at least one free stream

  int filled = _free_streams_avail - _free_streams_used;

  if (filled >= LMD_OUTPUT_FREE_STREAMS / 2) // there are actually free streams...
    return false; // this may happen, as we free up a stream after the
                  // filler wants one

  char *bufs = (char*) malloc(_stream_bufs * _buf_size);

  if (!bufs)
    ERROR("Memory allocation failure, could not allocate stream buffers.");

  lmd_output_stream *stream = new lmd_output_stream();

  if (!stream)
    ERROR("Memory allocation failure, could not allocate stream control.");

  stream->_bufs = bufs;

  _num_streams++;

  // For debugging, to keep track of the streams...
  static int alloc_stream_no = 0;
  stream->_alloc_stream_no = ++alloc_stream_no;

  // DGBprintf ("create_free_stream: %2d\n",stream->_alloc_stream_no);

  add_free_stream(stream);

  return true;
}


void lmd_output_state::unlink_stream(lmd_output_stream *stream)
{
  // DGBprintf ("unlink_stream: %2d\n",stream->_alloc_stream_no);

  // remove stream from list of active streams

  if (stream->_prev)
    stream->_prev->_next = stream->_next;
  else
    _stream_first = stream->_next;

  if (stream->_next)
    stream->_next->_prev = stream->_prev;
  else
    _stream_last = stream->_prev;

  stream->_next = NULL;
  stream->_prev = NULL;
}

void lmd_output_state::add_client_stream(lmd_output_stream *stream)
{
  // DGBprintf ("add_client_stream: %2d\n",stream->_alloc_stream_no);

  if (!_stream_last)
    {
      _stream_first = stream;
      _stream_last = stream;

      stream->_prev = NULL;
      stream->_next = NULL;
    }
  else
    {
      _stream_last->_next = stream;

      stream->_prev = _stream_last;
      stream->_next = NULL;

      _stream_last = stream;
    }
}

void lmd_output_state::free_oldest_unused()
{
  // find the oldest stream that noone is sending from

  lmd_output_stream *stream = _stream_first;

  while (stream)
    {
      if (!stream->_clients)
	break;
      stream = stream->_next;
    }

  if (!stream) // no unused stream
    return;

  if (stream->_prev &&
      (stream->_flags & LOS_FLAGS_HAS_STICKY_EVENT))
    stream->_prev->_flags |= LOS_FLAGS_STICKY_LOST_AFTER;

  // We must unlink the stream from the list...

  unlink_stream(stream);

  add_free_stream(stream);
}


void lmd_output_state::free_client_stream(lmd_output_stream *stream)
{
  stream->_clients--;

  bool force_free = false;

  if (_sendonce)
    {
      // noone should be sending from this one
      assert(stream->_clients == 0);
      force_free = true;
    }

  // DGBprintf ("free_client_stream: %2d(%d) ?\n",
  // DGB	  stream->_alloc_stream_no,
  // DGB	  stream->_clients);

  // Was this the first stream in the list, with no further clients
  // connected?

  while (force_free ||
	 (!stream->_clients &&
	  !stream->_prev))
    {
      // No-one will ever need us again

      lmd_output_stream *next_stream = stream->_next;

      assert (force_free || _stream_first == stream);

      unlink_stream(stream);

      add_free_stream(stream);

      stream = next_stream;
      force_free = false;

      if (!stream)
	break;
   }
}

bool matching_recovery_stream(lmd_output_stream *stream,
			      int *need_recovery_stream)
{
  int recovery_mask = stream->_flags & LOS_FLAGS_STICKY_RECOVERY_MASK;

  switch (*need_recovery_stream)
    {
    case 0:
      if (recovery_mask)
	return false;
      break;
    case 1:
      if (recovery_mask != LOS_FLAGS_STICKY_RECOVERY_FIRST)
	return false;
      *need_recovery_stream = 2;
      break;
    case 2:
      if (recovery_mask != LOS_FLAGS_STICKY_RECOVERY_MORE)
	*need_recovery_stream = 0;
      if (recovery_mask)
	return false;
      break;
    default:
      assert (false);
      break;
    }
  return true;
}

lmd_output_stream *lmd_output_state::get_next_client_stream(lmd_output_stream *stream, int *need_recovery_stream)
{
  if (!stream)
    return NULL;

  if (stream->_flags & LOS_FLAGS_STICKY_LOST_AFTER)
    {
#if DEBUG_REPLAYS
      printf ("Will need recovery stream.\n");
#endif
      *need_recovery_stream = 1;
    }

  // So, client is done with the stream, find the next one in the
  // list...  If there is no next stream, then we will return null,
  // indicating that the client will have to wait for more data to
  // become available

  lmd_output_stream *next_stream = stream->_next;

  // See if we have to skip forward through the streams...
  for ( ; next_stream; next_stream = next_stream->_next)
    {
      if (_sendonce &&
	  next_stream->_clients)
	{
	  if (next_stream->_flags & LOS_FLAGS_HAS_STICKY_EVENT)
	    {
	      WARNING("Sendonce and sticky events gives lousy performance!  "
		      "See code for details.");
	      // As soon as a client skips some stream, it will have to
	      // request a full recovery stream.  We need two things to
	      // adress this: variable-sized streams such that we
	      // need not transmit unduly large amounts of data as recovery.
	      // And recovery streams tailored for each receiver.
	      // ~ Alternatively: for each stream sent only once, we need
	      // an alternative stream to send to everyone else containing
	      // the summary of the sticky events in the stream that they
	      // just missed.

	      // Let's at least try to do it correctly.
	      *need_recovery_stream = 1;
	    }
	  goto skip_stream; // Do not send twice
	}

      assert(*need_recovery_stream >= 0 &&
	     *need_recovery_stream <= 2);

      if (!matching_recovery_stream(next_stream,need_recovery_stream))
	goto skip_stream;

      // This stream is to be sent
      break;

    skip_stream:
      if (next_stream->_flags & LOS_FLAGS_STICKY_LOST_AFTER)
	{
#if DEBUG_REPLAYS
	  printf ("Will need recovery stream II.\n");
#endif
	  *need_recovery_stream = 1;
	}    
    }

  // The next stream has a client now...
  if (next_stream)
    next_stream->_clients++;

  free_client_stream(stream);

  return next_stream;
}

#if 0 // unused, but should be?
lmd_output_stream *lmd_output_state::get_last_client_stream()
{
  assert (_stream_last);

  _stream_last->_clients++;
  return _stream_last;
}
#endif

lmd_output_stream *lmd_output_state::deque_filled_stream()
{
  lmd_output_stream *stream =
    _filled_streams[_filled_streams_used % LMD_OUTPUT_FILLED_STREAMS];
  MFENCE; // make sure we do not reorder
  _filled_streams_used++;

  return stream;
}

int lmd_output_state::streams_to_send()
{
  // count the number of streams that need to be sent before we're done

  int to_send = 0;
  int streams = 0;

  for (lmd_output_stream *s = _stream_first; s; s = s->_next)
    {
      streams += s->_clients;
      to_send += streams;
    }

  return to_send;
}


void lmd_output_state::dump_state()
{
  printf ("Active: ");
  for (lmd_output_stream *s = _stream_first; s; s = s->_next)
    {
      printf ("%2d(clients:%d)",s->_alloc_stream_no,s->_clients);

    }
  printf ("\n");

  printf ("Free   (%3d-%3d=%d): ",
	  _free_streams_avail,_free_streams_used,
	  _free_streams_avail-_free_streams_used);
  for (int i = _free_streams_used; i < _free_streams_avail; i++)
    printf (" %2d",_free_streams[i % LMD_OUTPUT_FREE_STREAMS]->_alloc_stream_no);
  printf ("\n");

  printf ("Filled (%3d-%3d=%d): ",
	  _filled_streams_avail,_filled_streams_used,
	  _filled_streams_avail-_filled_streams_used);
  for (int i = _filled_streams_used; i < _filled_streams_avail; i++)
    printf (" %2d",_filled_streams[i % LMD_OUTPUT_FILLED_STREAMS]->_alloc_stream_no);
  printf ("\n");

  printf ("Filling: ");
  if (_fill_stream)
    printf ("%2d\n",_fill_stream->_alloc_stream_no);
  else
    printf (" -\n");

}






/* Provide a server for MBS tcp data clients.  First (and only) we
 * implement a stream server, since this is the simplest
 */




lmd_output_client_con::lmd_output_client_con(int fd,
					     lmd_output_server_con *server_con,
					     lmd_output_state *data)
{
  _fd   = fd;
  _server_con = server_con;

  _data = data;

  _state = 0;
  _request._got = 0;

  _need_recovery_stream = 3; // 3 = figure out if we need on first attempt

  _current = NULL;
  _pending = NULL;
  _offset  = 0;
}







int lmd_output_client_con::setup_select(int nfd,
					fd_set *readfds,fd_set *writefds)
{
  switch (_state)
    {
    case LOCC_STATE_REQUEST_WAIT:
    case LOCC_STATE_CLOSE_WAIT:
      FD_SET(_fd,readfds);
      if (_fd > nfd)
	nfd = _fd;
      break;
    case LOCC_STATE_SEND_INFO:
    case LOCC_STATE_SEND_WAIT:
      FD_SET(_fd,writefds);
      if (_fd > nfd)
	nfd = _fd;
      break;
    }
  return nfd;
}

bool lmd_output_client_con::stream_is_available(lmd_output_stream *stream,
						bool sticky_events_seen,
						lmd_output_tcp *tcp_server)
{
  if (_current || _pending)
    return false;

  if (_data->_sendonce &&
      stream->_clients)
    return false; // someone got before us to this one, we are not interested

  if (_need_recovery_stream == 3)
    {
      if (sticky_events_seen)
	{
	  _need_recovery_stream = 1;
	  tcp_server->_need_recovery_stream = 1;
	}
      else
	_need_recovery_stream = 0;
    }

  if (!matching_recovery_stream(stream, &_need_recovery_stream))
    return false;

  stream->_clients++;

  // A stream just became available from the producer.
  // are we waiting?

  if (_state != LOCC_STATE_STREAM_WAIT)
    {
      _pending = stream;
      return true;
    }

  // find the new stream and move to next state (sending)

  _current = stream;
  _offset = 0;
  /*
  printf ("cur: %8d %04x\n",
	  ((uint32_t*) _current->_bufs)[3],
	  _current->_flags);
  */
  _state = LOCC_STATE_SEND_WAIT;

  return true;
}

void lmd_output_client_con::next_stream(lmd_output_tcp *tcp_server)
{
  if (_pending)
    {
      _current = _pending;
      _pending = NULL;
    }
  else
    _current = _data->get_next_client_stream(_current,
					     &_need_recovery_stream);
  _offset = 0; // in any case

  if (_current)
    {
      /*
      printf ("cur: %8d %04x\n",
	      ((uint32_t*) _current->_bufs)[3],
	      _current->_flags);
      */
      _state = LOCC_STATE_SEND_WAIT;
    }
  else
    {
      if (_need_recovery_stream == 1)
	tcp_server->_need_recovery_stream = 1;
      tcp_server->_tell_fill_stream = 1;
      _state = LOCC_STATE_STREAM_WAIT;
    }
}

bool lmd_output_client_con::after_select(fd_set *readfds,fd_set *writefds,
					 lmd_output_tcp *tcp_server)
{
  ssize_t n;

  switch (_state)
    {
    case LOCC_STATE_SEND_INFO:

      // INFO(0,"a:%d %d",_fd,FD_ISSET(_fd,writefds));

      if (!FD_ISSET(_fd,writefds))
	return true;

      // INFO(0,"a:%d %d",_fd,FD_ISSET(_fd,writefds));

      // just reformat the info buffer every time.  contents does not
      // change...

      ltcp_stream_trans_open_info info;

      info.testbit = 1; // native endian
      info.bufsize = tcp_server->_state._buf_size;
      info.bufs_per_stream = tcp_server->_state._stream_bufs;
      info.streams = 1; // we have a variable number of streams...

      if (!_server_con->_allow_data)
	{
	  // Put nasty data in the struct, to hint the client that bad
	  // things will happen.
	  info.bufsize = (uint32_t) -1;
	  info.bufs_per_stream = 0;
	}

      if (_server_con->_data_port != -1)
	{
	  // We use the dummy field to transmit the port number
	  info.streams = LMD_PORT_MAP_MARK | _server_con->_data_port;
	}


      // we abuse the _offset field to remember how much of the info
      // buffer has been sent so far...

      {
	size_t max_send = sizeof (ltcp_stream_trans_open_info) - _offset;

	n = write(_fd,((char *) &info) + _offset,max_send);
      }

      if (n == 0)
	{
	  WARNING("Write returned 0 when writing info to client.");
	  return false; // cannot happen...
	}

      if (n < 0)
	{
	  if (errno == EINTR ||
	      errno == EAGAIN)
	    return true; // try again next time

	  // all other errors are fatal

	  if (errno == EPIPE) {
	    WARNING("Client has disconnected, pipe closed.");
	  } else {
	    WARNING("Error while writing data to client.");
	  }
	  return false;
	}

      _offset += (size_t) n;
      tcp_server->_total_sent += n;

      if (_offset >= sizeof (ltcp_stream_trans_open_info))
	{
	  if (!_server_con->_allow_data)
	    {
	      gettimeofday(&_close_beginwait,NULL);
	      _state = LOCC_STATE_CLOSE_WAIT;
	      return true;
	    }
	  
	  // We've sent the info, go into next state...
	  _offset = 0;

	  if (_server_con->_mode == LMD_OUTPUT_STREAM_SERVER)
	    _state = LOCC_STATE_REQUEST_WAIT;
	  else
	    {
	      next_stream(tcp_server);
	    }
	  return true;
	}
      break;

    case LOCC_STATE_REQUEST_WAIT:
      assert(_server_con->_mode == LMD_OUTPUT_STREAM_SERVER);

      if (!FD_ISSET(_fd,readfds))
	return true;

      n = read(_fd,_request._msg+_request._got,12-_request._got);

      if (n == 0)
	return false; // other end closed...

      if (n < 0)
	{
	  if (errno == EINTR ||
	      errno == EAGAIN)
	    return true; // try again next time

	  // all other errors are fatal
	  WARNING("Error while reading request from client.");
	  return false;
	}

      _request._got += (size_t) n;

      if (_request._got == 12)
	{
	  _request._got = 0; // for next time

	  // We got the entire request...

	  _request._msg[12] = 0; // make sure it's 0 terminated

	  if (strcmp(_request._msg,"CLOSE") == 0)
	    return false;
	  if (strcmp(_request._msg,"PING") == 0)
	    return true; // we want another request

	  // Data wanted...
	  // See if we can get ourselves a buffer...

	  next_stream(tcp_server);
	}
      break;
      // case LOCC_STATE_STREAM_WAIT:
      // case LOCC_STATE_BUFFER_WAIT:
      // nothing to do, handled by stream_available()...
    case LOCC_STATE_SEND_WAIT:
      if (!FD_ISSET(_fd,writefds))
	return true;

      {
	size_t max_send = _current->_filled - _offset;

	n = write(_fd,_current->_bufs + _offset,max_send);
      }

      if (n == 0)
	{
	  WARNING("Write returned 0 when writing data to client.");
	  return false; // cannot happen...
	}

      if (n < 0)
	{
	  if (errno == EINTR ||
	      errno == EAGAIN)
	    return true; // try again next time

	  // all other errors are fatal

	  if (errno == EPIPE) {
	    WARNING("Client has disconnected, pipe closed.");
	  } else {
	    WARNING("Error while writing data to client.");
	  }
	  return false;
	}

      _offset += (size_t) n;
      tcp_server->_total_sent += n;
      
      if (_offset >= _current->_filled)
	{
	  // We reached the end of the data we currently know about

	  if (_current->_filled >= _current->_max_fill)
	    {
	      if (_server_con->_mode == LMD_OUTPUT_STREAM_SERVER)
		{
		  // We'll change buffer only after we got the
		  // request...

		  _state = LOCC_STATE_REQUEST_WAIT;
		}
	      else
		{
		  assert(_server_con->_mode == LMD_OUTPUT_TRANS_SERVER);

		  // This stream will never get more data, find ourselves
		  // a new one...

		  next_stream(tcp_server);
		}
	    }
	  else
	    {
	      // This stream _may_ get more data, wait for that to
	      // happen

	      tcp_server->_tell_fill_buffer = 1;
	      _state = LOCC_STATE_BUFFER_WAIT;
	    }
	}
      break;
    
    case LOCC_STATE_CLOSE_WAIT:
      // Note: there is no need to go into this state after we are out
      // of streams to send, i.e. after the disconnect request buffer
      // has been sent.  Since we only reach that end when wanting to
      // shut down, we will in a few seconds tear the connection down
      // unless the client does so first.  Either way, we would not
      // gain anything by us timing out a second earlier perhaps and
      // tearing it down.  (We need to client to do the first close to
      // not end up in network timeouts.)

      // We do get here to tear down pure portmap connections though.

      // If the timeout has passed, we close the connection.

      struct timeval now;

      gettimeofday(&now,NULL);
      
      if (now.tv_sec < _close_beginwait.tv_sec ||
	  now.tv_sec > _close_beginwait.tv_sec + 1)
	return false;
      
      // If the connection is ready for reading, either the client is
      // writing garbage to us, or actually did close.  In any case:
      // close the connection.
      
      if (FD_ISSET(_fd,readfds))
	return false;

      break;
    }
  return true;
}

void lmd_output_client_con::close()
{
  while (::close(_fd) != 0)
    {
      if (errno == EINTR)
	continue;

      WARNING("Failure closing client socket.");

      break;
    }

  if (_current)
    _data->free_client_stream(_current);
  else if (_pending)
    _data->free_client_stream(_pending);

  char client_dotted[INET_ADDRSTRLEN+1];

  inet_ntop(AF_INET,&_cliAddr.sin_addr,client_dotted,sizeof(client_dotted));

  INFO(0,"Closed connection [%s]...",client_dotted);
}

void lmd_output_client_con::dump_state()
{
  printf ("Client: mode=%d state=%d ",_server_con->_mode,_state);
  if (_current)
    printf ("%2d(off:%6d)",_current->_alloc_stream_no,(int)_offset);

  printf ("\n");
}



lmd_output_server_con::lmd_output_server_con()
{
  _socket = -1;
  _data_port = -1;
}

void lmd_output_server_con::bind(int mode, int port, int port_range_last)
{
  struct sockaddr_in servAddr;

  /* Create the server socket.
   */

  _socket = ::socket(PF_INET, SOCK_STREAM, 0);

  if (_socket < 0)
    ERROR("Could not open server socket.");

  if (port != -1) // otherwise bind to random port (for data)
    {
      for ( ; ; )
	{
	  servAddr.sin_family = AF_INET;
	  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	  servAddr.sin_port = htons((uint16_t) port);

	  int ret = ::bind (_socket,
			    (struct sockaddr *) &servAddr,sizeof(servAddr));

	  if (ret != 0)
	    {
	      if (errno == EADDRINUSE &&
		  port_range_last != -1)
		{
		  if ((uint16_t) port != (uint16_t) port_range_last)
		    {
		      /* Try with next port: */
		      port++;
		      continue;
		    }
		  /* Give up.  Print error. */
		}

	      ERROR("Failure binding server to port %d.",port);
	    }
	  break;
	}
    }

  if (::listen(_socket,3) != 0)
    ERROR("Failure to set server listening on port %d.",port);

  // make the server socket non-blocking, such that we're not hit by
  // false selects (see Linux man page bug notes)

  if (::fcntl(_socket,F_SETFL,fcntl(_socket,F_GETFL) | O_NONBLOCK) == -1)
    {
      perror("fcntl()");
      exit(1);
    }

  _mode = mode;
}

void lmd_output_server_con::close()
{
  while (::close(_socket) != 0)
    {
      if (errno == EINTR)
	continue;

      WARNING("Failure closing server socket.");

      break;
    }

  INFO(0,"Closed server...");
}


int lmd_output_server_con::setup_select(int nfd,
					fd_set *readfds,fd_set *writefds)
{
  FD_SET(_socket,readfds);
  if (_socket > nfd)
    nfd = _socket;
  return nfd;
}

bool lmd_output_server_con::after_select(fd_set *readfds,fd_set *writefds,
					 lmd_output_tcp *tcp_server)
{
  if (!FD_ISSET(_socket,readfds))
    return false;

  int client_fd;

  struct sockaddr_in cliAddr;
  socklen_t cliLen;

  cliLen = sizeof(cliAddr);

  client_fd = accept(_socket,(struct sockaddr *) &cliAddr,&cliLen);

  if (client_fd < 0)
    {
      if (errno == EINTR)
	return false; // we need to do it again, lets redo the select...

      if (errno == EAGAIN)
	return false; // false select...

      // There are many errors of accept that may happen as
      // consequences of the network (ECONNABORTED, EPERM, EPROTO),
      // so we only deal with it as a warning

      WARNING("Accepting client connection failed...");
      return false;
    }

  // make the socket non-blocking, such that we're not hit by false
  // selects (see Linux man page bug notes)

  if (fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL) | O_NONBLOCK) == -1)
    {
      perror("fcntl()");
      exit(1);
    }

  // ok, so we got a connection...

  lmd_output_client_con *client =
    new lmd_output_client_con(client_fd,this,&tcp_server->_state);

  if (!client)
    ERROR("Memory allocation failure, could not allocate client control.");

  tcp_server->_clients.push_back(client);
  // make it send the first info
  client->_state = LOCC_STATE_SEND_INFO;
  client->_cliAddr =cliAddr;

  char client_dotted[INET_ADDRSTRLEN+1];

  inet_ntop(AF_INET,&cliAddr.sin_addr,client_dotted,sizeof(client_dotted));

  INFO(0,"Accepted connection [%s]...",client_dotted);

  return true;
}




void *lmd_output_tcp::server_thread(void *us)
{
  return ((lmd_output_tcp *) us)->server();
}

void *lmd_output_tcp::server()
{
  // This thread does not want to see any SIGPIPEs.  It also does not
  // respond to the SIGINT requests (they are for the main thraed)

  bool show_connections = false;

  sigset_t sigmask;

  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGINT);
  sigaddset(&sigmask,SIGPIPE);

  pthread_sigmask(SIG_BLOCK,&sigmask,NULL);

  for ( ; ; )
    {
      if (show_connections)
	{
	  INFO(0,"%d clients...",(int) _clients.size());
	  show_connections = false;
	}

      fd_set readfds;
      fd_set writefds;
      int nfd = -1;

      FD_ZERO(&readfds);
      FD_ZERO(&writefds);

      // Check for incoming connections (if we have slots left)

      for (lmd_output_server_con_vect::iterator server = _servers.begin();
	   server != _servers.end(); ++server)
	nfd = (*server)->setup_select(nfd,&readfds,&writefds);

      // Loop over the clients, see if we can write to any of them

      for (lmd_output_client_con_vect::iterator client = _clients.begin();
	   client != _clients.end(); ++client)
	nfd = (*client)->setup_select(nfd,&readfds,&writefds);

      // If we're shutting down, then use a timeout, to make sure we
      // get a chance to also forcefully go down, if it seems no
      // client wants to make progress...

      struct timeval timeout;

      timeout.tv_sec  = 1;
      timeout.tv_usec = 0;

      if (!_hold || !_clients.empty())
	{
	  // We wont allow tokens from producer unless we are also
	  // ready to deque streams
	  nfd = _block_server.setup_select(nfd,&readfds);

	  // If there are things in the queue, set timeout to 0
	  if (_state._filled_streams_avail - _state._filled_streams_used > 0)
	    timeout.tv_sec = 0;
	}

      int ret = select(nfd+1,&readfds,&writefds,NULL,
		       (_shutdown_streams_to_send ||
			timeout.tv_sec == 0) ? &timeout : NULL);

      if (ret == -1)
	{
	  if (errno == EINTR)
	    continue; // try again

	  perror("select");
	  // Fatal, should never happen
	  exit(1);
	}

      // DGBprintf ("========================================================\n");
      // DGBprintf ("-------- after select --------\n");

      // printf ("streams: %d\n",_state._num_streams);

      /*
      _state.dump_state();
      for (lmd_output_client_con_vect::iterator client = _clients.begin();
	   client != _clients.end(); ++client)
	(*client)->dump_state();
      */

      for (lmd_output_server_con_vect::iterator server = _servers.begin();
	   server != _servers.end(); ++server)
	show_connections |= (*server)->after_select(&readfds,&writefds,this);

      if (_hold && _clients.empty())
	continue; // do not deque into emptiness

      // We always try to dequeue full streams.  The tokens are
      // basically just sent to wake us up if necessary, and also such
      // that a response can be sent

      bool got_stream = false;

      while (_state._filled_streams_avail - _state._filled_streams_used > 0)
	{
	  // a stream is available

	  got_stream = true;

	  lmd_output_stream *stream = _state.deque_filled_stream();

	  // DGBprintf ("server got filled stream: %2d\n",stream->_alloc_stream_no);

	  // there are no clients.  yet...
	  stream->_clients = 0;

	  if (stream->_flags & LOS_FLAGS_HAS_STICKY_EVENT)
	    _sticky_events_seen = true;

	  // TODO!!! (above) Hmm, do we eever propagare half-filled
	  // streams?  In that case, perhaps it gets ignored first,
	  // and then get the first sticky events,.  But a client that
	  // skipped it would not know to ask for a replay!
	  
	  // check if any client wants it
	  // we do the strange looping in order to spread the data evenly
	  // when we are in sendonce mode
	  /*
	  for (lmd_output_client_con_vect::iterator client = _clients.begin();
	       client != _clients.end(); ++client)
	    (*client)->stream_is_available(stream);
	  */
	  size_t n = _clients.size();
	  for (size_t i = 0; i < n; i++)
	    {
	      size_t client_i = (i + _next_search_client_i) % n;

	      lmd_output_client_con *client = _clients[client_i];

	      bool used = client->stream_is_available(stream,
						      _sticky_events_seen,
						      this);

	      if (used && _state._sendonce)
		{
		  _next_search_client_i = client_i+1;
		  break;
		}
	    }

	  if (_state._stream_last || stream->_clients)
	    {
	      // if the stream was wanted, or there already is a list of
	      // waiting streams, then insert it
	      _state.add_client_stream(stream);
	    }
	  else
	    {
	      // the stream was not wanted, add it to the free list
	      _state.add_free_stream(stream);
	    }

  	}

      if (got_stream)
	{
	  for (lmd_output_client_con_vect::iterator client_iter = _clients.begin();
	       client_iter != _clients.end(); )
	    {
	      lmd_output_client_con *client = *client_iter;

	      if (!client->_current && !client->_pending)
		{
		  // We got some free streams, but some client is
		  // still in need of streams.  Please tell us soon as
		  // some become available
		  _tell_fill_stream = 1;
		  break;
		}
	      ++client_iter;
	    }
	}
      
      /*
      _state.dump_state();
      for (lmd_output_client_con_vect::iterator client = _clients.begin();
	   client != _clients.end(); ++client)
	(*client)->dump_state();
      */
      for (lmd_output_client_con_vect::iterator client_iter = _clients.begin();
	   client_iter != _clients.end(); )
	{
	  lmd_output_client_con *client = *client_iter;
	  /*
	  INFO(0,"client...(fd:%d), r:%d w:%d",
	       client->_fd,
	       (int) FD_ISSET(client->_fd,&readfds),
               (int) FD_ISSET(client->_fd,&writefds));
	  */
	  if (!client->after_select(&readfds,&writefds,this))
	    {
	      // This client is over with.  Disconnect it

	      INFO(0,"client close...");
	      client->close();
	      client_iter = _clients.erase(client_iter);
	      show_connections = true;
	    }
	  else
	    ++client_iter;
	  // INFO(0,"client done...");
   	}
      int producer_token = 0;

      if (_block_server.has_token(&readfds,&producer_token))
	{
	  // DGBprintf ("server got token: %d\n",producer_token);
	  switch (producer_token)
	    {
	    case LOT_TOKEN_FILLED_BUFFER:
	    case LOT_TOKEN_FILLED_STREAM:
	      break;
	    case LOT_TOKEN_FILLED_STREAM_QUEUE_FULL:
	      // There were full streams to be dequeued
	      // This we have done...
	      _block_producer.wakeup(LOT_TOKEN_FILLED_STREAM_QUEUE_SLOT_FREE);
	      break;
	    case LOT_TOKEN_NEED_FREE_STREAM:
	      _need_free_stream = true;
	      break;
	    case LOT_TOKEN_SHUTDOWN:
	      // We got a shutdown token, let's stop business

	      /*
	      // Then remove all clients that have no data to send
	      for (lmd_output_client_con_vect::iterator client = _clients.begin();
		   client != _clients.end(); ++client)
		{
		  if ((*client)->_current ||
		      (*client)->_pending)
		    continue;
		  (*client)->close();
		  delete *client;
		}
	      */
	      // We'd now be left only with clients that should be
	      // making some progress...  This will be checked...

	      _shutdown_streams_to_send = _state.streams_to_send() + 1;
	      gettimeofday(&_shutdown_lasttime,NULL);
	    }
	}

      if (_need_free_stream)
	{
	  // the server is waiting for a free stream to write
	  // to this means that the free list is out of streams.
	  // perhaps our clients are to slow reading, so we'll be
	  // forced to steal a stream from the list of streams
	  // available

	  if (_state._num_streams < _state._max_streams)
	    {
	      // DGBprintf ("create free streams...\n");

	      // we allocate a new one...

	      while (_state._num_streams < _state._max_streams &&
		     _state.create_free_stream())
		;
	    }
	  else if (!_hold)
	    {
	      _state.free_oldest_unused();
	    }
	  /*
	    _free_streams[_free_stream_available & 3] = stream;
	    _free_stream_available++;
	  */

	  // If we got a stream free, then send the token

	  if (_state._free_streams_avail - _state._free_streams_used > 0)
	    {
	      _need_free_stream = false;
	      _block_producer.wakeup(LOT_TOKEN_HAVE_FREE_STREAM);
	    }
	}

      if (_shutdown_streams_to_send)
	{
	  // We are in the process of shutting down

	  // See how many streams are left to send

	  int streams_to_send = _state.streams_to_send();

	  if (!streams_to_send)
	    return NULL;

	  streams_to_send++;

	  struct timeval now;

	  gettimeofday(&now,NULL);

	  if (streams_to_send < _shutdown_streams_to_send)
	    {
	      // We made progress, at least one stream was sent as
	      // compared to last time

	      _shutdown_streams_to_send = streams_to_send;
	      _shutdown_lasttime = now;
	    }
	  else
	    {
	      // Have we waited more than between one and two seconds,
	      // without any progress, then we forcefully break down

	      if (now.tv_sec < _shutdown_lasttime.tv_sec ||
		  now.tv_sec > _shutdown_lasttime.tv_sec + 1)
		return NULL;
	    }
	}

      /*
      printf ("---------- all done ----------\n");
      _state.dump_state();
      for (lmd_output_client_con_vect::iterator client = _clients.begin();
	   client != _clients.end(); ++client)
	(*client)->dump_state();
      */

    }
  return NULL;
}



lmd_output_server_con *
lmd_output_tcp::create_server(int mode, int port, int port_range_last,
			      bool allow_data)
{
  lmd_output_server_con *server = NULL;

  server = new lmd_output_server_con();

  if (!server)
    ERROR("Memory allocation failure, could not allocate server control.");

  server->bind(mode, port, port_range_last);

  server->_allow_data = allow_data;

  _servers.push_back(server);

  return server;
}

void lmd_output_tcp::create_server(int mode, int port,
				   int dataport, int dataport_last,
				   bool allow_data_on_map_port)
{
  lmd_output_server_con *server_map  = NULL;
  lmd_output_server_con *server_data = NULL;

  server_map  = create_server(mode, port, -1, allow_data_on_map_port);
  if (dataport)
    {
      server_data = create_server(mode, dataport, dataport_last, true);

      // We need to know which port was chosen!

      struct sockaddr_in serv_addr;
      socklen_t len = sizeof(serv_addr);

      if (::getsockname(server_data->_socket,
			(struct sockaddr *) &serv_addr,&len) != 0)
	{
	  perror("getsockname()");
	  ERROR("Failure getting data server port number.");
	}

      if (len != sizeof(serv_addr) ||
	  serv_addr.sin_family != AF_INET)
	{
	  ERROR("Got bad length (%d) or address family (%d) for data socket.",
		len,serv_addr.sin_family);
	}

      server_map->_data_port = ntohs(serv_addr.sin_port);
    }

  const char *mode_str[] = { "","stream","trans" };

  assert (server_map->_mode > 0 && server_map->_mode <= 2);

  INFO(0,"Started %s server on port %d (data port %d)",
       mode_str[server_map->_mode], port, server_map->_data_port); 
}


void lmd_output_tcp::init()
{
  _block_server.init();
  _block_producer.init();

  if (pthread_create(&_thread,NULL,lmd_output_tcp::server_thread,this) != 0)
    {
      perror("pthread_create()");
      exit(1);
    }

  set_thread_name(_thread, "SRV", 5);

  gettimeofday(&_last_stream_enqueued,NULL);

  _active = true;
}

void lmd_output_tcp::send_last_buffer()
{
  while (_state._fill_stream->_filled + _state._buf_size <
	 _state._fill_stream->_max_fill)
    new_buffer();

  // make sure the last buffer gets sent...

  send_buffer();
}

void lmd_output_tcp::close()
{
  if (_active)
    {
      // make sure all buffers of the stream get's sent

      if (_cur_buf_start)
	{
	  while (_state._fill_stream->_filled + _state._buf_size <
		 _state._fill_stream->_max_fill)
	    new_buffer();
	}

      // Inject an empty buffer with l_evt negative to tell children
      // to close.  For stream server, it must be marked in the first
      // buffer of the stream.

      new_buffer();
      mark_close_buffer();
      send_last_buffer();

      // Tell the server that it should shut down

      // It will itself then enable timeouts in the processing.  If
      // some clients do not progress quickly enough, they will be
      // disconnected...

      _block_server.wakeup(LOT_TOKEN_SHUTDOWN);

      // pthread_cancel(_thread);

      if (pthread_join(_thread,NULL) != 0)
        {
          perror("pthread_join()");
          exit(1);
        }
      _active = false;

      // Close any remaining sockets

      for (lmd_output_server_con_vect::iterator server = _servers.begin();
	   server != _servers.end(); ++server)
	{
	  (*server)->close();
	  delete *server;
	}

      for (lmd_output_client_con_vect::iterator client = _clients.begin();
	   client != _clients.end(); ++client)
	{
	  (*client)->close();
	  delete *client;
	}
    }
}







void lmd_output_tcp::write_buffer(size_t count, bool has_sticky)
{
  // we do not allow compacting of the buffers!
  assert (count == _cur_buf_length);

  // this buffer is done with...

  _state._fill_stream->_filled += _state._buf_size;

  if (has_sticky)
    _state._fill_stream->_flags |= LOS_FLAGS_HAS_STICKY_EVENT;

  if (_state._fill_stream->_filled >= _state._fill_stream->_max_fill)
    {
      // We just completely filled the stream.

      // Put this one into the output circular buffer

      if (_mark_replay_stream)
	{
#if DEBUG_REPLAYS
	  printf ("Mark replay: %02x\n", _mark_replay_stream);
#endif
	}

      _state._fill_stream->_flags |= _mark_replay_stream;

      if (_mark_replay_stream == LOS_FLAGS_STICKY_RECOVERY_FIRST)
	_mark_replay_stream = LOS_FLAGS_STICKY_RECOVERY_MORE;

      if (_state._filled_streams_avail -
	  _state._filled_streams_used >= LMD_OUTPUT_FILLED_STREAMS)
	{
	  // Output buffer is full

	  _block_server.wakeup(LOT_TOKEN_FILLED_STREAM_QUEUE_FULL);

	  for ( ; ; )
	    {
	      int token = _block_producer.get_token();

	      if (token == LOT_TOKEN_FILLED_STREAM_QUEUE_SLOT_FREE)
		break;
	    }

	  // Since the server only sends this token in return for our
	  // request, we need not do a while loop.  just check...

	  assert (_state._filled_streams_avail -
		  _state._filled_streams_used < LMD_OUTPUT_FILLED_STREAMS);
	}

      // DGBprintf ("producer adding filled stream: %2d .. \n",
      // DGB	      _state._fill_stream->_alloc_stream_no);

      _state._filled_streams[_state._filled_streams_avail %
			     LMD_OUTPUT_FILLED_STREAMS] = _state._fill_stream;
      MFENCE;
      _state._filled_streams_avail++;
      _state._fill_stream = NULL;

      if (_tell_fill_stream)
	{
	  _tell_fill_buffer = 0;
	  _tell_fill_stream = 0;
	  _block_server.wakeup(LOT_TOKEN_FILLED_STREAM);
	}

      gettimeofday(&_last_stream_enqueued,NULL);
    }
  else if (_tell_fill_buffer)
    {
      _tell_fill_buffer = 0;
      _block_server.wakeup(LOT_TOKEN_FILLED_BUFFER);
    }
}

bool lmd_output_tcp::do_sticky_replay()
{
  if (!_need_recovery_stream)
    return false;

  // We must be at stream boundary in order for it to make sense to
  // inject a replay stream.

  if (_state._fill_stream->_filled != 0)
    return false;

  // And we should not be doing a replay!
  if (_mark_replay_stream)
    return false;

  // Remove the flag that we need a recovery stream.  This so that it
  // can be set again by the needing thread.  In principle, the
  // (better, i.e. last) point to remove the flag would be when the
  // receving (server) thread receives the first stream of the replay.
  // But if we wait that long, potentially the writing thread sees the
  // request still being active, and injects multiple replay
  // streams...

  // We cannot remove the mark in the producing thread after we have
  // performed the replay: if we produce multiple buffers, the first
  // may already have reached the server thread, been discarded and
  // after that, yet another client of the server ended up needing
  // another replay.

  _need_recovery_stream = 0;

  return true;
}

void lmd_output_tcp::mark_replay_stream(bool replay)
{
  if (replay)
    {
      _mark_replay_stream = LOS_FLAGS_STICKY_RECOVERY_FIRST;
      // We definately want to be told as soon as this one becomes
      // available; someone is waiting for it...
      _tell_fill_stream = 1;
    }
  else
    _mark_replay_stream = 0;

  // printf ("Set mark replay: %02x\n", _mark_replay_stream);
}

void lmd_output_tcp::get_buffer()
{
  // Are there any buffers left in the current stream?

  if (_state._fill_stream)
    {
      assert (_state._fill_stream->_filled < _state._fill_stream->_max_fill);

      // At least one buffer left.  Get that one

      _cur_buf_start =
	((uint8 *) _state._fill_stream->_bufs) + _state._fill_stream->_filled;
      _cur_buf_length = _state._buf_size;

      // Calculate the maximum payload data that can be stored into
      // what's left of this stream

      _stream_left = _state._fill_stream->_max_fill -
	_state._fill_stream->_filled;

      size_t streams = _stream_left / _state._buf_size;

      // First buffer loses the buffer header,
      // remaining buffers loses also the fragmentation overhead

      _stream_left -= sizeof (s_bufhe_host) +
	(streams - 1) * (sizeof (s_bufhe_host) + sizeof(lmd_event_header_host));
      return;
    }

  // We need to find ourselves a new stream...

  if (_state._free_streams_avail - _state._free_streams_used <= 0)
    {
      // There is no free stream in the circular buffer for free
      // streams...

      // Write a token to the server thread that we have a trouble
      // with that... :-)

      _block_server.wakeup(LOT_TOKEN_NEED_FREE_STREAM);

      // Token written.  Server is now fixing the problem.  Wait on
      // the token to get back, that the issue has been fixed.

      for ( ; ; )
	{
	  int token = _block_producer.get_token();

	  if (token == LOT_TOKEN_HAVE_FREE_STREAM)
	    break;
	}

      // Since the server only sends this token in return for our
      // request, we need not do a while loop.  just check...

      assert (_state._free_streams_avail - _state._free_streams_used > 0);
    }

  _state._fill_stream =
    _state._free_streams[_state._free_streams_used % LMD_OUTPUT_FREE_STREAMS];
  MFENCE;
  _state._free_streams_used++;

  // DGBprintf ("producer got free stream: %2d\n",
  // DGB	  _state._fill_stream->_alloc_stream_no);

  _state._fill_stream->_flags = 0;
  _state._fill_stream->_filled = 0;
  _state._fill_stream->_max_fill =
    _state._stream_bufs * _state._buf_size;

  _cur_buf_start = (uint8 *) _state._fill_stream->_bufs;
  _cur_buf_length = _state._buf_size;

  // _cur_buf_start = ;
  // _cur_buf_length = ;

  // Each extra buffer that an event uses, loses 8 bytes due to the
  // fragmentation...

  _stream_left = _stream_left_max =
    _state._stream_bufs *
    (_state._buf_size - sizeof(s_bufhe_host)) -
    (_state._stream_bufs - 1) * sizeof (lmd_event_header_host);
}

bool lmd_output_tcp::flush_buffer()
{
  // Return true if:
  // - some client is waiting for data
  // - if there is space on the outgoing stream queue to put more buffers
  // - it was more than _flush_interval seconds since
  // we last sent a buffer

  if (_flush_interval <= 0)
    return false; // no flushing requested

  if (!_tell_fill_buffer && !_tell_fill_stream)
    return false; // no client waiting

  if (_state._filled_streams_avail -
      _state._filled_streams_used >= LMD_OUTPUT_FILLED_STREAMS)
    return false; // there is no space on queue

  struct timeval now;

  gettimeofday(&now,NULL);

  int elapsed =
    (int) (now.tv_sec - _last_stream_enqueued.tv_sec +
	   (now.tv_usec - _last_stream_enqueued.tv_usec) / 1000000);

  if (elapsed > _flush_interval)
    {
      /*
      printf ("Flush...  (tfb:%d,tfs:%d,av:%d,use:%d,fill:%d,elapsed:%d)\n",
	      _tell_fill_buffer,_tell_fill_stream,
	      _state._filled_streams_avail,
	      _state._filled_streams_used,
	      _state._filled_streams_avail -
	      _state._filled_streams_used,
	      elapsed);
      */
      return true;
    }

  return false;
}

void lmd_output_tcp::print_status(double elapsed)
{
  double bufrate =
    (double) (_buffer_header.l_buf - _last_bufno) * 1.e-3 / elapsed;
  _last_bufno = _buffer_header.l_buf;
  uint64_t sent = _total_sent;
  double sentrate =
    (double) (sent - _last_sent) * 1.e-6 / elapsed;
  _last_sent = sent;
  double replayrate =
    (double) (_replays - _last_replays) / elapsed;
  _last_replays = _replays;

  fprintf (stderr,
	   "\nServer: (%s%.1f%skbuf/s, replay %s%.1f%sbuf/s) "
	   "Sent: %s%.1f%sMB/s   \r",
	   CT_ERR(BOLD),
	   bufrate,
	   CT_ERR(NORM),
	   CT_ERR(BOLD),
	   replayrate,
	   CT_ERR(NORM),
	   CT_ERR(BOLD_MAGENTA),
	   sentrate,
	   CT_ERR(NORM));
}

void lmd_server_usage()
{
  printf ("\n");
  printf ("LMD server (--server) options:\n");
  printf ("\n");
  lmd_out_common_options();
  printf ("bufsize=N           Buffer size [ki|Mi|Gi].\n");
  printf ("streambufs=N        Buffers per stream.\n");
  printf ("size=N              Total size of buffers [ki|Mi|Gi].\n");
  printf ("stream[:PORT]       Stream server protocol.\n");
  printf ("trans[:PORT]        Transport server protocol.\n");
  printf ("flush=N             Flush interval (s).\n");
  printf ("hold                Wait for clients, no data discarded.\n");
  printf ("sendonce            Only one receiver per stream, for fan-out.\n");
  printf ("forcemap            No data transmission on fixed port (avoid timeout on bind).\n");
  printf ("nopmap              Do not provide port mapping port.\n");
  printf ("dataport:PORT       Bind dataport at some [PORT,PORT+100].  Circumv. firewall.\n");
  printf ("\n");
}

lmd_output_tcp *parse_open_lmd_server(const char *command)
{
  lmd_output_tcp *out_tcp = new lmd_output_tcp();

  if (!out_tcp)
    ERROR("Memory allocation failure, could not allocate tcp output control.");

  // chop off any options of the filename
  // native, net, big, little
  // compact

  int stream_port = -1;
  int trans_port = -1;
  int data_port = -1; /* This default will bind to a random port by kernel. */
  bool forcemap = false;
  bool nopmap = false;

  uint64 max_size = LMD_OUTPUT_DEFAULT_MAX_BUF;

  const char *req_end;
  const char *next_cmd;

  while (*command)
    {
      req_end = strchr(command,',');
      if (!req_end)
	{
	  req_end = command+strlen(command);
	  next_cmd = req_end;
	}
      else
	next_cmd = req_end + 1;

      char *request = strndup(command,(size_t) (req_end-command));
      char *post;

#define MATCH_C_PREFIX(prefix,post) (strncmp(request,prefix,strlen(prefix)) == 0 && *(post = request + strlen(prefix)) != '\0')
#define MATCH_C_ARG(name) (strcmp(request,name) == 0)

      if (parse_lmd_out_common(request, true, false, out_tcp))
	;
      else if (MATCH_C_ARG("help"))
	{
	  lmd_server_usage();
	  exit(0);
	}
      else if (MATCH_C_ARG("hold"))
	out_tcp->_hold = true;
      else if (MATCH_C_ARG("sendonce"))
	out_tcp->_state._sendonce = true;
      else if (MATCH_C_ARG("forcemap"))
	forcemap = true;
      else if (MATCH_C_ARG("nopmap"))
	nopmap = true;
      else if (MATCH_C_PREFIX("bufsize=",post))
	{
	  out_tcp->_state._buf_size =
	    (uint32) parse_size_postfix(post,"kMG","BufSize",true);
	  if (out_tcp->_state._buf_size % 1024)
	    ERROR("Buffer size (%d) must be a multuple of 1024.",
		  out_tcp->_state._buf_size);
	}
      else if (MATCH_C_PREFIX("streambufs=",post))
	out_tcp->_state._stream_bufs = (uint32) atoi(post);
      else if (MATCH_C_PREFIX("size=",post))
	max_size = parse_size_postfix(post,"kMG","Size",false);
      else if (MATCH_C_ARG("stream"))
	stream_port = LMD_TCP_PORT_STREAM;
      else if (MATCH_C_PREFIX("stream:",post))
	stream_port = atoi(post);
      else if (MATCH_C_ARG("trans"))
	trans_port = LMD_TCP_PORT_TRANS;
      else if (MATCH_C_PREFIX("trans:",post))
	trans_port = atoi(post);
      else if (MATCH_C_PREFIX("dataport:",post))
	{
	  data_port = atoi(post);
	  if (!data_port)
	    {
	      // Either we disallow dataport=0, or we have to set nopmap
	      nopmap = true;
	    }
	}
      else if (MATCH_C_PREFIX("flush=",post))
	out_tcp->_flush_interval = atoi(post);
      else
	ERROR("Unrecognised option for TCP server: %s",request);

      free(request);
      command = next_cmd;
    }

  if (nopmap && forcemap)
    ERROR("nopmap and forcemap options are mutually exclusive.");

  out_tcp->_state._max_streams =
    (int) (max_size /
	   (out_tcp->_state._buf_size * out_tcp->_state._stream_bufs));

  if (out_tcp->_state._max_streams <= 0)
    out_tcp->_state._max_streams = 1;

  if (stream_port == -1 &&
      trans_port == -1)
    stream_port = LMD_TCP_PORT_STREAM;

  INFO(0,"TCP server, bufs=%d*%dkiB,streams=%d (%zdkiB)%s(flush=%ds):",
       out_tcp->_state._stream_bufs,
       out_tcp->_state._buf_size >> 10,
       out_tcp->_state._max_streams,
       (size_t) (out_tcp->_state._buf_size >> 10) *
       ((size_t) out_tcp->_state._stream_bufs*
	(size_t) out_tcp->_state._max_streams),
       out_tcp->_hold ? ", hold" : "",
       out_tcp->_flush_interval);

  if (stream_port != -1)
    out_tcp->create_server(LMD_OUTPUT_STREAM_SERVER,stream_port,
			   data_port, data_port + 100,
			   forcemap ? false : true);
  if (trans_port != -1)
    {
      /* Bind portmap port before legacy port, so clients do not accidentaly
       * revert to the legacy port while we are trying to bind.
       */
       if (!nopmap)
	 out_tcp->create_server(LMD_OUTPUT_TRANS_SERVER,
				trans_port + LMD_TCP_PORT_TRANS_MAP_ADD,
				data_port, data_port + 100,
				false);
      if (!forcemap)
	out_tcp->create_server(LMD_OUTPUT_TRANS_SERVER,trans_port,
			       nopmap ? data_port       : 0,
			       data_port + 100, //does not matter if !data_port
			       true);
    }

  out_tcp->init();

  return out_tcp;
}

/* 
Some TCP/trans performance numbers:

# E5-2690 v3 @ 2.60GHz, dual 10 Gbps network

evsize   sevsize    sent     sentmany (to other machines)
                    MB/s     MB/s

0        0          180      2350
10       0          185      2350
50       0          325      2330
100      0          380      2340
500      0          450      2340
1000     0          470      2340
10000    0          480      2340

100      10         450      2340
100      50         ~700     2340
500      50         ~750     2340
1000     100        ~900     2340
10000    1000       ~900     2320

# Sender:

file_input/empty_file --lmd --event-size=100 --subevent-size=10 | \
  empty/empty --file=- --server=trans

# Receivers (10+10+10 on different machines when many):

empty/empty --trans=vale 2> /dev/null &

*/
