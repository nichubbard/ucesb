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

#ifndef __EXTERNAL_DATA_HH__
#define __EXTERNAL_DATA_HH__

#include "data_src.hh"
#include "dummy_external.hh"

#include "data_src_force_impl.hh"

/*---------------------------------------------------------------------------*/

#define GET_BUFFER_UINT16(dest) {                         \
  if (!__buffer.get_uint16(&dest)) {                      \
    ERROR("Error while reading " #dest " from buffer.");  \
  }                                                       \
}

#define GET_BUFFER_UINT32(dest) {                         \
  if (!__buffer.get_uint32(&dest)) {                      \
    ERROR("Error while reading " #dest " from buffer.");  \
  }                                                       \
}

/*---------------------------------------------------------------------------*/

#define EXT_DECL_DATA_SRC_FCN(returns,fcn_name) \
template<typename __data_src_t> \
returns fcn_name(__data_src_t &__buffer)

#define EXT_DECL_DATA_SRC_FCN_ARG(returns,fcn_name,...) \
template<typename __data_src_t> \
returns fcn_name(__data_src_t &__buffer,__VA_ARGS__)

#define EXT_DECL_UNPACK()        EXT_DECL_DATA_SRC_FCN(void,__unpack)
#define EXT_DECL_MATCH()         EXT_DECL_DATA_SRC_FCN(static bool,__match)
#define EXT_DECL_UNPACK_ARG(...) EXT_DECL_DATA_SRC_FCN_ARG(void,__unpack,__VA_ARGS__)
#define EXT_DECL_MATCH_ARG(...)  EXT_DECL_DATA_SRC_FCN_ARG(static bool,__match,__VA_ARGS__)

#define EXT_FORCE_IMPL_DATA_SRC_FCN(returns,fcn_name) \
  FORCE_IMPL_DATA_SRC_FCN(returns,fcn_name)
#define EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(returns,fcn_name,...) \
  FORCE_IMPL_DATA_SRC_FCN_ARG(returns,fcn_name,__VA_ARGS__)

/*---------------------------------------------------------------------------*/

DUMMY_EXTERNAL_MAP_STRUCT_FORW(EXTERNAL_DATA_SKIP);

class EXTERNAL_DATA_SKIP
{
public:

public:
  void __clean();
  EXT_DECL_UNPACK_ARG(size_t length = (size_t) -1/*_ARG:any arguments*/);
  // Needed if it is part of a select statement,
  // or first member of a structure
  EXT_DECL_MATCH_ARG(size_t length = (size_t) -1/*_ARG:any arguments*/)
  { return true; }

public:
  DUMMY_EXTERNAL_DUMP(EXTERNAL_DATA_SKIP);
  // DUMMY_EXTERNAL_MAP_MEMBERS(EXTERNAL_DATA_SKIP); moved to .._map class
  DUMMY_EXTERNAL_SHOW_MEMBERS(EXTERNAL_DATA_SKIP);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(EXTERNAL_DATA_SKIP);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(EXTERNAL_DATA_SKIP);
};

DUMMY_EXTERNAL_MAP_STRUCT(EXTERNAL_DATA_SKIP);
DUMMY_EXTERNAL_WATCHER_STRUCT(EXTERNAL_DATA_SKIP);
DUMMY_EXTERNAL_CORRELATION_STRUCT(EXTERNAL_DATA_SKIP);

/*---------------------------------------------------------------------------*/

DUMMY_EXTERNAL_MAP_STRUCT_FORW(EXTERNAL_DATA_SKIP);

