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

#include "structure.hh"
#include "definitions.hh"
#include "unpack_code.hh"
#include "dump_list.hh"
#include "parse_error.hh"
#include "str_set.hh"
#include "account.hh"

// We generate the unpack code recursively, i.e. whenever an subevent
// needs another subevent, that other subevent has to be processed
// first.

// Since we have no idea where to begin, we'll simply start with
// whatever we find, and if something else is needed, we go there
// instead.

const struct_match_end *
has_match_end(const struct_item_list *_items,
	      const struct_match_end *prev_match_end,
	      bool *explicit_match_end)
{
  // return true if we found a match_end item

  struct_item_list::const_iterator i;

  for (i = _items->begin(); i != _items->end(); ++i)
    {
      struct_item *item = *i;

      const struct_data      *data;
      const struct_list      *list;
      const struct_cond      *cond;
      const struct_match_end *match_end;

      if ((match_end = dynamic_cast<const struct_match_end* >(item)))
	{
	  if (prev_match_end)
	    ERROR_LOC_PREV(match_end->_loc,
			   prev_match_end->_loc,
			   "MATCH_END already specified.");
	  prev_match_end = match_end;
	  *explicit_match_end = true;
	}
      if ((list    = dynamic_cast<const struct_list*   >(item)))
	{
	  // Hmm, this is actually not really sane, since a list
	  // cannot be guaranteed to contain any item (i.e. not be
	  // guaranteed to execute at all.  Same applies at least for
	  // the list generating code.  So interpretation is:  if list has
	  // no items, it cannot match!

	  prev_match_end =
	    has_match_end(list->_items,prev_match_end,explicit_match_end);
	}
      if ((cond    = dynamic_cast<const struct_cond*   >(item)))
	{
	  const struct_match_end *end1 =
	    has_match_end(cond->_items,prev_match_end,
			  explicit_match_end);
	  if (cond->_items_else)
	    {
	      const struct_match_end *end2 =
		has_match_end(cond->_items_else,prev_match_end,
			      explicit_match_end);

	      // If both clauses have a match_end, then we have a
	      // match_end (propagate last one, from else clause for
	      // eventual error messages)

	      if (end1 && end2)
		prev_match_end = end2;
	    }
	}
      if ((data    = dynamic_cast<const struct_data*   >(item)))
	{
	  // We inspect that no data item after a match end is called
	  // MATCH...  Hmm, that's for later...


	}
    }

  return prev_match_end;
}

bool match_end_info::can_end(bool had_item)
{
  if (_has_explicit_end)
    return _match_ended;

  if (had_item)
    _match_ended = true;

  return _match_ended;
}

void match_end_info::set_end()
{
  assert(_has_explicit_end);
  _match_ended = true;
}

void generate_unpack_code(struct_definition *structure)
{
  if (structure->_code)
    {
      if (!structure->_code->_done)
	ERROR_LOC(structure->_loc,
		  "Cannot generate code recursively, %s has a loop.",
		  structure->_header->_name);
      return; // The code was already generated
    }

  printf ("//\n"
	  "// Generating code for: %s\n"
	  "//\n",structure->_header->_name);

  structure->find_member_args();

  struct_unpack_code *code = structure->_code = new struct_unpack_code;

  dumper hd(&code->_headers);
  dumper udu(&code->_code_unpack);
  dumper udm(&code->_code_match);
  dumper udp(&code->_code_packer);

  if (structure->_opts & STRUCT_DEF_OPTS_EXTERNAL)
    {
      printf ("\n"
	      "// Structure is external.  Must be provided by the user\n"
	      "\n");
    }
  else
    {
      code->gen(structure,hd,UCT_HEADER,NULL);
      code->gen(structure,udu,UCT_UNPACK,NULL);
      code->gen(structure,udp,UCT_PACKER,NULL);

      match_end_info mei;

      mei._has_explicit_end = false;
      mei._match_ended = false;

      has_match_end(structure->_body,NULL,
		    &mei._has_explicit_end);

      code->gen(structure,udm,UCT_MATCH,&mei);
    }

  // Now we're done! (now we may be called without recursion)
  code->_done = true;
  // Now is also a good time to write us out to file?
  // This would then make sure that we end up in order

  char comment[512];

  sprintf (comment,
           "Event unpacker associated structures for %s.",
           structure->_header->_name);
  print_header("STRUCTURES",comment);
  code->_headers.fwrite(stdout);
  print_footer("STRUCTURES");

  sprintf (comment,
           "Event unpacker for %s.",
           structure->_header->_name);
  print_header("UNPACKER",comment);
  code->_code_unpack.fwrite(stdout);
  print_footer("UNPACKER");

  sprintf (comment,
           "Event matcher for %s.",
           structure->_header->_name);
  print_header("MATCHER",comment);
  code->_code_match.fwrite(stdout);
  print_footer("MATCHER");

  sprintf (comment,
           "Event packer for %s.",
           structure->_header->_name);
  print_header("PACKER",comment);
  code->_code_packer.fwrite(stdout);
  print_footer("PACKER");
}

void generate_unpack_code(event_definition *event)
{
  if (event->_code)
    {
      ERROR_LOC(event->_loc,"Recursive call generating event."); // cannot happen?
      return; // The code was already generated
    }

  printf ("//\n"
	  "// Generating code for EVENT\n"
	  "//\n");

  struct_unpack_code *code = event->_code = new struct_unpack_code;

  dumper hd(&code->_headers);
  dumper udu(&code->_code_unpack);
  dumper udp(&code->_code_packer);
  dumper udr(&code->_code_revoke);

  code->gen(event,hd,UCT_HEADER);
  code->gen(event,udu,UCT_UNPACK);
  code->gen(event,udp,UCT_PACKER);
  code->gen(event,udr,UCT_REVOKE);

  // Now we're done! (now we may be called without recursion)
  code->_done = true;
  // Now is also a good time to write us out to file?
  // This would then make sure that we end up in order

  char comment[512];

  sprintf (comment,
           "Event unpacker associated structures for EVENT.");
  print_header("STRUCTURES",comment);
  code->_headers.fwrite(stdout);
  print_footer("STRUCTURES");

  sprintf (comment,
           "Event unpacker for EVENT.");
  print_header("UNPACKER",comment);
  code->_code_unpack.fwrite(stdout);
  print_footer("UNPACKER");

  sprintf (comment,
           "Event packer for EVENT.");
  print_header("PACKER",comment);
  code->_code_packer.fwrite(stdout);
  print_footer("PACKER");

  sprintf (comment,
           "Event revoker for EVENT.");
  print_header("REVOKE",comment);
  code->_code_revoke.fwrite(stdout);
  print_footer("REVOKE");

  sprintf (comment,
           "Mappings of names for [incl|excl] name lookup.");
  print_header("SUBEVENT_NAMES",comment);
  dumper_dest_file d_dest(stdout);
  dumper d(&d_dest);
  gen_subevent_names(event,d);
  print_footer("SUBEVENT_NAMES");

  if (event->_opts & EVENT_OPTS_STICKY)
    {
      print_header("UNPACKER_DEFINES","Control" /* hmmm */);
      d.text_fmt("#define STICKY_EVENT_IS_NONTRIVIAL  %d\n\n",
		 !(event->_opts & EVENT_OPTS_INTENTIONALLY_EMPTY));
      print_footer("UNPACKER_DEFINES");
    }
}

#define COMMENT_DUMP_ORIG(d,i) { dumper __cd(d,0,true); i->dump(__cd,false); }

