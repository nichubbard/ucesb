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

#include "typedef.hh"
#include "watcher_window.hh"

#include "error.hh"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// If none specified
watcher_type_info dummy_watch_types[1] = { { COLOR_GREEN, NULL}, };

extern watcher_type_info WATCH_TYPE_NAMES[NUM_WATCH_TYPES];

watcher_window::watcher_window()
{
  _last_update = 0;

  _display_at_mask = 0;
  _display_counts  = 1000;
  _display_timeout = 1;

  _show_range_stat = 0;
}

#define TOP_WINDOW_LINES 6

typedef void (*void_void_func)(void);

#define COL_NORMAL      1
#define COL_DATA_BKGND  2

#define COL_TYPE_BASE   3

void watcher_window::init()
{
  // Get ourselves some window

  mw = initscr();
  start_color();
  atexit((void_void_func) endwin);
  nocbreak();
  noecho();
  nonl();
  curs_set(0);

  assert (NUM_WATCH_TYPES == sizeof(WATCH_TYPE_NAMES)/sizeof(WATCH_TYPE_NAMES[0]));

  init_pair(COL_NORMAL,     COLOR_WHITE  ,COLOR_BLUE);
  init_pair(COL_DATA_BKGND, COLOR_WHITE  ,COLOR_BLACK);

  for (int type = 0; type < NUM_WATCH_TYPES; type++)
    init_pair((short) (COL_TYPE_BASE+type),
	      WATCH_TYPE_NAMES[type]._color,COLOR_BLACK);

  wtop    = newwin(TOP_WINDOW_LINES,80,               0,0);
  wscroll = newwin(               0,80,TOP_WINDOW_LINES,0);

  scrollok(wscroll,1);

  // Fill it with some information

  wcolor_set(wtop,COL_NORMAL,NULL);
  wbkgd(wtop,COLOR_PAIR(COL_NORMAL));

  //wmove(wtop,0,0);
  //waddstr(wtop,"*** Watcher ***");
  wrefresh(wtop);

  wcolor_set(wscroll,COL_NORMAL,NULL);
  wbkgd(wscroll,COLOR_PAIR(COL_DATA_BKGND));
  wrefresh(wscroll);

  _time = 0;
  _event_no = 0;
  _counter = 0;
  memset(_type_count,0,sizeof(_type_count));

  if (!_display_at_mask)
    {
      _display_at_mask =
	(WATCHER_DISPLAY_COUNT |
	 WATCHER_DISPLAY_SPILL_EOS |
	 WATCHER_DISPLAY_TIMEOUT);
      _display_counts  = 10000;
      _display_timeout = 15;
    }
}

void watcher_window::event(watcher_event_info &info)
{
  {
      vect_watcher_present_channels::iterator ch;

      for (ch = _present_channels.begin(); ch != _present_channels.end(); ++ch)
	(*ch)->event_summary();
  }

  bool display = false;

  // _det_watcher.collect_raw(event_raw,type);
  // _det_watchcoinc.collect_raw(event_raw,type);
  _type_count[info._type]++;
  _counter++;

  if (info._info & WATCHER_DISPLAY_INFO_TIME)
    _time = info._time;

  if (info._event_no & WATCHER_DISPLAY_INFO_EVENT_NO)
    _event_no = info._event_no;


  //if (event_info._time)
  //_time = event_info._time;

  // _det_watcher.collect_physics_raw(event_raw);
  // _det_watcher.collect_physics(event_tcal);

  uint event_spill_on_off =
    info._display & (WATCHER_DISPLAY_SPILL_ON |
		     WATCHER_DISPLAY_SPILL_OFF);

  uint display_type = info._display;

  // see if we have detected a spill on/off change

  if (_last_event_spill_on_off &&
      event_spill_on_off &&
      _last_event_spill_on_off != event_spill_on_off)
    {
      if (event_spill_on_off & WATCHER_DISPLAY_SPILL_ON)
	display_type |= WATCHER_DISPLAY_SPILL_BOS;
      if (event_spill_on_off & WATCHER_DISPLAY_SPILL_OFF)
	display_type |= WATCHER_DISPLAY_SPILL_EOS;
    }

  _last_event_spill_on_off = event_spill_on_off;

  if (display_type & _display_at_mask)
    display = true;

  time_t now = time(NULL);

  if ((_display_at_mask & WATCHER_DISPLAY_COUNT) &&
      _counter >= _display_counts)
    display = true;
  else if ((_display_at_mask & WATCHER_DISPLAY_TIMEOUT) &&
	   (now - _last_update) > (int) _display_timeout)
    display = true;

  if (display)
    {
      _last_update = now;

      time_t t = _time;
      const char* t_str = ctime(&t);

      wcolor_set(wtop,COL_NORMAL,NULL);
      wmove(wtop,0,(int) (40-strlen(t_str) / 2));
      waddstr(wtop,t_str);

      char buf[256];

      sprintf (buf,"Event: %d",_event_no);

      wmove(wtop,2,0);
      waddstr(wtop,buf);

      for (int type = 0; type < NUM_WATCH_TYPES; type++)
	{
	  wcolor_set(wtop,(short) (COL_TYPE_BASE+type),NULL);
	  sprintf (buf,"%9s:%6d ",
		   WATCH_TYPE_NAMES[type]._name,
		   _type_count[type]);
	  wmove(wtop,1+type,80-18);
	  waddstr(wtop,buf);
	}

      wrefresh(wtop);

      watcher_display_info display_info;

      // info._requests = &_requests;

      display_info._w      = wscroll;
      display_info._line   = 0;
      display_info._counts = _counter;
      display_info._show_range_stat = _show_range_stat;

      getmaxyx(wscroll,display_info._max_line,display_info._max_width);

      display_info._col_norm    = COL_DATA_BKGND;
      for (int type = 0; type < NUM_WATCH_TYPES; type++)
	display_info._col_data[type] = (short) (COL_TYPE_BASE+type);

      /*
      _det_watcher.display(info);
      _det_watchcoinc.display(info);
      */

      vect_watcher_channel_display::iterator ch;

      for (ch = _display_channels.begin(); ch != _display_channels.end(); ++ch)
	(*ch)->display(display_info);

      wrefresh(wscroll);

      _counter = 0;
      memset(_type_count,0,sizeof(_type_count));

      for (ch = _display_channels.begin(); ch != _display_channels.end(); ++ch)
	(*ch)->clear_data();
      /*
      _det_watcher.clear();
      _det_watchcoinc.clear();
      */
    }
}

