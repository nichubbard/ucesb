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

#include "file_mmap.hh"
#include "error.hh"

#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

// Since only one thread will be doing the mmap()ing itself, and thus
// fiddling around with the _free list, we can keep that global
/*file_mmap_chunk *file_mmap::_free = NULL;*/

file_mmap::file_mmap()
{
  // was _SC_PAGE_SIZE, but that is just a synonym for _SC_PAGESIZE
  _page_mask = (size_t) ~(sysconf(_SC_PAGESIZE) - 1);

  _avail = 0;
  _done  = 0;

  _front = 0;
  _back  = 0;

  _last_has_eof = false;

  _fd    = -1;

  _free = NULL;

  // Init the end pointers

  _ends._prev = &_ends;
  _ends._next = &_ends;

  // such that it never will trigger a munmap if being FIRST, becomes 0x400...000
  _ends._end  = ((off_t) 1) << (sizeof(_ends._end)*8-2);
  _ends._base = NULL;

#if !defined(NDEBUG) && 0
  consistency_check();
#endif
}

#define FIRST (_ends._next)
#define LAST (_ends._prev)

file_mmap::~file_mmap()
{
  close();
}

void file_mmap::init(int fd)
{
  _fd = fd;
}

volatile size_t _global_mmaped = 0;

void file_mmap::close()
{
  // printf ("file_mmap::close\n");

  // First we need to unmap all ranges that we still hold

  for (file_mmap_chunk *item = FIRST; item != &_ends; )
    {
      /*
      printf ("munmap: %08x..%08x [%08x] (%p)\n",
      	      (int) item->_start,(int) item->_end,
	      (int) (item->_end - item->_start),
	      item->_base);
      */
      // The first range we have has been completely released

      size_t len = (size_t) (item->_end - item->_start);

      if (munmap(item->_base,len))
	{
	  ERROR("munmap() failure");
	}

      _global_mmaped -= len;

      sig_unregister_mmap(&item->_mmap_info, item->_base, len);

      file_mmap_chunk *next = item->_next;

      item->_next = _free;
      _free = item;

      item = next;
    }

  FIRST = &_ends;
  LAST  = &_ends;

  if (_fd != -1)
    {
      if (::close(_fd) == -1)
	perror("close");
    }
  _fd = -1;
}

void file_mmap::consistency_check()
{
  assert(_ends._next);
  assert(_ends._prev);
  assert(_ends._next->_prev == &_ends);

  assert(_ends._base == NULL);
  assert(_ends._end  == ((off_t) 1) << (sizeof(_ends._end)*8-2));

  assert(LAST->_next == &_ends);
  assert(FIRST->_prev == &_ends);

  for (file_mmap_chunk *item = FIRST; item != &_ends; item = item->_next)
    {
      assert(item->_next);
      assert(item->_prev);

      assert(item->_next->_prev == item);

      if (item != FIRST &&
	  item != LAST)
	assert(item->_end == item->_next->_start);

      //printf ("item: [%08x..%08x] @%08x\n",
      //        (int) item->_start,(int) item->_end,(int) item->_base);
    }
}

