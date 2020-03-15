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

#include "midas32_caen_item_limited.spec"
#include "spec/midas32.spec"

SUBEVENT(EV_EVENT)
{
  select several
    {
      adc[0] = ITEM_LIMITED_MIDAS_CAEN_V785(group=0x01);
      adc[1] = ITEM_LIMITED_MIDAS_CAEN_V785(group=0x02,minitem=20,maxitem=31);
      adc[2] = ITEM_LIMITED_MIDAS_CAEN_V785(group=0x03);
      adc[3] = ITEM_LIMITED_MIDAS_CAEN_V785(group=0x04);
      adc[4] = ITEM_LIMITED_MIDAS_CAEN_V785(group=0x05);

      tdc[0] = MIDAS_CAEN_V1190(group=0x18/*24*/);
      tdc[1] = MIDAS_CAEN_V1190(group=0x1a/*24*/);

      scaler = ITEM_LIMITED_MIDAS_CAEN_V830(group=2,maxitem=9,minitem=0);
      scaler = ITEM_LIMITED_MIDAS_CAEN_V830(group=7);
    }
}


EVENT
{
  ev = EV_EVENT();
}

SIGNAL(DSSSD1_F_1_E, ev.adc[0].data[0],
       DSSSD1_F_16_E,ev.adc[0].data[15],DATA12);
SIGNAL(DSSSD1_B_1_E, ev.adc[0].data[16],
       DSSSD1_B_16_E,ev.adc[0].data[31],DATA12);
SIGNAL(DSSSD2_F_1_E, ev.adc[4].data[0],
       DSSSD2_F_16_E,ev.adc[4].data[15],DATA12);
SIGNAL(DSSSD2_B_1_E, ev.adc[4].data[16],
       DSSSD2_B_16_E,ev.adc[4].data[31],DATA12);
SIGNAL(DSSSD3_F_1_E, ev.adc[2].data[0],
       DSSSD3_F_16_E,ev.adc[2].data[15],DATA12);
SIGNAL(DSSSD3_B_1_E, ev.adc[2].data[16],
       DSSSD3_B_16_E,ev.adc[2].data[31],DATA12);
SIGNAL(DSSSD4_F_1_E, ev.adc[3].data[0],
       DSSSD4_F_16_E,ev.adc[3].data[15],DATA12);
SIGNAL(DSSSD4_B_1_E, ev.adc[3].data[16],
       DSSSD4_B_16_E,ev.adc[3].data[31],DATA12);

SIGNAL(BACK1_E,      ev.adc[1].data[20],DATA12);
SIGNAL(BACK2_E,      ev.adc[1].data[21],DATA12);
SIGNAL(BACK3_E,      ev.adc[1].data[22],DATA12);
SIGNAL(BACK4_E,      ev.adc[1].data[23],DATA12);

SIGNAL(DSSSD1_F_1_T, ev.tdc[0].data[0],
       DSSSD1_F_16_T,ev.tdc[0].data[15],DATA16);
SIGNAL(DSSSD1_B_1_T, ev.tdc[0].data[16],
       DSSSD1_B_16_T,ev.tdc[0].data[31],DATA16);
SIGNAL(DSSSD2_F_1_T, ev.tdc[0].data[32],
       DSSSD2_F_16_T,ev.tdc[0].data[47],DATA16);
SIGNAL(DSSSD2_B_1_T, ev.tdc[0].data[48],
       DSSSD2_B_16_T,ev.tdc[0].data[63],DATA16);
SIGNAL(DSSSD3_F_1_T, ev.tdc[0].data[64],
       DSSSD3_F_16_T,ev.tdc[0].data[79],DATA16);
SIGNAL(DSSSD3_B_1_T, ev.tdc[0].data[80],
       DSSSD3_B_16_T,ev.tdc[0].data[95],DATA16);
SIGNAL(DSSSD4_F_1_T, ev.tdc[0].data[96],
       DSSSD4_F_16_T,ev.tdc[0].data[111],DATA16);
SIGNAL(DSSSD4_B_1_T, ev.tdc[0].data[112],
       DSSSD4_B_16_T,ev.tdc[0].data[127],DATA16);

SIGNAL(BACK1_T,      ev.tdc[1].data[0],DATA16);
SIGNAL(BACK2_T,      ev.tdc[1].data[1],DATA16);
SIGNAL(BACK3_T,      ev.tdc[1].data[2],DATA16);
SIGNAL(BACK4_T,      ev.tdc[1].data[3],DATA16);

SIGNAL(DSSSD1_TRIGT,  ev.tdc[1].data[4],DATA16);
SIGNAL(DSSSD2_TRIGT,  ev.tdc[1].data[5],DATA16);
SIGNAL(DSSSD3_TRIGT,  ev.tdc[1].data[6],DATA16);
SIGNAL(DSSSD4_TRIGT,  ev.tdc[1].data[7],DATA16);

SIGNAL(ZERO_SUPPRESS: DSSSD1_F_1);
SIGNAL(ZERO_SUPPRESS: DSSSD1_B_1);

SIGNAL(TRIG_ALL,      ev.scaler.data[0],DATA32);
SIGNAL(TRIG_ACCEPT,   ev.scaler.data[1],DATA32);

SIGNAL(SCALER_DSSSD1, ev.scaler.data[2],DATA32);
SIGNAL(SCALER_DSSSD2, ev.scaler.data[3],DATA32);
SIGNAL(SCALER_DSSSD3, ev.scaler.data[4],DATA32);
SIGNAL(SCALER_DSSSD4, ev.scaler.data[5],DATA32);

SIGNAL(SCALER_B1, ev.scaler.data[6],DATA32);
SIGNAL(SCALER_B2, ev.scaler.data[7],DATA32);
SIGNAL(SCALER_B3, ev.scaler.data[8],DATA32);
SIGNAL(SCALER_B4, ev.scaler.data[9],DATA32);


/*

SIGNAL(DSSSD1_F_1_E, ev.adc[0].data[0],
       DSSSD1_F_16_E,ev.adc[0].data[15],(DATA12, float));
SIGNAL(DSSSD1_B_1_E, ev.adc[0].data[16],
       DSSSD1_B_16_E,ev.adc[0].data[31],(DATA12, float));
SIGNAL(DSSSD2_F_1_E, ev.adc[1].data[0],
       DSSSD2_F_16_E,ev.adc[1].data[15],(DATA12, float));
SIGNAL(DSSSD2_B_1_E, ev.adc[1].data[16],
       DSSSD2_B_16_E,ev.adc[1].data[31],(DATA12, float));

SIGNAL(DSSSD1_F_1_T, ev.tdc[0].data[0],
       DSSSD1_F_16_T,ev.tdc[0].data[15],(DATA16, float));

*/
