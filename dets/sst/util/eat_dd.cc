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

// gcc -lm -O3 -o eat_dd dets/sst/util/eat_dd.cc

#include <stdio.h>
#include <float.h>
#include <math.h>

#include <vector>
#include <algorithm>

struct diffs
{
  std::vector<float> vals;
};

struct peakd
{
  float strip1, peak1, c1;
  float strip2, peak2, c2;
};

struct peaks
{
  std::vector<peakd> vals;
};

struct peakhd
{
  float strip1, peak1;
  float strip2, peak2;
};

struct peakhs
{
  std::vector<peakhd> vals;
};

diffs *dd[4][1024][10];

peaks *dp[4][640/8][384/8];

peakhs *dhp[4][640/8][384/8];

#define MAXCNT 100000000

bool find_mean_var(std::vector<float>& data,double *mean,double *mean_err_var,double *var)
{
  std::vector<float> sorted = data;

  sort(sorted.begin(),sorted.end());

  int n = sorted.size();

  // Try to find the steepest (smallest) difference with 1/2 of the data inbetween

  double prel_mean;
  double prel_sigma;
  double min_diff = DBL_MAX;

  for (int start = 0; start < n/2; start += n/16)
    {
      double first = sorted[start];
      double last  = sorted[start+n/2];

      double diff = last - first;

      if (diff < min_diff)
	{
	  prel_mean = 0.5 * (first + last);
	  prel_sigma = diff / 0.5 / sqrt(2 * M_PI);

	  min_diff = diff;
	}
    }

  if (min_diff > 0.025)
    {
      int min_k, max_k;

      double sum = 0;
      double sum_x = 0;
      double sum_x2 = 0;

      if (prel_sigma < 0.1)
	prel_sigma = 0.1;

      {
	double min_accept = prel_mean - prel_sigma * 4;
	double max_accept = prel_mean + prel_sigma * 4;
	/*
	printf ("prel %6.1f +/- %6.1f -> ( %6.1f .. %6.1f )\n",
		prel_mean,prel_sigma,min_accept,max_accept);
	*/
	// First loop until we find an acceptable value

	for (min_k = 0; min_k < n; min_k++)
	  if (sorted[min_k] >= min_accept)
	    break;

	for (max_k = min_k; max_k < n; max_k++)
	  {
	    if (sorted[max_k] > max_accept)
	      break;

	    sum_x  += sorted[max_k];
	    sum_x2 += sorted[max_k] * sorted[max_k];
	  }
      }

      sum = max_k - min_k;

      if ((max_k - min_k) < 0.8 * n)
	{
	  printf ("CMS_TOO_MUCH_NOISE (%d,%d)/%d\n",min_k,max_k,n);
	  return false;
	}

      *mean = sum_x / sum;
      *var  = (sum_x2 - sum_x*sum_x / sum)/(sum-1);

      *mean_err_var = *var / sum;

      return true;
    }

  return false;
}

template<typename T>
void swap(T &a,T &b)
{
  T tmp;

  tmp = a;
  a = b;
  b = tmp;
}


void plot_diffs(std::vector<float> vals)
{
  int sum[63];

  memset(sum,0,sizeof(sum));

  for (int i = 0; i < vals.size(); i++)
    {
      double val = vals[i];
      int index;

      if (val < -3)
	index = 0;
      else if (val > 3)
	index = 62;
      else
	index = (int) ((val + 0.1) / 0.2) + 30;

      // printf ("%6.2f %d\n",val,index);

      sum[index]++;
    }

  int maxsum = 0;

  for (int i = 0; i < 63; i++)
    if (sum[i] > maxsum)
      maxsum = sum[i];

  for (int y = 8; y > 0; y--)
    {
      for (int i = 0; i < 63; i++)
	printf (sum[i]*8 > maxsum*y ? "#" : " "/*,sum[i]*8,maxsum*y*/);

      printf("\n");
    }
  printf ("|---------|---------|---------|---------|---------|---------|\n");
}

