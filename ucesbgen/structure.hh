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

#ifndef __STRUCTURE_HH__
#define __STRUCTURE_HH__

#include "node.hh"
#include "param_arg.hh"
#include "variable.hh"
#include "bits_spec.hh"
#include "encode.hh"
#include "dumper.hh"

#include "file_line.hh"

class struct_item
{
public:
  struct_item(const file_line &loc)
  {
    _loc = loc;
  }

public:
  virtual ~struct_item() { }

public:
  virtual void dump(dumper &d,bool recursive = true) const = 0;

public:
  file_line _loc;

};

typedef std::vector<struct_item*> struct_item_list;

#define SD_FLAGS_NOENCODE 0x0001
#define SD_FLAGS_OPTIONAL 0x0002
#define SD_FLAGS_SEVERAL  0x0004

class struct_data
  : public struct_item
{
public:
  virtual ~struct_data() { }

public:
  struct_data(const file_line &loc,
	      int size,
	      const char *ident,
	      int flags,
	      const bits_spec_list *bits,
	      const encode_spec_list *encode)
    : struct_item(loc)
  {
    _size  = size;
    _ident = ident;
    _flags = flags;
    _bits  = bits;
    _encode = encode;

    check_bits();
  }

public:
  int                     _size;
  const char             *_ident;
  int                     _flags;
  const bits_spec_list   *_bits;
  const encode_spec_list *_encode;

protected:
  void check_bits() const;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

#define STRUCT_DECL_OPTS_MULTI      0x01
#define STRUCT_DECL_OPTS_EXTERNAL   0x02
#define STRUCT_DECL_OPTS_REVISIT    0x04
#define STRUCT_DECL_OPTS_NO_REVISIT 0x08
#define EVENT_OPTS_IGNORE_UNKNOWN_SUBEVENT  0x10
#define EVENT_OPTS_STICKY           0x20
#define EVENT_OPTS_INTENTIONALLY_EMPTY 0x40

class struct_decl
  : public struct_item
{
public:
  virtual ~struct_decl() { }

public:
  struct_decl(const file_line &loc,
	      const var_name *name,
	      const char *ident,
	      const argument_list *args,
	      int opts)
    : struct_item(loc)
  {
    _name  = name;
    _ident = ident;
    _args  = args;
    _opts  = opts;
  }

public:
  const var_name      *_name;
  const char          *_ident;
  const argument_list *_args;
  int                  _opts;

public:
  void check_valid_opts(int valid_opts);

  int is_event_opt();

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

typedef std::vector<struct_decl*> struct_decl_list;

void check_valid_opts(struct_decl_list *list,int valid_opts);

class struct_list
  : public struct_item
{
public:
  virtual ~struct_list() { }

public:
  struct_list(const file_line &loc,
	      const var_name *index,
	      const variable *min,
	      const variable *max,
	      const struct_item_list *items)
    : struct_item(loc)
  {
    _index = index;
    _min   = min;
    _max   = max;
    _items = items;
  }

public:
  const var_name *_index;
  const variable *_min;
  const variable *_max;
  const struct_item_list *_items;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

// flags 0 =                   // exactly 1 item
#define SS_FLAGS_OPTIONAL 0x01 // 0 or 1 items
#define SS_FLAGS_SEVERAL  0x02 // 0 or more items

class struct_select
  : public struct_item
{
public:
  virtual ~struct_select() { }

public:
  struct_select(const file_line &loc,
		const struct_decl_list *items,
		int flags)
    : struct_item(loc)
  {
    _items = items;
    _flags = flags;
  }

public:
  const struct_decl_list *_items;
  int                     _flags;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

#if 0
class struct_several
  : public struct_item
{
public:
  virtual ~struct_several() { }

public:
  struct_several(const struct_item *item)
  {
    _item = item;
  }

public:
  const struct_item *_item;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};
#endif

class struct_cond
  : public struct_item
{
public:
  virtual ~struct_cond() { }

public:
  struct_cond(const file_line &loc,
	      const variable *expr,
	      const struct_item_list *items,
	      const struct_item_list *items_else)
    : struct_item(loc)
  {
    _expr = expr;
    _items = items;
    _items_else = items_else;
  }

public:
  const variable         *_expr;
  const struct_item_list *_items;
  const struct_item_list *_items_else;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

#define SM_FLAGS_ZERO_SUPPRESS      0x0001
#define SM_FLAGS_NO_INDEX           0x0002
#define SM_FLAGS_LIST               0x0004
#define SM_FLAGS_MULTI              0x0008

#define SM_FLAGS_ZERO_SUPPRESS_LIST  (SM_FLAGS_ZERO_SUPPRESS | SM_FLAGS_LIST)
#define SM_FLAGS_NO_INDEX_LIST       (SM_FLAGS_NO_INDEX | SM_FLAGS_LIST)
#define SM_FLAGS_ZERO_SUPPRESS_MULTI (SM_FLAGS_ZERO_SUPPRESS | SM_FLAGS_MULTI)

#define SM_FLAGS_IS_ARGUMENT        0x0010

struct sm_flags
{
public:
  sm_flags(const sm_flags &src)
  {
    _flags      = src._flags;
    _multi_size = src._multi_size;
  }

  sm_flags(int flags,int multi_size = -1)
  {
    _flags = flags;
    _multi_size = multi_size;
  }

public:
  int _flags;
  int _multi_size; // if SM_FLAGS_MULTI
};

class struct_member
  : public struct_item
{
public:
  virtual ~struct_member() { }

public:
  struct_member(const file_line &loc,
		const char *name,
		const var_name *ident,
		sm_flags *flags)
    : struct_item(loc) , _flags(*flags)
  {
    _name  = name;
    _ident = ident;
  }

public:
  const char             *_name;
  const var_name         *_ident;
  sm_flags                _flags;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

#define STRUCT_MARK_FLAGS_GLOBAL    0x01

class struct_mark
  : public struct_item
{
public:
  virtual ~struct_mark() { }

public:
  struct_mark(const file_line &loc,
	      const char *name,
	      int flags)
    : struct_item(loc)
  {
    _name  = name;
    _flags = flags;
  }

public:
  const char             *_name;
  int                     _flags;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

class struct_check_count
  : public struct_item
{
public:
  virtual ~struct_check_count() { }

public:
  struct_check_count(const file_line &loc,
		     const variable *var,
		     const char *start,
		     const char *stop,
		     const variable *offset,
		     const variable *multiplier)
    : struct_item(loc)
    , _check(loc,start,stop,offset,multiplier)
  {
    _var = var;
  }

public:
  const variable        *_var;
  bits_cond_check_count  _check;

public:
  virtual void dump(dumper &d,bool recursive = true) const;

};

class struct_match_end
  : public struct_item
{
public:
  virtual ~struct_match_end() { }

public:
  struct_match_end(const file_line &loc)
    : struct_item(loc)
  {
  }

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

class struct_encode
  : public struct_item
{
public:
  virtual ~struct_encode() { }

public:
  struct_encode(const file_line &loc,
		encode_spec *encode)
    : struct_item(loc)
  {
    _encode = encode;
  }

public:
  encode_spec *_encode;

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

class struct_header
{
public:
  virtual ~struct_header() { }

public:
  struct_header(const char *name)
  {
    _name = name;
  }

public:
  const char       *_name;

public:
  virtual void dump(dumper &d) const = 0;
};

class struct_header_named
  : public struct_header
{
public:
  virtual ~struct_header_named() { }

public:
  struct_header_named(const char       *name,
		      const param_list *params)
    : struct_header(name)
  {
    _params = params;
  }

public:
  const param_list *_params;

public:
  virtual void dump(dumper &d) const;
};

class struct_header_subevent
  : public struct_header_named
{
public:
  virtual ~struct_header_subevent() { }

public:
  struct_header_subevent(const char *name,
			 const param_list *params)
    : struct_header_named(name,params)
  {
  }

public:
  virtual void dump(dumper &d) const;
};



class struct_unpack_code;

class str_ev_definition
  : public def_node
{
public:
  virtual ~str_ev_definition() { }

public:
  str_ev_definition(const file_line &loc)
  {
    _code = NULL;

    _loc = loc;
  }

public:
  struct_unpack_code *_code;

public:
  file_line _loc;

};

#define STRUCT_DEF_OPTS_EXTERNAL      0x02
#define STRUCT_DEF_OPTS_HAS_MATCH_END 0x04
#define STRUCT_DEF_OPTS_STICKY_SUBEV  0x08

class struct_definition
  : public str_ev_definition
{
public:
  virtual ~struct_definition() { }

public:
  struct_definition(const file_line &loc,
		    const struct_header *header,
		    const struct_item_list *body,
		    int opts = 0)
    : str_ev_definition(loc)
  {
    _header = header;
    _body = body;
    _opts = opts;
  }

public:
  const struct_header    *_header;
  const struct_item_list *_body;
  int                     _opts;

public:
  virtual void dump(dumper &d,bool recursive = true) const;

public:
  void find_member_args();
};

class event_definition
  : public str_ev_definition
{
public:
  virtual ~event_definition() { }

public:
  event_definition(const file_line &loc,
		   const struct_decl_list *items)
    : str_ev_definition(loc)
  {
    _items = items;
    _opts = 0;
  }

public:
  const struct_decl_list *_items;
  int                     _opts; // EVENT_OPTS_IGNORE_UNKNOWN_SUBEVENT

public:
  virtual void dump(dumper &d,bool recursive = true) const;
};

#endif//__STRUCTURE_HH__
