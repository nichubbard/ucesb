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

#ifndef __CORR_PLOT_DENSE2_HH__
#define __CORR_PLOT_DENSE2_HH__

#include "error.hh"

#include <math.h>
#include <stdlib.h>
#include <memory.h>

/**************************************************************/

class dense_corr2
{
public:
  dense_corr2()
  {
    _n = 0;
    _corr = NULL;
  }

  // We allocate memory in a triangular fashion.  This saves half the
  // memory, so we assume this pays for the extra calculations

  size_t  _n; // number of items

  size_t  _total;

  int *_np;
  int *_corr;  // number of correlated hits (first item in each column is self count)
  int *_sum1;
  int *_sum2;

  int  _events;
  double _total_counts;

  // There are n lines in _corr.
  // All have 2+n entries.  The first two tell the number of items.

public:
  void clear(size_t n)
  {
    if (_n != n)
      {
	size_t total = 2*n + n*n;
	int *np = (int *) realloc(_corr,
				  sizeof(int) * total);

	if (!np)
	  ERROR("Memory allocation error.");

	_np   = np;
	_sum1 = _np;
	_sum2 = _sum1 + n;
	_corr = _sum2 + n;

	_n = n;
	_total = total;
      }

    memset (_np,0,sizeof(int) * _total);

    _events = 0;
    _total_counts = 0;
  }

public:
  void add_lists_two(const int *start1,const int *end1,
		     const int *start2,const int *end2)
  {
    _events++;
    _total_counts += (double) (end1 - start1);

    /* printf ("%zd %zd\n", end1-start1, end2-start2); */

    // for each item in the list (which is to have been sorted!!!)

    for (const int *p1 = start1; p1 < end1; p1++)
      _sum1[*p1]++;

    for (const int *p2 = start2; p2 < end2; p2++)
      _sum2[*p2]++;

    for (const int *p1 = start1; p1 < end1; p1++)
      {
	int *line = _corr + ((size_t) *p1) * _n;

	for (const int *p2 = start2; p2 < end2; p2++)
	  line[*p2]++;
      }
  }

public:
  void merge(const dense_corr2 &rhs)
  {
    if (_n != rhs._n)
      ERROR("Internal error, "
	    "cannot merge dense_corr with different n (%zd != %zd).",
	    _n,rhs._n);

    // merge the data from rhs with us

    int *c1 = _np;
    int *c2 = rhs._np;

    for (size_t i = _total; i; --i)
      *(c1++) += *(c2++);
  }

public:
  void picture(const char *filename);

};

/**************************************************************/

#endif//__CORR_PLOT_DENSE2_HH__
