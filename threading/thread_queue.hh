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

#ifndef __THREAD_QUEUE_HH__
#define __THREAD_QUEUE_HH__

#include "thread_block.hh"

#include "optimise.hh"
#include "error.hh"

#include <unistd.h>
#include <sys/types.h>

////////////////////////////////////////////////////////////////////

// This is a queue of (usually events), that are to be sent to a
// specific destination (consumer) and comes from a specific producer.
// It is used both for spreading events out to n workers (that can run
// in parallel), then there are n threads all fed by the same producer
// (which must choose in what queue to put each event).  It is also
// used when collecting the events back into serail processing (at
// least for final retirement).  The idea is that for each queue only
// _one_ thread is producer and only _one_ thread is a consumer.

// For the 1->n operation, the difficult part is to figure out where
// it makes most sense to put the next event (basically round-robin,
// but with a tendency to not fragment the work too much (since when
// mmap()ing files, the source pages must be faulted into the
// processing processor)), and for the n->1 case, the issue is to
// quickly find the queue which contains the next event.  The n->1
// case becomes easy if each event carries in the 1->n case carries
// along the information about where to expect the next event.

// The queues have a fixed size.  This is no major limitation, since
// the memory we need for a queue is quite small, so we can allocate
// an offendingly large queue such that this should (almost) never be
// the reason for program stall.

// The size is to be a power of 2

// This structure is non-templated such that it works for a diagnostic class
struct thread_queue_base
{
public:
  volatile int _avail;
  volatile int _done;
  // int _size;

  // These two will NEVER be blocked at the same time.  If, then
  // we have a deadlock!
  volatile const thread_block *_need_avail_wakeup;
  volatile const thread_block *_need_done_wakeup;

  volatile int _wakeup_avail;
  volatile int _wakeup_done;
};

template<typename T,int n>
class thread_queue :
  public thread_queue_base
{
public:
  thread_queue()
  {
    _avail = 0;
    _done  = 0;

    _need_avail_wakeup = NULL;
    _need_done_wakeup  = NULL;
  }

public:
  //thread_queue_item<T> _items[n];
  T _items[n];

public:
  ////////////////////////////////////////////////////////////////////
  int fill() const
  {
    return ((_avail - _done) & (2*n-1));
  }
  int slots() const { return n; }

public:
  ////////////////////////////////////////////////////////////////////
  // Used by the producing thread:

  bool can_insert() const
  {
    //fprintf (stderr,"can_insert (%d,%d,%d) (%d)...\n",_avail,_done,n,
    //         ((_avail - _done) & (2*n-1)));
    // Is there space for one more element?
    // See note at can_remove
    return ((_avail - _done) & (2*n-1)) < n;
  }

  T &next_insert()
  {
    assert(can_insert());
    return _items[_avail & (n-1)];
  }

  void insert()
  {
    assert(can_insert());

    SFENCE; // make sure we wrote the data before we make it available
    // An sfence is enough, since the important accesses were writes
    // We always insert one item into the queue
    _avail++;

    // We have made an event available.  If anyone is blocking
    // on us, let's release them

    if (_need_avail_wakeup)
      {
	LFENCE;
	if ((int) (_avail - _wakeup_avail) > 0)
	  {
	    const thread_block *blocked = (const thread_block *) _need_avail_wakeup;
	    TDBG("(%p) blocked %p",this,blocked);
	    _need_avail_wakeup = NULL;
	    SFENCE;
	    blocked->wakeup();
	  }
      }
  }

  void flush_avail()
  {
    // If any item is left in the queue, and the other end has gone to sleep (i.e.
    // requested a wakeup, then wake hime up)

    if (_avail != _done &&
	_need_avail_wakeup)
      {
	const thread_block *blocked = (const thread_block *) _need_avail_wakeup;
	TDBG("(%p) blocked %p (flush)",this,blocked);
	_need_avail_wakeup = NULL;
	SFENCE;
	blocked->wakeup();
      }
  }

  /*
  void insert(T &item,int next_item_queue)
  {
    thread_queue_item<T> &slot = _items[_avail & ~(n-1)];

    slot._item            = item;
    slot._next_item_queue = next_item_queue;

    SFENCE; // make sure we wrote the data before we make it available
    // An sfence is enough, since the important accesses were writes
    // We always insert one item into the queue
    _avail++;
  }
  */
  ////////////////////////////////////////////////////////////////////
  // Used by the consuming thread:

  bool can_remove() const
  {
    // Is there an element to remove

    // Note, this may NOT be replaced by _avail > _done, since they
    // can wrap.  Forced by the and.  We cannot and with ~(n-1), since
    // the difference may be n (but not more).

    return ((_avail - _done) & (2*n-1)) > 0;
  }

  T &next_remove()
  {
    assert(can_remove());
    return _items[_done & (n-1)];
  }

  void remove()
  {
    assert(can_remove());
    MFENCE; // make sure we read the data before we release it
    // We always remove one item from the queue
    _done++;

    // We have removed an event available.  If anyone is blocking on
    // us (to free some space), let's release them

    if (_need_done_wakeup)
      {
	LFENCE;
	if ((int) (_done - _wakeup_done) > 0)
	  {
	    const thread_block *blocked = (const thread_block *) _need_done_wakeup;
	    TDBG("(%p) blocked %p",this,blocked);
	    _need_done_wakeup = NULL;
	    SFENCE;
	    blocked->wakeup();
	  }
      }
  }

  ////////////////////////////////////////////////////////////////////

  void request_remove_wakeup(thread_block *blocked)
  {
    TDBG("(%p) blocked %p",this,blocked);
    // assert(!_need_done_wakeup); // this may happen, but will be detected by second can_remove and canceled
    // We should tell blocked, whenever items become available
    _wakeup_avail = _done + (n >> 3);
    MFENCE;
    _need_avail_wakeup = blocked;
    MFENCE;
  }

  void cancel_remove_wakeup()
  {
    TDBG("(%p)",this);
    _need_avail_wakeup = NULL;
  }

  ////////////////////////////////////////////////////////////////////

  void request_insert_wakeup(thread_block *blocked)
  {
    TDBG("(%p) blocked %p",this,blocked);
    // assert(!_need_avail_wakeup); // this may happen, but will be detected by second can_insert and canceled
    // We should tell blocked, whenever free items become available
    _wakeup_done = _avail - n + (n >> 3);
    MFENCE;
    _need_done_wakeup = blocked;
    MFENCE;
  }

  void cancel_insert_wakeup()
  {
    TDBG("(%p)",this);
    _need_done_wakeup = NULL;
  }

  ////////////////////////////////////////////////////////////////////

#ifndef NDEBUG
  void debug_status()
  {
    TDBG_LINE("a:%d%c d:%d%c (%d)",
	      _avail,_need_avail_wakeup ? '*':' ',
	      _done ,_need_done_wakeup  ? '*':' ',
	      ((_avail - _done) & (2*n-1)));
  }
#endif
};

