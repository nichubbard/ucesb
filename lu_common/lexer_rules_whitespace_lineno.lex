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

	      LEXER_LINENO(yylineno, file, line);

#if SHOW_FILE_LINENO
	      /* fprintf(stderr,"Now at %s:%d (%d)\n",file,line,yylineno); */
	      /* INFO(0,"Now at %s:%d",file,line); */
#endif
	    }

