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

#include "event_base.hh"

#include "signal_id_map.hh"

#include "../common/node.hh"
#include "../common/prefix_unit.hh"

#include "enumerate.hh"

#include <set>

struct sid_node
{
public:
  virtual ~sid_node() { }

};

// A named entry, either leaf or struct or array
struct sid_named
  : public sid_node
{
public:
  sid_named(const char *name)
  {
    _name  = name;
    _entry = NULL;
  }

public:
  virtual ~sid_named() { }

public:
  const char       *_name;
  struct sid_node *_entry;

public:
  bool operator<(const sid_named &rhs) const
  {
    return strcmp(_name,rhs._name) < 0;
  }
};

typedef std::set<sid_named *,compare_ptr<sid_named> > sid_named_set;

// Contains named entries
struct sid_struct
  : public sid_node
{
public:
  virtual ~sid_struct() { }

public:
  sid_named_set _entries;
};

struct sid_array
  : public sid_node
{
public:
  virtual ~sid_array() { }

public:
  std::vector<sid_node *> _entries;
};

struct sid_leaf
  : public sid_node
{
public:
  sid_leaf()
  {
    _info = NULL;
  }

public:
  virtual ~sid_leaf() { }

public:
  signal_id_info *_info;

};

template<int create_missing>
sid_leaf *find_leaf(const signal_id &id,
		    sid_node **base_ptr)
{
  // Walk down the tree of items starting at base, and select the
  // member based on the signal_id

  sig_part_vector::const_iterator d;

  // sid_node *cur = base;

  sid_node **cur_ptr = base_ptr;

  for (d = id._parts.begin() ; d != id._parts.end(); ++d)
    {
      const sig_part &direction = *d;

      // If d is an index, we want the current item to be an array
      // If d is an named member, we want the current item to be a structure

      // A named entry always contain another member, either an array,
      // a structure or a leaf node!...

      if (direction._type == SIG_PART_NAME)
	{
	  sid_struct *str = dynamic_cast<sid_struct *>(*cur_ptr);

	  if (!str)
	    {
	      if (*cur_ptr || !create_missing)
		{
		  WARNING("signal_id type (index/name/leaf) mismatch (name)");
		  return NULL;
		}
	      else
		{
		  //INFO("Adding struct (for %s)...",direction._id._name);

		  str = new sid_struct;

		  if (!str)
		    ERROR("Memory allocation failure");

		  *cur_ptr = str;
		}
	    }

	  sid_named key(direction._id._name); // for finding the item

	  sid_named_set::iterator iter = str->_entries.lower_bound(&key);

	  sid_named *named;

	  if (iter == str->_entries.end() ||
	      key < **iter)
	    {
	      // Item does not exist!

	      if (!create_missing)
		{
		  WARNING("signal_id name (%s) does not exist",
			  direction._id._name);
		  return NULL;
		}
	      else
		{
		  //INFO("Adding named (%s)...",direction._id._name);

		  // We can use the pointer to the string, since it
		  // comes from the enumrate functions running over
		  // the data structures, and the strings are constant
		  // strings

		  named = new sid_named(direction._id._name);

		  if (!named)
		    ERROR("Memory allocation failure");

		  str->_entries.insert(iter,named);
		}
	    }
	  else
	    named = *iter;

	  cur_ptr = &named->_entry;
	}
      else if (direction._type == SIG_PART_INDEX)
	{
	  sid_array *array = dynamic_cast<sid_array *>(*cur_ptr);

	  if (!array)
	    {
	      if (*cur_ptr || !create_missing)
		{
		  WARNING("signal_id type (index/name/leaf) mismatch (index)");
		  return NULL;
		}
	      else
		{
		  // There is not even an array, let's create it!

		  //INFO("Adding array...");
		  array = new sid_array;

		  if (!array)
		    ERROR("Memory allocation failure");

		  // Insert the array

		  *cur_ptr = array;
		}
	    }

	  // Make sure the vector is large enough!

	  if ((unsigned int) direction._id._index >= array->_entries.size())
	    {
	      if (!create_missing)
		{
		  WARNING("signal_id array index (%d) out of bounds (%d)",
			  direction._id._index,(int) array->_entries.size());
		  return NULL;
		}
	      else
		{
		  //INFO("Expanding array (%d->%d)...",array->_entries.size(),direction._id._index+1);
		  array->_entries.resize((size_t) direction._id._index+1,NULL);
		}
	    }

	  cur_ptr = &(array->_entries[(size_t) direction._id._index]);

	  // *cur_ptr may be null if the item has not been initialized before
	}
      else
	{
	  /*
	  char str[256];
	  id.format(str,sizeof(str));
	  printf ("Fail: %s\n",str);
	  */
	  ERROR("Internal error finding signal id in tree."); // your sig_part is broken
	}
    }

  // cur_ptr should now point to a pointer to our element (if existing),
  // or point to the place where we should insert our element

  sid_leaf *leaf = dynamic_cast<sid_leaf *>(*cur_ptr);

  if (!leaf)
    {
      if (*cur_ptr || !create_missing)
	{
	  WARNING("signal_id type (index/name/leaf) mismatch (leaf)");
	  return NULL;
	}
      else
	{
	  leaf = new sid_leaf;

	  if (!leaf)
	    ERROR("Memory allocation failure");

	  leaf->_info = new signal_id_info;

	  if (!leaf->_info)
	    ERROR("Memory allocation failure");

	  memset(leaf->_info,0,sizeof(*leaf->_info));

	  *cur_ptr = leaf;

	  //INFO("Added leaf... (%p,%p)",leaf,leaf->_info);
	}
    }

  return leaf;
}


