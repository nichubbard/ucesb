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

#include "lmd_input.hh"

#include "error.hh"
#include "colourtext.hh"

#include "util.hh"
#include "config.hh"
#include "hex_dump.hh"

#include <inttypes.h>
#include <time.h>

#include "worker_thread.hh"
#include "thread_buffer.hh"

// Now, we'll take an extremely lazy approach.

// When asked for the next event, we'll figure out where it is.  And
// that's all.

// When (and if) we then get asked for the subevents, we'll figure out
// where they are, and their (respective) headers.  That's it.

// Only when asked for a pointer to the subevent data, we'll fix a
// handle to that.  No swapping will be done.  But if the subevent was
// crossing the border of an fragmented event (stored in several
// buffers (records)), we'll make a copy of the event such that it is
// continous in memory.  This is so that the unpackers need not worry
// about sudden boundaries of the event that need to be crossed.
// (Doing the copy operation here is cheaper than having to constantly
// check if the data pointer has passed one of several boundaries.)


// Hmm, multi-processor operation?  We'll in general try to take
// advantage of SMP by letting several processors unpack and process
// several events in parallel.  The actual file reading (that must be
// sequential), is left to one of them, and also any final operation
// (like ntuple-writing) will be done by the master process.  Also, in
// case the program internally collect some sort of data (histograms),
// they must be mergable at the end, i.e. each thread that process
// events has it's own copy, and only at the end we'll merge the data.
// (So make sure that they still would not care in what order or what
// slices etc the events were processed).  In order to have the slaves
// constantly busy, the master will deliver many events to the slaves
// for processing, before starting to harvest the fruits (retire the
// events), and tell the file-reading process (if any, e.g. if reading
// a pipe from a decompressor) that 'mmap()ed' memory can be used
// again.

// This lmd reader is thread-safe in the sense that the memory-area
// that is used by an event after it has been returned as an event
// with header info and handle to the data, will not be reclaimed
// until the event is retired.  So another processor might start to
// call the functions to locate and get subevents if necessary.  Also,
// as long as the handle is shared, and only used by one thread at a
// time, the handle may be started to be operated upon by one thread,
// and then continued to be modified by someone else.  Also, all data
// inbetween the any fragments (i.e. buffer headers) will never be
// read again, so it is valid to do a memmove of any part of the event
// in order to join fragments, overwriting the header inbetween).
// Such memmove is however not allowed if the file was mmap()ed from
// the file-system (in which case we are anyhow read-only, and will
// cause a full seg-fault)

#if !USE_THREADING && !USE_MERGING
FILE_INPUT_EVENT _file_event;
#endif

lmd_source::lmd_source()
{
#ifndef USE_THREADING
  size_t n = 4;
  _file_event._chunks_ptr =
    (buf_chunk *) malloc(n * sizeof (buf_chunk));
  if (!_file_event._chunks_ptr)
    ERROR("Memory allocation failure!");
  _file_event._chunk_alloc = _file_event._chunks_ptr + n;
#endif
}

void lmd_source::new_file()
{
  _last_buffer_no = 0;

  _expect_file_header = true;
  _skip_record_try_again = false;

  _chunk_cur = _chunk_end = NULL;
  _events_left = 0;

  _prev_record_release_to  = 0;
  _prev_record_release_to2 = 0;
  _buffers_maybe_missing = false;

  _close_is_error = false;

  _file_header_seen = false;
}

#define min(a,b) ((a)<(b)?(a):(b))