void struct_unpack_code::gen(const struct_definition *str,
			     dumper &d,uint32 type,
			     match_end_info *mei)
{
  COMMENT_DUMP_ORIG(d,str);

  const struct_header_subevent *subevent_header = dynamic_cast<const struct_header_subevent *>(str->_header);

  if (type & UCT_HEADER)
    {
      d.text("#if !PACKER_CODE\n");
      d.text("# define DECLARED_UNPACK_");
      d.text(str->_header->_name);
      d.text("\n");
      d.text("class ");
      d.text(str->_header->_name);
      d.text("\n");
      d.text("#else//PACKER_CODE\n");
      d.text("# define DECLARED_PACKER_");
      d.text(str->_header->_name);
      d.text("\n");
      d.text("class PACKER_");
      d.text(str->_header->_name);
      d.text("\n");
      d.text("#endif//PACKER_CODE\n");
      if (subevent_header)
	d.text_fmt(" : public unpack_%ssubevent_base",
		   str->_opts & STRUCT_DEF_OPTS_STICKY_SUBEV ?
		   "sticky_" : "");
      d.text("\n");
      d.text("{\n");
      d.text("public:\n");
    }

  const struct_header_named *named_header = dynamic_cast<const struct_header_named *>(str->_header);

  if (type & UCT_UNPACK)
    {
      d.text("template<typename __data_src_t>\n");
      d.text("void ");
      d.text(str->_header->_name);
      d.text("::__unpack(__data_src_t &__buffer");
      if (named_header)
	gen_params(named_header->_params,d,type,true,false);
      d.text(")\n");
    }
  if (type & UCT_MATCH)
    {
      if (subevent_header)
	{
	  d.text("// No __match function for subevents.\n");
	  return;
	}
      d.text("template<typename __data_src_t>\n");
      d.text("bool ");
      d.text(str->_header->_name);
      d.text("::__match(__data_src_t &__buffer");
      if (named_header)
	gen_params(named_header->_params,d,type,false,false);
      d.text(")\n");
    }
  if (type & UCT_PACKER)
    {
      d.text("template<typename __data_dest_t>\n");
      d.text("void PACKER_");
      d.text(str->_header->_name);
      d.text("::__packer(__data_dest_t &__buffer");
      if (named_header)
	gen_params(named_header->_params,d,type,true,false);
      d.text(")\n");
    }

  gen(str->_header,str->_body,d,type,mei,!!subevent_header,true);

  if (type & UCT_HEADER)
    {
      d.nl();
      d.text("public:\n");
      d.col0_text("#ifndef __PSDC__\n");
      d.col0_text("# if !PACKER_CODE\n");
      d.text("template<typename __data_src_t>\n");
      d.text("  void __unpack(__data_src_t &__buffer");
      if (named_header)
	gen_params(named_header->_params,d,type,true,true);
      d.text(");\n");
      d.text("template<typename __data_src_t>\n");
      d.text("  static bool __match(__data_src_t &__buffer");
      if (named_header)
	gen_params(named_header->_params,d,type,false,true);
      d.text(");\n");
      d.text("  // void __clean();\n");
      d.col0_text("# else//PACKER_CODE\n");
      d.text("template<typename __data_dest_t>\n");
      d.text("  void __packer(__data_dest_t &__buffer");
      if (named_header)
	gen_params(named_header->_params,d,type,true,true);
      d.text(");\n");
      d.col0_text("# endif//PACKER_CODE\n");
      d.nl();
      d.text_fmt("  STRUCT_FCNS_DECL(%s);\n",str->_header->_name);
      d.col0_text("#endif//!__PSDC__\n");
      d.text("};\n");
    }

  if (type & (UCT_UNPACK | UCT_MATCH | UCT_PACKER))
    {
      bool args = named_header && !named_header->_params->empty();

      d.text_fmt("FORCE_IMPL_DATA_SRC_FCN%s(%s,%s::%s",
		 args ? "_ARG" : "",
		 type & UCT_MATCH ? "bool" : "void",
		 str->_header->_name,
		 type & UCT_MATCH ? "__match" :
		 (type & UCT_UNPACK ? "__unpack" : "__packer"));
      if (named_header)
	gen_params(named_header->_params,d,type,!!(type & UCT_UNPACK),false);
      d.text(");\n");
    }

}

void gen_indexed_decl(indexed_decl_map &indexed_decl,dumper &d)
{
  indexed_decl_map_rev_sort indexed_decl_rev_sort;

  for (indexed_decl_map::iterator array = indexed_decl.begin();
       array != indexed_decl.end(); array++)
    {
      indexed_decl_rev_sort.
	insert(indexed_decl_map_rev_sort::value_type(array->second,
						     array->first));
    }

  for (indexed_decl_map_rev_sort::iterator array =
	 indexed_decl_rev_sort.begin();
       array != indexed_decl_rev_sort.end(); array++)
    {
      indexed_type_ind *info = array->first;

      d.text_fmt("%s(",
		 (info->_opts & STRUCT_DECL_OPTS_MULTI) ?
		 "MULTI" : "SINGLE");
      d.text_fmt("%s,",info->_type);
      d.text_fmt("%s",array->second);
      if (info->_max_items)
	d.text_fmt("[%d]",info->_max_items);
      if (info->_max_items2)
	d.text_fmt("[%d]",info->_max_items2);
      d.text(");\n");
    }
}

void struct_unpack_code::gen(const struct_header *header,
			     const struct_item_list *list,
			     dumper &d,uint32 type,
			     match_end_info *mei,
			     bool last_subevent_item,
			     bool is_function)
{
  if (type & (UCT_UNPACK | UCT_MATCH | UCT_PACKER))
    d.text("{\n");

  struct_item_list::const_iterator i;

  dumper sd(d,2);

  indexed_decl_map indexed_decl;

  for (i = list->begin(); i < list->end(); )
    {
      // We now need to call the correct function per the type

      const struct_item*    item = *i;

      ++i;

      COMMENT_DUMP_ORIG(sd,item);

      sd.nl();

      gen(indexed_decl,
	  header,item,sd,type,mei,last_subevent_item && i == list->end());

      if (mei && mei->_match_ended)
	break;
    }

  if (type & UCT_HEADER)
    {
      // Now dump any (delayed) array items
      gen_indexed_decl(indexed_decl,sd);
    }

  if (type & UCT_MATCH)
    {
      if (mei->_match_ended || is_function)
	sd.text("return false;\n");
    }
  if (type & (UCT_UNPACK | UCT_MATCH | UCT_PACKER))
    d.text("}\n");
}

void struct_unpack_code::gen(const file_line &loc,
			     const struct_decl_list *list,
			     dumper &d,uint32 type)
{
  struct_decl_list::const_iterator i;

  dumper sd(d,2);

  for (i = list->begin(); i < list->end(); ++i)
    {
      // We now need to call the correct function per the type

      const struct_item*    item = *i;

      COMMENT_DUMP_ORIG(sd,item);

      sd.nl();
    }
}

indexed_type_ind::indexed_type_ind(const char *type,
				   int max_items,int max_items2,int opts,
				   const file_line &loc)
{
  _type       = type;
  _max_items  = max_items;
  _max_items2 = max_items2;
  _opts       = opts;
  _loc        = loc;
}

