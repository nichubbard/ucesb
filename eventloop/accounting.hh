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

#ifndef __ACCOUNTING_HH__
#define __ACCOUNTING_HH__

#include "data_src.hh"

// extern _data_account[] is in data_src.hh, such that that does not
// need to include more (this) files.

struct account_id
{
  int         _internal;

  const char* _name;
  const char* _ident;
};

void account_init();
void account_show();

#endif//__ACCOUNTING_HH__

