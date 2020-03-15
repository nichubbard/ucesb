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

#ifndef __CORR_PLOT_DENSE_HH__
#define __CORR_PLOT_DENSE_HH__

#include "error.hh"

#include <math.h>
#include <stdlib.h>
#include <memory.h>

/**************************************************************/

class dense_corr
{
public:
  dense_corr()
  {
    _n = 0;
    _corr = NULL;
  }

  // We allocate memory in a triangular fashion.  This saves half the
  // memory, so we assume this pays for the extra calculations

  size_t  _n; // number of items

  size_t  _n2plus1;

  int *_corr;  // number of correlated hits (first item in each column is self count)

  int  _events;
  double _total_counts;

  // There are n lines in _corr.  The first one with n entries,
  // the last one with 1.  In total n*(n+1)/2 = (n+1)*n/2

  // line i starts at index 0, n, n-1, n-2 ..., i.e.
  // i * n - i*(i-1)/2 = i*(2*n-i+1)/2

#define DC_LINE_START(i)  ((i*(_n2plus1-i))/2)
#define DC_TOTAL_ITEMS(n) (((n+1)*n)/2)

public:
  void clear(size_t n)
  {
    if (_n != n)
      {
	int *np = (int *) realloc(_corr,
				  sizeof(int) * DC_TOTAL_ITEMS(n));

	if (!np)
	  ERROR("Memory allocation error.");

	_corr = np;

	_n = n;
	_n2plus1 = 2*n + 1;
      }

    memset (_corr,0,sizeof(int) * DC_TOTAL_ITEMS(_n));

    _events = 0;
    _total_counts = 0;
  }

public:
  void add(const int *start,const int *end)
  {
    _events++;
    _total_counts += (double) (end - start);

    // for each item in the list (which is to have been sorted!!!)

    for (const int *p1 = start; p1 < end; p1++)
      {
	int *line = _corr + DC_LINE_START((size_t) *p1);

	(*line)++;

	line -= *p1;

	for (const int *p2 = p1+1; p2 < end; p2++)
	  line[*p2]++;
      }
  }

public:
  void merge(const dense_corr &rhs)
  {
    if (_n != rhs._n)
      ERROR("Internal error, "
	    "cannot merge dense_corr with different n (%zd != %zd).",
	    _n,rhs._n);

    // merge the data from rhs with us

    int *c1 = _corr;
    int *c2 = rhs._corr;

    for (size_t i = DC_TOTAL_ITEMS(_n); i; --i)
      *(c1++) += *(c2++);
  }

public:
  void picture(const char *filename);

};

/**************************************************************/

#endif//__CORR_PLOT_DENSE_HH__
