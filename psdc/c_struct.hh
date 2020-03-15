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

#ifndef __C_STRUCT_HH__
#define __C_STRUCT_HH__

#include "node.hh"
#include "dumper.hh"
#include "file_line.hh"
#include "prefix_unit.hh"

#include <assert.h>
#include <vector>
#include <stack>

class signal_id;
struct corr_items;

class c_struct_base
{
public:
  c_struct_base(const file_line &loc)
  {
    _loc = loc;
  }

public:
  virtual ~c_struct_base() { }

public:
  virtual void dump(dumper &d,bool recursive = true) const = 0;

public:
  file_line _loc;

};

class c_arg
  : public c_struct_base
{
public:
  c_arg(const file_line &loc)
    : c_struct_base(loc)
  {

  }

};

typedef std::vector<c_arg*> c_arg_list;

typedef std::vector<int> c_array_ind;

class c_arg_named
  : public c_arg
{
public:
  c_arg_named(const file_line &loc,
	      const char *name,
	      const c_array_ind *array_ind,
	      bool toggle)
    : c_arg(loc)
  {
    _name      = name;
    _array_ind = array_ind;
    _toggle    = toggle;
  }

public:
  const char *_name;
  const c_array_ind *_array_ind;
  bool        _toggle;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

class c_arg_const
  : public c_arg
{
public:
  c_arg_const(const file_line &loc,
	      const int value)
    : c_arg(loc)
  {
    _value = value;
  }

public:
  int         _value;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

class c_typename
  : public c_struct_base
{
public:
  c_typename(const file_line &loc,
	     const char *name)
    : c_struct_base(loc)
  {
    _name = name;
    _toggle = false;
  }

public:
  const char *_name;
  bool        _toggle;

public:
  void set_toggle() { _toggle = true; }

public:
  virtual void dump(dumper &d,bool recursive = true) const;

  virtual void mirror_struct(dumper &d,bool full_template = true) const;

public:
  virtual bool primitive_type() const;
};

class c_typename_template
  : public c_typename
{
public:
  c_typename_template(const file_line &loc,
		      const char *name,
		      const c_arg_list *args)
    : c_typename(loc,name)
  {
    _args = args;
  }

public:
  const c_arg_list *_args;

public:
  virtual void dump(dumper &d,bool recursive = true) const;

  virtual void mirror_struct(dumper &d,bool full_template) const;

public:
  virtual bool primitive_type() const;
};
/*
class c_bitfield_item
  : public c_struct_base
{
public:
  c_bitfield_item(const file_line &loc,
		  const char *type,
		  const char *ident,
		  int bits)
    : c_struct_base(loc)
  {
    _type = type;
    _ident = ident;
    _bits = bits;
  }

public:
  const char           *_type;
  const char           *_ident;
  int                   _bits;

public:
  virtual void dump(dumper &d,bool recursive = true) const;

};

typedef std::vector<c_bitfield_item*> c_bitfield_item_list;
*/

class c_struct_item
  : public c_struct_base
{
public:
  c_struct_item(const file_line &loc,
		const c_typename *type,
		const char *ident,
		const c_array_ind *array_ind,
		int bitfield)
    : c_struct_base(loc)
  {
    _type      = type;
    _ident     = ident;
    _array_ind = array_ind;
    _bitfield  = bitfield;
    _unit      = NULL;
  }

public:
  const c_typename  *_type;
  const char        *_ident;
  const c_array_ind *_array_ind;
  int                _bitfield;
  const char        *_unit;

public:
  void set_array_ind(const c_array_ind *array_ind)
  {
    assert(!_array_ind);
    _array_ind = array_ind;
  }

  void set_bitfield(int bits)
  {
    _bitfield = bits;
  }

  void set_unit(const char *unit)
  {
    _unit = unit;
  }

public:
  virtual void dump(dumper &d,bool recursive = true) const = 0;
  virtual void function_call(dumper &d,
			     int array_i_offset,
			     const char *array_prefix) const = 0;

  virtual void mirror_struct(dumper &d) const = 0;

  virtual void find_members(dumper &d,signal_id &id,
			    corr_items &items) const = 0;

  dumper *function_call_array_loop_open(std::stack<dumper *> &dumpers,
					dumper *ld,
					int array_i_offset,
					char **sub_array_index) const;
  void function_call_array_loop_close(std::stack<dumper *> &dumpers,
				      dumper *ld,
				      int array_i_offset) const;

};

typedef std::vector<c_struct_item*> c_struct_item_list;

class c_struct_member
  : public c_struct_item
{
public:
  c_struct_member(const file_line &loc,
		  const c_typename *type,
		  const char *ident,
		  const c_array_ind *array_ind,
		  int bitfield,
		  int multi_flag)
    : c_struct_item(loc,
		    type,ident,array_ind,bitfield)
  {
    _multi_flag = multi_flag;
  }

public:
  int _multi_flag;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
  virtual void function_call(dumper &d,
			     int array_i_offset,
			     const char *array_prefix) const;

  virtual void mirror_struct(dumper &d) const;

  virtual void find_members(dumper &d,signal_id &id,
			    corr_items &items) const;
};









typedef std::vector<const char*> c_base_class_list;

#define C_STR_STRUCT 0x01
#define C_STR_UNION  0x02
#define C_STR_CLASS  0x04

class c_struct_def
  : public def_node, public c_struct_item
{
public:
  c_struct_def(const file_line &loc,
	       int struct_type,
	       const c_typename *type,
	       const c_base_class_list* base_list,
	       const c_struct_item_list* items,
	       const char *ident,
	       const c_array_ind *array_ind)
    : c_struct_item(loc,
		    type,ident,array_ind,0)
  {
    _struct_type = struct_type;
    _base_list = base_list;
    _items = items;
  }

public:
  virtual ~c_struct_def() { }

public:
  int                       _struct_type; // struct/union/class
  const c_base_class_list*  _base_list;
  const c_struct_item_list* _items;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
  virtual void function_call(dumper &d,
			     int array_i_offset,
			     const char *array_prefix) const;

  virtual void mirror_struct(dumper &d) const;

  virtual void function_call_per_member(dumper &d) const;

  virtual void mirror_struct_decl(dumper &d) const;

  virtual void corr_struct(dumper &d) const;
  virtual void find_members(dumper &d,signal_id &id,
			    corr_items &items) const;
  void make_corr_struct(const char *type,
			dumper &d,
			corr_items &items,
			int mode) const;
};


/*
class c_struct_bitfield
  : public c_struct_def
{
public:
  c_struct_bitfield(const file_line &loc,
		    const c_typename *type,
		    const c_bitfield_item_list *items,
		    const char *ident)
    : c_struct_def(loc,
		   C_STR_STRUCT,
		   type,NULL,NULL,ident)
  {
    _items = items;
  }

public:
  const c_bitfield_item_list *_items;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
  virtual void function_call(dumper &d) const;

};
*/

////////////////////////////////////////////////////////////////////////

#include <map>
#include <vector>

typedef std::map<const char*,const c_struct_def*> named_c_struct_def_map;
typedef std::vector<const c_struct_def*> named_c_struct_def_vect;

extern named_c_struct_def_map  all_named_c_struct;
extern named_c_struct_def_vect all_named_c_struct_vect;

////////////////////////////////////////////////////////////////////////

void map_definitions();

void dump_definitions();

void mirror_struct();

void mirror_struct_decl();

void fcn_call_per_member();

#endif // __C_STRUCT_HH__
