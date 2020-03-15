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

#include "definitions.hh"
#include "unpack_code.hh"
#include "parse_error.hh"

#include <assert.h>
#include <limits.h>

bool struct_unpack_code::get_match_bits(const struct_item* item,dumper &d,const arguments *args,
					match_info &bits)
{
  const struct_data*    data;
  //const struct_decl*    decl;
  //const struct_list*    list;
  //const struct_select*  select;
  //const struct_several* several;
  //const struct_cond*    cond;

  if ((data    = dynamic_cast<const struct_data*   >(item))) return get_match_bits(data   ,d,args,bits);
  //if ((decl    = dynamic_cast<const struct_decl*   >(item))) return get_match_bits(decl   ,d,args,bits);
  //if ((list    = dynamic_cast<const struct_list*   >(item))) return get_match_bits(list   ,d,args,bits);
  //if ((select  = dynamic_cast<const struct_select* >(item))) return get_match_bits(select ,d,args,bits);
  //if ((cond    = dynamic_cast<const struct_cond*   >(item))) return get_match_bits(cond   ,d,args,bits);

  return false;
}


bool struct_unpack_code::get_match_bits(const struct_decl*    decl,dumper &d,
					match_info &bits)
{
  struct_definition *str_decl;

  str_decl = find_named_structure(decl->_ident);

  if (!str_decl)
    ERROR_LOC(decl->_loc,"Structure %s not defined.",decl->_ident);

  if (str_decl->_opts & STRUCT_DEF_OPTS_EXTERNAL)
    return false; // cannot calculate match bits for external unpacker

  // get match bits from structure body

  const struct_header_named *named_header =
    dynamic_cast<const struct_header_named *>(str_decl->_header);

  arguments args(decl->_args,
		 named_header ? named_header->_params : NULL,
		 NULL);

  return get_match_bits(str_decl->_body,d,&args,bits);
}

bool struct_unpack_code::get_match_bits(const struct_item_list *list,dumper &d,const arguments *args,
					match_info &bits)
{
  // Get match bits from first entry in list, except for
  // member entries, which do not unpack into anything

  struct_item_list::const_iterator i;

  i = list->begin();

  for (i = list->begin(); i < list->end(); ++i)
    {
      // We now need to call the correct function per the type

      const struct_item *item = *i;

      if ((dynamic_cast<const struct_member* >(item)))
	continue; // look for next one
      if ((dynamic_cast<const struct_mark* >(item)))
	continue; // look for next one
      if ((dynamic_cast<const struct_encode* >(item)))
	continue; // look for next one

      return get_match_bits(item,d,args,bits);
    }

  return false;
}

