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
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "endian.hh"

#include "lmd_event.hh"

#define USE_LMD_INPUT 1
#define TDAS_CONV 1

#include "lmd_output.hh"

typedef unsigned char uchar;

#define NO_SUBEVENT_NAMES 1

#include "lmd_event.cc"
#include "lmd_output.cc"
#include "parse_util.cc"
#include "lmd_sticky_store.cc"
#include "forked_child.cc"
#include "limit_file_size.cc"
#include "select_event.cc"
#include "markconvbold.cc"
#include "colourtext.cc"
#include "error.cc"
#include "logfile.cc"



/* Quick'n dirty program to convert some TDAS files.  Specifically S107
 * (deutron-on-LAND data) from 1992.  No guarantees that it would work
 * for anything else.  Data format seems to be packed in LMD buffers
 * (GOOSY) somehow.  They are fragmented, but do not obey the
 * h_begin/h_end flags, and just continue in the next buffer, without
 * any special markings.
 *
 * The general plan is to just read buffers, with some minimalistic
 * buffer checking, and then dump the data on stdout, reformatted in a
 * way understandable by e.g. land02.
 */

// To get a list of files:
// for i in `ls /misc/scratch.land1/s034//lmd/s034.*` ; do echo -n "$i : " ; file_input/tdas_conv --only-tags $i 2> /dev/null | grep FileT ; done

// old way to create files
// for i in `ls /misc/scratch.land1/s034/lmd/s034.*` ; do echo "$i..." ; FILE=`echo $i.lmd | sed -e "s/.*\///" -e "s/\./_/"` ; echo /misc/scratch.land1/johansso/s034/lmd/$FILE ; file_input/tdas_conv $i --outname=/misc/scratch.land1/johansso/s034/lmd/$FILE ; done

// new way (using file names from file header)
// for i in `ls /misc/scratch.land1/s034/lmd/orig/s034.raw*` ; do echo $i ; file_input/tdas_conv $i --outprefix=/misc/scratch.land1/s034/lmd/ ; done
// for s034, the files *cos* and *las* are PARTS of the *raw* files,
// so no need to transfer them...

// extracting (and cleaning) comments:
// for i in `ls /net/data1/s107/lmd/orig/s034.raw*` ; do file_input/tdas_conv $i --fix-comments ; done

int get_buffer(FILE *fid,s_bufhe_host *bufhe,char **data,uint32 *data_left)
{
  *data_left = 0;

  if (fread(bufhe,sizeof(*bufhe),1,fid) != 1)
    return 0;

  uint32 buf_len = BUFFER_SIZE_FROM_DLEN(bufhe->l_dlen);
  /*
  printf ("Buffer: len=%d (%5d/%5d) %d\n",
	  buf_len,
	  bufhe->i_type,bufhe->i_subtype,
	  bufhe->l_buf);
  */
  uint32 data_size = buf_len - sizeof (*bufhe);

  *data = (char *) malloc (data_size);

  if (fread(*data,data_size,1,fid) != 1)
    return 0;

  uint32 buf_used = BUFFER_SIZE_FROM_DLEN(bufhe->i_used);

  *data_left = buf_used - sizeof (*bufhe);


  return 1;
}

char *frag = NULL;
int frag_alloc = 0;
int frag_len = 0;
int frag_done = 0;

char *outbuf = NULL;

lmd_output_file *out_file = NULL;

int camac_ev_counter_offset = 0;

