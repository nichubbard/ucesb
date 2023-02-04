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
#include "tdcpm_error.h"
#include "tdcpm_serialize_util.h"

void tdcpm_assign_nodes(tdcpm_deserialize_info *deser,
			tdcpm_var_name_tmp *var_name_tmp);

void *tdcpm_match_var_name_struct_info(tdcpm_var_name_tmp *var_name_tmp,
				       tdcpm_dbl_unit **dbl_unit)
{
  uint32_t part_i = 0;

  tdcpm_struct_info *cur_struct = _tdcpm_li_global;

  uintptr_t offset = 0;

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

	return ((void *) offset);
      }
    }
}

void tdcpm_assign_item(tdcpm_var_name_tmp *var_name_tmp,
		       double value,
		       tdcpm_unit_index unit_idx,
		       tdcpm_tspec_index tspec_idx)
{
  /* Try to find the pointer to the item. */

  void *p;
  tdcpm_dbl_unit *dbl_unit;

  p = tdcpm_match_var_name_struct_info(var_name_tmp, &dbl_unit);

  if (p)
    {
      double factor;
      
      fprintf (stderr, "%p %.4f\n", p, value);

      /* Can the units be reconciled? */
      if (!tdcpm_unit_factor(unit_idx,
			     dbl_unit->_unit_idx,
			     &factor))
	{
	  fprintf(stderr, "Unit mismatch, cannot assign.\n");
	  exit(1);
	}
      
      *((double *) p) =
	value / factor / dbl_unit->_value;
    }
}

void tdcpm_assign_var_name_join(tdcpm_deserialize_info *deser,
				tdcpm_var_name_tmp *var_name_tmp)
{
  uint32_t i;
  uint32_t num_parts;
  uint32_t *destparts;

  num_parts = TDCPM_DESER_UINT32(deser);

  tdcpm_var_name_tmp_alloc_extra(var_name_tmp, num_parts);

  destparts = &var_name_tmp->_parts[var_name_tmp->_num_parts];

  for (i = 0; i < num_parts; i++)
    {
      uint32_t part;

      part = TDCPM_DESER_UINT32(deser);

      *(destparts++) = part;
    }

  var_name_tmp->_num_parts += num_parts;
}

void tdcpm_assign_vect_loop(tdcpm_deserialize_info *deser,
			    tdcpm_var_name_tmp *var_name_tmp,
			    uint32_t num)
{
  uint32_t i;

  for (i = 0; i < num; i++)
    {
      double value;
      tdcpm_unit_index unit_idx;
      tdcpm_tspec_index tspec_idx;

      TDCPM_DESERIALIZE_DOUBLE(deser, value);
      unit_idx  = TDCPM_DESER_UINT32(deser);
      tspec_idx = TDCPM_DESER_UINT32(deser);

      tdcpm_assign_item(var_name_tmp, value, unit_idx, tspec_idx);
    }
}

void tdcpm_assign_vect(tdcpm_deserialize_info *deser,
		       tdcpm_var_name_tmp *var_name_tmp)
{
  uint32_t num;

  num = TDCPM_DESER_UINT32(deser);

  tdcpm_assign_vect_loop(deser, var_name_tmp, num);
}

void tdcpm_assign_table(tdcpm_deserialize_info *deser,
			tdcpm_var_name_tmp *var_name_tmp)
{
  uint32_t columns;
  uint32_t rows;
  int has_units;
  uint32_t i;

  columns   = TDCPM_DESER_UINT32(deser);
  rows      = TDCPM_DESER_UINT32(deser);
  has_units = TDCPM_DESER_UINT32(deser);

  for (i = 0; i < columns; i++)
    {
      tdcpm_tspec_index tspec_idx;

      tdcpm_assign_var_name_join(deser, var_name_tmp);
      tspec_idx = TDCPM_DESER_UINT32(deser);

      (void) tspec_idx;
    }

  if (has_units)
    {
      for (i = 0; i < columns; i++)
	{
	  double value;
	  tdcpm_unit_index unit_idx;

	  TDCPM_DESERIALIZE_DOUBLE(deser, value);
	  unit_idx = TDCPM_DESER_UINT32(deser);

	  // Remember the unit?
	  (void) value;
	  (void) unit_idx;
	}
    }

  for (i = 0; i < rows; i++)
    {
      int has_var_name;

      has_var_name = TDCPM_DESER_UINT32(deser);

      if (has_var_name)
	{
	  tdcpm_assign_var_name_join(deser, var_name_tmp);
	}

      tdcpm_assign_vect_loop(deser, var_name_tmp, columns);
    }
}


void tdcpm_assign_node(tdcpm_deserialize_info *deser,
		       tdcpm_var_name_tmp *var_name_tmp)
{
  uint32_t type;

  int depth = var_name_tmp->_num_parts;

  type = TDCPM_DESER_UINT32(deser);

  if (type != TDCPM_NODE_TYPE_VALID)
    {
      tdcpm_assign_var_name_join(deser, var_name_tmp);
    }

  switch (type)
    {
    case TDCPM_NODE_TYPE_VECT:
      {
	tdcpm_assign_vect(deser, var_name_tmp);
      }
      break;
    case TDCPM_NODE_TYPE_TABLE:
      {
	tdcpm_assign_table(deser, var_name_tmp);
      }
      break;
    case TDCPM_NODE_TYPE_SUB_NODE:
      {
	tdcpm_assign_nodes(deser, var_name_tmp);
      }
      break;
    case TDCPM_NODE_TYPE_VALID:
      {
	tdcpm_tspec_index tspec_idx_from, tspec_idx_to;

	/* Jump past the size specifier. */
	(void) TDCPM_DESER_UINT32(deser);
	(void) TDCPM_DESER_UINT32(deser);

	tspec_idx_from = TDCPM_DESER_UINT32(deser);
	tspec_idx_to   = TDCPM_DESER_UINT32(deser);

	// tdcpm_dump_tspec(tspec_idx_from);
	// tdcpm_dump_tspec(tspec_idx_to);

	(void) tspec_idx_from;
	(void) tspec_idx_to;

	tdcpm_assign_nodes(deser, var_name_tmp);
      }
      break;
    default:
      TDCPM_ERROR("Unknown node type (%d).\n",
		  type);
      break;
    }

  var_name_tmp->_num_parts = depth;
}

void tdcpm_assign_nodes(tdcpm_deserialize_info *deser,
			tdcpm_var_name_tmp *var_name_tmp)
{
  uint32_t num, i;

  int depth = var_name_tmp->_num_parts;

  num = TDCPM_DESER_UINT32(deser);

  for (i = 0; i < num; i++)
    {
      tdcpm_assign_node(deser, var_name_tmp);

      var_name_tmp->_num_parts = depth;
    }
}

void tdcpm_assign_all_nodes(void)
{
  tdcpm_var_name_tmp var_name_tmp = { 0, 0, NULL };

  tdcpm_deserialize_info deser;

  deser._cur = _tdcpm_all_nodes_serialized._buf;
  deser._end = deser._cur + _tdcpm_all_nodes_serialized._offset;

  tdcpm_assign_nodes(&deser, &var_name_tmp);

  if (var_name_tmp._parts)
    free(var_name_tmp._parts);
}

