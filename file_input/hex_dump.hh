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

#ifndef __HEX_DUMP_HH__
#define __HEX_DUMP_HH__

#include "data_src.hh"
#include "colourtext.hh"

#include "hex_dump_mark.hh"

class hex_dump_buf
{
public:
  hex_dump_buf()
  {
    _end = _buf;
    _added = 0;
  }

  ~hex_dump_buf()
  {
    if (_end != _buf)
      eject();
  }

public:
  char _buf[80+64]; /* +64 for red marker */
  char *_end;
  size_t _added;

public:
  void eject()
  {
    *_end = 0;
    printf("%s\n",_buf);
    _end = _buf;
    _added = 0;
  }
};

template<typename T>
void hex_dump(FILE *fid,
	      T *start,void *end,bool swapping,
	      const char *fmt,
	      int fmt_max_len,
	      hex_dump_buf &buf,
	      hex_dump_mark_buf *erraddr)
{
  for ( ; start < end; start++)
    {
      // if (!line_items)
      //   printf (line_start);

      if (buf._added + (size_t) fmt_max_len > 79)
	buf.eject();

      T item = *start;

      item = swapping ? bswap(item) : item;

      bool is_erraddr_prev = false;
      bool is_erraddr_this = false;
      bool is_erraddr_next = false;

      if (erraddr)
	{
	  is_erraddr_prev =
	    (((((size_t) start) ^
	       ((size_t) erraddr->_prev)) & ~(sizeof (T) - 1)) == 0);
	  is_erraddr_this =
	    (((((size_t) start) ^
	       ((size_t) erraddr->_this)) & ~(sizeof (T) - 1)) == 0);
	  is_erraddr_next =
	    (((((size_t) start) ^
	       ((size_t) erraddr->_next)) & ~(sizeof (T) - 1)) == 0);
	}

      if (is_erraddr_this)
	buf._end += sprintf(buf._end,"%s",CT_OUT(BOLD_RED));
      else if (is_erraddr_next)
	buf._end += sprintf(buf._end,"%s",CT_OUT(BOLD_BLUE));
      else if (is_erraddr_prev)
	buf._end += sprintf(buf._end,"%s",CT_OUT(BOLD));

      size_t add = (size_t) sprintf(buf._end,fmt,
				    is_erraddr_this ? '<' :
				    is_erraddr_next ? '>' : ' ',item);

      buf._end += add;
      buf._added += add;

      if (is_erraddr_prev || is_erraddr_this || is_erraddr_next)
	buf._end += sprintf(buf._end,"%s",CT_OUT(NORM_DEF_COL));

      assert(buf._end <= buf._buf + sizeof(buf._buf));
    }
}

#endif//__HEX_DUMP_HH__
