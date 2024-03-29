/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "markconvbold.h"
#include "colourtext.h"

#include <stdio.h>

#include <regex.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <assert.h>

regex_t *markconvbold_compile()
{
  regex_t *regex;

  regex = (regex_t *) malloc(sizeof (regex_t));

  if (!regex)
    {
      fprintf(stderr,"Regex allocation failure!");
      exit (1);
    }

  /* printf ("Compiling...\n"); */

#define REGEXP_NOBOLD_CONVERSION	 	        \
  "(" ERR_NOBOLD ")?"              /* no-bold mark 1 */ \
  "(0x)?"                          /* lead-hex     2 */ \
  "%"                              /* leading %      */ \
  "(%|"                            /* pure %       3 */ \
  "[-#0 \\+'I]*"                   /* flags          */ \
  "([1-9][0-9]*|\\*([0-9]+\\$)?)?" /* field width 45 */ \
  "(\\.[0-9]*|\\*([0-9]+\\$)?)?"   /* precision   67 */ \
  "([hl]{1,2}|[Lqjzt])?"           /* length       8 */ \
  "[diouxXeEfFgGaAcsCSpnm])"       /* conversion     */

#define REGEXP_NOBOLD_CONVERSION_MATCH_ENTIRE   0
#define REGEXP_NOBOLD_CONVERSION_MATCH_NOBOLD   1
#define REGEXP_NOBOLD_CONVERSION_MATCH_LEADHEX  2
#define REGEXP_NOBOLD_CONVERSION_MATCH_PERCENT  3
#define REGEXP_NOBOLD_CONVERSION_NUM_MATCH      4

  int ret = regcomp(regex,
		    REGEXP_NOBOLD_CONVERSION,
		    REG_EXTENDED);

  if (ret != 0)
    {
      size_t len = regerror(ret,regex,NULL,0);
      char errstr[len];
      regerror(ret,regex,errstr,sizeof(errstr));

      free (regex);

      fprintf(stderr,"Regex compilation failure!  (Error: %s)",errstr);
      exit (1);
    }

  return regex;
}

static regex_t *_markconvbold_regex =
#ifdef __cplusplus
  markconvbold_compile()
  #else
  NULL /* Note: must call markconvbold_init() ! */
#endif
  ;

void markconvbold_init(void)
{
  if (_markconvbold_regex)
    return;

  _markconvbold_regex = markconvbold_compile();
}

char *markconvbold(const char *fmt)
{
  regmatch_t pmatch[REGEXP_NOBOLD_CONVERSION_NUM_MATCH];

  int ret;

  ret = regexec(_markconvbold_regex,fmt,
		REGEXP_NOBOLD_CONVERSION_NUM_MATCH,pmatch,
		0);

  if (ret == REG_NOMATCH)
    return NULL;

  /* We need to allocate a temporary buffer.  We will not add more
   * than at most 4 characters per % found, so do a counting.
   */

  size_t percent_count = 0;

  const char *cur = fmt;

  for ( ; ; percent_count++)
    {
      const char *percent = strchr(cur,'%');

      if (percent)
	cur = percent + 1;
      else
	break;
    }

  size_t len = strlen(cur) + (size_t) (cur - fmt);

  /* printf ("Length %d, percent %d: '%s'\n",(int) len,percent_count,fmt); */

  size_t newsize = len + 1 + 4 * percent_count;

  char *newfmt = (char *) malloc(newsize);
  char *endfmt = newfmt;

  const char *curfmt = fmt;

  while (ret != REG_NOMATCH)
    {
      /*
      printf ("{%d,%d} {%d,%d} {%d,%d} '%c'\n",
	      pmatch[0].rm_so,pmatch[0].rm_eo,
	      pmatch[1].rm_so,pmatch[1].rm_eo,
	      pmatch[2].rm_so,pmatch[2].rm_eo,curfmt[pmatch[2].rm_so]);
      */

      /* if the match is just a percent?
       * (we must handle them, so that the matcher does not do a false
       * start after a %%)
       */

      int dobold = 1;

      if (pmatch[REGEXP_NOBOLD_CONVERSION_MATCH_PERCENT].rm_so != -1 &&
	  curfmt[pmatch[REGEXP_NOBOLD_CONVERSION_MATCH_PERCENT].rm_so] == '%')
	dobold = 0;

      /* copy up to the match (destroys curfmt <-> rm_so relationship) */

      memcpy(endfmt,curfmt,(size_t) pmatch[0].rm_so);
      curfmt += pmatch[0].rm_so;
      endfmt += pmatch[0].rm_so;

      regoff_t match_length = pmatch[0].rm_eo - pmatch[0].rm_so;

      if (pmatch[REGEXP_NOBOLD_CONVERSION_MATCH_NOBOLD].rm_so != -1)
	{
	  curfmt += strlen(ERR_NOBOLD);
	  match_length -= (regoff_t) strlen(ERR_NOBOLD);
	  dobold = 0;
	}

      if (dobold)
	{
	  memcpy(endfmt,ERR_BOLD,strlen(ERR_BOLD));
	  endfmt += strlen(ERR_BOLD);
	}

      memcpy(endfmt,curfmt,(size_t) match_length);
      curfmt += match_length;
      endfmt += match_length;
      if (dobold)
	{
	  memcpy(endfmt,ERR_ENDBOLD,strlen(ERR_ENDBOLD));
	  endfmt += strlen(ERR_ENDBOLD);
	}

      ret = regexec(_markconvbold_regex,curfmt,
		    REGEXP_NOBOLD_CONVERSION_NUM_MATCH,pmatch,
		    0);
  }

  strcpy(endfmt,curfmt);

  return newfmt;
}

