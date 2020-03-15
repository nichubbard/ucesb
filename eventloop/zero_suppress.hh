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

#ifndef __ZERO_SUPPRESS_HH__
#define __ZERO_SUPPRESS_HH__

#include "typedef.hh"
#include "../common/signal_id.hh"
#include "bitsone.hh"
#include "enumerate.hh"
#include "multi_chunk.hh"

#include "simple_data_ops.hh"

////////////////////////////////////////////////////////////////////

struct zero_suppress_info;

struct used_zero_suppress_info;

void zero_suppress_info_ptrs(void* us,used_zero_suppress_info &used_info);

//template<typename Tsingle,typename T,int n>
//class raw_array_map;
//template<typename Tsingle,typename T,int n>
//class raw_list_ii_map;

//template<typename Tsingle,typename T,int n>
//class raw_array_watcher;

template<typename Tsingle,typename T,int n>
class raw_array
{
public:
  typedef T item_t;

public:
  T _items[n];

public:
  void __clean()
  {
    // If we go object oriented with the items:
    for (int i = 0; i < n; ++i)
      call___clean(_items[i]);
    // memset(_items,0,sizeof(_items));
  }

  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    for (int i = 0; i < n; ++i)
      call_dump(_items[i],signal_id(id,i),pdi);
  }

  const item_t &operator[](size_t i) const
  {
    return _items[i];
  }

public:
  item_t &insert_index(int loc,uint32 i)
  {
    if (i >= n)
      ERROR_U_LOC(loc,"Attempt to index outside array (%d>=%d)",i,n);
    // TODO: check that we did not find it again!!!
    return _items[i];
  }

  void zzp_on_insert_index(/*int loc,*/uint32 i,zero_suppress_info &info);

public:
  void show_members(const signal_id &id,const char* unit) const
  {
    call_show_members(_items[0],signal_id(id,n,SIG_PART_INDEX_LIMIT),unit);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&_items[0],signal_id(id,0),info,callback,extra);
    else
      for (int i = 0; i < n; ++i)
	call_enumerate_members(&_items[i],signal_id(id,i),info,callback,extra);
  }

  //void map_members(const raw_array_map<Tsingle,T,n> &map MAP_MEMBERS_PARAM) const;

  //void map_members(const raw_array_calib_map<Tsingle,T,n> &map) const;

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);
};

////////////////////////////////////////////////////////////////////

//template<typename Tsingle,typename T,int n>
//class raw_array_zero_suppress_map;

template<typename Tsingle,typename T,int n>
class raw_array_zero_suppress
{
public:
  typedef T item_t;

public:
  bitsone<n> _valid;
  T          _items[n];

public:
  void __clean()
  {
    //printf ("Clean...\n");
    _valid.clear();
  }
  /*
  void dump()
  {
    bitsone_iterator iter;
    ssize_t i;

    while ((i = _valid.next(iter)) >= 0)
      _items[i].dump();
  }
  */
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    bitsone_iterator iter;
    ssize_t i;

    while ((i = _valid.next(iter)) >= 0)
      call_dump(_items[i],signal_id(id,(int) i),pdi);
  }

public:
  item_t &operator[](size_t i)
  {
    // This is only to be used while setting pointers etc up,
    // so we won't affect _valid...

    assert(i < n);
    return _items[i];
  }

  const item_t &operator[](size_t i) const
  {
    return _items[i];
  }

public:
  void clean_item(item_t &item)
  {
    Tsingle *p = (Tsingle *) &item;

    for (size_t i = sizeof(T)/sizeof(Tsingle); i; --i, ++p)
      call___clean(*p);
  }

  item_t &insert_index(int loc,uint32 i)
  {
    if (i >= n)
      ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);

    // Do we know this item since before?

    if (_valid.get_set(i))
      {
	item_t &item = _items[i];
	return item;
      }

    // Now, we can insert our item.  First clean it out

    item_t &item = _items[i];
    clean_item(item);
    return item;
  }

  void on_insert_index(uint32 i)
  {
    if (_valid.get_set(i))
      return; // item was used before

    // Not used before, we need to clean it out
    item_t &item = _items[i];
    clean_item(item);
  }

  void zzp_on_insert_index(/*int loc,*/uint32 i,zero_suppress_info &info);