sid_node *signal_id_map_base[SID_MAP_MAX_NUM];

void enumerate_member_signal_id(const signal_id &id,
				const enumerate_info &info,
				void *extra)
{
  enumerate_msid_info *extra_info = (enumerate_msid_info *) extra;

  // We do not (currently) handle the zero-suppress control
  // information in our signal_id map

  // Due to it ending up at other levels as other data, and also not
  // having any proper names, it caused clashes in the map
  /*
  {
    char str[64];
    id.format(str,sizeof(str));

    INFO("Enumerate for member %d: %s ... (0x%x)",
         extra_info->_map_no,str,info._type);
  }
  */
  // NOTE: unsure why ENUM_IS_TOGGLE_I/V has to be omitted
  if (info._type & (ENUM_IS_ARRAY_MASK |
		    ENUM_IS_LIST_LIMIT |
		    ENUM_IS_LIST_LIMIT2 |
		    ENUM_IS_LIST_INDEX |
		    ENUM_IS_TOGGLE_I |
		    ENUM_IS_TOGGLE_V))
    return;

  sid_leaf *leaf;

  leaf = find_leaf<1>(id,&signal_id_map_base[extra_info->_map_no]);

  assert(leaf);

  leaf->_info->_addr = info._addr;
  leaf->_info->_type = info._type;
  leaf->_info->_unit = info._unit;
}

signal_id_info *find_signal_id_info(const signal_id &id,
				    enumerate_msid_info *extra)
{
  sid_leaf *leaf;

  leaf = find_leaf<1>(id,&signal_id_map_base[extra->_map_no]);

  assert (leaf);

  return leaf->_info;
}

const signal_id_info *get_signal_id_info(const signal_id &id,
					 int map_no)
{
  sid_leaf *leaf;

  leaf = find_leaf<0>(id,&signal_id_map_base[map_no]);

  if (!leaf)
    return NULL;
  /*
  {
    char str[64];
    id.format(str,sizeof(str));

    INFO("Get for %s ... (%p,%p)",str,leaf,leaf->_info);
  }
  */
  return leaf->_info;
}


typedef std::map<signal_id,signal_id_zzp_info> signal_id_zzp_info_map;

signal_id_zzp_info_map _signal_id_zzp_info_map[SID_MAP_MAX_NUM];

void enumerate_member_signal_id_zzp_part(const signal_id &id,
					 const enumerate_info &info,
					 void *extra)
{
  enumerate_msid_info *extra_info = (enumerate_msid_info *) extra;

  // We do not (currently) handle the zero-suppress control
  // information in our signal_id map

  // Due to it ending up at other levels as other data, and also not
  // having any proper names, it caused clashes in the map
  /*
  {
    char str[64];
    id.format(str,sizeof(str));

    INFO("Enumerate for zzp part %d: %s ... (0x%x)",
         extra_info->_map_no,str,info._type);
  }
  */
  // NOTE: unsure why ENUM_IS_TOGGLE_I/V has to be omitted
  if (info._type & (ENUM_IS_ARRAY_MASK |
		    ENUM_IS_LIST_LIMIT |
		    ENUM_IS_LIST_LIMIT2 |
		    ENUM_IS_LIST_INDEX |
		    ENUM_IS_TOGGLE_I |
		    ENUM_IS_TOGGLE_V))
    return;

  signal_id_zzp_info_map *map = &_signal_id_zzp_info_map[extra_info->_map_no];

  signal_id_zzp_info zzp_info;

  zzp_info._zzp_part = info._signal_id_zzp_part;

  map->insert(signal_id_zzp_info_map::value_type(id,zzp_info));
}

