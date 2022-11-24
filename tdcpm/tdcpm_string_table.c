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
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>

#include "tdcpm_string_table.h"
#include "tdcpm_hash_table.h"
#include "tdcpm_malloc.h"

typedef struct tdcpm_string_table_hash_t
{
  uint32_t _hash;
  uint32_t _index;
} tdcpm_string_table_hash;

struct tdcpm_string_table_t
{
  char                     *_strings;
  size_t                   *_offsets;

  uint32_t                  _num_strings;
  size_t                    _size_strings;
  size_t                    _alloc_strings;
  size_t                    _alloc_offsets;

  tdcpm_hash_table          _hash_table;
};

typedef struct tdcpm_string_table_stats_t
{
  uint64_t _lookups;
  uint64_t _hash_lookups;
  uint64_t _hash_match;
  uint64_t _full_match;
} tdcpm_string_table_stats;

tdcpm_string_table_stats _tdcpm_string_table_stats = { 0, 0, 0, 0 };

void tdcpm_string_table_print_stats(tdcpm_string_table *table)
{
  fprintf (stderr, "#: %d ", table->_num_strings);
  tdcpm_hash_table_print_stats(&(table->_hash_table));
}

tdcpm_string_table *tdcpm_string_table_init()
{
  tdcpm_string_table *table;

  table = (tdcpm_string_table *) TDCPM_MALLOC(tdcpm_string_table);
  
  table->_strings = NULL;
  table->_offsets = NULL;
  
  table->_num_strings = 0;
  table->_size_strings = 0;
  table->_alloc_strings = 0;
  table->_alloc_offsets = 0;

  tdcpm_hash_table_init(&(table->_hash_table));

  return table;
}

typedef union tdcpm_pun_chars_uint64_t
{
  char     _chars[8];
  uint64_t _u64;
} tdcpm_pun_chars_uint64;

uint32_t tdcpm_string_table_hash_calc(const char *str, size_t len)
{
  size_t off;
  uint64_t x = 0;
  
  for (off = 0; off < len; off += sizeof (uint64_t))
    {
      tdcpm_pun_chars_uint64 pun;
      size_t use;

      use = len - off;
      if (use > 8)
	use = 8;

      pun._u64 = 0;
      memcpy(pun._chars, str + off, use);

      /* printf ("%016" PRIx64 " %016" PRIx64 "\n", x, pun._u64); */

      TDCPM_HASH_ROUND(x, pun._u64);
    }

  /* printf ("%016" PRIx64 "\n", x); */

  TDCPM_HASH_MUL(x);

  /* printf ("%016" PRIx64 "\n", x); */

  return TDCPM_HASH_RET32(x);
}

typedef struct tdcpm_str_len_t
{
  const char *_str;
  size_t      _len;
} tdcpm_str_len;

int tdcpm_compare_str_len(const void *compare_info,
			  uint32_t index,
			  const void *item)
{
  const tdcpm_string_table *table = (tdcpm_string_table *) compare_info;
  const tdcpm_str_len *str_len = (tdcpm_str_len *) item;
  
  size_t offset = table->_offsets[index];

  const char *ref = table->_strings + offset;

  /* We must check the string first, since we do not know the length
   * of the ref string (in the table).  After that, we can check
   * proper trailing length.
   */
  
  if (strncmp(ref, str_len->_str, str_len->_len) != 0)
    return 0; /* No match. */

  if (ref[str_len->_len] != 0)
    return 0; /* No match. */

  return 1; /* Match. */
}

tdcpm_string_index tdcpm_string_table_insert(tdcpm_string_table *table,
					     const char *str, size_t len)
{
  uint32_t index;
  uint32_t hash;
  char *this_str;
  tdcpm_str_len str_len;
  
  /* Does the string exist in the hash table? */
  hash = tdcpm_string_table_hash_calc(str, len);

  /* printf ("hash: %08" PRIx32 " '%.*s'", hash, (int) len, str); */

  _tdcpm_string_table_stats._lookups++;

  str_len._str = str;
  str_len._len = len;

  index = tdcpm_hash_table_find(&(table->_hash_table),
				&str_len, hash,
				table, tdcpm_compare_str_len);

  if (index != (uint32_t) -1)
    {
      /* printf (" -> idx %" PRId32 " off %zd\n",
	 index, table->_offsets[index]); */
      return index;
    }

  /* Entry did not exist. */

  /* Add to the end of the strings list. */
  if (table->_alloc_strings < table->_size_strings + len + 1)
    {
      if (!table->_alloc_strings)
	table->_alloc_strings = 0x1000;

      while (table->_alloc_strings < table->_size_strings + len + 1)
	table->_alloc_strings *= 2;

      table->_strings =
	(char *) tdcpm_realloc (table->_strings, table->_alloc_strings,
				"string table");
    }

  if (table->_alloc_offsets <= table->_num_strings)
    {
      if (!table->_alloc_offsets)
	table->_alloc_offsets = 16;

      table->_alloc_offsets *= 2;

      table->_offsets = (size_t *)
	tdcpm_realloc (table->_offsets,
		       table->_alloc_offsets * sizeof (table->_offsets[0]),
		       "string table offsets");

      tdcpm_hash_table_realloc(&(table->_hash_table),
			       table->_alloc_offsets * 2);
    }

  index = table->_num_strings++;

  tdcpm_hash_table_insert(&(table->_hash_table), hash, index);

  this_str = table->_strings + table->_size_strings;

  memcpy(this_str, str, len);
  *(this_str + len) = 0;

  /* Add to the offsets list. */
  table->_offsets[index] = table->_size_strings;
  table->_size_strings += len+1;

  /* printf (" => idx %" PRId32 " off %zd\n",
     index, table->_offsets[index]); */

  return (tdcpm_string_index) index;
}

const char *tdcpm_string_table_get(tdcpm_string_table *table,
				   tdcpm_string_index idx)
{
  size_t offset = table->_offsets[idx];
  const char *ref = table->_strings + offset;

  return ref;
}
