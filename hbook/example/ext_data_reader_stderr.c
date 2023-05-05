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

/* Template for an 'ext_data' reader.
 *
 * To generate the needed ext_h101.h file (ucesb), use e.g.
 *
 * empty/empty /dev/null --ntuple=UNPACK,STRUCT_HH,ext_h101.h
 *
 * Compile with (from unpacker/ directory):
 *
 * cc -g -O3 -o ext_reader_h101 -I. -Ihbook hbook/example/ext_data_reader.c hbook/ext_data_client.o
 */

#include "ext_data_client.h"

/* Change these, here or replace in the code. */

#define EXT_EVENT_STRUCT_H_FILE       "ext_h101.h"
#define EXT_EVENT_STRUCT              EXT_STR_h101
#define EXT_EVENT_STRUCT_ITEMS_INFO   EXT_STR_h101_ITEMS_INFO

/* */

#include EXT_EVENT_STRUCT_H_FILE

#include <stdlib.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
  struct ext_data_client *client;
  int ok;

  EXT_EVENT_STRUCT event;
  struct ext_data_structure_info *struct_info = NULL;
  uint32_t struct_map_success;

  if (argc < 2)
    {
      fprintf (stderr,"No server name given, usage: %s SERVER\n",argv[0]);
      exit(1);
    }

  /* Connect. */

  client = ext_data_connect_stderr(argv[1]);

  if (client == NULL)
    exit(1);

  struct_info = ext_data_struct_info_alloc_stderr();
  if (struct_info == NULL)
    exit(1);

  EXT_EVENT_STRUCT_ITEMS_INFO(ok, struct_info, 0, EXT_EVENT_STRUCT, 1);
  if (!ok)
    exit(1);

 if (ext_data_setup_stderr(client,
			   NULL,0,
			   struct_info, &struct_map_success,
			   sizeof(event),
			   "", NULL,
			   EXT_DATA_ITEM_MAP_OK))
    {
      /* Handle events. */

      for ( ; ; )
	{
	  /* To 'check'/'protect' against mis-use of zero-suppressed
	   * data items, fill the entire buffer with random junk.
	   *
	   * Note: this IS a performance KILLER, and is not
	   * recommended for production!
	   */

#ifdef BUGGY_CODE
	  ext_data_rand_fill(&event,sizeof(event));
#endif

	  /* Fetch the event. */

	  if (!ext_data_fetch_event_stderr(client,&event,sizeof(event),0))
	    break;

	  /* Do whatever is wanted with the data. */

	  printf ("%10d: %2d\n",event.EVENTNO,event.TRIGGER);

	  /* ... */
	}
    }

  ext_data_close_stderr(client);

  return 0;
}
