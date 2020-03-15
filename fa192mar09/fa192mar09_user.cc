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

#include "multi_chunk_fcn.hh"

#include <curses.h> // needed for the COLOR_ below

#define NUM_SCALERS_IN_USE 1

const char *_scaler_names[2*32] = { // max 10 letters, please
  /* 0  0 */ "TR_REQUEST",
  /* 0  1 */ "TR_ACCEPT",
  /* 0  2 */ "",
  /* 0  3 */ "",
  /* 0  4 */ "",
  /* 0  5 */ "",
  /* 0  6 */ "",
  /* 0  7 */ "",
  /* 0  8 */ "",
  /* 0  9 */ "",
  /* 0 10 */ "",
  /* 0 11 */ "",
  /* 0 12 */ "",
  /* 0 13 */ "",
  /* 0 14 */ "",
  /* 0 15 */ "",
  /* 0 16 */ "",
  /* 0 17 */ "",
  /* 0 18 */ "",
  /* 0 19 */ "",
  /* 0 20 */ "",
  /* 0 21 */ "",
  /* 0 22 */ "",
  /* 0 23 */ "",
  /* 0 24 */ "",
  /* 0 25 */ "",
  /* 0 26 */ "",
  /* 0 27 */ "",
  /* 0 28 */ "",
  /* 0 29 */ "",
  /* 0 30 */ "",
  /* 0 31 */ "",
#if 0
  /* 1  0 */ "",
  /* 1  1 */ "",
  /* 1  2 */ "",
  /* 1  3 */ "",
  /* 1  4 */ "",
  /* 1  5 */ "",
  /* 1  6 */ "",
  /* 1  7 */ "",
  /* 1  8 */ "",
  /* 1  9 */ "",
  /* 1 10 */ "",
  /* 1 11 */ "",
  /* 1 12 */ "",
  /* 1 13 */ "",
  /* 1 14 */ "",
  /* 1 15 */ "",
  /* 1 16 */ "",
  /* 1 17 */ "",
  /* 1 18 */ "",
  /* 1 19 */ "",
  /* 1 20 */ "",
  /* 1 21 */ "",
  /* 1 22 */ "",
  /* 1 23 */ "",
  /* 1 24 */ "",
  /* 1 25 */ "",
  /* 1 26 */ "",
  /* 1 27 */ "",
  /* 1 28 */ "",
  /* 1 29 */ "",
  /* 1 30 */ "",
  /* 1 31 */ "",
#endif
};

bool _show_scalers = false;

watcher_type_info fa192mar09_watch_types[NUM_WATCH_TYPES] =
  {
    { COLOR_GREEN,   "Physics" },
    { COLOR_RED,     "Periodic" },
  };

void fa192mar09_watcher_event_info(watcher_event_info *info,
				   unpack_event *event)
{
  info->_type = FA192MAR09_WATCH_TYPE_PHYSICS;

  if (event->trigger == 2)
    {
      info->_type = FA192MAR09_WATCH_TYPE_PERIODIC;
      info->_display |= WATCHER_DISPLAY_SPILL_EOS;
    }


  // Since we have a time stamp (on some events), we'd rather use that
  // than the buffer timestamp, since the buffer timestamp is the time
  // of packaging, and not of event occuring

  // clean it for events not having it
  info->_info &= ~WATCHER_DISPLAY_INFO_TIME;
  if (event->vme.header.failure.has_time_stamp)
    {
      info->_time = event->vme.header.time_stamp;
      info->_info |= WATCHER_DISPLAY_INFO_TIME;
    }
}



// int multi_events = 0;
// int multi_subevents = 0;

