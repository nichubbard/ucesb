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

#include "signal.hh"
#include "parse_error.hh"
#include "definitions.hh"
#include "str_set.hh"
#include "unpack_code.hh"

#include <ctype.h>
#include <string.h>
#include <assert.h>

#ifndef strndup
// Actually, since strndup is not a macro, we'll always be done...
char *strndup(const char *src,size_t length)
{
  // We wast memory in case the string actually is shorter...
  char *dest = (char *) malloc(length+1);
  if (!dest)
    return NULL;
  strncpy(dest,src,length);
  dest[length]='\0'; // since strncpy would not handle this
  return dest;
}
#endif

void signal_spec::dump_tag(dumper &d, int toggle_tag_hide) const
{
  int tag = _tag & ~toggle_tag_hide;

  if (!tag)
    return;

#define DUMP_TAG(x, str) \
  if (tag & (x)) { tag &= ~(x); d.text(str); goto handled; }

  while (tag)
    {
      DUMP_TAG(SIGNAL_TAG_FIRST_EVENT,"FIRST_EVENT");
      DUMP_TAG(SIGNAL_TAG_LAST_EVENT, "LAST_EVENT");
      DUMP_TAG(SIGNAL_TAG_TOGGLE_1,   "TOGGLE 1");
      DUMP_TAG(SIGNAL_TAG_TOGGLE_2,   "TOGGLE 2");

      ERROR("Internal error, bad tag %d.", tag);

    handled:
      if (tag)
	d.text(", ");      
    }
  d.text(":");
}

void signal_spec_type_unit::dump(dumper &d) const
{
  d.text(_type);
  if (_unit)
    d.text_fmt(" \"%s\"",_unit);
}

void signal_spec_types::dump(dumper &d) const
{
  if (_tu[1])
    {
      d.text("(");
      _tu[0]->dump(d);
      d.text(",");
      _tu[1]->dump(d);
      d.text(")");
    }
  else
    {
      _tu[0]->dump(d);
    }
}

void signal_spec::dump_idents(dumper &d, int toggle_i_hide) const
{
  int n = 0;

  for (int i = 0; i < MAX_NUM_TOGGLE; i++)
    if (_ident[i] && i != toggle_i_hide)
      n++;

  int comma = n;

  if (n > 1)
    d.text("(");
  for (int i = 0; i < MAX_NUM_TOGGLE; i++)
    if (_ident[i] && i != toggle_i_hide)
      {
	_ident[i]->dump(d);
	if (--comma)
	  d.text(",");
      }
  if (n > 1)
    d.text(")");
}

void signal_spec::dump_one_toggle(dumper &d, int tag_hide, int i_hide) const
{
  d.text("SIGNAL(");
  dump_tag(d, tag_hide);
  d.text(_name);
  d.text(",");
  dump_idents(d, i_hide);
  d.text(",");
  _types->dump(d);
  d.text(");");
  d.nl();
}

void signal_spec::dump(dumper &d) const
{
  if ((_tag & (SIGNAL_TAG_TOGGLE_1 | SIGNAL_TAG_TOGGLE_2)) ==
      (SIGNAL_TAG_TOGGLE_1 | SIGNAL_TAG_TOGGLE_2))
    {
      dump_one_toggle(d, SIGNAL_TAG_TOGGLE_2, 1);
      dump_one_toggle(d, SIGNAL_TAG_TOGGLE_1, 0);
    }
  else
    dump_one_toggle(d, 0, -1);
}

void signal_spec_range::dump(dumper &d) const
{
  d.text("SIGNAL(");
  dump_tag(d, 0);
  d.text(_name);
  d.text(",");
  dump_idents(d, -1);
  d.text(",");
  d.text(_name_last);
  d.text(",");
  _ident_last->dump(d);
  d.text(",");
  _types->dump(d);
  d.text(");");
  d.nl();
}