bool lmd_source::read_record(bool expect_fragment)
{
  // Read the next record of the file

  //printf ("** READ-REC: prev %zd\n",_input._cur);

 read_buffer_again:
  _prev_record_release_to = _input._cur;

  _first_buf_status = 0;
  // Note: we may not release the previous record.  (We may just be
  // getting the next record for a fragmented event!)

  if (!_input.read_range(&_buffer_header,sizeof(_buffer_header)))
    return false;

  /* First we need to find out if the buffer header is in
   * big endian or little endian format...
   *
   * (We however treat it as: native endian or foreign endian)
   *
   * s_bufhe.l_free[0] = 0x00000001 in native endian
   */

  uint32 endian_free0 = (uint32) _buffer_header.l_free[0];

  if (endian_free0 == 0x00000001)
    _swapping = 0;
  else if (endian_free0 == bswap_32(0x00000001))
    _swapping = 1;
#if 0
  if (endian_free0 == 0x00000001)
    _buffer_status = 0;
  else if (endian_free0 == bswap_32(0x00000001))
    _buffer_status = SDS_SWAPPING;
  // This is for a suggested alternative, where the data itself is
  // never touched (left in producer endianess and order, and the
  // headers are always in network order (in which case we could
  // handle it still more easily, if we'd drop support for old lmd
  // files (which we wont :-) ))
  else if (endian_free0 == ntohl(0x00000003)) // headers network
					      // order, data big
					      // endian (network
					      // order)
    _buffer_status =
      (BYTE_ORDER == LITTLE_ENDIAN ? SDS_SWAPPING : 0) |
      SDS_NETWORKORDER_HEADERS |
      (BYTE_ORDER == LITTLE_ENDIAN ? SDS_DATA_SWAPPING : 0);
  else if (endian_free0 == ntohl(0x02000001)) // headers network
					      // order, data little
					      // endian (non-network)
    _buffer_status =
      (BYTE_ORDER == LITTLE_ENDIAN ? SDS_SWAPPING : 0) |
      SDS_NETWORKORDER_HEADERS |
      (BYTE_ORDER == BIG_ENDIAN ? SDS_DATA_SWAPPING : 0);
#endif
  else
    ERROR("Buffer header endian marker broken (l_free[0]): %08x",endian_free0);

  // swap members of the array

  //  if (IS_SWAPPING(_buffer_status))
  if (_swapping)
    byteswap_32(_buffer_header);

  if (_buffer_header.l_buf == 0 &&
      _buffer_header.i_used == 0 &&
      _buffer_header.l_free[2] == 0 &&
      _buffer_header.l_evt == 0 &&
      _buffer_header.i_type    == LMD_DUMMY_BUFFER_MARK_TYPE &&
      _buffer_header.i_subtype == LMD_DUMMY_BUFFER_MARK_SUBTYPE &&
      _buffer_header.l_free[3] == LMD_DUMMY_BUFFER_MARK_FREE_3)
    {
      // A dummy buffer (most likely internally produced in
      // lmd_input_tcp.cc).

      size_t buffer_size_dlen =
	BUFFER_SIZE_FROM_DLEN((size_t) _buffer_header.l_dlen);
      size_t data_size = buffer_size_dlen - sizeof(_buffer_header);

      // printf ("skipping %zd\n", data_size);

      // Map the data to skip it
      _input.map_range(data_size,_chunks);

      // And then try to read the next buffer!
      goto read_buffer_again;
    }

  if (_conf._print_buffer)
    print_buffer_header(&_buffer_header);

  if (!_buffer_header.l_dlen)
    ERROR("Buffer size l_dlen header zero.  Cannot be.");

  if (/*_buffer_header.l_dlen < 0 || */ /* unsigned, no check needed */
      _buffer_header.l_dlen > 0x20000000)
    ERROR("Buffer size l_dlen (0x%08x) header "
	  "negative or very large, refusing.",
	  _buffer_header.l_dlen);

  size_t buffer_size_dlen =
    BUFFER_SIZE_FROM_DLEN((size_t) _buffer_header.l_dlen);

  // buffer length should be multiple of 1024 bytes
  if (!buffer_size_dlen ||
      buffer_size_dlen % 1024)
    {
      // The MBS eventapi creates broken file headers, with l_dlen
      // of original size/2, without taking itself out of account

      if (_buffer_header.i_type    == LMD_FILE_HEADER_2000_1_TYPE &&
          _buffer_header.i_subtype == LMD_FILE_HEADER_2000_1_SUBTYPE)
	{
	  size_t buffer_size_dlen_broken =
	    (size_t) BUFFER_SIZE_FROM_DLEN_BROKEN(_buffer_header.l_dlen);

	  if ((buffer_size_dlen_broken &&
	       (buffer_size_dlen_broken % 1024) == 0))
            {
	      buffer_size_dlen = buffer_size_dlen_broken;
	      goto recovered_buffer_size_dlen;
            }
	}

      ERROR("l_dlen (%d, raw: 0x%08x) of buffer header does not result "
            "in blocksize (%d, swapped: %d) %% 1024 bytes! (Wrong endianess?)",
            _buffer_header.l_dlen,
            _buffer_header.l_dlen,
            (int) buffer_size_dlen,
            (int) BUFFER_SIZE_FROM_DLEN(bswap_32(_buffer_header.l_dlen)));

     recovered_buffer_size_dlen:
      ;
    }

  // Check that buffer header size is not too large for input pipe,

  if (buffer_size_dlen > _input._input->max_item_length())
    {
      ERROR("LMD buffer size (%zd=0x%08zx) too large for "
	    "input buffer (%zd=0x%08zx)/3.  "
	    "Use at least --input-buffer=%zdMi.",
	    buffer_size_dlen, buffer_size_dlen,
	    _input._input->buffer_size(),
	    _input._input->buffer_size(),
	    (buffer_size_dlen * 3 + (1024*1024-1))/(1024*1024));
    }

  size_t last_ev_size =
    (size_t) EVENT_DATA_LENGTH_FROM_DLEN(_buffer_header.l_free[1]);
  if (last_ev_size > _input._input->max_item_length())
    {
      /* Add some margin for buffer sizes. */
      size_t buffers = last_ev_size / buffer_size_dlen;
      size_t margin = buffers*64;
      ERROR("Last event size (%zd=0x%08zx) too large for for input buffer.  "
	    "Use at least --input-buffer=%zdMi.",
	    last_ev_size, last_ev_size,
	    ((last_ev_size + margin) * 3 + (1024*1024-1))/(1024*1024));
    }

  // so now we should check if it is a file header
  // or simply a buffer header

  size_t header_size = sizeof (s_bufhe_host);

  size_t file_header_size = 0;
  ssize_t file_header_used_size = -1;

  if (_buffer_header.i_type    == LMD_FILE_HEADER_2000_1_TYPE &&
      _buffer_header.i_subtype == LMD_FILE_HEADER_2000_1_SUBTYPE)
    {
      if (!_expect_file_header)
	WARNING("Unexpected file header.");

      file_header_size = sizeof(_file_header);

      //printf ("file header sz: %zd = 0x%zx\n",
      //      file_header_size,file_header_size);

      // For large buffers, the LMD file format lies about the buffer
      // size in file headers.  Instead it is stored in the i_used
      // member.

      // (The buffer is also unaligned :-( - great work guys!)  (Well,
      // unaligned buffers does not really matter for ucesb, as we do
      // input in much larger chunks anyhow.  But likely every other
      // unpacker system issues a read() per buffer, which then is
      // unaligned.  Ha!  In your face!)

      // Actaully - this could be seen as a precedent for making all
      // buffers just their used size (and unaligned).  We then stop
      // wasting space all over the place.  And would also be rid of
      // the fragmented events...

      if (_buffer_header.l_dlen > LMD_BUF_HEADER_MAX_IUSED_DLEN)
	{
	  // To complicate matters further, ucesb has been writing
	  // buffers with correct length in the header dlen for file
	  // headers.  I.e. file header buffer being of that length.
	  // How to know which to trust?  (I.e. is the length given in
	  // the header correct?)

	  if (_buffer_header.l_evt != 0)
	    {
	      // Old-style ucesb, that even wrote events in the file
	      // header buffer too.  Length is correct.
	    }
	  else if (_buffer_header.i_used == 0)
	    {
	      // Semi-old ucesb stopped writing events in the file
	      // header buffer, but did not lie about the length.
	      // It did set i_used as zero.
	    }
	  else
	    {
	      // Lying (normal) format.

	      file_header_size =
		BUFFER_USED_FROM_IUSED((uint) (ushort)
				       _buffer_header.i_used);

	      buffer_size_dlen = header_size + file_header_size;
	    }
	}

      // file header

      // s_filhe_extra_host file_header_extra;

      if (!_input.read_range(&_file_header,file_header_size))
	ERROR("Failed to read file header (extras after buffer header).");

      // the first part (buffer header) has already been swapped
      //if (IS_SWAPPING(_buffer_status))
      if (_swapping)
        byteswap_32(_file_header);

      _file_header_seen = true;

      if (_conf._print_buffer)
	print_file_header(&_file_header);

      // We should in principle check the members of the header, but
      // that would anyway be constrained to check that the string
      // length objects are within the lengths of the string arrays.  As
      // we don't use the strings, we do not check...

      file_header_used_size =
	sizeof(_file_header) -
	sizeof(_file_header.s_strings) +
	(min(30,_file_header.filhe_lines)) *
	sizeof(_file_header.s_strings[0]);

      _expect_file_header = false; // we'll accept buffer headers from now on
    }
  else if ((_buffer_header.i_type    == LMD_BUF_HEADER_10_1_TYPE &&
	    _buffer_header.i_subtype == LMD_BUF_HEADER_10_1_SUBTYPE) ||
	   (_buffer_header.i_type    == LMD_BUF_HEADER_HAS_STICKY_TYPE &&
	    _buffer_header.i_subtype == LMD_BUF_HEADER_HAS_STICKY_SUBTYPE))
    {
      // buffer header
      if (_buffer_header.i_type    == LMD_BUF_HEADER_HAS_STICKY_TYPE &&
	  _buffer_header.i_subtype == LMD_BUF_HEADER_HAS_STICKY_SUBTYPE)
	_first_buf_status = LMD_EVENT_FIRST_BUFFER_HAS_STICKY;

      if (_expect_file_header)
        {
          WARNING("Expected file header but got buffer header.");
          _expect_file_header = false;
        }
    }
  else
    {
      ERROR("Buffer header neither buffer header nor file header. "
            "Type/subtype error. (%d=0x%04x/%d=0x%04x)",
            (uint16_t) _buffer_header.i_type,
	    (uint16_t) _buffer_header.i_type,
            (uint16_t) _buffer_header.i_subtype,
	    (uint16_t) _buffer_header.i_subtype);
    }

  size_t buf_used;

  if (_buffer_header.l_dlen <= LMD_BUF_HEADER_MAX_IUSED_DLEN)
    {
      // old style, used in i_used, l_free[2] either 0 or same

      buf_used = BUFFER_USED_FROM_IUSED((uint) (ushort) _buffer_header.i_used);

      if (_buffer_header.l_free[2] != 0 &&
	  _buffer_header.l_free[2] != _buffer_header.i_used)
	ERROR("Used buffer space double defined differently (small buffer) "
	      "(i_used:%d, l_free[2]:%d)",
	      BUFFER_USED_FROM_IUSED((uint) (ushort) _buffer_header.i_used),
	      BUFFER_USED_FROM_IUSED(_buffer_header.l_free[2]));
    }
  else
    {
      // new style, used only in l_free[2], i_used 0

      buf_used = (size_t) BUFFER_USED_FROM_IUSED(_buffer_header.l_free[2]);

      // See comment below.  Ignore when newer MBS has written the
      // used part of the file header in i_used, but not l_free[2].

      if (_buffer_header.i_used != 0 &&
	  !(_buffer_header.i_type    == LMD_FILE_HEADER_2000_1_TYPE &&
	    _buffer_header.i_subtype == LMD_FILE_HEADER_2000_1_SUBTYPE &&
	    (ssize_t) BUFFER_USED_FROM_IUSED((uint) (ushort)
					     _buffer_header.i_used) ==
	    file_header_used_size))
	ERROR("Used buffer space double defined differently (large buffer) "
	      "(i_used:%d, l_free[2]:%d, filehe_eff_used:%zd)",
	      BUFFER_USED_FROM_IUSED((uint) (ushort) _buffer_header.i_used),
	      BUFFER_USED_FROM_IUSED(_buffer_header.l_free[2]),
	      file_header_used_size);
    }
  /*
  INFO(0,"(%zd+%zd=%zd) (%zd)",
       header_size,
       buf_used,
       header_size + buf_used,
       buffer_size_dlen);
  */

  if ((_buffer_header.i_type    == LMD_FILE_HEADER_2000_1_TYPE &&
       _buffer_header.i_subtype == LMD_FILE_HEADER_2000_1_SUBTYPE))
    {
      if (buf_used)
	{
	  // Newer MBS (since 5.1) writes i_used as the used size of the
	  // file header.  And then as the size of the fixed part plus the
	  // number of comment strings.

	  if ((ssize_t) buf_used == file_header_used_size)
	    buf_used = 0;
	  else if (_buffer_header.l_evt == 0)
	    WARNING("Header (%d/%d) with no events has used size (%d).",
		    _buffer_header.i_type,
		    _buffer_header.i_subtype,
		    (int) buf_used);
	}

      // Header size is as much as we have read so far.

      header_size += file_header_size;
    }

  if (header_size + buf_used >
      buffer_size_dlen)
    ERROR("Header says that more bytes (%zd+%zd=%zd) "
          "than length of buffer (%zd) are used.",
          header_size,
          buf_used,
          header_size + buf_used,
          buffer_size_dlen);

  // Check buffer number continuity

  if (_last_buffer_no + 1 != _buffer_header.l_buf &&
      _last_buffer_no)
    {
      /*
      INFO(0,"buf_header.l_dlen      = %08x",_buffer_header.l_dlen         );
      INFO(0,"buf_header.i_subtype   =     %04x",(ushort) _buffer_header.i_subtype );
      INFO(0,"buf_header.i_type      =     %04x",(ushort) _buffer_header.i_type    );
      INFO(0,"buf_header.h_begin     =       %02x",(unsigned char) _buffer_header.h_begin  );
      INFO(0,"buf_header.h_end       =       %02x",(unsigned char) _buffer_header.h_end    );
      INFO(0,"buf_header.i_used      =     %04x",(ushort) _buffer_header.i_used    );
      INFO(0,"buf_header.l_buf       = %08x",_buffer_header.l_buf          );
      INFO(0,"buf_header.l_evt       = %08x",_buffer_header.l_evt          );
      INFO(0,"buf_header.l_current_i = %08x",_buffer_header.l_current_i   );
      INFO(0,"buf_header.l_time[0]   = %08x",_buffer_header.l_time[0]   );
      INFO(0,"buf_header.l_time[1]   = %08x",_buffer_header.l_time[1]   );
      INFO(0,"buf_header.l_free[0]   = %08x",_buffer_header.l_free[0]   );
      INFO(0,"buf_header.l_free[1]   = %08x",_buffer_header.l_free[1]   );
      INFO(0,"buf_header.l_free[2]   = %08x",_buffer_header.l_free[2]   );
      INFO(0,"buf_header.l_free[3]   = %08x",_buffer_header.l_free[3]   );
      INFO(0,"=============================");
      */
      if (_buffer_header.l_buf <= _last_buffer_no ||
	  expect_fragment) // when expecting fragments, it must be exact!
	{
	  ERROR("Buffer numbers not increasing in steps of 1 (old=%d,new=%d).",
		_last_buffer_no,
		_buffer_header.l_buf);
	}
      else if (!_buffers_maybe_missing)
        WARNING("Buffer numbers not increasing in steps of 1 (old=%d,new=%d).",
                _last_buffer_no,
                _buffer_header.l_buf);
    }

  // Then, finally map the data We map the entire buffer, even if part
  // of it is not used (as according to how much is used in the
  // buffer), after we'll cut away at the end the used parts from the
  // chunks

  _chunk_cur = _chunk_end = _chunks;

  size_t data_size = buffer_size_dlen - header_size;

  if (((ssize_t) data_size) < 0)
    ERROR("Buffer has (%zd) < 0 bytes for data after header"
	  "(buffer is %zd, header uses %zd).",
	  data_size,
	  buffer_size_dlen, header_size);

  int chunks = 0;

  chunks = _input.map_range(data_size,_chunks);
  /*
  printf ("Buffer: len: %d, used: %d  (%d/%d)  (data_size:%d) events: %d, \n",
	  (int) buffer_size_dlen,
	  (int) buf_used,
	  _buffer_header.i_used,
	  _buffer_header.l_free[2],
	  (int) data_size,
	  _buffer_header.l_evt);
  */
  if (UNLIKELY(!chunks))
    {
      ERROR("Error while reading record data.");
      return false;
    }

  _chunk_end += chunks;

  // Now, cut away the data that is not used

  size_t unused = buffer_size_dlen - (header_size + buf_used);

  if (UNLIKELY(unused == data_size))
    {
      // Buffer is completely empty.  We must handle this with special
      // case here, since the normal checks in get_event does not fit.

      // Also the handling below if not complete

      if (_buffer_header.h_end ||
	  _buffer_header.h_begin ||
	  _buffer_header.l_evt)
	ERROR("Empty buffer claims to have fragments or events.");
      _events_left = 0;
      _chunk_cur = _chunk_end;
      return true;
    }

  if (UNLIKELY(_buffer_header.l_evt == 0))
    {
      if (data_size)
	ERROR("Non-empty buffer has no events.");
    }

  // buffer seems more or less sane...

  _last_buffer_no = _buffer_header.l_buf;

  assert(unused < data_size);

  if (UNLIKELY(unused >= _chunk_end[-1]._length))
    {
      assert(chunks > 1); // or we have an internal error

      // we cut the second chunk away entirely

      unused -= _chunk_end[-1]._length;

      _chunk_end--;
    }
  _chunk_end[-1]._length -= unused;

  _events_left = _buffer_header.l_evt;

  // Find out where the next non-spanned event starts (in case we were
  // skipping a fragment)

  // check fragment continuity

  if ((_buffer_header.h_end == 1) != expect_fragment)
    {
      if (expect_fragment)
	{
	  // We want a fragment, but none is there.  This is a major
	  // incident, but is handled in getevent.  (it must abort the
	  // event that it was fetching as fragments, but can then
	  // restart fetching the first event from this buffer)

	  // TODO: put a marker that skip_record should actually allow
	  // this record to be processed for events again...
	  // (it does not help to put the error in get_event, it would have
	  // same effect)
	  _skip_record_try_again = true;
	  ERROR("Expected fragment at beginning of new record.");
	}
      else
	{
	  WARNING("Buffer header had unexpected fragment at start.");

	  // TODO:

	  // We must take the appropriate action to seek past the unused
	  // end of an event... (the rest of the events we namely want)

	  // This is necessary, since otherwise it may take ages to
	  // syncronize if we get an file with almost exclusively
	  // fragmented events

	  // There will be an event header at the start of the buffer...

	  lmd_event_header_host frag_header;

	  if (!get_range((char *) &frag_header,
			 _chunk_cur,_chunk_end,
			 sizeof(lmd_event_header_host)))
	    {
	      ERROR("Failure reading fragment event header.");
	    }

	  if (_swapping)
	    byteswap ((uint32*) &frag_header,sizeof(lmd_event_header_host));

	  size_t frag_size =
	    (size_t) EVENT_DATA_LENGTH_FROM_DLEN(frag_header.l_dlen);

	  if (frag_size > buf_used)
	    ERROR("Fragment larger than buffer (%d>%d).",
		  (int)frag_size,(int)buf_used);

	  if (!skip_range(_chunk_cur,_chunk_end,frag_size))
	    ERROR("Internal error.");  // should have been caught above

	  _events_left--; // we've just eaten a fragment (yummy!)
	}
    }
  /*
  printf("buf_header.l_dlen      = %08x\n",_buffer_header.l_dlen         );
  printf("buf_header.i_subtype   =     %04x\n",(ushort) _buffer_header.i_subtype );
  printf("buf_header.i_type      =     %04x\n",(ushort) _buffer_header.i_type    );
  printf("buf_header.h_begin     =       %02x\n",(unsigned char) _buffer_header.h_begin  );
  printf("buf_header.h_end       =       %02x\n",(unsigned char) _buffer_header.h_end    );
  printf("buf_header.i_used      =     %04x\n",(ushort) _buffer_header.i_used    );
  printf("buf_header.l_buf       = %08x\n",_buffer_header.l_buf          );
  printf("buf_header.l_evt       = %08x\n",_buffer_header.l_evt          );
  printf("buf_header.l_current_i = %08x\n",_buffer_header.l_current_i   );
  printf("buf_header.l_time[0]   = %08x\n",_buffer_header.l_time[0]   );
  printf("buf_header.l_time[1]   = %08x\n",_buffer_header.l_time[1]   );
  printf("buf_header.l_free[0]   = %08x\n",_buffer_header.l_free[0]   );
  printf("buf_header.l_free[1]   = %08x\n",_buffer_header.l_free[1]   );
  printf("buf_header.l_free[2]   = %08x\n",_buffer_header.l_free[2]   );
  printf("buf_header.l_free[3]   = %08x\n",_buffer_header.l_free[3]   );
  printf("=============================\n");
  */

  return true;
}




