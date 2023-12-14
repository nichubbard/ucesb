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
#define EXT_EVENT_STRUCT_LAYOUT       EXT_STR_h101_layout
#define EXT_EVENT_STRUCT_LAYOUT_INIT  EXT_STR_h101_LAYOUT_INIT

/* */

#include EXT_EVENT_STRUCT_H_FILE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

/*static const std::string fatima_scalers_names[16] =*/
/*{*/
  /*"bPlast Free",*/
  /*"bPlast Accepted",*/
  /*"",*/
  /*"LOAX Free",*/
  /*"LOAX Accepted",*/
  /*"",*/
  /*"bPlast Up",*/
  /*"bPlast Down",*/
  /*"bPlast Up AND Down",*/
  /*"SCI41 L",*/
  /*"SCI41 R",*/
  /*"SCI21 L",*/
  /*"SCI21 R",*/
  /*"10Hz",*/
  /*"DTAS Anodes",*/
  /*"",*/
/*};*/

static const std::string fatima_scalers_names[16] =
{
  "bPlast Free",
  "bPlast Accepted",
  "FATIMA TAMEX Free",
  "FATIMA TAMEX Accepted",
  "FATIMA VME Free",
  "FATIMA VME Accepted",
  "Ge Free",
  "Ge Accepted",
  "bPlast Up",
  "bPlast Down",
  "bPlast AND",
  "SCI41 L",
  "SCI41 R",
  "SCI42 L",
  "SCI42 R"
};

static const std::string frs_frs_names[32] =
{
  "IC01curr-old",
  "SEETRAM-old",
  "SEETRAM",
  "IC01curr",
  "IC01",
  "SCI00",
  "SCI01",
  "SCI02",
  "Start Extr",
  "Stop Extr",
  "Beam Transformer",
};

static const std::string frs_main_names[32] =
{
  "FRS Free",
  "FRS Accepted",
  "Spill Counter",
  "1 Hz clock",
  "10 Hz clock",
  "100 kHz X veto",
  "100 kHz clock",
  "1 kHz clock",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "SCI21L",
  "SCI41L",
  "SCI42L",
  "SCI43L",
  "SCI81L",
  "SCI21R",
  "SCI41R",
  "SCI42R",
  "SCI43R",
  "SCI81R",
  "SCI31L",
  "SCI31R",
  "SCI11",
  "SCI22L",
  "SCI22R"
};

#ifndef USE_ITEMS_INFO
#define USE_ITEMS_INFO 0
#endif

