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

#ifndef __EXT_HADES_RICH_HH__
#define __EXT_HADES_RICH_HH__

#include "data_src.hh"
#include "dummy_external.hh"
#include "external_data.hh"

#include "zero_suppress.hh"

/*---------------------------------------------------------------------------*/

#include "rich_xy2upi_map.hh"

extern rich_xy2upi_map _rich_map;

/*---------------------------------------------------------------------------*/

DUMMY_EXTERNAL_MAP_STRUCT_FORW(EXT_HADES_RICH);

union rich_rawdata_word
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  struct
  {
    uint32 value         : 10;
    uint32 channel       : 6;
    uint32 module        : 3;
    uint32 port          : 3;
    uint32 controller    : 1;
    uint32 sector        : 3;
    uint32 dummy         : 6;
  };
  struct
  {
    uint32 cpmc_value    : 10;
    uint32 cpmc          : 13;
    uint32 cpmc_sector   : 3;
    uint32 cpmc_dummy    : 6;
  };
  struct
  {
    uint32 cpm_value     : 10;
    uint32 cpm_channel   : 6;
    uint32 cpm           : 7;
    uint32 cpm_sector    : 3;
    uint32 cpm_dummy     : 6;
  };
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  struct
  {
    uint32 dummy         : 6;
    uint32 sector        : 3;
    uint32 controller    : 1;
    uint32 port          : 3;
    uint32 module        : 3;
    uint32 channel       : 6;
    uint32 value         : 10;
  };
  struct
  {
    uint32 cpmc_dummy    : 6;
    uint32 cpmc_sector   : 3;
    uint32 cpmc          : 13;
    uint32 cpmc_value    : 10;
  };
  struct
  {
    uint32 cpm_dummy     : 6;
    uint32 cpm_sector    : 3;
    uint32 cpm           : 7;
    uint32 cpm_channel   : 6;
    uint32 cpm_value     : 10;
  };
#endif
  uint32 u32;

  void __clean()
  {
    u32 = 0;
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;
};

class EXT_HADES_RICH
{
public:
  EXT_HADES_RICH();
  ~EXT_HADES_RICH();

public:
  // wire_hits data;

  raw_list_ii_zero_suppress<rich_rawdata_word,rich_rawdata_word,2*8*8*64> data;

public:
  // For checking data: (hmm, should not be here... this is an
  // event-wise data structure)

  // rich_rawdata_word _fixed;

public:
  void __clean();
  EXT_DECL_UNPACK(/*_ARG:any arguments*/);
  // Needed if it is part of a select statement
  // EXT_DECL_MATCH();

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

public:
  DUMMY_EXTERNAL_SHOW_MEMBERS(EXT_HADES_RICH);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(EXT_HADES_RICH);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(EXT_HADES_RICH);
};

DUMMY_EXTERNAL_MAP_STRUCT(EXT_HADES_RICH);
DUMMY_EXTERNAL_WATCHER_STRUCT(EXT_HADES_RICH);
DUMMY_EXTERNAL_CORRELATION_STRUCT(EXT_HADES_RICH);

/*---------------------------------------------------------------------------*/

#endif//__EXT_HADES_RICH_HH__