class EXTERNAL_DATA_INFO
{
public:
  EXTERNAL_DATA_INFO();

public:
  char   *_data;   // pointing into original data
  size_t  _length;
  bool    _swapping;

public:
  void __clean();
  EXT_DECL_UNPACK(/*_ARG:any arguments*/);
  // Needed if it is part of a select statement,
  // or first member of a structure
  EXT_DECL_MATCH(/*_ARG:any arguments*/)
  { return true; }

public:
  DUMMY_EXTERNAL_DUMP(EXTERNAL_DATA_INFO);
  // DUMMY_EXTERNAL_MAP_MEMBERS(EXTERNAL_DATA_INFO); moved to .._map class
  DUMMY_EXTERNAL_SHOW_MEMBERS(EXTERNAL_DATA_INFO);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(EXTERNAL_DATA_INFO);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(EXTERNAL_DATA_INFO);
};

DUMMY_EXTERNAL_MAP_STRUCT(EXTERNAL_DATA_INFO);
DUMMY_EXTERNAL_WATCHER_STRUCT(EXTERNAL_DATA_INFO);
DUMMY_EXTERNAL_CORRELATION_STRUCT(EXTERNAL_DATA_INFO);

/*---------------------------------------------------------------------------*/

DUMMY_EXTERNAL_MAP_STRUCT_FORW(EXTERNAL_DATA16);

class EXTERNAL_DATA16
{
public:
  EXTERNAL_DATA16();
  ~EXTERNAL_DATA16();

public:
  uint16 *_data;
  size_t  _length;
  size_t  _alloc;

public:
  void __clean();
  EXT_DECL_UNPACK_ARG(size_t length = (size_t) -1/*_ARG:any arguments*/);
  // Needed if it is part of a select statement,
  // or first member of a structure
  EXT_DECL_MATCH_ARG(size_t length = (size_t) -1/*_ARG:any arguments*/)
  { return true; }

public:
  DUMMY_EXTERNAL_DUMP(EXTERNAL_DATA16);
  // DUMMY_EXTERNAL_MAP_MEMBERS(EXTERNAL_DATA16); moved to .._map class
  DUMMY_EXTERNAL_SHOW_MEMBERS(EXTERNAL_DATA16);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(EXTERNAL_DATA16);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(EXTERNAL_DATA16);
};

DUMMY_EXTERNAL_MAP_STRUCT(EXTERNAL_DATA16);
DUMMY_EXTERNAL_WATCHER_STRUCT(EXTERNAL_DATA16);
DUMMY_EXTERNAL_CORRELATION_STRUCT(EXTERNAL_DATA16);

/*---------------------------------------------------------------------------*/

DUMMY_EXTERNAL_MAP_STRUCT_FORW(EXTERNAL_DATA32);

class EXTERNAL_DATA32
{
public:
  EXTERNAL_DATA32();
  ~EXTERNAL_DATA32();

public:
  uint32 *_data;
  size_t  _length;
  size_t  _alloc;

public:
  void __clean();
  EXT_DECL_UNPACK_ARG(size_t length = (size_t) -1/*_ARG:any arguments*/);
  // Needed if it is part of a select statement,
  // or first member of a structure
  EXT_DECL_MATCH_ARG(size_t length = (size_t) -1/*_ARG:any arguments*/)
  { return true; }

public:
  DUMMY_EXTERNAL_DUMP(EXTERNAL_DATA32);
  // DUMMY_EXTERNAL_MAP_MEMBERS(EXTERNAL_DATA32); moved to .._map class
  DUMMY_EXTERNAL_SHOW_MEMBERS(EXTERNAL_DATA32);
  DUMMY_EXTERNAL_ENUMERATE_MEMBERS(EXTERNAL_DATA32);
  DUMMY_EXTERNAL_ZERO_SUPPRESS_INFO_PTRS(EXTERNAL_DATA32);
};

DUMMY_EXTERNAL_MAP_STRUCT(EXTERNAL_DATA32);
DUMMY_EXTERNAL_WATCHER_STRUCT(EXTERNAL_DATA32);
DUMMY_EXTERNAL_CORRELATION_STRUCT(EXTERNAL_DATA32);

/*---------------------------------------------------------------------------*/

#endif//__EXTERNAL_DATA_HH__
