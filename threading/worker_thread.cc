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

#include "worker_thread.hh"

#include "thread_buffer.hh"
#include "thread_queue.hh"

#include "thread_debug.hh"
#include "error.hh"
#include "set_thread_name.hh"

#include <signal.h>

#define WT_DATA_INIT { NULL, NULL, NULL, }

#ifdef USE_THREADING
#ifdef HAVE_THREAD_LOCAL_STORAGE
__thread worker_thread_data _wt = WT_DATA_INIT;
#else
// We do not have the __thread attribute, so will go via
// pthread_getspecific
pthread_key_t _wt_key;
pthread_once_t _wt_key_once = PTHREAD_ONCE_INIT;

void wt_key_destroy(void * buf)
{
  worker_thread_data *data = (worker_thread_data *) buf;
  delete data;
}

void wt_key_alloc()
{
  pthread_key_create(&_wt_key,wt_key_destroy);
}

void wt_init()
{
  pthread_once(&_wt_key_once,wt_key_alloc);

  worker_thread_data *data = new worker_thread_data;

  worker_thread_data data_init = WT_DATA_INIT;

  *data = data_init;

  pthread_setspecific(_wt_key,data);
}

#endif
#else
worker_thread_data _wt = WT_DATA_INIT;
#endif


void worker_thread_data::init()
{
  _defrag_buffer = new thread_buffer;

}




worker_thread::worker_thread()
{
  _data = NULL;

#ifdef USE_PTHREAD
  _active = false;
#endif
}



worker_thread::~worker_thread()
{
  join();

}



void worker_thread::join()
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


// To be called by the tread itself on initialisation
void worker_thread::thread_init()
{
  TDBG("");

#if USE_THREADING
#ifndef HAVE_THREAD_LOCAL_STORAGE
  wt_init();
  fe_init();
#endif
#endif

  _data = &_wt;
  _data->init();
}


void worker_thread::init()
{
  TDBG("");

  sigset_t sigmask;

  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGINT);

  pthread_sigmask(SIG_BLOCK,&sigmask,NULL);

  _block.init();
}

void *worker_thread::worker(void *us)
{
  TDBG("");
  worker_thread *wt = ((worker_thread *) us);
  wt->thread_init();
  return wt->worker();
}


// Called by the parent thread (master), to start operations
void worker_thread::spawn()
{
  if (pthread_create(&_thread,NULL,worker_thread::worker,this) != 0)
    {
      perror("pthread_create()");
      exit(1);
    }

  set_thread_name((*relay_info)->_thread, "WORK", 5);

  // INFO(0,"Thread created...");

  _active = true;
}
