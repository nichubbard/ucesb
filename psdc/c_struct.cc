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

#include "c_struct.hh"
#include "dump_list.hh"
#include "parse_error.hh"

#include <stack>

def_node_list *all_definitions;

////////////////////////////////////////////////////////////////////////

//void c_bitfield_item::dump(dumper &d,bool /*recursive*/) const
//{
//  d.text_fmt("%s %s : %d;",_type,_ident,_bits);
//}

void c_arg_named::dump(dumper &d,bool /*recursive*/) const
{
  d.text_fmt("%s%s%s",
	     _toggle ? "TOGGLE(" : "",
	     _name,
	     _toggle ? ")" : "");
  if (_array_ind)
    for (unsigned int i = 0; i < _array_ind->size(); i++)
      d.text_fmt("[%d]",(*_array_ind)[i]);
}

void c_arg_const::dump(dumper &d,bool /*recursive*/) const
{
  d.text_fmt("%d",_value);
}

void c_typename::dump(dumper &d,bool /*recursive*/) const
{
  d.text_fmt("%s%s%s",
	     _toggle ? "TOGGLE(" : "",
	     _name,
	     _toggle ? ")" : "");
}

void c_typename_template::dump(dumper &d,bool recursive) const
{
  c_typename::dump(d, recursive);
  dump_list_paren(_args,d,"<>");
}

void c_struct_member::dump(dumper &d,bool /*recursive*/) const
{
  if (_multi_flag)
    d.text("__MULTI__ ");
  _type->dump(d);
  d.text_fmt(" %s",_ident);
  if (_array_ind)
    for (unsigned int i = 0; i < _array_ind->size(); i++)
      d.text_fmt("[%d]",(*_array_ind)[i]);
  if (_unit)
    d.text_fmt(" UNIT(\"%s\")",_unit);
  if (_bitfield)
    d.text_fmt(" : %d",_bitfield);
  d.text(";");
}

void c_struct_def::dump(dumper &d,bool recursive) const
{
  switch (_struct_type)
    {
    case C_STR_STRUCT: d.text("struct "); break;
    case C_STR_UNION:  d.text("union "); break;
    case C_STR_CLASS:  d.text("class "); break;
    };

  if (_type)
    _type->dump(d);
  if (_base_list)
    {
      d.text(" : ");
      for (unsigned int i = 0; i < _base_list->size(); i++)
	d.text_fmt("%spublic %s",i ? ", " : "",(*_base_list)[i]);
    }
  d.nl();
  if (recursive)
    dump_list_braces(_items,d);
  d.text_fmt(" %s",_ident ? _ident : "");
  if (_array_ind)
    for (unsigned int i = 0; i < _array_ind->size(); i++)
      d.text_fmt("[%d]",(*_array_ind)[i]);
  d.text(";");
}

////////////////////////////////////////////////////////////////////////

void c_typename::mirror_struct(dumper &d,bool full_template) const
{
  d.text_fmt("STRUCT_MIRROR_TYPE%s(%s) ",
	     _toggle ? "_TOGGLE" : "",
	     _name);
  if (full_template)
    d.text_fmt("STRUCT_MIRROR_TYPE_TEMPLATE_FULL");
}

void dump_mirror_template_arg(const c_arg *arg,dumper &d)
{
  const c_arg_named *named = dynamic_cast<const c_arg_named *>(arg);

  if (named)
    {
      if (!named->_array_ind)
	d.text_fmt("STRUCT_MIRROR_TEMPLATE_ARG%s(%s)",
		   named->_toggle ? "_TOGGLE" : "",
		   named->_name);
      else
	{
	  d.text_fmt("STRUCT_MIRROR_TEMPLATE_ARG%s_N(%s,",
		     named->_toggle ? "_TOGGLE" : "",
		     named->_name);
	  for (unsigned int i = 0; i < named->_array_ind->size(); i++)
	    d.text_fmt("[%d]",(*named->_array_ind)[i]);
	  d.text(")");
	}
    }
  else
    arg->dump(d);
}

