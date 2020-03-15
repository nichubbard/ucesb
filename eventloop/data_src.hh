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

#ifndef __DATA_SRC_HH__
#define __DATA_SRC_HH__

#include "typedef.hh"

#include "endian.hh"
#include "swapping.hh"

#include <stdlib.h>

#define SWAPPING_BSWAP_16(x) (swapping ? bswap_16(x) : (x))
#define SWAPPING_BSWAP_32(x) (swapping ? bswap_32(x) : (x))

// If the compiler has any wits at all, all branching on swapping and
// scramble below will never happen, since they are compile time
// constants

#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)

template<int swapping,int scramble>
class __data_src
{
public:
  __data_src(__data_src &src)
  {
    _data     = src._data;
    _end      = src._end;

    _this_read = 0;
    _prev_read = 0;
  }

  __data_src(void *data,void *end)
  {
    _data     = (char*) data;
    _end      = (char*) end;

    _this_read = 0;
    _prev_read = 0;
  }

public:
  int is_swapping()  { return swapping; }
  int is_scrambled() { return scramble; }

public:
  char *_data;
  char *_end;

  signed char _this_read;
  signed char _prev_read;

public:
  bool get_uint8(uint8 *dest)
  {
    uint8 *src = (uint8 *) (_data);

    _data += sizeof(uint8);

    if (_data > _end)
      return false;
    if (scramble)
      *dest = *((uint8 *) (((size_t) src) ^ 3));
    else
      *dest = *src;
    //printf("u8: %04x\n",*dest);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = 1;

    return true;
  }

  bool get_uint16(uint16 *dest)
  {
    uint16 *src = (uint16 *) (_data);

    _data += sizeof(uint16);

    if (_data > _end || (((size_t) _data) & 1)) // outside or unaligned
      return false;
    if (scramble)
      *dest = SWAPPING_BSWAP_16(*((uint16 *) (((size_t) src) ^ 2)));
    else
      *dest = SWAPPING_BSWAP_16(*src);
    //printf("u16: %04x\n",*dest);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = 2;

    return true;
  }

  bool get_uint32(uint32 *dest)
  {
    uint32 *src = (uint32 *) (_data);

    _data += sizeof(uint32);

    if (_data > _end || (((size_t) _data) & 3)) // outside or unaligned
      return false;
    *dest = SWAPPING_BSWAP_32(*src);
    //printf("u32: %08x\n",*dest);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = 4;

    return true;
  }

  bool get_uint64(uint64 *dest)
  {
    // we read 64 bit data as two items of 32 bits,
    // only 32-bit alignment is required

    uint32 *src = (uint32 *) (_data);

    _data += 2 * sizeof(uint32);

    if (_data > _end || (((size_t) _data) & 3)) // outside or unaligned
      return false;

    // BE stored data will have HI:LO
    // LE stored data will have LO:HI

    uint32 first  = SWAPPING_BSWAP_32(*src);
    uint32 second = SWAPPING_BSWAP_32(*(src+1));

    if ((__BYTE_ORDER == __BIG_ENDIAN) ^ !swapping) // BE
      *dest = (((uint64) first) << 32) | second;
    else
      *dest = (((uint64) second) << 32) | first;
    //printf("u32: %08x\n",*dest);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = 8;

    return true;
  }

  void peeking()
  {
    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -128;
  }

  bool peek_uint8(uint8 *dest)
  {
    uint8 *src = (uint8 *) (_data);

    if ((_data + sizeof(uint8)) > _end)
      return false;
    if (scramble)
      *dest = *((uint8 *) (((size_t) src) ^ 3));
    else
      *dest = *src;
    //printf("u8: %04x\n",*dest);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -1;

    return true;
  }

  bool peek_uint16(uint16 *dest)
  {
    uint16 *src = (uint16 *) (_data);

    if ((_data + sizeof(uint16)) > _end || (((size_t) _data) & 1)) // outside or unaligned
      return false;
    if (scramble)
      *dest = SWAPPING_BSWAP_16(*((uint16 *) (((size_t) src) ^ 2)));
    else
      *dest = SWAPPING_BSWAP_16(*src);
    //printf("u16: %04x\n",*dest);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -2;

    return true;
  }

