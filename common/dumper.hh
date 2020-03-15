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

#ifndef __DUMPER_HH__
#define __DUMPER_HH__

#include <stdio.h>

class dumper_dest
{
public:
  virtual ~dumper_dest() { }

public:
  virtual void put(char c) = 0;
  virtual void write(const char* p,size_t len) = 0;
};

class dumper_dest_file
  : public dumper_dest
{
public:
  dumper_dest_file(FILE *fid);
  virtual ~dumper_dest_file() { }

protected:
  FILE *_fid;

public:
  virtual void put(char c);
  virtual void write(const char* p,size_t len);
};

class dumper_dest_memory
  : public dumper_dest
{
public:
  dumper_dest_memory();
  virtual ~dumper_dest_memory();

protected:
  char *_start;
  char *_end;
  char *_cur;

public:
  char *get_string();
  void fwrite(FILE* fid);

public:
  virtual void put(char c) { write(&c,1); }
  virtual void write(const char* p,size_t len);
};



class dumper
{
public:
  dumper(dumper_dest *dest);
  dumper(dumper &src,int more = 0,bool comment = false);

public:
  size_t _indent;
  size_t _current;

  bool _comment;
  bool _comment_written;

  dumper_dest *_dest;

public:
  void indent();

  void nl();
  void text(const char *str,bool optional_newline = false);
  void text_fmt(const char *fmt,...)
    __attribute__ ((__format__ (__printf__, 2, 3)));
  void col0_text(const char *str);
};

void print_header(const char *marker,
                  const char *comment);
void print_footer(const char *marker);

#endif//__DUMPER_HH__
