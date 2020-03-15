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

/* This file contains the grammar for a cabling documentation file file.
 *
 * With the help of bison(yacc), a C parser is produced.
 *
 * The lexer.lex produces the tokens.
 *
 * Don't be afraid.  Just look below and you will figure
 * out what is going on.
 */


%{
#define YYERROR_VERBOSE

#define YYTYPE_INT16 int

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "definitions.hh"
#include "parse_error.hh"

int yylex(void);

int yyparse(void);

void yyerror(const char *s);

extern int yylineno;

void print_lineno(FILE* fid,int internal);

struct md_ident_fl
{
 public:
  md_ident_fl(const char *id,const file_line &loc)
  {
    _id   = id;
    _loc = loc;
  }

 public:
  const char *_id;
  file_line   _loc;
};

#define CURR_FILE_LINE file_line(yylineno)

int _signal_spec_order_index = 0;

#define CURR_SIGNAL_COUNT (_signal_spec_order_index++)

#define CHECK_INDEX(index,max) { \
  { if ((index) <  0)     ERROR_LOC(CURR_FILE_LINE,"Variable index may not be negative (%d).\n",(index)); } \
  { if ((index) >= (max)) ERROR_LOC(CURR_FILE_LINE,"Cowardly refusing variable index (%d) >= %d.\n",(index),(max)); } \
}

#define CHECK_ARRAY_SIZE(index,max) { \
  { if ((index) <= 0)     ERROR_LOC(CURR_FILE_LINE,"Array size may not be negative (%d).\n",(index)); } \
  { if ((index) > (max)) ERROR_LOC(CURR_FILE_LINE,"Cowardly refusing array size (%d) > %d.\n",(index),(max)); } \
}

%}

%union {
  // What we get from the lexer:

  double  fValue;              /* double value */
  int     iValue;              /* integer value */
  const char *strValue;        /* string */

  int_range iRange;

  // We generate internally:

  def_node       *node;
  def_node_list  *nodes;

  struct_cond    *str_cond;
  //struct_several *str_several;
  struct_select  *str_select;
  struct_member  *str_member;
  struct_mark    *str_mark;
  struct_check_count *str_check;
  struct_decl    *str_decl;
  struct_decl_list *str_decl_list;
  struct_list    *str_list;

  struct_item    *str_item;

  struct_item_list *str_body;
  struct_header    *str_header;

  struct_definition *str_def;
  event_definition *ev_def;

  variable       *var;
  var_name       *var_n;

  sm_flags       *sm_flag;

  bits_spec_list *bits_list;
  bits_spec      *bits_item;

  encode_spec    *encode;
  encode_spec_list *encode_list;

  param_list *par_list;
  param      *par_item;
  argument_list   *arg_list;
  argument        *arg_item;

  bits_condition  *bits_cond;

  signal_spec     *sig;
  signal_info     *sig_info;

  signal_spec_ident_var *sig_id_var;
  signal_spec_types *sig_types;
  signal_spec_type_unit *sig_type_unit;

};

/* Things we get from the lexer */

%token <iValue>    INTEGER
%token <iRange>    INTEGER_RANGE
%token <fValue>    DOUBLE
%token <strValue>  STRING
%token <strValue>  IDENTIFIER
%token EVENT
%token STICKY_EVENT
%token SUBEVENT
%token SIGNAL
%token STICKY
%token TOGGLE
%token UINT64
%token UINT32
%token UINT16
%token UINT8
%token T_IF
%token T_ELSE
%token T_LIST
%token T_SEVERAL
%token T_OPTIONAL
%token T_SELECT
%token T_MULTI
%token T_EXTERNAL
%token T_NOREVISIT
%token T_REVISIT
%token T_IGNORE_UNKNOWN_SUBEVENT
%token T_STATIC_CAST
%token T_LOR         /* || */
%token T_LAND        /* && */
%token T_SHL         /* << */
%token T_SHR         /* >> */
%token T_LE          /* <= */
%token T_GE          /* >= */
%token T_EQ          /* == */
%token T_NEQ         /* != */
%token MATCH
%token CHECK
%token RANGE
%token ENCODE
%token NOENCODE
%token EXTERNAL
%token MEMBER
%token ZERO_SUPPRESS
%token ZERO_SUPPRESS_LIST
%token ZERO_SUPPRESS_MULTI
%token NO_INDEX_LIST
%token APPEND_LIST
%token FIRST_EVENT
%token LAST_EVENT
%token MARK
%token MARK_COUNT
%token CHECK_COUNT
%token MATCH_END