void signal_info::dump(dumper &d) const
{
  const char *print_name = _name;
  char *tmp = NULL;

  d.text("SIGNAL(");
  if (_info == SIGNAL_INFO_ZERO_SUPPRESS)
    d.text(" ZERO_SUPPRESS");
  if (_info == SIGNAL_INFO_NO_INDEX_LIST)
    {
      /* We rather print with the NO_INDEX_LIST(n) format, which means,
       * that we have to cut one element from the name...
       *
       * This is UGLY!
       */
      tmp = strdup(_name);
      char *last_start = tmp+strlen(tmp);
      int index;
      while (last_start > tmp &&
	     isdigit(*(last_start-1)))
	last_start--;
      index = atoi(last_start);
      if (last_start > tmp &&
	  *(last_start-1) == '_')
	last_start--;
      *last_start = 0;
      print_name = tmp;
      d.text_fmt(" NO_INDEX_LIST(%d)", index);
    }
  if (_info == SIGNAL_INFO_ZERO_SUPPRESS_MULTI)
    d.text_fmt(" ZERO_SUPPRESS_MULTI(%d)",_multi_size);
  d.text(":");

  d.text(print_name);
  d.text(");");
  d.nl();

  free (tmp);
}

event_signal::event_signal(signal_spec_base *decl,
			   const char *name)
{
  _type = NULL;
  _unit = NULL;
  _name = name;
  _decl = decl;
  _multi_size = -1;
  _tag = 0;
}


// TODO: implement check that the src variables are not reused, since
// this will only make the last entry (which becomes random, since the
// list is resorted) take it's variable, e.g.

// SIGNAL(POS1_1_T,vme.tdc0.data[0],DATA12);
// SIGNAL(POS1_2_T,vme.tdc0.data[0],DATA12);

// has a collision, since both wants data[0]

