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

#include "ext_hirich.hh"

#include "error.hh"

#include "pretty_dump.hh"

/*---------------------------------------------------------------------------*/

EXT_HIRICH::EXT_HIRICH()
{
}

EXT_HIRICH::~EXT_HIRICH()
{
}

void hirich_header_word::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"i:%3d,ec:%3d,c:%3d",module_no,event_count,count);
  pretty_dump(id,buf,pdi);
}

void hirich_data_word::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"(%3d:%3d)",channel,value);
  pretty_dump(id,buf,pdi);
}

void EXT_HIRICH_module::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  header.dump(signal_id(id,"header"),pdi);
  data.dump(signal_id(id,"data"),pdi);
}

void EXT_HIRICH::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  for (int i = 0; i < _num_modules; i++)
    _modules[i].dump(signal_id(id,i),pdi);
}

void EXT_HIRICH::__clean()
{
  _num_modules = 0;
}

void EXT_HIRICH_module::append_channel(uint16 d,
				       bitsone<256> &channels_seen)
{
  hirich_data_word &item1 = data.append_item();
  item1.u16 = d;

  if (channels_seen.get_set(item1.channel))
    ERROR("Channel %d seen twice.",item1.channel);
}

EXT_DECL_DATA_SRC_FCN(void,EXT_HIRICH::__unpack)
{
  // The data format has no global length specifiers, and the subevent
  // is also surrounded by some dummy values, HIRICH_MARKER_HEADER
  // before all the data and HIRICH_MARKER_FOOTER after all the data

  // As the footer does not contain the 'module header valid marker'
  // we'll use a strategy whereby if the next data word for a moudle
  // header is not valid, we'll also check to see if it perhaps is the
  // footer marker, in which case we'll abort unpacking, and leave it
  // for the subevent itself to handle the footer.  This way, we'd
  // also be able to handle data without these headers and footers...

  if (__buffer.left() & 3)
    ERROR("Subevent length not multiple of 4.");
  // Without the multiple of 4 check, we'd need to check that
  // peek_uint32 succeeded

  bitsone<128> modules_seen;
  modules_seen.clear();

  bitsone<256> channels_seen;

  while (!__buffer.empty())
    {
      hirich_header_word h;

      // peek ourselves an header (may be footer, then we should not
      // eat it)

      __buffer.peek_uint32(&h.u32);

      if (h.u32 == HIRICH_MARKER_FOOTER)
	break; // we're done

      __buffer.advance(sizeof(hirich_header_word));

      if (!h.valid)
	{

	  // We'll completely ignore module headers which were marked
	  // invalid
	  continue;
	}

      // So we have a new module, check that we have not seen it before

      if (modules_seen.get_set(h.module_no))
	ERROR("Header for module %d seen twice...",h.module_no);

      // Since we don't allow duplictes, we cannot overflow the
      // _modules array

      EXT_HIRICH_module *module = &_modules[_num_modules++];

      module->header = h;
      module->data.__clean();

      // Then unpack the data, we should have (h.count + 1) / 2 data
      // words with 2 16 bit entries each, and if count is even!!
      // then there is one more 32-bit data word, with the high part
      // holding another 16 bit entry

      // count   data words         size
      // 0       1: DE              4
      // 1       2: DD              4
      // 2       3: DD DE           8
      // 3       4: DD DD           8
      // 4       5: DD DD DE        12

      // 255 -> 256 data words

      size_t data_size = (2*h.count + 4) & ~3;

      if (__buffer.left() < data_size)
	ERROR("Not enough data left (%d) in subevent "
	      "for module data (count=%d -> size=%d).",
	      (int) __buffer.left(),h.count,(int) data_size);

      channels_seen.clear();

      for (int i = (h.count + 1) / 2; i; --i)
	{
	  uint32 d = 0; // = 0 to avoid compiler warning

	  __buffer.get_uint32(&d);

	  module->append_channel((uint16) (d >> 16),channels_seen);
	  module->append_channel((uint16) d,channels_seen);
	}

      if (!(h.count & 1))
	{
	  uint32 d = 0; // = 0 to avoid compiler warning

	  __buffer.get_uint32(&d);

	  module->append_channel((uint16) (d >> 16),channels_seen);

	  if (d & 0x0000ffff)
	    ERROR("hirich filler data not 0, %08x",d);
   	}





    }

}
EXT_FORCE_IMPL_DATA_SRC_FCN(void,EXT_HIRICH::__unpack)

/*---------------------------------------------------------------------------*/

void EXT_HIRICH::map_members(const struct EXT_HIRICH_map &map MAP_MEMBERS_PARAM) const
{


}

/*---------------------------------------------------------------------------*/
