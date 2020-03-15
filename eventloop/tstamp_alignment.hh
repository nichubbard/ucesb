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

#ifndef __TSTAMP_ALIGNMENT_HH__
#define __TSTAMP_ALIGNMENT_HH__

#include "lmd_input.hh"

#include <stdint.h>

#include <map>
#include <vector>

#ifdef USE_LMD_INPUT
struct titris_subevent
{
    uint                   _titris_id;
    lmd_subevent_10_1_host _subev;
};

union titris_subevent_ident
{
  uint                   _compare[4]; // second is dummy (_info._header.l_dlen, always 0)
  titris_subevent        _info;

public:
  bool operator<(const titris_subevent_ident &rhs) const
  {
    for (size_t i = 0; i < 1; i++)
      {
	if (_compare[i] == rhs._compare[i])
	  continue;
	return _compare[i] < rhs._compare[i];
      }
    for (size_t i = 2; i < countof(_compare); i++)
      {
	if (_compare[i] == rhs._compare[i])
	  continue;
	return _compare[i] < rhs._compare[i];
      }
    return false;
  }
};

typedef std::map<titris_subevent_ident,size_t> set_titris_subevent_index;

struct ts_a_hist
{
public:
  ts_a_hist();

public:
  int      _is_lin;
  uint     _range;
  size_t   _bin_num;
  uint     *_bins[2]; // [0] = passed, [1] = future

  void add(int i,uint64_t val);
  void clear();
  void init(int, uint, size_t);
};

typedef std::vector<ts_a_hist> vect_ts_a_hist;

struct tstamp_alignment_histo
{
public:
  uint64_t _prev_stamp;

  vect_ts_a_hist _hists;

public:
  void init_clear();
  void add_histos(size_t n, int, uint, size_t);
};

typedef std::vector<tstamp_alignment_histo *> vect_tstamp_alignment_histo;


class tstamp_alignment
{

public:
  set_titris_subevent_index    _map_index;
  vect_tstamp_alignment_histo  _vect_histo;

public:
  tstamp_alignment(char const *);
  ssize_t get_index(const lmd_subevent *subevent_info,
		   uint titris_branch_id);
  void account(ssize_t index, uint64_t stamp);
  int get_style() const;
  void show();
  void usage();

private:
  int      _style;
  int      _is_lin;
  uint     _range;
  size_t   _bin_num;

};
#endif


#endif/*__TSTAMP_ALIGNMENT_HH__*/
