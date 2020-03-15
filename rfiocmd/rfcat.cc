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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef USE_RAWAPI
#include "rawapin.h"
#else
#include "rfio_api.h"
#endif

/* *** */

#include "../lu_common/colourtext.hh"
#ifdef USE_CURSES
#include "../lu_common/colourtext.cc"
#endif

const char *_argv0 = NULL;

#define MSG(...) do {							\
    fprintf(stderr,"%s%s:%s ",CT_ERR(BLUE),_argv0,CT_ERR(DEF_COL));	\
    fprintf(stderr,__VA_ARGS__);					\
    fputc('\n',stderr);							\
  } while (0)

#define WARN_MSG(...) do {						\
    fprintf(stderr,"%s%s:%s ",CT_ERR(BLUE),_argv0,CT_ERR(DEF_COL));	\
    fprintf(stderr,"%s",CT_ERR(BLACK_BG_YELLOW));			\
    fprintf(stderr,__VA_ARGS__);					\
    fprintf(stderr,"%s",CT_ERR(DEF_COL));				\
    fputc('\n',stderr);							\
  } while (0)

#define ERR_MSG(...) do {						\
    fprintf(stderr,"%s%s:%s ",CT_ERR(BLUE),_argv0,CT_ERR(DEF_COL));	\
    fprintf(stderr,"%s",CT_ERR(WHITE_BG_RED));				\
    fprintf(stderr,__VA_ARGS__);					\
    fprintf(stderr,"%s",CT_ERR(DEF_COL));				\
    fputc('\n',stderr);							\
    exit(1);								\
  } while(0)

/* *** */

#ifdef USE_RAWAPI

extern FILE* fLogFile;
extern FILE* fLogClient;

void rfio_setup_logfile()
{
  assert(fLogFile == NULL);
  assert(fLogClient == NULL);

  if ((fLogFile = fopen("/dev/null","w")) == NULL)
    ERR_MSG("Failure opening /dev/null to dump rfio messages.");

  fLogClient = fLogFile;
}

int rfio_query_media_status(const char *filename)
{
  int cache_stat = rfio_cache_stat(filename);

  // printf ("cache_stat: %s %d\n",filename,cache_stat);

  if (cache_stat == 1)
    return 1;
  if (cache_stat == 0)
    {
      WARN_MSG("Rfio file '%s' in offline storage.  Refusing to use it.  "
	       "Staging required.",
	       filename);
      return 0;
    }
  if (cache_stat == 2)
    {
      WARN_MSG("Rfio file '%s' in write cache (disk).  Staging recommended.",
	       filename);
      return 2;
    }
  // (cache_stat < 0) and other errors

  WARN_MSG("Rfio file '%s' not in gstore or other error.",
	   filename);
  return -1;
}
#endif//USE_RAWAPI

/* *** */

#define BUFSIZE (4*1024*1024)

char buf[BUFSIZE];

int main(int argc,char *argv[])
{
  int total = 0;
  _argv0 = argv[0];

  colourtext_init();

  int out_fd = STDOUT_FILENO;

#ifdef USE_RAWAPI
  rfio_setup_logfile();

  /* Redirect any output on stdout to /dev/null.  But first get hold
   * of stdout so we can write the file there.
   */

  out_fd = dup(STDOUT_FILENO);

  if (out_fd == -1)
    ERR_MSG("Failed to dup() stdout.");

  int divert_fd = dup2(fileno(fLogFile), STDOUT_FILENO);

  if (divert_fd == -1)
    ERR_MSG("Failed to dup2() diversion for stdout.");
#endif

  int debug = 0;
  bool test_status_only = false;

  bool exist_status_ok = true;
  bool media_status_ok = true;

  for (int i = 1; i < argc; i++)
    {
      if (strcmp(argv[i], "--check-media-status") == 0)
	{
	  test_status_only = true;
	  continue;
	}
      if (strcmp(argv[i], "--debug") == 0)
	{
	  debug = 1;
	  continue;
	}

      int cache_stat;
      int fd;
      //RFILE *fid;

      const char *filename = NULL;

      filename = argv[i];

      int media_status = rfio_query_media_status(filename);

      if (media_status < 0)
	exist_status_ok = false;
      if (media_status == 0)
	media_status_ok = false;

      if (test_status_only)
	continue;
      else if (!exist_status_ok)
	ERR_MSG("RFIO file not EXISTING, exit.");
      else if (!media_status_ok)
	ERR_MSG("RFIO file should be STAGED before usage, exit.");

      if (debug)
	MSG("Opening: %s",filename);

      // char fname[1024];
      // strcpy(fname,filename);
      // char mode[16];
      // strcpy(mode,"r");
      // fid = rfio_fopen(fname,mode);

      /* The libshift man page claim const char*, prototype is char * :-( */
      char *filename_dup = strdup(filename);
      fd = rfio_open(filename_dup,O_RDONLY,0/* -1 = query only */);
      free(filename_dup);

      /*
      cache_stat = rfio_cache_stat(filename);

      fprintf (stderr,"Cache-stat: %s : %d\n",filename,cache_stat);
      */

      if (fd < 0)
	{
	  ERR_MSG("Failure opening %s: %s",
		  filename,rfio_serror());
	  exit(1);
	}

      if (debug)
	MSG("File opened (fd = %d), start reading...",fd);

      // if (debug)
      //   MSG("File opened, start reading...");

      for ( ; ; )
	{
	  char status[4096];
	  memset(status,' ',sizeof(status));
	  status[4095] = 0;
	  /*
	  rfio_gsi_query(fd, 1, status);

	  fprintf (stderr,"status: %s",status);
	  */

	  // int n = rfio_fread(buf,1,BUFSIZE,fid);

	  int n = rfio_read(fd,buf,BUFSIZE);
	  /*
	  cache_stat = rfio_cache_stat(filename);

	  fprintf (stderr,"Cache-stat: %s : %d",filename,cache_stat);
	  */
	  if (n == 0)
	    {
	      break;
	    }

	  if (n < 0)
	    {
	      ERR_MSG("Failure reading %s: %s",
		      filename,rfio_serror());
	    }

	  char *p = buf;
	  ssize_t done = 0;

	  for ( ; done < n; )
	    {
	      ssize_t nn = write (out_fd, p + done, n - done);

	      if (n == -1)
		{
		  if (errno == EAGAIN ||
		      errno == EINTR)
		    continue;

		  ERR_MSG("Failure writing.");
		}

	      done += nn;
	    }

	  total += n;

	  if (debug)
	    MSG("Got: %d bytes... (%d)",n,total);
	}

      if (debug)
	MSG("Done reading, close file...");

      // int ret = rfio_fclose(fid);

      int ret = rfio_close(fd);

      if (ret != 0)
	{
          ERR_MSG("Failure closing %s: %s",
		  filename, rfio_serror());
	}
    }

  return 0 | (exist_status_ok ? 0 : 2) | (media_status_ok ? 0 : 4);
}

