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

#ifndef __MULTI_CHUNK_FCN_HH__
#define __MULTI_CHUNK_FCN_HH__

template<typename T,typename T_map>
inline void multi_chunks<T,T_map>::assign_events(uint32 events)
{
  if (_alloc_events < events)
    {
      // We need to reallocate

      uint32 new_size = _alloc_events ? _alloc_events * 2 : 8;
      while (new_size < events)
	new_size *= 2;

      int *new_event_index = new int[new_size];

      if (!new_event_index)
	{
	  ERROR("Memory allocation failure, "
		"could not allocate %d event index entries for multi-event.",
		new_size);
	}

      _alloc_events = new_size;

      delete[] _event_index;
      _event_index = new_event_index;
    }

  // First set the entire (used) array to empty data (implicit zero-suppression)

  for (unsigned int i = 0; i < events; i++)
    _event_index[i] = -1;

  // Then set the entries that we have

  for (unsigned int item = 0; item < _num_items; item++)
    {
      int event = _item_event[item];

      if (event < 0 || (unsigned int) event >= events)
	ERROR("Invalid event number (%d >= %d) for item (%d).",
	      event,events,item);

      if (_event_index[event] != -1)
	{
	  // One could think about implementing merge functions to fix this!
	  ERROR("Several module chunks of data for the same event.");
	}

      _event_index[event] = item;
    }
  /*
  for (int i = 0; i < events; i++)
    printf ("%d  ",_event_index[i]);
  printf ("\n");
  */
  _num_events = events;
}

template<typename T,typename T_map,int handle_toggle>
void map_multi_events(multi_chunks<T,T_map> &module,
		      external_toggle_map *toggle_map,
		      uint32 counter_start,
		      uint32 events)
{
  // Loop over all events for this module, and set the indices
  // in the multi_chunk array to tell to what multi subevent each chunk
  // belongs

  for (unsigned int i = 0; i < module._num_items; i++)
    {
      T &item = module._items[i];

      //int local_counter = item. header.count;
      //int offset = (local_counter - counter_start) & 0x0000ffff;
      unsigned int offset = item.get_event_counter_offset(counter_start);

      if (!handle_toggle)
	{
	  if (offset >= events)
	    ERROR("Local event counter (0x%08x) "
		  "outside allowed range [0x%08x,0x%08x)",
		  item.get_event_counter(),
		  counter_start,counter_start+events);
	}
      else
	{
	  if (offset >= toggle_map->_cnts)
	    ERROR("Local event counter (before toggle map) (0x%08x) "
		  "outside allowed range [0x%08x,0x%08x).",
		  item.get_event_counter(),
		  counter_start,counter_start+toggle_map->_cnts);

	  offset = toggle_map->_cnt_to_event[offset];

	  if (offset >= events)
	    ERROR("Local event counter (after toggle map) (0x%08x) "
		  "outside allowed range [0,0x%08x)",
		  offset,events);
	}

      // event counter valid

      module._item_event[i] = offset;

      //printf ("%d:%d  ",i,offset);
    }
  //printf ("\n");

  module.assign_events(events);
}

template<typename T,typename T_map>
void map_multi_events(multi_chunks<T,T_map> &module,
		      uint32 counter_start,
		      uint32 events)
{
  map_multi_events<T,T_map,0>(module, NULL, counter_start, events);
}

template<typename T,typename T_map>
void map_multi_events(multi_chunks<T,T_map> &module,
		      external_toggle_map *toggle_map,
		      uint32 counter_start,
		      uint32 events)
{
  map_multi_events<T,T_map,1>(module, toggle_map, counter_start, events);
}

template<typename T,typename T_map>
void map_continuous_multi_events(multi_chunks<T,T_map> &module,
				 uint32 counter_start,
				 uint32 events)
{
  if (module._num_items > events)
    ERROR("Cannot map - number of items (%d) "
	  "different from number of events (%d).",
	  module._num_items,events);

  for (unsigned int i = 0; i < module._num_items; i++)
    {
      T &item = module._items[i];

      if (!item.good_event_counter_offset(counter_start + i))
	ERROR("Local event counter (0x%08x) "
	      "gives bad offset (expect 0x%08x (masked)).",
	      item.get_event_counter(),counter_start + i);

      module._item_event[i] = i;
    }

  module.assign_events(events);
}

template<int n_toggle>
void external_toggle_info<n_toggle>::clear_alloc(uint32 events)
{
  if (_alloc_events < events)
    {
      // We need to reallocate

      uint32 new_size = _alloc_events ? _alloc_events * 2 : 8;
      while (new_size < events)
	new_size *= 2;

      _maps[0]._cnt_to_event =
	(unsigned int *) realloc(_maps[0]._cnt_to_event,
				 n_toggle * new_size * sizeof (int));

      if (!_maps[0]._cnt_to_event)
	{
	  ERROR("Memory allocation failure, "
		"could not allocate %d event toggle map for multi-event.",
		new_size);
	}

      _alloc_events = new_size;

      for (int i = 1; i < n_toggle; i++)
	_maps[i]._cnt_to_event = _maps[0]._cnt_to_event + new_size * i;
    }

  for (int i = 0; i < n_toggle; i++)
    _maps[i]._cnts = 0;
}

template<int n_toggle>
void external_toggle_info<n_toggle>::dump() const
{
  for (int i = 0; i < n_toggle; i++)
    {
      printf ("toggle %d: %d items:", i, _maps[i]._cnts);
      for (unsigned int j = 0; j < _maps[i]._cnts; j++)
	printf (" %d", _maps[i]._cnt_to_event[j]);
      printf ("\n");
    }
}

template<typename T,typename T_map,int n_toggle>
void build_external_toggle_map(multi_chunks<T,T_map> &module,
			       external_toggle_info<n_toggle> *toggle_info,
			       uint32 counter_start,
			       uint32 events)
{
  toggle_info->clear_alloc(events);

  for (unsigned int i = 0; i < events; i++)
    {
      // Get the item that applies to this event

      int entry = module._event_index[i];

      if (entry < 0)
	ERROR("No item to describe toggle for multi-event (%d).", i);

      T *item = &module._items[entry];

      uint32 toggle_mask = item->get_external_toggle_mask();

      if (!toggle_mask)
	ERROR("Toggle mask empty for multi-event (%d).", i);

      if (toggle_mask >= (1 << n_toggle))
	ERROR("Toggle mask outside range (0x%x >= 0x%x) multi-event (%d).",
	      toggle_mask, (1 << n_toggle), i);

      for (int toggle_i = 0; toggle_i < n_toggle; toggle_i++)
	if (toggle_mask & (1 << toggle_i))
	  {
	    external_toggle_map &toggle_map = toggle_info->_maps[toggle_i];

	    int cnt = toggle_map._cnts++;

	    toggle_map._cnt_to_event[cnt] = i;
	  }
    }
}

#endif//__MULTI_CHUNK_FCN_HH__
