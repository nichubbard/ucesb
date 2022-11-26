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

#ifndef __TDCPM_DEFS_H__
#define __TDCPM_DEFS_H__

#include <stdlib.h>

#include "pd_linked_list.h"
#include "tdcpm_string_table.h"
#include "tdcpm_units.h"
#include "tdcpm_tspec_table.h"

extern const char *_tdcpm_lexer_buf_ptr;
extern size_t _tdcpm_lexer_buf_len;

extern tdcpm_dbl_unit_build *_tdcpm_parse_catch_unitdef;

void tdcpm_init_defs(void);

tdcpm_string_index find_str_identifiers(const char* str);
tdcpm_string_index find_str_strings(const char* str, size_t len);

#define TDCPM_DEF_VAR_KIND_VAR   1
#define TDCPM_DEF_VAR_KIND_UNIT  2

void tdcpm_insert_def_var(tdcpm_string_index str_idx,
			  tdcpm_dbl_unit_build *dbl_unit,
			  int kind);

int tdcpm_find_def_var(tdcpm_string_index str_idx,
		       tdcpm_dbl_unit_build *dbl_unit);

int parse_definitions();

#include "tdcpm_var_name.h"

typedef struct tdcpm_vect_var_names_t tdcpm_vect_var_names;

tdcpm_vect_var_names *tdcpm_vect_var_names_new(tdcpm_var_name *var_name);

tdcpm_vect_var_names *tdcpm_vect_var_names_join(tdcpm_vect_var_names *vect,
						tdcpm_vect_var_names *add);

#include "tdcpm_units.h"

typedef struct tdcpm_vect_units_t tdcpm_vect_units;

tdcpm_vect_units *tdcpm_vect_units_new(tdcpm_unit_index unit_idx);

tdcpm_vect_units *tdcpm_vect_units_join(tdcpm_vect_units *vect,
					tdcpm_vect_units *add);

typedef struct tdcpm_vect_dbl_units_t tdcpm_vect_dbl_units;

tdcpm_vect_dbl_units *tdcpm_vect_dbl_units_new(tdcpm_dbl_unit dbl_unit);

void tdcpm_vect_dbl_units_set_tspec(tdcpm_vect_dbl_units *vect,
				    tdcpm_tspec_index tspecidx);

tdcpm_vect_dbl_units *tdcpm_vect_dbl_units_join(tdcpm_vect_dbl_units *vect,
						tdcpm_vect_dbl_units *add);

typedef struct tdcpm_table_line_t tdcpm_table_line;
typedef struct tdcpm_vect_table_lines_t tdcpm_vect_table_lines;

/* Return a vector directly, for easy next merge (join). */
tdcpm_vect_table_lines *tdcpm_vect_table_line_new(tdcpm_var_name *var_name,
						 tdcpm_vect_dbl_units *line);
tdcpm_vect_table_lines *
tdcpm_vect_table_lines_join(tdcpm_vect_table_lines* vect,
			    tdcpm_vect_table_lines* add);

typedef struct tdcpm_table_t tdcpm_table;

tdcpm_table *tdcpm_table_new(tdcpm_vect_var_names   *header,
			     tdcpm_vect_dbl_units   *units,
			     tdcpm_vect_table_lines *lines);

typedef struct tdcpm_node_t tdcpm_node;
typedef struct tdcpm_vect_node_t tdcpm_vect_node;

/* Return a vector directly, for easy next merge (join). */
tdcpm_vect_node *tdcpm_node_new_vect(tdcpm_var_name       *var_name,
				     tdcpm_vect_dbl_units *vect);
tdcpm_vect_node *tdcpm_node_new_table(tdcpm_var_name *var_name,
				      tdcpm_table    *table);
tdcpm_vect_node *tdcpm_node_new_sub_node(tdcpm_var_name  *var_name,
					 tdcpm_vect_node *sub_node);

tdcpm_vect_node *tdcpm_vect_node_join(tdcpm_vect_node *vect,
				      tdcpm_vect_node *add);
void tdcpm_vect_nodes_all(tdcpm_vect_node *vect);

void tdcpm_init_defs(void);

extern PD_LL_SENTINEL_ITEM(_tdcpm_all_nodes);

void tdcpm_dump_all_nodes(void);

#endif/*__TDCPM_DEFS_H__*/
