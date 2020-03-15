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

#include "struct_calib.hh"

#include "event_base.hh"

#include "signal_id_map.hh"

#include "calib_info.hh"

#include "mc_def.hh"

raw_event_calib_map the_raw_event_calib_map;

template<typename T,int n_toggle>
void calib_map_base<T,n_toggle>::show(const signal_id &id)
{
  for (int i = 0; i < n_toggle; i++)
    {
      if (_calib[i])
	{
	  char buf[256];
	  id.format(buf,sizeof(buf));
	  if (n_toggle == 1 ||
	      (i == 0 && _calib[0] == _calib[n_toggle-1]))
	    printf ("%s : ",buf);
	  else
	    printf ("%s tgl %d : ",buf,i+1);
	  _calib[i]->show();
	  printf ("\n");
	}
      else
	{
	  /*
	    char buf[256];
	    id.format(buf,sizeof(buf));
	    printf ("%s : --\n",buf);
	    fflush(stdout);
	  */
	}
      if (n_toggle != 1 &&
	  (i == 0 && _calib[0] == _calib[n_toggle-1]))
	break;
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_array_calib_map<Tsingle_map,Tsingle,T_map,T,n>::show(const signal_id &id)
{
  for (int i = 0; i < n; i++)
    _items[i].show(signal_id(id,i));
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n,int n1>
void raw_array_1_calib_map<Tsingle_map,Tsingle,T_map,T,n,n1>::show(const signal_id &id)
{
  for (int i = 0; i < n; i++)
    for (int i1 = 0; i1 < n1; i1++)
      _items[i][i1].show(signal_id(signal_id(id,i),i1));
}

#define FCNCALL_CLASS_NAME(name) name##_calib_map
#define FCNCALL_NAME(name) show(const signal_id &__id)
#define FCNCALL_CALL_BASE() show(__id)
#define FCNCALL_CALL(member) show(__id)
#define FCNCALL_CALL_TYPE(type,member) member.show(__id) // ::show<type>(member,id,info)
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,index);
#define FCNCALL_SUBNAME(name)   const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,name);
#define FCNCALL_MULTI_MEMBER(name)
#define FCNCALL_MULTI_ARG(name)
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/raw_struct_fcncall.hh"

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

void show_calib_map()
{
  the_raw_event_calib_map.show(signal_id());
}


mix_rnd_seed::mix_rnd_seed(uint64 A,uint64 B)
{
  _state_A = A;
  _state_B = B;
}

#define LCG_A1 3935559000370003845LL
#define LCG_C1 2691343689449505681LL
#define LCG_A2 3202034522624059733LL
#define LCG_C2 4354685564936845319LL
#define LCG_A3 2862933555777941757LL
#define LCG_C3          3037000493LL

#define XOR_SHIFT(x) x^=(x<<13); x^=(x>>7); x^=(x<<17);

/*
mix_rnd_seed::mix_rnd_seed(const mix_rnd_seed &src)
{
  _state_A = src._state_A;
  _state_B = src._state_B;
}
*/


mix_rnd_seed::mix_rnd_seed(const mix_rnd_seed &src,uint32 index)
{
  // printf ("index: %016llx %016llx (+) %d",src._state_A,src._state_B,index);

  _state_A = src._state_A + index + 0x7654321000000000LL;
  _state_A = _state_A * LCG_A1 + LCG_C1;

  _state_B = src._state_B - (((uint64) index) << 32) + 0x00000000fedcba98LL;
  XOR_SHIFT(_state_B);

  // printf (" -> %016llx %016llx\n",_state_A,_state_B);
}

mix_rnd_seed::mix_rnd_seed(const mix_rnd_seed &src,const char *name)
{
  // printf ("name: %016llx %016llx (+) %s",src._state_A,src._state_B,name);

  // also an empty string should cause an update!  (for base class)
  // using other constants to differentiate from the index

  _state_A = src._state_A + 0xf7e6d5c400000000LL;
  _state_A = _state_A * LCG_A2 + LCG_C2;

  _state_B = src._state_B - 0xb3a2918000000000LL;
  XOR_SHIFT(_state_B);

  for ( ; *name; name++)
    {
      _state_A = _state_A + ((uint8) *name);
      _state_A = _state_A * LCG_A3 + LCG_C3;

      _state_B = _state_B - (((uint64) ((uint8) *name)) << 32);
      XOR_SHIFT(_state_B);
    }
  // printf (" -> %016llx %016llx\n",_state_A,_state_B);
}


uint64 mix_rnd_seed::get() const
{
  // printf ("get: %016llx %016llx\n",_state_A,_state_B);
  return _state_A ^ _state_B;
}


template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_array_calib_map<Tsingle_map,Tsingle,T_map,T,n>::set_rnd_seed(const mix_rnd_seed &rnd_seed)
{
  for (int i = 0; i < n; i++)
    _items[i].set_rnd_seed(mix_rnd_seed(rnd_seed,i));
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n,int n1>
void raw_array_1_calib_map<Tsingle_map,Tsingle,T_map,T,n,n1>::set_rnd_seed(const mix_rnd_seed &rnd_seed)
{
  for (int i = 0; i < n; ++i)
    {
      mix_rnd_seed rnd_seed_i(rnd_seed,(uint32) i);
      for (int i1 = 0; i1 < n1; ++i1)
	_items[i][i1].set_rnd_seed(mix_rnd_seed(rnd_seed_i,(uint32) i1));
    }
}

#define FCNCALL_CLASS_NAME(name) name##_calib_map
#define FCNCALL_NAME(name) set_rnd_seed(const mix_rnd_seed &rnd_seed)
#define FCNCALL_CALL_BASE() set_rnd_seed(mix_rnd_seed(rnd_seed,""))
#define FCNCALL_CALL(member) set_rnd_seed(rnd_seed)
#define FCNCALL_CALL_TYPE(type,member) member.set_rnd_seed(rnd_seed)
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) const mix_rnd_seed &__shadow_rnd_seed = rnd_seed; mix_rnd_seed rnd_seed(__shadow_rnd_seed,index);
#define FCNCALL_SUBNAME(name)   const mix_rnd_seed &__shadow_rnd_seed = rnd_seed; mix_rnd_seed rnd_seed(__shadow_rnd_seed,name);
#define FCNCALL_MULTI_MEMBER(name)
#define FCNCALL_MULTI_ARG(name)
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/raw_struct_fcncall.hh"

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

void set_rnd_seed_calib_map()
{
  mix_rnd_seed rnd_seed(0x0123456789abcdefLL,0xf0e1d2c3b4a59687LL);

  the_raw_event_calib_map.set_rnd_seed(mix_rnd_seed(rnd_seed,"RAW_CALIB"));
}





template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_array_calib_map<Tsingle_map,Tsingle,T_map,T,n>::clear()
{
  for (int i = 0; i < n; i++)
    _items[i].clear();
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n,int n1>
void raw_array_1_calib_map<Tsingle_map,Tsingle,T_map,T,n,n1>::clear()
{
  for (int i = 0; i < n; ++i)
    for (int i1 = 0; i1 < n1; ++i1)
      _items[i][i1].clear();
}

#define FCNCALL_CLASS_NAME(name) name##_calib_map
#define FCNCALL_NAME(name) clear()
#define FCNCALL_CALL_BASE() clear()
#define FCNCALL_CALL(member) clear()
#define FCNCALL_CALL_TYPE(type,member) member.clear()  // ::show<type>(member,id,info)
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) ;
#define FCNCALL_SUBNAME(name)   ;
#define FCNCALL_MULTI_MEMBER(name)
#define FCNCALL_MULTI_ARG(name)
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/raw_struct_fcncall.hh"

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

void clear_calib_map()
{
  the_raw_event_calib_map.clear();
}





template<typename T>
void calib_map<T>::map_members(const T &src) const
{
  raw_to_tcal_base *calib = calib_map_base<T,1>::get_calib(0);
  
  //printf("%f...\n",(double) src.value);
  if (calib)
    {
      calib->_convert(calib,&src);
    }
}

template<typename T>
void toggle_calib_map<T>::map_members(const toggle_item<T> &src) const
{
  int toggle_i = 0;

  if (src._toggle_i)
    toggle_i = src._toggle_i - 1;

  raw_to_tcal_base *calib = calib_map_base<T,2>::get_calib(toggle_i);
    
  //printf("%f...\n",(double) src.value);
  if (calib)
    {
      calib->_convert(calib,&src._item);
    }
}



template<typename T_src>
void map_members(const calib_map<T_src> &map,const T_src &src)
{
  map.map_members(src);
  /*
  if (map._calib)
    {
      map._calib->_convert(map._calib,&src);
    }
  */
}

/*
void rawdata12::map_members(const calib_map<rawdata12> &map) const { ::map_members(*this,map); }
void rawdata24::map_members(const calib_map<rawdata24> &map) const { ::map_members(*this,map); }
void rawdata32::map_members(const calib_map<rawdata32> &map) const { ::map_members(*this,map); }
*/

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_array_calib_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_array<Tsingle,T,n> &src) const
{
  for (int i = 0; i < n; i++)
    {
      _items[i].map_members(src[i]);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_array_calib_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_array_zero_suppress<Tsingle,T,n> &src) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = src._valid.next(iter)) >= 0)
    {
      _items[i].map_members(src[i]);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
template<int max_entries>
void raw_array_calib_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_array_multi_zero_suppress<Tsingle,T,n,max_entries> &src) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = src._valid.next(iter)) >= 0)
    {
      for (uint j = 0; j < src._num_entries[i]; j++)
	_items[i].map_members(src._items[i][j]);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n,int n1>
void raw_array_1_calib_map<Tsingle_map,Tsingle,T_map,T,n,n1>::map_members(const raw_array_zero_suppress_1<Tsingle,T,n,n1> &src) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = src._valid.next(iter)) >= 0)
    {
      const raw_array_calib_map<Tsingle_map,Tsingle,Tsingle_map,Tsingle,n1> &map_list = _items[i];
      //const raw_array_zero_suppress<Tsingle,T,n> &src_list = src[i];

      const raw_array_zero_suppress<Tsingle,T,n> &src2 = src;

      for (int i1 = 0; i1 < n1; i1++)
	map_list[i1].map_members(src2[i][i1]);
    }
}

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
void raw_list_ii_calib_map<Tsingle_map,Tsingle,T_map,T,n>::map_members(const raw_list_ii_zero_suppress<Tsingle,T,n> &src) const
{
  for (uint32 i = 0; i < src._num_items; i++)
    {
      T_map::map_members(src._items[i]);
    }
}

