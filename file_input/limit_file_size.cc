/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "limit_file_size.hh"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void limit_file_size::parse_open_first_file(const char *filename)
{
  /*
  char *_filename_base;
  int   _filename_no;
  */

  // If the filename contains digit(s) before the dot, they are used as
  // the initial number, otherwise, we add an _%d and start with 1

  const char *last_slash = strrchr(filename,'/');

  if (!last_slash)
    last_slash = filename;

  // Only look for a dot after the last slash; use the last dot
  const char *dot = strrchr(last_slash,'.');

  // If no dot, we assume dot is at the end
  if (!dot)
    dot = last_slash + strlen(last_slash);

  // If we find an occurance of '.lmd', that takes precedence.
  const char *dot_lmd = strstr(last_slash, ".lmd");

  while (dot_lmd)
    {
      const char *next_dot_lmd = strstr(dot_lmd+1, ".lmd");

      if (!next_dot_lmd)
	break;
      dot_lmd = next_dot_lmd;
    }

  if (dot_lmd)
    dot = dot_lmd;

  const char *digit = dot;

  while (digit > last_slash &&
	 isdigit(digit[-1]))
    digit--;

  const char *format;

  format = "%0*d";

  if (isdigit(digit[0]))
    {
      // size of field to use
      _filename_no_len = (int) (dot - digit);
      _filename_no = atoi(digit);
    }
  else
    {
      // No digits found, use defaults:

      _filename_no_len = 4;
      _filename_no = 1;

      if (dot[-1] != '_' && dot[-1] != '-')
	{
	  // Name did not end with underscore or dash, add an underscore
	  format = "_%0*d";
	}
    }

  _filename_base = new char[strlen(filename) + 5 + 1]; // "_" "%0*d" "\0"

  strncpy(_filename_base,filename,(size_t) (digit-filename));
  _filename_base[digit-filename] = 0;
  strcat(_filename_base,format);
  strcat(_filename_base,dot);

  assert(strlen(_filename_base) < strlen(filename) + 5 + 1);

  INFO(0,"Automatic filename: %s (first=%d) (w=%d)",
       _filename_base,_filename_no,_filename_no_len);

  change_file();
}

