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

#include "../hbook/example/test_caen_v775_data.h"
#include "../hbook/example/test_caen_v1290_data.h"

/* Change these, here or replace in the code. */

#define EXT_EVENT_STRUCT_H_FILE       "ext_xtst_regress.h"
#define EXT_EVENT_STRUCT              EXT_STR_xtst_regress
#define EXT_EVENT_STRUCT_LAYOUT       EXT_STR_xtst_regress_layout
#define EXT_EVENT_STRUCT_LAYOUT_INIT  EXT_STR_xtst_regress_LAYOUT_INIT

/* */

#define EXT_STICKY_STRUCT              EXT_STR_hsticky
#define EXT_STICKY_STRUCT_LAYOUT       EXT_STR_hsticky_layout
#define EXT_STICKY_STRUCT_LAYOUT_INIT  EXT_STR_hsticky_LAYOUT_INIT

/* */

#include EXT_EVENT_STRUCT_H_FILE

#include <stdlib.h>
#include <stdio.h>

int main(int argc,char *argv[])
{
  struct ext_data_client *client;

  EXT_EVENT_STRUCT event;
  EXT_STICKY_STRUCT sticky;
#if 0
  EXT_EVENT_STRUCT_LAYOUT event_layout = EXT_EVENT_STRUCT_LAYOUT_INIT;
#endif
  struct ext_data_structure_info *struct_info = NULL;
  struct ext_data_structure_info *sticky_struct_info = NULL;
  int ok;

  uint64_t num_good = 0;
  uint64_t num_good_data = 0;

  uint64_t sticky_corr_base_sum = 0;

  int hevent_id = -1;
  int hsticky_id = -1;

  if (argc < 2)
    {
      fprintf (stderr,"No server name given, usage: %s SERVER\n",argv[0]);
      exit(1);
    }

  struct_info = ext_data_struct_info_alloc();
  if (struct_info == NULL)
    {
      perror("ext_data_struct_info_alloc");
      fprintf (stderr,"Failed to allocate structure information.\n");
      exit(1);
    }

  EXT_STR_xtst_regress_ITEMS_INFO(ok, struct_info, 0, EXT_EVENT_STRUCT, 1);
  if (!ok)
    {
      perror("ext_data_struct_info_item");
      fprintf (stderr,"Failed to setup structure information.\n");
      exit(1);
    }

  sticky_struct_info = ext_data_struct_info_alloc();
  if (sticky_struct_info == NULL)
    {
      perror("ext_data_struct_info_alloc");
      fprintf (stderr,"Failed to allocate sticky structure information.\n");
      exit(1);
    }

  EXT_STR_hsticky_ITEMS_INFO(ok, sticky_struct_info, 0, EXT_STICKY_STRUCT, 1);
  if (!ok)
    {
      perror("ext_data_struct_info_item");
      fprintf (stderr,"Failed to setup sticky structure information.\n");
      exit(1);
    }

  /* Connect. */

  client = ext_data_connect_stderr(argv[1]);

  if (client == NULL)
    exit(1);

  if (!ext_data_setup_stderr(client,
#if 0 /* Do it by the structure info, so we can handle differing input. */
			     &event_layout,sizeof(event_layout),
#else
			     NULL, 0,
#endif
			     struct_info,/*NULL,*/
			     sizeof(event),
			     "", &hevent_id))
    {
      fprintf (stderr,"Failed to setup data structure from stream.\n");
      exit(1);
    }
  
  if (!ext_data_setup_stderr(client,
			     NULL, 0,
			     sticky_struct_info,/*NULL,*/
			     sizeof(sticky),
			     "hsticky", &hsticky_id))
    {
      fprintf (stderr,"Failed to setup sticky data structure from stream.\n");
      exit(1);
    }

  /* printf ("hevent_id: %d  hsticky_id: %d\n", hevent_id, hsticky_id); */
  
    {
      /* Handle events. */

      for ( ; ; )
	{
	  /*uint32_t i;*/
	  int ok;

	  caen_v775_data v775a;
	  caen_v775_data v775b;

	  caen_v775_data v775a_good;
	  caen_v775_data v775b_good;

	  caen_v1290_data v1290a;
	  caen_v1290_data v1290b;

	  caen_v1290_data v1290a_good;
	  caen_v1290_data v1290b_good;

	  int struct_id = 0;

	  if (!ext_data_next_event_stderr(client, &struct_id))
	    break;

	  if (struct_id == hsticky_id)
	    {
	      if (!ext_data_fetch_event_stderr(client,&sticky,sizeof(sticky),
					       hsticky_id))
		break;

	      /*
	      printf ("sticky: %d %d %d %d\n",
		      sticky.EVENTNO, sticky.STIDX,
		      sticky.corr_base, sticky.corrbase);
	      */

	      continue;
	    }

	  if (struct_id != hevent_id)
	    {
	      fprintf (stderr, "Unexpected next structure id (%d).\n",
		       struct_id);
	      exit(1);
	    }

	  /* To 'check'/'protect' against mis-use of zero-suppressed
	   * data items, fill the entire buffer with random junk.
	   *
	   * Note: this IS a performance KILLER, and is not
	   * recommended for production!
	   */

	  ext_data_rand_fill(&event,sizeof(event));

	  /* Fetch the event. */

	  if (!ext_data_fetch_event_stderr(client,&event,sizeof(event),
					   hevent_id))
	    break;

	  /* Do whatever is wanted with the data. */

	  /*
	  printf ("%10d: %2d\n",event.EVENTNO,event.TRIGGER);
	  */
	  /*
	  printf ("%d\n",event.regress_v775mod1n);
	  for (i = 0; i < event.regress_v775mod1n; i++)
	    {
	      printf ("%2d %d\n",
		      event.regress_v775mod1nI[i],
		      event.regress_v775mod1data[i]);
	    }
	  */
	  /*
	  printf ("h:%x\n",event.regress_v1290mod1header);
	  printf ("t:%x\n",event.regress_v1290mod1trigger);
	  printf ("n:%d\n",event.regress_v1290mod1n);
	  for (i = 0; i < event.regress_v1290mod1n; i++)
	    {
	      printf ("%d\n",
		      event.regress_v1290mod1data[i]);
	    }
	  */

	  ok = 1;

	  /* */

	  ok &= fill_caen_v775_data(&v775a,
				    event.regress1v775mod1n,
				    event.regress1v775mod1nI,
				    event.regress1v775mod1data,
				    event.regress1v775mod1eob);

	  ok &= fill_caen_v775_data(&v775b,
				    event.regress1v775mod2n,
				    event.regress1v775mod2nI,
				    event.regress1v775mod2data,
				    event.regress1v775mod2eob);

	  create_caen_v775_event(&v775a_good,
				 1, 0x80 - 1,
				 event.EVENTNO + 0xdef,
				 event.regress1seed);

	  create_caen_v775_event(&v775b_good,
				 2, 0x80 - 2,
				 event.EVENTNO + 0xdef,
				 event.regress1seed);

	  ok &= compare_caen_v775_event(&v775a_good, &v775a);
	  ok &= compare_caen_v775_event(&v775b_good, &v775b);

	  /* */

	  ok &= fill_caen_v1290_data(&v1290a,
				     event.regress1v1290mod1nM,
				     event.regress1v1290mod1nMI,
				     event.regress1v1290mod1nME,
				     event.regress1v1290mod1n,
				     event.regress1v1290mod1data,
				     event.regress1v1290mod1header,
				     event.regress1v1290mod1trigger);

	  ok &= fill_caen_v1290_data(&v1290b,
				     event.regress1v1290mod2nM,
				     event.regress1v1290mod2nMI,
				     event.regress1v1290mod2nME,
				     event.regress1v1290mod2n,
				     event.regress1v1290mod2data,
				     event.regress1v1290mod2header,
				     event.regress1v1290mod2trigger);

	  create_caen_v1290_event(&v1290a_good,
				  1,
				  event.EVENTNO + 0xdef,
				  event.regress1seed);

	  create_caen_v1290_event(&v1290b_good,
				  2,
				  event.EVENTNO + 0xdef,
				  event.regress1seed);

	  ok &= compare_caen_v1290_event(&v1290a_good, &v1290a);
	  ok &= compare_caen_v1290_event(&v1290b_good, &v1290b);

	  /* */

	  if (ok)
	    {
	      num_good++;
	      num_good_data += event.regress1v775mod1n;
	      num_good_data += event.regress1v775mod2n;
	      num_good_data += event.regress1v1290mod1n;
	      num_good_data += event.regress1v1290mod2n;
	    }
	  else
	    {
	      printf ("Error was in event %d.\n",event.EVENTNO);
	    }

	  /* ... */

	  if (event.STCORR / 100 != sticky.corrbase)
	    {
	      fprintf (stderr,
		       "Bad sticky correlation: sticky base: %d, event %d\n",
		       sticky.corrbase, event.STCORR);
	      exit(1);
	    }

	  /* Summing this, just to ensure that there is non-zero
	   * sticky data.
	   */
	  sticky_corr_base_sum += sticky.corrbase;
	}
    }

  printf ("%" PRIu64 " events passed test (%" PRIu64 " words).\n"
	  "(Sticky corr base sum: %" PRIu64 ")\n",
	  num_good, num_good_data,
	  sticky_corr_base_sum);

  ext_data_close_stderr(client);

  return 0;
}
