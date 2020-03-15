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

#ifndef __UNPACK_CODE_HH__
#define __UNPACK_CODE_HH__

#include "dumper.hh"
#include "structure.hh"

#include <map>

#define UCT_HEADER   0x01
#define UCT_UNPACK   0x02
#define UCT_MATCH    0x04
#define UCT_PACKER   0x08
#define UCT_REVOKE   0x10
#define UCT_MEMBER_ARG 0x20 // dirty trick

struct indexed_type_ind
{
public:
  indexed_type_ind(const char *type,int max_items,int max_items2,int opts);

public:
  const char *_type;
  int         _max_items;
  int         _max_items2;

  int         _opts;
};

typedef std::map<const char*/*name*/,indexed_type_ind> indexed_decl_map;

struct match_info
{
  int         _size;

  uint32      _mask;
  uint32      _value;

  const struct_decl *_decl;

  int         _index; // TODO: to go
};

struct match_end_info
{
  bool        _has_explicit_end;
  bool        _match_ended;

public:
  bool can_end(bool had_item);
  void set_end();
};

class struct_unpack_code
{
public:
  struct_unpack_code()
  {
    _done = false;
  }



public:
  dumper_dest_memory _headers;
  dumper_dest_memory _code_unpack;
  dumper_dest_memory _code_match;
  dumper_dest_memory _code_packer;
  dumper_dest_memory _code_revoke;

public:
  bool _done;

public:
  void gen(const struct_definition *str,dumper &d,uint32 type,
	   match_end_info *mei);
  void gen(const struct_item_list *list,dumper &d,uint32 type,
	   match_end_info *mei,bool last_subevent_item,bool is_function);
  void gen(const file_line &loc,const struct_decl_list *list,dumper &d,uint32 type);

  void gen(indexed_decl_map &indexed_decl,
	   const struct_item   *item,   dumper &d,uint32 type,
	   match_end_info *mei,bool last_subevent_item);
  void gen(const struct_data   *data,   dumper &d,uint32 type,
	   match_end_info *mei);
  void gen(indexed_decl_map &indexed_decl,
	   const struct_decl   *decl,   dumper &d,uint32 type,
	   match_end_info *mei);
  void gen(const struct_list   *list,   dumper &d,uint32 type,
	   match_end_info *mei,bool last_subevent_item);
  void gen(const struct_select *select, dumper &d,uint32 type,
	   match_end_info *mei,bool last_subevent_item);
  void gen(const struct_cond   *cond,   dumper &d,uint32 type,
	   match_end_info *mei,bool last_subevent_item);
  void gen(const struct_member *member,  dumper &d,uint32 type);
  void gen(const struct_mark   *mark,    dumper &d,uint32 type);
  void gen(const struct_check_count *check,dumper &d,uint32 type);
  void gen(const struct_encode *encode,  dumper &d,uint32 type);
  void gen(const struct_match_end *match_end,dumper &d,uint32 type,
	   match_end_info *mei);

  void gen_match(const file_line &loc,
		 const struct_decl_list *items,dumper &d,uint32 type,
		 match_end_info *mei,
		 bool subevent,
		 bool last_subevent_item,
		 bool warn_empty,
		 bool sticky,
		 int flags);

  bool gen_optimized_match(const file_line &loc,
			   const struct_decl_list *items,
			   dumper &d,
			   const char *abort_spurious_label,
			   bool last_subevent_item);

  void gen(const event_definition *evt,dumper &d,uint32 type);

  void gen(const encode_spec  *encode,   dumper &d,uint32 type,
	   const prefix_ident *prefix);

  const var_external *gen_external_header(const variable *v,dumper &d,uint32 type);

  void gen_params(const param_list *params,
		  dumper &d,
		  uint32 type,
		  bool dump_member_args,
		  bool dump_default_args);

public:
  bool get_match_bits(const struct_decl      *decl,dumper &d,match_info &bits);
  bool get_match_bits(const struct_item_list *list,dumper &d,const arguments *args,match_info &bits);
  bool get_match_bits(const struct_item      *item,dumper &d,const arguments *args,match_info &bits);
  bool get_match_bits(const struct_data      *data,dumper &d,const arguments *args,match_info &bits);

protected:
  void gen_match_decl_quick(const std::vector<match_info> &infos,
			    dumper &d,int size,
			    const char *abort_spurious_label);

  void gen_unpack_decl(const struct_decl *decl,const char *func_decl,
		       dumper &d,bool dump_member_args);
  void gen_check_spurios_match(const struct_decl *decl,dumper &d,
			       const char *abort_spurious_label);

};

const struct_header_named *find_decl(const struct_decl* decl,bool subevent);

void generate_unpack_code(struct_definition *structure);
void generate_unpack_code(event_definition *event);

void gen_subevent_names(const event_definition *evt,dumper &d);

#endif//__UNPACK_CODE_HH__
