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

AD413A_4CH()
{
  MEMBER(DATA16 data[4] ZERO_SUPPRESS);

  list(0 <= index < 4)
    {
      UINT16 item NOENCODE
	{
	  0_12: value;
	  13_15: 0;

	  ENCODE(data[index],(value=value));
	}
    }
}


SUBEVENT(GAMMA_K8_AD413A)
{
  select several
    {
      multi adc = AD413A_4CH();
    }
}

EVENT
{
  ad413a = GAMMA_K8_AD413A(type=10,subtype=1);

}

SIGNAL(EA , ad413a.adc.data[0],(DATA16, float));
SIGNAL(EB , ad413a.adc.data[1],(DATA16, float));
SIGNAL(T ,  ad413a.adc.data[3],(DATA16, float));

/*
SIGNAL(MWPC_X_TL, ev.tdc.data[0],DATA12);
SIGNAL(MWPC_X_TR, ev.tdc.data[1],DATA12);
SIGNAL(MWPC_Y_TU, ev.tdc.data[2],DATA12);
SIGNAL(MWPC_Y_TD, ev.tdc.data[3],DATA12);
SIGNAL(MWPC_X_EO, ev.qdc.data[0],DATA12);
SIGNAL(MWPC_X_EE, ev.qdc.data[1],DATA12);
SIGNAL(MWPC_Y_EO, ev.qdc.data[2],DATA12);
SIGNAL(MWPC_Y_EE, ev.qdc.data[3],DATA12);

SIGNAL(SCIN_T,    ev.tdc.data[8],DATA12);
SIGNAL(SCIN_E,    ev.qdc.data[8],DATA12);

*/
