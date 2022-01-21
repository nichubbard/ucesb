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

#include "cros3_ext.hh"

#include "error.hh"

#define STORE_BUFFER_PARAM ,wire_hits &store
#define STORE_BUFFER_ARG   ,store
#define STORE_WIRE_HIT(wire,start_slice)                { store.hit(wire,start_slice); }
#define STORE_WIRE_HIT_TOT(wire,start_slice,stop_slice) { store.hit(wire,start_slice,stop_slice); }

#define STORE_TRC_PARAM ,cros3_threshold_stat *trc
#define STORE_TRC_COUNTS(thr,wire,count,max_count)      { STORE_CROS3_THR(trc,thr,wire,count,max_count); }

#define restrict

#define CROS3_ERROR(error,found) ERROR("cros3 error: " #error " %04x",found)

#define CROS3_TOT_ENCODED_DETECT_NOISY_WIRES 1

#include "rewrite_cros3_v27.c"

/*---------------------------------------------------------------------------*/

#include "structures.hh"

CROS3_REWRITE cros3_rewrite_unpack_test;

/*---------------------------------------------------------------------------*/

EXT_CROS3::EXT_CROS3()
{
  _scramble_buffer = NULL;
  _scramble_length = 0;

  _dest_buffer = (uint32 *) malloc (sizeof(uint32) * CROS3_REWRITE_MAX_DWORDS);
  _dest_buffer_end = _dest_buffer + CROS3_REWRITE_MAX_DWORDS;

  memset(&trc,0,sizeof(trc));

  _noise_stat = malloc(sizeof(rw_cros3_tot_noisy));
  memset(_noise_stat,0,sizeof(rw_cros3_tot_noisy));
}

EXT_CROS3::~EXT_CROS3()
{
  free (_scramble_buffer);
  free (_dest_buffer);

  free (_noise_stat);
}

void EXT_CROS3::__clean()
{
  data.__clean();
  _dest_end = _dest_buffer;
}

void wire_hit::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  char buf[32];
  sprintf(buf,"(%d,%d,%d)",wire,start,stop);
  pretty_dump(id,buf,pdi);
}

void wire_hits::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  for (int i = 0; i < num_hits; i++)
    hits[i].dump(id,pdi);
}

void EXT_CROS3::dump(const signal_id &id,pretty_dump_info &pdi) const
{
  data.dump(id,pdi);
}

