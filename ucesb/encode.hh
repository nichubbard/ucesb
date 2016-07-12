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

#ifndef __ENCODE_HH__
#define __ENCODE_HH__

#include "file_line.hh"

#include "variable.hh"
#include "param_arg.hh"

#include <vector>

#define ES_APPEND_LIST 0x01

struct encode_spec
{
public:
  encode_spec(const file_line &loc,
	      const var_name *name,
	      const argument_list *args,
	      int flags)
  {
    _loc = loc;

    _name = name;
    _args = args;

    _flags = flags;
  }

public:
  virtual ~encode_spec() { }

public:
  virtual void dump(dumper &d) const;

public:
  const var_name *_name;
  const argument_list *_args;
  int _flags;

public:
  file_line _loc;
};

typedef std::vector<encode_spec*> encode_spec_list;

#endif//__ENCODE_HH__
