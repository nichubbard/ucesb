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

#include "c_struct.hh"
#include "dump_list.hh"
#include "parse_error.hh"
#include "signal_id.hh"

#include "corr_struct.hh"


#define countof(x) (sizeof(x)/sizeof(x[0]))

struct corr_struct_item
{
  const char *_type;
  signal_id   _id;
};

typedef std::vector<corr_struct_item> corr_struct_item_vect;
typedef std::vector<corr_struct_item*> corr_struct_item_ptr_vect;

struct corr_items
{
  corr_struct_item_vect _vect;
};

bool _ignore_missing_corr_struct = false;

void corr_find_members(const char *type,
		       dumper &d,
		       signal_id &id,
		       corr_items &items)
{
  named_c_struct_def_map::iterator iter;

  iter = all_named_c_struct.find(type);

  if (iter == all_named_c_struct.end())
    {
      // First check if it is any of the types that interest us (or
      // that does not interest us)

      const char *corr_types[] = {
	"DATA12",
	"DATA12_RANGE",
	"DATA12_OVERFLOW",
	"DATA14",
	"DATA14_RANGE",
	"DATA14_OVERFLOW",
	"DATA16",
	"DATA16_OVERFLOW"
      };

      const char *no_corr_types[] = {
	"DATA8",
	"DATA24",
	"DATA32",
	"DATA64",
	"uint64",
	"uint32",
	"uint16",
	"uint8",
	"float",
	"unpack_subevent_base",
	"unpack_event_base",
	"raw_event_base",
	"cal_event_base"
      };

      for (unsigned int i = 0; i < countof(corr_types); i++)
	if (strcmp(type,corr_types[i]) == 0)
	  {
	    corr_struct_item item;

	    item._type = type;
	    item._id = id;

	    items._vect.push_back(item);

	    return;
	  }

      for (unsigned int i = 0; i < countof(no_corr_types); i++)
	if (strcmp(type,no_corr_types[i]) == 0)
	  {
	    d.text("---\n");
	    return;
	  }

      if (!_ignore_missing_corr_struct)
	WARNING("Cannot find definition of structure %s, corr_struct finding",
		type);
      return;
    }

  iter->second->find_members(d,id,items);
}

void c_struct_def::find_members(dumper &d,signal_id &id,
				corr_items &items) const
{
  if (_base_list)
    {
      for (unsigned int i = 0; i < _base_list->size(); i++)
	{
	  d.text_fmt("%spublic %s\n",
		     i ? " , " : " : ",(*_base_list)[i]);
	  corr_find_members((*_base_list)[i],d,id,items);
	}
    }

  for (c_struct_item_list::const_iterator i = _items->begin();
       i != _items->end(); ++i)
    {
      const c_struct_item* item = *i;

      item->find_members(d,id,items);
    }
}

void c_struct_member::find_members(dumper &d,signal_id &id_base,
				   corr_items &items) const
{
  const char *type = "UNKNOWN";

  if (_type)
    type = _type->_name;

  // d.text_fmt("// Find members: %s %s (mem)\n",type,_ident);

  signal_id id(id_base,_ident);

  d.text_fmt("%s  .%s",type,_ident);

  if (_array_ind)
    for (unsigned int i = 0; i < _array_ind->size(); i++)
      {
	d.text_fmt("[%d]",(*_array_ind)[i]);
	id.push_back((*_array_ind)[i]);
      }

  // If it is a templated class, then we need to do some dirty hacky
  // tricks to figure out both the actual class name, and any
  // additional indices in use.  We can only parse parameters for
  // known template types (all of them fortunately the same)

  const c_typename_template *type_template =
    dynamic_cast<const c_typename_template*>(_type);

  if (type_template)
    {
      // template<typename Tsingle,typename T,int n,...> // ... is n1, n2
      // raw_array
      // raw_array_zero_suppress
      // raw_array_zero_suppress_1
      // raw_array_zero_suppress_2
      // raw_list_zero_suppress
      // raw_list_ii_zero_suppress

      // We are interested in the Tsingle as the actual typename, and
      // of all indices

      const char *raw_array_templates[] = {
	"raw_array",
	"raw_array_zero_suppress",
	"raw_array_zero_suppress_1",
	"raw_array_zero_suppress_2",
	"raw_array_multi_zero_suppress",
	"raw_list_zero_suppress",
	"raw_list_zero_suppress_1",
	"raw_list_zero_suppress_2",
	"raw_list_ii_zero_suppress",
      };

      for (unsigned int i = 0; i < countof(raw_array_templates); i++)
	if (strcmp(_type->_name,raw_array_templates[i]) == 0)
	  {
	    // There should be at least three arguments

	    const c_arg_list *v = type_template->_args;

	    if (v->size() < 3)
	      ERROR_LOC(_loc,"Too few args for template, "
			"corr_struct finding");

	    // The first one should be a named argument

	    const c_arg_named *named =
	      dynamic_cast<const c_arg_named *>((*v)[0]);

	    if (!named)
	      ERROR_LOC(_loc,"Expected name as first template argument, "
			", corr_struct finding");

	    type = named->_name;

	    d.text_fmt("(%s)",type);

	    // arguments[2...] are to be indices

	    for (unsigned int j = 2; j < v->size(); j++)
	      {
		const c_arg_const *index =
		  dynamic_cast<const c_arg_const *>((*v)[j]);

		if (!index)
		  ERROR_LOC(_loc,"Expected index as parameter %d template argument, "
			    ", corr_struct finding",j);

		d.text_fmt("[%d]",index->_value);
		id.push_back(index->_value);

	      }

	    goto handled_template;
      	  }

      WARNING_LOC(_loc,"unknown template %s for corr_struct.",
		  _type->_name);

    handled_template:
      ;
    }

  d.text("\n");

  // Now we must look up the structure definition for this type and
  // continue the search

  dumper sd(d,2);
  corr_find_members(type,sd,id,items);


}

