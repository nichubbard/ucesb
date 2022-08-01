BISON_PATCH_PIPELINE=\
	sed -e "s/YYSIZE_T yysize = yyssp - yyss + 1/YYSIZE_T yysize = (YYSIZE_T) (yyssp - yyss + 1)/" \
	    -e "s/yystpcpy (yyres, yystr) - yyres/(YYSIZE_T) (yystpcpy (yyres, yystr) - yyres)/" \
	    -e "/YYPTRDIFF_T/,/quee3oNa/s/(YYSIZE_T) (yystpcpy (yyres, yystr) - yyres)/(YYPTRDIFF_T) (yystpcpy (yyres, yystr) - yyres)/" \
	    -e "s/(sizeof(\(.*\)) \/ sizeof(char \*))/(int) (sizeof(\\1) \/ sizeof(char *))/" \
	    -e "s/int \(.*\)char1;/int \\1char1 = 0;/" \
# intentionally empty

# The yystpcpy is a bit difficult.  For newer versions (which have
# it), we want to cast to YYPTRDIFF_T, while for older we want
# to cast to YYPTRDIFF_T.  Therefore we have to expressions, both
# preceeded by an address range: /startregex/,/stopregex/.
# The address range is used to tell if YYPTRDIFF_T at all has appeared
# in the file or not.  If yes, then change to YYPTRDIFF_T, otherwise use
# YYSIZE_T.  The pattern /quee3oNa/ is something which would not be
# found, so end of file.