int fa192mar09_user_function_multi(unpack_event *event)
{
  // Our job is to
  //
  // * make sure the event is intact (i.e. the data can safely be unpacked)
  //
  // * make the mapping to subevents for all the modules

#define EVENT_FAILURE_ERROR(failmode,message) \
  { if (event->vme.header.failure.failmode) ERROR(message); }
#define EVENT_FAILURE_WARNING(failmode,message) \
  { if (event->vme.header.failure.failmode) WARNING(message); }

  EVENT_FAILURE_ERROR(fail_general,               "Event with DAQ general failure.");
  EVENT_FAILURE_ERROR(fail_data_corrupt,          "Event with DAQ data corruption.");
  EVENT_FAILURE_ERROR(fail_data_missing,          "Event with DAQ data missing.");
  EVENT_FAILURE_ERROR(fail_data_too_much,         "Event with DAQ data too much.");
  EVENT_FAILURE_ERROR(fail_event_counter_mismatch,"Event with DAQ event counter mismatch.");
  EVENT_FAILURE_ERROR(fail_readout_error_driver,  "Event with DAQ readout driver error.");
  EVENT_FAILURE_WARNING(fail_unexpected_trigger,  "Event with DAQ unexpected trigger.");

  // event->vme.header.multi_events
  // event->vme.header.multi_events
  // event->vme.header.multi_events
  // event->vme.header.multi_events

  // Since the code has been built for handling multi-event,
  // we must do the mapping of the modules!

  uint32 scaler_counter0;
  uint32 adctdc_counter0;
  uint32 multi_events;

  // For this experiment, multi-event mode is always enabled
  if (!event->vme.header.failure.has_multi_event)
    ERROR("Multi-event flag not set.");

  // When running in multi-event mode, we must
  // know the start value for the event counters for the modules

  if (!event->vme.header.failure.has_multi_scaler_counter0)
    ERROR("Event counter for scaler at start unknown.");

  // Loop over all the modules which are multi-event, mapping
  // their event using their event counters

  if (!event->vme.header.failure.has_multi_adctdc_counter0)
    ERROR("Event counter for adc/tdc at start unknown.");

  scaler_counter0 = event->vme.header.multi_scaler_counter0;
  adctdc_counter0 = event->vme.header.multi_adctdc_counter0;

  multi_events = event->vme.header.multi_events;

  for (unsigned int i = 0; i < countof(event->vme.multi_scaler); i++)
    map_multi_events(event->vme.multi_scaler[i],
		     scaler_counter0,
		     multi_events);

  for (unsigned int i = 0; i < countof(event->vme.multi_adc); i++)
    map_multi_events(event->vme.multi_adc[i],
		     adctdc_counter0,
		     multi_events);

  for (unsigned int i = 0; i < countof(event->vme.multi_tdc); i++)
    map_multi_events(event->vme.multi_tdc[i],
		     adctdc_counter0,
		     multi_events);

  for (unsigned int i = 0; i < countof(event->vme.multi_mhtdc); i++)
    map_multi_events(event->vme.multi_mhtdc[i],
		     adctdc_counter0,
		     multi_events);
  /*
  for (unsigned int i = 0; i < countof(event->vme.multi_krx); i++)
    map_multi_events(event->vme.multi_krx[i],
		     adctdc_counter0,
		     multi_events);
  */
  return multi_events;
}

void fa192mar09_user_function_raw(unpack_event *event,
				  raw_event    *raw_event
				  MAP_MEMBERS_PARAM)
{
  /* Do the unpacking of the bit-packed sampling ADC data.
   */

  for (int iadc = 0; iadc < 2; iadc++)
    {
      EXTERNAL_DATA32 &d32 = event->vme.sadc_pack[iadc].data32;

      raw_list_ii_zero_suppress<DATA16,DATA16,2560> &raw =
	raw_event->sadcp[iadc];

      // printf ("%d: %d\n",iadc,(int) d32._length);

      if (d32._length)
	{
	  uint16 dest[2560];
	  uint16 prev = 0x1000;

	  // There should be data for us to unpack

#define PACK_BIT_CHUNK  16

	  uint32 *src = d32._data;
	  uint32 *src_end = src + d32._length;

	  uint64 bucket = 0;
	  int avail = 0;

	  for (int i = 0; i < 2560; i += PACK_BIT_CHUNK)
	    {
	      uint64 src_val = 0;
	      // if we got outside stream, then we give 0's (will break out
	      // when dest > dest_end)
	      if (src < src_end)
		src_val = ((uint64) *src) << avail;
	      if (avail < 4)
		{
		  bucket |= src_val;
		  avail += 32;
		  // printf ("[%08x]",*src);
		  src++;
		}
	      int bits = bucket & ((1 << 4) - 1);
	      avail -= 4;
	      bucket >>= 4;

	      // printf ("%2d:",bits);

	      uint32 bit_mask = ((1 << bits) - 1);
	      int bias = -(1 << (bits-1));

	      for (int j = 0; j < PACK_BIT_CHUNK; j++)
		{
		  src_val = 0;
		  if (src < src_end)
		    src_val = ((uint64) *src) << avail;
		  if (avail < bits)
		    {
		      bucket |= src_val;
		      avail += 32;
		      // printf ("[%08x]",*src);
		      src++;
		    }
		  uint16 value = (uint16) (bucket & bit_mask);
		  avail -= bits;
		  bucket >>= bits;

		  // printf (" %2d",bias + value);

		  prev = (uint16) (prev + bias + value);
		  dest[i+j] = prev;
		  raw.append_item().value = prev;
		}
	      // printf ("\n");
	    }
	  // Check that data was good
	  if (avail >= 32)
	    {
	      src--; // we have read one dummy word
	      avail -= 32;
	    }
	  if (avail > 0 && bucket != 1)
	    ERROR("Unpack failure, did not reach end bit exactly...\n");
	  if (src < src_end)
	    ERROR("Unpack failure, read too little...\n");
	  if (src != src_end)
	    ERROR("Unpack failure, read too far...\n");

	  UNUSED(dest);
  	}
    }
}

