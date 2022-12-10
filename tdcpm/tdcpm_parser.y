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

/* This file contains the grammar for time-dependent calibration parameters.
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

#include "../tdcpm_defs.h"
#include "../tdcpm_string_table.h"
#include "../tdcpm_var_name.h"
#include "../tdcpm_units.h"
#include "../tdcpm_tspec_table.h"
#include "../tdcpm_file_line.h"

/* #include "parse_error.hh" */
/* #include "file_line.hh" */

int yylex(void);

int yyparse(void);

void yyerror(const char *s);

extern int yylineno;

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
  /* What we get from the lexer: */

  double  fValue;               /* double value */
  int     iValue;               /* integer value */
  tdcpm_string_index   str_idx; /* string */

  /* We generate internally: */

  tdcpm_vect_node     *vect_node;

  tdcpm_var_name      *var;

  tdcpm_vect_var_names *vect_var;

  tdcpm_unit_index     unit;

  tdcpm_unit_build     unit_build;

  tdcpm_vect_units    *vect_unit;

  tdcpm_dbl_unit       dbl_unit;

  tdcpm_dbl_unit_build dbl_unit_build;

  tdcpm_vect_dbl_units *vect_dbl_unit;

  tdcpm_vect_table_lines *vect_table_line;

  tdcpm_table            *table;

  tdcpm_tspec_index    tspec;
};

/* Things we get from the lexer */

%token <iValue>    INTEGER
%token <fValue>    DOUBLE
%token <str_idx>   STRING
%token <str_idx>   IDENTIFIER
%token <dbl_unit_build>  VALUE_UNIT
%token <dbl_unit_build>  UNIT

%token KW_DEF
%token KW_DEF_UNIT
%token KW_VALID
%token KW_START
%token KW_END
%token KW_WR

/* Operands for simple calculations */

%left '+' '-'
%left '*' '/'
%nonassoc UMINUS

/* The statements (nodes) */

%type <vect_node>  pgm_stmt
%type <vect_node>  pgm_stmt_list
%type <vect_node>  stmt
%type <vect_node>  stmt_list

/* Compounds(?) */

%type <vect_node>  calib_param
%type <vect_node>  valid_range

%type <var>        var_or_name

%type <var>        var_ident
%type <var>        var_ident_rec

%type <var>        array_spec_list_null
%type <var>        array_spec_list
%type <var>        array_spec

%type <var>        array_index_list
%type <iValue>     array_index

/* These surely need help: */

%type <table>      value_table

%type <vect_var>   header_idents
%type <vect_var>   header_idents_list_null
%type <vect_var>   header_idents_list
%type <vect_var>   header_ident

%type <vect_dbl_unit>  header_units
%type <vect_dbl_unit>  header_units_list_null
%type <vect_dbl_unit>  header_units_list
%type <vect_dbl_unit>  header_unit

%type <vect_table_line>  value_vector_lines
%type <vect_table_line>  value_vector_line_comma
%type <vect_table_line>  value_vector_line

/* Time specifier. */

%type <tspec>  tspec

/* A vector of doubles. */

%type <dbl_unit_build> value
%type <dbl_unit_build> value_raw_unit

%type <dbl_unit_build> value_paren
%type <fValue>         value_raw

%type <vect_dbl_unit>  value_vector_list
%type <vect_dbl_unit>  value_vector_np
%type <vect_dbl_unit>  value_vector_np_single

%type <dbl_unit>       value_unit

%type <dbl_unit>       unit
%type <dbl_unit_build> unit_part
%type <dbl_unit_build> unit_part_many
%type <dbl_unit_build> unit_part_block

%%

/* This specifies an entire file */
program:
          pgm_stmt_list         { tdcpm_vect_nodes_all($1); }
        | /* NULL */
        ;

pgm_stmt_list:
          pgm_stmt               { $$ = $1; }
        | pgm_stmt_list pgm_stmt { $$ = tdcpm_vect_node_join($1,$2); }
        ;

/* The program (outer level) statements are separate, since
 * definitions (aliases) are not allowed inside sub-units.
 * (They short-cut into the lexer, so scope is not easily handled.)
 */
pgm_stmt:
          stmt                  { $$ = $1; }
        | var_def               { $$ = NULL; }
        | unit_def              { $$ = NULL; }
        ;

