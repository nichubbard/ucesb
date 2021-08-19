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

#include "pipe_buffer.hh"
#include "error.hh"
#include "set_thread_name.hh"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/select.h>

// Principle of operation:
//
// We have a buffer, which is used to read data from the pipe from the
// external source.  The buffer is also used when unpacking the
// events, i.e.  we cannot reuse the buffer until release_to(end)
// has been called.  When a certain range is requested for allocation,
// we'll make sure that range is available as a continous stretch of
// memory, (for this class) without affecting any previously given
// ranges (after any call to release_to).  (the direct file mmap
// class may need to reorganize the memory pointers)

// Reorganising the memory pointers is NOT allowed, unless we are not
// running in SMP mode.  In SMP mode we will have given away so many
// pointers within the already mapped area that it is impossible to
// change the mapping.  Besides, that would destroy the entrire idea
// of the SMP decoupling of the different tasks to be done.


// For the thread reading from the pipe (writing buffer), we'll:
//
// * Check to see if we have buffer space available
//   - If no space is available, we must sleep/wait/select(signal)
//     until told by consumer (release_to)
//   - When space is available, select for reading on the incoming pipe
//     when data is available, read as much as we get, update the write
//     pointer.
//     We might have to tell the consumer that data got available.

// For the thread unpacking data (reading buffer), our interface is
// via the map_range and release_to functions:
//
// release_to:
// * Update the read pointer telling that more space is available,
//   - If the write pointer had reached the read pointer, we
//     signal the write thread that there is space available
//
// map_range:
// * Check if the requested range is available
//   - If not available, wait for the producer to make it available
//   - If available, make sure it is continous.  If non-continous (i.e. at
//     end of buffer and wrapping to starting end, copy data to
//     a temporary buffer such that the range is continous.


// Since we never want to empty all the event processing threads just
// because a new file is opened, we'll open files (and buffer their
// contents) long before they start to be used.  However, the next
// stage in the process must still be able to correctly handle the EOF
// condition on the previous file, i.e. should not notice this
// handling.  An extra complication comes from the fact that we'd like
// to use the _same_ buffer.  If we for each file allocate a new
// buffer, the operating system will have to give us a new set of
// pages, which it must have cleaned out before our using them, and
// they have to be faulted into our process and so on.  If that can be
// avoided, we'd like to do that.







#ifdef USE_PTHREAD

void *pipe_buffer_base::reader_thread(void *us)
{
  return ((pipe_buffer_base *) us)->reader();
}


#define max(x,y) ((x)>(y)?(x):(y))

