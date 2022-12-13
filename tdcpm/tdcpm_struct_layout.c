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

#define _DEFAULT_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "tdcpm_struct_info.h"
#include "tdcpm_struct_layout.h"
#include "tdcpm_malloc.h"
#include "tdcpm_error.h"

tdcpm_struct_info *_tdcpm_li_global = NULL;

tdcpm_struct_info *tdcpm_struct(size_t size, const char *type_name)
{
  tdcpm_struct_info *info;
  tdcpm_string_index type_name_idx;

  type_name_idx =
    tdcpm_string_table_insert(_tdcpm_parse_string_idents,
			      type_name, strlen(type_name));

  info = (tdcpm_struct_info *)
    TDCPM_MALLOC(tdcpm_struct_info);

  PD_LL_INIT(&info->_items);
  
  info->_type_name_idx = type_name_idx;
  info->_size = size;

  return info;
};

tdcpm_struct_info_item *tdcpm_struct_item(tdcpm_struct_info *parent,
					  size_t size,
					  size_t offset,
					  const char *name)
{
  tdcpm_struct_info_item *item;
  tdcpm_string_index name_idx;

  name_idx =
    tdcpm_string_table_insert(_tdcpm_parse_string_idents,
			      name, strlen(name));

  item = (tdcpm_struct_info_item *)
    TDCPM_MALLOC(tdcpm_struct_info_item);

  PD_LL_INIT(&item->_levels);

  item->_parent   = parent;
  item->_name_idx = name_idx;
  item->_offset   = offset;
  item->_size     = size;
  item->_kind     = 0;

  PD_LL_ADD_BEFORE(&parent->_items, &item->_items);

  return item;
};

void tdcpm_struct_item_check_size(tdcpm_struct_info_item *item,
				  size_t size)
{
  size_t enclosing_size;

  /* Check that the size of one item is smaller than the enclosing
   * structure, or if there already was a dimension, the last such.
   */
  if (PD_LL_IS_EMPTY(&item->_levels))
    {
      enclosing_size = item->_size;
    }
  else
    {
      pd_ll_item *prev = PD_LL_LAST(item->_levels);

      tdcpm_struct_info_array_level *level_prev =
	PD_LL_ITEM(prev, tdcpm_struct_info_array_level, _levels);

      enclosing_size = level_prev->_max_index * level_prev->_stride;	
    }

  if (size > enclosing_size)
    {
      TDCPM_ERROR("Item actual size (%zd) > "
		  "enclosing item (array) size (%zd).\n",
		  size, enclosing_size);
    }
}

tdcpm_struct_info_item *tdcpm_struct_item_array(tdcpm_struct_info_item *item,
						size_t size_one,
						size_t size_all)
{
  tdcpm_struct_info_array_level *level;

  level = (tdcpm_struct_info_array_level *)
    TDCPM_MALLOC(tdcpm_struct_info_array_level);

#if 0
  if (item->_kind)
    {
      fprintf (stderr,
               "It is not allowed to specify array dimension "
	       "after item type (cannot check size of item).\n");
      /* Unless we keep track of the actual size of the item assigned. */
      exit (1);
    }
#endif

  tdcpm_struct_item_check_size(item, size_all);

  level->_max_index = size_all / size_one;
  level->_stride    = size_one;

  PD_LL_ADD_BEFORE(&item->_levels, &level->_levels);

  return item;
}

tdcpm_struct_info_item *tdcpm_struct_leaf_double(tdcpm_struct_info_item *item,
						 const char *unit)
{
  tdcpm_struct_info_leaf *leaf;
  tdcpm_string_index unit_str_idx;

  leaf = (tdcpm_struct_info_leaf *)
    TDCPM_MALLOC(tdcpm_struct_info_leaf);

  tdcpm_struct_item_check_size(item, sizeof (double));

  unit_str_idx =
    tdcpm_string_table_insert(_tdcpm_parse_string_idents,
			      unit, strlen(unit));
  
  leaf->_parent = item;
  leaf->_kind   = STRUCT_INFO_LEAF_KIND_DOUBLE;

  tdcpm_unit_new_dissect(&leaf->_dbl_unit, unit_str_idx);

  if (item->_kind)
    {
      TDCPM_ERROR("Item already has leaf/substructure assigned.\n");
    }

  item->_kind = STRUCT_INFO_ITEM_KIND_LEAF;
  item->_leaf._item = leaf;

  return item;
}

tdcpm_struct_info_item *tdcpm_struct_sub_struct(tdcpm_struct_info_item *item,
						tdcpm_struct_info *sub_struct)
{
  if (item->_kind)
    {
      TDCPM_ERROR("Item already has leaf/substructure assigned.\n");
    }

  tdcpm_struct_item_check_size(item, sub_struct->_size);

  item->_kind = STRUCT_INFO_ITEM_KIND_SUB_STRUCT;
  item->_sub_struct._def = sub_struct;

  return item;
}


void tdcpm_struct_init()
{
  _tdcpm_li_global = tdcpm_struct((size_t) -1, ".global");
}
