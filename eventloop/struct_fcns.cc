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

#include "structures.hh"
#include "error.hh"
#include "util.hh"

#include "simple_data_ops.hh"

#include "../common/prefix_unit.hh"

#include <math.h>

template<typename T>
void show_members(const signal_id &id,const char *unit)
{
  char buf[256];
  id.format(buf,sizeof(buf));

  char buf_paw[256];
  id.format_paw(buf_paw,sizeof(buf_paw));

  printf ("%-30s %-30s %s\n",buf_paw,buf,unit ? unit : "");
}

void rawdata64::show_members(const signal_id &id,const char *unit) const
{ ::show_members<rawdata64>(id,unit); }
void rawdata32::show_members(const signal_id &id,const char *unit) const
{ ::show_members<rawdata32>(id,unit); }
void rawdata24::show_members(const signal_id &id,const char *unit) const
{ ::show_members<rawdata24>(id,unit); }
void rawdata16::show_members(const signal_id &id,const char *unit) const
{ ::show_members<rawdata16>(id,unit); }
void rawdata16plus::show_members(const signal_id &id,const char *unit) const
{ ::show_members<rawdata16plus>(id,unit); }
void rawdata14::show_members(const signal_id &id,const char *unit) const
{ ::show_members<rawdata14>(id,unit); }
void rawdata12::show_members(const signal_id &id,const char *unit) const
{ ::show_members<rawdata12>(id,unit); }
void rawdata8::show_members(const signal_id &id,const char *unit) const
{ ::show_members<rawdata8>(id,unit); }

#define IMPL_CALL_SHOW_MEMBERS(type)					\
  void call_show_members(const type &us,				\
			 const signal_id &id,const char* unit)		\
  {									\
    ::show_members<type>(id,unit);					\
  }

IMPL_CALL_SHOW_MEMBERS(float);
IMPL_CALL_SHOW_MEMBERS(double);
IMPL_CALL_SHOW_MEMBERS(uint64);
IMPL_CALL_SHOW_MEMBERS(uint32);
IMPL_CALL_SHOW_MEMBERS(uint16);
IMPL_CALL_SHOW_MEMBERS(uint8);

////////////////////////////////////////////////////////////////////

void enumerate_members_double(const double *ptr,const signal_id &id,
			      const enumerate_info &info,
			      enumerate_fcn callback,void *extra)
{
  /*
  char buf[256];
  id.format(buf,sizeof(buf));
  WARNING("Cannot enumerate unknown type: %s",buf);
  */
  callback(id,enumerate_info(info,ptr,ENUM_TYPE_DOUBLE),extra);
}

void enumerate_members_float(const float *ptr,const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra)
{
  callback(id,enumerate_info(info,ptr,ENUM_TYPE_FLOAT),extra);
}

void enumerate_members_uint8(const uint8 *ptr,const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra)
{
  callback(id,enumerate_info(info,ptr,ENUM_TYPE_UCHAR),extra);
}

void enumerate_members_uint16(const uint16 *ptr,const signal_id &id,
			      const enumerate_info &info,
			      enumerate_fcn callback,void *extra)
{
  callback(id,enumerate_info(info,ptr,ENUM_TYPE_USHORT),extra);
}

void enumerate_members_uint32(const uint32 *ptr,const signal_id &id,
			      const enumerate_info &info,
			      enumerate_fcn callback,void *extra)
{
  callback(id,enumerate_info(info,ptr,ENUM_TYPE_UINT),extra);
}

void enumerate_members_uint64(const uint64 *ptr,const signal_id &id,
			      const enumerate_info &info,
			      enumerate_fcn callback,void *extra)
{
  callback(id,enumerate_info(info,ptr,ENUM_TYPE_UINT64),extra);
}

void enumerate_members_int(const int *ptr,const signal_id &id,
			   const enumerate_info &info,
			   enumerate_fcn callback,void *extra)
{
  callback(id,enumerate_info(info,ptr,ENUM_TYPE_INT),extra);
}

