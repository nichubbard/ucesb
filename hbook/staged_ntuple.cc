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

#include "staged_ntuple.hh"
#include "staging_ntuple.hh"

#include "error.hh"

#include <string.h>
#include <ctype.h>

staged_ntuple::staged_ntuple()
{
  _index_item = NULL;
  _array_item = NULL;
  _array_entries = 0;

  _entries_index = 0;
  _entries_array = 0;

  _x_ntuple_type = 0;
  _x_ntuple_opt = 0;

  _struct_index = 0;

  _ext = NULL;
  _external_ext = false;
}

staged_ntuple::~staged_ntuple()
{
  close();
}

void staged_ntuple::set_ext(external_writer *ext)
{
  _ext = ext;
  _external_ext = true;
}

void staged_ntuple::open_x(const char *filename,
			   const char *ftitle,
			   int ntuple_type,
			   int ntuple_opt,
			   int server_port,
			   int timeslice, int timeslice_subdir,
			   int autosave,
			   uint sort_u32_words)
{
  _ext = new external_writer();

  if (!_ext)
    ERROR("Failure allocating external writer.");

  _ext->init_x(ntuple_type,ntuple_opt,
	       filename,ftitle,
	       server_port,
	       !!(ntuple_type & NTUPLE_TYPE_STRUCT_HH),
	       timeslice,timeslice_subdir,autosave);
  _ext->send_file_open(sort_u32_words);

  _x_ntuple_type = ntuple_type;
  _x_ntuple_opt = ntuple_opt;
}

void fix_case_none(char *) { }
void fix_case_upper(char *p)
{
  for ( ; *p; p++)
    {
      assert(*p != ':' && *p != '/');
      *p = (char) toupper(*p);
    }
}
void fix_case_lower(char *p)
{
  for ( ; *p; p++)
    {
      assert(*p != ':' && *p != '/');
      *p = (char) tolower(*p);
    }
}
void fix_case_h2root(char *p)
{
  if (*p)
    {
      *p = (char) toupper(*p);
      for (p++ ; *p; p++)
	{
	  assert(*p != ':' && *p != '/');
	  assert(*p != '(' && *p != ']');
	  *p = (char) tolower(*p);
	}
    }
}

template<typename Tsni_ind, typename Tsni_vect>
void insert_index_var(stage_ntuple_block *block,
		      ntuple_item *item,
		      const Tsni_ind &index)
{
  /* Find the variable that controls us.
   */

  /* Most likely it is in the list of such.
   */

  stage_ntuple_index_var<Tsni_ind,Tsni_vect>* stage_index_var = NULL;
  {
    vect_stage_ntuple_block_item::iterator i_index_vars;

    for (i_index_vars  = block->_items.begin();
	 i_index_vars != block->_items.end();
	 ++i_index_vars)
      {
	stage_ntuple_index_var<Tsni_ind,Tsni_vect> *index_var =
	  dynamic_cast<stage_ntuple_index_var<Tsni_ind,Tsni_vect> *>((*i_index_vars)._item);

	if (index_var &&
	    index_var->_item->_name == item->_index_var)
	  {
	    stage_index_var = index_var;
	    goto found_index_var;
	  }
      }
  }

  /* Then it has to be in the list of variables, and moved.
   */

  {
    vect_stage_ntuple_block_item::iterator i_items;

    for (i_items  = block->_items.begin();
	 i_items != block->_items.end();
	 ++i_items)
      {
	stage_ntuple_item_plain *plain_item =
	  dynamic_cast<stage_ntuple_item_plain *>((*i_items)._item);

	if (plain_item &&
	    plain_item->_item->_name == item->_index_var)
	  {
	    // If it is to be an index variable, it has to be
	    // an integer with a limit...

	    ntuple_item *index_item = plain_item->_item;

	    if (index_item->_type != ntuple_item::INT &&
		index_item->_type != ntuple_item::UINT)
	      ERROR("Index variable %s not integer (%d).",
		    index_item->_name.c_str(),index_item->_type);

	    if (!(index_item->_flags & NTUPLE_ITEM_HAS_LIMIT))
	      ERROR("Index variable %s does not have limit.",
		    index_item->_name.c_str());

	    if (index_item->_limits._int._min != 0)
	      ERROR("Index variable %s minimum limit (=%d) not 0.",
		    index_item->_name.c_str(),index_item->_limits._int._min);

	    // We upgrade it...

	    Tsni_ind lim_max(index_item->_limits._int._max,
			     item->_max_limit2);

	    stage_index_var =
	      new stage_ntuple_index_var<Tsni_ind,Tsni_vect>((*i_items)._item,
							     lim_max);
	    delete (*i_items)._item;

	    (*i_items)._item = stage_index_var;

	    goto found_index_var;
	  }
      }
  }

  ERROR("Cannot find index variable %s for variable length array %s.",
	item->_index_var.c_str(),item->_name.c_str());

 found_index_var:;
  /* We should add ourselves to the proper list of
   * stage_index_var
   */

  if (index.outside(stage_index_var->_length_max))
    {
      char str_index[32];
      char str_max[32];
      index.dbg_fmt(str_index,sizeof(str_index));
      stage_index_var->_length_max.dbg_fmt(str_max,sizeof(str_max));

      ERROR("Array %s has index %s, "
	    "larger than maximum of index variable %s: %s.",
	    item->_name.c_str(),
	    str_index,
	    stage_index_var->_item->_name.c_str(),
	    str_max);
    }

  stage_index_var->_length_used.expand_end(index);

  stage_ntuple_indexed_var<Tsni_ind,Tsni_vect> *array;
  typename vect_stage_ntuple_indexed_var<Tsni_ind,Tsni_vect>::type::iterator i_array;

  for (i_array =  stage_index_var->_vars.begin();
       i_array != stage_index_var->_vars.end();
       ++i_array)
    {
      if ((*i_array)->_name == item->_name)
	{
	  array = *i_array;
	  goto found_array;
	}
    }

  // Create array

  array = new stage_ntuple_indexed_var<Tsni_ind,Tsni_vect>(item->_name,
				       stage_index_var->_length_max,
				       stage_index_var);
  stage_index_var->_vars.push_back(array);

 found_array:;

  /* Insert our item. */

  array->_items.set(index,item);
}

