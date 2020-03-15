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

#include "ebye_external.hh"
#include "error.hh"

EXT_EBYE_DATA::EXT_EBYE_DATA()
{

}

void EXT_EBYE_DATA::__clean()
{
}

EXT_DECL_DATA_SRC_FCN(void,EXT_EBYE_DATA::__unpack)
{
  while (!__buffer.empty())
    {
      uint16 w1;
      uint16 d;

      GET_BUFFER_UINT16(w1);

      switch (w1 >> 14)
	{
	case 0: // simple data
	  GET_BUFFER_UINT16(d);

	  //printf ("0: %02x:%02x : %04x\n",
	  //	  w1 & 0xff,(w1 >> 8) & 0x3f,d);

	  _group_item[w1 & 0xff][(w1 >> 8) & 0x3f]++;
	  break;
	case 1: // group data
	  {
	    uint16 count = (w1 >> 8) & 0x3f;

	    //printf ("1: %02x (%02x) :",
	    //	    w1 & 0xff,count);

	    for (int i = count; i; --i)
	      {
		GET_BUFFER_UINT16(d);
		//printf (" %04x",d);
	      }
	    if (!(count & 1))
	      {
		GET_BUFFER_UINT16(d);
		if (d != 0)
		  ERROR("Group data padding non-zero: 0x%04x",d);
	      }
	    //printf ("\n");

	    _group_data[w1 & 0xff]++;

	  }
	  break;
	case 2: // extended group data
	  {
	    uint16 count = w1 & 0x3fff;
	    uint16 group;

	    GET_BUFFER_UINT16(group);

	    //printf ("1: %02x (%02x) :",
	    //	    group,count);

	    for (int i = count; i; --i)
	      {
		GET_BUFFER_UINT16(d);
		//printf (" %04x",d);
	      }
	    if (count & 1)
	      {
		GET_BUFFER_UINT16(d);
		if (d != 0)
		  ERROR("Extended group data padding non-zero: 0x%04x",d);
	      }
	    //printf ("\n");
	    _ext_group_data[group]++;
	  }
	  break;
	case 3: // start/stop token
	  // this will throw an exception
	  ERROR("Unexpected start/stop token in data: 0x%04x",w1);
	}
    }
}
EXT_FORCE_IMPL_DATA_SRC_FCN(void,EXT_EBYE_DATA::__unpack);
