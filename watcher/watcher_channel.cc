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
#include "watcher_channel.hh"

#include "util.hh"

#include <assert.h>
#include <math.h>


inline void display_bins(watcher_display_info& info,
			 watch_range data[NUM_WATCH_TYPES],
			 int range,int width)
{
  char buf[256];
  int bins_per_char = (NUM_WATCH_BINS+width-1) / width;

  assert (bins_per_char * width >= NUM_WATCH_BINS);

  for (int j = 0; j < width; j++)
    {
      uint sum = 0;
      uint max_part = 0;
      int  max_type = -1;

      for (int t = 0; t < NUM_WATCH_TYPES; t++)
	{
	  uint d = 0;

	  for (int k = 0; k < bins_per_char; k++)
	    d += data[t]._bins[range][bins_per_char*j+k];

	  sum += d;

	  if (d > max_part)
	    {
	      max_part = d;
	      max_type = t;
	    }
	}

      if (max_type != -1)
	{
	  wcolor_set(info._w,info._col_data[max_type],NULL);
	  sprintf (buf,"%c",hexilog2b1(sum));
	  waddstr(info._w,buf);
	}
      else
	{
	  wcolor_set(info._w,info._col_norm,NULL);
	  waddstr(info._w,".");
	}
    }
}

inline void display_range_bins(watcher_display_info& info,
			       watch_stat_range *data,
			       int range,int width)
{
#define DRB_OUTPUT_WIDTH 4

  char buf[256];

  for (int j = 0; j < width; j += DRB_OUTPUT_WIDTH)
    {
      int    sum   = 0;
      double sum_x = 0;
      /*
      printf ("J: %d  st: %d  en: %d\n",j,
	      (j * NUM_WATCH_STAT_RANGE_BINS) / width,
	      ((j + DRB_OUTPUT_WIDTH) * NUM_WATCH_STAT_RANGE_BINS) / width);
      */
      for (int bin = (j * NUM_WATCH_STAT_RANGE_BINS) / width;
	   bin < ((j + DRB_OUTPUT_WIDTH) * NUM_WATCH_STAT_RANGE_BINS) / width;
	   bin++)
	{
	  watch_stat_range_bin &stat = data->_bins[range][bin];

	  int start = (stat._start >> 4);
	  int end = stat._num - start;
	  if (end > WATCH_STAT_RANGE_HISTORY)
	    end = WATCH_STAT_RANGE_HISTORY;

	  for (int i = 0; i < end; i++)
	    sum_x += stat._history[(i+start) & (WATCH_STAT_RANGE_HISTORY-1)];
	  sum += end;
	  //sum += stat._num - (stat._start >> 4);
	}

      if (sum >= 2) // 1 noise hit is no hit
	{
	  double mean = sum_x / sum;

	  sprintf (buf,"%4d",(int) (mean + 0.5));
	  //sprintf (buf,"%4d",sum);
	}
      else
	{
	  sprintf (buf,"%*s",DRB_OUTPUT_WIDTH,".");
	}
      waddstr(info._w,buf);


    }
}

void watch_stat_range_bin::forget()
{
  // slowly forget the statistics

  if ((_num - (_start >> 4)) > WATCH_STAT_RANGE_HISTORY)
    _start = (_num - WATCH_STAT_RANGE_HISTORY) << 4; // limit to known statistics
  else if ((_start >> 4) < _num)
    _start++; // forget one count
}

#define HI_LOW_HYSTERESIS 10

void watcher_channel::collect_raw(uint raw,uint type,
				  watcher_event_info *watch_info)
{
  uint range = !!(raw & _rangemark);
  uint value = (raw & _valmask);
  uint other = (raw & ~(_rangemark | _valmask));
  int bin;

  if (other)
    {
      _data[type]._overflow++;
      return;
    }

  if (value < _min)
    {
      _data[type]._zero++;
      return;
    }
  if (value > _max)
    {
      _data[type]._overflow++;
      return;
    }

  if (_log)
    {
      if (!value)
	bin = 0;
      else
	bin = 1 + (int) ((log(value) * (double) (NUM_WATCH_BINS-1)) /
			 log((double) _max + 1.0));
    }
  else
    bin = (int) ((((double) value - (double) _min) * (double) NUM_WATCH_BINS) /
		 ((double) _max - (double) _min + 1.0));

  _range_hit[range] = HI_LOW_HYSTERESIS;
  _data[type]._bins[range][bin]++;

  if (watch_info->_info & WATCHER_DISPLAY_INFO_RANGE)
    {
      if (_log)
	{
	  if (!value)
	    bin = 0;
	  else
	    bin = (int) ((log(value) * (double) NUM_WATCH_STAT_RANGE_BINS) /
			 log((double) _max + 1.0));
	}
      else
	bin = (int) ((((double) value - (double) _min) *
		      (double) NUM_WATCH_STAT_RANGE_BINS) /
		     ((double) _max - (double) _min + 1.0));

      watch_stat_range_bin &stat = _stat_range._bins[range][bin];

      stat._history[stat._num & (WATCH_STAT_RANGE_HISTORY - 1)] =
	(float) watch_info->_range_loc;
      stat._num++;
    }
}

