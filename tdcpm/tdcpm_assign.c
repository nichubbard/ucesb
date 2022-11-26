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
#include <string.h>
#include <assert.h>

#include "tdcpm_defs.h"
#include "tdcpm_defs_struct.h"
#include "tdcpm_struct_layout.h"
#include "tdcpm_assign.h"





void *tdcpm_match_var_name_struct_info(tdcpm_var_name_tmp *var_name_tmp,
				       tdcpm_dbl_unit **dbl_unit)
{
  int part_i = 0;

  tdcpm_struct_info *cur_struct = _tdcpm_li_global;

  size_t offset = 0;

  for ( ; ; )
    {
      tdcpm_string_index name_idx;
      pd_ll_item *iter;
      tdcpm_struct_info_item *item;

      /* At this point, we are at a structure (cur_struct), which
       * means that the next var_name part needs to be a name, to find
       * the relevant item.
       */

      if (part_i >= var_name_tmp->_num_parts)
	{
	  /* var_name out of parts, but we are at a structure. */
	  return NULL;
	}

      if (!(var_name_tmp->_parts[part_i] & TDCPM_VAR_NAME_PART_FLAG_NAME))
	{
	  /* Expected name, but got index. */
	  return NULL;
	}

      name_idx =
	var_name_tmp->_parts[part_i] & (~TDCPM_VAR_NAME_PART_FLAG_NAME);

      PD_LL_FOREACH(cur_struct->_items, iter)
	{
	  item = PD_LL_ITEM(iter, tdcpm_struct_info_item, _items);

	  if (item->_name_idx == name_idx)
	    goto found_item_name;
	}
      /* We did not find the named item. */
      return NULL;

    found_item_name:
      offset += item->_offset;
      part_i++;
      
      /* If the item has indices, then we eat indices. */
      PD_LL_FOREACH(item->_levels, iter)
        {
          tdcpm_struct_info_array_level *level;
	  uint32_t idx;
          
          level = PD_LL_ITEM(iter, tdcpm_struct_info_array_level, _levels);

	  if (part_i >= var_name_tmp->_num_parts)
	    {
	      /* var_name out of parts, but we are at an array dimension. */
	      return NULL;
	    }

	  if ((var_name_tmp->_parts[part_i] & TDCPM_VAR_NAME_PART_FLAG_NAME))
	    {
	      /* Expected index, but got name. */
	      return NULL;
	    }

	  idx =
	    var_name_tmp->_parts[part_i] & (~TDCPM_VAR_NAME_PART_FLAG_NAME);

	  if (idx >= level->_max_index)
	    {
	      /* Index out of range. */
	      return NULL;
	    }

	  /* Index successfully consumed. */
	  offset += idx * level->_stride;
	  part_i++;
        }

      if (item->_kind == STRUCT_INFO_ITEM_KIND_SUB_STRUCT)
	{
	  cur_struct = item->_sub_struct._def;
	  continue;
	}

      assert (item->_kind == STRUCT_INFO_ITEM_KIND_LEAF);
      
      if (part_i < var_name_tmp->_num_parts)
	{
	  /* var_name has parts still, but structure reached end. */
	  return NULL;
	}

      /* We have reached and of both the var_name and the structure.
       * I.e. item found.
       */
      
      {
	tdcpm_struct_info_leaf *leaf = item->_leaf._item;

	switch (leaf->_kind)
	  {
	    /* Should check that types match! */
	  }

	/* printf ("Match! (%zd)\n", offset); */

	*dbl_unit = &leaf->_dbl_unit;

	return ((void *) 0) + offset;
      }
    }
}

void tdcpm_assign_item(tdcpm_var_name_tmp *var_name_tmp,
		       tdcpm_vect_dbl_units *item)
{
  /* Try to find the pointer to the item. */

  void *p;
  tdcpm_dbl_unit *dbl_unit;

  p = tdcpm_match_var_name_struct_info(var_name_tmp, &dbl_unit);

  if (p)
    {
      double factor;
      
      fprintf (stderr, "%p %.4f\n", p, item->_item._dbl_unit._value);

      /* Can the units be reconciled? */
      if (!tdcpm_unit_factor(item->_item._dbl_unit._unit_idx,
			     dbl_unit->_unit_idx,
			     &factor))
	{
	  fprintf(stderr, "Unit mismatch, cannot assign.\n");
	  exit(1);
	}
      
      *((double *) p) =
	item->_item._dbl_unit._value / factor / dbl_unit->_value;
    }
}




void tdcpm_assign_vect(tdcpm_var_name_tmp *var_name_tmp,
		       pd_ll_item *sentinel, int several)
{
  pd_ll_item *iter;

  if (PD_LL_NEXT(sentinel,PD_LL_FIRST(*sentinel)))
    several = 1;

  (void) several;

  PD_LL_FOREACH(*sentinel, iter)
    {
      tdcpm_vect_dbl_units *item;

      item = PD_LL_ITEM(iter, tdcpm_vect_dbl_units, _items);

      tdcpm_assign_item(var_name_tmp, item);

      /*
      if (item->_item._unit_idx != 0)
	{
	  tdcpm_assign_unit(item->_item._unit_idx);
	}
      */
    }
}

void tdcpm_assign_node(tdcpm_var_name_tmp *var_name_tmp,
		       tdcpm_vect_node *node)
{
  tdcpm_var_name_tmp_join(var_name_tmp,
			  node->_node._var_name);

  switch (node->_node._type)
    {
    case TDCPM_NODE_TYPE_VECT:
      {
	pd_ll_item *sentinel;

	sentinel = &(node->_node.u._vect);

	tdcpm_assign_vect(var_name_tmp, sentinel, 0);
      }      
      break;
    }
}

void tdcpm_assign_nodes(tdcpm_var_name_tmp *var_name_tmp,
			pd_ll_item *nodes)
{
  pd_ll_item *iter;

  int depth = var_name_tmp->_num_parts;

  PD_LL_FOREACH(*nodes, iter)
    {
      tdcpm_vect_node *node;

      node = PD_LL_ITEM(iter, tdcpm_vect_node, _nodes);

      tdcpm_assign_node(var_name_tmp, node);

      var_name_tmp->_num_parts = depth;
    }
}

void tdcpm_assign_all_nodes(void)
{
  tdcpm_var_name_tmp var_name_tmp = { 0, 0, NULL };

  tdcpm_assign_nodes(&var_name_tmp, &_tdcpm_all_nodes);
}

