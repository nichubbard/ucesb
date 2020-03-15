/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2018  Haakan T. Johansson  <f96hajo@chalmers.se
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

/* NOTE: this file may be included multiple times! */

DECL_PRIMITIVE_TYPE(uint8);
DECL_PRIMITIVE_TYPE(uint16);
DECL_PRIMITIVE_TYPE(uint32);
DECL_PRIMITIVE_TYPE(uint64);

DECL_PRIMITIVE_TYPE(DATA8);
DECL_PRIMITIVE_TYPE(DATA12);
DECL_PRIMITIVE_TYPE(DATA12_RANGE);
DECL_PRIMITIVE_TYPE(DATA12_OVERFLOW);
DECL_PRIMITIVE_TYPE(DATA14);
DECL_PRIMITIVE_TYPE(DATA14_RANGE);
DECL_PRIMITIVE_TYPE(DATA14_OVERFLOW);
DECL_PRIMITIVE_TYPE(DATA16);
DECL_PRIMITIVE_TYPE(DATA16_OVERFLOW);
DECL_PRIMITIVE_TYPE(DATA24);
DECL_PRIMITIVE_TYPE(DATA32);
DECL_PRIMITIVE_TYPE(DATA64);

DECL_PRIMITIVE_TYPE(rawdata8);
DECL_PRIMITIVE_TYPE(rawdata12);
DECL_PRIMITIVE_TYPE(rawdata14);
DECL_PRIMITIVE_TYPE(rawdata16);
DECL_PRIMITIVE_TYPE(rawdata16plus);
DECL_PRIMITIVE_TYPE(rawdata24);
DECL_PRIMITIVE_TYPE(rawdata32);
DECL_PRIMITIVE_TYPE(rawdata64);
DECL_PRIMITIVE_TYPE(float);
DECL_PRIMITIVE_TYPE(double);