void insert_indexed_decl(indexed_decl_map &indexed_decl,
			 const struct_decl *decl,
			 const char *type,
			 const char *name,
			 const var_index *vi,
			 int opts)
{
  // const char *name = vi->_name;

  std::pair<indexed_decl_map::iterator,bool> exist;

  indexed_type_ind *info = new indexed_type_ind(type,
					      vi->_index+1,
					      vi->_index2+1,
					      opts,
					      decl->_loc);

  exist = indexed_decl.insert(indexed_decl_map::value_type(name,info));

  if (!exist.second) // name was already in list
    {
      info = exist.first->second;

      if (strcmp(info->_type,type) != 0)
	ERROR_LOC(decl->_loc,"Items with same name (%s), has different types (%s != %s)",
		  name,info->_type,type);

      if (info->_opts != opts)
	ERROR_LOC(decl->_loc,"Items with same name (%s), has different options (multi)",
		  name);

      if ((info->_max_items == 0) !=
	  (vi->_index == -1))
	ERROR_LOC(decl->_loc,"Items with same name (%s), has different array [] / not array",
		  name);

      if ((info->_max_items2 == 0) !=
	  (vi->_index2 == -1))
	ERROR_LOC(decl->_loc,"Items with same name (%s), has different array [] depths",
		  name);

      if (info->_max_items < vi->_index+1)
	info->_max_items = vi->_index+1;
      if (info->_max_items2 < vi->_index2+1)
	info->_max_items2 = vi->_index2+1;
    }
}


void struct_unpack_code::gen(indexed_decl_map &indexed_decl,
			     const struct_header *header,
			     const struct_item *item,dumper &d,uint32 type,
			     match_end_info *mei,
			     bool last_subevent_item)
{
  const struct_data      *data;
  const struct_decl      *decl;
  const struct_list      *list;
  const struct_select    *select;
  //const struct_several* several;
  const struct_cond      *cond;
  const struct_member    *member;
  const struct_mark      *mark;
  const struct_check_count *check;
  const struct_encode    *encode;
  const struct_match_end *match_end;

  if ((data    = dynamic_cast<const struct_data*   >(item))) gen(header,
								 data   ,d,type,mei);
  if ((decl    = dynamic_cast<const struct_decl*   >(item))) gen(indexed_decl,
								 decl   ,d,type,mei);
  if ((list    = dynamic_cast<const struct_list*   >(item))) gen(header,
								 list   ,d,type,mei,last_subevent_item);
  if ((select  = dynamic_cast<const struct_select* >(item))) gen(select ,d,type,mei,last_subevent_item);
  if ((cond    = dynamic_cast<const struct_cond*   >(item))) gen(header,
								 cond   ,d,type,mei,last_subevent_item);
  if ((member  = dynamic_cast<const struct_member* >(item))) gen(member ,d,type);
  if ((mark    = dynamic_cast<const struct_mark*   >(item))) gen(mark   ,d,type);
  if ((check   = dynamic_cast<const struct_check_count*>(item))) gen(check  ,d,type);
  if ((encode  = dynamic_cast<const struct_encode* >(item))) gen(encode ,d,type);
  if ((match_end = dynamic_cast<const struct_match_end* >(item))) gen(match_end ,d,type,mei);

  if (d._current > d._indent) d.nl();
}

void struct_unpack_code::gen(const encode_spec_base *item,
			     dumper &d,uint32 type,
			     const prefix_ident *prefix)
{
  const encode_spec      *spec;
  const encode_cond      *cond;

  if ((spec = dynamic_cast<const encode_spec*>(item))) gen(spec,d,type,prefix);
  if ((cond = dynamic_cast<const encode_cond*>(item))) gen(cond,d,type,prefix);
}

void struct_unpack_code::gen(const encode_spec *encode,dumper &d,uint32 type,
			     const prefix_ident *prefix)
{
  if (type & UCT_UNPACK)
    {
      // Now we need to recode the data into an member item
      // i.e. first get a pointer to the item

      d.text("{\n");
      dumper sd(d,2);

      const char *item = NULL;

      const var_indexed *vi = dynamic_cast<const var_indexed *>(encode->_name);

      if (vi || (encode->_flags & ES_APPEND_LIST))
	{
	  item = "__item";
	  // Without the use of typeof here, we'd have to keep
	  // track of the types of variables...
	  // The dirty trick with *(&(...)) is such that it should work also
	  // on a reference...
	  sd.text_fmt("typedef __typeof__(*(&(%s",encode->_name->_name);
	  if (vi && vi->_index2)
	    vi->_index2->dump_bracket(sd,prefix);
	  if (vi && (encode->_flags & ES_APPEND_LIST) && vi->_index)
	    vi->_index->dump_bracket(sd,prefix);
	  sd.text("))) __array_t;\n");
	  sd.text("typedef typename __array_t::item_t __item_t;\n");
	  sd.text_fmt("__item_t &%s = %s",
		      item,encode->_name->_name);
	  if (vi && vi->_index2)
	    vi->_index2->dump_bracket(sd,prefix);
	  if (encode->_flags & ES_APPEND_LIST)
	    {
	      if (vi && vi->_index)
		vi->_index->dump_bracket(sd,prefix);
	      sd.text_fmt(".append_item(%d);\n",
			  encode->_loc._internal);
	    }
	  else
	    {
	      sd.text_fmt(".insert_index(%d,",
			  encode->_loc._internal);
	      vi->_index->dump(sd,prefix);
	      sd.text(");\n");
	    }
	}
      else
	item = encode->_name->_name;

      // Then fill in the data members

      argument_list::const_iterator al;
      const argument_list *args = encode->_args;

      for (al = args->begin(); al != args->end(); ++al)
	{
	  const argument *arg = *al;
	  // Handle items with name _ as if it is not a struct
	  if (strcmp(arg->_name,"_") == 0)
	    sd.text_fmt("%s/*._*/ = ",item);
	  else
	    sd.text_fmt("%s.%s = ",item,arg->_name);
	  // TODO, this needs improvement to figure out from where to resolve the symbol,
	  // right now it must come from the unpacked data word!  (or be a constant)
	  // if (!dynamic_cast<const var_const *>(arg->_var))
	  //  sd.text_fmt("%s",name_prefix);
	  arg->_var->dump(sd,prefix);
	  sd.text(";\n");
	}

      d.text("}\n");
    }
}

void struct_unpack_code::gen(const encode_cond *cond,dumper &d,uint32 type,
			     const prefix_ident *prefix)
{
  // This is largely duplicated from gen(const struct_cond *cond, ...)

  const var_external *external;

  external = gen_external_header(cond->_expr,d,type);

  if (type & UCT_UNPACK)
    {
      d.text("if (");

      if (external)
	d.text_fmt("%s()",external->_name);
      else
	cond->_expr->dump(d,prefix);

      d.text(")\n");
    }

  gen(cond->_items,d,type,prefix);

  if (cond->_items_else)
    {
      if (type & (UCT_UNPACK | UCT_MATCH | UCT_PACKER))
	{
	  d.text("else\n");
	  gen(cond->_items_else,d,type,prefix);
	}
    }
}

void struct_unpack_code::gen(const encode_spec_list *list,dumper &d,uint32 type,
			     const prefix_ident *prefix)
{
  encode_spec_list::const_iterator encode;
  bool indent = list->size() > 1;

  if (indent)
    d.text("{\n");
  dumper sd(d,indent ? 2 : 0);

  for (encode = list->begin();
       encode != list->end(); ++encode)
    gen(*encode,sd,type,prefix);

  if (indent)
    d.text("}\n");
}

void struct_unpack_code::gen(const struct_encode *encode,dumper &d,uint32 type)
{
  gen(encode->_encode,d,type,NULL);
}

