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

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <stdint.h>

#include "staging_ntuple.hh"

#include "error.hh"

#include <string.h>
#include <set>

/******************************************************************/

#define DEBUG_STAGING 0

void stage_ntuple_item::storage_size(indexed_item &write_ptrs,size_t &size)
{
  if (DEBUG_STAGING)
    INFO(0,"Storage:  %s",_item->_name.c_str());

  write_ptrs._items_per_entry++;

  write_ptrs._info_slots_per_entry += 2;
  if (_item->_ctrl_mask._ptr) // bitmask
    write_ptrs._info_slots_per_entry += 2;

  size += sizeof(float);
}

void init_cwn_hbname(stage_ntuple_info &info,
		     void *ptr,size_t len,
		     const char *block,
		     const char *var_name,uint var_array_len,
		     const char *var_ctrl_name,int var_type,
		     uint limit_min,uint limit_max)
{
  if (DEBUG_STAGING)
    INFO(0,"HBNAME(%s,%zd,%zd,...,%s);",
	 block,
	 (((char*) ptr)-info.base_ptr),
	 len,
	 var_name);

  if (info.ext)
    info.ext->send_hbname_branch(block,
				 (uint32_t) (((char*) ptr)-info.base_ptr),
				 (uint32_t) len,
				 var_name,var_array_len,
				 var_ctrl_name,var_type,
				 limit_min,limit_max);
}

