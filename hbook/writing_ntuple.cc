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

#include <stdint.h>

#include "error.hh"

#include "writing_ntuple.hh"

#include <math.h>

#ifndef ISFINITE
#define ISFINITE(x) isfinite(x)
#endif
#ifndef SQR
#define SQR(x) ((x)*(x))
#endif

#define DEBUG_WRITING 0

// Plan for the type flags of each item:
//
// We will need to branch on most of the flags.  A switch statement
// could map to a indirect branch (table), which only has good
// prediction if going the same way as last time.  A decision tree may
// be more efficient, as the jumps within each decision has a higher
// chance of being predictable.
//
// We must branch on the sqrt / inv operations, as those 'depend' on
// the values being suitable.  I.e., it is not healthy to
// speculatively do a sqrt on a negative value.
//
// The bitmasks are not good for branching, but there is not much to
// do.  We may expect overflows to be seldom, so only handle them in
// case the valid flag was not found.
//
// We only deliver floats to the ntuple stream, so may join floats and
// doubles at the start, by doing a conversion of float to double.
// (we should however not read the variables into floating point
// variables unless the valid mask is set, converting or just loading
// 'ugly' values into FP registers may be an unoptimised instruction.)

// We keep src, type and mask (as well as dest) in separate arrays as
// they have different alignment requirements (particularly with
// 64-bit pointers)

// New plan!  (partially): we operate strictly from statically
// allocated arrays.  They are not (NEVER) extremely large, so we can
// use a base pointer (which we will anyhow need when having many
// events in-flight simultaneously) and an offset.  The offset does
// not need full 32 bits (would be 4 GB events).  So we may encode the
// offset together with the flags marking the type of item.  The
// offset need either be shifted or anded to rid the flags.  Let's
// shift it down.  That way, we can keep the flags at low bits.

// Currently, this is 1<<(32-5)=1<<(27)=128MB

// Same trick with the masks, using 5 bits to store which bit is used
// by the mask.  I.e. one extra word, with an offset and a bit
// location.

// As overflow is not always present, we need to be able to shift that
// out completely.  For the overflow mark, working along these lines,
// one may think of two alternatives:
//
// a) Use another word, offset+location.  Either use one extra bit
//    in the location to shift the bit-mask out completely, to mark the
//    non-existent overflow word.  Or use one extra bit flag in the
//    type field above to mark the presence of overflows or not.
//
// b) Put the location of the overflow in the valid bit-mask.  This then
//    needs 5 bits for valid location and 5 bits for overflow location.
//    5 for overflow is enough by requiring the overflow to be located
//    above the valid.  As that gives an extra shift of one, we can shift
//    out in 31 shifts.  This leaves 22 bits for the offset, 4 MB.

// 4 MB static events requires something like (assume 16 words per
// channel) 64000 channels.  Could be with silicons, but such crazy
// things likely either has fewer words per channel.  Or at such event
// sizes, one would start to use more base pointers for a limited
// amount of dynamic memory allocation.  Let's go for option b!

// We still keep the destinations in a separate array, as that is not
// used eventwise (to keep cache-pollution down).  Cache-pollution is
// also reduced by interleaving the mask information in the
// info-array.  But we require the info array to always have a dummy
// item at the end, such that the mask address can be retrieved
// speculatively.  (I.e. before branching on having a mask).

// Update: plan b turned out to be too restrictive, so rehash the code
// for plan a, i.e. separate the info bitmask from the offset.  This
// then allows for 4 GB structures.

/*****************************************************************************/