/*
template<typename Tsingle,typename T,int n>
void raw_array<Tsingle,T,n>::map_members(const raw_array_calib_map<Tsingle,T,n> &map) const
{
  for (int i = 0; i < n; i++)
    {
      _items[i].map_members(map[i]);
    }
}

template<typename Tsingle,typename T,int n>
void raw_array_zero_suppress<Tsingle,T,n>::map_members(const raw_array_calib_map<Tsingle,T,n> &map) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = _valid.next(iter)) >= 0)
    {
      _items[i].map_members(map[i]);
    }
}

template<typename Tsingle,typename T,int n,int n1>
void raw_array_zero_suppress_1<Tsingle,T,n,n1>::map_members(const raw_array_1_calib_map<Tsingle,T,n,n1> &map) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = _valid.next(iter)) >= 0)
    {
      (raw_array_zero_suppress<Tsingle,T,n>::_items[i])[0].map_members(map[i][0]);
    }
}

template<typename Tsingle,typename T,int n>
void raw_list_zero_suppress<Tsingle,T,n>::map_members(const raw_array_calib_map<Tsingle,T,n> &map) const
{
  for (int i = 0; i < _num_items; i++)
    {
      _items[i]._item.map_members(map[_items[i]._index]);
    }
}
*/

#define FCNCALL_CLASS_NAME(name) name##_calib_map
#define FCNCALL_NAME(name) map_members(const name &__src) const
#define FCNCALL_CALL_BASE() map_members(__src)
#define FCNCALL_CALL(member) map_members(__src.member) // show_members(signal_id(id,#member))
#define FCNCALL_CALL_TYPE(type,member) ::map_members<type>(member,__src.member) // ::show_members<type>(signal_id(id,#member))
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) ;
#define FCNCALL_SUBNAME(name)   ;
#define FCNCALL_MULTI_MEMBER(name) // name = multi_##name
#define FCNCALL_MULTI_ARG(name) // name
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/raw_struct_fcncall.hh"

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

