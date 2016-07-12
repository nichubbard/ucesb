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

/* From s_filhe.h
 * changed the first members to be s_bufhe
 */

#ifndef __LMD_FILE_HEADER_H__
#define __LMD_FILE_HEADER_H__

#include "lmd_buf_header.h"

/* ------------------------------------------------------------------ */

template<int maxlen>
struct s_l_string
{
  uint16 len;            /* Length of string */
  sint8  string[maxlen]; /* String data      */
};

template<int len>
struct s_string
{
  char   string[len];    /* String data      */
};

typedef struct
{
  s_l_string<30> filhe_label;    	/* tape label */
  s_l_string<86> filhe_file;		/* file name */
  s_l_string<30> filhe_user;		/* user name */
  s_string<24>   filhe_time;		/* date and time string (no length specifier) */
  s_l_string<66> filhe_run;		/* run id */
  s_l_string<66> filhe_exp;		/* explanation */
  uint32         filhe_lines;           /* # of comment lines */
  s_l_string<78> s_strings[30];         /* max 30 comment lines */
} s_filhe_extra_network_order;

#define s_filhe_extra_host s_filhe_extra_network_order

/* ------------------------------------------------------------------ */

typedef struct
{
  s_bufhe_little_endian            _buf_header;
  s_filhe_extra_network_order      _file_extra;
} s_filhe_little_endian;

typedef struct
{
  s_bufhe_big_endian               _buf_header;
  s_filhe_extra_network_order      _file_extra;
} s_filhe_big_endian;

typedef s_filhe_big_endian         s_filhe_network_order;
#define s_filhe_host               HOST_ENDIAN_TYPE(s_filhe)

/* ------------------------------------------------------------------ */

#define LMD_FILE_HEADER_10_1_TYPE      2000
#define LMD_FILE_HEADER_10_1_SUBTYPE   1

/* ------------------------------------------------------------------ */

#endif // __LMD_FILE_HEADER_H__
