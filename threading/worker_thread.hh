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

#ifndef __WORKER_THREAD_HH__
#define __WORKER_THREAD_HH__

#include "thread_block.hh"

#include <stdlib.h>
#include <pthread.h>

// Each thread has a structure associated with it, which holds it's
// status variables, and e.g. buffers that solely it can allocate from
// (but others free to)

class thread_buffer;
struct reclaim_item;

// The worker threads operate on one event at a time

class event_base;

// Please note: since this structure has the __thread attribute, it
// cannot hold classes with complex initializers, so we'll use
// pointers (at least for now).

struct worker_thread_data
{
  thread_buffer  *_defrag_buffer;
  reclaim_item  **_last_reclaim;

  event_base     *_current_event;

public:
  void init();
};

#ifdef USE_THREADING
# ifdef HAVE_THREAD_LOCAL_STORAGE
extern __thread worker_thread_data _wt;
# else
// We do not have the __thread attribute, so will go via
// pthread_getspecific
extern pthread_key_t _wt_key;
#  define _wt (*((worker_thread_data*) pthread_getspecific(_wt_key)))
worker_thread_data &get_wt();
void wt_init();
# endif
# define CURRENT_EVENT (_wt._current_event)
#else
extern worker_thread_data _wt;
# ifndef USE_MERGING
#  define CURRENT_EVENT (&_static_event)
# else
extern event_base *_current_event;
#  define CURRENT_EVENT (_current_event)
# endif
#endif

class worker_thread
{
public:
  worker_thread();
  virtual ~worker_thread();

public:
  thread_block _block;

protected:
  // This pointer is to only be used for diagnostic purposes!
  worker_thread_data *_data;

public:
  worker_thread_data *get_data() { return _data; }

#ifdef USE_PTHREAD
public:
  pthread_t _thread;
  bool      _active; // has thread

public:
  static void *worker(void *us);
  virtual void *worker() = 0;
#endif

public:
  void thread_init();
  void init();

  void spawn();
  void join();

};

#endif//__WORKER_THREAD_HH__