void do_calib_map(raw_event *raw_ev)
{
#ifndef USE_MERGING
  the_raw_event_calib_map.map_members(*raw_ev /* _static_event._raw */);
#endif
}

void do_calib_map(raw_sticky *raw_ev)
{
}





#define FCNCALL_CLASS_NAME(name) name##_calib_map
#define FCNCALL_NAME(name) \
  enumerate_map_members(const signal_id &__id,const enumerate_info &__info,enumerate_fcn __callback,void *__extra) const
#define FCNCALL_CALL_BASE() enumerate_map_members(__id,__info,__callback,__extra)
#define FCNCALL_CALL(member) enumerate_map_members(__id,__info,__callback,__extra) // enumerate_members(signal_id(id,#member))
#define FCNCALL_CALL_TYPE(type,member) member.enumerate_map_members(__id,__info,__callback,__extra) // ::enumerate_members<type>(signal_id(id,#member))
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,index);
#define FCNCALL_SUBNAME(name)   const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,name);
#define FCNCALL_MULTI_MEMBER(name) name
#define FCNCALL_MULTI_ARG(name) name
#define FCNCALL_UNIT(unit) \
  if (__info._unit)                                         \
    ERROR("%s:%d: Cannot have several unit specifications! (should only occur at leaf)",__FILE__,__LINE__); \
  const enumerate_info &__shadow_info = __info;             \
  enumerate_info __info = __shadow_info;                    \
  __info._unit = insert_prefix_units_exp(file_line(),unit);
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/raw_struct_fcncall.hh"

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


