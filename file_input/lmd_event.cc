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

#include "lmd_event.hh"

#include "util.hh"

#include <string.h>

///////////////////////////////////////////////////////
// swap between little and big endian formats

void byteswap_32(s_bufhe_host& buf_header)
{
  // byte swap members of s_bufhe struct

  byteswap ((uint32*) &buf_header,sizeof (buf_header));
}

#define mymin(a,b) ((a)<(b)?(a):(b))

void byteswap_32(s_filhe_extra_host& file_extra)
{
  // byte swap members of s_filhe struct

  bswap_16(file_extra.filhe_label_l);
  bswap_16(file_extra.filhe_file_l);
  bswap_16(file_extra.filhe_user_l);
  bswap_16(file_extra.filhe_run_l);
  bswap_16(file_extra.filhe_exp_l);
  bswap_32(file_extra.filhe_lines);

  // But only as many as are used.

  for (uint i = 0; i < mymin(countof(file_extra.s_strings),
			     file_extra.filhe_lines); i++)
    bswap_16(file_extra.s_strings[i].string_l);
}

#define ZERO_PADDING(str) do {			\
    if (str##_l > sizeof (str))			\
      str##_l = sizeof (str); /* normalize */	\
    if (str##_l < sizeof (str))			\
      memset(str+str##_l,0,sizeof (str)-str##_l);	\
  } while (0)

void zero_padding(s_filhe_extra_host &file_extra)
{
  ZERO_PADDING(file_extra.filhe_label);
  ZERO_PADDING(file_extra.filhe_file);
  ZERO_PADDING(file_extra.filhe_user);
  ZERO_PADDING(file_extra.filhe_run);
  ZERO_PADDING(file_extra.filhe_exp);

  if (file_extra.filhe_lines > countof(file_extra.s_strings))
    file_extra.filhe_lines = (uint32) countof(file_extra.s_strings);

  for (uint i = 0; i < countof(file_extra.s_strings); i++)
    ZERO_PADDING(file_extra.s_strings[i].string);

  for (uint i = file_extra.filhe_lines; i < countof(file_extra.s_strings); i++)
    memset(&file_extra.s_strings[i],0,sizeof(file_extra.s_strings[i]));
}
