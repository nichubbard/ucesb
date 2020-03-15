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

#include "structures.hh"

#include "calib_param.hh"

#include "user.hh"

#include "corr_plot.hh"

#include <limits.h>

//////////////////////////////////////////////////////////////////////

#include "is446_repack.hh"

#define USE_IS446_UNPACK 1
#define USE_IS446_NTUPLE 1

#if !USE_CERNLIB
#undef  USE_IS446_NTUPLE
#endif

#if USE_IS446_UNPACK
is446_unpack _state;
#define INVALIDATE_SCALER_STATE _state.invalidate_scaler_state();
int _dump_ebis_clear = 0;
#if USE_IS446_NTUPLE
const char* _fortntu_name = NULL;
const char *_fixed_tebis_name = NULL;
FILE       *_fixed_tebis_fid = NULL;
struct fixed_tebis_info
{
  int mbs_ec;
  int tshort;
  int tebis;
};
fixed_tebis_info _fixed_tebis_info;
#endif
#else
#define INVALIDATE_SCALER_STATE
#endif

//////////////////////////////////////////////////////////////////////

#include <vector>

#define CHECK_EXPECT_EC 0
#define DUMP_EC_COUNTS  0

#define NR_OF_ADCS 7
#define NR_OF_TDCS 5

#if CHECK_EXPECT_EC

int expect_event_no = 0;

int expect_scaler0 = 0;

int expect_adc[NR_OF_ADCS] = { 0, 0, 0, 0, 0, 0, 0,};
int expect_tdc[NR_OF_TDCS] = { 0, 0, 0, 0, 0,};

int diff_adc[NR_OF_ADCS] = { 0, 0, 0, 0, 0, 0, 0,};
int diff_tdc[NR_OF_TDCS] = { 0, 0, 0, 0, 0,};

int expect_diff_adc_tdc[4] = { 0,0,0,0, };

#define SYNCPRINTF(x) printf x
//#define SYNCPRINTF(x) (void) 0

#define EE_FLAG_READOUT      0x01
#define EE_FLAG_EC_CORRECT   0x02
#define EE_FLAG_EC_INCORRECT 0x04
#define EE_FLAG_EC_UNEXP     0x08

struct ec_event
{
  union
  {
    int all[14]; // event_no + scaler0 + adc[7] + tdc[7]
    struct
    {
      int no;
      int scaler;
      int adc[7];
      int tdc[5];
    } p;
  } ec;

  char flags[14];

  std::vector<unsigned short> fired;
};

std::vector<ec_event*> events;

bool missing_count(int current,int &expect,bool zero_is_reset)
{
  bool consistent = true;

  if (zero_is_reset)
    {
      SYNCPRINTF (("%02x",
		   current % 0xff));
      if (current - expect && current)
	{
	  SYNCPRINTF ((":%d ",
		       current - expect));
	  consistent = false;
	}
      else
	SYNCPRINTF (("   "));
      expect = current + 1;
    }
  else
    {
      if (current)
	{
	  SYNCPRINTF (("%02x",
		       current % 0xff));
	  if (current - expect)
	    {
	      SYNCPRINTF ((":%d ",
			   current - expect));
	      consistent = false;
	    }
	  else
	    SYNCPRINTF (("   "));
	  expect = current + 1;
	}
      else
	{
	  SYNCPRINTF (("     "));
	  expect++;
	}
    }
  return consistent;
}
#endif

/**************************************************************/

#define TIMING_MATCH_NTU 0

#define CORR_PICTURES 0

#if CORR_PICTURES
ppcorr<NR_OF_ADCS+NR_OF_TDCS,32>   dam_corr;
ppcorr<NR_OF_ADCS+NR_OF_TDCS,32>   dam_corr_m[16];
#endif

int compare_int(const void *p1,const void *p2)
{
  return *((const int*) p1) - *((const int*) p2);
}

#if TIMING_MATCH_NTU

#include "hbook.hh"

#define NT 109

hbook            _file;
hbook_ntuple_cwn _ntu;

struct ntu_struct_t
{
  int fi, bi;
  float fe, be;
  float ft, bt;
} _ntu_struct;
#endif

int last_naccept = -1;
int last_nebis   = -1;

