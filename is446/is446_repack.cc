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

// This file comes from CVS: isodaq/checker/is430_aug05_unpack.cc
// Name has been changed sine it will undergo major changes/adaptions.

#include "is446_repack.hh"

#include "../is430_05/scaler_timer.cc"

#include <memory.h>
#include <stdio.h>

is446_unpack::is446_unpack()
{
  file_open = false;

  last_time       = 0;
  last_num_ebis   = 0;
  last_num_accept = 0;

  last_scaler_ec = (uint32) -1;

  // tshort.bits_mask = 0x03ffffff;
  tebis .bits_mask = 0x03ffffff;

  _warn_ebis_missed = 0;

  _conf_quiet = 1;
}

void is446_unpack::clear()
{
  memset(&event,0,sizeof(event));

  memset(&ntup_event,0,sizeof(ntup_event));

  scaler_ec = (uint32) -1;

  // event.ievent = ; /* invent something. */
}

void is446_unpack::set_trigger(uint32 trig)
{
  event.trigger = trig;
  ntup_event.others.trigger = trig;
}

void is446_unpack::set_scaler_value(uint32 channel,uint32 value)
{
  //printf ("%2d: %d  XXXXXXXXXXXXXXXXX\n",channel,value);
  //fflush(stdout);
  event.scaler_mask |= 1 << channel;
  event.scaler[channel] = value;
}

void is446_unpack::set_scaler_ec(uint32 ec)
{
  scaler_ec = ec;
}

void is446_unpack::set_adc(uint32 n_adc,uint32 channel,uint32 value)
{
  switch (n_adc)
    {
    case 0:
      event.dsssd[0].front.add_e(channel,value);
      ntup_event.strips[0].fe  [ntup_event.strips[0].nfe] = value;
      ntup_event.strips[0].nsfe[ntup_event.strips[0].nfe] = channel+1;
      ntup_event.strips[0].nfe++;

      break;

    case 1:
      event.dsssd[0].back.add_e(channel,value);
      ntup_event.strips[0].be  [ntup_event.strips[0].nbe] = value;
      ntup_event.strips[0].nsbe[ntup_event.strips[0].nbe] = channel+1;
      ntup_event.strips[0].nbe++;
      break;

    case 2:
      event.dsssd[1].front.add_e(channel,value);
      ntup_event.strips[1].fe  [ntup_event.strips[1].nfe] = value;
      ntup_event.strips[1].nsfe[ntup_event.strips[1].nfe] = channel+1;
      ntup_event.strips[1].nfe++;

      break;

    case 3:
      event.dsssd[1].back.add_e(channel,value);
      ntup_event.strips[1].be  [ntup_event.strips[1].nbe] = value;
      ntup_event.strips[1].nsbe[ntup_event.strips[1].nbe] = channel+1;
      ntup_event.strips[1].nbe++;
      break;

    case 4:
      event.dsssd[2].front.add_e(channel,value);
      ntup_event.strips[2].fe  [ntup_event.strips[2].nfe] = value;
      ntup_event.strips[2].nsfe[ntup_event.strips[2].nfe] = channel+1;
      ntup_event.strips[2].nfe++;

      break;

    case 5:
      event.dsssd[2].back.add_e(channel,value);
      ntup_event.strips[2].be  [ntup_event.strips[2].nbe] = value;
      ntup_event.strips[2].nsbe[ntup_event.strips[2].nbe] = channel+1;
      ntup_event.strips[2].nbe++;
      break;

    case 6:
      switch(channel)
	{
	case 0:
	  event.back[0].add_e(value);
	  ntup_event.singles.eback[0] = value;
	  break;

	case 1:
	  event.back[1].add_e(value);
	  ntup_event.singles.eback[1] = value;
	  break;

	case 2:
	  event.mon[0].add_e(value);
	  ntup_event.singles.emon[0] = value;
	  break;

	case 3:
	  event.mon[1].add_e(value);
	  ntup_event.singles.emon[1] = value;
	  break;

	default:
	  if (_conf_quiet < 1)
	    fprintf (stderr,"%d Unexpected channel #%d for ADC #%d.\n",_conf_quiet,channel,n_adc);
	  break;
	}
      break;
    default:
      if (_conf_quiet < 2)
	fprintf (stderr,"Unexpected ADC #%d.\n",n_adc);
      break;
    }
}