void init_cwn_var(ntuple_item *item,
		  indexed_item &write_ptrs,
		  type_indices &indices,
		  stage_ntuple_info &info,
		  char *&ptr,const char *block,
		  bool first_item = false,
		  const char *index_var = NULL,
		  uint32_t index = 0,
		  uint32_t length_used = 0)
{
  char var_name[128], var_ctrl_name[128];
  const char *type = NULL;
  uint var_array_len = (uint) -1;
  int var_type = 0;
  ntuple_limit limit;

  uint32_t write_info = 0;

  limit = item->_limits;

  strcpy (var_name,item->_name.c_str());

  if (index_var)
    {
      // In case the (implicit) index var has the same name as the
      // variable itself (when using an array directly, without it
      // enclosing a structure, such that it terminates with a name)
      // Then we would get complaints about re-using names...
      // Append a 'v' to the names for the values
      if (strcmp(var_name,index_var) == 0)
	{
	  strcat(var_name,"v");
	}
      var_array_len = length_used;
    }

  if (item->_flags & NTUPLE_ITEM_BIT_MASK)
    {
      assert(!(item->_flags & NTUPLE_ITEM_HAS_LIMIT));

      // TODO: dirty hack.  instead introduce UINT type
      // to be able to set proper limits
      if (item->_mask_bits <= 0x00ffffff)
	{
	  item->_flags |= NTUPLE_ITEM_HAS_LIMIT;

	  item->_limits._int._min = 0;
	  item->_limits._int._max = item->_mask_bits;
	}
    }

  if (item->_flags & NTUPLE_ITEM_HAS_LIMIT)
    {
      if (item->_type == ntuple_item::USHORT ||
	  item->_type == ntuple_item::UCHAR ||
	  item->_type == ntuple_item::INT ||
	  item->_type == ntuple_item::UINT ||
	  item->_type == ntuple_item::INT_INDEX_CUT ||
	  item->_type == ntuple_item::INT_INDEX)
	{
	  var_type |= EXTERNAL_WRITER_FLAG_HAS_LIMIT;
	}
    }

  if (item->_type == ntuple_item::USHORT)
    {
      // force ushorts down to 16 bit storage (CWN)
      if (!(item->_flags & NTUPLE_ITEM_HAS_LIMIT))
	{
	  item->_limits._int._min = 0;
	  item->_limits._int._max = 0xffff;
	  var_type |= EXTERNAL_WRITER_FLAG_HAS_LIMIT;
	}

      type = "us";
      var_type |= EXTERNAL_WRITER_FLAG_TYPE_UINT32;
      write_info |= IND_ITEM_TYPE_INT_USHORT;
    }
  else if (item->_type == ntuple_item::UCHAR)
    {
      // force uchars down to 8 bit storage (CWN)
      if (!(item->_flags & NTUPLE_ITEM_HAS_LIMIT))
	{
	  item->_limits._int._min = 0;
	  item->_limits._int._max = 0xff;
	  var_type |= EXTERNAL_WRITER_FLAG_HAS_LIMIT;
	}

      type = "uc";
      var_type |= EXTERNAL_WRITER_FLAG_TYPE_UINT32;
      write_info |= IND_ITEM_TYPE_INT_UCHAR;
    }
  else if (item->_type == ntuple_item::DOUBLE)
    {
      type = "d";
      var_type |= EXTERNAL_WRITER_FLAG_TYPE_FLOAT32;
      write_info |= IND_ITEM_TYPE_FLOAT | IND_ITEM_TYPE_FLOAT_DOUBLE;

      if (item->_flags & NTUPLE_ITEM_DO_SQRT)
	write_info |= IND_ITEM_TYPE_FLOAT_SQRT;
      if (item->_flags & NTUPLE_ITEM_DO_SQRT_INV)
	write_info |= IND_ITEM_TYPE_FLOAT_SQRT | IND_ITEM_TYPE_FLOAT_INV;
    }
  else if (item->_type == ntuple_item::FLOAT)
    {
      type = "f";
      var_type |= EXTERNAL_WRITER_FLAG_TYPE_FLOAT32;
      write_info |= IND_ITEM_TYPE_FLOAT;

      if (item->_flags & NTUPLE_ITEM_DO_SQRT)
	write_info |= IND_ITEM_TYPE_FLOAT_SQRT;
      if (item->_flags & NTUPLE_ITEM_DO_SQRT_INV)
	write_info |= IND_ITEM_TYPE_FLOAT_SQRT | IND_ITEM_TYPE_FLOAT_INV;
    }
  else if (item->_type == ntuple_item::INT)
    {
      type = "i";
      var_type |= EXTERNAL_WRITER_FLAG_TYPE_INT32;
    }
  else if (item->_type == ntuple_item::UINT)
    {
      type = "ui";
      var_type |= EXTERNAL_WRITER_FLAG_TYPE_UINT32;
    }
  else if (item->_type == ntuple_item::UINT64)
    {
      ERROR("64-bit uint not handled.");
      // TODO: can be fixed; suggestion to make 2 entries, hi and lo.
      // issue will be in unpacking part to place items correctly.
      // possibly with the compresed data, which give fixed
      // locations.  Perhaps give extra word-swap array to make
      // post-unpack corrections?
    }
  else if (item->_type == ntuple_item::INT_INDEX_CUT ||
	   item->_type == ntuple_item::INT_INDEX)
    {
      if (item->_type == ntuple_item::INT_INDEX_CUT && !first_item)
	{
	  ERROR("Internal error, ntuple index item (%s) not first in group.",
		var_name);
	  write_info |= IND_ITEM_TYPE_INT_INDEX_CUT;
	}

      type = "i+1";
      var_type |= EXTERNAL_WRITER_FLAG_TYPE_UINT32;
      write_info |= IND_ITEM_TYPE_INT_ADD_1;
    }
  else
    assert (false);

  if (item->_flags & NTUPLE_ITEM_TS_LO)    write_info |= IND_ITEM_TYPE_TS_LO;
  if (item->_flags & NTUPLE_ITEM_TS_HI)    write_info |= IND_ITEM_TYPE_TS_HI;
  if (item->_flags & NTUPLE_ITEM_TS_SRCID) write_info |=IND_ITEM_TYPE_TS_SRCID;
  if (item->_flags & NTUPLE_ITEM_MEVENTNO) write_info |=IND_ITEM_TYPE_MEVENTNO;
  if (item->_flags & NTUPLE_ITEM_MRG_STAT) write_info |=IND_ITEM_TYPE_MRG_STAT;
  if (item->_flags & NTUPLE_ITEM_MRG_MASK) write_info |=IND_ITEM_TYPE_MRG_MASK;
  if (item->_flags & NTUPLE_ITEM_MULT_NON0)write_info |=IND_ITEM_TYPE_MULT_NON0;

  info.fix_case(var_name);

  //Warning("Storing:  %s %d+%d %s",vars,ind,index_offset,type);

  if (index == 0)
    {
      uint32_t length = sizeof(float);
      var_ctrl_name[0] = 0;
      if (index_var)
	{
	  length *= length_used;
	  strcpy (var_ctrl_name,index_var);
	  info.fix_case(var_ctrl_name);
	}

      init_cwn_hbname(info,ptr,length,block,
		      var_name,var_array_len,
		      var_ctrl_name,var_type,
		      item->_limits._int._min,
		      item->_limits._int._max);
    }

  size_t offset =
    (size_t) ((char*) item->_src._ptr_int - (char*) info.src_base);

  if (offset > IND_ITEM_MAX_OFFSET)
    ERROR("Internal error, ntuple item (%s) offset too large (%zd > %d).",
	  var_name,offset,IND_ITEM_MAX_OFFSET);

  write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
		    indices._slot_ind++] = write_info;
  write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
		    indices._slot_ind++] = (uint32_t) offset;

  if (DEBUG_STAGING)
    INFO(0,"Storing:  %s [%d] %s @%d (%08x %08x) [p%p]",
	 var_name,index,type,(int) offset,
	 write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
			   indices._slot_ind-2],
	 write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
                           indices._slot_ind-1],
	 ptr);

  if (item->_ctrl_mask._ptr) // bitmask
    {
      // By design, we have the same ptr for both bitmasks, it is
      // additionally required that they each are one bit, and one bit
      // only

      uint32_t valid_bit_shift;
      uint32_t overflow_bit_shift;

      if (!item->_ctrl_mask._valid)
	ERROR("Bitmask (%s) must have valid bit.",var_name);
      else if (numbits(item->_ctrl_mask._valid) != 1)
	ERROR("Bitmask (%s) has more than 1 valid bit (%08x).",
	      var_name,item->_ctrl_mask._valid);
      else
	valid_bit_shift = firstbit(item->_ctrl_mask._valid);

      if (!item->_ctrl_mask._overflow)
	overflow_bit_shift = 31; // will be shifted out, so no overflow
      else if (numbits(item->_ctrl_mask._overflow) != 1)
	ERROR("Bitmask (%s) has more than 1 overflow bit (%08x).",
	      var_name,item->_ctrl_mask._overflow);
      else if (item->_ctrl_mask._overflow == 1)
	ERROR("Bitmask (%s) has overflow bit in position 0, not supported.",
	      var_name); // clashes with the use of 31 as no-overflow-bit
      else if (!(write_info & IND_ITEM_TYPE_FLOAT))
	ERROR("Bitmask (%s) has overflow bit for non-float value, not supported.",var_name);
      else
	overflow_bit_shift = firstbit(item->_ctrl_mask._overflow) - 1;

      uint32_t mask_info = valid_bit_shift | (overflow_bit_shift << 5);

      size_t mask_offset =
	(size_t) ((char*) item->_ctrl_mask._ptr - (char*) info.src_base);

      if (mask_offset > IND_ITEM_MAX_MASK_OFFSET)
	ERROR("Internal error, ntuple item (%s) mask offset too large (%zd > %d).",
	      var_name,mask_offset,IND_ITEM_MAX_MASK_OFFSET);

      write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
			indices._slot_ind-2] |= IND_ITEM_TYPE_IS_MASKED;

      write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
			indices._slot_ind++] = mask_info;
      write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
			indices._slot_ind++] = (uint32_t) mask_offset;

      if (DEBUG_STAGING)
	INFO(0,"Bitmask:  %s [%d] v%d o%d @%d ([%08x] %08x %08x) [p%p]",
	     var_name,index,
	     valid_bit_shift,overflow_bit_shift,(int) mask_offset,
	     write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
			       indices._slot_ind-4],
	     write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
			       indices._slot_ind-2],
	     write_ptrs._infos[index * write_ptrs._info_slots_per_entry +
			       indices._slot_ind-1],
	     ptr);
    }

  write_ptrs._dests[index * write_ptrs._items_per_entry +
		    indices._dest_ind++] = ptr;

  ptr += sizeof(float);
}

