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

external EXTERNAL_DATA_SKIP();

SUBEVENT(XTST_TRIG10)
{
  external data = EXTERNAL_DATA_SKIP();
}

SUBEVENT(XTST_TRIG1)
{

}

SUBEVENT(XTST_TRIG14)
{

}

SUBEVENT(XTST_TRIG15)
{

}

EVENT
{
  trig1  = XTST_TRIG10(type=0x100,subtype=0x101);
  trig10 = XTST_TRIG10(type=0x100,subtype=0x10a);
  trig14 = XTST_TRIG14(type=0x100,subtype=0x10e);
  trig15 = XTST_TRIG15(type=0x100,subtype=0x10f);




}
