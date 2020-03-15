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

#ifndef __ZERO_SUPPRES_MAP_HH__
#define __ZERO_SUPPRES_MAP_HH__

#include "zero_suppress.hh"

#define ZZP_INFO_NONE                           0x00
#define ZZP_INFO_FIXED_LIST                     0x01
#define ZZP_INFO_CALL_ARRAY_INDEX               0x02
#define ZZP_INFO_CALL_ARRAY_MULTI_INDEX         0x03
#define ZZP_INFO_CALL_LIST_INDEX                0x04
#define ZZP_INFO_CALL_LIST_II_INDEX             0x05
#define ZZP_INFO_CALL_ARRAY_LIST_II_INDEX       0x06 // ARRAY and LIST_II
#define ZZP_INFO_CALL_LIST_LIST_II_INDEX        0x07 // LIST  and LIST_II
#define ZZP_INFO_MASK                           0x07

#define ZZP_INFO_PTR_OFFSET                     0x08 // bitmask

typedef void(*call_on_array_insert_fcn)(void *,uint);
typedef size_t(*call_on_list_insert_fcn)(void *,uint);
typedef size_t(*call_on_list_ii_insert_fcn)(void *);

struct zero_suppress_info
{
public:
  zero_suppress_info(const zero_suppress_info* src,
		     bool ignore_none_check = false)
  {
    _type       = src->_type;
    _toggle_max = src->_toggle_max;
    _ptr_offset = src->_ptr_offset;
    
    if (!ignore_none_check &&
	(_type & ZZP_INFO_MASK) != ZZP_INFO_NONE)
      {
	// If you reach this point, then you have not protected the
	// code enough!  This is a consequence of us not being able to
	// handle several levels or zero suppress info
	ERROR("Internal error in zero_suppress_info constructor (%x).",
	      _type);
      }
  }

public:
  zero_suppress_info()
  {
    _type       = ZZP_INFO_NONE;
    _toggle_max = 0;
    _ptr_offset = NULL;
  }

public:
  int   _type;
  int   _toggle_max;

  union
  {
    struct
    {
      union
      {
	call_on_array_insert_fcn  _call;
	call_on_list_insert_fcn   _call_multi;
      };
      void                     *_item;
      uint                      _index;

      unsigned long            *_limit_mask;
    } _array;
    struct
    {
      call_on_list_insert_fcn    _call;

      void                    *_item;
      uint                     _index;

      // to be subtracted from _dest before use/insert
      size_t                   _dest_offset;

      uint32                  *_limit;
    } _list;
    struct
    {
      uint                     _index;
    } _fixed_list;
  };
  struct
  {
    call_on_list_ii_insert_fcn _call_ii;

    void                    *_item;
    // TODO: is this one really needed for list_ii ???
    // causes one extra copy of this structure per member!
    // could be calculated with a base and stride??
    uint                     _index_x;

    // to be subtracted from _dest before use/insert
    // TODO: is this one really needed for list_ii ???
    // causes one extra copy of this structure per member!
    size_t                   _dest_offset_x;

    uint32                  *_limit;
  } _list_ii;

  void set_fixed_array(uint index)
  {
    assert (!(_type & ZZP_INFO_MASK));
    _type |= ZZP_INFO_FIXED_LIST;
    _fixed_list._index = index;
  }

  void set_zzp_array(call_on_array_insert_fcn call,
		     call_on_list_insert_fcn call_multi,
		     void *item, uint index, unsigned long *limit_mask)
  {
    assert (!(_type & ZZP_INFO_MASK));

    if (call)
      {
	_type |= ZZP_INFO_CALL_ARRAY_INDEX;
	_array._call = call;
      }
    else if (call_multi)
      {
	_type |= ZZP_INFO_CALL_ARRAY_MULTI_INDEX;
	_array._call_multi = call_multi;
      }
    else
      {
	assert(false);
      }
    
    _array._item = item;
    _array._index = index;
    _array._limit_mask = limit_mask;
  }

  void set_zzp_list(call_on_list_insert_fcn call,
		    void *item, uint index, uint32 *limit,
		    size_t dest_offset)
  {
    assert (!(_type & ZZP_INFO_MASK));
    _type |= ZZP_INFO_CALL_LIST_INDEX;
    _list._call = call;

    _list._item = item;
    _list._index = index;
    _list._limit = limit;

    _list._dest_offset = dest_offset;
  }

  void set_zzp_list_ii(call_on_list_ii_insert_fcn call_ii,
		       void *item, uint index, uint32 *limit,
		       size_t dest_offset)
  {
    int new_type = 0;

    switch (_type & ZZP_INFO_MASK)
      {
      case ZZP_INFO_CALL_ARRAY_INDEX:
	new_type       = ZZP_INFO_CALL_ARRAY_LIST_II_INDEX;
	break;
      case ZZP_INFO_CALL_LIST_INDEX:
	new_type       = ZZP_INFO_CALL_LIST_LIST_II_INDEX;
	break;
      case ZZP_INFO_NONE:
	new_type       = ZZP_INFO_CALL_LIST_II_INDEX;
	break;
      default:
	ERROR("Two levels of unsupported zero suppression combination!");
	break;
	
	/* Special case, set_zzp_array was just called. */
      case ZZP_INFO_CALL_ARRAY_MULTI_INDEX:
	new_type       = ZZP_INFO_CALL_ARRAY_MULTI_INDEX;
	break;
      }

    _type &= ~ZZP_INFO_MASK;
    assert (!(_type & ZZP_INFO_MASK));
    _type |= new_type;
    _list_ii._call_ii = call_ii;

    _list_ii._item = item;
    _list_ii._index_x = index;
    _list_ii._limit = limit;

    _list_ii._dest_offset_x = dest_offset;
  }

