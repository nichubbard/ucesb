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

#ifdef USER_WATCHER_INIT
void  USER_WATCHER_INIT();
#endif

#ifdef USER_WATCHER_DISPLAY
void USER_WATCHER_DISPLAY(watcher_display_info&);
#endif

#ifdef USER_WATCHER_CLEAR
void USER_WATCHER_CLEAR();
#endif

#ifdef USER_KEEPALIVE_FUNCTION
void USER_KEEPALIVE_FUNCTION(bool dead);
#endif

// If none specified
watcher_type_info dummy_watch_types[1] = { { COLOR_GREEN, NULL}, };

extern watcher_type_info WATCH_TYPE_NAMES[NUM_WATCH_TYPES];

watcher_window::watcher_window()
{
  _last_update = 0;
  _init = false;

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

#define COL_TEXT_NORMAL 10
#define COL_TEXT_ERROR 11
#define COL_TEXT_WARNING 12
#define COL_TEXT_INFO 13
#define COL_TEXT_ERROR_BLINK 14

std::deque<std::pair<std::string, int>> errors;
#include "colourtext.hh"

void watcher_window::init()
{
  // Get ourselves some window
  
  mw = initscr();
  start_color();
  atexit([]
    { 
      endwin();
      for (auto const& i : errors)
      {
        markconvbold_output(i.first.c_str(),
  			  i.second == FE_ERROR ? CTR_WHITE_BG_RED :
  			  i.second == FE_WARNING ? CTR_BLACK_BG_YELLOW :
  			  CTR_NONE);
      }
    }
  );
  nocbreak(); 
  noecho();
  nonl();
  curs_set(0);

  assert (NUM_WATCH_TYPES == sizeof(WATCH_TYPE_NAMES)/sizeof(WATCH_TYPE_NAMES[0]));

  init_pair(COL_NORMAL,     COLOR_WHITE  ,COLOR_BLUE);
  init_pair(COL_DATA_BKGND, COLOR_WHITE  ,COLOR_BLACK);
  
  init_pair(COL_TEXT_NORMAL, COLOR_WHITE  ,COLOR_BLACK);
  init_pair(COL_TEXT_ERROR,  COLOR_WHITE  ,COLOR_RED);
  init_pair(COL_TEXT_WARNING,COLOR_YELLOW ,COLOR_BLACK);
  init_pair(COL_TEXT_INFO,   COLOR_GREEN  ,COLOR_BLACK);
  init_pair(COL_TEXT_ERROR_BLINK, COLOR_RED, COLOR_WHITE);

  for (int type = 0; type < NUM_WATCH_TYPES; type++)
    init_pair((short) (COL_TYPE_BASE+type),
	      WATCH_TYPE_NAMES[type]._color,COLOR_BLACK);
      

  wtop      = newwin(TOP_WINDOW_LINES,80,               0,    0); 
  wscroll   = newwin(              41,80,TOP_WINDOW_LINES,    0);
  werrortop = newwin(               1,80,TOP_WINDOW_LINES+41,  0);
  werror    = newwin(               0, 0,TOP_WINDOW_LINES+42, 0);

  scrollok(wscroll,1);
  scrollok(werror, 1);

  // Fill it with some information

  wcolor_set(wtop,COL_NORMAL,NULL);
  wbkgd(wtop,COLOR_PAIR(COL_NORMAL));

  //wmove(wtop,0,0);
  //waddstr(wtop,"*** Watcher ***");
  wrefresh(wtop);

  wcolor_set(wscroll,COL_NORMAL,NULL);
  wbkgd(wscroll,COLOR_PAIR(COL_DATA_BKGND));
  mvwaddstr(wscroll, 2, 10, "Waiting for events...");
  wrefresh(wscroll);

  wcolor_set(werrortop, COL_TEXT_ERROR, NULL);
  wbkgd(werrortop,COLOR_PAIR(COL_TEXT_NORMAL));
  wmove(werrortop, 0, 0);
  whline(werrortop, ACS_HLINE, 80);
  mvwaddstr(werrortop, 0, 1, "ucesb Log");
  wrefresh(werrortop);  
  
  wcolor_set(werror,COL_TEXT_NORMAL,NULL);
  wbkgd(werror,COLOR_PAIR(COL_TEXT_NORMAL));
  
  
  wmove(werror, 0, 0);
  wrefresh(werror);  

  _init = true;
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

#ifdef USER_WATCHER_INIT
  USER_WATCHER_INIT();
#endif
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
	  sprintf (buf,"%9s:%8d ",
		   WATCH_TYPE_NAMES[type]._name,
		   _type_count[type]);
	  wmove(wtop,1+type,80-20);
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

#ifdef USER_WATCHER_DISPLAY
      USER_WATCHER_DISPLAY(display_info);
#endif

      wrefresh(wscroll);
      wrefresh(werror);  

      _counter = 0;
      memset(_type_count,0,sizeof(_type_count));

      for (ch = _display_channels.begin(); ch != _display_channels.end(); ++ch)
	(*ch)->clear_data();

#ifdef USER_WATCHER_CLEAR
      USER_WATCHER_CLEAR();
#endif
      /*
      _det_watcher.clear();
      _det_watchcoinc.clear();
      */
    }
}

