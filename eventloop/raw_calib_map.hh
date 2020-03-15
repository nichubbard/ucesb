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

#ifndef __RAW_CALIB_MAP_HH__
#define __RAW_CALIB_MAP_HH__

//#include "zero_suppress_map.hh"

#include "raw_to_cal.hh"

#include "enumerate.hh"

template<typename T,int n_toggle>
bool set_raw_to_tcal(void *info,
		     void *dummy,
		     int toggle_i_dummy);

struct mix_rnd_seed
{
  // The function to just make a copy of the seed does not
  // exist.  To prevent accidental non-updating calls.
  // (we want unique seeds for all members)
public:
  // Due to some compile bug before and at (least) 4.1, private
  // does not work.  Only use it for known ok compiler.
#ifdef __GNUC_PREREQ
#if __GNUC_PREREQ (4, 3)
 private:
#endif
#endif
  mix_rnd_seed(const mix_rnd_seed &src) { assert(false); }

 public:
  mix_rnd_seed(uint64 A,uint64 B);
  // mix_rnd_seed(const mix_rnd_seed &src);
  mix_rnd_seed(const mix_rnd_seed &src,uint32 index);
  mix_rnd_seed(const mix_rnd_seed &src,const char *name);

 public:
  uint64 _state_A, _state_B;

 public:
  uint64 get() const;
};

template<typename T,int n_toggle>
class calib_map_base
{
public:
  calib_map_base()
  {
    // src  = NULL;
    // _dest = NULL;

    for (int i = 0; i < n_toggle; i++)
      _calib[i] = NULL;
    // _dest  = NULL;
    // _zzp_info = NULL;

    // fprintf (stderr,"%p:calib_map::calib_map()\n",this);

    _rnd_seed = 0;
  }

public:
  typedef T  src_t;
  typedef T* src_ptr_t;

public:
  // r2c_union<T>              _calib;
  raw_to_tcal_base            *_calib[n_toggle];
  // void                     *_dest;
  // const zero_suppress_info *_zzp_info;
  /*
public:
  void set_dest(T *dest,const zero_suppress_info &zzp_info)
  {
    printf ("Set dest: %p\n",dest);

    _dest     = dest;
    _zzp_info = zzp_info;
  }
  */

  uint64                       _rnd_seed;

  //public:
  //template<typename T_dest>
  //void set_dest(T_dest *dest);

  void enumerate_map_members(const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,this,get_enum_type((T *) NULL)).
	     set_dest_function(set_raw_to_tcal<T,n_toggle>),extra);
  }

public:
  raw_to_tcal_base *get_calib(int i) const { return _calib[i]; }
  int get_n_toggle() const { return n_toggle; }

public:
  void show(const signal_id &id);

public:
  void set_rnd_seed(const mix_rnd_seed &rnd_seed)
  {
    _rnd_seed = rnd_seed.get();
  }

public:
  void clear()
  {
    // fprintf (stderr,"%p:calib_map::clear() , _calib=%p\n",this,_calib);
    for (int i = 0; i < n_toggle; i++)
      {
	delete _calib[i];
	_calib[i] = NULL;
      }
  }
};

template<typename T>
class calib_map :
  public calib_map_base<T,1>
{
  /*
public:
  calib_map() : calib_map_base<T,1>()
  {

  }
  */
  
public:
  void map_members(const T &src) const;
};

template<typename T>
class toggle_calib_map :
  public calib_map_base<T,2>
{


public:
  void map_members(const toggle_item<T> &src) const;
};

#define DECL_PRIMITIVE_TYPE(type)			\
  typedef calib_map<type>  type##_calib_map;		\
  typedef toggle_calib_map<type>  toggle_##type##_calib_map;

#include "decl_primitive_types.hh"

#undef DECL_PRIMITIVE_TYPE