template<typename T>
void toggle_item<T>::
enumerate_members(const signal_id &id,
		  const enumerate_info &info,
		  enumerate_fcn callback,void *extra) const
{
  call_enumerate_members(&_item, id, info, callback, extra);
  
  callback(signal_id(id, "tgli"),
	   enumerate_info(info, &_toggle_i,
			  ENUM_TYPE_UINT |
			  ENUM_IS_TOGGLE_I),extra);

  for (int i = 0; i < 2; i++)
    call_enumerate_members(&_toggle_v[i],
			   signal_id(id, i == 0 ? "tglv1" : "tglv2"),
			   enumerate_info(info, NULL,
					  ENUM_IS_TOGGLE_V),
			   callback, extra);
}

#define FCNCALL_CLASS_NAME(name) name
#define FCNCALL_NAME(name) \
  show_members(const signal_id &__id,const char *__unit) const
#define FCNCALL_CALL_BASE() show_members(__id,__unit)
#define FCNCALL_CALL(member) show_members(__id,__unit) // show_members(signal_id(id,#member))
#define FCNCALL_CALL_TYPE(type,member) ::show_members<type>(__id,__unit) // ::show_members<type>(signal_id(id,#member))
#define FCNCALL_FOR(index,size) int index = size-1; // -1 to not get warnings
#define FCNCALL_SUBINDEX(index) const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,index+1,SIG_PART_INDEX_LIMIT);
#define FCNCALL_SUBNAME(name)   const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,name);
#define FCNCALL_MULTI_MEMBER(name) name()
#define FCNCALL_MULTI_ARG(name) name
#define FCNCALL_UNIT(unit) \
  if (__unit)              \
    ERROR("%s:%d: Cannot have several unit specifications! (should only occur at leaf)",__FILE__,__LINE__); \
  const char *&__shadow_unit = __unit; \
  const char *__unit = __shadow_unit; \
  __unit = unit;
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/struct_fcncall.hh"
#include "gen/raw_struct_fcncall.hh"
#include "gen/cal_struct_fcncall.hh"
#ifdef USER_STRUCT
#include "gen/user_struct_fcncall.hh"
#endif
#ifdef CALIB_STRUCT
#include "gen/calib_struct_fcncall.hh"
#endif

#undef  FCNCALL_CLASS_NAME
#undef  FCNCALL_NAME
#undef  FCNCALL_CALL_BASE
#undef  FCNCALL_CALL
#undef  FCNCALL_CALL_TYPE
#undef  FCNCALL_FOR
#undef  FCNCALL_SUBINDEX
#undef  FCNCALL_SUBNAME
#undef  FCNCALL_MULTI_MEMBER
#undef  FCNCALL_MULTI_ARG
#undef  FCNCALL_UNIT
#undef  STRUCT_ONLY_LAST_UNION_MEMBER



#define FCNCALL_CLASS_NAME(name) name
#define FCNCALL_NAME(name) \
  enumerate_members(const signal_id &__id,const enumerate_info &__info,enumerate_fcn __callback,void *__extra) const
#define FCNCALL_CALL_BASE() enumerate_members(__id,__info,__callback,__extra)
#define FCNCALL_CALL(member) enumerate_members(__id,__info,__callback,__extra) // enumerate_members(signal_id(id,#member))
#define FCNCALL_CALL_TYPE(type,member) ::enumerate_members_##type(&member,__id,__info,__callback,__extra) // ::enumerate_members<type>(signal_id(id,#member))
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,index);
#define FCNCALL_SUBNAME(name)   const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,name);
#define FCNCALL_MULTI_MEMBER(name) \
  if (__info._type != 0)                                    \
    ERROR("%s:%d: Cannot have address info before multi-member!",__FILE__,__LINE__); \
  const enumerate_info &__shadow_info = __info;             \
  UNUSED(__shadow_info);                                    \
  enumerate_info __info;                                    \
  __info._type = ENUM_HAS_PTR_OFFSET;                       \
  __info._ptr_offset = name.get_ptr_addr();                 \
  name()