void stage_ntuple_item::init_cwn(indexed_item &write_ptrs,
				 type_indices &indices,
				 stage_ntuple_info &info,
				 char *&ptr,const char *block)
{
  init_cwn_var(_item,write_ptrs,indices,info,ptr,block);
}

/******************************************************************/

void storage_size_index(const sni_ind1 &lim_used,bool is_array,size_t &size)
{
  size += sizeof(float); // for us

  if (is_array)
    size += lim_used._1 * sizeof(float); // for the index vars

}

void storage_size_index(const sni_ind2 &lim_used,bool is_array,size_t &size)
{
  size += sizeof(float); // for us

  size += sizeof(float); // for the number-of-indices ctrl var

  if (is_array)
    size += 2 * lim_used._1 * sizeof(float); // for the index vars, and the end vars
}

template<typename Tsni_ind,typename Tsni_vect>
void stage_ntuple_index_var<Tsni_ind,Tsni_vect>::storage_size(size_t &size)
{
  if (DEBUG_STAGING)
    {
      char str_used[32];
      char str_max[32];
      _length_used.dbg_fmt(str_used,sizeof(str_used));
      _length_max.dbg_fmt(str_max,sizeof(str_max));
      INFO(0,"Storage:  %s[0,%s/%s]",
	   _item->_name.c_str(),
	   str_used,
	   str_max);
    }

  storage_size_index(_length_used,
		     _item->_flags & NTUPLE_ITEM_IS_ARRAY_MASK,
		     size);

  // stage_ntuple_item::storage_size(write_ptrs,size);

  // _write_ptrs = new index_item;

  typename vect_stage_ntuple_indexed_var<Tsni_ind,
					 Tsni_vect>::type::iterator i_vars;

  for (i_vars  = _vars.begin();
       i_vars != _vars.end();
       ++i_vars)
    (*i_vars)->storage_size(_write_ptrs,size,_length_used);
}

