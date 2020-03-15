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

#include "prefix_unit.hh"
#include "node.hh"

#include "parse_error.hh"
#include "str_set.hh"

#include <set>
#include <math.h>
#include <ctype.h>

#define UNKNOWN_PREFIX -127

int get_unit_prefix(char prefix)
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

    case '#': return  0;

    case 'k': return  3;
    case 'M': return  6;
    case 'G': return  9;
    case 'T': return  12;
    case 'P': return  15;
    case 'E': return  18;
    case 'Z': return  21;
    case 'Y': return  24;
    }
  return UNKNOWN_PREFIX; // unknown prefix
}



void insert_unit_reduce(unit_exponent_map &unit_set,
			const char *unit,int exponent)
{
  /*
  unit_exponent_set::iterator iter = unit_set.lower_bound(unit);

  if (iter == unit_set.end() ||
      unit < *iter)
    unit_set.insert(unit_exponent_map::value_pair(unit,exponent));

  (*iter)->second += unit._exponent;
  */


  std::pair<unit_exponent_map::iterator,bool> result =
    unit_set.insert(unit_exponent_map::value_type(unit,exponent));

  if (result.second == false)
    {
      // This unit already existed, add the exponents in
      // the existing one

      result.first->second += exponent;

      if (result.first->second == 0)
	{
	  // we got rid of the unit, exponent 0 :  x^0 = 1 (no unit)
	  unit_set.erase(result.first);
	}
    }

  // dump_unit_exponent_map(unit_set); printf ("|\n");
}


void convert_units_exponent(const file_line &loc,const char *str,
			    units_exponent *units,
			    prefix_units_exponent *prefix_units)
{
  const char *p = str;

  if (prefix_units)
    prefix_units->_total_prefix = 0;

  int mult = 1;

  while (*p)
    {
      // first skip any spaces.  if we find a multiplier '*' or
      // divider '/' remember that

      int this_mult = 0;

      for ( ; ; p++)
	{
	  if (*p == '*')
	    {
	      if (this_mult)
		ERROR_LOC(loc,"Multiple consequtive '*' or '/' in unit '%s'.",str);
	      this_mult = 1;
	      continue;
	    }
	  if (*p == '/')
	    {
	      if (this_mult)
		ERROR_LOC(loc,"Multiple consequtive '*' or '/' in unit '%s'.",str);
	      this_mult = -1;
	      continue;
	    }
	  if (*p == ' ')
	    continue;
	  break;
	}

      // It's ok to end, i.e. having eaten spaces, if there were no
      // '*' or '/'

      if (!*p)
	{
	  if (this_mult)
	    ERROR_LOC(loc,"Ending with '*' or '/' in unit '%s'.",str);
	  break;
	}

      if (this_mult)
	mult = this_mult;

      // Is there an prefix marker?

      const char *prefix_marker = p;

      int prefix = 0;

      if (*p == '#')
	{
	  if (!prefix_units)
	    ERROR_LOC(loc,"Unexpected prefix marker '#' in unit '%s'.",str);

	  p++;

	  prefix = get_unit_prefix(*p);

	  if (prefix == UNKNOWN_PREFIX)
	    ERROR_LOC(loc,"Unknown prefix '%c' in unit '%s'.",*p,str);

	  p++;

	  // prefix has been handled
	}

      // So, we should now have the text for the unit.

      if (!isalpha((int) *p))
	{
	  if (*p) {
	    ERROR_LOC(loc,"Unexpected character '%c' in unit '%s'.",*p,str);
	  } else {
	    ERROR_LOC(loc,"Unexpected end of unit '%s'.",str);
	  }
	}

      const char *start = p;

      // go on as long as we have characters

      while (isalpha((int) *p))
	p++;

      const char *end = p;

      // Perhaps we are followed by digits for the exponent (possibly
      // preceeded by a minus sign)

      if (*p == '^' &&
	  ((*(p+1) == '-' && isdigit((int) *(p+2))) || isdigit((int) *(p+1))))
	p++;

      int sign = 1;

      if (*p == '-' && isdigit((int) *(p+1)))
	{
	  sign = -1;
	  p++;
	}

      const char *exp = p;

      while (isdigit((int) *p))
	p++;

      int exponent = 1;

      if (p != exp)
	{
	  exponent = sign * atoi(exp);
	}

      exponent *= mult;

      // ok, so we have parsed our way through the block

      const char *unit_str = find_str_strings(start,(size_t) (end - start));

      if (units)
	insert_unit_reduce(units->_blocks,unit_str,exponent);
      else if (prefix_units)
	{
	  if (*prefix_marker == '#')
	    {
	      insert_unit_reduce(prefix_units->_blocks_prefix,unit_str,exponent);
	      prefix_units->_total_prefix += prefix * exponent;
	    }
	  else
	    insert_unit_reduce(prefix_units->_blocks_simple,unit_str,exponent);
	}
    }
  //if (units)        dump_units(*units);
  //if (prefix_units) dump_units(*prefix_units);
}



