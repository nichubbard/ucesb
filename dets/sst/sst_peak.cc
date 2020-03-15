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

#include "sst_peak.hh"

#include <math.h>

#include "fit_parabola.hh"

#include <stdio.h>
#include <string.h>

#include <stdlib.h>

#include "gauss_fit.hh"


#define SID_PEAK_DEBUG_PRINT 0
#define SID_PEAK_CALIB_PRINT 1


#if 0
void test_parabola1()
{
  double a = -2.0 + (4.0 * random()) / RAND_MAX;
  double b = -2.0 + (4.0 * random()) / RAND_MAX;
  double c = -2.0 + (4.0 * random()) / RAND_MAX;

  int n = 3 + (int) ((5.0 * random()) / RAND_MAX);

  fit_parabola pfit;
  pfit.clear();

  for (int i = 0; i < n; i++)
    {
      double x = -4.0 + (8.0 * random()) / RAND_MAX;

      double y = a + b * x + c * x * x;

      pfit.fill(x,y,1.0);
    }

  double fa = pfit.a();
  double fb = pfit.b();
  double fc = pfit.c();

  printf ("%6.2f %6.2f %6.2f\n",a-fa,b-fb,c-fc);

}


void test_parabola2()
{
  double x0 = -2.0 + (4.0 * random()) / RAND_MAX;
  double y0 = -2.0 + (4.0 * random()) / RAND_MAX;
  double c = -2.0 + (4.0 * random()) / RAND_MAX;

  int n = 3 + (int) ((5.0 * random()) / RAND_MAX);

  fit_parabola pfit;
  pfit.clear();

  for (int i = 0; i < n; i++)
    {
      double x = -4.0 + (8.0 * random()) / RAND_MAX;

      double y = c * (x - x0) * (x - x0) + y0;

      pfit.fill(x,y,1.0);
    }

  double fx0 = pfit.x0();
  double fy0 = pfit.y0();
  double fc  = pfit.c0();

  printf ("%6.2f %6.2f %6.2f\n",x0-fx0,y0-fy0,c-fc);

}
#endif



struct sid_peak_info
{
  double strip;
  double peak;
  double c;
};

sid_peak_info   sid_peaks[1024];
int           n_sid_peaks;

struct sid_mini_peak_info
{
  double strip;
  double peak;
  double c;
};

sid_mini_peak_info   sid_mini_peaks[1024];
int                n_sid_mini_peaks;

