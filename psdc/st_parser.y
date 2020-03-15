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

#include "c_struct.hh"
#include "file_line.hh"
#include "parse_error.hh"
#include "node.hh"

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

%}

%union {
  // What we get from the lexer:

  double  fValue;              /* double value */
  int     iValue;              /* integer value */
  const char *strValue;        /* string */

  // We generate internally:

  def_node       *node;
  def_node_list  *nodes;

  c_struct_def   *str_def;
  c_struct_item_list  *str_item_list;
  c_struct_item  *str_item;

  c_typename     *item_type;

  c_array_ind    *array_list;

  c_arg_list     *arg_list;
  c_arg          *arg;

  //c_bitfield_item_list *bitfield_list;
  //c_bitfield_item *bitfield;

  c_base_class_list *base_list;

};

/* Things we get from the lexer */

%token <iValue>    INTEGER
%token <fValue>    DOUBLE
%token <strValue>  STRING
%token <strValue>  IDENTIFIER
%token STRUCT
%token UNION
%token CLASS
%token PUBLIC
%token MULTI
%token UNIT
%token TOGGLE

/* Operands for simple calculations */

/* The statements (nodes) */

%type <node>  stmt
%type <nodes> stmt_list

/* A vector of doubles */

/*%type <vect_d> vector_items*/
/*%type <vect_d> vector*/
/*%type <fValue> value*/

/* Compounds(?) */

%type <strValue>   ident_null
%type <iValue>     struct_or_class
%type <str_def>    struct_definition

%type <str_item_list>   struct_item_list_null
%type <str_item_list>   struct_item_list
%type <str_item>   struct_item
%type <str_item>   struct_member
//%type <str_def>    bitfield

%type <strValue>   unit

%type <item_type>  item_toggle_typename
%type <item_type>  item_typename
%type <item_type>  typename_null
%type <item_type>  typename

%type <arg_list>   argument_list
%type <arg>        argument
/*
%type <bitfield_list> bitfield_item_list
%type <bitfield>   bitfield_item
*/
%type <array_list> array_spec_list_null
%type <array_list> array_spec_list
%type <iValue>     array_spec

%type <base_list>  base_class_list
%type <strValue>   base_class

%expect 0

// shift/reduce conflift:
//
// IDENTIFIER IDENTIFIER  followed by  ':'  is part of a bitfield
// IDENTIFIER IDENTIFIER  followed by  '[' or ';' is part of an variable

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
        | struct_definition                        { $$ = $1; }
/*        | '{' stmt_list '}'                        { $$ = node_list_pack($2); }*/
/*        | hardware_definition                      { append_hardware($1); $$ = NULL; } */
/*        | module_definition                        { $$ = $1; } */
/* | LT_RANGE '(' STRING ',' STRING ')' stmt  { $$ = lt_range($3,$5,append_node(NULL,$7)); } */
        ;

/*******************************************************/

struct_definition:
          struct_or_class typename_null '{' struct_item_list_null '}' ident_null array_spec_list_null ';' { $$ = new c_struct_def(CURR_FILE_LINE,$1,$2,NULL,$4,$6,$7); }
        | struct_or_class typename ':' base_class_list '{' struct_item_list_null '}' ident_null array_spec_list_null ';' { $$ = new c_struct_def(CURR_FILE_LINE,$1,$2,$4,$6,$8,$9); }
//| bitfield                                 { $$ = $1; }
        ;
/*
bitfield:
          struct_or_class typename_null '{' bitfield_item_list '}' ident_null ';' { $$ = new c_struct_bitfield(CURR_FILE_LINE,$2,$4,$6); }
        ;
*/
struct_or_class:
          STRUCT       { $$ = C_STR_STRUCT; }
        | UNION        { $$ = C_STR_UNION; }
        | CLASS        { $$ = C_STR_CLASS; }
        ;

ident_null:
                       { $$ = NULL; }
	| IDENTIFIER   { $$ = $1; }
	;

base_class_list:
          base_class                     { $$ = create_list($1); }
        | base_class_list ',' base_class { $$ = append_list($1,$3); }
        ;

base_class:
	  PUBLIC IDENTIFIER    { $$ = $2; }
	;

struct_item_list_null:
                           { null_list(&$$); }
        | struct_item_list { $$ = $1; }
        ;

struct_item_list:
          struct_item                  { $$ = create_list($1); }
        | struct_item_list struct_item { $$ = append_list($1,$2); }
        ;

struct_item:
          PUBLIC ':'                   { $$ = NULL; /* silenty ignore */ }
        | ';'                          { $$ = NULL; /* silenty ignore */ }
        | struct_definition            { $$ = $1; }
        | struct_member ';'            { $$ = $1; }
        | struct_member unit ';'       { $$ = $1; $1->set_unit($2); }
        | struct_member array_spec_list ';' { $$ = $1; $1->set_array_ind($2); }
        | struct_member array_spec_list unit ';' { $$ = $1; $1->set_array_ind($2); $1->set_unit($3); }
        | struct_member ':' INTEGER ';' { $1->set_bitfield($3); $$ = $1; }
        | struct_member unit ':' INTEGER ';' { $1->set_bitfield($4); $1->set_unit($2); $$ = $1; }
        ;

struct_member:
          item_toggle_typename IDENTIFIER        { $$ = new c_struct_member(CURR_FILE_LINE,$1,$2,NULL,0,0); }
        | MULTI item_toggle_typename IDENTIFIER  { $$ = new c_struct_member(CURR_FILE_LINE,$2,$3,NULL,0,1); }
        ;

unit:
          UNIT '(' STRING ')' { insert_prefix_units_exp(CURR_FILE_LINE,$3); $$ = $3; }
	;

typename_null:
                   { $$ = NULL; }
	| typename { $$ = $1; }
	;

typename:
	  IDENTIFIER   { $$ = new c_typename(CURR_FILE_LINE,$1); }
	;

item_toggle_typename:
          item_typename                { $$ = $1; }
        | TOGGLE '(' item_typename ')' { $3->set_toggle(); $$ = $3; }
        ;

item_typename:
	  IDENTIFIER                   { $$ = new c_typename(CURR_FILE_LINE,$1); }
        | IDENTIFIER '<' argument_list '>'  { $$ = new c_typename_template(CURR_FILE_LINE,$1,$3); }
        ;

argument_list:
          argument                     { $$ = create_list($1); }
        | argument_list ',' argument   { $$ = append_list($1,$3); }
        ;

argument:
          IDENTIFIER array_spec_list_null { $$ = new c_arg_named(CURR_FILE_LINE,$1,$2,false); }
        | TOGGLE '(' IDENTIFIER ')' array_spec_list_null { $$ = new c_arg_named(CURR_FILE_LINE,$3,$5,true); }
	| INTEGER                         { $$ = new c_arg_const(CURR_FILE_LINE,$1); }
        ;

array_spec_list_null:
                                       { $$ = NULL; }
        | array_spec_list              { $$ = $1; }
        ;

array_spec_list:
	  array_spec                   { $$ = new c_array_ind; $$->push_back($1); }
	| array_spec_list array_spec   { $1->push_back($2); $$ = $1; }
	;

array_spec:
	  '[' INTEGER ']'              { $$ = $2; }
        ;




/*
bitfield_item_list:
          bitfield_item                    { $$ = create_list($1); }
        | bitfield_item_list bitfield_item { $$ = append_list($1,$2); }
        ;

bitfield_item:
	  IDENTIFIER IDENTIFIER ':' INTEGER ';'   { $$ = new c_bitfield_item(CURR_FILE_LINE,$1,$2,$4); }
        ;
*/

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
