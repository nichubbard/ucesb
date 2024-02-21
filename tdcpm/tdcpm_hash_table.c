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

#include <stdio.h>
#include <inttypes.h>

#include "tdcpm_hash_table.h"
#include "tdcpm_malloc.h"

void tdcpm_hash_table_init(tdcpm_hash_table *table)
{
  table->_htable = NULL;
  table->_mask  = 0;

  table->_lookups      = 0;
  table->_hash_lookups = 0;
  table->_hash_match   = 0;
  table->_full_match   = 0;
}

void tdcpm_hash_table_realloc(tdcpm_hash_table *table, size_t entries)
{
  size_t old_entries;
  size_t i;

  old_entries = table->_htable ? table->_mask + 1 : 0;

  table->_htable = (tdcpm_hash_table_entry *)
    tdcpm_realloc(table->_htable,
		  entries * sizeof (table->_htable[0]),
		  "tdcpm_hash_table_entry");

  /* First fill the new entries. */
  
  for (i = old_entries; i < entries; i++)
    {
      tdcpm_hash_table_entry *entry = &(table->_htable[i]);

      entry->_hash  = 0; /* Not needed, but gives easier debugging. */
      entry->_index = (uint32_t) -1;
    }

  table->_mask = entries - 1; 

  /* Then all old entries may need to be moved. */
  /* Since an entry may have been placed at a later slot due
   * to collision, we must move possibly several times.
   * Move until no further movement happens.
   */

  for ( ; ; )
    {
      int anymove = 0;
      
      for (i = 0; i < old_entries; i++)
	{
	  tdcpm_hash_table_entry *prev_entry = &(table->_htable[i]);

	  if (prev_entry->_index != (uint32_t) -1)
	    {
	      size_t hash_i;

	      hash_i = prev_entry->_hash & table->_mask;

	      for ( ; ; hash_i = ((hash_i + 1) & table->_mask))
		{
		  tdcpm_hash_table_entry *entry = &(table->_htable[hash_i]);

		  if (hash_i == i)
		    goto no_move;

		  if (entry->_index == (uint32_t) -1)
		    {
		      /* Slot is not occupied - insert. */
		      entry->_hash  = prev_entry->_hash;
		      entry->_index = prev_entry->_index;
		      break;
		    }
		  /* Try next... */
		}

	      /* Remove old slot. */
	      prev_entry->_hash  = 0;
	      prev_entry->_index = (uint32_t) -1;
	      
	      anymove = 1;
	    no_move:
	      ;
	    }
	}

      if (!anymove)
	break;
    }
}

uint32_t tdcpm_hash_table_find(tdcpm_hash_table *table,
			       void *find, uint32_t hash,
			       const void *compare_info,
			       tdcpm_hash_compare compare_func)
{
  uint64_t hash_i;

  table->_lookups++;
  
  if (!table->_mask)
    return (uint32_t) -1;

  hash_i = hash & table->_mask;

  for ( ; ; hash_i = ((hash_i + 1) & table->_mask))
    {
      tdcpm_hash_table_entry *entry = &(table->_htable[hash_i]);

      table->_hash_lookups++;

      if (entry->_index == (uint32_t) -1)
	break; /* Item is unused, abort search. */

      table->_hash_match++;

      if (entry->_hash != hash)
	continue; /* Wrong hash, try next one. */

      table->_full_match++;

      if (compare_func(compare_info, entry->_index, find) == 1)
	{
	  return entry->_index;
	}
    }

  return (uint32_t) -1;  
}

void tdcpm_hash_table_insert(tdcpm_hash_table *table,
			     uint32_t hash, uint32_t index)
{
  uint64_t hash_i;
  
  hash_i = hash & table->_mask;

  for ( ; ; hash_i = ((hash_i + 1) & table->_mask))
    {
      tdcpm_hash_table_entry *entry = &(table->_htable[hash_i]);

      if (entry->_index == (uint32_t) -1)
	{
	  /* Item is not occupied - insert. */
	  entry->_hash  = hash;
	  entry->_index = index;
	  break;
	}
      /* Try next... */
    }
}

void tdcpm_hash_table_print_stats(tdcpm_hash_table *table)
{
  fprintf (stderr,
	   "lu: %" PRIu64", h: %" PRIu64"(=%.1fx) "
	   "hm: %" PRIu64"(=%.1fx) fm: %" PRIu64"(=%.1fx)\n",
	   table->_lookups,
	   table->_hash_lookups,
	   ((double) table->_hash_lookups) /
	   ((double) table->_lookups),
	   table->_hash_match,
	   ((double) table->_hash_match) /
	   ((double) table->_hash_lookups),
	   table->_full_match,
	   ((double) table->_full_match) /
	   ((double) table->_hash_match));
}

