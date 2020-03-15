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

#include "thread_info_window.hh"
#include "thread_info.hh"

#include "error.hh"

#include <stdlib.h>
#include <string.h>
#include <time.h>

thread_info_window::thread_info_window()
{


}

#define INPUT_WINDOW_LINES      3
#define TASK_WINDOW_HDR_LINES   2
#define THREAD_WINDOW_HDR_LINES 2
#define TOTAL_WINDOW_LINES      2

typedef void (*void_void_func)(void);

#define COL_NORMAL       1
#define COL_TEXT_NORMAL  2
#define COL_TEXT_ERROR   3 // red
#define COL_TEXT_WARNING 4 // blue
#define COL_TEXT_INFO    5 // blue

void thread_info_window::init(thread_info *ti)
{
  _ti = ti;

  // Get ourselves some window

  mw = initscr();
  start_color();
  atexit((void_void_func) endwin);
  nocbreak();
  noecho();
  nonl();
  curs_set(0);

  init_pair(COL_NORMAL,      COLOR_WHITE  ,COLOR_BLUE);
  init_pair(COL_TEXT_NORMAL, COLOR_BLACK  ,COLOR_WHITE);
  init_pair(COL_TEXT_ERROR,  COLOR_RED    ,COLOR_WHITE);
  init_pair(COL_TEXT_WARNING,COLOR_BLUE   ,COLOR_WHITE);
  init_pair(COL_TEXT_INFO,   COLOR_GREEN  ,COLOR_WHITE);


  //init_pair(COL_DATA_BKGND, COLOR_WHITE  ,COLOR_BLACK);
  //init_pair(COL_PHYSICS,    COLOR_GREEN   | A_STANDOUT,COLOR_BLACK);
  //init_pair(COL_OFFSPILL,   COLOR_MAGENTA | A_STANDOUT,COLOR_BLACK);
  //init_pair(COL_TCAL,       COLOR_YELLOW  | A_STANDOUT,COLOR_BLACK);
  //init_pair(COL_OTHER,      COLOR_RED     | A_STANDOUT,COLOR_BLACK);

  int used = 0;
  int use;

  winput   = newwin(use = INPUT_WINDOW_LINES,80,
		    0,0); used += use;
#ifdef USE_THREADING
  wtasks   = newwin(use = TASK_WINDOW_HDR_LINES+_ti->_num_tasks,80,
		    used,0); used += use;
  wthreads = newwin(use = THREAD_WINDOW_HDR_LINES+_ti->_num_threads,80,
		    used,0); used += use;
#endif
  wtotals  = newwin(use = TOTAL_WINDOW_LINES,80,
		    used,0); used += use;
  werrors  = newwin(0,80,used,0);

  scrollok(werrors,1);

  for (int i = 0; i < 4; i++)
    {
      wcolor_set(wall[i],COL_NORMAL,NULL);
      wbkgd(wall[i],COLOR_PAIR(COL_NORMAL));
      wnoutrefresh(wall[i]);
    }

  wcolor_set(werrors,COL_TEXT_NORMAL,NULL);
  wbkgd(werrors,COLOR_PAIR(COL_TEXT_NORMAL));
  wnoutrefresh(werrors);

  doupdate();
}

int wadd_mag_str(WINDOW *win,
		 int chars,double value,
		 int prefix = 0,int fp = 1)
{
  char buf[64];

  // Hmm.  Lets not care about rounding, then we just need to move the decimal point around...

  // First print the value into the buffer, with as many decimals as
  // we have characters to use (such that floating point values
  // also can get decimals if needed)

  sprintf (buf,"%.*f",fp ? chars : 1,value);

  // Then, figure out where the decimal point ended up.

  int pre_radix_digits = strchr(buf,'.') - buf;
  int left;

  // Now, if there are more digits than we may output characters, we
  // have a problem, i.e. must use a prefix

  if (pre_radix_digits > chars || prefix)
    {
      const char *prefixes = " kMGTPE????";

      // The thing needs pre_radix_digits characters, but we can
      // only spend chars-1 (1 goes for the prefix).

      int min_prefix = (pre_radix_digits - (chars-1) + 2) / 3;
      int max_prefix = (pre_radix_digits-1) / 3;

      if (prefix < min_prefix)
	prefix = min_prefix;
      if (prefix > max_prefix)
	prefix = max_prefix;

      // So, we'll chop of prefix*3 characters
      // Since we needed to use a prefix, no of the original decimals will survive,
      // so we may do a memmove, to insert the decimal point

      int radix = pre_radix_digits - 3 * prefix;

      // move all characters behind our radix point away

      memmove(buf+radix+1,buf+radix,prefix*3);

      // Now, if the radix location is just before the prefix, do not emit it

      if (radix == chars-2)
	{
	  buf[chars-2] = prefixes[prefix];
	  buf[chars-1] = 0;
	  left = 1;
	}
      else if (!fp && !prefix)
	{
	  // We were not floating point.  So kill the '.'
	  buf[radix]   = 0;
	  left = chars - radix;
	}
      else
	{
	  buf[radix]   = '.';
	  buf[chars-1] = prefixes[prefix];
	  buf[chars]   = 0;
	  left = 0;
 	}
    }
  else if (pre_radix_digits == chars - 1)
    {
      // The decimal point ended up as the last character.  Kill it
      buf[chars-1] = 0;
      left = 1;
    }
  else if (!fp)
    {
      // We were not floating point.  So kill the '.'
      buf[pre_radix_digits] = 0;
      left = chars - pre_radix_digits;
    }
  else
    {
      // Kill at our maximum allowed length (string may be smaller on
      // it's own - harmless)
      buf[chars] = 0;
      left = 0;
    }

  // insert spaces to the left of the string

  memmove(buf+left,buf,chars+1-left);
  memset(buf,' ',left);

  return waddstr(win,buf);
}