typedef std::set<const units_exponent*,
		 compare_ptr<units_exponent> >        set_units_exponent;
typedef std::set<const prefix_units_exponent*,
		 compare_ptr<prefix_units_exponent> > set_prefix_units_exponent;


set_units_exponent        _set_units;
set_prefix_units_exponent _set_prefix_units;


const units_exponent *insert_units_exp(const file_line &loc,
				       const char *str)
{
  units_exponent *units = new units_exponent;

  convert_units_exponent(loc,str,units,NULL);

  std::pair<set_units_exponent::iterator,bool> result =
    _set_units.insert(units);

  if (result.second == false)
    delete units;

  return *result.first;
}


const prefix_units_exponent *insert_prefix_units_exp(const file_line &loc,
						     const char *str)
{
  prefix_units_exponent *units = new prefix_units_exponent;

  convert_units_exponent(loc,str,NULL,units);

  std::pair<set_prefix_units_exponent::iterator,bool> result =
    _set_prefix_units.insert(units);

  if (result.second == false)
    delete units;

  return *result.first;
}

struct divide_units_item
{
  const prefix_units_exponent *_nom;
  const prefix_units_exponent *_denom;

public:
  bool operator<(const divide_units_item &rhs) const
  {
    if (_nom < rhs._nom)
      return true;
    if (_nom > rhs._nom)
      return false;

    return _denom < rhs._denom;
  }
};

typedef std::map<divide_units_item,prefix_units_exponent *> divide_units_item_map;

divide_units_item_map _map_divide_units_item;

void insert_blocks_exp_mult(unit_exponent_map &dest,
			    const unit_exponent_map &src,int mult)
{
  for (unit_exponent_map::const_iterator iter = src.begin();
       iter != src.end(); ++iter)
    {
      insert_unit_reduce(dest,iter->first,mult*iter->second);
    }
}

const prefix_units_exponent *divide_units(const prefix_units_exponent *nom,
					  const prefix_units_exponent *denom)
{
  if (!nom && !denom)
    return NULL;

  divide_units_item mdu;

  mdu._nom   = nom;
  mdu._denom = denom;

  divide_units_item_map::iterator iter = _map_divide_units_item.find(mdu);

  if (iter != _map_divide_units_item.end())
    {
      // We have divided these units before
      return iter->second;
    }

  prefix_units_exponent *units = new prefix_units_exponent;
  units->_total_prefix = 0;

  if (nom)
    {
      insert_blocks_exp_mult(units->_blocks_simple,  nom->_blocks_simple, 1);
      insert_blocks_exp_mult(units->_blocks_prefix,  nom->_blocks_prefix, 1);
      units->_total_prefix += nom->_total_prefix;
    }
  if (denom)
    {
      insert_blocks_exp_mult(units->_blocks_simple,denom->_blocks_simple,-1);
      insert_blocks_exp_mult(units->_blocks_prefix,denom->_blocks_prefix,-1);
      units->_total_prefix -= denom->_total_prefix;
    }

  _map_divide_units_item.insert(divide_units_item_map::value_type(mdu,units));

  return units;
}

struct match_unit_item
{
  const prefix_units_exponent *_dest;
  const units_exponent *_src;

public:
  bool operator<(const match_unit_item &rhs) const
  {
    if (_dest < rhs._dest)
      return true;
    if (_dest > rhs._dest)
      return false;

    return _src < rhs._src;
  }
};

typedef std::map<match_unit_item,double> match_unit_item_map;

match_unit_item_map _map_match_unit_item;

