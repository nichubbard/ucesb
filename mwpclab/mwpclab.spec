// -*- C++ -*-

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

#include "spec/midas.spec"

SCALER_DATA(group)
{
  MEMBER(DATA32 data[10] ZERO_SUPPRESS_LIST);

  UINT16 header NOENCODE
    {
      0_7:   group = MATCH(group);
      8_13:  item_count;
      14_15: 0b01;
    }

  UINT16 daq_accepted_trig; // mostly pad

  list(0 <= index < static_cast<uint32>((header.item_count-1)>>1))
    {
      UINT16 count_hi NOENCODE;
      UINT16 count_lo NOENCODE;

      ENCODE(data[index],(value=((static_cast<uint32>(count_hi) << 16) |
				 static_cast<uint32>(count_lo))));

    }
}

TCAL_NO(group)
{
  UINT16 header NOENCODE
    {
      0_7:   group = MATCH(group);
      8_13:  item_count = 5;
      14_15: 0b01;
    }

  UINT16 pad NOENCODE;

  UINT16 starts;
  UINT16 stops;

  UINT16 count_hi NOENCODE;
  UINT16 count_lo NOENCODE;

  MEMBER(uint32 count);

  ENCODE(count,(_=((static_cast<uint32>(count_hi) << 16) |
		   static_cast<uint32>(count_lo))));
}

CLOCK_SAMPLE(group)
{
  UINT16 header NOENCODE
    {
      0_7:   group = MATCH(group);
      8_13:  item_count = 11;
      14_15: 0b01;
    }

  UINT16 flags;

  MEMBER(uint32 before_s);
  MEMBER(uint32 before_frac);

  MEMBER(uint32 after_s);
  MEMBER(uint32 after_frac);

  UINT16 before_s_hi NOENCODE;
  UINT16 before_s_lo NOENCODE;
  UINT16 before_frac_hi NOENCODE;
  UINT16 before_frac_lo NOENCODE;

  UINT16 after_s_hi NOENCODE;
  UINT16 after_s_lo NOENCODE;
  UINT16 after_frac_hi NOENCODE;
  UINT16 after_frac_lo NOENCODE;

  ENCODE(before_s,(_=((static_cast<uint32>(before_s_hi) << 16) |
		      static_cast<uint32>(before_s_lo))));
  ENCODE(before_frac,(_=((static_cast<uint32>(before_frac_hi) << 16) |
			 static_cast<uint32>(before_frac_lo))));
  ENCODE(after_s,(_=((static_cast<uint32>(after_s_hi) << 16) |
		     static_cast<uint32>(after_s_lo))));
  ENCODE(after_frac,(_=((static_cast<uint32>(after_frac_hi) << 16) |
			static_cast<uint32>(after_frac_lo))));

  MEMBER(uint32 count);

  UINT16 count_hi NOENCODE;
  UINT16 count_lo NOENCODE;

  ENCODE(count,(_=((static_cast<uint32>(count_hi) << 16) |
		   static_cast<uint32>(count_lo))));
}

SIMPLE_DATA12_16(group)
{
  MEMBER(DATA12 data[16] ZERO_SUPPRESS);

  UINT16 entry NOENCODE
    {
      // 0_13: address;
      0_7:   group = MATCH(group);
      8_13:  item;
      14_15: 0b00;
    }

  UINT16 value NOENCODE
    {
      0_11:  value;
    }

  ENCODE(data[entry.item],(value=value.value));
}

SIMPLE_DATA_ITEM(group,item)
{
  UINT16 entry NOENCODE
    {
      // 0_13: address;
      0_7:   group = MATCH(group);
      8_13:  item  = MATCH(item);
      14_15: 0b00;
    }

  UINT16 value;
}

SUBEVENT(EV_EVENT)
{
  select several
    {
      tdc = SIMPLE_DATA12_16(group=16);
      qdc = SIMPLE_DATA12_16(group=17);

      trig = SIMPLE_DATA_ITEM(group=255,item=1);

      tcal = TCAL_NO(group=253);

      clock = CLOCK_SAMPLE(group=254);

      scaler = SCALER_DATA(group=252);
    }
}


EVENT
{
  ev = EV_EVENT();

}

SIGNAL(MWPC_X_TL, ev.tdc.data[0],DATA12);
SIGNAL(MWPC_X_TR, ev.tdc.data[1],DATA12);
SIGNAL(MWPC_Y_TU, ev.tdc.data[2],DATA12);
SIGNAL(MWPC_Y_TD, ev.tdc.data[3],DATA12);
SIGNAL(MWPC_X_EO, ev.qdc.data[0],DATA12);
SIGNAL(MWPC_X_EE, ev.qdc.data[1],DATA12);
SIGNAL(MWPC_Y_EO, ev.qdc.data[2],DATA12);
SIGNAL(MWPC_Y_EE, ev.qdc.data[3],DATA12);

SIGNAL(SCIN_T,    ev.tdc.data[8],DATA12);
SIGNAL(SCIN_E,    ev.qdc.data[8],DATA12);

