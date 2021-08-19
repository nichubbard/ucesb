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

#ifndef __NTUPLE_ITEM_HH__
#define __NTUPLE_ITEM_HH__

#include "util.hh"

#include <string>
#include <vector>

union src_ptr
{
  double* _ptr_double;
  float*  _ptr_float;
  ushort* _ptr_ushort;
  unsigned char* _ptr_uchar;
  int*    _ptr_int;
  uint*   _ptr_uint;
  uint64* _ptr_uint64;
};

union ntuple_limit
{
  struct
  {
    uint _min;
    uint _max;
  } _int;
};

struct ntuple_mask
{
  // for the time being: valid & overflow in same word

  uint* _ptr;
  uint  _valid;
  uint  _overflow;
};

#define NTUPLE_ITEM_HAS_LIMIT      0x0001
#define NTUPLE_ITEM_IS_ARRAY_MASK  0x0002
#define NTUPLE_ITEM_DO_SQRT        0x0004
#define NTUPLE_ITEM_DO_SQRT_INV    0x0008
#define NTUPLE_ITEM_BIT_MASK       0x0010
#define NTUPLE_ITEM_HAS_MASK_VALID 0x0020
#define NTUPLE_ITEM_HAS_MASK_OFL   0x0040
#define NTUPLE_ITEM_OMIT           0x0080

/* Timestamp info for time-stitching in struct_writer. */
#define NTUPLE_ITEM_TS_LO          0x0100
#define NTUPLE_ITEM_TS_HI          0x0200
#define NTUPLE_ITEM_TS_SRCID       0x0400
#define NTUPLE_ITEM_MEVENTNO       0x0800
#define NTUPLE_ITEM_MRG_STAT       0x1000
#define NTUPLE_ITEM_MRG_MASK       0x2000
#define NTUPLE_ITEM_MULT_NON0      0x4000

struct ntuple_item
{
  enum ptr_type
    {
      UNUSED = 0,
      DOUBLE,
      FLOAT,
      USHORT,
      UCHAR,
      INT,
      UINT,
      UINT64,
      INT_INDEX_CUT,
      INT_INDEX,
    };

public:
  ntuple_item(const std::string& name,
	      unsigned char* ptr,
	      const void *const *ptr_offset = NULL)
  {
    _type = UCHAR;
    _src._ptr_uchar = ptr;
    init (name,ptr_offset);
  }

  ntuple_item(const std::string& name,
	      ushort* ptr,
	      const void *const *ptr_offset = NULL)
  {
    _type = USHORT;
    _src._ptr_ushort = ptr;
    init (name,ptr_offset);
  }

  ntuple_item(const std::string& name,
	      int* ptr,
	      const void *const *ptr_offset = NULL)
  {
    _type = INT;
    _src._ptr_int = ptr;
    init (name,ptr_offset);
  }

  ntuple_item(const std::string& name,
	      uint* ptr,
	      const void *const *ptr_offset = NULL)
  {
    _type = UINT;
    _src._ptr_uint = ptr;
    init (name,ptr_offset);
  }

  ntuple_item(const std::string& name,
	      uint64* ptr,
	      const void *const *ptr_offset = NULL)
  {
    _type = UINT64;
    _src._ptr_uint64 = ptr;
    init (name,ptr_offset);
  }

  ntuple_item(const std::string& name,
	      float* ptr,
	      const void *const *ptr_offset = NULL)
  {
    _type = FLOAT;
    _src._ptr_float = ptr;
    init (name,ptr_offset);
  }

  ntuple_item(const std::string& name,
	      double* ptr,
	      const void *const *ptr_offset = NULL)
  {
    _type = DOUBLE;
    _src._ptr_double = ptr;
    init (name,ptr_offset);
  }

protected:
  void init(const std::string& name,
	    const void *const *ptr_offset)
  {
    _name = name;
    // _type = ...;
    // _src._ptr... = ...;
    _ptr_offset = ptr_offset;
    _index = 0;

    _ptr_limit2 = NULL;
    _max_limit2 = 0;
    _index2 = 0;

    _mask_bits = 0;

    _flags = 0;

    _ctrl_mask._ptr = NULL;
  }

public:
  // Common for RWN and CWN
  std::string _name;
  ptr_type _type;

  src_ptr  _src;
  const void *const *_ptr_offset;

  // Additional info for CWN
  std::string _block;
  std::string _index_var;
  uint        _index;

  const int*  _ptr_limit2;
  uint        _max_limit2; /* hmmm, different from _index (1) handling */
  uint        _index2;

  uint        _mask_bits;

  uint        _flags;

  ntuple_limit _limits;

  ntuple_mask _ctrl_mask;
};

typedef std::vector<ntuple_item*> vect_ntuple_items;

#endif//__NTUPLE_ITEM_HH__
