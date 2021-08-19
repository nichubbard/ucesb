/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2021  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include <vector>
#include <stdlib.h>

#include "dumper.hh"

class account_item
{
public:
  account_item(const char *name,
	       const char *ident)
  {
    _name  = name;
    _ident = ident;
  }

public:
  const char *_name;
  const char *_ident;
};

typedef std::vector<account_item*> account_item_list;

account_item_list _account_items;

int new_account_item(const char *name, const char *ident)
{
  size_t index = _account_items.size();

  _account_items.push_back(new account_item(name, ident));

  return (int) index;
}

void generate_account_items()
{
  print_header("ACCOUNT_IDS",
	       "Structure and identifier for raw data items.");

  printf ("account_id _account_ids[] =\n"
	  "{ \n");
  size_t i = 0;
  for (account_item_list::iterator iter = _account_items.begin();
       iter !=_account_items.end(); i++, ++iter)
    {
      printf ("  { %zd, \"%s\", \"%s\" },\n",
	      i,
	      (*iter)->_name,
	      (*iter)->_ident);
    }
  printf ("};\n\n");

  printf ("#define NUM_ACCOUNT_IDS  %zd\n",
	  _account_items.size());

  print_footer("ACCOUNT_IDS");
}