void markconvbold_output(const char *buf,int linemarkup)
{
  const char *ctext = "";
  const char *ctextback = "";

  if (linemarkup == CTR_WHITE_BG_RED)
    {
      ctext = CT_ERR(WHITE_BG_RED);
      ctextback = CT_ERR(DEF_COL);
    }
  else if (linemarkup == CTR_BLACK_BG_YELLOW)
    {
      ctext = CT_ERR(BLACK_BG_YELLOW);
      ctextback = CT_ERR(DEF_COL);
    }
  else
    assert (linemarkup == CTR_NONE);

  const char *escape = strchr(buf,'\033');

  if (!escape)
    fprintf (stderr,"%s%s%s\n",ctext,buf,ctextback);
  else
    {
      fprintf (stderr,"%s",ctext);

      const char *curbuf = buf;

      do
	{
	  const char *ctext_esc = "";
	  const char *ctext_repeat = "";
	  size_t eat = 2;

	  switch (escape[1])
	    {
	    case 'A':
	      ctext_esc = CT_ERR(BOLD);
	      break;
	    case 'B':
	      ctext_esc = CT_ERR(NORM);
	      ctext_repeat = ctext; /* in case NORM switches colours off */
	      break;
	    case 'C':
	      ctext_esc = CT_ERR(RED);
	      break;
	    case 'D':
	      ctext_esc = CT_ERR(GREEN);
	      break;
	    case 'E':
	      ctext_esc = CT_ERR(BLUE);
	      break;
	    case 'F':
	      ctext_esc = CT_ERR(MAGENTA);
	      break;
	    case 'G':
	      ctext_esc = CT_ERR(CYAN);
	      break;
	    case 'H':
	      ctext_esc = CT_ERR(BLACK);
	      break;
	    case 'I':
	      ctext_esc = CT_ERR(WHITE);
	      break;
	    case 'J':
	      if (linemarkup == CTR_WHITE_BG_RED)
		ctext_esc = CT_ERR(WHITE);
	      else if (linemarkup == CTR_BLACK_BG_YELLOW)
		ctext_esc = CT_ERR(BLACK);
	      else
		ctext_esc = CT_ERR(DEF_COL);
	      break;
	 /* case 'K': ERR_NOBOLD, handled in REGEXP_NOBOLD_CONVERSION */
	    default:
	      ctext_esc = "\033";
	      eat = 1;
	      break;
	    }

	  fprintf (stderr,"%.*s%s%s",
		   (int) (escape - curbuf),curbuf,
		   ctext_esc,ctext_repeat);

	  curbuf = escape + eat;

	  escape = strchr(curbuf,'\033');
	}
      while (escape);

      fprintf (stderr,"%s%s\n",curbuf,ctextback);
    }
}
