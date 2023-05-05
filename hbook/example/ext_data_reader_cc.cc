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

#include "ext_data_clnt_stderr.hh"
#include "ext_data_struct_info.hh"

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
  ext_data_clnt_stderr client;
  int ok;

  EXT_EVENT_STRUCT event;
  ext_data_struct_info struct_info;
  uint32_t struct_map_success;

  if (argc < 2)
    {
      fprintf (stderr,"No server name given, usage: %s SERVER\n",argv[0]);
      exit(1);
    }

  /* Connect. */

  if (!client.connect(argv[1]))
    exit(1);

  EXT_EVENT_STRUCT_ITEMS_INFO(ok, struct_info, 0, EXT_EVENT_STRUCT, 0);
  if (!ok)
    {
      perror("ext_data_struct_info_item");
      fprintf (stderr,"Failed to setup structure information.\n");
      exit(1);
    }

  if (client.setup(NULL, 0,
		   &struct_info, &struct_map_success,
		   sizeof(event),
		   "",NULL,
		   EXT_DATA_ITEM_MAP_OK))
    {
      /* Handle events. */

      for ( ; ; )
	{
	  /* Fetch the event. */

	  if (!client.fetch_event(&event,sizeof(event)))
	    break;

	  /* Do whatever is wanted with the data. */

	  printf ("%10d: %2d\n",event.EVENTNO,event.TRIGGER);

	  /* ... */
	}
    }

  client.close();

  return 0;
}