void insert_signal(event_signal &top,
		   signal_spec_base *s,
		   signal_id &id,
		   int type_no,
		   bool create_nodes)
{
  // We simply need to walk the tree of top, for each item in parts,
  // and make sure that a), the item exist, b) there are indices enough

  event_signal *node = &top;
  sig_part_vector::iterator part = id._parts.begin();

  index_vector all_indices;

  while (part != id._parts.end())
    {
      // The part must be at an name!

      if (part->_type != SIG_PART_NAME)
	ERROR_LOC(s->_loc,
		  "Cannot insert signal %s into list "
		  "(index/name) mismatch to previous entries.",
		  s->_name);

      // So, find the node with the correct name

      event_signal_map::iterator child = node->_children.find(part->_id._name);

      // printf ("part->_id._name = %s\n",part->_id._name);

      if (child == node->_children.end())
	{
	  if (!create_nodes)
	    return; // was just a zero_suppress marker, but had no data there

	  // This signal name not known before, insert...

	  event_signal *es = new event_signal(s,part->_id._name);

	  node->_children.insert(event_signal_map::value_type(part->_id._name,
							      es));
	  //fprintf (stderr,"Insert name: %s\n",part->_id._name);
	  // As long as we now have indices, we now add them to the list...
	  ++part;
	  while (part != id._parts.end() &&
		 part->_type == SIG_PART_INDEX)
	    {
	      index_info *ii;

	      all_indices.push_back(part->_id._index);
	      // fprintf (stderr,"Insert index: %d\n",part->_id._index+1);
	      es->_indices.push_back(ii = new index_info(part->_id._index+1));
	      part->_index_info = ii;
	      ++part;
	    }
	  // For us it does not matter if we end here completely or
	  // with a name, since it is the first addition

	  node = es;
	}
      else
	{
	  // fprintf (stderr,"Found name: %s\n",part->_id._name);
	  // So, the correctly named item has been found...
	  ++part;
	  // The amount of indices must be the same, but they may be
	  // too small...
	  // Now, run in parallell with two iterators, one for part,
	  // one for the indices

	  node = child->second;

	  index_info_vector &indices = node->_indices;

	  index_info_vector::iterator i = indices.begin();

	  while (part != id._parts.end() &&
		 part->_type == SIG_PART_INDEX &&
		 i != indices.end())
	    {
	      all_indices.push_back(part->_id._index);

	      // printf ("part->_id._index = %d\n",part->_id._index);

	      // We both request one more index, and actually have it!

	      //fprintf (stderr,"Compare index: %d (prev %d)\n",part->_id._index+1,*i);

	      index_info *ii = *i;

	      if (ii->_size <= part->_id._index)
		{
		  //fprintf (stderr,"Increase index: %d\n",part->_id._index+1);
		  ii->_size = part->_id._index+1;
		}

	      part->_index_info = ii;

	      ++part;
	      ++i;

	      if (part == id._parts.end())
		{
		  signal_info *s_info = dynamic_cast<signal_info *>(s);

		  if (s_info)
		    {
		      // We have some information for this entry!

		      if (s_info->_info & SIGNAL_INFO_ZERO_SUPPRESS_MULTI)
			{
			  assert (s_info->_multi_size != -1);

			  if (!node->_children.empty() ||
			      i != indices.end())
			    ERROR_LOC(s_info->_loc,
				      "Multi-entry can only be leaf node.");

			  if (node->_multi_size != -1 &&
			      node->_multi_size != s_info->_multi_size)
			    {
			      WARNING_LOC(s_info->_loc,
					  "Different multi-entry sizes "
					  "specified.");
			      ERROR_LOC(node->_multi_loc,
					"Previous declaration was here.");
			    }
			  node->_multi_size = s_info->_multi_size;
			  node->_multi_loc  = s_info->_loc;

			}

		      if (ii->_info != 0 &&
			  ii->_info != s_info->_info)
			{
			  WARNING_LOC(s_info->_loc,
				      "Different zero-suppression specified.");
			  ERROR_LOC(ii->_info_loc,
				    "Previous declaration was here.");
			}

		      ii->_info |= s_info->_info;
		      ii->_info_loc = s_info->_loc;

		      /*
		      printf ("INFO: %08x %d\n",
			      s_info->_info,s_info->_multi_size);
		      */
		      return;
		    }
		}
	    }

	  // Ok, so we stopped for some reason.
	  // The stop is valid if we wanted no more indices, and there
	  // also are no further
	  // Then we must just check that the terminal rule is ok

	  if (part != id._parts.end() &&
	      part->_type == SIG_PART_INDEX)
	    {
#define WARNING_DECL_SIGNAL(decl) {					\
		if (decl)						\
		  WARNING_LOC((decl)->_loc,"Declaring signal %s was here.", \
			      (decl)->_name);				\
	      }
	      
	      WARNING_DECL_SIGNAL(node->_decl);
	      ERROR_LOC(s->_loc,"Cannot insert signal %s into list "
			"(index mismatch).",
			s->_name);
	    }
	  else if (part == id._parts.end())
	    {
	      // We want no further indices, were there further?

	      if (i != indices.end())
		{
		  WARNING_DECL_SIGNAL(node->_decl);
		  ERROR_LOC(s->_loc,"Cannot insert signal %s into list "
			    "(index mismatch).",
			    s->_name);
		}

	      // We are terminal, previous must also have been

	      if (!node->_children.empty())
		{
		  WARNING_DECL_SIGNAL(node->_decl);
		  ERROR_LOC(s->_loc,
			    "Cannot insert signal %s into list "
			    "(named child mismatch, request empty).",
			    s->_name);
		}

	      signal_info *s_info = dynamic_cast<signal_info *>(s);

	      if (s_info)
		{
		  if (s_info->_info & SIGNAL_INFO_ZERO_SUPPRESS_MULTI)
		    {
		      assert (s_info->_multi_size != -1);

		      if (!node->_children.empty())
			ERROR_LOC(s_info->_loc,
				  "Multi-entry can only be leaf node.");

		      if (node->_multi_size != -1 &&
			  node->_multi_size != s_info->_multi_size)
			{
			  WARNING_LOC(s_info->_loc,
				      "Different multi-entry sizes "
				      "specified.");
			  ERROR_LOC(node->_multi_loc,
				    "Previous declaration was here.");
			}
		      node->_multi_size = s_info->_multi_size;
		      node->_multi_loc  = s_info->_loc;

		    }
		  else
		    {
		      ERROR_LOC(s_info->_loc,
				"Cannot zero-suppress non-array.");
		    }

		      /*
		  printf ("INFO2: %08x %d\n",
			  s_info->_info,s_info->_multi_size);
		      */
		  return;
		}
	    }
	  else
	    {
	      // We are not terminal.  Also the child may have no
	      // children, or no further indices

	      if (i != indices.end())
		{
		  WARNING_DECL_SIGNAL(node->_decl);
		  ERROR_LOC(s->_loc,"Cannot insert signal %s into list "
			    "(index mismatch).",
			    s->_name);
		}

	      if (node->_children.empty())
		{
		  WARNING_DECL_SIGNAL(node->_decl);
		  ERROR_LOC(s->_loc,"Cannot insert signal %s into list "
			    "(named child mismatch, prev empty).",
			    s->_name);
		}
	    }
	}
    }

  signal_spec *s_spec = dynamic_cast<signal_spec *>(s);

  if (s_spec)
    {
      /* Now node, is the leaf node (possibly with indices)
       * Set the type of the leaf node!
       */

      if (node->_type)
	{
	  if (node->_type != s_spec->_types->_tu[type_no]->_type)
	    {
	      WARNING_DECL_SIGNAL(node->_decl);
	      ERROR_LOC(s->_loc,"Cannot insert signal %s into list "
			"(type mismatch, %s != %s).",
			s->_name,
			s_spec->_types->_tu[type_no]->_type,node->_type);
	    }
	}
      else
	node->_type = s_spec->_types->_tu[type_no]->_type;

      if (s_spec->_types->_tu[type_no]->_unit)
	{
	  if (node->_unit)
	    {
	      if (node->_unit != s_spec->_types->_tu[type_no]->_unit)
		{
		  WARNING_DECL_SIGNAL(node->_decl_unit);
		  ERROR_LOC(s->_loc,"Cannot insert signal %s into list "
			    "(unit mismatch, %s != %s).",
			    s->_name,
			    s_spec->_types->_tu[type_no]->_unit, node->_unit);
		}
	    }
	  else
	    {
	      node->_unit = s_spec->_types->_tu[type_no]->_unit;
	      node->_decl_unit = s;
	    }
	}

      node->_tag |= s_spec->_tag & SIGNAL_TAG_TOGGLE_MASK;

      /* Check that the node (leaf node) does not yet have
       * anyone reaching here with the same index structure.
       */

      bool has_ident = false;

      for (int i = 0; i < MAX_NUM_TOGGLE; i++)
	if (s_spec->_ident[i])
	  has_ident = true;	  

      if (has_ident)
	{
	  std::pair<event_index_item::iterator,bool> known =
	    node->_items.insert(event_index_item::value_type(all_indices,
							     s_spec));

	  if (known.second == false)
	    {
	      /*
		fprintf (stderr,"Indices: ");
		for (index_vector::iterator i = all_indices.begin();
		i != all_indices.end(); ++i)
		fprintf(stderr,"[%d]",*i);
		fprintf (stderr,"\n");
	      */
	      WARNING_DECL_SIGNAL(known.first->second);
	      ERROR_LOC(s->_loc,"Cannot insert signal %s into list, "
			"same entry already in use (all indices same).",
			s->_name);
	    }
	}
    }
}

