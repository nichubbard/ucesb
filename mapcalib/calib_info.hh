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

#ifndef __CALIB_INFO_HH__
#define __CALIB_INFO_HH__

#include "map_info.hh"
#include "../common/prefix_unit.hh"

#include <vector>

struct double_unit
{
public:
  double                _value;
  const units_exponent *_unit;
};

typedef std::vector<double_unit> vect_double_unit;

#define CALIB_TYPE_SLOPE              1
#define CALIB_TYPE_OFFSET             2
#define CALIB_TYPE_SLOPE_OFFSET       3
#define CALIB_TYPE_OFFSET_SLOPE       4
#define CALIB_TYPE_CUT_BELOW_OR_EQUAL 5

struct calib_param
  : public map_info
{
public:
  virtual ~calib_param() { }

public:
  calib_param(const file_line &loc,
	      const signal_id_info *src,const signal_id_info *dest,
	      int type,vect_double_unit *param,
	      int toggle_i)
    : map_info(loc,src,dest,NULL,NULL,0,toggle_i/*0*/)
  {
    _type   = type;
    _param  = param;

    /* _toggle_i = toggle_i; */
  }

public:
  int               _type;
  vect_double_unit *_param;

  /* TODO: there is also a map_info member with same name...? */
  /* int               _toggle_i; */

};

struct user_calib_param
  : public map_info // src not USED
{
public:
  virtual ~user_calib_param() { }

public:
  user_calib_param(const file_line &loc,
		   const signal_id_info *dest,
		   vect_double_unit *param)
    : map_info(loc,NULL,dest,NULL,NULL,0,0)
  {
    _param  = param;
  }

public:
  vect_double_unit *_param;

};

int calib_param_type(const char *type);

#endif//__CALIB_INFO_HH__
