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

#ifndef __SIGNAL_ID_MAP_HH__
#define __SIGNAL_ID_MAP_HH__

#include "../common/signal_id.hh"

#include "enumerate.hh"

#define SID_MAP_UNPACK     0x01
#define SID_MAP_RAW        0x02
#define SID_MAP_CAL        0x03
#define SID_MAP_USER       0x04
#define SID_MAP_CALIB      0x05

#define SID_MAP_STICKY     0x08
#define SID_MAP_MIRROR_MAP 0x10
#define SID_MAP_REVERSE    0x20

#define SID_MAP_MAX_NUM    0x40

class prefix_units_exponent;

struct signal_id_info
{

public:
  int         _type;
  const void *_addr;
  const prefix_units_exponent *_unit;

public:
  set_dest_fcn _set_dest; // only declared when part of a SID_MAP_MIRROR_MAP
};

struct signal_id_zzp_info
{
  size_t _zzp_part;
};

struct enumerate_msid_info
{
  int _map_no;
};

struct sid_leaf;
struct sid_node;

template<int create_missing>
sid_leaf *find_leaf(const signal_id &id,
		    sid_node **base_ptr);

signal_id_info *find_signal_id_info(const signal_id &id,
				    enumerate_msid_info *extra);
const signal_id_info *get_signal_id_info(const signal_id &id,
					 int map_no);

const signal_id_zzp_info *get_signal_id_zzp_info(const signal_id &id,
						 int map_no);

void setup_signal_id_map();

void setup_signal_id_map_unpack_map(void *extra);
void setup_signal_id_map_raw_map(void *extra);

void setup_signal_id_map_unpack_sticky_map(void *extra);

void setup_signal_id_map_raw_reverse_map(void *extra);
void setup_signal_id_map_raw_sticky_reverse_map(void *extra);

#endif//__SIGNAL_ID_MAP_HH__
