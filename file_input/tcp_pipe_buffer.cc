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

#include "tcp_pipe_buffer.hh"

#include "lmd_input_tcp.hh"

#include <signal.h>

tcp_pipe_buffer::tcp_pipe_buffer()
{
  _server = NULL;
}


#ifdef USE_PTHREAD
void *tcp_pipe_buffer::reader()
{
  sigset_t sigmask;

  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGINT);

  pthread_sigmask(SIG_BLOCK,&sigmask,NULL);

  for ( ; ; )
    {
      // It make no sense to attempt getting buffers from the server, if
      // the available space in our buffer is too small, since the
      // server will then simply fill up our buffer to the end with an
      // empty buffer... (we could end up in an infinite loop...)

      size_t min_space = _server->preferred_min_buffer_size();

      if (min_space > _size)
	ERROR("TCP pipe buffer space too small (%zd < %zd)",
	      _size,min_space);

      while (_size - (_avail - _done) < min_space)
	{
	  // Buffer is currently too FULL for us to have any chance of
	  // getting the data.  Hold

	  MFENCE; // make sure we do not reorder

	  // min_space is quite large, so no hysteresis needed (each
	  // tcp buffer does read an entire buffer/stream before
	  // returning)
	  _wakeup_done = _avail - _size + min_space;
	  MFENCE;
	  _need_reader_wakeup = &_block;

	  MFENCE; // make sure we do not reorder

	  if (_size - (_avail - _done) < min_space)
	    {
	      // Buffer is actually full, do the blocking

	      //printf ("... wait for space ...\n");

	      _block.block();
	    }
	}

      // So, space should be available...

      size_t space  = _size - (_avail - _done);
      size_t offset = _avail & (_size - 1);

      size_t segment = _size - offset;

      if (segment > space)
	segment = space;
      /*
      printf ("_done: %08x  _avail: %08x  (space: %08x  segment: %08x)\n",
	      _done,_avail,space,segment);
      */
      size_t n;

      try
	{
	  n = _server->get_buffer(_buffer + offset,segment);
	}
      catch (error &e)
	{
	  WARNING("Closing this input...");
	  n = 0;
	}

      //printf ("got:%08x\n",n);

      if (n == 0)
	{
#ifdef USE_THREADING
	  request_next_file();
#endif

	  // Other side of connection closed...

	  _reached_eof = true;

	  MFENCE;

	  if (_need_consumer_wakeup)
	    {
	      // The consumer was waiting for us.
	      // wake him up to tell him that data till never be available :-(

	      const thread_block *blocked = (const thread_block *) _need_consumer_wakeup;
	      _need_consumer_wakeup = NULL;
	      SFENCE;
	      blocked->wakeup();
	    }

	  break;
	}

      _avail += n;

      if (_need_consumer_wakeup &&
	  ((ssize_t) (_avail - _wakeup_avail)) >= 0)
	{
	  // The consumer was waiting for us.

	  const thread_block *blocked = (const thread_block *) _need_consumer_wakeup;
	  _need_consumer_wakeup = NULL;
	  SFENCE;
	  blocked->wakeup();
	}

    }

  return NULL;
}
#else//!USE_PTHREAD
int tcp_pipe_buffer::read_now(off_t end)
{
  // We have no reader thread.  We are responsible for making sure the
  // requested range exist in the buffer

  //printf ("read_now(end=%d)\n",end);
  //fflush(stdout);

  while (((ssize_t) _avail - (ssize_t) end)) < 0)
    {
      if (_reached_eof)
	return 0; // data requested is NOT available

      // see how much space we have left to be filled...

      size_t space  = _size - (_avail - _done);
      size_t offset = _avail & (_size - 1);

      size_t segment = _size - offset;
      // size_t need = end - _avail;

      if (UNLIKELY(space <= 0))
	ERROR("pipe_buffer too small");

      if (segment > space)
	segment = space;

      // So, at current point of buffer there is segment bytes to fill
      // (before we reach unused data, or end) ...

      // please note: if get_buffer decides that the buffer space is
      // too small (or it has an error), it may eject a dummy buffer
      // header noting this, that eats the space up to the end,
      // then we'll have to be called again later

      size_t n;

      try
	{
	  n = _server->get_buffer(_buffer + offset,segment);
	}
      catch (error &e)
	{
	  WARNING("Closing this input...");
	  n = 0;
	}

      if (n == 0)
	{
	  // Other side of connection closed...

	  _reached_eof = true;
	  continue;
	}

      //printf ("read_now(end=%d) got %d\n",end,n);
      //fflush(stdout);

      _avail += n;
    }
  return 1;
}
#endif//!USE_PTHREAD



void tcp_pipe_buffer::init(lmd_input_tcp *server,size_t bufsize
#ifdef USE_PTHREAD
			   ,thread_block *block_reader
#endif
			   )
{
  _server = server;

  pipe_buffer_base::init(NULL,0,bufsize
#ifdef USE_PTHREAD
			 ,block_reader,true
#endif
			 );
}

void tcp_pipe_buffer::close()
{
  pipe_buffer_base::close();

  if (_server)
    {
      _server->close();
      delete _server;
      _server = NULL;
    }
}