/* Operands for simple calculations */

%left T_LOR
%left T_LAND
%left '|'
%left '^'
%left '&'
%left T_EQ T_NEQ
%left '<' '>' T_LE T_GE
%left T_SHL T_SHR
%left '+' '-'
%left '*' '/' '%'
%nonassoc ULNOT
%nonassoc UMINUS

/* The statements (nodes) */

%type <node>  stmt
%type <nodes> stmt_list

/* A vector of doubles */

/*%type <vect_d> vector_items*/
/*%type <vect_d> vector*/
/*%type <fValue> value*/

/* Compounds(?) */

%type <ev_def>      event_definition
%type <ev_def>      event_definition_body
%type <str_def>     structure_definition
%type <str_cond>    conditional_item
//%type <str_several> several_item
%type <str_select>  select_item
%type <str_member>  member_item
%type <str_mark>    mark_count_item
%type <str_check>   check_count_item
%type <str_decl>    declared_item
%type <str_decl_list> declared_item_list_null
%type <str_decl_list> declared_item_list
%type <str_decl>    event_declared_item
%type <str_decl_list> event_declared_item_list
%type <str_list>    list_item
%type <str_item>    data_item
%type <str_body>    conditional_else_item
%type <str_item>    structure_item
%type <str_body>    structure_body_null
%type <str_body>    structure_body
%type <str_header>  structure_header
%type <var>         var_or_const_or_external
%type <var_n>       var_named
%type <var_n>       var_named_single
%type <var_n>       var_named_single_indexed
%type <var>         var_or_const
%type <var>         var_expr
%type <iValue>      var_eval_int_const

%type <iValue>      declared_item_multi_ext
%type <iValue>      declared_item_multi_ext_list
%type <iValue>      declared_item_multi_ext_item
%type <iValue>      data_item_flag
%type <iValue>      noencode
%type <sm_flag>     zero_suppress
%type <iValue>      encode_flags
%type <iValue>      select_flag
%type <encode_list> encode_item_list
%type <encode>      encode_item
%type <bits_list>   data_item_bits_list
%type <bits_item>   data_item_bits
%type <iValue>      data_item_size
%type <iRange>      bits_range

%type <par_list>    param_list_null
%type <par_list>    param_list
%type <par_item>    param_item
%type <arg_list>    arg_list_null
%type <arg_list>    arg_list
%type <arg_item>    arg_item

%type <bits_cond>   bits_condition

%type <sig>         signal
%type <sig_info>    signal_info
%type <iValue>      signal_tags
%type <iValue>      signal_tag_list
%type <iValue>      signal_tag

%type <sig_id_var>  signal_ident_var
%type <sig_id_var>  signal_ident_null
%type <sig_types>   signal_types
%type <strValue>    signal_type
%type <sig_type_unit> signal_type_unit

/*
%type <hw_def> hardware_definition
%type <hw_attr_list> hardware_attr_list
%type <hw_attr> hardware_attr
%type <hw_attr> connector_input
%type <hw_attr> connector_output
%type <hw_attr> connector_bidi
%type <iValue> connector_info_list
%type <iValue> connector_info
%type <hw_def> hardware_decl
%type <hw_def> hardware_decl_crate
%type <hw_def> hardware_decl_module
%type <hw_def> hardware_decl_unit

%type <module>        module_definition
%type <module>        module_decl
%type <md_part_list>  module_stmt_list
%type <md_part>       module_stmt
%type <md_part>       module_setting
%type <md_part>       module_connection
%type <md_connection> md_cnct_other
%type <md_idfl>       md_ident
%type <rcsuRangeValue> part_rcsu_range
%type <cb_mark>       cable_marker
%type <strValue>      cm_string

%type <string_list>   string_list
*/

%%

