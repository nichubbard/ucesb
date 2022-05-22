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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <math.h>

#include <algorithm>

tstamp_sync_check::tstamp_sync_check(char const *command)
{
  _ref_id = 0x10;

  _prev_timestamp = 0;

  _num_items = 0;
  _num_alloc = 4096;

  _list = new tstamp_sync_info[_num_alloc];

  if (!_list)
    ERROR("Memory allocation failure!");
}

void tstamp_sync_check::dump(tstamp_sync_info &ts_sync_info)
{
  uint64_t ts;
  
  if (ts_sync_info._timestamp != 0 &&
      ts_sync_info._timestamp - _prev_timestamp > 2000)
    {
      printf ("\n");
    }

  printf ("%s%-12u%s %s%02x%s  ",
	  CT_OUT(BOLD_BLUE),
	  ts_sync_info._event_no,
	  CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD),
	  ts_sync_info._id,
	  CT_OUT(NORM));

  ts = ts_sync_info._timestamp;

  printf ("%s%04x%s:%s%04x%s:%s%04x%s:%s%04x%s",
	  CT_OUT(BOLD_BLUE), (int) (ts >> 48) & 0xffff, CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD_BLUE), (int) (ts >> 32) & 0xffff, CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD_BLUE), (int) (ts >> 16) & 0xffff, CT_OUT(NORM_DEF_COL),
	  CT_OUT(BOLD_BLUE), (int) (ts      ) & 0xffff, CT_OUT(NORM_DEF_COL));

  if (ts_sync_info._timestamp == 0)
    printf ("  %s%10s%s",
	    CT_OUT(BOLD_RED), "err", CT_OUT(NORM_DEF_COL));
  else
    printf ("  %s%10" PRIu64 "%s",
	    CT_OUT(BOLD), ts_sync_info._timestamp - _prev_timestamp,
	    /* */                                       CT_OUT(NORM));

  printf ("  %s[%x]%s %s%5d%s ",
	  CT_OUT(BOLD), ts_sync_info._sync_check_flags, CT_OUT(NORM),
	  CT_OUT(BOLD_MAGENTA), ts_sync_info._sync_check_value,
	  /*                                         */ CT_OUT(NORM_DEF_COL));

  printf ("\n");

  if (ts_sync_info._timestamp != 0)
    {
      _prev_timestamp = ts_sync_info._timestamp;
    }
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

  printf ("%2d: %.1f\n", try_period, sum);

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

  printf ("find_phase: %d..%d..%d\n",
	  start_x, start_x + good_period, start_x + 2 * good_period);

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

      printf ("%2d: %.1f\n", phase, sum);

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

void tstamp_sync_check::estimate_ref_sync_value_period(size_t end)
{
  uint16_t diffs[4096];
  uint16_t ref_vals[256];
  int n = 0;

  int peaks[256]; /* Cannot be more then number of reference values. */
  int npeaks = 0;

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

	  if (n >= 256)
	    break;
	}
    }

  /* Sort the values. */

  std::sort(ref_vals, ref_vals+n);

  printf ("refs: ");
  for (int i = 0; i < n; i++)
    printf (" %d", ref_vals[i]);
  printf ("\n");

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
   * Counts: 90 10 0  0  0  5  80 5  0  0  0  10 60 10 0  0  0  15 40 15 0
   */

  for (int i = 0; i < n; i++)
    for (int j = 0; j < i; j++)
      {
	int diff = abs(((int) ref_vals[i]) - ((int) ref_vals[j]));

	/* printf ("%d ", diff); */

	if (diff < 4096)
	  diffs[diff]++;
      }

  printf ("diffs: ");
  for (int i = 0; i < 4096; i++)
    printf (" %d", diffs[i]);
  printf ("\n");

  /* To find the period, we try successively larger periods, and
   * count the number of values in the difference histogram in
   * a few intervals surrounding the expected 0th, 1st and 2nd peak,
   * as well as the areas inbetween:
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
   * and 2nd peaks at least half the numbe of counts as are in the
   * zeroth peak, and at the same time at most 1/10 as many counts in
   * the regions inbetween.
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

      printf ("%2d: %5d %5d %5d %5d %5d %5d  %.1f\n",
	      try_period,
	      counts[0], counts[1],
	      counts[2], counts[3],
	      counts[4], counts[5],
	      quality);

      if (counts[1] < counts[0] / 10 &&
	  counts[2] > counts[0]      &&
	  counts[3] < counts[0] / 10 &&
	  counts[4] > counts[0] / 2  &&
	  counts[5] < counts[0] / 2)
	{
	  /* We cannot be lower than 0. */
	  if (quality <= good_quality)
	    break;

	  good_period_1 = try_period;
	  good_quality = quality;
	}
      else if (good_period_1)
	break;
    }

  printf ("good_period_1: %d\n", good_period_1);

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

  printf ("good_period: %d\n", good_period);

  /* Find the location of the peaks in the original data.
   *
   * This is also done with a cosine function convolution.  The serach
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

	  if (!npeaks)
	    break; /* We had no start peak even. */

	  start_x = peaks[0] - good_period * 2;
	  continue; /* Try again. */
	}

      int peak_x = start_x + info._phase + good_period / 2;

      peaks[npeaks++] = peak_x;

      printf ("good_phase: %d (-> %d)\n",
	      info._phase, peak_x);

      sum_prev = info._sum;
      start_x = peak_x;
      start_i = info._start_i;

      if (forward)
	start_x = peak_x;
      else
	start_x = peak_x - good_period * 2;
    }

  std::sort(peaks, peaks + npeaks);

  printf ("peaks: ");
  for (int i = 0; i < npeaks; i++)
    printf (" %d", peaks[i]);
  printf ("\n");


}

void tstamp_sync_check::analyse(bool to_end)
{
  size_t analyse_end;
  size_t i;

  /* Figure out how far to analyse. */
  if (to_end)
    analyse_end = _num_items;
  else
    {
      ssize_t last_i;
      ssize_t prev_i;
      uint64_t max_diff = 0;

      /* Find the two last reference values. */
      /* Find the largest timestamp difference between any items
       * between these two values.
       */

      for (last_i = _num_items-1; last_i >= 0; last_i--)
	if (_list[last_i]._id == _ref_id)
	  break;

      for (prev_i = last_i-1; prev_i >= 0; prev_i--)
	if (_list[prev_i]._id == _ref_id)
	  break;

      if (prev_i < 0)
	prev_i = 0;

      analyse_end = 0;

      for (i = prev_i ; (ssize_t) i < last_i; i++)
	{
	  uint64_t diff = 0;

	  if (_list[i+1]._timestamp >= _list[i]._timestamp)
	    diff = _list[i+1]._timestamp - _list[i]._timestamp;

	  if (diff >= max_diff)
	    {
	      max_diff = diff;
	      analyse_end = i+1;
	    }
	}

      /*
      printf ("%zd %zd %zd -----------------\n",
	      prev_i, last_i, analyse_end);
      */

      /* This typically does not happen.  To ensure progress. */
      if (analyse_end <= 0)
	analyse_end = _num_items / 2;
    }

  estimate_ref_sync_value_period(analyse_end);






  /* Dump the items. */
  for (i = 0; i < analyse_end; i++)
    {
      dump(_list[i]);
    }

  /* Copy the unused items. */
  for (i = 0; analyse_end < _num_items; i++, analyse_end++)
    _list[i] = _list[analyse_end];

  _num_items = i;
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
}
