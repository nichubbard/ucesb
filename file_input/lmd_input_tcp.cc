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

#ifdef TEST_LMD_INPUT_TCP
#include <stdio.h>
#define ERROR(...)  { printf(__VA_ARGS__); printf("\n"); exit(1); }
#define INFO(x,...) { printf(__VA_ARGS__); printf("\n"); }
#define DEBUG_LMD_INPUT_TCP 1
#endif

#ifndef TEST_LMD_INPUT_TCP
#include "error.hh"
#endif

// We are concerned with the reading of LMD data over the net,
// possibly directly from MBS, i.e.  the --trans, --stream, --event
// input modes.  --rfio is something else, essentially a normal file
// system file call sequence, and handled elsewhere


// Protocol, as reverse engineered, from the eventapi/

// --event:

// OPEN: (f_evcli_con) (timeout default: 300 (TCP__TIMEOUT))
// node, port=6003(PORT__EVENT_SERV,l_gl_rev_port)
// sample=downscale factor

// socket (inet,stream,tcp)
// connect (node,port)

// write (filter specification (needs to be figured out))
//

// client filter structure (5*4+32*12+16*8+4*2):
// uint32  testbit;  // 0x00000001
// uint32  endian;   // 0x0 for little endian,
//                   // 0xffffffff for big endian (stupid!!)
// int32   numev;    // number of events to send
// int32   sample;   // downscale factor
// int32   flush;    // flush rate (seconds) (default: 3 FLUSH__TIME)
// struct filter[32 GPS__MAXFLT]
// --- ONLY part above is sent to server ---
// struct filter_descr[16 GPS__MAXFLTDESCR]
// uint16  filter_ev // filter on event
// uint16  filter_subev // filter on subevent
// uint16  write_ev  // write whole event
// uint16  write_subev // write subevents


// filter structure (3*4=12 bytes):
// uint32  pattern
// int32   offset
// uint32  opcode

// filter_descr structure (8 bytes):
// uint8   write_descr
// uint8   filter_descr
// uint8   filter_block_begin  // index into filter structure
// uint8   filter_block_end    // index into filter structure
// uint8   next_descr          // index into filter_descr structure
// uint8   dummy
// uint16  num_descriptors

// opcode structure:
// uint8   length             // length of filter
// uint8   next_filter_block  //
// uint8   filter_spec
// uint8   link_f2   : 1
// uint8   link_f1   : 1      // 1=and 0=or
// uint8   opc       : 3
// uint8   selwrite  : 1      // select write of event/subevent
// uint8   selfilter : 1      // select filter of event/subevent
// uint8   ev_subev  : 1      // 1:event 0:subev active for selection

// The select all-filter seems to be to set the opcode to
// 1 for ev_subev,selfilter,selwrite,
// 0 for opc,link_f2,link_f1,filter_spec
// 1 for next_filter_block,length

// write (filter specification (needs to be figured out)) writes what
// is above (till --- marker), using a send() call (flags =0, so
// equivalent to a write)

// read response, the read goes via a function that first select()s on
// the input socket, such that the timeout can be handled as soon as
// some bytes have been read, the timeout is set to 100 s (probably
// since we now MUST read, or we will be desyncronised)

// the response comes in a large data structure:
// which is first read the 512 (CLNT__SMALLBUF) first bytes

// event_client_buffer structure (s_clntbuf)
// uint32      testbit
// uint32      endian        // sender endian
// --- 8 bytes to here
// int32       dlen          // data length (bytes)
// int32       free          // free length (bytes)
// int32       num_events    // number of events in buffer
// int32       max_buf_size  // maximum buffer size
// int32       send_bytes    // bytes sent
// int32       send_buffers  // number of buffers to send
// int32       con_client(s) // currently connect(ed?) clients
// uint32      buffer_type   // 1:data 2:message 4:flush 8:last 16:first (inclusive/or)
// uint32      message_type  // 1:info 2:warn 4:error 8:fatal(?)
// --- 8+36=44 bytes to here
// char        message[256]
// --- 300 bytes to here
// uint32      read_count    // read buffers
// uint32      read_count_ok // good read buffers
// uint32      skip_count    // skipped buffers
// uint32      byte_read_count // read bytes
// uint32      count_bufs    // processed buffers
// uint32      count_events  // processed events
// --- 324 bytes to here
// uint32      sent_events
// uint32      sent_bytes
// uint32      send_buffer
// uint32      con_proc_events // processed events since connect
// uint32      filter_matches  // filter matched events
// --- 344 bytes to here
// char        output_buffer[16384] // data(?)

// the read routine copies the send_buffers, send_bytes, and
// buffer_type items.  Then it checks if testbit is 0x00000001, if
// not, swapping is needed and endian is checked for 0 (little
// endian), otherwise (unchecked), big-endian (0xffffffff we would
// assume) It then (if necessary) swaps the three items, checking for
// errors, that can never happen.  jaja.

// It then checks send_buffers.  If 1, then checks if send_bytes >
// 512, if so, gives an error message, and returns failure.  If not,
// goes to acknowledge stage.

