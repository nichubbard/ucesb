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

#include "spec/spec.spec"

// Decode using generated unpacker.  Not used, using external
// instead

RICH_MODULE(module)
{
  UINT32 header
    {
      0:     count_low;
      1_7:   modified_count;  // modified as word_count is value _plus_ 2
      8_11:  dummy;           // should be 0, but sometimes garbage
      12_15: data_source;     // == 0xa ?
      16_23: event_number;
      24_26: module_low;      // 0b001; // low bits of module number, not
                              // fixed when not data_valid
      27_30: module;          //  = CHECK(module);
      31:    data_valid;
    };

  if (header.data_valid)
    {
      list (0 <= index < (header.modified_count << 1) + header.count_low + 2)
	{
	  UINT16 data NOENCODE
	    {
	      0_7:  value;
	      8_15: channel;
	    };
	}

      if (header.count_low)
	{
	  UINT16 data_skip;
	}
    }
}

SUBEVENT(RICH_SUBEVENT)
{
  UINT32 dummy_header NOENCODE
    {
      0_31: 0x4655434b;
    }

  mod[0] = RICH_MODULE(module=0);
  mod[1] = RICH_MODULE(module=1);
  mod[2] = RICH_MODULE(module=2);
  mod[3] = RICH_MODULE(module=3);
  mod[4] = RICH_MODULE(module=4);
  mod[5] = RICH_MODULE(module=5);
  mod[6] = RICH_MODULE(module=6);
  mod[7] = RICH_MODULE(module=7);
  mod[8] = RICH_MODULE(module=8);
  mod[9] = RICH_MODULE(module=9);
  mod[10] = RICH_MODULE(module=10);
  mod[11] = RICH_MODULE(module=11);
  mod[12] = RICH_MODULE(module=12);
  mod[13] = RICH_MODULE(module=13);
  mod[14] = RICH_MODULE(module=14);
  mod[15] = RICH_MODULE(module=15);

  UINT32 dummy_footer NOENCODE
    {
      0_31: 0x53484954;
    }
}

// To use an external unpacker (this is what is in use)

external EXT_HIRICH();

SUBEVENT(EXT_HIRICH_SUBEVENT)
{
  UINT32 dummy_header NOENCODE
    {
      0_31: 0x4655434b;
    }

  external rich = EXT_HIRICH();

  UINT32 dummy_footer NOENCODE
    {
      0_31: 0x53484954;
    }
}

// To use an (semi-)external unpacker, all unpacking done in user function

external EXTERNAL_DATA32();

SUBEVENT(EXTERNAL_DATA32_RICH_SUBEVENT)
{
  external data = EXTERNAL_DATA32();
}

EVENT
{
  // rich = EXTERNAL_DATA32_RICH_SUBEVENT(type=249,subtype=3,control=9);

  rich = EXT_HIRICH_SUBEVENT(type=249,subtype=3,control=9);

  // rich = RICH_SUBEVENT(type=249,subtype=3,control=9);

  ignore_unknown_subevent;

}

// SIGNAL(SST1_1_E,sst.sst1.data[0][0],DATA12);