// TODO: exactly like enumerate_member_signal_id_map_unpack?!? (merge?)
void enumerate_member_signal_id_map_raw(const signal_id &id,
					const enumerate_info &info,
					void *extra)
{
  enumerate_msid_info *extra_info = (enumerate_msid_info *) extra;

  signal_id_info *sid_info;

  sid_info = find_signal_id_info(id,extra_info);

  assert (sid_info);

  sid_info->_addr = info._addr;
  sid_info->_type = info._type;
  sid_info->_unit = info._unit;

  sid_info->_set_dest = info._set_dest;

  // leaf->_info->_addr = info._addr;
}

void setup_signal_id_map_raw_map(void *extra)
{
  the_raw_event_calib_map.enumerate_map_members(signal_id(),enumerate_info(),
						enumerate_member_signal_id_map_raw,extra);
}


#define CHECK_NUM_PARAMS(expect) \
  if (param->_param->size() != (expect)) \
    ERROR_LOC(param->_loc,"Wrong number of parameters (expect %d).",(expect));

template<typename T_src,typename T_dest>
raw_to_tcal_base *new_raw_to_tcal(const calib_param *param)
{
  if (param->_type == CALIB_TYPE_SLOPE_OFFSET)
    {
      CHECK_NUM_PARAMS(2);

      double factor_slope;
      double factor_offset;

      verify_units_match(divide_units(param->_dest->_unit,param->_src->_unit),
			 param->_param[0][0]._unit,
			 param->_loc,"slope",factor_slope);

      verify_units_match(param->_dest->_unit,
			 param->_param[0][1]._unit,
			 param->_loc,"offset",factor_offset);

      return new_slope_offset((const T_src *) NULL,
			      (T_dest *) (void*) param->_dest->_addr,
			      param->_param[0][0]._value * factor_slope,  // first [0] is actually just dereferencing a pointer!!!
			      param->_param[0][1]._value * factor_offset);
    }
  if (param->_type == CALIB_TYPE_OFFSET_SLOPE)
    {
      CHECK_NUM_PARAMS(2);

      double factor_offset;
      double factor_slope;

      verify_units_match(param->_src->_unit,
			 param->_param[0][0]._unit,
			 param->_loc,"offset",factor_offset);

      verify_units_match(divide_units(param->_dest->_unit,param->_src->_unit),
			 param->_param[0][1]._unit,
			 param->_loc,"slope",factor_slope);

      return new_offset_slope((const T_src *) NULL,
			      (T_dest *) (void*) param->_dest->_addr,
			      param->_param[0][0]._value * factor_offset,
			      param->_param[0][1]._value * factor_slope);
    }
  if (param->_type == CALIB_TYPE_SLOPE)
    {
      CHECK_NUM_PARAMS(1);
      return new_slope((const T_src *) NULL,
		       (T_dest *) (void*) param->_dest->_addr,
		       param->_param[0][0]._value);
    }
  if (param->_type == CALIB_TYPE_OFFSET)
    {
      CHECK_NUM_PARAMS(1);
      return new_offset((const T_src *) NULL,
			(T_dest *) (void*) param->_dest->_addr,
			param->_param[0][0]._value);
    }
  if (param->_type == CALIB_TYPE_CUT_BELOW_OR_EQUAL)
    {
      CHECK_NUM_PARAMS(1);
      return new_cut_below_oe((const T_src *) NULL,
			      (T_dest *) (void*) param->_dest->_addr,
			      param->_param[0][0]._value);
    }

  ERROR_LOC(param->_loc,"Internal error: unhandled parameter type: %d",
	    param->_type);

  return NULL;
}