class corr_signal_part;

typedef std::vector<corr_signal_part*> corr_signal_part_vect;

class corr_signal_part
  : public sig_part
{
public:
  corr_signal_part(const sig_part &src)
    : sig_part(src)
  {
    init();
  }

  corr_signal_part(const char *name)
    : sig_part(name)
  {
    init();
  }

  corr_signal_part(int index)
    : sig_part(index)
  {
    init();
  }

public:
  void init()
  {
    _num_children = 0;
  }

public:
  // A std::set instead of std::vector would have been better for searching,
  // but we must keep the order, since we'll use it for making indices?
  corr_signal_part_vect _children;

  int _num_children; // including all children

public:
  void insert(sig_part_vector::iterator item,
	      sig_part_vector::iterator end)
  {
    sig_part &ins = *item;

    // Do we have a child equalling the iter member?

    corr_signal_part *child = NULL;

    for (corr_signal_part_vect::iterator iter = _children.begin();
	 iter != _children.end(); ++iter)
      {
	child = *iter;

	if (*child == ins)
	  {
	    // We found the appropriate item

	    goto insert_children;
	  }
      }

    // The item was not found.  Insert it.

    child = new corr_signal_part(ins);

    _children.push_back(child);

  insert_children:
    // See if there are further levels to insert
    ++item;

    if (item == end)
      return; // we were last
    child->insert(item,end);
  }

public:
  int calc_sizes(std::set<int> &sizes)
  {
    int children = 0;

    for (corr_signal_part_vect::iterator iter = _children.begin();
	 iter != _children.end(); ++iter)
      {
	corr_signal_part *child = *iter;

	children += child->calc_sizes(sizes);
      }

    if (!children)
      children = 1; // We are leaf node

    _num_children = children;

    sizes.insert(children);

    if (_type & SIG_PART_INDEX)
      children *= _id._index;

    sizes.insert(children);

    return children;
  }

public:
  int num_chunks(int size)
  {
    // If we can fit all the children into one chunk, then we are happy

    int chunks;

    if (_num_children <= size)
      chunks = 1;
    else
      {
	// We could not fit all the children into one chunk, let's then
	// Fit them each one into chunks

	chunks = 0;

	for (corr_signal_part_vect::iterator iter = _children.begin();
	     iter != _children.end(); ++iter)
	  {
	    corr_signal_part *child = *iter;

	    chunks += child->num_chunks(size);
	  }
      }

    if (_type & SIG_PART_INDEX)
      chunks *= _id._index;

    return chunks;
  }

public:
  void dump(dumper &d)
  {
    if (_type & SIG_PART_NAME)
      d.text_fmt(".%s",_id._name);
    if (_type & SIG_PART_INDEX)
      d.text_fmt("[%d]",_id._index);

    if (!_children.empty())
      {
	d.text_fmt("/%d/",_num_children);

	dumper sd(d,0);

	for (corr_signal_part_vect::iterator iter = _children.begin();
	     iter != _children.end(); ++iter)
	  {
	    corr_signal_part *child = *iter;

	    if (iter != _children.begin())
	      sd.nl();

	    child->dump(sd);
	  }

	//if (_children.size() == 0)
	//  sd.nl();
      }
  }
};


