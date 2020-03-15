/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2018  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "external_writer.hh"
#include "error.hh"

#include <arpa/inet.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#ifndef EXT_STRUCT_CTRL
# define EXT_STRUCT_CTRL(x)
#endif

external_writer *ew;

struct mystruct
{
  int32_t a;
  float c;
  int32_t b;
  float d[4 EXT_STRUCT_CTRL(b)];
  float e[7];
};

struct secondstruct
{
  int32_t g;
  float h;
};

/* The code for the below can be obtained from:
 *
 * ../make_external_struct_sender.pl --struct=mystruct < ext_writer_test.cc
 *
 * Which even better would be invoked in the build process.
 * But not in this example.
 */

void send_offsets_mystruct(external_writer *ew)
{
  struct mystruct event;

  ew->send_book_ntuple_x(99,"h99","TestTree");

  ew->send_alloc_array(sizeof(mystruct));

  ew->send_hbname_branch("DEF",offsetof(mystruct,a),sizeof(event.a),
			 "a",-1,"",EXTERNAL_WRITER_FLAG_TYPE_INT32);
  ew->send_hbname_branch("DEF",offsetof(mystruct,c),sizeof(event.c),
			 "c",-1,"",EXTERNAL_WRITER_FLAG_TYPE_FLOAT32);
  ew->send_hbname_branch("DEF",offsetof(mystruct,b),sizeof(event.b),
			 "b",-1,"",EXTERNAL_WRITER_FLAG_TYPE_INT32);
  ew->send_hbname_branch("DEF",offsetof(mystruct,d),sizeof(event.d),
			 "d",4,"b",EXTERNAL_WRITER_FLAG_TYPE_FLOAT32);
  ew->send_hbname_branch("DEF",offsetof(mystruct,e),sizeof(event.e),
			 "e",7,"",EXTERNAL_WRITER_FLAG_TYPE_FLOAT32);

  uint32_t offset_msg_size = (1+1+1+(2)+4+7) * (uint32_t) sizeof(uint32_t);
  uint32_t fill_msg_size = (1 + (1+1+1+4+7)) * (uint32_t) sizeof(uint32_t);

  uint32_t maxmsgsize =
    fill_msg_size > offset_msg_size ?
    fill_msg_size : offset_msg_size;

  ew->set_max_message_size(maxmsgsize);

  {
    uint32_t *o = ew->prepare_send_offsets(offset_msg_size);

    *(o++) = htonl((uint32_t) offsetof(mystruct,a) | 0x40000000);
    *(o++) = htonl((uint32_t) offsetof(mystruct,c));
    *(o++) = htonl((uint32_t) offsetof(mystruct,b) | 0x80000000 | 0x40000000);
    *(o++) = htonl(4);
    *(o++) = htonl(1);
    for (int l = 0; l < 4; l++) {
      *(o++) = htonl((uint32_t) ((offsetof(mystruct,d)+l*sizeof(float))));
    }
    for (int i = 0; i < 7; i++)
      *(o++) = htonl((uint32_t) ((offsetof(mystruct,e)+i*sizeof(float))));
    ew->send_offsets_fill(o);
  }
}

void send_fill_mystruct(external_writer *ew,
                       const mystruct &s,
                       uint32_t ntuple_index = 0)
{
  uint32_t fill_msg_size = (1 + (1+1+1+4+7)) * (uint32_t) sizeof(uint32_t);

  uint32_t *p = ew->prepare_send_fill_x(fill_msg_size, 0, ntuple_index);

  *(p++) = htonl(0x40000000); /* marker */

  {
    *(p++) = htonl((s.a));
    *(p++) = htonl(external_write_float_as_uint32(s.c));
    {
      uint32_t loop_n_b = s.b;
      assert (loop_n_b <= 4);
      *(p++) = htonl(loop_n_b);
      for (int l = 0; l < loop_n_b; l++) {
	*(p++) = htonl(external_write_float_as_uint32(s.d[l]));
      }
    }
    for (int i = 0; i < 7; i++)
      *(p++) = htonl(external_write_float_as_uint32(s.e[i]));
  }

  ew->send_offsets_fill(p);
}

