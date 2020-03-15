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

SUBEVENT(LAND_CAMAC_CONVERTER)
{
  UINT16 tpat;
  UINT16 coinc2 { 0_15: 0x0000; }
  UINT16 coinc3 { 0_15: 0x0000; }
  UINT16 pad {
    0_15: 0x4321;
  }

  tPHI7079  = CAMAC_PHILLIPS_7164H(channels=16,mark_channel_no=1);
  tPHI7186  = CAMAC_PHILLIPS_7164H(channels=16,mark_channel_no=1); // not before COSMIC2
  aPHI12160 = CAMAC_PHILLIPS_7186H(channels=16,mark_channel_no=1);

  qSIA0415 = CAMAC_SILENA_4418Q(channels=8,mark_channel_no=0);
  qSIA0458 = CAMAC_SILENA_4418Q(channels=8,mark_channel_no=0);
  qSIA0418 = CAMAC_SILENA_4418Q(channels=8,mark_channel_no=0);
  tSIA0428 = CAMAC_SILENA_4418T(channels=8,mark_channel_no=0);
  tSIA0400 = CAMAC_SILENA_4418T(channels=8,mark_channel_no=0);
  tSIA0426 = CAMAC_SILENA_4418T(channels=8,mark_channel_no=0);
  tSIA0401 = CAMAC_SILENA_4418T(channels=8,mark_channel_no=0);
}
