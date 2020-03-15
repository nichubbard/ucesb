FLEX_PATCH_PIPELINE=\
	sed -e "s/typedef unsigned char YY_CHAR/typedef int YY_CHAR/" \
	  -e "s/int yyleng;/size_t yyleng;/" \
	  -e "s/int yyget_leng/size_t yyget_leng/" \
	  -e "s/int yyl;/size_t yyl;/" \
	  -e "s/int yy_n_chars;/yy_size_t yy_n_chars;/" \
	  -e "s/int yy_buf_size;/yy_size_t yy_buf_size;/" \
	  -e "s/int number_to_move/yy_size_t number_to_move/" \
	  -e "s/(int) ((yy_n_chars) + number_to_move) >/((yy_n_chars) + number_to_move) >/" \
	  -e "s/int num_to_alloc;/size_t num_to_alloc;/" \
	  -e "s/int num_to_read =/size_t num_to_read =/" \
	  -e "s/int new_size =/size_t new_size =/" \
	  -e "s/int offset = \(.*\);/size_t offset = (size_t) (\\1);/" \
	  -e "s/yyleng = (int) \(.*\);/yyleng = (size_t) (\\1);/" \
	  -e "s/yy_buf_size = (int)/yy_buf_size = (size_t)/" \
	  -e "s/int grow_size =/size_t grow_size =/" \
	  -e "s/register int number_to_move, i;/yy_size_t number_to_move, i;/" \
	  -e "s/register int number_to_move =/yy_size_t number_to_move =/" \
	  -e "s/number_to_move = (int) \(.*\);/number_to_move = (yy_size_t)(\\1);/" \
	  -e "s/yy_scan_bytes\(.*\)int /yy_scan_bytes\\1size_t /" \
	  -e "s/return yyleng/return (int) yyleng/" \
	  -e "s/i < _yybytes_len/(size_t) i < (size_t) _yybytes_len/" \
	  -e "s/; i < len;/; (size_t) i < (size_t) len;/" \
	  -e "s/ECHO fwrite( yytext, yyleng, 1, yyout )/ECHO do { if (fwrite( yytext, yyleng, 1, yyout )) {} } while (0)/" \
	  -e "s/yy_nxt\[yy_base\[\(.*\)(unsigned int) /yy_nxt[(unsigned int) yy_base[\\1(unsigned int) /" \
	  -e "s/yy_create_buffer\(.*\),\(.*\)int\(.*\)size/yy_create_buffer\\1,\\2yy_size_t\\3size/" \
	  -e "s/scan_bytes(\(.*\), *len *);/scan_bytes( \\1, (size_t) len );/" \
	  -e "s/scan_bytes(\(.*\), *(int) *strlen\(.*\));/scan_bytes( \\1, strlen \\2);/" \
	  -e "s/\(\s*\)\(.*\) offset = \(.*\);/\\1\\2 offset = (\\2) (\\3);/" \
	  -e "s/return (int) \(.*\);/return \\1;/" \
	  -e "s/find_rule:/goto find_rule; find_rule:/" \
# intentionally empty

# Previous (non-TRLO II)
#	  -e "s/int offset =/size_t offset =/"
#	  -e "s/int number_to_move =/int number_to_move = (int)/"
#	  -e "s/i < _yybytes_len/i < (int) _yybytes_len/"
