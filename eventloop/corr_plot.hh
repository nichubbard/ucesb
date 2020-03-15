/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
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

#ifndef __CORR_PLOT_HH__
#define __CORR_PLOT_HH__

#include <math.h>

/**************************************************************/

template<int buckets,int chunk>
class pcorr
{
public:
  pcorr()
  {
    _ptr = NULL;
    clear();
  }

public:
  int  *_ptr;
  int   _alloc;

  short _ptr_offset[buckets];

public:
  void clear()
  {
    free(_ptr);
    _ptr = NULL;
    _alloc = 0;

    for (int b = 0; b < buckets; b++)
      _ptr_offset[b] = -1;
  }

  void alloc_bucket(int bucket)
  {
    // This bucket is not allocated yet...

    // We'll need to reallocate

    int *np = (int*) realloc(_ptr,sizeof(int) * chunk * (_alloc+1));

    if (!np)
      ERROR("Memory allocation failure in corr.");

    _ptr = np;

    _ptr_offset[bucket] = _alloc;

    memset(_ptr + _alloc * chunk,0,sizeof(int) * chunk);

    _alloc++;
  }

  void add(int start)
  {
    int bucket = start / chunk;

    if (_ptr_offset[bucket] == -1)
      alloc_bucket(bucket);

    int *p = _ptr + ((int) _ptr_offset[bucket]) * chunk;

    int offset = start - bucket * chunk;
    p += offset;

    (*p)++;
  }

  void add2(int bucket,int offset)
  {
    if (_ptr_offset[bucket] == -1)
      alloc_bucket(bucket);

    int *p = _ptr + ((int) _ptr_offset[bucket]) * chunk;

    p += offset;

    (*p)++;
  }

  void add(int start,int end)
  {
    while (start < end)
      {
	int bucket = start / chunk;

	if (_ptr_offset[bucket] == -1)
	  alloc_bucket(bucket);

	int *p = _ptr + ((int) _ptr_offset[bucket]) * chunk;

	int offset = start - bucket * chunk;
	p += offset;

	while (start < end && offset < chunk)
	  {
	    (*p)++;
	    p++;
	    offset++;
	    start++;
	  }
      }
  }

public:
  void merge(const pcorr &rhs)
  {
    // Loop over the buckets of rhs, and add any content to ours

    for (int b = 0; b < buckets; b++)
      if (rhs._ptr_offset[b] != -1)
	{
	  // Make sure we have the bucket

	  if (_ptr_offset[b] == -1)
	    alloc_bucket(b);

	  int *p_src  = rhs._ptr + ((int) rhs._ptr_offset[b]) * chunk;
	  int *p_dest =     _ptr + ((int)     _ptr_offset[b]) * chunk;

	  for (int c = chunk; c; --c)
	    *(p_dest++) = *(p_src++);
	}
  }

};

template<int buckets,int chunk>
class ppcorr
{
public:



public:
  int                  _count[buckets*chunk];
  pcorr<buckets,chunk> _corr[buckets*chunk];

public:
  void clear()
  {
    for (int i = 0; i < buckets*chunk; i++)
      _count[i] = 0;
    for (int i = 0; i < buckets*chunk; i++)
      _corr[i].clear();
  }

public:
  void add_self(int i1)
  {
    _count[i1]++;
  }

  void add(int i1,int i2)
  {
    _corr[i1].add(i2);
  }

  void add2(int i1,/*int i1_b,int i1_o,*/int i2_b,int i2_o)
  {
    // i1 = i1_b * chunk + i1_o
    _corr[i1].add(i2_b,i2_o);
  }

  void add_range_self(int start1,int end1)
  {
    for (int i1 = start1; i1 < end1-1; i1++)
      {
	_count[i1]++;
	_corr[i1].add(i1+1,end1);
      }
    _count[end1-1]++;
  }