////////////////////////////////////////////////////////////////////

template<typename T,int n>
class single_thread_queue :
  public thread_queue<T,n>
{





};

////////////////////////////////////////////////////////////////////

template<typename T>
struct thread_queue_item
{
  T       _item;
  int     _next_item_queue;
};

////////////////////////////////////////////////////////////////////

class multi_thread_queue_base
{
public:
  virtual ~multi_thread_queue_base() { }

public:
  // Only to be used for diagnostic purposes!!!
  virtual thread_queue_base *get_queue(int index) = 0;
};

////////////////////////////////////////////////////////////////////

template<typename T_one_queue,typename T,int n>
class multi_thread_queue :
  public multi_thread_queue_base
{
public:
  typedef T_one_queue queue_t;

public:
  multi_thread_queue()
  {
    _size = 0;
    _queues = NULL;

    _next_item_queue = 0;
  }

  virtual ~multi_thread_queue()
  {
    delete[] _queues;
  }

public:
  int          _size;   // number of queues
  int          _next_item_queue;
  int          _next_next_item_queue;
  T_one_queue *_queues;

public:
  void init(int size)
  {
    // Allocate the queues.  Here one has to be careful, since we do
    // NOT want them to be sharing any cache lines between each other.

    // We actually would also like them to be allocated at either the
    // writing node, or the reading node...  For the time being, let's
    // claim that this is the job of the OS kernel to make that happen

    _size = size;
    _next_item_queue = 0;
    _next_next_item_queue = -1; // invalid (next_{remove/insert} must be called first)

    _queues = new T_one_queue[size];
  }

public:
  T_one_queue &next_queue()
  {
    return _queues[_next_item_queue];
  }

public:
  // Only to be used for diagnostic purposes!!!
  virtual thread_queue_base *get_queue(int index)
  {
    assert(_queues);
    return &_queues[index];
  }

public:
#ifndef NDEBUG
  void debug_status()
  {
    TDBG_LINE("size:%d next:%d",_size,_next_item_queue);
    for (int i = 0; i < _size; i++)
      _queues[i].debug_status();
  }
#endif
};

////////////////////////////////////////////////////////////////////

template<typename T,int n>
class fan_out_thread_queue;