void user_function(unpack_event *event,
		   raw_event    *raw_event,
		   cal_event    *cal_event,
                   USER_STRUCT  *user_event)
{
#if TIMING_MATCH_NTU
  //#define F_I (16-1)
  //#define B_I (23-1)
  if (raw_event->D[0].F._valid._bits[0] &&
      raw_event->D[0].B._valid._bits[0])
  for (int F_I = 0; F_I < 32; F_I++)
  for (int B_I = 0; B_I < 32; B_I++)
  if (raw_event->D[0].F._valid._bits[0] == (1 << F_I) &&
      raw_event->D[0].B._valid._bits[0] == (1 << B_I))
  {
    _ntu_struct.fe = raw_event->D[0].F._items[F_I].E.value;
    _ntu_struct.ft = raw_event->D[0].F._items[F_I].T.value;
    _ntu_struct.be = raw_event->D[0].B._items[B_I].E.value;
    _ntu_struct.bt = raw_event->D[0].B._items[B_I].T.value;

    if (_ntu_struct.fe > 0 && _ntu_struct.be > 0 &&
	_ntu_struct.ft > 0 && _ntu_struct.bt > 0)
      {
	_ntu_struct.fi = F_I;
	_ntu_struct.bi = B_I;
	printf ("%2d %2d %6.1f %6.1f | %6.1f | %6.1f %6.1f\n",
		F_I,B_I,
		_ntu_struct.fe,_ntu_struct.be,
		_ntu_struct.fe-_ntu_struct.be,
		_ntu_struct.ft,_ntu_struct.bt);
	_ntu.hfnt();
      }
  }
#endif
#if CHECK_EXPECT_EC || DUMP_EC_COUNTS
  int readout_adc[NR_OF_ADCS];
  int readout_tdc[NR_OF_TDCS];

  memset(readout_adc,0,sizeof(readout_adc));
  memset(readout_tdc,0,sizeof(readout_tdc));

#define IF_HAS_DATA(type,no) if (event->vme.type[no].eob.event_number)
#define RO(type,no) { readout_##type[no] = 1; }

  IF_HAS_DATA(adc,0) { RO(adc,1); RO(adc,6); RO(tdc,0); RO(tdc,1); RO(tdc,4); }
  IF_HAS_DATA(adc,1) { RO(adc,0); RO(adc,6); RO(tdc,0); RO(tdc,1); RO(tdc,4); }
  IF_HAS_DATA(adc,2) { RO(adc,3); RO(adc,6); RO(tdc,2); RO(tdc,3); RO(tdc,4); }
  IF_HAS_DATA(adc,3) { RO(adc,2); RO(adc,6); RO(tdc,2); RO(tdc,3); RO(tdc,4); }
  //IF_HAS_DATA(adc,4) { RO(adc,5); RO(adc,6); RO(tdc,2);            RO(tdc,4); }
  //IF_HAS_DATA(adc,5) { RO(adc,4); RO(adc,6); RO(tdc,2);            RO(tdc,4); }
  IF_HAS_DATA(adc,6) {                                             RO(tdc,4); }

  IF_HAS_DATA(tdc,0) { RO(adc,0); RO(adc,1); RO(adc,6); RO(tdc,1); RO(tdc,4); }
  IF_HAS_DATA(tdc,1) { RO(adc,0); RO(adc,1); RO(adc,6); RO(tdc,0); RO(tdc,4); }
  IF_HAS_DATA(tdc,2) { RO(adc,2); RO(adc,3); RO(adc,6); RO(tdc,3); RO(tdc,4); }
  IF_HAS_DATA(tdc,3) { RO(adc,2); RO(adc,3); RO(adc,6); RO(tdc,2); RO(tdc,4); }
  //IF_HAS_DATA(tdc,2) { RO(adc,6);                                  RO(tdc,4); }
  //IF_HAS_DATA(tdc,3) { RO(adc,2); RO(adc,3); RO(adc,6); RO(tdc,2); RO(tdc,4); }
  IF_HAS_DATA(tdc,4) {                       RO(adc,6);                       }
#endif

#if DUMP_EC_COUNTS
  printf ("EC: ");
  printf (" %8d",event->event_no);
  printf (" %8d",event->vme.scaler[0].header.event_number);
  for (int i = 0; i < NR_OF_ADCS; i++)
    printf (" %8d",event->vme.adc[i].eob.event_number);
  for (int i = 0; i < NR_OF_TDCS; i++)
    printf (" %8d",event->vme.tdc[i].eob.event_number);

  for (int i = 0; i < 3; i++)
  printf (" %8d",event->vme.scaler[0].data[i]);

  printf ("\n");
#endif

  if (_dump_ebis_clear)
    {
      // Figure out where the scaler value really belongs...

      int nebis   = event->vme.scaler[0].data[1];
      int naccept = event->vme.scaler[0].data[2];

      bool adc_tdc_clean = true;

      for (int i = 0; i < NR_OF_ADCS; i++)
	if (event->vme.adc[i].eob.event_number)
	  adc_tdc_clean = false;
      for (int i = 0; i < NR_OF_TDCS; i++)
	if (event->vme.tdc[i].eob.event_number)
	  adc_tdc_clean = false;

      printf ("SC: ");
      printf (" %8d",event->event_no);


      for (int i = 0; i < 3; i++)
	printf (" %8d",event->vme.scaler[0].data[i].value);

      printf (" %8d",event->vme.scaler[0].header.event_number);
      /*
	for (int i = 0; i < NR_OF_ADCS; i++)
	printf (" %c",event->vme.adc[i].eob.event_number ? '*' : ' ');
	for (int i = 0; i < NR_OF_TDCS; i++)
	printf (" %c",event->vme.tdc[i].eob.event_number ? '*' : ' ');
      */
      printf (" ");

      if (nebis == last_nebis)
	printf ("-  ");
      else if (nebis == last_nebis + 1)
	printf ("E  ");
      else
	printf ("El ");

      if (naccept == last_naccept + 1)
	printf ("-  ");
      else
	printf ("AL ");

      if (!adc_tdc_clean)
	printf ("- ");
      else
	printf ("C ");

      last_nebis   = nebis;
      last_naccept = naccept;

      printf ("\n");
    }


#if USE_IS446_UNPACK && USE_IS446_NTUPLE
  if (_fixed_tebis_fid)
    {
      while (_fixed_tebis_info.mbs_ec < (int) event->event_no &&
	     !feof(_fixed_tebis_fid))
	{
	  // read next

	  int n = fscanf(_fixed_tebis_fid,"%d %d %d",
			 &_fixed_tebis_info.mbs_ec,
			 &_fixed_tebis_info.tshort,
			 &_fixed_tebis_info.tebis);

	  if (n != 3)
	    ERROR("Failure reading fixed tebis.");
	}

      if (_fixed_tebis_info.mbs_ec == (int) event->event_no)
	{
	  raw_event->TSHORT.value = _fixed_tebis_info.tshort;
	  raw_event->TEBIS.value  = _fixed_tebis_info.tebis;
	}
      else
	{
	  raw_event->TSHORT.value = (uint32) -500;
	  raw_event->TEBIS.value  = (uint32) -500;
	}
    }
#endif



#if 0
  /*
  SYNCPRINTF ("%7d: %5d %5d %5d %5d\n",
	  event->event_no,
	  raw_event->SCI[0][0].T,
	  raw_event->SCI[0][1].T,
	  raw_event->SCI[0][0].E,
	  raw_event->SCI[0][1].E);
  */
  /*
  SYNCPRINTF ("tdc0: "); event->vme.tdc0.data.dump(); SYNCPRINTF ("\n");
  SYNCPRINTF ("qdc0: "); event->vme.qdc0.data.dump(); SYNCPRINTF ("\n");
  SYNCPRINTF ("adc0: "); event->vme.adc0.data.dump(); SYNCPRINTF ("\n");
  SYNCPRINTF ("scaler: "); event->vme.scaler0.data.dump(); SYNCPRINTF ("\n");
  */

  bool ok_event_no, ok_scaler0;
  bool ok_adc[NR_OF_ADCS];
  bool ok_tdc[NR_OF_TDCS];

  ok_event_no =
    missing_count(event->event_no,expect_event_no,false);

  ok_scaler0 =
    missing_count(event->vme.scaler[0].header.event_number,expect_scaler0,true);
  SYNCPRINTF (("#"));
  for (int i = 0; i < NR_OF_ADCS; i++)
    ok_adc[i] = missing_count(event->vme.adc[i].eob.event_number,expect_adc[i],false);
  SYNCPRINTF (("#"));
  for (int i = 0; i < NR_OF_TDCS; i++)
    ok_tdc[i] = missing_count(event->vme.tdc[i].eob.event_number,expect_tdc[i],false);
  SYNCPRINTF (("#"));

  int mismatch = 0;

  for (int i = 0; i < 4; i++)
    if (event->vme.adc[i].eob.event_number &&
	event->vme.tdc[i].eob.event_number &&
	ok_adc[i] != ok_tdc[i])
      mismatch |= 1 << i;

  if (mismatch)
    {
      SYNCPRINTF (("%x",mismatch));
      // printf ("%d",mismatch);
    }
  else
    SYNCPRINTF ((" "));

  //

  int off_adc[NR_OF_ADCS] = { 0, 0, 0, 0, 0, 0, 0,};
  int off_tdc[NR_OF_TDCS] = { 0, 0, 0, 0, 0,};

  //

  SYNCPRINTF (("| %02x | ",
	       (event->vme.scaler[0].header.event_number - event->event_no) & 0xff));

  for (int i = 0; i < NR_OF_ADCS; i++)
    {
      if (event->vme.adc[i].eob.event_number)
	{
	  SYNCPRINTF (("%02x",
		       (event->vme.adc[i].eob.event_number - event->event_no) & 0xff));

	  int diff = event->vme.adc[i].eob.event_number - event->event_no;
	  if (diff != diff_adc[i])
	    {
	      off_adc[i] = diff-diff_adc[i];
	      SYNCPRINTF((";%d",diff-diff_adc[i]));
	      diff_adc[i] = diff;
	    }
	  else
	    SYNCPRINTF(("  "));
	}
      else
	{
	  SYNCPRINTF (("    "));
	}

      SYNCPRINTF(("%c",readout_adc[i] ? (event->vme.adc[i].eob.event_number ? '.' : '*') : ' '));
      SYNCPRINTF((" "));
    }

  for (int i = 0; i < NR_OF_TDCS; i++)
    {
      if (event->vme.tdc[i].eob.event_number)
	{
	  SYNCPRINTF (("%02x",
		       (event->vme.tdc[i].eob.event_number - event->event_no) & 0xff));

	  int diff = event->vme.tdc[i].eob.event_number - event->event_no;
	  if (diff != diff_tdc[i])
	    {
	      off_tdc[i] = diff-diff_tdc[i];
	      SYNCPRINTF((";%d",diff-diff_tdc[i]));
	      diff_tdc[i] = diff;
	    }
	  else
	    SYNCPRINTF(("  "));
	}
      else
	{
	  SYNCPRINTF (("    "));
	}
      SYNCPRINTF(("%c",readout_tdc[i] ? (event->vme.tdc[i].eob.event_number ? '.' : '*') : ' '));
      SYNCPRINTF((" "));
    }

  int off_mismatch = 0;

  for (int i = 0; i < 4; i++)
    if (event->vme.adc[i].eob.event_number &&
	event->vme.tdc[i].eob.event_number &&
	(diff_adc[i] - diff_tdc[i]) != expect_diff_adc_tdc[i])
      {
	expect_diff_adc_tdc[i] = (diff_adc[i] - diff_tdc[i]);
	off_mismatch |= 1 << i;
      }

  SYNCPRINTF (("@"));
  if (off_mismatch)
    {
      SYNCPRINTF (("%x",off_mismatch));
      // printf ("%d",mismatch);
    }
  else
    SYNCPRINTF ((" "));
  SYNCPRINTF (("@"));

  /*
  int mindiff = INT_MAX;

  for (int i = 0; i < 10; i++)
    if (hasdiff[i] &&
	diff[i] < mindiff)
      mindiff = diff[i];

  for (int i = 0; i < 10; i++)
    {
      if (hasdiff[i])
	SYNCPRINTF ("%2x ",(diff[i]-mindiff) & 0xff);
      else
	SYNCPRINTF ("   ");
    }
  */

  SYNCPRINTF (("\n"));
#endif


#if CORR_PICTURES
  // Let's make life easy for us.  First loop over the modules, and
  // find all channels that fired.  Then loop over those channels to
  // plot the pictures

  int fired[(NR_OF_ADCS+NR_OF_TDCS)*32];
  int nfired = 0;

  for (int m = 0; m < NR_OF_ADCS; m++)
    {
      bitsone_iterator iter;
      int i;

      while ((i = event->vme.adc[m].data._valid.next(iter)) >= 0)
	fired[nfired++] = (m) * 32 + i;
    }

  for (int m = 0; m < NR_OF_TDCS; m++)
    {
      bitsone_iterator iter;
      int i;

      while ((i = event->vme.tdc[m].data._valid.next(iter)) >= 0)
	fired[nfired++] = (NR_OF_ADCS + m) * 32 + i;
    }

  // printf ("%d ",nfired);

  qsort(fired,nfired,sizeof(int),compare_int);

  for (int i = 0; i < nfired; i++)
    {
      dam_corr.add_self(fired[i]);
      for (int j = i+1; j < nfired; j++)
	dam_corr.add(fired[i],fired[j]);
    }
  /*
  if (mismatch)
    {
      for (int i = 0; i < nfired; i++)
	{
	  dam_corr_m[mismatch].add_self(fired[i]);
	  for (int j = i+1; j < nfired; j++)
	    dam_corr_m[mismatch].add(fired[i],fired[j]);
	}
    }
  */
#endif

#if CHECK_EXPECT_EC
  ec_event* ee = new ec_event;

  memset(ee->flags,0,sizeof(ee->flags));

  ee->ec.p.no = event->event_no;
  ee->ec.p.scaler = event->vme.scaler[0].header.event_number;
  for (int i = 0; i < NR_OF_ADCS; i++)
    {
      ee->ec.p.adc[i] = event->vme.adc[i].eob.event_number;
      if (readout_adc[i])
	ee->flags[2+i] |= EE_FLAG_READOUT;
    }
  for (int i = 0; i < NR_OF_TDCS; i++)
    {
    ee->ec.p.tdc[i] = event->vme.tdc[i].eob.event_number;
      if (readout_tdc[i])
	ee->flags[2+NR_OF_ADCS+i] |= EE_FLAG_READOUT;
    }

  ee->fired.resize(nfired);
  for (int i = 0; i < nfired; i++)
    ee->fired[i] = fired[i];

  /*
  if ((ee->ec.p.no & 0xff) == 0x79 &&
      (ee->ec.p.scaler & 0xff) == 0xdd)
    {
      int match = 0;

      for (int m = 0; m < 12; m++)
	if ((ee->ec.all[2+m] & 0xff) == 0x4c ||
	    (ee->ec.all[2+m] & 0xff) == 0x54 ||
	    (ee->ec.all[2+m] & 0xff) == 0x4d ||
	    (ee->ec.all[2+m] & 0xff) == 0x3a)
	  match++;

      if (match == 7)
	printf ("***\nhere\n\n");

    }

  delete ee;
  */
  events.push_back(ee);
#endif


  static DATA32 last_TSHORT = { 0 };
  static DATA32 last_TLONG = { 0 };

  raw_event->TLAST.value = raw_event->TSHORT.value - last_TSHORT.value;

  if (raw_event->TSHORT.value < last_TSHORT.value)
    last_TLONG += last_TSHORT.value;

  raw_event->TLONG.value = raw_event->TSHORT.value + last_TLONG;

  last_TSHORT.value = raw_event->TSHORT.value;

#if USE_IS446_UNPACK
  _state.clear();

  // Take the trigger from the raw event (calculated by the generic code)
  _state.set_trigger(event->trigger);

  // Loop over the unpack event, modules from this event

  _state.set_scaler_ec(event->vme.scaler[0].header.event_number);

  ssize_t i;

  for (bitsone_iterator iter; (i = event->vme.scaler[0].data._valid.next(iter))
>= 0; )
    _state.set_scaler_value((uint32) i,event->vme.scaler[0].data[i]);

  // Overflow from adc and tdc are ignored (just as in the old unpacker)

  for (unsigned int adc = 0; adc < countof(event->vme.adc); adc++)
    for (bitsone_iterator iter; (i = event->vme.adc[adc].data._valid.next(iter)) >= 0; )
      _state.set_adc(adc,(uint32) i,event->vme.adc[adc].data[i].value);

  for (unsigned int tdc = 0; tdc < countof(event->vme.tdc); tdc++)
    for (bitsone_iterator iter; (i = event->vme.tdc[tdc].data._valid.next(iter)) >= 0; )
      _state.set_tdc(tdc,(uint32) i,event->vme.tdc[tdc].data[i].value);

  //_state.set_adc();
  //_state.set_tdc();

  _state.process();

  _state.ntup_event.others.oldtshort = _state.ntup_event.others.tshort / 5;
  _state.ntup_event.others.oldtebis  = _state.ntup_event.others.tebis / 5;
  _state.ntup_event.others.tshort    = raw_event->TSHORT.value / 5;
  _state.ntup_event.others.tebis     = raw_event->TEBIS.value / 5;

#if USE_IS446_NTUPLE
  if (_fortntu_name)
    _state.output_event();
#endif

  // raw_event->TSHORT.value = _state.event.time_t2;
  raw_event->OLDTEBIS.value  = _state.event.time_ebis;

#endif
}

