/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2018  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "parse_util.hh"
#include "error.hh"

#include <string.h>
#include <stdlib.h>

uint64 parse_size_postfix(const char *post,const char *allowed,
			  const char *error_name,bool fit32bits)
{
  char *size_end;
  uint64 size = (uint64) strtol(post,&size_end,10);

  if (*size_end == 0)
    return size;

  if (strchr(allowed,*size_end) != NULL)
    {
      if (strcmp(size_end,"k") == 0)
	{ size *= 1000; goto success; }
      else if (strcmp(size_end,"ki") == 0)
	{ size <<= 10; goto success; }
      else if (strcmp(size_end,"M") == 0)
	{ size *= 1000000; goto success; }
      else if (strcmp(size_end,"Mi") == 0)
	{ size <<= 20; goto success; }
      else if (strcmp(size_end,"G") == 0)
	{ size *= 1000000000; goto success; }
      else if (strcmp(size_end,"Gi") == 0)
	{ size <<= 30; goto success; }
    }

  ERROR("%s request malformed (number[%s]): %s",error_name,allowed,post);

  // no return needed :-)

 success:
  if (fit32bits && size > 0xffffffff)
    ERROR("%s request too large (%lld): post",error_name,size);

  return size;
}

