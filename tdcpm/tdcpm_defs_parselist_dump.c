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

#include "tdcpm_defs.h"
#include "tdcpm_defs_struct.h"
#include "tdcpm_error.h"

void tdcpm_dump_parselist_nodes(pd_ll_item *nodes, int indent);

void tdcpm_dump_parselist_var_name(tdcpm_var_name *v, int no_index_dot)
{
  uint32_t i;

  /* printf ("[ {%p} ]\n", v); */

  for (i = 0; i < v->_num_parts; i++)
    {
      if (v->_parts[i] & TDCPM_VAR_NAME_PART_FLAG_NAME)
	{
	  tdcpm_string_index name_idx;
	  const char *name;

	  name_idx =
	    v->_parts[i] & (~TDCPM_VAR_NAME_PART_FLAG_NAME);

	  name =
	    tdcpm_string_table_get(_tdcpm_parse_string_idents,
				   /* _tdcpm_var_name_strings, */
				   name_idx);

	  if (i != 0)
	    printf (".");	  
	  printf ("%s", name);
	}
      else
	{
	  if (i == 0 && !no_index_dot)
	    printf (".");	  
	  printf ("(%d)", v->_parts[i] + 1);
	}
    }
}

/* Does not depend directly depend on the parse structure data,
 * so use function for serialized data.
 */

void tdcpm_dump_unit(tdcpm_unit_index unit_idx);
void tdcpm_dump_tspec(tdcpm_tspec_index tspec_idx);

#define tdcpm_dump_parselist_unit tdcpm_dump_unit
#define tdcpm_dump_parselist_tspec tdcpm_dump_tspec

void tdcpm_dump_parselist_vect(pd_ll_item *sentinel, int several)
{
  pd_ll_item *iter;
  int first = 1;

  if (PD_LL_NEXT(sentinel,PD_LL_FIRST(*sentinel)))
    several = 1;

  if (several)
    printf ("{ ");

  PD_LL_FOREACH(*sentinel, iter)
    {
      tdcpm_vect_dbl_units *item;

      item = PD_LL_ITEM(iter, tdcpm_vect_dbl_units, _items);

      if (!first)
	printf (", ");
      first = 0;
	    
      printf ("%.4g", item->_item._dbl_unit._value);

      if (item->_item._dbl_unit._unit_idx != 0)
	{
	  printf (" ");
	  tdcpm_dump_parselist_unit(item->_item._dbl_unit._unit_idx);
	}
      if (item->_item._tspec_idx != 0)
	{
	  printf (" @ ");
	  tdcpm_dump_parselist_tspec(item->_item._tspec_idx);
	}
    }
	
  if (several)
    printf (" }");
}


void tdcpm_dump_parselist_table(tdcpm_table *table, int indent)
{
  pd_ll_item *iter;
  int first;

  printf ("%*s[ ", indent, "");
  first = 1;
  PD_LL_FOREACH(table->_header, iter)
    {
      tdcpm_vect_var_names *vn;

      vn = PD_LL_ITEM(iter, tdcpm_vect_var_names, _items);

      if (!first)
	printf (", ");
      first = 0;

      tdcpm_dump_parselist_var_name(vn->_item._item, 0);
      if (vn->_item._tspec_idx != 0)
	{
	  printf (" @ ");
	  tdcpm_dump_parselist_tspec(vn->_item._tspec_idx);
	}
    }
  printf (" ]\n");

  if (!PD_LL_IS_EMPTY(&(table->_units)))
    {
      printf ("%*s[[ ", indent, "");
      first = 1;
      PD_LL_FOREACH(table->_units, iter)
	{
	  tdcpm_vect_dbl_units *unit;

	  unit = PD_LL_ITEM(iter, tdcpm_vect_dbl_units, _items);

	  if (!first)
	    printf (", ");
	  first = 0;

	  if (unit->_item._dbl_unit._value != 1)
	    printf ("%.4g ", unit->_item._dbl_unit._value);

	  tdcpm_dump_parselist_unit(unit->_item._dbl_unit._unit_idx);
	}
      printf (" ]]\n");
    }
  
  PD_LL_FOREACH(table->_lines, iter)
    {
      tdcpm_vect_table_lines *line;

      line = PD_LL_ITEM(iter, tdcpm_vect_table_lines, _items);

      printf ("%*s", indent, "");
      if (line->_item._var_name)
	{
	  tdcpm_dump_parselist_var_name(line->_item._var_name, 1);
	  printf (": ");
	}
      
      tdcpm_dump_parselist_vect(&(line->_item._line_items),
			   1 /* several = print { } */);
      
      printf ("\n");
    }
}

void tdcpm_dump_parselist_node(tdcpm_vect_node *node, int indent)
{
  printf ("%*s", indent, "");

  if (node->_node._type != TDCPM_NODE_TYPE_VALID)
    {
      tdcpm_dump_parselist_var_name(node->_node.n._var_name, 0);

      printf (" = ");
    }

  switch (node->_node._type)
    {
    case TDCPM_NODE_TYPE_VECT:
      {
	pd_ll_item *sentinel;

	sentinel = &(node->_node.u._vect);

	tdcpm_dump_parselist_vect(sentinel, 0);

	printf (";\n");
      }      
      break;
    case TDCPM_NODE_TYPE_TABLE:
      {
	printf ("\n");
	tdcpm_dump_parselist_table(node->_node.u._table, indent + 2);
	printf ("\n");
      }
      break;
    case TDCPM_NODE_TYPE_SUB_NODE:
      {
	pd_ll_item *sentinel;

	sentinel = &(node->_node.u._sub_nodes);

	printf ("{\n");

	tdcpm_dump_parselist_nodes(sentinel, indent + 2);

	printf ("%*s}\n", indent, "");
      }
      break;
    case TDCPM_NODE_TYPE_VALID:
      {
	pd_ll_item *sentinel;

	printf ("valid( ");
	tdcpm_dump_parselist_tspec(node->_node.n._tspec_idx._from);
	printf (", ");
	tdcpm_dump_parselist_tspec(node->_node.n._tspec_idx._to);
	printf (")\n");

	sentinel = &(node->_node.u._sub_nodes);

	printf ("%*s{\n", indent, "");

	tdcpm_dump_parselist_nodes(sentinel, indent + 2);

	printf ("%*s}\n", indent, "");
      }
      break;
    default:
      TDCPM_ERROR("Unknown node type (%d).\n",
		  node->_node._type);
      break;
    }
}


void tdcpm_dump_parselist_nodes(pd_ll_item *nodes, int indent)
{
  pd_ll_item *iter;

  PD_LL_FOREACH(*nodes, iter)
    {
      tdcpm_vect_node *node;

      node = PD_LL_ITEM(iter, tdcpm_vect_node, _nodes);

      tdcpm_dump_parselist_node(node, indent);
    }
}

void tdcpm_dump_parselist_all_nodes(void)
{
  tdcpm_dump_parselist_nodes(&_tdcpm_all_nodes, 0);
}
