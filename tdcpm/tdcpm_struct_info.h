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

#ifndef __TDCPM_STRUCT_INFO_H__
#define __TDCPM_STRUCT_INFO_H__

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

typedef struct tdcpm_struct_info_t tdcpm_struct_info;

typedef struct tdcpm_struct_info_item_t tdcpm_struct_info_item;

typedef struct tdcpm_struct_info_leaf_t tdcpm_struct_info_leaf;

extern tdcpm_struct_info *_tdcpm_li_global;

void tdcpm_struct_init(void);

tdcpm_struct_info *tdcpm_struct(size_t size, const char *typename);

tdcpm_struct_info_item *tdcpm_struct_item(tdcpm_struct_info *parent,
					  size_t size,
					  size_t offset,
					  const char *name);

tdcpm_struct_info_item *tdcpm_struct_item_array(tdcpm_struct_info_item *item,
						size_t size_one,
						size_t size_all);

tdcpm_struct_info_item *tdcpm_struct_leaf_double(tdcpm_struct_info_item *item,
						 const char *unit);

tdcpm_struct_info_item *tdcpm_struct_sub_struct(tdcpm_struct_info_item *item,
						tdcpm_struct_info *sub_struct);

void tdcpm_struct_dump_all(void);

void tdcpm_struct_value_dump_all(void);

/* Dummy macro for structure parser .pl to find structure. */
#define TDCPM_STRUCT_DEF
#define TDCPM_STRUCT_INST(name)

/* Macro for unit to parser .pl. */
#define TDCPM_UNIT(unit_str)

/* Declare a structure (type). */
#define TDCPM_STRUCT(typename, name)	\
  tdcpm_struct(sizeof (typename),	\
	       name)

/* Declare one item of a structure. */
#define TDCPM_STRUCT_ITEM(parent, typename_struct, item, name)	\
  tdcpm_struct_item(parent,					\
		    sizeof (((typename_struct *) (0))->item),	\
		    offsetof (typename_struct, item),		\
		    name)

/* Specify that a structure item is an array.  (Once per dimension.)  */
#define TDCPM_STRUCT_ITEM_ARRAY(item, typename_struct, array_item)	\
  tdcpm_struct_item_array(item,						\
			  sizeof (((typename_struct *) (0))->array_item[0]), \
			  sizeof (((typename_struct *) (0))->array_item))

/* Specify that a structure item is a 'double'. */
#define TDCPM_STRUCT_ITEM_DOUBLE(base, typename_struct,			\
				 item, name, unit)			\
  tdcpm_struct_leaf_double(TDCPM_STRUCT_ITEM(base, typename_struct,	\
					     item, name),		\
			   unit)

/* Specify that a structure item is a (nested) structure. */
#define TDCPM_STRUCT_ITEM_STRUCT(base, typename_struct,		       \
				 item, name, sub_struct)	       \
  tdcpm_struct_sub_struct(TDCPM_STRUCT_ITEM(base, typename_struct,     \
					    item, name),               \
			  sub_struct)

/* Specify one instance of a (top-level / global) structure. */
#define TDCPM_STRUCT_INSTANCE(/*typename_struct, */item, name, sub_struct) \
  tdcpm_struct_sub_struct(tdcpm_struct_item(_tdcpm_li_global,		\
					    sizeof (item),		\
					    (uintptr_t) ((void *) &(item)), \
					    name),			\
			  sub_struct)

/* Specify that a top-level / global structure is an array. */
#define TDCPM_STRUCT_INSTANCE_ARRAY(item, /*typename_struct, */array_item) \
  tdcpm_struct_item_array(item,						\
			  sizeof (array_item[0]),			\
			  sizeof (array_item))

#endif/*__TDCPM_STRUCT_INFO_H__*/
