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

#include "signal_id_range.hh"
#include "error.hh"

#include "../common/str_set.hh"

#include "../common/strndup.hh"

#include <ctype.h>

void dissect_name_range(const char *name,
			signal_id_range& id)
{
  // We split the name up into a leading ascii name,
  // after that, into a list of integers and ascii strings,
  // which are separated either by being the other kind of
  // character, or an underscore.

  const char *start = name;

  if (!isalpha(*start) && *start != '*' && *start != '?')
    ERROR("Signal name (%s) not starting with an character (%c).",
	  name,*start);

  while(*start != 0)
    {
      if (*start == '_')
	start++;
      else if (*start == '?')
	{
	  start++;
	  id.push_back_wildcard();
	}
      else if (*start == '*')
	{
	  start++;
	  id.push_back_wildcards();
	}
      else if (isalpha(*start))
	{
	  const char *end = start+1;

	  // as a string we take anything starting with a character
	  // it may then continue with underscore or digits, if those
	  // are escaped with an backslash (note, in bash you either
	  // need to give them as a double backslash, or put the command
	  // line in quotes (single or double)

	  for ( ; ; )
	    {
	      // allow characters
	      while (isalpha(*end))
		end++;
	      // allow escaped continuation
	      if (*end == '\\')
		{
		  if (*(end+1) == '_')
		    {
		      end++;
		      while (*end == '_')
			end++;
		    }
		  if (isdigit(*(end+1)))
		    {
		      end++;
		      while (isdigit(*end))
			end++;
		    }
		}
	      else
		break;
	    }
	  char *p = strndup(start,(size_t) (end-start));
	  // and then we need to remove any escape backslashes from the string
	  char *w = p;
	  for (char *r = p; *r; ++r)
	    if (*r != '\\')
	      *(w++) = *r;
	  *w = 0;
	  //printf ("p:<%s>(%s,%s)\n",p,start,end);
	  const char *name = find_str_identifiers(p);
	  free(p);
	  start = end;

	  id.push_back(name);
	}
      else if (isdigit(*start))
	{
	  const char *end = start+1;
	  while (isdigit(*end))
	    end++;
	  char *end2;
	  int index = (int) strtol(start,&end2,10);
	  if (end2 != end)
	    ERROR("Error converting signal name (%s) integer.",name);
	  if (index >= 0x1000000)
	    ERROR("Cowardly refusing index (%d) >= 64K from name (%s).",
		  index,name);
	  start = end;
	  int index2 = index;
	  if (*start == '-')
	    {
	      start++;
	      if (!isdigit(*start))
		ERROR("Expected digit after range marker (-) in name (%s).",
		      name);
	      end = start+1;
	      while (isdigit(*end))
		end++;
	      index2 = (int) strtol(start,&end2,10);
	      if (end2 != end)
		ERROR("Error converting signal name (%s) integer.",name);
	      if (index2 >= 0x1000000)
		ERROR("Cowardly refusing index (%d) >= 64K from name (%s).",
		      index2,name);
	      start = end;
	      if (index2 < index)
		ERROR("High range index (%d) < low range index (%d) "
		      "in name (%s).",index2,index,name);
	    }

	  id.push_back(index-1,index2-1); // -1 since we internally are 0-based
	}
      else
	ERROR("Unrecognized character (%c) in signal name (%s).",
	      *start,name);
    }
}

bool signal_id_range_encloses(sig_part_range_vector::const_iterator i,
			      sig_part_vector::const_iterator j,
			      sig_part_range_vector::const_iterator i_end,
			      sig_part_vector::const_iterator j_end,
			      bool sub_channel)
{
  while (i != i_end)
    {
      const sig_part_range &part = *i;

      if (part._type == SIG_PART_WILDCARDS)
	{
	  ++i;

	  // We have to try all amounts of stripping of elements in the
	  // id list...

	  for ( ; ; )
	    {
	      if (signal_id_range_encloses(i,j,i_end,j_end,
					   sub_channel))
		return true;

	      if (j == j_end)
		return sub_channel;

	      ++j;
	    }
	}

      if (j == j_end)
	return sub_channel /*false*/; // id ran out of vector.  No match

      const sig_part    &id_part = *j;

      if (part._type != id_part._type &&
	  part._type != SIG_PART_WILDCARD)
	return false;

      if (part._type == SIG_PART_NAME)
	{
	  // strcasecmp seems not to be posix, we should (if necessary)
	  // define our own function...
	  if (strcasecmp(part._id._name,id_part._id._name) != 0)
	    return false;
	}
      if (part._type == SIG_PART_INDEX)
	{
	  if (part._id._index._min > id_part._id._index ||
	      part._id._index._max < id_part._id._index)
	    return false;
	}

      ++i;
      ++j;
    };

  // We ran out of vector.  (if id also did does not matter) We match

  return true;
}

bool signal_id_range::encloses(const signal_id &id,bool sub_channel)
{
  // We follow the two vectors.  Names must match.  Indices must be
  // within the bounds.  If we run out of vector first, we match, if
  // we are equally long, we match.  If id runs out of vector first,
  // we do not match (unless sub_channel is true).  (And basically
  // have a problem (since it won't match anyone else either), but
  // that does not concern us here)

  sig_part_range_vector::const_iterator i = _parts.begin();
  sig_part_vector::const_iterator j    = id._parts.begin();

  return signal_id_range_encloses(i,j,
				  _parts.end(),id._parts.end(),
				  sub_channel);
}


