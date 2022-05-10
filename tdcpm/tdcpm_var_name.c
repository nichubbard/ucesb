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
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "tdcpm_var_name.h"
#include "tdcpm_string_table.h"
#include "tdcpm_malloc.h"

/*
tdcpm_string_table *_tdcpm_var_name_strings = NULL;

void tdcpm_var_name_init(void)
{
  _tdcpm_var_name_strings = tdcpm_string_table_init();
}
*/

tdcpm_var_name *tdcpm_var_name_new()
{
  tdcpm_var_name *base;

  base = (tdcpm_var_name *) TDCPM_MALLOC(tdcpm_var_name);

  base->_num_parts = 0;
   
  return base;
}

tdcpm_var_name *tdcpm_var_name_alloc(tdcpm_var_name *base, int extra)
{
  size_t sz = sizeof (tdcpm_var_name) +
    (base->_num_parts + extra - 1) * sizeof (base->_parts[0]);
  /* - 1 above since structure has [1] item by default. */

  /* TODO: we should not reallocate all the time... */

  base = (tdcpm_var_name *) tdcpm_realloc (base, sz, "tdcpm_var_name");

  return base;
}

tdcpm_var_name *tdcpm_var_name_index(tdcpm_var_name *base, int index)
{
  base = tdcpm_var_name_alloc(base, 1);

  base->_parts[base->_num_parts] = index;
  base->_num_parts++;

  /* printf ("idx: %d\n", index); */

  return base;  
}

tdcpm_var_name *tdcpm_var_name_name(tdcpm_var_name *base,
				    tdcpm_string_index name_idx)
{
  base = tdcpm_var_name_alloc(base, 1);

  base->_parts[base->_num_parts] =
    TDCPM_VAR_NAME_PART_FLAG_NAME | name_idx;
  base->_num_parts++;

  /* printf ("nidx: %d\n", name_idx); */

  return base;  
}

tdcpm_var_name *tdcpm_var_name_off(tdcpm_var_name *base, int offset)
{
  int i;
  
  for (i = 0; i < base->_num_parts; i++)
    {
      if (((int) base->_parts[i]) - offset < 0)
	{
	  fprintf (stderr, "Negative index (%d) not allowed.\n",
		   base->_parts[i]);
	  exit(1);
	}
      base->_parts[i] -= offset;
    }

  return base;
}

tdcpm_var_name *tdcpm_var_name_join(tdcpm_var_name *base,
				    tdcpm_var_name *add)
{
  if (add == NULL)
    return base;

  base = tdcpm_var_name_alloc(base, add->_num_parts);

  memcpy(base->_parts + base->_num_parts,
	 add->_parts, add->_num_parts * sizeof (add->_parts[0]));

  /* {
    int i;

    printf ("[ {%p,%p} ", base, add);
    for (i = 0; i < base->_num_parts; i++)
      printf ("%08x,",base->_parts[i]);
    printf (" + ");
    for (i = 0; i < add->_num_parts; i++)
      printf ("%08x,",add->_parts[i]);
    printf (" -> ");
    for (i = 0; i < base->_num_parts + add->_num_parts; i++)
      printf ("%08x,",base->_parts[i]);
    printf ("]\n");
  } */
  
  base->_num_parts += add->_num_parts;

  free (add);

  return base;
}

void tdcpm_var_name_tmp_join(tdcpm_var_name_tmp *base,
                             tdcpm_var_name *add)
{
  int num_parts = base->_num_parts + add->_num_parts;
    
  if (num_parts > base->_num_alloc)
    {
      if (!base->_num_alloc)
	base->_num_alloc = 16;
      
      while (num_parts > base->_num_alloc)
	base->_num_alloc *= 2;

      base->_parts = (uint32_t *)
	tdcpm_realloc (base->_parts, base->_num_alloc * sizeof (uint32_t),
		       "var_name parts");
    }

  memcpy(base->_parts + base->_num_parts,
	 add->_parts, add->_num_parts * sizeof (add->_parts[0]));

  base->_num_parts = num_parts;
}
