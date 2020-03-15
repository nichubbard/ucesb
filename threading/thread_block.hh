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

#ifndef __THREAD_BLOCK_HH__
#define __THREAD_BLOCK_HH__

#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "error.hh"
#include "optimise.hh"

#include "thread_debug.hh"

#define TOKEN_OPEN_FILE          0x01000000
#define TOKEN_FILES_TO_OPEN_MASK 0x00ffffff // next file to open

#if USE_PTHREAD
#define IF_USE_PTHREAD(x) x
#else
#define IF_USE_PTHREAD(x)
#endif

class thread_block
{
public:
  thread_block()
  {
    // _need_wakeup = false;

    for (int i = 0; i < 2; i++)
      _fd_wakeup[i] = -1;
  }

  ~thread_block()
  {
    for (int i = 0; i < 2; i++)
      if (_fd_wakeup[i] != -1)
	close(_fd_wakeup[i]);
  }

public:
  //bool _need_wakeup;
  int  _fd_wakeup[2];

public:
  void init()
  {
    //assert (!_need_wakeup);

    if (pipe(_fd_wakeup) == -1)
      {
	perror("pipe");
	ERROR("Error creating pipe.");
      }

    TDBG("rd:%d wr:%d",_fd_wakeup[0],_fd_wakeup[1]);
   }

  ////////////////////////////////////////////////////////
  // Called by the thread itself:
public:
  // void request_wakeup() { _need_wakeup = true;  MFENCE; }
  // void cancel_wakeup()  { _need_wakeup = false; }

public:
  int setup_select(int nfd,fd_set *readfds) const
  {
    FD_SET(_fd_wakeup[0],readfds);

    if (nfd < _fd_wakeup[0])
      nfd = _fd_wakeup[0];

    return nfd;
  }

public:
  int block(int nfd,fd_set *readfds,struct timeval *timeout) const
  {
    TDBG("nfd:%d",nfd);

    nfd = setup_select(nfd,readfds);

    TDBG("nfd:%d",nfd);

    int n = select(nfd+1,readfds,NULL,NULL,timeout);

    if (n == -1)
      {
	if (errno == EINTR)
	  {
	    // Since we may not rely on the file descriptors, or the
	    // timeout, act as if we were waked up, but do not read
	    // the pipe.  We'll quickly got to sleep again.
	    return 0;
	  }
	perror("select()");
	// This error is fatal, since it should not happen
	exit(1);
      }

    // as there may have been spurious tokens, we try to read more
    // than one to quickly empty the pipe.  But also 1 would work,
    // just cause us to go to sleep and wake an extra time if extra
    // tokens were there

    if (!FD_ISSET(_fd_wakeup[0],readfds))
      {
	TDBG("timeout");
	return 0;
      }

    // There is data available in the readout buffer

    return get_token();
  }

  void block() const
  {
    fd_set rfds;

    FD_ZERO(&rfds);

    block(-1,&rfds,NULL);
  }

  bool has_token(fd_set *readfds,int *token) const
  {
    if (!FD_ISSET(_fd_wakeup[0],readfds))
      return false;

    *token = get_token();

    return true;
  }

  int get_token() const
  {
    for ( ; ; )
      {
	int token;

	ssize_t n = read(_fd_wakeup[0],&token,sizeof(token));

	TDBG("n:%d",n);

	assert(!((size_t) n & (sizeof(token)-1))); // even multiple of token (which must be power of 2)

	if (n >= 1)
	  return token;

	if (n == -1)
	  {
	    if (errno == EINTR)
	      continue; // try again
	    // We failed to write, for no good reason!
	    // EPIPE could happen, but we never close the reading end
	    // before the threads are done, so cannot happen!!
	    perror("write");
	  }
	ERROR("Failed to read from wakeup pipe.  strange");
	// The error reports may not make it to the output.  Bypass...
	fprintf (stderr,"Failed to write to wakeup pipe.  strange\n");
	exit(1);
      }
  }

  ////////////////////////////////////////////////////////
  // Called by any (other) thread, wishing to wake it up
public:
  // bool need_wakeup() { return _need_wakeup; }

  void wakeup(int token = 0) const
  {
    //_need_wakeup = false; // we _will_ wake him up

    TDBG("wr:%d",_fd_wakeup[1]);

    // SFENCE;

    // Write one token to wake the thread up

    for ( ; ; )
      {
	ssize_t n = write(_fd_wakeup[1],&token,sizeof(token));

	if (n == sizeof(token))
	  break;

	if (n == -1)
	  {
	    if (errno == EINTR)
	      continue; // try again
	    // We failed to write, for no good reason!
	    // EPIPE could happen, but we never close the reading end
	    // before the threads are done, so cannot happen!!
	    perror("write");
	  }
	// The error reports may not make it to the output.  Bypass...
	fprintf (stderr,"Failed to write to wakeup pipe.  DEADLOCK\n");
	abort();
      }
  }

};

#endif//__THREAD_BLOCK_HH__


