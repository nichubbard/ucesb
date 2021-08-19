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
#include "lmd_event.hh"
#include <stdint.h>

#ifdef UNPACKER_IS_xtst_toggle
#include "multi_chunk_fcn.hh"
#endif

#ifdef UNPACKER_IS_xtst

#define NUM_CRATES  16

static uint32_t _sticky_active[NUM_CRATES];
static uint32_t _sticky_mark[NUM_CRATES];

static uint32_t _sticky_last_ev[NUM_CRATES][32];

uint64_t _spurious_revokes = 0;
uint64_t _same_sticky_ev_seen = 0;

void init_function()
{
  memset(_sticky_active, 0, sizeof (_sticky_active));
  memset(_sticky_mark, 0, sizeof (_sticky_mark));
  memset(_sticky_last_ev, 0, sizeof (_sticky_last_ev));
}

void sticky_subevent_user_function(unpack_sticky_event *sticky_event,
				   const void *header,
				   const char *start, const char *end,
				   bool swapping)
{
  lmd_subevent_10_1_host *sev_header = (lmd_subevent_10_1_host *) header;
  /*
  printf ("%5d %5d %2d (%d)\n",
	  sev_header->_header.i_type,
	  sev_header->_header.i_subtype,
	  sev_header->h_control,
	  sev_header->_header.l_dlen);
  */
  if (sev_header->_header.i_type == 10 &&
      sev_header->_header.i_subtype == 1)
    return; // Timestamp subevent (not real sticky)
  if (sev_header->_header.i_type == 0x0cae &&
      sev_header->_header.i_subtype == 0x0caf)
    return; // Correlation sticky subevent (not handled here)

  int crate = sev_header->h_subcrate;
  int isev = sev_header->h_control;
  
  if (sev_header->_header.l_dlen == LMD_SUBEVENT_STICKY_DLEN_REVOKE)
    {
      if (!(_sticky_active[crate] & (1 << isev)))
	{
	  /* Revoking markers may be spurious, so just count them, do
	   * not inform.
	   */
	  _spurious_revokes++;
	  /*
	  WARNING("Event %d: "
		  "Non-active sticky subevent (%d) revoked.",
		  event->event_no,
		  isev);
	  */
	}
      _sticky_active[crate] &= ~(1 << isev);
    }
  else
    {
      _sticky_active[crate] |= 1 << isev;

      if (end > start)
	{
	  const uint32_t *p = (const uint32_t *) start;

	  uint32_t payload = *p;

	  _sticky_mark[crate] = (_sticky_mark[crate] & ~(1 << isev)) |
	    ((payload & 1) << isev);
	}
    }

  if (isev < 32)
    {
      if (_sticky_last_ev[crate][isev] == sticky_event->event_no)
	{
	  /* It may happen during replay that we get the same
	   * sticky event multiple times.
	   */
	  _same_sticky_ev_seen++;
	  /*
	  WARNING("Event %d: crate %d: "
		  "Sticky subevent (%d) seen for same event no.",
		  sticky_event->event_no, crate,
		  isev);
	  */
	}      
      _sticky_last_ev[crate][isev] = sticky_event->event_no;
    }
}

int user_function(unpack_event *event)
{
  for (int crate = 0; crate < NUM_CRATES; crate++)
    {
      if (!event->regress[crate].sticky_active.separator.u32)
	continue;

      if (_sticky_active[crate] != event->regress[crate].sticky_active.active)
	{
	  for (int isev = 0; isev < 8; isev++)
	    WARNING("Event %d: crate %d: "
		    "Sticky subevent (%d) from event: %d.  (%s)",
		    event->event_no, crate,
		    isev,
		    _sticky_last_ev[crate][isev],
		    _sticky_active[crate] & (1 << isev) ? "active" : "inactive");

	  ERROR("Event %d: crate %d: "
		"Wrong sticky events active (active: %08x, event says: %08x).",
		event->event_no, crate,
		_sticky_active[crate],
		event->regress[crate].sticky_active.active);
	}

      if ((_sticky_mark[crate] & _sticky_active[crate]) !=
	  (event->regress[crate].sticky_active.mark & _sticky_active[crate]))
	{
	  ERROR("Event %d: crate %d: "
		"Wrong sticky events payload (mark: %08x, event says: %08x).",
		event->event_no, crate,
		_sticky_mark[crate] & _sticky_active[crate],
		event->regress[crate].sticky_active.active &
		_sticky_active[crate]);
	}
      /*
      printf ("%d: %02x %02x\n",
	      crate, _sticky_active[crate], _sticky_mark[crate]);
      */
    }

  return 1;
}

