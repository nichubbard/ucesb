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

extern int lexer_read_fd;

void tdcpm_test_struct(void);

int main(int argc, char *argv[])
{
  tdcpm_file_line_table_init();
  
  tdcpm_init_defs();
  /* tdcpm_var_name_init(); */

  tdcpm_struct_init();

  if (argc >= 2 && strcmp(argv[1],"--manual") == 0)
    tdcpm_test_struct();
  if (argc >= 2 && strcmp(argv[1],"--parsed") == 0)
    tdcpm_declare_struct();

  tdcpm_struct_dump_all();

  printf ("===\n");

  lexer_read_fd = STDIN_FILENO;
  
  parse_definitions();

  tdcpm_dump_all_nodes();

  printf ("===\n");

  tdcpm_assign_all_nodes();

  tdcpm_struct_value_dump_all();

  tdcpm_string_table_print_stats(_tdcpm_parse_string_idents);

  return 0;
}
