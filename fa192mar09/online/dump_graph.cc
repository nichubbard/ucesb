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
 * ../fa192mar09 /dev/null --ntuple=RAW,STRUCT_HH,fa192_onl.h
 *
 * Compile with (from unpacker/ directory):
 *
 * g++ -o dump_graph -I . -I ../../hbook dump_graph.cc ../../hbook/ext_data_client.o
 */

#include "ext_data_client.h"

/* Change these, here or replace in the code. */

#define EXT_EVENT_STRUCT_H_FILE       "fa192_onl.h"
#define EXT_EVENT_STRUCT              EXT_STR_h101
#define EXT_EVENT_STRUCT_ONION        EXT_STR_h101_onion
#define EXT_EVENT_STRUCT_LAYOUT       EXT_STR_h101_layout
#define EXT_EVENT_STRUCT_LAYOUT_INIT  EXT_STR_h101_LAYOUT_INIT

/* */

#include EXT_EVENT_STRUCT_H_FILE

#include <stdlib.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
  struct ext_data_client *client;
  int ret;

  EXT_EVENT_STRUCT event;
  EXT_EVENT_STRUCT_LAYOUT event_layout = EXT_EVENT_STRUCT_LAYOUT_INIT;
  EXT_EVENT_STRUCT_ONION &e = (EXT_EVENT_STRUCT_ONION &) event;

  if (argc < 2)
    {
      fprintf (stderr,"No server name given, usage: %s SERVER\n",argv[0]);
      exit(1);
    }

  /* Connect. */

  client = ext_data_connect_stderr(argv[1]);

  if (client == NULL)
    exit(1);

  if (ext_data_setup_stderr(client,
			    &event_layout,sizeof(event_layout),
			    sizeof(event)))
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

	  if (!ext_data_fetch_event_stderr(client,
					   &event,sizeof(event)))
	    break;

	  /* Do whatever is wanted with the data. */
	  /*
	  printf ("%10d: %2d  %4d %4d  %4d %4d\n",
		  e.EVENTNO,e.TRIGGER,
		  e.sadc[0]._,e.sadc[1]._,
		  e.sadcp[0]._,e.sadcp[1]._);
	  */

	  if (e.sadc[0]._ && e.sadcp[0]._)
	    {
	      for (int i = 0; i < 40; i++)
		printf ("%04x %04x\n",e.sadc[0].v[i],e.sadcp[0].v[i]);

	      printf ("---------\n");
	    }

	  /* ... */
	}
    }

  /* Disconnect. */

  ext_data_close_stderr(client);

  return 0;
}