int main(int argc,char *argv[])
{
  struct ext_data_client *client;
  uint32_t struct_map_success;
  int ret, ok;
  FILE *empty_fid = NULL;

  EXT_EVENT_STRUCT event;
#if !USE_ITEMS_INFO
  EXT_EVENT_STRUCT_LAYOUT event_layout = EXT_EVENT_STRUCT_LAYOUT_INIT;
#else
  struct ext_data_structure_info *struct_info = NULL;
#endif
  uint32_t last_event_no = 0;
  const void *raw;
  ssize_t raw_words;

  uint64_t fatima_scalers[16] = {0};
  bool fatima_scalers_init = false;
  uint32_t fatima_scalers_last[16] = {0};

  uint64_t frs_scalers[64] = {0};
  bool frs_scalers_init = false;
  uint32_t frs_scalers_last[64] = {0};

  if (argc < 2)
    {
      fprintf (stderr,"No server name given, usage: %s SERVER\n",argv[0]);
      exit(1);
    }

#define EMPTY_READS_LMD_OPT "--empty-reads-lmd="

  if (strncmp(argv[1],EMPTY_READS_LMD_OPT,strlen(EMPTY_READS_LMD_OPT)) == 0)
    {
      const char *filename;
      char *command;
      size_t n;
      int empty_fd;

      filename = argv[1] + strlen (EMPTY_READS_LMD_OPT);

      /* Fork of a process for reading, and use an output pipe from it
       * to receive the data.  The same can be done more laboriously
       * using pipe(), fork(), exec(), wait(), close() and so on (see
       * lu_common/forked_child.cc).  But since we only want an output
       * pipe, popen() is enough.
       */

      n = strlen(filename) + 128;
      command = (char*)malloc (n);

      if (!command)
	{
	  fprintf (stderr,"Memory allocation error.\n");
	  exit(1);
	}

      /* Assume we run from the unpacker/ directory. */

      snprintf (command,n,
		"empty/empty"
		" %s"
		" --ntuple=UNPACK,STRUCT,-",
		filename);

      empty_fid = popen(command, "r");

      if (empty_fid == NULL)
	{
	  perror("popen");
	  fprintf (stderr,"Failed to fork empty unpacker: '%s'.\n", command);
	  exit(1);
	}

      /* The ext_data_... interface wants the file descriptor and not
       * a file handle.  Is ok, since we never use the file handle as
       * such, and thus do not interfere with its buffering.
       */

      empty_fd = fileno (empty_fid);

      client = ext_data_from_fd(empty_fd);

      if (client == NULL)
	{
	  perror("ext_data_connect");
	  fprintf (stderr,"Failed to use file descriptor input.\n");
	  exit(1);
	}

      printf ("Forked unpacker '%s'.\n",command);

      free (command);
    }
  else
    {
      /* Connect. */

      client = ext_data_connect(argv[1]);

      if (client == NULL)
	{
	  perror("ext_data_connect");
	  fprintf (stderr,"Failed to connect to server '%s'.\n",argv[1]);
	  exit(1);
	}

      printf ("Connected to server '%s'.\n",argv[1]);
    }

#if USE_ITEMS_INFO
  struct_info = ext_data_struct_info_alloc();
  if (struct_info == NULL)
    {
      perror("ext_data_struct_info_alloc");
      fprintf (stderr,"Failed to allocate structure information.\n");
      exit(1);
    }

  EXT_STR_h101_ITEMS_INFO(ok, struct_info, 0, EXT_EVENT_STRUCT, 0);
  if (!ok)
    {
      perror("ext_data_struct_info_item");
      fprintf (stderr,"Failed to setup structure information.\n");
      exit(1);
    }
#else
  (void) ok;
#endif

  if (ext_data_setup(client,
#if !USE_ITEMS_INFO
		     &event_layout,sizeof(event_layout),
		     NULL, NULL,
#else
		     NULL,0,
		     struct_info, &struct_map_success
#endif
		     sizeof(event),
		     "", NULL) != 0)
    {
      perror("ext_data_setup");
      fprintf (stderr,"Failed to setup with data from server '%s': %s\n",
	       argv[1],ext_data_last_error(client));
      /* Not more fatal than that we can disconnect. */
    }
  else
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

	  ret = ext_data_fetch_event(client,
				     &event,sizeof(event),
				     0);

	  if (ret == 0)
	    {
	      printf ("End from server.\n");
	      break; /* Out of data. */
	    }

	  if (ret == -1)
	    {
	      perror("ext_data_fetch_event");
	      fprintf (stderr,"Failed to fetch event: %s\n",
		       ext_data_last_error(client));
	      /* Not more fatal than that we can disconnect. */
	      break;
	    }

	  /* See if it has some raw data. */

	  ret = ext_data_get_raw_data(client, &raw, &raw_words);

	  if (ret != 0)
	    {
	      perror("ext_data_get_raw_data");
	      fprintf (stderr,"Failed to get raw data: %s\n",
		       ext_data_last_error(client));
              /* Not more fatal than that we can disconnect. */
              break;
	    }

	  if (raw)
	    {
	      const uint32_t *u = (const uint32_t *) raw;
	      ssize_t i, j;

	      printf ("RAW words: 0x%zx\n", raw_words);

	      for (i = 0; i < raw_words; i += 8)
		{
		  printf ("RAW%4zx:", i);
		  for (j = 0; j < 8 && i+j < raw_words; j++)
		    printf (" %08x", u[i+j]);
		  printf ("\n");
		}
	    }

	  /* Do whatever is wanted with the data. */
	  if (event.fatima_scaler_scalars == 16)
	  {
	    if (!fatima_scalers_init)
	    {
	      fatima_scalers_init = true;
	      for(int i = 0; i < 16; i++) fatima_scalers_last[i] = event.fatima_scaler_scalarsv[i] ;
	      printf("\x1B[KDESPEC Scalers initialised\n");
	    }

	    for (int i = 0; i < 16; i++)
	    {
	      int64_t delta = event.fatima_scaler_scalarsv[i] - fatima_scalers_last[i];
	      if (delta < 0) {
		delta += std::numeric_limits<uint32_t>::max();
	      }
	      fatima_scalers[i] += delta;
	      fatima_scalers_last[i] = event.fatima_scaler_scalarsv[i];
	    }
	  }

	  if (event.frs_frs_scaler_scalers == 32 && event.frs_main_scaler_scalers == 32)
	  {
	    if (!frs_scalers_init)
	    {
	      frs_scalers_init = true;
	      for(int i = 0; i < 32; i++) frs_scalers_last[i] = event.frs_frs_scaler_scalersv[i];
	      for(int i = 0; i < 32; i++) frs_scalers_last[32 + i] = event.frs_main_scaler_scalersv[i];
	      printf("\x1B[KFRS Scalers initialised\n");
	    }

	    for (int i = 0; i < 32; i++)
	    {
	      int64_t delta = (int64_t)event.frs_frs_scaler_scalersv[i] - (int64_t)frs_scalers_last[i];
	      if (delta < 0) {
		delta += std::numeric_limits<uint32_t>::max();
	      }
	      frs_scalers[i] += delta;
	      frs_scalers_last[i] = event.frs_frs_scaler_scalersv[i];
	    }

	    for (int i = 0; i < 32; i++)
	    {
	      int64_t delta = (int64_t)event.frs_main_scaler_scalersv[i] - (int64_t)frs_scalers_last[32 + i];
	      if (event.frs_main_scaler_scalersv[i] < frs_scalers_last[32 + i]) {
		/*printf("Overflow: delta = %lld\n", delta);*/
		/*printf("-> Now = %u, Last = %u\n", event.frs_main_scaler_scalersv[i], frs_scalers_last[32 + i]);*/
		/*delta += std::numeric_limits<uint32_t>::max();*/
	      }
	      frs_scalers[32 + i] += delta;
	      frs_scalers_last[32 + i] = event.frs_main_scaler_scalersv[i];
	    }
	  }

	  last_event_no = event.EVENTNO;

	  /* ... */
	}
    }

