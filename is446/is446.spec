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

// The DAQ is rewriting headers, and abusing(?)
// undefined bits
#define VME_CAEN_V830_HEADER_24_25_UNDEFINED

#include "spec/spec.spec"

#include "is446_vme.spec"

EVENT
{
  vme  = IS446_VME(type=10,subtype=1,subcrate=0);
  vme2 = IS446_VME2(type=10,subtype=1,subcrate=1);




}
/*
SIGNAL(SCI1_1_T,vme.tdc0.data[0],DATA12);
SIGNAL(SCI1_2_T,vme.tdc0.data[1],DATA12);
SIGNAL(SCI1_1_E,vme.qdc0.data[0],DATA12);
SIGNAL(SCI1_2_E,vme.qdc0.data[1],DATA12);
*/
/*
SIGNAL(SCI1_1_T,vme.tdc[0].data[0],(DATA12,float));
SIGNAL(SCI1_1_E,vme.adc[0].data[0],(DATA12,float));
*/

SIGNAL(TEBIS         ,                     ,(DATA32,float));
SIGNAL(TSHORT        ,                     ,(DATA32,float));

SIGNAL(OLDTEBIS      ,                     ,(DATA32,float));

SIGNAL(OLDTSHORT     ,vme.scaler[0].data[0],(DATA32,float));
SIGNAL(EBIS          ,vme.scaler[0].data[1],(DATA32,float));
SIGNAL(NACCEPT       ,vme.scaler[0].data[2],(DATA32,float));

#ifdef UNPACKER_IS_is446
SIGNAL(BACK_1_E      ,vme.adc[6].data[0],   (DATA12,float));
SIGNAL(BACK_2_E      ,vme.adc[6].data[1],   (DATA12,float));

SIGNAL(BACK_1_T      ,vme.tdc[4].data[0],   (DATA12,float));
SIGNAL(BACK_2_T      ,vme.tdc[4].data[1],   (DATA12,float));
#endif/*UNPACKER_IS_is446*/

/* is446 was not using toggle.  We just use the unpacker
 * for some testing, as there are easy data files. */
#ifdef UNPACKER_IS_is446_toggle
SIGNAL(TOGGLE 1: TGL_1_E      ,vme.adc[6].data[0],   (DATA12,float));
SIGNAL(TOGGLE 2: TGL_1_E      ,vme.adc[6].data[1],   (DATA12,float));

SIGNAL(TOGGLE 1: TGL_1_T      ,vme.tdc[4].data[0],   (DATA12,float));
SIGNAL(TOGGLE 2: TGL_1_T      ,vme.tdc[4].data[1],   (DATA12,float));
#endif
#ifdef UNPACKER_IS_is446_tglarray
SIGNAL(TOGGLE 1: TGL_3_E      ,vme.adc[6].data[0],   (DATA12,float));
SIGNAL(TOGGLE 2: TGL_3_E      ,vme.adc[6].data[1],   (DATA12,float));

SIGNAL(TOGGLE 1: TGL_2_T      ,vme.tdc[4].data[0],   (DATA12,float));
SIGNAL(TOGGLE 2: TGL_2_T      ,vme.tdc[4].data[1],   (DATA12,float));
SIGNAL(ZERO_SUPPRESS: TGL_3);
#endif

SIGNAL(MON_1_E       ,vme.adc[6].data[2],   (DATA12,float));
SIGNAL(MON_2_E       ,vme.adc[6].data[3],   (DATA12,float));

SIGNAL(MON_1_T       ,vme.tdc[4].data[4],   (DATA12,float));
SIGNAL(MON_2_T       ,vme.tdc[4].data[5],   (DATA12,float));

SIGNAL(D_1_F_1_E ,vme.adc[0].data[0],
       D_1_F_32_E,vme.adc[0].data[31],  (DATA12,float));
SIGNAL(D_1_B_1_E ,vme.adc[1].data[0],
       D_1_B_32_E,vme.adc[1].data[31],  (DATA12,float));
SIGNAL(D_1_F_1_T ,vme.tdc[0].data[0],
       D_1_F_32_T,vme.tdc[0].data[31],  (DATA12,float));
SIGNAL(D_1_B_1_T ,vme.tdc[1].data[0],
       D_1_B_32_T,vme.tdc[1].data[31],  (DATA12,float));

SIGNAL(D_2_F_1_E ,vme.adc[2].data[0],
       D_2_F_32_E,vme.adc[2].data[31],  (DATA12,float));
SIGNAL(D_2_B_1_E ,vme.adc[3].data[0],
       D_2_B_32_E,vme.adc[3].data[31],  (DATA12,float));
SIGNAL(D_2_F_1_T ,vme.tdc[2].data[0],
       D_2_F_32_T,vme.tdc[2].data[31],  (DATA12,float));

SIGNAL(D_3_F_1_E ,vme.adc[4].data[0],
       D_3_F_32_E,vme.adc[4].data[31],  (DATA12,float));
SIGNAL(D_3_B_1_E ,vme.adc[5].data[0],
       D_3_B_32_E,vme.adc[5].data[31],  (DATA12,float));
SIGNAL(D_3_F_1_T ,vme.tdc[3].data[0],
       D_3_F_32_T,vme.tdc[3].data[31],  (DATA12,float));

SIGNAL(ZERO_SUPPRESS: D_1_F_1);
SIGNAL(ZERO_SUPPRESS: D_1_B_1);

SIGNAL(TLAST,,DATA32);
SIGNAL(TLONG,,DATA32);