void c_typename_template::mirror_struct(dumper &d,bool /*full_template*/) const
{
  c_typename::mirror_struct(d,false);
  // Cannot use, since we need to figure out what args are types...
  // dump_list_paren(_args,d,"<>");

  d.text("< STRUCT_MIRROR_TYPE_TEMPLATE "); // '('

  const c_arg_list *v = _args;

  if (v)
    {
      dumper sd(d);

      c_arg_list::const_iterator i;

      i = v->begin();

      if (i != v->end())
	{
	  dump_mirror_template_arg(*i,sd);
	  ++i;

	  for (; i != v->end(); ++i)
	    {
	      sd.text(",");
	      dump_mirror_template_arg(*i,sd);
	    }
	}
    }
  d.text(">");

}

void c_struct_member::mirror_struct(dumper &d) const
{
  d.text_fmt("STRUCT_MIRROR_ITEM_CTRL(%s);\n",_ident);

  unsigned int max_index = _array_ind ? (unsigned int) _array_ind->size() : 0;

  for (unsigned int end_index = 0; end_index < max_index; end_index++)
    {
      d.text_fmt("STRUCT_MIRROR_ITEM_CTRL_ARRAY(%s",_ident);
      for (unsigned int i = 0; i <= end_index; i++)
	d.text_fmt("__i%d",i);
      d.text(",");
      for (unsigned int i = 0; i+1 <= end_index; i++)
	d.text_fmt("[%d]",(*_array_ind)[i]);
      d.text_fmt(",%d);\n",(*_array_ind)[end_index]);
    }

  _type->mirror_struct(d);
  //d.text_fmt("STRUCT_MIRROR_TYPE(%s)",_type->_name);
  //dump_list_paren(_type->_args,d,"<>");
  d.text_fmt(" STRUCT_MIRROR_NAME(%s",_ident);
  if (_array_ind)
    for (unsigned int i = 0; i < _array_ind->size(); i++)
      d.text_fmt("[%d]",(*_array_ind)[i]);
  d.text(");\n");
}

void c_struct_def::mirror_struct_decl(dumper &d) const
{
  if (_type)
    {
      d.text_fmt("STRUCT_MIRROR_TEMPLATE\n");
      d.text_fmt("struct STRUCT_MIRROR_STRUCT(%s);\n",_type->_name);
    }
};

void c_struct_def::mirror_struct(dumper &d) const
{
  if(_type)
    {
      d.text_fmt("#ifndef USER_DEF_%s\n",_type->_name);
      d.text_fmt("STRUCT_MIRROR_TEMPLATE\n");
    }
  switch (_struct_type)
    {
    case C_STR_STRUCT: d.text("struct "); break;
    case C_STR_UNION:
      d.col0_text("#if STRUCT_ONLY_LAST_UNION_MEMBER\n");
      d.text("struct \n");
      d.col0_text("#else\n");
      d.text("union \n");
      d.col0_text("#endif\n");
      break;
    case C_STR_CLASS:  d.text("class "); break;
    };

  if (_type)
    d.text_fmt("STRUCT_MIRROR_STRUCT(%s)",_type->_name);
  if (_base_list)
    {
      d.text(" : ");
      for (unsigned int i = 0; i < _base_list->size(); i++)
	d.text_fmt("%spublic STRUCT_MIRROR_BASE(%s)",i ? ", " : "",(*_base_list)[i]);
    }
  d.nl();
  d.text("{\n");
  d.text("public:\n");
  dumper sd(d,2);

  if (_base_list)
    {
      for (unsigned int i = 0; i < _base_list->size(); i++)
	sd.text_fmt("STRUCT_MIRROR_ITEM_CTRL_BASE(%s);\n",(*_base_list)[i]);
    }

  if (_struct_type == C_STR_UNION)
    d.col0_text("#if !STRUCT_ONLY_LAST_UNION_MEMBER\n");

  for (c_struct_item_list::const_iterator i = _items->begin();
       i != _items->end(); )
    {
      const c_struct_item* item = *i;

      ++i;

      if (_struct_type == C_STR_UNION &&
	  i == _items->end())
	d.col0_text("#endif// !STRUCT_ONLY_LAST_UNION_MEMBER\n");

      //dumper dc(sd,0,true);
      //item->dump(dc);
      //sd.nl();
      item->mirror_struct(sd);
    }

  if (_type)
    sd.text_fmt("STRUCT_MIRROR_FCNS_DECL(%s);\n",_type->_name);

  d.text("}");

  if (_ident)
    {
      d.text_fmt(" STRUCT_MIRROR_NAME(%s",_ident);
      if (_array_ind)
	for (unsigned int i = 0; i < _array_ind->size(); i++)
	  d.text_fmt("[%d]",(*_array_ind)[i]);
      d.text(")");
    }
  d.text(";\n");
  if(_type)
    d.text_fmt("#endif//USER_DEF_%s\n",_type->_name);
}

