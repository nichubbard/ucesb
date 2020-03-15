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

#include "struct_mapping.hh"

#include "event_base.hh"

#include "signal_id_map.hh"

#include <stddef.h>

/*
#define STRUCT_MIRROR_FCNS_DECL(name)
#define STRUCT_MIRROR_TYPE(type)         type##_map
#define STRUCT_MIRROR_NAME(name)         name
#define STRUCT_MIRROR_STRUCT(type)       STRUCT_MIRROR_TYPE(type)
#define STRUCT_MIRROR_BASE(type)         STRUCT_MIRROR_TYPE(type)
#define STRUCT_MIRROR_TEMPLATE_ARG(arg)  arg
#define STRUCT_MIRROR_TEMPLATE_ARG_N(arg,array)  arg array
#define STRUCT_ONLY_LAST_UNION_MEMBER    1

#include "gen/struct_mirror.hh"

#undef  STRUCT_MIRROR_FCNS_DECL
#undef  STRUCT_MIRROR_TYPE
#undef  STRUCT_MIRROR_NAME
#undef  STRUCT_MIRROR_STRUCT
#undef  STRUCT_MIRROR_BASE
#undef  STRUCT_MIRROR_TEMPLATE_ARG
#undef  STRUCT_MIRROR_TEMPLATE_ARG_N
#undef  STRUCT_ONLY_LAST_UNION_MEMBER
*/

unpack_event_map the_unpack_event_map;
#if THIS_SEEMS_UNUSED
raw_event_map the_raw_event_reverse_map;
#endif

unpack_sticky_event_map the_unpack_sticky_event_map;

template<typename T>
bool data_map<T>::set_dest(T *dest, int toggle_i)
{
  if (_dest)
    return false;

  // printf ("Set dest: %p\n",dest);

  const zero_suppress_info *info;

  info = get_ptr_zero_suppress_info(dest,NULL,false);

  // printf ("Got info: %p\n",info);

  if (info->_type == ZZP_INFO_CALL_LIST_INDEX ||
      info->_type == ZZP_INFO_CALL_LIST_II_INDEX)
    _dest = (T *) (((char*) _dest) - info->_list._dest_offset);

  _dest     = dest;
  _zzp_info = info;

  if (!_zzp_info->_toggle_max && toggle_i)
    ERROR("Trying to map non-toggle destination with toggle index.");
  if (_zzp_info->_toggle_max)
    {
      if (!toggle_i)
	{
	  // It is valid to map toggle items without a toggle source.
	  // We may have a toggling module for some of the channels,
	  // but not all.
	  // ERROR("Trying to map toggle destination without toggle index.");
	}
      else
	{
	  if (toggle_i > _zzp_info->_toggle_max)
	    ERROR("Trying to map toggle destination with "
		  "too large toggle index, (%d > %d). ",
		  toggle_i, _zzp_info->_toggle_max);
	  _toggle_i = toggle_i;
	}
    }

  return true;
}

template<typename T>
bool do_set_dest(void *void_src_map,
		 void *void_dest,
		 int toggle_i)
{
  data_map<T> *src_map = (data_map<T> *) void_src_map;
  T           *dest    = (T *) void_dest;

  return src_map->set_dest(dest, toggle_i);
}

//#define SIGNAL_MAPPING(type,name,src,dest) { the_unpack_event_map.src.set_dest(the_raw_event.dest.get_dest_info()); }
/*
#define ZERO_SUPPRESS_ITEM(dest_item,index) zero_suppress_item(&(_event._raw.dest_item),index)

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define SIGNAL_MAPPING(type,name,src,dest,__VA_ARGS__...) \
  { the_unpack_event_map.src.set_dest(&(_event._raw.dest),zero_suppress(__VA_ARGS__)); }
#else
#define SIGNAL_MAPPING(type,name,src,dest,...) \
  { the_unpack_event_map.src.set_dest(&(_event._raw.dest),zero_suppress(__VA_ARGS__)); }
#endif
*/

