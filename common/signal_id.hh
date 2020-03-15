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

#ifndef __SIGNAL_ID_HH__
#define __SIGNAL_ID_HH__

#include "signal_id_extra.hh"
#include "file_line.hh"

#include <string.h>
#include <assert.h>
#include <vector>

#define SIG_PART_NAME        0x01
#define SIG_PART_INDEX       0x02
#define SIG_PART_TYPE_MASK   0x03

#define SIG_PART_INDEX_LIMIT 0x04 // only used for signal_id that will only be printed (/formatted by show_members)
#define SIG_PART_INDEX_ZZP   0x08 // only used for signal_id that will only be printed (/formatted by show_members)
#define SIG_PART_MULTI_ENTRY 0x10 // only used for signal_id that will only be printed (/formatted by show_members)

#define SIG_PART_WILDCARD    0x20 // only used for signal_id_range (when selecting signals)
#define SIG_PART_WILDCARDS   0x40 // only used for signal_id_range (when selecting signals)

struct sig_part
{
public:
  sig_part(const char *name)
  {
    _id._name = name;
    _type = SIG_PART_NAME;

    SIGNAL_ID_EXTRA_INIT;
  }

  sig_part(int index)
  {
    _id._index = index;
    _type = SIG_PART_INDEX;

    SIGNAL_ID_EXTRA_INIT;
  }

  sig_part(int index,int type_extra)
  {
    assert(!(type_extra & (SIG_PART_TYPE_MASK)));

    _id._index = index;
    _type = SIG_PART_INDEX | type_extra;

    SIGNAL_ID_EXTRA_INIT;
  }

public:
  int _type;

  union
  {
    const char *_name;
    int         _index;
  } _id;

  SIGNAL_ID_EXTRA;

public:
  int format(char *str,size_t size) const;
  int format_paw(char *str,size_t size) const;

public:
  bool operator<(const sig_part &rhs) const
  {
    int type_diff = (_type & SIG_PART_TYPE_MASK) - (rhs._type & SIG_PART_TYPE_MASK);

    if (type_diff)
      return type_diff < 0;

    if (_type & SIG_PART_INDEX)
      return _id._index < rhs._id._index;
    if (_type & SIG_PART_NAME)
      return strcmp(_id._name,rhs._id._name) < 0;

    return false;
  }

  bool operator==(const sig_part &rhs) const
  {
    int type_diff = (_type & SIG_PART_TYPE_MASK) - (rhs._type & SIG_PART_TYPE_MASK);

    if (type_diff)
      return false;

    if (_type & SIG_PART_INDEX)
      return _id._index == rhs._id._index;
    if (_type & SIG_PART_NAME)
      return strcmp(_id._name,rhs._id._name) == 0;

    return false;
  }

};

typedef std::vector<sig_part> sig_part_vector;
typedef std::vector<sig_part*> sig_part_ptr_vector;

class signal_id
{
public:
  signal_id()
  {
  }

  signal_id(const char *name)
  {
    _parts.push_back(sig_part(name));
  }

  signal_id(const signal_id &src,const char *name)
  {
    _parts = src._parts;
    _parts.push_back(sig_part(name));
  }

  signal_id(const signal_id &src,int index)
  {
    _parts = src._parts;
    _parts.push_back(sig_part(index));
  }

  signal_id(const signal_id &src,int index,int type_extra)
  {
    _parts = src._parts;
    _parts.push_back(sig_part(index,type_extra));
  }

public:
  size_t format(char *str,size_t size) const;
  size_t format_paw(char *str,size_t size,
		    size_t omit_index = (size_t) -1,
		    size_t omit_index2 = (size_t) -1) const;

public:
  sig_part_vector _parts;

public:
  void push_back(const char *name)
  {
    _parts.push_back(sig_part(name));
  }

  void push_back(int index)
  {
    _parts.push_back(sig_part(index));
  }

  void take_over(sig_part_ptr_vector *src);

public:
  bool difference(const signal_id &rhs,
		  int &diff_index,
		  int &diff_length) const;

  char *generate_indexed(int diff_index,
			 int diff_offset);

public:
  bool operator<(const signal_id &rhs) const
  {
    sig_part_vector::const_iterator iter_this =     _parts.begin();
    sig_part_vector::const_iterator iter_rhs  = rhs._parts.begin();

    for ( ; ; )
      {
	if (iter_rhs == rhs._parts.end())
	  {
	    // Either: both ended -> so equal.
	    // Or: rhs ended, but this not -> so rhs is smaller.
	    return false;
	  }
	if (iter_this == _parts.end())
	  {
	    // We ended, but rhs did not -> so we are smaller.
	    return true;
	  }

	bool same = ((*iter_this) == (*iter_rhs));

	if (!same)
	  {
	    return ((*iter_this) < (*iter_rhs));
	  }

	++iter_this;
	++iter_rhs;
      }
  }
};

void dissect_name(const file_line &loc,
		  const char *name,
		  signal_id& id);

#endif//__SIGNAL_ID_HH__
