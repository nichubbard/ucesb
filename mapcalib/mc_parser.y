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

#include "mc_def.hh"
#include "str_set.hh"

#include "signal_id_map.hh"
#include "../common/prefix_unit.hh"

//#include "parse_error.hh"

//#include "file_line.hh"

int yylex(void);

int yyparse(void);

void yyerror(const char *s);

extern int yylineno;

void print_lineno(FILE* fid,int internal);
/*
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
*/

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

  map_info       *map;
  calib_param    *calib;
  user_calib_param    *user_calib;

  sig_part            *var_part;
  sig_part_ptr_vector *var_vect;
  signal_id           *var;

  double_unit          du;

  vect_double_unit    *v_double_unit;
};

/* Things we get from the lexer */

%token <iValue>    INTEGER
%token <fValue>    DOUBLE
%token <strValue>  STRING
%token <strValue>  IDENTIFIER

%token SIGNAL_MAPPING
%token STICKY_MAPPING
%token CALIB_PARAM
%token CALIB_PARAM_C
%token TOGGLE
%token UINT32
%token UINT16
%token UINT8

/* Operands for simple calculations */

%left '+' '-'
%left '*' '/'
%nonassoc UMINUS

/* The statements (nodes) */

%type <node>       stmt
%type <nodes>      stmt_list

/* Compounds(?) */

%type <map>        signal_mapping
%type <calib>      calib_param
%type <user_calib> user_calib_param

%type <iValue>     data_type
%type <iValue>     calib_type

%type <iValue>     toggle_null
%type <iValue>     toggle

%type <var>        var_or_name
%type <var>        data_name

%type <var_vect>   var_ident
%type <var_vect>   var_ident_rec

%type <var_vect>   array_spec_list_null
%type <var_vect>   array_spec_list
%type <var_part>   array_spec

/* A vector of doubles */

%type <fValue>     value
//%type <fValue>     value_vector
%type <v_double_unit> value_vector_np
%type <v_double_unit> value_vector_np_single

%type <du>         value_unit

%type <strValue>   unit
%type <strValue>   unit_part
%type <strValue>   unit_part_many
%type <strValue>   unit_part_block

%%

/* This specifies an entire file */
program:
          stmt_list             { append_list(&all_mc_defs,$1); }
        | /* NULL */
        ;

stmt_list:
          stmt                  { $$ = create_list($1); }
        | stmt_list stmt        { $$ = append_list($1,$2); }
        ;

/* Each statement is either an range specification or a parameter specification. */
stmt:
          ';'                                      { $$ = NULL; }
        | signal_mapping                           { $$ = $1; }
        | calib_param                              { $$ = $1; }
        | user_calib_param                         { $$ = $1; }
/*        | '{' stmt_list '}'                        { $$ = node_list_pack($2); }*/
/*        | hardware_definition                      { append_hardware($1); $$ = NULL; } */
/*        | module_definition                        { $$ = $1; } */
/* | LT_RANGE '(' STRING ',' STRING ')' stmt  { $$ = lt_range($3,$5,append_node(NULL,$7)); } */
        ;

signal_mapping:
	  SIGNAL_MAPPING '(' data_type ',' data_name ',' var_or_name ',' toggle_null var_or_name ')' ';'
          {
	    const signal_id_info *src_info  =
	      get_signal_id_info($7,SID_MAP_UNPACK | SID_MAP_MIRROR_MAP);
	    const signal_id_info *dest_info =
	      get_signal_id_info($10,SID_MAP_RAW);
	    /*const signal_id_info *rev_src_info  = get_signal_id_info($9,SID_MAP_RAW | SID_MAP_MIRROR_MAP | SID_MAP_REVERSE);*/
	      /*const signal_id_info *rev_dest_info = get_signal_id_info($7,SID_MAP_UNPACK);*/
	    delete $7;
	    delete $10;
	    $$ = new map_info(CURR_FILE_LINE,src_info,dest_info,
			      NULL,NULL/*,rev_src_info,rev_dest_info*/,
			      0,$9);
          }
	| STICKY_MAPPING '(' data_type ',' data_name ',' var_or_name ',' toggle_null var_or_name ')' ';'
          {
	    const signal_id_info *src_info  =
	      get_signal_id_info($7,SID_MAP_UNPACK | SID_MAP_STICKY | SID_MAP_MIRROR_MAP);
	    const signal_id_info *dest_info =
	      get_signal_id_info($10,SID_MAP_RAW | SID_MAP_STICKY);
	    /*const signal_id_info *rev_src_info  = get_signal_id_info($9,SID_MAP_RAW | SID_MAP_MIRROR_MAP | SID_MAP_REVERSE);*/
	      /*const signal_id_info *rev_dest_info = get_signal_id_info($7,SID_MAP_UNPACK);*/
	    delete $7;
	    delete $10;
	    $$ = new map_info(CURR_FILE_LINE,src_info,dest_info,
			      NULL,NULL/*,rev_src_info,rev_dest_info*/,
			      1,$9);
          }
        ;

