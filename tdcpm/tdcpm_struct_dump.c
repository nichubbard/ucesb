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

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "tdcpm_struct_info.h"
#include "tdcpm_struct_layout.h"
#include "tdcpm_malloc.h"
#include "tdcpm_error.h"

typedef struct tdcpm_struct_dump_seen_t
{
  size_t _num_entries;
  size_t _num_alloc;
  tdcpm_struct_info **_seen;
} tdcpm_struct_dump_seen;

void tdcpm_struct_dump_list(int indent, pd_ll_item *nodes,
			    tdcpm_struct_dump_seen *seen);

void tdcpm_struct_dump_struct(int indent, tdcpm_struct_info *info,
			      tdcpm_struct_dump_seen *seen)
{
  pd_ll_item *iter;
  
  tdcpm_struct_dump_list(indent, &info->_items, seen);
  
  printf ("%*sstruct %s\n", indent, "",
	  tdcpm_string_table_get(_tdcpm_parse_string_idents,
				 info->_type_name_idx));
  printf ("%*s{\n", indent, "");

  PD_LL_FOREACH(info->_items, iter)
    {
      tdcpm_struct_info_item *item;
      const char *typename = NULL;
      pd_ll_item *iter2;

      item = PD_LL_ITEM(iter, tdcpm_struct_info_item, _items);

      switch (item->_kind)
	{
	case STRUCT_INFO_ITEM_KIND_LEAF:
	  {
	    switch (item->_leaf._item->_kind)
	      {
	      case STRUCT_INFO_LEAF_KIND_DOUBLE:
		typename = "double";
		break;
	      default:
		TDCPM_ERROR("Unknown struct leaf type (%d).\n",
			    item->_leaf._item->_kind);
		break;
	      }
	    break;
	  }	  
	case STRUCT_INFO_ITEM_KIND_SUB_STRUCT:
	  typename =
	    tdcpm_string_table_get(_tdcpm_parse_string_idents,
				   item->_sub_struct._def->_type_name_idx);
	  break;
	default:
	  TDCPM_ERROR("Unknown struct item type (%d).\n",
		      item->_kind);
	  break;	  
	}

      printf ("%*s/* %6zd %6zd */ %-7s %s",
	      indent + 2, "",
	      info->_size == (size_t) -1 ? (size_t) -1 : item->_offset,
	      item->_size,
	      typename,
	      tdcpm_string_table_get(_tdcpm_parse_string_idents,
				     item->_name_idx));

      PD_LL_FOREACH(item->_levels, iter2)
	{
	  tdcpm_struct_info_array_level *level;
	  
	  level = PD_LL_ITEM(iter2, tdcpm_struct_info_array_level, _levels);

	  printf ("[%zd]", level->_max_index);
	}
      
      printf (";\n");
    }

  printf ("%*s/* %6zd */\n", indent + 2, "",
	  info->_size);
  printf ("%*s};\n", indent, "");
}

void tdcpm_struct_dump_list(int indent, pd_ll_item *nodes,
			    tdcpm_struct_dump_seen *seen)
{
  pd_ll_item *iter;

  PD_LL_FOREACH(*nodes, iter)
    {
      tdcpm_struct_info_item *item;

      item = PD_LL_ITEM(iter, tdcpm_struct_info_item, _items);

      if (item->_kind == STRUCT_INFO_ITEM_KIND_SUB_STRUCT)
	{
	  tdcpm_struct_info *info = item->_sub_struct._def;
	  size_t i;

	  /* Have we dumped this strtucture before? */
	  for (i = 0; i < seen->_num_entries; i++)
	    if (seen->_seen[i] == info)
	      goto already_dumped;

	  if (seen->_num_entries >= seen->_num_alloc)
	    {
	      seen->_num_alloc *= 2;
	      if (seen->_num_alloc < 16)
		seen->_num_alloc = 16;

	      seen->_seen = (tdcpm_struct_info **)
		tdcpm_realloc(seen->_seen,
			      seen->_num_alloc *
			      sizeof (tdcpm_struct_info *),
			      "tdcpm_struct_info *");
	    }

	  /* Mark this structure as (being) dumped. */
	  seen->_seen[seen->_num_entries++] = info;	  
	  
	  tdcpm_struct_dump_struct(indent, info, seen);

	already_dumped:
	  ;
	}
    }
}

void tdcpm_struct_dump_all(void)
{
  tdcpm_struct_dump_seen seen = { 0, 0, NULL };

  tdcpm_struct_dump_struct(0, _tdcpm_li_global, &seen);
}













