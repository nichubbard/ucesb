 /* Lexer rules to recognize a string
  *
  * These would usually be towards the end
  */

\"[^\"\n\0]*\" { /* Cannot handle \" inside strings. */
                /*yylval.strValue = new std::string(yytext);*/
                yylval.strValue = strndup(yytext+1,strlen(yytext)-2);
                return STRING;
            }