void is446_unpack::set_tdc(uint32 n_tdc,uint32 channel,uint32 value)
{
  switch (n_tdc)
    {
    case 0:
      event.dsssd[0].front.add_t(channel,value);
      ntup_event.strips[0].ft  [ntup_event.strips[0].nft] = value;
      ntup_event.strips[0].nsft[ntup_event.strips[0].nft] = channel+1;
      ntup_event.strips[0].nft++;
      break;
    case 1:
      event.dsssd[0].back.add_t(channel,value);
      ntup_event.strips[0].bt  [ntup_event.strips[0].nbt] = value;
      ntup_event.strips[0].nsbt[ntup_event.strips[0].nbt] = channel+1;
      ntup_event.strips[0].nbt++;
      break;

    case 2:
      event.dsssd[1].front.add_t(channel,value);
      ntup_event.strips[1].ft  [ntup_event.strips[1].nft] = value;
      ntup_event.strips[1].nsft[ntup_event.strips[1].nft] = channel+1;
      ntup_event.strips[1].nft++;
      break;
    case 3:
      event.dsssd[2].front.add_t(channel,value);
      ntup_event.strips[2].ft  [ntup_event.strips[2].nft] = value;
      ntup_event.strips[2].nsft[ntup_event.strips[2].nft] = channel+1;
      ntup_event.strips[2].nft++;
      break;

    case 4:
      switch(channel)
	{
	case 0:
	  event.back[0].add_t(value);
	  ntup_event.singles.tback[0] = value;
	  break;
	case 1:
	  event.back[1].add_t(value);
	  ntup_event.singles.tback[1] = value;
	  break;
	case 2:
	  event.mon[0].add_t(value);
	  ntup_event.singles.tmon[0] = value;
	  break;
	case 3:
	  event.mon[1].add_t(value);
	  ntup_event.singles.tmon[1] = value;
	  break;

	default:
	  if (_conf_quiet < 1)
	    fprintf (stderr,"Unexpected channel #%d for TDC #%d.\n",channel,n_tdc);
	  break;
	}
      break;
    default:
      if (_conf_quiet < 2)
	fprintf (stderr,"Unexpected TDC #%d.\n",n_tdc);
      break;
    }
}

#define SCALER_TSHORT  0
//#define SCALER_T2
#define SCALER_EBIS    1
#define SCALER_NACCEPT 2

void is446_unpack::invalidate_scaler_state()
{
  // tshort.set_invalid();
  tebis.set_invalid();
}

void is446_unpack::process()
{
  /*
  printf ("trigger: %d   (%d %d %d %d %d)\n",
	  event.trigger,
	  event.scaler[0],
	  event.scaler[1],
	  event.scaler[2],
	  event.scaler[3],
	  event.scaler[4]);
  */

  uint32 now = event.scaler[0];

  if (event.scaler[SCALER_EBIS] < last_num_ebis ||
      event.scaler[SCALER_NACCEPT] < last_num_accept ||
      (scaler_ec == 0 && last_scaler_ec != 0xffff))
    {
      // We had proton pulse, scaler was reset, scaler event counter
      // is to be 0

      if (scaler_ec != 0)
	{
          if (_conf_quiet < 2)
            fprintf(stderr,"Scaler values (ebis | naccept) < previous, "
		    "but EC (%d) not reset.\n",scaler_ec);
	}

      tebis.set_invalid();

      last_num_ebis = 0;
      last_num_accept = 0;
    }

  last_scaler_ec = 0;

  //bool is_t2    = !!((event.trigger < 8) && (event.trigger & 0x2));
  //bool is_ebis  = !!((event.trigger < 8) && (event.trigger & 0x4));

  //bool had_t2   = event.scaler[SCALER_T2] > last_num_t2;
  bool had_ebis = event.scaler[SCALER_EBIS] > last_num_ebis;
  /*
  printf ("%2d: %d %d %d %d | %8d %8d %8d %d (%8d)\n",
	  event.trigger,
	  is_t2,had_t2,is_ebis,had_ebis,
          event.time_ebis,
          ntup_event.others.tebis,
          tebis.current0,tebis.current0_valid,
          now);
  */

  if (had_ebis)
    {
      // presume we got the ebis pulse
      tebis.set0(now);

      if (event.scaler[SCALER_EBIS] != last_num_ebis+1)
	{
	  if (_conf_quiet < 2)
	    fprintf(stderr,"Several EBIS were missed (%d), EBIS invalid.\n",
		    event.scaler[SCALER_EBIS]-last_num_ebis);
	}
    }

  last_time       = event.scaler[0];
  last_num_ebis   = event.scaler[SCALER_EBIS];
  last_num_accept = event.scaler[SCALER_NACCEPT];

  // printf ("EBIS: %d -> %d (%d)\n",last_num_ebis,event.scaler[SCALER_EBIS],event.trigger);

  if (!(event.scaler_mask & 0x00000001))
    {
      fprintf(stderr,"Scaler data channel 0 missing.\n");
      return; // We have no data
    }

  // event.time_t2   = tshort.diff(now);
  event.time_short = event.scaler[0];
  event.time_ebis  = tebis.diff(now);

  ntup_event.others.tshort = event.time_short > 0 ? (int32) (event.time_short) : -999;
  ntup_event.others.tebis  = event.time_ebis  > 0 ? (int32) (event.time_ebis ) : -999;

  /*
  printf ("%8d %8d %8d %d | %8d %8d %8d %d (%8d)\n",
	  event.time_t2,
	  ntup_event.others.tshort,
	  tshort.current0,tshort.current0_valid,
	  event.time_ebis,
	  ntup_event.others.tebis,
	  tebis.current0,tebis.current0_valid,
	  now);
  fflush(stdout);
  */
}