void tdcpm_struct_value_dump_struct(size_t offset, const char *name,
				    tdcpm_struct_info *info);

void tdcpm_struct_value_dump_struct_item(size_t offset, const char *name,
					 tdcpm_struct_info_item *item)
{
  switch (item->_kind)
    {
    case STRUCT_INFO_ITEM_KIND_LEAF:
      {
	void *p = ((void *) 0) + offset;

	switch (item->_leaf._item->_kind)
	  {
	  case STRUCT_INFO_LEAF_KIND_DOUBLE:
	    {
	      double *dp = (double *) p;

	      if (*dp != 0)
		{
		  char unit_val[128];
		  char unit_str[128];
		  size_t n;

		  n = tdcpm_unit_to_string(unit_str, sizeof (unit_str),
					   item->_leaf.
					   /**/_item->_dbl_unit._unit_idx);

		  if (n >= sizeof (unit_str))
		    {
		      /* This could be fixed, but - really? */
		      TDCPM_ERROR("Unit too long for string.");
		    }

		  unit_val[0] = '\0';

		  if (item->_leaf._item->_dbl_unit._value != 1.)
		    {
		      snprintf (unit_val, sizeof(unit_val), " * %.3g",
				item->_leaf._item->_dbl_unit._value);
		    }

		  printf ("%s = %.4f%s%s%s\n",
			  name, *dp,
			  unit_val, *unit_str != '\0' ? " " : "", unit_str);
		}

	      break;
	    }
	  default:
	    TDCPM_ERROR("Unknown struct leaf type (%d).\n",
			item->_leaf._item->_kind);
	    break;
	  }
	break;
      }	  
    case STRUCT_INFO_ITEM_KIND_SUB_STRUCT:
      tdcpm_struct_value_dump_struct(offset, name, item->_sub_struct._def);
      break;
    default:
      TDCPM_ERROR("Unknown struct item type (%d).\n",
		  item->_kind);
      break;	  
    }
}

void tdcpm_struct_value_dump_struct_item_array(size_t offset, const char *name,
					       tdcpm_struct_info_item *item,
					       pd_ll_item *level_iter)
{
  tdcpm_struct_info_array_level *level;
  size_t i;
  char *arrayname = NULL;
  size_t arrayoffset;      
  char *arrayindex;
  
  level = PD_LL_ITEM(level_iter, tdcpm_struct_info_array_level, _levels);

  arrayname = tdcpm_malloc(strlen(name) + 2 + 16 + 1,
			   "dump string array");

  strcpy (arrayname, name);

  arrayindex = arrayname + strlen(name);

  level_iter = level_iter->_next;

  arrayoffset = offset;

  for (i = 0; i < level->_max_index; i++)
    {
      sprintf (arrayindex, "[%zd]", i);

      if (level_iter == &(item->_levels))
	tdcpm_struct_value_dump_struct_item(arrayoffset, arrayname, item);
      else
	tdcpm_struct_value_dump_struct_item_array(arrayoffset, arrayname, item,
						  level_iter);

      arrayoffset += level->_stride;
    }

  free (arrayname);  
}

void tdcpm_struct_value_dump_struct(size_t offset,
				    const char *name,
				    tdcpm_struct_info *info)
{
  pd_ll_item *iter;
  
  PD_LL_FOREACH(info->_items, iter)
    {
      tdcpm_struct_info_item *item;
      char *itemname = NULL;
      size_t itemoffset;
      const char *itemname_this;

      item = PD_LL_ITEM(iter, tdcpm_struct_info_item, _items);

      itemname_this = tdcpm_string_table_get(_tdcpm_parse_string_idents,
					     item->_name_idx);

      itemname = tdcpm_malloc(strlen(name) + strlen(itemname_this) + 2,
			      "dump string");

      strcpy (itemname, name);
      strcat (itemname, ".");
      strcat (itemname, itemname_this);

      itemoffset = offset + item->_offset;

      if (PD_LL_IS_EMPTY(&item->_levels))
	tdcpm_struct_value_dump_struct_item(itemoffset, itemname, item);
      else
	tdcpm_struct_value_dump_struct_item_array(itemoffset, itemname, item,
						  PD_LL_FIRST(item->_levels));

      free (itemname);
    }
}






void tdcpm_struct_value_dump_all(void)
{
  tdcpm_struct_value_dump_struct(0, "", _tdcpm_li_global);
}
