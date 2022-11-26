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

#ifndef __TDCPM_UNITS_H__
#define __TDCPM_UNITS_H__

#include <stdint.h>

#include "tdcpm_string_table.h"
#include "tdcpm_tspec_table.h"

/**************************************************************************/

/* A reference to a full unit. */

typedef uint32_t tdcpm_unit_index;

/**************************************************************************/

extern tdcpm_unit_index _tdcpm_unit_none;

/**************************************************************************/

typedef struct tdcpm_unit_item_part_t
{
  tdcpm_string_index _str_idx;
  int32_t            _exponent;

} tdcpm_unit_item_part;

/* Used when building a unit during parsing.
 * Avoids reallocations for most cases, by having storage for two parts.
 * Since it is copied at places, it should be small!
 */
typedef struct tdcpm_unit_build_t
{
  uint32_t             _num_parts;
  union
  {
    tdcpm_unit_item_part  _direct[2];
    struct
    {
      tdcpm_unit_item_part *_ptr;
      int                   _own;
    } _mem;
  } _parts;
} tdcpm_unit_build;

#define TDCPM_UNIT_PARTS(unit)  \
  ((unit)->_num_parts > 2 ? (unit)->_parts._mem._ptr : (unit)->_parts._direct)

extern tdcpm_unit_build _tdcpm_unit_build_none;

/**************************************************************************/

typedef struct tdcpm_dbl_unit_t
{
  double           _value;
  tdcpm_unit_index _unit_idx;
} tdcpm_dbl_unit;

typedef struct tdcpm_dbl_unit_tspec_t
{
  tdcpm_dbl_unit        _dbl_unit;
  tdcpm_tspec_index _tspec_idx;
} tdcpm_dbl_unit_tspec;

typedef struct tdcpm_dbl_unit_build_t
{
  double           _value;
  tdcpm_unit_build _unit_bld;
} tdcpm_dbl_unit_build;

/**************************************************************************/

void tdcpm_unit_table_init(void);

/**************************************************************************/

void tdcpm_unit_new_dissect(tdcpm_dbl_unit *dbl_unit,
			    tdcpm_string_index str_idx);

void tdcpm_unit_build_new(tdcpm_unit_build *dest,
			  tdcpm_string_index str_idx,
			  int exponent);

void tdcpm_unit_build_mul(tdcpm_unit_build *dest,
			  tdcpm_unit_build *a, tdcpm_unit_build *b,
			  int mul);

/**************************************************************************/

tdcpm_unit_index tdcpm_unit_make_full(tdcpm_unit_build *src,
				      int make_canonical);

/* Note: only valid while nothing is inserted into table. */
const tdcpm_unit_build *tdcpm_unit_table_get(tdcpm_unit_index unit_idx);

void tdcpm_unit_build_no_own(tdcpm_unit_build *a);

/**************************************************************************/

/* tdcpm_unit_index tdcpm_unit_mul(tdcpm_unit_index a, tdcpm_unit_index b,
   int mul); */

int tdcpm_unit_factor(tdcpm_unit_index a_idx, tdcpm_unit_index b_idx,
		      double *factor);

int tdcpm_unit_build_factor(tdcpm_unit_build *a, tdcpm_unit_build *b,
			    double *factor);

/**************************************************************************/

size_t tdcpm_unit_to_string(char *str, size_t n,
			    tdcpm_unit_index unit_idx);

/**************************************************************************/

#endif/*__TDCPM_UNITS_H__*/