stmt_list:
          stmt                  { $$ = $1; }
        | stmt_list stmt        { $$ = tdcpm_vect_node_join($1,$2); }
        ;

/* Each statement is either an range specification or a parameter
 * specification.
 */
stmt:
          ';'                   { $$ = NULL; }
        | calib_param           { $$ = $1; }
        | valid_range           { $$ = $1; }
        ;

/* The empty semicolon above in stmt handles the case of optional
 * semicolons after the statements.
 */
calib_param:
          var_or_name '=' value_vector_np_single ';'
          {
	    $$ = tdcpm_node_new_vect($1, $3/*, TDCPM_TSPEC_TYPE_NONE*/);
          }
        | var_or_name '=' '{' value_vector_np '}'
          {
	    $$ = tdcpm_node_new_vect($1, $4/*, TDCPM_TSPEC_TYPE_NONE*/);
          }
        | var_or_name '=' value_table
          {
	    $$ = tdcpm_node_new_table($1, $3);
          }
        | var_or_name '=' '{' stmt_list '}'
	  {
	    $$ = tdcpm_node_new_sub_node($1, $4);
	  }
        ;

valid_range:
          KW_VALID '(' tspec ',' tspec ')' '{' stmt_list '}'
	  {
	    $$ = tdcpm_node_new_valid_range($3, $5, $8);
	  }
	;

/*******************************************************/

var_def:
          KW_DEF IDENTIFIER '=' value_unit ';'
	  {
	    tdcpm_dbl_unit_build dbl_unit;

	    dbl_unit._value = $4._value;
	    dbl_unit._unit_bld = *tdcpm_unit_table_get($4._unit_idx);
	    
	    tdcpm_insert_def_var($2, &dbl_unit, TDCPM_DEF_VAR_KIND_VAR);
	  }
        ;

unit_def:
          KW_DEF_UNIT IDENTIFIER '=' value_unit ';'
	  {
	    tdcpm_dbl_unit_build dbl_unit;

	    dbl_unit._value = $4._value;
	    dbl_unit._unit_bld = *tdcpm_unit_table_get($4._unit_idx);

	    if (_tdcpm_parse_catch_unitdef)
	      *_tdcpm_parse_catch_unitdef = dbl_unit;
	    else
	      tdcpm_insert_def_var($2, &dbl_unit, TDCPM_DEF_VAR_KIND_UNIT);
	  }
        | KW_DEF_UNIT IDENTIFIER '=' unit ';'
	  {
	    tdcpm_dbl_unit_build dbl_unit;

	    dbl_unit._value = $4._value;
	    dbl_unit._unit_bld = *tdcpm_unit_table_get($4._unit_idx);

	    if (_tdcpm_parse_catch_unitdef)
	      *_tdcpm_parse_catch_unitdef = dbl_unit;
	    else
	      tdcpm_insert_def_var($2, &dbl_unit, TDCPM_DEF_VAR_KIND_UNIT);
	  }
        ;

/*******************************************************/

tspec:
          KW_START  { $$ = tdcpm_tspec_fixed(TDCPM_TSPEC_TYPE_START); }
        | KW_END    { $$ = tdcpm_tspec_fixed(TDCPM_TSPEC_TYPE_END);   }
        | KW_WR INTEGER  { $$ = tdcpm_tspec_wr($2); }
        ;

/*******************************************************/

var_or_name:
          var_ident_rec { $$ = $1; }
        ;

var_ident_rec:
	  var_ident                    { $$ = $1; }
	| var_ident_rec '.' var_ident  { $$ = tdcpm_var_name_join($1, $3); }
	;

/* If no identifier, '.' can be used as placeholder, to continue indices. */
var_ident:
           '.'        array_spec_list      { $$ = $2; }
	 | IDENTIFIER array_spec_list_null
          {
	    tdcpm_var_name *tmp;
	    tmp = tdcpm_var_name_new();
	    tmp = tdcpm_var_name_name(tmp, $1);
	    $$ = tdcpm_var_name_join(tmp, $2);
	  }
	;

array_spec_list_null:
                                       { $$ = NULL; }
        | array_spec_list              { $$ = $1; }
        ;