void init_function()
{
#if TIMING_MATCH_NTU
  char filename_wrt[1024]; //  = "ntuple.ntu";
  strcpy(filename_wrt,"ntuple.ntu");

  _ntu.hbset_bsize(4096);
  _file.open(filename_wrt,"NTUPLE",4096);
  _ntu.hbnt(NT,"NTUPLE"," ");

  char vars[1024];

  _ntu.hbname("SINGLES",&_ntu_struct,
	      "FI:I,BI:I,FE:R,BE:R,FT:R,BT:R");
#endif


#if CORR_PICTURES
  dam_corr.clear();
  for (int i = 0; i < 15; i++)
    dam_corr_m[i].clear();
#endif

#if CHECK_EXPECT_EC
  printf ("IS446 : INIT\n\n");
#endif

  // CALIB_PARAM( SCI[0][0].T ,SLOPE_OFFSET,  2.4 , 6.7 );
  // CALIB_PARAM( SCI[0][0].E ,SLOPE_OFFSET,  2.2 , 6.8 );

  show_calib_map();

  // the_raw_event_calib_map.show_

#if USE_IS446_UNPACK && USE_IS446_NTUPLE
  if (_fortntu_name)
    _state.open_output(_fortntu_name);

  if (_fixed_tebis_name)
    {
      if (strcmp(_fixed_tebis_name,"-") == 0)
	_fixed_tebis_fid = stdin;
      else
	{
	  _fixed_tebis_fid = fopen(_fixed_tebis_name,"rt");

	  if (!_fixed_tebis_fid)
	    ERROR("Cannot open file '%s'.",_fixed_tebis_name);
	}
      _fixed_tebis_info.mbs_ec = -1;
    }

#endif
}

