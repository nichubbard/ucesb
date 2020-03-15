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

#include "structures.hh"

#include "event_base.hh"

#include "user.hh"

void user_function(unpack_event *event,
		   raw_event    *raw_event,
		   cal_event    *cal_event)
{
  /*
  printf ("%7d: %5d %5d %5d %5d\n",
	  event->event_no,
	  raw_event->SCI[0][0].T,
	  raw_event->SCI[0][1].T,
	  raw_event->SCI[0][0].E,
	  raw_event->SCI[0][1].E);
  */
  /*
  printf ("tdc0: "); event->vme.tdc0.data.dump(); printf ("\n");
  printf ("qdc0: "); event->vme.qdc0.data.dump(); printf ("\n");
  printf ("adc0: "); event->vme.adc0.data.dump(); printf ("\n");
  printf ("scaler: "); event->vme.scaler0.data.dump(); printf ("\n");
  */
  /*
  volatile int j = 0;

  for (int i = 0; i < 2000; i++)
    j = ((j * 3) + 1) ^ j;
  */
}

void user_init()
{
  EXT_EBYE_DATA &data = _static_event._unpack.ev.data;

  memset(data._group_item,0,sizeof(data._group_item));
  memset(data._group_data,0,sizeof(data._group_data));
  memset(data._ext_group_data,0,sizeof(data._ext_group_data));
}

void user_exit()
{
  EXT_EBYE_DATA &data = _static_event._unpack.ev.data;

  for (int group = 0; group < 0x100; group++)
    {
      bool has_group = 0;

      for (int item = 0; item < 0x40; item++)
	if (data._group_item[group][item])
	  has_group = 1;

      if (has_group)
	{
	  printf ("Group 0x%02x: ",group);

	  for (int item = 0; item < 0x40; item++)
	    printf ("%s%s",
		    (item && (item & 0x0f)) == 0 ? "." : "",
		    data._group_item[group][item] ? "x" : " ");

	  printf ("\n");
	}
    }

  for (int group_base = 0; group_base < 0x100; group_base += 0x40)
    {
      bool has_group = 0;

      for (int offset = 0; offset < 0x40; offset++)
	if (data._group_data[group_base+offset])
	  has_group = 1;

      if (has_group)
	{
	  printf ("Group 0x%02x+ ",group_base);

	  for (int offset = 0; offset < 0x40; offset++)
	    printf ("%s%s",
		    (offset && (offset & 0x0f)) == 0 ? "." : "",
		    data._group_data[group_base+offset] ? "x" : " ");

	  printf ("\n");
	}
    }

  for (int group_base = 0; group_base < 0x10000; group_base += 0x40)
    {
      bool has_group = 0;

      for (int offset = 0; offset < 0x40; offset++)
	if (data._ext_group_data[group_base+offset])
	  has_group = 1;

      if (has_group)
	{
	  printf ("EGrp0x%04x+ ",group_base);

	  for (int offset = 0; offset < 0x40; offset++)
	    printf ("%s%s",
		    (offset && (offset & 0x0f)) == 0 ? "." : "",
		    data._ext_group_data[group_base+offset] ? "x" : " ");

	  printf ("\n");
	}
    }

}