void struct_unpack_code::gen(const struct_header *header,
			     const struct_data *data,dumper &d,uint32 type,
			     match_end_info *mei)
{
  const char *data_type = "";
  const char *full_name = "";
  static int data_done_counter = 0;

  // if the value is declared as no-enocde, it should not go into the data
  // structure, but is needed in the event unpacker...

  switch (data->_size)
    {
    case 64: data_type = "uint64 "; full_name = "u64"; break;
    case 32: data_type = "uint32 "; full_name = "u32"; break;
    case 16: data_type = "uint16 "; full_name = "u16"; break;
    case 8:  data_type = "uint8  "; full_name = "u8"; break;
    }

  const char *prefix = "";

  bool do_declare = false;

  if (!(data->_flags & SD_FLAGS_NOENCODE))
    {
      // We want it in the header

      if (data->_flags & SD_FLAGS_SEVERAL)
	ERROR_LOC(data->_loc,
		  "Item marked several without marked NOENCODE, "
		  "cannot store multiple items...");

      if (type & (UCT_HEADER | UCT_MATCH))
	do_declare = true;
      if (type & UCT_MATCH)
	prefix = "__";
    }
  else
    {
      // We do not want it in the header

      if (type & (UCT_UNPACK | UCT_MATCH))
	do_declare = true;

      // prefix = "__"; // TODO: remove comment, implement variable tracker
    }

  if ((type & UCT_UNPACK) &&
      (data->_flags & SD_FLAGS_SEVERAL))
    d.text("for ( ; ; ) {\n");

  char* data_done_label = NULL;

  if (!data->_bits)
    {
      // Simple case of no bit-fields

      if (do_declare)
	d.text_fmt("%s %s%s;",data_type,prefix,data->_ident);
      if (type & UCT_MATCH)
	d.nl();

      // Unpack the data from the buffer

      if (type & (UCT_UNPACK | UCT_MATCH))
	{
	  const char *match_prefix = "";

	  if (type & UCT_MATCH)
	    match_prefix = "MATCH_";

	  if (data->_flags & (SD_FLAGS_OPTIONAL | SD_FLAGS_SEVERAL))
	    {
	      // If there is no bitfield, the only thing that can stop
	      // us is if the buffer is empty.  (if there are half the
	      // bytes left, we declare it as non-empty, but will puke
	      // at not being able to read the entire item.  Normal
	      // behaviour of a select statement would be to say that
	      // we do not fit...

	      char label[128];
	      sprintf (label,"data_done_%d",data_done_counter++);
	      data_done_label = strdup(label);

	      d.text_fmt("if (__buffer.empty()) goto %s;\n",data_done_label);
	    }

	  int account_id =
	    new_account_item(header->_name, data->_ident);

	  d.text_fmt("%sREAD_FROM_BUFFER(%d,%s,%s%s,%d);\n",
		     match_prefix,
		     data->_loc._internal,
		     data_type,prefix,data->_ident,
		     account_id);

	  if ((type & UCT_UNPACK) &&
	      (data->_flags & SD_FLAGS_SEVERAL))
	    d.text("}\n");

	  if (data_done_label)
	    d.text_fmt("%s:;\n",data_done_label);
	  free(data_done_label);
	}
      if (type & UCT_MATCH)
	{
	  if (mei->can_end(true))
	    {
	      d.text("return true;\n");
	      return;
	    }
	}
      return;
    }

  bool declare_temp = false;

  if ((type & UCT_UNPACK) &&
      (data->_flags & (SD_FLAGS_OPTIONAL | SD_FLAGS_SEVERAL)))
    {
      if (!do_declare)
	{
	  do_declare = true;
	  prefix = "__";

	  declare_temp = true;
	}
    }

  if (do_declare)
    {
      d.text("union\n");
      d.text("{\n");
      dumper sd(d,2);
      // Now dump the bitfields
      sd.text("struct\n");
      sd.text("{\n");
      dumper ssd(sd,2);
      // We need to eject the bitfield in two directions, for little and
      // big endian machines...

#define BITFIELD_DUMMY(d,data_type,first,last) { \
  (d).text(data_type);    \
  if ((first) == (last))  \
    (d).text_fmt("dummy_%d : 1;\n",(first)); \
  else                                       \
    (d).text_fmt("dummy_%d_%d : %d;\n",(first),(last),(last)-(first)+1); \
}
#define BITFIELD_NAMED(d,data_type,b) { \
  (d).text(data_type);     \
  if ((b)->_name)          \
    (d).text((b)->_name);  \
  else                     \
    (d).text_fmt("unnamed_%d_%d",(b)->_min,(b)->_max); \
  if ((b)->_min == (b)->_max)                          \
    (d).text_fmt(" : 1; // %d\n",(b)->_min);           \
  else                                                 \
    (d).text_fmt(" : %d; // %d..%d\n",(b)->_max - (b)->_min + 1,(b)->_min,(b)->_max); \
}

      {
	ssd.col0_text("#if __BYTE_ORDER == __LITTLE_ENDIAN\n");
	bits_spec_list::const_iterator i;
	int next_bit = 0;
	for (i = data->_bits->begin(); i != data->_bits->end(); ++i)
	  {
	    bits_spec *b = *i;
	    if (b->_min > next_bit)
	      BITFIELD_DUMMY(ssd,data_type,next_bit,b->_min-1);

	    BITFIELD_NAMED(ssd,data_type,b);
	    next_bit = b->_max+1;
	  }
	if (data->_size > next_bit)
	  BITFIELD_DUMMY(ssd,data_type,next_bit,data->_size-1);
	ssd.col0_text("#endif\n");
      }
      {
	ssd.col0_text("#if __BYTE_ORDER == __BIG_ENDIAN\n");
	bits_spec_list::const_reverse_iterator ri;
	int last_bit = data->_size-1;
	for (ri = data->_bits->rbegin(); ri != data->_bits->rend(); ++ri)
	  {
	    bits_spec *b = *ri;
	    if (b->_max < last_bit)
	      BITFIELD_DUMMY(ssd,data_type,b->_max+1,last_bit);

	    BITFIELD_NAMED(ssd,data_type,b);
	    last_bit = b->_min-1;
	  }
	if (last_bit > 0)
	  BITFIELD_DUMMY(ssd,data_type,0,last_bit);
	ssd.col0_text("#endif\n");
      }
      sd.text("};\n"); // unnamed structure (gcc extension)
      // The dump the full value
      sd.text_fmt("%s %s;\n",data_type,full_name);
      d.text_fmt("} %s%s;",prefix,data->_ident);
      if (!(type & UCT_HEADER))
	d.nl();
    }

  if (type & (UCT_UNPACK | UCT_MATCH))
    {
      const char *match_prefix = "";
      const char *read_prefix = "READ";

      if (type & UCT_MATCH)
	match_prefix = "MATCH_";

      if (data->_flags & (SD_FLAGS_OPTIONAL | SD_FLAGS_SEVERAL))
	{
	  char label[128];
	  sprintf (label,"data_done_%d",data_done_counter++);
	  data_done_label = strdup(label);
	  d.text_fmt("if (__buffer.empty()) goto %s;\n",data_done_label);

	  match_prefix = "";
	  read_prefix = "PEEK";
	}

      // First we need to get the data from the buffer

      int account_id =
	new_account_item(header->_name, data->_ident);

      d.text_fmt("%s%s_FROM_BUFFER_FULL(%d,%s,%s,%s%s.%s,%d);\n",
		 match_prefix,
		 read_prefix,
		 data->_loc._internal,
		 data_type,data->_ident,prefix,data->_ident,full_name,
		 account_id);

      // Then we need to check it...

      const char *check_prefix = type & UCT_MATCH ? "MATCH" : "CHECK";

      const char *check_data_done_label = NULL;

      if ((type & UCT_UNPACK) &&
	  (data->_flags & (SD_FLAGS_OPTIONAL | SD_FLAGS_SEVERAL)))
	{
	  check_prefix = "CHECK_JUMP";
	  check_data_done_label = data_done_label;
	}

      {
#define NUM_BITS_MASK(bits) ((bits) == 64 ? 0xffffffffffffffffLL : ((((uint64) 1) << (bits)) - 1))

	bits_spec_list::const_iterator i;
	uint64 unnamed_bits = NUM_BITS_MASK(data->_size);
	for (i = data->_bits->begin(); i != data->_bits->end(); ++i)
	  {
	    bits_spec *b = *i;

	    unnamed_bits &= ~( NUM_BITS_MASK(b->_max+1) ^ NUM_BITS_MASK(b->_min));

	    //if (b->_min > next_bit)
	    //BITFIELD_DUMMY(sshd,data_type,next_bit,b->_min-1);

	    //BITFIELD_NAMED(sshd,data_type,b);
	    //next_bit = b->_max+1;

	    if (b->_cond)
	      {
		char name[512];

		if (b->_name)
		  sprintf(name,"%s%s.%s",prefix,data->_ident,b->_name);
		else
		  sprintf(name,"%s%s.unnamed_%d_%d",prefix,data->_ident,b->_min,b->_max);

		b->_cond->check(d,check_prefix,name,check_data_done_label);
	      }
	  }

	if (unnamed_bits)
	  {
	    d.text_fmt("%s_UNNAMED_BITS_ZERO(%d,%s%s.%s,0x%0*llx%s",
		       check_prefix,
		       data->_loc._internal,
		       prefix,data->_ident,full_name,data->_size/4,
		       unnamed_bits,data->_size == 64 ? "ll" : "");
	    if (check_data_done_label)
	      d.text_fmt(",%s",check_data_done_label);
	    d.text(");\n");
	  }

	if (declare_temp)
	  d.text_fmt("%s.%s = %s%s.%s;\n",
		     data->_ident,full_name,
		     prefix,data->_ident,full_name);

	if (data->_flags & (SD_FLAGS_OPTIONAL | SD_FLAGS_SEVERAL))
	  {
	    // Data item has been declared to match, so copy it from
	    // the temporary to the real item.  And advance the source
	    // pointer...  Hmm, a match will never check more than the
	    // first, but better advance anyhow...

	    d.text_fmt("__buffer.advance(sizeof(%s%s.%s));\n",
		       prefix,data->_ident,full_name);
	  }

	if (data->_encode)
	  {
	    char name_prefix_str[512];

	    sprintf(name_prefix_str,"%s.",data->_ident);

	    prefix_ident name_prefix;

	    name_prefix._list = data->_bits;
	    name_prefix._prefix = name_prefix_str;

	    gen(data->_encode,d,type,&name_prefix);
	  }
      }

      if (type & UCT_MATCH)
	{
	  if (mei->can_end(true))
	    {
	      d.text("return true;\n");
	      if (!data_done_label)
		return;
	    }
	}

      if ((type & UCT_UNPACK) &&
	  (data->_flags & SD_FLAGS_SEVERAL))
	d.text("}\n");

      if (data_done_label)
	d.text_fmt("%s:;\n",data_done_label);
      free(data_done_label);
    }
}

