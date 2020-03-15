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

#include "corr_plot_dense.hh"

#include "convert_picture.hh"

void dense_corr::picture(const char *filename)
{
  // The we need to allocate space for a picture, large enough...

  size_t dim = (size_t) _n;

  printf ("dim %zd\n",dim);

  char *pict = (char *) malloc(sizeof(char) * dim * dim);

  memset(pict,255,sizeof(char) * dim * dim);

  // get the counts from the _corr array

  int *count = (int *) malloc (sizeof(int) * _n);

  for (size_t i = 0; i < _n; i++)
    {
      int *c1 = _corr + DC_LINE_START(i);
      count[i] = *c1;
    }

  // What is the random chance of a coincidence?  If we take the
  // average number of counts in one channel, divided by the total
  // amount of events.  Squaring this gives the probability of a
  // random coincidence.  These we want to be fairly close to being
  // white.  Then the randoms are divided by sqrt(prob1*prob2), i.e.
  // we are back at the single-probability giving the scale.

  double scale = -1.0;

  if (_total_counts != 0.0 && _n && _events)
    {
      double prob_1 = (_total_counts / (double) _n) / _events;
      // double prob_2 = prob_1 * prob_1;

      // printf ("p1: %.3f  p2: %.3f  l(p2): %.3f\n",prob_1,prob_2,log(prob_2));

      scale = log(prob_1) * 1.5; // fudge factor :-)
    }

  // Then we need to dump the information onto the picture

  for (size_t i1 = 0; i1 < _n; i1++)
    {
      int *c1 = _corr + DC_LINE_START(i1);

      size_t y = i1;

      int cnt1 = count[i1];

      int *data = c1 + 1;

      if (cnt1)
	{
	  // now loop over the data in the chunk and eject it

	  for (size_t i2 = i1+1; i2 < _n; i2++)
	    {
	      size_t x = i2;

	      int corr = *(data++);
	      int cnt2 = count[i2];

	      double frac;

	      if (corr && /*cnt1 &&*/ cnt2)
		{
		  frac = ((double) corr) /
		    sqrt(((double) cnt1) *
			 ((double) cnt2));

		  // frac will always be <= 1.0, since corr < cnt1 and
		  // corr < cnt2

		  double fraclog = log(frac);

		  if (fraclog > scale)
		    {
		      pict[y * dim + x] = (char) ((fraclog / scale) * 254);
		    }
		}
	    }
	}
    }

  convert_picture(filename,pict,(int) dim,(int) dim);

  free(count);

  free(pict);
}

