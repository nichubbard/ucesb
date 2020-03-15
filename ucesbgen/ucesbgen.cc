/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
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
#include <stdlib.h>

#include "definitions.hh"
#include "parse_error.hh"

#include "signal_errors.hh"

extern int lexer_read_fd;

bool parse_definitions();

void usage()
{
  printf ("ucesbgen\n"
	  "usage ucesbgen\n");
  printf ("    ");
}

int main(int /*argc*/,char *argv[])
{
  lexer_read_fd = 0; // read from stdin

  setup_segfault_coredump(argv[0]);

  if (!parse_definitions())
    ERROR("%s: Aborting!\n",argv[0]);

  map_definitions();

  dump_definitions();

  check_consistency();

  generate_unpack_code();

  generate_signals();

  generate_locations();

  return 0;
}

