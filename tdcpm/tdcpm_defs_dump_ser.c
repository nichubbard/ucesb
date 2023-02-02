/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2023  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "tdcpm_defs.h"
#include "tdcpm_defs_struct.h"
#include "tdcpm_error.h"
#include "tdcpm_serialize_util.h"

typedef struct tdcpm_deserialize_info_t
{
  uint32_t  *_cur;
  uint32_t  *_end;  
} tdcpm_deserialize_info;

#define TDCPM_DESER_UINT32(deser) (*((deser)->_cur++))

#define TDCPM_DESERIALIZE_UINT64(deser, value) do { \
    uint32_t __hi = TDCPM_DESER_UINT32(deser);	    \
    uint32_t __lo = TDCPM_DESER_UINT32(deser);	    \
    value = (((uint64_t) __hi) << 32) | __lo;	    \
  } while (0)

#define TDCPM_DESERIALIZE_DOUBLE(deser, value) do { \
    tdcpm_pun_double_uint64 pun;		    \
    TDCPM_DESERIALIZE_UINT64(deser, pun._u64);	    \
    value = pun._dbl;				    \
  } while (0)

void tdcpm_dump_ser_nodes(int indent, tdcpm_deserialize_info *deser);

void tdcpm_dump_ser_var_name(int no_index_dot,
			     tdcpm_deserialize_info *deser)
{
  uint32_t i;
  uint32_t num_parts;

  num_parts = TDCPM_DESER_UINT32(deser);

  /* printf ("[ {%p} ]\n", v); */

  for (i = 0; i < num_parts; i++)
    {
      uint32_t part;

      part = TDCPM_DESER_UINT32(deser);
      
      if (part & TDCPM_VAR_NAME_PART_FLAG_NAME)
	{
	  tdcpm_string_index name_idx;
	  const char *name;

	  name_idx =
	    part & (~TDCPM_VAR_NAME_PART_FLAG_NAME);

	  name =
	    tdcpm_string_table_get(_tdcpm_parse_string_idents,
				   /* _tdcpm_var_name_strings, */
				   name_idx);

	  if (i != 0)
	    printf (".");	  
	  printf ("%s", name);
	}
      else
	{
	  if (i == 0 && !no_index_dot)
	    printf (".");	  
	  printf ("(%d)", part + 1);
	}
    }
}

void tdcpm_dump_ser_unit(tdcpm_unit_index unit_idx)
{
  char str[128];
  size_t n;

  n = tdcpm_unit_to_string(str, sizeof (str), unit_idx);

  if (n >= sizeof (str))
    {
      /* This could be fixed, but - really? */
      TDCPM_ERROR("Unit too long for string.");
    }

  printf ("%s", str);
}

void tdcpm_dump_ser_tspec(tdcpm_tspec_index tspec_idx)
{
  char str[128];
  size_t n;

  n = tdcpm_tspec_to_string(str, sizeof (str), tspec_idx);

  if (n >= sizeof (str))
    {
      /* This could be fixed, but - really? */
      TDCPM_ERROR("Time specifier too long for string.");
    }

  printf ("%s", str);
}

void tdcpm_dump_ser_vect_loop(int several, uint32_t num,
			      tdcpm_deserialize_info *deser)
{
  uint32_t i;
  int first = 1;
 
  if (num > 1)
    several = 1;

  if (several)
    printf ("{ ");

  for (i = 0; i < num; i++)
    {
      double value;
      tdcpm_unit_index unit_idx;
      tdcpm_tspec_index tspec_idx;

      TDCPM_DESERIALIZE_DOUBLE(deser, value);
      unit_idx  = TDCPM_DESER_UINT32(deser);
      tspec_idx = TDCPM_DESER_UINT32(deser);

      if (!first)
	printf (", ");
      first = 0;
	    
      printf ("%.4g", value);

      if (unit_idx != 0)
	{
	  printf (" ");
	  tdcpm_dump_ser_unit(unit_idx);
	}
      if (tspec_idx != 0)
	{
	  printf (" @ ");
	  tdcpm_dump_ser_tspec(tspec_idx);
	}
    }
	
  if (several)
    printf (" }");
}

void tdcpm_dump_ser_vect(int several,
			 tdcpm_deserialize_info *deser)
{
  uint32_t num;

  num = TDCPM_DESER_UINT32(deser);

  tdcpm_dump_ser_vect_loop(several, num, deser);
}


