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

#ifdef TEST_THREAD_BUFFER
#include <stdio.h>
#define ERROR(x) {printf(x);exit(1);}
#define DEBUG_THREAD_BUFFER 1
#endif

// #error Huh?

#ifndef __THREAD_BUFFER_HH__
#define __THREAD_BUFFER_HH__

#ifndef DEBUG_THREAD_BUFFER
#define DEBUG_THREAD_BUFFER 0
#endif

#ifndef TEST_THREAD_BUFFER
#include "reclaim.hh"
#include "worker_thread.hh"
#include "error.hh"
#endif

#include <stdlib.h>

#ifdef USE_THREADING

class thread_buffer;

struct tb_reclaim
  : public reclaim_item
{
  thread_buffer *_buffer;
  size_t         _reclaim;
};

struct tb_buf
{
  size_t   _size;
  tb_buf  *_next;

};

class thread_buffer
{
public:
  thread_buffer()
  {
    _allocated = 0;
    _reclaimed = 0;

    _cur = NULL;
    _cur_used = 0;

    _total = 0;
    _alloc_size = 0;

    // Allocate a dummy buffer, such that _cur is non-NULL

    _cur = (tb_buf *) malloc(sizeof(tb_buf));

    if (!_cur)
      ERROR("Memory allocation failure!");

    _total += 0;

    _cur->_next = _cur;
    _cur->_size = sizeof(tb_buf);

    _cur_used = sizeof(tb_buf);
  }

public:
  tb_buf* _cur;
  size_t  _cur_used;

  size_t _allocated;
  size_t _reclaimed; // ONLY updated by reclaim (and then only
		     // growing)

  size_t _total; // total memory in our buffers
  size_t _alloc_size;

public:
  void *allocate(size_t size,size_t *reclaim)
  {
    // Make sure we are never more misaligned than 8 bytes (for double)
    // TODO: other archs may want larger, like PPU/SPU

#define THREAD_BUFFER_ALIGN 8

    size = (size + (THREAD_BUFFER_ALIGN-1)) & ~(THREAD_BUFFER_ALIGN-1);

    // From the current buffer we can always allocate, if there is space enough

    if (_cur->_size - _cur_used >= size)
      {
	void *ptr = ((char*) _cur) + _cur_used;

	_cur_used += size;
	_allocated += size;

	*reclaim = size;
#if DEBUG_THREAD_BUFFER
	printf ("AVAIL:      %d (%d [%p]) (%d,%d,%d)\n",(int) size,(int) *reclaim,ptr,
		(int) (_cur->_size - _cur_used),(int) _allocated,(int) _total);
#endif

	return ptr;
      }

    // We could not get the needed size from the allocated buffer,
    // so it has been wasted.  But the wasted space will be returned when we free
    // up the associated item

    _allocated += _cur->_size - _cur_used;
    *reclaim = _cur->_size - _cur_used;
#if DEBUG_THREAD_BUFFER
    printf ("addRECLAIM:     (%d)\n",(int) *reclaim);
#endif
    // Now, we are very much thinking about advancing the _cur ptr to _cur->_next
    // but this is ONLY allowed if that entire buffer has been reclaimed!

    for ( ; ; )
      {
	tb_buf *next = _cur->_next;

	size_t reclaimed_size = _total - _allocated + _reclaimed;

	if (reclaimed_size < next->_size ||
	    next == _cur) // this is not only needed at startup!
	  {
	    // We may NOT advance, next buffer is not completely
	    // reclaimed yet.  Must allocate a new one
	    break;
	  }

	// If the buffer is too small, get rid of it

	if (next->_size < (_alloc_size >> 1) ||
	    next->_size < sizeof(tb_buf) + size)
	  {
	    // The buffer on the free list is too small, kill it
	    // Either generally too small, or too small for our item

	    _cur->_next = next->_next; /// UPDATES CIRCULAR LIST

	    _total -= (next->_size - sizeof(tb_buf));
#if DEBUG_THREAD_BUFFER
	    fprintf (stderr,"FREE:        (%d [%p]) (%d-%d=%d,%d)\n",(int) next->_size,
		     next,
		     (int) _allocated,(int) _reclaimed,(int) (_allocated-_reclaimed),(int) _total);
#endif
	    free(next);
	    continue; // so, try again
	  }

	// Ok, so buffer next is free, and has space enough, move there

	_cur = next;
	_cur_used = sizeof(tb_buf) + size;

	_allocated += size;

	*reclaim += size;
	void *ptr = ((char*) _cur) + sizeof(tb_buf);

#if DEBUG_THREAD_BUFFER
	printf ("AVAIL-FREE: %d (%d [%p]) (%d,%d,%d)\n",(int) size,(int) *reclaim,ptr,
		(int) (_cur->_size - _cur_used),(int) _allocated,(int) _total);
#endif
	return ptr;
      }

    // We must allocate a new buffer

    _alloc_size = _alloc_size + (_alloc_size >> 3); // + 12.5 %

    if (_alloc_size < sizeof(tb_buf) + size + 0x1000)
      _alloc_size = sizeof(tb_buf) + size + 0x1000;

    if (_alloc_size > 0x8000)
      _alloc_size &= ~0xfff; // page align when more than 32k
    else
      _alloc_size &= ~0xff;

    tb_buf *new_bfr = (tb_buf *) malloc(_alloc_size);

    if (!new_bfr)
      ERROR("Memory allocation failure!");

    _total += _alloc_size - sizeof(tb_buf);

    new_bfr->_size = _alloc_size;
    new_bfr->_next = _cur->_next;
    _cur->_next = new_bfr;
    _cur = new_bfr;

    _cur_used = sizeof(tb_buf) + size;

    _allocated += size;
    *reclaim += size;

    void *ptr = ((char*) _cur) + sizeof(tb_buf);
#if DEBUG_THREAD_BUFFER
    fprintf (stderr,"ALLOC:       (%d [%p]) (%d-%d=%d,%d)\n",(int) _alloc_size,new_bfr,
	     (int)_allocated,(int)_reclaimed,(int) (_allocated-_reclaimed),(int) _total);

    printf ("AVAIL-NEW:  %d (%d [%p]) (%d,%d,%d)\n",(int) size,(int) *reclaim,ptr,
	    (int) (_cur->_size - _cur_used),(int) _allocated,(int) _total);
#endif

    return ptr;
  }