/* This specifies an entire file */
program:
          stmt_list             { append_list(&all_definitions,$1); }
        | /* NULL */
        ;

stmt_list:
          stmt                  { $$ = create_list($1); }
        | stmt_list stmt        { $$ = append_list($1,$2); }
        ;

/* Each statement is either an range specification or a parameter specification. */
stmt:
          ';'                                      { $$ = NULL; }
        | event_definition                         { $$ = $1; }
        | structure_definition                     { $$ = $1; }
        | signal ';'                               { $$ = $1; }
        | signal_info ';'                          { $$ = $1; }
/*        | '{' stmt_list '}'                        { $$ = node_list_pack($2); }*/
/*        | hardware_definition                      { append_hardware($1); $$ = NULL; } */
/*        | module_definition                        { $$ = $1; } */
/* | LT_RANGE '(' STRING ',' STRING ')' stmt  { $$ = lt_range($3,$5,append_node(NULL,$7)); } */
        ;


event_definition:
	  EVENT event_definition_body { $$ = $2; }
        | STICKY_EVENT event_definition_body { $2->_opts |= EVENT_OPTS_STICKY; $$ = $2; }
        ;

event_definition_body:
	  '{' event_declared_item_list '}' { event_definition *event = new event_definition(CURR_FILE_LINE,$2); check_valid_opts($2,STRUCT_DECL_OPTS_REVISIT | EVENT_OPTS_IGNORE_UNKNOWN_SUBEVENT); $$ = event; }
        ;

structure_definition:
          structure_header '{' structure_body_null '}' { $$ = new struct_definition(CURR_FILE_LINE,$1,$3); }
        | T_EXTERNAL structure_header ';' { $$ = new struct_definition(CURR_FILE_LINE,$2,NULL,STRUCT_DEF_OPTS_EXTERNAL); }
        ;

structure_header:
          SUBEVENT '(' IDENTIFIER ')'            { $$ = new struct_header_subevent($3,new std::vector<param*>); }
        | IDENTIFIER '(' param_list_null ')'     { $$ = new struct_header_named($1,$3); }
        ;

structure_body_null:
                         { null_list(&$$); }
        | structure_body { $$ = $1; }
        ;

structure_body:
          structure_item                { $$ = create_list($1); }
        | structure_body structure_item { $$ = append_list($1,$2); }
        ;

structure_item:
          ';'       { $$ = NULL; }
        | data_item { $$ = $1; }
        | list_item { $$ = $1; }
        | select_item { $$ = $1; }
//        | several_item { $$ = $1; }
        | conditional_item { $$ = $1; }
        | declared_item { $$ = $1; $1->check_valid_opts(STRUCT_DECL_OPTS_MULTI | STRUCT_DECL_OPTS_EXTERNAL); }
        | member_item { $$ = $1; }
        | mark_count_item { $$ = $1; }
        | check_count_item { $$ = $1; }
        | encode_item { struct_encode *encode = new struct_encode(CURR_FILE_LINE,$1); $$ = encode; }
        | MATCH_END ';' { struct_match_end *end = new struct_match_end(CURR_FILE_LINE); $$ = end; }
        ;

member_item:
          MEMBER '(' IDENTIFIER var_named_single zero_suppress ')' ';' { struct_member *member = new struct_member(CURR_FILE_LINE,$3,$4,$5); delete $5; $$ = member; }
        ;

mark_count_item:
          MARK '(' IDENTIFIER ')' ';' { struct_mark *mark = new struct_mark(CURR_FILE_LINE,$3,STRUCT_MARK_FLAGS_GLOBAL); $$ = mark; }
	| MARK_COUNT '(' IDENTIFIER ')' ';' { struct_mark *mark = new struct_mark(CURR_FILE_LINE,$3,0); $$ = mark; }
        ;

check_count_item:
	  CHECK_COUNT '(' var_expr ',' IDENTIFIER ',' IDENTIFIER ',' var_expr ',' var_expr ')' { struct_check_count *check = new struct_check_count(CURR_FILE_LINE,$3,$5,$7,$9,$11); $$ = check; }
        ;

event_declared_item_list:
          event_declared_item              { $$ = create_list($1); }
        | event_declared_item_list event_declared_item { $$ = append_list($1,$2); }
        ;

