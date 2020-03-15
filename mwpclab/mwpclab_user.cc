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

#include "user.hh"

#include <curses.h> // needed for the COLOR_ below

watcher_type_info mwpclab_watch_types[NUM_WATCH_TYPES] =
  {
    { COLOR_GREEN,   "Physics" },
    { COLOR_YELLOW,  "Tcal" },
    { COLOR_MAGENTA, "TcalClock" },
    { COLOR_RED,     "Scaler" },
  };

// Number of seconds from 1900 to 1970, 17 leap years...
#define SECONDS_1970 ((365*70+17)*24*3600ul)

void mwpclab_watcher_event_info(watcher_event_info *info,
				unpack_event *event)
{
  info->_type = MWPCLAB_WATCH_TYPE_PHYSICS;

  if (event->ev.trig.value == 1)
    info->_type = MWPCLAB_WATCH_TYPE_PHYSICS;
  else if (event->ev.trig.value == 2)
    info->_type = MWPCLAB_WATCH_TYPE_TCAL;
  else if (event->ev.trig.value == 4)
    {
      info->_type = MWPCLAB_WATCH_TYPE_TCAL_CLOCK;

      if (event->ev.clock.before_s)
	{
	  info->_info |= WATCHER_DISPLAY_INFO_TIME;

	  // With flags & 1, we're using the NTP epoch...

	  if (event->ev.clock.flags & 1)
	    info->_time = (int) (event->ev.clock.before_s - SECONDS_1970);
	  else
	    info->_time = event->ev.clock.before_s;
	}
    }
  else if (event->ev.trig.value == 8)
    {
      info->_type = MWPCLAB_WATCH_TYPE_SCALER;
      info->_display |= WATCHER_DISPLAY_SPILL_EOS;
    }



}

void user_function(unpack_event *event,
		   raw_event    *raw_event,
		   cal_event    *cal_event)
{

}
