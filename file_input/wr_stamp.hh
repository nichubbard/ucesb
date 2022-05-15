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

#ifndef __WR_STAMP_HH__
#define __WR_STAMP_HH__

/* TODO: confirm this! */
#define WR_STAMP_EBID_ERROR           0x00010000
#define WR_STAMP_EBID_BRANCH_ID_MASK  0x00003f00
#define WR_STAMP_EBID_BRANCH_ID_SHIFT 8
#define WR_STAMP_EBID_UNUSED		\
  (0xffffffff ^				\
   WR_STAMP_EBID_ERROR ^		\
   WR_STAMP_EBID_BRANCH_ID_MASK)
#define WR_STAMP_DATA_TIME_MASK       0x0000ffff
#define WR_STAMP_DATA_ID_MASK         0xffff0000
#define WR_STAMP_DATA_0_16_ID         0x03e10000
#define WR_STAMP_DATA_1_16_ID         0x04e10000
#define WR_STAMP_DATA_2_16_ID         0x05e10000
#define WR_STAMP_DATA_3_16_ID         0x06e10000

/* Extra marker for sync check value. */
#define SYNC_CHECK_MAGIC_MASK  0xfff00000
#define SYNC_CHECK_FLAGS_MASK  0x000f0000
#define SYNC_CHECK_VALUE_MASK  0x0000ffff
#define SYNC_CHECK_MAGIC       0xf1a00000  /* High 4 bits for geom mism. */
#define SYNC_CHECK_REF         0x00010000  /* Reference value (master). */
#define SYNC_CHECK_RECV        0x00020000  /* Received value (slave). */
#define SYNC_CHECK_LOCAL       0x00040000  /* Local (expect no match, use
					    * for both master and slave). */

#endif//__WR_STAMP_HH__
