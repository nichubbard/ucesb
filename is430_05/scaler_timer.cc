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

// This file comes from CVS: isodaq/checker/scaler_timer.cc

#include "scaler_timer.hh"
#include "util.hh"

#include <stdio.h>
#include <limits.h>

scaler_timer::scaler_timer()
{
  bits_mask = 0;
  current0 = 0;
  current0_valid = 0;

  hist_i = 0;
}

void scaler_timer::set0(uint32 new0)
{
  current0 = new0;
  current0_valid = true;

  //printf ("%2d: %10d\n",(hist_i) & (ST_HISTORY_SIZE - 1),new0);

  history[(hist_i++) & (ST_HISTORY_SIZE - 1)] = new0;
  /*
  for (int i = 0; i < ST_HISTORY_SIZE; i++)
    printf ("+ %2d: %10d\n",i,history[i]);
  */
}

bool scaler_timer::set0_estimate(uint32 prev,uint32 now)
{
  //printf ("Estimate: prev: %10d  now: %10d\n",prev,now);

  // Analyse our history

  if (hist_i < ST_HISTORY_SIZE)
    return false;

  uint32 diff[ST_HISTORY_SIZE-1];
  /*
  for (int i = 0; i < ST_HISTORY_SIZE; i++)
    printf ("%2d: %10d\n",i,history[i]);
*/
  uint32 min_diff = INT_MAX;

  for (int i = 0; i < ST_HISTORY_SIZE-1; i++)
    {
      int i1 = (hist_i + i    ) & (ST_HISTORY_SIZE - 1);
      int i2 = (hist_i + i + 1) & (ST_HISTORY_SIZE - 1);

      diff[i] = (history[i2] -
		 history[i1]) & bits_mask;
      /*
      printf ("%2d %2d  %10d %10d  %10d\n",
	      i1,i2,
	      history[i1],
	      history[i2],
	      diff[i]);
      */
      if (diff[i] < min_diff)
	min_diff = diff[i];
    }

  // Now see if all differences can be suspected to be integer
  // multiples of the minimum found.

  for (int i = 0; i < ST_HISTORY_SIZE-1; i++)
    {
      double multiplier = (double) diff[i] / min_diff;

      UNUSED(multiplier);

      //printf ("%6.1f ",multiplier);
    }
  //printf ("\n");


  return false;
}

uint32 scaler_timer::diff(uint32 now)
{
  //printf ("%d : %10d - %10d = %10d\n",(int) current0_valid,now,current0,(now - current0) & bits_mask);
  if (current0_valid)
    return (now - current0) & bits_mask;
  else
    return 0;
}
