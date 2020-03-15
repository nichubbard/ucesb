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

#ifndef __MULTI_CHUNK_HH__
#define __MULTI_CHUNK_HH__

#include "error.hh"

#include "multi_info.hh"

#include "../common/signal_id.hh"

#define SINGLE(name,ident) name ident;

#if !USING_MULTI_EVENTS
template<typename T,typename T_map>
class multi_chunks;
template<typename T>
class ptr;
#endif//!USING_MULTI_EVENTS

#if USING_MULTI_EVENTS

#define MAP_MEMBER_TYPE_MULTI_FIRST  0x0100
#define MAP_MEMBER_TYPE_MULTI_LAST   0x0200
#define MAP_MEMBER_TYPE_MULTI_OTHER  0x0400

#define MULTI(name,ident) ptr<name> ident; multi_chunks<name,name##_map> multi_##ident;

template<typename T>
class ptr
{
public:
  ptr()
  {
    _ptr = NULL;
  }

public:
  T *_ptr;

public:
  T &operator=(T *rhs)
  {
    _ptr = rhs;
    return *_ptr;
  }

public:
  T& operator() ()
  {
    return *_ptr;
  }

  const T& operator() () const
  {
    return *_ptr;
  }

public:
  const void *const *get_ptr_addr() const
  {
    return (const void *const *) (const void*) &(_ptr);
  }
};

struct external_toggle_map
{
  unsigned int  _cnts;
  unsigned int *_cnt_to_event;
};

template<int n_toggle>
class external_toggle_info
{
public:
  external_toggle_info()
  {
    _alloc_events = 0;
    _maps[0]._cnt_to_event = NULL;
  }

  ~external_toggle_info()
  {
    free(_maps[0]._cnt_to_event);
  }

public:
  void clear_alloc(uint32 events);

  void dump() const;

public:
  uint32              _alloc_events;
  external_toggle_map _maps[n_toggle];
};

struct correlation_list;
struct watcher_event_info;
struct pretty_dump_info;

template<typename T,typename T_map>
class multi_chunks
{
public:
  typedef T         item_t;
  typedef T_map     item_map_t;

public:
  multi_chunks()
  {
    _num_items = 0;
    _alloc_items = 0;

    _num_events = 0;
    _alloc_events = 0;

    _items = NULL;
    _item_event = NULL;

    _event_index = NULL;
    //_items[0].__clean();
    _item_null = new T;
    _item_null->__clean();
  };

  ~multi_chunks()
  {
    delete[] _items;
    delete[] _item_event;
    delete[] _event_index;
    delete _item_null;
  }

public:
  // The number of members we have
  uint32  _num_items;
  uint32  _alloc_items;

  // Our members (in unpack order)
  T      *_items;

  T      *_item_null;

  // An array for telling to which event each item belongs
  int *_item_event;

  uint32  _num_events;   // not strictly needed, used for checking
  uint32  _alloc_events;

  // An array holding the indices of our members to be used per event
  // (-1) means that no member is to be used for an event, but this
  // then is the implicitly zero-suppressed member
  int *_event_index;

public:
  void __clean()
  {
    _num_items  = 0;
    _num_events = 0;
    /*
    _alloc_items = 0;
    _alloc_events = 0;

    delete[] _items;
    delete[] _item_event;
    delete[] _event_index;

    _items = NULL;
    _item_event = NULL;
    _event_index = NULL;
    */
  }

public:
  item_t &next_free()
  {
    // We start by returning item[1], since item[0] is the
    // empty one!

    if (_num_items >= _alloc_items)
      {
	// We need to reallocate!

	uint32 new_size = _alloc_items ? _alloc_items * 2 : 8;

	item_t *new_items      = new item_t[new_size];
	int    *new_item_event = new int[new_size];

	if (!new_items || !new_item_event)
	  {
	    delete[] new_items;
	    delete[] new_item_event;
	    ERROR("Memory allocation failure, "
		  "could not allocate %d data entries for multi-event.",
		  _alloc_items);
	  }

	_alloc_items = new_size;

	// We need to copy the already unpacked items

	for (unsigned int i = 0; i < _num_items; i++)
	  new_items[i] = _items[i];

	delete[] _items;
	_items = new_items;

	// The index need not be copied!  (only used after unpacking)

	delete[] _item_event;
	_item_event = new_item_event;
      }

    item_t &item = _items[_num_items];
    _item_event[_num_items] = -1; // to force error, if unitialized when assigning!
    _num_items++;
    item.__clean(); // clean it before we start to fill it!
    return item;
  }

public:
  void dump(const signal_id &id,pretty_dump_info &pdi) const
  {
    // The dumping is done before data is interpreted in any way, so
    // we dump all contents

    for (uint32 i = 0; i < _num_items; i++)
      {
	item_t &item = _items[i];

	item.dump(signal_id(id,i,SIG_PART_MULTI_ENTRY),pdi);
      }
  }

public:
  void assign_events(uint32 events);

public:
  /*
  T &get_event_item(int i)
  {
    return _items[_index[i]];
  }
  */
  /*
  void show_members()
  {
    ERROR("multi_chunks::show_members unimplemented");
  }

  void enumerate_members()
  {
    ERROR("multi_chunks::enumerate_members unimplemented");
  }
  */
  item_t *map_members(const item_map_t &map
		      MAP_MEMBERS_PARAM) const
  {
    // we are now operating at multi_subevent no i, what index should
    // we use in the source tables?

    if ((unsigned int) info._multi_event_no > _num_events)
      ERROR("Attempt to map multi-event (%d) beyond known events (%d).",
	    info._multi_event_no,_num_events);

    if (!_event_index)
      ERROR("Attempt to map multi-event without event assignment.");

    int entry = _event_index[info._multi_event_no];

    //printf ("%d (this=%p _items=%p)\n",entry,this,_items);
    //fflush(stdout);

    if (entry < 0)
      return _item_null; // we have no data for this event

    item_t *item = &_items[entry];

    map.map_members(*item MAP_MEMBERS_ARG);

    return item;
  }

  template<typename T_watcher>
  void watch_members(const T_watcher &watcher,
		     watcher_event_info *watch_info
		     WATCH_MEMBERS_PARAM) const
  {
    // we are now operating at multi_subevent no i, what index should
    // we use in the source tables?

    if ((unsigned int) info._multi_event_no > _num_events)
      ERROR("Attempt to map multi-event (%d) beyond known events (%d).",
	    info._multi_event_no,_num_events);

    int entry = _event_index[info._multi_event_no];

    if (entry < 0)
      return; // we have no data for this event

    watcher.watch_members(_items[entry],watch_info WATCH_MEMBERS_ARG);
  }


  template<typename T_correlation>
  void add_corr_members(const T_correlation &correlation,
			correlation_list *list
			WATCH_MEMBERS_PARAM) const
  {
    // we are now operating at multi_subevent no i, what index should
    // we use in the source tables?

    if ((unsigned int) info._multi_event_no > _num_events)
      ERROR("Attempt to map multi-event (%d) beyond known events (%d).",
	    info._multi_event_no,_num_events);

    int entry = _event_index[info._multi_event_no];

    if (entry < 0)
      return; // we have no data for this event

    correlation.add_corr_members(_items[entry],list WATCH_MEMBERS_ARG);
  }

};



#endif//USING_MULTI_EVENTS

#endif//__MULTI_CHUNK_HH__
