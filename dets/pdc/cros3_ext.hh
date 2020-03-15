/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
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

#ifndef __CROS3_EXT_HH__
#define __CROS3_EXT_HH__

#include "data_src.hh"
#include "dummy_external.hh"
#include "external_data.hh"

#include "threshold_cros3.h"

/*---------------------------------------------------------------------------*/

DUMMY_EXTERNAL_MAP_STRUCT_FORW(EXT_CROS3);

struct wire_hit
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

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const;
};

struct wire_hits
{
public:
  wire_hit hits[256*128];
  int      num_hits;

public:
  void __clean()
  {
    num_hits = 0;
  }

public:
  void hit(char wire,char start_slice,char stop_slice = 0)
  {
    wire_hit &hit = hits[num_hits++];

    hit.wire  = wire;
    hit.start = start_slice;
    hit.stop  = stop_slice;
  }

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const;
};

class EXT_CROS3
{
public:
  EXT_CROS3();
  ~EXT_CROS3();

public:
  uint16 *_scramble_buffer;
  size_t  _scramble_length;

public:
  uint32 *_dest_buffer;
  uint32 *_dest_buffer_end;
  uint32 *_dest_end;

public:
  wire_hits data;

  cros3_threshold_stat trc;

  void *_noise_stat; // (rw_cros3_tot_noisy *)

public:
  void __clean();
  EXT_DECL_UNPACK_ARG(uint16 ccb_id);
  // Needed if it is part of a select statement
  EXT_DECL_MATCH_ARG(uint16 ccb_id);

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

public:
  DUMMY_EXTERNAL_SHOW_MEMBERS(EXT_CROS3);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(EXT_CROS3);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(EXT_CROS3);
};

DUMMY_EXTERNAL_MAP_STRUCT(EXT_CROS3);
DUMMY_EXTERNAL_WATCHER_STRUCT(EXT_CROS3);
DUMMY_EXTERNAL_CORRELATION_STRUCT(EXT_CROS3);

/*---------------------------------------------------------------------------*/

#endif//__CROS3_EXT_HH__