/*******************************************************************
 * Deprecated...
 * This is not used any longer, as we use the parsing of the source
 * file.  Compiling this function took ages!!!
 */

#if 0
#define SIGNAL_MAPPING(type,name,src,dest) \
  { the_unpack_event_map.src.set_dest(&(_event._raw.dest)); }

void setup_unpack_map()
{
#include "gen/data_mapping.hh"
}
#endif

template<typename T>
void map_members(const data_map<T> &map,const T &src MAP_MEMBERS_PARAM)
{
  /*
  printf("type: %d ("
	 //"call %p  item %p  index %d  "
	 "dest %p) 0x%x\n",
	 map._dest ? map._zzp_info->_type : 0,
	 //map._zzp_info->_call,
	 //map._zzp_info->_item,
	 //map._zzp_info->_index,
	 map._dest,
	 *((uint32*) &src));
  fflush(stdout);
  */
  
  if (map._dest)
    {
      T *dest = map._dest;

      //WARNING("%d",map._zzp_info._type);
      switch (map._zzp_info->_type)
	{
	case ZZP_INFO_NONE: // no zero supress item
	  // case ZZP_INFO_FIXED_LIST: // part of fixed list
	  break;
	case ZZP_INFO_CALL_ARRAY_INDEX:
	  (*map._zzp_info->_array._call)(map._zzp_info->_array._item,
					 map._zzp_info->_array._index);
	  break;
	case ZZP_INFO_CALL_ARRAY_MULTI_INDEX:
	  {
	    size_t offset =
	      (*map._zzp_info->_array._call_multi)(map._zzp_info->_array._item,
						   map._zzp_info->_array._index);
	    dest = (T *) (((char *) dest) + offset);
	    // printf ("%d - %d\n",zzp_info->_array._index,offset);
	    break;
	  }
	case ZZP_INFO_CALL_LIST_INDEX:
	  {
	    size_t offset =
	      (*map._zzp_info->_list._call)(map._zzp_info->_list._item,
					    map._zzp_info->_list._index);
	    dest = (T *) (((char *) dest) + offset);
	    // printf ("%d - %d\n",zzp_info->_array._index,offset);
	    break;
	  }
	case ZZP_INFO_CALL_ARRAY_LIST_II_INDEX:
	  {
	    size_t offset =
	      (*map._zzp_info->_array._call_multi)(map._zzp_info->_array._item,
						   map._zzp_info->_array._index);
	    dest = (T *) (((char *) dest) + offset);
	    goto call_list_ii_index;
	  }
	case ZZP_INFO_CALL_LIST_LIST_II_INDEX:
	  {
	    size_t offset =
	      (*map._zzp_info->_list._call)(map._zzp_info->_list._item,
					    map._zzp_info->_list._index);
	    dest = (T *) (((char *) dest) + offset);
	    goto call_list_ii_index;
	  }
	case ZZP_INFO_CALL_LIST_II_INDEX:
	call_list_ii_index:
	  {
	    size_t offset =
	      (*map._zzp_info->_list_ii._call_ii)(map._zzp_info->_list_ii._item);
	    dest = (T *) (((char *) dest) + offset);
	    // printf ("%d - %d\n",map._zzp_info->_array._index,offset);
	    break;
	  }
	default:
	  ERROR("Internal error in data mapping!");
	  break;
	}
      if (!map._toggle_i)
	*dest = src;
      else
	{
	  // We are an toggle item.

	  toggle_item<T> *toggle_dest =
	    (toggle_item<T> *) (((char *) dest) -
				offsetof(toggle_item<T>,_item));

	  // Copy the value to the dedicated slot for this toggle
	  toggle_dest->_toggle_v[map._toggle_i - 1] = src;

	  // And copy to the main slot if no value has been so far, or
	  // we are the lowest toggle
	  if (!toggle_dest->_toggle_i ||
	      map._toggle_i < toggle_dest->_toggle_i)
	    {
	      toggle_dest->_toggle_i = map._toggle_i;
	      toggle_dest->_item = src;
	    }
	}
    }
  //char buf[256];
  //id.format(buf,sizeof(buf));
  //printf ("%s\n",buf);
}





