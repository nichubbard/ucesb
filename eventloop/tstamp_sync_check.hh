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

/* The sync-check status of an event is marked by flags.
 *
 * The first checks are based on time-stamp ordering.
 * If those fail, then further checks are not possible.
 * Note that timestamp checks are done before a local trigger
 * is 'excused'.  Timestamps shall always be ordered.
 *
 * - TS out of order with respect to adjacent (prev+next) ref TS.
 * - Adjacent (prev+next) ref TS themselves out of order.
 * - No adjacent (prev+next) ref TS close-by (~4000 events).
 *
 * - Item itself marked local.
 * - Closest reference marked local -> item spurious.
 * - Not closest item to closest reference -> item duplicate.
 *
 * - Item has no sync check value.
 * - Item has no sync check peak fit (no good history).
 * - Item sync check value is not within any known peak.
 * - Item sync check value gives ambiguous peak assignment.
 *
 * - Reference value has (any of the above) issues, no peak.
 * - Item peak mismatches reference.
 *
 * - Item sych check peak matches reference.
 */



/* Cannot check due to being out-of-order with adjacent refs. */
#define TSTAMP_SYNC_INFO_OUT_OF_ORDER_TS       0x0001
/* Cannot check due to references out of order. */
#define TSTAMP_SYNC_INFO_OUT_OF_ORDER_REFS_TS  0x0002
/* Cannot check due to no adjacent references. */
#define TSTAMP_SYNC_INFO_NO_ENCLOSING_REFS     0x0004

/* Local trigger. */
#define TSTAMP_SYNC_INFO_LOCAL_TRIGGER         0x0010  /* Good. */

/* Cannot check due to no adjacent reference, or reference local trigger. */
#define TSTAMP_SYNC_INFO_SPURIOUS              0x0020
/* Extra item trigger, not closest to reference. */
#define TSTAMP_SYNC_INFO_EXTRA                 0x0040

/* No peak value. */
#define TSTAMP_SYNC_INFO_NO_VALUE              0x0100
/* No peak value. */
#define TSTAMP_SYNC_INFO_NO_PEAK_FIT           0x0200
/* Sync check value is outside any of the known peaks.  (May be in-between.) */
#define TSTAMP_SYNC_INFO_OUTSIDE_PEAK          0x0400
/* Peak assignment ambiguous. */
#define TSTAMP_SYNC_INFO_AMBIGUOUS_PEAK        0x0800

/* No peak value of reference. */
#define TSTAMP_SYNC_INFO_NO_REF_PEAK           0x1000
/* Mismatch sync check peak of reference value. */
#define TSTAMP_SYNC_INFO_PEAK_MISMATCH         0x2000
/* Match sync check peak of reference value.    */
#define TSTAMP_SYNC_INFO_PEAK_MATCH            0x4000  /* Good. */

struct tstamp_sync_account
{
  uint64_t _out_of_order_ts;
  uint64_t _out_of_order_refs_ts;
  uint64_t _no_adjacent_refs;
  uint64_t _local;
  uint64_t _spurious;
  uint64_t _extra;
  uint64_t _no_sc_value;
  uint64_t _no_peak_fit;
  uint64_t _outside;
  uint64_t _ambiguous;
  uint64_t _no_ref_peak;
  uint64_t _mismatch;
  uint64_t _match;
};


struct tstamp_sync_info
{
  uint16_t _id;
  uint16_t _trigger;
  uint32_t _event_no;
  uint64_t _timestamp;
  uint16_t _sync_check_flags;
  uint16_t _sync_check_value;

  /* These are generated from the analysis. */
  uint16_t _flags;
  int16_t  _peak;
  float    _frac;
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

  void analyse_values(size_t end, size_t at_least);

  void find_peak(tstamp_sync_info &ts_sync_info);

public:
  void account(tstamp_sync_info &ts_sync_info);

  void show();

  void show_account(int id, tstamp_sync_account *account);
  void show_accounts(tstamp_sync_account *accounts);

public:
  uint16_t _ref_id;

public:
  uint64_t _prev_timestamp;

public:
  size_t   _i_check_next;
  size_t   _i_dump_next;

protected:
  tstamp_sync_account _account[64];
  tstamp_sync_account _account_prev[64];

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