bool match_units(const prefix_units_exponent *dest,
		 const units_exponent *src,
		 double &factor)
{
  if (!dest && !src)
    {
      factor = 1.;
      return true;
    }

  match_unit_item mui;

  mui._dest = dest;
  mui._src  = src;

  match_unit_item_map::iterator iter = _map_match_unit_item.find(mui);

  if (iter != _map_match_unit_item.end())
    {
      // We have matched these units before (successfully)
      factor = iter->second;
      return true;
    }

  // Non-successful: this always leads to errors, so may take long time!

  // So, we have three lists of unit blocks:
  // src  , dest -non-prefixed and dest -prefixed

  // First, we match away anything in dest-non-prefixed Then, what is
  // left we add to the temporary source list, and then try to match
  // away everything in the prefixed list.

  unit_exponent_map tmp_src;

  if (src)
    tmp_src = src->_blocks;

  if (dest)
    for (unit_exponent_map::const_iterator iter = dest->_blocks_simple.begin();
	 iter != dest->_blocks_simple.end(); ++iter)
      {
	// removing from temp list is easy, just add with negative
	// exponent

	insert_unit_reduce(tmp_src,iter->first,-iter->second);
      }

  // removing the items with prefixes are somewhat harder, as we
  // cannot do direct string comparisons until we've removed the
  // prefixes.  In the src (temp) list, we actally also do not know if
  // the first character is a prefix or not.

  unit_exponent_map tmp_pre_dest;

  if (dest)
    tmp_pre_dest = dest->_blocks_prefix;

  // First we try to remove anything as if it does not have any
  // exponents

  for (unit_exponent_map::iterator iter = tmp_src.begin();
       iter != tmp_src.end(); )
    {
      unit_exponent_map::iterator check = iter;
      ++iter;

      if (tmp_pre_dest.find(check->first) != tmp_pre_dest.end())
	{
	  // Did exist, so we remove as much as possible
	  insert_unit_reduce(tmp_pre_dest,check->first,-check->second);
	  tmp_src.erase(check);
	}
    }

  // Now, anything left in the src list has to be with a prefix, and
  // removed that way.  Any item that does not match now is a fault.

  int total_prefix = dest ? dest->_total_prefix : 0;

  for (unit_exponent_map::iterator iter = tmp_src.begin();
       iter != tmp_src.end(); ++iter)
    {
      const char *s1 = iter->first;

      int prefix = get_unit_prefix(*s1);

      if (!prefix || prefix == UNKNOWN_PREFIX)
	return false;

      const char *s2 = find_str_strings(s1+1); // rest of string

      unit_exponent_map::iterator item = tmp_pre_dest.find(s2);

      if (item == tmp_pre_dest.end())
	return false;

      // ok, so we found the item.  Kill of exponents

      insert_unit_reduce(tmp_pre_dest,s2,-iter->second);

      // and now handle our prefix troubles

      total_prefix -= prefix * iter->second;
    }

  // everything should now be gone from the dest list

  if (tmp_pre_dest.size())
    return false;

  factor = pow(10.0,-total_prefix);

  _map_match_unit_item.insert(match_unit_item_map::value_type(mui,factor));

  return true;
}


struct format_unit_item
{
  int         _exponent;
  bool        _prefix;
  const char *_str;

  bool operator<(const format_unit_item &rhs) const
  {
    int s1 = _exponent      < 0;
    int s2 = rhs._exponent < 0;

    if (s1 != s2)
      return s1 < s2;

    int e1 = abs(_exponent);
    int e2 = abs(rhs._exponent);

    if (e1 != e2)
      return e1 < e2;

    if (_prefix != rhs._prefix)
      return rhs._prefix;

    return strcmp(_str,rhs._str) < 0;
  }
};

typedef std::multiset<format_unit_item> multiset_format_unit_item;

char *format_unit_str(multiset_format_unit_item &items,
		      char *str,size_t n)
{
  char *p = str;
  char *e = str+n;

  *p = 0;

  bool first = true;

  for (multiset_format_unit_item::iterator iter = items.begin();
       iter != items.end(); ++iter)
    {
      if (iter->_exponent >= 0) {
	if (!first) p += snprintf (p,(size_t) (e-p),"*");
      } else {
	p += snprintf (p,(size_t) (e-p),"/");
      }
      if (p >= e) break;
      if (iter->_prefix) {
	p += snprintf (p,(size_t) (e-p),"#");
      }
      if (p >= e) break;
      p += snprintf (p,(size_t) (e-p),"%s",iter->_str);
      if (p >= e) break;
      if (iter->_exponent < -1 || iter->_exponent > 1)
	p += snprintf (p,(size_t) (e-p),"^%d",abs(iter->_exponent));
      if (p >= e) break;

      first = false;
    }
  return str;
}