// Calculates how much is left to read (send_bytes - 512), makes sure
// enough memory is available.  344+168=512 Continues to read data
// into output_buffer...  The reading is done in chunks of 16384
// bytes, with the routine which handles timeouts...  hmm...

// When done, and no errors occured, an acknowledge buffer is prepared
// (but not sent by this routine)

// acknowledge_buffer structure
// uint32   buffers           // same as send_buffers
// uint32   bytes             // same as send_bytes
// uint32   client_status     // 1=success#

// if buffer_type & 8 (i.e. last buffer), then a 8 is or'ed into the
// client_status as well (supposedly acknowledging that fact)

// side note (because not yet used): acknowledge send routine or's in
// the status argument to bits already set

// Then we're back to the connect routine, which now takes care of the
// recieved data...  swapping is checked...  i.e. testbit.  it is
// checked if it becomes better by swapping it.  Then there is
// something strange, the data_length is used unswapped together with
// the length of the 11 last data words to swap those and the data...
// I.e. the size of the swapped area might be completely wrong!!!

// Then there are empty-if-statements verifying (but without actions),
// that it is a first buffer and that it is a message buffer

// we never sent the acknowledge (on purpose)

// CONNECTION IS NOW DONE , OPEN returns

// GET_EVENT: (f_evcli_evt) if f_evcli_evt cannot get an event (buffer
// empty) f_evcli_buf is called to get events

// f_evcli_evt: simply get the next event from the data buffer,
// if there should be any more according to the number of events
// no checking is done to see if we are outside the buffer

// f_evcli_buf: starts by sending the acknowledge.  aha, acknowledge
// is sent when we're done processing, and can accept more data

// then the big read routine is called to get a new chunk of data
// 4-byte swapping is performed (a la last time)

// if there is a message, it is printed (unless it is ((info or
// warning) and (contain substring "no event data" or "flushed"))

// first event is extracted (or rather: pointer to it)

// READING BUFFER done

// CLOSING a CONNECTION

// send the acknowledge (with a 8, marking last) actually, while
// reading events, it's never checked if we got last buffer

// then disconnect and close stream
//

// disconnect calls close(fid)
// close first calls shutdown(socket,RDWR), then close (socket)

/////////////////////////////////////////////////////////////////////////
//                                                                     //
/////////////////////////////////////////////////////////////////////////

// stream/transport:

// OPEN:

// do the normal server connect (i.e. open a socket and file descriptor)
// then read a 16 byte structure

// stream_trans_open_info structure
// uint32   testbit          //  should be 0x00000001 (else swap)
// uint32   bufsize          //  size of buffer (bytes?)
// uint32   bufs_per_stream
// uint32   dummy

// the io_buf_size is then set to either bufsize (TRANSPORT)
// or bufsize*bufs_per_stream (STREAM SERVER)

// OPEN done


// CLOSE:

// stream server only:
// write "CLOSE" to the server, with 12 bytes length

// both:
// disconnect (file descriptor)
// close      (socket)


// READ A BUFFER:

// stream server only:
// write "GETEVT" to the server, with 12 bytes length
// loop over number of buffers in stream
//  read a buffer
// if first buffer is empty, then all are empty, (checked with l_evt of buffer header)
// if empty: complain about timeout (although we got data)

// transport only
//  read a buffer

// both: if l_free[0] of buffer header of first buffer is not 0x00000001, then
// do 4-byte swapping

// both: if l_evt of buffer header is negative, close connection


#include "lmd_input_tcp.hh"
#include "swapping.hh"
#include "lmd_event.hh"

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

// The LMD tcp connection input works as a front-end for the
// pipe_buffer.  Instead of reading directly from a file, we read data
// from ther tcp socket and place it into the buffer.  Care is taken
// not to start a transaction that would read data until we internally
// have buffer space available to hold it.  This way, we cannot lock
// the server up (even though it also is the servers responsibility to
// never allow itself to be locked up).

// Whenever an error or unexpected condition occurs, we print the
// entire state of the connection, in order to simplify debugging.
// (It is generally hard to recreate problems, as they depend on data
// etc)




lmd_input_tcp::lmd_input_tcp()
{
  _fd = -1;
}

lmd_input_tcp::~lmd_input_tcp()
{
  close_connection();
}

#define max(x,y) ((x)>(y)?(x):(y))

bool lmd_input_tcp::parse_connection(const char *server,
				     struct sockaddr_in *p_serv_addr,
				     uint16_t *port,
				     uint16_t default_port)
{
  struct hostent *h;
  char *hostname;

  *port = default_port;

  // if there is a colon in the hostname, interpret what follows as a
  // port number, overriding the default port
  {
    hostname = strdup(server);

    char *colon = strchr(hostname,':');

    if (colon)
      {
	int parsed_port;

	parsed_port = atoi(colon+1);
	if (parsed_port < 0 || parsed_port > 65535)
	  {
	    ERROR("Port out of range [0, 65535], is %d.", parsed_port);
	    return false;
	  }

	*port = (uint16_t) parsed_port;
	*colon = 0; // cut the hostname
      }

    /* get server IP address (no check if input is IP address or DNS name */
    h = gethostbyname(hostname);
    free(hostname);
  }
  if(h==NULL) {
    ERROR("Unknown host '%s'.",server);
    return false;
  }

  INFO(0,"Server '%s' known... (IP : %s) (port: %d).",
       h->h_name,
       inet_ntoa(*(struct in_addr *)h->h_addr_list[0]),
       *port);

  p_serv_addr->sin_family = (sa_family_t) h->h_addrtype;
  memcpy((char *) &p_serv_addr->sin_addr.s_addr,
	 h->h_addr_list[0], (size_t) h->h_length);
  // p_serv_addr->sin_port = htons(port);

  return true;
}