EXT_DECL_DATA_SRC_FCN_ARG(void,EXT_CROS3::__unpack,uint16 ccb_id)
{
  rw_cros3_check  check;  // static information (known by DAQ)
  rw_cros3_status status; // event-wise information

  // Now, in order to be able to use the rewriter functions also for
  // unpacking, we'll first do some preliminary unpacking of the headers.

  // This is necessary as the rewriter is meant to be running in the
  // DAQ, which in turn is expected to know how the cros3 boards are
  // programmed.  With that knowledge, many header words are checked,
  // but then forgotten (as they are always the same).  As it would
  // complain at anything not being as expected, we need to set up the
  // unpacker functions with the information we get from the header.

  // we do not accept all kinds of data that in principle are valid, as
  // we expect the DAQ not to e.g. program one front-end board to
  // deliver threshold data while others deliver encoded data etc, so
  // we'll declare it enough to peek into the first device's data

  // One special thing is if there comes data from a CCB without any
  // AD16 board data, i.e. 4 header words and 2 footer words.  since the
  // data then is equal for all modes, we'll use the encoded data format
  // handling, just to validate the headers...


  // Peeking into the sixth word (where TRC) should always be possible,
  // just be careful if it is a footer

  uint16 *data_begin = (uint16*) __buffer._data;

  uint16 *src_begin  = (uint16*) __buffer._data;
  uint16 *src_end    = (uint16*) __buffer._end;

  if (__buffer.left() < sizeof(uint16) * 6)
    ERROR("cros3 data too small, no space for ccb header and footer");

  uint16 ch1;
  uint16 ch2;
  uint16 ch3;
  uint16 ch4;

  GET_BUFFER_UINT16(ch1);
  GET_BUFFER_UINT16(ch2);
  GET_BUFFER_UINT16(ch3);
  GET_BUFFER_UINT16(ch4);

  check.orig.csr_den = (uint16) ((ch1 & 0x00ff) | (ch2 & 0x00ff) << 8);
  check.orig.csr_ini = (uint16) ((ch3 & 0x00ff) | (ch4 & 0x00ff) << 8);
  check.orig.ccb_id = ccb_id;

  // The header has been peeked into.  Now figure out what kind of
  // data to expect

  uint16 ad1;
  uint16 ad2;

  GET_BUFFER_UINT16(ad1);
  GET_BUFFER_UINT16(ad2);

  uint32 mode = 0;

  // sixth word should either be a device header word two, or a footer
  // word 6

  if ((ad2 & 0xff00) == 0xde00)
    {
      // We found a footer!

      // There is no data to be expected

      check.orig.trc      = 0;
      check.orig.data.drc = 0;

      cros3_precalc_data(&check);

      mode = 3; // do header+footer checking as if encoded data

      // It can also be an syncronisation trigger, in which case
      // the first byte as 0xaf

      if ((ch1 & 0xff00) == 0xaf00)
	{
	  mode = 5; // 5 is just an internal number
	}
    }
  else if ((ad2 & 0xf000) == 0xc000)
    {
      mode = ad1 & 0x0803;

      if (mode == 0) // threshold curve mode
	{
	  check.orig.trc       = ad1 & 0x0fff;
	  check.orig.curve.thc = ad2 & 0x0fff;

	  cros3_precalc_curve(&check);
	}
      else
	{
	  // raw or encoded data mode

	  check.orig.trc      = ad1 & 0x0fff;
	  check.orig.data.drc = ad2 & 0x0fff;

	  cros3_precalc_data(&check);
	}
    }
  else
    {
      ERROR("cros3 data malformed, sixth word (%04x) neither "
	    "device header, nor ccb footer.",ad2);
    }

  // Hmm, another, not so funny part with using the rewriter functions
  // is that the data we provide here has been swapped by the MBS, just
  // in order to help us...  I've said it before, and I'll say it again,
  // the MBS byte swapping the LMD data on 32-bit boundaries is (at
  // best) ill-advised, since it then _will_ scramble 16-bit data (as is
  // the case here.

  // Of course, when passing data from one architecture to another, byte
  // swapping is necessary.  But to also 're-define' the order of data
  // does not exactly help.  Either one defines the data structures with a
  // specific endianess (e.g. 'network order' = big endian), and both
  // writer and reader obeys this, or one marks the endianess (basically
  // only if one is concerned that the writer does not have the time to
  // do the swapping.

  // As is right now, the rewriter code expects the data to come in
  // order.  There is currently no provision for byte-swapping, but
  // this can easily be done.  So we'll take the extremely ugly
  // approach of creating a second array of the data that is
  // native-endian, correct order.  The major bad thing about this, is
  // that we'll be swapping the second ccb board data twice (since we
  // do not now how long the first board data is...)

  size_t length = (char*) src_end - (char*) src_begin;

  if (__buffer.is_swapping() || __buffer.is_scrambled())
    {
      if (length > _scramble_length)
	{
	  uint16 *new_buf = (uint16 *) realloc (_scramble_buffer,length);
	  _scramble_buffer = new_buf;
	  _scramble_length = length;
	}

      __data_src_t src_all(src_begin,src_end);

      uint16 *pd = _scramble_buffer;

      for (size_t i = length / 2; i; --i)
	{
	  src_all.get_uint16(pd);
	  pd++;
	}

      src_begin = _scramble_buffer;
      src_end   = _scramble_buffer + length / 2;
    }

  //////////////////////////////////////////////////////////////////////

  uint32 *dest = _dest_buffer;

  uint16 *src_stop = NULL;

  // Now we have done the dirty tricks necessary to simulate knowing how
  // the modules were programmed.  The checking by the DAQ (which should
  // know) should thus be similar to the code below

  if (mode == 2) // seems mode 2 behaves unexpectedly, treat as mode 1 instead
    mode = 1;

  status.ad16_seen = 0;

  switch (mode)
    {
    case 0:
      dest = cros3_rewrite_threshold_curve(src_begin,src_end,
					   &src_stop,
					   &check,&status,
					   dest,
					   &trc);
      break;
    case 1:
      dest = cros3_rewrite_tot_raw(src_begin,src_end,
				   &src_stop,
				   &check,&status,
				   dest,
				   data);
      break;
    case 2:
      dest = cros3_rewrite_le_raw(src_begin,src_end,
				  &src_stop,
				  &check,&status,
				  dest,
				  data);
      break;
    case 3:
      dest = cros3_rewrite_le_encoded(src_begin,src_end,
				      &src_stop,
				      &check,&status,
				      dest,
				      data);
      break;
    case 0x803:
      dest = cros3_rewrite_tot_encoded(src_begin,src_end,
				       &src_stop,
				       &check,&status,
#if CROS3_TOT_ENCODED_DETECT_NOISY_WIRES
				       (rw_cros3_tot_noisy *) _noise_stat,
#endif
				       dest,
				       data);

#if CROS3_TOT_ENCODED_DETECT_NOISY_WIRES
      if (!(((rw_cros3_tot_noisy *) _noise_stat)->events & 0x1ff))
	cros3_check_tot_noise((rw_cros3_tot_noisy *) _noise_stat);
#endif
      break;
    case 5:
      dest = cros3_rewrite_empty(src_begin,src_end,
				 &src_stop,
				 &check,&status,
				 dest);
      break;
    default:
      // cannot happen
      assert(false);
      break;
    }

  //////////////////////////////////////////////////////////////////////
  /*
  printf ("%08x : %08x : %08x\n",
	  _dest_buffer,
	  _dest_buffer + _dest_length,
	  dest);
  */
  if (dest > _dest_buffer_end)
    {
      ERROR("Internal rewriter error, destination buffer overflow.");
    }

  _dest_end = dest;

  //////////////////////////////////////////////////////////////////////
  // Now doing some checking that the routines did not screw up

  size_t used = (char *) src_stop - (char *) src_begin;

  if (used > length || src_stop < src_begin)
    {
      // This should never happen, even with malformed data
      ERROR("cros3 rewriter read too far (should be impossible)");
    }

  //////////////////////////////////////////////////////////////////////
  // And check that the rewriting was successful (we should be able)
  // to unpack it again...
  /*
  printf ("Original:\n");
  for (uint16 *p = src_begin; p < src_stop; p++)
    printf ("%04x ",*p);
  printf ("\n");

  printf ("Repacked:\n");
  for (uint32 *p = _dest_buffer; p < dest; p++)
    printf ("%08x ",*p);
  printf ("\n");
  */
  /*
  static int s_src  = 0;
  static int s_dest = 0;

  s_src  += 2*(src_stop-src_begin);
  s_dest += 4*(dest-_dest_buffer);

  printf ("%d -> %d  %d -> %d\n",
	  2*(src_stop-src_begin),4*(dest-_dest_buffer),
	  s_src,s_dest);
  */
  cros3_rewrite_unpack_test.__clean();
  __data_src<0,0,0/*account*/> buffer_dest(_dest_buffer,dest);
  cros3_rewrite_unpack_test.__unpack(buffer_dest,ccb_id);
  if (!buffer_dest.empty())
    ERROR("Did not read all rewritten data - pack error!");
/*
  printf("%5d -> %5d  %08x %08x\n",
	used,
	(dest-_dest_buffer)*4,
	*_dest_buffer,cros3_rewrite_unpack_test.h1);
*/
  //////////////////////////////////////////////////////////////////////

  __buffer._data = ((char *) data_begin) + used;
}
EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(void,EXT_CROS3::__unpack,uint16 ccb_id)

EXT_DECL_DATA_SRC_FCN_ARG(bool,EXT_CROS3::__match,uint16 ccb_id)
{
  uint16 ch3;

  if (__buffer.left() < sizeof(uint16) * 4)
    return false; // no space for header

  __buffer.advance(sizeof(uint16) * 2);

  GET_BUFFER_UINT16(ch3);

  // In principle, we could check all data up to the ccb_id, but that is
  // only necessary if we want to see if the matches are valid or not.
  // As we only expect to have to distinguish data from differend ccb's
  // (and nothing else, i.e. the subevent only has ccb data in it),
  // we'll simply check the ccb id

  // printf ("ch3:%08x\n",ch3);

  if ((ch3 & 0x0f00) == (ccb_id << 8))
    return true;

  return false;
}
EXT_FORCE_IMPL_DATA_SRC_FCN_ARG(bool,EXT_CROS3::__match,uint16 ccb_id)

/*---------------------------------------------------------------------------*/