int fetch_data(char **ptr,uint32 *data_left)
{
  if (frag_len > frag_done)
    {
      // printf ("get remainder (%d)...\n",frag_len-frag_done);

      int l = frag_len-frag_done;
      if (l > *data_left)
	l = *data_left;

      memcpy (frag+frag_done,*ptr,l);

      *ptr += l;
      *data_left -= l;
      frag_done += l;

      if (frag_len > frag_done)
	return 0;
    }
  else
    {
      // new (sub)event

      uint32 *p32 = (uint32 *) *ptr;

      uint32 len = EVENT_DATA_LENGTH_FROM_DLEN(p32[0]) + 8;
      /*
      printf ("l 0x%08x (%5d) tst 0x%08x : %5d\n",
	      p32[0],len,p32[1],p32[3]);
      */
      frag_len = len;

      if (frag_alloc < len)
	{
	  frag = (char*) realloc(frag,len + 32); // margin for bad events
	  outbuf = (char*) realloc(outbuf,len + 32); // margin for bad events
  	  frag_alloc = len;
	}

      if (len > *data_left)
	{
	  memcpy(frag,*ptr,*data_left);
	  frag_done = *data_left;
	  *data_left = 0;
	  return 0;
	}

      memcpy(frag,*ptr,len);
      *ptr += len;
      *data_left -= len;
      frag_done = frag_len = len;
    }

 has_subevent:
  // There is a event in our data now, print it...

  uint32* p32 = (uint32*) frag;
  uint32* e32 = (uint32*) (frag + frag_len);

  //printf ("Ev: %08x %08x %08x %08x\n",p32[0],p32[1],p32[2],p32[3]);

  uint32* s32 = p32+4;

  char *o = outbuf;

  lmd_event_out ev_out;

  memset(&ev_out,0,sizeof(ev_out));

  lmd_event_info_host *ev = NULL;

  if (p32[1] == 0x0026000a)
    {
      ev_out._header.i_type    = LMD_EVENT_10_1_TYPE;
      ev_out._header.i_subtype = LMD_EVENT_10_1_SUBTYPE;

      ev = (lmd_event_info_host *) o;

      ev->i_trigger = p32[2] & 0x0000ffff;
      ev->l_count   = p32[3];

      o = (char *) (ev + 1);
    }
  else
    {
      /*
      fprintf(stderr,"Unknown event header word: "
	      "0x%08x 0x%08x 0x%08x 0x%08x\n",
	      p32[0],p32[1],p32[2],p32[3]);
      */
    }

  lmd_subevent_10_1_host *fbsev = NULL;

  while (s32 < e32)
    {
      uint32 slen = EVENT_DATA_LENGTH_FROM_DLEN(s32[0]) + 8;

      //printf ("Sev: %08x %08x %08x (%d)\n",s32[0],s32[1],s32[2],slen);

      if (s32 + slen / 4 > e32)
	fprintf (stderr,"Subevent longer than event\n");
      else
	{
	  uint32 *se = s32 + slen / 4;
	  /*
	  int i = 0;
	  for (uint32 *p = s32 + 3; p < se; p++)
	    {
	      printf (" %08x",*p);
	      if (++i % 8 == 0)
		printf ("\n");
	    }
	  if (i % 8 != 0)
	    printf ("\n");
	  */
	  if (s32[1] == 0x0bb90022) // CAMAC
	    {
	      lmd_subevent_10_1_host *sev = (lmd_subevent_10_1_host *) o;

	      sev->_header.l_dlen = 0;
	      sev->_header.i_type    = 34;
	      sev->_header.i_subtype = 3200;

	      sev->h_control  = 9;
	      sev->h_subcrate = 0;
	      sev->i_procid   = 1;

	      o = (char *) (sev + 1);

	      /// Find the regions

	      int nreg = s32[2];

	      if (nreg != 2)
		fprintf (stderr,"Unexpected number of regions: %d\n",nreg);
	      if (s32[3] != 6)
		fprintf (stderr,"Unexpected region 1 start: %d\n",s32[3]);
	      if (s32[4] != 18)
		fprintf (stderr,"Unexpected region 2 start: %d\n",s32[4]);
	      if (s32[5] * 4 != slen)
		fprintf (stderr,"Unexpected regions end: %d\n",s32[5]);

	      ///

	      if (s32[6] & 0xffff0000)
		fprintf (stderr,"Junk in high bits of TPAT: %08x\n",s32[3]);

	      if (s32[7] - p32[3] != camac_ev_counter_offset)
		{
		  fprintf (stderr,"Ev and CAMAC event counters mismatch,"
			   " %08x != %08x\n",s32[7],p32[3]);
		  camac_ev_counter_offset = s32[7] - p32[3];
		}

	      ///

	      for (uint32 *p = s32 + 8; p < s32 + 18; p++)
		{
		  if (*p)
		    fprintf (stderr,"Non-zero word in region 1: %08x\n",
			     *p);
		}

	      // printf ("tpat: %04x\n",s32[3]);

	      uint16 *o16 = (uint16 *) o;

	      // printf ("tpat: %04x\n",s32[6] & 0x0000ffff);

	      // Other order due to fake swapping...
	      *(o16++) = 0;
	      *(o16++) = s32[6] & 0x0000ffff;
	      *(o16++) = 0x4321;
	      *(o16++) = 0;

	      for (uint32 *p = s32 + 18; p < se; p++)
		{
		  if (*p & 0xffff0000)
		    fprintf (stderr,"Junk in high bits of CAMAC word: %08x\n",
			     *p);

		  *(o16++) = *p;
		}

	      if (((char*) o16 - (char*) sev) & 3)
		*(o16++) = 0;

	      sev->_header.l_dlen =
		SUBEVENT_DLEN_FROM_DATA_LENGTH((char*) o16 - (char*) (sev+1));

	      o = (char *) o16;
	    }
	  else if (s32[1] == 0x0bb80020)
	    {
	      uint32 *o32 = (uint32 *) o;

	      if (!fbsev)
		{
		  fbsev = (lmd_subevent_10_1_host *) o;

		  fbsev->_header.l_dlen = 0;
		  fbsev->_header.i_type    = 32;
		  fbsev->_header.i_subtype = 3100;

		  fbsev->h_control  = 0;
		  fbsev->h_subcrate = 0;
		  fbsev->i_procid   = 1;

		  o = (char *) (fbsev + 1);

		  o32 = (uint32 *) o;

		  *(o32++) = 0; // first fabu data word (error marker...)
		}

	      if (s32[3])
		fprintf (stderr,"First FB data word not 0: %08x.\n", s32[3]);

	      for (uint32 *p = s32 + 4; p < se; p++)
		{


		  *(o32++) = *p;
		}

	      fbsev->_header.l_dlen =
		SUBEVENT_DLEN_FROM_DATA_LENGTH((char*) o32 - (char*) (fbsev+1));

              o = (char *) o32;
	    }
	  else
	    {
	      /*
	      fprintf(stderr,"Unknown sebevent header word: "
		      "0x%08x 0x%08x 0x%08x\n",
		      s32[0],s32[1],s32[2]);
	      */
	    }

	}

      s32 += slen / 4;
    }

  if (ev)
    {
      ev_out.add_chunk(outbuf,o-outbuf,false);

      if (out_file)
	out_file->write_event(&ev_out);
    }

  return 1;
}

