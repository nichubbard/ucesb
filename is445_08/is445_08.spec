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

SUBEVENT(IS445_08_VME)
{
  header = LAND_STD_VME();

  select several
    {
      multi scaler0 = VME_CAEN_V830(geom=30);

      multi tdc[0] = VME_CAEN_V775(geom=16,crate=129);
      multi tdc[1] = VME_CAEN_V775(geom=17,crate=130);
      multi tdc[2] = VME_CAEN_V775(geom=18,crate=131);

      multi adc[0] = VME_CAEN_V792(geom=0,crate=1);
      multi adc[1] = VME_CAEN_V785(geom=1,crate=2);
      multi adc[2] = VME_CAEN_V785(geom=2,crate=3);
    }
}

EVENT
{
  vme = IS445_08_VME(type=10,subtype=1);


}

// This enforces zero suppression at strip level

SIGNAL(ZERO_SUPPRESS: DSSSD1_F_32);
SIGNAL(ZERO_SUPPRESS: DSSSD1_B_32);

// ADC channels

SIGNAL(DSSSD1_F_1_E, vme.adc[0].data[0],
       DSSSD1_F_32_E,vme.adc[0].data[31],(DATA12, float));

SIGNAL(DSSSD1_B_1_E, vme.adc[1].data[0],
       DSSSD1_B_32_E,vme.adc[1].data[31],(DATA12, float));

SIGNAL(BACK1_E,      vme.adc[2].data[0], (DATA12, float));
SIGNAL(MON_E,        vme.adc[2].data[1], (DATA12, float));

// TDC channels

SIGNAL(DSSSD1_B_1_T, vme.tdc[0].data[0],
       DSSSD1_B_32_T,vme.tdc[0].data[31],(DATA12, float));

SIGNAL(DSSSD1_F_1_T, vme.tdc[1].data[0],
       DSSSD1_F_32_T,vme.tdc[1].data[31],(DATA12, float));

SIGNAL(BACK1_T,      vme.tdc[2].data[0],(DATA12, float));
SIGNAL(TRIG_T,       vme.tdc[2].data[1],(DATA12, float));

SIGNAL(DSSSD1_FT,    vme.tdc[2].data[2],(DATA12, float));
SIGNAL(DSSSD1_BT,    vme.tdc[2].data[3],(DATA12, float));

// Scaler channels

SIGNAL(CLOCK,        vme.scaler0.data[0],DATA32);
SIGNAL(CNTTRIGRAW,   vme.scaler0.data[1],DATA32);
SIGNAL(CNTTRIGACCEPT,vme.scaler0.data[2],DATA32);
SIGNAL(CNTPROTONS,   vme.scaler0.data[3],DATA32);
SIGNAL(CNTEBIS,      vme.scaler0.data[4],DATA32);

// Values that will be calculated in the user function

SIGNAL(TEBIS,                           ,(DATA32, float));
SIGNAL(TSHORT,                          ,(DATA32, float));

// This one should not map!!!  One can just as well take
// it from the unpack event.  It is here just to test LAST_EVENT
SIGNAL(LAST_EVENT: TIME,     vme.header.time_stamp,uint32);

