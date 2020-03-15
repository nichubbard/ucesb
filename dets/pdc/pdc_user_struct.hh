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

#ifndef __PDC_USER_STRUCT_HH__
#define __PDC_USER_STRUCT_HH__

// Careful.  These structures are also parsed by psdc in order to
// prepare for enumeration for ntuple writing.  I.e. only a small
// subset of C struct features are allowed!

#ifndef __PSDC__
#include "zero_suppress.hh"
#endif

struct PDC_wire_hit
{
  uint8 wire;
  uint8 start;
  uint8 stop;

  USER_STRUCT_FCNS_DECL;
};

#define MAX_CROS3_hits 0x0400

struct PDC_hits
{
  // to handle all possible data, it should be 256*128 (0x8000),
  // ntuple has other limits, so lets say four hits per wire

  raw_list_ii_zero_suppress<PDC_wire_hit,PDC_wire_hit,MAX_CROS3_hits> hits;

  USER_STRUCT_FCNS_DECL;
};

#endif//__PDC_USER_STRUCT_HH__

