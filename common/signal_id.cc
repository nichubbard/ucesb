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

#include "signal_id.hh"

#if defined(UCESB_SRC) || defined(PSDC_SRC)
#include "parse_error.hh"
#include "file_line.hh"
#else
#include "error.hh"
#endif

#include "str_set.hh"

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "strndup.hh"

void dissect_name(const file_line &loc,
		  const char *name,
		  signal_id& id)
{
  // We split the name up into a leading ascii name,
  // after that, into a list of integers and ascii strings,
  // which are separated either by being the other kind of
  // character, or an underscore.

  const char *start = name;

  if (!isalpha((int) *start))
    ERROR_LOC(loc,"Signal name not starting with an character (%c) (%s).",
	      *start,name);

  while(*start != 0)
    {
      if (*start == '_')
	start++;
      else if (isalpha((int) *start))
	{
	  const char *end = start+1;
	  while (isalpha((int) *end))
	    end++;
	  char *p = strndup(start,(size_t) (end-start));
	  // printf ("p:<%s>(%s,%s)\n",p,start,end);

	  const char *name2 = find_str_identifiers(p);
	  free(p);
	  start = end;

	  id.push_back(name2);
	}
      else if (isdigit((int) *start))
	{
	  const char *end = start+1;
	  while (isdigit((int) *end))
	    end++;
	  char *end2;
	  int index = (int) strtol(start,&end2,10);
	  if (end2 != end)
	    ERROR_LOC(loc,"Error converting signal name integer (%s).",name);
	  if (index <= 0)
	    ERROR_LOC(loc,"String-like variable indexing starts at 1 (%s).",name);
	  if (index >= 0x1000000)
	    ERROR_LOC(loc,"Cowardly refusing index >= 64K (%s).",name);
	  start = end;

	  id.push_back(index-1); // -1 since we internally are 0-based
	}
      else
	ERROR_LOC(loc,"Unrecognized character (%c) in signal name (%s).",
		  *start,name);
    }
}

void signal_id::take_over(sig_part_ptr_vector *src)
{
  sig_part_ptr_vector::iterator iter;

  for (iter = src->begin(); iter != src->end(); ++iter)
    {
      sig_part *i = *iter;
      _parts.push_back(*i);
      delete i;
    }
  delete src;
}

int sig_part::format(char *str,size_t size) const
{
  if (_type & SIG_PART_NAME)
    return snprintf(str,size,".%s",_id._name);
  if (_type & SIG_PART_INDEX)
    return snprintf(str,size,"[%s%s%d]",
		    (_type & SIG_PART_INDEX_ZZP ? "z:" : ""),
		    (_type & SIG_PART_MULTI_ENTRY ? "m:" : ""),
		    _id._index);

  return snprintf(str,size,".<UNKNOWN>");
}

size_t signal_id::format(char *str,size_t size) const
{
  char *ptr = str;
  *ptr = '\0';

  sig_part_vector::const_iterator part;

  for (part = _parts.begin(); part != _parts.end(); ++part)
    {
      int length = part->format(ptr,size > 0 ? size : 0);

      ptr  += length;
      size -= (size_t) length;
    }
  return (size_t) (ptr - str);
}

int sig_part::format_paw(char *str,size_t size) const
{
  if (_type & SIG_PART_NAME)
    return snprintf(str,size,"%s",_id._name);
  if (_type & SIG_PART_INDEX)
    {
      if (_type & SIG_PART_INDEX_LIMIT)
	return snprintf(str,size,"%d",_id._index);
      else
	return snprintf(str,size,"%d",_id._index+1);
    }

  return snprintf(str,size,"<UNKNOWN>");
}

size_t signal_id::format_paw(char *str,size_t size,
			     size_t omit_index,size_t omit_index2) const
{
  char *ptr = str;
  *ptr = '\0';

  sig_part_vector::const_iterator part;

  int prev_type = 0;

  for (part = _parts.begin(); part != _parts.end();
       ++part, --omit_index, --omit_index2)
    {
      if (!omit_index || !omit_index2)
	continue; // this index is an array index (thus not part of the name)

      if ((part->_type & (SIG_PART_NAME |
			  SIG_PART_INDEX)) ==
	  (prev_type   & (SIG_PART_NAME |
			  SIG_PART_INDEX)))
	{
	  if (size > 0)
	    {
	      *(ptr++) = '_';
	      size--;
	    }
	}
      prev_type = part->_type;

      int length = part->format_paw(ptr,size > 0 ? size : 0);

      ptr  += length;
      size -= (size_t) length;
    }
  return (size_t) (ptr - str);
}

bool signal_id::difference(const signal_id &rhs,
			   int &diff_index,
			   int &diff_length) const
{
  if (_parts.size() != rhs._parts.size())
    return false;

  diff_index = -1;
  diff_length = 0;

  for (unsigned int i = 0; i < _parts.size(); i++)
    {
      if (_parts[i]._type != rhs._parts[i]._type)
	return false;

      if (_parts[i]._type & SIG_PART_NAME)
	if (_parts[i]._id._name != rhs._parts[i]._id._name)
	  return false;

      if (_parts[i]._type & SIG_PART_INDEX)
	if (_parts[i]._id._index != rhs._parts[i]._id._index)
	  {
	    if (diff_index != -1)
	      return false; // only one difference allowed

	    diff_index  = (int) i;
	    diff_length = rhs._parts[i]._id._index - _parts[i]._id._index;
	  }
    }

  return true;
}

char *signal_id::generate_indexed(int diff_index,
				  int diff_offset)
{
  // figure out how much length we may need

  size_t length = _parts.size(); // for "_"
  for (unsigned int i = 0; i < _parts.size(); i++)
    {
      if (_parts[i]._type & SIG_PART_NAME)
	length += strlen(_parts[i]._id._name);
      if (_parts[i]._type & SIG_PART_INDEX)
	length += 16; // maximum digits, hmm...
    }

  char *result = (char *) malloc(length);
  char *dest = result;

  int prev_type = 0;

  for (unsigned int i = 0; i < _parts.size(); i++)
    {
      if ((prev_type       & (SIG_PART_NAME | SIG_PART_INDEX)) ==
	  (_parts[i]._type & (SIG_PART_NAME | SIG_PART_INDEX)))
	dest += sprintf(dest,"_");

      if (_parts[i]._type & SIG_PART_NAME)
	dest += sprintf(dest,"%s",_parts[i]._id._name);

      if (_parts[i]._type & SIG_PART_INDEX)
	dest += sprintf(dest,"%d",_parts[i]._id._index +
			((unsigned int) diff_index == i ? diff_offset : 0) + 1);

      prev_type = _parts[i]._type;
    }

  return result;
}


