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

#include "file_line.hh"
#include "dumper.hh"

lineno_map *_last_lineno_map  = NULL;
lineno_map *_first_lineno_map = NULL;;

void print_lineno(FILE* fid,int internal)
{
  lineno_map *map = _last_lineno_map;

  if (!map)
    {
      fprintf (fid,"BEFORE_START:0:");
      return;
    }

  while (internal < map->_internal)
    {
      map = map->_prev;
      if (!map)
	{
	  fprintf (fid,"LINE_NO_TOO_EARLY(%d):0:",internal);
	  return;
	}
    }

  fprintf(fid,"%s:%d:",map->_file,map->_line + internal - map->_internal);
}


#ifdef UCESB_SRC
void generate_locations()
{
  print_header("LOCATIONS",
	       "File and line locations from the parsed specification files.");

  printf ("// It's left to the compiler to only store one copy of each\n"
	  "// unique string.\n"
	  "\n");

  printf ("location spec_locations[] =\n"
	  "{ \n");
  for (lineno_map *map = _first_lineno_map; map; map = map->_next)
    {
      printf ("  { %d, %d, \"%s\" },\n",
	      map->_internal,
	      map->_line,
	      map->_file);
    }
  printf ("};\n");

  print_footer("LOCATIONS");
}
#endif
