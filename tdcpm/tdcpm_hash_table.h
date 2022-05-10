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

#ifndef __TDCPM_HASH_TABLE_H__
#define __TDCPM_HASH_TABLE_H__

#include <stdint.h>
#include <stdlib.h>

typedef struct tdcpm_hash_table_entry_t
{
  uint32_t _hash;
  uint32_t _index;
} tdcpm_hash_table_entry;

typedef  struct tdcpm_hash_table_t
{
  tdcpm_hash_table_entry *_htable;
  uint32_t                _mask;  /* Size of table - 1. */

  /* Statistics. */
  uint64_t _lookups;
  uint64_t _hash_lookups;
  uint64_t _hash_match;
  uint64_t _full_match;

} tdcpm_hash_table;

void tdcpm_hash_table_init(tdcpm_hash_table *table);

void tdcpm_hash_table_realloc(tdcpm_hash_table *table, size_t entries);

typedef int (*tdcpm_hash_compare)(const void *compare_info,
				  uint32_t index,
				  const void *item);

uint32_t tdcpm_hash_table_find(tdcpm_hash_table *table,
			       void *find, uint32_t hash,
			       const void *compare_info,
			       tdcpm_hash_compare compare_func);

void tdcpm_hash_table_insert(tdcpm_hash_table *table,
			     uint32_t hash, uint32_t index);

void tdcpm_hash_table_print_stats(tdcpm_hash_table *table);

#endif/*__TDCPM_HASH_TABLE_H__*/
