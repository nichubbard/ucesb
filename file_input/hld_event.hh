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

#ifndef __HLD_EVENT_HH__
#define __HLD_EVENT_HH__

#include "endian.hh"

union hld_decoding
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
  struct
  {
    uint32 _type  : 16;
    uint32 _align : 8;  // for subevents: word size
    uint32 _zero  : 8;
  };
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
  struct
  {
    uint32 _zero  : 8;
    uint32 _align : 8;  // for subevents: word size
    uint32 _type  : 16;
  };
#endif
  uint32 u32; // among others, gives endianness
                    // high byte = 00, low != 0
                    // second byte gives subevent alignment shift
                    // 0=8bits,1=16bits,2=32bits align=1<<shift
                    // last two bytes gives decoding type
};

struct hld_event_header
{
  uint32 _size;     // size of event in bytes, including header
  hld_decoding _decoding; // subevent alignment and data decoding type
  uint32 _id;       // type of event
  uint32 _seq_no;   // event counter (use with file_nr)
  uint32 _date;     // date 0x00yymmdd yy in years since 1900
  uint32 _time;     // time 0x00hhmmss
  uint32 _file_no;  // file counter
  uint32 _pad;
};

struct hld_subevent_header
{
  uint32 _size;     // size of event in bytes, including header
  hld_decoding _decoding; // subevent word size and data decoding type
  uint32 _id;       // type of subevent
  uint32 _trig_no;  // trigger number
};


#endif//__HLD_EVENT_HH__
