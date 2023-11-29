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

#include "tstamp_sync_check.hh"
#include "colourtext.hh"
#include "error.hh"
#include "wr_stamp.hh"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <algorithm>

#define DEBUG_TSTAMP_SYNC_CHECK  0

tstamp_sync_check::tstamp_sync_check(char const *command)
{
  _ref_id = 0x10;

  _prev_timestamp = 0;

  _num_items = 0;
  _num_alloc = 4096;

  _list = new tstamp_sync_info[_num_alloc];

  if (!_list)
    ERROR("Memory allocation failure!");

  _corr = new tstamp_sync_corr[_num_alloc];

  if (!_corr)
    ERROR("Memory allocation failure!");

  _i_check_next = 0;
  _i_dump_next = 0;

  memset(_account,      0, sizeof (_account));
  memset(_account_prev, 0, sizeof (_account_prev));
}

tstamp_sync_check::~tstamp_sync_check()
{
  delete[] _list;
  delete[] _corr;
}

void tstamp_sync_check::dump(tstamp_sync_info &ts_sync_info)
{
  uint64_t ts;
  
  if (ts_sync_info._timestamp != 0 &&
      ts_sync_info._timestamp - _prev_timestamp > 2000)
    {
      printf ("\n");
    }

  printf ("%s%-12u%s %s%2d%s %s%02x%s  ",
	  CT_OUT(BOLD_BLUE),
	  ts_sync_info._event_no,
	  CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD_BLUE),
	  ts_sync_info._trigger,
	  CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD),
	  ts_sync_info._id,
	  CT_OUT(NORM));

  ts = ts_sync_info._timestamp;

  if (ts_sync_info._timestamp == 0)
    printf ("%s----%s:%s----%s:%s----%s:%s----%s",
	    CT_OUT(BOLD_BLUE), CT_OUT(NORM_DEF_COL),
	    CT_OUT(BOLD_BLUE), CT_OUT(NORM_DEF_COL),
	    CT_OUT(BOLD_BLUE), CT_OUT(NORM_DEF_COL),
	    CT_OUT(BOLD_BLUE), CT_OUT(NORM_DEF_COL));
  else
    printf ("%s%04x%s:%s%04x%s:%s%04x%s:%s%04x%s",
	    CT_OUT(BOLD_BLUE), (int) (ts >> 48) & 0xffff,CT_OUT(NORM_DEF_COL),
	    CT_OUT(BOLD_BLUE), (int) (ts >> 32) & 0xffff,CT_OUT(NORM_DEF_COL),
	    CT_OUT(BOLD_BLUE), (int) (ts >> 16) & 0xffff,CT_OUT(NORM_DEF_COL),
	    CT_OUT(BOLD_BLUE), (int) (ts      ) & 0xffff,CT_OUT(NORM_DEF_COL));

  if (ts_sync_info._timestamp == 0)
    printf ("  %s%10s%s",
	    CT_OUT(BOLD_RED), "err", CT_OUT(NORM_DEF_COL));
  else
    printf ("  %s%10" PRIu64 "%s",
	    CT_OUT(BOLD), ts_sync_info._timestamp - _prev_timestamp,
	    /* */                                       CT_OUT(NORM));

  printf ("  %s[%c%c%c]%s %s%5d%s ",
	  CT_OUT(BOLD),
	  ts_sync_info._sync_check_flags &
	  (SYNC_CHECK_REF  >> SYNC_CHECK_FLAGS_SHIFT) ? 'R' : ' ',
	  ts_sync_info._sync_check_flags &
	  (SYNC_CHECK_RECV >> SYNC_CHECK_FLAGS_SHIFT)  ? 's' : ' ',
	  ts_sync_info._sync_check_flags &
	  (SYNC_CHECK_LOCAL>> SYNC_CHECK_FLAGS_SHIFT) ? 'L' : ' ',
	  CT_OUT(NORM),
	  CT_OUT(BOLD_MAGENTA), ts_sync_info._sync_check_value,
	  /*                                         */ CT_OUT(NORM_DEF_COL));

  if (ts_sync_info._peak >= 0)
    {
      printf ("  %s%5.2f%s ",
	      CT_OUT(BOLD_CYAN),
	      ts_sync_info._peak + ts_sync_info._frac,
	      CT_OUT(NORM_DEF_COL));
    }
  else
    printf ("        ");

  const char *flag_str = NULL;
  short flag_color = 0;

  if (ts_sync_info._flags & TSTAMP_SYNC_INFO_OUT_OF_ORDER_TS) {
    flag_str = "out-of-order-ts";
    flag_color = CTR_WHITE_BG_RED;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_OUT_OF_ORDER_REFS_TS) {
    flag_str = "out-of-order-refs-ts";
    flag_color = CTR_RED;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_NO_ENCLOSING_REFS) {
    flag_str = "no-adjacent-refs";
    flag_color = CTR_BLUE;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_LOCAL_TRIGGER) {
    flag_str = "local";
    flag_color = CTR_GREEN;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_SPURIOUS) {
    flag_str = "spurious";
    flag_color = CTR_MAGENTA;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_EXTRA) {
    flag_str = "extra";
    flag_color = CTR_MAGENTA;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_NO_VALUE) {
    flag_str = "no-sc-value";
    flag_color = CTR_RED;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_NO_PEAK_FIT) {
    flag_str = "no-peak-fit";
    flag_color = CTR_RED;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_OUTSIDE_PEAK) {
    flag_str = "outside";
    flag_color = CTR_WHITE_BG_RED;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_AMBIGUOUS_PEAK) {
    flag_str = "ambiguous";
    flag_color = CTR_YELLOW_BG_RED;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_NO_REF_PEAK) {
    flag_str = "no-ref-peak";
    flag_color = CTR_RED;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_PEAK_MISMATCH) {
    flag_str = "mismatch";
    flag_color = CTR_WHITE_BG_RED;
  } else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_PEAK_MATCH) {
    flag_str = "match";
    flag_color = CTR_GREEN;
  }

  if (flag_str)
    printf ("  %s%s%s",
	    COLOURTEXT_GET(0,flag_color), flag_str, CT_OUT(NORM_DEF_COL));

  if (ts_sync_info._id == _ref_id)
    {
      printf (" %s%s%s",
	      COLOURTEXT_GET(0,CTR_CYAN), "(ref)", CT_OUT(NORM_DEF_COL));
    }

  printf ("\n");
  fflush(stdout);

  if (ts_sync_info._timestamp != 0)
    {
      _prev_timestamp = ts_sync_info._timestamp;
    }
}