/* If we were to use the toggle_null rule below before IDENTIFIER,
 * we get shift/reduce conflicts with the rules in user_calib_param.
 * Where also the default action of shifting is not really helpful.
 * So, we take to ugly route of duplicating the rules...
 */

calib_param:
	  CALIB_PARAM '(' IDENTIFIER ',' calib_type ',' value_vector_np ')' ';'
          {
	    signal_id id;
	    dissect_name(CURR_FILE_LINE,$3,id);
	    const signal_id_info *src_info  =
	      get_signal_id_info(&id,SID_MAP_RAW | SID_MAP_MIRROR_MAP);
	    const signal_id_info *dest_info =
	      get_signal_id_info(&id,SID_MAP_CAL);
	    $$ = new calib_param(CURR_FILE_LINE,src_info,dest_info,
				 $5,$7,0);
          }
	| CALIB_PARAM '(' toggle IDENTIFIER ',' calib_type ',' value_vector_np ')' ';'
          {
	    signal_id id;
	    dissect_name(CURR_FILE_LINE,$4,id);
	    const signal_id_info *src_info  =
	      get_signal_id_info(&id,SID_MAP_RAW | SID_MAP_MIRROR_MAP);
	    const signal_id_info *dest_info =
	      get_signal_id_info(&id,SID_MAP_CAL);
	    $$ = new calib_param(CURR_FILE_LINE,src_info,dest_info,
				 $6,$8,$3);
          }
	| CALIB_PARAM_C '(' var_or_name ',' calib_type ',' value_vector_np ')' ';'
          {
	    const signal_id_info *src_info  =
	      get_signal_id_info($3,SID_MAP_RAW | SID_MAP_MIRROR_MAP);
	    const signal_id_info *dest_info =
	      get_signal_id_info($3,SID_MAP_CAL);
	    delete $3;
	    $$ = new calib_param(CURR_FILE_LINE,src_info,dest_info,
				 $5,$7,0);
          }
	| CALIB_PARAM_C '(' toggle var_or_name ',' calib_type ',' value_vector_np ')' ';'
          {
	    const signal_id_info *src_info  =
	      get_signal_id_info($4,SID_MAP_RAW | SID_MAP_MIRROR_MAP);
	    const signal_id_info *dest_info =
	      get_signal_id_info($4,SID_MAP_CAL);
	    delete $4;
	    $$ = new calib_param(CURR_FILE_LINE,src_info,dest_info,
				 $6,$8,$3);
          }
        | var_or_name '=' calib_type '(' value_vector_np ')' ';'
          {
	    const signal_id_info *src_info  =
	      get_signal_id_info($1,SID_MAP_RAW | SID_MAP_MIRROR_MAP);
	    const signal_id_info *dest_info =
	      get_signal_id_info($1,SID_MAP_CAL);
	    delete $1;
	    $$ = new calib_param(CURR_FILE_LINE,src_info,dest_info,
				 $3,$5,0);
          }
        | toggle var_or_name '=' calib_type '(' value_vector_np ')' ';'
          {
	    const signal_id_info *src_info  =
	      get_signal_id_info($2,SID_MAP_RAW | SID_MAP_MIRROR_MAP);
	    const signal_id_info *dest_info =
	      get_signal_id_info($2,SID_MAP_CAL);
	    delete $2;
	    $$ = new calib_param(CURR_FILE_LINE,src_info,dest_info,
				 $4,$6,$1);
          }
        ;

user_calib_param:
	  CALIB_PARAM '(' IDENTIFIER ',' value_vector_np ')' ';'
          {
	    signal_id id;
	    dissect_name(CURR_FILE_LINE,$3,id);
	    const signal_id_info *dest_info =
	      get_signal_id_info(&id,SID_MAP_CALIB);
	    $$ = new user_calib_param(CURR_FILE_LINE,dest_info,$5);
          }
	| CALIB_PARAM_C '(' var_or_name ',' value_vector_np ')' ';'
          {
	    const signal_id_info *dest_info =
	      get_signal_id_info($3,SID_MAP_CALIB);
	    delete $3;
	    $$ = new user_calib_param(CURR_FILE_LINE,dest_info,$5);
          }
        | var_or_name '=' value_vector_np_single ';'
          {
	    const signal_id_info *dest_info =
	      get_signal_id_info($1,SID_MAP_CALIB);
	    delete $1;
	    $$ = new user_calib_param(CURR_FILE_LINE,dest_info,$3);
          }
        | var_or_name '=' '{' value_vector_np '}' ';'
          {
	    const signal_id_info *dest_info =
	      get_signal_id_info($1,SID_MAP_CALIB);
	    delete $1;
	    $$ = new user_calib_param(CURR_FILE_LINE,dest_info,$4);
          }
        ;

//SIGNAL_MAPPING(DATA12,SCI1_1_E,vme.adc[0].data[0],SCI[0][0].E);
//SIGNAL_MAPPING(DATA12,SCI1_1_T,vme.tdc[0].data[0],SCI[0][0].T);