const struct_header_named *find_decl(const struct_decl* decl,bool subevent)
{
  struct_definition *str_decl;

  if (subevent)
    str_decl = find_subevent_structure(decl->_ident);
  else
    str_decl = find_named_structure(decl->_ident);

  if (!str_decl)
    ERROR_LOC(decl->_loc,"%s %s not defined.",subevent ? "Subevent":"Structure",decl->_ident);
  const struct_header_named *named_header =
    dynamic_cast<const struct_header_named *>(str_decl->_header);
  if (!named_header && !subevent)
    ERROR_LOC(decl->_loc,"Structure %s defintion does not have a parameter list (even empty).",
	      decl->_ident);

  if (!(decl->_opts & STRUCT_DECL_OPTS_EXTERNAL) !=
      !(str_decl->_opts & STRUCT_DEF_OPTS_EXTERNAL))
    {
      WARNING_LOC(decl->_loc,"Structure %s usage and declaration do not agree about being external.",
		  decl->_ident);
      ERROR_LOC(str_decl->_loc,"Declaration was here.");
    }

  generate_unpack_code(str_decl);

  return named_header;
}

void gen_decl(indexed_decl_map &indexed_decl,
	      const struct_decl* decl,dumper &d,bool subevent)
{
  // Make sure the structure exists!

  find_decl(decl,subevent);

  // Sanity check.  We can only declare items that have a
  // name, and possibly an index.  If they have an index,
  // we must find all members, and only declare once, with
  // size one larger than largest index

  const var_name  *vn = dynamic_cast<const var_name*>(decl->_name);
  const var_index *vi = dynamic_cast<const var_index*>(decl->_name);

  if (vi && vi->_index != -1)
    {
      // We cannot declare indexed items until we have seen
      // all, such that they get declared with the maximum
      // index+1

      // If the type names are different, it is also
      // impossible

      insert_indexed_decl(indexed_decl,decl,decl->_ident,vi->_name,vi,decl->_opts);
    }
  else if (vn)
    {
      var_index var_index_non_array(file_line(-1), NULL,-1);

      // We insert also non-array members in the list.  Thus
      // we'll be able to handle cases where the same name is reused

      insert_indexed_decl(indexed_decl,decl,decl->_ident,vn->_name,&var_index_non_array,decl->_opts);
    }
  else
    ERROR_LOC(decl->_loc,"Internal error, cannot declare item.");
}

void struct_unpack_code::gen(indexed_decl_map &indexed_decl,
			     const struct_decl *decl,dumper &d,uint32 type,
			     match_end_info *mei)
{
  if (type & (UCT_HEADER))
    {
      gen_decl(indexed_decl,decl,d,false);
      /*
      d.text_fmt("%s(",(decl->_opts & STRUCT_DECL_OPTS_MULTI) ? "MULTI" : "SINGLE");
      d.text(decl->_ident);
      d.text(",");
      decl->_name->dump(d);
      d.text(");");
      */
    }

  if (type & (UCT_UNPACK | UCT_MATCH | UCT_PACKER))
    {
      // before we can unpack, we need to find the associated structure

      const char *func_decl = NULL;

      if (type & UCT_UNPACK) func_decl = "UNPACK_DECL";
      if (type & UCT_PACKER) func_decl = "PACK_DECL";
      if (type & UCT_MATCH) func_decl = "CHECK_MATCH_DECL";

      gen_unpack_decl(decl,func_decl,d,
		      !!(type & (UCT_UNPACK | UCT_PACKER)));

      if (type & UCT_MATCH)
	{
	  if (mei->can_end(true))
	    d.text("return true;\n");
	}
    }
}

const var_external *struct_unpack_code::gen_external_header(const variable *var,dumper &d,uint32 type)
{
  const var_external *external;

  external = dynamic_cast<const var_external *>(var);

  if (type & (UCT_HEADER) &&
      external)
    {
      d.col0_text("#ifndef __PSDC__\n");
      d.text_fmt("uint32 %s() const;\n",external->_name);
      d.col0_text("#endif//!__PSDC__\n");
    }

  return external;
}

void struct_unpack_code::gen(const struct_header *header,
			     const struct_list*    list,   dumper &d,uint32 type,
			     match_end_info *mei,
			     bool last_subevent_item)
{
  const var_external *external_max;

  external_max = gen_external_header(list->_max,d,type);

  if (type & (UCT_UNPACK | UCT_MATCH))
    {
      d.text("for (uint32 ");
      list->_index->dump(d);
      d.text(" = ");
      list->_min->dump(d);
      d.text("; ");
      list->_index->dump(d);
      d.text(" < (uint32) (");
      if (external_max)
	d.text_fmt("%s()",external_max->_name);
      else
	list->_max->dump(d);
      d.text("); ++");
      list->_index->dump(d);
      d.text(")\n");
    }

  gen(header,list->_items,d,type,mei,last_subevent_item,false);
}

