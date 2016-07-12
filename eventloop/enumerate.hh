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

#ifndef __ENUMERATE_HH__
#define __ENUMERATE_HH__

#include "typedef.hh"
#include "error.hh"

#define ENUM_TYPE_FLOAT     0x0001
#define ENUM_TYPE_DOUBLE    0x0002
#define ENUM_TYPE_USHORT    0x0003
#define ENUM_TYPE_UCHAR     0x0004
#define ENUM_TYPE_INT       0x0005
#define ENUM_TYPE_UINT      0x0006
#define ENUM_TYPE_ULINT     0x0007
#define ENUM_TYPE_UINT64    0x0008
#define ENUM_TYPE_DATA8     0x0009
#define ENUM_TYPE_DATA12    0x000a
#define ENUM_TYPE_DATA16    0x000b
#define ENUM_TYPE_DATA24    0x000c
#define ENUM_TYPE_DATA32    0x000d
#define ENUM_TYPE_DATA64    0x000e
#define ENUM_TYPE_MASK      0x000f
#define ENUM_HAS_INT_LIMIT  0x0010
#define ENUM_IS_LIST_LIMIT  0x0020
#define ENUM_IS_LIST_LIMIT2 0x0040 // second-level limit (multi-hit)
#define ENUM_IS_ARRAY_MASK  0x0080
#define ENUM_IS_LIST_INDEX  0x0100

// item is (non-first) part of indexed list, may not be used as mapping
// destination
#define ENUM_NO_INDEX_DEST  0x0200

#define ENUM_HAS_PTR_OFFSET 0x0400

typedef bool(*set_dest_fcn)(void *,void *);

class prefix_units_exponent;

struct enumerate_info
{
public:
  enumerate_info()
  {
    _type = 0;
    _unit = NULL;

    _only_index0 = false;
    _signal_id_zzp_part = (size_t) -1;
  }

public:
  enumerate_info(const enumerate_info &src,const void *addr,int type)
  {
    if ((src._type & ~(ENUM_NO_INDEX_DEST | ENUM_HAS_PTR_OFFSET)) != 0)
      {
	// If you reach this point, you have not protected the code enough.
	// We can only handle one level of ptr_offset
	ERROR("Internal error in enumerate_info constructor (1).");
      }

    _addr = addr;
    _type = src._type | type;

    _min = _max = 0;

    _ptr_offset = src._ptr_offset;
    _unit = src._unit;

    _only_index0 = src._only_index0;
    _signal_id_zzp_part = src._signal_id_zzp_part;
  }

  enumerate_info(const enumerate_info &src,const void *addr,int type,
		 unsigned int min,unsigned int max)
  {
    if ((src._type & ~(ENUM_NO_INDEX_DEST | ENUM_HAS_PTR_OFFSET)) != 0)
      {
	// If you reach this point, you have not protected the code enough.
	// We can only handle one level of ptr_offset
	ERROR("Internal error in enumerate_info constructor (2).");
      }

    _addr = addr;
    _type = src._type | type | ENUM_HAS_INT_LIMIT;
    _min  = min;
    _max  = max;

    _ptr_offset = src._ptr_offset;
    _unit = src._unit;

    _only_index0 = src._only_index0;
    _signal_id_zzp_part = src._signal_id_zzp_part;
  }

  enumerate_info(const enumerate_info &src,
		 size_t signal_id_zzp_part)
  {
    if (src._signal_id_zzp_part != (size_t) -1)
      {
	// If you reach this point, you have not protected the code enough.
	// We can only handle one level of zero suppression.
	ERROR("Internal error in enumerate_info constructor (3).");
      }

    _type = src._type;
    _unit = src._unit;

    _only_index0 = src._only_index0;
    _signal_id_zzp_part = signal_id_zzp_part;
  }

public:
  enumerate_info &set_dest(set_dest_fcn sd)
  {
    _set_dest = sd;
    return *this;
  }

public:
  const void  *_addr;
  int          _type;
  const prefix_units_exponent *_unit;

  unsigned int _min;
  unsigned int _max;

  const void *const *_ptr_offset;

  set_dest_fcn _set_dest;

  bool         _only_index0;
  size_t       _signal_id_zzp_part;
};

struct rawdata8;
struct rawdata12;
struct rawdata16;
struct rawdata24;
struct rawdata32;
struct rawdata64;

inline int get_enum_type(rawdata8  *) { return ENUM_TYPE_DATA8; }
inline int get_enum_type(rawdata12 *) { return ENUM_TYPE_DATA12; }
inline int get_enum_type(rawdata16 *) { return ENUM_TYPE_DATA16; }
inline int get_enum_type(rawdata24 *) { return ENUM_TYPE_DATA24; }
inline int get_enum_type(rawdata32 *) { return ENUM_TYPE_DATA32; }
inline int get_enum_type(rawdata64 *) { return ENUM_TYPE_DATA64; }
inline int get_enum_type(uint64    *) { return ENUM_TYPE_UINT64; }
inline int get_enum_type(uint32    *) { return ENUM_TYPE_UINT; }
inline int get_enum_type(uint16    *) { return ENUM_TYPE_USHORT; }
inline int get_enum_type(uint8     *) { return ENUM_TYPE_UCHAR; }
inline int get_enum_type(float     *) { return ENUM_TYPE_FLOAT; }
inline int get_enum_type(double    *) { return ENUM_TYPE_DOUBLE; }

void enumerate_member(const signal_id &id,const enumerate_info &info,
		      void *extra);

typedef void(*enumerate_fcn)(const signal_id &,const enumerate_info &,void *);

void get_enum_type_name(int type,char *dest,size_t length);

#endif//__ENUMERATE_HH__
