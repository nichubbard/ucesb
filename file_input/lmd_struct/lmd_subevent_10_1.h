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

#ifndef __LMD_STRUCT__LMD_SUBEVENT_10_1_H__
#define __LMD_STRUCT__LMD_SUBEVENT_10_1_H__

#include "lmd_event_10_1.h"

/********************************************************************/
/* GSI VME subevent header */

typedef struct
{
  lmd_event_header_little_endian _header;

  uint16_t  i_procid;     /* Processor ID [as loaded from VAX] */
  uint8_t   h_subcrate;   /* Subcrate number */
  uint8_t   h_control;    /* Processor type code */
} lmd_subevent_10_1_little_endian;

typedef struct
{
  lmd_event_header_big_endian _header;

  uint8_t   h_control;    /* Processor type code */
  uint8_t   h_subcrate;   /* Subcrate number */
  uint16_t  i_procid;     /* Processor ID [as loaded from VAX] */
} lmd_subevent_10_1_big_endian;

typedef lmd_subevent_10_1_big_endian        lmd_subevent_10_1_network_order;
#define lmd_subevent_10_1_host              HOST_ENDIAN_TYPE(lmd_subevent_10_1)

/********************************************************************/

#endif/*__LMD_STRUCT__LMD_SUBEVENT_10_1_H__*/
