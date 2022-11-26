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

#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

#include "tdcpm_tspec_table.h"
#include "tdcpm_hash_table.h"
#include "tdcpm_malloc.h"

typedef struct tdcpm_tspec_item_t
{
  int                  _type;

  uint64_t             _value;
 
} tdcpm_tspec_item;

typedef struct tdcpm_tspec_table_t
{
  tdcpm_tspec_item       *_tspecs;

  uint32_t                _num_tspecs;
  size_t                  _alloc_tspecs;

  tdcpm_hash_table        _hash_table;
} tdcpm_tspec_table;

tdcpm_tspec_table *_tdcpm_tspec_table = NULL;

void tdcpm_tspec_table_init(void)
{
  tdcpm_tspec_table *table;

  table = (tdcpm_tspec_table *) TDCPM_MALLOC(tdcpm_tspec_table);
  
  table->_tspecs = NULL;
  
  table->_num_tspecs = 0;
  table->_alloc_tspecs = 0;

  tdcpm_hash_table_init(&(table->_hash_table));

  _tdcpm_tspec_table = table;

  /* Insert the NONE, START and END items, to give them the first indices. */
  tdcpm_tspec_index idx_none =
    tdcpm_tspec_fixed(TDCPM_TSPEC_TYPE_NONE);
  tdcpm_tspec_index idx_start =
    tdcpm_tspec_fixed(TDCPM_TSPEC_TYPE_START);
  tdcpm_tspec_index idx_end =
    tdcpm_tspec_fixed(TDCPM_TSPEC_TYPE_END);

  (void) idx_none;
  (void) idx_start;
  (void) idx_end;

  assert (idx_none  == 0); /* Indices gotten above. */
  assert (idx_start == 1);
  assert (idx_end   == 2);
}

uint32_t tdcpm_tspec_table_hash_calc(tdcpm_tspec_item *item)
{
  uint64_t x = 0;

  TDCPM_HASH_ROUND(x, item->_type);

  TDCPM_HASH_ROUND(x, item->_value);

  TDCPM_HASH_MUL(x);

  return TDCPM_HASH_RET32(x);
}

int tdcpm_compare_tspec(const void *compare_info,
			uint32_t index,
			const void *item)
{
  const tdcpm_tspec_table *table = (tdcpm_tspec_table *) compare_info;
  const tdcpm_tspec_item *src = (tdcpm_tspec_item *) item;
  const tdcpm_tspec_item *ref = &(table->_tspecs[index]);

  if (src->_type != ref->_type)
    return 0;

  if (src->_value != ref->_value)
    return 0;

  return 1; /* Match. */
}

tdcpm_tspec_index tdcpm_tspec_table_insert(tdcpm_tspec_item *src)
{
  uint32_t index;
  uint32_t hash;
  tdcpm_tspec_table *table = _tdcpm_tspec_table;
  tdcpm_tspec_item *item;
  
  /* Does the tspec exist in the hash table? */
  hash = tdcpm_tspec_table_hash_calc(src);

  index = tdcpm_hash_table_find(&(table->_hash_table),
				src, hash,
				table, tdcpm_compare_tspec);

  if (index != (uint32_t) -1)
    return index;

  /* Entry did not exist. */

  /* Add to the end of the tspecs list. */
  if (table->_alloc_tspecs <= table->_num_tspecs)
    {
      if (!table->_alloc_tspecs)
	table->_alloc_tspecs = 16;

      table->_alloc_tspecs *= 2;

      table->_tspecs = (tdcpm_tspec_item *)
	tdcpm_realloc (table->_tspecs,
		       table->_alloc_tspecs *sizeof (table->_tspecs[0]),
		       "tdcpm_tspec_item");

      tdcpm_hash_table_realloc(&(table->_hash_table),
                               table->_alloc_tspecs * 2);
    }

  index = table->_num_tspecs++;

  tdcpm_hash_table_insert(&(table->_hash_table), hash, index);

  /* We can now use the allocated unit. */
  item = &(table->_tspecs[index]);
  /* Note: if src has an allocated parts pointer, we take it over. */
  *item = *src;

  return (tdcpm_tspec_index) index;
}

const tdcpm_tspec_item *tdcpm_tspec_table_get(tdcpm_tspec_index idx)
{
  tdcpm_tspec_table *table = _tdcpm_tspec_table;
  uint32_t index = (uint32_t) idx;

  return &(table->_tspecs[index]);
}

size_t tdcpm_tspec_to_string(char *str, size_t n,
			     tdcpm_tspec_index tspec_idx)
{
  const tdcpm_tspec_item *tspec;
  /* size_t got = 0; */
 
  tspec = tdcpm_tspec_table_get(tspec_idx);

  switch (tspec->_type)
    {
    case TDCPM_TSPEC_TYPE_NONE:
      strncpy(str, "none", n); /* Typically not printed at all. */
      break;
    case TDCPM_TSPEC_TYPE_START:
      strncpy(str, "START", n);
      break;
    case TDCPM_TSPEC_TYPE_END:
      strncpy(str, "END", n);
      break;
    case TDCPM_TSPEC_TYPE_WR:
      snprintf(str, n, "WR %" PRIu64 "", tspec->_value);
      break;
    }

  return 1; /* TODO FIXME (see tdcpm_unit_to_string().) */
}

tdcpm_tspec_index tdcpm_tspec_fixed(int type)
{
  tdcpm_tspec_item item;

  item._type  = type;
  item._value = 0;

  return tdcpm_tspec_table_insert(&item);
}

tdcpm_tspec_index tdcpm_tspec_wr(uint64_t wr_ts)
{
  tdcpm_tspec_item item;

  item._type = TDCPM_TSPEC_TYPE_WR;
  item._value = wr_ts;

  return tdcpm_tspec_table_insert(&item);
}
