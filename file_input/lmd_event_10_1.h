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

#ifndef __LMD_EVENT_10_1_H__
#define __LMD_EVENT_10_1_H__

#include "swapping.hh"
#include "typedef.hh"

/*  ================= GSI VME Event header =======================  */

typedef struct
{
  sint32  l_dlen;    /*  Data length + 4 in words(16) */
  sint16  i_type;
  sint16  i_subtype;
} lmd_event_header_little_endian;

typedef struct
{
  sint32  l_dlen;    /*  Data length + 4 in words(16) */
  sint16  i_subtype;
  sint16  i_type;
} lmd_event_header_big_endian;

typedef lmd_event_header_big_endian         lmd_event_header_network_order;
#define lmd_event_header_host               HOST_ENDIAN_TYPE(lmd_event_header)

/* ------------------------------------------------------------------ */

typedef struct
{
  sint16  i_dummy;   /*  Not used yet */
  sint16  i_trigger; /*  Trigger number */
  sint32  l_count;   /*  Current event number */
} lmd_event_info_little_endian;

typedef struct
{
  sint16  i_trigger; /*  Trigger number */
  sint16  i_dummy;   /*  Not used yet */
  sint32  l_count;   /*  Current event number */
} lmd_event_info_big_endian;

typedef lmd_event_info_big_endian           lmd_event_info_network_order;
typedef HOST_ENDIAN_TYPE(lmd_event_info)    lmd_event_info_host;

/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */

#define LMD_EVENT_10_1_TYPE      10
#define LMD_EVENT_10_1_SUBTYPE   1
#define LMD_EVENT_10_1_X_TYPE      10
#define LMD_EVENT_10_1_X_SUBTYPE   2

/* ------------------------------------------------------------------ */

#endif// __LMD_EVENT_10_1_H__
