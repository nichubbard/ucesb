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

#define DO_EXT_NET_DECL
#include "ext_file_writer.hh"

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "ext_file_error.hh"

extern const char *_argv0;
extern int _got_sigio;

/* Very simple network communication.
 *
 * The server several connections, and handle them with a select()
 * statement.  (As usual.)
 *
 * When reading data, which can only be done by one source at a time,
 * also the source may be included in the select statement, making the
 * server operational also at times when waiting for more data.
 *
 * The communication is _unidirectional_.  I.e. whenever a client opens
 * a connection, data will be thrown at it, but we do not care about
 * any errors it may have.  Closed pipe is a closed pipe and that's
 * that.
 *
 * With one exception.  To be able to serve also simple http "GET "
 * requests when connected from a web browser, clients should initiate
 * the connection by writing a 32-bit magic number first.  This is
 * necessary to distinguish the two.  The magic number is not intended
 * to be used.  (The server sends the magic, and the client is
 * expected to do its own checking).  [This server can handle clients
 * that send data without crashing itself, so we begin the
 * introduction of this feature by making the clients send the magic.]
 *
 * Data is sent in chunks of one or more messages (as the input
 * protocol).  We keep a buffer of messages to send, of several MB.
 * It's arranged in one item per chunk.  Whenever an item is being
 * used as source by any server connection, it cannot be reclaimed.
 * Other items are subject to reclaim if too much data is in the
 * buffer.  The chunks are arranged as a linked list.  Server clients
 * get items in order.  Missing items (i.e. that were reclaimed) are
 * just skipped.  This takes care of slow clients.  Note that it may
 * get a few more old items, if they were stuck in other clients, as
 * they may then survive reclaiming.  Still in order, and as good as
 * anything else.
 *
 * This is all quite similar to the file_input/lmd_output_tcp.cc
 * server handling, except that we do the buffering on direct items,
 * which certainly is less efficient.  This can actually be somewhat
 * fixed by merging items at the end into larger chunks (as long as
 * there are no readers.
 */

struct send_item_chunk
{
  struct send_item_chunk *_next;
  struct send_item_chunk *_prev;

  size_t _alloc;
  size_t _length;    // of data
  int    _consumers; // active consumers (cannot reclaim if > 0)

  char   _data[];
};

// data that must be sent to all clients (at startup)
send_item_chunk _init_chunks = { &_init_chunks, &_init_chunks, 0, 0 };
// data sent to clients, as far as they can consume it
send_item_chunk _event_chunks = { &_event_chunks, &_event_chunks, 0, 0 };

#define MAX_BUFFER_MEMORY 0x01000000 // 16 MB

struct send_client
{
  struct send_client *_next;
  struct send_client *_prev;

  // Communicating socket

  int _fd;

  // Current chunk we're writing data from

  send_item_chunk* _chunk;
  size_t           _offset;
};

send_client _ext_net_clients =
  { &_ext_net_clients, &_ext_net_clients, -1, NULL, 0 };

struct portmap_client
{
  struct portmap_client *_next;
  struct portmap_client *_prev;

  // Communicating socket

  int _fd;

  // Where we're writing

  size_t           _offset;
};

portmap_client _ext_net_portmaps =
  { &_ext_net_portmaps, &_ext_net_portmaps, -1, 0 };

void check_list_integrity()
{
  for (send_item_chunk *iter = &_init_chunks; ; )
    {
      assert(iter->_next->_prev == iter);
      assert(iter->_prev->_next == iter);
      iter = iter->_next;
      if (iter == &_init_chunks)
	break;
    }
  for (send_item_chunk *iter = &_event_chunks; ; )
    {
      assert(iter->_next->_prev == iter);
      assert(iter->_prev->_next == iter);
      iter = iter->_next;
      if (iter == &_event_chunks)
	break;
    }
  for (send_client *iter = &_ext_net_clients; ; )
    {
      if (iter->_chunk)
	{
	  assert(iter->_chunk->_consumers);
	  assert(iter->_offset <= iter->_chunk->_length);
	}
      assert(iter->_next->_prev == iter);
      assert(iter->_prev->_next == iter);
      iter = iter->_next;
      if (iter == &_ext_net_clients)
	break;
    }
  for (portmap_client *iter = &_ext_net_portmaps; ; )
    {
      assert(iter->_next->_prev == iter);
      assert(iter->_prev->_next == iter);
      iter = iter->_next;
      if (iter == &_ext_net_portmaps)
	break;
    }
}