template<typename Tsni_ind,typename Tsni_vect>
template<typename Tindexed_item>
void stage_ntuple_index_var<Tsni_ind,Tsni_vect>::init_cwn_vars(Tindexed_item &write_ptrs,
					   //type_indices &indices,
					   stage_ntuple_info &info,
					   char *&ptr,const char *block)
{
  typename vect_stage_ntuple_indexed_var<Tsni_ind,
					 Tsni_vect>::type::iterator i_vars;

  write_ptrs.alloc(_length_used.tot());

  type_indices base_indices;

  bool first_item = true;

  for (i_vars  = _vars.begin();
       i_vars != _vars.end();
       ++i_vars)
    {
      // Make sure we only increase the indices once per variable,
      // the array is handled via offsets.

      type_indices indexed_indices = base_indices;

      Tsni_ind index;

      // This loops 1d, or 2d, depending on Tsni_ind
      for (index.clear();
	   index.iter_within(_length_used); index.iter(_length_used))
	{
	  indexed_indices = base_indices;

	  (*i_vars)->init_cwn(write_ptrs,indexed_indices,
			      index,
			      _item->_name.c_str(),
			      _length_used,
			      info,ptr,block,
			      first_item);
	}

      first_item = false;
      base_indices = indexed_indices;
    }

  assert(write_ptrs._info_slots_per_entry == base_indices._slot_ind);
  assert(write_ptrs._items_per_entry      == base_indices._dest_ind);
}

template<typename Tsni_ind,typename Tsni_vect>
void stage_ntuple_index_var<Tsni_ind,Tsni_vect>::init_cwn_ctrl_var(stage_ntuple_info &info,
					       char *&ptr,const char *block)
{
  char var_name[128];

  assert (_item->_flags & NTUPLE_ITEM_HAS_LIMIT);
  assert (_item->_type == ntuple_item::USHORT ||
	  _item->_type == ntuple_item::UINT ||
	  _item->_type == ntuple_item::INT);
  strcpy (var_name,_item->_name.c_str());

  info.fix_case(var_name);

  if (DEBUG_STAGING)
    INFO(0,"Storing:  %s (index) [p%p]",var_name,ptr);

  init_cwn_hbname(info,ptr,sizeof(int),block,
		  var_name,(uint) -1,"",
		  EXTERNAL_WRITER_FLAG_TYPE_UINT32 |
		  EXTERNAL_WRITER_FLAG_HAS_LIMIT,
		  0,_length_used.tot());
}

template<typename T>
void set_writing_max_used(T &dest,
			  sni_ind1 &max, sni_ind1 &used)
{
  dest._items_max  = max._1;
  dest._items_used = used._1;
}