void watcher_channel::display(watcher_display_info& info/*,
			      const signal_id& id,
			      const char* te*/)
{
  //  if (!info._requests->is_channel_requested(id,te))
  //    return;

  if (info._line >= info._max_line)
    return; // or we would overflow

  wcolor_set(info._w,info._col_norm,NULL);

  wmove(info._w,info._line,0);

  waddstr(info._w,_name.c_str());

  //waddstr(info._w,id.to_string().c_str());
  //waddstr(info._w,"_");
  //waddstr(info._w,te);

  char buf[256];

  for (int i = 0; i < NUM_WATCH_TYPES; i++)
    {
      wcolor_set(info._w,info._col_data[i],NULL);

      wmove(info._w,info._line,10+14+i);
      if (_data[i]._zero)
	{
	  sprintf (buf,"%c",hexilog2b1(_data[i]._zero));
	  waddstr(info._w,buf);
	}
      else
	waddstr(info._w,".");
    }

  wmove(info._w,info._line,10+20);

  if (_range_hit[0] && _range_hit[1])
    {
      display_bins(info,_data,0,NUM_WATCH_BINS/2);
      wcolor_set(info._w,info._col_norm,NULL);
      waddstr(info._w,":");
      display_bins(info,_data,1,NUM_WATCH_BINS/2);
    }
  else if (_range_hit[0])
    display_bins(info,_data,0,NUM_WATCH_BINS);
  else if (_range_hit[1])
    display_bins(info,_data,1,NUM_WATCH_BINS);

  for (int i = 0; i < NUM_WATCH_TYPES; i++)
    {
      wcolor_set(info._w,info._col_data[i],NULL);

      wmove(info._w,info._line,10+20+NUM_WATCH_BINS+1+1+i);
      if (_data[i]._overflow)
	{
	  sprintf (buf,"%c",hexilog2b1(_data[i]._overflow));
	  waddstr(info._w,buf);
	}
      else
	waddstr(info._w,".");
    }

  wmove(info._w,info._line,10+20+NUM_WATCH_BINS+1+1+4+2);
  wcolor_set(info._w,info._col_norm,NULL);

  if (_range_hit[0] && _range_hit[1])
    waddstr(info._w,"a");
  else if (_range_hit[0])
    waddstr(info._w,"l");
  else if (_range_hit[1])
    waddstr(info._w,"h");

  for (int r = 0; r < 2; r++)
    if (_range_hit[r])
      _range_hit[r]--;

  info._line++;

  /* Plotting of where (e.g. TCAL) is within another range.
   */

  if (info._show_range_stat)
    {
      if (info._line >= info._max_line)
	return; // or we would overflow

      wcolor_set(info._w,info._col_norm,NULL);
      wmove(info._w,info._line,0);

      wmove(info._w,info._line,10+20);

      if (_range_hit[0] && _range_hit[1])
	{
	  display_range_bins(info,&_stat_range,0,NUM_WATCH_BINS/2);
	  wcolor_set(info._w,info._col_norm,NULL);
	  waddstr(info._w,":");
	  display_range_bins(info,&_stat_range,1,NUM_WATCH_BINS/2);
	}
      else if (_range_hit[0])
	display_range_bins(info,&_stat_range,0,NUM_WATCH_BINS);
      else if (_range_hit[1])
	display_range_bins(info,&_stat_range,1,NUM_WATCH_BINS);



      info._line++;
    }
}


void watcher_present_channel::collect_raw(uint raw,uint type,
					  watcher_event_info *watch_info)
{
  uint value = (raw & _valmask);
  uint other = (raw & ~(_rangemark | _valmask));

  if (!other &&
      (value < _min ||
       value > _max))
    {
      return;
    }

  _data[type]++;
  if (_container)
    _container->_has[type] = true;
}

void watcher_present_channel::display(watcher_display_info& info)
{
  char buf[256];

  uint sum = 0;
  uint max_part = 0;
  int  max_type = -1;

  for (int t = 0; t < NUM_WATCH_TYPES; t++)
    {
      uint d = _data[t];

      sum += d;

      if (d > max_part)
	{
	  max_part = d;
	  max_type = t;
	}
    }

  if (max_type != -1)
    {
      wcolor_set(info._w,info._col_data[max_type],NULL);
      sprintf (buf,"%c",hexilog2b1(sum));
      waddstr(info._w,buf);
    }
  else
    {
      wcolor_set(info._w,info._col_norm,NULL);
      waddstr(info._w,".");
    }
}

watcher_present_channels::watcher_present_channels()
{
  for (int i = 0; i < NUM_WATCH_PRESENT_CHANNELS; i++)
    _channels[i] = NULL;
  clear();
}

void watcher_present_channels::clear_data()
{
  memset(_has,0,sizeof(_has));
  memset(_data,0,sizeof(_data));

  for (int i = 0; i < NUM_WATCH_PRESENT_CHANNELS; i++)
    if (_channels[i])
      _channels[i]->clear_data();
}

void watcher_present_channels::event_summary()
{
  for (int i = 0; i < NUM_WATCH_TYPES; i++)
    {
      if (_has[i])
	_data[i]++;
      _has[i] = false;
    }
}

void watcher_present_channels::display(watcher_display_info& info/*,
			      const signal_id& id,
			      const char* te*/)
{
  //  if (!info._requests->is_channel_requested(id,te))
  //    return;

  if (info._line >= info._max_line)
    return; // or we would overflow

  wcolor_set(info._w,info._col_norm,NULL);

  wmove(info._w,info._line,0);

  waddstr(info._w,_name.c_str());

  wmove(info._w,info._line,10+20);

  for (int i = 0; i < NUM_WATCH_PRESENT_CHANNELS; i++)
    {
      if (_channels[i])
	{
	  wmove(info._w,info._line,10+20+i+i/4+i/8);
	  _channels[i]->display(info);
	}
    }

  info._line++;
}
