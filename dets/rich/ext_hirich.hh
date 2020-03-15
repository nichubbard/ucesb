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

#ifndef __EXT_HIRICH_HH__
#define __EXT_HIRICH_HH__

#include "data_src.hh"
#include "dummy_external.hh"
#include "external_data.hh"

#include "zero_suppress.hh"

/*---------------------------------------------------------------------------*/

DUMMY_EXTERNAL_MAP_STRUCT_FORW(EXT_HIRICH);

union hirich_header_word
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  struct
  {
    uint32 count         : 8;
    uint32 dummy         : 8;
    uint32 event_count   : 8;
    uint32 module_no     : 7;
    uint32 valid         : 1;
  };
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  struct
  {
    uint32 valid         : 1;
    uint32 module_no     : 7;
    uint32 event_count   : 8;
    uint32 dummy         : 8;
    uint32 count         : 8;
  };
#endif
  uint32 u32;

  void __clean()
  {
    u32 = 0;
  }
  void dump(const signal_id &id,pretty_dump_info &pdi) const;
};


union hirich_data_word
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  struct
  {
    uint16 value         : 8;
    uint16 channel       : 8;
  };
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  struct
  {
    uint16 channel       : 8;
    uint16 value         : 8;
  };
#endif
  uint16 u16;

  void __clean()
  {
    u16 = 0;
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;
};

struct EXT_HIRICH_module
{
public:
  hirich_header_word header;
  raw_list_ii_zero_suppress<hirich_data_word,hirich_data_word,256> data;

  void __clean()
  {
    header.__clean();
    data.__clean();
  }

  void append_channel(uint16 d,
		      bitsone<256> &channels_seen);

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const;
};

#define HIRICH_MARKER_HEADER  0x4655434b
#define HIRICH_MARKER_FOOTER  0x53484954

class EXT_HIRICH
{
public:
  EXT_HIRICH();
  ~EXT_HIRICH();

public:
  int               _num_modules;
  EXT_HIRICH_module _modules[128];

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
  DUMMY_EXTERNAL_SHOW_MEMBERS(EXT_HIRICH);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(EXT_HIRICH);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(EXT_HIRICH);

public:
  void map_members(const struct EXT_HIRICH_map &map MAP_MEMBERS_PARAM) const;
};

DUMMY_EXTERNAL_WATCHER_STRUCT(EXT_HIRICH);
DUMMY_EXTERNAL_CORRELATION_STRUCT(EXT_HIRICH);

/*---------------------------------------------------------------------------*/

class EXT_HIRICH_RAW;

class EXT_HIRICH_map
{
public:
  void add_map(int start,EXT_HIRICH_RAW *dest,int wire);
  void clean_maps();

public:
  DUMMY_EXTERNAL_MAP_ENUMERATE_MAP_MEMBERS(EXT_HIRICH);
};

/*---------------------------------------------------------------------------*/

#if 0
class EXT_HIRICH_RAW
{
public:
  EXT_HIRICH_RAW();

public:
  // Just one big array of masks, and one big array of values... :-)
  // implicitly indexed as y*HIRICH_SIZE+x

  raw_array_zero_suppress<uint8,uint8,HIRICH_SIZE*HIRICH_SIZE> data;

public:
  void __clean();

  DUMMY_EXTERNAL_DUMP(EXT_HIRICH_RAW);
  DUMMY_EXTERNAL_SHOW_MEMBERS(EXT_HIRICH_RAW);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(EXT_HIRICH_RAW);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(EXT_HIRICH_RAW);
};

DUMMY_EXTERNAL_CALIB_MAP_STRUCT(EXT_HIRICH_RAW);
DUMMY_EXTERNAL_WATCHER_STRUCT(EXT_HIRICH_RAW);
DUMMY_EXTERNAL_CORRELATION_STRUCT(EXT_HIRICH_RAW);
#endif

/*---------------------------------------------------------------------------*/

#endif//__EXT_HIRICH_HH__
