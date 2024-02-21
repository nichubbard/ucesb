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

#include "tdcpm_serialize_util.h"
#include "tdcpm_defs.h"
#include "tdcpm_defs_struct.h"
#include "tdcpm_error.h"

void tdcpm_serialize_nodes(pd_ll_item *nodes,
			   tdcpm_serialize_info *ser);

size_t tdcpm_serialize_var_name_size(tdcpm_var_name *v)
{
  return 1 + v->_num_parts;
}

uint32_t *tdcpm_serialize_var_name(tdcpm_var_name *v,
				   uint32_t *dest)
{
  uint32_t i;

  TDCPM_SERIALIZE_UINT32(dest, v->_num_parts);
  
  for (i = 0; i < v->_num_parts; i++)
    {
      TDCPM_SERIALIZE_UINT32(dest, v->_parts[i]);
    }

  return dest;
}

size_t tdcpm_serialize_vect_size(uint32_t num)
{
  /* Per item: 4 words:
   * 2 for data (double), one for unit and one for spec.
   */
  return 4 * num;
}

uint32_t *tdcpm_serialize_vect_loop(pd_ll_item *sentinel,
				    uint32_t *dest)
{
  pd_ll_item *iter;

  PD_LL_FOREACH(*sentinel, iter)
    {
      tdcpm_vect_dbl_units *item;

      item = PD_LL_ITEM(iter, tdcpm_vect_dbl_units, _items);

      TDCPM_SERIALIZE_DOUBLE(dest, item->_item._dbl_unit._value);

      TDCPM_SERIALIZE_UINT32(dest, item->_item._dbl_unit._unit_idx);

      TDCPM_SERIALIZE_UINT32(dest, item->_item._tspec_idx);
    }

  return dest;
}

void tdcpm_serialize_vect(pd_ll_item *sentinel,
			  tdcpm_serialize_info *ser)
{
  pd_ll_item *iter;
  uint32_t num = 0;
  uint32_t *dest;
  size_t words;

  /* First need to count the items. */
  PD_LL_FOREACH(*sentinel, iter)
    num++;

  /* Data: first word telling how many items. */
  words = 1 + tdcpm_serialize_vect_size(num);
  
  dest = tdcpm_serialize_alloc_words(ser, words);

  TDCPM_SERIALIZE_UINT32(dest, num);

  dest = tdcpm_serialize_vect_loop(sentinel, dest);

  tdcpm_serialize_commit_words(ser, dest);
}


void tdcpm_serialize_table(tdcpm_table *table,
			   tdcpm_serialize_info *ser)
{
  pd_ll_item *iter;
  uint32_t *dest;
  size_t words = 0;
  uint32_t columns = 0;
  uint32_t rows = 0;
  uint32_t has_units = 0;
  uint32_t has_names = 0;

  dest = NULL;

  words += 1 /* columns */ + 1 /* rows */ + 1 /* flag have units */;

  /* Since we allow tables without headers (or empty), we need to
   * handle that case.  Here may be fine, but unpacking needs to know.
   */
  has_names = !PD_LL_IS_EMPTY(&(table->_header));

  if (has_names)
    {
      /* Get size of header items. */
      PD_LL_FOREACH(table->_header, iter)
	{
	  tdcpm_vect_var_names *vn;
	  vn = PD_LL_ITEM(iter, tdcpm_vect_var_names, _items);
	  words += tdcpm_serialize_var_name_size(vn->_item._item) +
	    1 /* tspec */;
	  /* columns++; */
	}
    }

  columns = table->_columns;

  has_units = !PD_LL_IS_EMPTY(&(table->_units));

  if (has_units)
    {
      /* 2 words for value, 1 for unit index */
      words += 3 * columns;
    }
  
  PD_LL_FOREACH(table->_lines, iter)
    {
      tdcpm_vect_table_lines *line;
      line = PD_LL_ITEM(iter, tdcpm_vect_table_lines, _items);

      words += 1; /* Flag if we have var name. */
      if (line->_item._var_name)
	words += tdcpm_serialize_var_name_size(line->_item._var_name);
      words += tdcpm_serialize_vect_size(columns);

      rows++;
    }

  /* Reserve the space. */
  
  dest = tdcpm_serialize_alloc_words(ser, words);

  TDCPM_SERIALIZE_UINT32(dest, columns);
  TDCPM_SERIALIZE_UINT32(dest, rows);
  TDCPM_SERIALIZE_UINT32(dest, has_units | (has_names << 1));

  PD_LL_FOREACH(table->_header, iter)
    {
      tdcpm_vect_var_names *vn;
      vn = PD_LL_ITEM(iter, tdcpm_vect_var_names, _items);
      dest = tdcpm_serialize_var_name(vn->_item._item, dest);
      TDCPM_SERIALIZE_UINT32(dest, vn->_item._tspec_idx);
    }

  if (!PD_LL_IS_EMPTY(&(table->_units)))
    {
      PD_LL_FOREACH(table->_units, iter)
	{
	  tdcpm_vect_dbl_units *unit;
	  unit = PD_LL_ITEM(iter, tdcpm_vect_dbl_units, _items);

	  TDCPM_SERIALIZE_DOUBLE(dest, unit->_item._dbl_unit._value);
	  TDCPM_SERIALIZE_UINT32(dest, unit->_item._dbl_unit._unit_idx);
	}
    }
  
  PD_LL_FOREACH(table->_lines, iter)
    {
      tdcpm_vect_table_lines *line;
      line = PD_LL_ITEM(iter, tdcpm_vect_table_lines, _items);

      TDCPM_SERIALIZE_UINT32(dest, !!(line->_item._var_name));
      if (line->_item._var_name)
	dest = tdcpm_serialize_var_name(line->_item._var_name, dest);
      
      dest = tdcpm_serialize_vect_loop(&(line->_item._line_items), dest);
    }

  tdcpm_serialize_commit_words(ser, dest);
}

