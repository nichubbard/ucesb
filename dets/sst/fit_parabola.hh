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

#ifndef __FIT_PARABOLA__
#define __FIT_PARABOLA__


//#include <math.h>
//#include <assert.h>

/** Used to fit a parabola function "y = y0 + c0*(x-x0)^2" (or "y = a + b*x + c*x^2" internally).
 *The values are put directly into the fit variables.
 *
 *  The fit is calculated with the least squares method, minimizing
 *  the squares of the errors in y, assuming the x values are exact.
 */

class fit_parabola
{
public:
  fit_parabola()
  {
    clear();
  }

public:
  void clear()
  {
    _n       = 0;
    _sum     = 0.0;
    _sum_x   = 0.0;
    _sum_y   = 0.0;
    _sum_x2  = 0.0;
    _sum_y2  = 0.0;
    _sum_xy  = 0.0;
    _sum_x3  = 0.0;
    _sum_x2y = 0.0;
    _sum_x4  = 0.0;

  }

public:
  int     _n;      /// Number of entries
  double _sum;    ///< Sum of 1 ...
  double _sum_x;  ///< Sum of x.
  double _sum_y;  ///< Sum of y.
  double _sum_x2; ///< Sum of x^2.
  double _sum_y2; ///< Sum of y^2.
  double _sum_xy; ///< Sum of x*y.
  double _sum_x3; ///< Sum of x^3
  double _sum_x2y; ///< Sum of x^2*y.
  double _sum_x4; ///< Sum of x^4.


public:
  /// Add counts from another collector of data.
  void add(const fit_parabola& src)
  {
    _n       += src._n;
    _sum     += src._sum;
    _sum_x   += src._sum_x;
    _sum_y   += src._sum_y;
    _sum_x2  += src._sum_x2;
    _sum_y2  += src._sum_y2;
    _sum_xy  += src._sum_xy;
    _sum_x3  += src._sum_x3;
    _sum_x2y += src._sum_x2y;
    _sum_x4  += src._sum_x4;
  }
  /// Add a count to the parabola fit.
  void fill( double x, ///< Count position.
	     double y  ///< Count value.
	    )
  {
    double x2 =x*x;
    double y2 =y*y;
    double xy =x*y;
    double x3 =x2*x;
    double x2y =x2*y;
    double x4 =x3*x;

    _n++;
    _sum++;
    _sum_x   += x;
    _sum_y   += y;
    _sum_x2  += x2;
    _sum_y2  += y2;
    _sum_xy  += xy;
    _sum_x3  += x3;
    _sum_x2y += x2y;
    _sum_x4  += x4;
  }

  /// Add a count to the parabola fit, with squared error e on the value y
  void fill( double x, ///< Count position.
	     double y, ///< Count value.
	    double e2 ///< Count squared error (sq. to avoid useless sqrt's).
	    )
  {
    double inv_e2 = 1.0/e2;
    double x2 =x*x;
    double y2 =y*y;
    double xy =x*y;
    double x3 =x2*x;
    double x2y =x2*y;
    double x4 =x3*x;

    _n++;
    _sum     += 1*inv_e2;
    _sum_x   += x*inv_e2;
    _sum_y   += y*inv_e2;
    _sum_x2  += x2*inv_e2;
    _sum_y2  += y2*inv_e2;
    _sum_xy  += xy*inv_e2;
    _sum_x3  += x3*inv_e2;
    _sum_x2y += x2y*inv_e2;
    _sum_x4  += x4*inv_e2;
  }

  /// Add a count to the parabola fit, with squared error e on the value y
  void fill_inv( double x,     ///< Count position.
		 double y,     ///< Count value.
		double inv_e2 ///< Count inverse squared error (sq. to avoid useless sqrt's).
		)
  {
    double x2 =x*x;
    double y2 =y*y;
    double xy =x*y;
    double x3 =x2*x;
    double x2y =x2*y;
    double x4 =x3*x;

    _n++;
    _sum     += 1*inv_e2;
    _sum_x   += x*inv_e2;
    _sum_y   += y*inv_e2;
    _sum_x2  += x2*inv_e2;
    _sum_y2  += y2*inv_e2;
    _sum_xy  += xy*inv_e2;
    _sum_x3  += x3*inv_e2;
    _sum_x2y += x2y*inv_e2;
    _sum_x4  += x4*inv_e2;
  }

public:

  /// Get the number of entry.
  double entry()  { return _n; }

public:
  // Internal functions  used to calculate the fit parameters for a + b*x + c*x^2.
  // do not panic, following expressions can be derived easily or take help from methematica.

  double div()  { return (_sum_x2*_sum_x2*_sum_x2  -  2.*_sum_x*_sum_x2*_sum_x3 +
                          _sum*_sum_x3*_sum_x3     +  _sum_x*_sum_x*_sum_x4     -
		          _sum*_sum_x2*_sum_x4); }

  double a()    { return (_sum_x2y*_sum_x2*_sum_x2 - _sum_x2y*_sum_x*_sum_x3 -
                          _sum_xy*_sum_x2*_sum_x3  + _sum_y*_sum_x3*_sum_x3  +
		          _sum_xy*_sum_x*_sum_x4   - _sum_y*_sum_x2*_sum_x4)/div(); }

  double da()   { return ((_sum_x2*_sum_x4         - _sum_x3*_sum_x3)/div()); }

  double b()    { return (-_sum_x2y*_sum_x*_sum_x2 + _sum_xy*_sum_x2*_sum_x2 +
                          _sum*_sum_x2y*_sum_x3    - _sum_y*_sum_x2*_sum_x3  -
		          _sum*_sum_xy*_sum_x4     + _sum_y*_sum_x*_sum_x4)/div(); }

  double db()   { return ((_sum*_sum_x4            - _sum_x2*_sum_x2)/div()); }

  double c()    { return (_sum_x2y*_sum_x*_sum_x   - _sum*_sum_x2y*_sum_x2   -
                          _sum_xy*_sum_x*_sum_x2   + _sum_y*_sum_x2*_sum_x2  +
		          _sum*_sum_xy*_sum_x3     - _sum_y*_sum_x*_sum_x3)/div(); }

  double dc()   { return ((_sum*_sum_x2            - _sum_x*_sum_x)/div()); }


  double chi2() { return (_sum*a()*a()                   + _sum_x*a()*b()*2.  +
                          _sum_x2*(b()*b() + 2.*a()*c()) + _sum_x3*b()*c()*2. +
			  _sum_x4*c()*c()                - _sum_y*a()*2.      -
			  _sum_xy*b()*2.                 -_sum_x2y*c()*2.     +
			  _sum_y2); }

//public:
//  double get_div() { return div(); } // this is used to see if dydx will be successful

public:

  /// Get the value of x0 and uncertainty dx0.
  double x0()   { return -b()/(2.*c()); }
  double dx0()  { return sqrt(db()*db()/(4.*c()*c()) +
                              b()*b()*dc()*dc()/(4.*c()*c()*c()*c())); }

  /// Get the value of y0 and uncertainty dy0.
  double y0()   { return (-b()*b()/(4.*c()) + a()); }
  double dy0()  { return sqrt(da()*da()                       +
                               b()*b()*db()*db()/(4.*c()*c()) +
                               b()*b()*b()*b()*dc()*dc()/(16.*c()*c()*c()*c())); }

   /// Get the value of c0 and uncertainty dc0.
  double c0()   { return c(); }
  double dc0()  { return dc(); }

  /// chi-sqr per degree of freedom
  double rchi2(){ return chi2()/(_n-3); }


};

#endif// __FIT_PARABOLA__