void tstamp_sync_check::find_peak(tstamp_sync_info &ts_sync_info)
{
  ts_sync_info._flags = 0;
  ts_sync_info._peak = -1;

  if (!(ts_sync_info._sync_check_flags &
	((SYNC_CHECK_REF |
	  SYNC_CHECK_RECV) >> SYNC_CHECK_FLAGS_SHIFT)))
    {
      ts_sync_info._flags |= TSTAMP_SYNC_INFO_NO_VALUE;
      return;
    }

  if (ts_sync_info._id >= 64)
    {
      ts_sync_info._flags |= TSTAMP_SYNC_INFO_NO_PEAK_FIT;
      return;
    }

  if (!_good_expect[ts_sync_info._id])
    {
      ts_sync_info._flags |= TSTAMP_SYNC_INFO_NO_PEAK_FIT;
      return;
    }

  {
    /* Find out which peak value this corresponds to. */
    int peak;

    /* In principle, should do binary search. */
    for (peak = 0; peak < _num_sync_check_peaks; peak++)
      {
	if (_expect[ts_sync_info._id][peak]._mean >
	    ts_sync_info._sync_check_value)
	  break;
      }

    /* peak now points to the peak number after the hit.
     * i.e. [peak-1] and [peak] are boxing in the value.
     *
     * If at the edges, then shift the indices.
     */

    if (peak == 0)
      peak++;
    if (peak == _num_sync_check_peaks)
      peak--;

    double frac =
      (ts_sync_info._sync_check_value -
       _expect[ts_sync_info._id][peak-1]._mean) /
      (_expect[ts_sync_info._id][peak  ]._mean -
       _expect[ts_sync_info._id][peak-1]._mean);

    /* If the fraction is < 0.5, then [peak-1] is closest. */
    if (frac < 0.5 &&
	peak > 0)
      {
	peak--;
	frac += 1;
      }

    ts_sync_info._peak = (int16_t) peak;
    ts_sync_info._frac = (float) frac;

    double dist =
      fabs(ts_sync_info._sync_check_value -
	   _expect[ts_sync_info._id][peak]._mean);

    int within = dist < _expect[ts_sync_info._id][peak]._tolerance;

    if (!within)
      ts_sync_info._flags |= TSTAMP_SYNC_INFO_OUTSIDE_PEAK;
    else
      {
	if (peak > 0)
	  {
	    double dist2 =
	      fabs(ts_sync_info._sync_check_value -
		   _expect[ts_sync_info._id][peak-1]._mean);

	    int within2 = dist2 < _expect[ts_sync_info._id][peak]._tolerance;

	    if (within2)
	      ts_sync_info._flags |= TSTAMP_SYNC_INFO_AMBIGUOUS_PEAK;
	  }
	if (peak < _num_sync_check_peaks - 1)
	  {
	    double dist2 =
	      fabs(ts_sync_info._sync_check_value -
		   _expect[ts_sync_info._id][peak+1]._mean);

	    int within2 = dist2 < _expect[ts_sync_info._id][peak]._tolerance;

	    if (within2)
	      ts_sync_info._flags |= TSTAMP_SYNC_INFO_AMBIGUOUS_PEAK;
	  }
      }

    if (ts_sync_info._flags)
      ts_sync_info._peak  = -1;
  }
}

void tstamp_sc_dump_value(uint64_t value, short color)
{
  if (value == 0)
    printf ("     -");
  else
    {
      printf (" %s%5" PRIu64 "%s",
	      COLOURTEXT_GET(0,color), value, CT_OUT(NORM_DEF_COL));
    }
}

