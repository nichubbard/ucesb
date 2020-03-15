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

#ifndef __CONTROL_HH__
#define __CONTROL_HH__

#ifdef UNPACKER_IS_xtst

#define INIT_USER_FUNCTION    init_function

#define STICKY_SUBEVENT_USER_FUNCTION    sticky_subevent_user_function

#define UNPACK_EVENT_USER_FUNCTION    user_function

#endif//UNPACKER_IS_xtst

#ifdef UNPACKER_IS_xtst_toggle

#define UNPACK_EVENT_USER_FUNCTION  user_function_multi

#define USING_MULTI_EVENTS 1

#endif//UNPACKER_IS_xtst_toggle

#endif//__CONTROL_HH__