void staged_ntuple::close()
{
  if (_external_ext)
    {
      _ext = NULL;
      _external_ext = false;
    }
  
  if (_ext)
    {
      _ext->close();
      delete _ext;
      _ext = NULL;
    }
}

void staged_ntuple::stage_x(vect_ntuple_items &listing,
			    int hid,
			    const char *id, const char *title,
			    const char *index_major,
			    const char *index_minor,
			    void *base,
			    uint max_raw_words)
{
  assert(_ext);

  if ((_x_ntuple_type & NTUPLE_TYPE_CWN) &&
      (_x_ntuple_type & NTUPLE_TYPE_ROOT))
    {
      ERROR("Cannot export both HBOOK and ROOT ntuple.");
    }

  ntuple_var_case_fcn fix_case = &fix_case_none;

  if (_x_ntuple_type & NTUPLE_CASE_UPPER)
    fix_case = &fix_case_upper;
  else if (_x_ntuple_type & NTUPLE_CASE_LOWER)
    fix_case = &fix_case_lower;
  else if (_x_ntuple_type & NTUPLE_CASE_H2ROOT)
    fix_case = &fix_case_h2root;

  if (_ext)
    _ext->send_book_ntuple_x(hid,id,title,
			     index_major,index_minor,
			     _struct_index,0,
			     max_raw_words);

  vect_stage_ntuple_blocks blocks;

  /* Go through the variables and put them into the staging array.
   */

  size_t entries = listing.size();

  for (size_t j = 0; j < entries; j++)
    {
      ntuple_item *item = listing[j];
      /*
      printf ("insert: %s (%s) [%s] [%d] fl:%x\n",
	      item->_name.c_str(),
	      item->_block.c_str(),
	      item->_index_var.c_str(),
	      item->_index,
	      item->_flags);
      */
      if (item->_flags & NTUPLE_ITEM_OMIT)
	continue; // item is e.g. a limit2 member

      /* First find out what block it belongs to.
       */

      stage_ntuple_block                 *block = NULL;
      vect_stage_ntuple_blocks::iterator  block_i;

      for (block_i = blocks.begin();
	   block_i != blocks.end(); ++block_i)
	{
	  if ((*block_i)->_block == item->_block)
	    {
	      block = *block_i;
	      goto found_block;
	    }
	}

      block = new stage_ntuple_block(item->_block);
      blocks.push_back(block);
    found_block:;

      /* Ok, now we know what block to put it in.
       * Does this really matter?!?  Hmm, we'll put things
       * on block order in the ntuple them.
       */

      /* Then we need to know if the variable is controlled
       * by someone else.
       */

      if (item->_index_var.size() != 0)
	{
	  if (!item->_ptr_limit2) // 1d array
	    {
	      insert_index_var<sni_ind1,sni_vect1>(block, item,
						   sni_ind1(item->_index));
	    }
	  else // 2d array (multi-hit)
	    {
	      insert_index_var<sni_ind2,sni_vect2>(block, item,
						   sni_ind2(item->_index,
							    item->_index2));
	    }
	}
      else
	{
	  /* It is an normal variable.  Add it to the block.
	   */

	  stage_ntuple_item_plain* stage_item =
	    new stage_ntuple_item_plain(item);

	  block->_items.push_back(stage_item);
	}
    }

  /* Now that the staging areas were built, it is time to
   * 1. count the amount of storage needed.
   * 2. set pointers up, and initialize the CWN ntuple.
   */

  vect_stage_ntuple_blocks::iterator  block_i;
  size_t storage = 0;
  uint32_t index_vars = 0;
  uint32_t array_vars = 0;
  /*int index2_vars = 0;*/
  uint32_t array2_vars = 0;

  for (block_i = blocks.begin();
       block_i != blocks.end(); ++block_i)
    {
      (*block_i)->storage_size(_global_array,storage,
			       index_vars,array_vars,
			       /*index2_vars,*/array2_vars);
    }

  INFO(0,"Storage size: %zd bytes.", storage);

  char *ptr;

  ptr = NULL;
  _array_entries = storage / sizeof(uint32_t); // in principle divided by sizeof uint32_t, but counted as bytes we cannot overflow

  if (_ext)
    _ext->send_alloc_array((uint32_t) storage);

  type_indices global_indices;
  _global_array.alloc(1);

  _index_item = index_vars ? new index_item[index_vars] : NULL;
  _entries_index = index_vars;

  index_item *ptr_index_item = _index_item;

  _array_item = array_vars ? new array_item[array_vars] : NULL;
  _entries_array = array_vars;

  array_item *ptr_array_item = _array_item;
  /*
  _index2_item = index2_vars ? new index2_item[index2_vars] : NULL;
  _entries_index2 = index2_vars;

  index2_item *ptr_index2_item = _index2_item;
  */
  _array2_item = array2_vars ? new array2_item[array2_vars] : NULL;
  _entries_array2 = array2_vars;

  array2_item *ptr_array2_item = _array2_item;

  for (block_i = blocks.begin();
       block_i != blocks.end(); ++block_i)
    {
      stage_ntuple_info info;

      info.ext  = _ext;

      info.fix_case = fix_case;
      info.base_ptr = NULL;
      info.src_base = (char*) base;

      (*block_i)->init_cwn(_global_array,global_indices,
			   ptr_index_item,ptr_array_item,
			   /*ptr_index2_item,*/ptr_array2_item,
			   info,
			   ptr);
    }

  //printf ("0x%zx : %p\n",storage,ptr);

  assert(((char*) NULL) + storage == ptr);
  assert(_index_item + index_vars == ptr_index_item);
  assert(_array_item + array_vars == ptr_array_item);

  // ERROR("Not finished yet!");

  if (_ext)
    {
      size_t size;

      size = (1 + _array_entries +
	      2 * (_entries_index + _entries_array) +
	      4 * (_entries_array2)) *
	sizeof(uint32_t);

      for (size_t i = 0; i < _named_strs.size(); i++)
	{
	  staged_ntuple_named_str &ns = _named_strs[i];

	  uint32_t str_size =
	    _ext->max_named_string_size(strlen(ns._id.c_str()),
					strlen(ns._str.c_str()));
	  if (str_size > size)
	    size = str_size;
	}

      if (size > UINT32_MAX) // Should be a fraction?
	ERROR("Internal error, max message size way too large.");

      _ext->set_max_message_size((uint32_t) size);

      for (size_t i = 0; i < _named_strs.size(); i++)
	{
	  staged_ntuple_named_str &ns = _named_strs[i];

	  _ext->send_named_string(ns._id.c_str(),
				  ns._str.c_str());
	}

      // Send the pointer layout of the data to be sent...

      read_write_ptrs_external w;

      size = (_array_entries +
	      2 * (_entries_index + _entries_array) +
	      4 * (_entries_array2)) *
	sizeof(uint32_t);
      // also see calculation at set_max_message_size()

      //printf ("%d %d %d %d ... -> %zd\n",
      //      _array_entries,
      //      _entries_index, _entries_array,
      //      /*_entries_index,*/ _entries_array2,
      //      size / sizeof (uint32_t));

      if (size > UINT32_MAX) // Should be a fraction?
	ERROR("Internal error, offset size way too large.");

      uint32_t *start = _ext->prepare_send_offsets((uint32_t) size);

      w._p = start;
      w._base_ptr = (char*) NULL;
      w._iter_info = NULL;

      cwn_ptrs_indexed_item(w,&_global_array,1,0);
      /*
      printf ("%zd : %zd (%d)\n",
	      w._p - start,size / sizeof(uint32_t),
	      _entries_array2);
      */
      cwn_ptrs_index_item(w,_index_item,_entries_index);
      cwn_ptrs_array_item(w,_array_item,_entries_array);
      /*cwn_ptrs_index2_item(w,_index2_item,_entries_index2);*/
      cwn_ptrs_array2_item(w,_array2_item,_entries_array2);
      /*
      printf ("array2: %zd : %zd (%d)\n",
	      w._p - start,size / sizeof(uint32_t),
	      _entries_array2);
      */
      assert (w._p == start + size / sizeof(uint32_t));

      _ext->send_offsets_fill(w._p);

      // And then we're done with the setup
    }
}