void *pipe_buffer::reader()
{
  sigset_t sigmask;

  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGINT);

  pthread_sigmask(SIG_BLOCK,&sigmask,NULL);

  // Make the read file non-blocking to handle cases where the
  // read would be from some socket of sorts, that may actually
  // offer nothing to read, despite the select succesful (see linux
  // bug notes of select)

  if (fcntl(_fd,F_SETFL,fcntl(_fd,F_GETFL) | O_NONBLOCK) == -1)
    {
      perror("fcntl()");
      exit(1);
    }

  for ( ; ; )
    {
      fd_set rfds;
      FD_ZERO(&rfds);

      int nfds = 0;

      assert((ssize_t) (_avail - _done) >= 0);

      if (_avail - _done >= _size)
	{
	  // Buffer is currently FULL, no need to issue a read
	  // until we get told (via SIGNAL) that _done has moved
	  // forward

	  MFENCE; // make sure we do not reorder

	  // Put in a bit of hysterises, to not wake the
	  // the reader up too often

	  _wakeup_done = _avail - _size +  (_size >> 4);
	  MFENCE;
	  _need_reader_wakeup = &_block;

	  // Now, here would be a race condition.  Assume, we find
	  // buffer to be full...  Then the consumer empties the
	  // entire buffer at once (checking our needs signal marker,
	  // finding it false), and buffer is now empty, no more
	  // checking will happen.  Only then we set our flag.  This
	  // is now too late.  Will never be checked and we're stuck.

	  MFENCE; // make sure we do not reorder

	  // So we check once more if we are really empty, this way,
	  // if the consumer cleaned the buffer at the first MFENCE
	  // above, and no signal was sent, it does not matter, because
	  // we check it here.  (a spurious signal will be sent, but that
	  // does not matter)

	  assert((ssize_t) (_avail - _done) > 0);

	  if (_avail - _done >= _size)
	    goto buffer_full;
	}

      // Buffer not full

      if (_fd != -1)
	{
	  FD_SET(_fd,&rfds);
	  nfds = max(nfds,_fd);
	}

    buffer_full:
      /*
      printf ("_avail: %08zx  _done: %08zx  _size: %08zx  (need_wakeup:%d)\n",
	      _avail,_done,_size,_need_consumer_wakeup != NULL);
      */
      // int n = select(nfds+1,&rfds,NULL);

      _block.block(nfds,&rfds,NULL);

      if (_fd != -1 &&
	  FD_ISSET(_fd,&rfds))
	{
	  // We can read

	  // How much space is free in buffer

	  size_t space  = _size - (_avail - _done);
	  size_t offset = _avail & (_size - 1);

	  // But in case of wraparound, i.e. when write pointer
	  // is not at the buffer start

	  size_t segment = _size - offset;

	  if (segment > space)
	    segment = space;

	  ssize_t n = read(_fd,_buffer + offset,segment);

	  if (n == 0)
	    {
#ifdef USE_THREADING
	      request_next_file();
#endif

	      // printf ("Reached eof, _avail: %08x\n",_avail);

	      _reached_eof = true;

	      MFENCE;

	      if (_need_consumer_wakeup)
		{
		  // The consumer was waiting for us.  wake him up to
		  // tell him that data till never be available :-(

		  const thread_block *blocked =
		    (const thread_block *) _need_consumer_wakeup;
		  _need_consumer_wakeup = NULL;
		  SFENCE;
		  blocked->wakeup();
		}

	      break;

	      // We are done reading.

	      ::close(_fd);
	      _fd = -1;

	      // printf ("read close\n",n);
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

	  _avail += (size_t) n;

	  if (_need_consumer_wakeup &&
	      ((ssize_t) (_avail - _wakeup_avail)) >= 0)
	    {
	      // The consumer was waiting for us.

	      const thread_block *blocked =
		(const thread_block *) _need_consumer_wakeup;
	      _need_consumer_wakeup = NULL;
	      SFENCE;
	      blocked->wakeup();
	    }
	}
    }

  return NULL;
}
#else//!USE_PTHREAD
int pipe_buffer::read_now(off_t end)
{
  // We have no reader thread.  We are responsible for making sure the
  // requested range exist in the buffer

  while (((ssize_t) _avail - (ssize_t) end) < 0)
    {
      if (_reached_eof)
	return 0; // data requested is NOT available

      size_t space  = _size - (_avail - _done);
      size_t offset = _avail & (_size - 1);

      size_t segment = _size - offset;
      size_t need = end - _avail;

      if (UNLIKELY(space <= 0))
	ERROR("pipe_buffer too small");

      if (segment > space)
	segment = space;

      if (segment > need)
	segment = need;
      /*
      printf ("offset: %08x, segment: %08x\n",
	      (int) offset,(int) segment);
      */
      ssize_t n = read(_fd,_buffer + offset,segment);

      if (n == 0)
	{
	  _reached_eof = true;
	  continue;
	}

      if (n == -1)
	{
	  if (errno == EINTR)
	    n = 0;
	  else
	    {
	      perror("read()");
	      ERROR("Error while reading");
	    }
	}

      _avail += (size_t) n;
    }
  return 1;
}
#endif//!USE_PTHREAD


