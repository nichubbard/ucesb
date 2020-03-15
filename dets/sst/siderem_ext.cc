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

#include "siderem_ext.hh"

#include "error.hh"

template<int n>
EXT_DECL_DATA_SRC_FCN_ARG(void,EXT_SIDEREM_plane<n>::__unpack,int start,int end)
{
  // number of data-word with full data

  uint32 *dest;

  _stretches[_num_stretches]._start = start;
  _stretches[_num_stretches]._end   = end;
  _stretches[_num_stretches]._data  = dest = _next_data;

  _next_data += end - start;

  int full = (end - start) >> 1; // / 2

  uint32 check = ((start << 12) | ((start + 1) << (12 + 16))) & 0x70007000;

  for ( ; full; --full)
    {
      uint32 buf;

      GET_BUFFER_UINT32(buf);

      if ((buf & 0xf000f000) != check)
	ERROR("Channel markers of data mismatch.  %08x & 0xf000f000 = %08x != %08x",
	      buf,(buf & 0xf000f000),check);

      check = (check + 0x20002000) & 0x70007000;

      *(dest++) = (buf      ) & 0x0fff;
      *(dest++) = (buf >> 16) & 0x0fff;
    }

  if ((end - start) & 1)
    {
      // There is an odd word at the end

      uint32 buf;

      GET_BUFFER_UINT32(buf);

      check = ((check & 0x00007000) | 0x80000000);

      if ((buf & 0xfffff000) != check)
	ERROR("Channel markers of data mismatch.  %08x & 0xfffff000 = %08x != %08x",
	      buf,(buf & 0xfffff000),check);

      *(dest++) = (buf      ) & 0x0fff;
    }

  assert (dest == _next_data);

  // we do not increase the number of stretches until we've seen that
  // all data unpacked properly!

  _num_stretches++;
}
//EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(void,EXT_SIDEREM_plane<640>::__unpack,uint16 ccb_id)
// Fortunately, we need no forced implementation here, since we are using the functions
// below...


void EXT_SIDEREM::__clean()
{
  _x.__clean();
  _y.__clean();
}

#define MAX_STRIPS   1024
#define NUM_STRIPS_X  640

EXT_DECL_DATA_SRC_FCN(void,EXT_SIDEREM::__unpack)
{
  // int n;

  // n is the total number of data-words

  // make sure the number of data-words are available, then we can use
  // a direct pointer.  No! because the byte-swapping etc is done in
  // the data_src

  _x.setup_unpack();
  _y.setup_unpack();

  int last_end = -2;

  while (!__buffer.empty())
    {
      uint32 stretch_header;

      GET_BUFFER_UINT32(stretch_header);

      uint32 start = (stretch_header      ) & 0x03ff; // 0-4095
      uint32 end   = (stretch_header >> 12) & 0x07ff; // 0-4096

      if ((int) start < last_end)
	ERROR("stretch_header start (%d) < previous stretch end (%d)",
	      start,last_end);

      last_end = end;

      if (end <= start)
	ERROR("stretch_header end (%d) <= start (%d)",
	      end,start);

      if (end > 1024)
	ERROR("stretch_header end (%d) > size of si-strip detector (1024)",
	      start);

      // printf ("%d..%d / %d\n",start,end,end-start);

      // ok, the values are consistent, we can now unpack the data

      if (start < NUM_STRIPS_X)
	{
	  if (end > NUM_STRIPS_X)
	    ERROR("stretch overlaps two planes, start (%d) end (%d)",
		  start,end);

	  _x.__unpack(__buffer,start,end);
	}
      else
	{
	  _y.__unpack(__buffer,start-NUM_STRIPS_X,end-NUM_STRIPS_X);
	}
    }
}
EXT_FORCE_IMPL_DATA_SRC_FCN(void,EXT_SIDEREM::__unpack)

