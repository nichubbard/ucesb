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

SUBEVENT(LAND_CAMAC_SCALER)
{
  scaler0 = CAMAC_LECROY_4434(channels=32);
  scaler1 = CAMAC_LECROY_4434(channels=32);
  scaler2 = CAMAC_LECROY_4434(channels=32);

  // On special conditions, the event ends here...

  if (EXTERNAL has_timestamp)
    {
      UINT32 timestamp;
      UINT32 endianness { 0_31: 0x87654321; }
    }
}
