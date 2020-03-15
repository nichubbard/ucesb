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

#include "convert_picture.hh"

#include "forked_child.hh"

#include <string.h>
#include <stdio.h>

void convert_picture(const char *filename,
		     const char *pict,
		     int width,int height)
{
  forked_child writer;
  int fd;
  char header[128];

  const char *argv[4] = { "convert", "-", filename, NULL };

  writer.fork(argv[0],argv,NULL,&fd);

  sprintf (header,
	   "P5\n"
	   "%d %d\n"
	   "255\n",
	   width,height);

  size_t sz = sizeof(char) * (size_t) width * (size_t) height;

  full_write(fd,header,strlen(header));
  full_write(fd,pict,sz);

  writer.wait(false);
}