uint32_t cwn_fill_indexed_item(writer_dest_external &w,indexed_item *array,
			       void *base,
			       uint32_t entries,uint32_t offset,
			       uint32_t limit_index)
{
  uint32_t    *infos = array->_infos + array->_info_slots_per_entry * offset;

  for (uint32_t k = 0; k < entries; k++)
    for (uint32_t l = array->_items_per_entry; l; l--)
      {
	uint32_t info = *(infos++);
	uint32_t off  = *(infos++);

	uint32_t mask_info = *(infos);
	uint32_t mask_off  = *(infos+1);

	uint32_t* mask_ptr = (uint32_t*) (((char *) base) + mask_off);

	uint32_t mask_valid = 1;
	uint32_t mask_ofl   = 0;

	if (info & IND_ITEM_TYPE_IS_MASKED)
	  {
	    uint32_t mask = *mask_ptr;

	    uint32_t mask_bit_valid = 1 << (mask_info & 0x1f);
	    uint32_t mask_bit_overflow = 2u << ((mask_info >> 5) & 0x1f);

	    infos += 2; // we used a mask

	    mask_valid = mask & mask_bit_valid;
	    mask_ofl   = mask & mask_bit_overflow;
	  }

	void *src = ((char *) base) + off;

	if (info & IND_ITEM_TYPE_FLOAT) // floating point
	  {
	    if (DEBUG_WRITING) {
	      if (info & IND_ITEM_TYPE_IS_MASKED)
		INFO(0,"info: %08x (%08x) v:%d o:%d (%.6f) (%p)(%08x)",
		     info,mask_info,mask_valid,mask_ofl,
		     (info & IND_ITEM_TYPE_FLOAT_DOUBLE) ?
		     *((double *) src) : (double) *((float *) src),
		     mask_ptr,*mask_ptr);
	      else
		INFO(0,"info: %08x (%08x) (%.6f)",
		     info,mask_info,
		     (info & IND_ITEM_TYPE_FLOAT_DOUBLE) ?
		     *((double *) src) : (double) *((float *) src));
	    }

	    if (!mask_valid)
	      {
		if (mask_ofl)
		  w.write_nan(); // 0x7f800000
		else
		  w.write_inf(); // 0x7fc00000
	      }
	    else
	      {
		double val;

		if (info & IND_ITEM_TYPE_FLOAT_DOUBLE)
		  val = *((double *) src);
		else
		  val = *((float *) src);

		if (info & IND_ITEM_TYPE_FLOAT_SQRT)
		  val = sqrt(val);
		if (info & IND_ITEM_TYPE_FLOAT_INV)
		  val = 1.0 / val;

		w.write_float((float) val);
	      }
	  }
	else // integer
	  {
	    uint32_t val;

	    if (info & IND_ITEM_TYPE_INT_USHORT)
	      val = *((uint16_t *) src);
	    else if (!(info & IND_ITEM_TYPE_INT_UCHAR))
	      val = *((uint32_t *) src);
	    else
	      val = *((uint8_t *) src);

	    if (DEBUG_WRITING) {
	      if (info & IND_ITEM_TYPE_IS_MASKED)
		INFO(0,"info: %08x (%08x) v:%d a:%d (%d) (%p)(%08x)",
		     info,mask_info,mask_valid,
		     info & IND_ITEM_TYPE_INT_ADD_1,
		     val,
		     mask_ptr,*mask_ptr);
	      else
		INFO(0,"info: %08x (%08x) a:%d (%d)",
		     info,mask_info,
		     info & IND_ITEM_TYPE_INT_ADD_1,
		     val);
	    }

	    val += (info & IND_ITEM_TYPE_INT_ADD_1); // is in bit 1

	    if (limit_index && (info & IND_ITEM_TYPE_INT_INDEX_CUT))
	      if (val > limit_index)
		{
		  assert (l == array->_items_per_entry);
		  return k;
		}

	    if (!mask_valid) // reading the integer before was harmless
	      val = 0;

	    w.write_int(val);
	  }
      }
  return entries;
}


void cwn_ptrs_indexed_item(read_write_ptrs_external &w,indexed_item *array,
			   uint32_t entries,uint32_t offset)
{
  uint32_t    *infos = array->_infos + array->_info_slots_per_entry * offset;
  void       **dests = array->_dests + array->_items_per_entry * offset;

  for (uint32_t k = entries * array->_items_per_entry; k; k--)
    {
      uint32_t info = *(infos++);
      infos++; // off

      void *dest = *(dests++);

      if (info & IND_ITEM_TYPE_IS_MASKED)
	  infos += 2; // we used a mask

      if (info & IND_ITEM_TYPE_FLOAT) // floating point
	{
	  w.dest_float((float *) dest);
	}
      else // integer
	{
	  w.dest_int((uint32_t *) dest);
	}
    }
}