void tstamp_sync_check::show_account(int id, tstamp_sync_account *account)
{
  printf ("%s%02x%s ",
	  CT_OUT(BOLD),
	  id,
	  CT_OUT(NORM));

  tstamp_sc_dump_value(account->_out_of_order_ts,     CTR_WHITE_BG_RED);
  tstamp_sc_dump_value(account->_out_of_order_refs_ts,CTR_RED);
  tstamp_sc_dump_value(account->_no_adjacent_refs,    CTR_BLUE);
  tstamp_sc_dump_value(account->_local,		      CTR_GREEN);
  tstamp_sc_dump_value(account->_spurious,	      CTR_MAGENTA);
  tstamp_sc_dump_value(account->_extra,		      CTR_MAGENTA);
  tstamp_sc_dump_value(account->_no_sc_value,	      CTR_RED);
  tstamp_sc_dump_value(account->_no_peak_fit,	      CTR_RED);
  tstamp_sc_dump_value(account->_outside,	      CTR_WHITE_BG_RED);
  tstamp_sc_dump_value(account->_ambiguous,	      CTR_YELLOW_BG_RED);
  tstamp_sc_dump_value(account->_no_ref_peak,	      CTR_RED);
  tstamp_sc_dump_value(account->_mismatch,	      CTR_WHITE_BG_RED);
  tstamp_sc_dump_value(account->_match,		      CTR_GREEN);

  printf ("\n");
}

void tstamp_sync_check::show_accounts(tstamp_sync_account *accounts)
{
  printf ("\n");
  printf ("         %soooref%s       %slocal%s       %sextra%s       %snofit%s       %sambig%s        %smism%s\n",
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM));
  printf ("%sID%s   %sooo%s       %snoaref%s        %sspur%s       %snoval%s      %soutfit%s       %snoref%s       %smatch%s\n",
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM));

  for (int id = 0; id < 64; id++)
    {
      tstamp_sync_account *account = &accounts[id];

      uint64_t *p = (uint64_t *) account;
      int non_zero = 0;

      for (size_t i = 0;
	   i < sizeof (tstamp_sync_account) / sizeof (uint64_t); i++, p++)
	if (*p)
	  non_zero = 1;

      if (non_zero)
	show_account(id, account);
    }
}

void tstamp_sync_check::analyse_values(size_t end, size_t at_least)
{
  size_t i_ref_prev;
  size_t i_ref_cur;
  size_t i_ref_next;

  /* Plan: work with triples of reference values.
   *
   * The previous and next reference value just give the timestamp
   * range to consider, by giving the halfpoint values between those
   * and the current value.
   *
   * Then investigate all values between those halfpoint values,
   * assuming that they (should) belong to the middle reference value.
   * If an id has more than one value, then the extra ones are
   * spurious.
   */

  /* Find the first reference value. */
  for (i_ref_prev = 0; i_ref_prev < end; i_ref_prev++)
    if (_list[i_ref_prev]._id == _ref_id)
      break;
  for (i_ref_cur = i_ref_prev + 1; i_ref_cur < end; i_ref_cur++)
    if (_list[i_ref_cur]._id == _ref_id)
      break;

  size_t i_check = _i_check_next;

  for ( ; ; i_ref_prev = i_ref_cur, i_ref_cur = i_ref_next)
    {
      /* Find the next reference value. */
      for (i_ref_next = i_ref_cur + 1; i_ref_next < end; i_ref_next++)
	if (_list[i_ref_next]._id == _ref_id)
	  goto found_cur_ref;

      /* No further reference value found, give up. */
      break;

    found_cur_ref:
      ;

      uint64_t t_prev = _list[i_ref_prev]._timestamp;
      uint64_t t_cur  = _list[i_ref_cur ]._timestamp;
      uint64_t t_next = _list[i_ref_next]._timestamp;

      if (t_cur < t_prev ||
	  t_next < t_cur)
	{
	  /* The reference has unordered timestamps.  We cannot know
	   * who is closer.  Ignore this interval.
	   */
	  for ( ; i_check < i_ref_next; i_check++)
	    {
	      find_peak(_list[i_check]);
	      _list[i_check]._flags |= TSTAMP_SYNC_INFO_OUT_OF_ORDER_REFS_TS;
	    }
	  continue;
	}

      /* We need to know the peak of the reference. */
      find_peak(_list[i_ref_cur]);

      uint64_t t_from = t_prev + ((t_cur - t_prev) / 2);

      /* Skip until the first midpoint. */
      for ( ; i_check < i_ref_next; i_check++)
	{
	  uint64_t t_check = _list[i_check]._timestamp;

	  if (t_check >= t_from)
	    break;

	  find_peak(_list[i_check]);

	  _list[i_check]._flags |= TSTAMP_SYNC_INFO_NO_ENCLOSING_REFS;
	}

      /* Cannot take direct average, since sum can be too large. */
      uint64_t t_until = t_cur + ((t_next - t_cur) / 2);

      /* Check until the second midpoint. */
      for ( ; i_check < i_ref_next; i_check++)
	{
	  uint64_t t_check = _list[i_check]._timestamp;

	  if (t_check > t_until)
	    break;

	  find_peak(_list[i_check]);

	  /* Check timestamp ordering first. */

	  if (i_check < i_ref_cur)
	    {
	      /* Should not be out of order, and closer to cur than prev. */
	      if (t_check < t_prev ||
		  t_check > t_cur ||
		  t_check - t_prev < t_cur - t_check)
		{
		  _list[i_check]._flags |= TSTAMP_SYNC_INFO_OUT_OF_ORDER_TS;
		  continue;
		}
	    }
	  else
	    {
	      /* Should not be out of order, and closer to cur than next. */
	      if (t_check < t_cur ||
		  t_check > t_next ||
		  t_next - t_check < t_check - t_cur)
		{
		  _list[i_check]._flags |= TSTAMP_SYNC_INFO_OUT_OF_ORDER_TS;
		  continue;
		}
	    }

	  /* After ordering, check for local trigger. */

	  if (_list[i_check]._sync_check_flags &
	      (SYNC_CHECK_LOCAL >> SYNC_CHECK_FLAGS_SHIFT))
	    {
	      _list[i_check]._flags |= TSTAMP_SYNC_INFO_LOCAL_TRIGGER;
	      continue;
	    }

	  /* Is reference a local trigger. */

	  if (_list[i_ref_cur]._sync_check_flags &
	      (SYNC_CHECK_LOCAL >> SYNC_CHECK_FLAGS_SHIFT))
	    {
	      _list[i_check]._flags |= TSTAMP_SYNC_INFO_SPURIOUS;
	      continue;
	    }

	  /* Now, do we match with the current reference? */

	  /* Us having no identified peak is worse than reference
	   * having no assigned peak, so check first.
	   */
	  if      (_list[i_check]._peak == -1)
	    {
	      assert(_list[i_check]._flags);
	    }
	  else if (_list[i_ref_cur]._peak == -1)
	    _list[i_check]._flags |= TSTAMP_SYNC_INFO_NO_REF_PEAK;
	  else if (_list[i_check]._peak == _list[i_ref_cur]._peak)
	    _list[i_check]._flags |= TSTAMP_SYNC_INFO_PEAK_MATCH;
	  else
	    _list[i_check]._flags |= TSTAMP_SYNC_INFO_PEAK_MISMATCH;
	}
    }

  for ( ; i_check < at_least; i_check++)
    {
      find_peak(_list[i_check]);

      _list[i_check]._flags |= TSTAMP_SYNC_INFO_NO_ENCLOSING_REFS;
    }

  /* Account the kinds of issues found. */
  for (size_t i = _i_check_next; i < i_check; i++)
    {
      tstamp_sync_info &ts_sync_info = _list[i];

      if (ts_sync_info._id < 64)
	{
	  tstamp_sync_account *account = &_account[ts_sync_info._id];

	  if (ts_sync_info._flags & TSTAMP_SYNC_INFO_OUT_OF_ORDER_TS)
	    account->_out_of_order_ts++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_OUT_OF_ORDER_REFS_TS)
	    account->_out_of_order_refs_ts++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_NO_ENCLOSING_REFS)
	    account->_no_adjacent_refs++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_LOCAL_TRIGGER)
	    account->_local++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_SPURIOUS)
	    account->_spurious++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_EXTRA)
	    account->_extra++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_NO_VALUE)
	    account->_no_sc_value++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_NO_PEAK_FIT)
	    account->_no_peak_fit++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_OUTSIDE_PEAK)
	    account->_outside++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_AMBIGUOUS_PEAK)
	    account->_ambiguous++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_NO_REF_PEAK)
	    account->_no_ref_peak++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_PEAK_MISMATCH)
	    account->_mismatch++;
	  else if (ts_sync_info._flags & TSTAMP_SYNC_INFO_PEAK_MATCH)
	    account->_match++;
	}
    }

  _i_check_next = i_check;
}

