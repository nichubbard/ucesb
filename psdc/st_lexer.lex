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
#include "c_struct.hh"
#include "file_line.hh"
#include "node.hh"
#include "str_set.hh"

struct md_ident_fl;

#include "generated/st_parser.hh"
#include "file_line.hh"

void yyerror(const char *);

ssize_t lexer_read(char* buf,size_t max_size);

#define LINENO_MAP_HAS_NEXT 1
#define INF_NAN_ATOF        1

#define LEXER_LINENO(yylineno, file, sz_file, line)	\
  insert_lineno_map(yylineno, file, sz_file, line)
 
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

"struct"      { return STRUCT; }
"union"       { return UNION; }
"class"       { return CLASS; }
"public"      { return PUBLIC; }
"__MULTI__"   { return MULTI; }
"UNIT"        { return UNIT; }
"TOGGLE"      { return TOGGLE; }

[_a-zA-Z][_a-zA-Z0-9]* {
                yylval.strValue = find_str_identifiers(yytext);
                return IDENTIFIER;
            }

[-+*/;(){},:\|\[\]<>=\.] {
                return *yytext;
            }

\"[^\"\n\0]*\" { /* Cannot handle \" inside strings. */
                yylval.strValue = find_str_strings(yytext+1,strlen(yytext)-2);
                return STRING;
            }

 /******************************************************************/
 /* BEGIN_INCLUDE_FILE "../lu_common/lexer_rules_whitespace_lineno.lex" */
 /* MD5SUM_INCLUDE 4ead390c1ed21ec1efecbe3cb5d19a36 */
 /* Lexer rules to eat and discard whitespace (space, tab, newline)
  * Complain at finding an unrecognized character
  * Handle line number information
  *
  * These would usually be among the last rules
  */

[ \t\n]+    ;       /* ignore whitespace */

.           {
	      char str[64];
	      snprintf (str, sizeof(str), "Unknown character: '%s'.", yytext);
	      yyerror(str);
            }

"# "[0-9]+" \"".+"\""[ 0-9]*\n {  /* Information about the source location. */
	      int line = 0;
	      char *endptr;
	      const char *endfile;
              const char *file = NULL;
	      size_t sz_file = 0;

	      /*yylloc.last_line++;*/

	      line = (int)(strtol(yytext+2,&endptr,10));

	      endfile = strchr(endptr+2,'\"');
	      if (endfile)
		{
		  file = endptr+2;
		  sz_file = endfile - file;
		}

	      LEXER_LINENO(yylineno, file, sz_file, line);

#if SHOW_FILE_LINENO
	      /* fprintf(stderr,"Now at %s:%d (%d)\n",file,line,yylineno); */
	      /* INFO(0,"Now at %s:%d",file,line); */
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
