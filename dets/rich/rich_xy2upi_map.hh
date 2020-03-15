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

#ifndef __RICH_XY2UPI_MAP_HH__
#define __RICH_XY2UPI_MAP_HH__

#include "typedef.hh"
#include "endian.hh"

// For mapping upi -> xy

struct rich_map_xy_item
{
  uint8 _x, _y;
};

// The f postfix indicates 'fast', index directly with fill cpm or
// cpmc variable

union rich_map_module_xy
{
  uint8 _used [2/*contoller*/][8/*port*/][8/*module*/];
  uint8 _usedf[2/*contoller*/ *8/*port*/ *8/*module*/];
};

union rich_map_xy
{
  rich_map_xy_item _upi [2/*contoller*/][8/*port*/][8/*module*/][64/*channel*/];
  rich_map_xy_item _upif[2/*contoller*/ *8/*port*/ *8/*module*/ *64/*channel*/];
};

// For mapping xy -> upi

union rich_map_upi_item
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  struct
  {
    uint16 channel      : 6;
    uint16 module       : 3;
    uint16 port         : 3;
    uint16 controller   : 1;
    uint16 sector       : 3; // always 0 in the map, as it is same for all sectors
  };
  struct
  {
    uint16 cpmc         : 13;
    uint16 dummy_sector : 3;
  };
  struct
  {
    uint16 dummy2_channel : 6;
    uint16 cpm            : 7;
    uint16 dummy2_sector  : 3;
  };
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  struct
  {
    uint16 sector       : 3; // always 0 in the map, as it is same for all sectors
    uint16 controller   : 1;
    uint16 port         : 3;
    uint16 module       : 3;
    uint16 channel      : 6;
  };
  struct
  {
    uint16 dummy_sector : 3;
    uint16 cpmc         : 13;
  };
  struct
  {
    uint16 dummy2_sector  : 3;
    uint16 cpm            : 7;
    uint16 dummy2_channel : 6;
  };
#endif
  uint16 u16;
};

struct rich_map_upi
{
  rich_map_upi_item _xy[96][96];
};

// For mapping upi -> new (sorted by y,x) index
// To do re-ordering before attempting Huffman encoding

struct rich_map_reindex_item
{
  uint16 _index_module; // new index within module
  uint16 _index_sector; // new index within sector
};

struct rich_map_reindex
{
  rich_map_reindex_item _upif[2/*contoller*/ *8/*port*/ *8/*module*/ *64/*channel*/];
};

// All remap info

struct rich_xy2upi_map
{
public:
  rich_xy2upi_map();

  void setup(const char *filename);

public:
  rich_map_upi       _xy2upi;
  rich_map_xy        _upi2xy;
  rich_map_module_xy _upi2mod; // for debug, know which modules exist at all

  rich_map_reindex   _upi2index;
};

#endif//__RICH_XY2UPI_MAP_HH__
