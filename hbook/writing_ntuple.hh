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

#ifndef __WRITING_NTUPLE_H__
#define __WRITING_NTUPLE_H__

#include "ntuple_item.hh"
#include "ext_data_proto.h"

#include <assert.h>
#include <stdint.h>
#include <arpa/inet.h>

#include <math.h>
#include <stdio.h>

struct writer_dest_external
{
public:
  uint32_t *_p;

public:
  void write_int(uint32_t src)
  {
    //uint32_t offset = ((char*) dest) - _base_ptr;
    //*(_p++) = htonl(offset);
    *(_p++) = htonl(src);
    // printf ("%d\n",src);
  }

  void write_nan() { write_int(0x7f800000); }
  void write_inf() { write_int(0x7fc00000); }

  uint32_t *write_int_dest()
  {
    uint32_t *t = _p;
    _p += 1;
    return t;
  }

  void write_int_to_dest(uint32_t *d,uint32_t src)
  {
    //uint32_t offset = ((char*) dest) - _base_ptr;
    //*(d++) = htonl(offset);
    *d = htonl(src);
    // printf ("[%d]\n",src);
  }

  void write_float(float src)
  {
    //printf("f:%12.5f\n",src);
    //uint32_t offset = ((char*) dest) - _base_ptr;
    union
    {
      float    _f;
      uint32_t _i;
    } value;
    value._f = src;
    //*(_p++) = htonl(offset);
    *(_p++) = htonl(value._i);
  }
};

/* Used to setup the lists needed by the external reader to read the
 * items in the correct order.
 *
 * The next class is used to do the actual reading, and expects to
 * be called in the same order/manner.
 */

struct read_write_ptrs_external
{
public:
  uint32_t *_p;
  char     *_base_ptr;

  uint32_t *_iter_info;

public:
  void dest_offset(void *dest,uint32_t mark)
  {
    uint32_t offset = (uint32_t) (((char*) dest) - _base_ptr);
    // printf ("%d...\n",offset);
    *(_p++) = htonl(mark | EXTERNAL_WRITER_MARK_CANARY);
    *(_p++) = htonl(offset);
  }

  void dest_int(uint32_t *dest, uint32_t mark = 0)
  {
    dest_offset(dest,mark | EXTERNAL_WRITER_MARK_CLEAR_ZERO);
  }

  void dest_float(float *dest)
  {
    dest_offset(dest,EXTERNAL_WRITER_MARK_CLEAR_NAN);
  }

  void dest_int_ctrl(uint32_t *dest,uint32_t max_items,
		     uint32_t mark = 0)
  {
    assert(_iter_info == NULL);
    assert(max_items);

    dest_offset(dest,
		mark |
		EXTERNAL_WRITER_MARK_LOOP |
		EXTERNAL_WRITER_MARK_CLEAR_ZERO);
    // And the information about the step size!
    _iter_info = _p;
    *(_p++) = htonl(max_items);
    *(_p++) = htonl(0); // place for the size of one iter

    //printf ("[%d]\n",max_items);
  }

  void ctrl_over()
  {
    assert(_iter_info);

    uint32_t all_iter_size = (uint32_t) (_p - (_iter_info + 2));
    uint32_t num_iter = ntohl(_iter_info[0]);
    uint32_t one_iter_items = all_iter_size / num_iter / 2;

    assert (2 * one_iter_items * num_iter == all_iter_size);

    _iter_info[1] = htonl(one_iter_items);

    _iter_info = NULL;
  }
};

union float_uint32_t_type_pun
{
  float    _f;
  uint32_t _i;
};

struct reader_src_external
{
public:
  uint32_t *_p;
  uint32_t *_end;

public:
  void read_int(uint32_t &src)
  {
    src = ntohl(*(_p++));
  }

  void read_float(float_uint32_t_type_pun &src)
  {
    src._i = ntohl(*(_p++));
  }
};

// For CWN

#define IND_ITEM_TYPE_FLOAT         0x0002
#define IND_ITEM_TYPE_IS_MASKED     0x0004

#define IND_ITEM_TYPE_FLOAT_DOUBLE  0x0001
#define IND_ITEM_TYPE_FLOAT_SQRT    0x0008
#define IND_ITEM_TYPE_FLOAT_INV     0x0010

#define IND_ITEM_TYPE_INT_ADD_1     0x0001 // is at bit one for saving a shift
#define IND_ITEM_TYPE_INT_USHORT    0x0008
#define IND_ITEM_TYPE_INT_UCHAR     0x0010
#define IND_ITEM_TYPE_INT_INDEX_CUT 0x0020 // reuse IND_ITEM_OFFSET_SHIFT

/* Timestamp info for time-stitching in struct_writer. */
#define IND_ITEM_TYPE_TS_LO         0x0100
#define IND_ITEM_TYPE_TS_HI         0x0200
#define IND_ITEM_TYPE_TS_SRCID      0x0400
#define IND_ITEM_TYPE_MEVENTNO      0x0800
#define IND_ITEM_TYPE_MRG_STAT      0x1000
#define IND_ITEM_TYPE_MRG_MASK      0x2000
#define IND_ITEM_TYPE_MULT_NON0     0x4000

