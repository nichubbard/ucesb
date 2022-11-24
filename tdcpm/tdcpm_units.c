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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "tdcpm_units.h"
#include "tdcpm_string_table.h"
#include "tdcpm_hash_table.h"
#include "tdcpm_malloc.h"
#include "tdcpm_defs.h"

/* Unit with no units (dimensionless). */
tdcpm_unit_index _tdcpm_unit_none = 0;

/* Has no allocated pointer, so will not be free'd. */
tdcpm_unit_build _tdcpm_unit_build_none = { 0, { { { 0, 0 }, { 0, 0 } } } };

/* Since units will recur many times over, they are stored in a table
 * of units, essentially like the strings.  A unit simply references
 * the entry with an index.
 *
 * Each entry contain a list of its constituent units, together with
 * exponents.  It also contains a pointer to a corresponding
 * (canonical) unit with no prefixes, and the magnitude change needed
 * to conform to that unit.
 *
 */



typedef struct tdcpm_unit_item_t
{
  /* Canonical unit, and magnification from that. */
  uint32_t             _canonical;
  int32_t              _mag_canonical;
  /* The unit itself. */
  tdcpm_unit_build     _unit;
  
} tdcpm_unit_item;

typedef struct tdcpm_unit_table_t
{
  tdcpm_unit_item        *_units;

  uint32_t                _num_units;
  size_t                  _alloc_units;

  tdcpm_hash_table        _hash_table;

} tdcpm_unit_table;

tdcpm_unit_table *_tdcpm_unit_table = NULL;


/**************************************************************************/

void tdcpm_unit_table_init(void)
{
  tdcpm_unit_table *table;

  table = (tdcpm_unit_table *) TDCPM_MALLOC(tdcpm_unit_table);
  
  table->_units = NULL;
  
  table->_num_units = 0;
  table->_alloc_units = 0;

  tdcpm_hash_table_init(&(table->_hash_table));

  _tdcpm_unit_table = table;

  /* Insert the empty unit (gets offset 0). */

  _tdcpm_unit_none = tdcpm_unit_make_full(&_tdcpm_unit_build_none, 0);
}

/**************************************************************************/

uint32_t tdcpm_unit_table_hash_calc(tdcpm_unit_build *unit)
{
  uint32_t i;
  uint64_t x = 0;

  tdcpm_unit_item_part *parts;

  parts = TDCPM_UNIT_PARTS(unit);
  
  for (i = 0; i < unit->_num_parts; i++)
    {
      uint64_t contrib;

      contrib =
	(((uint64_t) parts[i]._str_idx) << 32) |
	( (uint64_t) parts[i]._exponent);

      TDCPM_HASH_ROUND(x, contrib);
    }

  TDCPM_HASH_MUL(x);

  return TDCPM_HASH_RET32(x);
}

#if 0
uint32_t tdcpm_unit_table_insert(tdcpm_unit_item *unit,
				  int free_if_exist)
{
  uint32_t index;
  uint32_t hash;
  size_t   hash_i;
  tdcpm_unit_table *table = _tdcpm_unit_table;
  
  /* Does the unit exist in the hash table? */
  hash = tdcpm_unit_table_hash_calc(unit);

  /* printf ("hash: %08" PRIx32 " '%.*s'\n", hash); */

  _tdcpm_unit_table_stats._lookups++;
  
  if (table->_hash_size)
    {
      hash_i = hash & (table->_hash_size - 1);

      for ( ; ; hash_i = ((hash_i + 1) & (table->_hash_size - 1)))
	{
	  tdcpm_unit_table_hash *entry = &(table->_hash[hash_i]);
	  tdcpm_unit_item *ref;
	  uint32_t i;
	  
	  _tdcpm_unit_table_stats._hash_lookups++;

	  if (entry->_index == (uint32_t) -1)
	    break; /* Item is unused, abort search. */

	  _tdcpm_unit_table_stats._hash_match++;

	  if (entry->_hash != hash)
	    continue; /* Wrong hash, try next one. */

	  _tdcpm_unit_table_stats._full_match++;

	  /* The entry exist and hash matches...  Is it a perfect match? */
	  ref = table->_units[entry->_index];

	  if (ref->_num_parts != unit->_num_parts)
	    continue; /* Wrong number of parts. */

	  for (i = 0; i < unit->_num_parts; i++)
	    {
	      if (unit->_parts[i]._str_idx  !=
		  ref->_parts[i]._str_idx ||
		  unit->_parts[i]._exponent !=
		  ref->_parts[i]._exponent)
		goto mismatch;
	    }

	  if (free_if_exist)
	    free (unit);
	  
	  /* Perfect match. */
	  return entry->_index;
	  
	mismatch:
	  ;
	  /* Try next... */
	}
    }

  /* Entry did not exist. */

  if (!free_if_exist)
    {
      tdcpm_unit_item *copy;
      size_t sz;
      
      /* We cannot use the allocated unit.  Make a copy. */
      sz = sizeof (tdcpm_unit_item) +
	(unit->_num_parts - 1) * sizeof (tdcpm_unit_item_part);

      copy = (tdcpm_unit_item *) tdcpm_malloc(sz, "tdcpm_unit_item");

      memcpy(copy, unit, sz);

      unit = copy;
    }
  
  /* And the hash table. */
  hash_i = hash & (table->_hash_size - 1);

  for ( ; ; hash_i = ((hash_i + 1) & (table->_hash_size - 1)))
    {
      tdcpm_unit_table_hash *entry = &(table->_hash[hash_i]);

      if (entry->_index == (uint32_t) -1)
	{
	  /* Item is not occupied. */
	  entry->_hash  = hash;
	  entry->_index = index;
	  break;
	}
      /* Try next... */
    }

  return index;
}
#endif

