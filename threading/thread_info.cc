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

#include "thread_info.hh"
#include "thread_queue.hh"
#include "worker_thread.hh"
#include "thread_buffer.hh"

#include "file_mmap.hh"
#include "pipe_buffer.hh"

#include <string.h>

/*

Input: /path/to/very/long/.../dir/filename.lmd   xxx xxx
Buffer:  ahead xxxxxk active xxxxxk free xxxxxk

Task:     Speed:   Threads:  Todo (in buffer):
PreUpck:  xxxxk/s  SERIAL
Unpack:   xxxxk/s  x.x       xxx xxx xxx xxx xxx
user_unp: xxxxk/s  SERIAL    xxxx (xxxx))
-->raw:   xxxxk/s  x.x       xxx xxx xxx xxx xxx
user_raw: xxxxk/s  x.x       xxx xxx xxx xxx xxx
-->cal:   xxxxk/s  x.x       xxx xxx xxx xxx xxx
user_cal: xxxxk/s  x.x       xxx xxx xxx xxx xxx
user:     xxxxk/s  x.x       xxx xxx xxx xxx xxx
NTUPLE:   xxxxk/s  SERIAL    xxxx (xxxx))
RETIRE:   xxxxk/s  SERIAL    xxxx (xxxx))

Thread  Task k/s buf  Task k/s buf   TBUF      CPU
UNZIP:                                          99
1       UNP xxxx xxx                 xxxk/xxxk  99
2       UNP xxxx xxx
3       UNP xxxx xxx  UCL xxxx xxx
        NTU xxxx xxx  RET xxxx xxx   xxxk/xxxk  77

Memory:   raw data:  xxxxxk
          unpack ev: xxxxxk
          raw    ev: xxxxxk
          cal    ev: xxxxxk

Analysed: xxxGMk xxxxk/s              Errors: xxxx
          xxxGMk xxxxk/s (multi/ok)   (date-of-current-event)
=============================================================
Error messages, info etc...

*/

/*
  Scheduling ideas.

  Limiting factor.  When compiled for multi-processing mode, the
  buffers (the input file buffer, pipe_buffer), does when full and
  reading wait for some space to be released by the retirement
  process.  So the record reading routine (lmd/pax/ebey/genf_input)
  may not be running in the same thread as the retirement routine.  So
  at least two threads are necessary.  If we make the number of event
  buffers, that may have the same requirement (not to run in the same
  thread as the retire routine.  The same may apply for the
  unpack->raw expansion(explosion) when unpacking multi-event data.

  The different tasks to be performed are to be ordered along with the
  threads.  I.e. the last task (retirement) is always performed by the
  last thread.  Whenever we need to free cpu-time, we'll move tasks up
  in the thread list.

  At startup, we have nothing to do but to start unpacking events.
  Therefore, at startup all threads (but the record reading, which
  produces the data) should be unpacking events.

  The main question is: how to figure out if a thread is swamped with
  work (they should all be).  And to figure out when a thread has too
  little to do.

  We want to all threads (particularly the serial ones) to be able to
  survive the situation when any producer for it (particularly a
  singular one) gets scheduled away by the os for an execution block.
  (lets say 10 ms) I.e. the input buffers need to contain enough
  events.  With 10MB/s and 100 bytes/event, this 100000 events/s,
  i.e. 1000 events.  1000 events is then 100k data, i.e. large enough
  to vicitimize any L1 cache, and quite easilty also L2 caches (they
  also need to hold instructions and calibration parameters).  We can
  therefor say, that we need not consider the caching, i.e. to keep
  locality of the events.  The only exception we do to this is that of
  course, serial jobs that run in threads that also handle some
  parallell job, will handle these different tasks for the same event
  directly after each other.

  Let's make the main thread (i.e. the first one) responsible for:
  * stdin/out/err input/output, file open and close
  * thread monitoring (n-curses)
  * event retirement

  The unwanted part may be the event retirement, but since retirement
  is a light-weight task.  (No analysis, and no memory free, only some
  buffer releases (the free operations go into the treads, there are
  essentially no free, only an occasional munmap).  It may however
  also get some text data (error output) to be displayed.  (All such
  info is displayed in event order).  So then it's good that it has
  hold of the user I/O.

  Since it is such a light-weight operation, we however do not want to
  always fire the thread up, just to retire one more event.  But we
  also should not let the other treads wait for it to release
  memory...

  Anyhow, when the file reader (pipe_buffer, mmap) reaches EOF
  (i.e. long before the record reader (pre-unpacker) has consumed data
  so far), we can signal the main thread that another file should be
  opened and data start to be buffered.

*/




