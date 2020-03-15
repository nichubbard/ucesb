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

#ifndef __MAP_INFO_HH__
#define __MAP_INFO_HH__

#include "../common/node.hh"

#include "signal_id_map.hh"

#include "file_line.hh"

struct map_info
  : public def_node
{
public:
  virtual ~map_info() { }

public:
  map_info(const file_line &loc,
	   const signal_id_info *src,const signal_id_info *dest,
	   const signal_id_info *rev_src,const signal_id_info *rev_dest,
	   int sticky,
	   int toggle_i)
  {
    _loc  = loc;

    _src  = src;
    _dest = dest;

    _rev_src  = rev_src;
    _rev_dest = rev_dest;

    _sticky = sticky;
    _toggle_i = toggle_i;
  }

public:
  const signal_id_info   *_src;
  const signal_id_info   *_dest;

  const signal_id_info   *_rev_src;
  const signal_id_info   *_rev_dest;

  int                     _sticky;
  int                     _toggle_i;

public:
  file_line _loc;
};

int data_type(const char *type);

#endif//__MAP_INFO_HH__