void siderem_peaks(int det,sid_data &data,
		   hit_event_PEAK &peaks)
{
#if 0
  for (int i = 0; i < 100; i++)
    {
      test_parabola1();
      test_parabola2();
    }
#endif

  n_sid_peaks      = 0;
  n_sid_mini_peaks = 0;

  // Find the peaks...

  // First locate the maximum value

 attempt_again:

  double max_val = 0;
  int    max_i = -1;

  for (int i = 0; i < data.n; i++)
    if (data.data[i].value > max_val &&
	data.data[i].value > 25)
      {
	max_val = data.data[i].value;
	max_i   = i;
      }

  if (max_i == -1)
    {
#if SID_PEAK_CALIB_PRINT
      if (n_sid_peaks)
	{
	  printf ("DF %d",det);

	  for (int i = 0; i < n_sid_peaks; i++)
	    {
	      printf (" : %7.2f %5.2f %6.2f",
		      sid_peaks[i].strip,
		      sid_peaks[i].peak,
		      sid_peaks[i].c);
	    }
	  printf ("\n");
	}
      if (n_sid_mini_peaks)
	{
	  printf ("DH %d",det);

	  for (int i = 0; i < n_sid_mini_peaks; i++)
	    {
	      printf (" : %7.2f %5.2f",
		      sid_mini_peaks[i].strip,
		      sid_mini_peaks[i].peak);
	    }
	  printf ("\n");
	}
#endif
      return;
    }

  int max_strip = data.data[max_i].strip;

#if SID_PEAK_DEBUG_PRINT
  printf ("%d: Max: %6.1f @ %4d (%d)\n",
	  det,
	  data.data[max_i].value,
	  data.data[max_i].strip,
	  max_i);
#endif

  // Now, see how far back and forth this stretches

  int first_i = max_i;
  int last_i  = max_i;

  double first_val = max_val;
  double last_val  = max_val;

  double threshold = max_val / 3 > 20 ? 20 : max_val / 3;

  for (int i = max_i; i >= 0 && i >= first_i - 3; --i)
    {
      if (data.data[i].strip < data.data[first_i].strip - 3)
	break;

      if (data.data[i].value > threshold &&
	  data.data[i].value < first_val * 2)
	{
	  first_i   = i;
	  first_val = data.data[i].value;
	}
    }

  for (int i = max_i; i < data.n && i <= last_i + 3; ++i)
    {
      if (data.data[i].strip > data.data[last_i].strip + 3)
	break;

      if (data.data[i].value > threshold &&
	  data.data[i].value < last_val * 2)
	{
	  last_i   = i;
	  last_val = data.data[i].value;
	}
    }

  int first2_i = first_i;
  int last2_i  = last_i;

  int limit_i;

  limit_i = first2_i - 3;

  for (int i = first_i; i >= 0 && i >= limit_i; --i)
    {
      if (data.data[i].strip < data.data[first2_i].strip - 3)
	break;

      first2_i   = i;
    }

  limit_i = last2_i + 3;

  for (int i = last_i; i < data.n && i <= limit_i; ++i)
    {
      if (data.data[i].strip > data.data[last2_i].strip + 3)
	break;

      last2_i   = i;
    }
  /*
  printf ("First: %4d @ %4d (%d)\n",
	  data.data[first_i].value,
	  data.data[first_i].strip,
	  first_i);
  printf ("Last: %4d @ %4d (%d)\n",
	  data.data[last_i].value,
	  data.data[last_i].strip,
	  last_i);
  */

  ///////////////////////////////////////////////////////////
  // Now, try to do some fitting of the data

#define SIGMA_CONST_FACTOR 20.0

  {
    fit_parabola pfit;
    pfit.clear();

    int n_fit = 0;

    for (int i = first_i; i <= last_i; i++)
      {
	if (data.data[i].value > 20)
	  {
	    double strip = data.data[i].strip;
	    double value = data.data[i].value;

	    pfit.fill_inv(strip - max_strip,
			  log(value),
			  value/SIGMA_CONST_FACTOR);
	    n_fit++;
	  }
      }

    double f_c     = pfit.c0();
    double f_strip = pfit.x0() + max_strip;
    double f_peak  = pfit.y0();

#if SID_PEAK_DEBUG_PRINT
    printf (" %6.1f  :      %8.2f (%5.2f) (%5.2f) %d\n",
	    f_strip,f_peak,f_c,f_peak + log(sqrt(-f_c)),n_fit);
#endif

    if (n_fit >= 4)
      {
	function_fit<gauss_function> fg;

	fg._cur._param._center = f_strip;
	fg._cur._param._peak   = exp(f_peak);
	fg._cur._param._c      = f_c;

	fg._step._param._center = .1;
	fg._step._param._peak   = fg._cur._param._peak / 10.0;
	fg._step._param._c      = .05;

	fg.set_length(n_fit);

	for (int i = first_i; i <= last_i; i++)
	  if (data.data[i].value > 20)
	    {
	      fg.set_value(data.data[i].strip,
			   data.data[i].value);
	    }

	fg.fit();
      }



  ///////////////////////////////////////////////////////////

#define SQR(x) ((x)*(x))

    double log_diff[1024];

  int prev_i = first2_i - 1;
  for (int i = first2_i; i <= last2_i; i++)
    {
      if (i != prev_i+1)
	printf ("         : -\n");

      double strip = data.data[i].strip;
      double value = data.data[i].value;

      double log_value = log(value);

      double fit_log_val = f_c * SQR (strip - f_strip) + f_peak;

#if SID_PEAK_DEBUG_PRINT
      printf ("%d  %4d%c : %7.1f  (%5.2f - %5.2f = %5.2f)\n",
	      det,data.data[i].strip,
	      i >= first_i && i <= last_i ? '*' : ' ',
	      data.data[i].value,
	      log_value,fit_log_val,
	      log_value-fit_log_val);
#endif

      log_diff[i] = log_value-fit_log_val;

      prev_i = i;
    }

  if (last_i + 1 - first_i >= 5 && n_fit >= 4)
    {
#if SID_PEAK_CALIB_PRINT
      for (int i1 = first_i+1; i1 < last_i-1; i1++)
	for (int i2 = i1+1; i2 < last_i; i2++)
	  {
	    printf ("DD %d %3d %3d %6.2f\n",
		    det,data.data[i1].strip,data.data[i2].strip,log_diff[i1]-log_diff[i2]);

	  }
#endif

      sid_peaks[n_sid_peaks].strip = f_strip;
      sid_peaks[n_sid_peaks].peak  = f_peak;
      sid_peaks[n_sid_peaks].c     = f_c;
      n_sid_peaks++;

      hit_event_PEAK_one &item =
	peaks.hits.do_insert_index(peaks.hits._num_items);

      item.center = (float) f_strip;
      item.sum    = (float) f_peak;
      item.c0     = (float) f_c;

    }
  else if (last_i + 1 - first_i <= 4)
    {
      double sum   = 0;
      double sum_x = 0;

      for (int i = first_i; i <= last_i; i++)
	{
	  double strip = data.data[i].strip;
	  double value = data.data[i].value;

	  sum   += value;
	  sum_x += value * strip;
	}

      double mean_x = sum_x / sum;

      sid_mini_peaks[n_sid_mini_peaks].strip = mean_x;
      sid_mini_peaks[n_sid_mini_peaks].peak = log(sum);
      n_sid_mini_peaks++;
    }
   }

  ///////////////////////////////////////////////////////////

  // now, remove the data that we used, and continue

  memmove(&data.data[first_i],&data.data[last_i+1],sizeof (data.data[0]) * (data.n - (last_i+1)));

  data.n -= (last_i+1 - first_i);

  goto attempt_again;

}
