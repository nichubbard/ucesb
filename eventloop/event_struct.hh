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

class unpack_event_base
{

public:
  uint16 trigger;
  uint16 dummy;
  uint32 event_no;

#if STICKY_EVENT_IS_NONTRIVIAL
  uint32 sticky_idx;
#endif

public:
  void __clean()
  {
    trigger  = 0;
    dummy    = 0;
    event_no = 0;
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
			    ENUM_NTUPLE_ALWAYS,0,15),extra);
    callback(signal_id(id,"EVENTNO"),
	     enumerate_info(info,&event_no,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS),extra);
#if STICKY_EVENT_IS_NONTRIVIAL
    callback(signal_id(id,"STIDX"),
	     enumerate_info(info,&sticky_idx,
			    ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS),extra);
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

public:
  void __clean()
  {
#if USING_MULTI_EVENTS
    trigger      = 0;
    event_no     = 0;
    event_sub_no = 0;
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
			    ENUM_NTUPLE_NEVER,0,15),extra);
    callback(signal_id(id,"EVENTNO"),
	     enumerate_info(info,&event_no,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_NEVER),extra);
    callback(signal_id(id,"MEVENTNO"),
	     enumerate_info(info,&event_sub_no,ENUM_TYPE_UINT |
			    ENUM_NTUPLE_ALWAYS),extra);
#endif
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
#if USING_MULTI_EVENTS
    ::zero_suppress_info_ptrs(&trigger,used_info);
    ::zero_suppress_info_ptrs(&event_no,used_info);
    ::zero_suppress_info_ptrs(&event_sub_no,used_info);
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