int dct_try_period(uint16_t *vals, int n,
		   int try_period,
		   double *good_sum, int *good_period)
{
  double sum = 0;
  int i;

  double angle = (2 * M_PI) / try_period;

  for (i = 0; i < n && i < (try_period * 5) / 2; i++)
    {
      double cos_value = cos(i * angle);

      sum += cos_value * vals[i];
    }

#if DEBUG_TSTAMP_SYNC_CHECK
  printf ("%2d: %.1f\n", try_period, sum);
#endif

  if (sum < *good_sum)
    return 0;

  *good_sum = sum;
  *good_period = try_period;

  return 1;
}

struct dct_find_phase_info
{
  int    _phase;
  int    _start_i;
  double _sum;
};

void dct_one_period_find_phase(uint16_t *vals, int n,
			       int good_period,
			       int start_i, int start_x,
			       dct_find_phase_info &info)
{
  int good_phase = 0;
  double good_sum = 0.;
  int good_i = start_i;

#if DEBUG_TSTAMP_SYNC_CHECK
  printf ("find_phase: %d..%d..%d\n",
	  start_x, start_x + good_period, start_x + 2 * good_period);
#endif

  for (int phase = 0; phase <= good_period; phase++)
    {
      double sum = 0;
      int i;
      double angle = (2 * M_PI) / good_period;

      /* Note: we will only back up or move forward. */
      /* Back up to first value starting at this location. */
      while (start_i > 0 &&
	     vals[start_i-1] >= start_x)
	start_i--;
      /* Move forward. */
      while (start_i < n &&
	     vals[start_i] < start_x + phase)
	start_i++;

      for (i = start_i; i < n; i++)
	{
	  int x = vals[i];

	  if (x > start_x + good_period + phase)
	    break;

	  double cos_value = -cos((x - (start_x + phase)) * angle);

	  sum += cos_value;
	}

#if DEBUG_TSTAMP_SYNC_CHECK
      printf ("%2d: %.1f\n", phase, sum);
#endif

      if (sum > good_sum)
	{
	  good_sum   = sum;
	  good_phase = phase;
	  good_i     = start_i;
	}
    }

  info._phase   = good_phase;
  info._sum     = good_sum;
  info._start_i = good_i;
}

