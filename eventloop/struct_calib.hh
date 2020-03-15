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

#ifndef __STRUCT_CALIB_HH__
#define __STRUCT_CALIB_HH__

class unpack_event;
class unpack_sticky_event;

//void setup_calib_map();
void clear_calib_map();
void set_rnd_seed_calib_map();
void show_calib_map();
void do_calib_map(raw_event *raw_ev);
void do_calib_map(raw_sticky *raw_ev);

#include "raw_calib_map.hh"

#define STRUCT_MIRROR_FCNS_DECL(name)           \
 public:                                        \
  void show(const signal_id &id);               \
  void set_rnd_seed(const mix_rnd_seed &rnd_seed); \
  void clear();                                 \
  void map_members(const name &src) const;      \
  void enumerate_map_members(const signal_id &id,   \
                             const enumerate_info &info, \
                             enumerate_fcn callback,void *extra) const;
#define STRUCT_MIRROR_TYPE(type)         type##_calib_map
#define STRUCT_MIRROR_TYPE_TOGGLE(type)  toggle_##type##_calib_map
#define STRUCT_MIRROR_NAME(name)         name
#define STRUCT_MIRROR_STRUCT(type)       STRUCT_MIRROR_TYPE(type)
#define STRUCT_MIRROR_BASE(type)         STRUCT_MIRROR_TYPE(type)
#define STRUCT_MIRROR_TEMPLATE_ARG(arg)  arg##_calib_map,arg
#define STRUCT_MIRROR_TEMPLATE_ARG_N(arg,array)  arg##_calib_map array,arg array
#define STRUCT_MIRROR_TEMPLATE_ARG_TOGGLE(arg)  toggle_##arg##_calib_map,TOGGLE(arg)
#define STRUCT_MIRROR_TEMPLATE_ARG_TOGGLE_N(arg,array)  toggle_##arg##_calib_map array,TOGGLE(arg) array
#define STRUCT_ONLY_LAST_UNION_MEMBER    1

#include "gen/raw_struct_mirror.hh"

#undef  STRUCT_MIRROR_FCNS_DECL
#undef  STRUCT_MIRROR_TYPE
#undef  STRUCT_MIRROR_TYPE_TOGGLE
#undef  STRUCT_MIRROR_NAME
#undef  STRUCT_MIRROR_STRUCT
#undef  STRUCT_MIRROR_BASE
#undef  STRUCT_MIRROR_TEMPLATE_ARG
#undef  STRUCT_MIRROR_TEMPLATE_ARG_N
#undef  STRUCT_MIRROR_TEMPLATE_ARG_TOGGLE
#undef  STRUCT_MIRROR_TEMPLATE_ARG_TOGGLE_N
#undef  STRUCT_ONLY_LAST_UNION_MEMBER

extern raw_event_calib_map the_raw_event_calib_map;

template<typename T_src>
template<typename T_dest>
void raw_to_cal<T_src>::set_dest(T_dest *dest, int toggle_i)
{
  const zero_suppress_info *info;

  info = get_ptr_zero_suppress_info(dest,NULL,false);

  if (info->_type == ZZP_INFO_CALL_LIST_INDEX ||
      info->_type == ZZP_INFO_CALL_LIST_II_INDEX)
    dest = (T_dest *) (((char*) dest) - info->_list._dest_offset);

  _dest = dest;
  _zzp_info = info;
  (void) toggle_i;
}

#endif//__STRUCT_CALIB_HH__