#define FCNCALL_MULTI_ARG(name) name
#define FCNCALL_UNIT(unit) \
  if (__info._unit)                                         \
    ERROR("%s:%d: Cannot have several unit specifications! (should only occur at leaf)",__FILE__,__LINE__); \
  const enumerate_info &__shadow_info = __info;             \
  enumerate_info __info = __shadow_info;                    \
  __info._unit = insert_prefix_units_exp(file_line(),unit);
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/struct_fcncall.hh"
#include "gen/raw_struct_fcncall.hh"
#include "gen/cal_struct_fcncall.hh"
#ifdef USER_STRUCT
#include "gen/user_struct_fcncall.hh"
#endif
#ifdef CALIB_STRUCT
#include "gen/calib_struct_fcncall.hh"
#endif

#undef  FCNCALL_CLASS_NAME
#undef  FCNCALL_NAME
#undef  FCNCALL_CALL_BASE
#undef  FCNCALL_CALL
#undef  FCNCALL_CALL_TYPE
#undef  FCNCALL_FOR
#undef  FCNCALL_SUBINDEX
#undef  FCNCALL_SUBNAME
#undef  FCNCALL_MULTI_MEMBER
#undef  FCNCALL_MULTI_ARG
#undef  FCNCALL_UNIT
#undef  STRUCT_ONLY_LAST_UNION_MEMBER

#ifndef NAN
#warning NAN not defined, using hack...
// This NAN defintion comes from the GNU C library, bits/nan.h
#if __BYTE_ORDER == __BIG_ENDIAN
#define __nan_bytes { 0x7f, 0xc0, 0, 0 }
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __nan_bytes { 0, 0, 0xc0, 0x7f }
#endif

static union { unsigned char __c[4]; float __d; } __nan_union = { __nan_bytes };
# define NAN    (__nan_union.__d)

#endif// NAN


void __clean_uint8 (uint8  &item) { item = 0; }
void __clean_uint16(uint16 &item) { item = 0; }
void __clean_uint32(uint32 &item) { item = 0; }
void __clean_uint64(uint64 &item) { item = 0; }
void __clean_float (float  &item) { item = NAN; }
void __clean_double(double &item) { item = NAN; }


#define FCNCALL_CLASS_NAME(name) name
#define FCNCALL_NAME(name)  __clean()
#define FCNCALL_CALL_BASE() __clean()
#define FCNCALL_CALL(member) __clean()
#define FCNCALL_CALL_TYPE(type,member) ::__clean_##type(member);
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index)
#define FCNCALL_SUBNAME(name)
#define FCNCALL_MULTI_MEMBER(name) multi_##name
#define FCNCALL_MULTI_ARG(name) name
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/struct_fcncall.hh"
#include "gen/raw_struct_fcncall.hh"
#include "gen/cal_struct_fcncall.hh"
#ifdef USER_STRUCT
#include "gen/user_struct_fcncall.hh"
#endif
#ifdef CALIB_STRUCT
#include "gen/calib_struct_fcncall.hh"
#endif

#undef  FCNCALL_CLASS_NAME
#undef  FCNCALL_NAME
#undef  FCNCALL_CALL_BASE
#undef  FCNCALL_CALL
#undef  FCNCALL_CALL_TYPE
#undef  FCNCALL_FOR
#undef  FCNCALL_SUBINDEX
#undef  FCNCALL_SUBNAME
#undef  FCNCALL_MULTI_MEMBER
#undef  FCNCALL_MULTI_ARG
#undef  STRUCT_ONLY_LAST_UNION_MEMBER




void unpack_event_base::dump(const signal_id &id,pretty_dump_info &pdi) const
{

}

void unpack_subevent_base::dump(const signal_id &id,pretty_dump_info &pdi) const
{

}

void rawdata64::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[64];
  sprintf(buf,"0x%016llx=%lld",value,value);
  pretty_dump(id,buf,pdi);
}

void rawdata32::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"0x%08x=%d",value,value);
  pretty_dump(id,buf,pdi);
}

void rawdata24::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"0x%08x=%d",value,value);
  pretty_dump(id,buf,pdi);
}

void rawdata16::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"0x%04x=%d",value,value);
  pretty_dump(id,buf,pdi);
}