void staged_ntuple::stage_done()
{
  assert(_ext);

  // Only call when done with all ntuples

  if (_ext)
    _ext->send_setup_done(!!(_x_ntuple_opt & NTUPLE_OPT_READER_INPUT));
}

void staged_ntuple::event(void *base,uint *sort_u32,
			  fill_raw_info *fill_raw)
{
  assert(_ext);

  if (_x_ntuple_type & NTUPLE_TYPE_STRUCT_HH)
    return;

  /* We need to fill the structures holding the data.
   */

  // We need to reorder the data by filling a structure with the
  // data from the internal structures.  Since we now here have a
  // columnwise ntuple, and then can limit the sparse data to only
  // the number of entries needed for each event.  This also makes
  // this loop here O(n) where n is amount of data and not O(m)
  // where m would be the amount of possible data.

  // For each indexed variable, we then loop over all controlled
  // variables and fill them out.  Variables without controlling
  // expressions are filled immediately.

  // We have two kinds of destination variables: int and float both
  // 4-byte.

  if (_ext)
    {
      writer_dest_external w;

      size_t size;

      size = (1 + _array_entries) * sizeof(uint32_t);
      // also see calculation at set_max_message_size()

      assert (size <= UINT32_MAX);

      uint32_t *start =
	_ext->prepare_send_fill_x((uint32_t) size,_struct_index,0,sort_u32,
				  fill_raw ? &fill_raw->_ptr : NULL,
				  fill_raw ? fill_raw->_words : 0);

      if (fill_raw)
	fill_raw->_callback(fill_raw);

      start[0] = htonl(0x40000000); // marker that we're not compacted

      w._p = start + 1;

      cwn_fill_indexed_item(w,&_global_array,base,1,0);

      //printf ("entries array2 : (%d)\n",
      //        _entries_array2);
      //fflush(stdout);

      cwn_fill_index_item(w,_index_item,_entries_index,base);
      cwn_fill_array_item(w,_array_item,_entries_array,base);
      /*cwn_fill_index2_item(w,_index2_item,_entries_index2,base);*/
      cwn_fill_array2_item(w,_array2_item,_entries_array2,base);

      _ext->send_offsets_fill(w._p);
    }
}

