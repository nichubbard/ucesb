/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2018  Haakan T. Johansson  <f96hajo@chalmers.se
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

#ifndef __SIMPLE_DATA_OPS_HH__
#define __SIMPLE_DATA_OPS_HH__

#include "pretty_dump.hh"

#include "enumerate.hh"

struct used_zero_suppress_info;

void insert_zero_suppress_info_ptrs(void *us,
				    used_zero_suppress_info &used_info);

////////////////////////////////////////////////////////////////////

template<typename T>
inline void call___clean(T &us)
{
  us.__clean();
}

#define CALL___CLEAN_0(type)		\
  void __clean_##type(type &item);	\
  inline void call___clean(type &us)	\
  {					\
    __clean_##type(us);			\
  }

CALL___CLEAN_0(float);
CALL___CLEAN_0(double);
CALL___CLEAN_0(uint64);
CALL___CLEAN_0(uint32);
CALL___CLEAN_0(uint16);
CALL___CLEAN_0(uint8);

////////////////////////////////////////////////////////////////////

template<typename T>
inline void call_show_members(T &us,const signal_id &id,const char* unit)
{
  us.show_members(id,unit);
}

#define DEF_CALL_SHOW_MEMBERS(type)				\
  void call_show_members(const type &us,			\
			 const signal_id &id,const char* unit);

DEF_CALL_SHOW_MEMBERS(float);
DEF_CALL_SHOW_MEMBERS(double);
DEF_CALL_SHOW_MEMBERS(uint64);
DEF_CALL_SHOW_MEMBERS(uint32);
DEF_CALL_SHOW_MEMBERS(uint16);
DEF_CALL_SHOW_MEMBERS(uint8);

////////////////////////////////////////////////////////////////////

template<typename T>
inline void call_zero_suppress_info_ptrs(T *us,
					 used_zero_suppress_info &used_info)
{
  us->zero_suppress_info_ptrs(used_info);
}

#define CALL_ZERO_SUPPRESS_INFO_PTRS(type)				\
  inline void								\
  call_zero_suppress_info_ptrs(type *us,				\
			       used_zero_suppress_info &used_info)	\
  {									\
    ::insert_zero_suppress_info_ptrs(us,used_info);			\
  }

CALL_ZERO_SUPPRESS_INFO_PTRS(float);
CALL_ZERO_SUPPRESS_INFO_PTRS(double);
CALL_ZERO_SUPPRESS_INFO_PTRS(uint64);
CALL_ZERO_SUPPRESS_INFO_PTRS(uint32);
CALL_ZERO_SUPPRESS_INFO_PTRS(uint16);
CALL_ZERO_SUPPRESS_INFO_PTRS(uint8);

////////////////////////////////////////////////////////////////////

template<typename T>
inline void call_enumerate_members(const T *us,
				   const signal_id &id,
				   const enumerate_info &info,
				   enumerate_fcn callback,void *extra)
{
  us->enumerate_members(id, info, callback, extra);
}

#define CALL_ENUMERATE_MEMBERS(type/*,enum_type*/)			\
  inline void								\
  call_enumerate_members(const type *us,				\
			 const signal_id &id,				\
			 const enumerate_info &info,			\
			 enumerate_fcn callback,void *extra)		\
  {									\
    callback(id,enumerate_info(info,us,					\
			       /*enum_type*/get_enum_type(us)),		\
	     extra);							\
  }

CALL_ENUMERATE_MEMBERS(float/*, ENUM_TYPE_FLOAT*/);
CALL_ENUMERATE_MEMBERS(double/*,ENUM_TYPE_DOUBLE*/);
CALL_ENUMERATE_MEMBERS(uint64/*,ENUM_TYPE_DATA64*/);
CALL_ENUMERATE_MEMBERS(uint32/*,ENUM_TYPE_DATA32*/);
CALL_ENUMERATE_MEMBERS(uint16/*,ENUM_TYPE_DATA16*/);
CALL_ENUMERATE_MEMBERS(uint8/*, ENUM_TYPE_DATA8*/);

////////////////////////////////////////////////////////////////////

template<typename T>
inline void call_dump(const T &us,
		      const signal_id &id,pretty_dump_info &pdi)
{
  us.dump(id, pdi);
}

#define CALL_DUMP(type)						\
  void dump_##type(const type item,				\
		   const signal_id &id,pretty_dump_info &pdi);	\
  inline void							\
  call_dump(const type &us,					\
	    const signal_id &id,pretty_dump_info &pdi)		\
  {								\
    dump_##type(us, id, pdi);					\
  }

CALL_DUMP(float);
CALL_DUMP(double);
CALL_DUMP(uint64);
CALL_DUMP(uint32);
CALL_DUMP(uint16);
CALL_DUMP(uint8);

////////////////////////////////////////////////////////////////////


#endif//__SIMPLE_DATA_OPS_HH__