#define CHECK_LIST_INTEGRITY // check_list_integrity()

int _delayed_got_sigio = 0;

char *ext_net_io_reserve_chunk(size_t length,bool init_chunk,
			       send_item_chunk **chunk)
{
  send_item_chunk *chunk_list;

  if (init_chunk)
    chunk_list = &_init_chunks;
  else
    chunk_list = &_event_chunks;

  // See if there is room left in the last chunk of the appropriate
  // list

  if (chunk_list->_prev != chunk_list &&
      chunk_list->_prev->_alloc - chunk_list->_prev->_length >= length)
    {
      // There is room left, so use it!
      *chunk = chunk_list->_prev;
      return (*chunk)->_data + (*chunk)->_length;
    }

  // No free space in the appropriate chunk, we must get a new one

  CHECK_LIST_INTEGRITY;

  size_t alloc_size = sizeof(send_item_chunk) + length * 4;

  if (alloc_size < 0x10000)
    alloc_size = 0x10000;

  *chunk = NULL; // no memory allocated / stolen so far

  // Do we have too much memory allocated?  Then go through the chunk
  // list and free items.
  send_item_chunk *iter = _event_chunks._next;

  while (_init_chunks._length + _event_chunks._length +
	 alloc_size > MAX_BUFFER_MEMORY &&
	 iter != &_event_chunks)
    {
      send_item_chunk *try_free = iter;

      iter = iter->_next;

      if (!try_free->_consumers)
	{
	  try_free->_prev->_next = try_free->_next;
	  try_free->_next->_prev = try_free->_prev;
	  _event_chunks._length -=
	    (sizeof(send_item_chunk) + try_free->_length);

	  if (!*chunk &&
	      try_free->_alloc + sizeof(send_item_chunk) >= alloc_size)
	    *chunk = try_free; // steal this chunk!
	  else
	    free (try_free);
	}
    }

  if (!*chunk)
    {
      // Allocate memory for the chunk.

      *chunk = (send_item_chunk *) malloc(alloc_size);

      if (!chunk)
	ERR_MSG("Failure allocating memory for chunk.");
    }

  (*chunk)->_alloc = alloc_size - sizeof(send_item_chunk);
  (*chunk)->_length = 0;
  (*chunk)->_consumers = 0;

  assert (!(length & 3));

  // memcpy (chunk->_data,ptr,length);

  // splice the chunk in at the end of the items although it's not yet
  // active, we can add it to the list, as when the chunk becomes
  // longer below (in the commit function), we'll ask any client that
  // already passed that point to restart the last chunk from there

  (*chunk)->_next = chunk_list;
  (*chunk)->_prev = chunk_list->_prev;

  chunk_list->_prev->_next = *chunk;
  chunk_list->_prev = *chunk;

  chunk_list->_length += alloc_size;

  if (_delayed_got_sigio)
    {
      _delayed_got_sigio = 0;
      _got_sigio = 1;
    }

  return (*chunk)->_data;
}

ext_file_net_stat _net_stat = { 0, 0, 0 };

void ext_net_io_commit_chunk(size_t length,send_item_chunk *chunk)
{
  size_t prev_length = chunk->_length;

  assert (!(length & 3));

  chunk->_length += length;

  assert (chunk->_length <= chunk->_alloc); // or too much written

  _net_stat._committed_size += length;

  // If there are any clients that have reached the end of data, they
  // need to be made aware of the availabilty of new data

  send_client *client;

  for (client = _ext_net_clients._next; client != &_ext_net_clients;
       client = client->_next)
    {
      if (client->_chunk == NULL)
	{
	  client->_chunk  = chunk;
	  client->_offset = prev_length;
	  chunk->_consumers++;

	  // We may need to notify the big event handling loop that
	  // some client now (possibly) could do some I/O for us.

	  // But as this can get informed for every event, we delay
	  // the notification until a new chunk as allocated.  No harm
	  // if no new chunk is requested for a long time.  Then
	  // instead the big loop will voluntarily give the clients a chance.

	  _delayed_got_sigio = 1;
	}
    }

  CHECK_LIST_INTEGRITY;
}

void ext_net_io_nonblock_async(int fd,const char *descr)
{
  if (::fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) | O_NONBLOCK | O_ASYNC) == -1)
    {
      perror("fcntl()");
      ERR_MSG("Failure making %s non-blocking and asynchronous.",descr);
    }

  if (::fcntl(fd,F_SETOWN,getpid()) == -1)
    {
      perror("fcntl()");
      ERR_MSG("Failure setting receiver of SIGIO for %s.",descr);
    }
}

