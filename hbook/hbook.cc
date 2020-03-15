/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
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

#include "hbook.hh"
#ifndef DONT_INCLUDE_ERROR_HH
#ifdef LEG2
#include "../error.hh"
#else
#include "error.hh"
#endif
#endif

///////////////////////////////////////////////////////////////////////

#ifdef USING_CERNLIB

///////////////////////////////////////////////////////////////////////

#include <cfortran.h>
#include <packlib.h>

///////////////////////////////////////////////////////////////////////
//
// PAWC environment

#define PAWC_SIZE 10000000

// LLVM did not like alignment 16.  64 seems fine with gcc 4.9
#define PAWC_ALIGNMENT  64

typedef struct
{
  float PAW[PAWC_SIZE]  __attribute__ ((aligned (PAWC_ALIGNMENT)));
} PAWC_DEF;
#define PAWC COMMON_BLOCK(PAWC,pawc)
//COMMON_BLOCK_DEF(PAWC_DEF,PAWC);
PAWC_DEF PAWC;

typedef struct
{
  int iquest[100] __attribute__ ((aligned (PAWC_ALIGNMENT)));
} quest_def;
#define QUEST COMMON_BLOCK(QUEST,quest)
//COMMON_BLOCK_DEF(quest_def,QUEST);
quest_def QUEST;

///////////////////////////////////////////////////////////////////////
//
// PAWC environment

static bool set_pawc_size = false;

hbook::hbook()
{
  _open = false;

  if (!set_pawc_size)
    {
      HLIMIT(PAWC_SIZE);
      set_pawc_size = true;
    }
}

///////////////////////////////////////////////////////////////////////
//
// Open/close files.
void hbook::open(const char *filename,const char* top,
		 int record_length)
{
  // Constants are constants and nothing else.  Rotten fortran.

  char chfile[256];
  char chopt[] = "NQE";
  // with Q or QE iquest[9] (iquest(10)) will be used to set maximum
  // number of records.  Default is 32000.  with Q one can go to 1<<16 (64000 etc)
  // but that's only a factor of 2.  Going to QE immediately!

  // 32000 records * 8191 record size is 262144000 about 260 MB
  // let's later change the record size to 4096 so that paw auto detection works
  // anyhow, 500000 * 4096 is 2 GB (almost).  I hope noone wants larger ntuples :-)

  strcpy (chfile,filename);
  strcpy (_chtop,top);

  int record_size = record_length; // default is 8191 (stupid perhaps)
  int istat;

  QUEST.iquest[9] = 500000; // half a million (should limit at slightly below 2GB)
  HROPEN(1,_chtop,chfile,chopt,record_size,istat);

  if (istat != 0)
    ERROR("Failure doing HROPEN on %s.",chfile);

  _open = true;
}

void hbook::close()
{
  if (!_open)
    return;

  int icycle;

  char chopt[] = " ";

  char chtop[64];

  strcpy (chtop,"//");
  strcat (chtop,_chtop);

  HCDIR(chtop,chopt);
  HROUT(0,icycle,chopt);
  HREND(_chtop);
  KUCLOS(1,chopt,1);

  _open = false;
}

///////////////////////////////////////////////////////////////////////
//
// Row-wise Ntuple (RWN)

void hbook_ntuple::hbookn(int hid,
			  const char *title,
			  const char *rzpa,
			  char tags[][9],
			  int entries)
{
  _hid = hid;

  char chtitle[256]; //  = "An RWNtuple";
  char chrzpa[256];  //   = "//example";

  strcpy(chtitle,title);
  strcpy(chrzpa,rzpa);

  assert(entries <= 512);

  /* For some reason, if one sends tags to HBOOKN, all names are
   * garbled.  So i make a copy...
   *
   * Should really investigate cfortran.h to see what is going on.  Did
   * that, and any workaroung would be even more painful (if possible..)
   */

  char copy_tags[512][9];

  memcpy (copy_tags,tags,sizeof (copy_tags[0]) * entries);

  //for (int i = 0; i < entries; i++)
  //  INFO(0,"Entry %d: %s %s",i,tags[i],copy_tags[i]);

  HBOOKN(_hid,chtitle,entries,chrzpa,200000/*nwprim/nwbuff*/,copy_tags);
}