event_declared_item:
          T_IGNORE_UNKNOWN_SUBEVENT ';'    { struct_decl *decl = new struct_decl(CURR_FILE_LINE,NULL,NULL,NULL,EVENT_OPTS_IGNORE_UNKNOWN_SUBEVENT); $$ = decl; }
        | declared_item                    { $$ = $1; }
        ;

declared_item_list_null:
                             { null_list(&$$); }
        | declared_item_list { $$ = $1; }
        ;

declared_item_list:
          declared_item                    { $$ = create_list($1); }
        | declared_item_list declared_item { $$ = append_list($1,$2); }
        ;

declared_item:
          declared_item_multi_ext var_named_single '=' IDENTIFIER '(' arg_list_null ')' ';' { struct_decl *decl = new struct_decl(CURR_FILE_LINE,$2,$4,$6,$1); $$ = decl; }
        ;

declared_item_multi_ext:
	                               { $$ = 0; }
        | declared_item_multi_ext_list { $$ = $1; }
        ;

declared_item_multi_ext_list:
          declared_item_multi_ext_item                              { $$ = $1; }
        | declared_item_multi_ext_list declared_item_multi_ext_item { $$ = $1 | $2; }
        ;

declared_item_multi_ext_item:
          T_MULTI     { $$ = STRUCT_DECL_OPTS_MULTI; }
        | T_EXTERNAL  { $$ = STRUCT_DECL_OPTS_EXTERNAL; }
        | T_REVISIT   { $$ = STRUCT_DECL_OPTS_REVISIT; }
        | T_NOREVISIT { $$ = STRUCT_DECL_OPTS_NO_REVISIT; }
        ;

list_item:
	  T_LIST '(' var_or_const T_LE var_named '<' var_expr ')' '{' structure_body '}' { struct_list *list = new struct_list(CURR_FILE_LINE,$5,$3,$7,$10); $$ = list; }
        ;

select_item:
	  T_SELECT select_flag '{' declared_item_list_null '}' { struct_select *select = new struct_select(CURR_FILE_LINE,$4,$2); check_valid_opts($4,STRUCT_DECL_OPTS_MULTI | STRUCT_DECL_OPTS_EXTERNAL | STRUCT_DECL_OPTS_NO_REVISIT); $$ = select; }
//	| T_SELECT INTEGER   '{' structure_body '}' { struct_select *select = new struct_select($4,$2); $$ = select; }
        ;

select_flag:
	              { $$ = 0; }
	| T_SEVERAL   { $$ = SS_FLAGS_SEVERAL; }
	| T_OPTIONAL  { $$ = SS_FLAGS_OPTIONAL; }
        ;


//several_item:
//	  T_SEVERAL structure_item { struct_several *several = new struct_several($2); $$ = several; }
//        ;

conditional_item:
	  T_IF '(' var_expr ')' '{' structure_body '}' conditional_else_item { struct_cond *cond = new struct_cond(CURR_FILE_LINE,$3,$6,$8); $$ = cond; }
        ;

conditional_else_item:
	  T_ELSE '{' structure_body '}'  { $$ = $3; }
	| T_ELSE conditional_item        { $$ = create_list((struct_item*) $2); }
	|                                { $$ = NULL; }
	;

/*******************************************************/

var_or_const_or_external:
	  var_or_const                   { $$ = $1; }
        | EXTERNAL IDENTIFIER { variable* var = new var_external(CURR_FILE_LINE,$2); $$ = var; }
	;

var_or_const:
	  var_named            { $$ = $1; }
        | INTEGER              { variable* var = new var_const(CURR_FILE_LINE,$1); $$ = var; }
	;

var_named:
	  var_named_single         { $$ = $1; }
        | IDENTIFIER '.' var_named { var_name* var = new var_sub(CURR_FILE_LINE,$1,$3); $$ = var; }
        | IDENTIFIER '[' var_eval_int_const ']' '.' var_named { CHECK_INDEX($3,0x1000000); var_name* var = new var_sub(CURR_FILE_LINE,$1,$6,$3); $$ = var; }
	;

