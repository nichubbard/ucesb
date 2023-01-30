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

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "tdcpm_defs.h"
#include "tdcpm_defs_struct.h"

#include "tdcpm_var_name.h"
#include "tdcpm_string_table.h"
#include "tdcpm_tspec_table.h"
#include "tdcpm_hash_table.h"
#include "tdcpm_malloc.h"

/**************************************************************************/

/* All strings and identifiers found during parsing is handled with
 * the string table, in order to not have to allocate so much memory,
 * and to not have to care about their lifetime.
 */
tdcpm_string_table *_tdcpm_parse_string_idents = NULL;

tdcpm_string_index find_str_identifiers(const char* str)
{
  tdcpm_string_index idx;

  idx = tdcpm_string_table_insert(_tdcpm_parse_string_idents, str,
				  strlen (str));
  return idx;
}

tdcpm_string_index find_str_strings(const char* str, size_t len)
{
  tdcpm_string_index idx;

  idx = tdcpm_string_table_insert(_tdcpm_parse_string_idents, str, len);
  return idx;
}

/**************************************************************************/

typedef struct tdcpm_def_var_item_t
{
  tdcpm_string_index   _str_idx;
  int                  _kind;
  tdcpm_dbl_unit_build _dbl_unit;
} tdcpm_def_var_item;

typedef struct tdcpm_def_var_table_t
{
  tdcpm_def_var_item     *_items;

  uint32_t                _num_units;
  size_t                  _alloc_units;

  tdcpm_hash_table        _hash_table;

} tdcpm_def_var_table;

tdcpm_def_var_table *_tdcpm_def_var_table = NULL;

void tdcpm_def_var_table_init(void)
{
  tdcpm_def_var_table *table;

  table = (tdcpm_def_var_table *) TDCPM_MALLOC(tdcpm_def_var_table);
  
  table->_items = NULL;
  
  table->_num_units = 0;
  table->_alloc_units = 0;

  tdcpm_hash_table_init(&(table->_hash_table));

  _tdcpm_def_var_table = table;
}

uint32_t tdcpm_string_index_hash_calc(tdcpm_string_index str_idx)
{
  uint64_t x = 0;

  TDCPM_HASH_ROUND(x, (uint32_t) str_idx);

  TDCPM_HASH_MUL(x);

  return TDCPM_HASH_RET32(x);
}

int tdcpm_string_index_compare(const void *compare_info,
			       uint32_t index,
			       const void *item)
{
  const tdcpm_def_var_table *table = (tdcpm_def_var_table *) compare_info;
  const tdcpm_string_index *str_idx = (const tdcpm_string_index *) item;
  const tdcpm_string_index *ref = &(table->_items[index]._str_idx);

  if (*str_idx == *ref)
    return 1;

  return 0;
}

void tdcpm_insert_def_var(tdcpm_string_index str_idx,
			  tdcpm_dbl_unit_build *dbl_unit,
			  int kind)
{
  uint32_t index;
  uint32_t hash;
  tdcpm_def_var_table *table = _tdcpm_def_var_table;
  tdcpm_def_var_item *item;

  hash = tdcpm_string_index_hash_calc(str_idx);

  index = tdcpm_hash_table_find(&table->_hash_table,
				&str_idx, hash,
				table, tdcpm_string_index_compare);

  if (index != (uint32_t) -1)
    {
      fprintf (stderr, "Attempt to redeclare variable '%s'.\n",
	       tdcpm_string_table_get(_tdcpm_parse_string_idents, str_idx));
      exit(1);
      /*
       *dbl_unit = _tdcpm_def_var_table->_items[index]._dbl_unit;
       return 1;
      */
    }

    /* Entry did not exist. */

  if (table->_alloc_units <= table->_num_units)
    {
      if (!table->_alloc_units)
	table->_alloc_units = 16;

      table->_alloc_units *= 2;

      table->_items = (tdcpm_def_var_item *)
	tdcpm_realloc (table->_items,
		       table->_alloc_units * sizeof (table->_items[0]),
		       "tdcpm_def_var_item");

      tdcpm_hash_table_realloc(&(table->_hash_table),
                               table->_alloc_units * 2);
    }

  index = table->_num_units++;

  tdcpm_hash_table_insert(&(table->_hash_table), hash, index);

  /* We can now use the allocated unit. */
  item = &(table->_items[index]);
  /* Note: if src has an allocated parts pointer, we use it, but do not own. */
  item->_str_idx = str_idx;
  item->_kind    = kind;
  item->_dbl_unit = *dbl_unit;
  tdcpm_unit_build_no_own(&item->_dbl_unit._unit_bld);
}

