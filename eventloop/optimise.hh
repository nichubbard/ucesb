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

#ifndef __OPTIMISE_HH__
#define __OPTIMISE_HH__


#if defined(__GNUC__) && __GNUC__ >= 3
#define   LIKELY(expr) __builtin_expect(expr,1)
#define UNLIKELY(expr) __builtin_expect(expr,0)
#else
#define   LIKELY(expr) expr
#define UNLIKELY(expr) expr
#endif


// This is not really an optimising construct, it is necessary for
// correct operation when the code in question operates using several
// threads (in SMP mode).  When threading is not enabled (for that
// part of the code), it should not #ifdef'ed away.

// See the linux kernel, include/asm-<arch>/system.h, search for
// mb/wmb/rmb (we are only accessing memory, no devices)

#if defined (__i386__) || defined (__x86_64__)
// Careful here: cpuid bit 26 must be set (sse2, or we have no
// mfence)
// # ifdef HAS_XMM2
#if defined (__x86_64__)
#  define MFENCE asm __volatile__ ("    mfence  \n\t" : : : "memory")
#  define SFENCE asm __volatile__ ("    sfence  \n\t" : : : "memory")
#  define LFENCE asm __volatile__ ("    lfence  \n\t" : : : "memory")
# else
#  define MFENCE asm __volatile__ ("    lock; addl $0,0(%%esp)  \n\t" : : : "memory")
# endif
#endif

#ifdef __powerpc__
# define MFENCE asm __volatile__ ("    eieio  \n\t" : : : "memory")
#endif

#ifdef __arm__
//#define MFENCE asm __volatile__ ("    dmb  \n\t")
# define MFENCE asm __volatile__ ("" : : : "memory")
#endif

#ifdef __sparc__
# define MFENCE asm __volatile__ ("" : : : "memory")
#endif

#ifdef __alpha__
# define MFENCE asm __volatile__ ("    mb    \n\t" : : : "memory")
# define SFENCE asm __volatile__ ("    wmb   \n\t" : : : "memory")
#endif

#ifndef SFENCE
# define SFENCE MFENCE
#endif

#ifndef LFENCE
# define LFENCE MFENCE
#endif

#endif//__OPTIMISE_HH__