template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_array_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_array<Tsingle,T,n> &src MAP_MEMBERS_PARAM) const
{
  for (int i = 0; i < n; i++)
    {
      _items[i].map_members(src[i] MAP_MEMBERS_ARG);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_array_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_array_zero_suppress<Tsingle,T,n> &src MAP_MEMBERS_PARAM) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = src._valid.next(iter)) >= 0)
    {
      _items[i].map_members(src[i] MAP_MEMBERS_ARG);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
template<int max_entries>
void raw_array_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_array_multi_zero_suppress<Tsingle,T,n,max_entries> &src MAP_MEMBERS_PARAM) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = src._valid.next(iter)) >= 0)
    {
      for (uint j = 0; j < src._num_entries[i]; j++)
	_items[i].map_members(src._items[i][j] MAP_MEMBERS_ARG);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_array_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_list_zero_suppress<Tsingle,T,n> &src MAP_MEMBERS_PARAM) const
{
  for (uint32 i = 0; i < src._num_items; i++)
    {
      _items[src._items[i]._index].map_members(src._items[i]._item MAP_MEMBERS_ARG);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n,int n1>
void raw_array_1_map<Tsingle_map,Tsingle,T_map,T,n,n1>::map_members(const raw_array_zero_suppress_1<Tsingle,T,n,n1> &src MAP_MEMBERS_PARAM) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = src._valid.next(iter)) >= 0)
    {
      const raw_array_map<Tsingle_map,Tsingle,Tsingle_map,Tsingle,n1> &map_list = _items[i];
      //const raw_array_zero_suppress<Tsingle,T,n> &src_list = src[i];

      const raw_array_zero_suppress<Tsingle,T,n> &src2 = src;

      for (int i1 = 0; i1 < n1; i1++)
	map_list[i1].map_members(src2[i][i1] MAP_MEMBERS_ARG);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_list_ii_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_list_ii_zero_suppress<Tsingle,T,n> &src MAP_MEMBERS_PARAM) const
{
  for (uint32 i = 0; i < src._num_items; i++)
    {
      T_map::map_members(src._items[i] MAP_MEMBERS_ARG);
    }
}


template<typename T>
void data_map<T>::map_members(const T &src MAP_MEMBERS_PARAM) const
{
  ::map_members(*this,src MAP_MEMBERS_ARG);
}

template<typename T>
void data_map<T>::map_members(const toggle_item<T> &src MAP_MEMBERS_PARAM) const
{
  ::map_members(*this,src._item MAP_MEMBERS_ARG);
}

/*
template<typename T_src>
void map_members(const data_map<T_src> &map,const T_src &src MAP_MEMBERS_PARAM)
{
  map.map_members(src MAP_MEMBERS_ARG);
}
*/

#define FCNCALL_CLASS_NAME(name) name##_map
#define FCNCALL_NAME(name) map_members(const name &__src MAP_MEMBERS_PARAM) const
#define FCNCALL_CALL_BASE() map_members(__src MAP_MEMBERS_ARG)
#define FCNCALL_CALL(member) map_members(__src.member MAP_MEMBERS_ARG) // show_members(signal_id(id,#member))
#define FCNCALL_CALL_TYPE(type,member) ::map_members<type>(member,__src.member MAP_MEMBERS_ARG) // ::show_members<type>(signal_id(id,#member))
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) ;
#define FCNCALL_SUBNAME(name)   ;
#define FCNCALL_MULTI_MEMBER(name) /*__src.name =*/ name
#define FCNCALL_MULTI_ARG(name) multi_##name,&__src.name
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/struct_fcncall.hh"
#if THIS_SEEMS_UNUSED
#include "gen/raw_struct_fcncall.hh"
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
#undef STRUCT_ONLY_LAST_UNION_MEMBER




