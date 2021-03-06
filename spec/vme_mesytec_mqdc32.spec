// -*- C++ -*-

/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Michael Munch  <mm.munk@gmail.com> and Jesper Halkjaer <gedemagt@gmail.com>
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

VME_MESYTEC_MQDC32(geom)
{
  MEMBER(DATA12_OVERFLOW data[32] ZERO_SUPPRESS);

  MARK_COUNT(start);
  UINT32 header NOENCODE
  {
    0_11:  word_number; // includes end_of_event
    12_14: 0b000;
    16_23: geom = MATCH(geom);
    24_29: 0b000000;
    30_31: 0b01;
  }

  several UINT32 ch_data NOENCODE
    {
      0_11:  value;
      15:    outofrange;
      16_20: channel;
      21_29: 0b000100000;
      30_31: 0b00;

      ENCODE(data[channel], (value = value, overflow = outofrange));
    }

  optional UINT32 ch_data NOENCODE
  {
    0_31: 0x00000000;
  }
   

  UINT32 end_of_event 
  {
    0_29:  counter;
    30_31: 0b11;
  }

  MARK_COUNT(end);
  CHECK_COUNT(header.word_number,start,end,-4,4);
}
