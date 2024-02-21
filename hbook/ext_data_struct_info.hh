/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#ifndef __EXT_DATA_STRUCT_INFO_HH__
#define __EXT_DATA_STRUCT_INFO_HH__

class ext_data_clnt;
class ext_data_clnt_stderr;

struct ext_data_structure_info;

class ext_data_struct_info
{
public:
  ext_data_struct_info();
  ~ext_data_struct_info();

protected:
  void *_info; // void such that cint does not care about the type

  friend class ext_data_clnt;
  friend class ext_data_clnt_stderr;

public:
  // Giving this away is necessary due to the EXT_STR_ITEM_... macros.
  // Those macros in turn benefit from being macros (and not functions),
  // as they wrap some use of offsetof and sizeof.
  operator ext_data_structure_info*();

};

#endif//__EXT_DATA_STRUCT_HH__