var_named_single_indexed:
	  IDENTIFIER               { var_name* var = new var_name(CURR_FILE_LINE,$1); $$ = var; }
        | IDENTIFIER '[' var_expr ']'   { var_indexed* var = new var_indexed(CURR_FILE_LINE,$1,$3); $$ = var; }
        | IDENTIFIER '[' var_expr ']' '[' var_expr ']'   { var_indexed* var = new var_indexed(CURR_FILE_LINE,$1,$6,$3); $$ = var; }
        ;

var_named_single:
	  IDENTIFIER               { var_name* var = new var_name(CURR_FILE_LINE,$1); $$ = var; }
	| IDENTIFIER '[' var_eval_int_const ']' { CHECK_INDEX($3,0x1000000);
                                       var_index* var = new var_index(CURR_FILE_LINE,$1,$3); $$ = var;
                                     }
        | IDENTIFIER '[' var_eval_int_const ']' '[' var_eval_int_const ']' { CHECK_INDEX($6,0x1000000);
	                               CHECK_INDEX($3,0x1000);
                                       var_index* var = new var_index(CURR_FILE_LINE,$1,$6,$3); $$ = var;
                                     }
	;

var_expr:
	  var_or_const_or_external   { $$ = $1; }
        | '-' var_expr %prec UMINUS  { var_expr* var = new var_expr(CURR_FILE_LINE,NULL,$2,VAR_OP_NEG); $$ = var; }
        | '+' var_expr %prec UMINUS  { $$ = $2; }
        | '~' var_expr %prec ULNOT  { var_expr* var = new var_expr(CURR_FILE_LINE,NULL,$2,VAR_OP_NOT); $$ = var; }
        | '!' var_expr %prec ULNOT  { var_expr* var = new var_expr(CURR_FILE_LINE,NULL,$2,VAR_OP_LNOT); $$ = var; }
        | '(' var_expr ')' { $$ = $2; }
        | var_expr T_LOR var_expr  { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_LOR ); $$ = var; }
        | var_expr T_LAND var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_LAND); $$ = var; }
        | var_expr '+' var_expr   { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_ADD); $$ = var; }
        | var_expr '-' var_expr   { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_SUB); $$ = var; }
        | var_expr '*' var_expr   { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_MUL); $$ = var; }
        | var_expr '/' var_expr   { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_DIV); $$ = var; }
        | var_expr '%' var_expr   { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_REM); $$ = var; }
        | var_expr '&' var_expr   { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_AND); $$ = var; }
        | var_expr '|' var_expr   { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_OR ); $$ = var; }
        | var_expr '^' var_expr   { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_XOR); $$ = var; }
        | var_expr T_SHR var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_SHR); $$ = var; }
        | var_expr T_SHL var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_SHL); $$ = var; }
        | var_expr '<' var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_LS); $$ = var; }
        | var_expr '>' var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_GR); $$ = var; }
        | var_expr T_LE var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_LE); $$ = var; }
        | var_expr T_GE var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_GE); $$ = var; }
        | var_expr T_EQ var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_EQ); $$ = var; }
        | var_expr T_NEQ var_expr { var_expr* var = new var_expr(CURR_FILE_LINE,$1,$3,VAR_OP_NEQ); $$ = var; }
        | T_STATIC_CAST '<' IDENTIFIER '>' '(' var_expr ')' { var_cast* var = new var_cast(CURR_FILE_LINE,$3,$6); $$ = var; }
	;

var_eval_int_const:
          var_expr { uint32 result;
	    if (!$1->eval(NULL, &result)) {
	      ERROR_LOC($1->_loc, "Expression does not evaluate to an integer constant.");
	    }
	    $$ = result;
	  }
        ;

/* Above:  // '(' IDENTIFIER ')' var_expr  lead to many shift/reduce conflicts

/*******************************************************/

param_list_null:
                         { null_list(&$$); }
        | param_list     { $$ = $1; }
        ;

param_list:
          param_item                { $$ = create_list($1); }
        | param_list ',' param_item { $$ = append_list($1,$3); }
        ;

param_item:
	  IDENTIFIER              { param* par = new param($1); $$ = par; }
	| IDENTIFIER '=' var_eval_int_const  { param* par = new param($1,new const32($3)); $$ = par; }
	;