template<typename T>
void set_writing_max_used(T &dest,
			  sni_ind2 &max, sni_ind2 &used)
{
  dest._items_max   = max._1;
  dest._items_used  = used._1;
  dest._items_max2  = max._2;
  dest._items_used2 = used._2;
}

template<typename Tsni_ind,typename Tsni_vect>
void stage_ntuple_index_var<Tsni_ind,Tsni_vect>::init_cwn(index_item &write_ptrs,
				      //type_indices &indices,
				      stage_ntuple_info &info,
				      char *&ptr,const char *block)
{
  ((indexed_item &) write_ptrs) = _write_ptrs;

  // Fix the limits:

  // _item->_limits._int._max = _length_used;

  //init_cwn_var(item,cwn,block);

  init_cwn_ctrl_var(info,ptr,block);

  size_t offset =
    (size_t) ((char*) _item->_src._ptr_int - (char*) info.src_base);

  if (offset > IND_ITEM_MAX_OFFSET)
    ERROR("Internal error, ntuple item offset too large (%d > %d).",
	  (int) offset,IND_ITEM_MAX_OFFSET);

  write_ptrs._dest = (uint32_t *) ptr;
  write_ptrs._src_offset  = (uint32_t) offset;
  set_writing_max_used(write_ptrs, _length_max, _length_used);

  ptr += sizeof(float);

  // stage_ntuple_item::init_cwn(write_ptrs,indices,cwn,ptr,block);

  init_cwn_vars(write_ptrs,info,ptr,block);
}

template<typename Tsni_ind,typename Tsni_vect>
template<typename Tarray_item>
void stage_ntuple_index_var<Tsni_ind,Tsni_vect>::init_cwn(Tarray_item &write_ptrs,
				      //type_indices &indices,
				      stage_ntuple_info &info,
				      char *&ptr,const char *block)
{
  ((indexed_item &) write_ptrs) = _write_ptrs;

  // Fix the limits:

  // _item->_limits._int._max = _length_used;

  //init_cwn_var(item,cwn,block);

  if (Tsni_ind::is_2d())
    {
      // for the index vars

      char var_2d_idx_name[128], var_2d_end_name[128], var_2d_ctrl_name[128];

      snprintf (var_2d_ctrl_name,sizeof(var_2d_ctrl_name),"%sM",
		_item->_name.c_str());
      snprintf (var_2d_idx_name,sizeof(var_2d_idx_name),"%sMI",
		_item->_name.c_str());
      snprintf (var_2d_end_name,sizeof(var_2d_end_name),"%sME",
		_item->_name.c_str());

      info.fix_case(var_2d_ctrl_name);
      info.fix_case(var_2d_idx_name);
      info.fix_case(var_2d_end_name);

      write_ptrs.set_dest2((uint32_t *) ptr);

      if (DEBUG_STAGING)
	  INFO(0,"Storing:  %s (implicit 2d ctrl) [p%p]",var_2d_ctrl_name,ptr);

      init_cwn_hbname(info,ptr,sizeof(float),
		      block,
		      var_2d_ctrl_name,(uint) -1,"",
		      EXTERNAL_WRITER_FLAG_TYPE_UINT32 |
		      EXTERNAL_WRITER_FLAG_HAS_LIMIT,
		      1,_length_used._1);

      ptr += sizeof(float);

      if (DEBUG_STAGING)
	INFO(0,"Storing:  %s (implicit 2d-index) [p%p]",var_2d_idx_name,ptr);

      init_cwn_hbname(info,ptr,_length_used._1 * sizeof(float),
		      block,
		      var_2d_idx_name,_length_used._1,
		      var_2d_ctrl_name,
		      EXTERNAL_WRITER_FLAG_TYPE_UINT32 |
		      EXTERNAL_WRITER_FLAG_HAS_LIMIT,
		      1,_length_used._1);

      ptr += _length_used._1 * sizeof(float);

      if (DEBUG_STAGING)
	INFO(0,"Storing:  %s (implicit 2d-end) [p%p]",var_2d_end_name,ptr);

      init_cwn_hbname(info,ptr,_length_used._1 * sizeof(float),
		      block,
		      var_2d_end_name,_length_used._1,
		      var_2d_ctrl_name,
		      EXTERNAL_WRITER_FLAG_TYPE_UINT32 |
		      EXTERNAL_WRITER_FLAG_HAS_LIMIT,
		      1,_length_used.tot());

      ptr += _length_used._1 * sizeof(float);
    }

  init_cwn_ctrl_var(info,ptr,block);

  size_t offset =
    (size_t) ((char*) _item->_src._ptr_int - (char*) info.src_base);

  if (offset > IND_ITEM_MAX_OFFSET)
    ERROR("Internal error, ntuple array mask offset too large (%d > %d).",
	  (int) offset,IND_ITEM_MAX_OFFSET);

  write_ptrs._dest = (uint32_t *) ptr;
  write_ptrs._src_bits_offset = (uint32_t) offset;
  set_writing_max_used(write_ptrs, _length_max, _length_used);
  if (Tsni_ind::is_2d())
    write_ptrs.alloc2();

  ptr += sizeof(float);

  if (!Tsni_ind::is_2d())
    {
      // for the index vars

      char var_idx_name[128], var_ctrl_name[128];

      snprintf (var_ctrl_name,sizeof(var_ctrl_name),"%s",
		_item->_name.c_str());
      snprintf (var_idx_name,sizeof(var_idx_name),"%sI",
		_item->_name.c_str());

      info.fix_case(var_ctrl_name);
      info.fix_case(var_idx_name);

      if (DEBUG_STAGING)
	INFO(0,"Storing:  %s (implicit index) [p%p]",var_idx_name,ptr);

      init_cwn_hbname(info,ptr,_length_used._1 * sizeof(float),
		      block,
		      var_idx_name,_length_used._1,
		      var_ctrl_name,
		      EXTERNAL_WRITER_FLAG_TYPE_UINT32 |
		      EXTERNAL_WRITER_FLAG_HAS_LIMIT,
		      1,_length_used._1);

      ptr += _length_used._1 * sizeof(float);
    }

  // stage_ntuple_item::init_cwn(write_ptrs,indices,cwn,ptr,block);

  init_cwn_vars(write_ptrs,info,ptr,block);
}

