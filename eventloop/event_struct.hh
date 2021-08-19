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

#ifndef __EVENT_STRUCT_HH__
#define __EVENT_STRUCT_HH__

#include "gen/unpacker_defines.hh"
#include "../common/signal_id.hh"

#include "enumerate.hh"
#include "zero_suppress_map.hh"

#include "multi_info.hh"
#include "pretty_dump.hh"

#include <utility>
#include <vector>

class unpack_event_base
{

public:
  uint16 trigger;
  uint16 dummy;
  uint32 event_no;
  std::vector<std::pair<int, uint64>> wr;
  
#if STICKY_EVENT_IS_NONTRIVIAL
  uint32 sticky_idx;
#endif

#if USE_INPUTFILTER
  bool is_aida;
  bool aida_implant;
  int16_t aida_length;
#endif

public:
  void __clean()
  {
    trigger  = 0;
    dummy    = 0;
    event_no = 0;
    wr.clear();
  }

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const;

public:
  void show_members(const signal_id &id,const char *unit) const { UNUSED(id); UNUSED(unit); }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    callback(signal_id(id,"TRIGGER"),
	     enumerate_info(info,&trigger,ENUM_TYPE_USHORT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_MULT_NON0,0,15),extra);
    callback(signal_id(id,"EVENTNO"),
	     enumerate_info(info,&event_no,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_MULT_NON0),extra);
#if STICKY_EVENT_IS_NONTRIVIAL
    callback(signal_id(id,"STIDX"),
	     enumerate_info(info,&sticky_idx,
			    ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_MULT_NON0),extra);
#endif
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    ::zero_suppress_info_ptrs(&trigger,used_info);
    ::zero_suppress_info_ptrs(&event_no,used_info);
#if STICKY_EVENT_IS_NONTRIVIAL
    ::zero_suppress_info_ptrs(&sticky_idx,used_info);
#endif
  }

  // void map_members(const struct unpack_event_base_map& map MAP_MEMBERS_PARAM) const { }
};

class unpack_sticky_event_base :
  public unpack_event_base
{

};

class unpack_subevent_base
{

public:
#ifdef USE_LMD_INPUT
  uint16 type;
  uint16 subtype;
  uint8  control;
  uint8  subcrate;
  uint16 procid;
#elif defined(USE_RIDF_INPUT)
  uint8 rev;
  uint8 dev;
  uint8 fp;
  uint8 det;
  uint8 mod;
#endif

  void __clean()
  {
#ifdef USE_LMD_INPUT
    type = 0;
    subtype = 0;
    control = 0;
    subcrate = 0;
    procid = 0;
#elif defined(USE_RIDF_INPUT)
    rev = 0;
    dev = 0;
    fp = 0;
    det = 0;
    mod = 0;
#endif
  }

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const;

public:
  void show_members(const signal_id &id,const char *unit) const { }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const { }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info) { }

  // void map_members(const struct unpack_subevent_base_map& map MAP_MEMBERS_PARAM) const { }
};

class unpack_sticky_subevent_base :
  public unpack_subevent_base
{

};

class raw_event_base
{

public:
#if USING_MULTI_EVENTS
  uint16 trigger;
  uint32 event_no;
  uint32 event_sub_no;
#endif
#if USING_TSTAMP
  uint32 tstamp_lo;
  uint32 tstamp_hi;
  uint32 tstamp_srcid;
  // This is filled by merger in ext_writer, here to provide member:
  uint32 merge_status;
  uint32 merge_idmask;
#endif

public:
  void __clean()
  {
#if USING_MULTI_EVENTS
    trigger      = 0;
    event_no     = 0;
    event_sub_no = 0;
#endif
#if USING_TSTAMP
    tstamp_lo    = 0;
    tstamp_hi    = 0;
    tstamp_srcid = 0;
    merge_status = 0;
    merge_idmask = 0;
#endif
  }

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const { }

public:
  void show_members(const signal_id &id,const char *unit) const { }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
#if USING_MULTI_EVENTS
    callback(signal_id(id,"TRIGGER"),
	     enumerate_info(info,&trigger,ENUM_TYPE_USHORT |
			    ENUM_NTUPLE_NEVER |
			    ENUM_NTUPLE_MULT_NON0,0,15),extra);
    callback(signal_id(id,"EVENTNO"),
	     enumerate_info(info,&event_no,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_NEVER |
			    ENUM_NTUPLE_MULT_NON0),extra);
    callback(signal_id(id,"MEVENTNO"),
	     enumerate_info(info,&event_sub_no,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_MEVENTNO |
			    ENUM_NTUPLE_MULT_NON0),extra);
#endif
#if USING_TSTAMP
    callback(signal_id(id,"TSTAMPLO"),
	     enumerate_info(info,&tstamp_lo,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_TS_LO |
			    ENUM_NTUPLE_MULT_NON0),extra);
    callback(signal_id(id,"TSTAMPHI"),
	     enumerate_info(info,&tstamp_hi,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_TS_HI |
			    ENUM_NTUPLE_MULT_NON0),extra);
    callback(signal_id(id,"TSTAMPSRCID"),
	     enumerate_info(info,&tstamp_srcid,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_TS_SRCID |
			    ENUM_NTUPLE_MULT_NON0),extra);
    callback(signal_id(id,"MERGE_STATUS"),
	     enumerate_info(info,&merge_status,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_MRG_STAT |
			    ENUM_NTUPLE_MULT_NON0),extra);
    callback(signal_id(id,"MERGE_IDMASK"),
	     enumerate_info(info,&merge_idmask,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS |
			    ENUM_NTUPLE_MRG_MASK |
			    ENUM_NTUPLE_MULT_NON0),extra);
#endif
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
#if USING_MULTI_EVENTS
    ::zero_suppress_info_ptrs(&trigger,used_info);
    ::zero_suppress_info_ptrs(&event_no,used_info);
    ::zero_suppress_info_ptrs(&event_sub_no,used_info);
#endif
#if USING_TSTAMP
    ::zero_suppress_info_ptrs(&tstamp_lo,used_info);
    ::zero_suppress_info_ptrs(&tstamp_hi,used_info);
    ::zero_suppress_info_ptrs(&tstamp_srcid,used_info);
    ::zero_suppress_info_ptrs(&merge_status,used_info);
    ::zero_suppress_info_ptrs(&merge_idmask,used_info);
#endif
  }

  // void map_members(const struct unpack_subevent_base_map& map MAP_MEMBERS_PARAM) const { }
};

class raw_sticky_base :
  public raw_event_base
{

};

class cal_event_base
{

public:
  void __clean() { }

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const { }

public:
  void show_members(const signal_id &id,const char *unit) const { }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const { }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info) { }

  // void map_members(const struct unpack_subevent_base_map& map MAP_MEMBERS_PARAM) const { }
};

#endif//__EVENT_STRUCT_HH__
