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

#ifndef __UTIL_HH__
#define __UTIL_HH__

#include "typedef.hh"

#ifndef UNUSED
#define UNUSED(x) ((void) x)
#endif

#define stringify_r(x) #x
#define stringify(x)   stringify_r(x)

#define GCC_VERSION (__GNUC__ * 10000 \
		     + __GNUC_MINOR__ * 100 \
		     + __GNUC_PATCHLEVEL__)

inline unsigned int firstbit(unsigned int x)
{
  if (!x)
    return (unsigned int) -1;

  unsigned int l = 0;

  while (!(x & 0x3f))
    {
      x >>= 6;
      l += 6;
    }

  while (!(x & 1))
    {
      x >>= 1;
      l++;
    }
  return l;
}

inline unsigned int numbits(unsigned int x)
{
  unsigned int n = 0;

  while (x)
    {
      if (x & 1)
        n++;
      x >>= 1;
    }
  return n;
}

inline unsigned int ilog2(unsigned int x)
{
  unsigned int l = 0;

  while (x & 0xffffff00)
    {
      x >>= 8;
      l += 8;
    }

  while (x >>= 1)
    l++;
  return l;
}

inline char hexilog2(unsigned int x)
{
  if (!x)
    return '.';

  unsigned int il = ilog2(x);

  if (il < 10)
    return (char) (il + '0');
  return (char) (il - 10 + 'A');
}

inline char hexilog2b1(unsigned int x)
{
  if (!x)
    return '.';

  unsigned int il = ilog2(x);

  il++;

  if (il < 10)
    return (char) (il + '0');
  return (char) (il - 10 + 'A');
}

/// Return the number of items in an array.
#define countof(array) (sizeof(array)/sizeof((array)[0]))

template<typename T>
int compare_values(const void* a,const void* b)
{
  if (*((const T*) a) < *((const T*) b))
    return -1;
  return *((const T*) a) > *((const T*) b);
}

#endif//__UTIL_HH__