int wadd_magi_str(WINDOW *win,
		  int chars,double value,
		  int prefix = 0)
{
  return wadd_mag_str(win,chars,value,prefix,0);
}


void thread_info_window::display()
{
  //////////////////////////////////////////////////////////////
  // Input stage information

  wcolor_set(winput,COL_NORMAL,NULL);

  wmove(winput,0,0);
  waddstr(winput,"Input: ");
  if (_ti->_input._reader_type)
    {
      waddstr(winput,_ti->_input._reader_type);
      waddstr(winput," ");
    }
  if (_ti->_input._filename)
    waddstr(winput,_ti->_input._filename);
  else
    waddstr(winput,"-");

  wmove(winput,1,0);
  waddstr(winput,"Buffer:  ahead ");
  wadd_magi_str(winput,6,_ti->_input._ahead,1);
  waddstr(winput," active ");
  wadd_magi_str(winput,6,_ti->_input._active,1);
  waddstr(winput," free ");
  wadd_magi_str(winput,6,_ti->_input._free,1);

  {
    time_t now = time(NULL);
    char buf[128];

    sprintf (buf,"%d",(int) now);

    wmove(winput,0,60);
    waddstr(winput,buf);
  }

  wnoutrefresh(winput);

#ifdef USE_THREADING
  //////////////////////////////////////////////////////////////
  // Tasks information

  wcolor_set(wtasks,COL_NORMAL,NULL);
  wmove(wtasks,0,0);
  waddstr(wtasks,"Task      Speed    Threads  Queue");

  for (int ta = 0; ta < _ti->_num_tasks; ta++)
    {
      ti_task *task = _ti->_tasks._tasks[ta];

      if (!task)
	continue;

      wmove(wtasks,ta+1,0);
      waddstr(wtasks,task->_name);
      wmove(wtasks,ta+1,10);
      wadd_magi_str(wtasks,5,task->_speed,0);
      waddstr(wtasks,"/s  ");

      if (task->_type & TI_TASK_SERIAL)
	{
	  ti_task_serial *serial_task = (ti_task_serial*) task;

	  waddstr(wtasks,"SERIAL   ");

	  wadd_magi_str(wtasks,5,serial_task->_todo,0);
	  if (task->_type & TI_TASK_QUEUE_FAN_IN)
	    {
	      waddstr(wtasks," (");
	      wadd_magi_str(wtasks,5,serial_task->_available,0);
	      waddstr(wtasks,")");
	    }
	}
      else if (task->_type & TI_TASK_PARALLEL)
	{
	  ti_task_multi  *multi_task = (ti_task_multi*) task;

	  wadd_mag_str(wtasks,3,multi_task->_threads,0);

	  waddstr(wtasks,"      ");

	  for (int thw = 0; thw < _ti->_num_work_threads; thw++)
	    {
	      waddstr(wtasks," ");
	      wadd_magi_str(wtasks,3,multi_task->_todo[thw],0);
	    }
	}
    }

  wnoutrefresh(wtasks);

  //////////////////////////////////////////////////////////////
  // Threads information

  wcolor_set(wthreads,COL_NORMAL,NULL);
  wmove(wthreads,0,0);
  //               "01234567890        20        30        40        50        60        70        8"
  waddstr(wthreads,"Thr   Task k/s Buf  Task k/s Buf  Task k/s Buf  Task k/s Buf  TBuf      CPU");

  for (int th = 0; th < _ti->_num_threads; th++)
    {
      ti_thread *thread = _ti->_threads._threads[th];

      if (!thread)
	continue;

      wmove(wthreads,th+1,0);
      waddstr(wthreads,thread->_name);

      // Then we need to output the tasks that this thread has been
      // participating in.  If there are too many tasks, we'll cut it
      // down to the most prominent ones.

      /*
      waddstr(wtasks,"-T- ");
      wadd_magi_str(wthreads,5,task->_speed,0);
      */

      wmove(wthreads,th+1,62);
      wadd_magi_str(wthreads,4,thread->_buf_used,0);
      waddstr(wthreads,"/");
      wadd_magi_str(wthreads,4,thread->_buf_size,0);
    }

  wnoutrefresh(wthreads);

#endif
  //////////////////////////////////////////////////////////////
  // Totals information

  wcolor_set(wtotals,COL_NORMAL,NULL);
  wmove(wtotals,0,0);
  waddstr(wtotals,"Analysed: ");


  wnoutrefresh(wtotals);

  //////////////////////////////////////////////////////////////


  // wnoutrefresh(werrors);

  doupdate();
}

void thread_info_window::add_error(const char *text,
				   int severity)
{
  wcolor_set(werrors,COL_TEXT_NORMAL+severity,NULL);
  //waddstr(werrors,">>");
  waddstr(werrors,text);
  waddch(werrors,'\n');
  //waddstr(werrors,"<<");
  wrefresh(werrors);
}