bool staged_ntuple::get_event()
{
  assert(_ext);

  if (_x_ntuple_type & NTUPLE_TYPE_STRUCT_HH)
    return false;

  if (_ext)
    {
      // First, get the next message from the queue

      uint32_t *start;
      uint32_t *end;

      uint32_t request = _ext->get_message(&start,&end);

      switch (request)
	{
	case EXTERNAL_WRITER_BUF_DONE:
	  return false; // We're done
	case EXTERNAL_WRITER_BUF_ABORT:
	  ERROR("External reader aborted.");
	  return false;
	case EXTERNAL_WRITER_BUF_NTUPLE_FILL:
	  // We got an event;
	  break;
	default:
	  ERROR("Unexpected message %d from external reader.",request);
	  return false;
	}
      /*
      printf ("msg %08x start %p end %p\n", request, start, end);

      printf ("msg %08x start %p end %p [%08x] [%08x]\n",
	      request, start, end, ntohl(start[0]), ntohl(start[1]));
      */
      if (end < start+3 ||
	  ntohl(start[0]) != 0 || // struct_index != 0
	  ntohl(start[1]) != 0 || // ntuple_index != 0
	  ntohl(start[2]) != 0x40000000)   // non-packed
	ERROR("Malformed event message from external reader.");


      // start[0] = ntuple_index;
      // start[1] = 0; // marker that we're not compacted

      _ext_r._p = start + 3;
      _ext_r._end = end;

      return true;
    }
  return false;
}

void staged_ntuple::unpack_event(void *base)
{
  assert(_ext);

  if (_x_ntuple_type & NTUPLE_TYPE_STRUCT_HH)
    return;

  if (_ext)
    {
      // The message has already been fetched.  Just unpack!

      assert(_ext_r._p);

      // start[0] = 0; // marker that we're not compacted

      cwn_get_indexed_item(_ext_r,&_global_array,base,1,0);

      cwn_get_index_item(_ext_r,_index_item,_entries_index,base);
      cwn_get_array_item(_ext_r,_array_item,_entries_array,base);

      if (_ext_r._p != _ext_r._end)
	ERROR("Event message from external reader not completely consumed.");

      _ext->message_body_done(_ext_r._end);

      _ext_r._p = NULL;
    }
}

