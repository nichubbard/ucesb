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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for pthread_setname_np
#endif

#include "has_pthread_getname_np.h"

#include "set_thread_name.hh"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define THREADNAMELEN 16

void set_thread_name(pthread_t thread,
		     const char *name,
		     size_t maxlen)
{
#if HAS_PTHREAD_GETNAME_NP
  char fullname[THREADNAMELEN];
  size_t len;
  char *p;

  if (pthread_getname_np(thread, fullname, sizeof (fullname)) != 0)
    {
      fullname[0] = 0;
    }

  p = strchr(fullname, '\n');
  if (p)
    *p = 0;

  len = strlen(fullname);

  if (len > THREADNAMELEN - 1 - maxlen - 1)
    len = THREADNAMELEN - 1 - maxlen - 1;

  fullname[len] = 0;

  strcat(fullname, "/");
  strncat(fullname, name, maxlen);

  pthread_setname_np(thread, fullname);
#else
  (void) thread;
  (void) name;
  (void) maxlen;
#endif
}
