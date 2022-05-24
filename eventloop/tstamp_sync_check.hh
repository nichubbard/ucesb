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

#ifndef __TSTAMP_SYNC_CHECK_HH__
#define __TSTAMP_SYNC_CHECK_HH__

#include <stdint.h>
#include <stdlib.h>

struct tstamp_sync_info
{
  uint16_t _id;
  uint32_t _event_no;
  uint64_t _timestamp;
  uint16_t _sync_check_flags;
  uint16_t _sync_check_value;
};

struct tstamp_sync_corr
{
  uint16_t _id;
  uint16_t _ref_peak_i;
  uint16_t _sync_check_value;

public:
  bool operator<(const tstamp_sync_corr &rhs) const;
};

/* For a certain ID and peak number. */
struct tstamp_sync_expect
{
  int    _flags;
  double _mean;
  double _tolerance;
};

class tstamp_sync_check
{

public:
  tstamp_sync_check(char const *command);
  ~tstamp_sync_check();

protected:
  void dump(tstamp_sync_info &ts_sync_info);

  void analyse(bool to_end);

  void estimate_ref_sync_value_period(size_t end,
				      int *peaks, int *npeaks);

  void estimate_sync_values(size_t end,
			    const int *peaks, const int *npeaks);

public:
  void account(tstamp_sync_info &ts_sync_info);

  void show();

public:
  uint16_t _ref_id;

public:
  uint64_t _prev_timestamp;

protected:
  tstamp_sync_info *_list;
  size_t            _num_items;
  size_t            _num_alloc;

  tstamp_sync_corr *_corr;

  int                _num_sync_check_peaks;

  tstamp_sync_expect _expect[64][64];
  int                _good_expect[64];
};

#endif/*__TSTAMP_SYNC_CHECK_HH__*/