/*
#define IND_ITEM_OFFSET_SHIFT       5 // 0x0020 and above

  Info bits and offset are no longer stored together
#define IND_ITEM_MAX_OFFSET         ((1 << (32 - IND_ITEM_OFFSET_SHIFT))-1)
#define IND_ITEM_MAX_MASK_OFFSET    ((1 << (32 - 5 - 5)) - 1)
*/
#define IND_ITEM_MAX_OFFSET         UINT32_MAX
#define IND_ITEM_MAX_MASK_OFFSET    UINT32_MAX

struct indexed_item
{
public:
  indexed_item()
  {
    _infos = NULL;
    _dests = NULL;
    _masks = NULL;
    _info_slots_per_entry = 0;
    _items_per_entry = 0;
    _masks_per_entry = 0;
  }

  ~indexed_item()
  {
    delete[] _infos;
    delete[] _dests;
  }

public:
  void alloc(uint32_t mult)
  {
    // +2 to allow for the writer/reader to speculatively load a mask
    // info and address
    _infos = new uint32_t[_info_slots_per_entry * mult + 2];
    _dests = new void*[_items_per_entry * mult];

    // For niceness, give speculative load an initialised value
    _infos[_info_slots_per_entry * mult  ] = 0;
    _infos[_info_slots_per_entry * mult+1] = 0;
  }

  void setup_mask_list(uint32_t entries_used);

public:
  uint32_t     *_infos;
  void        **_dests;
  uint32_t     *_masks;

  uint32_t _info_slots_per_entry;
  uint32_t _items_per_entry;
  uint32_t _masks_per_entry;
};

struct limit_item
{
public:
  uint32_t _items_max;  /* For safety */
  uint32_t _items_used; /* For cutting down */
};

struct index_item
  : public indexed_item, limit_item
{
public:
  index_item()
  {
  }

  ~index_item()
  {
  }

public:
  uint32_t *_dest;
  uint32_t  _src_offset;

public:
  bool set_num_limit2_offset(uint32_t /*dummy*/,uint32_t /*dummy*/)
  { assert(false); return false; }
};

struct array_item
  : public indexed_item, limit_item
{
public:
  array_item()
  {
  }

  ~array_item()
  {
  }

public:
  uint32_t  *_dest;
  uint32_t   _src_bits_offset; // unsigned long

public:
  void set_dest2(uint32_t */*dummy*/) {assert(false);}
  void alloc2() {assert(false);}
  bool set_num_limit2_offset(uint32_t /*dummy*/,uint32_t /*dummy*/)
  { assert(false); return false; }
};

struct limit_item2
{
public:
  uint32_t _items_max2;  /* For safety */
  uint32_t _items_used2; /* For cutting down */
};

struct array2_item
  : public array_item, limit_item2
{

public:
  uint32_t  *_dest2;
  uint32_t  *_num_items2_offset;

public:
  void set_dest2(uint32_t *d2) { _dest2 = d2; }

public:
  void alloc2()
  {
    //printf ("ALLOC2: %d\n", _items_used);
    _num_items2_offset = new uint32_t[_items_used];
    for (uint32_t i = 0; i < _items_used; i++)
      _num_items2_offset[i] = (uint32_t) -1;
  }

public:
  bool set_num_limit2_offset(uint32_t index,uint32_t offset)
  {
    //printf ("LIM2: %d: %d\n", index, offset);
    if (_num_items2_offset[index] != (uint32_t) -1 &&
	_num_items2_offset[index] != offset)
      {
	//printf ("MISMATCH: %d: %d %d\n",
	//	index, _num_items2_offset[index], offset);
	return false;
      }
    _num_items2_offset[index] = offset;
    return true;
  }


};


uint32_t cwn_fill_indexed_item(writer_dest_external &w,indexed_item *array,
			       void *base,
			       uint32_t entries,uint32_t offset,
			       uint32_t limit_index = 0);
void cwn_fill_index_item(writer_dest_external &w,index_item *array,
			 uint32_t entries,void *base);
void cwn_fill_array_item(writer_dest_external &w,array_item *array,
			 uint32_t entries,void *base);
void cwn_fill_array2_item(writer_dest_external &w,array2_item *array,
			  uint32_t entries,void *base);

void cwn_get_indexed_item(reader_src_external &w,indexed_item *array,
			  void *base,uint32_t entries,uint32_t offset);
void cwn_get_index_item(reader_src_external &w,index_item *array,
			uint32_t entries,void *base);
void cwn_get_array_item(reader_src_external &w,array_item *array,
			uint32_t entries,void *base);
void cwn_get_array2_item(reader_src_external &w,array2_item *array,
			 uint32_t entries,void *base);

void cwn_ptrs_indexed_item(read_write_ptrs_external &w,indexed_item *array,
			   uint32_t entries,uint32_t offset);
void cwn_ptrs_index_item(read_write_ptrs_external &w,index_item *array,
			 uint32_t entries);
void cwn_ptrs_array_item(read_write_ptrs_external &w,array_item *array,
			 uint32_t entries);
void cwn_ptrs_array2_item(read_write_ptrs_external &w,array2_item *array,
			  uint32_t entries);

#endif/*__WRITING_NTUPLE_H__*/