void raw_user_function(unpack_event *unpack_event,
		       raw_event *raw_event)
{
  for (int j = 0; j < 16; j++)
    {
      for (int i = 1; i <= 4; i++)
	{
	  WR_STAMP *wrstamp = &(unpack_event->regress[j].wr[i]);

	  if (wrstamp->srcid.value == i)
	    {
	      raw_event->tstamp_srcid = i;
	      raw_event->tstamp_lo =
		(wrstamp->t2.value << 16) | wrstamp->t1.value;
	      raw_event->tstamp_hi =
		(wrstamp->t4.value << 16) | wrstamp->t3.value;
	    }
	}
    }

  /*
  fprintf (stderr,
	   "TS: %08x %08x:%08x\n",
	   raw_event->tstamp_srcid,
	   raw_event->tstamp_hi,
	   raw_event->tstamp_lo);
  */
}


#endif//UNPACKER_IS_xtst

#ifdef UNPACKER_IS_xtst_toggle

/*
  file_input/empty_file --lmd --events=200 \
    --max-multi=20 --toggle --caen-v775=6 --trloii-mtrig \
  | xtst/xtst_toggle --file=-
 */

external_toggle_info<2> _toggle_info;

int user_function_multi(unpack_event *event)
{
  uint32 multi_events;
  uint32 adctdc_counter0;
  uint32 ntgl[2];

  multi_events = event->vme.header.multi_events;
  adctdc_counter0 = event->vme.header.multi_adctdc_counter0;
  ntgl[0] = event->vme.header.toggle.ntgl1;
  ntgl[1] = event->vme.header.toggle.ntgl2;

  map_continuous_multi_events(event->vme.multitrig.multi_events,
                              0/*scaler_counter0*/,
                              multi_events);

  build_external_toggle_map(event->vme.multitrig.multi_events,
			    &_toggle_info,
			    0, multi_events);

  for (int i = 0; i < 2; i++)
    if (_toggle_info._maps[i]._cnts != ntgl[i])
      {
	ERROR("Mismatching number of toggle #%d "
	      "events (%d) vs. toggle marks (%d).",
	      i+1,
	      ntgl[i], _toggle_info._maps[i]._cnts);
      }

  /* _toggle_info.dump(); */

  for (unsigned int i = 0; i < countof(event->vme.multi_v775mod); i++)
    map_multi_events(event->vme.multi_v775mod[i],
                     adctdc_counter0,
                     multi_events);

  for (unsigned int i = 0; i < countof(event->vme.multi_v775_tgl1); i++)
    map_multi_events(event->vme.multi_v775_tgl1[i],
		     &_toggle_info._maps[0],
                     adctdc_counter0,
                     multi_events);

  for (unsigned int i = 0; i < countof(event->vme.multi_v775_tgl2); i++)
    map_multi_events(event->vme.multi_v775_tgl2[i],
		     &_toggle_info._maps[1],
                     adctdc_counter0,
                     multi_events);

  /* printf ("%d multi-events\n", multi_events); */

  return multi_events;
}

uint32 ROLLING_TRLO_EVENT_TRIGGER::get_event_counter() const
{
  return status.count;
}

bool ROLLING_TRLO_EVENT_TRIGGER::good_event_counter_offset(uint32 expect) const
{
  return status.count <= 0xf;
}

uint32 ROLLING_TRLO_EVENT_TRIGGER::get_external_toggle_mask() const
{
  uint32 toggle_mask = (status.tpat >> 22) & 0x03;

  return toggle_mask;
}

#endif//UNPACKER_IS_xtst_toggle