//void c_struct_bitfield::dump(dumper &d,bool /*recursive*/) const
//{
//  d.text("struct ");
//  if (_type)
//    _type->dump(d);
//  d.nl();
//  dump_list_braces(_items,d);
//  d.text_fmt("%s;",_ident ? _ident : "");
//}

////////////////////////////////////////////////////////////////////////

bool c_typename::primitive_type() const
{
  if (strcmp(_name,"uint8") == 0 ||
      strcmp(_name,"uint16") == 0 ||
      strcmp(_name,"uint32") == 0 ||
      strcmp(_name,"uint64") == 0 ||
      strcmp(_name,"uint8_t") == 0 ||
      strcmp(_name,"uint16_t") == 0 ||
      strcmp(_name,"uint32_t") == 0 ||
      strcmp(_name,"uint64_t") == 0 ||
      strcmp(_name,"float") == 0 ||
      strcmp(_name,"double") == 0)
    return true;
  return false;
}

bool c_typename_template::primitive_type() const
{
  return false;
}

void append_string(char **dest,const char *append)
{
  size_t len = (*dest ? strlen(*dest) : 0) + strlen(append) + 1;

  *dest = (char *) realloc(*dest,len);

  strcat(*dest,append);
}

dumper *c_struct_item::function_call_array_loop_open(std::stack<dumper *> &dumpers,
						     dumper *ld,
						     int array_i_offset,
						     char **sub_array_index) const
{
  if (_array_ind)
    for (unsigned int i = 0; i < _array_ind->size(); i++)
      {
	/*
	ld->text_fmt("for (int __i%d = 0; __i%d < %d; ++__i%d)\n",
		     i+array_i_offset,
		     i+array_i_offset,
		     (*_array_ind)[i],
		     i+array_i_offset);
	*/
	ld->text_fmt("FCNCALL_FOR(__i%d,%d)\n",
		     (int) i+array_i_offset,
		     (*_array_ind)[i]);
	ld->text("{\n");
	dumpers.push(ld);
	ld = new dumper(*ld,2);
	ld->text_fmt("FCNCALL_SUBINDEX(__i%d);\n",
		     (int) i+array_i_offset);
	char sub_index[32];
	sprintf(sub_index,"[__i%d]",(int) i+array_i_offset);
	append_string(sub_array_index,sub_index);
    }
  return ld;
}

void c_struct_item::function_call_array_loop_close(std::stack<dumper *> &dumpers,
						   dumper *ld,
						   int array_i_offset) const
{
  while (!dumpers.empty())
    {
      ld->text_fmt("FCNCALL_SUBINDEX_END(__i%d);\n",
		   ((int) dumpers.size())-1+array_i_offset);
      delete ld;
      ld = dumpers.top();
      dumpers.pop();
      ld->text("}\n");
    }
}