int main()
{
  memset(dd,0,sizeof(dd));
  int cnt1 = 0, cnt2 = 0, cnt3 = 0;
  char line[1024];

  while (!feof(stdin) && cnt1 + cnt2 < MAXCNT)
    {
      fgets(line,sizeof(line),stdin);

      if (strncmp(line,"DD ",3) == 0)
	{
	  int det, i1, i2;
	  double diff;

	  sscanf (line,"DD %d %d %d %lf\n",&det,&i1,&i2,&diff);

	  if (det >= 0 && det <= 3 &&
	      i1 >= 0 && i2 < 1024 &&
	      i2 > i1)
	    {
	      if (i2 - i1 < 10)
		{
		  diffs *d = dd[det][i1][i2-i1];

		  if (!d)
		    d = dd[det][i1][i2-i1] = new diffs;

		  d->vals.push_back(diff);
		  cnt1++;
		}
	    }
	  else
	    fprintf (stderr,"refusing: %d %d %d %f\n",det,i1,i2,diff);
	}
      else if (strncmp(line,"DF ",3) == 0)
	{
	  int det;
	  peakd v;
	  int n;

	  n = sscanf (line,"DF %d : %f %f %f : %f %f %f\n",
		      &det,
		      &v.strip1,&v.peak1,&v.c1,
		      &v.strip2,&v.peak2,&v.c2);

	  if (v.strip1 > 640 && v.strip2 < 640)
	    {
	      swap(v.strip1,v.strip2);
	      swap(v.peak1,v.peak2);
	      swap(v.c1,v.c2);
	    }

	  v.strip2 -= 640;

	  if (n == 7 &&
	      v.strip1 > 0 && v.strip1 < 640 &&
	      v.strip2 > 0 && v.strip2 < 384 &&
	      v.c1 < -0.05 && v.c2 < -0.05)
	    {
	      int i1 = (int) (v.strip1 / 8);
	      int i2 = (int) (v.strip2 / 8);

	      peaks *p = dp[det][i1][i2];

	      if (!p)
		p = dp[det][i1][i2] = new peaks;

	      p->vals.push_back(v);
	      cnt2++;
	    }
	  //else
	  //fprintf (stderr,"refusing: %d %f %f\n",det,v.strip1,v.strip2);
	}
       else if (strncmp(line,"DH ",3) == 0)
	{
	  int det;
	  peakhd v[4];
	  int n;

	  n = sscanf (line,"DH %d : %f %f : %f %f : %f %f : %f %f\n",
		      &det,
		      &v[0].strip1,&v[0].peak1,
		      &v[1].strip1,&v[1].peak1,
		      &v[2].strip1,&v[2].peak1,
		      &v[3].strip1,&v[3].peak1);

	  if (n == 5 || n == 7)
	    {
	      n = (n - 1) / 2;
	      peakhd vx[4];
	      peakhd vy[4];
	      int nx = 0;
	      int ny = 0;

	      for (int i = 0; i < 4; i++)
		{
		  if (v[i].strip1 > 0 &&
		      v[i].strip1 < 640)
		    {
		      vx[nx++] = v[i];
		    }
		  else if (v[i].strip1 > 640+0 &&
			   v[i].strip1 < 640+384)
		    {
		      v[i].strip1 -= 640;
		      vy[ny++] = v[i];
		    }
		}

	      for (int i = 0; i < nx; i++)
		for (int j = 0; j < ny; j++)
		  {
		    peakhd vv;

		    vv.strip1 = vx[i].strip1;
		    vv.peak1  = vx[i].peak1;
		    vv.strip2 = vy[j].strip1;
		    vv.peak2  = vy[j].peak1;

		    int i1 = (int) (vv.strip1 / 8);
		    int i2 = (int) (vv.strip2 / 8);

		    peakhs *p = dhp[det][i1][i2];

		    if (!p)
		      p = dhp[det][i1][i2] = new peakhs;

		    p->vals.push_back(vv);
		    cnt3++;
		  }
	      /*
	      &&
	      v.strip1 > 0 && v.strip1 < 640 &&
	      v.strip2 > 0 && v.strip2 < 384)
	    {
	      int i1 = (int) (v.strip1 / 8);
	      int i2 = (int) (v.strip2 / 8);

	      peaks *p = dp[det][i1][i2];

	      if (!p)
		p = dp[det][i1][i2] = new peaks;

	      p->vals.push_back(v);
	      cnt2++;
	      */
	    }
	  //else
	  //fprintf (stderr,"refusing: %d %f %f\n",det,v.strip1,v.strip2);
	}
     else
	fprintf (stderr,"unrecognized: %s",line);

      if (((cnt1 + cnt2 + cnt3) & 0xffff) == 0)
	{
	  fprintf (stderr,"%d %d %d / %d\r",cnt1,cnt2,cnt3,MAXCNT);
	  fflush(stderr);
	}
    }

  for (int det = 0; det < 4; det++)
    {
      for (int i1 = 0; i1 < 640/8; i1++)
	for (int i2 = 0; i2 < 384/8; i2++)
	  {
	    peakhs *p = dhp[det][i1][i2];

	    if (!p || p->vals.size() < 50)
	      {
		if (p)
		  printf ("--- %d / %4d - %4d / %4d - %4d --- %d -- \n",
			  det,i1*8,i1*8+8,i2*8,i2*8+8,p->vals.size());

		continue;
	      }

	    printf ("--- %d / %4d - %4d / %4d - %4d ---------------------------- %d -- \n",
		    det,i1*8,i1*8+8,i2*8,i2*8+8,p->vals.size());

	    std::vector<float> peakdiffs;
	    //std::vector<float> peakdiffs_d;

	    peakdiffs.resize(p->vals.size());
	    //peakdiffs_d.resize(p->vals.size());

	    for (int i = 0; i < p->vals.size(); i++)
	      {
		const peakhd &pd = p->vals[i];

		peakdiffs[i] = pd.peak2 - pd.peak1;

		//double d1 = log(sqrt(-pd.c1));
		//double d2 = log(sqrt(-pd.c2));

		//peakdiffs_d[i] = (pd.peak2 + d2) - (pd.peak1 + d1);
	      }

	    plot_diffs(peakdiffs);

	    //plot_diffs(peakdiffs_d);

	    ////////////////////////////////////////////////////////////////////////
	    // find mean, variance etc...

	    double mean, mean_err_var, var;

	    if (find_mean_var(peakdiffs,&mean,&mean_err_var,&var))
	      {
		printf ("-- mean %6.2f +/- %6.2f var %6.2f (%6.2f)\n",mean,sqrt(mean_err_var),var,sqrt(var));
		// printf ("DI %d %4d %4d %7.4f %7.4f %7.4f\n",det,i1,i2,mean,sqrt(mean_err_var),sqrt(var));
	      }
	    /*
	    if (find_mean_var(peakdiffs_d,&mean,&mean_err_var,&var))
	      {
		printf ("-d mean %6.2f +/- %6.2f var %6.2f (%6.2f)\n",mean,sqrt(mean_err_var),var,sqrt(var));
		// printf ("DE %d %d %d %7.4f %7.4f %7.4f\n",det,i1,i2,mean,sqrt(mean_err_var),sqrt(var));
	      }
	    */
	  }

      for (int i1 = 0; i1 < 640/8; i1++)
	for (int i2 = 0; i2 < 384/8; i2++)
	  {
	    peaks *p = dp[det][i1][i2];

	    if (!p || p->vals.size() < 50)
	      continue;

	    printf ("--- %d / %4d - %4d / %4d - %4d ---------------------------- %d -- \n",
		    det,i1*8,i1*8+8,i2*8,i2*8+8,p->vals.size());

	    std::vector<float> peakdiffs;
	    std::vector<float> peakdiffs_d;

	    peakdiffs.resize(p->vals.size());
	    peakdiffs_d.resize(p->vals.size());

	    for (int i = 0; i < p->vals.size(); i++)
	      {
		const peakd &pd = p->vals[i];

		peakdiffs[i] = pd.peak2 - pd.peak1;

		double d1 = log(1/sqrt(-pd.c1));
		double d2 = log(1/sqrt(-pd.c2));

		peakdiffs_d[i] = (pd.peak2 + d2) - (pd.peak1 + d1);
	      }

	    plot_diffs(peakdiffs);

	    plot_diffs(peakdiffs_d);

	    ////////////////////////////////////////////////////////////////////////
	    // find mean, variance etc...

	    double mean, mean_err_var, var;

	    if (find_mean_var(peakdiffs,&mean,&mean_err_var,&var))
	      {
		printf ("-- mean %6.2f +/- %6.2f var %6.2f (%6.2f)\n",mean,sqrt(mean_err_var),var,sqrt(var));
		//printf ("DG %d %4d %4d %7.4f %7.4f %7.4f\n",det,i1,i2,mean,sqrt(mean_err_var),sqrt(var));
	      }

	    if (find_mean_var(peakdiffs_d,&mean,&mean_err_var,&var))
	      {
		printf ("-d mean %6.2f +/- %6.2f var %6.2f (%6.2f)\n",mean,sqrt(mean_err_var),var,sqrt(var));
		printf ("DG %d %4d %4d %7.4f %7.4f %7.4f\n",det,i1,i2,mean,sqrt(mean_err_var),sqrt(var));
	      }
	  }

      for (int i1 = 0; i1 < 1024; i1++)
	for (int i2 = i1+1; i2 < i1+10; i2++)
	  {
	    diffs *d = dd[det][i1][i2-i1];

	    if (!d || d->vals.size() < 50)
	      continue;

	    printf ("--- %d / %4d - %4d --------------------------------- %d -- \n",det,i1,i2,d->vals.size());

	    plot_diffs(d->vals);

	    ////////////////////////////////////////////////////////////////////////
	    // find mean, variance etc...

	    double mean, mean_err_var, var;

	    if (find_mean_var(d->vals,&mean,&mean_err_var,&var))
	      {
		printf ("mean %6.2f +/- %6.2f var %6.2f (%6.2f)\n",mean,sqrt(mean_err_var),var,sqrt(var));

		printf ("DE %d %4d %4d %7.4f %7.4f %7.4f\n",det,i1,i2,mean,sqrt(mean_err_var),sqrt(var));
	      }
	  }
    }
}
