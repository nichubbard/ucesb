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

/* Template for an 'ext_data' writer.
 *
 * Note that the output data is written to stdout, so all other output
 * (information as well as error messages) must go to stdout.
 *
 * To generate the needed ext_h101.h file (ucesb), use e.g.
 *
 * empty/empty /dev/null --ntuple=UNPACK,STRUCT_HH,ext_h101.h
 *
 * Compile with (from unpacker/ directory):
 *
 * cc -g -O3 -o ext_writer_h101 -I. -Ihbook hbook/example/ext_data_writer.c hbook/ext_data_client.o
 */

#include "ext_data_client.h"

/* Change these, here or replace in the code. */

#define EXT_EVENT_STRUCT_H_FILE       "ext_h101.h"
#define EXT_EVENT_STRUCT              EXT_STR_h101
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

  /* Open the output (stdout). */

  client = ext_data_open_out();

  if (client == NULL)
    {
      perror("ext_data_open_out");
      fprintf (stderr,"Failed to setup output.\n");
      exit(1);
    }

  fprintf (stderr,"Writing data to stdout.\n");

  if (ext_data_setup(client,
		     &event_layout,sizeof(event_layout),
		     NULL,
		     sizeof(event),
		     "",NULL) != 0)
    {
      perror("ext_data_setup");
      fprintf (stderr,"Failed to setup output: %s\n",
	       ext_data_last_error(client));
      /* Not more fatal than that we can disconnect. */
    }
  else
    {
      int num_events = -1;
      int i;

      /* Generate events. */

      if (argc > 1)
	num_events = atoi(argv[1]);

      for (i = 0; num_events == -1 || i < num_events; i++)
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

	  /* If the @clear_zzp_lists argument is set to 1, all fields
	   * will be initialized and the random fill is useless.  This
	   * will however not circumvent any bugs, and is also a
	   * performance killer.
	   */

	  if (ext_data_clear_event(client,&event,sizeof(event),
				   /* clear_zzp_lists */ 0,
				   /* struct_id */ 0) != 0)
	    {
	      perror("ext_data_clear_event");
	      fprintf (stderr,"Failed to clear event: %s\n",
		       ext_data_last_error(client));
	      break;
	    }

	  /* Do whatever is wanted to create the event. */

	  event.EVENTNO = (uint32_t) (i+1);
	  event.TRIGGER = (uint32_t) (1 + i % 15);

	  /* ... */

	  /* Write the event. */

	  ret = ext_data_write_event(client,
				     &event,sizeof(event));

	  if (ret == -1)
	    {
	      perror("ext_data_write_event");
	      fprintf (stderr,"Failed to write event: %s\n",
		       ext_data_last_error(client));
	      /* Not more fatal than that we can disconnect. */
	      break;
	    }

	  if ((size_t) ret < sizeof(event))
	    {
	      fprintf (stderr,
		       "Writing event failed, error at offset %d = 0x%x: %s\n",
		       ret,ret,ext_data_last_error(client));
	      break;
	    }
	}
    }

  /* Disconnect. */

  ret = ext_data_close(client);

  if (ret != 0)
    {
      perror("ext_data_close");
      fprintf (stderr,"Errors reported during disconnect: %s\n",
	       ext_data_last_error(client));
      exit(1);
    }

  fprintf (stderr,"Done writing data.\n");

  return 0;
}