int file_mmap::map_range(off_t start,off_t end,buf_chunk chunks[2])
{
  while (_done >= FIRST->_end)
    {
      /*
      printf ("munmap: %08x..%08x [%08x] (%p) (d:%08x)\n",
      	      (int) FIRST->_start,(int) FIRST->_end,
	      (int) (FIRST->_end - FIRST->_start),
	      FIRST->_base,(int) _done);
      */
      // The first range we have has been completely released

      size_t len = (size_t) (FIRST->_end - FIRST->_start);

      if (munmap(FIRST->_base,len))
	{
	  ERROR("munmap() failure");
	}
      _global_mmaped -= len;

      sig_unregister_mmap(&FIRST->_mmap_info, FIRST->_base, len);

      _back = FIRST->_end;

      file_mmap_chunk *next = FIRST->_next;
      file_mmap_chunk *prev = FIRST->_prev;

      FIRST->_next = _free;
      _free = FIRST;

      next->_prev = prev;
      prev->_next = next;
      FIRST = next;

      if (FIRST != &_ends)
	_back = FIRST->_start;

#if !defined(NDEBUG) && 0
      consistency_check();
#endif
    }

  // We must never ask for a start earlier than
  // a point that has previously been released

  // Do we have enough data available?

  // In order to enforce some read-ahead, we map areas before they are
  // requested.  A disk usually has a seek-time of about 10 ms.  (5000
  // rpm is 100 turns/s, gives 10 ms).  If we analyse data at 10 MB/s,
  // this means that we need to tell that a certain chunk of data is
  // needed about 100 kB before we actually will use it, if the
  // read-ahead is to be effective.  At 100 MB/s, it becomes 1 MB.

#define MMAP_SIZE        0x00800000 // 8 MB chunks
#define MMAP_READ_AHEAD  0x00200000 // 2 MB read-ahead

  if (_avail < end + MMAP_READ_AHEAD &&
      !_last_has_eof)
    {
      // We must map data

      // First make sure we have a range to store the data in

      if (!_free)
	{
	  file_mmap_chunk *alloc =
	    (file_mmap_chunk *) malloc (sizeof (file_mmap_chunk));
	  if (!alloc)
	    ERROR("Memory allocation failure");
	  alloc->_next = _free;
	  _free = alloc;
	}

      size_t length;
      off_t offset;

      offset = LAST != &_ends ? LAST->_end : (start & (off_t) _page_mask);
      length = MMAP_SIZE;

      // Figure out how long the file is.

      struct stat stat_buf;

      if (fstat(_fd,&stat_buf))
	{
	  perror("stat");
	  ERROR("Error stat()ing file.");
	}
      /*
      printf ("stat_buf.st_size: %d\n",(int) stat_buf.st_size);
      */
      if (!S_ISREG(stat_buf.st_mode) ||
	  !S_ISCHR(stat_buf.st_mode))
	return 0; // Do not attempt mmap'ing

      if (offset + (off_t) length >= stat_buf.st_size)
	{
	  length = (size_t) (stat_buf.st_size - offset);
	  _last_has_eof = true; // We have reached end-of-file (do not try to map again!)

#ifdef USE_THREADING
	  request_next_file();
#endif
	}
      /*
      printf ("mmap:   %08x..%08x [%08x] (%08x)",
              (int) offset,(int) (offset+length),(int) length,(int) start);
      */
      char *base = (char *) mmap(0,length,
				 PROT_READ,
#ifdef MAP_POPULATE
				 //MAP_POPULATE | // pre-fault
#endif
				 MAP_PRIVATE/*SHARED*/,
				 _fd,offset);

      _global_mmaped += (size_t) length;
      /*
      printf (" -> (%p) (%d)\n",base,(int)_global_mmaped);
      */
      if (base == MAP_FAILED)
	{
	  perror("mmap");
	  if (start == 0 && (errno == ENODEV || errno == EACCES))
	    {
	      WARNING("File mmap'ing failed at first attempt, falling back to read.");
	      return 0;
	    }
	  /*
	  if (_using_mmap == 1)
	    {
	      PWARNING("File mmap'ing failed at first attempt, falling back to read.");
	      return NULL;
	    }
	  */
	  ERROR("File mmap'ing failed, not for the first block however.");
	}

      /* Tell the OS that we intend to use the data!
       *
       * Now, if we would ever come to events larger than pages, and we
       * are only wanting a few of the events, it make little sense to
       * bring all pages into core.  On the other hand, the read-ahead
       * will not know exactly whap pages can be skipped.  (Note: we
       * always look all all event headers anyhow...)
       */

      madvise(base,length,MADV_WILLNEED | MADV_SEQUENTIAL);

      // We do not try to merge it with the previous item, since
      // otherwise we'll not munmap the memory (with the current
      // implementation) (could also think about changing that to
      // remove chunks of memory, removing them from the first item)

      file_mmap_chunk *next = _free;
      _free = _free->_next;

      next->_next = &_ends;
      next->_prev = LAST;
      LAST->_next = next;
      LAST = next;

      next->_start = offset;
      next->_end   = offset + (off_t) length;
      next->_base  = base;

      sig_register_mmap(&next->_mmap_info, base, length, _fd, offset);

      _avail = offset + (off_t) length;

#if !defined(NDEBUG) && 0
      consistency_check();
#endif
    }

  // Now, we have tried all we could to mmap the needed range.
  // Are we available?

  if (UNLIKELY(_avail < end))
    return 0;
  assert (_done <= start); // or someone has released too much/too early

  // We are available.  Either in last buffer, or last-1, or both of
  // these it cannot be more, or further back, since our buffer size
  // and read-ahead are large enough to prevent that

  _front = end;

  file_mmap_chunk *item = LAST;
  /*
  printf ("item->start: %08x  item->end: %08x  start: %08x  end: %08x  avail: %08x\n",
	  (int)item->_start,(int)item->_end,(int) start,(int) end,(int) _avail);
  */
  while (UNLIKELY(item->_start > start))
    {
      assert(item != FIRST);
      item = item->_prev;
    }

  size_t offset     = (size_t) (start - item->_start);
  size_t size       = (size_t) (end - start);

  if (item->_end >= end)
    {
      chunks[0]._ptr    = item->_base + offset;
      chunks[0]._length = size;
      return 1;
    }

  size_t segment    = (size_t) (item->_end - item->_start) - offset;

  chunks[0]._ptr    = item->_base + offset;
  chunks[0]._length = segment;
  assert(item != LAST);
  item = item->_next;

  chunks[1]._ptr    = item->_base;
  chunks[1]._length = size - segment;
  /*
  printf ("ptr: %08x  len: %08x  ptr: %08x  len: %08x\n",
	  (int)chunks[0]._ptr,(int)chunks[0]._length,
	  (int)chunks[1]._ptr,(int)chunks[1]._length);
  */
  return 2;
}


void file_mmap::release_to(off_t end)
{

  // All allocation/deallocation of mmap()s go into the map_range
  // function (that way, we need not care about such shitty problems
  // as locking those structures etc.  We simply note that until so
  // and so far, we're done with the buffer.

  // We do however (whenever we've passed another page of data), call
  // madvise to tell the OS that we are done with the pages

  // We must do the madvise before we set _done, since otherwise
  // map_range might get the idea of unmapping the range just in
  // between...  :-(

  // There is no problem with _gone being smaller than _done.  It is
  // never smaller than a page, so the bytes inbetween will never have
  // been unmapped before us.

  off_t away = end & (off_t) _page_mask;

  if (_done < away)
    {
      // Hmm, we'd also not like to send small chunks of memory away,
      // since that equals system calls.  If machine has plenty of
      // memory, it anyhow does not matter


    }

  MFENCE;

  _done = end;
}

#ifdef USE_THREADING
void file_mmap::arrange_release_to(off_t end)
{
  fmm_reclaim *fmmr =
    _wt._defrag_buffer->allocate_reclaim_item<fmm_reclaim>(RECLAIM_MMAP_RELEASE_TO);

  fmmr->_mm = this;
  fmmr->_end = end;
}
#endif

size_t file_mmap::buffer_size()
{
  // see comment below
  return (size_t) (ssize_t) -1;
}

size_t file_mmap::max_item_length()
{
  // file_mmap will allocate as many chunks as needed, so no limit
  return (size_t) (ssize_t) -1;
}