void tdcpm_dump_ser_table(int indent,
			  tdcpm_deserialize_info *deser)
{
  uint32_t columns;
  uint32_t rows;
  int has_units;
  uint32_t i;
  int first;

  columns   = TDCPM_DESER_UINT32(deser);
  rows      = TDCPM_DESER_UINT32(deser);
  has_units = TDCPM_DESER_UINT32(deser);

  printf ("%*s[ ", indent, "");
  first = 1;
  for (i = 0; i < columns; i++)
    {
      tdcpm_tspec_index tspec_idx;
      
      if (!first)
	printf (", ");
      first = 0;

      tdcpm_dump_ser_var_name(0, deser);
      tspec_idx = TDCPM_DESER_UINT32(deser);
      
      if (tspec_idx != 0)
	{
	  printf (" @ ");
	  tdcpm_dump_ser_tspec(tspec_idx);
	}
    }
  printf (" ]\n");

  if (has_units)
    {
      printf ("%*s[[ ", indent, "");
      first = 1;
      for (i = 0; i < columns; i++)
	{
	  double value;
	  tdcpm_unit_index unit_idx;

	  TDCPM_DESERIALIZE_DOUBLE(deser, value);
	  unit_idx = TDCPM_DESER_UINT32(deser);

	  if (!first)
	    printf (", ");
	  first = 0;

	  if (value != 1)
	    printf ("%.4g ", value);

	  tdcpm_dump_ser_unit(unit_idx);
	}
      printf (" ]]\n");
    }
  
  for (i = 0; i < rows; i++)
    {
      int has_var_name;

      has_var_name = TDCPM_DESER_UINT32(deser);

      printf ("%*s", indent, "");
      if (has_var_name)
	{
	  tdcpm_dump_ser_var_name(1, deser);
	  printf (": ");
	}
      
      tdcpm_dump_ser_vect_loop(1, columns, deser);
      
      printf ("\n");
    }
}

void tdcpm_dump_ser_node(int indent, tdcpm_deserialize_info *deser)
{
  uint32_t type;

  printf ("%*s", indent, "");

  type = TDCPM_DESER_UINT32(deser);

  if (type != TDCPM_NODE_TYPE_VALID)
    {
      tdcpm_dump_ser_var_name(0, deser);

      printf (" = ");
    }

  switch (type)
    {
    case TDCPM_NODE_TYPE_VECT:
      {
	tdcpm_dump_ser_vect(0, deser);

	printf (";\n");
      }      
      break;
    case TDCPM_NODE_TYPE_TABLE:
      {
	printf ("\n");
	tdcpm_dump_ser_table(indent + 2, deser);
	printf ("\n");
      }
      break;
    case TDCPM_NODE_TYPE_SUB_NODE:
      {
	printf ("{\n");

	tdcpm_dump_ser_nodes(indent + 2, deser);

	printf ("%*s}\n", indent, "");
      }
      break;
    case TDCPM_NODE_TYPE_VALID:
      {
	tdcpm_tspec_index tspec_idx_from, tspec_idx_to;
	
	/* Jump past the size specifier. */
	(void) TDCPM_DESER_UINT32(deser);
	(void) TDCPM_DESER_UINT32(deser);

	tspec_idx_from = TDCPM_DESER_UINT32(deser);
	tspec_idx_to   = TDCPM_DESER_UINT32(deser);
	
	printf ("valid( ");
	tdcpm_dump_ser_tspec(tspec_idx_from);
	printf (", ");
	tdcpm_dump_ser_tspec(tspec_idx_to);
	printf (")\n");
	
	printf ("%*s{\n", indent, "");

	tdcpm_dump_ser_nodes(indent + 2, deser);

	printf ("%*s}\n", indent, "");
      }
      break;
    default:
      TDCPM_ERROR("Unknown node type (%d).\n",
		  type);
      break;
    }
}


void tdcpm_dump_ser_nodes(int indent, tdcpm_deserialize_info *deser)
{
  uint32_t num, i;

  num = TDCPM_DESER_UINT32(deser);

  for (i = 0; i < num; i++)
    {
      tdcpm_dump_ser_node(indent, deser); 
    }
}

void tdcpm_dump_ser_all_nodes(void)
{
  tdcpm_deserialize_info deser;

  deser._cur = _tdcpm_all_nodes_serialized._buf;
  deser._end = deser._cur + _tdcpm_all_nodes_serialized._offset;
  
  tdcpm_dump_ser_nodes(0, &deser);
}