template<typename T,int n>
class fan_out_thread_one_queue :
  public thread_queue<thread_queue_item<T>,n>
{
public:
  virtual ~fan_out_thread_one_queue() { }

public:
  fan_out_thread_queue<T,n> *_master;
  int                       _index;

public:
  T &next_remove(int *next_queue)
  {
    thread_queue_item<T> &slot =
      thread_queue<thread_queue_item<T>,n>::next_remove();

    *next_queue = slot._next_item_queue;

    return slot._item;
  }

};

////////////////////////////////////////////////////////////////////

template<typename T,int n>
class fan_out_thread_queue :
  public multi_thread_queue<fan_out_thread_one_queue<T,n>,T,n>
{
  // All the this-> gymnastics below is such that we can find the
  // members in the
  // multi_thread_queue<fan_out_thread_one_queue<T,n>,T,n> base class.
  // This would otherwise not happen, since it is not searched...

public:
  bool can_insert()
  {
    // return _queues[_next_item_queue].can_insert();
    return this->next_queue().can_insert();
  }

  T &next_insert(int next_item_queue)
  {
    thread_queue_item<T> &slot = this->next_queue().next_insert();

    slot._next_item_queue = next_item_queue;
    this->_next_next_item_queue = next_item_queue;

    return slot._item;
  }

public:
  void insert()
  {
    this->next_queue().insert();
    this->_next_item_queue = this->_next_next_item_queue;
  }

  ////////////////////////////////////////////////////////////////////

public:
  void request_insert_wakeup(thread_block *blocked)
  {
    this->next_queue().request_insert_wakeup(blocked);
  }

  void cancel_insert_wakeup()
  {
    this->next_queue().cancel_insert_wakeup();
  }

  ////////////////////////////////////////////////////////////////////

public:
  void flush_avail()
  {
    // We need to see if _any_ queue has any elements left, if so, then
    // trigger their consumers

    for (int i = 0; i < this->_size; i++)
      this->_queues[i].flush_avail();
  }
};

////////////////////////////////////////////////////////////////////

template<typename T,int n>
class fan_in_thread_queue;

template<typename T,int n>
class fan_in_thread_one_queue :
  public thread_queue<thread_queue_item<T>,n>
{

public:
  fan_in_thread_queue<T,n> *_master;
  int                       _index;

public:
  T &next_insert(int next_queue)
  {
    thread_queue_item<T> &slot =
      thread_queue<thread_queue_item<T>,n>::next_insert();

    slot._next_item_queue = next_queue;

    return slot._item;
  }
};

////////////////////////////////////////////////////////////////////

// To handle a set of queues, that are all to be processed by one
// thread, but may be produced by many threads (we keep one queue per
// producer to avoid locking.

// We always know which is the next queue to read items from, since
// we are to read things in order.  So the previous element stores
// which queue to use next.

template<typename T,int n>
class fan_in_thread_queue :
  public multi_thread_queue<fan_in_thread_one_queue<T,n>,T,n>
{
public:
  virtual ~fan_in_thread_queue() { }

public:
  bool can_remove()
  {
    return this->next_queue().can_remove();
  }

  T &next_remove()
  {
    /*T_one_queue*/
    fan_in_thread_one_queue<T,n> *queue = &this->next_queue();

    thread_queue_item<T> &slot = queue->next_remove();

    this->_next_next_item_queue = slot._next_item_queue;

    return slot._item;
  }

public:
  void remove()
  {
    // Get the next item.
    // Precondition: can_remove has returned true

    // First get the item itself

    /*T_one_queue*/
    //fan_in_thread_one_queue<T,n> *queue = &this->next_queue();

    // Then remove the item from the queue (slot is no longer valid)

    //queue->remove();

    this->next_queue().remove();

    this->_next_item_queue = this->_next_next_item_queue;
  }

  ////////////////////////////////////////////////////////////////////

public:
  void request_remove_wakeup(thread_block *blocked)
  {
    this->next_queue().request_remove_wakeup(blocked);
  }

  void cancel_remove_wakeup()
  {
    this->next_queue().cancel_remove_wakeup();
  }

  ////////////////////////////////////////////////////////////////////


  /*
public:
  bool request_wakeup()
  {
    // Call this routine after you (consumer) have investigated all
    // the queues, and determined them to be empty

    // Since there are many queues, we cannot give a global count,
    // since that would need to be locked.



    // We need to check the queue once more after asking for wakeup.

    if (this->next_queue().can_remove())
      {
	return true; // we actually had data, do not sleep!
      }
    return false;
  }
  */

};

////////////////////////////////////////////////////////////////////

#endif//__THREAD_QUEUE_HH__