public:
  void show_members(const signal_id &id,const char* unit) const
  {
    call_show_members(_items[0],signal_id(id,n,SIG_PART_INDEX_LIMIT |
					  SIG_PART_INDEX_ZZP),unit);
  }

  void enumerate_members_mask(const signal_id &id,
			      const enumerate_info &info,
			      enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,_valid._bits,
			       ENUM_TYPE_ULINT | ENUM_IS_ARRAY_MASK,0,n),
	     extra);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&_items[0],
			     signal_id(id,0),
			     enumerate_info(info,id._parts.size()),
			     callback,extra);
    else
      {
	enumerate_members_mask(id,info,callback,extra);
	for (int i = 0; i < n; ++i)
	  call_enumerate_members(&_items[i],
				 signal_id(id,i),info,callback,extra);
      }
  }

public:
  //void map_members(const /*raw_array_zero_suppress_map*/raw_array_map<Tsingle,T,n> &map MAP_MEMBERS_PARAM) const;

  //void map_members(const raw_array_calib_map<Tsingle,T,n> &map) const;

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);
};

//

template<typename Tsingle,typename T,int n,int n1>
class raw_array_zero_suppress_1 :
  public raw_array_zero_suppress<Tsingle,T,n>
{
public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    bitsone_iterator iter;
    ssize_t i;

    while ((i = raw_array_zero_suppress<Tsingle,T,n>::_valid.next(iter)) >= 0)
      for (int i1 = 0; i1 < n1; ++i1)
	call_dump((raw_array_zero_suppress<Tsingle,T,n>::_items[i])[i1],
		  signal_id(signal_id(id,(int) i),i1),pdi);
  }

  void show_members(const signal_id &id,const char* unit) const
  {
    // item_t &items = _items;

    call_show_members((raw_array_zero_suppress<Tsingle,T,n>::
		       _items[0])[0],
		      signal_id(signal_id(id,n,SIG_PART_INDEX_LIMIT |
					  SIG_PART_INDEX_ZZP),
				n1,SIG_PART_INDEX_LIMIT),unit);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&(raw_array_zero_suppress<Tsingle,T,n>::_items[0])[0],
			     signal_id(signal_id(id,0),0),
			     enumerate_info(info,id._parts.size()),
			     callback,extra);
    else
      {
	raw_array_zero_suppress<Tsingle,T,n>::
	  enumerate_members_mask(id,info,callback,extra);
	for (int i = 0; i < n; ++i)
	  for (int i1 = 0; i1 < n1; ++i1)
	    call_enumerate_members(&(raw_array_zero_suppress<Tsingle,T,n>::
				    _items[i])[i1],
				   signal_id(signal_id(id,i),i1),
				   info,callback,extra);
      }
  }

  //void map_members(const raw_array_1_calib_map<Tsingle,T,n,n1> &map) const;

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);
};

//

template<typename Tsingle,typename T,int n,int n1,int n2>
class raw_array_zero_suppress_2 :
  public raw_array_zero_suppress<Tsingle,T,n>
{
public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    bitsone_iterator iter;
    ssize_t i;

    while ((i = raw_array_zero_suppress<Tsingle,T,n>::_valid.next(iter)) >= 0)
      for (int i1 = 0; i1 < n1; ++i1)
	for (int i2 = 0; i2 < n2; ++i2)
	  call_dump((raw_array_zero_suppress<Tsingle,T,n>::_items[i])[i1][i2],
		    signal_id(signal_id(signal_id(id,(int) i),i1),i2),pdi);
  }

  void show_members(const signal_id &id,const char* unit) const
  {
    call_show_members((raw_array_zero_suppress<Tsingle,T,n>::_items[0])[0][0],
		      signal_id(signal_id(signal_id(id,n,SIG_PART_INDEX_LIMIT |
						    SIG_PART_INDEX_ZZP),
					  n1,SIG_PART_INDEX_LIMIT),
				n2,SIG_PART_INDEX_LIMIT),unit);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&(raw_array_zero_suppress<Tsingle,T,n>::
			      _items[0])[0][0],
			     signal_id(signal_id(signal_id(id,0),0),0),
			     enumerate_info(info,id._parts.size()),
			     callback,extra);
    else
      {
	raw_array_zero_suppress<Tsingle,T,n>::
	  enumerate_members_mask(id,callback,extra);
	for (int i = 0; i < n; ++i)
	  for (int i1 = 0; i1 < n1; ++i1)
	    for (int i2 = 0; i2 < n2; ++i2)
	      call_enumerate_members(&(raw_array_zero_suppress<Tsingle,T,n>::
				      _items[i])[i1][i2],
				     signal_id(signal_id(signal_id(id,
								   i),i1),i2),
				     info,callback,extra);
      }
  }

  // void map_members(const raw_array_calib_map<Tsingle,T,n> &map) const;

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);
};

