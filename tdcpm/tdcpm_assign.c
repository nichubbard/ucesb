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

typedef struct tdcpm_match_vn_struct_state_t
{
  /* Structure we are to look at.
   * If NULL, then we've hit the leaf - then _item is set.
   * If item also is NULL, then we did not match at all.
   */
  tdcpm_struct_info      *_cur_struct;
  /* If we are currently looking at indices. */
  tdcpm_struct_info_item *_item;
  /* Iterator of index to look at. */
  pd_ll_item             *_iter_index;
  /* The offset in the structure so far. */
  uintptr_t               _offset;
} tdcpm_match_vn_struct_state;

void tdcpm_assign_nodes(tdcpm_deserialize_info *deser,
			const tdcpm_match_vn_struct_state *struct_state);


void
tdcpm_match_var_name_struct(tdcpm_deserialize_info *deser,
			    tdcpm_match_vn_struct_state *state)
{
  uint32_t num_parts;
  uint32_t i;

  num_parts = TDCPM_DESER_UINT32(deser);

  for (i = 0; i < num_parts; i++)
    {
      uint32_t part;

      part = TDCPM_DESER_UINT32(deser);

      if (!state->_cur_struct)
	{
	  /* We expect a name or index, but are at a leaf.  No match. */
	  goto no_match;
	}

      if (part & TDCPM_VAR_NAME_PART_FLAG_NAME)
	{
	  tdcpm_string_index name_idx;
	  pd_ll_item *iter;
	  tdcpm_struct_info_item *item;

	  /*
	  printf ("Match name %d %s ...",
		  part & (~TDCPM_VAR_NAME_PART_FLAG_NAME),
		  tdcpm_string_table_get(_tdcpm_parse_string_idents,
					 part &
					 (~TDCPM_VAR_NAME_PART_FLAG_NAME)));
	  */

	  /* We expect to be at a structure. */

	  if (state->_item)
	    {
	      /* Structure has index, but we have name.  No match. */
	      goto no_match;
	    }

	  name_idx =
	    part & (~TDCPM_VAR_NAME_PART_FLAG_NAME);

	  PD_LL_FOREACH(state->_cur_struct->_items, iter)
	    {
	      item = PD_LL_ITEM(iter, tdcpm_struct_info_item, _items);

	      if (item->_name_idx == name_idx)
		goto found_item_name;
	    }
	  /* We did not find the named item. */
	  goto no_match;

	found_item_name:
	  /* printf (" ! %p\n", item); */

	  state->_offset += item->_offset;

	  state->_item = item;
	  state->_iter_index = state->_item->_levels._next;
	}
      else
	{
          tdcpm_struct_info_array_level *level;
	  uint32_t idx;

	  /*
	  printf ("Match idx [%d] ... { %p %p }", part,
		  state->_iter_index, &(state->_item->_levels));
	  */

	  /* We expect to find an index. */

	  if (state->_item == NULL)
	    {
	      /* No indices left.  No match. */
	      goto no_match;
	    }

	  level = PD_LL_ITEM(state->_iter_index,
			     tdcpm_struct_info_array_level, _levels);

	  idx = part;

	  if (idx >= level->_max_index)
	    {
	      /* Index out of range. */
	      goto no_match;
	    }

	  /*
	  printf (" of [%zd] * %zd\n",
		  level->_max_index, level->_stride);
	  */
	  /* Index successfully consumed. */
	  state->_offset += idx * level->_stride;

	  state->_iter_index = state->_iter_index->_next;
	}

      /* Did we run out of indices?
       * (Or perhaps had none if it was a name match.)
       */
      if (state->_iter_index == &(state->_item->_levels))
	{
	  if (state->_item->_kind == STRUCT_INFO_ITEM_KIND_SUB_STRUCT)
	    {
	      state->_cur_struct = state->_item->_sub_struct._def;
	      state->_item = NULL;
	      continue;
	    }

	  assert (state->_item->_kind == STRUCT_INFO_ITEM_KIND_LEAF);

	  /* We have now hit the leaf. */

	  state->_cur_struct = NULL;
	}
    }

  /* We matched so far. */
  return;

 no_match:
  /* printf (" No match\n"); */
  /* We must eat the remaining name parts. */
  for (i++ ; i < num_parts; i++)
    {
      uint32_t part;

      part = TDCPM_DESER_UINT32(deser);
      (void) part;
    }

  state->_cur_struct = NULL;
  state->_item = NULL;
  return;
}


void *
tdcpm_match_var_name_struct_leaf(const tdcpm_match_vn_struct_state *state,
				 tdcpm_dbl_unit **dbl_unit)
{
  tdcpm_struct_info_leaf *leaf;

  /* printf ("Try leaf\n"); */

  if (state->_cur_struct)
    {
      /* We are not yet at a leaf.  I.e. not a complete match. */
      return NULL;
    }

  if (!state->_item)
    {
      /* We did not match. */
      return NULL;
    }

  /* printf ("Hit\n"); */

  assert (state->_item->_kind == STRUCT_INFO_ITEM_KIND_LEAF);

  leaf = state->_item->_leaf._item;

  switch (leaf->_kind)
    {
      /* Should check that types match! */
    }

  /* printf ("Match! (%zd)\n", offset); */

  *dbl_unit = &leaf->_dbl_unit;

  return ((void *) state->_offset);
}