void hbook_ntuple::hfn(float *array)
{
  HFN(_hid,array);
}

///////////////////////////////////////////////////////////////////////
//
// Column-wise Ntuple (CWN)

void hbook_ntuple_cwn::hbset_bsize(int bsize)
{
  char chopt[] = "BSIZE";
  int ierr;

  //HBSET ("BSIZE",4096,ierr);
  HBSET(chopt,bsize,ierr);

  if (ierr != 0)
    ERROR("Failure doing HBSET BSIZE.");
}

void hbook_ntuple_cwn::hbnt(int hid,
			    const char *title,
			    const char *opt)
{
  _hid = hid;

  char chtitle[256];
  char chopt[256];

  strcpy(chtitle,title);
  strcpy(chopt,opt);

  //  HBNT(NT,"NTUPLE"," ");
  HBNT(_hid,chtitle,chopt);
}

void hbook_ntuple_cwn::hbname(const char *block,
			      void *structure,
			      const char *vars)
{
  char chblock[256];
  char chvars[1024];

  strcpy(chblock,block);
  strcpy(chvars,vars);

  HBNAME(_hid,chblock,*((char *) structure),chvars);
}

void hbook_ntuple_cwn::hfnt()
{
  HFNT(_hid);
}

///////////////////////////////////////////////////////////////////////
//
// Histogram

void hbook_histogram::hbook1(int hid,
			     const char *content,
			     int bins,
			     float min,
			     float max)
{
  _hid = hid;

  char chtitl[256];

  strcpy(chtitl,content);

  HBOOK1(_hid,chtitl,bins,min,max,0.);
}

// This function is called hfill1, since we intend to fill an one-
// dimensional histogram, and the dummy y argument is left out.
void hbook_histogram::hfill1(float x,float w)
{
  HFILL(_hid,x,0.,w);
}

void hbook_histogram::hrout()
{
  int icycle;

  char chopt[] = " ";

  HROUT(_hid,icycle,chopt);
}

///////////////////////////////////////////////////////////////////////

// This makes sure that we do not see a trillion messages
// from hbook about NaN in our ntuples.

extern "C"
{
  int hfpbug_()
  {
    static bool complained = false;

    if (!complained)
      {
	WARNING("Complaints from HFPBUG silently ignored!");
	complained = true;
      }
    return 0;
  }
}

///////////////////////////////////////////////////////////////////////

#if USING_OLD_LIBC_CERNLIB
// Fixes to be able to use old cernlib (pcepisdaq3 local)

#include <ctype.h>

extern "C" {

  __const unsigned short int **__ctype_b (void) {
    return __ctype_b_loc();
  }

  __const __int32_t **__ctype_tolower (void) {
    return __ctype_tolower_loc();
  }

  __const __int32_t **__ctype_toupper (void) {
    return __ctype_tolower_loc();
  }
}
#endif

///////////////////////////////////////////////////////////////////////

#else

///////////////////////////////////////////////////////////////////////

hbook::hbook() { }
void hbook::open(const char */*filename*/,const char */*top*/, int /*record_length*/) { }
void hbook::close() { }

void hbook_ntuple::hbookn(int /*hid*/,
			  const char */*title*/,
			  const char */*rzpa*/,
			  char /*tags*/[][9],
			  int /*entries*/) { }
void hbook_ntuple::hfn(float */*array*/) { }

void hbook_histogram::hbook1(int /*hid*/,
			     const char */*content*/,
			     int /*bins*/,
			     float /*min*/,
			     float /*max*/) { }
void hbook_histogram::hfill1(float /*x*/,float /*w*/) { }
void hbook_histogram::hrout() { }

///////////////////////////////////////////////////////////////////////

#endif // USING_CERNLIB





