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

#ifndef __LMD_STRUCT__LMD_FILE_HEADER_H__
#define __LMD_STRUCT__LMD_FILE_HEADER_H__

#include "lmd_buf_header.h"

/********************************************************************/

#define LMD_FILHE_STRING(name,len)			\
  uint16_t name##_l;					\
  char     name[len]

typedef struct
{
  uint16_t string_l;                /* Length of string */
  char     string[78];              /* String data      */
} s_filhe_comment_string;

typedef struct
{
  LMD_FILHE_STRING(filhe_label,30); /* tape label */
  LMD_FILHE_STRING(filhe_file,86);  /* file name */
  LMD_FILHE_STRING(filhe_user,30);  /* user name */
  char         filhe_time[24];      /* date and time string (no length spec) */ 
  LMD_FILHE_STRING(filhe_run,66);   /* run id */
  LMD_FILHE_STRING(filhe_exp,66);   /* explanation */
  uint32_t       filhe_lines;       /* # of comment lines */
  s_filhe_comment_string s_strings[30];  /* max 30 comment lines */
} s_filhe_extra_network_order;

#define s_filhe_extra_host s_filhe_extra_network_order

/********************************************************************/

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

/********************************************************************/

#define LMD_FILE_HEADER_2000_1_TYPE      2000
#define LMD_FILE_HEADER_2000_1_SUBTYPE   1

/********************************************************************/

#endif/*__LMD_STRUCT__LMD_FILE_HEADER_H__*/
