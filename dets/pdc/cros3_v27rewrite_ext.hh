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

#ifndef __CROS3_V27REWRITE_EXT_HH__
#define __CROS3_V27REWRITE_EXT_HH__

#include "data_src.hh"
#include "dummy_external.hh"
#include "external_data.hh"

/*---------------------------------------------------------------------------*/

struct WIRE_START_END
{
  uint8 wire;
  uint8 start;
  uint8 stop;

public:
  bool operator<(const wire_hit &rhs) const
  {
    int wd = wire - rhs.wire;

    if (wd)
      return wd < 0;

    return start < rhs.start;
  }

  void __clean()
  {
    wire  = 0;
    start = 0;
    stop  = 0;
  }

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    char buf[32];
    sprintf(buf,"(%d,%d,%d)",wire,start,stop);
    pretty_dump(id,buf,pdi);
  }

public:
  DUMMY_EXTERNAL_SHOW_MEMBERS(WIRE_START_END);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(WIRE_START_END);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(WIRE_START_END);
};

DUMMY_EXTERNAL_MAP_STRUCT(WIRE_START_END);
DUMMY_EXTERNAL_CALIB_MAP_STRUCT(WIRE_START_END);
DUMMY_EXTERNAL_WATCHER_STRUCT(WIRE_START_END);
DUMMY_EXTERNAL_CORRELATION_STRUCT(WIRE_START_END);

inline int get_enum_type(WIRE_START_END *) { return 0; }

/*---------------------------------------------------------------------------*/

#endif//__CROS3_V27REWRITE_EXT_HH__