void struct_unpack_code::gen(const struct_select*select,dumper &d,uint32 type,
			     match_end_info *mei,bool last_subevent_item)
{
  gen_match(select->_loc,
	    select->_items,d,type,mei,false,last_subevent_item,true,false,
	    select->_flags);
}

void struct_unpack_code::gen_check_spurios_match(const struct_decl *decl,dumper &d,
						 const char *abort_spurious_label)
{
  const struct_header_named *named_header = find_decl(decl,false);

  d.text_fmt("CHECK_SPURIOUS_MATCH_DECL(%d,%s,",
	     decl->_loc._internal,abort_spurious_label);
  d.text(decl->_ident);
  dump_param_args(decl->_loc,named_header->_params,decl->_args,d,false);
  d.text(");\n");
}

void struct_unpack_code::gen_unpack_decl(const struct_decl *decl,
					 const char *func_decl,
					 dumper &d,
					 bool dump_member_args)
{
  const struct_header_named *named_header = find_decl(decl,false);

  d.text_fmt("%s(%d,",
	     func_decl,decl->_loc._internal);
  d.text(decl->_ident);
  //decl->_name->dump(ssd);
  d.text(",");
  if (decl->_opts & STRUCT_DECL_OPTS_MULTI)
    {
      d.text("multi_");
      decl->_name->dump(d);
      d.text(".next_free()");
    }
  else
    decl->_name->dump(d);
  //d.text_fmt(",%s,",decl->_ident);

  dump_param_args(decl->_loc,named_header->_params,decl->_args,
		  d,dump_member_args);
  d.text(");\n");
}

param_list *subevent_cond_params = NULL;

void struct_unpack_code::gen_match(const file_line &loc,
				   const struct_decl_list *items,
				   dumper &d,uint32 type,
				   match_end_info *mei,
				   bool subevent,
				   bool last_subevent_item,
				   bool warn_empty,
				   bool sticky,
				   int flags)

{
  gen(loc,items,d,type); // dump comments

  if (type & (UCT_HEADER))
    {
      indexed_decl_map indexed_decl;

      int event_opts = 0;
      struct_decl_list::const_iterator i;
      for (i = items->begin(); i != items->end(); ++i)
	{
	  struct_decl *decl = *i;

	  int event_opt = decl->is_event_opt();

	  if (event_opt)
	    {
	      event_opts |= event_opt;
	      continue;
	    }

	  gen_decl(indexed_decl,decl,d,subevent);
	}
      // Now dump any array items
      gen_indexed_decl(indexed_decl,d);

      // An bitsone array to keep track of who has been visited
      if (subevent)
	{
	  d.text("public:\n");
	  d.col0_text("#ifndef __PSDC__\n");
	  if (items->size())
	    d.text_fmt("  bitsone<%d> __visited;\n",(int) items->size());
	  d.text("  void __clear_visited() {");
	  if (items->size())
	    d.text(" __visited.clear();");
	  d.text(" }\n");
	  d.text_fmt("  bool ignore_unknown_subevent() { return %s; }\n",
		 event_opts & EVENT_OPTS_IGNORE_UNKNOWN_SUBEVENT ? "true" : "false");	  d.col0_text("#endif//!__PSDC__\n");
  	}
    }

  if (type & (UCT_UNPACK | UCT_MATCH | UCT_REVOKE))
    {
      char *abort_spurious_label = NULL;
      static int check_visit_counter = 0;
      static int no_match_counter = 0;

      // The case of several:
      //
      // We try to match items, and continue as long as we find matches.
      // In case there is no match, we return

      if (flags & SS_FLAGS_SEVERAL)
	{
	  int num_check_visit = 0;

	  struct_decl_list::const_iterator i;
	  for (i = items->begin(); i != items->end(); ++i)
	    {
	      struct_decl *decl = *i;
	      if (decl->_opts & STRUCT_DECL_OPTS_NO_REVISIT)
		num_check_visit++;
	    }
	  if (num_check_visit)
	    {
	      check_visit_counter++;
	      d.text_fmt("bitsone<%d> __visited%d;\n",
			 num_check_visit,check_visit_counter);
	      d.text_fmt("__visited%d.clear();\n",
			 check_visit_counter);
	    }

	  d.text("for ( ; ; )\n");
	}
      else if (!subevent)
	d.text("do\n");
      d.text("{\n");
      // First we need to match the next word
      dumper sd(d,2);

      // First try to see if we can generate an optimized matcher, i.e.
      // if all the parts that we want to select between reads the same kind
      // of data word (uint16 / uint32...) and then if they are looking at
      // the bits in such a way that we can just take a quick look at the
      // word in order to decide

      if (!subevent)
	{
	  if (flags & SS_FLAGS_SEVERAL)
	    sd.text("if (__buffer.empty()) break;\n"); // nothing more in subevent, we're done
	  else if (flags & SS_FLAGS_OPTIONAL)
	    {
	      no_match_counter++;
	      sd.text_fmt("if (__buffer.empty()) goto no_match_%d;\n",no_match_counter);
	    }
	  else
	    {
	      // If we're not optional, the peek operation will fail
	      // (as it should)
	    }
	}

      sd.text("int __match_no = 0;\n");
      // All elements in a select must be encoded via substructures, such
      // that we can use the match functions, and do not need to do any
      // inlining.  This also has the side-effect that our __match
      // variable will not shadow any outer one...  To be empty will
      // however cause a run-time error since no-one will match...
      if (warn_empty &&
	  items->size() == 0)
	WARNING_LOC(loc,"select statement with no entries "
		    "(will give run-time error)\n");

      int normal_match = true;

      if (!subevent && !(type & UCT_MATCH))
	{
	  if (!last_subevent_item)
	    {
	      char label[128];
	      static int spurious_label_counter = 0;

	      sprintf (label,"spurious_match_abort_loop_%d",
		       spurious_label_counter++);
	      abort_spurious_label = strdup(label);
	    }

	  if (gen_optimized_match(loc,items,sd,
				  abort_spurious_label,last_subevent_item))
	    normal_match = false;
	  else
	    {
	      sd.text("__buffer.peeking();\n");
	      free(abort_spurious_label);
	      abort_spurious_label = NULL;
	    }
	}

      if (normal_match)
      {
	struct_decl_list::const_iterator i;
	int index = 1;
	for (i = items->begin(); i != items->end(); ++i, ++index)
	  {
	    struct_decl *decl = *i;

	    if (decl->is_event_opt())
	      continue;

	    const struct_header_named *named_header = find_decl(decl,subevent);

	    if (subevent)
	      {
		sd.text_fmt("MATCH_SUBEVENT_DECL(%d,__match_no,%d,(",
			    decl->_loc._internal,index);
		dump_match_args(decl->_loc,subevent_cond_params,decl->_args,sd);
		sd.text("),");
		decl->_name->dump(sd);
		// dump_param_args(decl->_loc,named_header->_params,decl->_args,sd);
		sd.text(");\n");
	      }
	    else
	      {
		sd.text_fmt("MATCH_DECL(%d,__match_no,%d,",
			    decl->_loc._internal,index);
		sd.text(decl->_ident);
		sd.text(",");
		decl->_name->dump(sd);
		dump_param_args(decl->_loc,named_header->_params,decl->_args,sd,false);
		sd.text(");\n");
	      }
	  }
      }
      {
	// If we are running with several, then we may at any time run out of matches...
	if (subevent)
	  sd.text("if (!__match_no) return 0;\n");
	else
	  {
	    if (flags & SS_FLAGS_SEVERAL)
	      sd.text("if (!__match_no) break;\n");
	    else if (flags & SS_FLAGS_OPTIONAL)
	      {
		sd.text_fmt("if (!__match_no) goto no_match_%d;\n",no_match_counter);
	      }
	    else
	      {
		sd.text_fmt("if (!__match_no) ERROR_U_LOC(%d,\"No match for select statement.\");\n",loc._internal);
	      }
	  }
	if (type & UCT_MATCH)
	  {
	    if (mei->can_end(true))
	      sd.text("return true;\n");
	  }
	if (type & (UCT_UNPACK | UCT_REVOKE))
	  {
	    sd.text("switch (__match_no)\n");
	    sd.text("{\n");
	    dumper ssd(sd,2);
	    struct_decl_list::const_iterator i;
	    int index = 1;
	    int index_check_visit = 0;
	    for (i = items->begin(); i != items->end(); ++i, ++index)
	      {
		struct_decl *decl = *i;

		if (decl->is_event_opt())
		  continue;

		// TODO: is the function call needed?
		/*const struct_header_named *named_header =*/ find_decl(decl,subevent);

		ssd.text_fmt("case %d:\n",index);
		dumper sssd(ssd,2);
		if (subevent)
		  {
		    if (!(decl->_opts & STRUCT_DECL_OPTS_REVISIT))
		      {
			sssd.text_fmt("UNPACK_SUBEVENT_CHECK_NO_REVISIT(%d,%s,",
				      decl->_loc._internal,decl->_ident);
			decl->_name->dump(sssd);
			sssd.text_fmt(",%d);\n",index_check_visit++);
		      }
		    sssd.text_fmt("%s_SUBEVENT_DECL(%d,%d,%s,",
				  type & UCT_UNPACK ? "UNPACK" : "REVOKE",
				  decl->_loc._internal,sticky ? 1 : 0,
				  decl->_ident);
		    decl->_name->dump(sssd);
		    sssd.text(");\n");
		  }
		else
		  {
		    if (abort_spurious_label)
		      {
			// We ran the optimized matcher.  Since we are
			// not the last item in the subevent, we can
			// however not guarantee that just because we
			// found 1 item matching, that it _is_ our
			// match, since it may have been spurios.  If
			// the check however fails, this just means
			// that we should continue unpack the subevent
			// after this select statement

			gen_check_spurios_match(decl,sssd,abort_spurious_label);
		      }

		    if (decl->_opts & STRUCT_DECL_OPTS_NO_REVISIT)
		      {
			sssd.text_fmt("UNPACK_CHECK_NO_REVISIT(%d,%s,",
				      decl->_loc._internal,decl->_ident);
			decl->_name->dump(sssd);
			sssd.text_fmt(",__visited%d,%d);\n",
				      check_visit_counter,index_check_visit++);
		      }

		    gen_unpack_decl(decl,"UNPACK_DECL",sssd,true);
		  }
		sssd.text("break;\n");
	      }
	    // ssud.text("default: // internal error... (cannot happen)\n");
	    // If not in mode several, then not finding a match is fatal...:
	    // ssud.text("case 0: NO_MATCH(); // will throw exception\n");
	    sd.text("}\n");
	  }
      }

      if (subevent)
	sd.text("return 0;\n");
      d.text("}\n");
      if (!subevent && !(flags & SS_FLAGS_SEVERAL))
	d.text("while (0);\n");
      if (abort_spurious_label)
	{
	  d.text_fmt("%s:;\n",abort_spurious_label);
	  free(abort_spurious_label);
	}
      if (!subevent && (flags & SS_FLAGS_OPTIONAL))
	sd.text_fmt("no_match_%d:;\n",no_match_counter);
    }
  if (type & UCT_PACKER)
    {
      d.text("{\n");
      dumper sd(d,2);

      struct_decl_list::const_iterator i;
      for (i = items->begin(); i != items->end(); ++i)
	{
	  struct_decl *decl = *i;

	  if (decl->is_event_opt())
	    continue;

	  const struct_header_named *named_header = find_decl(decl,subevent);

	  if (subevent)
	    {
	      /*
	      sd.text_fmt("PACK_SUBEVENT_DECL(%d,(",
			  decl->_loc._internal);
	      dump_match_args(decl->_loc,subevent_cond_params,decl->_args,sd);
	      sd.text("),");
	      decl->_name->dump(sd);
	      // dump_param_args(decl->_loc,named_header->_params,decl->_args,sd);
	      sd.text(");\n");
	      */
	    }
	  else
	    {
	      sd.text_fmt("PACK_DECL(%d,",
			  decl->_loc._internal);
	      sd.text(decl->_ident);
	      sd.text(",");
	      decl->_name->dump(sd);
	      dump_param_args(decl->_loc,named_header->_params,decl->_args,sd,true);
	      sd.text(");\n");
	    }
	}

      d.text("}\n");
    }
}