////////////////////////////////////////////////////////////////////

template<typename Tsingle,typename T,int n,int max_entries>
class raw_array_multi_zero_suppress
{
public:
  typedef T item_t;

public:
  // The assumption is that most of the time, most of the channels are
  // not used, and most of the entries are not used.  We have no
  // guarantee on the ordering of the channels though, so must have
  // enough space allocated.

  bitsone<n> _valid;
  uint32     _num_entries[n];
  T          _items[n][max_entries];

public:
  void __clean()
  {
    _valid.clear();
  }
  /*
  void dump()
  {
    bitsone_iterator iter;
    ssize_t i;

    while ((i = _valid.next(iter)) >= 0)
      {
	for (uint32 j = 0; j < _num_entries[i]; j++)
	  _items[i][j].dump();
      }
  }
  */
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    bitsone_iterator iter;
    ssize_t i;

    while ((i = _valid.next(iter)) >= 0)
      {
	for (uint32 j = 0; j < _num_entries[i]; j++)
	  call_dump(_items[i][j],signal_id(id,(int) i),pdi);
      }
  }

public:
  void clean_item(item_t &item)
  {
    Tsingle *p = (Tsingle *) &item;

    for (size_t i = sizeof(T)/sizeof(Tsingle); i; --i, ++p)
      call___clean(*p);
  }

  item_t &insert_index(int loc,uint32 i)
  {
    if (i >= n)
      ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);

    // Do we know this item since before?

    if (!_valid.get_set(i))
      _num_entries[i] = 0;

    // Now, we can insert our item.  First clean it out

    if (_num_entries[i] >= max_entries)
      ERROR_U_LOC(loc,
		  "Attempt to insert to many items "
		  "in multi-entry list (%d>=%d)",
		  _num_entries[i],max_entries);

    item_t &item = _items[i][_num_entries[i]++];
    clean_item(item);
    return item;
  }

  size_t on_insert_index(uint32 i)
  {
    if (!_valid.get_set(i))
      _num_entries[i] = 0;

    if (_num_entries[i] >= max_entries)
      ERROR("Attempt to insert to many items in multi-entry list (%d>=%d)",
	    _num_entries[i],max_entries);

    item_t &item = _items[i][_num_entries[i]++];
    clean_item(item);

    void *ptr   = (void *) &item;
    void *first = (void *) &_items[i][0];

    return (size_t) (((char*) ptr) - ((char*) first));
  }

  void zzp_on_insert_index(/*int loc,*/
			   uint32 i,uint32 j,zero_suppress_info &info);

public:
  void show_members(const signal_id &id,const char* unit) const
  {
    call_show_members(_items[0][0],
		      signal_id(signal_id(id,n,
					  SIG_PART_INDEX_LIMIT |
					  SIG_PART_INDEX_ZZP),
				max_entries,
				SIG_PART_INDEX_LIMIT |
				SIG_PART_MULTI_ENTRY),unit);
  }

  void enumerate_members_mask(const signal_id &id,
			      const enumerate_info &info,
			      enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,_valid._bits,
			       ENUM_TYPE_ULINT |
			       ENUM_IS_ARRAY_MASK/* | ENUM_IS_MULTI_ENTRY*/,
			       0,n),extra);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&_items[0][0],
			     signal_id(signal_id(id,0),0),
			     enumerate_info(info,id._parts.size()),
			     callback,extra);
    else
      {
	enumerate_members_mask(id,info,callback,extra);
	for (int i = 0; i < n; ++i)
	  {
	    signal_id id_i = signal_id(id,i);

	    callback(id_i,enumerate_info(info,&_num_entries[i],
					 ENUM_TYPE_UINT |
					 ENUM_IS_LIST_LIMIT |
					 ENUM_IS_LIST_LIMIT2,
					 0,max_entries),extra);
	    enumerate_info info2 = info;
	    for (int j = 0; j < max_entries; j++)
	      {
		call_enumerate_members(&_items[i][j],
				       signal_id(id_i,j),
				       info2,callback,extra);
		info2._type |= ENUM_NO_INDEX_DEST; // on non-first items
	      }
	  }
      }
  }

public:
  //void map_members(const /*raw_array_zero_suppress_map*/raw_array_map<Tsingle,T,n> &map MAP_MEMBERS_PARAM) const;

  //void map_members(const raw_array_calib_map<Tsingle,T,n> &map) const;

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);

};

