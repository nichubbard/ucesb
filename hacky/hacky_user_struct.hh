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

#ifndef __HACKY_USER_STRUCT_HH__
#define __HACKY_USER_STRUCT_HH__

// Careful.  These structures are also parsed by psdc in order to
// prepare for enumeration for ntuple writing.  I.e. only a small
// subset of C struct features are allowed!

#ifndef __PSDC__
#include "zero_suppress.hh"
#endif

struct CROS3_wire_hit
{
  uint16 wire;
  uint16 start;

  USER_STRUCT_FCNS_DECL;
};

struct CROS3_hits
{
  // to handle all possible data, it should be 256*128 (0x8000),
  // ntuple has other limits, so lets say four hits per wire

  raw_list_zero_suppress<CROS3_wire_hit,CROS3_wire_hit,0x0400> hits;

  USER_STRUCT_FCNS_DECL;
};

struct hacky_user_struct
{
  CROS3_hits ccb[2];

  USER_STRUCT_FCNS_DECL;
};

#endif//__HACKY_USER_STRUCT_HH__

