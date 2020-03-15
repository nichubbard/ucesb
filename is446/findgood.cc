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

// g++ -o findgood -O3 -Wall is446/findgood.cc

/* IS446: event mixing
 *
 * What can we use?
 *
 * What is seen?
 *
 * The event counters of some modules (ADCs, TDCs) at times increase
 * not in sync with the exact event counter that counts each accepted
 * event.  In general it seems that the scaler is ok.
 *
 * If scaler would not be ok, we'd be in deep shit, as that one
 * generates an event for every acepted trigger.  As only one scaler
 * event is read every read-out, if a spurious trigger would generate
 * a scaler event, we'd never get rid of the pile-up!
 *
 * How do we find good events?  Or rather, how do we find that a
 * module is in sync?  One place where we know a module to be in sync
 * is when we (from other info) that that it has been read out, but it
 * delivered no data.  In that case, there was no pile-up present and
 * thus should be in sync.  This would in itself not guarantee that a
 * previous event is good, as a previous event may have been piled-up,
 * that finally got a chance to get out on the previous read-out.  Or
 * a later event, which may be a spurious trigger.
 *
 * A sequence of events, : events - empty(WRO=with read-out) - events
 * however, if the event counter is in sync over the empty(WRO)
 * event(s), i.e. for the event before the EWRO and the event after
 * the EWRO has the same difference as the MBS event counter, we know
 * that no spurious trigger has happened in-between.  We still cannot
 * declare any of the two events as good, as the previous events may
 * have been one that just got out, and the later event may itself be
 * a spurious trigger, but we have a good point.  Now, if we before or
 * after find a similar sequence which is also a good point, and these
 * two good points are in sync with each other, then all events in
 * between are also good.
 *
 * Similarly, we may recover more around these, but let's where this
 * takes us...
 */







#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

struct ec_event
{
  union
  {
    int all[14]; // event_no + scaler0 + adc[7] + tdc[7]
    struct
    {
      int no;
      int scaler;
      int adc[7];
      int tdc[5];
    } p;
  } ec;

  int cnt[3];

  union
  {
    char all[14]; // event_no + scaler0 + adc[7] + tdc[7]
    struct
    {
      char no;
      char scaler;
      char adc[7];
      char tdc[5];
    } p;
  } flags;

  int diff[14]; // difference to MBS counter for 'good' (synced) event
};

#define FLAG_HAS_DATA     0x01
#define FLAG_HAD_READOUT  0x02

#define FLAG_IS_IN_SYNC   0x04
#define FLAG_IS_IN_SYNC2  0x08

#define CNT_TSHORT  0
#define CNT_EBIS    1
#define CNT_NACCEPT 2

