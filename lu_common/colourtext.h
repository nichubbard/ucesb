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

#ifndef __COLOURTEXT_H__
#define __COLOURTEXT_H__

#include <stddef.h>

/* Since we do not know what background colour the user has in his
 * terminal, we cannot use e.g. yellow.  Assuming the background is
 * either black or white, the following are readable:
 *
 * red, green, (blue), magenta, (cyan)
 */

#define CTR_NONE                0 /* used in markconvbold_output */
#define CTR_WHITE_BG_RED        1
#define CTR_YELLOW_BG_RED       2
#define CTR_RED_BG_GREEN        3
#define CTR_YELLOW_BG_BLUE      4
#define CTR_BLACK_BG_YELLOW     5
#define CTR_BLUE_BG_YELLOW      6
#define CTR_WHITE_BG_MAGENTA    7
#define CTR_RED                 8
#define CTR_GREEN               9
#define CTR_BLUE               10
#define CTR_MAGENTA            11
#define CTR_CYAN               12
#define CTR_WHITE              13 /* not for general use!!! */
#define CTR_BLACK              14 /* not for general use!!! */
#define CTR_NUM_COLORS         15 /* end of colors */

#define CTR_BOLD_RED           15
#define CTR_BOLD_GREEN         16
#define CTR_BOLD_BLUE          17
#define CTR_BOLD_MAGENTA       18
#define CTR_BOLD_CYAN          19
#define CTR_UL_RED             20
#define CTR_UL_GREEN           21
#define CTR_UL_BLUE            22
#define CTR_UL_MAGENTA         23
#define CTR_UL_CYAN            24
#define CTR_NORM_DEF_COL       25
#define CTR_DEF_COL            26
#define CTR_NORM               27
#define CTR_BOLD               28
#define CTR_UL                 29
#define CTR_UP1LINE            30
#define CTR_NUM_REQUEST        31

#ifdef USE_CURSES

#include <curses.h>
#include <time.h>

size_t colourtext_init(void);

size_t colourtext_setforce(int force_colour); /* -1 = never, 0 = auto, 1 = always */

int colourtext_getforce(void);

/* Call whenever stdout or stderr might have been remapped
 * (checks isatty status).
 */
size_t colourtext_prepare(void);

void colourpair_prepare(void);

typedef struct colourtext_prepared_item_t
{
  const char *_str;
  size_t      _len;
} colourtext_prepared_item;

extern colourtext_prepared_item _colourtext_prepared[2][CTR_NUM_REQUEST];

#define COLOURTEXT_GET(fd,request) \
  (_colourtext_prepared[(fd)][(request)]._str)
#define COLOURTEXT_GET_PREPARED(fd,request) \
  (&_colourtext_prepared[(fd)][(request)])

#define CT_OUT(request) \
  COLOURTEXT_GET(0,CTR_##request)
#define CT_ERR(request) \
  COLOURTEXT_GET(1,CTR_##request)

#define CTP_OUT(request) \
  COLOURTEXT_GET_PREPARED(0,CTR_##request)
#define CTP_ERR(request) \
  COLOURTEXT_GET_PREPARED(1,CTR_##request)

char *escapeashash(const char *text);  /* For debugging */

#else

/* Older compilers do not know inline, so we cannot have these
 * functions marked inline.  Thus cannot have them as functions here
 * if we want the cost of them to be zero when not having curses
 * available.  Either do them as macros, or small dummy functions in
 * colourtext.c...  Small dummies, so types are still checked.  They
 * are anyhow only called around printing, which is expensive in
 * itself.
 */

size_t colourtext_init(void);

size_t colourtext_setforce(int force_colour);

int colourtext_getforce(void);

size_t colourtext_prepare(void);

void colourpair_prepare(void);

#define COLOURTEXT_GET(fd,request) ""

#define CT_OUT(request) ""
#define CT_ERR(request) ""

#define CTP_OUT(request) NULL
#define CTP_ERR(request) NULL

#endif

#endif/*__COLOURTEXT_H__*/