//CALIB_PARAM( SCI[0][0].T ,SLOPE_OFFSET,  2.4 , 6.7 );
//CALIB_PARAM( SCI[0][0].E ,SLOPE_OFFSET,  2.2 , 6.8 );

/*******************************************************/

calib_type:
	  IDENTIFIER { $$ = calib_param_type($1);
	    if (!$$) { yyerror("Unknown calibration type."); YYERROR; } }
        ;

/*******************************************************/

data_type:
	  IDENTIFIER { $$ = 0; /* $$ = data_type($1); if (!$$) { yyerror("Unknown data type."); YYERROR; } */ }
        ;

toggle_null:
          /* empty */ { $$ = 0; }
        | toggle
        ;

toggle:
          TOGGLE INTEGER ':' {
	    if ($2 == 1)      { $$ = 1; }
	    else if ($2 == 2) { $$ = 2; }
	    else {
	      yyerror("Toggle # must be 1 or 2.");
	    }
	  }
        ;

var_or_name:
	  var_ident_rec { $$ = new signal_id(); $$->take_over($1); /* Note: this deletes $1! */ }
        ;

data_name:
	  IDENTIFIER { $$ = NULL; /* $$ = new signal_id(); dissect_name(0,$1,*$$); */ }
        ;

var_ident_rec:
	  var_ident                   { $$ = $1; }
        | var_ident_rec '.' var_ident { $$ = $1; $$->insert($$->end(),$3->begin(),$3->end()); delete $3; }
	;

var_ident:
	  IDENTIFIER array_spec_list_null
          {
	    $$ = new sig_part_ptr_vector;
	    $$->push_back(new sig_part($1));
	    if ($2)
	      {
		$$->insert($$->end(),$2->begin(),$2->end());
		delete $2;
	      }
	  }
	;

array_spec_list_null:
                                       { $$ = NULL; }
        | array_spec_list              { $$ = $1; }
        ;

array_spec_list:
	  array_spec                   { $$ = new sig_part_ptr_vector; $$->push_back($1); }
	| array_spec_list array_spec   { $1->push_back($2); $$ = $1; }
	;

array_spec:
	  '[' INTEGER ']'              { $$ = new sig_part($2); }
        ;

/*******************************************************/

/* A vector of floating point values */
/*
value_vector:
          '(' value_vector_np ')'      { $$ = $2; }
	;
*/
value_vector_np:
	  value_vector_np_single            { $$ = $1; }
        | value_vector_np ',' value_unit    { $$ = $1; $$->push_back($3); }
        ;

value_vector_np_single:
	  value_unit                        { $$ = new vect_double_unit; $$->push_back($1); }
	;

value_unit:
	  value                   { $$._value = $1; $$._unit = NULL; }
	| value unit              { $$._value = $1; $$._unit = insert_units_exp(CURR_FILE_LINE,$2); }
        ;

/* Floating point values.  Simple calculations allowed. */
value:
          DOUBLE                  { $$ = $1; }
        | INTEGER                 { $$ = $1; }
        | '-' value %prec UMINUS  { $$ = -$2; }
        | '+' value %prec UMINUS  { $$ = $2; }
        | value '+' value         { $$ = $1 + $3; }
        | value '-' value         { $$ = $1 - $3; }
        | value '*' value         { $$ = $1 * $3; }
        | value '/' value
          {
	    if ($3 != 0.0)
	      yyerror("Warning: Division by zero.");
	    $$ = $1 / $3;
	  }
        | '(' value ')'           { $$ = $2; }
        ;

/* Specifying a unit. */
unit:
	  STRING        { $$ = $1; }
        | unit_part     { $$ = $1; }
        | '/' unit_part { $$ = find_concat_str_strings("/",$2,""); }
        ;

unit_part:
	  unit_part_many               { $$ = $1; }
        | unit_part_many '*' unit_part { $$ = find_concat_str_strings($1,"*",$3); }
        | unit_part_many '/' unit_part { $$ = find_concat_str_strings($1,"/",$3); }
        ;

unit_part_many:
	  unit_part_block                { $$ = $1; }
	| unit_part_many unit_part_block { $$ = find_concat_str_strings($1," ",$2); }
        ;

unit_part_block:
	  IDENTIFIER             { $$ = $1; }
        | IDENTIFIER INTEGER     { char tmp[32];
	    sprintf (tmp,"%d",$2); $$ = find_concat_str_strings($1,tmp,""); }
        | IDENTIFIER '-' INTEGER { char tmp[32];
	    sprintf (tmp,"%d",$3); $$ = find_concat_str_strings($1,"-",tmp); }
        | IDENTIFIER '^' INTEGER     { char tmp[32];
	    sprintf (tmp,"%d",$3); $$ = find_concat_str_strings($1,"^",tmp); }
        | IDENTIFIER '^' '-' INTEGER { char tmp[32];
	    sprintf (tmp,"%d",$4); $$ = find_concat_str_strings($1,"^-",tmp); }
        ;

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
  throw error();
}

bool parse_definitions()
{
  // yylloc.first_line = yylloc.last_line = 1;
  // yylloc.first_column = yylloc.last_column = 0;

  return yyparse() == 0;
}
