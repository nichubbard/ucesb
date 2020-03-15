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

#ifndef __EVENT_BASE_HH__
#define __EVENT_BASE_HH__

#include "gen/unpacker_defines.hh"
#include "structures.hh"
#include "hex_dump_mark.hh"

class event_base
{
public:
#if USE_THREADING || USE_MERGING
  // FILE_INPUT_EVENT *event;
  void        *_file_event;
#endif
  hex_dump_mark_buf _unpack_fail;
  unpack_event _unpack;
#ifndef USE_MERGING
  raw_event    _raw;
  cal_event    _cal;

#ifdef USER_STRUCT
  USER_STRUCT   _user;
#endif
#endif//!USE_MERGING

public:
  void raw_cal_user_clean();

public:
  bool is_sticky() { return false; }
};

class dummy_container
{
};

class sticky_event_base
{
public:
#if USE_THREADING || USE_MERGING
  // FILE_INPUT_EVENT *event;
  void        *_file_event;
#endif
  hex_dump_mark_buf _unpack_fail;
  unpack_sticky_event _unpack;
#ifndef USE_MERGING
  raw_sticky      _raw;
  dummy_container _cal;
  //raw_event    _raw;
  //cal_event    _cal;

#ifdef USER_STRUCT
  dummy_container _user;
  //USER_STRUCT   _user;
#endif
#endif//!USE_MERGING

public:
  void raw_cal_user_clean();

public:
  bool is_sticky() { return true; }
};

// The event data structure!

// This should not be used by any user code!
// Use CURRENT_EVENT instead!!
// Otherwise multi-threading/merging will not function properly!!!

extern event_base _static_event;

extern sticky_event_base _static_sticky_event;

// See user.hh for the following entry...

#ifdef CALIB_STRUCT
extern CALIB_STRUCT      _calib;
#endif

#endif//__EVENT_BASE_HH__
