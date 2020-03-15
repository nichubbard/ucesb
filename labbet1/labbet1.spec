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

#include "spec/midas.spec"

SUBEVENT(EV_EVENT)
{
  select several
    {
      adc1 = SIMPLE_DATA(group=0x20);
    }
}


EVENT
{
  // vme = RPC2006_VME(type=10,subtype=1);

  ev = EV_EVENT();



}

SIGNAL(DSSSD1_F_1_E, ev.adc1.data[0],
       DSSSD1_F_16_E,ev.adc1.data[15],(DATA16, float));
SIGNAL(DSSSD1_B_1_E, ev.adc1.data[16],
       DSSSD1_B_16_E,ev.adc1.data[31],(DATA16, float));

/*
SIGNAL(SCI1_1_T,vme.tdc0.data[0],DATA12);
SIGNAL(SCI1_2_T,vme.tdc0.data[1],DATA12);
SIGNAL(SCI1_1_E,vme.qdc0.data[0],DATA12);
SIGNAL(SCI1_2_E,vme.qdc0.data[1],DATA12);
*/