void rawdata16plus::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"0x%04x=%d%c%c%c%c",
          value,value,
	  underflow ? 'U' : ' ',
	  range ?     'R' : ' ',
	  pileup ?    'P' : ' ',
	  overflow ?  'O' : ' ');
  pretty_dump(id,buf,pdi);
}

void rawdata14::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"0x%04x=%d%c%c",
	  value,value,
	  range ?    'R' : ' ',
	  overflow ? 'O' : ' ');
  pretty_dump(id,buf,pdi);
}

void rawdata12::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"0x%03x=%d%c%c",
	  value,value,
	  range ?    'R' : ' ',
	  overflow ? 'O' : ' ');
  pretty_dump(id,buf,pdi);
}

void rawdata8::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"0x%02x=%d",value,value);
  pretty_dump(id,buf,pdi);
}

void dump_uint8 (const uint8  item,const signal_id &id,
		 pretty_dump_info &pdi) {
  char buf[32]; sprintf(buf,"0x%02x=%d",item,item); pretty_dump(id,buf,pdi);}
void dump_uint16(const uint16 item,const signal_id &id,
		 pretty_dump_info &pdi) {
  char buf[32]; sprintf(buf,"0x%04x=%d",item,item); pretty_dump(id,buf,pdi);}
void dump_uint32(const uint32 item,const signal_id &id,
		 pretty_dump_info &pdi) {
  char buf[32]; sprintf(buf,"0x%08x=%d",item,item); pretty_dump(id,buf,pdi);}
void dump_uint64(const uint64 item,const signal_id &id,
		 pretty_dump_info &pdi) {
  char buf[64]; sprintf(buf,"0x%016llx=%lld",item,item); pretty_dump(id,buf,pdi);}
void dump_float (const float  item,const signal_id &id,
		 pretty_dump_info &pdi) {
  if (!pdi._dump_nan && isnan(item)) return;
  char buf[32]; sprintf(buf,"%.7g",item);   pretty_dump(id,buf,pdi);}
void dump_double(const double item,const signal_id &id,
		 pretty_dump_info &pdi) {
#if GCC_VERSION == 40902
/* Workaround for buggy compiler */
/* #pragma message(stringify(GCC_VERSION)) */
  if (!pdi._dump_nan && isnan((float)(item))) return;
#else
  if (!pdi._dump_nan && isnan(item)) return;
#endif
  char buf[32]; sprintf(buf,"%.13g",item);   pretty_dump(id,buf,pdi);}


#define FCNCALL_CLASS_NAME(name) name
#define FCNCALL_NAME(name) dump(const signal_id &__id,pretty_dump_info &pdi) const
#define FCNCALL_CALL_BASE() dump(__id,pdi)
#define FCNCALL_CALL(member) dump(__id,pdi)
#define FCNCALL_CALL_TYPE(type,member) ::dump_##type(member,__id,pdi)
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,index);
#define FCNCALL_SUBNAME(name)   const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,name);
#define FCNCALL_MULTI_MEMBER(name) multi_##name
#define FCNCALL_MULTI_ARG(name) name
#define STRUCT_ONLY_LAST_UNION_MEMBER 0

#include "gen/struct_fcncall.hh"
#include "gen/raw_struct_fcncall.hh"
#include "gen/cal_struct_fcncall.hh"
#ifdef USER_STRUCT
#include "gen/user_struct_fcncall.hh"
#endif
#ifdef CALIB_STRUCT
#include "gen/calib_struct_fcncall.hh"
#endif

#undef  FCNCALL_CLASS_NAME
#undef  FCNCALL_NAME
#undef  FCNCALL_CALL_BASE
#undef  FCNCALL_CALL
#undef  FCNCALL_CALL_TYPE
#undef  FCNCALL_FOR
#undef  FCNCALL_SUBINDEX
#undef  FCNCALL_SUBNAME
#undef  FCNCALL_MULTI_MEMBER
#undef  FCNCALL_MULTI_ARG
#undef  STRUCT_ONLY_LAST_UNION_MEMBER
