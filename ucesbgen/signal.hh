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

#ifndef __SIGNAL_HH__
#define __SIGNAL_HH__

#include "variable.hh"
#include "node.hh"
#include "dumper.hh"

#include "file_line.hh"
#include "prefix_unit.hh"

#include <vector>
#include <map>

#include <string>

#include "signal_id.hh"

struct event_signal;

typedef std::map<const char *,event_signal*> event_signal_map;

typedef std::map<int,event_signal*> ordered_event_signal_map;

struct index_info
{
public:
  index_info(int size,
	     int info = 0)
  {
    _size = size;
    _info = info;
  }

public:
  int _size;
  int _info;

public:
  file_line _info_loc;
};

typedef std::vector<index_info*> index_info_vector;
typedef std::vector<int> index_vector;

class signal_spec_base;
class signal_spec;

typedef std::map<index_vector,signal_spec*> event_index_item;

struct event_signal
{
public:
  event_signal(signal_spec_base *decl,
	       const char *name);
  virtual ~event_signal() { }

public:
  const char      *_type;
  const char      *_unit;
  const char      *_name;
  index_info_vector _indices; // no entries => no indices

public:
  // If we have no children, then we're a leaf
  event_signal_map _children;

  // If (leaf) is supposed to be multi-entry
  int              _multi_size;
  file_line        _multi_loc;

  int              _tag;

public:
  signal_spec_base *_decl;
  signal_spec_base *_decl_unit;
  // file_line _loc_decl; // location of first signal forcing (declaring) our existance

public:
  // When we are a leaf, list all the indices that are used to reach us.
  // This is used to prevent double usage.

  event_index_item _items;

public:
  virtual void dump(dumper &d,int level,
		    const char *zero_suppress_type,
		    const index_info *prev_zero_suppress_index,
		    const std::string &prefix,
		    const char *base_suffix,
		    bool toggle) const;
};

class signal_spec_base
  : public def_node
{
public:
  virtual ~signal_spec_base() { }

public:
  signal_spec_base(const file_line &loc,
		   int signal_count,
		   int sticky,
		   const char *name)
  {
    _loc = loc;

    _name = name;

    _sticky = sticky;

    // For each seen in the .spec, increase a counter and use for
    // ordering.  Using the line-number led to lost signals with
    // multi-line signals.  And that is not what line-numbers are for!
    // BOZO!
    _order_index = signal_count;
  }

public:
  const char     *_name;

  signal_id       _id;

  int             _sticky;

public:
  file_line _loc;
  int       _order_index;

public:
  virtual void dump(dumper &d) const = 0;
};

#define MAX_NUM_TOGGLE  2

#define SIGNAL_TAG_FIRST_EVENT  0x0001
#define SIGNAL_TAG_LAST_EVENT   0x0002
#define SIGNAL_TAG_TOGGLE_1     0x0004
#define SIGNAL_TAG_TOGGLE_2     0x0008
#define SIGNAL_TAG_TOGGLE_MASK (SIGNAL_TAG_TOGGLE_1 | \
				SIGNAL_TAG_TOGGLE_2)

// Temporary for parsing.
struct signal_spec_ident_var
{
public:
  signal_spec_ident_var(const char *name,
			const var_name *ident)
  {
    _name  = name;
    _ident = ident;
  }

public:
  const char *_name;
  const var_name *_ident;
};

class signal_spec_type_unit
{
public:
  virtual ~signal_spec_type_unit() { }

public:
  signal_spec_type_unit(const char *type,
			const char *unit = NULL)
  {
    _type = type;
    _unit = unit;
  }

public:
  const char *_type;
  const char *_unit;

public:
  virtual void dump(dumper &d) const;
};

class signal_spec_types
{
public:
  virtual ~signal_spec_types() { }

public:
  signal_spec_types(const signal_spec_type_unit *tu_raw,
		    const signal_spec_type_unit *tu_cal = NULL)
  {
    _tu[0] = tu_raw;
    _tu[1] = tu_cal;
  }

public:
  const signal_spec_type_unit *_tu[2];

public:
  virtual void dump(dumper &d) const;
};

class signal_spec
  : public signal_spec_base
{
public:
  virtual ~signal_spec() { }

public:
  signal_spec(const file_line &loc,
	      int signal_count,
	      int sticky,
	      const char *name,
	      const var_name *ident,
	      const signal_spec_types *types,
	      int tag)
    : signal_spec_base(loc,signal_count,sticky,name)
  {
    _ident[0] = _ident[1] = NULL;

    int toggle_i = 0;
    
    if (tag & SIGNAL_TAG_TOGGLE_2)
      toggle_i = 1;

    _ident[toggle_i] = ident;

    /* fprintf (stderr, "%s: %x\n", name, tag); */

    _types = types;
    _tag = tag;
  }

public:
  const var_name *_ident[MAX_NUM_TOGGLE];
  const signal_spec_types *_types;

  signal_id       _id;

  int             _tag;

public:
  virtual void dump(dumper &d) const;

public:
  void dump_one_toggle(dumper &d, int toggle_tag_hide, int toggle_i_hide) const;
  void dump_tag(dumper &d, int toggle_tag_hide) const;
  void dump_idents(dumper &d, int toggle_i_hide) const;
};

class signal_spec_range
  : public signal_spec
{
public:
  virtual ~signal_spec_range() { }

public:
  signal_spec_range(const file_line &loc,
		    int signal_count,
		    int sticky,
		    const char *name,
		    const var_name *ident,
		    const char *name_last,
		    const var_name *ident_last,
		    const signal_spec_types *types,
		    int tag)
    : signal_spec(loc,signal_count,sticky,name,ident,types,tag)
  {
    _name_last = name_last;
    _ident_last = ident_last;
  }

public:
  const char     *_name_last;
  const var_name *_ident_last;

public:
  virtual void dump(dumper &d) const;
};

#define SIGNAL_INFO_ZERO_SUPPRESS         0x01
#define SIGNAL_INFO_NO_INDEX_LIST         0x02
#define SIGNAL_INFO_ZERO_SUPPRESS_MULTI   0x04

class signal_info
  : public signal_spec_base
{
public:
  virtual ~signal_info() { }

public:
  signal_info(const file_line &loc,
	      int signal_count,
	      int sticky,
	      const char *name,
	      int info,
	      int multi_size = -1)
    : signal_spec_base(loc,signal_count,sticky,name)
  {
    _info = info;
    _multi_size = multi_size;
  }

public:
  int    _info;
  int    _multi_size;

public:
  virtual void dump(dumper &d) const;
};


void dissect_name(const signal_spec *s,
		  const char *name,
		  signal_id& id);

void insert_signal_to_all(signal_spec *signal);
void expand_insert_signal_to_all(signal_spec_range *signal);

const char *data_item_size_to_signal_type(int sz);

const char *add_index_to_identifier(const char *ident,
				    int index);

#endif//__SIGNAL_HH__
