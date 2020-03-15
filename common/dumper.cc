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

#include "dumper.hh"
#include "parse_error.hh"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

dumper_dest_file::dumper_dest_file(FILE *fid)
{
  _fid = fid;
}

void dumper_dest_file::put(char c)
{
  fputc(c,_fid);
}

void dumper_dest_file::write(const char* p,size_t len)
{
  if (fwrite(p,1,len,_fid) != len)
    ERROR("Failure writing to file.");
}


dumper_dest_memory::dumper_dest_memory()
{
  _start = _end = _cur = NULL;
}

dumper_dest_memory::~dumper_dest_memory()
{
  free(_start);
}

void dumper_dest_memory::write(const char* p,size_t len)
{
  if ((size_t) (_end - _cur) < len)
    {
      size_t newlen = (size_t) (_end - _start) * 2 + len;
      char *new_start = (char*) realloc(_start,newlen);
      if (!new_start)
	ERROR("Memory allocation error.\n");
      // printf ("Reallocated: %d -> %d\n",_end - _start,newlen);

      _cur += new_start - _start;
      _start = new_start;
      _end = new_start + newlen;
    }

  memcpy(_cur,p,len);
  _cur += len;
}

char *dumper_dest_memory::get_string()
{
  // Make sure we are null terminated

  if (_cur == _end)
    write("",1); // reallocate and write a 0
  _cur = 0; // write a 0
  return _start;
}

void dumper_dest_memory::fwrite(FILE* fid)
{
  size_t len = (size_t) (_cur - _start);

  if (len)
    if (::fwrite(_start,1,len,fid) != len)
      ERROR("Failure writing to file.");
}

dumper::dumper(dumper_dest *dest)
{
  _indent = 0;
  _current = 0;
  _dest = dest;
  _comment = false;
  _comment_written = false;
}

#define __max(a,b) ((a)>(b)?(a):(b))

dumper::dumper(dumper &src,int more,bool comment)
{
  _current = src._current;
  _indent = __max(src._current,src._indent) + (size_t) more;
  _dest = src._dest;
  _comment = comment || src._comment;
  _comment_written = src._comment_written;
}

void dumper::nl()
{
  _dest->put('\n');
  _current = 0;
  _comment_written = false;
}

void dumper::indent()
{
  if (_current < _indent)
    {
      for (ssize_t i = (ssize_t) (_indent - _current); i; i--)
	_dest->put(' ');
      _current = _indent;
    }
}

#define DUMPER_MAX_LINE_LENGTH 72

void dumper::text(const char *str,bool optional_newline)
{
  assert(str);

  indent();

  size_t len = strlen(str);

  if (_current > _indent &&
      _current + len > DUMPER_MAX_LINE_LENGTH &&
      optional_newline)
    {
      nl();
      indent();
    }

  if (_comment && !_comment_written)
    {
      _comment_written = true;
      _dest->write("// ",3);
    }

  if (len > 0 && str[len-1] == '\n')
    {
      _dest->write(str,len-1);
      _current += len;
      nl();
    }
  else
    {
      _dest->write(str,len);
      _current += len;
    }
}

void dumper::col0_text(const char *str)
{
  assert(str);

  if (_current > 0)
    nl();

  if (_comment && !_comment_written)
    {
      _comment_written = true;
      _dest->write("// ",3);
    }

  size_t len = strlen(str);

  if (len > 0 && str[len-1] == '\n')
    {
      _dest->write(str,len-1);
      _current += len;
      nl();
    }
  else
    {
      _dest->write(str,len);
      _current += len;
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

char *make_message(const char *fmt,va_list ap)
{
  /* Guess we need no more than 100 bytes. */
  int n;
  size_t size = 1 + strlen(fmt);
  char *p;

  if ((p = (char*) malloc (size)) == NULL)
    return NULL;
  while (1) {
    /* We must make a copy, since amd64 (and ppc?)
     * choke on multiple traversals
     */
    va_list aq;
#ifdef va_copy
    va_copy(aq,ap);
#else
    __va_copy(aq,ap);
#endif
    /* Try to print in the allocated space. */
    n = vsnprintf (p, size, fmt, aq);
    va_end(aq);
    /* If that worked, return the string. */
    if (n > -1 && (size_t) n < size)
      return p;
    /* Else try again with more space. */
    if (n > -1)    /* glibc 2.1 */
      size = (size_t) n+1; /* precisely what is needed */
    else           /* glibc 2.0 */
      size *= 2;  /* twice the old size */
    if ((p = (char*) realloc (p, size)) == NULL)
      return NULL;
  }
}

void dumper::text_fmt(const char *fmt,...)
{
  va_list ap;
  char *p;

  assert(fmt);

  va_start(ap, fmt);
  p = make_message(fmt,ap);
  va_end(ap);

  text(p);
  free(p);
}

const char *str_stars = "************************************************************************";

void print_header(const char *marker,
                  const char *comment)
{
  size_t stars = strlen(marker)+(4+5+2);
  if (stars > strlen(str_stars))
    stars = strlen(str_stars);

  printf ("\n"
          "/** BEGIN_%s %s\n"
          " *\n"
          " * %s\n"
          " *\n"
          " * Do not edit - automatically generated.\n"
          " */\n"
	  "\n",
          marker,
	  str_stars+stars,
          comment);
}

void print_footer(const char *marker)
{
  size_t stars = strlen(marker)+(4+3+2+2);
  if (stars > strlen(str_stars))
    stars = strlen(str_stars);

  printf ("\n"
	  "/** END_%s %s*/\n"
          "\n",
          marker,
	  str_stars+stars);
}