bool lmd_source::skip_record()
{
  if (_skip_record_try_again)
    {
      // We should try to read events from the current record once
      // more.  (the failure was 'intermittent')
      _skip_record_try_again = false;
      return true;
    }



  return false;
}




void lmd_source::release_events()
{
  if (_prev_record_release_to2)
    {
      //printf ("** RELEASE: %zd\n",_prev_record_release_to2);
      _input.release_to(_prev_record_release_to2);
      _prev_record_release_to2 = 0;
    }
}

lmd_event *lmd_source::get_event()
{
  // We'll need to get records until we find an good event

  lmd_event *dest;

#ifdef USE_THREADING
  // Even if we blow up with an error, the reclaim item will be in the
  // reclaim list, so the memory won't leak.
  // We get the chunk array in the same piece of memory...
  int n = 4;
  // TODO: For optimal performance, we should not allocate 4 chunks, but rather
  // a value remembered to be rather good...  So that we not for every
  // fragmented event have to do the costly reallocate-copy below
  // Similar style as the subevent locator.
  dest = (lmd_event *)
    _wt._defrag_buffer->allocate_reclaim(sizeof (lmd_event) +
					 n * sizeof (buf_chunk));
  dest->_chunks_ptr = (buf_chunk *) (dest + 1);
  dest->_chunk_alloc = dest->_chunks_ptr + n;
#else
  _file_event.release();
  dest = &_file_event;
#endif
  /*
  fprintf (stderr,"%16p %16p %16p %16p\n",
	   dest->_chunks_ptr,
	   dest->_chunk_cur,
	   dest->_chunk_end,
	   dest->_chunk_alloc);
  */
  dest->_status = 0;
  dest->_subevents = NULL;

  // we are to get a new event.  So we may release up till the
  // previous record.  And staggered by one more event, due to event
  // stitching.  (We do not know that we can release previous bunch
  // until we see a new event that cannot be combined, and we have
  // written it all...)

  // This check has to be in get_event, cannot be in read_record,
  // since we may have a continous stretch of fragmented buffers,
  // which would then always when requesting a new record call
  // with expect_fragment

  _prev_record_release_to2 = _prev_record_release_to;

  //printf ("** GETEVT: prev2 %zd\n",_prev_record_release_to2);

  for ( ; ; )
    {
      //printf ("get record?...\n");
      if (_chunk_cur == _chunk_end)
	{
	  if (_events_left)
	    WARNING("Too few events were found in record (%u lost).",
		    _events_left);

	  //printf ("get record...\n");
	  if (!read_record())
	    {
	      if (_close_is_error)
		ERROR("Out of data considered an error.");
	      return NULL;
	    }
	  continue; // We read a record, just make sure we're not
		    // immediately at it's end
	}
      /*
      printf ("ge: %x (%d)\n",
	      _chunk_cur->_length,
	      _chunk_end - _chunk_cur);
      fflush(stdout);
      */
      // Ok, so we have found a chunk which promises to have at least
      // some data (left)

      // The header should always fit in the first buffer

      lmd_event_10_1_host *event_header = &dest->_header;

      if (!get_range((char *) &event_header->_header,
		     _chunk_cur,_chunk_end,
		     sizeof(lmd_event_header_host)))
	{
	  //printf ("eh fail\n");
	  //fflush(stdout);

	  ERROR("Failure reading event header.");
	}

      // swapping, etc

      if (_swapping)
	byteswap ((uint32*) &event_header->_header,
		  sizeof(lmd_event_header_host));
      /*
      printf ("header %08x %04x %04x .. \n",
	      event_header->_header.l_dlen,
	      event_header->_header.i_type,
	      event_header->_header.i_subtype);
      */
      // Hmm, the only way to now that we are fragmented is the
      // fragment bit in the record header.  And for that to apply to
      // us, we must be the event at the end, and we should also be
      // the n'th event.  So first check that if we are at the end, we
      // are the n'th event.  If this is a failure, then the fragment
      // info is most likely also crap, so do not dare to continue.

      // Hmm, if (at end) != (being n'th), but fragment at end = 0,
      // then there is nothing worse with this event than all the
      // other ones, in the buffer.  Accept this, one (but we'll go
      // haywire for the next event, most likely)

      // How long is our event, do we reach the end?  Hmm, actually
      // the end reaching discussions are left for the getting of data
      // from the chunks, so we'll rely on the event counter, and
      // catch the mess later...  :-(

      _events_left--;

      size_t event_size =
	(size_t) EVENT_DATA_LENGTH_FROM_DLEN(event_header->_header.l_dlen);
      dest->_swapping = _swapping;
      dest->_status |= _first_buf_status;

      // Now, we'd like to get the data, but it may be larger than the
      // current chunk, in case the event is fragmented...

      //printf ("%d (%d)\n",_events_left,_buffer_header.h_begin);

      if (LIKELY(_events_left > 0) ||
	  !_buffer_header.h_begin)
	{
	  // This event is not fragmented

	  // Cannot be more than two chunks
	  assert (_chunk_end - _chunk_cur <= 2);
	  // And we shall have the space, dest->_chunk_end will be set
	  // from dest->_chunks_ptr in get_range (more importantly: and
	  // dest->_chunk_end is not valid yet)
	  assert (dest->_chunk_alloc - dest->_chunks_ptr >= 2);

	  if (!get_range(dest->_chunks_ptr,&dest->_chunk_end,
			 _chunk_cur,_chunk_end,
			 event_size))
	    {
	      //printf ("ged: fail  %d\n",event_size);
	      //fflush(stdout);

	      ERROR("Event larger (%d) than record.",(int) event_size);
	    }
	  /*
	  printf ("got event %d bytes\n",
		  event_size);
	  fflush(stdout);
	  */

	  //for (buf_chunk *p = dest->_chunks; p < dest->_chunk_end; p++)
	  //  INFO(0,"got(1): chunk(%8p) (%8p,%d)",p,p->_ptr,p->_length);

	  return dest;
	}

      //printf ("get fragemented...\n");

      // Handle long case, fragmented event...

      // Since fragmented events are so fragile, we'll take caution
      // that they make perfect sense...

      // First of all we may have ended here because we have read to
      // many events, but not reached the end of the buffer yet!

      if (!_buffer_header.h_begin)
	ERROR("Too many events were found in buffer.");

      assert(_events_left == 0);
      // Ok, so we're marked fragment, and are the last event.  We
      // should begin simply copying the chunks, since they should
      // mark exactly the data that we want.  Must also check that this
      // then ends nicely at the buffer end.

      // The total event size is however given by l_free[0]

      // make sure our header is correct
      event_header->_header.l_dlen = _buffer_header.l_free[1];

      size_t total_size =
	(size_t) EVENT_DATA_LENGTH_FROM_DLEN(_buffer_header.l_free[1]);

      dest->_chunk_end = dest->_chunks_ptr;

      for ( ; ; )
	{
	  if (event_size > total_size)
	    ERROR("Fragment larger (%d) than total remaining event size (%d).",
		  (int) event_size,(int) total_size);

	  // We must allocate space to hold our chunks.  We also make
	  // sure there is enough space to hold two more chunks, as
	  // this is the maximum that may be needed by the final
	  // fragment (that is retrieved outside the loop)

	  if (dest->_chunk_alloc < dest->_chunk_end + 4)
	    {
	      size_t n = 2 * (size_t) (dest->_chunk_alloc - dest->_chunks_ptr);
	      size_t e = (size_t) (dest->_chunk_end - dest->_chunks_ptr);
#ifdef USE_THREADING
	      dest->_chunks_ptr = (buf_chunk *)
		_wt._defrag_buffer->allocate_reclaim_copy(n * sizeof (buf_chunk),
							  dest->_chunks_ptr,
							  dest->_chunk_end);
#else
	      _file_event._chunks_ptr =
		(buf_chunk *) realloc(_file_event._chunks_ptr,
				      n * sizeof (buf_chunk));
	      if (!_file_event._chunks_ptr)
		ERROR("Memory allocation failure!");
#endif
	      dest->_chunk_alloc = dest->_chunks_ptr + n;
	      dest->_chunk_end = dest->_chunks_ptr + e;
	    }

	  size_t retrieved = copy_range(dest->_chunk_end,
					_chunk_cur,_chunk_end);

	  if (UNLIKELY(retrieved != event_size))
	    {
	      ERROR("Fragmented event (%d) %s than record (%d).",
		    (int) event_size,
		    (retrieved < event_size) ? "larger" : "smaller",
		    (int) retrieved);
	    }

	  // So we've gotten some part of the event, reduce

	  total_size -= event_size;

	  // We must get the next record...  Hmm, actually, we can now
	  // be missing exactly 0 bytes, and be at the end of file,
	  // but if the producer is brain damaged enough to mark such
	  // an event as fragmented, we pay back by giving an error :-)

	  if (!read_record(true))
	    ERROR("Could not get next record "
		  "for fragmented event (missing %d).",
		  (int) total_size);

	  if (_swapping != dest->_swapping)
	    // bad boy (hot-swap event builder !?!)
	    ERROR("Swapping changed within fragmented event.");

	  // Aha, so let's try to get the fragment header...

	  lmd_event_header_host frag_header;

	  if (!get_range((char *) &frag_header,
			 _chunk_cur,_chunk_end,
			 sizeof(lmd_event_header_host)))
	    {
	      ERROR("Failure reading fragment event header.");
	    }

	  if (_swapping)
	    byteswap ((uint32*) &frag_header,sizeof(lmd_event_header_host));

	  if (frag_header.i_type    != event_header->_header.i_type ||
	      frag_header.i_subtype != event_header->_header.i_subtype)
	    ERROR("Fragment event type/subtype mismatch from original.");

	  event_size = (size_t) EVENT_DATA_LENGTH_FROM_DLEN(frag_header.l_dlen);

	  if (event_size == total_size)
	    break; // this is final fragment

	  // We'll check for being to large later

	  if (!_buffer_header.h_begin)
	    {
	      if (UNLIKELY(event_size > total_size))
		ERROR("Final fragment larger (%d) than "
		      "total remaining event size (%d).",
		      (int) event_size,(int) total_size);

	      break; // this is the last fragment, handle below...
	    }

	  if (_events_left != 1)
	    ERROR("Buffer with only partial fragment wrong event count.");
	}

      _events_left--;

      // Now, get the chunks for the last data!

      if (!get_range(dest->_chunk_end,&dest->_chunk_end,
		     _chunk_cur,_chunk_end,
		     event_size))
	ERROR("Final fragment larger than record.");

      // so, we managed to get our fragmented event.

      //for (buf_chunk *p = dest->_chunks; p < dest->_chunk_end; p++)
      //	INFO(0,"got(m): chunk(%8p) (%8p,%d)",p,p->_ptr,p->_length);

      return dest;
    }

  return NULL;
}


