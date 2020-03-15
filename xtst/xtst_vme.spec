// -*- C++ -*-

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

XTST_STD_VME()
{
  UINT32 failure
    {
      0:  fail_general;
      1:  fail_data_corrupt;
      2:  fail_data_missing;
      3:  fail_data_too_much;
      4:  fail_event_counter_mismatch;
      5:  fail_readout_error_driver;
      6:  fail_unexpected_trigger;

      27: has_no_zero_suppression;
      28: has_multi_adctdc_counter0;
      29: has_multi_scaler_counter0;
      30: has_multi_event;
      31: has_time_stamp;
    }

  if (failure.has_time_stamp) {
    UINT32 time_stamp;
  }

  if (failure.has_multi_event) {
    UINT32 multi_events;
  }

  if (failure.has_multi_scaler_counter0) {
    UINT32 multi_scaler_counter0;
  }

  if (failure.has_multi_adctdc_counter0) {
    UINT32 multi_adctdc_counter0;
  }

}


SINGLE_DATA_MODULE()
{
  UINT32 data;
};

SUBEVENT(XTST_VME)
{
  header = XTST_STD_VME();

  sdm = SINGLE_DATA_MODULE();

  select several
    {
      tdc0 = VME_CAEN_V775(geom=20,crate=20);
      tdc1 = VME_CAEN_V775(geom=21,crate=20);
      tdc2 = VME_CAEN_V775(geom=22,crate=20);
      qdc0 = VME_CAEN_V775(geom=21,crate=40);
      qdc1 = VME_CAEN_V775(geom=21,crate=40);
    }
  select several
    {
      mdpp16 = VME_MESYTEC_MDPP16(geom=31);
    }
}

#ifdef UNPACKER_IS_xtst_toggle

XTST_TGL_STD_VME()
{
  UINT32 separator {
    0_31: 9;
  }

  UINT32 multi_events;
  UINT32 multi_adctdc_counter0;

  UINT32 toggle {
    0_15:  ntgl1;
    16_31: ntgl2;
  }
}

ROLLING_TRLO_EVENT_TRIGGER()
{
  MEMBER(DATA32 lo);
  MEMBER(DATA32 hi);
  MEMBER(DATA32 tpat);
  
  UINT32 time_lo NOENCODE
  {
    0_31: val; // to be similar as time_hi
    ENCODE(lo, (value=val));
  }
  UINT32 time_hi NOENCODE
  {
    0_30: val;
    31:   missed_event = 0; // or we had serious issues; bad event
    ENCODE(hi, (value=val));
  }
  UINT32 status
  {
    0_23:  tpat;
    24_27: trig;
    28_31: count;

    ENCODE(tpat, (value=tpat));
  };
}

TRLO_SHADOW_MULTI_TRIGGERS()
{
  UINT32 header
  {
    0_16: events;
    24_31: 0xdf;
  }

  list(0 <= index < header.events)
    {
      multi events = ROLLING_TRLO_EVENT_TRIGGER();
    }
}

SUBEVENT(XTST_VME_TOGGLE)
{
  header = XTST_TGL_STD_VME();

  UINT32 separator1 NOENCODE
  {
    0_31: 0;
  }

  UINT32 seed;

  // UINT32 adc_cnt0;

  select several
  {
    multitrig = TRLO_SHADOW_MULTI_TRIGGERS();
  }

  select several
    {
      multi v775_tgl1[0] = VME_CAEN_V775(geom=1,crate=0x7f);
      multi v775_tgl2[0] = VME_CAEN_V775(geom=2,crate=0x7e);
      multi v775mod[0]   = VME_CAEN_V775(geom=3,crate=0x7d);
      multi v775_tgl1[1] = VME_CAEN_V775(geom=4,crate=0x7c);
      multi v775_tgl2[1] = VME_CAEN_V775(geom=5,crate=0x7b);
      multi v775mod[1]   = VME_CAEN_V775(geom=6,crate=0x7a);
    }

  UINT32 separator2 NOENCODE
  {
    0_31: 0;
  }
}

#endif
