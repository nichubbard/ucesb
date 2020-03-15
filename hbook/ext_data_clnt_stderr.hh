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

#ifndef __EXT_DATA_CLNT_STDERR_HH__
#define __EXT_DATA_CLNT_STDERR_HH__

#ifndef __CINT__
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#endif

class ext_data_struct_info;

class ext_data_clnt_stderr
{
public:
  ext_data_clnt_stderr();
  ~ext_data_clnt_stderr();

public:
  bool connect(const char *server);
  bool connect(const char *server,int port);
  // bool connect(int fd);
  int nonblocking_fd();

  /* NOTE: default arguments for name_id and struct_id will be removed.
   * Only here to simplify transition.  Also fetch_event.
   */
  int setup(const void *struct_layout_info,size_t size_info,
	    ext_data_struct_info *struct_info,
	    size_t size_buf,
	    const char *name_id = "", int *struct_id = NULL);

  int next_event(int *struct_id);
  int fetch_event(void *buf,size_t size,int struct_id = 0);
  int get_raw_data(const void **raw, ssize_t *raw_words);

  void close();

  static void rand_fill(void *buf,size_t size);

protected:
  void *_client; // void such that cint does not care about the type

};

#endif//__EXT_DATA_CLNT_STDERR_HH__
