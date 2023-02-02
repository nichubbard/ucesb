/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2023  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include <assert.h>

#include "tdcpm_serialize_util.h"
#include "tdcpm_malloc.h"
#include "tdcpm_error.h"

/**************************************************************************/

uint32_t *tdcpm_serialize_alloc_words(tdcpm_serialize_info *ser,
				      size_t words)
{
  size_t reserve;
  
  assert(!ser->_reserved);

  reserve = ser->_offset + words;

  if (reserve > ser->_alloc)
    {
      if (!ser->_alloc)
	ser->_alloc = 0x1000;

      while (ser->_alloc < reserve)
	if (ser->_alloc < 0x1000000)
	  ser->_alloc *= 2;
	else
	  ser->_alloc += 0x1000000;
      
      ser->_buf = (uint32_t *)
	tdcpm_realloc(ser->_buf, ser->_alloc * sizeof (uint32_t),
		      "serialized data buffer");
    }

  ser->_reserved = reserve;

  return ser->_buf + ser->_offset;
}

void tdcpm_serialize_commit_words(tdcpm_serialize_info *ser, uint32_t *dest)
{
  ptrdiff_t used;

  used = dest - ser->_buf;

  assert(used <= ser->_reserved);

  ser->_offset = used;

  ser->_reserved = 0;
}

/**************************************************************************/