int main()
{
  // Read the event counters from stdin...

  std::vector<ec_event> v;

  while (!feof(stdin))
    {
      int n;
      ec_event e;

      memset(&e,0,sizeof(e));

      n = fscanf(stdin,
		 "EC:  %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		 &e.ec.p.no,
		 &e.ec.p.scaler,
		 &e.ec.p.adc[0],&e.ec.p.adc[1],&e.ec.p.adc[2],
		 &e.ec.p.adc[3],&e.ec.p.adc[4],&e.ec.p.adc[5],
		 &e.ec.p.adc[6],
		 &e.ec.p.tdc[0],&e.ec.p.tdc[1],&e.ec.p.tdc[2],
		 &e.ec.p.tdc[3],&e.ec.p.tdc[4],
		 &e.cnt[0],&e.cnt[1],&e.cnt[2]);

      if (n != 17)
	{
	  printf ("Cannot convert... (got %d)!",n);
	  exit(1);
	}

      v.push_back(e);
    }


  for (unsigned int i = 0; i < v.size(); i++)
    {
      ec_event &e = v[i];
      ec_event &pe = (i == 0 ? v[0] : v[i-1]);

      for (int j = 2; j < 14; j++)
	{
	  if (e.ec.all[j])
	    e.flags.all[j] |= FLAG_HAS_DATA;
	}

      if (e.ec.p.scaler ||
	  (e.cnt[CNT_TSHORT]  <= pe.cnt[CNT_TSHORT] &&
	   e.cnt[CNT_EBIS]    <= pe.cnt[CNT_EBIS] &&
	   e.cnt[CNT_NACCEPT] <= pe.cnt[CNT_NACCEPT]))
	e.flags.all[1] |= FLAG_HAS_DATA;

#define IF_HAS_DATA(type,no) if (e.ec.p.type[no])
#define RO(type,no) { e.flags.p.type[no] |= FLAG_HAD_READOUT; }

      IF_HAS_DATA(adc,0) { RO(adc,1); RO(adc,6); RO(tdc,0); RO(tdc,1); RO(tdc,4); }
      IF_HAS_DATA(adc,1) { RO(adc,0); RO(adc,6); RO(tdc,0); RO(tdc,1); RO(tdc,4); }
      IF_HAS_DATA(adc,2) { RO(adc,3); RO(adc,6); RO(tdc,2); RO(tdc,3); RO(tdc,4); }
      IF_HAS_DATA(adc,3) { RO(adc,2); RO(adc,6); RO(tdc,2); RO(tdc,3); RO(tdc,4); }
      //IF_HAS_DATA(adc,4) { RO(adc,5); RO(adc,6); RO(tdc,2);            RO(tdc,4); }
      //IF_HAS_DATA(adc,5) { RO(adc,4); RO(adc,6); RO(tdc,2);            RO(tdc,4); }
      IF_HAS_DATA(adc,6) {                                             RO(tdc,4); }

      IF_HAS_DATA(tdc,0) { RO(adc,0); RO(adc,1); RO(adc,6); RO(tdc,1); RO(tdc,4); }
      IF_HAS_DATA(tdc,1) { RO(adc,0); RO(adc,1); RO(adc,6); RO(tdc,0); RO(tdc,4); }
      IF_HAS_DATA(tdc,2) { RO(adc,2); RO(adc,3); RO(adc,6); RO(tdc,3); RO(tdc,4); }
      IF_HAS_DATA(tdc,3) { RO(adc,2); RO(adc,3); RO(adc,6); RO(tdc,2); RO(tdc,4); }
      //IF_HAS_DATA(tdc,2) { RO(adc,6);                                  RO(tdc,4); }
      //IF_HAS_DATA(tdc,3) { RO(adc,2); RO(adc,3); RO(adc,6); RO(tdc,2); RO(tdc,4); }
      IF_HAS_DATA(tdc,4) {                       RO(adc,6);                       }
    }

  for (int j = 2; j < 14; j++)
    {
      int prev_data_i = -1;
      int prev_data_diff = 0;

      for (unsigned int i = 0; i < v.size(); i++)
	{
	  ec_event &e = v[i];

	  if (e.flags.all[j] & FLAG_HAS_DATA)
	    {
	      prev_data_i = i;
	      prev_data_diff = e.ec.all[j] - e.ec.p.no;
	    }

	  if ((e.flags.all[j] & (FLAG_HAS_DATA | FLAG_HAD_READOUT)) == FLAG_HAD_READOUT)
	    {
	      // event is empty, make search for next event with some
	      // data, see if diff is same as for previous event with
	      // data...

	      if (prev_data_i != -1)
		{
		  for (unsigned int ii = i+1; ii < v.size(); ii++)
		    {
		      ec_event &ee = v[ii];

		      if (ee.flags.all[j] & FLAG_HAS_DATA)
			{
			  if ((ee.ec.all[j] - ee.ec.p.no) != prev_data_diff)
			    goto diff_not_same;
			  // Difference is the same, mark all events
			  // between i and ii (not including the
			  // boundaries) as good

			  // printf ("j:%d,i:%d,ii:%d\n",j,i,ii);

			  for (unsigned int iii = prev_data_i+1; iii < ii; iii++)
			    {
			      ec_event &eee = v[iii];

			      eee.flags.all[j] |= FLAG_IS_IN_SYNC;
			      eee.diff[j] = prev_data_diff;
			    }

			  break;
			}
		    }
		}
	    diff_not_same:
	      // Difference not correct, just continue...
	      ;
	    }
	}

      int prev_sync_i = -1;

      for (unsigned int i = 0; i < v.size(); i++)
	{
	  ec_event &e = v[i];

	  if (e.flags.all[j] & FLAG_IS_IN_SYNC)
	    {
	      if (prev_sync_i != -1 &&
		  prev_data_diff == e.diff[j])
		{
		  // difference is same since last time

		  for (unsigned int iii = prev_sync_i+1; iii < i; iii++)
		    {
		      ec_event &eee = v[iii];

		      eee.flags.all[j] |= FLAG_IS_IN_SYNC2;
		      eee.diff[j] = prev_data_diff;
		    }
		}
	      prev_sync_i = i;
	      prev_data_diff = e.diff[j];
	    }
	}




    }


  for (unsigned int i = 0; i < v.size(); i++)
    {
      ec_event &e = v[i];

      printf ("%08d |",e.ec.p.no);

      printf (" %08d %4d %4d |",e.cnt[CNT_TSHORT],e.cnt[CNT_EBIS],e.cnt[CNT_NACCEPT]);

      printf (" %02x %02x %02x | ",
	      (e.cnt[CNT_TSHORT] - e.ec.p.no) & 0xff,
	      (e.cnt[CNT_EBIS] - e.ec.p.no) & 0xff,
	      (e.cnt[CNT_NACCEPT] - e.ec.p.no) & 0xff);

      for (int j = 1; j < 14; j++)
	{
	  if (e.flags.all[j] & FLAG_HAD_READOUT)
	    printf (" >");
	  else
	    printf ("  ");

	  if (e.flags.all[j] & FLAG_HAS_DATA)
	    {
	      printf ("%02x",(e.ec.all[j] - e.ec.p.no) & 0xff);

	    }
	  else
	    {
	      printf ("  ");
	    }

	  if (e.flags.all[j] & (FLAG_IS_IN_SYNC | FLAG_IS_IN_SYNC2))
	    {
	      printf ("%c%x",((e.flags.all[j] & FLAG_IS_IN_SYNC) ? '.' : ','),e.diff[j] & 0x0f);
	    }
	  else
	    printf ("  ");

	}

      printf ("\n");

    }

  int needok = ((1 <<  2) | (1 <<  3) | (1 <<  4) | (1 <<  5) | (1 <<  8) |
		(1 <<  9) | (1 << 10) | (1 << 11) | (1 << 12) | (1 << 13));

  for (unsigned int i = 0; i < v.size(); i++)
    {
      ec_event &e = v[i];

      int ok = 0;

      for (int j = 1; j < 14; j++)
	{
	  if (e.flags.all[j] & (FLAG_IS_IN_SYNC | FLAG_IS_IN_SYNC2))
	    ok |= (1 << j);
	}

      if ((ok & needok) == needok)
	printf ("GOOD: %d\n",e.ec.p.no);
    }


}
