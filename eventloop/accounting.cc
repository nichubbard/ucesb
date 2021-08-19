/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2021  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "accounting.hh"
#include "colourtext.hh"

#include "gen/account_ids.hh"

#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>

uint64 _data_account[NUM_ACCOUNT_IDS];

void account_init()
{
  ssize_t i;

  for (i = 0; i < NUM_ACCOUNT_IDS; i++)
    _data_account[i] = 0;
}

void account_show()
{
  ssize_t i;
  uint64_t total = 0;

  for (i = 0; i < NUM_ACCOUNT_IDS; i++)
    total += _data_account[i];

  printf ("\n%sstructure%s                      %smember%s               "
	  "     %sbytes%s    %sfrac%s\n\n",
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM));

  for (i = 0; i < NUM_ACCOUNT_IDS; i++)
    {
      account_id *acc_id = &_account_ids[i];
      
      if (_data_account[i])
	printf ("%-30s.%-20s %10" PRIu64 " %6.2f%%\n",
		acc_id->_name,
		acc_id->_ident,
		(uint64_t) _data_account[i],
		100. * ((double)_data_account[i]) / (double) total);
    }

}