void event_signal::dump(dumper &d,int level,
			const char *zero_suppress_type,
			const index_info *prev_zero_suppress_index,
			const std::string &prefix,
			const char *base_suffix,
			bool toggle) const
{
  // Before we dump ourselves, we should make sure all our contained
  // data structures have been dumped

  if (level == 0)
    {
      event_signal_map::const_iterator c;

      ordered_event_signal_map dump_map;

      for (c = _children.begin(); c != _children.end(); ++c)
	{
	  if (!c->second->_children.empty())
	    {
	      dump_map.
		insert(ordered_event_signal_map::
		       value_type(c->second->_decl->_order_index,
				  c->second));
	    }
	}

      ordered_event_signal_map::const_iterator di;

      for (di = dump_map.begin(); di != dump_map.end(); ++di)
	{
	  di->second->dump(d,0,
			   zero_suppress_type,prev_zero_suppress_index,
			   prefix+_name+"_",NULL,toggle);
	  d.nl();
	}
    }

  index_info_vector::const_iterator zero_suppress_i = _indices.end();
  const index_info *zero_suppress_index = NULL;

  for (index_info_vector::const_iterator i = _indices.begin();
       i != _indices.end(); ++i)
    {
      const index_info *ii = *i;

      printf ("//DUMPY: %d %d\n",ii->_size,ii->_info);

      if (ii->_info & (SIGNAL_INFO_ZERO_SUPPRESS |
		       SIGNAL_INFO_NO_INDEX_LIST |
		       SIGNAL_INFO_ZERO_SUPPRESS_MULTI))
	{
	  if (prev_zero_suppress_index)
	    {
	      WARNING_LOC(prev_zero_suppress_index->_info_loc,
			  "Previous was declared here.");
	      ERROR_LOC(ii->_info_loc,
			"Several indices requested zero suppression.");
	    }
	  zero_suppress_i     = i;
	  zero_suppress_index = ii;

	  prev_zero_suppress_index = ii; // catch multi-level zero suppression

	  if (ii->_info & SIGNAL_INFO_NO_INDEX_LIST)
	    zero_suppress_type = "list_ii";
	}
    }

  if (_multi_size != -1)
    {
      // It's requested that the leaf node be made multi-entry Altough
      // our unpacker C++ could handle it, psdc is not able to handle
      // two raw_array_..._zero_suppress templates for the same
      // variable, so we must not be zero suppressed also.
      // Except possibly in the last variable, in which case we can employ the
      // raw_array_multi_... class.  In other cases, we employ the
      // raw_list_ii_zero_suppress, which is a non-indexed list

      if (zero_suppress_index)
	{
	  ssize_t indices = distance(zero_suppress_i,_indices.end());

	  if (indices > 1)
	    {
	      WARNING_LOC(zero_suppress_index->_info_loc,
			  "Unhandled amount (%d) of indices "
			  "between zero suppression",(int) indices-1);
	      ERROR_LOC(_multi_loc,"and multi-entry leaf node.");
	    }

	  zero_suppress_type = "array_multi";
	}
      else
	zero_suppress_type = "list_ii";
    }



  if (level != 0 && _children.empty() && _type == NULL)
    {
      // We are a leaf node that was instantiated just due to
      // zero_suppression, but have no data members here.
      // (i.e. probably cal level, while raw level has data...)
      assert(false); // nowadays, those leaves are not even created
      return;
    }

  /*
  if (level != 0 && !zero_suppress_index && _children.empty())
    {
      // We are a leaf node

      if (_type == NULL)

      d.text_fmt("%s %s",_type,_name);
    }
  else
  */
    {
      if (level == 0)
	d.text("class ");

      const char *toggle1 =
	(toggle && (_tag & SIGNAL_TAG_TOGGLE_MASK) ? "TOGGLE(" : "");
      const char *toggle2 =
	(toggle && (_tag & SIGNAL_TAG_TOGGLE_MASK) ? ")" : "");

      if (level != 0 && (zero_suppress_index || _multi_size != -1))
	{
	  // d.text("typedef ");

	  ssize_t indices =
	    zero_suppress_index ?
	    distance(zero_suppress_i,_indices.end())-1 : 0;

	  if (indices)
	    {
	      if (indices > 2)
		{
		  // one simply need to implement the
		  // raw_array_zero_suppress_%d class (very easy)
		  // look at the smaller ones...
		  ERROR_LOC(zero_suppress_index->_info_loc,
			    "Unhandled amount (%d) of indices "
			    "after zero suppressed",(int) indices);
		}
	      d.text_fmt("raw_%s_zero_suppress_%d<",
			 zero_suppress_type,(int) indices);
	    }
	  else
	    d.text_fmt("raw_%s_zero_suppress<",zero_suppress_type);

	  if (_children.empty())
	    d.text_fmt("%s%s%s,",toggle1,_type,toggle2);
	  else
	    d.text_fmt("%s%s,",prefix.c_str(),_name);
	}

      if (level != 0 && _children.empty())
	d.text_fmt("%s%s%s",toggle1,_type,toggle2);
      else
	{
	  d.text_fmt("%s%s",prefix.c_str(),_name);

	  if (base_suffix)
	    d.text_fmt(" : public %s%s%s",prefix.c_str(),_name,base_suffix);
	}

      if (level != 0 && (zero_suppress_index || _multi_size != -1))
	{
	  if (zero_suppress_index)
	    {
	      index_info_vector::const_iterator i = zero_suppress_i;
	      ++i;
	      for ( ; i != _indices.end(); ++i)
		d.text_fmt("[%d]",(*i)->_size);
	      //d.text_fmt(" %s_zsp;\n",_name);
	      //d.text_fmt("raw_array_zero_suppress<%s_zsp",_name);
	      d.text_fmt(",%d",zero_suppress_index->_size);
	      i = zero_suppress_i;
	      ++i;
	      for ( ; i != _indices.end(); ++i)
		d.text_fmt(",%d",(*i)->_size);
	    }
	  if (_multi_size != -1)
	    d.text_fmt(",%d",_multi_size);
	  d.text(">");
	}

      if (level == 0 || !_children.empty())
	{
	  d.text("\n");
	  d.text_fmt("%s{\n",level == 1 ? "/* " : "");
	  d.text("public:\n");
	  dumper sd(d,2);
	  // Now, dump all our children

	  event_signal_map::const_iterator c;

	  ordered_event_signal_map dump_map;

	  for (c = _children.begin(); c != _children.end(); ++c)
	    {
	      // sd.text_fmt("/*%s:%d*/",
	      //             c->second->_name,c->second->_decl->_order_index);
	      // sd.nl();
	      dump_map.
		insert(ordered_event_signal_map::
		       value_type(c->second->_decl->_order_index,
				  c->second));
	    }

	  ordered_event_signal_map::const_iterator di;

	  for (di = dump_map.begin(); di != dump_map.end(); ++di)
	    {
	      // sd.text_fmt("/*%d*/",di->first);
	      di->second->dump(sd,level+1,zero_suppress_type,
			       zero_suppress_index,
			       prefix+_name+"_",NULL,toggle);
	    }

	  if (level == 0)
	    {
	      d.nl();
	      d.text("public:\n");
	      d.col0_text("#ifndef __PSDC__\n");
	      d.text_fmt("  STRUCT_FCNS_DECL(%s%s);\n",prefix.c_str(),_name);
	      d.col0_text("#endif//!__PSDC__\n");
	    }

	  d.text_fmt("}%s",level == 1 ? " */" : "");
	}
      d.text_fmt(" %s",level != 0 ? _name : "");
    }

  // dump all our indices
  if (level != 0)
    {
      index_info_vector::const_iterator end_index;

      if (zero_suppress_index)
	end_index = zero_suppress_i;
      else
	end_index = _indices.end();

      for (index_info_vector::const_iterator i = _indices.begin();
	   i != end_index; ++i)
	{
	  const index_info *ii = *i;

	  d.text_fmt("[%d]",ii->_size);
	  //if (i->_info)
	  //  d.text_fmt("/*%d*/",i->_info);
	}
    }

  if (_unit)
    d.text_fmt(" UNIT(\"%s\")",_unit);

  d.text(";\n");
}