template<typename T, int n_toggle>
bool set_raw_to_tcal(void *info,
		     void *dummy,
		     int toggle_i)
{
  // We know the source type (via T)

  // We need to create casts for the destination type
  // We need to select on the

  const calib_param *param = (const calib_param *) info;

  calib_map_base<T,n_toggle>* src =
    (calib_map_base<T,n_toggle>*) (void*) param->_src->_addr;

  if (src->get_n_toggle() == 1 && toggle_i)
    {
      WARNING("Trying to map toggle calibration for non-toggle source.");
      return false;
    }

  /* For non-toggle items, use0 and use1 will be the same (0).
   * For toggle items, they will by defult be different, and thus
   * check and (at bottom) assign both slots.
   * Unless toggle is given, in which case they only deal with the
   * wanted item.
   */

  int use0 = 0;
  int use1 = n_toggle-1;

  if (toggle_i == 1)
    use1 = use0;
  else if (toggle_i == 2)
    use0 = use1;

  if (src->_calib[use0] ||
      src->_calib[use1])
    return false;   // one could try to merge!!!

  // info->_src   _addr  _type
  // info->_dest  _addr  _type
  // info->_type                CALIB_TYPE_xxx
  // info->_param               vect_double *

  raw_to_tcal_base *r2c = NULL;

#define SET_RAW_TO_TCAL2(src_type_name,src_type,dest_type_name,dest_type) \
  if (param->_src->_type  == ENUM_TYPE_##src_type_name &&                 \
      param->_dest->_type == ENUM_TYPE_##dest_type_name) {                \
      r2c = new_raw_to_tcal<src_type,dest_type>(param);                   \
    }

  SET_RAW_TO_TCAL2(DATA12,rawdata12,FLOAT,float);
  SET_RAW_TO_TCAL2(DATA12,rawdata12,DOUBLE,double);
  SET_RAW_TO_TCAL2(DATA14,rawdata14,FLOAT,float);
  SET_RAW_TO_TCAL2(DATA14,rawdata14,DOUBLE,double);
  SET_RAW_TO_TCAL2(DATA16,rawdata16,FLOAT,float);
  SET_RAW_TO_TCAL2(DATA16,rawdata16,DOUBLE,double);
  SET_RAW_TO_TCAL2(DATA24,rawdata24,FLOAT,float);
  SET_RAW_TO_TCAL2(DATA24,rawdata24,DOUBLE,double);
  SET_RAW_TO_TCAL2(DATA32,rawdata32,FLOAT,float);
  SET_RAW_TO_TCAL2(DATA32,rawdata32,DOUBLE,double);
  SET_RAW_TO_TCAL2(DATA64,rawdata64,FLOAT,float);
  SET_RAW_TO_TCAL2(DATA64,rawdata64,DOUBLE,double);

  if (!r2c)
    {
      // Hmm, as we knew the src type, it would have been enough to switch
      // on the destination type, but this way, we get to decide what
      // pairs can be handled (as if it seems to matter...)

      char src_type[64];
      char dest_type[64];

      get_enum_type_name(param->_src->_type,src_type,sizeof(src_type));
      get_enum_type_name(param->_dest->_type,dest_type,sizeof(dest_type));

      ERROR_LOC(param->_loc,"Unhandled type (pair of?) for calibration parameters (src and/or dest) (%s -> %s) (0x%x -> 0x%x).",
		src_type,dest_type,
		param->_src->_type,
		param->_dest->_type);
      return false;
    }

  src->_calib[use0] = r2c;
  src->_calib[use1] = r2c;

  return true;
}
