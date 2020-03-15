// -*- C++ -*-

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

EVENT
{
  ignore_unknown_subevent;
}


// Before 'ignore_unknown_subevent' was implemented, all
// this was needed...  (along with the now removed files
// empty_external.hh:  #include "external_data.hh"
// control.hh:         #define USER_EXTERNAL_UNPACK_STRUCT_FILE \
//                     "empty_external.hh"

/*

external EXTERNAL_DATA_SKIP();

SUBEVENT(ANY_SUBEVENT)
{
  // We'll skip the entire subevent...

  external skip = EXTERNAL_DATA_SKIP();
}

EVENT
{
  // handle any subevent
  // allow it several times

  revisit any = ANY_SUBEVENT();
}

*/