void add_format_unit_item(multiset_format_unit_item &items,
			  unit_exponent_map::const_iterator item,bool prefix)
{
  format_unit_item tmp;

  tmp._str      = item->first;
  tmp._exponent = item->second;
  tmp._prefix   = prefix;

  items.insert(tmp);
}

char *format_unit_str(const units_exponent *unit,char *str,size_t n)
{
  multiset_format_unit_item items;

  if (unit)
    for (unit_exponent_map::const_iterator iter = unit->_blocks.begin();
	 iter != unit->_blocks.end(); ++iter)
      add_format_unit_item(items,iter,false);

  return format_unit_str(items,str,n);
}

char *format_unit_str(const prefix_units_exponent *unit,char *str,size_t n)
{
  multiset_format_unit_item items;

  if (unit)
    for (unit_exponent_map::const_iterator iter = unit->_blocks_simple.begin();
	 iter != unit->_blocks_simple.end(); ++iter)
      add_format_unit_item(items,iter,false);
  if (unit)
    for (unit_exponent_map::const_iterator iter = unit->_blocks_prefix.begin();
	 iter != unit->_blocks_prefix.end(); ++iter)
      add_format_unit_item(items,iter,true);

  return format_unit_str(items,str,n);
}


#ifdef TEST_PREFIX_UNIT

// For testing:
// g++ -g -DTEST_PREFIX_UNIT -o test_prefix_unit prefix_unit.cc str_set.cc file_line.cc
// ./test_prefix_unit

#define countof(array) (sizeof(array)/sizeof((array)[0]))

int main(int argc,char *argv[])
{
  const char *str1[] = {
    "ns", "ns*ns", "cm", "ns/cm", "cm/ns", "ns2", "cm3", "ns-4", "pF",
    "/ns", "/ns-1", "ns-2", "cm-1", "/cm", "mm-1", "/mm", "mm", "um", "km"
  };

  const char *str2[] = {
    "##m", "##s", "/#cm", "/#cm-1", "/#mm", "/#mm-1", "/#um", "/#um-1",
    "#ns", "#cm", "#ns/#cm", "#cm/#ns", "#ns2", "#cm3", "#ns-4", "#pF",
    "#ns", "#cm", "#ns/cm", "cm/#ns", "#ns2", "#cm3", "#ns-4", "#pF",
    "#ns*#ns",
  };

  char tmp1[64], tmp2[64];

  for (int i = 0; i < countof (str1); i++)
    {
      const prefix_units_exponent *pue;
      pue = insert_prefix_units_exp(file_line(),str1[i]);
      printf ("%10s -> %8p : %s\n",str1[i],pue,
	      format_unit_str(pue,tmp1,sizeof(tmp1)));
    }

  for (int i = 0; i < countof (str2); i++)
    {
      const prefix_units_exponent *pue;
      pue = insert_prefix_units_exp(file_line(),str2[i]);
      printf ("%10s -> %8p : %s\n",str2[i],pue,
	      format_unit_str(pue,tmp1,sizeof(tmp1)));
    }

  for (int i = 0; i < countof (str1); i++)
    {
      const units_exponent *ue;
      ue = insert_units_exp(file_line(),str1[i]);
      printf ("%10s -> %8p : %s\n",str1[i],ue,
	      format_unit_str(ue,tmp1,sizeof(tmp1)));
    }

  for (int i = 0; i < countof (str2); i++)
    {
      const prefix_units_exponent *pue1;
      pue1 = insert_prefix_units_exp(file_line(),str2[i]);
      for (int j = 0; j < countof (str2); j++)
	{
	  const prefix_units_exponent *pue2;
	  pue2 = insert_prefix_units_exp(file_line(),str2[j]);
	  const prefix_units_exponent *div;
	  div = divide_units(pue1,pue2);
	  printf ("%10s / %10s -> %8p : %s\n",str2[i],str2[j],div,
		  format_unit_str(div,tmp1,sizeof(tmp1)));
	}
    }

  for (int i = 0; i < countof (str2); i++)
    {
      const prefix_units_exponent *pue;
      pue = insert_prefix_units_exp(file_line(),str2[i]);
      for (int j = 0; j < countof (str1); j++)
	{
	  const units_exponent *ue;
	  ue = insert_units_exp(file_line(),str1[j]);

	  printf ("%10s : %10s ",str2[i],str1[j]);

	  double factor;

	  bool match = match_units(pue,ue,factor);

	  if (match)
	    printf ("-> %10.5f\n",factor);
	  else
	    printf ("-\n");
	}
    }

  return 0;
}

#endif//TEST_PREFIX_UNIT
