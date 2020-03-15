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

/* This file contains the lexical analyser for
 * cabling documentation files.
 *
 * With the help of flex(lex), a C lexer is produced.
 *
 * The perser.y contain the grammar which will read
 * the tokens that we produce.
 *
 * Don't be afraid.  Just look below and you will figure
 * out what is going on.
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "definitions.hh"
#include "str_set.hh"

struct md_ident_fl;

#include "generated/parser.hh"
#include "file_line.hh"

void yyerror(const char *);

ssize_t lexer_read(char* buf,size_t max_size);

#define LINENO_MAP_HAS_NEXT 1
#define INF_NAN_ATOF        1

#define YY_INPUT(buf,result,max_size) \
{ \
  result = lexer_read(buf,max_size); \
  if (!result) result = YY_NULL; \
}

%}

%option yylineno

%%

[0-9]+      {
                yylval.iValue = atoi(yytext);
                return INTEGER;
            }

0x[0-9a-fA-F]+ {
                yylval.iValue = strtoul(yytext+2,NULL,16);
                return INTEGER;
            }

0b[01]+     {
                yylval.iValue = strtoul(yytext+2,NULL,2);
                return INTEGER;
            }

 /******************************************************************/
 /* BEGIN_INCLUDE_FILE "../lu_common/lexer_rules_double.lex" */
 /* MD5SUM_INCLUDE b467a933b92a527b77ec8050a260efa3 */
 /* Lexer rules to recognize a floating point value
  *
  * These would usually be among the first rules
  */

[Nn][Aa][Nn] {
#if INF_NAN_ATOF
		yylval.fValue = atof(yytext);
#else
		yylval.fValue = NAN ;
#endif
		return DOUBLE;
            }

[Ii][Nn][Ff] {
#if INF_NAN_ATOF
		yylval.fValue = atof(yytext);
#else
		yylval.fValue = INFINITY ;
#endif
		return DOUBLE;
            }

[0-9]+"."[0-9]*([eE][+-]?[0-9]+)? {
		yylval.fValue = atof(yytext);
                return DOUBLE;
            }
"."[0-9]+([eE][+-]?[0-9]+)? {
		yylval.fValue = atof(yytext);
                return DOUBLE;
            }
 /* END_INCLUDE_FILE "../lu_common/lexer_rules_double.lex" */
 /******************************************************************/

[0-9]+_[0-9]+ {
                char *end;
                yylval.iRange._min = strtol(yytext,&end,10);
		if (*end != '_') yyerror("Internal error: INTEGER_RANGE.");
                yylval.iRange._max = atoi(end+1);
		if (yylval.iRange._min > yylval.iRange._max) yyerror("Inversed range.");
                return INTEGER_RANGE;
            }

"EVENT"       { return EVENT; }
"STICKY_EVENT" { return STICKY_EVENT; }
"SUBEVENT"    { return SUBEVENT; }
"SIGNAL"      { return SIGNAL; }
"STICKY"      { return STICKY; }
"TOGGLE"      { return TOGGLE; }
"UINT64"      { return UINT64; }
"UINT32"      { return UINT32; }
"UINT16"      { return UINT16; }
"UINT8"       { return UINT8; }
"if"          { return T_IF; }
"else"        { return T_ELSE; }
"list"        { return T_LIST; }
"several"     { return T_SEVERAL; }
"optional"    { return T_OPTIONAL; }
"select"      { return T_SELECT; }
"multi"       { return T_MULTI; }
"external"    { return T_EXTERNAL; }
"norevisit"   { return T_NOREVISIT; }
"revisit"     { return T_REVISIT; }
"ignore_unknown_subevent" { return T_IGNORE_UNKNOWN_SUBEVENT; }
"static_cast" { return T_STATIC_CAST; }
"MATCH"       { return MATCH; }
"CHECK"       { return CHECK; }
"RANGE"       { return RANGE; }
"ENCODE"      { return ENCODE; }
"NOENCODE"    { return NOENCODE; }
"EXTERNAL"    { return EXTERNAL; }
"MEMBER"      { return MEMBER; }
"ZERO_SUPPRESS" { return ZERO_SUPPRESS; }
"ZERO_SUPPRESS_LIST" { return ZERO_SUPPRESS_LIST; }
"ZERO_SUPPRESS_MULTI" { return ZERO_SUPPRESS_MULTI; }
"NO_INDEX_LIST" { return NO_INDEX_LIST; }
"APPEND_LIST" { return APPEND_LIST; }
"FIRST_EVENT" { return FIRST_EVENT; }
"LAST_EVENT"  { return LAST_EVENT; }
"MARK"        { return MARK; }
"MARK_COUNT"  { return MARK_COUNT; }
"CHECK_COUNT" { return CHECK_COUNT; }
"MATCH_END"   { return MATCH_END; }

