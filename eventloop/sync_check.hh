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

#ifndef __SYNC_CHECK_HH__
#define __SYNC_CHECK_HH__

#include "sync_check_info.hh"

template<int bits>
struct event_counter_bits
{
public:
  event_counter_bits()
  {
  }

  event_counter_bits(int value)
  {
    _value =value;
  }

public:
  int _value;
};

#define SYNC_CHECK_VALID_UNKNOWN   0 // silently adjust
#define SYNC_CHECK_VALID_KNOWN     1

template<int n>
class sync_check
{
public:
  sync_check()
  {
    reset();
  }

public:
  int _offset[n];
  int _valid[n];

public:
  void reset()
  {
    for (int i = 0; i < n; i++)
      {
	// wrong offset, to induce error
	// (since the normal path in check_sync is fast), we do
	// init on normal
	_offset[i] = 0xe474adbc; // we do _not_ choose deedbeef, since some joker may start the ec there!
	_valid[i] = SYNC_CHECK_VALID_UNKNOWN;
      }
  }

public:
  template<int bits,int bits_ref>
  bool check_sync(const sync_check_info<n,bits> &info,
		  event_counter_bits<bits_ref> reference)
  {
    // @reference is passed directly, as it is just one variable (with
    // template info) so pass by value (copy) is perfectly fine!

    // loop over the modules for which we have information

    bool success = true;

    for (int i = 0; i < info._num_items; i++)
      {
	// we want the difference of between the reference and the
	// event counters to be the same as noted by the offset

	// (of course, if the DAQ would make sure (reset stuff on
	// error) that the event counters always are in sync, we would
	// just have to check that the difference is zero...  (but
	// since people usually forget these little _extremely_ useful
	// debug possibilities, we need to go to these extremes...)

	const sync_check_info_item<bits> &item = info._items[i];

	int diff = (item._counter - reference._value);

	const int mask_cnt = (bits     == 32 ? 0xffffffff : (1 << bits)-1);
	const int mask_ref = (bits_ref == 32 ? 0xffffffff : (1 << bits_ref)-1);

	const int mask = mask_cnt & mask_ref;

	if (!((diff ^ _offset[item._index]) & mask))
	  continue; // difference is as prescribed

	if (_valid[item._index] != SYNC_CHECK_VALID_UNKNOWN)
	  {
	    const int bits_min = (bits < bits_ref) ? bits : bits_ref;

	    WARNING("Event counter %d (%0*x) mism. ref. (%0*x) by wrong diff. (%0*x!=%0*x)",
		    item._index,
		    (bits+3)/4,item._counter,
		    (bits_ref+3)/4,reference._value,
		    (bits_min+3)/4,diff & mask,
		    (bits_min+3)/4,_offset[item._index]);

	    success = false;
	  }

	if (_valid[item._index] == SYNC_CHECK_VALID_UNKNOWN)
	  _valid[item._index] = SYNC_CHECK_VALID_KNOWN;

	_offset[item._index] = diff & mask;
      }

    if (!success)
      {
	for (int i = 0; i < info._num_items; i++)
	  {
	    const sync_check_info_item<bits> &item = info._items[i];

	    printf ("%d (%0*x v %0*x)\n",
		    item._index,
		    (bits+3)/4,item._counter,
		    (bits_ref+3)/4,reference._value);
	  }



      }

    return success;
  }


};

#endif//__SYNC_CHECK_HH__
