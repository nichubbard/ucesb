/* This file is part of DRASI - a data acquisition data pump.
 *
 * Copyright (C) 2017  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#ifndef __FORMAT_PREFIX_H__
#define __FORMAT_PREFIX_H__

#include <stdint.h>
#include <stdlib.h>

/********************************************************************/

typedef struct format_diff_info_t
{
  uint8_t _prefix;
  uint8_t _prefix_age;
} format_diff_info;

/********************************************************************/

void format_prefix(char *buf, size_t size,
		   double val, int max_width,
		   format_diff_info *info);

/********************************************************************/

#endif/*__FORMAT_PREFIX_H__*/
