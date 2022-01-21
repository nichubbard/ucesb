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

#ifndef __LMD_STRUCT__LMD_EVENT_10_1_H__
#define __LMD_STRUCT__LMD_EVENT_10_1_H__

#include "lmd_types.h"

/* See comment in lwroc_lmd_buf_header regarding signed/unsigned. */

/********************************************************************/
/* GOOSY VME event header. */

typedef struct
{
  uint32_t  l_dlen;       /* Data length in words(16), excl. this header. */
  uint16_t  i_type;
  uint16_t  i_subtype;
} lmd_event_header_little_endian;

typedef struct
{
  uint32_t  l_dlen;       /* Data length in words(16), excl. this header. */
  uint16_t  i_subtype;
  uint16_t  i_type;
} lmd_event_header_big_endian;

typedef lmd_event_header_big_endian         lmd_event_header_network_order;
#define lmd_event_header_host               HOST_ENDIAN_TYPE(lmd_event_header)

/********************************************************************/

typedef struct
{
  uint16_t  i_dummy;      /* Not used yet */
  uint16_t  i_trigger;    /* Trigger number */
  uint32_t  l_count;      /* Current event number */
} lmd_event_info_little_endian;

typedef struct
{
  uint16_t  i_trigger;    /* Trigger number */
  uint16_t  i_dummy;      /* Not used yet */
  uint32_t  l_count;      /* Current event number */
} lmd_event_info_big_endian;

typedef lmd_event_info_big_endian           lmd_event_info_network_order;
typedef HOST_ENDIAN_TYPE(lmd_event_info)    lmd_event_info_host;

/********************************************************************/

typedef struct
{
  lmd_event_header_big_endian    _header;
  lmd_event_info_big_endian      _info;
} lmd_event_10_1_big_endian;

typedef struct
{
  lmd_event_header_little_endian _header;
  lmd_event_info_little_endian   _info;
} lmd_event_10_1_little_endian;

typedef lmd_event_10_1_big_endian           lmd_event_10_1_network_order;
#define lmd_event_10_1_host                 HOST_ENDIAN_TYPE(lmd_event_10_1)

/********************************************************************/

#define LMD_EVENT_10_1_TYPE        10
#define LMD_EVENT_10_1_SUBTYPE     1

#define LMD_EVENT_STICKY_TYPE      0x4b59
#define LMD_EVENT_STICKY_SUBTYPE   0x5354

#define LMD_SUBEVENT_STICKY_TSTAMP_TYPE      10
#define LMD_SUBEVENT_STICKY_TSTAMP_SUBTYPE   1

#define LMD_SUBEVENT_STICKY_DLEN_REVOKE  ((uint32_t) -1)

/********************************************************************/

#endif/*__LMD_STRUCT__LMD_EVENT_10_1_H__*/