void struct_unpack_code::gen(const struct_header *header,
			     const struct_cond *cond,dumper &d,uint32 type,
			     match_end_info *mei,
			     bool last_subevent_item)
{
  const var_external *external;

  external = gen_external_header(cond->_expr,d,type);

  if (type & (UCT_UNPACK | UCT_MATCH | UCT_PACKER))
    {
      d.text("if (");

      if (external)
	d.text_fmt("%s()",external->_name);
      else
	cond->_expr->dump(d);

      d.text(")\n");
    }

  match_end_info *smei1 = NULL;
  if (mei)
    smei1 = new match_end_info(*mei);

  gen(header,cond->_items,d,type,smei1,last_subevent_item,false);
  if (cond->_items_else)
    {
      if (type & (UCT_UNPACK | UCT_MATCH | UCT_PACKER))
	{
	  d.nl();
	  d.text("else\n");
	}
      match_end_info *smei2 = NULL;
      if (mei)
	smei2 = new match_end_info(*mei);
      gen(header,cond->_items_else,d,type,smei2,last_subevent_item,false);

      // only in case both branches ended their matching, we end
      // the matching...

      if (mei)
	mei->_match_ended |= smei1->_match_ended && smei2->_match_ended;

      delete smei2;
    }

  delete smei1;
}

