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
#include "dump_list.hh"
#include "parse_error.hh"


void struct_data::check_bits() const
{
  if (_bits)
    {
      bits_spec_list::const_reverse_iterator ri;
      ri = _bits->rbegin();
      bits_spec *b = *ri;
      if (b->_max >= _size)
	ERROR_LOC(_loc,"Bits for '%s' outside (%d) type size (%d).",
		  b->_name ? b->_name : "unnamed",b->_max,_size);
    }
}


void struct_data::dump(dumper &d,bool /*recursive*/) const
{
  if (_flags & SD_FLAGS_OPTIONAL)
    d.text("optional ");
  if (_flags & SD_FLAGS_SEVERAL)
    d.text("several ");
  switch (_size)
    {
    case 64: d.text("UINT64 "); break;
    case 32: d.text("UINT32 "); break;
    case 16: d.text("UINT16 "); break;
    case 8:  d.text("UINT8 "); break;
    }
  d.text(_ident);
  if (_flags & SD_FLAGS_NOENCODE)
    d.text(" NOENCODE");
  if (_bits || _encode)
    {
      d.nl();
      d.text("{\n");
      dump_list_braces(_bits,d,false,false); // leave the brace open for encode
      dump_list_braces(_encode,d,false,false);
      d.text("}");
    }
  else
    d.text(";");
}

void struct_decl::dump(dumper &d,bool /*recursive*/) const
{
  if (_opts & EVENT_OPTS_IGNORE_UNKNOWN_SUBEVENT)
    {
      d.text("ignore_unknown_subevent;");
      return;
    }

  if (_opts & STRUCT_DECL_OPTS_MULTI)
    d.text("multi ");
  if (_opts & STRUCT_DECL_OPTS_EXTERNAL)
    d.text("external ");
  if (_opts & STRUCT_DECL_OPTS_REVISIT)
    d.text("revisit ");
  if (_opts & STRUCT_DECL_OPTS_NO_REVISIT)
    d.text("norevisit ");
  _name->dump(d);
  d.text(" = ");
  d.text(_ident);
  dump_list_paren(_args,d,"()");
  d.text(";");
}

void struct_encode::dump(dumper &d,bool /*recursive*/) const
{
  _encode->dump(d);
  d.nl();
}

void struct_member::dump(dumper &d,bool /*recursive*/) const
{
  d.text_fmt("MEMBER(%s ",_name);
  _ident->dump(d);

  int flags_left = _flags._flags;

  if ((flags_left & SM_FLAGS_ZERO_SUPPRESS_LIST) ==
      SM_FLAGS_ZERO_SUPPRESS_LIST)
    {
      d.text(" ZERO_SUPPRESS_LIST");
      flags_left &= ~(SM_FLAGS_ZERO_SUPPRESS_LIST);
    }
  if ((flags_left & SM_FLAGS_ZERO_SUPPRESS_MULTI) ==
      SM_FLAGS_ZERO_SUPPRESS_MULTI)
    {
      assert(_flags._multi_size != -1);
      d.text_fmt(" ZERO_SUPPRESS_MULTI(%d)",_flags._multi_size);
      flags_left &= ~(SM_FLAGS_ZERO_SUPPRESS_MULTI);
    }
  if ((flags_left & SM_FLAGS_NO_INDEX_LIST) == SM_FLAGS_NO_INDEX_LIST)
    {
      d.text(" NO_INDEX_LIST");
      flags_left &= ~(SM_FLAGS_NO_INDEX_LIST);
    }
  if (flags_left & SM_FLAGS_ZERO_SUPPRESS)
    d.text(" ZERO_SUPPRESS");
  if (flags_left & SM_FLAGS_NO_INDEX)
    d.text(" NO_INDEX");
  if (flags_left & SM_FLAGS_LIST)
    d.text(" LIST");
  d.text(");");
}

void struct_mark::dump(dumper &d,bool /*recursive*/) const
{
  if (_flags & STRUCT_MARK_FLAGS_GLOBAL)
    d.text_fmt("MARK(%s);",_name);
  else
    d.text_fmt("MARK_COUNT(%s);",_name);
}

void struct_check_count::dump(dumper &d,bool /*recursive*/) const
{
  _check.dump(d,_var);
}

void struct_match_end::dump(dumper &d,bool /*recursive*/) const
{
  d.text_fmt("MATCH_END;");
}

void struct_list::dump(dumper &d,bool recursive) const
{
  d.text("list(");
  _min->dump(d);
  d.text("<=");
  _index->dump(d);
  d.text("<");
  _max->dump(d);
  d.text(")");
  d.nl();
  if (recursive)
    dump_list_braces(_items,d);
}