void exit_function()
{
#if TIMING_MATCH_NTU
  _file.close();
#endif

#if CHECK_EXPECT_EC
  /* Now, look for events where the event counters are out of sync.
   *
   * We'd like to find the clear cases, where we are quite sure
   * so to say.  Now, this requires 'co-operation' from previous and
   * following events.
   *
   * Now, an event has _old_ data, if the next event is increasing by
   * two.  Or possibly the next event has data that is to late
   * relative to it's neighbours.
   */

  /* What we'd rather like to do is to be able to find event counters
   * which are correct.  An event counter is correct when we know that
   * the module would have been read out, but produces no data.  (Does
   * not help to much, but we know there is no stale data in the
   * buffer.)
   *
   * Actually, the problem has two things to it.  Event counter being
   * correct, and having no data in the MEB.  What of course concerns
   * us is that what we read out, is in sync between all the modules,
   * i.e. data for the same event.
   *
   * We can then from these locations work our way backwards.  Whatever
   * event data we find, would be for that previous event.  So that is
   * an correct counter.  And before that is also ok, if the difference
   * of the counter is the same as the distance to the event.
   */

  int n = events.size();

  for (int m = 0; m < 12; m++)
    {
      char flags = 0;
      int good_i = 0;
      int good_ec = 0;

      for (int i = n; i;)
	{
	  ec_event* ee = events[--i];

	  if (!ee->ec.all[2+m])
	    {
	      // we have no data

	      if (ee->flags[2+m] & EE_FLAG_READOUT)
		{
		  // would have been read out, are thus ok!

		  ee->flags[2+m] |= EE_FLAG_EC_CORRECT;
		  flags = ee->flags[2+m];
		  // remember our position and count
		  good_i  = i;
		  good_ec = 0;
		}
	    }
	  else
	    {
	      // we have data

	      if (flags & EE_FLAG_EC_CORRECT)
		{
		  // last known item had good counter.

		  if (!good_ec)
		    {
		      // it was an empty one.  so we're are also good!

		      ee->flags[2+m] |= EE_FLAG_EC_CORRECT;
		      flags = ee->flags[2+m];
		      // remember our position and count
		      good_i  = i;
		      good_ec = ee->ec.all[2+m];
		    }
		  else if (good_ec - ee->ec.all[2+m] == good_i - i)
		    {
		      // we have same difference as distance, so we're good!

		      ee->flags[2+m] |= EE_FLAG_EC_CORRECT;
		      flags = ee->flags[2+m];
		      // remember our position and count
		      good_i  = i;
		      good_ec = ee->ec.all[2+m];
		    }
		}
	    }
	}
      {
      ec_event* prev = NULL;
      int prev_i = 0;

      for (int i = 0; i < n; i++)
	{
	  ec_event* ee = events[i];

	  if (ee->ec.all[2+m])
	    {
	      if (prev)
		{
		  // prev was correct, or had good distance to his previous
		  // do we have correct distance to prev

		  if (ee->ec.all[2+m] - prev->ec.all[2+m] == i - prev_i)
		    {
		      // distance is good, this means that we can mark
		      // prev as good (if it was not already)

		      prev->flags[2+m] |= EE_FLAG_EC_CORRECT;
		      prev = ee;
		      prev_i = i;
		    }
		  else
		    prev = NULL;
		}
	      else if (ee->flags[2+m] & EE_FLAG_EC_CORRECT)
		{
		  prev = ee;
		  prev_i = i;
		}
	    }
	}
    }
  {
      ec_event* prev = NULL;
      int prev_i = 0;

      for (int i = 0; i < n; i++)
	{
	  ec_event* ee = events[i];




	  if (ee->ec.all[2+m])
	    {
	      if (prev)
		{
		  if (ee->ec.all[2+m] - prev->ec.all[2+m] != i - prev_i)
		    ee->flags[2+m] |= EE_FLAG_EC_UNEXP;
		}

	      prev = ee;
	      prev_i = i;
	    }
	}
  }

    }

  for (int i = 0; i < n-1; i++)
    {
      ec_event* ee = events[i];
      ec_event* ne = events[i+1];

      printf ("%08x",ee->ec.p.no);
      printf (" %02x",ee->ec.p.scaler & 0xff);
      for (int m = 0; m < 12; m++)
	{
	  if (ee->ec.all[2+m] &&
	      ne->ec.all[2+m] &&
	      (ne->flags[2+m] & EE_FLAG_EC_CORRECT) &&
	      !(ee->flags[2+m] & EE_FLAG_EC_CORRECT))
	    ee->flags[2+m] |= EE_FLAG_EC_INCORRECT;

	  if (ee->ec.all[2+m])
	    {
	      printf (" %02x",ee->ec.all[2+m] & 0xff);

	      if (ee->flags[2+m] & EE_FLAG_EC_UNEXP)
		printf (":");
	      else
		printf (" ");

	      if (ee->flags[2+m] & EE_FLAG_EC_INCORRECT)
		printf ("@");
	      else if (ee->flags[2+m] & EE_FLAG_EC_CORRECT)
		printf (" ");
	      else if (ee->flags[2+m] & EE_FLAG_READOUT)
		printf (".");
	      else
		printf ("+");
	    }
	  else
	    {
	      printf ("    ");
	      if (ee->flags[2+m] & EE_FLAG_READOUT)
		printf (".");
	      else
		printf (" ");
	    }



	  // printf();
	}
      /*
      if ((ee->ec.p.no & 0xff) == 0x79 &&
	  (ee->ec.p.scaler & 0xff) == 0xdd)
	{
	  int match = 0;

	  for (int m = 0; m < 12; m++)
	    if ((ee->ec.all[2+m] & 0xff) == 0x4c ||
		(ee->ec.all[2+m] & 0xff) == 0x54 ||
		(ee->ec.all[2+m] & 0xff) == 0x4d ||
		(ee->ec.all[2+m] & 0xff) == 0x3a)
	      match++;

	  if (match == 7)
	    printf ("here");

	}
      */

      // If this event has an unexpted increase somewhere, and
      // the next event also has it, for an module that we have
      // data for, then...!

      int from_next = 0;
      int from_prev = 0;
      int has = 0;

      for (int m = 0; m < 12; m++)
	if (ee->flags[2+m] & EE_FLAG_EC_UNEXP)
	  from_next |= (1 << m);

      if (from_next)
	{
	  for (int m = 0; m < 12; m++)
	    if (ee->ec.all[2+m])
	      {
		has |= (1 << m);
		if ((ne->flags[2+m] & EE_FLAG_EC_UNEXP) &&
		    !(ee->flags[2+m] & EE_FLAG_EC_UNEXP))
		  {
		    from_prev |= (1 << m);
		  }
	      }

	  if (from_prev)
	    {
	      int mismatch =
		(has & (has >> 7)) &
		((from_next & (from_prev >> 7)) |
		 ((from_next >> 7) & from_prev) ) &
		0x0f;

	      printf ("gotcha: %03x %03x %03x -> %03x %03x %03x -> %03x",
		      from_next,from_prev,has,
		      (has & (has >> 7)) & 0x0f,
		      (from_next & (from_prev >> 7)) & 0x0f,
		      ((from_next >> 7) & from_prev) & 0x0f,
		      mismatch);

	      if (mismatch)
		{
		  printf (" mismatch, %d",(int) ee->fired.size());

		  for (unsigned int i = 0; i < ee->fired.size(); i++)
		    {
		      dam_corr_m[mismatch].add_self(ee->fired[i]);
		      for (unsigned int j = i+1; j < ee->fired.size(); j++)
			dam_corr_m[mismatch].add(ee->fired[i],ee->fired[j]);
		    }
		}


	    }


	}

      printf ("\n");
    }
#endif

#if CORR_PICTURES
  dam_corr.picture("dam.pgm");

  for (int i = 0; i < 15; i++)
    {
      char filename[256];

      sprintf (filename,"dam_m%d.pgm",i);

      dam_corr_m[i].picture(filename);
    }
#endif

#if USE_IS446_UNPACK && USE_IS446_NTUPLE
  if (_fortntu_name)
    _state.close_output();
#endif
}

void is446_usage_command_line_options()
{
  //      "  --option          Explanation.\n"
#if USE_IS446_UNPACK && USE_IS446_NTUPLE
  printf ("  --fortntu=FILE    Generate fortran code style ntuple.\n");
  printf ("  --dump-ebis-clear  Dump scaler_ebis and adc/tdc-clear status.\n");
  printf ("  --fixed-tebis=FILE  Read fixed tebis and tshort from FILE.\n");
#endif
}

bool is446_handle_command_line_option(const char *arg)
{
#if USE_IS446_UNPACK && USE_IS446_NTUPLE
  const char *post;

#define MATCH_PREFIX(prefix,post) (strncmp(arg,prefix,strlen(prefix)) == 0 && *(post = arg + strlen(prefix)) != '\0')
#define MATCH_ARG(name) (strcmp(arg,name) == 0)

 if (MATCH_PREFIX("--fortntu=",post)) {
   _fortntu_name = post;
   return true;
 }
 else if (MATCH_ARG("--dump-ebis-clear")) {
   _dump_ebis_clear = 1;
   return true;
 }
 else if (MATCH_PREFIX("--fixed-tebis=",post)) {
   _fixed_tebis_name = post;
   return true;
 }
#endif

 return false;
}
