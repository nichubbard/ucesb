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

#ifndef __LIMIT_FILE_SIZE_H__
#define __LIMIT_FILE_SIZE_H__

#include "typedef.hh"

#include "error.hh"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

class limit_file_size
{
public:
  limit_file_size()
  {
    _limit_size   = (uint64) -1;
    _limit_events = (uint32) -1;

    _cur_size   = 0;
    _cur_events = 0;

    _filename_base = NULL;

    _avoid_used_number = false;

    _may_change_file = true;
  }

  virtual ~limit_file_size()
  {
    delete[] _filename_base;
  }

public:
  uint64 _cur_size;
  uint32 _cur_events;

public:
  uint64 _limit_size;
  uint32 _limit_events;

  char *_filename_base;
  int   _filename_no;
  int   _filename_no_len;

  bool  _avoid_used_number;

  bool  _may_change_file; // e.g. not stdout pipe

public:
  void add_size(size_t size)
  {
    _cur_size += size;
    // printf ("add %d -> %d\n",size,_cur_size);
  }

  void add_events(uint32 events = 1)
  {
    _cur_events += events;
    // printf ("add %d -> %d\n",events,_cur_events);
  }

  void change_file()
  {
    if (_may_change_file)
      {
	char *filename = new char[strlen(_filename_base)+32];

	for ( ; ; )
	  {
	    sprintf(filename,_filename_base,_filename_no_len,_filename_no);

	    if (!_avoid_used_number)
	      break;

	    // See if we can stat the file.  If so, we must continue with
	    // another name.  If we cannot stat it, allow things to
	    // proceed.

	    struct stat st;

	    if (stat(filename,&st) != 0)
	      break;

	    _filename_no++;
	  }

	new_file(filename);
	delete[] filename;

	_filename_no++;
      }

    _cur_size = 0;
    _cur_events = 0;
  }

  void can_change_file()
  {
    // printf ("can change, %d cmp %d .. %d cmp %d\n",
    // _cur_size,_limit_size,_cur_events,_limit_events);
    if (_cur_size >= _limit_size ||
	_cur_events >= _limit_events) {
      /*
      INFO(0,"Reached size:%lld/%lld events: %d/%d",
	   _cur_size,_limit_size,_cur_events,_limit_events);
      */
      change_file();
    }
  }

  void parse_open_first_file(const char *filename);

public:
  virtual void new_file(const char *filename) = 0;

};

#endif//__LIMIT_FILE_SIZE_H__
