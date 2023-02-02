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
#include <unistd.h>
#include <string.h>

#include "tdcpm_file_line.h"
#include "tdcpm_defs.h"
#include "tdcpm_string_table.h"
#include "tdcpm_struct_info.h"
#include "tdcpm_assign.h"
#include "tdcpm_error.h"

extern int lexer_read_fd;

void tdcpm_test_struct(void);
void tdcpm_declare_struct(void);

int main(int argc, char *argv[])
{
  int dump_struct   = 1;
  int dump_input    = 1;
  int dump_serialized = 0;
  int dump_assigned = 1;
  int dump_stats    = 1;
  int dumped = 0;

  int struct_manual = 0;
  int struct_parsed = 0;
  int struct_none = 0;

  int i;

  tdcpm_file_line_table_init();
  
  tdcpm_init_defs();
  /* tdcpm_var_name_init(); */

  tdcpm_struct_init();

  for (i = 1; i < argc; i++)
    {
      if (     strcmp(argv[i], "--manual") == 0)
	struct_manual = 1;
      else if (strcmp(argv[i], "--parsed") == 0)
	struct_parsed = 1;
      else if (strcmp(argv[i], "--none") == 0)
	struct_none = 1;
      else if (strcmp(argv[i], "--input-only") == 0)
	{
	  dump_struct   = 0;
	  dump_assigned = 0;
	  dump_stats    = 0;
	}
      else if (strcmp(argv[i], "--dump-serialized") == 0)
	{
	  dump_serialized = 1;
	  struct_parsed = 1; /* Just to allow single-argument Makefile test. */
	}
      else
	{
	  TDCPM_ERROR("Unknown option: %s\n", argv[i]);
	}
    }

  if (!struct_manual && !struct_parsed && !struct_none)
    {
      TDCPM_ERROR("--manual, --parsed or --none missing.\n");
    }

  if (struct_manual)
    tdcpm_test_struct();
  if (struct_parsed)
    tdcpm_declare_struct();

  if (dump_struct)
    {
      tdcpm_struct_dump_all();
      dumped = 1;
    }

  lexer_read_fd = STDIN_FILENO;
  
  parse_definitions();

  tdcpm_serialize_all_nodes();

  if (dump_input)
    {
      if (dumped)
	printf ("===\n");
      if (dump_serialized)
	tdcpm_dump_ser_all_nodes();
      else
	tdcpm_dump_all_nodes();
      dumped = 1;
    }

  tdcpm_assign_all_nodes();

  if (dump_assigned)
    {
      if (dumped)
	printf ("===\n");
      tdcpm_struct_value_dump_all();
      dumped = 1;
    }

  if (dump_stats)
    {
      if (dumped)
	printf ("===\n");
      tdcpm_string_table_print_stats(_tdcpm_parse_string_idents);
      dumped = 1;
    }

  return 0;
}
