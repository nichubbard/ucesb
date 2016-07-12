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

SUBEVENT(LAND_FASTBUS_ERROR)
{
  UINT32 error_code
    {
      0:  timeout_waiting_smi;
      1:  illegal_block_transfer;
      2:  fifa_address_overflow;
      3:  fifa_register_error;
      16: init_error;
    }
  UINT32 error_num;
}


SUBEVENT(LAND_FASTBUS)
{
  UINT32 zero NOENCODE { 0_31: 0; }

  select several
    {
      T87044 = FASTBUS_LECROY_1875(geom= 4,channels=64);
      T87024 = FASTBUS_LECROY_1875(geom= 5,channels=64);
      T81800 = FASTBUS_LECROY_1875(geom= 6,channels=64);
      Q47342 = FASTBUS_LECROY_1885(geom= 7,channels=96);
      Q11111 = FASTBUS_LECROY_1885(geom= 8,channels=96);
      Q47264 = FASTBUS_LECROY_1885(geom= 9,channels=96);
      Q46848 = FASTBUS_LECROY_1885(geom=10,channels=96);
      T81855 = FASTBUS_LECROY_1875(geom=12,channels=64);
      Q17320 = FASTBUS_LECROY_1885(geom=13,channels=96);
      Q47315 = FASTBUS_LECROY_1885(geom=14,channels=96);
      Q15372 = FASTBUS_LECROY_1885(geom=15,channels=96);
      Q46962 = FASTBUS_LECROY_1885(geom=16,channels=96);
      Q15358 = FASTBUS_LECROY_1885(geom=17,channels=96);
      Q46993 = FASTBUS_LECROY_1885(geom=18,channels=96);
      T87047 = FASTBUS_LECROY_1875(geom=19,channels=64);
      T48854 = FASTBUS_LECROY_1875(geom=20,channels=64);
      T81808 = FASTBUS_LECROY_1875(geom=21,channels=64);
      T81859 = FASTBUS_LECROY_1875(geom=22,channels=64);
      T81795 = FASTBUS_LECROY_1875(geom=23,channels=64);
      T81806 = FASTBUS_LECROY_1875(geom=24,channels=64);
      T48834 = FASTBUS_LECROY_1875(geom=25,channels=64);
    }
}