  //#ifndef TEST_THREAD_BUFFER
  void *allocate_reclaim(size_t size,uint32 type = RECLAIM_THREAD_BUFFER_ITEM)
  {
    size_t reclaim;

    void *ptr = allocate(sizeof (tb_reclaim) + size,&reclaim);

    tb_reclaim *tbr = (tb_reclaim *) ptr;

    tbr->_type = type;
    tbr->_next = NULL;
    tbr->_buffer = this;
    tbr->_reclaim = reclaim;

    *(_wt._last_reclaim) =   tbr;
     (_wt._last_reclaim) = &(tbr->_next);

    return (void*) (tbr+1);
  }
  //#endif

  void *allocate_reclaim_copy(size_t size, void *src, size_t src_size,
			      uint32 type = RECLAIM_THREAD_BUFFER_ITEM)
  {
    void *ptr = allocate_reclaim(size, type);

    assert (src_size <= size);

    memcpy(ptr, src, src_size);

    return ptr;
  }

  void *allocate_reclaim_copy(size_t size, void *src, void *src_end,
			      uint32 type = RECLAIM_THREAD_BUFFER_ITEM)
  {
    void *ptr = allocate_reclaim(size, type);

    size_t src_size = (char *) src_end - (char *) src;

    assert (src_size <= size);

    memcpy(ptr, src, src_size);

    return ptr;
  }

  // This may ONLY be called for the most recently allocated item (and
  // while it is still live.  It may only adjust the used space down.
  // It is used to trim the allocated space down to the actually used
  // size.  The purpose is that one should be able to use call
  // allocate_reclaim with a size which is large enough, and then give
  // back the space not used, without a large overhead
  void allocate_adjust_end(void *start,void *old_end,void *new_end)
  {
    tb_reclaim *tbr = ((tb_reclaim *) start) - 1;

    assert(old_end == ((char*) _cur) + _cur_used);
    assert(new_end >= start);
    assert(new_end <= old_end);

    size_t unused = ((char*) old_end) - ((char*) new_end);

    // The reclaim item must be adjusted to the new reclaim location

    tbr->_reclaim -= unused;
    _cur_used     -= unused;
    _allocated    -= unused;
  }