int pipe_buffer_base::map_range(off_t start,off_t end,buf_chunk chunks[2])
{
  // The requested range must be in memory!

  // Hmm, first of all, the start offset may not be before what has
  // been returned before, because then we're toast...  :-(


  // Now, assuming that everything is fine, we just have to make sure
  // that data is available for the range up till end
  /*
  printf("_avail: %08x  start: %08x  end: %08x\n",
	 (int)_avail,(int)start,(int)end);
  */
#ifdef USE_PTHREAD
  ssize_t enough = (ssize_t) _avail - (ssize_t) end;

  while (enough < 0)
    {
      // We have to sleep here, waiting for data to become available
      // in the memory buffer.  Of course, we have no clue how long
      // that may take...  (also, it can easily be that we have
      // requested so much that, such amounts will not be available by
      // the first read done by the reader.  And there is no sense in
      // waking us up before all that is needed is available)

      MFENCE;
      _wakeup_avail = (size_t) end;
      MFENCE;
      // This is used EXCLUSIVELY by the event reader thread, so we
      // use his block/unblocker pipe
      _need_consumer_wakeup = _block_reader;
      MFENCE;

      // same story as for the reader versus release with a deadlock,
      // we check once more after we know that the reader must have
      // known to tell us

      enough = (ssize_t) _avail - (ssize_t) end;

      if (enough >= 0)
	break; // data is available

      // Have we perhaps reached EOF?  (note, update of avail and
      // reached_eof cannot reorder, since they are so far apart)

      if (_reached_eof)
	return 0; // data requested is NOT available

      // put up a select call to wait for the signal to appear

      //printf ("... wait for data ...\n");

      _block_reader->block();
    }
#else//!USE_PTHREAD
  // printf ("need read_now(end=%d)\n",end);
  if (!read_now(end))
    return 0;
#endif//!USE_PTHREAD

  _front = (size_t) end;

  /*
  printf ("_avail: %08x  start: %08x  end: %08x\n",
  	  (int)_avail,(int)start,(int)end);
  */
  // Ok, data is available (have been read in).  Now see if we're
  // are lucky and it exists in one chunk.

  size_t offset = ((size_t) (start)) & (_size - 1);
  size_t size   = ((size_t) (end - start));
  size_t segment = _size - offset;

  if (size <= segment)
    {
      // We have good luck.  It's continous

      chunks[0]._ptr    = _buffer + offset;
      chunks[0]._length = size;
      /*
      for (size_t i = 0; i < chunks[0]._length; i++)
	{
	  printf ("%02x ",(int) (unsigned char) chunks[0]._ptr[i]);
	}
      printf ("\n");
      */
      return 1;
    }

  // The data we want is divided into two parts.  One starting at
  // @offset with length @segment, and one starting at 0, with length
  // @segment - @size

  // Now, either we need to make a copy here, or we have the rest of
  // the code prepared to handle broken segments (two of them).  The
  // lmd subevent reader is anyhow also able to deal with fragments,
  // since that is fairly common there (if fragments exist) to have to
  // glue them back together.  Also...

  // If the rest of the code can handle fragments, then also the mmap
  // operation may use fragments, and we also there get rid of a many
  // locking conditions.  (at the expense of the mmap()er handling
  // several mmap()ed ranges (much better).

  // Also, simply returning two fragments is a constant cost solution,
  // since we do not copy data.  It means that events which finally get
  // ignored ar handled as cheaply as possible, i.e. minimum copy operations

  chunks[0]._ptr    = _buffer + offset;
  chunks[0]._length = segment;
  chunks[1]._ptr    = _buffer;
  chunks[1]._length = size - segment;
  /*
  printf ("ptr: %08x  len: %08x  ptr: %08x  len: %08x  \n",
	  (int)chunks[0]._ptr,(int)chunks[0]._length,
	  (int)chunks[1]._ptr,(int)chunks[1]._length);
  */
  return 2;
}

void pipe_buffer_base::release_to(off_t end)
{
  // We're done with all data up to the @end location

  // This means that we can move the read pointer forward, and if
  // necessary, also release the thread doing the reading

  _done = (size_t) end;

  // Now, if the reader has requested a signal to be sent, we'll do
  // that.  We need not check if we actually emptied part of the
  // buffer, since if he gets a signal, he'll just wake up.  After
  // which it checks if there is space in the buffer.  If not, another
  // signal will be requested.

#ifdef USE_PTHREAD
  if (_need_reader_wakeup &&
      ((ssize_t) (_done - _wakeup_done)) >= 0)
    {
      const thread_block *blocked = (const thread_block *) _need_reader_wakeup;
      _need_reader_wakeup = NULL;
      SFENCE; // make sure we do not reset the signal flag after
              // sending the signal, though
      blocked->wakeup();
    }
#endif
}


