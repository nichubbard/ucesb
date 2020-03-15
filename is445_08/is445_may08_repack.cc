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

#include "is445_may08_repack.hh"

#include "../is430_05/scaler_timer.cc"

#include <memory.h>
#include <stdio.h>

is445_may08_unpack::is445_may08_unpack()
{
  file_open = false;

  last_time     = 0;
  last_num_t2   = 0;
  last_num_ebis = 0;

  tshort.bits_mask = 0x03ffffff;
  tebis .bits_mask = 0x03ffffff;

  _warn_ebis_missed = 0;

  _conf_quiet = 1;
}

void is445_may08_unpack::clear()
{
  memset(&event,0,sizeof(event));

  memset(&ntup_event,0,sizeof(ntup_event));


  // event.ievent = ; /* invent something. */
}

void is445_may08_unpack::set_trigger(uint32 trig)
{
  event.trigger = trig;
  ntup_event.others.trigger = trig;
}

void is445_may08_unpack::set_scaler_value(uint32 channel,uint32 value)
{
  //printf ("%2d: %d  XXXXXXXXXXXXXXXXX\n",channel,value);
  //fflush(stdout);
  event.scaler_mask |= 1 << channel;
  event.scaler[channel] = value;
}

void is445_may08_unpack::set_adc(uint32 n_adc,uint32 channel,uint32 value)
{
  switch (n_adc)
    {
    case 0:
      event.dsssd[0].front.add_e(channel,value);
      ntup_event.strips[0].fe  [ntup_event.strips[0].nfe] = value;
      ntup_event.strips[0].nsfe[ntup_event.strips[0].nfe] = channel;
      ntup_event.strips[0].nfe++;
      //if (value>0) {printf("value not zero: %i,%i\n", value,channel);}
	//fprintf (stderr,"Unexpected ADC test %f, %f \n",value,channel);

      break;

    case 1:
      event.dsssd[0].back.add_e(channel,value);
      ntup_event.strips[0].be  [ntup_event.strips[0].nbe] = value;
      ntup_event.strips[0].nsbe[ntup_event.strips[0].nbe] = channel;
      ntup_event.strips[0].nbe++;
      break;

    case 2:
      switch(channel)
	{
	case 0:
	  event.back[0].add_e(value);
	  ntup_event.singles.e1back = value;
	  break;

	case 1:
	  event.mon[0].add_e(value);
	  ntup_event.singles.emon = value;
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

void is445_may08_unpack::set_tdc(uint32 n_tdc,uint32 channel,uint32 value)
{
  switch (n_tdc)
    {
    case 0:
      event.dsssd[0].front.add_t(channel,value);
      ntup_event.strips[0].ft  [ntup_event.strips[0].nft] = value;
      ntup_event.strips[0].nsft[ntup_event.strips[0].nft] = channel;
      ntup_event.strips[0].nft++;
      break;
    case 1:
      event.dsssd[0].back.add_t(channel,value);
      ntup_event.strips[0].bt  [ntup_event.strips[0].nbt] = value;
      ntup_event.strips[0].nsbt[ntup_event.strips[0].nbt] = channel;
      ntup_event.strips[0].nbt++;
      break;

    case 2:
      switch(channel)
	{
	case 0:
	  event.back[0].add_t(value);
	  ntup_event.singles.t1back = value;
	  break;
	case 1:
	  event.t_trig = (uint16) value;
	  ntup_event.singles.ttrigger = value;
	  break;
	case 2:
	  event.t_dsssd_front = (uint16) value;
	  ntup_event.singles.f1ct = value;
	  break;
	case 3:
	  event.t_dsssd_back = (uint16) value;
	  ntup_event.singles.b1ct = value;
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

#define SCALER_T2    3
#define SCALER_EBIS  4

void is445_may08_unpack::invalidate_scaler_state()
{
  tshort.set_invalid();
  tebis.set_invalid();
}

void is445_may08_unpack::process()
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

  bool is_t2    = !!((event.trigger < 8) && (event.trigger & 0x2));
  bool is_ebis  = !!((event.trigger < 8) && (event.trigger & 0x4));

  bool had_t2   = event.scaler[SCALER_T2] > last_num_t2;
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

  if (is_t2)
    {
      if (!had_t2)
	fprintf(stderr,"T2 trigger, but no increase in scaler (=%d).\n",
		event.scaler[SCALER_T2]);
      tshort.set0(now);
    }
  else if (had_t2)
    {
      // printf ("T2: %d -> %d (%d)\n",last_num_t2,event.scaler[SCALER_T2],event.trigger);
      tshort.set_invalid();
      if (event.scaler[SCALER_T2] != last_num_t2+1)
	{
	  if (_conf_quiet < 2)
	    fprintf(stderr,"Several T2 were missed (%d), T2 invalid. (=%d)\n",
		    event.scaler[SCALER_T2]-last_num_t2,
		    event.scaler[SCALER_T2]);
	}
      else if (tshort.set0_estimate(last_time,now))
	{
	  if (_conf_quiet < 2)
	    fprintf(stderr,"T2 pulse missed, applying corrections. (=%d)\n",
		    event.scaler[SCALER_T2]);
	}
      else
	{
	  if (_conf_quiet < 2)
	    fprintf(stderr,"T2 pulse missed, T2 invalid. (=%d)\n",
		    event.scaler[SCALER_T2]);
	}
    }

  if (is_ebis)
    {
      if (!had_ebis)
	{
	  fprintf(stderr,"EBIS trigger, but no increase in scaler (=%d).\n",event.scaler[SCALER_EBIS]);
	  _warn_ebis_missed = 0;
	}
      tebis.set0(now);
    }
  else if (had_ebis)
    {
      // printf ("EBIS: %d -> %d (%d)\n",last_num_ebis,event.scaler[SCALER_EBIS],event.trigger);
      tebis.set_invalid();
      if (event.scaler[SCALER_EBIS] != last_num_ebis+1)
	{
	  if (_conf_quiet < 2)
	    fprintf(stderr,"Several EBIS were missed (%d), EBIS invalid.\n",
		    event.scaler[SCALER_EBIS]-last_num_ebis);
	}
      else if (tebis.set0_estimate(last_time,now))
	{
	  if (_conf_quiet < 2)
	    fprintf(stderr,"EBIS pulse missed, applying corrections.\n");
	}
      else
	{
	  if (_conf_quiet < 2 && _warn_ebis_missed < 5)
	    fprintf(stderr,"EBIS pulse missed, EBIS invalid.\n");
	  _warn_ebis_missed++;
	}
    }

  last_time     = event.scaler[0];
  last_num_t2   = event.scaler[SCALER_T2];
  last_num_ebis = event.scaler[SCALER_EBIS];

  // printf ("EBIS: %d -> %d (%d)\n",last_num_ebis,event.scaler[SCALER_EBIS],event.trigger);

  if (!(event.scaler_mask & 0x00000001))
    {
      fprintf(stderr,"Scaler data channel 0 missing.\n");
      return; // We have no data
    }

  event.time_t2   = tshort.diff(now);
  event.time_ebis = tebis.diff(now);

  ntup_event.others.tshort = event.time_t2   > 0 ? (int32) (event.time_t2  ) : -999;
  ntup_event.others.tebis  = event.time_ebis > 0 ? (int32) (event.time_ebis) : -999;
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

void is445_may08_unpack::open_output(const char *filename)
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

  for (int i = 0; i < 1; i++)
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
      ADD_VARIABLE_I_ARRAY_MIN_MAX("FnE","NFnE",-999,4096);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("BnE","NBnE",-999,4096);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("FnT","NFnT",-999,4096);
      ADD_VARIABLE_I_ARRAY_MIN_MAX("BnT","NBnT",-999,4096);
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
  ADD_VARIABLE_I_MIN_MAX("E1BACK",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("T1BACK",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("EMON",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("TTRIGGER",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("F1CT",-999,4096);
  ADD_VARIABLE_I_MIN_MAX("B1CT",-999,4096);
  _ntu.hbname("SINGLES",&ntup_event.singles,vars);
  printf ("Variables: %s\n",vars);

  /* OTHERS */
  vars[0] = 0;
  ADD_VARIABLE_I("IEVENT");
  ADD_VARIABLE_I("TSHORT");
  ADD_VARIABLE_I("TEBIS");
  ADD_VARIABLE_I_MIN_MAX("TRIGGER",0,15);
  _ntu.hbname("OTHERS",&ntup_event.others,vars);
  printf ("Variables: %s\n",vars);

  file_open = true;
}

void is445_may08_unpack::output_event()
{
  if (file_open)
    _ntu.hfnt();
}

void is445_may08_unpack::close_output()
{
  _file.close();
  printf ("END! (of ntuple writing.  file closed)\n");
}

#endif