void cwn_get_indexed_item(reader_src_external &w,indexed_item *array,
			  void *base,uint32_t entries,uint32_t offset)
{
  uint32_t    *masks = array->_masks + array->_masks_per_entry * offset;

  for (uint32_t k = entries * array->_masks_per_entry; k; k--)
    {
      uint32_t mask_off = *(masks++);

      uint32_t* mask_ptr = (uint32_t*) (((char *) base) + mask_off);

      *mask_ptr = 0;
    }

  uint32_t    *infos = array->_infos + array->_info_slots_per_entry * offset;

  for (uint32_t k = entries * array->_items_per_entry; k; k--)
    {
      uint32_t info = *(infos++);
      uint32_t off  = *(infos++);

      uint32_t mask_info = *(infos);
      uint32_t mask_off  = *(infos+1);

      uint32_t dummy_mask; // write here if not IND_ITEM_TYPE_IS_MASKED
      uint32_t* mask_ptr = &dummy_mask;

      uint32_t mask_bit_valid = 0;
      uint32_t mask_bit_overflow = 0;

      if (info & IND_ITEM_TYPE_IS_MASKED)
	{
	  mask_ptr = (uint32_t*) (((char *) base) + mask_off);

	  mask_bit_valid = 1 << (mask_info & 0x1f);
	  mask_bit_overflow = 2u << ((mask_info >> 5) & 0x1f);
	  infos += 2; // we used a mask
	}

      void *src = ((char *) base) + off;

      if (info & IND_ITEM_TYPE_FLOAT) // floating point
	{
	  float_uint32_t_type_pun valf;

	  w.read_float(valf);

	  if (mask_bit_valid)
	    {
	      // We do a mapping: finite values -> stored as usual and
	      // marked value.  Else: if we can mark overflows,
	      // overflows will be marked such.  Else: mark invalid.

	      // Check that the value is finite

	      if (!ISFINITE(valf._f))
		{
		  // Value not good.  We will not mark valid, and we
		  // will not do any further processing for this item
		  // except checking for overflow

#define ISPLUSINF(valf) (valf == 0x7fc00000)

		  if (!ISPLUSINF(valf._i))
		    *mask_ptr |= mask_bit_overflow;

		  continue; // next item!
		}

	      // Item good!
	      *mask_ptr |= mask_bit_valid;
	    }

	  double val = valf._f;

	  if (info & IND_ITEM_TYPE_FLOAT_SQRT)
	    val = SQR(val);
	  if (info & IND_ITEM_TYPE_FLOAT_INV)
	    val = 1.0 / val;

	  if (info & IND_ITEM_TYPE_FLOAT_DOUBLE)
	    *((double *) src) = val;
	  else
	    *((float *) src) = (float) val;
	}
      else // integer
	{
	  uint32_t val;

	  w.read_int(val);

	  // We do not care about the destination size when deciding
	  // if a value should get a valid marker or not.  (A similar
	  // thing actually goes for the unpacking - it does not mark
	  // zeros actually read out as not valid=present.)

	  *mask_ptr |= (val != 0 ? mask_bit_valid : 0);

	  val -= (info & IND_ITEM_TYPE_INT_ADD_1); // is in bit 1

	  if (info & IND_ITEM_TYPE_INT_USHORT)
	    *((uint16_t *) src) = (uint16_t) val;
	  else if (!(info & IND_ITEM_TYPE_INT_UCHAR))
	    *((uint32_t *) src) = (uint32_t) val;
	  else
	    *((uint8_t *) src)  = (uint8_t) val;
	}
    }
}

/*****************************************************************************/

void cwn_fill_index_item(writer_dest_external &w,index_item *array,
			 uint32_t entries,void *base)
{
  for (uint32_t i = 0; i < entries; i++)
    {
      // copy the index variable.

      index_item *item = &array[i];

      uint32_t *src = (uint32_t *) (((char *) base) + item->_src_offset);
      uint32_t items = *src;

      if (items > item->_items_max)
	ERROR("Internal error rewriting array for CWN filling "
	      "(index too large %d > %d).",
	      items,item->_items_max);
      if (items > item->_items_used)
	items = item->_items_used;

      if (DEBUG_WRITING) {
	INFO(0,"items: %d",
	     items);
      }

      //printf ("%08x  %08x in: %08x\n",(int) item->_src,(int) item->_dest,items);

      // Since the protocol need to have the number of items first, we
      // must reserve space for this, and write it at the end
      uint32_t *dest_items = w.write_int_dest();

      items = cwn_fill_indexed_item(w,item,base,items,0,item->_items_used);

      // Write the number of items
      w.write_int_to_dest(dest_items,items);
    }
}

void cwn_get_index_item(reader_src_external &w,index_item *array,
			uint32_t entries,void *base)
{
  for (uint32_t i = 0; i < entries; i++)
    {
      uint32_t items;

      index_item *item = &array[i];

      w.read_int(/*(item->_dest),*/items);

      if (items > item->_items_used)
	ERROR("Error reading array from CWN "
	      "(index too large %d > %d).",
	      items,item->_items_used);

      uint32_t *src = (uint32_t *) (((char *) base) + item->_src_offset);
      *src = items;

      cwn_get_indexed_item(w,item,base,items,0);
    }
}