external_writer_portmap_msg _portmap_msg = { 0, 0 };

int _ext_net_server_fd = -1;
int _ext_net_data_serv_fd = -1;

void ext_net_io_server_bind(int port)
{
  struct sockaddr_in serv_addr;

  /* Create the server socket.
   */

  _ext_net_server_fd = ::socket(PF_INET, SOCK_STREAM, 0);

  if (_ext_net_server_fd < 0)
    {
      perror("socket()");
      ERR_MSG("Could not open server socket.");
    }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  if (::bind (_ext_net_server_fd,
	      (struct sockaddr *) &serv_addr,sizeof(serv_addr)) != 0)
    {
      perror("bind()");
      ERR_MSG("Failure binding server to port %d.",port);
    }

  if (::listen(_ext_net_server_fd,3) != 0)
    {
      perror("listen()");
      ERR_MSG("Failure to set server listening on port %d.",port);
    }

  // make the server socket non-blocking, such that we're not hit by
  // false selects (see Linux man page bug notes)

  ext_net_io_nonblock_async(_ext_net_server_fd,"server socket");

  // To avoid most of the problems with the server port getting stuck
  // with 'cannot bind: address in use', we'll open one more port, on
  // which all data traffic (persistent connections) goes.  The 'well
  // numbered' port is only used as a cheap portmapper, i.e. giving
  // the port number of the real data server port

  _ext_net_data_serv_fd = ::socket(PF_INET, SOCK_STREAM, 0);

  if (_ext_net_data_serv_fd < 0)
    {
      perror("socket()");
      ERR_MSG("Could not open (data) server socket.");
    }

  // The data server socket is not bound to any specific port.  That's
  // the entire gag - listen will assign a random port.

  if (::listen(_ext_net_data_serv_fd,3) != 0)
    {
      perror("listen()");
      ERR_MSG("Failure to set data server listen on a port.");
    }

  // make the data server socket non-blocking, such that we're not hit
  // by false selects (see Linux man page bug notes)

  ext_net_io_nonblock_async(_ext_net_data_serv_fd,"data server socket");

  // We need to know which port was chosen!

  socklen_t len = sizeof(serv_addr);

  if (::getsockname(_ext_net_data_serv_fd,
		    (struct sockaddr *) &serv_addr,&len) != 0)
    {
      perror("getsockname()");
      ERR_MSG("Failure getting data server port number.");
    }

  if (len != sizeof(serv_addr) ||
      serv_addr.sin_family != AF_INET)
    {
      ERR_MSG("Got bad length (%d) or address family (%d) for data socket.",
	      len,serv_addr.sin_family);
    }

  _portmap_msg._magic = htonl(~EXTERNAL_WRITER_MAGIC);
  _portmap_msg._port  = serv_addr.sin_port; // already network order

  // Both sockets successfully connected

  MSG("Started server on port %d (data port %d).",
      port,ntohs(serv_addr.sin_port));

  // We don't want any SIGPIPE signals to kill us

  sigset_t sigmask;

  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGPIPE);

  sigprocmask(SIG_BLOCK,&sigmask,NULL);
}

void ext_net_io_server_accept()
{
  int client_fd;

  struct sockaddr_in cli_addr;
  socklen_t cli_len;

  cli_len = sizeof(cli_addr);

  client_fd = accept(_ext_net_server_fd,
		     (struct sockaddr *) &cli_addr,&cli_len);

  if (client_fd < 0)
    {
      if (errno == EINTR)
	return; // we need to do it again, lets redo the select...

      if (errno == EAGAIN)
	return; // false select...

      // There are many errors of accept that may happen as
      // consequences of the network (ECONNABORTED, EPERM, EPROTO),
      // so we only deal with it as a warning

      perror("accept");
      MSG("Accepting client connection failed...");
      return;
    }

  // make the socket non-blocking, such that we're not hit by false
  // selects (see Linux man page bug notes)

  // make it asynchronous (deliver SIGIO when I/O possible) to prevent
  // possible infinte loop handling events without being sent in the
  // 'main' routine

  ext_net_io_nonblock_async(client_fd,"client connection");

  portmap_client *portmap = (portmap_client *) malloc(sizeof(portmap_client));

  if (!portmap)
    ERR_MSG("Failure allocating memory for portmap client state.");

  // ok, so we got a connection...

  portmap->_fd = client_fd;
  portmap->_offset = 0;

  // Add it to the list of clients

  portmap->_next = &_ext_net_portmaps;
  portmap->_prev =  _ext_net_portmaps._prev;

  _ext_net_portmaps._prev->_next = portmap;
  _ext_net_portmaps._prev = portmap;

  CHECK_LIST_INTEGRITY;

  // Tell from where the connection is

  char client_dotted[INET_ADDRSTRLEN+1];

  inet_ntop(AF_INET,&cli_addr.sin_addr,client_dotted,sizeof(client_dotted));

  MSG("Accepted pmap connection [%s]...",client_dotted);

  // Make sure the select() gets called once more, to do the write
  _got_sigio = 1;
}