void generate_signal(dumper &d, signal_spec *s,
		     const char *name,
		     int toggle_tag_hide, int toggle_i_hide)
{
  d.text_fmt("%s(", name);
  d.text(s->_types->_tu[0]->_type);
  d.text(",");
  d.text(s->_name);
  d.text(",");
  for (int i = 0; i < MAX_NUM_TOGGLE; i++)
    if (s->_ident[i] && i != toggle_i_hide)
      s->_ident[i]->dump(d);
  d.text(",");
  s->dump_tag(d,
	      toggle_tag_hide |
	      SIGNAL_TAG_FIRST_EVENT | SIGNAL_TAG_LAST_EVENT);
  int num_zero_suppress = 0;
  {
    sig_part_vector::iterator part = s->_id._parts.begin();
    for (part = s->_id._parts.begin(); part != s->_id._parts.end(); ++part)
      {
	if (part->_type == SIG_PART_NAME)
	  {
	    if (part != s->_id._parts.begin())
	      d.text(".");
	    d.text(part->_id._name);
	  }
	else
	  {
	    d.text_fmt("[%d]",part->_id._index);
	  }
	if (part->_index_info &&
	    (part->_index_info->_info & SIGNAL_INFO_ZERO_SUPPRESS))
	  num_zero_suppress++;
	if (part->_index_info &&
	    (part->_index_info->_info & SIGNAL_INFO_ZERO_SUPPRESS_MULTI))
	  {
	    // We add a (dummy?) index [0] for multi-entry
	    // zero-suppressed destinations.
	    d.text_fmt("/*multi:*/[0]");
	  }
      }
  }
  d.text(");");
  for (int zzp = 0; zzp < num_zero_suppress; zzp++)
    {
      d.text("/*,ZERO_SUPPRESS_ITEM(");

      int zzp_seen = 0;

      sig_part_vector::iterator part = s->_id._parts.begin();
      for (part = s->_id._parts.begin(); part != s->_id._parts.end(); ++part)
	{
	  if (part->_index_info &&
	      (part->_index_info->_info & SIGNAL_INFO_ZERO_SUPPRESS))
	    {
	      if (zzp_seen == zzp)
		{
		  d.text_fmt(",%d)*/",part->_id._index);
		  break;
		}
	      zzp_seen++;
	    }
	  if (part->_type == SIG_PART_NAME)
	    {
	      if (part != s->_id._parts.begin())
		d.text(".");
	      d.text(part->_id._name);
	    }
	  else
	    {
	      d.text_fmt("[%d]",part->_id._index);
	    }
	}
    }
  d.text("\n");
}

