/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2022  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "tstamp_sync_check.hh"
#include "colourtext.hh"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

tstamp_sync_check::tstamp_sync_check(char const *command)
{
  _prev_timestamp = 0;
}

void tstamp_sync_check::account(tstamp_sync_info &ts_sync_info)
{
  uint64_t ts;
  
  if (ts_sync_info._timestamp != 0 &&
      ts_sync_info._timestamp - _prev_timestamp > 2000)
    {
      printf ("\n");
    }

  printf ("%s%-12u%s %s%02x%s  ",
	  CT_OUT(BOLD_BLUE),
	  ts_sync_info._event_no,
	  CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD),
	  ts_sync_info._id,
	  CT_OUT(NORM));

  ts = ts_sync_info._timestamp;

  printf ("%s%04x%s:%s%04x%s:%s%04x%s:%s%04x%s",
	  CT_OUT(BOLD_BLUE), (int) (ts >> 48) & 0xffff, CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD_BLUE), (int) (ts >> 32) & 0xffff, CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD_BLUE), (int) (ts >> 16) & 0xffff, CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD_BLUE), (int) (ts      ) & 0xffff, CT_OUT(NORM_DEF_COL));

  if (ts_sync_info._timestamp == 0)
    printf ("  %s%10s%s",
	    CT_OUT(BOLD_RED), "err", CT_OUT(NORM_DEF_COL));
  else
    printf ("  %s%10" PRIu64 "%s",
	    CT_OUT(BOLD), ts_sync_info._timestamp - _prev_timestamp,
	    /* */                                       CT_OUT(NORM));

  printf ("  %s[%x]%s %s%5d%s ",
	  CT_OUT(BOLD), ts_sync_info._sync_check_flags, CT_OUT(NORM),
	  CT_OUT(BOLD_MAGENTA), ts_sync_info._sync_check_value,
	  /*                                         */ CT_OUT(NORM_DEF_COL));

  printf ("\n");

  if (ts_sync_info._timestamp != 0)
    {
      _prev_timestamp = ts_sync_info._timestamp;
    }
}