bool struct_unpack_code::get_match_bits(const struct_data *data,dumper &d,const arguments *args,
					match_info &bits)
{
  // we've found a data member.
  // figure out what bits are fixed, and what value they have

  bits._size = data->_size;

  d.text_fmt(" (s%d)",data->_size);

  if (data->_size > 32)
    return false; // bits._mask and bits._value cannot handle this

  if (data->_flags & SD_FLAGS_OPTIONAL)
    {
      d.text_fmt(" optional");
     // Item is optional, so in principle we should also try the next
      // one.  Code currently only handles one bit-pattern per
      // structure to match.  Just adding more match_info structs not
      // enough, as they must be merged when doing spurious checks
      // also...  (i.e. two may actually just be one...)
      return false;
    }

  // _ident we do not care about
  // _flags (for now), only has the NOENCODE marker, which we do not care about

  bits._mask  = 0; // no fixed bits found (yet)
  bits._value = 0; // no bits

  // first check that we have a bit field

  if (!data->_bits)
    return true;

  // now loop over the bits specification, and figure everything out!

  bits_spec_list::const_iterator i;
  uint32 unnamed_bits = (data->_size == 32 ? 0xffffffff : (data->_size == 16 ? 0xffff : 0xff));

  for (i = data->_bits->begin(); i != data->_bits->end(); ++i)
    {
      bits_spec *b = *i;

#define NUM_BITS_MASK(bits) ((bits) == 32 ? 0xffffffff : ((1 << (bits)) - 1))

      unnamed_bits &= ~( NUM_BITS_MASK(b->_max+1) ^ NUM_BITS_MASK(b->_min));

      if (b->_cond)
	{
	  /*
	  d.text_fmt("// bits_%d_%d",
		     b->_min,b->_max);
	  */
	  uint32 fixed_mask = 0;
	  uint32 fixed_value = 0;
	  /*
	  char name[512];

	  if (b->_name)
	    sprintf(name,"%s%s.%s",prefix,data->_ident,b->_name);
	  else
	    sprintf(name,"%s%s.unnamed_%d_%d",prefix,data->_ident,b->_min,b->_max);

	  b->_cond->check(d,check_prefix,name);
	  */

	  b->_cond->get_match(d,args,&fixed_mask,&fixed_value);

	  // first shift the two values into their location

	  fixed_mask  <<= b->_min;
	  fixed_value <<= b->_min;

	  // then clamp away any bits that are outside us (can only be on high side)

	  fixed_mask  &= NUM_BITS_MASK(b->_max+1);
	  fixed_value &= NUM_BITS_MASK(b->_max+1);

	  // Now, whatever is fixed, we can add into our global mask

	  bits._mask  |= fixed_mask;
	  bits._value |= fixed_value;
	  /*
	  d.text_fmt("(0x%0*x,0x%0*x)\n",
		     data->_size/4,fixed_mask,
		     data->_size/4,fixed_value);
	  */
	}
    }
  /*
  if (unnamed_bits)
    d.text_fmt("// unnamed_zero(0x%0*x)",
	       data->_size/4,unnamed_bits);
  */
  bits._mask  |= unnamed_bits;
  bits._value |= 0; // not really needed :-)

  d.text_fmt(" => (0x%0*x,0x%0*x)",
	     data->_size/4,bits._mask,
	     data->_size/4,bits._value);

  return true;
}