array_spec_list:
          array_spec                   { $$ = $1; }
        | array_spec_list array_spec   { $$ = tdcpm_var_name_join($1, $2); }
	;

array_spec:
          '[' array_index_list ']'     { $$ = tdcpm_var_name_off($2, 0); }
        | '(' array_index_list ')'     { $$ = tdcpm_var_name_off($2, 1); }
        ;

/*******************************************************/

array_index_list:
          array_index
	  { $$ = tdcpm_var_name_index(tdcpm_var_name_new(),$1); }
        | array_index_list ',' array_index
	  { $$ = tdcpm_var_name_index($1, $3); }
	;

/* Array index values.  Simple calculations allowed. */
array_index:
          INTEGER                       { $$ = $1; }
        | '-' array_index %prec UMINUS  { $$ = -$2; }
        | '+' array_index %prec UMINUS  { $$ = $2; }
        | array_index '+' array_index   { $$ = $1 + $3; }
        | array_index '-' array_index   { $$ = $1 - $3; }
        | array_index '*' array_index   { $$ = $1 * $3; }
        | array_index '/' array_index
          {
	    if ($3 == 0.0)
	      yyerror("Warning: Array index division by zero.");
	    if ($1 % $3 != 0)
	      yyerror("Warning: Array index division gives "
		      "nonzero remainder.");
	    $$ = $1 / $3;
	  }
        | '(' array_index ')'           { $$ = $2; }
        ;

/*******************************************************/

value_table:
          header_idents
	  { $$ = tdcpm_table_new($1, NULL, NULL); }
        | header_idents              value_vector_lines
	  { $$ = tdcpm_table_new($1, NULL, $2); }
        | header_idents header_units value_vector_lines
          { $$ = tdcpm_table_new($1, $2, $3); }
        ;

/*******************************************************/

header_idents:
          '[' header_idents_list_null ']'  { $$ = $2; }
        ;

/* A rule for optional comma did not work (shift-reduce).
 * So instead duplicating the list rules such that a
 * bare trailing comma is accepted (discarded).
 */

header_idents_list_null:
                                           { $$ = NULL; }
        | ','                              { $$ = NULL; }
        | header_idents_list               { $$ = $1; }
        | header_idents_list ','           { $$ = $1; }
        ;

header_idents_list:
          header_ident                     { $$ = $1;}
        | header_idents_list ',' header_ident
	  { $$ = tdcpm_vect_var_names_join($1, $3); }
	;

header_ident:
          var_or_name
	  { $$ = tdcpm_vect_var_names_new($1, TDCPM_TSPEC_TYPE_NONE); }
        | var_or_name '@' tspec
	  { $$ = tdcpm_vect_var_names_new($1, $3); }
        ;

/*******************************************************/

/* Double '[[' since we otherwise are ambiguous with an index [i]:
 * before a value line, if we are to allow units preceeded by values.
 * (Reduce-reduce conflict.)
 */
header_units:
          '[' '[' header_units_list_null ']' ']'   { $$ = $3; }
        ;

header_units_list_null:
                                           { $$ = NULL; }
        | ','                              { $$ = NULL; }
        | header_units_list                { $$ = $1; }
        | header_units_list ','            { $$ = $1; }
        ;

header_units_list:
          header_unit                      { $$ = $1; }
        | header_units_list ',' header_unit
	  { $$ = tdcpm_vect_dbl_units_join($1, $3); }
	;

header_unit:
          unit
	  { $$ = tdcpm_vect_dbl_units_new($1, TDCPM_TSPEC_TYPE_NONE); }
        | value_unit
	  { $$ = tdcpm_vect_dbl_units_new($1, TDCPM_TSPEC_TYPE_NONE); }
        ;

/*******************************************************/

value_vector_lines:
          value_vector_line_comma              { $$ = $1; }
        | value_vector_lines value_vector_line_comma
          { tdcpm_vect_table_lines_join($1, $2); }
        ;

value_vector_line_comma:
          value_vector_line                { $$ = $1; }
        | value_vector_line ','            { $$ = $1; }
        ;

value_vector_line:
          '{' value_vector_list '}' { $$ = tdcpm_vect_table_line_new(NULL,$2);}
        | array_spec_list ':'
	  '{' value_vector_list '}' { $$ = tdcpm_vect_table_line_new($1, $4); }
        ;