/* Function to figure out the 'period' of the sync check values.  The
 * 'period' is the difference between the nominal sync check values.
 *
 * Note: it is not a time period.
 */

void tstamp_sync_check::estimate_ref_sync_value_period(size_t end,
						       int *peaks, int *npeaks)
{
  uint16_t diffs[4096];
  uint16_t ref_vals[256];
  int n = 0;

  /* The number of reference values that are used is limited to 256.
   * This is enough since we expect at most 32 or 64 different values
   * (typically much less).  This still gives statistics for each
   * value.
   */

  memset(diffs, 0, sizeof (diffs));

  /* Collect all sync check values for the reference system, when it
   * reports to be a reference.  But not when it reports local
   * trigger.
   */

  for (size_t i = 0; i < end; i++)
    {
      if (_list[i]._id == _ref_id &&
	  (_list[i]._sync_check_flags & (1 | 2)) &&
	  !(_list[i]._sync_check_flags & 4))
	{
	  ref_vals[n++] = _list[i]._sync_check_value;

	  /* Do not use more values than known in the list. */
	  if (n >= 256)
	    break;
	}
    }

  /* Sort the values. */

  std::sort(ref_vals, ref_vals+n);

#if DEBUG_TSTAMP_SYNC_CHECK
  printf ("refs: ");
  for (int i = 0; i < n; i++)
    printf (" %d", ref_vals[i]);
  printf ("\n");
#endif

  /* Calculated the distances between _all pairs_ of values.
   *
   * A histogram of the thus calculated differences should have peaks
   * around (positive side) of 0, and around each multiple of the
   * differences between the sync check values.
   *
   * Note: if the sync check values are non-linear, then there would
   * be multiple peaks.  The game here is that even if we have some
   * slight non-linearities, they would lead to larger deviations
   * for the longer distances.  Since we will only use the first two
   * differences, the effect is not very large.
   *
   * Rough histogram (for difference 6):
   *
   * Diff:   0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 20..
   * Counts: 40 10 0  0  0  5  80 5  0  0  0  10 60 10 0  0  0  15 40 15 0
   *
   * Assuming that we have n different values.  If all distances are
   * roughly the same, we will then have n peaks (including the 0th
   * peak).  With a total statistics of m counts (typically 256), we
   * will then have approximately m/n members of each peak in the raw
   * data, call that N (=m/n).  Since all pairs are accumulated:
   *
   * The 0th difference peak thus contain n*N*(N-1)/2 ~= n*N^2/2 values,
   * i.e. all self-differences.
   *
   * The 1st difference peak would contain (n-1)*N^2 values.
   *
   * The 2nd difference peak would contain (n-2)*N^2 values.
   */

  for (int i = 0; i < n; i++)
    for (int j = 0; j < i; j++)
      {
	int diff = abs(((int) ref_vals[i]) - ((int) ref_vals[j]));

	/* printf ("%d ", diff); */

	if (diff < 4096)
	  diffs[diff]++;
      }

#if DEBUG_TSTAMP_SYNC_CHECK
  printf ("diffs: ");
  for (int i = 0; i < 4096; i++)
    printf (" %d", diffs[i]);
  printf ("\n");
#endif

  /* To find the period, we try successively larger periods, and
   * count the number of values in the difference histogram in
   * a few intervals surrounding the expected 0th, 1st and 2nd peak,
   * as well as the areas in-between:
   *
   * [0]:  0     ..    P/4   around 0th peak   ref
   * [1]:  1*P/4 ..  3*P/4                     < ref/10
   * [2]:  3*P/4 ..  5*P/4   around 1st peak   > ref
   * [3]:  5*P/4 ..  7*P/4                     < ref/5
   * [4]:  7*P/4 ..  9*P/4   around 2nd peak   > ref/2
   * [4]:  9*P/4 .. 11*P/4                     < ref/2
   *
   * _    __    __
   *  |__|  |__|  |
   * 0  1  2  3  4
   *
   * We then use the first period which manages to find within the 1st
   * and 2nd peaks at least half the number of counts as are in the
   * zeroth peak, and at the same time at most 1/10 as many counts in
   * the regions in-between.
   *
   * When that condition is fulfilled, the search is continued
   * as long as a quality measure based on a product of the number
   * of counts in the regions that expect counts divided by the number
   * of counts in the regions that expect no/few counts.  If the no/few
   * count regions have no counts, the division is with 1.
   */

  int try_period;
  int good_period_1 = 0;
  double good_quality = 0.;

  for (try_period = 1; try_period < 50; try_period++)
    {
      int counts[6];
      int i, j;

      memset(counts, 0, sizeof (counts));

      i = 0;
      for (j = 0; j < 6; j++)
	{
	  for ( ; i <= (try_period * (1 + 2*j)) / 4; i++)
	    counts[j] += diffs[i];
	}

      double be_high =
	((double) counts[0]) * ((double) counts[2]) * ((double) counts[4]);
      double be_low =
	((double) counts[1]) * ((double) counts[3]) * ((double) counts[5]);

      if (be_low < 1)
	be_low = 1;

      double quality = be_high / be_low;

#if DEBUG_TSTAMP_SYNC_CHECK
      printf ("%2d: %5d %5d %5d %5d %5d %5d  %.1f\n",
	      try_period,
	      counts[0], counts[1],
	      counts[2], counts[3],
	      counts[4], counts[5],
	      quality);
#endif

      if (counts[1] < counts[0] / 10 &&
	  counts[2] > counts[0]      &&
	  counts[3] < counts[0] / 10 &&
	  counts[4] > counts[0] / 2  &&
	  counts[5] < counts[0] / 2)
	{
	  /* We cannot be lower than 0, so first round always accepted
	   * (no break).
	   */
	  if (quality <= good_quality)
	    break;

	  good_period_1 = try_period;
	  good_quality = quality;
	}
      else if (good_period_1)
	break;
    }

#if DEBUG_TSTAMP_SYNC_CHECK
  printf ("good_period_1: %d\n", good_period_1);
#endif

  /* To further optimise the period, a cosine function with varying
   * period is convoluted with the data, up to 2.5 periods.  The
   * inclusion ends at a half-period, where no data is expected.  This
   * since we want to end at low diff counts, to not be affected by
   * how much diffs we manage to include at the edge.
   *
   * In principle, we could work with sub-integer period steps.
   */

  double good_sum = 0.;
  int good_period = 0;

  for (try_period = good_period_1; try_period > 1; try_period--)
    if (!dct_try_period(diffs, n, try_period, &good_sum, &good_period))
      break;
  for (try_period = good_period_1+1; try_period < 2*good_period_1;
       try_period++)
    if (!dct_try_period(diffs, n, try_period, &good_sum, &good_period))
      break;

#if DEBUG_TSTAMP_SYNC_CHECK
  printf ("good_period: %d\n", good_period);
#endif

  /* Find the location of the peaks in the original data.
   *
   * This is also done with a cosine function convolution.  The search
   * is effectively finding the phase of the cosine function.  This
   * time for one period, from -PI to PI, i.e. wanting to find a peak
   * at the center of the function, and no data at the edges.
   *
   * Since the rough period is known, the search needs only to
   * consider one period.
   *
   * Once the first peak has been found, the same game is repeated
   * successively further away from the center.
   */

  int start_i = n/2;
  int start_x = ref_vals[start_i] - good_period / 2;

  double sum_prev = 1;
  int forward = 1;

  *npeaks = 0;

  for ( ; ; )
    {
      dct_find_phase_info info;

      dct_one_period_find_phase(ref_vals, n,
				good_period,
				start_i, start_x,
				info);

      if (info._sum < 0.25 * sum_prev)
	{
	  /* Very little data in peak - assume it is noise.
	   * Stop search in this direction.
	   */

	  if (!forward)
	    break; /* Done. */

	  forward = 0;

	  if (!*npeaks)
	    break; /* We had no start peak even. */

	  start_x = peaks[0] - good_period * 2;
	  continue; /* Try again. */
	}

      int peak_x = start_x + info._phase + good_period / 2;

      peaks[(*npeaks)++] = peak_x;

#if DEBUG_TSTAMP_SYNC_CHECK
      printf ("good_phase: %d (-> %d)\n",
	      info._phase, peak_x);
#endif

      sum_prev = info._sum;
      start_x = peak_x;
      start_i = info._start_i;

      if (forward)
	start_x = peak_x;
      else
	start_x = peak_x - good_period * 2;
    }

  std::sort(peaks, peaks + *npeaks);

#if DEBUG_TSTAMP_SYNC_CHECK
  printf ("peaks: ");
  for (int i = 0; i < *npeaks; i++)
    printf (" %d", peaks[i]);
  printf ("\n");
#endif
}

