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

#include <stdio.h>

#include "test_rxs64s.h"

typedef struct caen_v1290_data_t
{
  uint32_t eventno;
  uint32_t trigtime;
  uint32_t mask;
  uint32_t num[32];
  uint32_t ch[32][16];
} caen_v1290_data;

void create_caen_v1290_event(caen_v1290_data *data,
			     uint32_t geom,
			     uint32_t eventno, uint32_t seed)
{
  data->eventno = eventno & 0x003fffff;

  uint64_t rstate =
    (((uint64_t) data->eventno) << 32) | ((geom << 8) ^ seed);

  uint64_t a = rxs64s(&rstate);
  int n, i;
  uint32_t j;

  // We most often want few channels, so we bias towards that

  int nmax = 1 << ((a & 0xff) % 6); // (1 << 0) .. (1 << 5) == 1 .. 32

  n = ((int) (a >> 8)) % (nmax + 1);

  data->mask = 0;

  for (i = 0; i < n; i++)
    {
      a = rxs64s(&rstate);

      int ch = a & 0x1f;

      while (data->mask & (((uint32_t) 1) << ch))
	ch = (ch + 1) & 0x1f;

      data->mask |= ((uint32_t) 1) << ch;

      /* number of hits, also biased towards few */

      uint32_t nummax =
	((uint32_t) 1) << (((a >> 8) & 0xff) % 5); // (1 << 0) .. (1 << 4) == 1 .. 16

      data->num[ch] = 1 + (uint32_t) ((a >> 16) % (nummax));

      for (j = 0; j < data->num[ch]; j++)
	{
	  a = rxs64s(&rstate);

	  // fprintf(stderr,"%d,%d[%d,%d]\n",ch,j,nmax,data->num[ch]);

	  data->ch[ch][j] = (uint32_t) (a & 0x1fffff);
	}
    }

  data->trigtime = 0x07ffffff & rxs64s(&rstate);

  // fprintf (stderr,"eventno: %d\n", data->eventno);
  // fprintf (stderr,"trigt: %d\n", data->trigtime);
}

uint32_t *create_caen_v1290_data(uint32_t *dest,
				 const caen_v1290_data *data,
				 uint32_t geom)
{
  uint32_t words = 0;
  int ch;
  uint32_t j;

  //if (!data->mask) // let's keep as we check the eventno
  //  return dest;

  // header:
  *(dest++) = geom | (data->eventno << 5) | (0x08 << 27);

  for (ch = 0; ch < 32; ch++)
    if (data->mask & (((uint32_t) 1) << ch))
      for (j = 0; j < data->num[ch]; j++)
	{
	  uint32_t value = data->ch[ch][j];
	  // data:
	  *(dest++) =
	    (((uint32_t) ch) << 21) | (value);
	  words++;
	}

  // trigger:
  *(dest++) = (((uint32_t) 0x11) << 27) | data->trigtime;

  words += 3; // header, trigger, trailer

  // trailer
  *(dest++) =
    geom | (words << 5) | (0 << 24) | (0 << 25) | (0 << 26) |
    (((uint32_t) 0x10) << 27);

  return dest;
}

int fill_caen_v1290_data(caen_v1290_data *data,
			 uint32_t M, uint32_t *MI, uint32_t *ME,
			 uint32_t n, uint32_t *v,
			 uint32_t header, uint32_t trigger)
{
  uint32_t i, j;
  uint32_t start;

  data->mask = 0;
  data->eventno = (header >> 5) & 0x003fffff;
  data->trigtime = trigger & 0x07ffffff;

  if (M > 32)
    {
      printf ("%s: M(=%d) > 32\n",__func__,n);
      M = 32;
      return 0;
    }

  if (n > 32 * 16)
    {
      printf ("%s: n(=%d) > 32 * 16\n",__func__,n);
      n = 32 * 16;
      return 0;
    }

  start = 0;

  for (i = 0; i < M && i < 32; i++)
    {
      uint32_t ch = MI[i]-1;
      uint32_t end = ME[i];
      uint32_t num = end - start;

      if (num > 16)
	{
	  printf("%s: num(=%d) > 16 (from ME[i=%d](=%d)-prevME(=%d))\n",
		 __func__,num,ch,end,start);
	  num = 16;
	  return 0;
	}

      if (ch >= 32)
	{
	  printf ("%s: MI[%d] -1 (=%d) >= 32\n",__func__,i,ch);
	  return 0;
	}
      else if (end > n)
	{
	  printf ("%s: ME[i=%d](=%d) > n(=%d)\n",__func__,i,end,n);
	  return 0;
	}
      else
	{
	  data->mask |= ((uint32_t) 1) << ch;
	  data->num[ch] = num;
	  for (j = 0; j < num; j++)
	    {
	      data->ch[ch][j] = v[start+j];
	    }
	}

      start = end;
    }

  if (start != n)
    {
      printf ("%s: ME[last](=%d) != n(=%d)\n",__func__,start,n);
      return 0;
    }
  return 1;
}

int compare_caen_v1290_event(const caen_v1290_data *good,
			     const caen_v1290_data *cmp)
{
  int ok = 1;
  int ch;
  uint32_t j;

  if ((good->eventno ^ cmp->eventno) & 0x003fffff)
    {
      printf ("%s: eventno mismatch %d != %d\n",
	      __func__,good->eventno,cmp->eventno);
      ok = 0;
    }
  if (good->trigtime != cmp->trigtime)
    {
      printf ("%s: trigtime mismatch %d != %d\n",
	      __func__,good->trigtime,cmp->trigtime);
      ok = 0;
    }
  if (good->mask != cmp->mask)
    {
      printf ("%s: mask mismatch %08x != %08x\n",
	      __func__,good->mask,cmp->mask);
      ok = 0;
    }

  for (ch = 0; ch < 32; ch++)
    if (good->mask & cmp->mask & (((uint32_t) 1) << ch))
      {
	if (good->num[ch] != cmp->num[ch])
	  {
	    printf ("%s: num[%d] mismatch %d != %d\n",
		    __func__,ch,good->num[ch],cmp->num[ch]);
	    ok = 0;
	  }
	for (j = 0; j < good->num[ch] && j < cmp->num[ch]; j++)
	  {
	    if (good->ch[ch][j] != cmp->ch[ch][j])
	      {
		printf ("%s: data[%d][%d] mismatch %d != %d\n",
			__func__,ch,j,good->ch[ch][j],cmp->ch[ch][j]);
		ok = 0;
	      }
	  }
      }
  return ok;
}
