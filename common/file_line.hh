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

#ifndef __FILE_LINE_H__
#define __FILE_LINE_H__

#include <stdio.h>

struct lineno_map
{
  int         _internal;

  int         _line;
  const char* _file;

  lineno_map *_prev;
  lineno_map *_next;
};

extern lineno_map *_last_lineno_map;
extern lineno_map *_first_lineno_map;

void print_lineno(FILE* fid,int internal);

class file_line
{
public:
  file_line(int internal)
  {
    _internal = internal;
  }

  file_line()
  {
    _internal = 0;
  }

public:
  void print_lineno(FILE* fid) const
  {
    ::print_lineno(fid,_internal);
  }

public:
  int _internal;
};

void generate_locations();

void insert_lineno_map(int internal, const char* _file, int line);

#endif//__FILE_LINE_H__