/*******************************************************/

value_vector_list:
                                           { $$ = NULL; }
        | ','                              { $$ = NULL; }
        | value_vector_np                  { $$ = $1; }
        | value_vector_np ','              { $$ = $1; }
        ;

/* A vector of floating point values (with units). */
value_vector_np:
	  value_vector_np_single  { $$ = $1; }
	| value_vector_np ',' value_vector_np_single
	  { $$ = tdcpm_vect_dbl_units_join($1, $3); }
        ;

value_vector_np_single:
          value_unit
	  { $$ = tdcpm_vect_dbl_units_new($1, TDCPM_TSPEC_TYPE_NONE); }
        | value_unit '@' tspec
	  { $$ = tdcpm_vect_dbl_units_new($1, $3);}
	;

value_unit:
          value
	  {
	    $$._value = $1._value;
	    $$._unit_idx = tdcpm_unit_make_full(&$1._unit_bld, 1);
	  }
        ;

/* Floating point values.  Simple calculations allowed. */
/* Note: unit after the LHS value of multiplication or division is not
 * allowed, since we get a shift-reduce conflict with the
 * multiplication in the unit.  Use parenthesis or place total unit
 * after the RHS.
 */
/* Generally, we would like to handle 'value' as part of the
 * expressions, as that is the most generic.  When that is not
 * possible (shift-reduce), we accept either a raw value, or value
 * within parenthesis.
 */
/* For some reason, 'value_raw_paren '/' value' led to a shift-reduce
 * conflict, but stating the rules explicitly works.
 */
/* TODO TODO TODO TODO TODO TODO TODO TODO TODO
 *
 * Check very carefully that multiplication and division
 * has correct precedence.  Also related to addition and subtraction
 * and related to other operations, since we had to use not 'value'
 * but more basic types!
 *
 * TODO TODO TODO TODO TODO TODO TODO TODO TODO
 */
value:
	  value_raw_unit               { $$ = $1;      }
        | '-' value %prec UMINUS  { $$._value    = -$2._value;
	                            $$._unit_bld = $2._unit_bld; }
        | '+' value %prec UMINUS  { $$ = $2;      }
        | value '+' value
	  {
	    double factor;
	    if (!tdcpm_unit_build_factor(&$1._unit_bld, &$3._unit_bld,
					 &factor))
	      yyerror("Error: cannot add, unit mismatch between terms.");
	    $$._value = $1._value + factor * $3._value;
	    $$._unit_bld = $1._unit_bld;
	    /*printf ("*** %.2f + %.2f -> %.2f\n",
	      $1._value,$3._value,$$._value);*/
	  }
        | value '-' value
	  {
	    double factor;
	    if (!tdcpm_unit_build_factor(&$1._unit_bld, &$3._unit_bld,
					 &factor))
	      yyerror("Error: cannot subtract, unit mismatch between terms.");
	    $$._value = $1._value + factor * $3._value;
	    $$._unit_bld = $1._unit_bld;
	    /*printf ("*** %.2f - %.2f -> %.2f\n",
	      $1._value,$3._value,$$._value);*/
          }
        | value '*' value
	  {
	    $$._value = $1._value * $3._value;
	    tdcpm_unit_build_mul(&$$._unit_bld,&$1._unit_bld,&$3._unit_bld,1);
	    /*printf ("*** %.2f * %.2f -> %.2f\n",
	      $1._value,$3._value,$$._value);*/
          }
        | value '*' unit_part_many
	  {
	    $$._value = $1._value * $3._value;
	    tdcpm_unit_build_mul(&$$._unit_bld,&$1._unit_bld,&$3._unit_bld,1);
	    /*printf ("*** %.2f * unit -> %.2f\n",
	      $1._value,$$._value);*/
          }
        | value  '/' value
          {
	    if ($3._value == 0.0)
	      yyerror("Warning: Division by zero.");
	    $$._value = $1._value / $3._value;
	    tdcpm_unit_build_mul(&$$._unit_bld,&$1._unit_bld,&$3._unit_bld,-1);
	    /*printf ("*** %.2f / %.2f -> %.2f\n",
	      $1._value,$3._value,$$._value);*/
	  }
        | value  '/' unit_part_many
          {
	    $$._value = $1._value / $3._value;
	    tdcpm_unit_build_mul(&$$._unit_bld,&$1._unit_bld,&$3._unit_bld,-1);
	    /*printf ("*** %.2f / unit -> %.2f\n",
	      $1._value,$$._value);*/
	  }
        | value_paren             { $$ = $1; }
        ;

