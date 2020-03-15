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

#include "sid_genf_external.hh"

#include "error.hh"

/*---------------------------------------------------------------------------*/

EXT_LADDER::EXT_LADDER()
{
  /*
  _scramble_buffer = NULL;
  _scramble_length = 0;

  _dest_buffer = (uint32 *) malloc (sizeof(uint32) * LADDER_REWRITE_MAX_DWORDS);
  _dest_length = LADDER_REWRITE_MAX_DWORDS;

  memset(&trc,0,sizeof(trc));
  */
}

EXT_LADDER::~EXT_LADDER()
{
  /*
  free (_scramble_buffer);
  free (_dest_buffer);
  */
}

void EXT_LADDER::__clean()
{
  /*
  data.__clean();
  */
}

EXT_DECL_DATA_SRC_FCN_ARG(void,EXT_LADDER::__unpack,uint16 id)
{
  uint16 size;
  uint16 info;

  GET_BUFFER_UINT16(size);

  /* Since the mode info etc is at the end, we must peek down there.
   */

  {
    __data_src_t peek_buffer(__buffer);

    peek_buffer.advance(sizeof(uint16) * (size - 1));

    if (!peek_buffer.peek_uint16(&info))
      ERROR("Internal error."); // match managed to do the access... :-(
  }
  /*
  if ((info & 0xfff0) != 0x81e0 || size < 1025)
    {
      printf ("UU Size: %04x = %d  ",size,size);
      printf ("UU Info: %04x (%02x)\n",info,(info & 0x001f));
    }
  */
  for (int i = 0; i < size-1; i++)
    {
      uint16 data;

      GET_BUFFER_UINT16(data);

    }

  // Finally, get past the info item

  GET_BUFFER_UINT16(info);


  /*
  uint16 *data_begin = (uint16*) __buffer._data;

  uint16 *src_begin  = (uint16*) __buffer._data;
  uint16 *src_end    = (uint16*) __buffer._end;

  if (__buffer.left() < sizeof(uint16) * 6)
    ERROR("cros3 data too small, no space for ccb header and footer");

  uint16 ch1;
  uint16 ch2;
  uint16 ch3;
  uint16 ch4;

  GET_BUFFER_UINT16(ch1);
  GET_BUFFER_UINT16(ch2);
  GET_BUFFER_UINT16(ch3);
  GET_BUFFER_UINT16(ch4);

  check.orig.csr_den = (ch1 & 0x00ff) | (ch2 & 0x00ff) << 8;
  check.orig.csr_ini = (ch3 & 0x00ff) | (ch4 & 0x00ff) << 8;
  check.orig.ccb_id = ccb_id;

  // The header has been peeked into.  Now figure out what kind of
  // data to expect

  uint16 ad1;
  uint16 ad2;

  GET_BUFFER_UINT16(ad1);
  GET_BUFFER_UINT16(ad2);

  uint32 mode = 0;
  */
}
EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(void,EXT_LADDER::__unpack,uint16 id);

EXT_DECL_DATA_SRC_FCN_ARG(bool,EXT_LADDER::__match,uint16 id)
{
  uint16 size;

  if (__buffer.left() < sizeof(uint16))
    return false; // no space for header

  GET_BUFFER_UINT16(size);

  // printf ("Size: %04x = %d\n",size,size);

  if (__buffer.left() < sizeof(uint16) * size)
    return false; // no space data

  __buffer.advance(sizeof(uint16) * (size - 1));

  uint16 info;

  GET_BUFFER_UINT16(info);

  //   printf ("Info: %04x (%02x)\n",info,(info & 0x001f));

  if ((info & 0x001f) == id)
    return true;

  return false;
}
EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(bool,EXT_LADDER::__match,uint16 id);

/*---------------------------------------------------------------------------*/
