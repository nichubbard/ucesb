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

typedef struct caen_v775_data_t
{
  uint32_t eventno;
  uint32_t mask;
  uint32_t ch[32];
} caen_v775_data;

void create_caen_v775_event(caen_v775_data *data,
			    uint32_t geom, uint32_t crate,
			    uint32_t eventno, uint32_t seed)
{
  data->eventno = eventno & 0x00ffffff;

  //fprintf (stderr,"%d %d %d %d\n", geom, crate, eventno, seed);

  uint64_t rstate =
    (((uint64_t) data->eventno) << 32) | (((geom << 8) | crate) ^ seed);

  uint64_t a = rxs64s(&rstate);
  int n, i;

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

      data->ch[ch] = (a >> 5) & 0xfff;
    }
}

uint32_t *create_caen_v775_data(uint32_t *dest,
				const caen_v775_data *data,
				uint32_t geom, uint32_t crate)
{
  uint32_t *header = dest++;
  uint32_t n = 0;
  int ch;

  //if (!data->mask) // let's keep as we check the eventno
  //  return dest;

  for (ch = 0; ch < 32; ch++)
    if (data->mask & (((uint32_t) 1) << ch))
      {
	uint32_t value = data->ch[ch];
	*(dest++) =
	  (geom << 27) | (0 << 24) | (((uint32_t) ch) << 16) | (value);
	//fprintf (stderr,"D: %08x (%08x %08x %08x %08x)\n", *(dest-1),
	//	 (geom << 27), (0 << 24), (ch << 16), (value));
	n++;
      }

  *header = (geom << 27) | (2 << 24) | (crate << 16) | (n << 8);
  //fprintf (stderr,"H: %08x\n", *header);

  // eob:
  *(dest++) = (geom << 27) | (4 << 24) | (data->eventno);
  //fprintf (stderr,"E: %08x\n", *(dest-1));

  return dest;
}

int fill_caen_v775_data(caen_v775_data *data,
			uint32_t n, uint32_t *I, uint32_t *v, uint32_t eob)
{
  uint32_t i;

  data->mask = 0;
  data->eventno = eob & 0x00ffffff;

  if (n > 32)
    {
      printf ("%s: n(=%d) > 32\n",__func__,n);
      n = 32;
      return 0;
    }

  for (i = 0; i < n && i < 32; i++)
    {
      uint32_t ch = I[i]-1;

      if (ch >= 32)
	{
	  printf ("%s: I[%d] -1 (=%d) >= 32\n",__func__,i,ch);
	  return 0;
	}
      else
	{
	  data->mask |= ((uint32_t) 1) << ch;
	  data->ch[ch] = v[i];
	}
    }
  return 1;
}

int compare_caen_v775_event(const caen_v775_data *good,
			    const caen_v775_data *cmp)
{
  int ok = 1;
  int ch;

  if ((good->eventno ^ cmp->eventno) & 0x00ffffff)
    {
      printf ("%s: eventno mismatch %d != %d\n",
	      __func__,good->eventno,cmp->eventno);
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
      if (good->ch[ch] != cmp->ch[ch])
	{
	  printf ("%s: data[%d] mismatch %d != %d\n",
		  __func__,ch,good->ch[ch],cmp->ch[ch]);
	  ok = 0;
	}
  return ok;
}