/*******************************************************/

arg_list_null:
                         { null_list(&$$); }
        | arg_list       { $$ = $1; }
        ;

arg_list:
          arg_item                    { $$ = create_list($1); }
        | arg_list ',' arg_item       { $$ = append_list($1,$3); }
        ;

arg_item:
	  IDENTIFIER '=' var_expr { argument* arg = new argument($1,$3); $$ = arg; }
	;

/*******************************************************
 *
 * An actual data word:
 */

data_item:
          data_item_flag data_item_size IDENTIFIER noencode ';'                         { struct_data *item = new struct_data(CURR_FILE_LINE,$2,$3,$4 | $1,NULL,NULL); $$ = item; }
        | data_item_flag data_item_size IDENTIFIER noencode '{' data_item_bits_list '}' { struct_data *item = new struct_data(CURR_FILE_LINE,$2,$3,$4 | $1,$6,NULL); $$ = item; }
        | data_item_flag data_item_size IDENTIFIER noencode '{' data_item_bits_list encode_item_list '}' { struct_data *item = new struct_data(CURR_FILE_LINE,$2,$3,$4 | $1,$6,$7); $$ = item; }
	;

data_item_flag:
	             { $$ = 0; }
	| T_OPTIONAL { $$ = SD_FLAGS_OPTIONAL; }
	| T_SEVERAL  { $$ = SD_FLAGS_SEVERAL; }
        ;

noencode:
	           { $$ = 0; }
	| NOENCODE { $$ = SD_FLAGS_NOENCODE; }
        ;

zero_suppress:
	                { $$ = new sm_flags(0); }
	| ZERO_SUPPRESS { $$ = new sm_flags(SM_FLAGS_ZERO_SUPPRESS); }
	| ZERO_SUPPRESS_LIST { $$ = new sm_flags(SM_FLAGS_ZERO_SUPPRESS_LIST); }
        | ZERO_SUPPRESS_MULTI '(' var_eval_int_const ')' { CHECK_ARRAY_SIZE($3,0x10000); $$ = new sm_flags(SM_FLAGS_ZERO_SUPPRESS_MULTI,$3); }
	| NO_INDEX_LIST { $$ = new sm_flags(SM_FLAGS_NO_INDEX_LIST); }
        ;

encode_item_list:
          encode_item                  { $$ = create_list($1); }
        | encode_item_list encode_item { $$ = append_list($1,$2); }
        ;

encode_item:
	  ENCODE '(' var_named_single_indexed encode_flags ',' '(' arg_list ')' ')' ';' { encode_spec *encode = new encode_spec(CURR_FILE_LINE,$3,$7,$4); $$ = encode; }
	;

encode_flags:
	              { $$ = 0; }
	| APPEND_LIST { $$ = ES_APPEND_LIST; }
        ;

data_item_size:
	  UINT64 { $$ = 64; }
	| UINT32 { $$ = 32; }
	| UINT16 { $$ = 16; }
	| UINT8  { $$ = 8;  }
        ;

data_item_bits_list:
          data_item_bits                     { $$ = create_set($1); }
        | data_item_bits_list data_item_bits { if (!insert_set(&$$,$1,$2) ) ERROR_LOC(CURR_FILE_LINE,"Bits are overlapping.\n"); }
        ;

data_item_bits:
	  bits_range IDENTIFIER ';' { bits_spec *bits = new bits_spec($1,$2,NULL); $$ = bits; }
	| bits_range INTEGER    ';' { bits_spec *bits = new bits_spec($1,NULL,new bits_cond_check(CURR_FILE_LINE,new var_const(CURR_FILE_LINE,$2))); $$ = bits; }
	| bits_range '(' var_eval_int_const ')'    ';' { bits_spec *bits = new bits_spec($1,NULL,new bits_cond_check(CURR_FILE_LINE,new var_const(CURR_FILE_LINE,$3))); $$ = bits; }
	| bits_range IDENTIFIER '=' var_eval_int_const ';' { bits_spec *bits = new bits_spec($1,$2,new bits_cond_check(CURR_FILE_LINE,new var_const(CURR_FILE_LINE,$4))); $$ = bits; }
	| bits_range IDENTIFIER '=' bits_condition ';' { bits_spec *bits = new bits_spec($1,$2,$4); $$ = bits; }
        ;