void cwn_ptrs_index_item(read_write_ptrs_external &w,index_item *array,
			 uint32_t entries)
{
  for (uint32_t i = 0; i < entries; i++)
    {
      index_item *item = &array[i];

      w.dest_int_ctrl((item->_dest),item->_items_used);

      cwn_ptrs_indexed_item(w,item,item->_items_used,0);

      w.ctrl_over();
    }
}

/*****************************************************************************/

void cwn_fill_array_item(writer_dest_external &w,array_item *array,
			 uint32_t entries,void *base)
{
  for (uint32_t i = 0; i < entries; i++)
    {
      // Since the protocol need to have the number of items first, we
      // must reserve space for this, and write it at the end
      uint32_t *dest_items = w.write_int_dest();

      array_item *item = &array[i];

      // First we need to go through the array to see how many entries we have

      uint32_t items = 0;

      const unsigned long *src_bits =
	(const unsigned long *) (((char *) base) + item->_src_bits_offset);
      uint32_t used_items = item->_items_used;

      uint32_t index_base = 0;

      /*int *indices = item->_dest+1;*/

      for ( ; index_base < used_items;
	    index_base += (uint32_t) sizeof(unsigned long) * 8)
	{
	  unsigned long bits = *(src_bits++);
	  uint32_t index = index_base;

	  while (bits)
	    {
	      while (!(bits & 0xff)) { bits >>= 8; index += 8; }
	      if (!(bits & 0x0f)) { bits >>= 4; index += 4; }
	      if (!(bits & 0x03)) { bits >>= 2; index += 2; }
	      if (!(bits & 0x01)) { bits >>= 1; index += 1; }

	      // so bits & 1

	      // item @index is there

	      if (index >= item->_items_used) // test only needed for last item, but (this is still the fastest we can do (makes no sense to test if we are last.  That's just another test...))...
		{
		  if (index < item->_items_max)
		    break;
		  ERROR("Internal error rewriting masked array for "
			"CWN filling (index too large %d >= %d).",
			index,item->_items_max);
		}

	      bits >>= 1;
	      uint32_t use_index = index++;

	      // Set the index after the addition, since we want 1 added!
	      w.write_int(/*(indices++),*/index);

	      cwn_fill_indexed_item(w,item,base,1,use_index);

	      items++;
	    }
	}

      // Write the number of items
      w.write_int_to_dest(/*(item->_dest),*/dest_items,items);
    }
}

void cwn_get_array_item(reader_src_external &w,array_item *array,
			uint32_t entries,void *base)
{
  for (uint32_t i = 0; i < entries; i++)
    {
      array_item *item = &array[i];

      unsigned long *src_bits =
	(unsigned long *) (((char *) base) + item->_src_bits_offset);

      uint32_t items;

      w.read_int(/*(item->_dest),*/items);

      //int *indices = item->_dest+1;

      for (uint32_t k = 0; k < items; k++)
	{
	  uint32_t index;

	  w.read_int(/*(indices++),*/index);

	  if (index > item->_items_used)
	    ERROR("Error reading masked array from CWN "
		  "(index too large %d > %d).",
		  index,item->_items_used);

	  // index of the item is index - 1

	  index--;

	  *(src_bits + (index / (sizeof(unsigned long) * 8))) |=
	    ((unsigned long) 1) << (index % (sizeof(unsigned long) * 8));
	  /*
	  printf ("[%3d] %16p + %d : %16lx --> %16lx\n",
		  index,src_bits,(int) (index / (sizeof(unsigned long) * 8)),
		  ((unsigned long) 1) << (index % (sizeof(unsigned long) * 8)),
		  *(src_bits + (index / (sizeof(unsigned long) * 8))));
	  */
	  cwn_get_indexed_item(w,item,base,1,index);
	}
    }
}

void cwn_ptrs_array_item(read_write_ptrs_external &w,array_item *array,
			 uint32_t entries)
{
  for (uint32_t i = 0; i < entries; i++)
    {
      array_item *item = &array[i];

      uint32_t used_items = item->_items_used;

      w.dest_int_ctrl((item->_dest),used_items);

      uint32_t *indices = item->_dest+1;

      for (uint32_t k = 0; k < used_items; k++)
	{
	  w.dest_int((indices++)/*,k*/);

	  cwn_ptrs_indexed_item(w,item,1,k);
	}
      w.ctrl_over();
    }
}

/*****************************************************************************/

