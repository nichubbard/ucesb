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

/* From s_bufhe.h
 */

#ifndef __LMD_STRUCT__LMD_BUF_HEADER_H__
#define __LMD_STRUCT__LMD_BUF_HEADER_H__

#include "lmd_types.h"

/* Signed and unsigned has the same representation in a register.
 * Be (respecifying) the items as unsigned, we avoid having to check
 * for negatives all over the place.  It is enough with checking
 * that the values are not insanely large.
 */

/********************************************************************/
/* GOOSY buffer header. */

/* (0x8000 - sizeof(s_bufhe))/2
 * Above this buffer size (as given by l_dlen) l_free[2] is used to
 * give the number of used words.
 */
#define LMD_BUF_HEADER_MAX_IUSED_DLEN 16360

typedef struct
{
  /*  0 */ uint32_t  l_dlen;      /* Length of data field in words */
  /*  4 */ uint16_t  i_type;
  /*    */ uint16_t  i_subtype;
  /*  8 */ uint16_t  i_used;      /* Used length of data field in words */
  /*    */ uint8_t   h_end;       /* Fragment at beginning of buffer */
  /*    */ uint8_t   h_begin;     /* Fragment at end of buffer */
  /* 12 */ uint32_t  l_buf;       /* Current buffer number */
  /* 16 */ uint32_t  l_evt;       /* Number of fragments */
  /* 20 */ uint32_t  l_current_i; /* Index, temporarily used */
  /* 24 */ uint32_t  l_time[2];
  /* 32 */ uint32_t  l_free[4];
  /* 48 */
} s_bufhe_little_endian;

typedef struct
{
  /*  0 */ uint32_t  l_dlen;      /* Length of data field in words */
  /*  4 */ uint16_t  i_subtype;
  /*    */ uint16_t  i_type;
  /*  8 */ uint8_t   h_begin;     /* Fragment at end of buffer */
  /*    */ uint8_t   h_end;       /* Fragment at beginning of buffer */
  /*    */ uint16_t  i_used;      /* Used length of data field in words */
  /* 12 */ uint32_t  l_buf;       /* Current buffer number */
  /* 16 */ uint32_t  l_evt;       /* Number of fragments */
  /* 20 */ uint32_t  l_current_i; /* Index, temporarily used */
  /* 24 */ uint32_t  l_time[2];
  /* 32 */ uint32_t  l_free[4];
  /* 48 */
} s_bufhe_big_endian;

typedef s_bufhe_big_endian         s_bufhe_network_order;
#define s_bufhe_host               HOST_ENDIAN_TYPE(s_bufhe)

/********************************************************************/

#define LMD_BUF_HEADER_10_1_TYPE                  10  /* fixed size */
#define LMD_BUF_HEADER_10_1_SUBTYPE               1

#define LMD_BUF_HEADER_100_1_TYPE                 100 /* variable size */
#define LMD_BUF_HEADER_100_1_SUBTYPE              1

#define LMD_BUF_HEADER_HAS_STICKY_TYPE            0x4b59
#define LMD_BUF_HEADER_HAS_STICKY_SUBTYPE         0x5354

#define LMD_BUF_HEADER_HAS_STICKY_VARSZ_TYPE      0x4bb3
#define LMD_BUF_HEADER_HAS_STICKY_VARSZ_SUBTYPE   0x5354

/********************************************************************/

#endif/*__LMD_STRUCT__LMD_BUF_HEADER_H__*/
