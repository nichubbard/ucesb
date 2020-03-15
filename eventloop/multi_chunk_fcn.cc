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

// Helper functions needed for some of the modules
// when using the multi-event capabilities

// There is no harm in compiling this, since unless they are used, the
// linker will throw it out.  The modules are part of the commen spec/
// directory, and therefore we'll have the classes defined

// If one included spec/spec.hh .  Try to fix such that one can avoid
// that.  (Eg siderem01)

#if !defined(USE_PAX_INPUT) && !defined(USE_GENF_INPUT) && \
  !defined(USE_EBYE_INPUT)

////////////////////////////////////////////////////////////////////////

#ifdef DECLARED_UNPACK_VME_CAEN_V830
uint32 VME_CAEN_V830::get_event_counter() const
{
  return header.event_number;
}

uint32 VME_CAEN_V830::get_event_counter_offset(uint32 start) const
{
  return (header.event_number - start) & 0x0000ffff;
}
#endif

////////////////////////////////////////////////////////////////////////

#ifdef DECLARED_UNPACK_VME_CAEN_V775
uint32 VME_CAEN_V775::get_event_counter() const
{
  return eob.event_number;
}

uint32 VME_CAEN_V775::get_event_counter_offset(uint32 start) const
{
  return (eob.event_number - start) & 0x00ffffff;
}

bool VME_CAEN_V775::good_event_counter_offset(uint32 expect) const
{
  return 1;
}
#endif

////////////////////////////////////////////////////////////////////////

#ifdef DECLARED_UNPACK_VME_CAEN_V1290
uint32 VME_CAEN_V1290::get_event_counter() const
{
  return header.event_number;
}

uint32 VME_CAEN_V1290::get_event_counter_offset(uint32 start) const
{
  return (header.event_number - start) & 0x000fffff;
}
#endif

////////////////////////////////////////////////////////////////////////

#ifdef DECLARED_UNPACK_VME_CAEN_V1190
uint32 VME_CAEN_V1190::get_event_counter() const
{
  return header.event_number;
}

uint32 VME_CAEN_V1190::get_event_counter_offset(uint32 start) const
{
  return (header.event_number - start) & 0x000fffff;
}
#endif

////////////////////////////////////////////////////////////////////////

#ifdef DECLARED_UNPACK_VME_CAEN_V1290
uint32 VME_CAEN_V1290_SHORT::get_event_counter() const
{
  return header.event_number;
}

uint32 VME_CAEN_V1290_SHORT::get_event_counter_offset(uint32 start) const
{
  return (header.event_number - start) & 0x000fffff;
}
#endif

////////////////////////////////////////////////////////////////////////

#ifdef DECLARED_UNPACK_VME_CAEN_V1190
uint32 VME_CAEN_V1190_SHORT::get_event_counter() const
{
  return header.event_number;
}

uint32 VME_CAEN_V1190_SHORT::get_event_counter_offset(uint32 start) const
{
  return (header.event_number - start) & 0x000fffff;
}
#endif

////////////////////////////////////////////////////////////////////////

#ifdef DECLARED_UNPACK_VME_MESYTEC_MADC32
uint32 VME_MESYTEC_MADC32::get_event_counter() const
{
  return end_of_event.counter;
}

uint32 VME_MESYTEC_MADC32::get_event_counter_offset(uint32 start) const
{
  return (end_of_event.counter - start) & 0x3FFFFFFF;
}

bool VME_MESYTEC_MADC32::good_event_counter_offset(uint32 expect) const
{
  return 1;
}
#endif


#ifdef DECLARED_UNPACK_VME_MESYTEC_MQDC32
uint32 VME_MESYTEC_MQDC32::get_event_counter() const
{
  return end_of_event.counter;
}

uint32 VME_MESYTEC_MQDC32::get_event_counter_offset(uint32 start) const
{
  return (end_of_event.counter - start) & 0x3FFFFFFF;
}

bool VME_MESYTEC_MQDC32::good_event_counter_offset(uint32 expect) const
{
  return 1;
}
#endif

#ifdef DECLARED_UNPACK_VME_MESYTEC_MTDC32
uint32 VME_MESYTEC_MTDC32::get_event_counter() const
{
  return end_of_event.counter;
} 

uint32 VME_MESYTEC_MTDC32::get_event_counter_offset(uint32 start) const
{
  return (end_of_event.counter - start) & 0x3FFFFFFF;
} 
bool VME_MESYTEC_MTDC32::good_event_counter_offset(uint32 expect) const
{
  return 1;
} 
#endif

////////////////////////////////////////////////////////////////////////

// The following does not have files in spec/ - should rather be
// along with whatever code actually use them...

#ifdef DECLARED_UNPACK_TRLO_EVENT_TRIGGER
uint32 TRLO_EVENT_TRIGGER::get_event_counter() const
{
  return status.count;
}

bool TRLO_EVENT_TRIGGER::good_event_counter_offset(uint32 expect) const
{
  return ((status.count ^ expect) & 0xf) == 0;
}
#endif

////////////////////////////////////////////////////////////////////////

// Same applies to this one.

#ifdef DECLARED_UNPACK_VME_STRUCK_SIS3316_CHANNEL_DATA
uint32 VME_STRUCK_SIS3316_CHANNEL_DATA::get_event_counter() const
{
  return 0;
}

uint32 VME_STRUCK_SIS3316_CHANNEL_DATA::get_event_counter_offset(uint32 start) const
{
  return 0;
}

bool VME_STRUCK_SIS3316_CHANNEL_DATA::good_event_counter_offset(uint32 expect) const
{
  return 1;
}
#endif

////////////////////////////////////////////////////////////////////////

#endif
