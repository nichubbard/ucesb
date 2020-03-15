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

#ifndef __RAW_DATA_HH__
#define __RAW_DATA_HH__

#include "typedef.hh"
#include "error.hh"

#include "bitsone.hh"

#include "../common/signal_id.hh"

#include "simple_data_ops.hh"

#include "enumerate.hh"
#include "pretty_dump.hh"

#include "zero_suppress_map.hh"

#include <assert.h>
#include <string.h>

//template<typename T>
//class calib_map;

template<typename T>
class data_map;

struct rawdata64
{
public:
  uint64  value;

public:
  void show_members(const signal_id &id,const char *unit) const;

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,this,ENUM_TYPE_DATA64),extra);
  }

  void dump()
  {
    printf ("0x%016llx",value);
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

  void __clean()
  {
    uint64* pthis = (uint64 *) (this);
    *pthis = 0;
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    insert_zero_suppress_info_ptrs(this,used_info);
  }

  operator uint64&() { return value; }
  operator const uint64&() const { return value; }

public:
  //uint32 *get_dest_info() { return &value; }

public:
  void map_members(const data_map<rawdata64> &map MAP_MEMBERS_PARAM) const;
};

struct rawdata32
{
public:
  uint32  value;

public:
  void show_members(const signal_id &id,const char *unit) const;

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,this,ENUM_TYPE_DATA32),extra);
  }

  void dump()
  {
    printf ("0x%08x",value);
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

  void __clean()
  {
    uint32* pthis = (uint32 *) (this);
    *pthis = 0;
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    insert_zero_suppress_info_ptrs(this,used_info);
  }

  operator uint32&() { return value; }
  operator const uint32&() const { return value; }

public:
  //uint32 *get_dest_info() { return &value; }

public:
  void map_members(const data_map<rawdata32> &map MAP_MEMBERS_PARAM) const;
};

struct rawdata24
{
public:
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint32  value    : 24;
  uint32  dummy1   : 8;
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  uint32  dummy1   : 8;
  uint32  value    : 24;
#endif

public:
  void show_members(const signal_id &id,const char *unit) const;

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,this,ENUM_TYPE_DATA24,0,0x00ffffff),extra);
  }

  void dump()
  {
    printf ("0x%08x",value);
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

  void __clean()
  {
    uint32* pthis = (uint32 *) (this);
    *pthis = 0;
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    insert_zero_suppress_info_ptrs(this,used_info);
  }

public:
  //uint32 *get_dest_info() { return &value; }

public:
  void map_members(const data_map<rawdata24> &map MAP_MEMBERS_PARAM) const;
};

struct rawdata16
{
public:
  uint16  value;

public:
  void show_members(const signal_id &id,const char *unit) const;

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,this,ENUM_TYPE_DATA16,0,0x0000ffff),extra);
  }

  void dump()
  {
    printf ("0x%04x",value);
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

  void __clean()
  {
    uint16* pthis = (uint16 *) (this);
    *pthis = 0;
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    insert_zero_suppress_info_ptrs(this,used_info);
  }

public:
  //uint32 *get_dest_info() { return &value; }

public:
  void map_members(const data_map<rawdata16> &map MAP_MEMBERS_PARAM) const;
};

// rawdata16 plus has additional range / over / underflow / pileup fields.
// Is therefore stored in 32 bit wide struct.
struct rawdata16plus
{
public:
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint16  value    : 16;
  uint16  underflow : 1;
  uint16  range    : 1;
  uint16  pileup   : 1;
  uint16  overflow : 1;
  uint16  dummy1   : 12;
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  uint16  dummy1   : 12;
  uint16  overflow : 1;
  uint16  pileup   : 1;
  uint16  range    : 1;
  uint16  underflow : 1;
  uint16  value    : 16;
#endif

public:
  void show_members(const signal_id &id, const char *unit) const;

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback, void *extra) const
  {
    // We actually have 20 bits, due to overrange and range bits...
    callback(id,
	     enumerate_info(info, this, ENUM_TYPE_DATA16PLUS, 0, 0x000fffff),
             extra);
  }

  void dump()
  {
    printf ("0x%04x%c%c%c%c",
	    value,
	    underflow ? 'U' : ' ',
	    range ?     'R' : ' ',
	    pileup ?    'P' : ' ',
	    overflow ?  'O' : ' ');
  }

  void dump(const signal_id &id, pretty_dump_info &pdi) const;

  void __clean()
  {
    uint32* pthis = (uint32 *) (this);
    *pthis = 0;
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    insert_zero_suppress_info_ptrs(this, used_info);
  }