void ext_net_io_data_serv_accept()
{
  int client_fd;

  struct sockaddr_in cli_addr;
  socklen_t cli_len;

  cli_len = sizeof(cli_addr);

  client_fd = accept(_ext_net_data_serv_fd,
		     (struct sockaddr *) &cli_addr,&cli_len);

  if (client_fd < 0)
    {
      if (errno == EINTR)
	return; // we need to do it again, lets redo the select...

      if (errno == EAGAIN)
	return; // false select...

      // There are many errors of accept that may happen as
      // consequences of the network (ECONNABORTED, EPERM, EPROTO),
      // so we only deal with it as a warning

      perror("accept");
      MSG("Accepting client data connection failed...");
      return;
    }

  // make the socket non-blocking, such that we're not hit by false
  // selects (see Linux man page bug notes)

  ext_net_io_nonblock_async(client_fd,"client data connection");

  send_client *client = (send_client *) malloc(sizeof(send_client));

  if (!client)
    ERR_MSG("Failure allocating memory for client state.");

  // ok, so we got a connection...

  client->_fd = client_fd;
  // client->_cli_addr = cli_addr;

  client->_chunk = _init_chunks._next;
  client->_offset = 0;
  client->_chunk->_consumers++;

  assert(client->_chunk != &_init_chunks); // or there is no init data

  // Add it to the list of clients

  client->_next = &_ext_net_clients;
  client->_prev =  _ext_net_clients._prev;

  _ext_net_clients._prev->_next = client;
  _ext_net_clients._prev = client;

  _net_stat._cur_clients++;

  CHECK_LIST_INTEGRITY;

  // Tell from where the connection is

  char client_dotted[INET_ADDRSTRLEN+1];

  inet_ntop(AF_INET,&cli_addr.sin_addr,client_dotted,sizeof(client_dotted));

  MSG("Accepted data connection [%s]...",client_dotted);

  // Make sure the select() gets called once more to do the first write()
  _got_sigio = 1;
}



bool ext_net_io_client_write(send_client *client)
{
  bool again = false;

 do_write:
  assert (client->_chunk);

  size_t left = client->_chunk->_length - client->_offset;

  ssize_t n = write(client->_fd,client->_chunk->_data+client->_offset,left);

  if (n == -1)
    {
      if (errno == EINTR)
	return true;
      if (errno == EAGAIN && again)
	return true;
      if (errno == EPIPE) {
	MSG("Client has disconnected, pipe closed.");
      } else {
	perror("write");
	MSG("Error while writing data to client.");
      }
      return false;
    }

  // Write was successful

  _net_stat._sent_size += n;

  client->_offset += n;

  if (client->_offset < client->_chunk->_length)
    return true;

  // Done with this chunk, find the next one

  client->_chunk->_consumers--;
  client->_chunk = client->_chunk->_next;

  if (client->_chunk == &_init_chunks)
    client->_chunk = _event_chunks._next;

  if (client->_chunk == &_event_chunks)
    {
      client->_chunk = NULL; // no more data available currently
      client->_offset = 0; // for use when doing shutdown (consume magic)
    }

  if (client->_chunk)
    {
      client->_chunk->_consumers++;
      client->_offset = 0;
      // Try to write once more, from this new buffer.  (otherwise,
      // would have to simulate with _got_sigio to avoid getting stuck
      // in main process loop).  However, if we try once more, then the
      // guarantee from select that we can write more may not hold, so
      // must accept EAGAIN as a valid error message!
      again = true;
      goto do_write;
    }

  CHECK_LIST_INTEGRITY;

  return true;
}

