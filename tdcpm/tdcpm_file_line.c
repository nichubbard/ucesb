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

#include "tdcpm_file_line.h"
#include "tdcpm_string_table.h"
#include "tdcpm_malloc.h"

typedef struct tdcpm_file_line_item_t
{
  int                 _internal;
  int                 _line;
  tdcpm_string_index  _file_idx;
} tdcpm_file_line_item;

typedef struct tdcpm_file_line_table_t
{
  tdcpm_file_line_item  *_items;
  
  size_t                 _num_items;
  size_t                 _alloc_items;
} tdcpm_file_line_table;

tdcpm_file_line_table *_tdcpm_file_line_table = NULL;

void tdcpm_file_line_table_init()
{
  tdcpm_file_line_table *table;

  table = (tdcpm_file_line_table *) TDCPM_MALLOC(tdcpm_file_line_table);

  table->_items       = NULL;
  table->_num_items   = 0;
  table->_alloc_items = 0;

  _tdcpm_file_line_table = table;
}

void tdcpm_file_line_insert(int internal,
			    const char *file, size_t sz_file,
			    int line)
{
  tdcpm_file_line_table *table = _tdcpm_file_line_table;
  tdcpm_file_line_item *item;
  
  if (table->_num_items >= table->_alloc_items)
    {
      table->_alloc_items *= 2;
      if (!table->_alloc_items)
	table->_alloc_items = 16;

      table->_items = (tdcpm_file_line_item *)
	tdcpm_realloc(table->_items,
		      table->_alloc_items * sizeof (table->_items[0]),
		      "file line item");
    }

  /* Insert the item. */

  item = &(table->_items[table->_num_items++]);

  item->_internal = internal;
  item->_line     = line;

  item->_file_idx = tdcpm_string_table_insert(_tdcpm_parse_string_idents,
					      file, sz_file);

  /* fprintf (stderr, "%d:%d\n", internal, line); */
}

void tdcpm_lineno_get(int internal, const char **file, int *line)
{
  tdcpm_file_line_table *table = _tdcpm_file_line_table;
  size_t first = 0;                /* First possible item. */
  size_t last = table->_num_items; /* Beyond last possible item. */

  if (!table->_num_items ||
      internal < table->_items[0]._internal)
    {
      *file = "BEFORE_FIRST_LINE";
      *line = -1;
      return;
    }

  for ( ; ; )
    {
      size_t i = (first + last) / 2;

      if (first + 1 >= last)
	{
	  tdcpm_file_line_item *item = &table->_items[first];
	  
	  *file = tdcpm_string_table_get(_tdcpm_parse_string_idents,
					 item->_file_idx);
	  *line = item->_line + internal - item->_internal;

	  return;
	}

      if (internal < table->_items[i]._internal)
	last = i;
      else
	first = i;
    }  
}

void tdcpm_lineno_format(FILE *stream, int internal)
{
  const char *file;
  int line;

  tdcpm_lineno_get(internal, &file, &line);
  fprintf(stream, "%s:%d:", file, line);
}