int tdcpm_find_def_var(tdcpm_string_index str_idx,
		       tdcpm_dbl_unit_build *dbl_unit)
{
  uint32_t index;
  uint32_t hash;
  tdcpm_def_var_table *table = _tdcpm_def_var_table;

  hash = tdcpm_string_index_hash_calc(str_idx);

  index = tdcpm_hash_table_find(&table->_hash_table,
				&str_idx, hash,
				table, tdcpm_string_index_compare);

  if (index != (uint32_t) -1)
    {
      *dbl_unit = table->_items[index]._dbl_unit;
      return table->_items[index]._kind;
    }  
  
  return 0;
}

/**************************************************************************/

void tdcpm_init_defs(void)
{
  _tdcpm_parse_string_idents = tdcpm_string_table_init();

  tdcpm_unit_table_init();

  tdcpm_def_var_table_init();

  tdcpm_tspec_table_init();
}

/**************************************************************************/

tdcpm_vect_var_names *tdcpm_vect_var_names_new(tdcpm_var_name *var_name,
					       tdcpm_tspec_index tspec_idx)
{
  tdcpm_vect_var_names *vn;

  vn = (tdcpm_vect_var_names *) TDCPM_MALLOC(tdcpm_vect_var_names);

  vn->_item._item      = var_name;
  vn->_item._tspec_idx = tspec_idx;

  PD_LL_INIT(&(vn->_items));

  return vn; 
}

tdcpm_vect_var_names *tdcpm_vect_var_names_join(tdcpm_vect_var_names *vect,
						tdcpm_vect_var_names *add)
{
  assert (vect);
  assert (add);

  PD_LL_JOIN(&(vect->_items), &(add->_items));

  return vect;
}

/**************************************************************************/

tdcpm_vect_units *tdcpm_vect_units_new(tdcpm_unit_index unit_idx)
{
  tdcpm_vect_units *vu;

  vu = (tdcpm_vect_units *) TDCPM_MALLOC(tdcpm_vect_units);

  vu->_unit_idx = unit_idx;

  PD_LL_INIT(&(vu->_items));

  return vu; 
}

tdcpm_vect_units *tdcpm_vect_units_join(tdcpm_vect_units *vect,
					tdcpm_vect_units *add)
{
  assert (vect);
  assert (add);

  PD_LL_JOIN(&(vect->_items), &(add->_items));

  return vect;
}


/**************************************************************************/

tdcpm_vect_dbl_units *tdcpm_vect_dbl_units_new(tdcpm_dbl_unit dbl_unit,
					       tdcpm_tspec_index tspec_idx)
{
  tdcpm_vect_dbl_units *vdu;

  vdu = (tdcpm_vect_dbl_units *) TDCPM_MALLOC(tdcpm_vect_dbl_units);

  vdu->_item._dbl_unit = dbl_unit;
  vdu->_item._tspec_idx = tspec_idx;

  PD_LL_INIT(&(vdu->_items));

  return vdu; 
}

tdcpm_vect_dbl_units *tdcpm_vect_dbl_units_join(tdcpm_vect_dbl_units *vect,
						tdcpm_vect_dbl_units *add)
{
  assert (vect);
  assert (add);

  PD_LL_JOIN(&(vect->_items), &(add->_items));

  return vect;
}

/**************************************************************************/