void cwn_fill_array2_item(writer_dest_external &w,array2_item *array,
			  uint32_t entries,void *base)
{
  for (uint32_t i = 0; i < entries; i++)
    {
      //printf ("cfa2i: [%d]\n",i);

      // Since the protocol need to have the number of items first, we
      // must reserve space for this, and write it at the end
      uint32_t *dest_items2 = w.write_int_dest();

      array2_item *item = &array[i];

      // First we need to go through the array to see how many entries we have

      uint32_t items1 = 0;
      uint32_t items2 = 0;

      const unsigned long *src_bits =
	(const unsigned long *) (((char *) base) + item->_src_bits_offset);
      uint32_t used_items = item->_items_used;

      uint32_t index_base = 0;

      reader_src_external ie;

      ie._p = w._p;

      for ( ; index_base < used_items;
	    index_base += (uint32_t) sizeof(unsigned long) * 8)
	{
	  unsigned long bits = *(src_bits++);
	  uint32_t index = index_base;

	  while (bits)
	    {
	      while (!(bits & 0xff)) { bits >>= 8; index += 8; }
	      if (!(bits & 0x0f)) { bits >>= 4; index += 4; }
	      if (!(bits & 0x03)) { bits >>= 2; index += 2; }
	      if (!(bits & 0x01)) { bits >>= 1; index += 1; }

	      // so bits & 1

	      if (index >= item->_items_used)
		{
		  if (index < item->_items_max)
		    break;
		  ERROR("Internal error rewriting masked array2 for "
			"CWN filling (index too large %d >= %d).",
			index,item->_items_max);
		}

	      bits >>= 1;

	      // how many items of this one?

	      //printf ("IDX: %d\n",index);
	      //fflush(stdout);

	      uint32_t* num_items2_ptr =
		(uint32_t*) (((char *) base) + item->_num_items2_offset[index]);

	      index++;

	      uint32_t items_this = *num_items2_ptr;

	      items1 += items_this;

	      // Set the index after the addition, since we want 1 added!
	      w.write_int(/*(indices++),*/index);
	      w.write_int(/*(endloc++),*/items1);

	      items2++;
	    }
	}

      if ((uint32_t) items1 > item->_items_max * item->_items_max2)
	{
	  ERROR("Internal error rewriting masked array2 for "
		"CWN filling (total used too large %d > %d*%d = %d).",
		items1,
		item->_items_used,item->_items_used2,
		item->_items_used * item->_items_used2);
	}

      w.write_int_to_dest(/*(item->_dest),*/dest_items2,items2);

      // Number of real items (control item)
      w.write_int(items1);

      // Since we already have worked through the zero-suppression
      // lists, it is faster to get the indices and number of
      // data from the produced lists.

      uint32_t start = 0;

      for (uint32_t j = items2; j; j--)
	{
	  uint32_t use_index;
	  uint32_t end;

	  ie.read_int(use_index);
	  ie.read_int(end);

	  use_index--;
	  uint32_t items_this = end - start;
	  start = end;

	  //printf ("cfa2i: ..[%d] %d %d\n",i,use_index,items_this);
	  //fflush(stdout);

	  cwn_fill_indexed_item(w,item,base,
				items_this,use_index * item->_items_used2);
	}
    }
}

void cwn_ptrs_array2_item(read_write_ptrs_external &w,array2_item *array,
			  uint32_t entries)
{
  //printf ("%p\n",w._p);

  for (uint32_t i = 0; i < entries; i++)
    {
      array2_item *item = &array[i];

      /* First the array with indices and end location. */

      uint32_t used_items = item->_items_used;
      uint32_t used_items2 = item->_items_used2;

      w.dest_int_ctrl((item->_dest2),used_items);

      uint32_t *indices = item->_dest2+1;
      uint32_t *endloc  = indices+used_items;

      //printf ("%p\n",w._p);
      for (uint32_t k = 0; k < used_items; k++)
	{
	  w.dest_int((indices++)/*,k*/);
	  w.dest_int((endloc++)/*,k*/);
	}
      w.ctrl_over();

      //printf ("%p\n",w._p);
      /* Then the actual items. */


      //printf ("%p\n",w._p);
      w.dest_int_ctrl((item->_dest),used_items * used_items2);

      for (uint32_t k = 0; k < used_items; k++)
	{
	  cwn_ptrs_indexed_item(w,item,
				item->_items_used2,k * item->_items_used2);
	}
      w.ctrl_over();
      //printf ("%p\n",w._p);
    }
}

/*****************************************************************************/