////////////////////////////////////////////////////////////////////

template<typename Tsingle,typename T,int n>
class suppressed_item
{
public:
  unsigned int _index;
  T            _item;

public:
  void clear_set_index(uint32 i)
  {
    _index = i;
    // If we go object oriented with the items:
    Tsingle *p = (Tsingle *) &_item;

    for (size_t i = sizeof(T)/sizeof(Tsingle); i; --i, ++p)
      call___clean(*p);
    // memset(&_item,0,sizeof(_item));
  }

  void show_members(const signal_id &id,const char* unit) const
  {
    call_show_members(_item,id,unit);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    // enumerate _index also!
    callback(id,enumerate_info(info,&_index,
			       ENUM_TYPE_INT | ENUM_IS_LIST_INDEX,1,n),
	     extra);
    call_enumerate_members(&_item,id,info,callback,extra);
  }

public:
  /*
  void dump()
  {
    printf (" %02d:",_index);
    _item.dump();
  }
  */
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    call_dump(_item,signal_id(id,_index),pdi);
  }

  void dump_1(const signal_id &id,int i1,pretty_dump_info &pdi) const
  {
    call_dump(_item[i1],signal_id(signal_id(id,_index),i1),pdi);
  }

  void dump_2(const signal_id &id,int i1,int i2,pretty_dump_info &pdi) const
  {
    call_dump(_item[i1][i2],
	      signal_id(signal_id(signal_id(id,_index),i1),i2),pdi);
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    ::zero_suppress_info_ptrs(&_index,used_info);
    //_item.zero_suppress_info_ptrs(used_info);
    call_zero_suppress_info_ptrs(&_item,used_info);
  }

};

template<typename Tsingle,typename T,int n>
class raw_list_zero_suppress
{
public:
  typedef T item_t;
  typedef suppressed_item<Tsingle,T,n> suppressed_item_t;

public:
  uint32                       _num_items;
  suppressed_item<Tsingle,T,n> _items[n];

public:
  void __clean()
  {
    //printf ("Clean...\n");
    _num_items = 0;
  }
  /*
  void dump()
  {
    for (uint32 i = 0; i < _num_items; i++)
      _items[i].dump();
  }
  */
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    for (uint32 i = 0; i < _num_items; i++)
      call_dump(_items[i],id,pdi);
  }

public:
  item_t &operator[](size_t i)
  {
    // This is only to be used while setting pointers etc up,
    // so we won't affect _valid...

    assert(i < n);
    return _items[i]._item;
  }

  const item_t &operator[](size_t i) const
  {
    return _items[i];
  }

public:
  item_t &do_insert_index(uint32 i)
  {
    // Now we need to find our item...
    // This is now a little hairy.  We always keep the list sorted.

    // Special case if the item is past the end...
    if (!_num_items || _items[_num_items-1]._index < i)
      {
	// The item is not in the list, we need to add it to the end

	//printf ("%08x Insert %d at end\n",this,i);

	assert (_num_items < n);
	suppressed_item_t &item = _items[_num_items++];

	item.clear_set_index(i);
	return item._item;
      }

    // Special case if the item is the last in the list
    if (_num_items)
      {
	suppressed_item_t &item = _items[_num_items-1];
	if (item._index == i)
	  return item._item;
      }

    // The entry may be somewhere in the list...
    // Do a binary search...

    // We could use bsearch, but even if we do not find the particular
    // item, we would end up knowing where it belongs, so it makes sense
    // to use our own routine.

    int first = 0;          // first item that may be candidate
    int last  = _num_items; // last item+1 that may be candidate

    while (first < last)
      {
	int check = (first + last) / 2;

	suppressed_item_t &item = _items[check];

	if (item._index == i)
	  {
	    // TODO: check that we did not find it again!!!
	    // We found our candidate item
	    return item._item;
	  }

	if (item._index > i)
	  last = check;
	else
	  first = check+1;
      }

    // printf ("%08x Insert at %d [%d]\n",this,i,last);

    // So, we've reached the point where we know that entry does
    // not exist yet, but needs to be inserted.  At [last]

    assert (last == (int) _num_items || _items[last]._index > i);
    assert (last == 0 || _items[last-1]._index < i);

    // So first, we need to move all later items away

    memmove(&_items[last+1],
	    &_items[last],
	    sizeof (_items[0]) * (_num_items-last));

    _num_items++;

    // Now, we can insert our item.  First clean it out

    suppressed_item_t &item = _items[last];

    item.clear_set_index(i);
    return item._item;
  }

  item_t &insert_index(int loc,uint32 i)
  {
    // printf ("%08x Insert %d\n",this,i);

    if (i >= n)
      ERROR_U_LOC(loc,"Attempt to index outside list (%d>=%d)",i,n);

    return do_insert_index(i);
  }

  size_t on_insert_index(uint32 i)
  {
    item_t &item = do_insert_index(i);

    void *ptr   = (void *) &item;
    void *first = (void *) &_items[0]._item;

    return (size_t) (((char*) ptr) - ((char*) first));
  }

  void zzp_on_insert_index(/*int loc,*/uint32 i,zero_suppress_info &info);