bits_range:
	  INTEGER ':'           { $$._min = $1; $$._max = $1; }
	| INTEGER_RANGE ':'     { $$ = $1; }
	;

bits_condition:
	  CHECK '(' var_expr ')'         { bits_condition *cond = new bits_cond_check(CURR_FILE_LINE,$3); $$ = cond; }
	| MATCH '(' var_expr ')'         { bits_condition *cond = new bits_cond_match(CURR_FILE_LINE,$3); $$ = cond; }
	| RANGE '(' var_expr ',' var_expr ')' { bits_condition *cond = new bits_cond_range(CURR_FILE_LINE,$3,$5); $$ = cond; }
	| CHECK_COUNT '(' IDENTIFIER ',' IDENTIFIER ',' var_expr ',' var_expr ')' { bits_condition *cond = new bits_cond_check_count(CURR_FILE_LINE,$3,$5,$7,$9); $$ = cond; }
	;

/*******************************************************/

signal:
	  SIGNAL '(' signal_ident_var ',' signal_types ')'
	    { signal_spec* signal =
		new signal_spec(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				0,
				$3->_name,$3->_ident,$5,0);
	      delete $3; $$ = signal;
	    }
        | SIGNAL '(' signal_ident_var ','
                     signal_ident_var ',' signal_types ')'
	    { signal_spec* signal =
		new signal_spec_range(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				      0,
				      $3->_name,$3->_ident,
				      $5->_name,$5->_ident,$7,0);
	      delete $3; delete $5; $$ = signal;
	    }
	| SIGNAL '(' signal_tags
                     signal_ident_var ',' signal_types ')'
	    { signal_spec* signal =
		new signal_spec(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				0,
				$4->_name,$4->_ident,$6,$3);
	      delete $4; $$ = signal;
	    }
        | SIGNAL '(' signal_tags
                     signal_ident_var ','
                     signal_ident_var ',' signal_types ')'
	    { signal_spec* signal =
		new signal_spec_range(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				      0,
				      $4->_name,$4->_ident,
				      $6->_name,$6->_ident,$8,$3);
	      delete $4; delete $6; $$ = signal;
	    }
	;

signal:
	  SIGNAL '(' signal_ident_null ',' signal_types ')'
	    { signal_spec* signal =
		new signal_spec(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				0,
				$3->_name,$3->_ident,$5,0);
	      delete $3; $$ = signal;
	    }
	;

signal:
	  STICKY '(' signal_ident_var ',' signal_types ')'
	    { signal_spec* signal =
		new signal_spec(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				EVENT_OPTS_STICKY,
				$3->_name,$3->_ident,$5,0);
	      delete $3; $$ = signal;
	    }
        | STICKY '(' signal_ident_var ','
                     signal_ident_var ',' signal_types ')'
	    { signal_spec* signal =
		new signal_spec_range(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				      EVENT_OPTS_STICKY,
				      $3->_name,$3->_ident,
				      $5->_name,$5->_ident,$7,0);
	      delete $3; delete $5; $$ = signal;
	    }
	;

signal:
	  STICKY '(' signal_ident_null ',' signal_types ')'
	    { signal_spec* signal =
		new signal_spec(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				EVENT_OPTS_STICKY,
				$3->_name,$3->_ident,$5,0);
	      delete $3; $$ = signal;
	    }
	;

signal_ident_var:
	  IDENTIFIER ',' var_named
	    { signal_spec_ident_var* idvar =
		new signal_spec_ident_var($1,$3); $$ = idvar;
	    }
        ;

signal_ident_null:
	IDENTIFIER ',' /* var_named omitted */
	    { signal_spec_ident_var* idvar =
		new signal_spec_ident_var($1,NULL); $$ = idvar;
	    }
        ;

signal_types:
	  signal_type_unit
	    { $$ = new signal_spec_types($1); }
	| '(' signal_type_unit ',' signal_type_unit ')'
	    { $$ = new signal_spec_types($2,$4); }
	;

