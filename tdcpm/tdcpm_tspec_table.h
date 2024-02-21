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

#ifndef __TDCPM_TSPEC_TABLE_H__
#define __TDCPM_TSPEC_TABLE_H__

#include <stdint.h>
#include <stdlib.h>

/**************************************************************************/

/* A reference to a tspec. */

typedef uint32_t tdcpm_tspec_index;

/**************************************************************************/

#define TDCPM_TSPEC_TYPE_NONE   0
#define TDCPM_TSPEC_TYPE_START  1
#define TDCPM_TSPEC_TYPE_END    2
#define TDCPM_TSPEC_TYPE_WR     3

/**************************************************************************/

void tdcpm_tspec_table_init(void);

/**************************************************************************/

tdcpm_tspec_index tdcpm_tspec_fixed(int start);

tdcpm_tspec_index tdcpm_tspec_wr(uint64_t wr_ts);

/**************************************************************************/

size_t tdcpm_tspec_to_string(char *str, size_t n,
			     tdcpm_tspec_index tspec_idx);

/**************************************************************************/

#endif/*__TDCPM_TSPEC_TABLE_H__*/