void struct_unpack_code::gen_match_decl_quick(const std::vector<match_info> &infos,
					      dumper &d,int size,
					      const char *abort_spurious_label)
{
  // First try to find a bit which is in use by all items, and then
  // among those (if they are several), find the one which partitions
  // the set of data the most, i.e. has the most even distribution of
  // having ones and zeros in the different items

  if (infos.size() == 1)
    {
      const match_info &info = infos[0];

      // the item that is left is the one we want

      if (abort_spurious_label)
	gen_check_spurios_match(info._decl,d,abort_spurious_label);
      gen_unpack_decl(info._decl,"UNPACK_DECL",d,true);

      d.text_fmt("continue;\n");
      return;
    }

  assert(size <= 32);

  int not_one_bit[32][2];

  memset(not_one_bit,0,sizeof(not_one_bit));

  for (unsigned int i = 0; i < infos.size(); i++)
    {
      const match_info &info = infos[i];

      for (int b = 0; b < size; b++)
	{
	  if (info._mask & (1 << b))
	    not_one_bit[b][(info._value >> b) & 1]++;
	}
    }

  int select_bit = -1;
  int select_bit_min_discard = 0;
  int select_bit_total_discard = 0;

  // now find the bit which we want, even if the bit does not separate
  // some items, the important thing would be for it to divide the
  // items as evenly as possible.  begin from above, to favour high
  // order bits

  for (int b = size-1; b >= 0; b--)
      {
	// int discard_by_1 = infos.size() - one_bit[b][1];
	// int discard_by_0 = infos.size() - one_bit[b][0];

	// which discards the fewest

	int min_discard;

	if (not_one_bit[b][1] < not_one_bit[b][0])
	  min_discard = not_one_bit[b][1];
	else
	  min_discard = not_one_bit[b][0];

	int total_discard = not_one_bit[b][1] + not_one_bit[b][0];

	// this bit is better if it in the least discarding partition
	// discards more than the previous selected bit

	// it must do some partition! (or better: some discarding)
	// guaranteed by the >, and init of select_bit_min_discard to 0

	if (min_discard > select_bit_min_discard ||
	    (min_discard == select_bit_min_discard &&
	     total_discard > select_bit_total_discard))
	  {
	    select_bit = b;
	    select_bit_min_discard = min_discard;
	    select_bit_total_discard = total_discard;
	  }
      }

  if (select_bit != -1 && infos.size() > 3)
    {
      d.text_fmt("// select on bit %d, partition: 1:%d(d%d) 0:%d(d%d)\n",
		 select_bit,
		 (int)infos.size()-not_one_bit[select_bit][1],
		 not_one_bit[select_bit][1],
		 (int)infos.size()-not_one_bit[select_bit][0],
		 not_one_bit[select_bit][0]);

      std::vector<match_info> sel_infos[2];

      for (unsigned int i = 0; i < infos.size(); i++)
	{
	  const match_info &info = infos[i];

	  if (!(info._mask & info._value & (1 << select_bit)))
	    sel_infos[0].push_back(info);
	  if (!(info._mask & ~info._value & (1 << select_bit)))
	    sel_infos[1].push_back(info);
	}

      d.text_fmt("if (__match_peek & 0x%0*x) {\n",size/4,1 << select_bit);
      {
	dumper sd(d,2);
	gen_match_decl_quick(sel_infos[1],sd,size,abort_spurious_label);
      }
      d.text("} else {\n");
      {
	dumper sd(d,2);
	gen_match_decl_quick(sel_infos[0],sd,size,abort_spurious_label);
      }
      d.text("}\n");
    }
  else
    {
      // If any of the possible items may be apparently overlapping,
      // i.e. if we even by checking all known bits may not be able to
      // determine the single successful item, then we must call the
      // full __match() functions, and not produce an error that
      // several items were eligible

      // The only way to check this is to inspect all pairs...  Every
      // pair must differ in at least one bit on their each common,
      // mask, or trouble ahead...

      std::vector<bool> ambigous(infos.size(),false);

      for (unsigned int i = 0; i < infos.size()-1; i++)
	{
	  const match_info &info1 = infos[i];

	  for (unsigned int j = i+1; j < infos.size(); j++)
	    {
	      const match_info &info2 = infos[j];

	      if (!((info1._mask  & info2._mask) &
		    (info1._value ^ info2._value)))
		{
		  // items i and j not distinguishable...

		  d.text_fmt("// Indistinguishable: %d %d\n",i,j);

		  ambigous[i] = true;
		  ambigous[j] = true;
		}
	    }
	}


      for (unsigned int i = 0; i < infos.size(); i++)
	{
	  const match_info &info = infos[i];
	  const struct_decl *decl = info._decl;

	  if (ambigous[i])
	    d.text("VERIFY_");
	  d.text_fmt("MATCH_DECL_QUICK(%d,__match_no,%d,",
		     decl->_loc._internal,info._index);
	  decl->_name->dump(d);
	  d.text_fmt(",__match_peek,0x%0*x,0x%0*x",
		     size/4,info._mask,
		     size/4,info._value);
	  if (ambigous[i])
	    {
	      const struct_header_named *named_header = find_decl(decl,false);

	      d.text(",");
	      d.text(decl->_ident);
	      dump_param_args(decl->_loc,named_header->_params,decl->_args,d,false);
	    }
	  d.text(");\n");
	}
    }
}