bool tstamp_sync_corr::operator<(const tstamp_sync_corr &rhs) const
{
  if (_id < rhs._id)
    return true;
  if (_id > rhs._id)
    return false;

  if (_ref_peak_i < rhs._ref_peak_i)
    return true;
  if (_ref_peak_i > rhs._ref_peak_i)
    return false;

  if (_sync_check_value < rhs._sync_check_value)
    return true;
  return false;
}


void tstamp_sync_check::estimate_sync_values(size_t end,
					     const int *peaks,
					     const int *npeaks)
{
  /* For each ID (reference or not) we would like to associate
   * it with the closest reference value (timestamp-wise), which
   * then allows to determine which peak it belongs to.
   *
   * After that, except for spurious outliers, the values for each
   * such group should be a sharp distribution, which we can determine
   * the location and width of.
   */

  /* How to match a value with a reference value:
   *
   * Work in steps of the reference values.  This gives a previous
   * and current reference timestamp and value.
   *
   * Within such a region, any other system can be associated with the
   * closest reference value.  In case there are additional (spurious)
   * timestamps, those are ignored, i.e. only the closest is taken.
   *
   * As an additional complication: if a system has a timestamp on both
   * sides of the reference value, only the closest is taken.
   */

  size_t i_ref_prev;
  size_t i_ref_cur;
  size_t i_ref_next;

  /* Find the first reference value. */
  for (i_ref_prev = 0; i_ref_prev < end; i_ref_prev++)
    if (_list[i_ref_prev]._id == _ref_id)
      break;
  for (i_ref_cur = i_ref_prev + 1; i_ref_cur < end; i_ref_cur++)
    if (_list[i_ref_cur]._id == _ref_id)
      break;

  size_t i_check = i_ref_prev + 1;

  size_t n_corr = 0;

  for ( ; ; i_ref_prev = i_ref_cur, i_ref_cur = i_ref_next)
    {
      /* Find the next reference value. */
      for (i_ref_next = i_ref_cur + 1; i_ref_next < end; i_ref_next++)
	if (_list[i_ref_next]._id == _ref_id)
	  goto found_cur_ref;

      /* No further reference value found, give up. */
      break;

    found_cur_ref:
      ;

      uint64_t t_prev = _list[i_ref_prev]._timestamp;
      uint64_t t_cur  = _list[i_ref_cur ]._timestamp;
      uint64_t t_next = _list[i_ref_next]._timestamp;

      if (t_cur < t_prev ||
	  t_next < t_cur)
	{
	  /* The reference has unordered timestamps.  We cannot know
	   * who is closer.  Ignore this interval.
	   */
	  i_check = i_ref_next + 1;
	  continue;
	}

      uint16_t ref_peak_i;

      uint16_t best_ref_peak_i = -1;
      int      best_ref_dist = INT_MAX;

      /* Which peak value is the current reference closest to? */

      for (ref_peak_i = 0; ref_peak_i < *npeaks; ref_peak_i++)
	{
	  int dist = abs(((int) _list[i_ref_cur]._sync_check_value) -
			 ((int) peaks[ref_peak_i]));

	  if (dist < best_ref_dist)
	    {
	      best_ref_peak_i = ref_peak_i;
	      best_ref_dist = dist;
	    }
	}

      /*
      printf ("%d %d  E:%d\n",
	      best_ref_peak_i, best_ref_dist,
	      _list[i_ref_cur]._event_no);
      */

      if (best_ref_peak_i == (uint16_t) -1)
	continue;

      ref_peak_i = best_ref_peak_i;

      /* Cannot take direct average, since sum can be too large. */
      uint64_t t_until = t_cur + ((t_next - t_cur) / 2);

      for ( ; i_check < i_ref_next; i_check++)
	{
	  uint64_t t_check = _list[i_check]._timestamp;

	  if (t_check > t_until)
	    break;

	  /* Note: we also check for the reference value itself. */

	  if (i_check < i_ref_cur)
	    {
	      /* Should not be out of order, and closer to cur than prev. */
	      if (t_check < t_prev ||
		  t_check > t_cur ||
		  t_check - t_prev < t_cur - t_check)
		continue;
	    }
	  else
	    {
	      /* Should not be out of order, and closer to cur than next. */
	      if (t_check < t_cur ||
		  t_check > t_next ||
		  t_next - t_check < t_check - t_cur)
		continue;
	    }

	  /* So the value should be checked. */
	  /*
	  printf ("id: %02x  value: %5d  ref_value: %5d (%2d) E:%d\n",
		  _list[i_check]._id,
		  _list[i_check]._sync_check_value,
		  _list[i_ref_cur]._sync_check_value, ref_peak_i,
		  _list[i_check]._event_no);
	  fflush(stdout);
	  */

	  /* Do not add values which are marked as local trigger. */
	  if (_list[i_check]._sync_check_flags &
	      (SYNC_CHECK_LOCAL >> SYNC_CHECK_FLAGS_SHIFT))
	    continue;

	  /* We add the value to list of values. */

	  _corr[n_corr]._id         = _list[i_check]._id;
	  _corr[n_corr]._ref_peak_i = ref_peak_i;
	  _corr[n_corr]._sync_check_value = _list[i_check]._sync_check_value;

	  n_corr++;

	  /* TODO TODO TODO: only add if closer to this
	   * reference than a previous.
	   *
	   * Replace previous if closer to this reference.
	   */
	}
    }

  std::sort(_corr, _corr + n_corr);

  for (int id = 0; id < 64; id++)
    for (int peak = 0; peak < 64; peak++)
      _expect[id][peak]._flags = 0;

  for (size_t i = 0; i < n_corr; )
    {
      size_t j;

      for (j = i+1; j < n_corr; j++)
	{
	  if (_corr[j]._id != _corr[i]._id ||
	      _corr[j]._ref_peak_i != _corr[i]._ref_peak_i)
	    break;
	}

      /* Assume data from 25% to 75% of statistics is what we want. */

      /* Find the range of 50 % of values which give the smallest
       * difference.
       */

      int min_diff = INT_MAX;
      int cut_low = 0, cut_high = 0;

      for (size_t k = 0; k <= 16; k++)
	{
	  size_t low_i  = i + ((j - i) *  k      ) / 32;
	  size_t high_i = i + ((j - i) * (k + 16)) / 32;

	  if (high_i > low_i)
	    {
	      int val_low  = _corr[low_i     ]._sync_check_value;
	      int val_high = _corr[high_i - 1]._sync_check_value;

	      if (val_high - val_low < min_diff)
		{
		  min_diff = val_high - val_low;

		  cut_low  = val_low  - (val_high - val_low) - 2;
		  cut_high = val_high + (val_high - val_low) + 2;
		}
	    }
	}

      double sum    = 0.;
      double sum_x  = 0.;
      double sum_x2 = 0.;

      for (size_t k = i; k < j; k++)
	{
	  uint16_t value = _corr[k]._sync_check_value;

	  if (value >= cut_low &&
	      value <= cut_high)
	    {
	      sum++;
	      sum_x  += value;
	      sum_x2 += value * value;
	    }

#if DEBUG_TSTAMP_SYNC_CHECK
	  printf ("%d ", value);
#endif
	}
#if DEBUG_TSTAMP_SYNC_CHECK
      printf ("\n");
#endif

#if DEBUG_TSTAMP_SYNC_CHECK
      printf ("%02x %3d %5d   %5d %5d",
	      _corr[i]._id,
	      _corr[i]._ref_peak_i,
	      _corr[i]._sync_check_value,
	      cut_low, cut_high);
      fflush(stdout);
#endif

      if (sum >= 2)
	{
	  double mean;
	  double var;

	  mean = sum_x / sum;
	  var  = (sum_x2 - sum_x*sum_x / sum)/(sum-1);

#if DEBUG_TSTAMP_SYNC_CHECK
	  printf ("  %6.1f %6.1f\n",
		  mean, sqrt(var));
	  fflush(stdout);
#endif

	  if (_corr[i]._id < 64 &&
	      _corr[i]._ref_peak_i < 64)
	    {
	      tstamp_sync_expect *expect =
		&_expect[_corr[i]._id][_corr[i]._ref_peak_i];

	      expect->_flags     = 1;
	      expect->_mean      = mean;
	      expect->_tolerance = sqrt(5*5 * var + 1*1);
	    }
	}
      else
	{
#if DEBUG_TSTAMP_SYNC_CHECK
	  printf ("  -\n");
	  fflush(stdout);
#endif
	}

      i = j;
    }

  /* For us to like the expected values, they have to be in order.
   *
   * We do not care if they are close, but if not in order, we may
   * have a static shift (in case the values are always delivered
   * in sequence).
   *
   * We also require all peaks to have associated values (else cannot
   * check order).
   *
   * If values are too close, that will be reported as being ambiguous.
   */

  for (int id = 0; id < 64; id++)
    {
      _good_expect[id] = 0;

      for (int peak = 0; peak < *npeaks; peak++)
	{
	  tstamp_sync_expect *expect = &_expect[id][peak];

	  if (!(expect->_flags & 1))
	    goto bad_peaks;

	  if (peak > 0 &&
	      expect->_mean <= (expect-1)->_mean)
	    goto bad_peaks;
	}

#if DEBUG_TSTAMP_SYNC_CHECK
      printf ("Accept expect! %02x\n", id);
#endif

      if (*npeaks >= 2)
	_good_expect[id] = 1;

    bad_peaks:
      ;
    }

  _num_sync_check_peaks = *npeaks;
}





