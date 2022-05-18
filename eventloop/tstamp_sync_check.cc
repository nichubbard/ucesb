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
#include "error.hh"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

tstamp_sync_check::tstamp_sync_check(char const *command)
{
  _ref_id = 0x10;

  _prev_timestamp = 0;

  _num_items = 0;
  _num_alloc = 4096;

  _list = new tstamp_sync_info[_num_alloc];

  if (!_list)
    ERROR("Memory allocation failure!");
}

void tstamp_sync_check::dump(tstamp_sync_info &ts_sync_info)
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

void tstamp_sync_check::analyse(bool to_end)
{
  size_t analyse_end;
  size_t i;

  /* Figure out how far to analyse. */
  if (to_end)
    analyse_end = _num_items;
  else
    {
      ssize_t last_i;
      ssize_t prev_i;
      uint64_t max_diff = 0;

      /* Find the two last reference values. */
      /* Find the largest timestamp difference between any items
       * between these two values.
       */

      for (last_i = _num_items-1; last_i >= 0; last_i--)
	if (_list[last_i]._id == _ref_id)
	  break;

      for (prev_i = last_i-1; prev_i >= 0; prev_i--)
	if (_list[prev_i]._id == _ref_id)
	  break;

      if (prev_i < 0)
	prev_i = 0;

      analyse_end = 0;

      for (i = prev_i ; (ssize_t) i < last_i; i++)
	{
	  uint64_t diff = 0;

	  if (_list[i+1]._timestamp >= _list[i]._timestamp)
	    diff = _list[i+1]._timestamp - _list[i]._timestamp;

	  if (diff >= max_diff)
	    {
	      max_diff = diff;
	      analyse_end = i+1;
	    }
	}

      /*
      printf ("%zd %zd %zd -----------------\n",
	      prev_i, last_i, analyse_end);
      */

      /* This typically does not happen.  To ensure progress. */
      if (analyse_end <= 0)
	analyse_end = _num_items / 2;
    }






  /* Dump the items. */
  for (i = 0; i < analyse_end; i++)
    {
      dump(_list[i]);
    }

  /* Copy the unused items. */
  for (i = 0; analyse_end < _num_items; i++, analyse_end++)
    _list[i] = _list[analyse_end];

  _num_items = i;
}



void tstamp_sync_check::account(tstamp_sync_info &ts_sync_info)
{
  if (_num_items >= _num_alloc)
    analyse(false);

  /* That should have free'd up some items. */
  assert(_num_items < _num_alloc);

  /* Store the most recent item. */
  _list[_num_items++] = ts_sync_info;
}

void tstamp_sync_check::show()
{
  analyse(true);
}