static void rectangle(WINDOW* w, int y1, int x1, int y2, int x2)
{
  mvwhline(w, y1, x1, 0, x2-x1);
  mvwhline(w, y2, x1, 0, x2-x1);
  mvwvline(w, y1, x1, 0, y2-y1);
  mvwvline(w, y1, x2, 0, y2-y1);
  mvwaddch(w, y1, x1, ACS_ULCORNER);
  mvwaddch(w, y2, x1, ACS_LLCORNER);
  mvwaddch(w, y1, x2, ACS_URCORNER);
  mvwaddch(w, y2, x2, ACS_LRCORNER);
}

static void cprintw(WINDOW* w, int y, const char* msg, ...)
{
  char buffer[80];
  va_list args;
  va_start(args, msg);
  vsprintf(buffer, msg, args);
  int x = 5 + 70/2 - strlen(buffer)/2;
  mvwprintw(w, y, x, buffer);
  va_end(args);
}

void watcher_window::keepalive()
{
  time_t now = time(NULL);

#ifdef USER_KEEPALIVE_FUNCTION
  USER_KEEPALIVE_FUNCTION((now - _last_update > 10));
#endif

  if (now - _last_update > 10)
  {
    werase(wscroll);
    rectangle(wscroll, 2, 5, 6, 75);
    static int flash = 0;
    if (flash == 0)
    {
      wcolor_set(wscroll, COL_TEXT_ERROR, NULL);
      flash = 1;
    }
    else
    {
      wcolor_set(wscroll, COL_TEXT_ERROR_BLINK, NULL);
      flash = 0;
    }
    cprintw(wscroll, 3, "! DAQ ERROR !");
    wcolor_set(wscroll, 2, NULL);
    wrefresh(wscroll);
    if (_last_update)
    {
      cprintw(wscroll, 4, "No data received from MBS in %d seconds", now - _last_update);
    }
    else
    {
      cprintw(wscroll, 4, "No data received from MBS");
    }
    wrefresh(wscroll);
    cprintw(wscroll, 5, "Check the DAQ!");
    wmove(wscroll, 7, 0);
    wrefresh(wscroll);
  }
}

void watcher_window::on_error(const char* buf, int type)
{
  errors.push_back({buf, type});
  if (errors.size() > 20)
    errors.pop_front();
  
  wcolor_set(werror,COL_TEXT_NORMAL+type,NULL);  
  
  const char *escape = strchr(buf,'\033');
  
  if (!escape)
    wprintw (werror,"%s\n",buf);
  else
  {    
    const char *curbuf = buf;
    
    do
    {
      size_t eat = 2;
      int attr = 0;
      
      /*
    A_NORMAL        Normal display (no highlight)
    A_STANDOUT      Best highlighting mode of the terminal.
    A_UNDERLINE     Underlining
    A_REVERSE       Reverse video
    A_BLINK         Blinking
    A_DIM           Half bright
    A_BOLD          Extra bright or bold
    A_PROTECT       Protected mode
    A_INVIS         Invisible or blank mode
    A_ALTCHARSET    Alternate character set
    A_CHARTEXT      Bit-mask to extract a character
    COLOR_PAIR(n)   Color-pair number n 
    */
      
      switch (escape[1])
      {
        case 'A':
        attr = A_BOLD;
        break;
        case 'B':
        attr = A_NORMAL;
        break;
        case 'C':
        
        break;
        case 'D':
        //ctext_esc = CT_ERR(GREEN);
        break;
        case 'E':
        //ctext_esc = CT_ERR(BLUE);
        break;
        case 'F':
        //ctext_esc = CT_ERR(MAGENTA);
        break;
        case 'G':
        //ctext_esc = CT_ERR(CYAN);
        break;
        case 'H':
        //ctext_esc = CT_ERR(BLACK);
        break;
        case 'I':
        //ctext_esc = CT_ERR(WHITE);
        break;
        default:
        eat = 1;
        break;
      }
      
      wprintw (werror,"%.*s",
      (int) (escape - curbuf),curbuf);
      
      curbuf = escape + eat;
      
      wattrset(werror, attr);
      wcolor_set(werror,COL_TEXT_NORMAL+type,NULL);  
      
      escape = strchr(curbuf,'\033');
    }
    while (escape);
    
    wprintw (werror,"%s\n",curbuf);
  }
    
  wrefresh(werror);
}