public:
  void show_members(const signal_id &id,const char* unit) const
  {
    call_show_members(_items[0],signal_id(id,n,
					  SIG_PART_INDEX_LIMIT |
					  SIG_PART_INDEX_ZZP),unit);
  }

  void enumerate_members_limit(const signal_id &id,
			      const enumerate_info &info,
			      enumerate_fcn callback,void *extra) const
  {
    callback(id,enumerate_info(info,&_num_items,
			       ENUM_TYPE_UINT |
			       ENUM_IS_LIST_LIMIT,0,n),extra);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&_items[0],
			     signal_id(id,0),
			     enumerate_info(info,id._parts.size()),
			     callback,extra);
    else
      {
	enumerate_members_limit(id,info,callback,extra);
	for (int i = 0; i < n; ++i)
	  call_enumerate_members(&_items[i],
				 signal_id(id,i),info,callback,extra);
      }
  }

public:
  //void map_members(const /*raw_array_zero_suppress_map*/raw_array_map<Tsingle,T,n> &map MAP_MEMBERS_PARAM) const;

  //void map_members(const raw_array_calib_map<Tsingle,T,n> &map) const;

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);
};

//

template<typename Tsingle,typename T,int n,int n1>
class raw_list_zero_suppress_1 :
  public raw_list_zero_suppress<Tsingle,T,n>
{
public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    for (uint32 i = 0;
	 i < raw_list_zero_suppress<Tsingle,T,n>::_num_items; i++)
      for (int i1 = 0; i1 < n1; ++i1)
	(raw_list_zero_suppress<Tsingle,T,n>::_items[i]).dump_1(id,i1,pdi);
  }

  void show_members(const signal_id &id,const char* unit) const
  {
    // item_t &items = _items;

    call_show_members((raw_list_zero_suppress<Tsingle,T,n>::
		       _items[0])._item[0],
		      signal_id(signal_id(id,n,
					  SIG_PART_INDEX_LIMIT |
					  SIG_PART_INDEX_ZZP),
				n1,SIG_PART_INDEX_LIMIT),unit);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&(raw_list_zero_suppress<Tsingle,T,n>::
			      _items[0])._item[0],
			     signal_id(signal_id(id,0),0),
			     enumerate_info(info,id._parts.size()),
			     callback,extra);
    else
      {
	raw_list_zero_suppress<Tsingle,T,n>::
	  enumerate_members_limit(id,info,callback,extra);
	for (int i = 0; i < n; ++i)
	  for (int i1 = 0; i1 < n1; ++i1)
	    call_enumerate_members(&(raw_list_zero_suppress<Tsingle,T,n>::
				    _items[i])._item[i1],
				   signal_id(signal_id(id,i),i1),
				   info,callback,extra);
      }
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);
};

//

template<typename Tsingle,typename T,int n,int n1,int n2>
class raw_list_zero_suppress_2 :
  public raw_list_zero_suppress<Tsingle,T,n>
{
public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    for (uint32 i = 0; i < raw_list_zero_suppress<Tsingle,T,n>::_num_items; i++)
      for (int i1 = 0; i1 < n1; ++i1)
	for (int i2 = 0; i2 < n2; ++i2)
	  (raw_list_zero_suppress<Tsingle,T,n>::_items[i]).dump_1(id,i1,i2,pdi);
  }

  void show_members(const signal_id &id,const char* unit) const
  {
    // item_t &items = _items;

    call_show_members((raw_list_zero_suppress<Tsingle,T,n>::
		       _items[0])._item[0][0],
		      signal_id(signal_id(signal_id(id,n,
						    SIG_PART_INDEX_LIMIT |
						    SIG_PART_INDEX_ZZP),
					  n1,SIG_PART_INDEX_LIMIT),
				n2,SIG_PART_INDEX_LIMIT),unit);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&(raw_list_zero_suppress<Tsingle,T,n>::
			      _items[0])._item[0][0],
			     signal_id(signal_id(signal_id(id,0),0),0),
			     enumerate_info(info,id._parts.size()),
			     callback,extra);
    else
      {
	raw_list_zero_suppress<Tsingle,T,n>::
	  enumerate_members_limit(id,info,callback,extra);
	for (int i = 0; i < n; ++i)
	  for (int i1 = 0; i1 < n1; ++i1)
	    for (int i2 = 0; i2 < n2; ++i2)
	      call_enumerate_members(&(raw_list_zero_suppress<Tsingle,T,n>::
				      _items[i])._item[i1][i2],
				     signal_id(signal_id(signal_id(id,
								   i),i1),i2),
				     info,callback,extra);
      }
  }

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);
};