void thread_info::init(int num_tasks,
		       int num_threads,
		       int num_work_threads)
{
  memset (&_input,0,sizeof (_input));
  memset (&_totals,0,sizeof (_totals));

#ifdef USE_THREADING
  _num_tasks   = num_tasks;
  _num_threads = num_threads;
  _num_work_threads = num_work_threads;

  _tasks._tasks     = new ti_task*[_num_tasks];
  _threads._threads = new ti_thread*[_num_threads];

  for (int ta = 0; ta < _num_tasks; ta++)
    _tasks._tasks[ta] = NULL;
  for (int th = 0; th < _num_threads; th++)
    _threads._threads[th] = NULL;
#endif

  _file_input = NULL;
}

#ifdef USE_THREADING
ti_task *thread_info::set_task(int index,const char *name,int type)
{
  assert(index < _num_threads);
  assert(!_tasks._tasks[index]);

  ti_task *task = NULL;

  if (type & TI_TASK_SERIAL)
    task = new ti_task_serial;
  if (type & TI_TASK_PARALLEL)
    {
      ti_task_multi *task_multi = new ti_task_multi;

      task_multi->_todo = new int[_num_work_threads];

      memset(task_multi->_todo,0,sizeof (int) * _num_work_threads);

      task = task_multi;
    }

  assert(task);

  _tasks._tasks[index] = task;

  memset(task,0,sizeof (ti_task));

  task->_type = type;
  task->_name = name;

  return task;
}

void thread_info::set_task(int index,const char *name,int type,thread_queue_base *queue)
{
  ti_task *task = set_task(index,name,type);

  task->_queue._single = queue;
}

void thread_info::set_task(int index,const char *name,int type,multi_thread_queue_base *queue)
{
  ti_task *task = set_task(index,name,type);

  task->_queue._multi = queue;
}

void thread_info::set_thread(int index,const char *name,worker_thread *worker)
{
  assert(index < _num_threads);
  assert(!_threads._threads[index]);

  ti_thread *thread = new ti_thread;

  _threads._threads[index] = thread;

  memset(thread,0,sizeof (ti_thread));

  thread->_name = strdup(name);
  thread->_worker = worker;
}
#endif