/**************************************************************************/

void tdcpm_unit_build_free(tdcpm_unit_build *a)
{
  if (a->_num_parts > 2 && a->_parts._mem._own)
    free(a->_parts._mem._ptr);  
}

void tdcpm_unit_build_no_own(tdcpm_unit_build *a)
{
  if (a->_num_parts > 2)
    a->_parts._mem._own = 0;
}

void tdcpm_unit_build_new(tdcpm_unit_build *dest,
			  tdcpm_string_index str_idx,
			  int exponent)
{
  /* By definition, it is a unit with one part, so no allocation. */
  dest->_num_parts = 1;
  dest->_parts._direct[0]._str_idx = str_idx;
  dest->_parts._direct[0]._exponent = exponent;
}

void tdcpm_unit_build_mul(tdcpm_unit_build *dest,
                          tdcpm_unit_build *a, tdcpm_unit_build *b,
                          int mul)
{
  tdcpm_unit_item_part *dest_parts;
  tdcpm_unit_item_part *a_parts;
  tdcpm_unit_item_part *b_parts;
  uint32_t i, j;
  
  dest->_num_parts = a->_num_parts + b->_num_parts;

  if (dest->_num_parts > 2)
    {
      dest_parts = dest->_parts._mem._ptr = (tdcpm_unit_item_part *)
	tdcpm_malloc(dest->_num_parts * sizeof (tdcpm_unit_item_part),
		     "unit parts");
      dest->_parts._mem._own = 1;
    }
  else
    dest_parts = dest->_parts._direct;

  a_parts = TDCPM_UNIT_PARTS(a);
  b_parts = TDCPM_UNIT_PARTS(b);

  for (i = 0; i < a->_num_parts; i++)
    dest_parts[i] = a_parts[i];

  for (j = 0; j < b->_num_parts; j++, i++)
    {
      dest_parts[i] = b_parts[j];
      dest_parts[i]._exponent *= mul;
    }

  /*printf ("%d+%d->%d\n", a->_num_parts, b->_num_parts, dest->_num_parts);*/

  /* The sources are no longer used, so free any allocated memory. */
  tdcpm_unit_build_free(a);
  tdcpm_unit_build_free(b);
}

/**************************************************************************/

int tdcpm_compare_unit_build(const void *compare_info,
			     uint32_t index,
			     const void *item)
{
  const tdcpm_unit_table *table = (tdcpm_unit_table *) compare_info;
  const tdcpm_unit_build *unit = (tdcpm_unit_build *) item;
  const tdcpm_unit_build *ref = &(table->_units[index]._unit);
  uint32_t i;

  const tdcpm_unit_item_part *a_parts;
  const tdcpm_unit_item_part *b_parts;

  if (unit->_num_parts != ref->_num_parts)
    return 0; /* No match. */

  a_parts = TDCPM_UNIT_PARTS(unit);
  b_parts = TDCPM_UNIT_PARTS(ref);

  for (i = 0; i < unit->_num_parts; i++)
    {
      if (a_parts[i]._str_idx  != b_parts[i]._str_idx ||
	  a_parts[i]._exponent != b_parts[i]._exponent)
	return 0; /* No match. */
    }

  return 1; /* Match. */
}