signal_type:
          IDENTIFIER         { $$ = $1; }
        | data_item_size     { $$ = data_item_size_to_signal_type($1); }
        ;

signal_type_unit:
	  signal_type         { $$ = new signal_spec_type_unit($1); }
	| signal_type STRING  { insert_prefix_units_exp(CURR_FILE_LINE,$2);
	                        $$ = new signal_spec_type_unit($1,$2); }
	;

signal_tags:
	  signal_tag_list ':' { $$ = $1; }
	;

signal_tag_list:
          signal_tag                     { $$ = $1; }
        | signal_tag_list ',' signal_tag { $$ = $1 | $3; }
        ;

signal_tag:
	  FIRST_EVENT    { $$ = SIGNAL_TAG_FIRST_EVENT; }
        | LAST_EVENT     { $$ = SIGNAL_TAG_LAST_EVENT; }
        | TOGGLE INTEGER { 
	    if ($2 == 1)      { $$ = SIGNAL_TAG_TOGGLE_1; }
	    else if ($2 == 2) { $$ = SIGNAL_TAG_TOGGLE_2; }
	    else {	    
	      ERROR_LOC(CURR_FILE_LINE, "Toggle # must be 1 or 2.");
	    }
	  }
	;

signal_info:
          SIGNAL '(' ZERO_SUPPRESS ':' IDENTIFIER ')'
	    { signal_info* signal =
		new signal_info(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				$5,SIGNAL_INFO_ZERO_SUPPRESS); $$ = signal;
	    }
	| SIGNAL '(' NO_INDEX_LIST ':' IDENTIFIER ')'
	    { signal_info* signal =
		new signal_info(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				$5,SIGNAL_INFO_NO_INDEX_LIST); $$ = signal;
	    }
	| SIGNAL '(' NO_INDEX_LIST '(' var_eval_int_const ')' ':'
	/**/         IDENTIFIER ')'
	    { signal_info* signal =
		new signal_info(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				add_index_to_identifier($8,$5),
				SIGNAL_INFO_NO_INDEX_LIST); $$ = signal;
	    }
	| SIGNAL '(' ZERO_SUPPRESS_MULTI '(' var_eval_int_const ')' ':'
	/**/         IDENTIFIER ')'
	    { CHECK_ARRAY_SIZE($5,0x10000);
	      signal_info* signal =
		new signal_info(CURR_FILE_LINE,CURR_SIGNAL_COUNT,
				$8,SIGNAL_INFO_ZERO_SUPPRESS_MULTI,$5);
	      $$ = signal;
	    }
        ;

/*******************************************************/





/* A vector of floating point values */
/*
vector:
          '(' vector_items ')'     { $$ = $2; }
	;

vector_items:
	  value                  { $$ = append_vector(NULL,$1); }
        | vector_items ',' value { $$ = append_vector($1,$3); }
        ;
*/
/* Floating point values.  Simple calculations allowed. */
/*
value:
          DOUBLE                  { $$ = $1; }
        | '-' value %prec UMINUS  { $$ = -$2; }
        | '+' value %prec UMINUS  { $$ = $2; }
        | value '+' value         { $$ = $1 + $3; }
        | value '-' value         { $$ = $1 - $3; }
        | value '*' value         { $$ = $1 * $3; }
        | value '/' value
          {
	    if (!$3)
#ifdef YYBISON
	      fprintf(stderr,"Warning: Division by zero, l%d,c%d-l%d,c%d",
		      @3.first_line, @3.first_column,
		      @3.last_line,  @3.last_column);
#endif
	    $$ = $1 / $3;
	  }
        | '(' value ')'           { $$ = $2; }
        ;
*/
%%

void yyerror(const char *s) {
  print_lineno(stderr,yylineno);
  fprintf(stderr," %s\n", s);
/*
  Current.first_line   = Rhs[1].first_line;
  Current.first_column = Rhs[1].first_column;
  Current.last_line    = Rhs[N].last_line;
  Current.last_column  = Rhs[N].last_column;
*/
}

bool parse_definitions()
{
  // yylloc.first_line = yylloc.last_line = 1;
  // yylloc.first_column = yylloc.last_column = 0;

  return yyparse() == 0;
}