/******************************************************************/

void check_var_vect_filled(const sni_vect1 &vect,sni_ind1 &length_used,
			   const std::string &name)
{
  for (uint i = 0; i < length_used._1; i++)
    if (!vect._items[i])
      ERROR("Variable length variable %s has no entry "
	    "for (1-based) index %d.",
	    name.c_str(),i+1);
}

void check_var_vect_filled(const sni_vect2 &vect,sni_ind2 &length_used,
			   const std::string &name)
{
  for (uint i = 0; i < length_used._1; i++)
    {
      for (uint j = 0; j < length_used._2; j++)
	if (!vect._items[i][j])
	  ERROR("Multi-hit variable length array %s has no entry "
		"for (1-based) index %d,%d.",
		name.c_str(),i+1,j+1);

      const int* ptr_limit2 = vect._items[i][0]->_ptr_limit2;

      for (uint j = 0; j < length_used._2; j++)
	{
	  if (vect._items[i][j]->_ptr_limit2 == NULL)
	    ERROR("Multi-hit variable length array %s has no "
		  "limit item for index %d,%d.",
		  name.c_str(),i,j);
	  if (vect._items[i][j]->_ptr_limit2 != ptr_limit2)
	    ERROR("Multi-hit variable length array %s has different "
		  "limit item for index %d, subindices %d vs 0.",
		  name.c_str(),i,j);
	}
    }
}

template<typename Tsni_ind,typename Tsni_vect>
void stage_ntuple_indexed_var<Tsni_ind,Tsni_vect>::storage_size(indexed_item &write_ptrs,size_t &size,
					    Tsni_ind length_used)
{
  assert (length_used.tot() <= _items.size());

  if (DEBUG_STAGING)
    {
      char str_used[32];
      length_used.dbg_fmt(str_used,sizeof(str_used));
      INFO(0,"Storage:  %s[0,%s/%zd]",
	   _name.c_str(),
	   str_used,
	   _items.size());
    }

  check_var_vect_filled(_items,length_used,_name);

  ntuple_item *item = _items.first_item();

  UNUSED(item);

  write_ptrs._items_per_entry++;
  write_ptrs._info_slots_per_entry += 2;
  if (item->_ctrl_mask._ptr) // bitmask
    write_ptrs._info_slots_per_entry += 2;

  size += length_used.tot() * sizeof(float);
}

