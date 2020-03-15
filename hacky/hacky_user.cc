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

#include "structures.hh"

#include "user.hh"

#define PRINT_RAW_DATA  0
#define PRINT_HITS      0

void user_function(unpack_event *event,
		   raw_event    *raw_event,
		   cal_event    *cal_event,
		   hacky_user_struct *user_event)
{
  /*-------------------------------------------------------------------------*/

#if PRINT_RAW_DATA | PRINT_HITS
  printf ("== Event: %d  Trigger: %d ==========================\n",
	  event->event_no,
	  event->trigger);
#endif

  /*-------------------------------------------------------------------------*/

  EXTERNAL_DATA16 &data16 = event->cros3.data16;

#if PRINT_RAW_DATA
  // First simply print the data;

  for (size_t i = 0; i < data16._length; i++)
    {
      if ((i & 0x07) == 0)
	printf ("%04x: ",i);

      printf (" %04x",data16._data[i]);

      if ((i & 0x07) == 0x07)
	printf ("\n");
    }
  if (data16._length & 0x07)
    printf ("\n");
#endif

  /*-------------------------------------------------------------------------*/

  // Then we do a very quick and dirty unpacking of the data.  Dirty in
  // many ways, e.g. that it does not do all consistency checks that
  // should be done.

  uint16 *p = data16._data;
  uint16 *e = p + data16._length;

  while(p < e)
    {
      // There should be another CCB header

#define CHECK_AVAIL(n) if (p + (n) > e) ERROR("Unexpected end-of-data");

      CHECK_AVAIL(4);

      if ((p[0] & 0xff00) == 0xaf00)
	{
	  p += 4;
	  CHECK_AVAIL(2); // at least trailer should fit
	  goto trailer;
	}

      {
	if ((p[0] & 0xff00) != 0xab00)
	  ERROR("Expected header, got %04x,",*p);

	uint16 ccb_id = (uint16) ((p[2] & 0x0f00) >> 8);

	if (ccb_id != 1 && ccb_id != 2)
	  ERROR("Unknown CCB id: %d",ccb_id);

	CROS3_hits &ccb = user_event->ccb[ccb_id - 1];

	p += 4;

	// Next is either trailer, or an AD16 header

	CHECK_AVAIL(2); // at least trailer should fit

	while ((p[0] & 0xf000) == 0xc000)
	  {
	    CHECK_AVAIL(4);

	    uint16 ad16_id = (uint16) ((p[2] & 0x0f00) >> 8);

	    p += 4;

	    // As long as we have data words, eat them

	    while (p < e && (p[0] & 0xf000) == 0x0000)
	      {
		uint16 ch    = (uint16) ((p[0] & 0x0f00) >> 8);
		uint16 value = (p[0] & 0x00ff);

		uint16 wire = (uint16) ((ad16_id << 4) | ch);

#if PRINT_HITS
		printf ("%3d: %3d\n",wire,value);
#endif

		// This is somewhat ugly, due to two things:
		// a) we do not really have an location (used by ucesb
		//    generated code)
		// b) The zero-suppressed data structure insists on
		//    inserting at an index, we'd like to add to the end

		CROS3_wire_hit &hit =
		  ccb.hits.insert_index(-1,ccb.hits._num_items);

		hit.wire  = wire;
		hit.start = value;

		p++;
	      }
	  }
      }

    trailer:
      // trailer existence already checked, now investigate value

      uint16 wordcount = (uint16) ((p[0] & 0x00ff) | ((p[0] & 0x00ff) << 8));

      p += 2;

      if (wordcount & 1)
	{
	  // odd number of words...

	  CHECK_AVAIL(1);

	  if (p[0])
	    ERROR("Data padding not 0");

	  p++;
	}
    }
}

void user_init()
{

}

void user_exit()
{

}


