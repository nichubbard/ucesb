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

#ifndef __BITSONE_HH__
#define __BITSONE_HH__

#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include "endian.hh"

/* A class to store a certain number of bits, that may be zero (no
 * content) and one (content).  An iterator function is provided to
 * get the indices of bits which are one in an easy and fast way.
 */

// Use the largest machine handleable _unsigned_ integer on the
// machine for storing the bits

#define BITSONE_CONTAINER_TYPE    unsigned long

#define BITSONE_CONTAINER_BITS    (sizeof(BITSONE_CONTAINER_TYPE)*8)
#define BITSONE_CONTAINERS        ((n+BITSONE_CONTAINER_BITS-1)/BITSONE_CONTAINER_BITS)

struct bitsone_iterator
{
public:
  bitsone_iterator() { init(); }

public:
  BITSONE_CONTAINER_TYPE _left;
  size_t _index;
  size_t _i;

public:
  void init()
  {
    _left = 0;
    _i = 0;
    // The following is just to make the compiler happy,
    // it is not needed, as _left = 0 will force it to
    // be set before used by bitsone::next
    _index = 0;
  }
};

template<int n>
class bitsone
{
public:

public:
  BITSONE_CONTAINER_TYPE _bits[BITSONE_CONTAINERS];

public:
  void clear()
  {
    memset(_bits,0,sizeof(_bits));
  }

  void set(unsigned int i)
  {
    assert(i < n);
    _bits[i / BITSONE_CONTAINER_BITS] |= ((BITSONE_CONTAINER_TYPE) 1) << (i % BITSONE_CONTAINER_BITS);
  }

  bool get(unsigned int i) const
  {
    assert(i < n);
    return (_bits[i / BITSONE_CONTAINER_BITS] >> (i % BITSONE_CONTAINER_BITS)) & 1;
  }

  bool get_set(unsigned int i)
  {
    assert(i < n);

    size_t                 shift =        i % BITSONE_CONTAINER_BITS;
    BITSONE_CONTAINER_TYPE *addr = &_bits[i / BITSONE_CONTAINER_BITS];

    BITSONE_CONTAINER_TYPE  prev = *addr;

    *addr |= ((BITSONE_CONTAINER_TYPE) 1) << ((BITSONE_CONTAINER_TYPE) shift);

    // printf ("%3d %16x %16p %16p\n",i,_bits[0],addr,&_bits[0]);

    return (prev >> shift) & 1;
  }

  void get_set_ptr(unsigned int i,uint32 **addr,uint32 *mask)
  {
    // Figure out at what address (and bit) one need to read/modify to
    // see if we're on for one index.

    // Even in case machine is 64 bits, we'll only use a 32 bit
    // bitmask (and modified pointer accordingly, to save storage
    // space for the mask) (The places where we'll set the masks will
    // (hopefully) be sufficiently far away from where we later read
    // the memory back, such that the processor won't be slower due to
    // the size mismatch)

#if __BYTE_ORDER == __BIG_ENDIAN
    // On a big endian machine, the bits are stored 'in order' in memory,
    // just calculate as usual, but with 32 bit pointer size
#define BITSONE_FLIP_MASK 0
#endif
#if __BYTE_ORDER == __LITTLE_ENDIAN
    // On little endian, the bits above the type size for uint32 up to the
    // machine size gets flipped (0->1,1->0) (0->3,1->2,2->1,3->0)

    // FLIP_MASK will be e.g. 0x0001 for BITSONE_CONTAINER_TYPE 64 bits,
    // and would be 0x0003 for BITSONE_CONTAINER_TYPE 128 bits
#define BITSONE_FLIP_MASK ((sizeof(BITSONE_CONTAINER_TYPE)/sizeof(uint32))-1)
#endif

    *mask = 1 << (i % (sizeof(uint32)*8));
    *addr = ((uint32*) _bits) + ((i / (sizeof(uint32)*8)) ^ BITSONE_FLIP_MASK);
    /*
    printf ("Ptr: %16p: %16x   %16p %08x\n",
	    &_bits[i / BITSONE_CONTAINER_BITS],1 << (i % BITSONE_CONTAINER_BITS),
	    addr,mask);
    */
  }

public:
  ssize_t next(bitsone_iterator &iter) const
  {
    //printf ("> %16x %8d\n",iter._left,iter._i);
    if (!iter._left)
      {
	// see if there are any further

	for ( ; ; )
	  {
	    if (iter._i >= BITSONE_CONTAINERS)
	      return -1;

	    iter._left = _bits[iter._i];
	    //printf ("%16x %16p\n",iter._left,&_bits[iter._i]);
	    //fflush(stdout);
	    if (iter._left)
	      break;
	    iter._i++;
	  }
	iter._index = iter._i * (BITSONE_CONTAINER_BITS);
	iter._i++;
      }

    // more bits with this base

    while (!(iter._left & 0xff))
      {
	iter._left >>= 8;
	iter._index += 8;
      }
    if (!(iter._left & 0xf))
      {
	iter._left >>= 4;
	iter._index += 4;
      }
    if (!(iter._left & 0x3))
      {
	iter._left >>= 2;
	iter._index += 2;
      }
    if (!(iter._left & 0x1))
      {
	iter._left >>= 1;
	iter._index += 1;
      }

    iter._left >>= 1;     // shift us away
    return (ssize_t) iter._index++;
  }
};

#endif//__BITSONE_HH__

