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

#ifndef __STAGED_NTUPLE_HH__
#define __STAGED_NTUPLE_HH__

#include "hbook.hh"
#include "writing_ntuple.hh"

#include "external_writer.hh"

#include <vector>

struct staged_ntuple_named_str
{
  std::string _id;
  std::string _str;
};

struct fill_raw_info;

typedef void(*fill_raw_fnc)(fill_raw_info *);

struct fill_raw_info
{
  uint32_t      _words;
  uint32_t     *_ptr;
  fill_raw_fnc  _callback;
  void         *_extra;
};

class staged_ntuple
{
public:
  staged_ntuple();
  ~staged_ntuple();

public:
  external_writer  *_ext;
  reader_src_external _ext_r; // in progress (reading)
  bool _external_ext;

public:
  indexed_item  _global_array;  // rwn & cwn
  index_item   *_index_item;    // cwn
  uint32_t      _entries_index; // cwn
  array_item   *_array_item;    // cwn
  uint32_t      _entries_array; // cwn
  /*index2_item  *_index2_item;    // cwn
    int           _entries_index2; // cwn*/
  array2_item  *_array2_item;    // cwn
  uint32_t      _entries_array2; // cwn

  size_t        _array_entries;

  int           _struct_index;

  uint _x_ntuple_type;
  uint _x_ntuple_opt;

  std::vector<staged_ntuple_named_str> _named_strs;

public:
  void set_ext(external_writer *ext);
  external_writer *get_ext() { return _ext; }

public:
  void open_x(const char *filename,
	      const char *ftitle,
	      int ntuple_type,
	      int ntuple_opt,
	      int server_port = -1,
	      int timeslice = 0, int timeslice_subdir = 0,
	      int autosave = 0,
	      uint sort_u32_words = 0);
  void stage_x(vect_ntuple_items &list,int hid,
	       const char *id, const char *title,
	       const char *index_major,
	       const char *index_minor,
	       void *base,
	       uint max_raw_words = 0);
  void stage_done();
  void event(void *base,uint *sort_u32 = NULL,
	     fill_raw_info *fill_raw = NULL);
  bool get_event();
  void unpack_event(void *base);
  void close();
};

#endif//__STAGED_NTUPLE_HH__