tdcpm_vect_table_lines *tdcpm_vect_table_line_new(tdcpm_var_name *var_name,
						  tdcpm_vect_dbl_units *line)
{
  tdcpm_vect_table_lines *vtl;

  vtl = (tdcpm_vect_table_lines *) TDCPM_MALLOC(tdcpm_vect_table_lines);

  vtl->_item._var_name = var_name;   

  PD_LL_INIT(&(vtl->_item._line_items));

  if (line)
    PD_LL_JOIN(&(vtl->_item._line_items), &(line->_items));
  
  PD_LL_INIT(&(vtl->_items));

  return vtl; 
}

tdcpm_vect_table_lines *
tdcpm_vect_table_lines_join(tdcpm_vect_table_lines* vect,
			    tdcpm_vect_table_lines* add)
{
  assert (vect);
  assert (add);

  PD_LL_JOIN(&(vect->_items), &(add->_items));

  return vect;
}

tdcpm_table *tdcpm_table_new(tdcpm_vect_var_names   *header,
			     tdcpm_vect_dbl_units   *units,
			     tdcpm_vect_table_lines *lines)
{
  tdcpm_table *table;
  pd_ll_item *iter, *iter2;
  size_t n_header = 0, n_units = 0;
  size_t n_items;

  table = (tdcpm_table *) TDCPM_MALLOC(tdcpm_table);

  PD_LL_INIT(&(table->_header));
  PD_LL_INIT(&(table->_units));
  PD_LL_INIT(&(table->_lines));

  if (header)
    PD_LL_JOIN(&(table->_header), &(header->_items));
  if (units)
    PD_LL_JOIN(&(table->_units), &(units->_items));
  if (lines)
    PD_LL_JOIN(&(table->_lines), &(lines->_items));

  /* Do the consistency checks after adding to the table.
   * That way we have sentinels for all items, so PD_LL_FOREACH
   * macros work as expected.  In particular, the loop over lines.
   */

  PD_LL_FOREACH(table->_header, iter)
    n_header++;

  n_items = n_header;

  if (units)
    {
      /* units is not a sentinel, so need to be counted too! */
      PD_LL_FOREACH(table->_units, iter)
	n_units++;

      if (n_items && n_units != n_items)
	{
	  fprintf (stderr,
		   "Table header (%zd) and units (%zd) "
		   "have different lengths.\n", n_items, n_units);
	  exit(1);
	}
      else
	n_items = n_units;
    }

  /* printf ("h: %zd u: %zd\n", n_header, n_units); */
  /* printf ("%p %p %p\n", header, units, lines); */

  PD_LL_FOREACH(table->_lines, iter)
    {
      tdcpm_vect_table_lines *line;
      size_t n_line_items = 0;

      line = PD_LL_ITEM(iter, tdcpm_vect_table_lines, _items);

      PD_LL_FOREACH(line->_item._line_items, iter2)
	n_line_items++;

      /* printf ("l: %zd\n", n_line_items); */

      if (n_items && n_line_items != n_items)
	{
	  fprintf (stderr,
		   "Table header (or first line) (%zd) "
		   "and line (%zd) have different lengths.\n",
		   n_items, n_line_items);
	  exit(1);
	}
      else
	n_items = n_line_items;
    }

  return table;
}

/**************************************************************************/

PD_LL_SENTINEL(_tdcpm_all_nodes);

tdcpm_vect_node *tdcpm_node_new()
{
  tdcpm_vect_node *node;

  node = (tdcpm_vect_node *) TDCPM_MALLOC(tdcpm_vect_node);

  node->_node._type = 0;

  PD_LL_INIT(&(node->_nodes));

  return node; 
}

tdcpm_vect_node *tdcpm_node_new_vect(tdcpm_var_name       *var_name,
				     tdcpm_vect_dbl_units *vect)
{
  tdcpm_vect_node *node;

  node = tdcpm_node_new();

  node->_node._type       = TDCPM_NODE_TYPE_VECT;
  node->_node.n._var_name = var_name;

  PD_LL_INIT(&(node->_node.u._vect));

  if (vect)
    PD_LL_JOIN(&(node->_node.u._vect), &(vect->_items));

  return node;
}