bool ext_net_io_portmap_write(portmap_client *portmap)
{
  size_t left = sizeof(_portmap_msg) - portmap->_offset;

  ssize_t n = write(portmap->_fd,((char*) &_portmap_msg)+portmap->_offset,left);

  if (n == -1)
    {
      if (errno == EINTR)
	return true;
      if (errno == EPIPE) {
	MSG("Port client has disconnected, pipe closed.");
      } else {
	perror("write");
	MSG("Error while writing port to client.");
      }
      return false;
    }

  // Write was successful

  portmap->_offset += n;

  return true;
}


bool ext_net_io_select_clients(int read_fd,int write_fd,
			       bool shutdown,bool zero_timeout)
{
  fd_set readfds;
  fd_set writefds;
  int nfd = -1;

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);

  CHECK_LIST_INTEGRITY;

  // Our caller (incoming events) may need to wait too

  assert (read_fd == -1 || write_fd == -1); // or our return value
  // cannot be used to determine which is ready for I/O

  if (read_fd != -1)
    {
      FD_SET(read_fd,&readfds);
      if (read_fd > nfd)
	nfd = read_fd;
    }
  if (write_fd != -1)
    {
      FD_SET(write_fd,&writefds);
      if (write_fd > nfd)
	nfd = write_fd;
    }

  send_client *client;

  for (client = _ext_net_clients._next; client != &_ext_net_clients;
       client = client->_next)
    {
      if (client->_chunk) // it would like to write
	{
	  FD_SET(client->_fd,&writefds);
	  if (client->_fd > nfd)
	    nfd = client->_fd;
	}
      else if (shutdown)
	{
	  // If we have nothing more to write to this client, then it
	  // should as last message have gotten a shut down request.
	  // That ought to make the client quit, after which, reading
	  // from it should go into possible mode (a file handle is
	  // readable on EOF).  If it does not quit, we'll time out.
	  // And if it actually writes something, we'll close it then
	  // anyhow.

	  FD_SET(client->_fd,&readfds);
	  if (client->_fd > nfd)
	    nfd = client->_fd;
	}
    }

  portmap_client *portmap;

  for (portmap = _ext_net_portmaps._next; portmap != &_ext_net_portmaps;
       portmap = portmap->_next)
    {
      if (portmap->_offset < sizeof(_portmap_msg)) // it would like to write
	{
	  FD_SET(portmap->_fd,&writefds);
	  if (portmap->_fd > nfd)
	    nfd = portmap->_fd;
	}
      else
	{
	  // See above (for data client)

	  FD_SET(portmap->_fd,&readfds);
	  if (portmap->_fd > nfd)
	    nfd = portmap->_fd;
	}
    }

  if (_ext_net_server_fd != -1)
    {
      FD_SET(_ext_net_server_fd,&readfds);
      if (_ext_net_server_fd > nfd)
	nfd = _ext_net_server_fd;
    }

  if (_ext_net_data_serv_fd != -1)
    {
      FD_SET(_ext_net_data_serv_fd,&readfds);
      if (_ext_net_data_serv_fd > nfd)
	nfd = _ext_net_data_serv_fd;
    }

  // Then do the select

  struct timeval timeout;

  timeout.tv_sec = zero_timeout ? 0 : 2; // else shutdown
  timeout.tv_usec = 0;

  int ret = select(nfd+1,&readfds,&writefds,NULL,
		   shutdown || zero_timeout ? &timeout : NULL);

  if (ret == -1)
    {
      if (errno == EINTR)
	return false; // try again
      perror("select");
      ERR_MSG("Unexpected error in I/O multiplexer.");
    }

  if (ret == 0) // can only happen on timeout (i.e. if shutdown)
    return true;

  send_client *iter;

  for (iter = _ext_net_clients._next; iter != &_ext_net_clients; )
    {
      client = iter;
      iter = iter->_next;

      if (client->_chunk)
	{
	  // If nothing to do, or if write successful, continue

	  if (!FD_ISSET(client->_fd,&writefds) ||
	      ext_net_io_client_write(client))
	    continue;
	}
      else if (!shutdown)
	continue; // nothing to do
      else
	{
	  uint32_t dummy;
	  ssize_t n;

	  // We are in shutdown mode.  Continue until file handle is
	  // ready for reading.

	  if (!FD_ISSET(client->_fd,&readfds))
	    continue;

	  // It may be the magic coming from the client.  If so,
	  // consume that.

	  n = read(client->_fd,&dummy,sizeof(dummy));

	  // Do not care too much about errors, as we will just close
	  // anyhow.  Only if there was data to consume, we do not
	  // close.

	  if (n == -1 && errno == EINTR)
	    continue;

	  if (n > 0)
	    {
	      // Make sure we cannot get stuck here by the client
	      // sending copious amounts of data at us.

	      client->_offset += n;

	      if (client->_offset <= sizeof(uint32_t))
		continue;
	    }
	}

      // Client has exited, at least got EPIPE
      // close and remove it

      while (close(client->_fd) != 0)
	{
	  if (errno == EINTR)
	    continue;

	  perror("close");
	  ERR_MSG("Failure closing client socket.");
	  break;
	}
      if (client->_chunk)
	client->_chunk->_consumers--;

      client->_prev->_next = client->_next;
      client->_next->_prev = client->_prev;

      free(client);

      _net_stat._cur_clients--;

      CHECK_LIST_INTEGRITY;
    }

  portmap_client *iterp;

  for (iterp = _ext_net_portmaps._next; iterp != &_ext_net_portmaps; )
    {
      portmap = iterp;
      iterp = iterp->_next;

      if (portmap->_offset < sizeof(_portmap_msg))
	{
	  // If nothing to do, or if write successful, continue

	  if (!FD_ISSET(portmap->_fd,&writefds) ||
	      ext_net_io_portmap_write(portmap))
	    continue;
	}
      else
	{
	  uint32_t dummy;
	  ssize_t n;

	  // We have sent our info.  Continue until file handle is
	  // ready for reading.  Such that the client does the
	  // shutdown, and the client thus gets the timeout, and we
	  // don't.

	  if (!FD_ISSET(portmap->_fd,&readfds))
	    continue;

	  // It may be the magic coming from the client.  If so,
	  // consume that.

	  n = read(portmap->_fd,&dummy,sizeof(dummy));

	  // Do not care too much about errors, as we will just close
	  // anyhow.  Only if there was data to consume, we do not
	  // close.

	  if (n == -1 && errno == EINTR)
	    continue;

	  if (n > 0)
	    {
	      // Make sure we cannot get stuck here by the client
	      // sending copious amounts of data at us.

	      portmap->_offset += n;

	      if (portmap->_offset <= sizeof(_portmap_msg) + sizeof(uint32_t))
		continue;
	    }
	}

      // Client has exited, or at least got EPIPE -
      // close and remove it

      while (close(portmap->_fd) != 0)
	{
	  if (errno == EINTR)
	    continue;

	  perror("close");
	  ERR_MSG("Failure closing port request socket.");
	  break;
	}
      portmap->_prev->_next = portmap->_next;
      portmap->_next->_prev = portmap->_prev;

      free(portmap);

      CHECK_LIST_INTEGRITY;
    }

  // Accept new clients
  if (_ext_net_server_fd != -1)
    {
      if (FD_ISSET(_ext_net_server_fd,&readfds))
	ext_net_io_server_accept();
    }

  // Accept new clients
  if (_ext_net_data_serv_fd != -1)
    {
      if (FD_ISSET(_ext_net_data_serv_fd,&readfds))
	ext_net_io_data_serv_accept();
    }

  if (read_fd != -1)
    return FD_ISSET(read_fd,&readfds);
  if (write_fd != -1)
    return FD_ISSET(write_fd,&writefds);
  return false;
}

