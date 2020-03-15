// -*- C++ -*-

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

#define VME_CAEN_V792 VME_CAEN_V775
#define VME_CAEN_V785 VME_CAEN_V775

VME_CAEN_V775(geom,
	      crate)
{
  MEMBER(DATA12_OVERFLOW data[32] ZERO_SUPPRESS);

  UINT32 header NOENCODE
    {
      // 0_7: undefined;
      8_13:  count;
      16_23: crate = MATCH(crate);
      24_26: 0b010;
      27_31: geom = MATCH(geom);
    }

  list(0<=index<header.count)
    {
      UINT32 ch_data NOENCODE
	{
	  0_11:  value;

	  12:    overflow;
	  13:    underflow;
	  14:    valid;

	  // 15: undefined;

	  16_20: channel;

	  24_26: 0b000;
	  27_31: geom = CHECK(geom);

	  ENCODE(data[channel],(value=value,overflow=overflow));
	}
    }

  UINT32 eob
    {
      0_23:  event_number;
      24_26: 0b100;
      27_31: geom = CHECK(geom);
      // NOENCODE;
    }
}

