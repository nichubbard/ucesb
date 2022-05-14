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

#ifndef __TITRIS_STAMP_HH__
#define __TITRIS_STAMP_HH__

#define TITRIS_STAMP_EBID_ERROR           0x00010000
#define TITRIS_STAMP_EBID_BRANCH_ID_MASK  0x00000f00
#define TITRIS_STAMP_EBID_BRANCH_ID_SHIFT 8
#define TITRIS_STAMP_EBID_UNUSED         (0xffffffff ^ \
					  TITRIS_STAMP_EBID_ERROR ^	\
					  TITRIS_STAMP_EBID_BRANCH_ID_MASK)
#define TITRIS_STAMP_LMH_TIME_MASK        0x0000ffff
#define TITRIS_STAMP_LMH_ID_MASK          0xffff0000
#define TITRIS_STAMP_L16_ID               0x00f70000
#define TITRIS_STAMP_M16_ID               0x01f70000
#define TITRIS_STAMP_H16_ID               0x02f70000

#endif//__TITRIS_STAMP_HH__