#ifdef USE_CERNLIB

#include "hbook.hh"

#define NT 109

///////////////////////////////////////////////////////////////////////

hbook            _file;
hbook_ntuple_cwn _ntu;

void is446_unpack::open_output(const char *filename)
{
  char filename_wrt[1024]; //  = "ntuple.ntu";
  strcpy(filename_wrt,filename);

  _ntu.hbset_bsize(4096);
  _file.open(filename_wrt,"NTUPLE",4096);
  _ntu.hbnt(NT,"NTUPLE"," ");

  char vars[1024];

#define ADD_VAR(spec) {if(vars[0])strcat(vars,",");strcat(vars,spec);}

#define ADD_VARIABLE_I(name)                            ADD_VAR(name                                   ":I")
#define ADD_VARIABLE_I_MIN_MAX(name,min,max)            ADD_VAR(name             "[" #min "," #max "]" ":I")
#define ADD_VARIABLE_I_ARRAY_MIN_MAX(name,num,min,max)  ADD_VAR(name "(" num ")" "[" #min "," #max "]" ":I")

  for (int i = 0; i < 3; i++)
    {
      /* STRIPn */
      vars[0] = 0;
      ADD_VARIABLE_I_MIN_MAX("NFnE",0,32);
      ADD_VARIABLE_I_MIN_MAX("NBnE",0,32);
      ADD_VARIABLE_I_MIN_MAX("NFnT",0,32);
      ADD_VARIABLE_I_MIN_MAX("NBnT",0,32);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("NSFnE","NFnE",0,32);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("NSBnE","NBnE",0,32);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("NSFnT","NFnT",0,32);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("NSBnT","NBnT",0,32);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("EFn","NFnE",-999,4096);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("EBn","NBnE",-999,4096);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("TFn","NFnT",-999,4096);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("TBn","NBnT",-999,4096);
      char *p;
      while ((p = strchr(vars,'n')) != NULL)
	*p = (char) ('1'+i);
      char strip[64];
      sprintf(strip,"STRIP%d",i+1);
      _ntu.hbname(strip,&ntup_event.strips[i],vars);
      printf ("Variables: %s\n",vars);
    }

  /* SINGLES */
  vars[0] = 0;
  ADD_VARIABLE_I_MIN_MAX("EBACK1",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("EBACK2",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("EBACK3",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("TBACK1",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("TBACK2",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("TBACK3",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("EMONITOR1",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("EMONITOR2",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("TMONITOR1",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("TMONITOR2",-999,4096);
  _ntu.hbname("SINGLES",&ntup_event.singles,vars);
  printf ("Variables: %s\n",vars);

  /* OTHERS */
  vars[0] = 0;
  ADD_VARIABLE_I("IEVENT");
  ADD_VARIABLE_I("TSHORT");
  ADD_VARIABLE_I("TEBIS");
  ADD_VARIABLE_I("OLDTSHORT");
  ADD_VARIABLE_I("OLDTEBIS");
  ADD_VARIABLE_I_MIN_MAX("TRIGGER",0,15);
  _ntu.hbname("OTHERS",&ntup_event.others,vars);
  printf ("Variables: %s\n",vars);

  file_open = true;
}

void is446_unpack::output_event()
{
  if (file_open)
    _ntu.hfnt();
}

void is446_unpack::close_output()
{
  _file.close();
  printf ("END! (of ntuple writing.  file closed)\n");
}

#endif