void fa192mar09_user_function(unpack_event *event,
			      raw_event    *raw_event,
			      cal_event    *cal_event
			      MAP_MEMBERS_PARAM)
{
  if (_show_scalers &&
      event->trigger == 2)
    {
      static uint32 sca_prev[32] =
	{ (uint32) -1, (uint32) -1, (uint32) -1, (uint32) -1,
	  (uint32) -1, (uint32) -1, (uint32) -1, (uint32) -1,
	  (uint32) -1, (uint32) -1, (uint32) -1, (uint32) -1,
	  (uint32) -1, (uint32) -1, (uint32) -1, (uint32) -1,
	  (uint32) -1, (uint32) -1, (uint32) -1, (uint32) -1,
	  (uint32) -1, (uint32) -1, (uint32) -1, (uint32) -1,
	  (uint32) -1, (uint32) -1, (uint32) -1, (uint32) -1,
	  (uint32) -1, (uint32) -1, (uint32) -1, (uint32) -1, };
      uint32 sca_this[32];
      uint32 sca_incr[32];

      if (event->vme.multi_scaler[0]._num_items != 1)
	ERROR("Expected 1 scaler event for trigger 2, got %d (scaler[%d]).",
	      event->vme.multi_scaler[0]._num_items,0);

      for (int i = 0; i < 32; i++)
	sca_this[i] = (uint32) -1;

      bitsone_iterator iter;
      ssize_t ch;

      VME_CAEN_V830 *scaler = &(event->vme.scaler[0]());

      while ((ch = scaler->data._valid.next(iter)) >= 0)
	{
	  sca_this[ch] = scaler->data[ch].value;
	}

      for (int i = 0; i < 32; i++)
	{
	  if (sca_this[i] != (uint32) -1 && sca_prev[i] != (uint32) -1)
	    sca_incr[i] = sca_this[i] - sca_prev[i];
	  else
	    sca_incr[i] = (uint32) -1;

	  sca_prev[i] = sca_this[i];
	}

      uint32 trig_request = sca_incr[0]+1; // +1 for the soft-pulse
      uint32 trig_accept  = sca_incr[1];

      double dead_time = 1 - ((double) trig_accept / trig_request);

      printf (" -- Scalers: ------------------------- %10.2f%% DT -\n",
	      dead_time*100);

      for (int i = 0; i < 16; i++)
	{
	  printf ("%d/%2d:%10s %10d  %10d\n",
		  0,i,_scaler_names[i],
		  sca_this[i],sca_incr[i]);

	}





#if 0
      // Get the trig_request and trig_accept

      uint32 trig_request = -1;
      uint32 trig_accept  = -1;

      double dead_time = 100;
      /*
      if (event->vme.multi_scaler[0]._num_items == 1)
	{
	  VME_CAEN_V830 *scaler = &(event->vme.scaler[0]());

	  if (scaler->data._valid.get(0))
	    trig_request = scaler->data[0].value;
	  if (scaler->data._valid.get(1))
	    trig_accept = scaler->data[1].value;

	  if (trig_request == (uint32) -1 &&
	      trig_accept  == (uint32) -1)
	    {
	      dead_time = 1 - trig_accept / trig_request;
	    }
	}
      */
      printf (" -- Scalers: ------------------------- %10.2f%% DT -\n",
	      dead_time);

      int linebreak = 0;

      for (unsigned int i = 0;
	   i < NUM_SCALERS_IN_USE /*countof(event->vme.multi_scaler)*/; i++)
	{
	  // For trigger two, we expect (exactly) one scaler event

	  if (event->vme.multi_scaler[i]._num_items != 1)
	    ERROR("Expected 1 scaler event for trigger 2, got %d (scaler[%d]).",
		  event->vme.multi_scaler[i]._num_items,i);

	  bitsone_iterator iter;
	  int ch;

	  VME_CAEN_V830 *scaler = &(event->vme.scaler[i]());

	  while ((ch = scaler->data._valid.next(iter)) >= 0)
	    {
	      printf ("%d/%2d:%10s %10d  ",i,ch,"",scaler->data[ch].value);

	      if (++linebreak == 2)
		{
		  printf ("\n");
		  linebreak = 0;
		}
	    }
	}
#endif
    }

}

void fa192mar09_user_init()
{

}

void fa192mar09_user_exit()
{

}

void fa192mar09_usage_command_line_options()
{
  //      "  --option          Explanation.\n"
  printf ("  --scalers         Show scaler data (trigger 2).\n");
}

bool fa192mar09_handle_command_line_option(const char *arg)
{
  // const char *post;

#define MATCH_PREFIX(prefix,post) (strncmp(arg,prefix,strlen(prefix)) == 0 && *(post = arg + strlen(prefix)) != '\0')
#define MATCH_ARG(name) (strcmp(arg,name) == 0)

 if (MATCH_ARG("--scalers")) {
   _show_scalers = true;
   return true;
 }

 return false;
}