bool lmd_input_tcp::open_connection(const struct sockaddr_in *p_serv_addr,
				    uint16_t port, bool error_on_failure)
{
  int rc;
  struct sockaddr_in serv_addr;

  serv_addr = *p_serv_addr;
  serv_addr.sin_port = htons(port);

  /* socket creation */
  _fd = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (_fd < 0) {
    ERROR("Cannot open socket.");
    return false;
  }

  // Make the read file non-blocking to handle cases where the
  // read would be from some socket of sorts, that may actually
  // offer nothing to read, despite the select succesful (see linux
  // bug notes of select)


  if (fcntl(_fd,F_SETFL,fcntl(_fd,F_GETFL) | O_NONBLOCK) == -1)
    {
      perror("fcntl()");
      exit(1);
    }

  INFO(0,"Connecting port: %d",port);

  rc = ::connect(_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (rc < 0) {
    if (errno == EINPROGRESS)
      {
	struct timeval timeout;

	timeout.tv_sec  = 3;
	timeout.tv_usec = 0;

	// We are trying to connect presently.  See if we'll finish
	// at some point?

	for ( ; ; )
	  {
	    fd_set wfds;
	    FD_ZERO(&wfds);

	    int nfds = 0;

	    if (_fd != -1)
	      {
		FD_SET(_fd,&wfds);
		nfds = max(nfds,_fd);
	      }

	    int n = select(nfds+1,NULL,&wfds,NULL,&timeout);

	    if (n == -1)
	      {
		if (errno == EINTR)
		  {
		    // Since we may not rely on the file descriptors, or the
		    // timeout, act as if we were waked up, but do not read
		    // the pipe.  We'll quickly got to sleep again.
		    continue;
		  }
		perror("select()");
		// This error is fatal, since it should not happen
		exit(1);
	      }

	    if (n == 0)
	      {
		if (error_on_failure)
		  ERROR("timeout waiting for connect completion");
		else
		  {
		    WARNING("timeout waiting for connect completion");
		    return false;
		  }
	      }

	    if (_fd != -1 &&
		FD_ISSET(_fd,&wfds))
	      break;
	  }

	// So, time to investigate the success of the connect call...

	int connect_errno;
	int connect_errno_len = sizeof(connect_errno);

	rc = getsockopt(_fd,SOL_SOCKET,SO_ERROR,
			&connect_errno,(socklen_t*) &connect_errno_len);

	if (rc < 0) {
	  perror("getsockopt()");
	  exit(1);
	}
	if (!connect_errno)
	  goto connect_success;
	errno = connect_errno;
      }
    if (error_on_failure)
      {
	perror("connect()");
	ERROR("Cannot connect to port.");
      }
    return false;
  }
 connect_success:;
  return true;
}


void lmd_input_tcp::close_connection()
{
  if (_fd >= 0)
    {
      if (::close(_fd) == -1)
	ERROR("Failure closing connection.");
      _fd = -1;
    }
}



void lmd_input_tcp::do_read(void *buf,size_t count,int timeout)
{
  // If we are running threaded, then we'll wait indefinately for some
  // input... ?!?

  while (count)
    {
      // We must make sure that reading does not block...

      fd_set rfds;
      FD_ZERO(&rfds);

      int nfds = 0;

      if (_fd != -1)
	{
	  FD_SET(_fd,&rfds);
	  nfds = max(nfds,_fd);
	}

      struct timeval timewait;

      timewait.tv_sec  = timeout;
      timewait.tv_usec = 0;

      int n = select(nfds+1,&rfds,NULL,NULL,timeout >= 0 ? &timewait : NULL);

      if (n == -1)
	{
	  if (errno == EINTR)
	    {
	      // Since we may not rely on the file descriptors, or the
	      // timeout, act as if we were waked up, but do not read
	      // the pipe.  We'll quickly got to sleep again.
	      continue;
	    }
	  perror("select()");
	  // This error is fatal, since it should not happen
	  exit(1);
	}

      if (n == 0)
	{
	  ERROR("timeout during read");
	}

      if (_fd != -1 &&
	  FD_ISSET(_fd,&rfds))
	{
	  // We can read

	  ssize_t n = read(_fd,buf,count);

	  if (n == 0)
	    {
	      // Someone closed the writing end
	      ERROR("Socket closed on other end.");
	    }

	  if (n == -1)
	    {
	      // EAGAIN can happen if select fooled us (linux 'bug')
	      if (errno == EINTR || errno == EAGAIN)
		n = 0;
	      else
		{
		  perror("read()");
		  exit(1);
		}
	    }

	  buf = ((char *) buf) + n;
	  count -= (size_t) n;

	  if (timeout >= 0)
	    timeout = 100;
	}

    }
}



void lmd_input_tcp::do_write(void *buf,size_t count,int timeout)
{
  // Also the writes operate with timeout, since the stream server
  // may pile up the input...

  while (count)
    {
      // We must make sure that reading does not block...

      fd_set wfds;
      FD_ZERO(&wfds);

      int nfds = 0;

      if (_fd != -1)
	{
	  FD_SET(_fd,&wfds);
	  nfds = max(nfds,_fd);
	}

      struct timeval timewait;

      timewait.tv_sec  = timeout;
      timewait.tv_usec = 0;

      int n = select(nfds+1,NULL,&wfds,NULL,timeout >= 0 ? &timewait : NULL);

      if (n == -1)
	{
	  if (errno == EINTR)
	    {
	      // Since we may not rely on the file descriptors, or the
	      // timeout, act as if we were waked up, but do not read
	      // the pipe.  We'll quickly got to sleep again.
	      continue;
	    }
	  perror("select()");
	  // This error is fatal, since it should not happen
	  exit(1);
	}

      if (n == 0)
	{
	  ERROR("timeout during write");
	}

      if (_fd != -1 &&
	  FD_ISSET(_fd,&wfds))
	{
	  // We can read

	  ssize_t n = write(_fd,buf,count);

	  if (n == 0)
	    {
	      // Someone closed the writing end
	      ERROR("Write returned 0.");
	    }

	  if (n == -1)
	    {
	      // EAGAIN can happen if select fooled us (linux 'bug')
	      if (errno == EINTR || errno == EAGAIN)
		n = 0;
	      else
		{
		  perror("read()");
		  exit(1);
		}
	    }

	  buf = ((char *) buf) + n;
	  count -= (size_t) n;

	  if (timeout >= 0)
	    timeout = 100;
	}
    }
}



void lmd_input_tcp::create_dummy_buffer(void *buf,size_t count,
					int buffer_no, bool mark_dummy)
{
  if (count < sizeof (s_bufhe_host))
    ERROR("Destination (%d) is to small to hold dummy buffer (%d).",
	  (int) count,(int) sizeof (s_bufhe_host));

  s_bufhe_host *bufhe = (s_bufhe_host *) buf;

  memset(bufhe,0,sizeof (*bufhe));

  bufhe->l_dlen = (sint32) DLEN_FROM_BUFFER_SIZE(count);
  if (mark_dummy)
    {
      // Special markers...  not fool-proof, just to reduce the chance
      // that the reader triggers on other cases.

      bufhe->i_type    = LMD_DUMMY_BUFFER_MARK_TYPE;
      bufhe->i_subtype = LMD_DUMMY_BUFFER_MARK_SUBTYPE;
      bufhe->l_free[3] = LMD_DUMMY_BUFFER_MARK_FREE_3;
   }
  else
    {
      bufhe->i_type    = LMD_BUF_HEADER_10_1_TYPE;
      bufhe->i_subtype = LMD_BUF_HEADER_10_1_SUBTYPE;
    }
  bufhe->i_used = 0;
  bufhe->l_buf  = buffer_no;
  bufhe->l_evt  = 0;
  bufhe->l_free[0] = 0x00000001;
  bufhe->l_free[2] = 0;

  // printf ("dummy...\n");
}




size_t lmd_input_tcp_buffer::read_info(int *data_port)
{
  do_read(&_info,sizeof(_info));

  // ltcp_stream_trans_open_info _info;
  /*
  printf ("info: tb: %08x bs: %08x bps: %08x dmy: %08x\n",
	  _info.testbit,
	  _info.bufsize,
	  _info.bufs_per_stream,
	  _info.streams);
  */
  switch (_info.testbit)
    {
    case 0x00000001:
      break;
    case 0x01000000:
      byteswap((uint32*) &_info,sizeof(_info));
      break;
    default:
      ERROR("Buffer info endian marker broken: %08x",_info.testbit);
      break;
    }
  /*
  printf ("info: tb: %08x bs: %08x bps: %08x dmy: %08x\n",
	  _info.testbit,
	  _info.bufsize,
	  _info.bufs_per_stream,
	  _info.streams);
  */
  if (data_port &&
      (_info.streams & LMD_PORT_MAP_MARK_MASK) == LMD_PORT_MAP_MARK)
    {
      *data_port = _info.streams & LMD_PORT_MAP_PORT_MASK;
      INFO(0,"Redirected -> port %d", *data_port);
      return 0;
    }
  
  if (_info.bufsize == (uint32_t) LMD_TCP_INFO_BUFSIZE_NODATA)
    {
      // This should not happen to us!, since we know how to interpret the map
      ERROR("Buffer size -1, "
	    "hint that only port mapped clients allowed.");
    }

  if (_info.bufsize == (uint32_t) LMD_TCP_INFO_BUFSIZE_MAXCLIENTS)
    {
      ERROR("Buffer size -2, "
	    "hint that maximum number of clients are already connected.");
    }

  if (_info.bufsize % 1024)
    ERROR("Buffer size not multiple of 1024.");

  /* We have checks just to catch obviously stupid values. */

#define MAX_BUFFER_SIZE  0x08000000 // 128 MB
#define MAX_STREAM_SIZE  0x20000000 // 256 MB

  if (!_info.bufsize)
    ERROR("Refusing buffers with 0 size.  (connection was refused)");

  if (_info.bufsize > MAX_BUFFER_SIZE)
    ERROR("Cowardly refusing buffer size (%d bytes) "
	  "larger than %d MB.",
	  _info.bufsize, MAX_BUFFER_SIZE >> 20);

  if (_info.bufs_per_stream * _info.bufsize > MAX_STREAM_SIZE)
    ERROR("Cowardly refusing streams size (%d * %d = %d bytes) "
	  "larger than %d MB.",
	  _info.bufs_per_stream, _info.bufsize,
	  _info.bufs_per_stream * _info.bufsize,
	  MAX_STREAM_SIZE >> 20);

  return _info.bufs_per_stream * _info.bufsize;
}



size_t lmd_input_tcp_buffer::read_buffer(void *buf,size_t count,
					 int *nbufs)
{
  *nbufs = 0;

  if (count < _info.bufsize)
    {
      /*
      ERROR("Destination (%d) is to small to hold buffer (%d).",
	    (int) count,(int) _info.bufsize);
      */
      create_dummy_buffer(buf,count,
			  0/*this buffer no will be wrong, but...*/,true);

      // Set up a fake buffer with no content using up the space

      return count;
    }

  // we first make sure enough space is available in the output 'pipe'
  // memory

  do_read(buf,_info.bufsize);

  // Make sure enough of the buffer is intact, such that the lmd input
  // reader (also in case it detects an error) would be able to
  // resyncronize.  If not, we're to take the blow here...

  s_bufhe_host *buffer_header;

  buffer_header = (s_bufhe_host *) buf;

  // The only thing which has to be correct is the dlen.  If that is
  // ok, then resyncronisation will occur at the next buffer...
  // Well, before that, the swapping has to make sense...

  uint32 l_dlen = (uint32) buffer_header->l_dlen;
  uint32 l_evt = (uint32) buffer_header->l_evt;

  switch(buffer_header->l_free[0])
    {
    case 0x00000001:
      break;
    case 0x01000000:
      l_dlen = bswap_32(l_dlen);
      l_evt = bswap_32(l_evt);
      break;
    default:
      ERROR("Buffer header endian marker broken: %08x",_info.testbit);
      break;
    }

  size_t buffer_size_dlen = BUFFER_SIZE_FROM_DLEN(l_dlen);

  if (buffer_size_dlen != _info.bufsize)
    ERROR("Buffer size mismatch (buf:0x%x != info:0x%x).",
	  (int) buffer_size_dlen,_info.bufsize);

  // Check if it is the keep-alive buffer.  In that case, silently eat
  // it!  Careful: we have not been byte-swapped.  But the entries
  // used/end/begin are all in the same 32 bits, and should between
  // them all be 0!

  // It's no major disaster if the buffer slips through.  lmd_input
  // would complain at the buffer counters not being in order, but
  // should continue...

  // The buffer number seems to be quite random...

  *nbufs = 1;

  if (((sint32*) buffer_header)[2] == 0 && // used, end, begin
      l_evt == 0)
    {
      return 0;
    }

  // And again - be very careful to use the byteswapped value!

  if (((sint32) l_evt) < 0)
    *nbufs = -1;

  return _info.bufsize;
}

/////////////////////////////////////////////////////////////////////

size_t lmd_input_tcp_buffer::do_map_connect(const char *server,
					    int port_map_add,
					    uint16_t default_port)
{
  struct sockaddr_in serv_addr;
  uint16_t port;

  if (!lmd_input_tcp::parse_connection(server, &serv_addr, &port,
				       default_port))
    return false;

  int data_port = -1;
  size_t ret;

  if (port_map_add != -1)
    {
      // First try with port that only might provide mapping.
      uint16_t conn_port = (uint16_t) (port + port_map_add);
      if (lmd_input_tcp_buffer::open_connection(&serv_addr, conn_port, false))
	ret = lmd_input_tcp_buffer::read_info(&data_port);
    }

  if (data_port == -1)
    {
      lmd_input_tcp_buffer::open_connection(&serv_addr, port, true);
      ret = lmd_input_tcp_buffer::read_info(port_map_add == -1 ?
					    &data_port : NULL);
    }

  if (data_port != -1)
    {
      lmd_input_tcp_buffer::close_connection();
      lmd_input_tcp_buffer::open_connection(&serv_addr, (uint16_t) data_port,
					    true);
      ret = lmd_input_tcp_buffer::read_info(NULL);
    }

  return ret;
}

/////////////////////////////////////////////////////////////////////

size_t lmd_input_tcp_transport::connect(const char *server)
{
  return do_map_connect(server,
			LMD_TCP_PORT_TRANS_MAP_ADD,
			LMD_TCP_PORT_TRANS);
}

void lmd_input_tcp_transport::close()
{
  lmd_input_tcp_buffer::close_connection();
}

size_t lmd_input_tcp_transport::preferred_min_buffer_size()
{
  // We'd like to output all the data into one buffer

  return _info.bufsize;
}

size_t lmd_input_tcp_transport::get_buffer(void *buf,size_t count)
{
  for ( ; ; )
    {
      int nbufs;

      size_t n = lmd_input_tcp_buffer::read_buffer(buf,count,&nbufs);

      if (nbufs == -1)
	{
	  WARNING("Got disconnect buffer from tcp transport.");
	  return 0;
	}

      if (n)
	return n;

      if (!nbufs)
	{
	  // As a transport server _never_ should send any completely
	  // empty buffer, there is no valid reason for
	  // lmd_input_tcp_buffer::read_buffer to return 0, so this is
	  // a true problem, signal it as such
	  WARNING("Got zero size buffer from tcp transport.");
	  break; // we ran out of data!
	}
    }
  return 0;
}

/////////////////////////////////////////////////////////////////////

lmd_input_tcp_stream::lmd_input_tcp_stream()
{
  _buffers_to_read = 0;
}

size_t lmd_input_tcp_stream::connect(const char *server)
{
  return do_map_connect(server, -1, LMD_TCP_PORT_STREAM);
}

void lmd_input_tcp_stream::close()
{
  // Tell the stream server we're done

  char request[12];

  memset(request,0,sizeof (request));
  strcpy(request,"CLOSE");

  do_write(request,sizeof (request));

  // And close!

  lmd_input_tcp_buffer::close_connection();
}

size_t lmd_input_tcp_stream::preferred_min_buffer_size()
{
  // We'd like to output all the data into one buffer

  return _info.bufs_per_stream * _info.bufsize;
}

size_t lmd_input_tcp_stream::get_buffer(void *buf,size_t count)
{
  for ( ; ; )
    {
      // Every n'th buffer, before starting to read the buffer, we must
      // ask the stream server (kindly)

      if (_buffers_to_read <= 0)
	{
	  char request[12];

	  memset(request,0,sizeof (request));
	  strcpy(request,"GETEVT");

	  do_write(request,sizeof (request));

	  _buffers_to_read += _info.bufs_per_stream;
	}

      // We ought to read all the buffers of a stream at once, when we
      // have requested the data...

      size_t total = 0;

      while (_buffers_to_read > 0)
	{
	  int nbufs;

	  size_t n = lmd_input_tcp_buffer::read_buffer(buf,count,&nbufs);

	  if (nbufs == -1)
	    {
	      WARNING("Got disconnect buffer from stream transport.");
	      return 0;
	    }

	  _buffers_to_read -= nbufs;

	  buf   = ((char *) buf) + n;
	  total += n;
	  count -= n;

	  if (!count)
	    break; // no point in trying again

	  // This cannot happen.  It would either return 0, or it would
	  // return 0 buffers but n != 0 when discarding end of linear
	  // space
	  assert (!(n == 0 && nbufs == 0));
	}

      // a special valid case of total = 0 may happen: if we got a flush,
      // but also in the middle wrapped the linear space then we for the
      // second call may handle only empty buffers.  those will be
      // silently eaten, and thus we produce no data, but we did discard
      // buffers.  As the recepients of our return call treat total == 0
      // as an error, we must in this case try to read data again...


      // INFO(0,"Got %d bytes",total);

      if (total)
	return total;

      // since we got out of the while loop above, nbuf has returned
      // non-zero, or _buffers_to_read would never reach 0
    }
}

/////////////////////////////////////////////////////////////////////

lmd_input_tcp_event::lmd_input_tcp_event()
{
  _bytes_left = 0;
  _data_left = 0;

  _buffer_no = 1;
}

#ifndef offsetof
#define offsetof(type,member) ((size_t) &((type *) 0)->member)
#endif

size_t lmd_input_tcp_event::read_client_header()
{
  do_read(&_header,sizeof(_header));
  /*
  printf ("header: tb: %08x  end: %08x  dlen: %08x  free: %08x\n",
	  _header.testbit,
	  _header.endian,
	  _header.dlen,
	  _header.free);
  */
  _swapping = false;

  switch (_header.testbit)
    {
    case 0x00000001:
      break;
    case 0x01000000:
      // everything is uint 32 except the message
      byteswap((uint32*) &_header,
	       offsetof(ltcp_event_client_struct,message));
      byteswap((uint32*) &_header.read_count,
	       sizeof (_header) - offsetof(ltcp_event_client_struct,read_count));
      _swapping = true;
      break;
    default:
      ERROR("Buffer info endian marker (testbit) broken: %08x",_header.testbit);
      break;
    }
  /*
  printf ("header: tb: %08x  end: %08x  dlen: %08x  free: %08x\n",
	  _header.testbit,
	  _header.endian,
	  _header.dlen,
	  _header.free);
  */
  /*
  printf ("testbit         %08x (%d)\n",_header.testbit	  	,_header.testbit	);
  printf ("endian          %08x (%d)\n",_header.endian          ,_header.endian         );

  printf ("dlen            %08x (%d)\n",_header.dlen            ,_header.dlen           );
  printf ("free            %08x (%d)\n",_header.free            ,_header.free           );
  printf ("num_events      %08x (%d)\n",_header.num_events      ,_header.num_events     );
  printf ("max_buf_size    %08x (%d)\n",_header.max_buf_size    ,_header.max_buf_size   );
  printf ("send_bytes      %08x (%d)\n",_header.send_bytes      ,_header.send_bytes     );
  printf ("send_buffers    %08x (%d)\n",_header.send_buffers    ,_header.send_buffers   );
  printf ("con_clients     %08x (%d)\n",_header.con_clients     ,_header.con_clients    );
  printf ("buffer_type     %08x (%d)\n",_header.buffer_type     ,_header.buffer_type    );
  printf ("message_type    %08x (%d)\n",_header.message_type    ,_header.message_type   );

  _header.message[255] = 0; // safety

  printf ("message[256]:\n%s\n",_header.message);

  printf ("read_count      %08x (%d)\n",_header.read_count      ,_header.read_count     );
  printf ("read_count_ok   %08x (%d)\n",_header.read_count_ok   ,_header.read_count_ok  );
  printf ("skip_count      %08x (%d)\n",_header.skip_count      ,_header.skip_count     );
  printf ("byte_read_count %08x (%d)\n",_header.byte_read_count ,_header.byte_read_count);
  printf ("count_bufs      %08x (%d)\n",_header.count_bufs      ,_header.count_bufs     );
  printf ("count_events    %08x (%d)\n",_header.count_events    ,_header.count_events   );

  printf ("sent_events     %08x (%d)\n",_header.sent_events	,_header.sent_events	);
  printf ("sent_bytes      %08x (%d)\n",_header.sent_bytes	,_header.sent_bytes	);
  printf ("send_buffer     %08x (%d)\n",_header.send_buffer	,_header.send_buffer	);
  printf ("con_proc_events %08x (%d)\n",_header.con_proc_events ,_header.con_proc_events);
  printf ("filter_matches  %08x (%d)\n",_header.filter_matches  ,_header.filter_matches  );
  */

  // We should now take care of and propagate any message...
  // please note: it may be a multi-line message


  // Do some sanity checking on the contents

  if (_header.send_buffers == 1 &&
      _header.send_bytes > 512)
    ERROR("Unexpected send_bytes (%d) for send_buffers = 1.",
	  _header.send_bytes);

  // Basically the remainder of the data is to contain simply events.
  // So, we'll have the get_buffer routine just copy that amount of
  // data into the pipe (with) a crafted buffer header (to let the
  // lmd_input.cc do it's normal tricks on records)

  // This check is just to prevent obviously crazy data.

#define MAX_EVENT_BUFFER_SIZE  0x08000000 // 128 MB

  if (_header.send_bytes > MAX_EVENT_BUFFER_SIZE) // buffer larger than 1 MB
    ERROR("Cowardly refusing event buffer larger than %d MB.",
	  MAX_EVENT_BUFFER_SIZE >> 20);

  // Workaround for bad (keepalive??) messages from the mrevservnew
  if (_header.num_events == 0 && _header.dlen < 0)
    {
      // WARNING("Workaround! (%d = %x)",_header.dlen,_header.dlen);
      _bytes_left = _header.send_bytes - sizeof(_header);
      _data_left = 0;
      return 0;
    }

  if (sizeof(_header) + (size_t) _header.dlen > _header.send_bytes)
    ERROR("Header (%d) + data length (%d) > sent bytes (%d).  (evs=%d)",
	  (int) sizeof(_header),_header.dlen,_header.send_bytes,
	  _header.num_events);

  if ((!_header.dlen) != (!_header.num_events))
    ERROR("Data length being zero != number of events being zero.");

  _bytes_left = _header.send_bytes - sizeof(_header);
  _data_left  = (size_t) _header.dlen;

  // When called the first time, return value is used as suggestion for
  // the prefetch buffer size.

  size_t minsize = _header.send_bytes;

  if (minsize < (size_t) _header.max_buf_size)
    minsize = (size_t) _header.max_buf_size;

  return minsize;
}

void lmd_input_tcp_event::send_acknowledge()
{
  ltcp_event_client_ack_struct ack;

  ack.buffers       = (uint32) _header.send_buffers;
  ack.bytes         =          _header.send_bytes;
  ack.client_status = 1;

  do_write(&ack,sizeof (ack));
}

size_t lmd_input_tcp_event::connect(const char *server)
{
  struct sockaddr_in serv_addr;
  uint16_t port;
  
  // First establish connection
  if (!lmd_input_tcp::parse_connection(server, &serv_addr, &port,
				       LMD_TCP_PORT_EVENT) ||
      !lmd_input_tcp::open_connection(&serv_addr, port, true))
    return false;

  // Then we are to setup, specify and send a filter description...

  ltcp_base_filter_struct base_filter;

  memset(&base_filter,0,sizeof(base_filter));

  base_filter.testbit = 0x00000001;
#if __BYTE_ORDER == __LITTLE_ENDIAN
  base_filter.endian = 0;
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  base_filter.endian = 0xffffffff;
#endif
  base_filter.numev = -1;
  base_filter.sample = 1; /* downscale */
  base_filter.flush = 0; /* s */

  base_filter.items[0].opcode.length            = 1;
  base_filter.items[0].opcode.next_filter_block = 1;
  base_filter.items[0].opcode.filter_spec       = 0;
  base_filter.items[0].opcode.link_f2           = 0;
  base_filter.items[0].opcode.link_f1           = 0;
  base_filter.items[0].opcode.opc               = 0;
  base_filter.items[0].opcode.selwrite          = 1;
  base_filter.items[0].opcode.selfilter         = 1;
  base_filter.items[0].opcode.ev_subev          = 1;

  do_write(&base_filter,sizeof (base_filter));

  // After this, we should get some response

  return read_client_header();
}

void lmd_input_tcp_event::close()
{
  /*
  lmd_input_tcp_buffer::close_connection();
  */
}

size_t lmd_input_tcp_event::preferred_min_buffer_size()
{
  // We'd like to output all the data into one buffer

  return _data_left;
}

size_t lmd_input_tcp_event::get_buffer(void *buf,size_t count)
{
  for ( ; ; )
    {
      if (!_bytes_left)
	{
	  // there were no bytes left, we need to send an acknowledge, and
	  // get a new header

	  send_acknowledge();
	  read_client_header();
	}

      // now, is there space in the pipe to hold all the data?.  If not,
      // we'll dummy the space away and get another

      if (_data_left)
	{
	  // So there is some data.  Let's format ourselves a buffer...

	  if (count < _data_left)
	    {
	      /*
	      WARNING("Destination (%d) is to small to hold data+header (%d+%d).",
		      (int) count,_data_left,(int) sizeof(s_bufhe_host));
	      */

	      create_dummy_buffer(buf,count,(sint32) _buffer_no++,false);
	      /*
		// dummy buffers need no swapping
	      if (_swapping)
		byteswap((uint32*) bufhe,sizeof(*bufhe));
	      */
	      return count;
	    }

	  // Now, the problem is that the data will be in the byte
	  // order of the sending host.  (must be).  The normal input
	  // routine will infer that from the header.  So we'll be
	  // ugly and produce a header in native order, and swap it
	  // (if necessary...) to match the server

	  s_bufhe_host *bufhe = (s_bufhe_host *) buf;

	  size_t data_left_aligned =
	    ((_data_left + sizeof (*bufhe) + 0x3ff) & (size_t) ~0x3ff) - sizeof (*bufhe);

	  memset(bufhe,0,sizeof (*bufhe));

	  bufhe->l_dlen = (sint32) DLEN_FROM_BUFFER_SIZE(sizeof (*bufhe) + data_left_aligned);
	  bufhe->i_type = LMD_BUF_HEADER_10_1_TYPE;
	  bufhe->i_subtype = LMD_BUF_HEADER_10_1_SUBTYPE;
	  bufhe->l_buf  = (sint32) _buffer_no++;
	  bufhe->l_evt  = _header.num_events;
	  bufhe->l_free[0] = 0x00000001;
	  bufhe->l_free[2] = (sint32) IUSED_FROM_BUFFER_USED(_data_left);

	  if (bufhe->l_dlen <= LMD_BUF_HEADER_MAX_IUSED_DLEN)
	    bufhe->i_used = (sint16) bufhe->l_free[2];

	  if (_swapping)
	    byteswap((uint32*) bufhe,sizeof(*bufhe));

	  buf = ((char *) buf) + sizeof(*bufhe);
	  count -= sizeof(*bufhe);

	  size_t total = sizeof(*bufhe);
	  /*
	  printf ("Produced data buffer (%d+(%d -> %d))\n",
		  sizeof(*bufhe),_data_left,data_left_aligned);
	  */
	  // Then, get ourselves the data...

	  do_read(buf,_data_left);

	  _bytes_left -= _data_left;
	  _data_left = 0;

	  total += data_left_aligned;

	  return total;
	}

      // if there is still some dummy data to eat, then let's chew that

      while (_bytes_left)
	{
	  char skip_buf[4096];

	  size_t skip = _bytes_left;

	  if (skip > sizeof(skip_buf))
	    skip = sizeof(skip_buf);

	  do_read(skip_buf,skip);

	  _bytes_left -= skip;

	  // printf ("Ate %d bytes. (%d left)\n",skip,_bytes_left);
	}
    }
}

/////////////////////////////////////////////////////////////////////



#ifdef TEST_LMD_INPUT_TCP

// For testing:
// g++ -g -DTEST_LMD_INPUT_TCP -o lmd_input_tcp -x c++ lmd_input_tcp.cc -I ../eventloop
// ./lmd_input_tcp --event=localhost
// ./lmd_input_tcp --trans=localhost
// ./lmd_input_tcp --stream=localhost

int main(int argc,char *argv[])
{
#if 0
  lmd_input_tcp_transport trans;

  trans.connect("localhost",6000);

  for (int i = 0; i < 500; i++)
    trans.get_buffer();

  trans.close();
#endif


  lmd_input_tcp_stream stream;

  stream.connect("localhost",6002);

  for (int i = 0; i < 500; i++)
    stream.get_buffer();

  stream.close();
}

#endif//TEST_LMD_INPUT_TCP
