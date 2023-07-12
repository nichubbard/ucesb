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

#include "structures.hh"

#include "user.hh"

#include "multi_chunk_fcn.hh"

int iss_lmd_user_function_multi(unpack_event *event)
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

  uint32 multi_events = event->vme.header.multi_events;
  uint32 adctdc_counter0 = event->vme.header.multi_adctdc_counter0;

  map_multi_events(event->vme.multi_mdpp,
		   adctdc_counter0,
		   multi_events);

  return multi_events;
}