void struct_unpack_code::gen(const struct_member *member,dumper &d,uint32 type)
{
  if (type & (UCT_HEADER | UCT_MEMBER_ARG))
    {
      const char *name  = member->_name;
      const char *ident = member->_ident->_name;
      int length  = 0;
      int length2 = 0;

      const char *ref = (type & UCT_MEMBER_ARG) ? "&" : "";

      const var_index *vi = dynamic_cast<const var_index *>(member->_ident);

      if (vi)
	{
	  length  = vi->_index;
	  length2 = vi->_index2;
	}

      if ((member->_flags._flags & SM_FLAGS_IS_ARGUMENT) &&
	  !(type & UCT_MEMBER_ARG))
	{
	  // The member is to be an argument, so do not declare it in
	  // the header.

	  return;
	}

      if (length)
	{
	  const char *array_type = "raw_array";

	  if (member->_flags._flags & SM_FLAGS_ZERO_SUPPRESS)
	    {
	      if (member->_flags._flags & SM_FLAGS_LIST)
		array_type = "raw_list_zero_suppress";
	      else
		array_type = "raw_array_zero_suppress";
	    }
	  if (member->_flags._flags & SM_FLAGS_NO_INDEX)
	    {
	      if (member->_flags._flags & SM_FLAGS_LIST)
		array_type = "raw_list_ii_zero_suppress";
	      else
		ERROR("NO_INDEX must be with LIST");
	    }

	  if (member->_flags._flags & SM_FLAGS_MULTI)
	    {
	      const char *array_type = "raw_array_multi_zero_suppress";

	      assert(member->_flags._multi_size != -1);
	      d.text_fmt("%s<%s,%s,%d,%d>",
			 array_type,name,name,length,
			 member->_flags._multi_size);
	    }
	  else
	    d.text_fmt("%s<%s,%s,%d>",
		       array_type,name,name,length);

	  d.text_fmt(" %s%s",ref,ident);

	  if (length2 != -1)
	    d.text_fmt("[%d]",length2);
	}
      else
	{
	  if (member->_flags._flags & (SM_FLAGS_ZERO_SUPPRESS |
				       SM_FLAGS_NO_INDEX))
	    ERROR_LOC(member->_loc,
		      "Cannot zero-suppress/no-index non-array member.");
	  d.text_fmt("%s %s%s",name,ref,ident);
	}
      if (!(type & UCT_MEMBER_ARG))
	d.text(";\n");
      /*
      d.text();

      const char             *_name;
      const variable         *_ident;
      */
    }
}

void struct_unpack_code::gen(const struct_mark* mark,dumper &d,uint32 type)
{
  if (mark->_flags & STRUCT_MARK_FLAGS_GLOBAL)
    {
      if (type & UCT_HEADER)
	{
         // Using char* instead of void* such that it is easy
         // to make byte counts
         d.col0_text("#ifndef __PSDC__\n");
         d.text_fmt("char *%s;\n",mark->_name);
         d.col0_text("#endif//!__PSDC__\n");
       }
      if (type & UCT_UNPACK)
	{
         d.text_fmt("%s = __buffer._data;\n",mark->_name);
       }
      if (type & UCT_PACKER)
	{
         d.text_fmt("%s = __buffer._offset;\n",mark->_name);
       }
    }
  else
    {
      if (type & UCT_UNPACK)
	{
         d.text_fmt("void *__mark_%s = __buffer._data;\n",mark->_name);
       }
      if (type & UCT_PACKER)
	{
         d.text_fmt("void *__mark_%s = __buffer._offset;\n",mark->_name);
       }
    }
}

void struct_unpack_code::gen(const struct_check_count* check,dumper &d,uint32 type)
{
  if (type & UCT_UNPACK)
    {
      check->_check.check(d,"CHECK",NULL,check->_var,NULL);
    }
}

void struct_unpack_code::gen(const struct_match_end* match_end,dumper &d,uint32 type,
			     match_end_info *mei)
{
  if (type & UCT_MATCH)
    {
      mei->set_end();
      d.text("return true;\n");
    }
}


void struct_unpack_code::gen(const event_definition *evt,
			     dumper &d,uint32 type)
{
  COMMENT_DUMP_ORIG(d,evt);

  const char *event_class =
    evt->_opts & EVENT_OPTS_STICKY ? "unpack_sticky_event" : "unpack_event";

  if (type & UCT_HEADER)
    {
      d.text_fmt("class %s",event_class);
      d.text_fmt(" : public %s_base",event_class);
      d.text("\n");
      d.text("{\n");
      d.text("public:\n");
    }

  if (type & UCT_UNPACK)
    {
      d.text("template<typename __data_src_t>\n");
      d.text_fmt("int %s",event_class);
      d.text("::__unpack_subevent(subevent_header *__header,__data_src_t &__buffer");
      d.text(")\n");
    }

  if (type & UCT_REVOKE)
    {
      d.text_fmt("int %s",event_class);
      d.text("::__revoke_subevent(subevent_header *__header)\n");
    }

  if (!subevent_cond_params)
    {
      subevent_cond_params = new param_list;

      // LMD:
      subevent_cond_params->push_back(new param(find_str_identifiers("type")));
      subevent_cond_params->push_back(new param(find_str_identifiers("subtype")));
      subevent_cond_params->push_back(new param(find_str_identifiers("control")));
      subevent_cond_params->push_back(new param(find_str_identifiers("subcrate")));
      subevent_cond_params->push_back(new param(find_str_identifiers("procid")));

      // HLD:
      subevent_cond_params->push_back(new param(find_str_identifiers("id")));

      // RIDF:
      subevent_cond_params->push_back(new param(find_str_identifiers("rev")));
      subevent_cond_params->push_back(new param(find_str_identifiers("dev")));
      subevent_cond_params->push_back(new param(find_str_identifiers("fp")));
      subevent_cond_params->push_back(new param(find_str_identifiers("det")));
      subevent_cond_params->push_back(new param(find_str_identifiers("mod")));
    }

  gen_match(evt->_loc,evt->_items,d,type,NULL,true,false,
	    !(evt->_opts & EVENT_OPTS_INTENTIONALLY_EMPTY),
	    evt->_opts & EVENT_OPTS_STICKY,0);

  if (type & UCT_HEADER)
    {
      d.nl();
      d.text("public:\n");
      d.col0_text("#ifndef __PSDC__\n");
      d.text("template<typename __data_src_t>\n");
      d.text("  int __unpack_subevent(subevent_header *__header,__data_src_t &__buffer);\n");
      d.text("  int __revoke_subevent(subevent_header *__header);\n");
      d.text("  // void __clean_event();\n");
      d.nl();
      d.text_fmt("  STRUCT_FCNS_DECL(%s);\n",event_class);
      d.col0_text("#endif//!__PSDC__\n");
      d.text("};\n");
    }

  if (type & UCT_UNPACK)
    {
      d.text_fmt("FORCE_IMPL_DATA_SRC_FCN_HDR(int,%s::__unpack_subevent);\n",event_class);
      //d.text("::__unpack_subevent(subevent_header *__header,__data_src_t &__buffer");
      //d.text(");\n");
    }
}

void struct_unpack_code::gen_params(const param_list *params,
				    dumper &d,
				    uint32 type,
				    bool dump_member_args,
				    bool dump_default_args)
{
  const char *default_type = "uint32";

  dumper sd(d);

  param_list::const_iterator pl;

  for (pl = params->begin(); pl != params->end(); ++pl)
    {
      param *p = *pl;

      if (p->_member)
	{
	  if (dump_member_args)
	    {
	      sd.text(",",true);

	      gen(p->_member,sd,UCT_MEMBER_ARG);
	    }
	}
      else
	{
	  sd.text(",",true);
	  sd.text_fmt("%s ",default_type);
	  sd.text(p->_name);
	  if (p->_def && dump_default_args)
	    {
	      sd.text("=");
	      p->_def->dump(sd);
	    }
	}
    }
}

void gen_subevent_names(const event_definition *evt,
			dumper &d)
{
  struct_decl_list::const_iterator i;

  assert(evt->_items);

  for (i = evt->_items->begin(); i < evt->_items->end(); ++i)
    {
      // We now need to call the correct function per the type

      const struct_decl*    item = *i;

      if (item->_name)
	{
	  d.text_fmt("{ \"");
	  item->_name->dump(d);
	  d.text_fmt("\", \"");
	  //item->_name.dump();

	  argument_list::const_iterator ai;
	  bool had_item = false;

	  for (ai = item->_args->begin(); ai != item->_args->end(); ++ai)
	    {
	      argument *a = *ai;
	      {
		d.text_fmt("%s%s=",had_item ? ":" : "",a->_name);
		a->_var->dump(d);
		had_item = true;
	      }
	    }

	  d.text("\" },\n");
	}
    }
}