public:
  void map_members(const data_map<rawdata16plus> &map MAP_MEMBERS_PARAM) const;
};

struct rawdata12
{
public:
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint16  value    : 12;
  uint16  range    : 1;
  uint16  dummy1   : 2;
  uint16  overflow : 1;
  // uint16  dummy2   : 16;
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  // uint16  dummy2   : 16;
  uint16  overflow : 1;
  uint16  dummy1   : 2;
  uint16  range    : 1;
  uint16  value    : 12;
#endif

public:
  void show_members(const signal_id &id,const char *unit) const;

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    // We claim to be 12 bits, but actually have 16, due to
    // overrange and range bits... :-(
    callback(id,enumerate_info(info,this,ENUM_TYPE_DATA12,0,0x0000ffff),extra);
  }

  void dump()
  {
    printf ("0x%03x%c%c",
	    value,
	    range    ? 'R' : ' ',
	    overflow ? 'O' : ' ');
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

  void __clean()
  {
    uint16* pthis = (uint16 *) (this);
    *pthis = 0;
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    insert_zero_suppress_info_ptrs(this,used_info);
  }

public:
  //uint32 *get_dest_info() { return &value; }

public:
  void map_members(const data_map<rawdata12> &map MAP_MEMBERS_PARAM) const;
};

// rawdata12 and 14 are different in at what bit the range is.
// Hmm, what we needed was really a overflow bit,
// and they are at the same position.  Do it like this for now.
struct rawdata14
{
public:
#if __BYTE_ORDER == __LITTLE_ENDIAN
  uint16  value    : 14;
  uint16  range    : 1;
  uint16  overflow : 1;
  // uint16  dummy2   : 16;
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  // uint16  dummy2   : 16;
  uint16  overflow : 1;
  uint16  range    : 1;
  uint16  value    : 14;
#endif

public:
  void show_members(const signal_id &id,const char *unit) const;

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    // We actually have 16 bits, due to overrange and range bits...
    callback(id,enumerate_info(info,this,ENUM_TYPE_DATA14,0,0x0000ffff),extra);
  }

  void dump()
  {
    printf ("0x%04x%c%c",
	    value,
	    range ?    'R' : ' ',
	    overflow ? 'O' : ' ');
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

  void __clean()
  {
    uint16* pthis = (uint16 *) (this);
    *pthis = 0;
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    insert_zero_suppress_info_ptrs(this,used_info);
  }

public:
  //uint32 *get_dest_info() { return &value; }

public:
  void map_members(const data_map<rawdata14> &map MAP_MEMBERS_PARAM) const;
};

struct rawdata8
{
public:
  uint8   value;

public:
  void show_members(const signal_id &id,const char *unit) const;

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,this,ENUM_TYPE_DATA8,0,0x000000ff),extra);
  }

  void dump()
  {
    printf ("0x%02x",value);
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const;

  void __clean()
  {
    uint8* pthis = (uint8 *) (this);
    *pthis = 0;
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    insert_zero_suppress_info_ptrs(this,used_info);
  }

public:
  //uint32 *get_dest_info() { return &value; }

public:
  void map_members(const data_map<rawdata8> &map MAP_MEMBERS_PARAM) const;
};

#define DATA64           rawdata64
#define DATA32           rawdata32
#define DATA24           rawdata24
#define DATA16           rawdata16
#define DATA16_OVERFLOW  rawdata16plus
#define DATA12           rawdata12
#define DATA12_OVERFLOW  rawdata12
#define DATA12_RANGE     rawdata12
#define DATA14           rawdata14
#define DATA14_OVERFLOW  rawdata14
#define DATA14_RANGE     rawdata14
#define DATA8            rawdata8



template<typename T>
class toggle_item
{
public:
  typedef T item_t;

public:
  T _item;

  int _toggle_i;
  T _toggle_v[2];

public:
  void show_members(const signal_id &id,const char *unit) const
  {
    _item.show_members(id, unit);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const;

  void dump()
  {
    _item.dump();
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    _item.dump(id, pdi);
    dump_uint32((uint32) _toggle_i,signal_id(id,"tgli"),pdi);
    _toggle_v[0].dump(signal_id(signal_id(id,"tglv"),0),pdi);
    _toggle_v[1].dump(signal_id(signal_id(id,"tglv"),1),pdi);
  }

  void __clean()
  {
    _item.__clean();
    _toggle_i = 0;
    _toggle_v[0].__clean();
    _toggle_v[1].__clean();
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);

  void map_members(const data_map<T> &map MAP_MEMBERS_PARAM) const;  

};

#endif//__RAW_DATA_HH__