  template<typename T>
  T *allocate_reclaim_item(uint32 type = RECLAIM_THREAD_BUFFER_ITEM)
  {
    size_t reclaim;

    void *ptr = allocate(sizeof (T),&reclaim);

    T *tbr = (T *) ptr;

    tbr->_type = type;
    tbr->_next = NULL;
    tbr->_buffer = this;
    tbr->_reclaim = reclaim;

    *(_wt._last_reclaim) =   tbr;
     (_wt._last_reclaim) = &(tbr->_next);

    return tbr;
  }

public:
  void reclaim(size_t reclaim)
  {
#if DEBUG_THREAD_BUFFER
    printf ("RECLAIM: %d (%d)\n",(int)reclaim,(int)_reclaimed);
#endif
    _reclaimed += reclaim;
  }

};

// Defined in error.cc
void reclaim_eject_message(tb_reclaim *tbr);

#else//!USE_THREADING

class keep_buffer_single
{
public:
  keep_buffer_single()
  {
    _buf = NULL;
    _allocated = 0;
  }

public:
  void   *_buf;
  size_t  _allocated;

public:
  void *allocate(size_t size)
  {
    if (size > _allocated)
      {
	void *newbuf = realloc(_buf,size);
	if (!newbuf)
	  ERROR("Memory allocation failure.");
	_buf = newbuf;
	_allocated = size;
      }
    return _buf;
  }
};


struct kbm_buf
{
  size_t   _size;
  kbm_buf *_next;
};

class keep_buffer_many
{
public:
  keep_buffer_many()
  {
    // Allocate a dummy buffer, such that _cur is non-NULL

    _first = new_buf(NULL,0);

    _remain = 0;
    _allocated = 0;

    _end = NULL;
  }

public:
  char    *_end;
  size_t   _remain;

  kbm_buf *_first;
  size_t   _allocated;

protected:
  kbm_buf *new_buf(kbm_buf *next,size_t alloc)
  {
    // fprintf(stderr,"keep_buffer_many[%p]::new_buf(...,%d)\n",this,alloc);
    kbm_buf *newbuf = (kbm_buf *) malloc(sizeof(kbm_buf) + alloc);
    if (!newbuf)
      ERROR("Memory allocation failure!");
    newbuf->_next = next;
    newbuf->_size = alloc;

    return newbuf;
  }

public:
  void release()
  {
    // If we have multiple buffers, then reallocate to have only one.
    // (to reduce fragmentation), and more importantly: make most
    // future allocations go the fast path, i.e. space is available in
    // the current buffer

    if (UNLIKELY(_first->_next != NULL))
      {
	// there are multiple buffers.  Release them all and get a new one

	kbm_buf *f = _first;

	while (f)
	  {
	    kbm_buf *r = f;
	    f = f->_next;
	    free(r);
	  }

	// fprintf(stderr,"keep_buffer_many[%p]::release() -> %d\n",this,_allocated);

	_first = new_buf(NULL,_allocated);
      }

    // Reset start pointer

    _end = (char*) (_first+1);
    _remain = _allocated;
  }

public:
  void *allocate(size_t size)
  {
    // Do we fit in current last buffer?

    if (UNLIKELY(size > _remain))
      {
	// How much to allocate?

	size_t alloc = _allocated > 64 ? _allocated : 64;

	while (alloc < size)
	  alloc *= 2;

	// We do not fit, get a new one!

	_first = new_buf(_first,alloc);

	_end = (char*) (_first+1);
	_remain = alloc;
	_allocated += alloc;
      }

    void *p = _end;

    _end += size;
    _remain -= size;

    return p;
  }
};

#endif//!USE_THREADING

#endif//__THREAD_BUFFER_HH__





#ifdef TEST_THREAD_BUFFER

// For testing:
// g++ -g -DTEST_THREAD_BUFFER -o thread_buffer -x c++ thread_buffer.hh && ./thread_buffer

#include <queue>

thread_buffer _buffer;

int main(int argc,char *argv[])
{
  int n;

  if (argc >= 2)
    n = atoi(argv[1]);
  else
    n = 10000;

  std::queue<size_t> to_reclaim;

  for (int i = 0; i < n; i++)
    {
      size_t size = 100;
      size_t reclaim;
      void   *ptr;

      printf ("About to allocate: %d\n",size);
      fflush(stdout);

      _buffer.allocate(size,&reclaim,&ptr);

      to_reclaim.push(reclaim);

      printf ("Did Allocate: %d (reclaim: %d)\n",size,reclaim);
      fflush(stdout);

      if (i % 2 == 0)
	{
	  reclaim = to_reclaim.front();
	  to_reclaim.pop();

	  _buffer.reclaim(reclaim);

	}
    }
}

#endif//TEST_THREAD_BUFFER