void struct_select::dump(dumper &d,bool recursive) const
{
  d.text("select");
  if (_flags & SS_FLAGS_OPTIONAL)
    d.text(" optional");
  if (_flags & SS_FLAGS_SEVERAL)
    d.text(" several");
  d.nl();
  if (recursive)
    dump_list_braces(_items,d);
}
#if 0
void struct_several::dump(dumper &d,bool recursive) const
{
  d.text("several ");
  if (recursive)
    _item->dump(d,recursive);
}
#endif
void struct_cond::dump(dumper &d,bool recursive) const
{
  d.text("if(");
  _expr->dump(d);
  d.text(")");
  d.nl();
  if (recursive)
    dump_list_braces(_items,d);
  if (_items_else)
    {
      d.nl();
      d.text("else");
      d.nl();
      if (recursive)
	dump_list_braces(_items_else,d);
    }
}




void struct_header_named::dump(dumper &d) const
{
  d.text(_name);
  dump_list_paren(_params,d,"()");
  /*
    d.text("(");
    dumper pd(d);
    dump_list(_params,pd,",");
    d.text(")");
  */
  d.nl();
}

void struct_header_subevent::dump(dumper &d) const
{
  d.text("SUBEVENT(");
  d.text(_name);
  d.text(")");
  d.nl();
}

void struct_definition::dump(dumper &d,bool recursive) const
{
  if (_opts & STRUCT_DEF_OPTS_EXTERNAL)
    d.text("external ");
  _header->dump(d);
  if (_opts & STRUCT_DEF_OPTS_EXTERNAL)
    d.text(";\n");
  else
    if (recursive)
      {
	dump_list_braces(_body,d);
	d.nl();
      }
  /*
  d.text("{");
  d.nl();
  dumper id(d,2);
  dump_list(_body,id,"\n");
  d.text("}");
  */
}


void event_definition::dump(dumper &d,bool recursive) const
{
  if (_opts & EVENT_OPTS_STICKY)
    d.text("STICKY_EVENT\n");
  else
    d.text("EVENT\n");
  if (recursive)
    {
      dump_list_braces(_items,d);
      d.nl();
    }
}


void struct_decl::check_valid_opts(int valid_opts)
{
  int invalid_opts = _opts & ~valid_opts;

  if (invalid_opts)
    {
      const char *opt_name = "";

      if (invalid_opts & STRUCT_DECL_OPTS_MULTI)
	opt_name = "multi";
      else if (invalid_opts & STRUCT_DECL_OPTS_EXTERNAL)
	opt_name = "external";
      else if (invalid_opts & STRUCT_DECL_OPTS_REVISIT)
	opt_name = "revisit";
      else if (invalid_opts & STRUCT_DECL_OPTS_NO_REVISIT)
	opt_name = "norevisit";
      else if (invalid_opts & EVENT_OPTS_IGNORE_UNKNOWN_SUBEVENT)
	opt_name = "ignore_unknown_subevent";

      ERROR_LOC(_loc,"Option '%s' not allowed in this context opt=%x validopt=%x.",
		opt_name,_opts,valid_opts);
    }
}

int struct_decl::is_event_opt()
{
  return _opts & EVENT_OPTS_IGNORE_UNKNOWN_SUBEVENT;
}


void check_valid_opts(struct_decl_list *list,int valid_opts)
{
  struct_decl_list::iterator i;

  for (i = list->begin(); i != list->end(); ++i)
    (*i)->check_valid_opts(valid_opts);
}




// Find any parameters of this structure (unpack function) that have
// the same name as a member.  If it has the same name as a member,
// then the member is in reality coming from the parent structure and
// sent along as a reference.

typedef std::map<const char *,struct_member*> member_map;

void struct_definition::find_member_args()
{
  const struct_header_named *named_header =
    dynamic_cast<const struct_header_named *>(_header);

  if (!named_header || !_body)
    return; // no parameter list

  const param_list *params = named_header->_params;

  assert(params);

  member_map members;

  struct_item_list::const_iterator i;

  for (i = _body->begin(); i < _body->end(); ++i)
    {
      struct_item*    item = *i;
      struct_member*  member = dynamic_cast<struct_member* >(item);

      if (member)
	{
	  const char *ident = member->_ident->_name;

	  // printf ("member: %s [%08x]\n",ident,(int) ident);

	  member_map::iterator mi;

	  mi = members.find(ident);

	  if (mi != members.end())
	    {
	      WARNING_LOC(member->_loc,"Cannot have two members with same name (%s).",ident);
	      ERROR_LOC(mi->second->_loc,"Previous declared here.");
	    }

	  members.insert(member_map::value_type(ident,member));
	}
    }

  param_list::const_iterator pl;

  for (pl = params->begin(); pl != params->end(); ++pl)
    {
      param *p = *pl;

      // printf ("param: %s [%08x]\n",p->_name,(int) p->_name);

      member_map::iterator mi;

      mi = members.find(p->_name);

      if (mi != members.end())
	{
	  // printf ("Found!!!\n");

	  if (p->_def)
	    ERROR_LOC(_loc,
		      "Parameter (%s) which is a member reference cannot have a default value.",
		      p->_name);

	  p->_member = mi->second;
	  mi->second->_flags._flags |= SM_FLAGS_IS_ARGUMENT;
	}
    }
}