#define mymin(a,b) ((a)<(b)?(a):(b))

#define INFO_STRING_L2(name,str,maxlen,len) {	\
    char tmp[maxlen+1];				\
    memcpy (tmp,str,maxlen);			\
    tmp[mymin(maxlen,len)] = 0;			\
    printf("  %-8s %s\n",name,tmp);		\
 }
#define INFO_STRING_L(name,str,maxlen) \
  INFO_STRING_L2(name,str,maxlen,(unsigned int) str##_l)
#define INFO_STRING(name,str) \
  INFO_STRING_L(name,str,sizeof(str))

#define TAG_STRING_L2(str,maxlen,len) {         \
    char tmp[maxlen+1];                         \
    memcpy (tmp,str,maxlen);                    \
    tmp[mymin(maxlen,len)] = 0;                 \
    printf(" %-*s",maxlen,tmp);                 \
 }
#define TAG_STRING_L(str,maxlen) \
  TAG_STRING_L2(str,maxlen,(unsigned int) str##_l)

void dump_file_header(const s_filhe_extra_host *header)
{
  INFO_STRING("Label",   header->filhe_label);
  INFO_STRING("File",    header->filhe_file);
  INFO_STRING("User",    header->filhe_user);
  INFO_STRING_L2("Time", header->filhe_time,24,24);
  INFO_STRING("Run",     header->filhe_run);
  INFO_STRING("Exp",     header->filhe_exp);
  for (int i = 0; i < mymin(30,header->filhe_lines); i++)
    INFO_STRING("Comment",header->s_strings[i].string);

  printf ("\nFileTags:");
  TAG_STRING_L2(header->filhe_time,24,24);
  TAG_STRING_L(header->filhe_label,12);
  TAG_STRING_L(header->filhe_file,12);
  TAG_STRING_L(header->filhe_run,6);
  TAG_STRING_L(header->filhe_exp,6);
  printf ("\n");
}

void dump_comments(const s_filhe_extra_host *header)
{
  unsigned char filename[87];

  memcpy(filename,header->filhe_file,86);
  filename[86] = 0;

  unsigned char timestr[25];

  memcpy(timestr,header->filhe_time,24);
  timestr[24] = 0;

  printf ("%s: %s: ",filename,timestr);

  char fmt_cmt[4096];
  memset(fmt_cmt,0,sizeof(fmt_cmt));

  char *d = fmt_cmt;

  for (int i = 0; i < mymin(30,header->filhe_lines); i++)
    {
      unsigned char comment[79];

      memcpy(comment,header->s_strings[i].string,78);
      comment[78] = 0;

      if (i == 0 &&
	  strncmp((char *) comment,"38 - experiment number for peripheral collisions",48) == 0)
	continue;
      if (i == 1 &&
	  strncmp((char *) comment,"!",1) == 0)
	continue;

      unsigned char *s = comment;

      // printf ("::");

      while (*s)
	{
	  if (s[0] == 0x9b && s[1])
	    {
	      // printf ("<%x>",s[1]);
	      switch (s[1])
		{
		case 'D':
		  if (d > fmt_cmt)
		    {
		      d--;
		      s += 2;
		    }
		  else
		    {
		      *(d++) = '.';
		      /*
		      strcpy(d,"<@D>");
		      d += 4;
		      */
		      s += 2;
		    }
		  continue;
		case 'C':
		  if (*d)
		    {
		      d++;
		      s += 2;
		    }
		  else
		    {
		      *(d++) = '.';
		      /*
		      strcpy(d,"<@C>");
		      d += 4;
		      */
		      s += 2;
		    }
		  continue;
		default:
		  *(d++) = '.';
		  /*
		  strcpy(d,"<@.>");
		  d[2] = s[1];
		  d += 4;
		  */
		  s += 2;
		  continue;
		}
	    }
	  /*
	  if (isprint(s[0]))
	    printf ("%c",s[0]);
	  else
	    printf ("[%x]",s[0]);
	  */
	  if (isprint(s[0]))
	    *(d++) = *s;
	  else
	    {
	      *(d++) = '.';
	      /*
	      d += sprintf (d,"\\x%x",*(s++));
	      */
	    }
	  s++;
	}
    }
  //printf ("\n[%s]\n",fmt_cmt);

  printf ("%s",fmt_cmt);

  printf ("\n");
}

int main(int argc,char *argv[])
{
  FILE *infile = NULL;
  int only_tags = 0;
  int fix_comments = 0;

  char *outname = NULL;
  char *outprefix = NULL;

  for (int i = 0; i < argc; i++)
    {
      if (strcmp(argv[i],"--only-tags") == 0)
	only_tags = 1;
      else if (strcmp(argv[i],"--fix-comments") == 0)
	fix_comments = 1;
      else if (strncmp(argv[i],"--outname=",10) == 0)
	outname = argv[i]+10;
      else if (strncmp(argv[i],"--outprefix=",12) == 0)
	outprefix = argv[i]+12;
      else
	{
	  infile = fopen(argv[i],"r");

	  if (infile == NULL)
	    {
	      perror("fopen");
	      fprintf (stderr,"Failed to open: %s\n",argv[i]);
	      exit(1);
	    }
	}
    }

  if (!infile)
    {
      fprintf (stderr,"No input file!\n");
      exit(1);
    }

  if (outname)
    {
      out_file = new lmd_output_file();
      out_file->open_file(outname);
      out_file->_write_native = false; // simulate powerpc (on x86)
    }

  int total_events = 0;
  int total_size = 0;

  int last_mb = 0;

  while (!feof(infile))
    {
      s_bufhe_host bufhe;
      char *data;
      uint32 data_left;

      if (!get_buffer(infile,&bufhe,&data,&data_left))
	break;

      total_size += data_left;

      if (bufhe.i_type == 2000 &&
	  bufhe.i_subtype == 1)
	{
	  // We have a file header.  Dump it.

	  if (fix_comments)
	    {
	      dump_comments((s_filhe_extra_host*) data);
	      exit(0);
	    }

	  dump_file_header((s_filhe_extra_host*) data);

	  if (only_tags)
	    exit(0);

	  if (outprefix)
	    {
	      out_file = new lmd_output_file();

	      char filename[1024];
	      char name[128];

	      s_filhe_extra_host* filhe = (s_filhe_extra_host*) data;

	      memcpy(name,filhe->filhe_file,filhe->filhe_file_l);
	      name[filhe->filhe_file_l] = 0;

	      for (int i = 0; i < sizeof(name); i++)
		name[i] = tolower(name[i]);

	      sprintf (filename,"%s%s",outprefix,name);

	      printf ("Output file name: %s\n",filename);

	      out_file->open_file(filename);
	      out_file->_write_native = false; // simulate powerpc (on x86)
	    }

	  if (out_file)
	    out_file->write_file_header((s_filhe_extra_host*) data);

	  continue;
	}

      if (bufhe.i_type == 10103 &&
	  bufhe.i_subtype == 1)
	{
	  char *ptr = data;
	  int events = 0;

	  while (data_left)
	    {
	      events += fetch_data(&ptr,&data_left);

	    }

	  // printf ("%d\n",events);

	  total_events += events;
	}
      else
	fprintf (stderr,"Unknown buffer.\n");

      int mb = total_size >> 20;

      if (mb != last_mb)
	{
	  last_mb = mb;
	  printf ("Processed: %d (%d MB)\r",
		  total_events,mb);
	  fflush(stdout);
	}

      free(data);
    }

  printf ("\nTotal processed: %d (%.1f MB)\n",
	  total_events,total_size / 1.e6);

  if (out_file)
    delete out_file;
}

void report_open_close(bool open, const char* filename,
                       uint64 size, uint32 events)
{
}