#define CORR_MODE_ALL  0
#define CORR_MODE_T    1
#define CORR_MODE_E    2
#define NUM_CORR_MODES 3

void c_struct_def::make_corr_struct(const char *type,
				    dumper &d,
				    corr_items &all_items,
				    int mode) const
{
  // mode: 0=all, 1=t, 2=e

  // First, we need to find out what items are of
  // interest

  corr_struct_item_ptr_vect items;
  corr_signal_part tree(type);

  for (corr_struct_item_vect::iterator iter = all_items._vect.begin();
       iter != all_items._vect.end(); ++iter)
    {
      corr_struct_item &item = *iter;

      //char buf[256];
      //item._id.format(buf,sizeof(buf));

      //d.text_fmt("// %s %s\n",item._type,buf);

      // find out the name of the last named part
      sig_part_vector parts = item._id._parts;
      const char *name = NULL;

      for (sig_part_vector::reverse_iterator p_iter =
	     parts.rbegin(); p_iter != parts.rend(); ++p_iter)
	{
	  sig_part &part = *p_iter;

	  if (part._type & SIG_PART_NAME)
	    {
	      name = part._id._name;
	      break;
	    }
	}

      if (mode == CORR_MODE_T)
	if (!name || strcasecmp(name,"T") != 0)
	  continue;
      if (mode == CORR_MODE_E)
	if (!name || strcasecmp(name,"E") != 0)
	  continue;

      // The item is choosen

      items.push_back(&item);
      tree.insert(item._id._parts.begin(),
		  item._id._parts.end());

      //
    }

  // Calculate the sizes of all branches at all nodes, and get a list of them
  std::set<int> sizes;
  int members = tree.calc_sizes(sizes);
  // Get a list of all sizes that exist

  dumper cd(d,0,true);
  cd.text("\n");

  for (std::set<int>::iterator size_iter = sizes.begin();
       size_iter != sizes.end(); ++size_iter)
    {
      int size = *size_iter;
      int chunks = tree.num_chunks(size);

      int mem  = size*chunks;
      int line = mem+chunks;

      int total = members * line;

      cd.text_fmt("size=%2d  chunks=%3d  mem=%4d  line=%d  total=%d",
		  size,chunks,mem,line,total);

      cd.nl();
    }

  cd.text("\n");
  cd.text_fmt("corr structure: %s\n",type);

  dumper sd(cd,2);

  for (corr_struct_item_ptr_vect::iterator iter = items.begin();
       iter != items.end(); ++iter)
    {
      corr_struct_item *item = *iter;

      char buf[256];
      item->_id.format(buf,sizeof(buf));

      sd.text_fmt("%s %s\n",item->_type,buf);
    }

  tree.dump(cd);

}

void c_struct_def::corr_struct(dumper &d) const
{
  const char *type = "UNKNOWN";

  if (_type)
    type = _type->_name;

  dumper cd(d,0,true);

  cd.text_fmt("Corr struct for: %s\n",type);

  // Now, we need to for us (and any base classes) find all data
  // members that may be of interest for making correlation plots (of
  // channels that have fired in the same event)

  // We want to be able to make certain selections.  E.g. only T or
  // only E or both kind of variables for the entire sub-structures

  // When doing the correlation plot for a certain structure, this is
  // to contain all the substructures (which may be of differing
  // dimensions)

  // But basically, we must flatten the structures, since the picture
  // we are about to draw only can contain a flat linear index.

  // The base classes are as strings...  Do not care yet, just print
  // them...

  corr_items items;

  signal_id id;

  find_members(cd,id,items);

  ///////////////////////////////////////////////////////////////////
  // Just dump what we have
  corr_struct_item_vect::iterator iter;

  for (iter = items._vect.begin(); iter != items._vect.end(); ++iter)
    {
      corr_struct_item &item = *iter;

      char buf[256];
      item._id.format(buf,sizeof(buf));

      cd.text_fmt("%s %s\n",item._type,buf);
    }
  ///////////////////////////////////////////////////////////////////

  for (int i = 0; i < NUM_CORR_MODES; i++)
    make_corr_struct(type,d,items,i);




}


void corr_struct()
{
  print_header("CORR_STRUCT","Correlation structure.");

  dumper_dest_file d_dest(stdout);
  dumper d(&d_dest);

  {
    named_c_struct_def_vect::iterator i;

    for (i = all_named_c_struct_vect.begin(); i != all_named_c_struct_vect.end(); ++i)
      {
	(*i)->corr_struct(d);
	d.nl();
      }
  }



  print_footer("CORR_STRUCT");
}
