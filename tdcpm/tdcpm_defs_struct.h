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

#ifndef __TDCPM_DEFS_STRUCT_H__
#define __TDCPM_DEFS_STRUCT_H__

#include "tdcpm_defs.h"

/**************************************************************************/

/* Used for table header. */
struct tdcpm_vect_var_names_t
{
  tdcpm_var_name    *_item;
  tdcpm_tspec_index  _tspec_idx;

  pd_ll_item _items;
};

/**************************************************************************/

struct tdcpm_vect_units_t
{
  tdcpm_unit_index _unit_idx;

  pd_ll_item _items;
};

/**************************************************************************/

/* Used for table header. */
struct tdcpm_vect_dbl_units_t
{
  tdcpm_dbl_unit_tspec _item;

  pd_ll_item _items;
};

/**************************************************************************/

struct tdcpm_table_line_t
{
  tdcpm_var_name *_var_name;

  pd_ll_item     _line_items;
};

struct tdcpm_vect_table_lines_t
{
  tdcpm_table_line _item;

  pd_ll_item _items;
};

/**************************************************************************/

struct tdcpm_table_t
{
  pd_ll_item _header;
  pd_ll_item _units;
  pd_ll_item _lines;
};

/**************************************************************************/

#define TDCPM_NODE_TYPE_VECT      1  /* Also for just a single value. */
#define TDCPM_NODE_TYPE_TABLE     2
#define TDCPM_NODE_TYPE_SUB_NODE  3
#define TDCPM_NODE_TYPE_VALID     4  /* Validity range. */

struct tdcpm_node_t
{
  int _type;

  union
  {
    tdcpm_var_name *_var_name;
    struct
    {
      tdcpm_tspec_index _from;
      tdcpm_tspec_index _to;
    } _tspec_idx;
  } n;

  union
  {
    pd_ll_item    _vect;
    tdcpm_table  *_table;
    pd_ll_item    _sub_nodes;
  } u;
};
  
struct tdcpm_vect_node_t
{
  tdcpm_node _node;

  pd_ll_item _nodes;
};

/**************************************************************************/

#endif/*__TDCPM_DEFS_STRUCT_H__*/