void thread_info::update()
{
  // Update the information.

  // Care must be exercised when updating information that comes from
  // structures under control of other threads, since we do not want
  // a) seg-faults, b) wrong information.  a) is solved by NEVER
  // following pointers which the other thread may change (and thereby
  // e.g. free an old data structure) B) via care and handling of
  // obviously wrong data, e.g. when _done has passed _avail

  // input_buffer *_file_input;


  // when buffering several files, it is not completely correct just to take
  // the information from one file...

  memset(&_input,0,sizeof (_input));

#ifdef USE_CURSES
  _input._filename = "";
#endif
  _input._read_so_far = 0;

  file_mmap *mmap = dynamic_cast<file_mmap*>(_file_input);

  if (mmap)
    {
      _input._reader_type = "MMAP";
#ifdef USE_CURSES
      _input._filename = mmap->_filename;
#endif

      off_t back  = mmap->_back;
      off_t done  = mmap->_done;
      off_t front = mmap->_front;
      off_t avail = mmap->_avail;

      //fprintf (stderr,"back %08x  done %08x  front %08x  avail %08x",
      //       (int)back,(int)done,(int)front,(int)avail);

      // sanity check, since we might have gotten
      // done after avail (should never happen)
      if (back > done)
	done = back;
      if (done > front)
	front = done;
      if (front > avail)
	avail = front;

      _input._ahead  += (size_t) (avail - front);
      _input._active += (size_t) (front - done);
      _input._free   += (size_t) (done  - back);

      //fprintf (stderr,"  (ahead %08x  active %08x  free %08x)\n",
      //       _input._ahead,_input._active,_input._free);
    }

  pipe_buffer *pbuf = dynamic_cast<pipe_buffer*>(_file_input);

  if (pbuf)
    {
      _input._reader_type = "PIPE";
#ifdef USE_CURSES
      _input._filename = pbuf->_filename;
#endif

      size_t size  = pbuf->_size;
      size_t done  = pbuf->_done;
      size_t front = pbuf->_front;
      size_t avail = pbuf->_avail;

      ssize_t ahead  = (ssize_t) (avail - front);
      ssize_t active = (ssize_t) (front - done);
      ssize_t free   = (ssize_t) (size - (avail - done));

      // Sanity check, since we might have gotten the variables
      // desyncronised i.e. read out of order since we do not employ
      // any synchronisation directive while reading.  This is
      // the reason for using ssize_t above.
      if (ahead < 0)
	ahead = 0;
      if (active < 0)
	active = 0;
      if (free < 0)
	free = 0;

      _input._ahead  += (size_t) ahead;
      _input._active += (size_t) active;
      _input._free   += (size_t) free;
    }

  TDBG("update: %p %p %p",_file_input,mmap,pbuf);

#ifdef USE_THREADING
  for (int ta = 0; ta < _num_tasks; ta++)
    {
      ti_task *task = _tasks._tasks[ta];

      if (!task)
	continue;

      if (task->_type & TI_TASK_SERIAL)
	{
	  ti_task_serial *serial_task = (ti_task_serial*) task;

	  serial_task->_speed = 0;
	  serial_task->_todo  = 0;
	  serial_task->_available = 0;

	  if (task->_type & TI_TASK_QUEUE_SINGLE)
	    {
	      thread_queue_base *th_queue = serial_task->_queue._single;

	      size_t done  = th_queue->_done;
	      size_t avail = th_queue->_avail;

	      int todo = avail - done;
	      // sanity check, since we might have gotten the variables desyncronised
	      if (todo < 0)
		todo = 0;

	      serial_task->_todo = todo;
	    }
	  if (task->_type & TI_TASK_QUEUE_FAN_IN)
	    {
	      for (int thw = 0; thw < _num_work_threads; thw++)
		{
		  thread_queue_base *th_queue = serial_task->_queue._multi->get_queue(thw);

		  size_t done  = th_queue->_done;
		  size_t avail = th_queue->_avail;

		  int todo = avail - done;
		  // sanity check, since we might have gotten the variables desyncronised
		  if (todo < 0)
		    todo = 0;

		  serial_task->_available += todo;
		}



	    }

	}
      else if (task->_type & TI_TASK_PARALLEL)
	{
	  ti_task_multi  *multi_task = (ti_task_multi*) task;

	  multi_task->_speed = 0;
	  multi_task->_threads = 0;

	  memset(multi_task->_todo,0,sizeof (int) * _num_work_threads);

	  for (int thw = 0; thw < _num_work_threads; thw++)
	    {
	      thread_queue_base *th_queue = multi_task->_queue._multi->get_queue(thw);

	      size_t done  = th_queue->_done;
	      size_t avail = th_queue->_avail;

	      int todo = avail - done;
	      // sanity check, since we might have gotten the variables desyncronised
	      if (todo < 0)
		todo = 0;

	      multi_task->_todo[thw] = todo;
	    }
	}


    }


  for (int th = 0; th < _num_threads; th++)
    {
      ti_thread *thread = _threads._threads[th];

      if (!thread)
	continue;

      thread->_cpu      = 0;
      thread->_buf_used = 0;
      thread->_buf_size = 0;

      if (!thread->_worker)
	continue;

      if (!thread->_worker->get_data())
	continue;

      thread_buffer *buffer = thread->_worker->get_data()->_defrag_buffer;

      size_t done  = buffer->_reclaimed;
      size_t avail = buffer->_allocated;
      size_t size  = buffer->_total;

      int active = avail - done;

      // sanity check, since we might have gotten the variables desyncronised
      if (active < 0)
	active = 0;

      thread->_buf_used = active;
      thread->_buf_size = size;
    }
#endif
}
