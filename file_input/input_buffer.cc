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

#include "input_buffer.hh"
#include "error.hh"

#include "colourtext.hh"
#include "hex_dump.hh"

#include "typedef.hh"
#include <stdio.h>

input_buffer::input_buffer()
{
#ifdef USE_THREADING
  _blocked_next_file = NULL;
#endif
#ifdef USE_CURSES
  _filename = NULL;
#endif
}

input_buffer::~input_buffer()
{

}


// NOTE: the read_range is ONLY to be used in case we immediately want
// the entire contents in the specified range (i.e. a header that gets
// investigated NOW) In case of data, we are to only request the
// chunks (i.e. call map_range directly)

bool input_buffer::read_range(void *dest,off_t start,size_t length)
{
  buf_chunk chunks[2];

  // First we get the range mapped (virtual function call)

  int num_chunks = map_range(start,start+(off_t) length,chunks);

  if (!num_chunks)
    return false;

  // So the range has been mapped.  Simply do the copying needed.
  /*
  INFO(0,"input_buffer::read_range(dest=%08x,start=%08x,length=%08x)->%d",
       dest,(int)start,length,num_chunks);
  INFO(0,"chunk:%08x(%08x)",chunks[0]._ptr,chunks[0]._length);
  */
  memcpy (dest,
	  chunks[0]._ptr,chunks[0]._length);

  if (num_chunks > 1)
    {
      //INFO("chunk:%08x(%08x)",chunks[1]._ptr,chunks[1]._length);

      memcpy (((char *) dest) + chunks[0]._length,
	      chunks[1]._ptr,chunks[1]._length);
    }
  //INFO("done");
  return true;
}







/*
// Force instantiation
void hex_dump(FILE *fid,
	      uint32 *start,void *end,
	      const char *fmt,int *line_feed,int items_per_line);
void hex_dump(FILE *fid,
	      uint16 *start,void *end,
	      const char *fmt,int *line_feed,int items_per_line);
*/



void input_buffer_cont::close()
{
  if (_input)
    _input->close();
}

void input_buffer_cont::destroy()
{
  if (_input)
    {
      delete _input;
      _input = NULL;
      _cur   = -1;
    }
}

void input_buffer_cont::take_over(input_buffer_cont &src)
{
  _input = src._input;
  _cur   = src._cur;

  src._input = NULL;
  src._cur   = -1;
}

#ifdef USE_THREADING
void input_buffer::request_next_file()
{
  int token = TOKEN_OPEN_FILE | _wakeup_next_file;

  // Send the file opener thread a request to open another file
  _blocked_next_file->wakeup(token);
}

void input_buffer::set_next_file(thread_block *blocked_next_file,
				 int           wakeup_next_file)
{
  _blocked_next_file = blocked_next_file;
  _wakeup_next_file  = wakeup_next_file;
}
#endif

void input_buffer::set_filename(const char *filename)
{
#ifdef USE_CURSES
  _filename = filename;
#else
  UNUSED(filename);
#endif
}

void print_remaining_event(buf_chunk *chunk_cur,
			   buf_chunk *chunk_end,
			   size_t offset_cur,
			   bool swapping)
{
  if (chunk_cur >= chunk_end)
    return;

  size_t leftovers = 0;

  printf("  %sLeftovers:%s\n",CT_OUT(RED),CT_OUT(DEF_COL));

  {
    hex_dump_buf buf;

    while (chunk_cur < chunk_end)
      {
	leftovers += chunk_cur->_length - offset_cur;

	if (((chunk_cur->_length - offset_cur) & 3) == 0) // length is divisible by 4
	  hex_dump(stdout,
		   (uint32*) (chunk_cur->_ptr + offset_cur),
		   chunk_cur->_ptr + chunk_cur->_length,swapping,
		   "%c%08x",9,buf,NULL);
	else
	  hex_dump(stdout,
		   (uint16*) (chunk_cur->_ptr + offset_cur),
		   chunk_cur->_ptr + chunk_cur->_length,swapping,
		   "%c%04x",5,buf,NULL);

	chunk_cur++;
	offset_cur = 0;
      }
  }

  printf("  %sRemained: %s%8d%s%s bytes%s\n",
	 CT_OUT(RED),CT_OUT(BOLD),
	 (int) leftovers,
	 CT_OUT(NORM),CT_OUT(RED),CT_OUT(DEF_COL));
}