tdcpm_vect_node *tdcpm_node_new_table(tdcpm_var_name *var_name,
				      tdcpm_table    *table)
{
  tdcpm_vect_node *node;

  node = tdcpm_node_new();

  node->_node._type       = TDCPM_NODE_TYPE_TABLE;
  node->_node.n._var_name = var_name;
  node->_node.u._table    = table;

  return node;
}

tdcpm_vect_node *tdcpm_node_new_sub_node(tdcpm_var_name  *var_name,
					 tdcpm_vect_node *sub_node)
{
  tdcpm_vect_node *node;

  node = tdcpm_node_new();

  node->_node._type         = TDCPM_NODE_TYPE_SUB_NODE;
  node->_node.n._var_name   = var_name;

  PD_LL_INIT(&(node->_node.u._sub_nodes));

  if (sub_node)
    PD_LL_JOIN(&(node->_node.u._sub_nodes), &(sub_node->_nodes));

  return node;
}

tdcpm_vect_node *tdcpm_node_new_valid_range(tdcpm_tspec_index tspec_idx_from,
					    tdcpm_tspec_index tspec_idx_to,
					    tdcpm_vect_node *nodes)
{
  tdcpm_vect_node *node;

  node = tdcpm_node_new();

  node->_node._type              = TDCPM_NODE_TYPE_VALID;
  node->_node.n._tspec_idx._from = tspec_idx_from;
  node->_node.n._tspec_idx._to   = tspec_idx_to;

  PD_LL_INIT(&(node->_node.u._sub_nodes));

  if (nodes)
    PD_LL_JOIN(&(node->_node.u._sub_nodes), &(nodes->_nodes));

  return node;
}

tdcpm_vect_node *tdcpm_vect_node_join(tdcpm_vect_node *vect,
				      tdcpm_vect_node *add)
{
  if (!add)
    return vect;

  if (!vect)
    return add;

  PD_LL_JOIN(&(vect->_nodes), &(add->_nodes));

  return vect;
}

void tdcpm_vect_nodes_all(tdcpm_vect_node *vect)
{
  if (!vect)
    return;

  PD_LL_JOIN(&_tdcpm_all_nodes, &(vect->_nodes));
}

/**************************************************************************/

/*
  Storage overview:

  program:                         <vect_node>
    stmt_list:                     <vect_node>
      stmt:                        <vect_node>
        calib_param:               <vect_node>
        valid_range:               <vect_node> + <tspec>
          stmt_list:               <vect_node>

  calib_param:                     <vect_node>
    var = value_vector_np_single   node:<vect_dbl_unit> (1)
    var = value_vector_np          node:<vect_dbl_unit>
    var = value_table              node:<table>
    var = stmt_list                node:<vect_node>

 */

/*

  tdcpm_dump_nodes(vect:tdcpm_vect_node)
    tdcpm_dump_node(tdcpm_vect_node)
      name: tdcpm_var_name
      switch
        TYPE_VECT:
          tdcpm_dump_vect(vect:tdcpm_vect_dbl_units)
            <value> <unit> [@ <tspec>]
        TYPE_TABLE:
          tdcpm_dump_table(tdcpm_table)
            header: vect:tdcpm_vect_var_names
              <name> [@ <tspec>]
            units: vect:tdcpm_vect_dbl_units
              <value> <unit>
            lines: vect:tdcpm_vect_table_lines
              [name :] tdcpm_dump_vect(vect:tdcpm_vect_dbl_units)
        TYPE_SUB_NODE:
          tdcpm_dump_nodes() [recursion]
        TYPE_VALID_RANGE:
          tdcpm_dump_nodes() [recursion]




  tdcpm_vect_node = tdcpm_node : type [VECT/TABLE/SUB_NODE/VALID_RANGE]
                                 union { name / { to, from } }
                                 union { vect / table / sub_nodes}

  tdcpm_table : header (sentinel)
                units (sentinel)
                lines (sentinel)

  tdcpm_vect_table_lines = tdcpm_table_line : name
                                              line_items (sentinel)

  tdcpm_vect_dbl_units = tdcpm_dbl_unit_tspec : dbl_unit : value
                                                           unit_idx
                                                tspec_idx

  tdcpm_vect_var_names : item [tdcpm_var_name]
                         tspec_idx
 */