/**************************************************************************/

#define TDCPM_UNKNOWN_PREFIX -127

int tdcpm_get_unit_prefix(char prefix)
{
  switch (prefix)
    {
    case 'y': return -24;
    case 'z': return -21;
    case 'a': return -18;
    case 'f': return -15;
    case 'p': return -12;
    case 'n': return -9;
    case 'u': return -6;
    case 'm': return -3;
    case 'c': return -2;
    case 'd': return -1;

    /* case '#': return  0; */

    case 'k': return  3;
    case 'M': return  6;
    case 'G': return  9;
    case 'T': return  12;
    case 'P': return  15;
    case 'E': return  18;
    case 'Z': return  21;
    case 'Y': return  24;
    }
  return TDCPM_UNKNOWN_PREFIX;
}

tdcpm_string_index tdcpm_unit_strip_prefix(tdcpm_string_index str_idx,
					   int *prefix_mag)
{
  const char *name;
  int prefix;

  name = tdcpm_string_table_get(_tdcpm_parse_string_idents,
				str_idx);

  /* Does the name start with a prefix, and has more than one character? */

  if (name[0] != 0 &&
      name[1] != 0 &&
      (prefix = tdcpm_get_unit_prefix(name[0])) != TDCPM_UNKNOWN_PREFIX)
    {
      tdcpm_string_index new_idx;
      
      *prefix_mag = prefix;

      /* Find the string without the prefix. */
      new_idx = tdcpm_string_table_insert(_tdcpm_parse_string_idents,
					  name + 1, strlen(name + 1));
      
      return new_idx;
    }

  *prefix_mag = 0;
  return str_idx;
}

/**************************************************************************/

int tdcpm_compare_unit_item_part(const void *a, const void *b)
{
  const tdcpm_unit_item_part *part_a = (const tdcpm_unit_item_part *) a;
  const tdcpm_unit_item_part *part_b = (const tdcpm_unit_item_part *) b;

  if (part_a->_str_idx < part_b->_str_idx)
    return -1;
  return part_a->_str_idx > part_b->_str_idx;
}

/**************************************************************************/

void tdcpm_unit_make_canonical(tdcpm_unit_item *item)
{
  tdcpm_unit_build dest;
  tdcpm_unit_item_part *dest_parts;
  tdcpm_unit_item_part *a_parts;
  uint32_t i, j;
  int total_mag = 0;
  
  /* To make a canonical unit, we must go through each component, and
   * if necessary strip the prefix.
   */

  dest._num_parts = item->_unit._num_parts;

  if (dest._num_parts > 2)
    {
      dest_parts = dest._parts._mem._ptr = (tdcpm_unit_item_part *)
	tdcpm_malloc(dest._num_parts * sizeof (tdcpm_unit_item_part),
		     "unit parts");
      dest._parts._mem._own = 1;
    }
  else
    dest_parts = dest._parts._direct;

  /* Strip any prefixes. */
  
  a_parts = TDCPM_UNIT_PARTS(&(item->_unit));

  for (i = 0; i < item->_unit._num_parts; i++)
    {
      int prefix_mag;
      
      dest_parts[i]._str_idx =
	tdcpm_unit_strip_prefix(a_parts[i]._str_idx, &prefix_mag);
      
      prefix_mag *= a_parts[i]._exponent;
      dest_parts[i]._exponent = a_parts[i]._exponent;

      total_mag += prefix_mag;
    }

  /* We now must look for duplicate units, and combine by adding
   * exponents.
   *
   * The parts must be sorted, such that they are always found
   * as the same, no matter what.
   */

  qsort (dest_parts, dest._num_parts, sizeof (dest_parts[0]),
	 tdcpm_compare_unit_item_part);

  /* The first item does not need to be merged with any earlier,
   * so already handled.
   */
  i = 0;

  for (j = 1; j < dest._num_parts; j++)
    {
      if (dest_parts[i]._str_idx == dest_parts[j]._str_idx)
	{
	  /* Combine with previous item. */
	  dest_parts[i]._exponent += dest_parts[j]._exponent;
	}
      else
	{
	  /* It is the first item, or an item with a new unit. */
	  /* If the final exponent for the previous item is 0,
	   * then it is discarded.
	   */
	  if (dest_parts[i]._exponent != 0)
	    i++; /* Keep. */
	  dest_parts[i]._str_idx  = dest_parts[j]._str_idx;
	  dest_parts[i]._exponent = dest_parts[j]._exponent;
	}
    }

  if (dest_parts[i]._exponent != 0)
    i++; /* Keep the last item. */

  /* We now have i items total. */
  dest._num_parts = i;

  if (dest._num_parts <= 2 &&
      dest_parts != dest._parts._direct)
    {
      /* We went from having > 2 parts to <= 2 parts. */
      memcpy (dest._parts._direct, dest_parts,
	      dest._num_parts * sizeof (dest_parts[0]));
      free (dest_parts);
    }

  item->_canonical = tdcpm_unit_make_full(&dest, 0);
  item->_mag_canonical = total_mag;

  /* tdcpm_dump_unit(item->_canonical);
     printf ("\n"); */
}