  void set_zzp_multi_array(call_on_list_insert_fcn call_multi,
			   void *item, uint index, unsigned long *limit_mask,
			   uint index_ii, uint32 *limit_ii,
			   size_t dest_offset)
  {
    assert (!(_type & ZZP_INFO_MASK));

    set_zzp_array(NULL, call_multi,
		  item, index, limit_mask);

    set_zzp_list_ii(NULL,
		    NULL, index_ii, limit_ii,
		    dest_offset);
  }

  const void *const *_ptr_offset;
};

//template<typename T>
//void zero_suppress_info_ptrs(T* us,used_zero_suppress_info &used_info);
void zero_suppress_info_ptrs(void* us,used_zero_suppress_info &used_info);

void insert_zero_suppress_info_ptrs(void *us,
				    used_zero_suppress_info &used_info);

template<typename Tsingle,typename T,int n>
void raw_array<Tsingle,T,n>::
zzp_on_insert_index(/*int loc,*/uint32 i,zero_suppress_info &info)
{
  if (i >= n)
    // ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);
    ERROR("Attempt to index outside list (%d>=%d)",i,n);

  info.set_fixed_array(i);
}

template<typename T>
void call_on_insert_array_index(void *container,uint i)
{
  ((T *)container)->on_insert_index(i);
}

template<typename Tsingle,typename T,int n>
void raw_array_zero_suppress<Tsingle,T,n>::
zzp_on_insert_index(/*int loc,*/uint32 i,zero_suppress_info &info)
{
  if (i >= n)
    // ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);
    ERROR("Attempt to index outside list (%d>=%d)",i,n);
  /*
    _valid.get_set_ptr(i,
    &zero_suppress_info.addr,
    &zero_suppress_info.mask);

    zero_suppress_info._item = _items[i];

    item_t &item = _items[i];
  */

  call_on_array_insert_fcn call =
    call_on_insert_array_index< raw_array_zero_suppress<Tsingle,T,n> >;
  info.set_zzp_array(call, NULL,
		     this, i, _valid._bits);
}

template<typename T>
size_t call_on_insert_list_index(void *container,uint i)
{
  size_t offset = ((T *)container)->on_insert_index(i);
  return offset;
}

template<typename Tsingle,typename T,int n>
void raw_list_zero_suppress<Tsingle,T,n>::
zzp_on_insert_index(/*int loc,*/uint32 i,zero_suppress_info &info)
{
  if (i >= n)
    // ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);
    ERROR("Attempt to index outside list (%d>=%d)",i,n);

  call_on_list_insert_fcn call =
    call_on_insert_list_index< raw_list_zero_suppress<Tsingle,T,n> >;
  void *ptr   = (void *) &_items[i];
  void *first = (void *) &_items[0];
  info.set_zzp_list(call,
		    this, i, &_num_items,
		    (size_t) (((char*) ptr) - ((char*) first)));
}

template<typename Tsingle,typename T,int n,int max_entries>
void raw_array_multi_zero_suppress<Tsingle,T,n,max_entries>::
zzp_on_insert_index(/*int loc,*/uint32 i,uint32 j,zero_suppress_info &info)
{
  if (i >= n)
    // ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);
    ERROR("Attempt to index outside list (%d>=%d)",i,n);
  if (j >= max_entries)
    // ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);
    ERROR("Attempt to index outside list (%d>=%d)",j,max_entries);
  /*
    _valid.get_set_ptr(i,
    &zero_suppress_info.addr,
    &zero_suppress_info.mask);

    zero_suppress_info._item = _items[i];

    item_t &item = _items[i];
  */

  call_on_list_insert_fcn call_multi =
    call_on_insert_list_index< raw_array_multi_zero_suppress<Tsingle,T,
							     n,max_entries> >;

  void *ptr   = (void *) &_items[i][j];
  void *first = (void *) &_items[i][0];

  info.set_zzp_multi_array(call_multi,
			   this, i, _valid._bits,
			   j, &_num_entries[i],
			   (size_t) (((char*) ptr) - ((char*) first)));
}

template<typename T>
size_t call_on_insert_list_ii_item(void *container)
{
  size_t offset = ((T *)container)->on_append_item();
  return offset;
}

template<typename Tsingle,typename T,int n>
void raw_list_ii_zero_suppress<Tsingle,T,n>::
zzp_on_insert_index(/*int loc,*/uint32 i,zero_suppress_info &info)
{
  if (i >= n)
    // ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);
    ERROR("Attempt to index outside list (%d>=%d)",i,n);

  call_on_list_ii_insert_fcn call_ii =
    call_on_insert_list_ii_item< raw_list_ii_zero_suppress<Tsingle,T,n> >;

  void *ptr   = (void *) &_items[i];
  void *first = (void *) &_items[0];

  info.set_zzp_list_ii(call_ii,
		       this, i, &_num_items,
		       (size_t) (((char*) ptr) - ((char*) first)));
}

const zero_suppress_info *
get_ptr_zero_suppress_info(void *us,const void *const *ptr_offset,
			   bool allow_missing);

void setup_zero_suppress_info_ptrs();

#endif//__ZERO_SUPPRES_MAP_HH__
