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

#include "external_data.hh"
#include "error.hh"

/*---------------------------------------------------------------------------*/

#if !defined(USE_EBYE_INPUT)

void EXTERNAL_DATA_SKIP::__clean()
{

}

EXT_DECL_DATA_SRC_FCN_ARG(void,EXTERNAL_DATA_SKIP::__unpack,size_t length/*_ARG:,any arguments*/)
{
  // Read all available data (or till limit)

  if (length == (uint32) -1)
    __buffer._data = __buffer._end; // simulate reaching the end
  else
    {
      if (length > __buffer.left())
	ERROR("Attempt to skip (%zd) past end of data (%zd).",
	      length,__buffer.left());
      __buffer.advance(length);
    }
}

EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(void,EXTERNAL_DATA_SKIP::__unpack,size_t length/*_ARG:,any arguments*/);
#endif

/*---------------------------------------------------------------------------*/

EXTERNAL_DATA_INFO::EXTERNAL_DATA_INFO()
{
  _data = NULL;
}

void EXTERNAL_DATA_INFO::__clean()
{
  _length = 0;
  _swapping = false; // useless with length = 0...
}

EXT_DECL_DATA_SRC_FCN(void,EXTERNAL_DATA_INFO::__unpack/*_ARG:,any arguments*/)
{
  _data   = (char*) __buffer._data;
  _length = (size_t) (__buffer._end - __buffer._data);

  _swapping = __buffer.is_swapping();

  // we just want the info about the (remainings) of the subevent, so
  // not moving the source pointer
}

EXT_FORCE_IMPL_DATA_SRC_FCN(void,EXTERNAL_DATA_INFO::__unpack/*_ARG:,any arguments*/);

/*---------------------------------------------------------------------------*/

#if !defined(USE_EBYE_INPUT_32)

EXTERNAL_DATA16::EXTERNAL_DATA16()
{
  _data = NULL;
  _alloc = 0;
}

EXTERNAL_DATA16::~EXTERNAL_DATA16()
{
  free(_data);
}

void EXTERNAL_DATA16::__clean()
{
  _length = 0;
}

EXT_DECL_DATA_SRC_FCN_ARG(void,EXTERNAL_DATA16::__unpack,size_t length/*_ARG:,any arguments*/)
{
  // First allocate memory as necessary

  if (length == (uint32) -1)
    length = __buffer.left();
  else
    {
      length *= sizeof(uint16);
      if (length > __buffer.left())
	ERROR("Attempt to eat16 (%zd) past end of data (%zd).",
	      length,__buffer.left());
    }

  if (_alloc < length)
    {
      size_t needed = length / sizeof (_data[0]);
      uint16 *new_data = (uint16 *) realloc (_data,needed * sizeof (_data[0]));
      if (!new_data)
	ERROR("Memory allocation failure.");
      _data  = new_data;
      _alloc = needed;
    }

  // Read all available data (or till limit)
  // We use the normal read functions (no memcpy), to
  // a) show how to use them
  // b) let the __data_src handle byte-swapping and scrambling

  // This one uses the calculated number of 16-bit words,
  // see the 32-bit version below for a simpler (but slower) way,
  // that polls for the data end

  uint16* dest = _data;
  _length = length / sizeof (_data[0]);

  for (size_t i = _length; i > 0; i--)
    {
      uint16 data;

      GET_BUFFER_UINT16(data);

      *(dest++) = data;
    }
}

EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(void,EXTERNAL_DATA16::__unpack,size_t length/*_ARG:,any arguments*/);
#endif

/*---------------------------------------------------------------------------*/

#if !defined(USE_PAX_INPUT) && !defined(USE_GENF_INPUT) && !defined(USE_EBYE_INPUT_16)

EXTERNAL_DATA32::EXTERNAL_DATA32()
{
  _data = NULL;
  _alloc = 0;
}

EXTERNAL_DATA32::~EXTERNAL_DATA32()
{
  free(_data);
}

void EXTERNAL_DATA32::__clean()
{
  _length = 0;
}

EXT_DECL_DATA_SRC_FCN_ARG(void,EXTERNAL_DATA32::__unpack,size_t length/*_ARG:,any arguments*/)
{
  // First allocate memory as necessary

  if (length == (uint32) -1)
    length = __buffer.left();
  else
    {
      length *= sizeof(uint32);
      if (length > __buffer.left())
	ERROR("Attempt to eat32 (%zd) past end of data (%zd).",
	      length,__buffer.left());
    }

  if (_alloc < __buffer.left())
    {
      size_t needed = __buffer.left() / sizeof (_data[0]);
      uint32 *new_data = (uint32 *) realloc (_data,needed * sizeof (_data[0]));
      if (!new_data)
	ERROR("Memory allocation failure.");
      _data  = new_data;
      _alloc = needed;
    }

  // Read all available data (or till limit)

  uint32* dest = _data;
  _length = length / sizeof (_data[0]);

  for (size_t i = _length; i > 0; i--)
    {
      uint32 data;

      GET_BUFFER_UINT32(data);

      *(dest++) = data;
    }

#if 0
  // Simpler but slower that polls for the end.  Cannot use since
  // we also accept a length as argument.
  uint32* dest = _data;
  _length = 0;

  while (!__buffer.empty())
    {
      uint32 data;

      GET_BUFFER_UINT32(data);

      *(dest++) = data;
      _length++;
    }
#endif
}

EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(void,EXTERNAL_DATA32::__unpack,size_t length/*_ARG:,any arguments*/);
#endif

/*---------------------------------------------------------------------------*/