const signal_id_zzp_info *get_signal_id_zzp_info(const signal_id &id,
						 int map_no)
{
  signal_id_zzp_info_map *map = &_signal_id_zzp_info_map[map_no];

  signal_id_zzp_info_map::iterator iter = map->find(id);

  if (iter == map->end())
    return NULL;

  return &(iter->second);
}

void setup_signal_id_map()
{
  // used_zero_suppress_info used_info(new zero_suppress_info);

  for (int i = 0; i < SID_MAP_MAX_NUM; i++)
    signal_id_map_base[i] = NULL;

  enumerate_info info;
  enumerate_info info_zzp_part;

  info_zzp_part._only_index0 = true;

  enumerate_msid_info extra;

  memset(&extra,0,sizeof(extra));

  extra._map_no = SID_MAP_UNPACK;
  _static_event._unpack.enumerate_members(signal_id(),info,
					  enumerate_member_signal_id,&extra);
  _static_event._unpack.enumerate_members(signal_id(),info_zzp_part,
					  enumerate_member_signal_id_zzp_part,
					  &extra);

  extra._map_no = SID_MAP_UNPACK | SID_MAP_STICKY;
  _static_sticky_event._unpack.enumerate_members(signal_id(),info,
					  enumerate_member_signal_id,&extra);
  _static_sticky_event._unpack.enumerate_members(signal_id(),info_zzp_part,
					  enumerate_member_signal_id_zzp_part,
					  &extra);

#ifndef USE_MERGING
  extra._map_no = SID_MAP_RAW;
  _static_event._raw.enumerate_members(signal_id(),info,
				       enumerate_member_signal_id,&extra);
  _static_event._raw.enumerate_members(signal_id(),info_zzp_part,
				       enumerate_member_signal_id_zzp_part,
				       &extra);

  extra._map_no = SID_MAP_RAW | SID_MAP_STICKY;
  _static_sticky_event._raw.enumerate_members(signal_id(),info,
				       enumerate_member_signal_id,&extra);
  _static_sticky_event._raw.enumerate_members(signal_id(),info_zzp_part,
				       enumerate_member_signal_id_zzp_part,
				       &extra);

  extra._map_no = SID_MAP_CAL;
  _static_event._cal.enumerate_members(signal_id(),info,
				       enumerate_member_signal_id,&extra);
  _static_event._cal.enumerate_members(signal_id(),info_zzp_part,
				       enumerate_member_signal_id_zzp_part,
				       &extra);

#ifdef USER_STRUCT
  extra._map_no = SID_MAP_USER;
  _static_event._user.enumerate_members(signal_id(),info,
					enumerate_member_signal_id,&extra);
  _static_event._user.enumerate_members(signal_id(),info_zzp_part,
					enumerate_member_signal_id_zzp_part,
					&extra);
#endif

#ifdef CALIB_STRUCT
  extra._map_no = SID_MAP_CALIB;
  _calib.enumerate_members(signal_id(),info,
			   enumerate_member_signal_id,&extra);
#endif

  extra._map_no = SID_MAP_UNPACK | SID_MAP_MIRROR_MAP;
  setup_signal_id_map_unpack_map(&extra);

  extra._map_no = SID_MAP_UNPACK | SID_MAP_STICKY | SID_MAP_MIRROR_MAP;
  setup_signal_id_map_unpack_sticky_map(&extra);

  extra._map_no = SID_MAP_RAW | SID_MAP_MIRROR_MAP;
  setup_signal_id_map_raw_map(&extra);

  extra._map_no = SID_MAP_RAW | SID_MAP_MIRROR_MAP | SID_MAP_REVERSE;
  setup_signal_id_map_raw_reverse_map(&extra);

  extra._map_no =
    SID_MAP_RAW | SID_MAP_STICKY | SID_MAP_MIRROR_MAP | SID_MAP_REVERSE;
  setup_signal_id_map_raw_sticky_reverse_map(&extra);
#endif//!USE_MERGING
}