#ifndef USE_MERGING
void do_unpack_map(unpack_event *unpack_ev
		   MAP_MEMBERS_PARAM)
{
  //_static_event._unpack.map_members(the_unpack_event_map MAP_MEMBERS_ARG);
  the_unpack_event_map.map_members(*unpack_ev /* _static_event._unpack */
				   MAP_MEMBERS_ARG);
}

void do_unpack_map(unpack_sticky_event *unpack_ev
		   MAP_MEMBERS_PARAM)
{
  the_unpack_sticky_event_map.map_members(*unpack_ev
					  MAP_MEMBERS_ARG);
}
#endif

#ifndef USE_MERGING
void do_raw_reverse_map(raw_event *raw_ev
			MAP_MEMBERS_PARAM)
{
  //_static_event._unpack.map_members(the_unpack_event_map MAP_MEMBERS_ARG);
#if THIS_SEEMS_UNUSED
  the_raw_event_reverse_map.map_members(*raw_ev /* _static_event._raw */
					MAP_MEMBERS_ARG);
#endif
}

void do_raw_reverse_map(raw_sticky *raw_ev
			MAP_MEMBERS_PARAM)
{
}
#endif









#define FCNCALL_CLASS_NAME(name) name##_map
#define FCNCALL_NAME(name) \
  enumerate_map_members(const signal_id &__id,const enumerate_info &__info,enumerate_fcn __callback,void *__extra) const
#define FCNCALL_CALL_BASE() enumerate_map_members(__id,__info,__callback,__extra)
#define FCNCALL_CALL(member) enumerate_map_members(__id,__info,__callback,__extra) // enumerate_members(signal_id(id,#member))
#define FCNCALL_CALL_TYPE(type,member) member.enumerate_map_members(__id,__info,__callback,__extra) // ::enumerate_members<type>(signal_id(id,#member))
//#define FCNCALL_CALL_TYPE(type,member) ::enumerate_map_members_##type(&member,__id,__info,__callback,__extra) // ::enumerate_members<type>(signal_id(id,#member))
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,index);
#define FCNCALL_SUBNAME(name)   const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,name);
#define FCNCALL_MULTI_MEMBER(name) name
#define FCNCALL_MULTI_ARG(name) name
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/struct_fcncall.hh"
#if THIS_SEEMS_UNUSED
#include "gen/raw_struct_fcncall.hh"
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


void enumerate_member_signal_id_map_unpack(const signal_id &id,
					   const enumerate_info &info,
					   void *extra)
{
  enumerate_msid_info *extra_info = (enumerate_msid_info *) extra;

  signal_id_info *sid_info;

  sid_info = find_signal_id_info(id,extra_info);

  assert (sid_info);

  sid_info->_addr = info._addr;
  sid_info->_type = info._type;

  sid_info->_set_dest = info._set_dest;

  // leaf->_info->_addr = info._addr;
}

void setup_signal_id_map_unpack_map(void *extra)
{
  the_unpack_event_map.
    enumerate_map_members(signal_id(),enumerate_info(),
			  enumerate_member_signal_id_map_unpack,extra);
}

void setup_signal_id_map_unpack_sticky_map(void *extra)
{
  the_unpack_sticky_event_map.
    enumerate_map_members(signal_id(),enumerate_info(),
			  enumerate_member_signal_id_map_unpack,extra);
}

void setup_signal_id_map_raw_reverse_map(void *extra)
{
#if THIS_SEEMS_UNUSED
  the_raw_event_reverse_map.
    enumerate_map_members(signal_id(),enumerate_info(),
			  enumerate_member_signal_id_map_unpack,extra);
#endif
}

void setup_signal_id_map_raw_sticky_reverse_map(void *extra)
{

}
