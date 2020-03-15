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

#ifndef __STRUCT_FCNS_HH__
#define __STRUCT_FCNS_HH__

#include "../common/signal_id.hh"
#include "enumerate.hh"

struct used_zero_suppress_info;
struct pretty_dump_info;

#define CALIB_STRUCT_FCNS_DECL                  \
 public:                                        \
  void __clean();                               \
  void dump(const signal_id &id,                \
            pretty_dump_info &pdi) const;       \
  void show_members(const signal_id &id,const char *unit) const; \
  void enumerate_members(const signal_id &id,   \
                         const enumerate_info &info, \
                         enumerate_fcn callback,void *extra) const; \

#define USER_STRUCT_FCNS_DECL                   \
  CALIB_STRUCT_FCNS_DECL                        \
  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);

#define STRUCT_FCNS_DECL(name)                  \
  USER_STRUCT_FCNS_DECL;                        \
 public:                                        \
  void map_members(const class name##_map &map MAP_MEMBERS_PARAM); /* non-const for multi ptr assignment*/ \
  /*void map_members(const struct name##_calib_map &map) const;*/ \
  uint32 get_event_counter() const; /* only implemented for some modules! */ \
  uint32 get_event_counter_offset(uint32 start) const; /* only implemented for some modules! */ \
  bool good_event_counter_offset(uint32 expect) const; /* only implemented for some modules! */ \
  uint32 get_external_toggle_mask() const; /* only implemented for some modules! */ \
  ;

#define UNIT(x)   // replace with nothing, handled by PSDC

#define TOGGLE(x) toggle_item<x>

#endif//__STRUCT_FCNS_HH__