void tdcpm_assign_item(const tdcpm_match_vn_struct_state *struct_state,
		       double value,
		       tdcpm_unit_index unit_idx,
		       tdcpm_tspec_index tspec_idx)
{
  /* Try to find the pointer to the item. */

  void *p;
  tdcpm_dbl_unit *dbl_unit;

  p = tdcpm_match_var_name_struct_leaf(struct_state, &dbl_unit);

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

void tdcpm_assign_vect_loop(tdcpm_deserialize_info *deser,
			    const tdcpm_match_vn_struct_state *struct_state,
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

      tdcpm_assign_item(struct_state, value, unit_idx, tspec_idx);
    }
}

void tdcpm_assign_vect(tdcpm_deserialize_info *deser,
		       const tdcpm_match_vn_struct_state *struct_state)
{
  uint32_t num;

  num = TDCPM_DESER_UINT32(deser);

  tdcpm_assign_vect_loop(deser, struct_state, num);
}

void tdcpm_assign_table(tdcpm_deserialize_info *deser,
			const tdcpm_match_vn_struct_state *struct_state)
{
  uint32_t columns;
  uint32_t rows;
  uint32_t has_names_units;
  int has_units;
  int has_names;
  uint32_t i;

  columns         = TDCPM_DESER_UINT32(deser);
  rows            = TDCPM_DESER_UINT32(deser);
  has_names_units = TDCPM_DESER_UINT32(deser);
  has_names = (has_names_units >> 1) & 1;
  has_units = (has_names_units     ) & 1;

  if (has_names)
    {
      for (i = 0; i < columns; i++)
	{
	  tdcpm_tspec_index tspec_idx;
	  tdcpm_match_vn_struct_state struct_state_tmp;

	  struct_state_tmp = *struct_state;

	  tdcpm_match_var_name_struct(deser, &struct_state_tmp);

	  /* We must keep track of each item, for use as they are assigned! */

	  tspec_idx = TDCPM_DESER_UINT32(deser);

	  (void) tspec_idx;
	}
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
      tdcpm_match_vn_struct_state struct_state_tmp;

      struct_state_tmp = *struct_state; /* Only do this copy when used? */

      has_var_name = TDCPM_DESER_UINT32(deser);

      if (has_var_name)
	{
	  tdcpm_match_var_name_struct(deser, &struct_state_tmp);
	}

      tdcpm_assign_vect_loop(deser, &struct_state_tmp, columns);
    }
}


void tdcpm_assign_node(tdcpm_deserialize_info *deser,
		       const tdcpm_match_vn_struct_state *struct_state)
{
  uint32_t type;
  tdcpm_match_vn_struct_state struct_state_tmp;

  struct_state_tmp = *struct_state;

  type = TDCPM_DESER_UINT32(deser);

  if (type != TDCPM_NODE_TYPE_VALID)
    {
      tdcpm_match_var_name_struct(deser, &struct_state_tmp);
    }

  switch (type)
    {
    case TDCPM_NODE_TYPE_VECT:
      {
	tdcpm_assign_vect(deser, &struct_state_tmp);
      }
      break;
    case TDCPM_NODE_TYPE_TABLE:
      {
	tdcpm_assign_table(deser, &struct_state_tmp);
      }
      break;
    case TDCPM_NODE_TYPE_SUB_NODE:
      {
	tdcpm_assign_nodes(deser, &struct_state_tmp);
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

	tdcpm_assign_nodes(deser, &struct_state_tmp);
      }
      break;
    default:
      TDCPM_ERROR("Unknown node type (%d).\n",
		  type);
      break;
    }
}

void tdcpm_assign_nodes(tdcpm_deserialize_info *deser,
		 	const tdcpm_match_vn_struct_state *struct_state)
{
  uint32_t num, i;

  num = TDCPM_DESER_UINT32(deser);

  for (i = 0; i < num; i++)
    {
      tdcpm_assign_node(deser, struct_state);
    }
}

void tdcpm_assign_all_nodes(void)
{
  tdcpm_deserialize_info deser;

  tdcpm_match_vn_struct_state struct_state;

  deser._cur = _tdcpm_all_nodes_serialized._buf;
  deser._end = deser._cur + _tdcpm_all_nodes_serialized._offset;

  struct_state._cur_struct = _tdcpm_li_global;
  struct_state._item       = NULL;
  struct_state._iter_index = NULL;
  struct_state._offset     = 0;

  tdcpm_assign_nodes(&deser, &struct_state);
}