[_a-zA-Z][_a-zA-Z0-9]* {
                yylval.strValue = find_str_identifiers(yytext);
                return IDENTIFIER;
            }

"||"        { return T_LOR; }
"&&"        { return T_LAND; }
"<<"        { return T_SHL; }
">>"        { return T_SHR; }
"<="        { return T_LE; }
">="        { return T_GE; }
"=="        { return T_EQ; }
"!="        { return T_NEQ; }

[-+*/;(){},:\|\[\]&<>=\.!%] {
                return *yytext;
            }

\"[^\"\n]*\" { /* Cannot handle \" inside strings. */
                yylval.strValue = find_str_strings(yytext+1,strlen(yytext)-2);
                return STRING;
            }

 /******************************************************************/
 /* BEGIN_INCLUDE_FILE "../lu_common/lexer_rules_whitespace_lineno.lex" */
 /* MD5SUM_INCLUDE 06e1db2353eec1157e20fec02ad0ed49 */
 /* Lexer rules to eat and discard whitespace (space, tab, newline)
  * Complain at finding an unrecognized character
  * Handle line number information
  *
  * These would usually be among the last rules
  */

[ \t\n]+    ;       /* ignore whitespace */

.           {
	      char str[64];
	      sprintf (str,"Unknown character: '%s'.",yytext);
	      yyerror(str);
            }

"# "[0-9]+" \"".+"\""[ 0-9]*\n {  /* Information about the source location. */
              char file[1024] = "\0";
	      int line = 0;
	      char *endptr;
	      char *endfile;

	      /*yylloc.last_line++;*/

	      line = (int)(strtol(yytext+2,&endptr,10));

	      endfile = strchr(endptr+2,'\"');
	      if (endfile)
		strncpy(file,endptr+2,(size_t) (endfile-(endptr+2)));
	      else
		strcpy(file,"UNKNOWN");

	      // fprintf(stderr,"Now at %s:%d (%d)\n",file,line,yylineno);

	      lineno_map *map = new lineno_map;

	      map->_internal = yylineno;
	      map->_line = line;
	      map->_file = strdup(file);
	      map->_prev = _last_lineno_map;
#if LINENO_MAP_HAS_NEXT
	      map->_next = NULL;

	      if (!_first_lineno_map)
		_first_lineno_map = map;
	      else
		_last_lineno_map->_next = map;
#endif
	      _last_lineno_map = map;
#if SHOW_FILE_LINENO
	      // INFO(0,"Now at %s:%d",file,line);
#endif
	    }

 /* END_INCLUDE_FILE "../lu_common/lexer_rules_whitespace_lineno.lex" */
 /******************************************************************/

%%

int yywrap(void) {
    return 1;
}

int lexer_read_fd = -1;

ssize_t lexer_read(char* buf,size_t max_size)
{
  ssize_t n;

  for ( ; ; )
    {
      n = read(lexer_read_fd,buf,max_size);

      if (n == -1)
	{
	  if (errno == EINTR)
	    continue;
	  fprintf(stderr,"Failure reading from lexer pipe\n");
	}
      return n;
    }
}