void c_struct_member::function_call(dumper &d,
				    int array_i_offset,
				    const char *array_prefix) const
{
  /*
  _type->dump(d);
  d.text_fmt(" %s",_ident);
  if (_array_ind)
    for (int i = 0; i < _array_ind->size(); i++)
      d.text_fmt("[%d]",(*_array_ind)[i]);
  d.text(";");
  */

  std::stack<dumper *> dumpers;

  char *sub_array_prefix = NULL;
  char *sub_array_index = NULL;

  sub_array_prefix = strdup(array_prefix);
  sub_array_index  = strdup(_ident);

  d.text("{\n");
  d.text_fmt("FCNCALL_SUBNAME(\"%s\");\n",_ident);
  if (_unit)
    d.text_fmt("FCNCALL_UNIT(\"%s\");\n",_unit);

  dumper *ld = function_call_array_loop_open(dumpers,&d,array_i_offset,
					     &sub_array_index);

  ld->text("{ ");
  if (_array_ind)
    {
      ld->text_fmt("FCNCALL_CALL_CTRL_WRAP_ARRAY(%s",_ident);
      for (unsigned int i = 0; i < _array_ind->size(); i++)
	ld->text_fmt("__i%d",i);
      ld->text(",");
      for (unsigned int i = 0; i+1 < _array_ind->size(); i++)
	ld->text_fmt("[__i%d]",i);
      ld->text_fmt(",__i%d,",(int) _array_ind->size()-1);
    }
  else
    ld->text_fmt("FCNCALL_CALL_CTRL_WRAP(%s%s,",sub_array_prefix,sub_array_index);

  if (_type->primitive_type())
    {
      // ld->text_fmt("printf (\"call_type: %s\\n\");",_ident);
      if (_bitfield)
	ld->text("FCNCALL_CALL_TYPE/*_BITFIELD*/(");
      else
	ld->text("FCNCALL_CALL_TYPE(");
      _type->dump(*ld);
      ld->text_fmt(",%s%s)",sub_array_prefix,sub_array_index);
    }
  else
    {
      // ld->text_fmt("printf (\"call: %s\\n\");",_ident);

      ld->text_fmt("%s%s%s%s.FCNCALL_CALL",
		   _multi_flag ? "FCNCALL_MULTI_MEMBER(" : "",
		   sub_array_prefix,sub_array_index,
		   _multi_flag ? ")" : "");
      ld->text_fmt("(%s%s%s%s)",
		   _multi_flag ? "FCNCALL_MULTI_ARG(" : "",
		   sub_array_prefix,sub_array_index,
		   _multi_flag ? ")" : "");
    }
  ld->text("); }\n");

  free(sub_array_prefix);
  free(sub_array_index);

  function_call_array_loop_close(dumpers,ld,array_i_offset);

  d.text("FCNCALL_SUBNAME_END;\n");
  d.text("}\n");
}

void c_struct_def::function_call(dumper &d,
				 int array_i_offset,
				 const char *array_prefix) const
{
  std::stack<dumper *> dumpers;

  char *sub_array_prefix = NULL;

  sub_array_prefix = strdup(array_prefix);

  if (_ident)
    {
      append_string(&sub_array_prefix,_ident);
      d.text("{\n");
      d.text_fmt("FCNCALL_SUBNAME(\"%s\");\n",_ident);
      assert(_unit == NULL);
    }

  dumper *ld = function_call_array_loop_open(dumpers,&d,array_i_offset,
					     &sub_array_prefix);

  if (_ident)
    {
      append_string(&sub_array_prefix,".");
    }

  if (_struct_type == C_STR_UNION)
    d.col0_text("#if !STRUCT_ONLY_LAST_UNION_MEMBER\n");

  for (c_struct_item_list::const_iterator i = _items->begin();
       i != _items->end(); )
    {
      const c_struct_item* item = *i;

      ++i;

      if (_struct_type == C_STR_UNION &&
	  i == _items->end())
	d.col0_text("#endif// !STRUCT_ONLY_LAST_UNION_MEMBER\n");

      dumper dc(*ld,0,true);
      item->dump(dc);
      ld->nl();
      item->function_call(*ld,array_i_offset + (_array_ind ? (int) _array_ind->size() : 0),sub_array_prefix);
    }

  free(sub_array_prefix);

  function_call_array_loop_close(dumpers,ld,array_i_offset);

  if (_ident)
    {
      d.text("FCNCALL_SUBNAME_END;\n");
      d.text("}\n");
    }
}

//void c_struct_bitfield::function_call(dumper &d) const
//{
//
//}

void c_struct_def::function_call_per_member(dumper &d) const
{
  if (!_type)
    ERROR_LOC(_loc,"Cannot output function call for struct without name.");

  d.text_fmt("#ifndef USER_DEF_%s\n",_type->_name);
  d.text("FCNCALL_TEMPLATE\n");
  d.text("FCNCALL_RET_TYPE FCNCALL_CLASS_NAME(");
  _type->dump(d);
  d.text_fmt(")::FCNCALL_NAME(%s)\n",_type->_name);
  d.text("{\n");
  dumper sd(d,2);
  sd.text("FCNCALL_INIT;\n");
  // First make recursive calls for all parents...
  if (_base_list)
    {
      for (unsigned int i = 0; i < _base_list->size(); i++)
	sd.text_fmt("FCNCALL_CALL_CTRL_WRAP(%s,FCNCALL_CLASS_NAME(%s)::FCNCALL_CALL_BASE());\n",
		    (*_base_list)[i],(*_base_list)[i]);
    }

  function_call(sd,0,"");

  sd.text("FCNCALL_RET;\n");
  d.text("}\n");
  d.text_fmt("#endif//USER_DEF_%s\n",_type->_name);
}

