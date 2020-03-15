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

#ifndef __CORRELATION_HH__
#define __CORRELATION_HH__

#include "multi_info.hh"

#include "config.hh"

class unpack_event;
class unpack_sticky_event;

void correlation_init(const config_command_vect &commands);
void correlation_exit();
void correlation_event(unpack_event *unpack_ev
		       WATCH_MEMBERS_PARAM);
void correlation_event(unpack_sticky_event *unpack_ev
		       WATCH_MEMBERS_PARAM);

void correlation_one_event(WATCH_MEMBERS_SINGLE_PARAM);

#endif//__CORRELATION_HH__