void send_offsets_secondstruct(external_writer *ew)
{
  struct secondstruct event;

  ew->send_book_ntuple_x(98,"h98","TestTree2","","",1);

  ew->send_alloc_array(sizeof(secondstruct));

  ew->send_hbname_branch("DEF",offsetof(secondstruct,g),
                         sizeof(/*secondstruct.g*/int32_t),
                         "g",-1,"",EXTERNAL_WRITER_FLAG_TYPE_INT32);
  ew->send_hbname_branch("DEF",offsetof(secondstruct,h),
                         sizeof(/*secondstruct.h*/float),
                         "h",-1,"",EXTERNAL_WRITER_FLAG_TYPE_FLOAT32);

  uint32_t offset_msg_size = (2 + 2 * 0) * (uint32_t) sizeof(uint32_t);
  uint32_t fill_msg_size = (1 + 2) * (uint32_t) sizeof(uint32_t);

  ew->set_max_message_size(fill_msg_size > offset_msg_size ?
                           fill_msg_size : offset_msg_size);

  {
    uint32_t *o = ew->prepare_send_offsets(offset_msg_size);

    *(o++) = htonl((uint32_t) offsetof(secondstruct,g) | 0x40000000);
    *(o++) = htonl((uint32_t) offsetof(secondstruct,h));

    ew->send_offsets_fill(o);
  }
}

void send_fill_secondstruct(external_writer *ew,
			    const secondstruct &s,
			    uint32_t ntuple_index = 0)
{
  uint32_t fill_msg_size = (1 + 2) * (uint32_t) sizeof(uint32_t);

  uint32_t *p = ew->prepare_send_fill_x(fill_msg_size,1,ntuple_index,
					NULL,NULL,0);

  *(p++) = htonl(0x40000000); // marker that we are not compacted

  *(p++) = htonl((s.g));
  *(p++) = htonl(external_write_float_as_uint32(s.h));

  ew->send_offsets_fill(p);
}

int main(int argc, char *argv[])
{
  main_argv0 = argv[0];

  unsigned int type = 0;
  unsigned int opt = NTUPLE_OPT_WRITER_NO_SHM;
  const char *filename = NULL;
  bool generate_header = false;

  for (int i = 1; i < argc; i++)
    {
      char *post;

#define MATCH_PREFIX(prefix,post) (strncmp(argv[i],prefix,strlen(prefix)) == 0 && *(post = argv[i] + strlen(prefix)) != '\0')
#define MATCH_ARG(name) (strcmp(argv[i],name) == 0)

      if (MATCH_PREFIX("--root=",post)) {
	filename = post;
	type = NTUPLE_TYPE_ROOT;
      }
      else if (MATCH_PREFIX("--cwn=",post)) {
	filename = post;
	type = NTUPLE_TYPE_CWN;
      }
      else if (MATCH_PREFIX("--struct=",post)) {
	filename = post;
	type = NTUPLE_TYPE_STRUCT;
      }
      else if (MATCH_PREFIX("--struct_hh=",post)) {
	filename = post;
	type = NTUPLE_TYPE_STRUCT_HH;
	generate_header = true;
      }
    }

  if (!type)
    ERROR("No output type selected.");

  ew = new external_writer();

  ew->init_x(type | NTUPLE_CASE_KEEP, opt,
	     filename,"Title",-1,generate_header,
	     false,false,false);

  ew->send_file_open(0);

  send_offsets_mystruct(ew);

  send_offsets_secondstruct(ew);

  ew->send_setup_done();

  if (!generate_header)
    {
      /* loop over events */
      for (int i = 0; i < 12345; i++)
	{
	  struct mystruct s;

	  memset(&s, 0, sizeof (s));

	  s.a = i;
	  s.c = (float) (i * 0.1);
	  s.b = i % 3;
	  for (int j = 0; j < s.b; j++)
	    s.d[j] = (float) (((i % 7) + j) * 10);
	  s.e[0] = (float) (i * 0.01);
	  s.e[1] = (float) (i * 0.01 + 0.001);

	  send_fill_mystruct(ew, s, 0);

	  if (i % 99 == 0)
	    {
	      struct secondstruct s2;

	      memset(&s2, 0, sizeof (s2));

	      s2.g = i;
	      s2.h = (float) (i * 0.01);

	      send_fill_secondstruct(ew, s2, 0);
	    }
	}
    }

  ew->close(); // not required, done in destructor below

  delete ew;

  return 0;
}



/* Code that would normally be elsewhere, but that we for simplicity
 * just compile together.
 */

#define EXTERNAL_WRITER_NO_SHM 1

#include "external_writer.cc"
#include "forked_child.cc"
#include "markconvbold.cc"
#include "colourtext.cc"
#include "error.cc"