  void add_range(int start1,int end1,
		 int start2,int end2)
  {
    // start2 >= end1

    for (int i1 = start1; i1 < end1; i1++)
      _corr[i1].add(start2,end2);
  }

public:
  void merge(const ppcorr &rhs)
  {
    // merge the data from rhs with us

    for (int i = 0; i < buckets*chunk; i++)
      _count[i] += rhs._count[i];

    for (int i = 0; i < buckets*chunk; i++)
      _corr[i].merge(rhs._corr[i]);
  }

public:
  void picture(const char *filename)
  {
    /*
    for (int i = 0; i < buckets*chunk; i++)
      {
	printf ("w %d %4d : %d\n",m,i,mwpc_count[m][i]);
      }
    */
    // before we make the picture of the correlations, we need to
    // which buckets are in use at all

    int used[buckets];

    memset(used,0,sizeof(used));

    for (int i = 0; i < buckets*chunk; i++)
      {
	for (int b = 0; b < buckets; b++)
	  if (_corr[i]._ptr_offset[b] != -1)
	    {
	      used[b]++;
	      used[i / chunk]++;
	    }
      }

    int num_used = 0;

    for (int b = 0; b < buckets; b++)
      {
	if (used[b])
	  num_used++;

	//printf ("b %d %3d : %d\n",m,b,used[b]);
      }

    // The we need to allocate space for a picture, large enough...

    int dim = num_used * chunk + buckets+1;

    int off[buckets];

    int next_off = 1;

    for (int b = 0; b < buckets; b++)
      {
	if (used[b])
	  {
	    off[b] = next_off;
	    next_off += chunk;
	  }
	else
	  off[b] = 0;

	next_off++;
      }

    printf ("dim %d  next %d\n",dim,next_off);

    char *pict = (char *) malloc(sizeof(char) * dim * dim);

    memset(pict,255,sizeof(char) * dim * dim);

    // Then we need to dump the information onto the picture

    for (int b1 = 0; b1 < buckets; b1++)
      if (off[b1])
	{
	  fprintf(stderr,"%s %d , %d/%d        \r",
		  filename,off[b1],dim,
		  buckets * chunk + buckets+1);
	  fflush(stderr);

	  for (int c1 = 0; c1 < chunk; c1++)
	    {
	      int y = off[b1] + c1;

	      pcorr<buckets,
		chunk> *corr = &_corr[b1*chunk+c1];

	      int cnt1 = _count[b1*chunk+c1];

	      for (int b2 = 0; b2 < buckets; b2++)
		{
		  if (corr->_ptr_offset[b2] != -1)
		    {
		      int *data = corr->_ptr + corr->_ptr_offset[b2] * chunk;

		      // now loop over the data in the chunk and eject it

		      for (int c2 = 0; c2 < chunk; c2++)
			{
			  int x = off[b2] + c2;

			  int corr = data[c2];
			  int cnt2 = _count[b2*chunk+c2];

			  double frac;

			  /*
			    if (corr > cnt1 ||
			    corr > cnt2)
			    printf("correlation larger than count... (%d %d %d))\n",corr,cnt1,cnt2);
			  */

			  if (corr && cnt1 && cnt2)
			    {
			      frac = ((double) corr) /
				sqrt(((double) cnt1) *
				     ((double) cnt2));

				// frac will always be <= 1.0, since corr < cnt1 and corr < cnt2
				/*
				printf ("%d (%3d %2d) (%3d %2d) f %10.6f  %6.2f  (%5d / %5d , %5d)\n",
					m,b1,c1,b2,c2,
					frac,
					log(frac),
					corr,cnt1,cnt2);
				*/
				double fraclog = log(frac);

				if (fraclog > -7.0)
				  {
				    pict[y * dim + x] = (char) ((-fraclog / 7) * 250);
				  }
			    }
			}
		    }
		}
	    }
	}

    FILE *fid = fopen(filename,"wb");

    fprintf (fid,"P5\n");
    fprintf (fid,"%d %d\n",dim,dim);
    fprintf (fid,"255\n");

    fwrite(pict,sizeof(char),dim*dim,fid);

    fclose(fid);

    // Then dump the picture (as pgm - highly inefficient, but
    // gets the job done...)


    //

    free(pict);
  }


};

/**************************************************************/

#endif//__CORR_PLOT_HH__
