/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2022  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#ifndef __TDCPM_SERIALIZE_UTIL_H__
#define __TDCPM_SERIALIZE_UTIL_H__

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

/**************************************************************************/

typedef struct tdcpm_serialize_info_t
{
  uint32_t  *_buf;
  ptrdiff_t  _offset;
  size_t     _alloc;
  ptrdiff_t  _reserved;
  
} tdcpm_serialize_info;

uint32_t *tdcpm_serialize_alloc_words(tdcpm_serialize_info *ser,
				      size_t words);

void tdcpm_serialize_commit_words(tdcpm_serialize_info *ser, uint32_t *dest);

typedef union tdcpm_pun_double_uint64_t
{
  double   _dbl;
  uint64_t _u64;
} tdcpm_pun_double_uint64;

#define TDCPM_SERIALIZE_UINT32(dest, value) do { \
    *(dest++) = (uint32_t) (value);		 \
    /* printf("%s: %" PRIu32 "\n",#value, value); */ \
  } while (0)

#define TDCPM_SERIALIZE_UINT64(dest, value) do { \
    uint64_t u64 = (value);			 \
    uint32_t __hi = (uint32_t) (u64 >> 32);      \
    uint32_t __lo = (uint32_t) u64;              \
    TDCPM_SERIALIZE_UINT32(dest, __hi);	         \
    TDCPM_SERIALIZE_UINT32(dest, __lo);	         \
  } while (0)

#define TDCPM_SERIALIZE_DOUBLE(dest, value) do { \
    tdcpm_pun_double_uint64 pun;		 \
    pun._dbl = (value);				 \
    TDCPM_SERIALIZE_UINT64(dest, pun._u64);	 \
  } while (0)

#define TDCPM_SERIALIZE_CUROFF(ser, dest) \
  ((dest) - (ser)->_buf)

#define TDCPM_SERIALIZE_OFF_PTR(ser, offset) \
  ((ser)->_buf + offset)

/**************************************************************************/

typedef struct tdcpm_deserialize_info_t
{
  uint32_t  *_cur;
  uint32_t  *_end;
} tdcpm_deserialize_info;

#define TDCPM_DESERIALIZE_UINT32(deser, value) do { \
    value = (*((deser)->_cur++));                   \
    /* printf("%s: %" PRIu32 "\n",#value, value); */ \
  } while (0)

#define TDCPM_DESERIALIZE_UINT64(deser, value) do { \
    uint32_t __hi, __lo;			    \
    TDCPM_DESERIALIZE_UINT32(deser, __hi);          \
    TDCPM_DESERIALIZE_UINT32(deser, __lo);          \
    value = (((uint64_t) __hi) << 32) | __lo;	    \
  } while (0)

#define TDCPM_DESERIALIZE_DOUBLE(deser, value) do { \
    tdcpm_pun_double_uint64 pun;		    \
    TDCPM_DESERIALIZE_UINT64(deser, pun._u64);	    \
    value = pun._dbl;				    \
  } while (0)

/**************************************************************************/

#endif/*__TDCPM_SERIALIZE_UTIL_H__*/