void generate_signals()
{
  // Now, we need to figure out what kind of structure names are
  // needed

  // First step is to loop over all signals, dissect it, and figure
  // out what that particular one requires

  printf ("/**********************************************************\n"
          " * Generating event structure...\n"
          " */\n\n");

  event_signal signal_head_raw(NULL,"raw_event");
  event_signal signal_head_cal(NULL,"cal_event");

  event_signal sticky_head_raw(NULL,"raw_sticky");

  event_signal *signal_head[2 /* sticky */ ][2 /* type_no */ ] =
    { { &signal_head_raw, &signal_head_cal },
      { &sticky_head_raw, NULL } };

  {
    signal_map::iterator i;

    for (i = all_signals.begin(); i != all_signals.end(); ++i)
      {
	signal_spec *s = i->second;

	dissect_name(s->_loc,s->_name,s->_id);

	for (int sticky = 0; sticky < 2; sticky++)
	  for (int type_no = 0; type_no < 2; type_no++)
	    if (signal_head[sticky][type_no] &&
		!s->_sticky == !sticky &&
		s->_types->_tu[type_no])
	      insert_signal(*signal_head[sticky][type_no],
			    s,s->_id,type_no,true);
      }
  }
  {
    signal_multimap::iterator i;

    for (i = all_signals_no_ident.begin();
	 i != all_signals_no_ident.end(); ++i)
      {
	signal_spec *s = i->second;

	dissect_name(s->_loc,s->_name,s->_id);

	for (int sticky = 0; sticky < 2; sticky++)
	  for (int type_no = 0; type_no < 2; type_no++)
	    if (signal_head[sticky][type_no] &&
		!s->_sticky == !sticky &&
		s->_types->_tu[type_no])
	      insert_signal(*signal_head[sticky][type_no],
			    s,s->_id,type_no,true);
      }
  }
  {
    signal_info_map::iterator i;

    for (i = all_signal_infos.begin(); i != all_signal_infos.end(); ++i)
      {
	signal_info *s = i->second;

	dissect_name(s->_loc,s->_name,s->_id);

	for (int sticky = 0; sticky < 2; sticky++)
	  for (int type_no = 0; type_no < 2; type_no++)
	    if (signal_head[sticky][type_no] &&
		!s->_sticky == !sticky)
	      insert_signal(*signal_head[sticky][type_no],
			    s,s->_id,type_no,false);
      }
  }
  for (int sticky = 0; sticky < 2; sticky++)
    for (int type_no = 0; type_no < 2; type_no++)
      {
	const char *header_name = NULL;

	if (!signal_head[sticky][type_no])
	  continue;

	switch (type_no)
	  {
	  case 0: header_name = "EVENT_RAW_STRUCTURE"; break;
	  case 1: header_name = "EVENT_CAL_STRUCTURE"; break;
	  }

	print_header(header_name,"Event data structure.");

	dumper_dest_file d_dest(stdout);
	dumper d(&d_dest);

	signal_head[sticky][type_no]->dump(d,0,
					   type_no == 0 ? "array" : "list",
					   NULL,
					   "","_base", type_no == 0);

	print_footer(header_name);
      }
  for (int sticky = 0; sticky < 2; sticky++)
    {
      print_header("EVENT_DATA_MAPPING","Event data mapping.");

      printf("// The order in this file does not matter.\n"
	     "// This information parsed once and not treated eventwise,\n"
	     "// it is used to initialize a structure.\n\n");

      const char *name = NULL;

      switch (sticky)
	{
	case 0: name = "SIGNAL_MAPPING"; break;
	case 1: name = "STICKY_MAPPING"; break;
	}

      dumper_dest_file d_dest(stdout);
      dumper d(&d_dest);

      signal_map::iterator i;

      for (i = all_signals.begin(); i != all_signals.end(); ++i)
	{
	  signal_spec *s = i->second;

	  if (!s->_sticky == !sticky)
	    {
	      if ((s->_tag & (SIGNAL_TAG_TOGGLE_1 | SIGNAL_TAG_TOGGLE_2)) ==
		  (SIGNAL_TAG_TOGGLE_1 | SIGNAL_TAG_TOGGLE_2))
		{
		  generate_signal(d, s, name, SIGNAL_TAG_TOGGLE_2, 1);
		  generate_signal(d, s, name, SIGNAL_TAG_TOGGLE_1, 0);
		}
	      else
		{
		  generate_signal(d, s, name, 0, -1);
		}
	    }
	}

      print_footer("EVENT_DATA_MAPPING");
    }

  printf ("/**********************************************************/\n");

}


