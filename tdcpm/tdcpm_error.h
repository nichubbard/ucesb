/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2022  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#ifndef __TDCPM_ERROR_H__
#define __TDCPM_ERROR_H__

#include <stdio.h>

#define TDCPM_ERROR(...) do {				\
    fprintf(stderr,"%s:%d: ",__FILE__,__LINE__);	\
    fprintf(stderr,__VA_ARGS__);			\
    fputc('\n',stderr);					\
    exit(1);						\
  } while (0)

#define TDCPM_WARNING(...) do {				\
    fprintf(stderr,"%s:%d: ",__FILE__,__LINE__);	\
    fprintf(stderr,__VA_ARGS__);			\
    fputc('\n',stderr);					\
  } while (0)

#define TDCPM_ERROR_LOC(loc, ...) do {		\
    tdcpm_lineno_format(stderr, loc);		\
    fputc(' ',stderr);				\
    fprintf(stderr,__VA_ARGS__);		\
    fputc('\n',stderr);				\
    exit(1);					\
  } while (0)

#define TDCPM_WARNING_LOC(loc, ...) do {	\
    tdcpm_lineno_format(stderr, loc);		\
    fputc(' ',stderr);				\
    fprintf(stderr,__VA_ARGS__);		\
    fputc('\n',stderr);				\
  } while (0)

#endif/*__TDCPM_ERROR_H__*/