////////////////////////////////////////////////////////////////////

/*
template<typename Tsingle,typename T,int n>
class suppressed_item_ii
{
public:
  T   _item;

public:
  void clear_set_index(uint32 i)
  {
    _item.__clean();
  }

public:
  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info)
  {
    //_item.zero_suppress_info_ptrs(used_info);
    call_zero_suppress_info_ptrs(&_item,used_info);
  }
};
*/

template<typename Tsingle,typename T,int n>
class raw_list_ii_zero_suppress
{
public:
  typedef T item_t;
  // typedef suppressed_item_ii<Tsingle,T,n> suppressed_item_t;

public:
  uint32 _num_items;
  T      _items[n];

public:
  void __clean()
  {
    _num_items = 0;
  }
  /*
  void dump()
  {
    for (uint32 i = 0; i < _num_items; i++)
      _items[i].dump();
  }
  */
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    for (uint32 i = 0; i < _num_items; i++)
      call_dump(_items[i],id,pdi);
  }

public:
  item_t &operator[](size_t i)
  {
    // This is only to be used while setting pointers etc up,
    // so we won't affect _valid...

    assert(i < n);
    return _items[i];
  }

  const item_t &operator[](size_t i) const
  {
    return _items[i];
  }

public:
  item_t &append_item()
  {
    // The item is to be added to the end of the list

    //printf ("%08x Insert at end\n",this);

    if (_num_items >= n)
      ERROR("Attempt to append too many items (%d>=%d)",_num_items,n);
    item_t &item = _items[_num_items++];

    call___clean(item);
    return item;
  }

  item_t &append_item(int loc)
  {
    // printf ("%08x Insert %d\n",this,i);

    if (_num_items >= n)
      ERROR_U_LOC(loc,"Attempt to append too many items (%d>=%d)",_num_items,n);

    return append_item();
  }

  size_t on_append_item()
  {
    item_t &item = append_item();

    void *ptr   = (void *) &item;
    void *first = (void *) &_items[0];

    return (size_t) (((char*) ptr) - ((char*) first));
  }

  void zzp_on_insert_index(/*int loc,*/
			   uint32 i,zero_suppress_info &info);

public:
  void show_members(const signal_id &id,const char* unit) const
  {
    call_show_members(_items[0],signal_id(id,n,
					  SIG_PART_INDEX_LIMIT |
					  SIG_PART_MULTI_ENTRY),unit);
  }

  void enumerate_members(const signal_id &id,
			 const enumerate_info &info,
			 enumerate_fcn callback,void *extra) const
  {
    if (info._only_index0)
      call_enumerate_members(&_items[0],
			     signal_id(id,0),
			     enumerate_info(info,id._parts.size()),
			     callback,extra);
    else
      {
	callback(id,enumerate_info(info,&_num_items,
				   ENUM_TYPE_UINT | ENUM_IS_LIST_LIMIT,0,n),
		 extra);
	enumerate_info info2 = info;
	for (int i = 0; i < n; ++i)
	  {
	    call_enumerate_members(&_items[i],
				   signal_id(id,i),info2,callback,extra);
	    info2._type |= ENUM_NO_INDEX_DEST; // on non-first items
	  }
      }
  }

public:
  //void map_members(const /*raw_array_zero_suppress_map*/raw_list_ii_map<Tsingle,T,n> &map MAP_MEMBERS_PARAM) const;

  //void map_members(const raw_array_calib_map<Tsingle,T,n> &map) const;

  void zero_suppress_info_ptrs(used_zero_suppress_info &used_info);
};

////////////////////////////////////////////////////////////////////

#endif//__ZERO_SUPPRESS_HH__