void tstamp_sync_check::analyse(bool to_end)
{
  size_t mid;
  size_t analyse_end;
  size_t i;

  int peaks[256]; /* Cannot need more than number of reference values. */
  int npeaks;

  /* Figure out how far to analyse. */
  if (to_end)
    {
      analyse_end = _num_items;
      mid = analyse_end;
    }
  else
    {
      ssize_t last_i;
      ssize_t mid_i;
      ssize_t prev_i;

      /* Find the three last reference values (prev, mid, last).
       *
       * Actual matching will run to the midpoint (timestamp-wise)
       * between mid and last.
       *
       * We then retain values from mid for next round.
       */

      for (last_i = _num_items-1; last_i >= 0; last_i--)
	if (_list[last_i]._id == _ref_id)
	  break;

      for (mid_i = last_i-1; mid_i >= 0; mid_i--)
	if (_list[mid_i]._id == _ref_id)
	  break;

      for (prev_i = mid_i-1; prev_i >= 0; prev_i--)
	if (_list[prev_i]._id == _ref_id)
	  break;

      if (mid_i < 0)
	mid_i = 0;

      if (prev_i < 0)
	prev_i = 0;

      analyse_end = last_i + 1;

      mid = mid_i;

      /* That three reference values are not present does typically
       * not happen.  If it happens, we must still get rid of some
       * data from the buffer (it will be marked with various kinds of
       * errors).
       *
       * Since we typically remove data until the mid (second-last)
       * reference value, if that does not exist, then we arbitrarily
       * consume ('analyse') half the items.
       *
       * (Any reason to not use all?)
       */
      if (mid <= 0)
	{
	  mid = _num_items / 2;
	  if (analyse_end <= mid)
	    analyse_end = mid;
	}
    }

  /* Figure out the typical sync check values. */
  estimate_ref_sync_value_period(analyse_end, peaks, &npeaks);

  /* Figure out the mapping of sync check values for each id vs. the
   * reference.
   */
  estimate_sync_values(analyse_end, peaks, &npeaks);

  /* Investigate the actual matching of sync check values. */
  analyse_values(analyse_end, mid);



  /* Dump the items. */
  for (i = _i_dump_next; i < _i_check_next; i++)
    {
      dump(_list[i]);
    }

  _i_dump_next = _i_check_next;

  /* Copy from the mid and forward. */
  size_t j = mid;

  /* Copy the unused items. */
  for (i = 0; j < _num_items; i++, j++)
    _list[i] = _list[j];

  _num_items = i;

  /* These many events were removed. */
  _i_check_next -= mid;
  _i_dump_next  -= mid;
}



void tstamp_sync_check::account(tstamp_sync_info &ts_sync_info)
{
  if (_num_items >= _num_alloc)
    analyse(false);

  /* That should have free'd up some items. */
  assert(_num_items < _num_alloc);

  /* Store the most recent item. */
  _list[_num_items++] = ts_sync_info;
}

void tstamp_sync_check::show()
{
  analyse(true);

  show_accounts(_account);
}
