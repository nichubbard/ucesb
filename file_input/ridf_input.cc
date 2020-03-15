/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Hans Toernqvist  <h.toernqvist@gsi.de>
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

/* Based mainly on the HLD code, with pointers from the others. */

#include "ridf_input.hh"
#include "config.hh"
#include "hex_dump.hh"

#define HEADER_REV(header)    ((0xc0000000 & (header)._code) >> 30)
#define HEADER_LAYER(header)  ((0x30000000 & (header)._code) >> 28)
#define HEADER_CLSID(header)  ((0x0fc00000 & (header)._code) >> 22)
#define HEADER_SIZE(header)  (((0x003fffff & (header)._code) >>  0) * 2)

#define ID_REV(id) ((0xfc000000 & (id)) >> 26)
#define ID_DEV(id) ((0x03f00000 & (id)) >> 20)
#define ID_FP(id)  ((0x000fc000 & (id)) >> 14)
#define ID_DET(id) ((0x00003f00 & (id)) >>  8)
#define ID_MOD(id) ((0x000000ff & (id)) >>  0)

ridf_source::ridf_source()
{
}

void ridf_source::new_file()
{
  _is_new_file = true;
}

#if !USE_THREADING && !USE_MERGING
FILE_INPUT_EVENT _file_event;
#endif

ridf_event *ridf_source::get_event()
{
  ridf_event *dest;

#ifdef USE_THREADING
  dest = (ridf_event *)_wt._defrag_buffer->allocate_reclaim(sizeof *dest);
#else
  _file_event.release();
  dest = &_file_event;
#endif

  dest->_status = 0;
  dest->_subevents = NULL;

  _input.release_to(_input._cur);

  /* Loop over blocks until we find an event block. */
  for (;;) {
    ridf_header *block_header = &dest->_header._header;
    if (!_input.read_range(block_header, sizeof *block_header)) {
      return NULL;
    }

    int layer = HEADER_LAYER(*block_header);
    int class_id = HEADER_CLSID(*block_header);
    int size_bytes = HEADER_SIZE(*block_header);

    if (_conf._print_buffer && class_id != 3) {
      print_buffer_header(block_header);
    }

    if (_is_new_file) {
      if (0 != layer) {
        ERROR("Strange first header (layer != 0).");
      }
      if (1 != class_id) {
        ERROR("Strange first header (class id != 1).");
      }
      _is_new_file = false;
    }

    switch (class_id) {
      case 1: /* Le big block, go inside it. */
        break;
      case 3: /* Event, extract segments, err, subevents later. */
        if (!_input.read_range(&dest->_header._number, sizeof
            dest->_header._number)) {
          ERROR("Could not read event number.");
        }
        dest->_chunk_cur = dest->_chunk_end = dest->_chunks;
        dest->_offset_cur = 0;
        dest->_chunk_end += _input.map_range(size_bytes - 3 * 4,
            dest->_chunks);
        return dest;
      default: /* All udder blox. */
        /* Looks boring, skip. */
        _input.map_range(size_bytes - 2 * 4, dest->_chunks);
        break;
    }
  }
}

void ridf_source::print_buffer_header(ridf_header const *header)
{
  int rev = HEADER_REV(*header);
  int layer = HEADER_LAYER(*header);
  int class_id = HEADER_CLSID(*header);
  int size_bytes = HEADER_SIZE(*header);

  printf("Block Rev %s%1d%s Layer %s%1d%s ClassID %s%2d%s Size %s%8d%s\n",
	 CT_OUT(BOLD),rev,CT_OUT(NORM),
	 CT_OUT(BOLD),layer,CT_OUT(NORM),
	 CT_OUT(BOLD),class_id,CT_OUT(NORM),
	 CT_OUT(BOLD),size_bytes,CT_OUT(NORM)
      );

  printf("      Address %s%08x%s\n",
      CT_OUT(BOLD),header->_address,CT_OUT(NORM));
}

void ridf_event::print_event(int data, hex_dump_mark_buf *unpack_fail) const
{
  printf("Event %s%14u%s Address %s%8x%s Size %s%8d%s\n",
      CT_OUT(BOLD_BLUE),_header._number,CT_OUT(NORM_DEF_COL),
      CT_OUT(BOLD_BLUE),_header._header._address,CT_OUT(NORM_DEF_COL),
      CT_OUT(BOLD_BLUE),HEADER_SIZE(_header._header),CT_OUT(NORM_DEF_COL));

  // Subevents

  for (int subevent = 0; subevent < _nsubevents; subevent++) {
    ridf_subevent *subevent_info = &_subevents[subevent];

    bool subevent_error = (unpack_fail &&
			   unpack_fail->_next == &subevent_info->_header);

    printf(" %sSubEv%s Rev %s%2d%s Dev %s%2d%s FP %s%2d%s Det %s%2d%s "
        "Mod %s%3d%s Size %s%8d%s\n",
        subevent_error ? CT_OUT(BOLD_RED) : "",
        subevent_error ? CT_OUT(NORM_DEF_COL) : "",
        CT_OUT(BOLD_MAGENTA),
        ID_REV(subevent_info->_header._id),
        CT_OUT(NORM_DEF_COL),
        CT_OUT(BOLD_MAGENTA),
        ID_DEV(subevent_info->_header._id),
        CT_OUT(NORM_DEF_COL),
        CT_OUT(BOLD_MAGENTA),
        ID_FP(subevent_info->_header._id),
        CT_OUT(NORM_DEF_COL),
        CT_OUT(BOLD_MAGENTA),
        ID_DET(subevent_info->_header._id),
        CT_OUT(NORM_DEF_COL),
        CT_OUT(BOLD_MAGENTA),
        ID_MOD(subevent_info->_header._id),
        CT_OUT(NORM_DEF_COL),
        CT_OUT(BOLD_MAGENTA),
        HEADER_SIZE(subevent_info->_header._header) - 3 * 4,
        CT_OUT(NORM_DEF_COL));

    if (data) {
      hex_dump_buf buf;

      if (subevent_info->_data) {
        uint32 data_length =
	  HEADER_SIZE(subevent_info->_header._header) -
	  (uint32) sizeof (subevent_info->_header);

        if (0 == (3 & data_length)) {
          hex_dump(stdout,
              (uint32*) subevent_info->_data,
              subevent_info->_data + data_length,0,
              "%c%08x",9,buf,unpack_fail);
        } else {
          hex_dump(stdout,
              (uint16*) subevent_info->_data,
              subevent_info->_data + data_length,0,
              "%c%04x",5,buf,unpack_fail);
        }
      }
    }
  }

  // Is there any remaining data, that could not be assigned as a subevent?

  print_remaining_event(_chunk_cur,_chunk_end,_offset_cur,0);
}

