// -*- C++ -*-

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

#include "spec/spec.spec"

#include "spec/land_std_vme.spec"

#include "vme_m2j_matacq32.spec"

SUBEVENT(FA192MAR09_VME)
{
  header = LAND_STD_VME();

  select several
    {
      multi scaler[0] = VME_CAEN_V830(geom=30);
      multi scaler[1] = VME_CAEN_V830(geom=31);

      multi adc[0] = VME_CAEN_V785(geom=10,crate=1);
      multi adc[1] = VME_CAEN_V785(geom=11,crate=1);
      multi adc[2] = VME_CAEN_V785(geom=12,crate=1);
      multi adc[3] = VME_CAEN_V785(geom=0,crate=1);

      multi tdc[0] = VME_CAEN_V775(geom=7,crate=1);
      multi tdc[1] = VME_CAEN_V775(geom=8,crate=1);
      multi tdc[2] = VME_CAEN_V775(geom=9,crate=1);
      multi tdc[3] = VME_CAEN_V775(geom=18,crate=1);

      multi mhtdc[0] = VME_CAEN_V1290_SHORT(geom=24);
      multi mhtdc[1] = VME_CAEN_V1290_SHORT(geom=25);

      // For trigger 2 (and some others)
      sadc[0] = VME_M2J_MATACQ32(mod=0,ch=0);
      sadc[1] = VME_M2J_MATACQ32(mod=0,ch=1);

      // For trigger 1 (packed)
      sadc_pack[0] = VME_M2J_MATACQ32_PACK_EXT(mod=0,ch=0);
      sadc_pack[1] = VME_M2J_MATACQ32_PACK_EXT(mod=0,ch=1);
    }
}

EVENT
{
  vme = FA192MAR09_VME(type=10,subtype=1);

}

SIGNAL(ZERO_SUPPRESS: t1t1);
SIGNAL(ZERO_SUPPRESS: a1a1);

SIGNAL(ZERO_SUPPRESS: SSSD1_1);
SIGNAL(ZERO_SUPPRESS: LU1_1);
SIGNAL(ZERO_SUPPRESS: LU2_1);

SIGNAL(SSSD1_1t, vme.tdc[0].data[0],
       SSSD1_32t,vme.tdc[0].data[31], (DATA12, float));
SIGNAL(SSSD2_1t, vme.tdc[1].data[0],
       SSSD2_32t,vme.tdc[1].data[31], (DATA12, float));

SIGNAL(SSSD1_1e, vme.adc[0].data[0],
       SSSD1_32e,vme.adc[0].data[31], (DATA12, float));
SIGNAL(SSSD2_1e, vme.adc[1].data[0],
       SSSD2_32e,vme.adc[1].data[31], (DATA12, float));

SIGNAL(LU1_1t,   vme.tdc[2].data[0],
       LU1_16t,  vme.tdc[2].data[15],(DATA12, float));
SIGNAL(LU1_1e,   vme.adc[2].data[0],
       LU1_16e,  vme.adc[2].data[15],(DATA12, float));

SIGNAL(LU2_1t,   vme.tdc[2].data[16],
       LU2_16t,  vme.tdc[2].data[31],(DATA12, float));
SIGNAL(LU2_1e,   vme.adc[2].data[16],
       LU2_16e,  vme.adc[2].data[31],(DATA12, float));
/*
SIGNAL(t4t1,  vme.tdc[3].data[0],
       t4t32, vme.tdc[3].data[31], DATA12);

SIGNAL(a4a1,  vme.adc[3].data[0],
       a4a32, vme.adc[3].data[31], DATA12);
*/

SIGNAL(aa,    vme.adc[3].data[16],(DATA12, float));
SIGNAL(ab,    vme.adc[3].data[18],(DATA12, float));
SIGNAL(ac,    vme.adc[3].data[20],(DATA12, float));
SIGNAL(ad,    vme.adc[3].data[22],(DATA12, float));


//SIGNAL(ZERO_SUPPRESS: mht1t1);

//SIGNAL(NO_INDEX_LIST: mht1t32_128);
//
//SIGNAL(mht1t1_1,  vme.mhtdc[0].data[0],
//       mht1t32_1, vme.mhtdc[0].data[31], DATA24 /*(DATA24, float)*/);
//SIGNAL(mht2t1_1,  vme.mhtdc[1].data[0],
//       mht2t32_1, vme.mhtdc[1].data[31], DATA24 /*(DATA24, float)*/);

SIGNAL(ZERO_SUPPRESS_MULTI(128): mht1t1);

SIGNAL(mht1t1,  vme.mhtdc[0].data[0],
       mht1t32, vme.mhtdc[0].data[31], DATA24 /*(DATA24, float)*/);
SIGNAL(mht2t1,  vme.mhtdc[1].data[0],
       mht2t32, vme.mhtdc[1].data[31], DATA24 /*(DATA24, float)*/);

SIGNAL(NO_INDEX_LIST: sadc1_2560);
SIGNAL(sadc1_1, vme.sadc[0].data,DATA16);
SIGNAL(sadc2_1, vme.sadc[1].data,DATA16);

SIGNAL(NO_INDEX_LIST: sadcp1_2560);
SIGNAL(sadcp2_1, ,DATA16);

// Duplicate, for testing.
SIGNAL(NO_INDEX_LIST(2560): sadcp1);

SIGNAL(sadcpp_trigrec, vme.sadc_pack[0].trig_rec, uint16);
SIGNAL(sadcpp_vernier, vme.sadc_pack[0].vernier,  uint16);


