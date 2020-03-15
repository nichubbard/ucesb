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

#ifndef __VARIABLE_HH__
#define __VARIABLE_HH__

#include "dumper.hh"

#include "file_line.hh"

typedef unsigned long long int uint64;

typedef unsigned int   uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;

class prefix_ident;

struct arguments;

struct const32
{
public:
  const32(unsigned int value)
  {
    _value = value;
  }

public:
  unsigned int _value;

public:
  void dump(dumper &d) const;
};

class variable
{
public:
  variable(const file_line &loc)
  {
    _loc = loc;
  }
public:
  virtual ~variable() { }

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const = 0;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const = 0;

public:
  void dump_bracket(dumper &d,const prefix_ident* prefix = NULL);

public:
  file_line _loc;
};

class var_name
  : public variable
{
public:
  virtual ~var_name() { }

public:
  var_name(const file_line &loc,
	   const char *name)
    : variable(loc)
  {
    _name = name;
  }

public:
  const char *_name;

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const;
};

class var_sub
  : public var_name
{
public:
  virtual ~var_sub() { }

public:
  var_sub(const file_line &loc,
	  const char *parent,const var_name *child,int index = -1) :
    var_name(loc, parent)
  {
    _index  = index;
    _child  = child;
  }

public:
  int _index;

  const var_name *_child;

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const;
};

class var_index
  : public var_name
{
public:
  virtual ~var_index() { }

public:
  var_index(const file_line &loc,
	    const char *name,int index,int index2 = -1) :
    var_name(loc, name)
  {
    _index  = index;
    _index2 = index2;
  }

public:
  int _index;
  int _index2;

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const;
};

class var_indexed
  : public var_name
{
public:
  virtual ~var_indexed() { }

public:
  var_indexed(const file_line &loc,
	      const char *name,variable *index,variable *index2 = NULL) :
    var_name(loc, name)
  {
    _index  = index;
    _index2 = index2;
  }

public:
  variable *_index;
  variable *_index2;

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const;
};

class var_const
  : public variable
{
public:
  virtual ~var_const() { }

public:
  var_const(const file_line &loc,
	    const32 value)
    : variable(loc)
    , _value(value)
  {
  }

public:
  const32 _value;

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const;
};

class var_external
  : public variable
{
public:
  virtual ~var_external() { }

public:
  var_external(const file_line &loc,
	       const char *name)
    : variable(loc)
  {
    _name = name;
  }

public:
  const char *_name;

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const;
};

#define VAR_OP_NEG        1
#define VAR_OP_NOT        2
#define VAR_OP_LNOT       3  // !
#define VAR_OP_LOR        4  // ||
#define VAR_OP_LAND       5  // &&
#define VAR_OP_ADD        6
#define VAR_OP_SUB        7
#define VAR_OP_MUL        8
#define VAR_OP_DIV        9
#define VAR_OP_REM        10
#define VAR_OP_AND        11
#define VAR_OP_OR         12
#define VAR_OP_XOR        13
#define VAR_OP_SHR        14
#define VAR_OP_SHL        15
#define VAR_OP_LS         16
#define VAR_OP_GR         17
#define VAR_OP_LE         18
#define VAR_OP_GE         19
#define VAR_OP_EQ         20
#define VAR_OP_NEQ        21

extern const char *var_expr_op_str[];

class var_expr
  : public variable
{
public:
  virtual ~var_expr() { }

public:
  var_expr(const file_line &loc,
	   variable *lhs,variable *rhs,int op)
    : variable(loc)
  {
    _lhs = lhs;
    _rhs = rhs;
    _op = op;
  }

public:
  int       _op;
  variable *_lhs;
  variable *_rhs;

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const;
};

class var_cast
  : public variable
{
public:
  virtual ~var_cast() { }

public:
  var_cast(const file_line &loc,
	   const char *type,variable *rhs)
    : variable(loc)
  {
    _type = type;
    _rhs = rhs;
  }

public:
  const char *_type;
  variable   *_rhs;

public:
  virtual void dump(dumper &d,const prefix_ident* prefix = NULL) const;
  virtual bool eval(/*dumper &d,*/const arguments *args,uint32 *val) const;
};

bool difference(const var_name *first,
		const var_name *last,
		int &diff_index,
		int &diff_length);

const var_name *generate_indexed(const var_name *first,
				 int diff_index,
				 int diff_offset);

#endif//__VARIABLE_HH__