void tdcpm_serialize_node(tdcpm_vect_node *node,
			  tdcpm_serialize_info *ser)
{
  size_t words = 0;
  uint32_t *dest = NULL;
  ptrdiff_t off_size = (ptrdiff_t) -1;

  words += 1; /* type */

  if (node->_node._type != TDCPM_NODE_TYPE_VALID)
    {
      words += tdcpm_serialize_var_name_size(node->_node.n._var_name);
    }
  else
    {
      words += 2 /* storage=jump size */ +
	1 /* tspec from*/ + 1 /* tspec to */;
    }

  dest = tdcpm_serialize_alloc_words(ser, words);

  TDCPM_SERIALIZE_UINT32(dest, node->_node._type);

  if (node->_node._type != TDCPM_NODE_TYPE_VALID)
    {
      dest = tdcpm_serialize_var_name(node->_node.n._var_name, dest);
    }
  else
    {
      off_size = TDCPM_SERIALIZE_CUROFF(ser, dest);
      dest += 2; /* Used for size_valid_block below. */
      
      TDCPM_SERIALIZE_UINT32(dest, node->_node.n._tspec_idx._from);
      TDCPM_SERIALIZE_UINT32(dest, node->_node.n._tspec_idx._to);
    }

  tdcpm_serialize_commit_words(ser, dest);

  /* */

  switch (node->_node._type)
    {
    case TDCPM_NODE_TYPE_VECT:
      {
	pd_ll_item *sentinel;

	sentinel = &(node->_node.u._vect);

	tdcpm_serialize_vect(sentinel, ser);
      }      
      break;
    case TDCPM_NODE_TYPE_TABLE:
      {
	tdcpm_serialize_table(node->_node.u._table, ser);
      }
      break;
    case TDCPM_NODE_TYPE_SUB_NODE:
      {
	pd_ll_item *sentinel;

	sentinel = &(node->_node.u._sub_nodes);

	tdcpm_serialize_nodes(sentinel, ser);
      }
      break;
    case TDCPM_NODE_TYPE_VALID:
      {
	pd_ll_item *sentinel;
	ptrdiff_t off_end;
	size_t size_valid_block;

	sentinel = &(node->_node.u._sub_nodes);

	tdcpm_serialize_nodes(sentinel, ser);

	off_end = TDCPM_SERIALIZE_CUROFF(ser, dest);

	/* Calculate size of entire valid block. */
	size_valid_block = off_end - off_size;

	/* Write the size to the space allocated before the block. */
	dest = TDCPM_SERIALIZE_OFF_PTR(ser, off_size);
	TDCPM_SERIALIZE_UINT64(dest, (uint64_t) size_valid_block);
      }
      break;
    default:
      TDCPM_ERROR("Unknown node type (%d).\n",
		  node->_node._type);
      break;
    }
}


void tdcpm_serialize_nodes(pd_ll_item *nodes,
			   tdcpm_serialize_info *ser)
{
  pd_ll_item *iter;
  uint32_t num = 0;
  uint32_t *dest;

  /* First need to count the items. */
  PD_LL_FOREACH(*nodes, iter)
    num++;

  dest = tdcpm_serialize_alloc_words(ser, 1);
  TDCPM_SERIALIZE_UINT32(dest, num);
  tdcpm_serialize_commit_words(ser, dest);

  PD_LL_FOREACH(*nodes, iter)
    {
      tdcpm_vect_node *node;

      node = PD_LL_ITEM(iter, tdcpm_vect_node, _nodes);

      tdcpm_serialize_node(node, ser); 
    }
}

void tdcpm_serialize_all_nodes(void)
{
  /* ptrdiff_t i; */
  
  tdcpm_serialize_nodes(&_tdcpm_all_nodes, &_tdcpm_all_nodes_serialized);

  /*
  for (i = 0; i < _tdcpm_all_nodes_serialized._offset; i++)
    printf ("  %08x%s",
            _tdcpm_all_nodes_serialized._buf[i], i % 8 == 7 ? "\n" : "");
  printf ("\n");
  */
}