void lmd_source::print_buffer_header(const s_bufhe_host *header)
{
  char time_buf[64];
  time_t    time_utc;
  struct tm tm_utc;

  time_utc = header->l_time[0];

  gmtime_r(&time_utc,&tm_utc);
  strftime(time_buf,sizeof(time_buf),"%a %Y-%m-%d %H:%M:%S",&tm_utc);

  size_t size = BUFFER_SIZE_FROM_DLEN((size_t) header->l_dlen);

  size_t used;

  if (header->l_dlen <= LMD_BUF_HEADER_MAX_IUSED_DLEN)
    used = (size_t) header->i_used;
  else
    used = (size_t) header->l_free[2];

  used = BUFFER_USED_FROM_IUSED(used);

  if (header->i_type    == LMD_FILE_HEADER_2000_1_TYPE &&
      header->i_subtype == LMD_FILE_HEADER_2000_1_SUBTYPE)
    {
      if (header->l_dlen > LMD_BUF_HEADER_MAX_IUSED_DLEN)
	{
	  printf("File header claim size%s%8zd%s\n",
		 CT_OUT(BOLD),
		 size,
		 CT_OUT(NORM));

	  used = (size_t) header->i_used;

	  used = BUFFER_USED_FROM_IUSED(used);

	  size = used + sizeof (s_bufhe_host);
	}
    }

  printf("Buffer%s%11u%s Size%s%8zd%s Used%s%8zd%s "
	 "%s%s.%03d UTC%s\n",
	 CT_OUT(BOLD),(uint) header->l_buf,CT_OUT(NORM),
	 CT_OUT(BOLD),
	 size,
	 CT_OUT(NORM),
	 CT_OUT(BOLD),
	 used,
	 CT_OUT(NORM),
	 CT_OUT(BOLD),
	 time_buf,(uint) header->l_time[1],
	 CT_OUT(NORM));

  printf("      Events%s%6d%s Type/Subtype%s%5d%5d%s "
	 "FragEnd=%s%d%s FragBegin=%s%d%s LastSz%s%8d%s\n",
	 CT_OUT(BOLD),header->l_evt,CT_OUT(NORM),
	 CT_OUT(BOLD),
	 (uint16) header->i_type,
	 (uint16) header->i_subtype,
	 CT_OUT(NORM),
	 CT_OUT(BOLD),header->h_end,CT_OUT(NORM),
	 CT_OUT(BOLD),header->h_begin,CT_OUT(NORM),
	 CT_OUT(BOLD),
	 (int) EVENT_DATA_LENGTH_FROM_DLEN(header->l_free[1]),
	 CT_OUT(NORM));

  // not printed: l_current_i, l_free[0] l_free[2] l_free[3]
}