void ext_net_io_server_close()
{
  // Shutdown server more or less orderly.  Reason: if a server
  // (socket bind, listen & accept) shuts down before the clients
  // (socket connect()), then the port will be in use for some time
  // after the server is gone and a new server cannot connect.  By
  // making sure the clients go away orderly, i.e. they close the
  // connections first, we generally avoid this.

  if (_ext_net_clients._next != &_ext_net_clients)
    MSG("Shutdown.  Waiting for clients to finish...");

  // No further connections, please!

  if (_ext_net_server_fd != -1)
    {
      close(_ext_net_server_fd);
      _ext_net_server_fd = -1;
    }

  if (_ext_net_data_serv_fd != -1)
    {
      close(_ext_net_data_serv_fd);
      _ext_net_data_serv_fd = -1;
    }

  // We now serve the clients, as long as there is progress with a
  // timeout time.  As we have reached this point, they should have
  // gotten the shutdown message.  There is of course no guarantee
  // that they will honour it, so in shutdown mode, any client which
  // tells it's ready for writing with no more data to write, will be
  // closed as well.

  while (_ext_net_clients._next != &_ext_net_clients)
    {
      if (ext_net_io_select_clients(-1,-1,true,false))
	{
	  // We had a time-out.  Simply give up
	  MSG("Timeout...");
	  break;
	}
    }
}
