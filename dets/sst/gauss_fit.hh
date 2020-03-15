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

#ifndef __GAUSS_FIT_HH__
#define __GAUSS_FIT_HH__

struct gf_pair
{
  double _x;
  double _y;
};



class gauss_function
{
public:
  struct param
  {
    double _center;
    double _peak;
    double _c;
  };

  static const int _n_param = 3;

public:
  static double calc_dist(const param &param,
			  const gf_pair *vect,
			  size_t n)
  {
    double sum = 0;

    for ( ; n; n--)
      {
	double y;
	double x = vect->_x;

	double xd = param._center - x;

	y = param._peak * exp(param._c * xd * xd);

	// y = log(param._peak) + param._c * xd * xd;

	double d = (log(vect->_y) - y);

	// printf ("%6.2f , %6.2f - %6.2f -> %6.2f\n",vect->_x,vect->_y,y,d);

	sum += (d * d) * vect->_y;

	vect++;
      }
    return sum;
  }

};

template<typename fcn_t>
class function_fit
{

public:
  function_fit()
  {
    _data = NULL;
    _n_alloc = 0;
  }

public:
  union dp_param
  {
    typename fcn_t::param _param;
    double                _dp[fcn_t::_n_param];
  };

  dp_param _cur;
  dp_param _step;

public:
  size_t   _n;
  size_t   _n_alloc;
  gf_pair *_data;
  gf_pair *_next;

public:
  void set_length(size_t alloc)
  {
    if (alloc > _n_alloc)
      {
	gf_pair *new_data = (gf_pair *) realloc(_data,alloc * sizeof(gf_pair));
	if (!new_data)
	  ERROR("memory allocation failure");
	_data = new_data;
      }
    _next = _data;
  }

  void set_value(double x,double y)
  {
    _next->_x = x;
    _next->_y = y;
    _next++;
  }

public:
  double distance(dp_param &param)
  {
    double dist = fcn_t::calc_dist(param._param,_data,_n);
    /*
    for (int i = 0; i < fcn_t::_n_param; i++)
      printf ("%7.2f ",param._dp[i]);

    printf ("-> %6.2f",dist);
    */
    return dist;
  }

public:
  bool fit()
  {
    _n = _next - _data;

    dp_param param;

    int iter = 0;

  fit_again:

    param = _cur;

    double y0;
    double ydx[fcn_t::_n_param];
    // double ydxdx[fcn_t::_n_param * (fcn_t::_n_param-1) / 2];

    double dydx[fcn_t::_n_param];
    double dy2dx2[fcn_t::_n_param];
    double dy2dxdx[fcn_t::_n_param * (fcn_t::_n_param-1) / 2];

    y0 = distance(param);

    //printf ("\n");

    int k = 0;

    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	param._dp[i] = _cur._dp[i] + _step._dp[i] * 1e-2;
	ydx[i] = distance(param);
	param._dp[i] = _cur._dp[i];
	//printf ("\n");
      }

    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	param._dp[i] = _cur._dp[i] - _step._dp[i] * 1e-2;
	double ydmx = distance(param);
	param._dp[i] = _cur._dp[i] + _step._dp[i] * 1e-2;

	double dydx_i = ydx[i] - y0;

	dydx[i]   = (dydx_i)/* * inv_step_i*/;
	dy2dx2[i] = (ydx[i] + ydmx - 2 * y0)/* * (inv_step_i * inv_step_i)*/;

	//printf (" d²ydx%d²  %8.1f dydx%d %8.1f\n",i,dy2dx2[i],i,dydx[i]);

