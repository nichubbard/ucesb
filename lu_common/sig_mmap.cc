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

#include "sig_mmap.hh"
#include "error.hh"

#include <errno.h>
#include <unistd.h>
#include <stdint.h>

sig_mmap_info *_mmap_infos = NULL;

void sig_register_mmap(sig_mmap_info *info,
		       void *addr, size_t length, int fd, off_t offset)
{
  info->_addr = addr;
  info->_length = length;
  info->_fd = fd;
  info->_offset = offset;

  // Add it to the chained list.

  info->_next = _mmap_infos;

  _mmap_infos = info;
}

void sig_unregister_mmap(sig_mmap_info *info,
			 void *addr, size_t length)
{
  // Find the item

  sig_mmap_info **prev = &_mmap_infos;

  for ( ; ; )
    {
      sig_mmap_info *cur = *prev;

      if (cur == NULL)
	ERROR("Internal failure, cannot find mmap info structure.");

      if (cur == info)
	{
	  if (cur->_addr != addr ||
	      cur->_length != length)
	    ERROR("Internal failure, "
		  "mmap info structure does not match, cannot unregister.");

	  cur->_addr = NULL;

	  // Remove from the chained list.

	  *prev = cur->_next;

	  return;
	}

      prev = &cur->_next;
    }
}

void sig_diagnose_mmap(void *fault_addr)
{
  // See if the faulting address is within any of our known areas

  for (sig_mmap_info *info = _mmap_infos; info; info = info->_next)
    {
      // size_t is unsigned!, so need not check vs >= 0

      size_t map_offset =
	(size_t) ((char *) fault_addr - (char *) info->_addr);

      if (map_offset < info->_length)
	{
	  // Requested data is within range of our mapping.

	  off_t file_offset = (off_t) map_offset + info->_offset;

	  WARNING("Received SIGBUS signal for memory address of mmap'ed file "
		  "(fd = %d, file offset %jd).",
		  info->_fd, (uintmax_t) file_offset);
	  WARNING("Likely cause: file was truncated, or I/O error.  "
		  "Trying normal read for diagnostics.");

	  // First we must seek to the wanted position.  Note that we
	  // intend to exit the program after this report, so changing
	  // the file location ism harmless.

	  off_t new_off = lseek(info->_fd, file_offset, SEEK_SET);

	  if (new_off == (off_t) -1 ||
	      new_off != file_offset)
	    WARNING("Failed to seek to the specified offset.");

	  char dummy[32];

	  ssize_t ret;

	  for ( ; ; )
	    {
	      ret = read (info->_fd, dummy, sizeof (dummy));

	      if (ret == -1 &&
		  (errno == EAGAIN || errno == EINTR))
		continue; // try again

	      break;
	    }

	  if (ret == -1)
	    {
	      perror("read");
	      WARNING("Reading at offset %jd failed.", (uintmax_t) file_offset);
	    }
	  else
	    {
	      INFO(0, "Successfully read %zd bytes at offset %jd.",
		   ret, (uintmax_t) file_offset);
	    }

	  return;
	}
    }
}
