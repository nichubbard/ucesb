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

#ifndef __MULTI_INFO_HH__
#define __MULTI_INFO_HH__

#include "control_include.hh"

#if !USING_MULTI_EVENTS
#define MAP_MEMBERS_SINGLE_PARAM
#define MAP_MEMBERS_PARAM
#define MAP_MEMBERS_ARG

#define WATCH_MEMBERS_SINGLE_PARAM
#define WATCH_MEMBERS_PARAM
#define WATCH_MEMBERS_SINGLE_ARG
#define WATCH_MEMBERS_ARG
#endif//!USING_MULTI_EVENTS

#if USING_MULTI_EVENTS
struct map_members_info
{
  int _multi_event_no;
  int _event_type;
};

#define MAP_MEMBERS_SINGLE_PARAM    const map_members_info &info
#define MAP_MEMBERS_PARAM         , const map_members_info &info
#define MAP_MEMBERS_ARG           , info

#define WATCH_MEMBERS_SINGLE_PARAM    const map_members_info &info
#define WATCH_MEMBERS_PARAM         , const map_members_info &info
#define WATCH_MEMBERS_SINGLE_ARG      info
#define WATCH_MEMBERS_ARG           , info

#endif//USING_MULTI_EVENTS

#endif//__MULTI_INFO_HH__