bool struct_unpack_code::gen_optimized_match(const file_line &loc,
					     const struct_decl_list *items,
					     dumper &d,
					     const char *abort_spurious_label,
					     bool last_subevent_item)
{
  // We only handle a certain (but common) subset of matching possibilites.
  //
  // The candidates must all want to read the same kind of data word
  // (uint8/uint16/uint32)
  //
  // We can only separate on bits that are fixed.  No ranges!  Ranges
  // only in the sense that we mark all bits in the range as unknown,
  // so they cannot select.  We only select if the ranges are
  // 'bit-aligned' and disjoint between the candidates.

  // First find out what bits we know are fixed for specific modules

  if (!items->size())
    return false;

  std::vector<match_info> infos(items->size());

  {
    struct_decl_list::const_iterator i;
    size_t index = 0;
    for (i = items->begin(); i != items->end(); ++i, ++index)
      {
	struct_decl *decl = *i;

	// const struct_header_named *named_header = find_decl(decl,false);

	d.text_fmt("// optimized match %zd: %s ",
		   index+1,decl->_ident);
	decl->_name->dump(d);
	d.text(":");

	infos[index]._size = 0;
	infos[index]._decl = decl;
	infos[index]._index = (int) index+1;

	if (!get_match_bits(decl,d,infos[index]))
	  {
	    d.text(" could not get bits\n");
	    return false; // could not give bits, cannot generate
	  }

	d.text("\n");
      }
  }

  // now, make sure all our items want the same size

  int size = infos[0]._size;

  // Figure out what bits are actually different between our candidates

  uint32 differ = 0;

  for (unsigned int i = 0; i < infos.size(); i++)
    {
      if (infos[i]._size != size)
	{
	  d.text("// size mismatch, cannot generate\n");
	  return false;
	}

      for (unsigned int j = i+1; j < infos.size(); j++)
	differ |= infos[i]._value ^ infos[j]._value;
    }

  // Get our variable

  d.text("{\n");

  d.text_fmt("uint%d __match_peek;\n",size);
  d.text_fmt("PEEK_FROM_BUFFER(%d,uint%d,__match_peek);\n",
	     loc._internal,size);

  // Now, an even better way of doing the matching is if we can do an
  // binary search through the fixed bits, such that the number of tests
  // to find out who is our candidate

  // Figure out what bits are actually different between our candidates

  d.text_fmt("// differ = %08x :",differ);

  // find out what bits are differing

  std::vector<int> bits_differ;

  for (int i = 0; i < (int) sizeof(differ)*8; i++)
    if (differ & (1 << i))
      {
	d.text_fmt(" %d",i);
	bits_differ.push_back(i);
      }
  d.text_fmt("\n");

  if ((((unsigned int) 1) << bits_differ.size()) <= items->size() * (1 << 5))
    {
      // We at most make an array that has 2^5 times the number of elements
      // that are to be matched

      d.text("uint32 __match_index = 0");

      // Now, walk through the bits that we want to make an index of, and put
      // them into the slots

      for (size_t i = 0; i < bits_differ.size(); i++)
	{
	  int min_bit = bits_differ[i];
	  int max_bit = bits_differ[i];

	  int start_i = (int) i;

	  while (i+1 < bits_differ.size() &&
		 bits_differ[i+1] == max_bit+1)
	    {
	      i++;
	      max_bit++;
	    }

	  d.text_fmt(" | /* %d,%d */ ((__match_peek >> %d) & 0x%08x)",
		     min_bit,max_bit,
		     min_bit - start_i,((1 << (max_bit-min_bit+1)) - 1) << start_i);

	}
      d.text(";\n");

      int array_size = 1 << bits_differ.size();

      d.text_fmt("static const sint%d __match_index_array[%d] = { ",
		 items->size() <= 127 ? 8 : 32,
		 array_size);

      std::vector<std::vector<int> > ambigous;

      for (int index = 0; index < array_size; index++)
	{
	  int match_no = 0;

	  // now, for this bit pattern, we need to go through all the entries
	  // if they have a chance of matching, they get added.  If two can match,
	  // then we mark the entry as having a dual match and it will give an
	  // run-time error

	  std::vector<int> this_ambigous;

	  for (unsigned int i = 0; i < infos.size(); i++)
	    {
	      match_info &info = infos[i];

	      //d.text_fmt("\n(%d)",i);

	      for (unsigned int bi = 0; bi < bits_differ.size(); bi++)
		{
		  int bit = bits_differ[bi];

		  // d.text_fmt("[%d,%d]",bi,bits_differ[bi]);

		  if (info._mask & (1 << bit))
		    {
		      // The bit does matter
		      if (((info._value >> bit) & 1) !=
			  (unsigned int) ((index >> bi) & 1))
			{
			  // The bit would not match, so we're not an candidate
			  goto cannot_match;
			}
		    }
		  // Either we were a bit that mattered, and it matched,
		  // or we were a bit which did not matter (=> match)
		}
	      //d.text_fmt("=>%d",i+1);
	      // All bits have matched
	      if (match_no == 0)
		match_no = (int) i+1;
	      else
		{
		  if (match_no != -1)
		    {
		      d.text_fmt("/*%d*/",match_no);
		      this_ambigous.push_back(match_no);
		    }
		  d.text_fmt("/*%d*/",i+1);
		  this_ambigous.push_back((int) i+1);
		  match_no = -1; // we had several matches!
		}
	    cannot_match:
	      ;
	    }
	  if (this_ambigous.size())
	    {
	      // have we already seen this set of ambigous items?

	      for (size_t i = 0; i < ambigous.size(); i++)
		if (ambigous[i] == this_ambigous)
		  {
		    match_no = -((int) i+1);
		    goto found_ambigous_set;
		  }
	      ambigous.push_back(this_ambigous);
	      match_no = (int) -ambigous.size();
	    found_ambigous_set:
	      ;
	    }

	  d.text_fmt("%d, ",match_no);
	}

      d.text("};\n");

      // Now, if we have a clean match, everything is fine, we can go
      // to the unpacking directly.  However, a claim that several
      // matches, might be spurious, since we may have checked some
      // bit which cannot be the case for someone.  We thus go to the
      // standard matching routines.  TODO: this can actually be fixed
      // by a somewhat more advanced handling..

      // Hmm, no actully this cannot happen.  We have an array based
      // on all bits that at all can differ.  If this is not unique
      // here, it will also not be unique using the other method.
      // Well, with ranges this is not true.  They may be unique,
      // altough the bits are not.

      // d.text_fmt("fprintf(stderr,\" 0x%%08x %%d\\n\",__match_peek,__match_index);\n");

      d.text("__match_no = __match_index_array[__match_index];\n");

      // If we had a ambigous match, then we've gotten a negative
      // index out (which has an associated set of items to check

      if (!ambigous.empty())
	{
	  d.text("if (__match_no < 0)\n");
	  d.text("{\n");
	  dumper sd(d,2);
	  sd.text("switch (__match_no)\n");
	  sd.text("{\n");
	  dumper ssd(sd,2);

	  for (size_t i = 0; i < ambigous.size(); i++)
	    {
	      std::vector<int> &a = ambigous[i];

	      ssd.text_fmt("case %d:\n",-((int) i+1));
	      dumper sssd(ssd,2);
	      sssd.text("__match_no = 0;\n");

	      for (size_t j = 0; j < a.size(); j++)
		{
		  const match_info &info = infos[(size_t) a[j]-1];
		  const struct_decl *decl = info._decl;
		  const struct_header_named *named_header = find_decl(decl,false);

		  sssd.text_fmt("MATCH_DECL(%d,__match_no,%d,",
				decl->_loc._internal,a[j]);
		  sssd.text(decl->_ident);
		  sssd.text(",");
		  decl->_name->dump(sssd);
		  dump_param_args(decl->_loc,named_header->_params,decl->_args,sssd,false);
		  sssd.text(");\n");
		}

	      sssd.text("break;\n");
	    }

	  sd.text("}\n");
	  d.text("}\n");
	}


      //d.text_fmt("MATCH_DECL_ARRAY(%d,__match_no,__match_index,__match_index_array);\n",
      //loc._internal);
    }
  else
    {
      gen_match_decl_quick(infos,d,size,abort_spurious_label);

    }

  // We however have another problem.  If the select several
  // statement is not the last one in the subevent, it may be that
  // a proper match would have rejected our item (depending on
  // e.g. an variable that we could not evalute.  And then we
  // should not try to unpack, but rather continue after the
  // select several statement instead, (without issuing an error)

  // This problem is not inherent to the array method of selecting
  // matches.

  // The only thing we can do is to run the real matcher (but only for
  // the already found item) again, in order to verify the match.

  // We can only have one match!

  d.text_fmt("// last_subevent_item = %d\n",last_subevent_item);

  d.text("}\n");

  return true;
}