  bool peek_uint32(uint32 *dest)
  {
    uint32 *src = (uint32 *) (_data);

    if ((_data + sizeof(uint32)) > _end || (((size_t) _data) & 3)) // outside or unaligned
      return false;
    *dest = SWAPPING_BSWAP_32(*src);
    //printf("u32: %08x\n",*dest);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -4;

    return true;
  }

  bool peek_uint64(uint64 *dest)
  {
    uint32 *src = (uint32 *) (_data);

    if ((_data + 2*sizeof(uint32)) > _end || (((size_t) _data) & 3)) // outside or unaligned
      return false;

    uint32 first  = SWAPPING_BSWAP_32(*src);
    uint32 second = SWAPPING_BSWAP_32(*(src+1));

    if ((__BYTE_ORDER == __BIG_ENDIAN) ^ !swapping) // BE
      *dest = (((uint64) first) << 32) | second;
    else
      *dest = (((uint64) second) << 32) | first;
    //printf("u32: %08x\n",*dest);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -8;

    return true;
  }

public:
  bool empty() { return _data == _end; }

  size_t left() { return (size_t) (_end - _data); }
  void advance(size_t offset) { _data += offset; }
  void reverse(size_t offset) { _data -= offset; }

};

#endif

#if defined(USE_PAX_INPUT) || defined(USE_GENF_INPUT) || defined(USE_EBYE_INPUT_16)

template<int swapping>
class __data_src
{
public:
  __data_src(__data_src &src)
  {
    _data     = src._data;
    _end      = src._end;

    _this_read = 0;
    _prev_read = 0;
  }

  __data_src(uint16 *data,uint16 *end)
  {
    _data     = data;
    _end      = end;

    _this_read = 0;
    _prev_read = 0;
  }

public:
  int is_swapping()  { return swapping; }

public:
  uint16 *_data;
  uint16 *_end;

  signed char _this_read;
  signed char _prev_read;

public:
  bool get_uint16(uint16 *dest)
  {
    uint16 *src = _data;

    _data++;

    if (_data > _end) // outside or unaligned
      {
	// this should really not be here, but the compiler otherwise
	// complains about possible uninitialized usage.  Should be
	// fixable with some function attributes?
	*dest=0;
	return false;
      }
    *dest = SWAPPING_BSWAP_16(*src);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = 2;

    return true;
  }

  void peeking()
  {
    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -128;
  }

  bool peek_uint16(uint16 *dest)
  {
    uint16 *src = _data;

    if ((_data + 1) > _end) // outside or unaligned
      return false;
    *dest = SWAPPING_BSWAP_16(*src);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -2;

    return true;
  }

public:
  bool empty() { return _data == _end; }

  size_t left() { return (char*) _end - (char*) _data; }
  void advance(size_t offset) { _data += offset / sizeof (uint16); }
  void reverse(size_t offset) { _data -= offset / sizeof (uint16); }
};

#endif

#ifdef USE_EBYE_INPUT_32

template<int swapping>
class __data_src
{
public:
  __data_src(__data_src &src)
  {
    _data     = src._data;
    _end      = src._end;

    _this_read = 0;
    _prev_read = 0;
  }

  __data_src(uint32 *data,uint32 *end)
  {
    _data     = data;
    _end      = end;

    _this_read = 0;
    _prev_read = 0;
  }

public:
  int is_swapping()  { return swapping; }

public:
  uint32 *_data;
  uint32 *_end;

  signed char _this_read;
  signed char _prev_read;

public:
  bool get_uint32(uint32 *dest)
  {
    uint32 *src = _data;

    _data++;

    if (_data > _end) // outside or unaligned
      return false;
    *dest = SWAPPING_BSWAP_32(*src);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = 4;

    return true;
  }

  void peeking()
  {
    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -128;
  }

  bool peek_uint32(uint32 *dest)
  {
    uint32 *src = _data;

    if ((_data + 1) > _end) // outside or unaligned
      return false;
    *dest = SWAPPING_BSWAP_32(*src);

    if (_this_read >= 0)
      _prev_read = _this_read;
    _this_read = -4;

    return true;
  }

public:
  bool empty() { return _data == _end; }

  size_t left() { return (char*) _end - (char*) _data; }
  void advance(size_t offset) { _data += offset / sizeof (uint32); }
  void reverse(size_t offset) { _data -= offset / sizeof (uint32); }

};

#endif


#endif//__DATA_SRC_HH__

