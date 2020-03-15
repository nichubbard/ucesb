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

#ifndef __EVENT_SIZES_HH__
#define __EVENT_SIZES_HH__

#include "lmd_input.hh"
#include "pax_input.hh"
#include "genf_input.hh"
#include "ebye_input.hh"

#include <stdint.h>

#include <map>

#ifdef USE_LMD_INPUT
struct event_size
{
  uint64_t _min;
  uint64_t _max;

  uint64_t _sum;
  uint64_t _sum_x;

  uint64_t _sum_header;
};

union subevent_ident
{
  uint                   _compare[3]; // first is dummy (_info._header.l_dlen, always 0)
  lmd_subevent_10_1_host _info;

public:
  bool operator<(const subevent_ident &rhs) const
  {
    for (size_t i = 1; i < countof(_compare); i++)
      {
	if (_compare[i] == rhs._compare[i])
	  continue;
	return _compare[i] < rhs._compare[i];
      }
    return false;
  }
};

typedef std::map<subevent_ident,event_size *> set_subevent_size;

class event_sizes
{




public:
  set_subevent_size _subev_size[18]; // 0-15, others, sticky

  event_size ev_size[18];

public:
  void account(FILE_INPUT_EVENT *event);

public:
  void init();
  void show();


};
#endif

#ifndef USE_LMD_INPUT
class event_sizes
{
public:
  // void account(FILE_INPUT_EVENT *event);

public:
  void init() { }
  void show() { }
};
#endif

#endif//__EVENT_SIZES_HH__
