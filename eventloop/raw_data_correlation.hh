/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#ifndef __RAW_DATA_CORRELATION_HH__
#define __RAW_DATA_CORRELATION_HH__

#include "typedef.hh"

#include "detector_requests.hh"

struct enumerate_correlations_info
{
  int                _next_index;
  detector_requests *_requests;
};

struct correlation_list;

template<typename T>
class data_correlation
{
public:
  data_correlation()
  {
    _index = -1;
  }

public:
  int _index;

public:
  // void set_dest(T *dest);

public:
  void add_corr_members(const T &src,correlation_list *list WATCH_MEMBERS_PARAM) const;
  void add_corr_members(const toggle_item<T> &src,correlation_list *list WATCH_MEMBERS_PARAM) const;
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info);
};

#define DECL_PRIMITIVE_TYPE(type)			\
  typedef data_correlation<type>  type##_correlation;

#include "decl_primitive_types.hh"

#undef DECL_PRIMITIVE_TYPE

// TODO: Make sure that the user cannot specify source array indices
// in SIGNAL which are outside the available items.  Bad names get
// caught by the compiler, array indices not.

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n>
class raw_array_correlation
{
public:
  T_correlation _items[n];

public:
  T_correlation &operator[](size_t i)
  {
    // This function is used by the setting up of the arrays, i.e. we
    // can have checks here
    if (i < 0 || i >= n)
      ERROR("Correlation index outside bounds (%d >= %d)",i,n);
    return _items[i];
  }
  const T_correlation &operator[](size_t i) const
  {
    // This function is used by the mapping operations (since that one
    // needs a const function), no checks here (expensive, since
    // called often)
    return _items[i];
  }

public:
  static void add_corr_item(const T &src,const T_correlation &watch,correlation_list *list WATCH_MEMBERS_PARAM);

public:
  void add_corr_members(const raw_array<Tsingle,T,n> &src,correlation_list *list WATCH_MEMBERS_PARAM) const;
  void add_corr_members(const raw_array_zero_suppress<Tsingle,T,n> &src,correlation_list *list WATCH_MEMBERS_PARAM) const;
  template<int max_entries>
  void add_corr_members(const raw_array_multi_zero_suppress<Tsingle,T,n,max_entries> &src,correlation_list *list WATCH_MEMBERS_PARAM) const;
  void add_corr_members(const raw_list_zero_suppress<Tsingle,T,n> &src,correlation_list *list WATCH_MEMBERS_PARAM) const;
  void add_corr_members(const raw_list_ii_zero_suppress<Tsingle,T,n> &src,correlation_list *list WATCH_MEMBERS_PARAM) const;

public:
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info);

};

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n,int n1>
class raw_array_correlation_1 :
  public raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>
{
public:
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info);
};

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n,int n1,int n2>
class raw_array_correlation_2 :
  public raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>
{
public:
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info);
};


// For the mirror data structures, which will use the
// raw_array_zero_suppress_correlation name
#define raw_array_zero_suppress_correlation   raw_array_correlation
#define raw_list_zero_suppress_correlation    raw_array_correlation
#define raw_list_ii_zero_suppress_correlation raw_array_correlation

#define raw_array_zero_suppress_1_correlation raw_array_correlation_1
#define raw_array_zero_suppress_2_correlation raw_array_correlation_2
#define raw_list_zero_suppress_1_correlation  raw_array_correlation_1
#define raw_list_zero_suppress_2_correlation  raw_array_correlation_2

// The multi-entry array should eat one index

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n,int max_entries>
class raw_array_multi_correlation :
  public raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>
{
};

#define raw_array_multi_zero_suppress_correlation raw_array_multi_correlation

class unpack_subevent_base_correlation
{
public:
  void add_corr_members(const unpack_subevent_base &src,correlation_list *list WATCH_MEMBERS_PARAM) const { }
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info) { return false; }

};

class unpack_sticky_subevent_base_correlation :
  public unpack_subevent_base_correlation
{
};

class unpack_event_base_correlation
{
public:
  void add_corr_members(const unpack_event_base &src,correlation_list *list WATCH_MEMBERS_PARAM) const { }
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info) { return false; }

};

class unpack_sticky_event_base_correlation :
  public unpack_event_base_correlation
{
};

class raw_event_base_correlation
{
public:
  void add_corr_members(const raw_event_base &src,correlation_list *list WATCH_MEMBERS_PARAM) const { }
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info) { return false; }

};

class raw_sticky_base_correlation :
  public raw_event_base_correlation
{
};

class cal_event_base_correlation
{
public:
  void add_corr_members(const cal_event_base &src,correlation_list *list WATCH_MEMBERS_PARAM) const { }
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info) { return false; }

};

#endif//__RAW_DATA_CORRELATION_HH__

