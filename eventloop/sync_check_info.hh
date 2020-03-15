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

#ifndef __SYNC_CHECK_INFO_HH__
#define __SYNC_CHECK_INFO_HH__

#include "bitsone.hh"

template<int bits>
struct sync_check_info_item
{
  int _index;
  int _counter;
};

template<int n,int bits>
class sync_check_info
{
public:
  sync_check_info()
  {
    clear();
  }

public:
  sync_check_info_item<bits> _items[n];
  int                        _num_items;

  bitsone<n>                 _seen;

public:
  void clear()
  {
    _num_items = 0;
    _seen.clear();
  }

public:
  bool add(int index,int counter)
  {
    // since we have no overflow check, we instead also keep track of
    // seen modules.  They should only appear once!

    bool before = _seen.get_set(index);

    if (before)
      return before;

    sync_check_info_item<bits> &item = _items[_num_items++];

    item._index   = index;
    item._counter = counter;

    return 0;
  }


};

#endif//__SYNC_CHECK_INFO_HH__
