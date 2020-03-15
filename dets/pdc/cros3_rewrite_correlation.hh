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

#ifndef __CROS3_REWRITE_CORRELATION_HH__
#define __CROS3_REWRITE_CORRELATION_HH__

#define USER_DEF_CROS3_REWRITE // prevent generated definition

class CROS3_REWRITE_correlation
{
public:
  CROS3_REWRITE_correlation()
  {
    _index = -1;
  }

public:
  int _index;

public:
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info)
  {
    if (!info->_requests->is_channel_requested(id,false,0,false))
      return false;

    _index = info->_next_index;
    info->_next_index += 256;

    return true;
  }

  void add_corr_members(const CROS3_REWRITE &src,correlation_list *list) const
  {
    if (_index == -1)
      return;

    // We need to go through the data we have, find out what channels
    // have fired, and add them to the list.  This exercise is
    // necessary, as they may be both in disorder and have multiple
    // hits.

    // bitsone<256> wires;





  }
};

#endif//__CROS3_REWRITE_CORRELATION_HH__