#ifdef USE_THREADING
void pipe_buffer_base::arrange_release_to(off_t end)
{
  pbf_reclaim *pbfr =
    _wt._defrag_buffer->allocate_reclaim_item<pbf_reclaim>(RECLAIM_PBUF_RELEASE_TO);

  pbfr->_pb = this;
  pbfr->_end = end;
}
#endif

size_t pipe_buffer_base::buffer_size()
{
  return _size;
}

size_t pipe_buffer_base::max_item_length()
{
  // See comment in decompress.cc
  return _size / 3;
}


void pipe_buffer_base::init(unsigned char *push_magic,size_t push_magic_len,
			    size_t bufsize
#ifdef USE_PTHREAD
			    ,thread_block *block_reader,bool create_thread
#endif
			    )
{
  // Hmm, we should really try to make sure we get a page-aligned
  // buffer!

  _buffer = (char *) malloc(bufsize);

  if (!_buffer)
    ERROR("Memory allocation failure.");

  _size = bufsize;

  if (push_magic)
    {
      memcpy(_buffer,push_magic,push_magic_len);
      _avail += push_magic_len;
    }

#ifdef USE_PTHREAD
  _block_reader = block_reader;

  // _thread_consumer = pthread_self();

  if (create_thread)
    {
      _block.init();

      if (pthread_create(&_thread,NULL,pipe_buffer::reader_thread,this) != 0)
	{
	  perror("pthread_create()");
	  exit(1);
	}

      set_thread_name(_thread, "NETIN", 5);

      _active = true;
    }
#endif
}

void pipe_buffer_base::realloc(size_t bufsize)
{
  // We can only allocate if there is no data in the buffer;
  // for no - be stricter - must not have been used.

  assert (_avail == 0 && _done == 0);

  _buffer = (char *) ::realloc(_buffer, bufsize);

  if (!_buffer)
    ERROR("Memory allocation failure.");

  _size = bufsize;
}

void pipe_buffer::init(int fd,unsigned char *push_magic,size_t push_magic_len,
		       size_t bufsize
#ifdef USE_PTHREAD
		       ,thread_block *block_reader
#endif
		       )
{
  _fd = fd;

  pipe_buffer_base::init(push_magic,push_magic_len,bufsize
#ifdef USE_PTHREAD
			 ,block_reader,true
#endif
			 );
}


void pipe_buffer_base::close()
{
#ifdef USE_PTHREAD
  if (_active)
    {
      pthread_cancel(_thread);

      if (pthread_join(_thread,NULL) != 0)
	{
	  perror("pthread_join()");
	  exit(1);
	}
      _active = false;
    }
#endif
}

void pipe_buffer::close()
{
  pipe_buffer_base::close();

  if (_fd != -1 &&
      ::close(_fd) == -1)
    {
      perror("close()");
      exit(1);
    }
  _fd = -1;

  // The buffer is free()ed only on destroying our object, since we'd
  // really like to re-use it for another file, avoiding memory
  // reallocation
}


pipe_buffer::pipe_buffer()
{
  _fd = -1;
}


pipe_buffer::~pipe_buffer()
{
  close();
}


pipe_buffer_base::pipe_buffer_base()
{
  _buffer = NULL;

  _avail = 0;
  _done  = 0;

  _reached_eof = false;

#ifdef USE_PTHREAD
  _active = false;

  _need_reader_wakeup = NULL;
  _need_consumer_wakeup = NULL;

  _block_reader = NULL;
#endif
}


pipe_buffer_base::~pipe_buffer_base()
{
  free(_buffer);
  _buffer = NULL;
}


// Events: 443720   443701             (19 errors)
// Events: 443720   443701             (19 errors)
// Events: 443720   443701             (19 errors)