/**************************************************************************/

tdcpm_unit_index tdcpm_unit_make_full(tdcpm_unit_build *src,
				      int make_canonical)
{
  uint32_t index;
  uint32_t hash;
  tdcpm_unit_table *table = _tdcpm_unit_table;
  tdcpm_unit_item *item;

  /* A full unit means that it can be referenced by index from the
   * hash table of units.  First find out if it exist in the hash
   * table.
   */

  hash = tdcpm_unit_table_hash_calc(src);

  index = tdcpm_hash_table_find(&(table->_hash_table),
				src, hash,
				table, tdcpm_compare_unit_build);

  if (index != (uint32_t) -1)
    {
      tdcpm_unit_build_free(src);
      return index;
    }

  /* Entry did not exist. */

  if (table->_alloc_units <= table->_num_units)
    {
      if (!table->_alloc_units)
	table->_alloc_units = 16;

      table->_alloc_units *= 2;

      table->_units = (tdcpm_unit_item *)
	tdcpm_realloc (table->_units,
		       table->_alloc_units * sizeof (table->_units[0]),
		       "tdcpm_unit_item");

      tdcpm_hash_table_realloc(&(table->_hash_table),
                               table->_alloc_units * 2);
    }

  index = table->_num_units++;

  tdcpm_hash_table_insert(&(table->_hash_table), hash, index);
    
  /* We can now use the allocated unit. */
  item = &(table->_units[index]);
  /* Note: if src has an allocated parts pointer, we take it over. */
  item->_unit = *src;

  if (make_canonical)
    {
      tdcpm_unit_make_canonical(item);
    }
  else
    {
      /* We are the canonical, so reference ourselves. */
      item->_canonical     = (tdcpm_unit_index) index;
      item->_mag_canonical = 0;
    }

  return (tdcpm_unit_index) index;
}

/**************************************************************************/

const tdcpm_unit_build *tdcpm_unit_table_get(tdcpm_unit_index unit_idx)
{
  tdcpm_unit_table *table = _tdcpm_unit_table;
  uint32_t index = (uint32_t) unit_idx;

  return &(table->_units[index]._unit);
}

/**************************************************************************/

tdcpm_dbl_unit_build *_tdcpm_parse_catch_unitdef = NULL;

void tdcpm_unit_new_dissect(tdcpm_dbl_unit *dbl_unit,
			    tdcpm_string_index str_idx)
{
  const char *str;
  char *fullexpr;
  tdcpm_dbl_unit_build dbl_unit_build;
  int hasvalue = 0;
  const char *p;

  str = tdcpm_string_table_get(_tdcpm_parse_string_idents, str_idx);
  
  fullexpr = malloc(strlen(str) + 64);

  p = str;
  while (isblank(*p))
    p++;
  if (isdigit(*p))
    hasvalue = 1;

  /* The 'dummy_unit' is never assigned. */
  sprintf (fullexpr, "defunit: dummy_unit = %s%s;\n",
	   hasvalue ? "" : "1 ",
	   str);

  _tdcpm_parse_catch_unitdef = &dbl_unit_build;
  
  _tdcpm_lexer_buf_ptr = fullexpr;
  _tdcpm_lexer_buf_len = strlen(fullexpr);

  parse_definitions();

  _tdcpm_lexer_buf_ptr = NULL;
  _tdcpm_lexer_buf_len = 0;

  _tdcpm_parse_catch_unitdef = NULL;

  free(fullexpr);

  dbl_unit->_value = dbl_unit_build._value;
  dbl_unit->_unit_idx = tdcpm_unit_make_full(&dbl_unit_build._unit_bld, 1);
}

