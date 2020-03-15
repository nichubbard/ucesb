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

#ifndef __WATCHER_HH__
#define __WATCHER_HH__

#ifdef USE_CURSES

#include "multi_info.hh"

class unpack_event;
class unpack_sticky_event;

void watcher_init(const char *command);
void watcher_event();

void watcher_one_event(unpack_event *unpack_ev
		       WATCH_MEMBERS_PARAM);
void watcher_one_event(unpack_sticky_event *unpack_ev
		       WATCH_MEMBERS_PARAM);

#endif

#endif//__WATCHER_HH__