// TODO: Make sure that the user cannot specify source array indices
// in SIGNAL which are outside the available items.  Bad names get
// caught by the compiler, array indices not.

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
class raw_array_calib_map
{
public:
  T_map _items[n];

public:
  T_map &operator[](size_t i)
  {
    // This function is used by the setting up of the arrays, i.e. we
    // can have checks here
    if (i >= n)
      ERROR("Mapping index outside bounds (%d >= %d)",(int) i,n);
    return _items[i];
  }
  const T_map &operator[](size_t i) const
  {
    // This function is used by the mapping operations (since that one
    // needs a const function), no checks here (expensive, since
    // called often)
    return _items[i];
  }

public:
  void show(const signal_id &id);
  void set_rnd_seed(const mix_rnd_seed &rnd_seed);
  void clear();

public:
  void map_members(const raw_array<Tsingle,T,n> &map) const;
  void map_members(const raw_array_zero_suppress<Tsingle,T,n> &map) const;
  template<int max_entries>
  void map_members(const raw_array_multi_zero_suppress<Tsingle,T,n,max_entries> &map) const;


public:
  void enumerate_map_members(const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra) const
  {
    for (int i = 0; i < n; ++i)
      _items[i].enumerate_map_members(signal_id(id,i),info,callback,extra);
  }
};

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n,int n1>
class raw_array_1_calib_map
{
public:
  raw_array_calib_map<Tsingle_map,Tsingle,Tsingle_map,Tsingle,n1> _items[n];

public:
  raw_array_calib_map<Tsingle_map,Tsingle,Tsingle_map,Tsingle,n1> &operator[](size_t i)
  {
    // This function is used by the setting up of the arrays, i.e. we
    // can have checks here
    if (i >= n)
      ERROR("Mapping index outside bounds (%d >= %d)",(int) i,n);
    return _items[i];
  }
  const raw_array_calib_map<Tsingle_map,Tsingle,Tsingle_map,Tsingle,n1> &operator[](size_t i) const
  {
    // This function is used by the mapping operations (since that one
    // needs a const function), no checks here (expensive, since
    // called often)
    return _items[i];
  }

public:
  void show(const signal_id &id);
  void set_rnd_seed(const mix_rnd_seed &rnd_seed);
  void clear();

public:
  void map_members(const raw_array_zero_suppress_1<Tsingle,T,n,n1> &map) const;

public:
  void enumerate_map_members(const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra) const
  {
    for (int i = 0; i < n; ++i)
      for (int i1 = 0; i1 < n1; ++i1)
	_items[i][i1].enumerate_map_members(signal_id(signal_id(id,i),i1),info,callback,extra);
  }
};

// For the mirror data structures, which will use the
// raw_array_zero_suppress_map name
#define raw_array_zero_suppress_calib_map raw_array_calib_map
#define raw_array_zero_suppress_1_calib_map raw_array_1_calib_map

#define raw_list_zero_suppress_calib_map raw_array_calib_map
#define raw_list_zero_suppress_1_calib_map raw_array_1_calib_map

// no-index list

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n>
class raw_list_ii_calib_map
  : public T_map
{
public:
  void map_members(const raw_list_ii_zero_suppress<Tsingle,T,n> &src) const;
};

#define raw_list_ii_zero_suppress_calib_map raw_list_ii_calib_map

// For the multi-entry array (which has two 'indices', one index, and
// one max-number-of-entries), we only need to have a map with one index

template<typename Tsingle_map,typename Tsingle,typename T_map,typename T,int n,int max_entries>
class raw_array_multi_calib_map
  : public raw_array_calib_map<Tsingle_map,Tsingle,T_map,T,n>
{
};

#define raw_array_multi_zero_suppress_calib_map raw_array_multi_calib_map

class unpack_subevent_base_calib_map
{
public:
  void enumerate_map_members(const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra) const
  {
  }

};

class unpack_event_base_calib_map
{
public:
  void enumerate_map_members(const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra) const
  {
  }

};

class raw_event_base_calib_map
{
public:
  void show(const signal_id &id) { }
  void set_rnd_seed(const mix_rnd_seed &rnd_seed) { }
  void clear() { }
  void map_members(const raw_event_base &src) const { }

  void enumerate_map_members(const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra) const
  {
  }

};

class raw_sticky_base_calib_map :
  public raw_event_base_calib_map
{
  
};


/*
class cal_event_base_map
{
public:
  void enumerate_map_members(const signal_id &id,
			     const enumerate_info &info,
			     enumerate_fcn callback,void *extra) const
  {
  }

};
*/
#endif//__RAW_CALIB_MAP_HH__

