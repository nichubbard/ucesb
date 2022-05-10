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

#ifndef __TDCPM_STRING_TABLE_H__
#define __TDCPM_STRING_TABLE_H__

#include <stdint.h>
#include <stdlib.h>

/* A string table consist of three parts:
 *
 * A long memory area where the strings are stored.
 * An offset table, that tell where in the memory area each string starts.
 * A hash table of the strings, for easy lookup.
 *
 * Strings are identified by their index in the offset table.
 */

typedef uint32_t tdcpm_string_index;

typedef struct tdcpm_string_table_t tdcpm_string_table;

extern tdcpm_string_table *_tdcpm_parse_string_idents;

tdcpm_string_table *tdcpm_string_table_init(void);

void tdcpm_string_table_print_stats(tdcpm_string_table *table);

tdcpm_string_index tdcpm_string_table_find(tdcpm_string_table *table,
					   const char *str);
tdcpm_string_index tdcpm_string_table_insert(tdcpm_string_table *table,
					     const char *str, size_t len);

/* Note: only valid while nothing is inserted into table. */
const char *tdcpm_string_table_get(tdcpm_string_table *table,
				   tdcpm_string_index index);

#endif/*__TDCPM_STRING_TABLE_H__*/
