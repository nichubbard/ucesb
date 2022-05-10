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

#ifndef __TDCPM_STRUCT_LAYOUT_H__
#define __TDCPM_STRUCT_LAYOUT_H__

#include "pd_linked_list.h"

/* For typedefs. */
#include "tdcpm_struct_info.h"
#include "tdcpm_string_table.h"
/* For unit. */
#include "tdcpm_units.h"

#define STRUCT_INFO_ITEM_KIND_LEAF        1
#define STRUCT_INFO_ITEM_KIND_SUB_STRUCT  2

#define STRUCT_INFO_LEAF_KIND_DOUBLE      1

/* A structure item can be:
 *
 * - A structure itself.  Has:
 *   - Items (i.e. the members).
 * - An item.
 *   - Has a name.
 *   - Optionally has one or more array levels (giving dimension).
 *   - Is one of:
 *     - A leaf, i.e. plain member (double or int).
 *     - A nested structure.
 * - A leaf item.
 *   - Kind.
 */

/* Kind          Children   Size  Offset  Stride  Name  Max#
 *
 * Structure     Many       Y     Y       -       Y     -
 * Item          One        Y     Y       -       Y     -
 * Array level   -          -     -       Y       -     Y
 * Leaf          -          -     -       -       -     -
 */

struct tdcpm_struct_info_t
{
  /* pd_ll_item _users; */
  PD_LL_SENTINEL_ITEM(_items);

  /* The name of the item. */
  tdcpm_string_index _type_name_idx;

  /* What kind of item is this. */
  int    _kind;

  /* How large is this structure. */
  size_t _size;
  
};

typedef struct tdcpm_struct_info_array_level_t
{
  pd_ll_item _levels;

  size_t     _max_index;
  size_t     _stride;

} tdcpm_struct_info_array_level;

struct tdcpm_struct_info_item_t
{
  pd_ll_item _items;

  tdcpm_struct_info *_parent;
  PD_LL_SENTINEL_ITEM(_levels);

  /* What kind of item is this. */
  int _kind;
  /* The size of one item. */
  size_t _one_size;

  /* The name of the item. */
  tdcpm_string_index _name_idx;

  /* Where is the item relative to the parent. */
  size_t _offset;
  /* How large is this item. */
  size_t _size;

  union
  {
    struct
    {
      tdcpm_struct_info      *_def;
      
    } _sub_struct;
    struct
    {
      tdcpm_struct_info_leaf *_item;
      
    } _leaf;
  };

};

struct tdcpm_struct_info_leaf_t
{
  tdcpm_struct_info_item *_parent;

  /* What kind of item is this. */
  int _kind;

  /* Unit. */
  tdcpm_dbl_unit          _dbl_unit;

};

#endif/*__TDCPM_STRUCT_LAYOUT_H__*/