void ridf_event::locate_subevents(ridf_event_hint *hints)
{
  if (_status & RIDF_EVENT_LOCATE_SUBEVENTS_ATTEMPT)
    return;

  _status |= RIDF_EVENT_LOCATE_SUBEVENTS_ATTEMPT;

  assert(!_subevents);

#ifdef USE_THREADING
  hints->_hist_req_realloc <<= 1;
  if (UNLIKELY(hints->_hist_req_realloc == 0)) {
    hints->_max_subevents /= 2;
    if (hints->_max_subevents <= hints->_max_subevents_since_decrease)
      hints->_max_subevents = hints->_max_subevents_since_decrease + 2;
    hints->_max_subevents_since_decrease = 0;
  }
  _subevents = (ridf_subevent *)_wt._defrag_buffer->allocate_reclaim(
      hints->_max_subevents * sizeof (ridf_subevent));
#else
  _subevents = (ridf_subevent *)_defrag_event.allocate(
      hints->_max_subevents * sizeof (ridf_subevent));
#endif

  _nsubevents = 0;

  buf_chunk *chunk_cur  = _chunk_cur;
  size_t     offset_cur = _offset_cur;

  while (chunk_cur < _chunk_end) {

    if (_nsubevents >= hints->_max_subevents) {
      hints->_max_subevents *= 2;
      hints->_hist_req_realloc |= 1;
#ifdef USE_THREADING
      _subevents = (ridf_subevent *)_wt._defrag_buffer->allocate_reclaim_copy(
          hints->_max_subevents * sizeof (ridf_subevent),
          _subevents,
          _nsubevents * sizeof (ridf_subevent));
#else
      _subevents = (ridf_subevent *)_defrag_event.allocate(
          hints->_max_subevents * sizeof (ridf_subevent));
#endif
    }

    ridf_subevent *subevent_info = &_subevents[_nsubevents];

    if (!get_range_many((char *)&subevent_info->_header, chunk_cur,
        offset_cur, _chunk_end, sizeof subevent_info->_header)) {
      ERROR("Subevent header not complete before end of event.");
    }

    size_t data_length = HEADER_SIZE(subevent_info->_header._header) - sizeof
        subevent_info->_header;

    size_t chunk_left = chunk_cur < _chunk_end ? chunk_cur->_length -
        offset_cur : 0;

    if (data_length <= chunk_left) {
      if (0 == data_length) {
        /* Ugh. */
        subevent_info->_data = (char*)16;
      } else {
        subevent_info->_data = chunk_cur->_ptr + offset_cur;
      }
      offset_cur += data_length;
    } else {
      subevent_info->_data   = NULL;
      subevent_info->_frag   = chunk_cur;
      subevent_info->_offset = offset_cur;
      data_length -= chunk_left;

      chunk_cur++;

      if (chunk_cur >= _chunk_end || data_length > chunk_cur->_length) {
        ERROR("Subevent (header 0x%08x, address 0x%08x, id 0x%08x) length %d "
            "bytes, not complete before end of event, %d bytes missing",
            subevent_info->_header._header._code,
            subevent_info->_header._header._address,
            subevent_info->_header._id,
            HEADER_SIZE(subevent_info->_header._header),
            (int)data_length);
      }

      chunk_left = chunk_cur->_length;

      offset_cur = data_length;
    }
    _nsubevents++;

    if (chunk_left == data_length) {
      chunk_cur++;
      offset_cur = 0;
    }

    _chunk_cur = chunk_cur;
    _offset_cur = offset_cur;
  }

#ifdef USE_THREADING
  if (UNLIKELY(_nsubevents >= hints->_max_subevents)) {
    hints->_hist_req_realloc |= 1;
    hints->_max_subevents++;
  }
  if (UNLIKELY(_nsubevents > hints->_max_subevents_since_decrease))
    hints->_max_subevents_since_decrease = _nsubevents;
#endif
}

void ridf_event::get_subevent_data_src(ridf_subevent *subevent_info, char
    *&start, char *&end)
{
  size_t data_length = HEADER_SIZE(subevent_info->_header._header) - sizeof
      subevent_info->_header;

  if (UNLIKELY(subevent_info->_data == NULL))
    {
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

      memcpy(defrag,
          frag->_ptr + subevent_info->_offset,
          size0);

      defrag += size0;
      size_t length = data_length - size0;

      frag++;

      assert(length <= frag->_length);

      memcpy(defrag,
          frag->_ptr,
          length);
    }

  start = subevent_info->_data;
  end   = subevent_info->_data + data_length;
}