/* A complete expression in parenthesis. */
value_paren:
          '(' value ')'           { $$ = $2; }
        ;

value_raw_unit:
          value_raw
	  {
	    $$._value = $1;
	    $$._unit_bld = _tdcpm_unit_build_none;
	  }
        | value_raw unit_part_many
	  {
	    $$._value = $1 * $2._value;
	    $$._unit_bld = $2._unit_bld;
	  }	
        | VALUE_UNIT              { $$ = $1; }
        ;

/* A raw value itself, i.e. a floatig-point value. */
value_raw:
          DOUBLE                  { $$ = $1; }
        | INTEGER                 { $$ = $1; }
        ;

/*******************************************************/

/* Specifying a unit. */
unit:
	  STRING
	  {
	    tdcpm_unit_new_dissect(&$$, $1);
	  }
        | unit_part
	  {
	    $$._value = $1._value;
	    $$._unit_idx = tdcpm_unit_make_full(&$1._unit_bld, 1);
	  }
        | '/' unit_part
	  {
	    tdcpm_unit_build tmp;
	    $$._value = 1. / $2._value;
	    tdcpm_unit_build_mul(&tmp,
				 &_tdcpm_unit_build_none,&$2._unit_bld,-1);
	    $$._unit_idx = tdcpm_unit_make_full(&tmp, 1);
	  }
        ;

unit_part:
	  unit_part_many                 { $$ = $1; }
	| unit_part '*' unit_part
	  {
	    $$._value = $1._value * $3._value;
	    tdcpm_unit_build_mul(&$$._unit_bld,&$1._unit_bld,&$3._unit_bld,1);
	  }
	| unit_part '/' unit_part
	  {
	    $$._value = $1._value / $3._value;
	    tdcpm_unit_build_mul(&$$._unit_bld,&$1._unit_bld,&$3._unit_bld,-1);
	  }
        ;

unit_part_many:
	  unit_part_block                { $$ = $1; }
	| unit_part_many unit_part_block
	  {
	    $$._value = $1._value * $2._value;
	    tdcpm_unit_build_mul(&$$._unit_bld,&$1._unit_bld,&$2._unit_bld,1);
	  }
        ;

/* As a courtesy we allow e.g. 'ns 2' instead of 'ns^2'.
 * (Is that really helpful, or just confusing?)
 * We cannot handle 'ns-2', since it causes a shift-reduce conflict.
 */
unit_part_block:
          IDENTIFIER
	  { $$._value = 1; tdcpm_unit_build_new(&$$._unit_bld,$1,1); }
        | IDENTIFIER INTEGER
	  { $$._value = 1; tdcpm_unit_build_new(&$$._unit_bld,$1,$2); }
     /* | IDENTIFIER '-' INTEGER
          { $$._value = 1; tdcpm_unit_build_new(&$$._unit_bld,$1,-$3); } */
        | IDENTIFIER '^' INTEGER
	  { $$._value = 1; tdcpm_unit_build_new(&$$._unit_bld,$1,$3); }
        | IDENTIFIER '^' '-' INTEGER
	  { $$._value = 1; tdcpm_unit_build_new(&$$._unit_bld,$1,-$4); }
        | UNIT
	  { $$ = $1; }
        ;

/*******************************************************/

%%

void yyerror(const char *s)
{
  const char *file;
  int line;
  
  tdcpm_lineno_get(yylineno, &file, &line);
  fprintf(stderr,"%s:%d: %s\n", file, line, s);
/*
  Current.first_line   = Rhs[1].first_line;
  Current.first_column = Rhs[1].first_column;
  Current.last_line    = Rhs[N].last_line;
  Current.last_column  = Rhs[N].last_column;
*/
  /*throw error();*/
  exit(1);
}

int parse_definitions()
{
  /* yylloc.first_line = yylloc.last_line = 1; */
  /* yylloc.first_column = yylloc.last_column = 0; */

  return yyparse() == 0;
}
