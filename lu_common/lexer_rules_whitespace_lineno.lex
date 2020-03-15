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

