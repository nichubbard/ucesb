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

#include "ext_data_client.h"

#define EXT_EVENT_STRUCT_H_FILE       "ext_h99.h"
#define EXT_EVENT_STRUCT              EXT_STR_h99
#define EXT_EVENT_STRUCT_LAYOUT       EXT_STR_h99_layout
#define EXT_EVENT_STRUCT_LAYOUT_INIT  EXT_STR_h99_LAYOUT_INIT

#include EXT_EVENT_STRUCT_H_FILE

#include <stdlib.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
  struct ext_data_client *client;
  int i;

  EXT_EVENT_STRUCT event;
  EXT_EVENT_STRUCT_LAYOUT event_layout = EXT_EVENT_STRUCT_LAYOUT_INIT;

  if (argc < 2)
    {
      fprintf (stderr,"No server name given, usage: %s SERVER\n",argv[0]);
      exit(1);
    }

  client = ext_data_connect_stderr(argv[1]);

  if (client == NULL)
    exit(1);

  if (ext_data_setup_stderr(client,
			    &event_layout,sizeof(event_layout),
			    NULL,
			    sizeof(event)))
    {
      for ( ; ; )
	{
	  ext_data_rand_fill(&event,sizeof(event));

	  if (!ext_data_fetch_event_stderr(client,&event,sizeof(event)))
	    break;

	  if (event.a < 25 || (event.a % 577) == 0)
	    {
	      printf ("%10d: %6.3f,", event.a, event.c);
	      for (i = 0; i < 3; i++)
		printf (" %6.3f", event.e[i]);
	      printf (", %2d,", event.b);
	      for (i = 0; i < event.b; i++)
		printf (" %6.3f", event.d[i]);
	      printf ("\n");
	    }
	}
    }

  ext_data_close_stderr(client);

  return 0;
}
