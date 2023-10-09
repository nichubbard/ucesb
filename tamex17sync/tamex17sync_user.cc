/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2023  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include <math.h>

#include "structures.hh"
#include "convert_picture.hh"
#include "user.hh"

#define COARSE_RANGE   0x2000
#define COARSE_SHIFT   4
#define COARSE_BINS    (COARSE_RANGE >> COARSE_SHIFT)

uint32_t corr[8][16][8][16][COARSE_BINS];

void init_function()
{
  memset(corr, 0, sizeof (corr));
}

uint64 has[8];
uint16 val[8][64];

/* NOTE NOTE NOTE
 *
 * SFP 1 (which is nastily put in tamey instead of tamex member)
 * is NOT, repeat NOT, checked!
 */

template<typename t, typename board_data>
void loop_over_tamex(t *tmx, size_t countof_tmx, board_data */*dummy*/)
{
  int lec_checked = 0;

  for (size_t sys_i = 0;
       sys_i < countof_tmx;
       sys_i++)
    {
      uint32_t lec = -1;
      int has_lec = 0;
      
      for (size_t board_i = 0;
	   board_i < countof(tmx[0].data.tamex);
	   board_i++)
	{
	  board_data *tamex_board =
	    &(tmx[sys_i].data.tamex[board_i].data);

	  bitsone_iterator iter;
	  ssize_t ch_i;

	  while ((ch_i = tamex_board->time_coarse._valid.next(iter)) >= 0)
	    {
	      /* We are only interested in ch 0. */

	      if (ch_i != 0)
		continue;

	      /*
	      printf ("%zd/%2zd/%2zd: %2d :",
		      sys_i, board_i, ch_i,
		      tamex_board->time_coarse._num_entries[ch_i]);
	      */
	      
	      for (uint32 hit_i = 0;
		   hit_i < tamex_board->time_coarse._num_entries[ch_i];
		   hit_i++)
		{
		  /*
		  printf (" %d",
			  tamex_board->time_coarse._items[ch_i][hit_i].value);
		  */
		  
		  has[sys_i] |= ((uint64_t) 1) << board_i;
		  val[sys_i][board_i] =
		    tamex_board->time_coarse._items[ch_i][hit_i].value;
		}

	      if (tamex_board->time_coarse._num_entries[ch_i] > 0)
		{
		  if (!has_lec)
		    {
		      lec = tamex_board->tdc_header.lec;
		      has_lec = 1;
		    }
		  else
		    {
		      if (lec != tamex_board->tdc_header.lec)
			{
			  WARNING("lec mismatch %d != %d\n",
				  lec, tamex_board->tdc_header.lec);
			}
		      lec_checked++;
		    }

		}

	      /* printf ("\n"); */
	      
	      /* _items[n][max_entries] */

	    }

	}
    }

  (void) lec_checked;
  /* printf ("lec_checked: %d\n", lec_checked); */
}

void user_function(unpack_event *event,
		   raw_event    *raw_event)
{
  memset(has, 0, sizeof (has));

  loop_over_tamex(event->tmxfh, countof(event->tmxfh),
		  &(event->tmxfh[0].data.tamex[0].data));
  loop_over_tamex(event->tmxfh2, countof(event->tmxfh2),
		  &(event->tmxfh2[0].data.tamex[0].data));
  loop_over_tamex(event->tmx, countof(event->tmx),
		  &(event->tmx[0].data.tamex[0].data));

  for (size_t sys_i = 0; sys_i < 8; sys_i++)
    for (size_t board_i = 0; board_i < 16; board_i++)
      if (has[sys_i] & (((uint64_t) 1) << board_i))
	for (size_t sys_j = 0; sys_j < 8; sys_j++)
	  for (size_t board_j = 0; board_j < 16; board_j++)
	    if (has[sys_j] & (((uint64_t) 1) << board_j))
	      {
		uint16 vi = val[sys_i][board_i];
		uint16 vj = val[sys_j][board_j];
		uint16 bin = ((vi - vj) >> COARSE_SHIFT) & (COARSE_BINS - 1);

		corr[sys_i][board_i][sys_j][board_j][bin]++;
	      }
}

const char *_syncplot_name = NULL;

void exit_function()
{
  size_t dim = (size_t) (8 * 16);

  printf ("dim %zd\n",dim);

  unsigned char *pict =
    (unsigned char *) malloc(sizeof(unsigned char) * dim * dim);

  memset(pict, 0, sizeof(char) * dim * dim);

  for (int sys_i = 0; sys_i < 8; sys_i++)
    for (int board_i = 0; board_i < 16; board_i++)
      {
	int x = sys_i * 16 + board_i;

	for (int sys_j = 0; sys_j < 8; sys_j++)
	  for (int board_j = 0; board_j < 16; board_j++)
	    {
	      int y = sys_j * 16 + board_j;

	      int      max_i = -1;
	      uint32_t max_c = 0;
	      uint32_t sum_c = 0;
	      uint32_t sum_close_c = 0;

	      for (int i = 0; i < COARSE_BINS; i++)
		{
		  uint32_t c =
		    corr[sys_i][board_i][sys_j][board_j][i];
		  
		  if (c > max_c)
		    {
		      max_i = i;
		      max_c = c;
		    }
		  sum_c += c;
		}

	      /* Count everything within +/- of the max. */

	      for (int ii = -2; ii <= 2; ii++)
		{
		  uint32_t c =
		    corr[sys_i][board_i][sys_j][board_j]
		    [(max_i+ii) & (COARSE_BINS - 1)];

		  sum_close_c += c;
		}

	      /* printf (" [%d %4d/%4d]", max_i, sum_close_c, sum_c); */
	      /* printf (" %4d:%.3f", max_i, (1. * sum_close_c) / sum_c); */

	      double peak_fraction = (1. * sum_close_c) / sum_c;

	      if (sum_c == 0)
		pict[y * dim + x] = 255;
	      else
		pict[y * dim + x] = (unsigned char) (peak_fraction * 128);

	      if (sum_c == 0)
		printf ("-");
	      else if (peak_fraction > 0.98)
		printf (".");
	      else
		printf ("%1.0f",-log2(peak_fraction));
	    }
	printf ("\n");
      }

  if (_syncplot_name)
    convert_picture(_syncplot_name,(char *) pict,(int) dim,(int) dim);

  free(pict);
}

void tamex17sync_usage_command_line_options()
{
  //      "  --option          Explanation.\n"
  printf ("  --syncplot=FILE   Generate sync plot.\n");
}

bool tamex17sync_handle_command_line_option(const char *arg)
{
  const char *post;

#define MATCH_PREFIX(prefix,post) (strncmp(arg,prefix,strlen(prefix)) == 0 && *(post = arg + strlen(prefix)) != '\0')
#define MATCH_ARG(name) (strcmp(arg,name) == 0)

 if (MATCH_PREFIX("--syncplot=",post)) {
   _syncplot_name = post;
   return true;
 }

 return false;
}