/**************************************************************************/

/* Return the factor needed to make a value in units 'b' the same as
 * is units 'a'.  Error if not the same unit.
 */
int tdcpm_unit_factor(tdcpm_unit_index a_idx, tdcpm_unit_index b_idx,
		      double *factor)
{
  tdcpm_unit_table *table = _tdcpm_unit_table;

  tdcpm_unit_item *item_a;
  tdcpm_unit_item *item_b;

  int mag_a_to_b;

  /* We must find out if we have the same canonical unit. */

  /* First find the entries in the hash table, and if non-existing
   * then fix that.
   */

  item_a = &(table->_units[a_idx]);
  item_b = &(table->_units[b_idx]);

  if (item_a->_canonical != item_b->_canonical)
    return 0;

  mag_a_to_b = item_b->_mag_canonical - item_a->_mag_canonical;

  *factor = pow(10., mag_a_to_b);

  return 1;
}

int tdcpm_unit_build_factor(tdcpm_unit_build *a, tdcpm_unit_build *b,
			    double *factor)
{
  tdcpm_unit_table *table = _tdcpm_unit_table;

  tdcpm_unit_index a_idx;
  tdcpm_unit_index b_idx;

  tdcpm_unit_item *item_a;
  tdcpm_unit_item *item_b;

  int mag_a_to_b;

  /* We must find out if we have the same canonical unit. */

  /* First find the entries in the hash table, and if non-existing
   * then fix that.
   */

  a_idx = tdcpm_unit_make_full(a, 1);
  b_idx = tdcpm_unit_make_full(b, 1);

  item_a = &(table->_units[a_idx]);
  item_b = &(table->_units[b_idx]);

  /* Since the a and/or b units may have been used to add an entry to
   * the hash table, the memory pointer (if > 2 parts) may have been
   * taken over.  And if it was not taken over, it will have been
   * freed.  Make sure to get a (non-owned) pointer to the hash table
   * entry.  Note: it is not a pointer to the unit entry as such, but
   * a pointer to the (> 2) parts memory.  That will not be moved even
   * if the array of units of the hash table is reallocated.
   */

  if (a->_num_parts > 2)
    {
      a->_parts._mem._ptr = item_a->_unit._parts._mem._ptr;
      a->_parts._mem._own = 0;
    }

  if (item_a->_canonical != item_b->_canonical)
    return 0;

  mag_a_to_b = item_b->_mag_canonical - item_a->_mag_canonical;

  *factor = pow(10., mag_a_to_b);

  return 1;
}

/**************************************************************************/

size_t tdcpm_unit_to_string(char *str, size_t n,
			    tdcpm_unit_index unit_idx)
{
  const tdcpm_unit_build *unit;
  const tdcpm_unit_item_part *parts;
  uint32_t i;
  size_t off = 0;
  size_t remain = n;
  size_t total = 0;  /* Total length if str would be long enough. */
  size_t got;

#define TDCPM_UPDATE_OFF_REMAIN			\
  total += got;					\
  if (got > remain)				\
    {						\
      remain = 0;				\
      off += remain;				\
    }						\
  else						\
    {						\
      remain -= got;				\
      off += got;				\
    }

  if (n)
    *str = '\0';

  unit = tdcpm_unit_table_get(unit_idx);

  parts = TDCPM_UNIT_PARTS(unit);

  for (i = 0; i < unit->_num_parts; i++)
    {
      const char *name;

      name = tdcpm_string_table_get(_tdcpm_parse_string_idents,
				    parts[i]._str_idx);

      if (parts[i]._exponent < 0 || i > 0)
	{
	  got = snprintf (str+off, remain,
			  "%s", (parts[i]._exponent < 0 ? "/" : "*"));
	  TDCPM_UPDATE_OFF_REMAIN;
	}

      got = snprintf (str+off, remain,
		      "%s",name);
      TDCPM_UPDATE_OFF_REMAIN;

      if (abs(parts[i]._exponent) != 1)
	{
	  got = snprintf (str+off, remain,
			  "^%d", abs(parts[i]._exponent));
      	  TDCPM_UPDATE_OFF_REMAIN;
	}

      /* printf ("[%d]", parts[i]._exponent); */
    }
  
  /* printf ("UNIT(%d)", (uint32_t) unit_idx); */

  return total;
}

/**************************************************************************/