#define PRINT_STRING_L2(name,str,maxlen,len) {				\
    char tmp[maxlen+1];							\
    memcpy (tmp,str,maxlen);						\
    tmp[min(maxlen,len)] = 0;						\
    printf("  %-8s %s%s%s\n",name,CT_OUT(BOLD),tmp,CT_OUT(NORM_DEF_COL)); \
  }
#define PRINT_STRING_L(name,str,maxlen) \
  PRINT_STRING_L2(name,str,maxlen,(unsigned int) str##_l)
#define PRINT_STRING(name,str) \
  PRINT_STRING_L(name,str,sizeof(str))

void lmd_source::print_file_header(const s_filhe_extra_host *header)
{
  printf("File header:\n");
  PRINT_STRING("Label",   header->filhe_label);
  PRINT_STRING("File",    header->filhe_file);
  PRINT_STRING("User",    header->filhe_user);
  PRINT_STRING_L2("Time", header->filhe_time,24,24);
  PRINT_STRING("Run",     header->filhe_run);
  PRINT_STRING("Exp",     header->filhe_exp);
  for (uint i = 0; i < min(30,header->filhe_lines); i++)
    PRINT_STRING("Comment",header->s_strings[i].string);
}

/*
--------------------------------------------------------
File header info:
Size:    16360 [32768 b]
Label:   DISK
File:    /data3/li8_d_184.lmd
User:    user1
Time:    02-Dec-04 02:41:52
Run:
Exp:
--------------------------------------------------------
--------------------------------------------------------
Buffer     179713, Length 16360[w] Size 32768[b] 02-Dec-2004 02:41:51.77
       Events 376 Type/Subtype    10     1 FragEnd=0 FragBegin=1 Total    38[w]
--------------------------------------------------------

Event    65959990 Type/Subtype    10     1 Length    36[w] Trigger  2
  SubEv ID      1 Type/Subtype    10     1 Length    22[w] Control  9 Subcrate  0
  SubEv ID      1 Type/Subtype    10     1 Length     2[w] Control  9 Subcrate  1

Event    65959779 Type/Subtype    10     1 Length    42[w] Trigger  2
  SubEv ID      1 Type/Subtype    10     1 Length    28[w] Control  9 Subcrate  0
  fefe.fefe fd0c.0054 0004.e557 0800.0003 1000.0055 fa07.0100 f801.40cb fcee.d225
  fa0c.0100 f801.4ed6 fcee.e830 efef.efef fa0a.0100
  SubEv ID      1 Type/Subtype    10     1 Length     2[w] Control  9 Subcrate  1

*/


void lmd_event::print_event(int data,hex_dump_mark_buf *unpack_fail) const
{
  const char *ct_out_bold_ev;
  const char *ct_out_bold_sev;
  const char *evtypestr = "Event";

  if (!(_status & LMD_EVENT_IS_STICKY))
    {
      ct_out_bold_ev = CT_OUT(BOLD_BLUE);
      ct_out_bold_sev = CT_OUT(BOLD_MAGENTA);
    }
  else
    {
      evtypestr = "StEvent";
      ct_out_bold_ev = CT_OUT(BOLD_GREEN);
      ct_out_bold_sev = CT_OUT(BOLD_CYAN);
    }

  if (!(_status & LMD_EVENT_HAS_10_1_INFO))
    {
      printf("%-7s         %s-%s Type/Subtype%s%5d%5d%s "
	     "Size%s%8d%s Trigger  %s-%s\n",
	     evtypestr,
	     ct_out_bold_ev,CT_OUT(NORM_DEF_COL),
	     ct_out_bold_ev,
	     (uint16) _header._header.i_type,
	     (uint16) _header._header.i_subtype,
	     CT_OUT(NORM_DEF_COL),
	     ct_out_bold_ev,
	     (int) EVENT_DATA_LENGTH_FROM_DLEN(_header._header.l_dlen),
	     CT_OUT(NORM_DEF_COL),
	     ct_out_bold_ev,CT_OUT(NORM_DEF_COL));
    }
  else
    {
      printf("%-7s%s%12u%s Type/Subtype%s%5d%5d%s "
	     "Size%s%9d%s Trigger %s%2d%s\n",
	     evtypestr,
	     ct_out_bold_ev,_header._info.l_count,CT_OUT(NORM_DEF_COL),
	     ct_out_bold_ev,
	     (uint16) _header._header.i_type,
	     (uint16) _header._header.i_subtype,
	     CT_OUT(NORM_DEF_COL),
	     ct_out_bold_ev,
	     (int) EVENT_DATA_LENGTH_FROM_DLEN(_header._header.l_dlen),
	     CT_OUT(NORM_DEF_COL),
	     ct_out_bold_ev,
	     (uint16) _header._info.i_trigger,
	     CT_OUT(NORM_DEF_COL));

      // Subevents

      for (int subevent = 0; subevent < _nsubevents; subevent++)
	{
	  lmd_subevent *subevent_info = &_subevents[subevent];

	  bool subevent_error = (unpack_fail &&
				 unpack_fail->_next == &subevent_info->_header);

	  char data_len[32];

	  if ((_status & LMD_EVENT_IS_STICKY) &&
	      subevent_info->_header._header.l_dlen ==
	      LMD_SUBEVENT_STICKY_DLEN_REVOKE)
	    strcpy(data_len,"revoke");
	  else
	    sprintf (data_len,"%d",
		     SUBEVENT_DATA_LENGTH_FROM_DLEN(subevent_info->_header._header.l_dlen));
	  
	  printf(" %sSubEv ProcID%s%s%6d%s Type/Subtype%s%5d%5d%s "
		 "Size%s%9s%s Ctrl%s%4d%s Subcrate%s%4d%s\n",
		 subevent_error ? CT_OUT(BOLD_RED) : "",
		 subevent_error ? CT_OUT(NORM_DEF_COL) : "",
		 ct_out_bold_sev,
		 (uint16) subevent_info->_header.i_procid,
		 CT_OUT(NORM_DEF_COL),
		 ct_out_bold_sev,
		 (uint16) subevent_info->_header._header.i_type,
		 (uint16) subevent_info->_header._header.i_subtype,
		 CT_OUT(NORM_DEF_COL),
		 ct_out_bold_sev,
		 data_len,
		 CT_OUT(NORM_DEF_COL),
		 ct_out_bold_sev,
		 (uint8) subevent_info->_header.h_control,
		 CT_OUT(NORM_DEF_COL),
		 ct_out_bold_sev,
		 (uint8) subevent_info->_header.h_subcrate,
		 CT_OUT(NORM_DEF_COL));

     // NH print WR timestamp if found :)
     if (subevent_info->_data)
     {
        uint32 data_length = (uint32)
 		     SUBEVENT_DATA_LENGTH_FROM_DLEN(subevent_info->_header._header.l_dlen);
         if (data_length >= 20)
         {
           uint32_t* wr_p = (uint32*)subevent_info->_data;
           uint32_t wrID = *wr_p++;
           uint32_t wrL = *wr_p++;
           uint32_t wrML = *wr_p++;
           uint32_t wrMH = *wr_p++;
           uint32_t wrH = *wr_p++;
           if ((wrL & 0xFFFF0000) == 0x03e10000)
           {
             uint64_t wr =  0;
             wr = wrL & 0xffff;
             wr |= (wrML & 0xffff) << 16;
             wr |= (uint64_t)(wrMH & 0xffff) << 32;
             wr |= (uint64_t)(wrH & 0xffff) << 48;

             printf("                           WR ID%s%6x%s     Time       %s%16" PRIx64 "%s\n",
             CT_OUT(CYAN),
             wrID,
             CT_OUT(NORM_DEF_COL),
             CT_OUT(CYAN),
             wr,
             CT_OUT(NORM_DEF_COL)
             );
           }
         }
     }

	  if (data)
	    {
	      hex_dump_buf buf;

	      if (subevent_info->_data)
		{
		  size_t data_length;

		  if ((_status & LMD_EVENT_IS_STICKY) &&
		      subevent_info->_header._header.l_dlen ==
		      LMD_SUBEVENT_STICKY_DLEN_REVOKE)
		    data_length = 0;
		  else
		    data_length =
		      SUBEVENT_DATA_LENGTH_FROM_DLEN(subevent_info->_header._header.l_dlen);

		  if ((data_length & 3) == 0) // length is divisible by 4
		    hex_dump(stdout,
			     (uint32*) subevent_info->_data,
			     subevent_info->_data + data_length,_swapping,
			     "%c%08x",9,buf,unpack_fail);
		  else
		    hex_dump(stdout,
			     (uint16*) subevent_info->_data,
			     subevent_info->_data + data_length,_swapping,
			     "%c%04x",5,buf,unpack_fail);
		}
	    }
	}

      // Is there any remaining data, that could not be assigned as a subevent?

      print_remaining_event(_chunk_cur,_chunk_end,_offset_cur,_swapping);
    }
}


void lmd_event::get_10_1_info()
{
  // This is in a special function, such that one can retrieve the
  // trigger and event counter without locating all the subevents

  // Subevents are only known how to handle under format type/subtype
  // 10/1

  if (_status & LMD_EVENT_GET_10_1_INFO_ATTEMPT)
    return;

  _status |= LMD_EVENT_GET_10_1_INFO_ATTEMPT;

  if (_header._header.i_type    == LMD_EVENT_10_1_TYPE &&
      _header._header.i_subtype == LMD_EVENT_10_1_SUBTYPE)
    ;
  else if (_header._header.i_type    == LMD_EVENT_STICKY_TYPE &&
	   _header._header.i_subtype == LMD_EVENT_STICKY_SUBTYPE)
    {
      if (!(_status & LMD_EVENT_FIRST_BUFFER_HAS_STICKY))
	WARNING("Sticky event starts in buffer "
		"not marked to contain sticky events.");
      _status |= LMD_EVENT_IS_STICKY;
    }
  else
    {
      //printf ("type fail\n");
      ERROR("Event header type/subtype unsupported: (%d=0x%04x/%d=0x%04x).",
            (uint16_t) _header._header.i_type,
	    (uint16_t) _header._header.i_type,
            (uint16_t) _header._header.i_subtype,
	    (uint16_t) _header._header.i_subtype);
    }

  // Get the header info.  (this will eat some of the chunk info, but...

  _chunk_cur = _chunks_ptr;
  _offset_cur = 0;

  if (!get_range_many((char *) &_header._info,
		      _chunk_cur,_offset_cur,_chunk_end,
		      sizeof(lmd_event_info_host)))
    {
      //printf ("gi fail\n");
      // We're simply out of data.
      ERROR("Event header (second part) not complete before end of event.");
    }
  if (_swapping)
    byteswap ((uint32*) &_header._info,sizeof(lmd_event_info_host));

  _status |= LMD_EVENT_HAS_10_1_INFO;

  /*
  printf ("info:    %04x %04x %08x .. \n",
	  _header._info.i_trigger,
	  _header._info.i_dummy,
	  _header._info.l_count);
  */
}

void lmd_event::locate_subevents(lmd_event_hint *hints)
{
  if (_status & LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT)
    return;

  _status |= LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;

  assert(!_subevents); // we have already been called!

  // get some space, for at least so many subevents as we have seen at
  // most before...

#ifdef USE_THREADING
  // If some crazy event had a crazy number of subevents, we do not
  // want to allocate that amount for all subsequent events.  Decrease
  // after a number of events has not required reallocation.  (Meaning
  // that we will possibly will hit the limit again once in a while
  // and be forced to make another allocation).
  hints->_hist_req_realloc <<= 1; // shift history
  if (UNLIKELY(hints->_hist_req_realloc == 0))
    {
      hints->_max_subevents /= 2;
      // Since we would otherwise work in factors of two, and flip
      // above and below, we try to keep a better track as well
      // (max number of subevents seen since last decrease).
      // And add 2 just for having a bit of extra margin.
      // With the <= comparison and add at least 1, we never become < 1!
      if (hints->_max_subevents <= hints->_max_subevents_since_decrease)
	hints->_max_subevents = hints->_max_subevents_since_decrease + 2;
      hints->_max_subevents_since_decrease = 0;
      // if (hints->_max_subevents < 1)
      //   hints->_max_subevents = 1;
    }

  _subevents = (lmd_subevent *)
    _wt._defrag_buffer->
    allocate_reclaim(hints->_max_subevents * sizeof (lmd_subevent));
#else
  // With the single buffer always kept for this purpose, no reason
  // to try to adjust.

  _subevents = (lmd_subevent *)
    _defrag_event.allocate((size_t) hints->_max_subevents *
			   sizeof (lmd_subevent));
#endif

  _nsubevents = 0;

  // Then, let's chew over the event...

  buf_chunk *chunk_cur = _chunk_cur;
  size_t     offset_cur = _offset_cur;

  //void *start = fragment->_ptr;
  //size_t left;

  while (chunk_cur < _chunk_end)
    {
      // First of all, do we have space to dump this subevent?
      /*
      printf ("%x / %x (%d)\n",
	      offset_cur,chunk_cur->_length,
	      _chunk_end - chunk_cur);
      fflush(stdout);
      */

      if (_nsubevents >= hints->_max_subevents)
	{
	  // We need to expand the space available in our information
	  // buffer.

	  hints->_max_subevents *= 2;
	  hints->_hist_req_realloc |= 1; // had to realloc

#ifdef USE_THREADING
	  _subevents = (lmd_subevent *)
	    _wt._defrag_buffer->
	    allocate_reclaim_copy(hints->_max_subevents *
				  sizeof (lmd_subevent),
				  _subevents,
				  _nsubevents * sizeof (lmd_subevent));
#else
	  // despite the name, actually does reallocation
	  _subevents = (lmd_subevent *)
	    _defrag_event.allocate((size_t) hints->_max_subevents *
				   sizeof (lmd_subevent));
#endif
	}

      // We have more data to read.
      // Does this fragment have enough data for the subevent header?

      lmd_subevent           *subevent_info = &_subevents[_nsubevents];
      lmd_subevent_10_1_host *subevent_header = &subevent_info->_header;

      if (!get_range_many((char *) subevent_header,
			  chunk_cur,offset_cur,_chunk_end,
			  sizeof(lmd_subevent_10_1_host)))
	{
	  // We're simply out of data.
	  ERROR("Subevent header not complete before end of event.");
	}

      // Ok, we have gotten the subevent header.  Byteswap it (if needed)!

      if (_swapping)
	byteswap ((uint32*) subevent_header,sizeof(lmd_subevent_10_1_host));

      // So, subevent header is there, now remember where we got the
      // data

      size_t data_length;

      if ((_status & LMD_EVENT_IS_STICKY) &&
	  subevent_header->_header.l_dlen == LMD_SUBEVENT_STICKY_DLEN_REVOKE)
	data_length = 0;
      else
	data_length =
	  SUBEVENT_DATA_LENGTH_FROM_DLEN((size_t) subevent_header->_header.l_dlen);

      // Did the data come exclusively from this buffer?

      size_t chunk_left = chunk_cur < _chunk_end ?
	chunk_cur->_length - offset_cur : 0;

      if (LIKELY(data_length <= chunk_left))
	{
	  // likely, since most subevents smaller than buffer size

	  if (UNLIKELY(!data_length))
	    {
	      // if the data size is zero, and we're the last
	      // subevent, then chunk_cur will not be valid, and we
	      // should not give such a pointer away (i.e. we may not
	      // dereference chunk_cur, so we cannot give it away)

	      // But we want to give an invalid (but non-null) pointer
	      // (since the length is zero, noone should _ever_ look
	      // at it...

	      subevent_info->_data = (char*)16;
	    }
	  else
	    {
	      subevent_info->_data = chunk_cur->_ptr + offset_cur;
	    }

	  offset_cur += data_length;

	  /*
	  printf (":: %d %d (%p)\n",
		  data_length,chunk_left,
		  subevent_info->_data);
	  fflush(stdout);
	  */
      	}
      else
	{
	  // We have some fun, first of all, the data is not stored
	  // continous yet

	  subevent_info->_data   = NULL;

	  // This is where to get to the data

	  subevent_info->_frag   = chunk_cur;
	  subevent_info->_offset = offset_cur;

	  // Second of all, we must make sure that one cat get to
	  // the data at all!  (we must anyhow read past it to get to
	  // the next subevent in the buffer)

	  for ( ; ; )
	    {
	      // Each time we come into the loop, the current fragment
	      // has some data, but not enough

	      data_length -= chunk_left;

	      // So, go get next fragment

	      chunk_cur++;

	      if (chunk_cur >= _chunk_end)
		{
		  /*
		  for (buf_chunk *p = _chunks; p < _chunk_end; p++)
		    {
		      INFO(0,"chunk(%8p) (%8p,%d)",p,p->_ptr,p->_length);
		    }
		  INFO(0,"this: (%8p,off:%d)",
		       subevent_info->_frag,subevent_info->_offset);
		  INFO(0,"dlen-> %d",
		       SUBEVENT_DATA_LENGTH_FROM_DLEN(subevent_header->_header.l_dlen)
		       );
		  INFO(0,"data_length %d chunk_left %d",
		       data_length,chunk_left);
		  */
		  ERROR("Subevent (%d/%d) length %d bytes, "
			"not complete before end of event, %d bytes missing.",
			(uint16) subevent_info->_header._header.i_type,
			(uint16) subevent_info->_header._header.i_subtype,
			SUBEVENT_DATA_LENGTH_FROM_DLEN(subevent_info->_header._header.l_dlen),
			(int) data_length);
		}

	      chunk_left = chunk_cur->_length;

	      if (LIKELY(data_length <= chunk_left))
		break;
	    }

	  offset_cur = data_length;

	  // Account for how much data was used in ending fragment
	}

      // One more subevent fitted in the event...

      _nsubevents++;

      // Now, we might exactly have reached a fragment boundary
      // This we need not consider, since it would be handled at
      // the beginning of the loop as an fragmented subevent header!
      // But then, we'll at some times activate the branch for
      // fragmented headers.  Better let it be handled here

      if (chunk_left == data_length)
	{
	  chunk_cur++;
	  offset_cur = 0;
	}

      // Subevent was successfully parsed, note that...

      _chunk_cur = chunk_cur;
      _offset_cur = offset_cur;
    }

#ifdef USE_THREADING
  // Try to avoid the costly (reallocation) copy by keeping the limit.
  // If we are just on the limit, we will not become eligble for
  // decrease.  Count as being reallocated.
  if (UNLIKELY(_nsubevents >= hints->_max_subevents))
    {
      hints->_hist_req_realloc |= 1;
      hints->_max_subevents++; // go up by one, to not hit this check again
    }
  if (UNLIKELY(_nsubevents > hints->_max_subevents_since_decrease))
    hints->_max_subevents_since_decrease = _nsubevents;
#endif

  //printf ("===> %d\n",_nsubevents);
  //fflush(stdout);

  // Aha, so we have successfully gotten all subevent data
}





void lmd_event::get_subevent_data_src(lmd_subevent *subevent_info,
				      char *&start,char *&end)
{
  // Now, if we would be running on a Cell SPU, we would be
  // wanting to get the data from the main memory in any case,
  // with DMA, so we can handle the fragmented cases just as well
  // as the non-fragmented.  It is just a few more DMA operations.
  // So that one would use a special function, and not this one.
  // This one is for use by normal processors.

  if (is_subevent_sticky_revoke(subevent_info))
    {
      start = end = NULL;
      return;
    }

  // We have a few cases.  Either the data was not fragmented:

  size_t data_length = (size_t)
    SUBEVENT_DATA_LENGTH_FROM_DLEN(subevent_info->_header._header.l_dlen);

  if (UNLIKELY(subevent_info->_data == NULL))
    {
      if (1/*fragments_not_memory_ordered ||
	     fragments_read_only*/)
	{
	  // We need to copy the subevent to an defragment buffer.
	  // The trouble with this buffer is that it needs to be
	  // deallocated or marked free somehow also...

	  // Defragmentation buffers.  Need to survive until we have
	  // retired the event (retire = ALL operations done).  This
	  // is since we may be requested to do a data dump of an
	  // (broken) event (or a normal event dump).  However, since
	  // different processes might be responsible for allocating
	  // the defrag buffer (either the main file reading one (due
	  // to event printout), or the unpacking one (due to unpack)
	  // or the retirement one (due to debug printout), we need to
	  // have one defrag buffer pool per such process, since
	  // allocating should not need to wait for anyone else.  The
	  // pointer supplied will be stable for the rest of the event
	  // life-time, i.e. can be used by other threads on the same
	  // machine.

#ifdef USE_THREADING
	  char *defrag = (char *)
	    _wt._defrag_buffer->allocate_reclaim(data_length);
#else
	  char *defrag = (char *)
	    _defrag_event_many.allocate(data_length);
#endif

	  subevent_info->_data = defrag;

	  buf_chunk *frag = subevent_info->_frag;

	  size_t size0 = frag->_length - subevent_info->_offset;

	  // First copy data from first fragmented buffer

	  memcpy(defrag,
		 frag->_ptr + subevent_info->_offset,
		 size0);

	  defrag += size0;
	  size_t length = data_length - size0;

	  frag++;

	  // Then, copy all full fragments

	  size_t size = frag->_length;

	  while (UNLIKELY(size < length))
	    {
	      memcpy(defrag,
		     frag->_ptr,
		     size);

	      defrag += size;
	      length -= size;

	      frag++;
	      size = frag->_length;
	    }

	  // And copy last one

	  memcpy(defrag,frag->_ptr,length);
	  /*
	  for (uint i = 0; i < subevent_info->_length; i++)
	    printf ("%02x ",(uint8) subevent_info->_data[i]);
	  printf ("\n");
	  fflush(stdout);
	  */
	}
      else
	{
	  // or data was fragmented.  Now, if the fragments are in memory
	  // order, we also know that we may destroy whatever is inbetween
	  // them, so we need only copy the shortest portion of the subevent
	  // to make it adjacent to the rest.

	  // the use of memmove is slightly more expensive than using
	  // memcpy, but that is offset by the fact that we usually
	  // only need to copy < half of the subevent

	  buf_chunk *frag0 = subevent_info->_frag;

	  size_t size0  = frag0->_length - subevent_info->_offset;
	  size_t remain = data_length - size0;

	  buf_chunk *frag = frag0 + 1;

	  if (size0 < remain)
	    {
	      // The first fragment is smaller than the rest (and then,
	      // even if more than two fragements, most likely also
	      // smaller than fragment 1)

	      subevent_info->_data = frag->_ptr - size0;

	      memmove(subevent_info->_data,
		      frag0->_ptr + subevent_info->_offset,
		      size0);

	      // Next fragment need not be copied at all

	      remain -= frag->_length;
	      frag0   = frag;
	    }
	  else
	    {
	      subevent_info->_data = frag0->_ptr + subevent_info->_offset;
	    }

	  char *next = frag0->_ptr + frag0->_length;
	  frag = frag0 + 1;

	  size_t size = frag->_length;

	  while (UNLIKELY(size < remain))
	    {
	      memmove(next,
		      frag->_ptr,
		      size);

	      next   += size;
	      remain -= size;

	      frag++;
	    }

	  // And copy last one

	  memmove(next,frag->_ptr,remain);
	}
    }

  start = subevent_info->_data;
  end   = subevent_info->_data + data_length;

  return;
}
