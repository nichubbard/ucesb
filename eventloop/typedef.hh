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

#ifndef __TYPEDEF_HH__
#define __TYPEDEF_HH__

#include <stdint.h>

typedef uint64_t  uint64;
typedef uint32_t  uint32;
typedef uint16_t  uint16;
typedef uint8_t   uint8;

typedef int64_t   sint64;
typedef int32_t   sint32;
typedef int16_t   sint16;
typedef int8_t    sint8;

#ifdef __LAND02_CODE__
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
#endif

/* TODO: this really should be in its own file! */

#include <inttypes.h>

/* For really old compilers.
 *
 * Hmmm, this is actually really unhealthy, as arguments will be
 * consumed wrongly.
 */

#ifndef PRId8
#define PRId8   "d"
#define PRIi8   "i"
#define PRIo8   "o"
#define PRIu8   "u"
#define PRIx8   "x"
#define PRIX8   "X"
#endif

#ifndef PRId16
#define PRId16  "d"
#define PRIi16  "i"
#define PRIo16  "o"
#define PRIu16  "u"
#define PRIx16  "x"
#define PRIX16  "X"
#endif

#ifndef PRId32
#define PRId32  "d"
#define PRIi32  "i"
#define PRIo32  "o"
#define PRIu32  "u"
#define PRIx32  "x"
#define PRIX32  "X"
#endif

#ifndef PRId64
#define PRId64  "lld"
#define PRIi64  "lli"
#define PRIo64  "llo"
#define PRIu64  "llu"
#define PRIx64  "llx"
#define PRIX64  "llX"
#endif

#endif//__TYPEDEF_HH__