////////////////////////////////////////////////////////////////////////

named_c_struct_def_map  all_named_c_struct;
named_c_struct_def_vect all_named_c_struct_vect;

void map_definitions()
{
  if (!all_definitions)
    return;

  def_node_list::iterator i;

  for (i = all_definitions->begin(); i != all_definitions->end(); ++i)
    {
      def_node* node = *i;

      c_struct_def *c_struct =
	dynamic_cast<c_struct_def *>(node);

      if (c_struct)
	{
	  const char *name = NULL;
	  if (c_struct->_type)
	    name = c_struct->_type->_name;
	  if (!name)
	    ERROR_LOC(c_struct->_loc,
		      "Unnamed top-level structure.\n");

	  if (all_named_c_struct.find(name) !=
	      all_named_c_struct.end())
	    ERROR_LOC_PREV(c_struct->_loc,
			   all_named_c_struct.find(name)->second->_loc,
			   "Several structures with name: %s\n",
			   name);

	  all_named_c_struct.insert(named_c_struct_def_map::value_type(name,
								       c_struct));
	  all_named_c_struct_vect.push_back(c_struct);
	}
    }
}

////////////////////////////////////////////////////////////////////////

void dump_definitions()
{
  print_header("INPUT_DEFINITION","All specifications as seen by the parser.");
  printf ("/**********************************************************\n"
          " * Dump of all structures:\n"
          " */\n\n");

  dumper_dest_file d_dest(stdout);
  dumper d(&d_dest);

  {
    named_c_struct_def_vect::iterator i;

    for (i = all_named_c_struct_vect.begin(); i != all_named_c_struct_vect.end(); ++i)
      {
	(*i)->dump(d);
	d.nl();
      }
  }

  printf ("/**********************************************************/\n");
  print_footer("INPUT_DEFINITION");
}

void fcn_call_per_member()
{
  print_header("FUNCTION_CALL_PER_MEMBER","Recursive function calls per member.");

  dumper_dest_file d_dest(stdout);
  dumper d(&d_dest);

  d.text("#include \"gen/default_fcncall_define.hh\"\n\n");

  {
    named_c_struct_def_vect::iterator i;

    for (i = all_named_c_struct_vect.begin(); i != all_named_c_struct_vect.end(); ++i)
      {
	(*i)->function_call_per_member(d);
	d.nl();
      }
  }

  d.text("#include \"gen/default_fcncall_undef.hh\"\n\n");

  print_footer("FUNCTION_CALL_PER_MEMBER");
}

void mirror_struct()
{
  print_header("MIRROR_STRUCT","Mirror (1 to 1) structure.");

  dumper_dest_file d_dest(stdout);
  dumper d(&d_dest);

  d.text("#include \"gen/default_mirror_define.hh\"\n\n");

  {
    named_c_struct_def_vect::iterator i;

    for (i = all_named_c_struct_vect.begin(); i != all_named_c_struct_vect.end(); ++i)
      {
	(*i)->mirror_struct(d);
	d.nl();
      }
  }

  d.text("#include \"gen/default_mirror_undef.hh\"\n\n");

  print_footer("MIRROR_STRUCT");
}

void mirror_struct_decl()
{
  print_header("MIRROR_DECL_STRUCT","Mirror structure names.");

  dumper_dest_file d_dest(stdout);
  dumper d(&d_dest);

  d.text("#include \"gen/default_mirror_define.hh\"\n\n");

  {
    named_c_struct_def_vect::iterator i;

    for (i = all_named_c_struct_vect.begin(); i != all_named_c_struct_vect.end(); ++i)
      {
	(*i)->mirror_struct_decl(d);
	d.nl();
      }
  }

  d.text("#include \"gen/default_mirror_undef.hh\"\n\n");

  print_footer("MIRROR_DECL_STRUCT");
}

////////////////////////////////////////////////////////////////////////