template<typename Tsni_ind,typename Tsni_vect>
template<typename Tindexed_item>
void stage_ntuple_indexed_var<Tsni_ind,Tsni_vect>::init_cwn(Tindexed_item &write_ptrs,
					type_indices &indices,
					Tsni_ind index,
					const char *index_var,
					Tsni_ind length_used,
					stage_ntuple_info &info,
					char *&ptr,const char *block,
					bool first_item)
{
  // INFO(0,"Storing:   ??? %d",index);

  ntuple_item *item = _items.get(index);

  if (Tsni_ind::is_2d())
    {
      size_t num_limit2_offset =
	(size_t) ((const char*) item->_ptr_limit2 - (char*) info.src_base);

      (void) num_limit2_offset; // for those that do not use...

      if (!write_ptrs.set_num_limit2_offset(index._1,
					    (uint32_t) num_limit2_offset))
	{
	  // We do not match what has been found before for
	  // this index (for some other?) variable
	  ERROR("Multi-hit variable length array %s has "
		"mismatching limit2 ctrl item for index %d.",
		item->_name.c_str(),index._1+1);
	}
    }

  init_cwn_var(item,write_ptrs,indices,info,ptr,block,first_item,
	       index_var,index.lin_index(length_used),length_used.tot());
}

/******************************************************************/

void stage_ntuple_block::storage_size(indexed_item &write_ptrs,
				      size_t &size,
				      uint32_t &index_vars,uint32_t &array_vars,
				      /*int &index2_vars,*/uint32_t &array2_vars)
{
  if (DEBUG_STAGING)
    INFO(0,"Block:   %s",_block.c_str());

  vect_stage_ntuple_block_item::iterator i_item;

  for (i_item  = _items.begin();
       i_item != _items.end();
       ++i_item)
    {
      stage_ntuple_item_plain *plain_item =
	dynamic_cast<stage_ntuple_item_plain *>((*i_item)._item);
      stage_ntuple_index_var<sni_ind1,sni_vect1> *index1_var =
	dynamic_cast<stage_ntuple_index_var<sni_ind1,
					    sni_vect1> *>((*i_item)._item);
      stage_ntuple_index_var<sni_ind2,sni_vect2> *index2_var =
	dynamic_cast<stage_ntuple_index_var<sni_ind2,
					    sni_vect2> *>((*i_item)._item);

      if (plain_item)
	{
	  plain_item->storage_size(write_ptrs,size);
	}
      else if (index1_var)
	{
	  if (!(index1_var->_item->_flags &
		NTUPLE_ITEM_IS_ARRAY_MASK))
	    {
	      index1_var->storage_size(size);
	      index_vars++;
	    }
	  else
	    {
	      index1_var->storage_size(size);
	      array_vars++;
	    }
	}
      else if (index2_var)
	{
	  if (!(index2_var->_item->_flags &
		NTUPLE_ITEM_IS_ARRAY_MASK))
	    {
	      ERROR("index_item2 unimplemented");
	      /*
	      index2_var->storage_size(size);
	      index2_vars++;
	      */
	    }
	  else
	    {
	      index2_var->storage_size(size);
	      array2_vars++;
	    }
	}
      else
	assert(false);
    }
}

typedef std::set<uint32_t> set_uint32_t;