void expand_insert_signal_to_all(signal_spec_range *signal)
{
  // So, we need to decompose the signal name and identifiers,
  // figure out what indices are to be expanded, and then run them
  // in lockstep, to generate all signals inbetween.

  signal_id id_first;
  signal_id id_last;

  dissect_name(signal->_loc,signal->_name,     id_first);
  dissect_name(signal->_loc,signal->_name_last,id_last);

  // Find out which index is different between the two items, and
  // how large the difference is!

  int name_diff_index;
  int name_diff_length;

  if (!id_first.difference(id_last,
			   name_diff_index,
			   name_diff_length))
    ERROR_LOC(signal->_loc,
	      "Signal names (%s) and (%s) cannot make a range.",
	      signal->_name,
	      signal->_name_last);

  //  fprintf(stderr,"Extract: %s  %s  (%d,%d)\n",
  //	  signal->_name,signal->_name_last,name_diff_index,name_diff_length);

  int ident_diff_index;
  int ident_diff_length;

  int toggle_i = -1;

  for (int i = 0; i < MAX_NUM_TOGGLE; i++)
    if (signal->_ident[i])
      {
	if (toggle_i != -1)
	  ERROR_LOC(signal->_loc,"Internal error.  Multiple toggle set?");
	
	toggle_i = i;
      }

  if (!difference(signal->_ident[toggle_i],signal->_ident_last,
		  ident_diff_index,ident_diff_length))
    ERROR_LOC(signal->_loc,"The signal identifiers cannot make a range.");

  //fprintf (stderr,"(%d,%d)\n",ident_diff_index,ident_diff_length);

  if (abs(name_diff_length) != abs(ident_diff_length))
    ERROR_LOC(signal->_loc,"Cannot make range, "
	      "names and identifiers have different lengths (%d!=%d).",
	      name_diff_length,ident_diff_length);

  int length = abs(name_diff_length);

  for (int i = 0; i <= length; i++)
    {
      int i1 = (name_diff_length  >= 0 ? i : -i);
      int i2 = (ident_diff_length >= 0 ? i : -i);

      // generate the signal_spec

      char *name;
      const var_name *ident;

      name  = id_first.generate_indexed(name_diff_index,
					i1);

      ident = generate_indexed(signal->_ident[toggle_i],
			       ident_diff_index,i2);

      signal_spec *new_spec = new signal_spec(signal->_loc,
					      signal->_order_index,
					      signal->_sticky,
					      find_str_identifiers(name),ident,
					      signal->_types,
					      signal->_tag);

      free(name);

      insert_signal_to_all(new_spec);
    }
}

const char *data_item_size_to_signal_type(int sz)
{
  switch (sz)
    {
    case 8:  return "uint8";
    case 16: return "uint16";
    case 32: return "uint32";
    case 64: return "uint64";
    }
  assert(false);
}

/* This is a dirty hack (support function for parser).
 * Had we been storing signal_id instead of string names,
 * things would have been more natural.
 */
const char *add_index_to_identifier(const char *ident,
				    int index)
{
  const char *extended;
  char *tmp;

  tmp = (char *) malloc(strlen(ident) + 32);
  if (!tmp)
    ERROR("Memory allocation error.");
  sprintf (tmp, "%s_%d", ident, index);
  extended = find_str_identifiers(tmp);
  free(tmp);
  return extended;
}
