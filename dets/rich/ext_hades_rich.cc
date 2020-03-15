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

#include "ext_hades_rich.hh"

#include "error.hh"

#include "pretty_dump.hh"

/*---------------------------------------------------------------------------*/

rich_xy2upi_map _rich_map;

/*---------------------------------------------------------------------------*/

EXT_HADES_RICH::EXT_HADES_RICH()
{
}

EXT_HADES_RICH::~EXT_HADES_RICH()
{
}

void EXT_HADES_RICH::__clean()
{
  data.__clean();
}

void rich_rawdata_word::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"(%d,%d,%d,%d,%2d:%3d)",sector,controller,port,module,channel,value);
  pretty_dump(id,buf,pdi);
}

void EXT_HADES_RICH::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  data.dump(id,pdi);
}

EXT_DECL_DATA_SRC_FCN(void,EXT_HADES_RICH::__unpack)
{
  if (__buffer.left() >= sizeof(rich_rawdata_word) * 8192)
    ERROR("More data in subevent than structure can hold.  (impossible)");

  while (!__buffer.empty())
    {
      rich_rawdata_word d;

      if (!__buffer.get_uint32(&d.u32))
	ERROR("Failed to fetch data.  (unaligned?)");

      /*
      printf ("%d %d %d %d %2d : %3d : 0x%08x\n",
	      d.sector,
	      d.controller,
	      d.port,
	      d.module,
	      d.channel,
	      d.value,
	      d.u32);
      */

      // Check that the data value belongs to the correct sector

      //if (d.u32 & (0x03800000/*sector*/ | 0xfc000000/*dummy*/) != _fixed.u32)
      //	ERROR("Sector,unused (%d) mismatch subevent fixed sector,unused (%d,%d)",
      //	      d.sector,  d.dummy,
      //	      _fixed.sector,_fixed.dummy);

      rich_rawdata_word &item = data.append_item();

      item = d;

      /*
      uint16 cpmc = d.cpmc;

      rich_map_xy_item &xy = _rich_map._upi2xy._upif[cpmc];
      */
    }
}
EXT_FORCE_IMPL_DATA_SRC_FCN(void,EXT_HADES_RICH::__unpack)

/*---------------------------------------------------------------------------*/