#if USE_ITEMS_INFO
  ext_data_struct_info_free(struct_info);
#endif

  /* Disconnect. */

  ret = ext_data_close(client);

  if (ret != 0)
    {
      perror("ext_data_close");
      fprintf (stderr,"Errors reported during disconnect: %s\n",
	       ext_data_last_error(client));
      exit(1);
    }

  if (empty_fid)
    {
      if (pclose(empty_fid) == -1)
	{
	  perror("pclose");
	  fprintf (stderr,"Errors reported from empty unpacker.\n");
	  exit(1);
	}
    }

  printf ("Disconnected from server.\n");

  float scale = fatima_scalers[15] / 2.0;
  for (int i = 0; i < 16; i++)
  {
    if (fatima_scalers_names[i] == "") continue;
    std::cout << "Scaler #" << std::setw(2) << i << ": " << std::setw(25) << fatima_scalers_names[i]
      << " : " << std::setw(16) << fatima_scalers[i]
      << " : " << std::setw(16) << (fatima_scalers[i] / scale) << std::endl;
  }

  std::cout << std::endl;

  scale = frs_scalers[35];
  for (int i = 0; i < 64; i++)
  {
    std::string name = "";
    if (i < 32) name =frs_frs_names[i];
    else name = frs_main_names[i - 32];
    if (name == "") continue;
    std::cout << "Scaler #" << std::setw(2) << i << ": " << std::setw(25) << name
      << " : " << std::setw(16) << frs_scalers[i]
      << " : " << std::setw(16) << (frs_scalers[i] / scale)
      << std::endl;
  }

  return 0;
}
