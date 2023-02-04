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

#ifndef __TDCPM_VAR_NAME_H__
#define __TDCPM_VAR_NAME_H__

#include <stdint.h>

#include "tdcpm_string_table.h"

extern tdcpm_string_table *_tdcpm_var_name_strings;

typedef struct tdcpm_var_name_t tdcpm_var_name;

typedef struct tdcpm_var_name_tmp_t tdcpm_var_name_tmp;

#define TDCPM_VAR_NAME_PART_FLAG_NAME  0x80000000

/* The _parts array contain the indices or names.
 * When name, it has the flag value set, and the value
 * is an index into the name string set.
 */

struct tdcpm_var_name_t
{
  uint32_t _num_parts;
  uint32_t _parts[1];  /* Must be last, several items allocated. */
};

struct tdcpm_var_name_tmp_t
{
  uint32_t  _num_parts;
  uint32_t  _num_alloc;
  uint32_t *_parts;
};

void tdcpm_var_name_init(void);

tdcpm_var_name *tdcpm_var_name_new();

tdcpm_var_name *tdcpm_var_name_index(tdcpm_var_name *base, int index);

tdcpm_var_name *tdcpm_var_name_name(tdcpm_var_name *base,
				    tdcpm_string_index name_idx);

tdcpm_var_name *tdcpm_var_name_off(tdcpm_var_name *base, int offset);

tdcpm_var_name *tdcpm_var_name_join(tdcpm_var_name *base,
				    tdcpm_var_name *add);

void tdcpm_var_name_tmp_alloc_extra(tdcpm_var_name_tmp *base,
				    uint32_t extra);

void tdcpm_var_name_tmp_join(tdcpm_var_name_tmp *base,
			     tdcpm_var_name *add);

#endif/*__TDCPM_VAR_NAME_H__*/
