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

#ifndef __WATCHER_WINDOW_HH__
#define __WATCHER_WINDOW_HH__

#include "watcher_event_info.hh"
#include "watcher_channel.hh"

#include <curses.h>
#include <time.h>
#include <deque>

class watcher_window
{
public:
  watcher_window();

protected:
  WINDOW* mw;
  WINDOW* wtop;
  WINDOW* wscroll;
  WINDOW* werror;
  WINDOW* werrortop;

public:
  uint _time;
  uint _event_no;

  uint _counter;
  uint _type_count[NUM_WATCH_TYPES];

  int  _show_range_stat;

public:
  time_t _last_update;
  uint   _last_event_spill_on_off;

public:
  // detector_requests _requests;

  uint _display_at_mask;
  uint _display_counts;
  uint _display_timeout;
  
  bool _init;

public:
  vect_watcher_channel_display  _display_channels;
  vect_watcher_present_channels _present_channels;

public:
  void init();
  void event(watcher_event_info &info);
  void on_error(const char* buf, int type);
  void keepalive();

};

#endif//__WATCHER_WINDOW_HH__
