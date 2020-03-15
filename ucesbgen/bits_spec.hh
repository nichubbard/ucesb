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

#ifndef __BITS_SPEC_HH__
#define __BITS_SPEC_HH__

#include "file_line.hh"
#include <set>

struct int_range
{
public:
  int _min;              /* integer range min */
  int _max;              /* integer range max */

public:
  bool operator<(const int_range &rhs) const
  {
    // We are only less if we're not overlapping
    return _max < rhs._min;
  }
  /*
  bool overlap(const int_range &rhs) const
  {
    return _min <= rhs._max && _max >= rhs._min;
  }
  */
};

#include "variable.hh"


class bits_condition
{
public:
  bits_condition(const file_line &loc)
  {
    _loc = loc;
  }

public:
  virtual ~bits_condition() { }

public:
  virtual void dump(dumper &d) const = 0;
  virtual void check(dumper &d,const char *prefix,const char *name,
		     const char *jump_target = NULL) const = 0;

  virtual void get_match(dumper &d,const arguments *args,
			 uint32 *fixed_mask,uint32 *fixed_value) const = 0;

public:
  file_line _loc;
};

class bits_cond_check
  : public bits_condition
{
public:
  bits_cond_check(const file_line &loc,
		  const variable *chk)
    : bits_condition(loc)
  {
    _check = chk;
  }

public:
  const variable *_check;

public:
  virtual void dump(dumper &d) const;
  virtual void check(dumper &d,const char *prefix,const char *name,
		     const char *jump_target = NULL) const;
  virtual void get_match(dumper &d,const arguments *args,
			 uint32 *fixed_mask,uint32 *fixed_value) const;
};

class bits_cond_match
  : public bits_cond_check
{
public:
  bits_cond_match(const file_line &loc,
		  const variable *match)
    : bits_cond_check(loc,match)
  {

  }


public:
  virtual void dump(dumper &d) const;
};

class bits_cond_range
  : public bits_condition
{
public:
  bits_cond_range(const file_line &loc,
		  const variable *min,
		  const variable *max)
    : bits_condition(loc)
  {
    _min = min;
    _max = max;
  }

public:
  const variable *_min;
  const variable *_max;


public:
  virtual void dump(dumper &d) const;
  virtual void check(dumper &d,const char *prefix,const char *name,
		     const char *jump_target = NULL) const;
  virtual void get_match(dumper &d,const arguments *args,
			 uint32 *fixed_mask,uint32 *fixed_value) const;
};


class bits_cond_check_count
  : public bits_condition
{
public:
  bits_cond_check_count(const file_line &loc,
			const char *start,
			const char *stop,
			const variable *offset,
			const variable *multiplier)
    : bits_condition(loc)
  {
    _start = start;
    _stop  = stop;
    _offset     = offset;
    _multiplier = multiplier;
  }

public:
  const char     *_start;
  const char     *_stop;
  const variable *_offset;
  const variable *_multiplier;

public:
  virtual void dump(dumper &d,const variable *name) const;

  virtual void check(dumper &d,const char *prefix,
		     const char *name,const variable *var,
		     const char *jump_target) const;

public:
  virtual void dump(dumper &d) const;
  virtual void check(dumper &d,const char *prefix,const char *name,
		     const char *jump_target = NULL) const;
  virtual void get_match(dumper &d,const arguments *args,
			 uint32 *fixed_mask,uint32 *fixed_value) const;
};




class bits_spec
  : public int_range
{
public:
  bits_spec(int_range &bits,const char *name,const bits_condition *cond)
    : int_range(bits)
  {
    _name = name;
    _cond = cond;
  }

public:
  const char*    _name;
  const bits_condition* _cond;

public:
  void dump(dumper &d) const;
};

#include "node.hh"

typedef std::set<bits_spec*,compare_ptr<bits_spec> > bits_spec_list;

bool bits_spec_has_ident(const bits_spec_list *list,const char *name);

class prefix_ident
{
public:
  const bits_spec_list *_list;
  const char *_prefix;

public:
  const char *get_prefix(const char *name) const;
};

#endif//__BITS_SPEC_HH__
