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

#define CAMAC_SILENA_4418T  CAMAC_SILENA_4418
#define CAMAC_SILENA_4418Q  CAMAC_SILENA_4418
#define CAMAC_SILENA_4418A  CAMAC_SILENA_4418


CAMAC_SILENA_4418(channels,mark_channel_no)
{
  MEMBER(DATA12_OVERFLOW data[8] ZERO_SUPPRESS);

  list(0<=index<channels)
    {
      if (mark_channel_no) {
	UINT16 ch_data NOENCODE
	  {
	    0_11:  value;
	    12_14: channel = CHECK(index);
	    15:    overflow;

	    ENCODE(data[index],(value=value,overflow=overflow));
	  }
      } else {
	UINT16 ch_data NOENCODE
	  {
	    0_11:  value;
	    15:    overflow;

	    ENCODE(data[index],(value=value,overflow=overflow));
	  }
      }
    }
}