	for (int j = i+1; j < fcn_t::_n_param; j++)
	  {
	    param._dp[j] = _cur._dp[j] + _step._dp[j] * 1e-2;
	    double ydxdx = distance(param);
	    param._dp[j] = _cur._dp[j];

	    dy2dxdx[k] = (ydxdx - ydx[j] - dydx_i)/* * inv_step_i / _step._dp[j]*/;

	    //printf (" d²ydx%dx%d %8.1f\n",i,j,dy2dxdx[k]);

	    k++;
	  }
	param._dp[i] = _cur._dp[i];
      }

    // Now, to calculate a Cholesky factorisation is overkill, as we
    // just want to solve an equation system.  On the other hand, it
    // does check that the thing is positive definite, and if not,
    // there is anyhow no sane solution (minima), and we should
    // complain (at least at the start approximation)

    double cholesky[(fcn_t::_n_param + 1) * fcn_t::_n_param / 2];

    // ordering of the matrix:
    //
    // 0 1 2
    //   3 4
    //     5

    int l = 0;
    k = 0;

    // I would not count on it, but the compiler should be smart
    // enough to try to unroll this loop.  It will then cook down to
    // 14 flops or so (for fcn_t::_n_param == 3)

    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	double diag = dy2dx2[i];

	//printf ("d:%8.1f",diag);

	// we are to subtract the squares of all elements above us

	double *src1 = &cholesky[i];

	{
	  double *src = src1;
	  int add = fcn_t::_n_param - 1;

	  for (int j = 0; j < i; j++)
	    {
	      double v = *src;
	      src += add;
	      add--;
	      diag -= v * v;

	      //printf ("-%8.1f",v * v);
	    }
	}

	if (diag <= 0.0)
	  return false;

	double chol_diag = sqrt(diag);

	cholesky[l++] = chol_diag;

	//printf (" = %8.1f --> %8.1f\n",diag,chol_diag);

	double inv_chol_diag = 1 / chol_diag;

	// and then calculate the rest of the

	double *src2 = src1 + 1;

	for (int j = i+1; j < fcn_t::_n_param; j++)
	  {
	    double elem = dy2dxdx[k++];

	    //printf ("(%8.1f ",elem);

	    {
	      double *src  = src1++;
	      double *srcb = src2++;
	      int add = fcn_t::_n_param - 1;

	      for (int kk = 0; kk < i; kk++)
		{
		  double v = *src;
		  double w = *srcb;
		  src  += add;
		  srcb += add;
		  add--;
		  elem -= v * w;

		  //printf ("-%8.1f*%8.1f",v,w);
		}
	    }

	    //printf ("=%8.1f)/%8.1f --> %8.1f\n",elem,chol_diag,elem * inv_chol_diag);

	    cholesky[l++] = elem * inv_chol_diag;
	  }
      }

    //printf ("------------------------------\n");

    l = 0;
    /*
    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	for (int j = 0; j < i; j++)
	  printf ("         ");

	printf ("%8.1f ",cholesky[l++]);

	for (int j = i+1; j < fcn_t::_n_param; j++)
	  printf ("%8.1f ",cholesky[l++]);

	printf ("\n");
      }
    */
    //printf ("\n------------------------------\n");

    // we need a temporary array for the partial solution

    double *rhs_src = &dydx[0];

    double tmp[fcn_t::_n_param];
    // double *tmp_dest = &tmp[fcn_t::_n_param-1];

    double *src1 = &cholesky[0];

    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	double elem = *(rhs_src++);

	//printf ("(%8.1f ",elem);

	double *src = src1++;
	int add = fcn_t::_n_param - 1;

	for (int j = 0; j < i; j++)
	  {
	    double v = *src;
	    src += add;
	    add--;
	    elem -= v * tmp[j];
	    //printf ("-%8.1f*%8.1f",v,tmp[j]);
	  }

	//printf ("=%8.1f)/%8.1f --> %8.1f\n",elem,*src,elem / *src);

	tmp[i] = elem / *src;
      }

    //printf ("------------------------------\n");
    /*
    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	printf ("%8.1f ",tmp[i]);
      }
    */
    //printf ("\n------------------------------\n");

    double sol[fcn_t::_n_param];
    double *sol_ptr = &sol[fcn_t::_n_param-1];

    double *cholesky_src = &cholesky[(fcn_t::_n_param + 1) * fcn_t::_n_param / 2 - 1];

    double *tmp_src = &tmp[fcn_t::_n_param-1];

    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	double elem = *(tmp_src--);
	//printf ("(%8.1f ",elem);

	double *sol_src = &sol[fcn_t::_n_param-1];

	for (int j = 0; j < i; j++)
	  {
	    //printf ("-%8.1f*%8.1f",*cholesky_src,*sol_src);
	    elem -= *(cholesky_src--) * *(sol_src--);
	  }

	//printf ("=%8.1f)/%8.1f --> %8.1f\n",elem,*cholesky_src,elem/(*cholesky_src));

	*(sol_ptr--) = elem / *(cholesky_src--);
      }

    //printf ("------------------------------\n");
    /*
    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	printf ("%8.1f ",sol[i]);
      }
    */
    //printf ("\n------------------------------\n");

    for (int i = 0; i < fcn_t::_n_param; i++)
      {
	_cur._dp[i] -= sol[i] * _step._dp[i] * 1e-2;
      }

    iter++;

    if (iter < 3)
      goto fit_again;

    return true;
  }




};

#endif//__GAUSS_FIT_HH__
