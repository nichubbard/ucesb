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

VME_M2J_MATACQ32(mod,ch)
{
  MEMBER(DATA16 data[2560] NO_INDEX_LIST);

  UINT32 header
    {
      0_15:  data_length = RANGE(0,(2560+4)/2);
      16_19: ch = MATCH(ch);
      20_23: mod = MATCH(mod);
      24_31: id = 0xdf;
    }

  UINT16 trig_rec;
  UINT16 first_sample;
  UINT16 vernier;
  UINT16 reset_baseline;

  list(0<=index<(static_cast<uint32>(header.data_length)-2)*2)
    {
      UINT16 value;

      ENCODE(data APPEND_LIST,(value=value));
    }
}

external EXTERNAL_DATA32(length);

VME_M2J_MATACQ32_PACK_EXT(mod,ch)
{
  UINT32 header
    {
      0_15:  data_length = RANGE(0,(2560+4)/2);
      16_19: ch = MATCH(ch);
      20_23: mod = MATCH(mod);
      24_31: id = 0xde;
    }

  UINT16 trig_rec;
  UINT16 first_sample;
  UINT16 vernier;
  UINT16 reset_baseline;

  external data32 =
    EXTERNAL_DATA32(length=(static_cast<uint32>(header.data_length)-2));
}
