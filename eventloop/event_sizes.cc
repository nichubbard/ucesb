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

#include "event_sizes.hh"
#include "colourtext.hh"

#include <limits.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>



#ifdef USE_LMD_INPUT

void event_sizes::init()
{
  memset(ev_size,0,sizeof(ev_size));
}

void show_sizes(const char *prename,
		const char *name,
		const event_size *es_event,
		const set_subevent_size &subev_size)
{
  uint64_t triggers = es_event->_sum;

  printf ("\n%s%s%s%s: %*s",
	  prename,CT_OUT(BOLD),name,CT_OUT(NORM),
	  (int)(20-strlen(prename)-strlen(name)),"");

  printf ("    (%s%6" PRIu64 "%s %s%6" PRIu64 "%s)"
	  "        %s%8.1f%s%6.1f (%10.0f)\n",
	  CT_OUT(GREEN),es_event->_min,CT_OUT(DEF_COL),
	  CT_OUT(RED),es_event->_max,CT_OUT(DEF_COL),
	  CT_OUT(MAGENTA),
	  (double) es_event->_sum_x / (double) triggers,
	  CT_OUT(DEF_COL),
	  (double) es_event->_sum_header / (double) triggers,
	  (double) es_event->_sum);

  for (set_subevent_size::const_iterator iter = subev_size.begin();
       iter != subev_size.end(); ++iter)
    {
      const subevent_ident &ident = iter->first;
      const event_size *es_info = iter->second;

      printf ("%s%5d%s/%s%5d%s %s%5d%s:%s%3d%s:%s%3d%s "
	      "(%s%6" PRIu64 "%s %s%6" PRIu64 "%s)"
	      "%s%8.1f%s%s%8.1f%s%s%6.1f%s (%s%10.0f%s)\n",
	      CT_OUT(BOLD),(uint16) ident._info._header.i_type,CT_OUT(NORM),
	      CT_OUT(BOLD),(uint16) ident._info._header.i_subtype,CT_OUT(NORM),
	      CT_OUT(BOLD),(uint16) ident._info.i_procid,CT_OUT(NORM),
	      CT_OUT(BOLD),(uint8) ident._info.h_subcrate,CT_OUT(NORM),
	      CT_OUT(BOLD),(uint8) ident._info.h_control,CT_OUT(NORM),
	      CT_OUT(BOLD_GREEN),es_info->_min,CT_OUT(NORM_DEF_COL),
	      CT_OUT(BOLD_RED),es_info->_max,CT_OUT(NORM_DEF_COL),
	      CT_OUT(BOLD_BLUE),
	      (double) es_info->_sum_x / (double) es_info->_sum,
	      CT_OUT(NORM_DEF_COL),
	      CT_OUT(BOLD_MAGENTA),
	      (double) es_info->_sum_x / (double) triggers,
	      CT_OUT(NORM_DEF_COL),
	      CT_OUT(BOLD),
	      (double) es_info->_sum_header / (double) triggers,
	      CT_OUT(NORM),
	      CT_OUT(BOLD),
	      (double) es_info->_sum,
	      CT_OUT(NORM));
    }
}

void add(event_size *es_total,const event_size *es_info)
{
  if (es_info->_min < es_total->_min)
    es_total->_min = es_info->_min;
  if (es_info->_max > es_total->_max)
    es_total->_max = es_info->_max;
  es_total->_sum_x      += es_info->_sum_x;
  es_total->_sum_header += es_info->_sum_header;
  es_total->_sum        += es_info->_sum;
}

void event_sizes::show()
{
  set_subevent_size subev_total;
  event_size ev_total;

  memset(&ev_total,0,sizeof(ev_total));

  printf (" %stype/stype%s  %sprid%s %scrt%s %sctrl%s    %smin%s    %smax%s  %savg(ev)%s %savg(tot)%s %shead%s  %soccurances%s\n",
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM),
	  CT_OUT(UL),CT_OUT(NORM));

  for (int trig = 0; trig < 17; trig++)
    {
      uint64_t triggers = ev_size[trig]._sum;

      if (!triggers)
	continue;

      char trigno[16];

      sprintf (trigno,"%d",trig);

      show_sizes("trig ",trig == 16 ? "other" : trigno,
		 &ev_size[trig],_subev_size[trig]);

      add(&ev_total,&ev_size[trig]);

      for (set_subevent_size::const_iterator iter = _subev_size[trig].begin();
	   iter != _subev_size[trig].end(); ++iter)
	{
	  const subevent_ident &ident = iter->first;
	  const event_size *es_info = iter->second;

	  set_subevent_size::iterator iter_total = subev_total.find(ident);

	  event_size *es_total = NULL;

	  if (iter_total == subev_total.end())
	    {
	      es_total = new event_size;

	      memset(es_total,0,sizeof(es_total[0]));
	      es_total->_min = INT_MAX;

	      subev_total.insert(set_subevent_size::value_type(ident,
							       es_total));
	    }
	  else
	    es_total = iter_total->second;

	  add(es_total,es_info);
	}
    }

  show_sizes("trig ","all",&ev_total,subev_total);

  if (ev_size[17]._sum)
    {
      show_sizes("","sticky events",
		 &ev_size[17],_subev_size[17]);
    }
}

void event_sizes::account(FILE_INPUT_EVENT *event)
{
  int trigger = event->_header._info.i_trigger;

  if (event->_status & LMD_EVENT_IS_STICKY)
    trigger = 17;
  else if (trigger > 15)
    trigger = 16; // others

  uint64_t headers = 0;
  uint64_t payload = 0;

  for (int subevent = 0; subevent < event->_nsubevents; subevent++)
    {
      lmd_subevent *subevent_info = &event->_subevents[subevent];

      subevent_ident *ident = (subevent_ident *) &(subevent_info->_header);

      set_subevent_size::iterator iter = _subev_size[trigger].find(*ident);

      event_size *es_info = NULL;

      if (iter == _subev_size[trigger].end())
	{
	  subevent_ident ident_ins;

	  ident_ins._info = subevent_info->_header;
	  ident_ins._info._header.l_dlen = 0;

	  es_info = new event_size;

	  memset(es_info,0,sizeof(es_info[0]));
	  es_info->_min = INT_MAX;

	  _subev_size[trigger].insert(set_subevent_size::value_type(ident_ins,
								    es_info));
	}
      else
	es_info = iter->second;

      uint64_t sub_size;

      if ((event->_status & LMD_EVENT_IS_STICKY) &&
	  subevent_info->_header._header.l_dlen ==
	  LMD_SUBEVENT_STICKY_DLEN_REVOKE)
	sub_size = 0;
      else
	sub_size = (uint64_t)
	  SUBEVENT_DATA_LENGTH_FROM_DLEN(subevent_info->_header._header.l_dlen);

      if (sub_size < es_info->_min)
	es_info->_min = sub_size;
      if (sub_size > es_info->_max)
	es_info->_max = sub_size;
      es_info->_sum_x += sub_size;
      es_info->_sum_header += sizeof(lmd_subevent_10_1_host);
      es_info->_sum++;

      payload += sub_size;
      headers += sizeof(lmd_subevent_10_1_host);
    }

  headers += sizeof(lmd_event_10_1_host);

  event_size *es_info = &ev_size[trigger];

  if (payload < es_info->_min)
    es_info->_min = payload;
  if (payload > es_info->_max)
    es_info->_max = payload;
  es_info->_sum_x += payload;
  es_info->_sum_header += headers;
  es_info->_sum++;
}


#endif//USE_LMD_INPUT
