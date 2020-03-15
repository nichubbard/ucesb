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

#ifndef __THREAD_INFO_HH__
#define __THREAD_INFO_HH__

#include <sys/types.h>
#include <sys/time.h>

// TODO: remove
#include "input_buffer.hh"

struct ti_input
{
  const char *_reader_type;
  const char *_filename;
  off_t  _read_so_far;

  size_t _ahead;  // data read ahead (in core, to be processed)
  size_t _active; // buffer space in flight
  size_t _free;   // free buffer space
};

#define TI_TASK_NUM_MASK       0x000000ff
#define TI_TASK_SERIAL         0x00010000
#define TI_TASK_PARALLEL       0x00020000
#define TI_TASK_QUEUE_SINGLE   0x00100000
#define TI_TASK_QUEUE_FAN_OUT  0x00200000
#define TI_TASK_QUEUE_FAN_IN   0x00400000

class multi_thread_queue_base;
struct thread_queue_base;

struct ti_task
{
  // Static members

  int            _type;
  const char    *_name;

  union
  {
    thread_queue_base       *_single;
    multi_thread_queue_base *_multi;
  } _queue;

  // Dynamic members

  int    _speed;  // events/s
};

struct ti_task_multi
  : public ti_task
{
  // Dynamic members

  float  _threads;  // number of full-time threads
  int   *_todo;     // array with number of events in buffers /* size: ti._num_work_threads */
};

struct ti_task_serial
  : public ti_task
{
  // Dynamic members

  int _todo;      // events that can be directly processed (all available in order)
  int _available; // total number of events in buffers (but with holes)
};

struct ti_tasks
{
  ti_task **_tasks; /* size: ti._num_tasks */
};

struct ti_thread_task
{
  int      _type;
  int      _rate;
  int      _buf;
};

class worker_thread;

struct ti_thread
{
  // Static members

  const char    *_name;
  worker_thread *_worker;

  // Dynamic members

  int _cpu;  // cpu activity (should be max, whatever that is.  Units!)
  // For each thread, we keep track of all tasks that it may be
  // involved in (this is essentially all of them :-( )

  int _buf_used; // used size of thread's buffer
  int _buf_size; // total size of thread's buffer

  ti_thread_task *_tasks; /* size: ti._num_tasks */
};

struct ti_threads
{
  ti_thread **_threads; /* size: ti._num_threads */
};

struct ti_totals
{
  // Counting amount of data totally processed (retired)

  off_t  _retired;       // bytes of input data processed
  off_t  _retired_rate;  // rate of retiring data

  off_t  _events;        // Number of events processed
  off_t  _events_rate;   // Rate of processing events

  off_t  _events_ok;     // Number of events (physical for multi-event) accepted
  off_t  _events_ok_rate; // Rate of accepted events

  off_t  _errors;        // Number of errors
  timeval _last_ev_time; // Time associated with last event processed
};

class worker_thread;

struct thread_info
{
public:
  ti_input   _input;
  ti_totals  _totals;

#ifdef USE_THREADING
  int        _num_tasks;
  int        _num_threads;
  int        _num_work_threads;

  ti_tasks   _tasks;
  ti_threads _threads;
#endif

public:
  // TODO: move elsewhere
  input_buffer *_file_input;

public:
  void init(int num_tasks,
	    int num_threads,
	    int num_work_threads);

public:
  ti_task *set_task(int index,const char *name,int type);

  void set_task(int index,const char *name,int type,thread_queue_base *queue);
  void set_task(int index,const char *name,int type,multi_thread_queue_base *queue);

  void set_thread(int index,const char *name,worker_thread *worker);

public:
  void update();
};

extern thread_info _ti_info;

#endif//__THREAD_INFO_HH__