void indexed_item::setup_mask_list(uint32_t entries_used)
{
  // The list of masks is needed when reading lists of items that have
  // a controlling variable.  land02/ucesb will not initialize/zero
  // such (inner) masks during usual even cleaning, so it has to be
  // done before reading each such event.  (Which also makes sense, as
  // it does not make sense to clean more than are actually used.
  // Always cleaning all leads to lousy performance, too...)

  // For this to work, there have to be the same amount of masks
  // for all items.

  uint32_t *infos = _infos;

  for (uint32_t k = 0; k < entries_used; k++)
    {
      set_uint32_t mask_offsets;

      for (uint32_t l = _items_per_entry; l; l--)
	{
	  uint32_t info = *(infos++);
	  (infos++);

	  uint32_t mask_off  = *(infos+1);

	  if (info & IND_ITEM_TYPE_IS_MASKED)
	    {
	      infos += 2; // we used a mask

	      mask_offsets.insert(mask_off);
	    }
	}

      if (k == 0)
	_masks_per_entry = (uint32_t) mask_offsets.size();
      else
	{
	  if (_masks_per_entry != (uint32_t) mask_offsets.size())
	    {
	      ERROR("Different number of masks for different entries in list "
		    "(%d != %zd).",
		    _masks_per_entry, mask_offsets.size());
	    }
	}
    }

  if (!_masks_per_entry)
    return;

  _masks = new uint32_t[_masks_per_entry * entries_used];

  infos = _infos;
  uint32_t *masks = _masks;

  for (uint32_t k = 0; k < entries_used; k++)
    {
      set_uint32_t mask_offsets;

      for (uint32_t l = _items_per_entry; l; l--)
	{
	  uint32_t info = *(infos++);
	  (infos++);

	  uint32_t mask_off  = *(infos+1);

	  if (info & IND_ITEM_TYPE_IS_MASKED)
	    {
	      infos += 2; // we used a mask

	      mask_offsets.insert(mask_off);
	    }
	}

      for (set_uint32_t::iterator iter = mask_offsets.begin();
	   iter != mask_offsets.end(); ++iter)
	{
	  *(masks++) = *iter;
	}
    }

  if (DEBUG_STAGING)
    INFO(0,"Masks per entry: %d", _masks_per_entry);
}

void stage_ntuple_block::init_cwn(indexed_item &write_ptrs,
				  type_indices &indices,
				  index_item *&index_write_ptrs,
				  array_item *&array_write_ptrs,
				  /*index2_item *&index2_write_ptrs,*/
				  array2_item *&array2_write_ptrs,
				  stage_ntuple_info &info,
				  char *&ptr)
{
  const char *block = _block.size() ? _block.c_str() : "DEFAULT";

  if (DEBUG_STAGING)
    INFO(0,"Block:   %s",block);

  vect_stage_ntuple_block_item::iterator i_item;

  for (i_item  = _items.begin();
       i_item != _items.end();
       ++i_item)
    {
      stage_ntuple_item_plain *plain_item =
	dynamic_cast<stage_ntuple_item_plain *>((*i_item)._item);
      stage_ntuple_index_var<sni_ind1,sni_vect1> *index1_var =
	dynamic_cast<stage_ntuple_index_var<sni_ind1,
					    sni_vect1> *>((*i_item)._item);
      stage_ntuple_index_var<sni_ind2,sni_vect2> *index2_var =
	dynamic_cast<stage_ntuple_index_var<sni_ind2,
					    sni_vect2> *>((*i_item)._item);

      if (plain_item)
	{
	  plain_item->init_cwn(write_ptrs,indices,info,ptr,block);
	}
      else if (index1_var)
	{
	  if (!(index1_var->_item->_flags &
		NTUPLE_ITEM_IS_ARRAY_MASK))
	    {
	      index_item *index_item = index_write_ptrs++;
	      index1_var->init_cwn(*index_item,info,ptr,block);
	      index_item->setup_mask_list(index_item->_items_used);
	    }
	  else
	    {
	      array_item *array_item = array_write_ptrs++;
	      index1_var->init_cwn(*array_item,info,ptr,block);
	      array_item->setup_mask_list(array_item->_items_used);
	    }
	}
      else if (index2_var)
	{
	  if (!(index2_var->_item->_flags &
		NTUPLE_ITEM_IS_ARRAY_MASK))
	    {
	      ERROR("index_item2 unimplemented");
	      /*
	      index_item2 *index2_item = index2_write_ptrs++;
	      index2_var->init_cwn(*index2_item,info,ptr,block);
	      index_item->setup_mask_list(index2_item->_items_used);
	      */
	    }
	  else
	    {
	      array2_item *array2_item = array2_write_ptrs++;
	      index2_var->init_cwn(*array2_item,info,ptr,block);
	      array2_item->setup_mask_list(array2_item->_items_used);
	    }
	}
      else
	assert(false);
    }
}

/******************************************************************/
