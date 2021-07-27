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

#include "structures.hh"

#include "event_loop.hh"

#include "zero_suppress_map.hh"
#include "signal_id_map.hh"
#include "struct_mapping.hh"

#include "error.hh"
#include "colourtext.hh"

#include "user.hh"

#include "config.hh"

#include "event_base.hh"

#include "paw_ntuple.hh"

#include "watcher.hh"
#include "correlation.hh"
#include "pretty_dump.hh"

#include "struct_calib.hh"

#include "mc_def.hh"

#include "reclaim.hh"

#include "event_sizes.hh"
#include "tstamp_alignment.hh"

#include "../common/strndup.hh"

#include <assert.h>
#include <stdio.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <algorithm>

/********************************************************************/

ucesb_event_loop::ucesb_event_loop()
{
#ifdef USE_LMD_INPUT
  _file_output_bad = NULL;
#endif
#ifdef USE_EXT_WRITER
  _ext_source = NULL;
#endif
}

ucesb_event_loop::~ucesb_event_loop()
{
  // We should not even call this, as it throws
  close_output();
}

/********************************************************************/

event_sizes _event_sizes;

#ifdef USE_LMD_INPUT
tstamp_alignment *_ts_align_hist = NULL;
#endif

event_base _static_event;
sticky_event_base _static_sticky_event;

#ifdef USE_MERGING
// and not USE_THREADING, but does not compile together anyhow
event_base *_current_event = NULL;
#endif

#ifdef CALIB_STRUCT
CALIB_STRUCT      _calib;
#endif

#if defined(USE_CERNLIB) || defined(USE_ROOT) || defined(USE_EXT_WRITER)
paw_ntuple *_paw_ntuple = NULL;
#endif

/********************************************************************/

int _dump_request = 0;

#define DUMP_LEVEL_UNPACK 0x01
#define DUMP_LEVEL_RAW    0x02
#define DUMP_LEVEL_CAL    0x04
#define DUMP_LEVEL_USER   0x08

void add_dump_item(const char *request)
{
#define MATCH_ARG(name) (strcmp(request,name) == 0)

  if (MATCH_ARG("UNPACK"))
    _dump_request |= DUMP_LEVEL_UNPACK;
  else if (MATCH_ARG("RAW"))
    _dump_request |= DUMP_LEVEL_RAW;
  else if (MATCH_ARG("CAL"))
    _dump_request |= DUMP_LEVEL_CAL;
  else if (MATCH_ARG("USER"))
    _dump_request |= DUMP_LEVEL_USER;
  else
    ERROR("Unknown level for dump: %s",request);
}

void dump_init(const char *command)
{
  const char *cmd = command;
  const char *req_end;

  while ((req_end = strpbrk(cmd,",:")) != NULL)
    {
      char *request = strndup(cmd,(size_t) (req_end-cmd));

      add_dump_item(request);

      free(request);
      cmd = req_end+1;
    }

  // and handle the remainder

  add_dump_item(cmd);
}

template<typename T>
void level_dump(int level_mask,const char *level_name,T &level)
{
  if (_dump_request & level_mask)
    {
      pretty_dump_info pdi;
      memset(&pdi,0,sizeof(pdi));
      level.dump(signal_id(level_name),pdi);
    }
}

void level_dump(int level_mask,const char *level_name,dummy_container &level)
{
}

/********************************************************************/

void ucesb_event_loop::preprocess()
{
  //printf ("------------------------------------------------\n");
  //do_unpack_map();
  //printf ("------------------------------------------------\n");

  setup_zero_suppress_info_ptrs();

  setup_signal_id_map();

#ifndef USE_MERGING
  if (_conf._show_members)
    {
      _static_event._unpack.show_members(signal_id("UNPACK"),NULL);
      _static_event._raw.show_members(signal_id("RAW"),NULL);
      _static_event._cal.show_members(signal_id("CAL"),NULL);
#ifdef USER_STRUCT
      _static_event._user.show_members(signal_id("USER"),NULL);
#endif
#ifdef CALIB_STRUCT
      _calib.show_members(signal_id("CALIB"),NULL);
#endif
    }
#endif

#ifdef CALIB_STRUCT
  _calib.__clean();
#endif

#ifndef USE_MERGING
  read_map_calib_info();

  set_rnd_seed_calib_map(); // only required once on program startup

  clear_calib_map();

  process_map_calib_info();

  if (_conf._show_calib)
    {
      show_calib_map();
#ifdef CALIB_STRUCT
      pretty_dump_info pdi;
      memset(&pdi,0,sizeof(pdi));
      _calib.dump(signal_id("CALIB"),pdi);
#endif

    }
#endif

#if defined(USE_CERNLIB) || defined(USE_ROOT) || defined(USE_EXT_WRITER)
  if (_conf._paw_ntuple._command)
    _paw_ntuple = paw_ntuple_open_stage(_conf._paw_ntuple._command,false);
#endif

#ifndef USE_MERGING
#ifdef USE_CURSES
  if (_conf._watcher._command)
    watcher_init(_conf._watcher._command);
#endif
#endif

  if (_conf._event_sizes)
    _event_sizes.init();

  correlation_init(_corr_commands);

  _dump_request = 0;
  if (_conf._dump._command)
    dump_init(_conf._dump._command);

#ifdef INIT_USER_FUNCTION
  INIT_USER_FUNCTION();
#endif
}

void ucesb_event_loop::postprocess()
{
  bool boom = false;

#ifdef USE_LMD_INPUT
  if (_ts_align_hist)
    _ts_align_hist->show();
#endif

  if (_conf._event_sizes)
    _event_sizes.show();

#ifdef EXIT_USER_FUNCTION
  EXIT_USER_FUNCTION();
#endif

  correlation_exit();

#if defined(USE_CERNLIB) || defined(USE_ROOT) || defined(USE_EXT_WRITER)
  if (_paw_ntuple)
    {
      try {
	_paw_ntuple->close();
	delete _paw_ntuple;
	_paw_ntuple = NULL;
      } catch (error &e) {
	boom = true;
      }
    }
#endif

  if (boom)
    throw error();
}


#ifdef USE_MERGING
void ucesb_event_loop::close_source(source_event_base* seb)
{
  bool boom = false;
  try {
    seb->_src->close();
  } catch (error &e) {
    boom = true;
  }

  delete seb->_src;
  delete seb->_event;
  delete seb->_sticky_event;

  _sources.erase(find(_sources.begin(),_sources.end(),seb));

  delete seb;

  if (boom)
    throw error();
}

void ucesb_event_loop::close_sources()
{
  bool boom = false;
  while (!_sources.empty())
    {
      try {
	close_source(*_sources.begin());
      } catch (error &e) {
	boom = true;
      }
    }
  if (boom)
    throw error();
}
#else
void ucesb_event_loop::close_source()
{
  bool boom = false;
  try {
    _source.close();
  } catch (error &e) {
    boom = true;
  }
  if (boom)
    throw error();
}
#endif

void ucesb_event_loop::close_output()
{
  bool boom = false;
#ifdef USE_LMD_INPUT
  for (unsigned int i = 0; i < _output.size(); i++)
    {
      try {
	_output[i]._dest->close();
      } catch (error &e) {
	boom = true;
      }
      delete _output[i]._dest;
      // _output[i]._dest = NULL; // not needed thanks to below clear
    }
  _output.clear();
  if (_file_output_bad)
    {
      try {
	_file_output_bad->close();
      } catch (error &e) {
	boom = true;
      }
      delete _file_output_bad;
      _file_output_bad = NULL;
    }
#endif
  if (boom)
    throw error();
}

#include "titris_stamp.hh"
#include "wr_stamp.hh"

#if defined(USE_LMD_INPUT)
void apply_timestamp_slope(lmd_subevent_10_1_host const &a_header, int a_tsid,
    uint64_t &a_timestamp)
{
  bool found_hit = false;
  for (std::vector<time_slope>::const_iterator it =
      _conf_time_slope_vector.begin(); it != _conf_time_slope_vector.end();
      ++it) {
#define APPLY_TIMESTAMP_SLOPE_SKIP(ts_part, event_part)\
    (it->ts_part != -1 && it->ts_part != event_part)
    if (APPLY_TIMESTAMP_SLOPE_SKIP(proc, a_header.i_procid)) continue;
    if (APPLY_TIMESTAMP_SLOPE_SKIP(ctrl, a_header.h_control)) continue;
    if (APPLY_TIMESTAMP_SLOPE_SKIP(crate, a_header.h_subcrate)) continue;
    if (APPLY_TIMESTAMP_SLOPE_SKIP(tsid, a_tsid)) continue;
    if (found_hit) {
      ERROR("Several time-slope matches for event, please be more specific.");
    }
    found_hit = true;
    if (it->mult != -1) {
      a_timestamp *= it->mult;
    }
    if (it->add != -1) {
      a_timestamp += it->add;
    }
    // TODO: This cannot check the reverse, i.e. when different sub-events
    // match to one time-slope. Would require some clever book-keeping.
  }
}

bool get_titris_timestamp(FILE_INPUT_EVENT *src_event,
			  uint64_t *timestamp,
			  ssize_t *ts_align_index)
{
  if (src_event->_nsubevents < 1)
    ERROR("No subevents, cannot get a TITRIS time stamp.");

  typedef __typeof__(*src_event->_subevents) subevent_t;

  subevent_t *subevent_info = &src_event->_subevents[0 /* subevent */];

  char *start;
  char *end;

  src_event->get_subevent_data_src(subevent_info,start,end);

  // The timestamp info is as 32-bit words, so we need only care about
  // swapping.

  uint32_t *data     = (uint32_t *) start;
  uint32_t *data_end = (uint32_t *) end;

  if (data + 1 > data_end)
    ERROR("First subevent does not have data enough for TITRIS time stamp "
	"branch id.");

  int swapping = src_event->_swapping;

  uint32_t error_branch_id = SWAPPING_BSWAP_32(data[0]);

  if (error_branch_id & TITRIS_STAMP_EBID_UNUSED)
    ERROR("Unused bits set in TITRIS time stamp branch ID word: 0x%08x "
	  "(full: 0x%08x).",
	  error_branch_id & TITRIS_STAMP_EBID_UNUSED,
	  error_branch_id);

  if (error_branch_id & TITRIS_STAMP_EBID_ERROR)
    return false;

  if (ts_align_index)
    {
      if (!_ts_align_hist)
	*ts_align_index = -1;
      else
	*ts_align_index =
	  _ts_align_hist->get_index(subevent_info,
				    error_branch_id &
				    TITRIS_STAMP_EBID_BRANCH_ID_MASK);
    }

  if (data + 4 > data_end)
    ERROR("First subevent does not have data enough for TITRIS time stamp "
	"values.");

  uint32_t id     = SWAPPING_BSWAP_32(data[0]);
  uint32_t ts_l16 = SWAPPING_BSWAP_32(data[1]);
  uint32_t ts_m16 = SWAPPING_BSWAP_32(data[2]);
  uint32_t ts_h16 = SWAPPING_BSWAP_32(data[3]);

  if ((ts_l16 & TITRIS_STAMP_LMH_ID_MASK) != TITRIS_STAMP_L16_ID ||
      (ts_m16 & TITRIS_STAMP_LMH_ID_MASK) != TITRIS_STAMP_M16_ID ||
      (ts_h16 & TITRIS_STAMP_LMH_ID_MASK) != TITRIS_STAMP_H16_ID)
    ERROR("TITRIS time stamp word has wrong marker.  (0x%08x 0x%08x 0x%08x)",
	  ts_l16, ts_m16, ts_h16);

  *timestamp =
      (             ts_l16 & TITRIS_STAMP_LMH_TIME_MASK)         |
      (            (ts_m16 & TITRIS_STAMP_LMH_TIME_MASK)  << 16) |
      (((uint64_t) (ts_h16 & TITRIS_STAMP_LMH_TIME_MASK)) << 32);
  apply_timestamp_slope(subevent_info->_header, id, *timestamp);

  return true;
}

bool get_wr_timestamp(FILE_INPUT_EVENT *src_event,
		      uint64_t *timestamp,
		      ssize_t *ts_align_index)
{
  if (src_event->_nsubevents < 1)
    ERROR("No subevents, cannot get a WR time stamp.");

  typedef __typeof__(*src_event->_subevents) subevent_t;

  subevent_t *subevent_info = &src_event->_subevents[0 /* subevent */];

  char *start;
  char *end;

  src_event->get_subevent_data_src(subevent_info,start,end);

  // The timestamp info is as 32-bit words, so we need only care about
  // swapping.

  uint32_t *data     = (uint32_t *) start;
  uint32_t *data_end = (uint32_t *) end;

  if (data + 1 > data_end)
    ERROR("First subevent does not have data enough for WR time stamp branch"
	" id.");

  int swapping = src_event->_swapping;

  uint32_t error_branch_id = SWAPPING_BSWAP_32(data[0]);

  if (error_branch_id & WR_STAMP_EBID_UNUSED)
    ERROR("Unused bits set in WR time stamp branch ID word: 0x%08x "
	  "(full: 0x%08x).",
	  error_branch_id & WR_STAMP_EBID_UNUSED,
	  error_branch_id);

/* HTT: Don't know about this yet...
  if (error_branch_id & WR_STAMP_EBID_ERROR)
    return false;
*/

  if (ts_align_index)
    {
      if (!_ts_align_hist)
        *ts_align_index = -1;
      else
	*ts_align_index =
	  _ts_align_hist->get_index(subevent_info,
				    error_branch_id &
				    WR_STAMP_EBID_BRANCH_ID_MASK);
    }

  if (data + 5 > data_end)
    ERROR("First subevent does not have data enough "
	  "for WR time stamp values.");

  uint32_t id      = SWAPPING_BSWAP_32(data[0]);
  uint32_t ts_0_16 = SWAPPING_BSWAP_32(data[1]);
  uint32_t ts_1_16 = SWAPPING_BSWAP_32(data[2]);
  uint32_t ts_2_16 = SWAPPING_BSWAP_32(data[3]);
  uint32_t ts_3_16 = SWAPPING_BSWAP_32(data[4]);

  if ((ts_0_16 & WR_STAMP_DATA_ID_MASK) != WR_STAMP_DATA_0_16_ID ||
      (ts_1_16 & WR_STAMP_DATA_ID_MASK) != WR_STAMP_DATA_1_16_ID ||
      (ts_2_16 & WR_STAMP_DATA_ID_MASK) != WR_STAMP_DATA_2_16_ID ||
      (ts_3_16 & WR_STAMP_DATA_ID_MASK) != WR_STAMP_DATA_3_16_ID)
    ERROR("WR time stamp word has wrong marker.  "
	  "(0x%08x 0x%08x 0x%08x 0x%08x)",
	  ts_0_16, ts_1_16, ts_2_16, ts_3_16);

  *timestamp =
      (             ts_0_16 & WR_STAMP_DATA_TIME_MASK)         |
      ((            ts_1_16 & WR_STAMP_DATA_TIME_MASK)  << 16) |
      (((uint64_t) (ts_2_16 & WR_STAMP_DATA_TIME_MASK)) << 32) |
      (((uint64_t) (ts_3_16 & WR_STAMP_DATA_TIME_MASK)) << 48);
  apply_timestamp_slope(subevent_info->_header, id, *timestamp);

  return true;
}

bool get_timestamp(int timestamp_type,
		   FILE_INPUT_EVENT *src_event,
		   uint64_t *timestamp,
		   ssize_t *ts_align_index)
{
  switch (timestamp_type)
    {
    case TIMESTAMP_TYPE_TITRIS:
      return get_titris_timestamp(src_event, timestamp, ts_align_index);
    case TIMESTAMP_TYPE_WR:
      return get_wr_timestamp(src_event, timestamp, ts_align_index);
    }
  assert(false);
}
#endif//USE_LMD_INPUT

#if USE_MERGING
void do_merge_prepare_event_info(source_event_base *x)
{
  // hmmm, rather be in a global clean, as then only needed in
  // the appropriate case?
  x->_tstamp_align_index = -1;

  switch (_conf._merge_event_mode)
    {
    case MERGE_EVENTS_MODE_EVENTNO:
      // Nothing to prepare
      break;
    case MERGE_EVENTS_MODE_TITRIS_TIME:
    case MERGE_EVENTS_MODE_WR_TIME:
      {
	FILE_INPUT_EVENT *src_event =
	  (FILE_INPUT_EVENT *) x->_event->_file_event;

	x->_tstamp_align_index = -1;

	bool good_stamp =
	  get_timestamp(_conf._merge_event_mode,
			src_event, &x->_timestamp,
			&x->_tstamp_align_index);

	if (!good_stamp)
	  {
	    WARNING("Error set in time-stamp.  "
		    "Propagating event immediately.");
	    // compares less than anyone else, so immediate eject
	    x->_timestamp = 0;
	  }
      }
    case MERGE_EVENTS_MODE_USER:
      // Nothing to prepare ???  (if so, where to store it?)
      break;
    }
}

bool do_merge_compare_events_after(const source_event_base *x,
				   const source_event_base *y)
{
  switch (_conf._merge_event_mode)
    {
#ifdef MERGE_COMPARE_EVENTS_AFTER
    case MERGE_EVENTS_MODE_USER:
      return MERGE_COMPARE_EVENTS_AFTER(&x->_event->_unpack,
					&y->_event->_unpack);
#endif
#ifdef USE_LMD_INPUT
    case MERGE_EVENTS_MODE_EVENTNO:
      return x->_event->_unpack.event_no > y->_event->_unpack.event_no;
    case MERGE_EVENTS_MODE_TITRIS_TIME:
    case MERGE_EVENTS_MODE_WR_TIME:
      return x->_timestamp > y->_timestamp;
#else
#error Merge needs a comparison function to order the events
#endif
    default:
      ERROR("Internal error, no merge comparison function.");
    }
}

union merge_event_order
{
  uint32   event_no;   // for MERGE_EVENTS_MODE_EVENTNO
  uint64_t _timestamp; // for MERGE_EVENTS_MODE_*_TIME
};

merge_event_order _static_merge_event_order;

int do_check_merge_event_after(const merge_event_order *prev,
			       const source_event_base *x)
{
  switch (_conf._merge_event_mode)
    {
    case MERGE_EVENTS_MODE_EVENTNO:
      /*
      printf ("cmp prev: %d   x: %d\n",
	      prev->event_no,x->_event->_unpack.event_no);
      */
      if (x->_event->_unpack.event_no > prev->event_no)
	return 0;

      // Whenever we come here, bad things are going on, so printing is fine
      WARNING("From source: '" ERR_WHITE "%s" ERR_ENDCOL "':",
	      x->_name);
      WARNING("Previous event: " ERR_RED "%d" ERR_ENDCOL ", "
	      "this event: " ERR_BLUE "%d" ERR_ENDCOL ".",
	      prev->event_no,x->_event->_unpack.event_no);

      if (x->_event->_unpack.event_no == prev->event_no)
	return MERGE_EVENTS_ERROR_ORDER_SAME;
      return MERGE_EVENTS_ERROR_ORDER_BEFORE;
    case MERGE_EVENTS_MODE_TITRIS_TIME:
    case MERGE_EVENTS_MODE_WR_TIME:

      if (x->_timestamp < prev->_timestamp)
	{
	  WARNING("From source: '" ERR_WHITE "%s" ERR_ENDCOL "':",
		  x->_name);
	  WARNING("This timestamp:     "
		  ERR_RED "%016" PRIx64 "" ERR_ENDCOL ", "
		  "out of order:",
		  x->_timestamp);
	  WARNING("Previous timestamp: "
		  ERR_BLUE "%016" PRIx64 "" ERR_ENDCOL ".",
		  prev->_timestamp);
	  // We continue to sort anyhow.  Will just mean that current
	  // event gets immediate dump.
	}

      return 0;
    default:
      return 0;
    }
}

merge_event_order *
do_record_merge_event_info(merge_event_order *info,
			   const source_event_base *x)
{
  switch (_conf._merge_event_mode)
    {
    case MERGE_EVENTS_MODE_EVENTNO:
      _static_merge_event_order.event_no = x->_event->_unpack.event_no;
      break;
    case MERGE_EVENTS_MODE_TITRIS_TIME:
    case MERGE_EVENTS_MODE_WR_TIME:
      _static_merge_event_order._timestamp = x->_timestamp;
      break;
    }
  return &_static_merge_event_order;
}

void print_current_merge_order(const merge_event_order *prev)
{
  switch (_conf._merge_event_mode)
    {
    case MERGE_EVENTS_MODE_EVENTNO:
      fprintf(stderr,
	      "Ev# %s%10d%s",
	      CT_OUT(BOLD_BLUE),
	      prev->event_no,
	      CT_OUT(NORM_DEF_COL));
      break;
    case MERGE_EVENTS_MODE_TITRIS_TIME:
      fprintf(stderr,
	      "HI: %s0x%08" PRIx64 ":%02" PRIx64 "%s",
	      CT_OUT(BOLD_BLUE),
	      prev->_timestamp >> 32,
	      (prev->_timestamp >> 24) & 0xff,
	      CT_OUT(NORM_DEF_COL));
      break;
    }
}
#endif

#if !USE_THREADING
template<typename source_type>
void ucesb_event_loop::open_source(source_type &source,
#if defined(USE_EXT_WRITER)
				   paw_ntuple *&ext_source,
#endif
				   config_input &input,
				   input_buffer **file_input
				   PTHREAD_PARAM(thread_block *block_reader) )
{
#ifdef USE_EXT_WRITER
  ext_source = NULL;
#endif
#if 0 /* Handled below with normal open(). */
#ifdef USE_RFIO
  if (input._type == INPUT_TYPE_RFIO)
    {
      source.open_rfio(input._name
		       PTHREAD_ARG(block_reader) );
    }
  else
#endif
#endif
#ifdef USE_EXT_WRITER
  if (input._type == INPUT_TYPE_EXTERNAL)
    {
      _ext_source = paw_ntuple_open_stage(input._name,true);
      *file_input = NULL;
      return;
    }
  else
#endif
#ifdef USE_LMD_INPUT
  if (input._type != INPUT_TYPE_FILE &&
      input._type != INPUT_TYPE_RFIO &&
      input._type != INPUT_TYPE_FILE_SRM)
    {
      source.connect(input._name,
		     input._type
		     PTHREAD_ARG(block_reader) );
    }
  else
#endif
    {
      source.open(input._name
		  PTHREAD_ARG(block_reader) ,
		  _conf._no_mmap,
		  input._type == INPUT_TYPE_RFIO,
		  input._type == INPUT_TYPE_FILE_SRM);
    }
  source.new_file();
  *file_input = source._input._input;

#ifdef USE_LMD_INPUT
  if (input._type != INPUT_TYPE_RFIO &&
      input._type != INPUT_TYPE_FILE &&
      input._type != INPUT_TYPE_FILE_SRM)
    {
      source._expect_file_header = false;
      source._buffers_maybe_missing = true;
      source._close_is_error = true;
    }
#endif
}

void ucesb_event_loop::open_source(config_input &input,
				   input_buffer **file_input
				   PTHREAD_PARAM(thread_block *block_reader) )
{
#ifdef USE_MERGING
  // The following should preferably have been something like:
  // typedef __typeof__(_sources.value_type::_src) source_type;

#ifdef USE_INPUTFILTER
  typedef lmd_source_multievent source_type;
#else
  typedef lmd_source source_type;
#endif

  source_type *source = new source_type();

  try
    {
      open_source(*source,
#ifdef USE_EXT_WRITER
		  _ext_source,
#endif
		  input,file_input
		  PTHREAD_ARG(block_reader) );
    }
  catch (error &e)
    {
      delete (source);
      throw;
    }

  source_event_base *seb = new source_event_base();

  seb->_src   = source;
  seb->_event = new event_base;
  seb->_sticky_event = new sticky_event_base;

  seb->_sticky_event->_file_event =
    seb->_event->_file_event = &seb->_src->_file_event;

  seb->_name = input._name;

  seb->_events = 0;
  seb->_events_last_show = 0;

  _sources.push_back(seb);
  _sources_need_event.push_back(seb);
#else
  open_source(_source,
#ifdef USE_EXT_WRITER
	      _ext_source,
#endif
	      input,file_input
	      PTHREAD_ARG(block_reader) );
#endif
}
#endif//!USE_THREADING

bool ucesb_event_loop::get_ext_source_event(event_base &eb)
{
#if defined(USE_EXT_WRITER)
  assert(_ext_source);
  return _ext_source->get_event();
#else
  return false;
#endif
}

void ucesb_event_loop::close_ext_source()
{
#if defined(USE_EXT_WRITER)
  if (_ext_source)
    {
      _ext_source->close();
      delete _ext_source;
    }
  _ext_source = NULL;
#endif
}

template<typename subevent_header_t>
void err_bold_header(char* headermsg,
		     subevent_header_t *ev_header)
{
#if defined(USE_LMD_INPUT)
  sprintf (headermsg,
	   ERR_BOLD "%5d" ERR_ENDBOLD "/"
	   ERR_BOLD "%5d" ERR_ENDBOLD
	   " (id:" ERR_BOLD "%d" ERR_ENDBOLD
	   ",crate:" ERR_BOLD "%d" ERR_ENDBOLD
	   ",ctrl:" ERR_BOLD "%d" ERR_ENDBOLD ")",
	   ev_header->_header.i_type,
	   ev_header->_header.i_subtype,
	   ev_header->i_procid,
	   ev_header->h_subcrate,
	   ev_header->h_control);
#elif defined(USE_HLD_INPUT)
  sprintf (headermsg,
	   "id:" ERR_BOLD "%08x" ERR_ENDBOLD
	   "(" ERR_BOLD "%d" ERR_ENDBOLD
	   "/" ERR_BOLD "%4d" ERR_ENDBOLD ")",
	   ev_header->_id,
	   ev_header->_id >> 31,
	   ev_header->_id & 0x7fff);
#elif defined(USE_PAX_INPUT)
  sprintf (headermsg,
	   ERR_BOLD "%5d" ERR_ENDBOLD,
	   ev_header->_type);
#elif defined(USE_RIDF_INPUT)
  sprintf (headermsg,
	   "id:" ERR_BOLD "%08x" ERR_ENDBOLD,
	   ev_header->_id);
#else
  headermsg[0] = 0;
  UNUSED(ev_header);
#endif
}

#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT)
#define HAS_WORD_32 1
#define HAS_WORD_16 1
#define HAS_WORD_8  1
#elif defined(USE_PAX_INPUT) || defined(USE_GENF_INPUT) || \
  defined(USE_EBYE_INPUT_16)
#define HAS_WORD_16 1
#elif defined(USE_EBYE_INPUT_32) || defined(USE_RIDF_INPUT) // ridf is 32 only?
#define HAS_WORD_32 1
#endif

// Disable warnings for WARNING below.  (Only prints numbers, so safe).
#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 6)))
#pragma GCC diagnostic push
#endif
#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)))
#pragma GCC diagnostic ignored "-Wformat-security"
#endif

template<typename __data_src_t,typename start_ptr_t,
	 typename event_base_t,typename subevent_header_t>
void show_remaining(event_base_t &eb,
		    subevent_header_t *ev_header,
		    __data_src_t &src,start_ptr_t *start,
		    int loc)
{
  char headermsg[128];

  char msg[256];
  char msgthis[64];
  size_t msglen = 0;
  size_t msgthislen = 0;
  msg[0] = 0;

  const char *subevstr =
    eb.is_sticky() ? "Sticky subevent" : "Subevent";

  eb._unpack_fail._next = src._data;

  err_bold_header(headermsg,ev_header);

  size_t used = (size_t) (src._data - start);
  size_t left = (size_t) (src._end - src._data);

  msglen += (size_t) sprintf (msg+msglen,
			      ERR_BOLD "%zd" ERR_ENDBOLD " bytes used",used);

  if (used)
    {
      __data_src_t tmpsrc  = src;
      __data_src_t tmpsrc2 = src;
      size_t prev = 0;

#define ADD_MSG_DATA(len,msg,word,col,fmt,data)				\
      len +=								\
	(size_t) sprintf (msg + len,					\
			  " (" word " "					\
			  ERR_BOLD col "0x" fmt ERR_ENDCOL ERR_ENDBOLD ")", \
			  data);

#if HAS_WORD_32
      if (!(used % sizeof(uint32))) {
	uint32 this_data = 0;
	tmpsrc.reverse(sizeof(uint32));
	tmpsrc2 = tmpsrc; prev = used - sizeof(uint32);
	tmpsrc.get_uint32(&this_data);
	ADD_MSG_DATA(msgthislen,msgthis,"this dword",ERR_RED,"%08x",this_data);
      }
      else
#endif
      {
#if HAS_WORD_16
      if (!(used % sizeof(uint16))) {
	uint16 this_data = 0;
	tmpsrc.reverse(sizeof(uint16));
	tmpsrc2 = tmpsrc; prev = used - sizeof(uint16);
	tmpsrc.get_uint16(&this_data);
	ADD_MSG_DATA(msgthislen,msgthis,"this word",ERR_RED,"%04x",this_data);
      }
#endif
#if HAS_WORD_8
      else {
	uint8 this_data = 0;
	tmpsrc.reverse(sizeof(uint8));
	tmpsrc2 = tmpsrc; prev = used - sizeof(uint8);
	tmpsrc.get_uint8(&this_data);
	ADD_MSG_DATA(msgthislen,msgthis,"this byte",ERR_RED,"%04x",this_data);
      }
#endif
      }

      eb._unpack_fail._this = tmpsrc2._data;

      if (prev)
	{
#if HAS_WORD_32
	  if (!(prev % sizeof(uint32))) {
	    uint32 prev_data = 0;
	    tmpsrc2.reverse(sizeof(uint32));
	    tmpsrc = tmpsrc2;
	    tmpsrc2.get_uint32(&prev_data);
	    ADD_MSG_DATA(msglen,msg,"prev dword",ERR_BLACK,"%08x",prev_data);
	  }
	  else
#endif
	  {
#if HAS_WORD_16
	  if (!(prev % sizeof(uint16))) {
	    uint16 prev_data = 0;
	    tmpsrc2.reverse(sizeof(uint16));
	    tmpsrc = tmpsrc2;
	    tmpsrc2.get_uint16(&prev_data);
	    ADD_MSG_DATA(msglen,msg,"prev word",ERR_BLACK,"%04x",prev_data);
	  }
#endif
#if HAS_WORD_8
	  else {
	    uint8 prev_data = 0;
	    tmpsrc2.reverse(sizeof(uint8));
	    tmpsrc = tmpsrc2;
	    tmpsrc2.get_uint8(&prev_data);
	    ADD_MSG_DATA(msglen,msg,"prev byte",ERR_BLACK,"%02x",prev_data);
	  }
#endif
	  }
	  eb._unpack_fail._prev = tmpsrc._data;
	}

      strcpy(msg+msglen,msgthis);
      msglen += msgthislen;
    }

  msglen += (size_t) sprintf (msg+msglen,".");
  WARNING(msg);

  msg[0] = 0;
  msglen = 0;

  msglen += (size_t) sprintf (msg+msglen,
			      ERR_BOLD "%zd" ERR_ENDBOLD " bytes left",left);

  __data_src_t tmpsrc = src;

#if HAS_WORD_32
  if (left >= sizeof(uint32) && !(used % sizeof(uint32))) {
    uint32 next_data = 0;
    tmpsrc.get_uint32(&next_data);
    ADD_MSG_DATA(msglen,msg,"next dword",ERR_BLUE,"%08x",next_data);
  }
  else
#endif
  {
#if HAS_WORD_16
  if (left >= sizeof(uint16) && !(used % sizeof(uint16))) {
    uint16 next_data = 0;
    tmpsrc.get_uint16(&next_data);
    ADD_MSG_DATA(msglen,msg,"next word",ERR_BLUE,"%04x",next_data);
  }
#endif
#if HAS_WORD_8
  else if (left >= sizeof(uint8)) {
    uint8 next_data = 0;
    tmpsrc.get_uint8(&next_data);
    ADD_MSG_DATA(msglen,msg,"next byte",ERR_BLUE,"%02x",next_data);
  }
#endif
  }
  msglen += (size_t) sprintf (msg+msglen,".");
  WARNING(msg);

  if (loc) {
    ERROR_U_LOC(loc,ERR_NOBOLD "%s: " ERR_NOBOLD "%s not completely read.",
		subevstr, headermsg);
  } else {
    ERROR(ERR_NOBOLD "%s: " ERR_NOBOLD "%s not completely read.",
	  subevstr, headermsg);
  }
}
#if ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 6)))
#pragma GCC diagnostic pop
#endif

template<typename __data_src_t,typename start_ptr_t,
	 typename event_base_t,typename subevent_header_t>
void unpack_subevent(event_base_t &eb,
		     subevent_header_t *ev_header,
		     __data_src_t &src,
		     start_ptr_t *start)
{
  /* Loop over all our known subevents, and see if it is there.
   *
   * We have: type, subtype, control, subcrate, procid
   *
   * The idea is that these are essentially fixed, i.e. every subevent
   * would only occur with one specific set of values.
   *
   * - type, subtype particularly specifies the subevent type
   *
   * - procid would specify the type of processor, so could change,
   * but should then not change the format
   *
   * - control and subcrate would be used to differentiate between
   * different instances of the same subevent.
   *
   * So we should rather have subevents switch on their type/subtype,
   * and then control and subcrate would choose the particular
   * instance, while procid would need to tell the endianess of
   * the producer.
   */

  {
    uint64 wr = 0;
    __data_src_t wr_reader = src;
    uint32 bits0, bits1, bits2, bits3, bits4;
    wr_reader.get_uint32(&bits0); // useless
    wr_reader.get_uint32(&bits1);
    wr_reader.get_uint32(&bits2);
    wr_reader.get_uint32(&bits3);
    wr_reader.get_uint32(&bits4);
    if ((bits1 & 0xffff0000) == 0x03e10000 && (bits2 & 0xffff0000) == 0x04e10000
      && (bits3 & 0xffff0000) == 0x05e10000 && (bits4 & 0xffff0000) == 0x06e10000)
    {
      wr = (bits1 & 0xffff) | (bits2 & 0xffff) << 16
	| ((uint64)bits3 & 0xffff) << 32
	| ((uint64)bits4 & 0xffff) << 48;
      eb._unpack.wr.push_back({ bits0, wr });
    }
  }

  int loc;
  try {
    loc = eb._unpack.__unpack_subevent((subevent_header*) ev_header,src);
  } catch (error &e) {
    // eb._unpack_fail._next = src._data; // done in show_remaining
    show_remaining(eb,ev_header,src,start,0);
    throw;
  }

  if (!loc)
    {
      char headermsg[128];

      if (eb._unpack.ignore_unknown_subevent())
	return;

      const char *subevstr =
	eb.is_sticky() ? "Sticky subevent" : "Subevent";

      err_bold_header(headermsg,ev_header);

      eb._unpack_fail._next = ev_header;
      ERROR(ERR_NOBOLD "%s: " ERR_NOBOLD "%s (%d bytes) unknown.",
	    subevstr, headermsg,
	    (int) (src._end - src._data));
    }
  else
    eb._unpack_fail._next = src._data;
  assert(src._data <= src._end);
  if (src._data < src._end)
    show_remaining(eb,ev_header,src,start,loc);
}

template<typename event_base_t,typename subevent_header_t>
void revoke_subevent(event_base_t &eb,
		     subevent_header_t *ev_header)
{
  int loc;
  loc = eb._unpack.__revoke_subevent((subevent_header*) ev_header);

  if (!loc)
    {
      char headermsg[128];

      if (eb._unpack.ignore_unknown_subevent())
	return;

      const char *subevstr =
	eb.is_sticky() ? "Sticky subevent" : "Subevent";

      err_bold_header(headermsg,ev_header);

      eb._unpack_fail._next = ev_header;
      ERROR(ERR_NOBOLD "%s: " ERR_NOBOLD "%s unknown, cannot revoke.",
	    subevstr, headermsg);
    }
}

void unpack_clean(event_base &eb)
{
  eb._unpack.__clean();
  eb._unpack.__clear_visited();
}

void unpack_clean(sticky_event_base &eb)
{
  // Sticky events are NOT cleared every time they get unpacked.
  eb._unpack.__clear_visited();
}

#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
void ucesb_event_loop::pre1_unpack_event(FILE_INPUT_EVENT *src_event)
{
#ifdef USE_LMD_INPUT
  // First get the basic event info
  src_event->get_10_1_info();    // this may throw up (also)...
#endif

  // The above figures out if an event is sticky, so must before
  // we call the correct version of pre2_unpack_event...
}

template<typename T_event_base>
void ucesb_event_loop::pre2_unpack_event(T_event_base &eb,
					 source_event_hint_t *hints)
{
  unpack_clean(eb);

#if USE_THREADING || USE_MERGING
  FILE_INPUT_EVENT *src_event = (FILE_INPUT_EVENT *) eb._file_event;
#else
  FILE_INPUT_EVENT *src_event = &_file_event;
#endif

#ifdef USE_LMD_INPUT
  eb._unpack.trigger  = (uint16) src_event->_header._info.i_trigger;
  eb._unpack.dummy    = (uint16) src_event->_header._info.i_dummy;
  eb._unpack.event_no = (uint32) src_event->_header._info.l_count;
#ifdef USE_INPUTFILTER
  eb._unpack.is_aida = src_event->_aida_extra;
  eb._unpack.aida_implant = src_event->_aida_implant;
  eb._unpack.aida_length = src_event->_aida_length;
#endif
#endif
#ifdef USE_HLD_INPUT
  eb._unpack.event_no = src_event->_header._seq_no;
#endif
#ifdef USE_RIDF_INPUT
  eb._unpack.event_no = src_event->_header._number;
#endif

  // Next thing is to localise the subevents inside the event

  src_event->locate_subevents(hints); // this may throw up...
}

// Force instantiation
template
void ucesb_event_loop::
pre2_unpack_event<event_base>(event_base &eb,
			      source_event_hint_t *hints);
template
void ucesb_event_loop::
pre2_unpack_event<sticky_event_base>(sticky_event_base &eb,
				     source_event_hint_t *hints);
#endif//USE_LMD_INPUT || USE_HLD_INPUT || USE_RIDF_INPUT


#if defined(USE_LMD_INPUT)
void ucesb_event_loop::stitch_event(event_base &eb,
				    stitch_info *stitch)
{
  assert(_conf._event_stitch_mode == EVENT_STITCH_MODE_TITRIS_TIME ||
	 _conf._event_stitch_mode == EVENT_STITCH_MODE_WR_TIME);

  bool prev_badstamp = stitch->_badstamp;

  // In case any fetch fails, we do not combine
  stitch->_badstamp = true;
  stitch->_combine = false;

  // Timestamp is in first subevent

#if USE_THREADING || USE_MERGING
  FILE_INPUT_EVENT *src_event = (FILE_INPUT_EVENT *) eb._file_event;
#else
  FILE_INPUT_EVENT *src_event = &_file_event;
#endif

  uint64_t timestamp;

  bool good_stamp =
    get_timestamp(_conf._event_stitch_mode, src_event, &timestamp, NULL);

  if (!good_stamp)
    {
      WARNING("Error set in time-stamp.  Dumping event by itself.");
      // printf("!good -> bad !combine\n");
      stitch->_has_stamp = false;
      return;
    }

  // This event has a good timestamp.  Even if unordered it is good
  // (unordered may be due to other (previous) stamp being wrong).
  stitch->_badstamp = false;

  bool is_implant_aida = false;
  bool is_implant_frs = false;
  if (src_event->_aida_implant) is_implant_aida = true;
  // todo: figure out how to spot an FRS event in a non stupid way
  if(src_event->_subevents[0]._data[1] == 0x1) {
    is_implant_frs = true;
  }


  if (stitch->_has_stamp)
    {
      if (false && timestamp < stitch->_last_stamp)
	{
	  // Unordered, dump!
	  // printf("unordered -> !bad !combine\n");
	  // Since it may have been 'previous' stamp that was bad,
	  // we fall through and set our new stamp.
	}
      else if (prev_badstamp)
	{
	  // Previous timestamp was bad.  We cannot combine with it.
	  // printf("prevbad -> !bad !combine\n");
	}
      else
	{
	  if ((int64_t) (timestamp - stitch->_last_stamp) <
	      _conf._event_stitch_value)
	    {
	      if ((is_implant_aida && !stitch->_implant[0])
		    || (is_implant_frs && !stitch->_implant[1]))
	      {
		stitch->_combine = true;
		stitch->_implant[0] |= is_implant_aida;
		stitch->_implant[1] |= is_implant_frs;
		stitch->_last_stamp = timestamp + _conf._event_stitch_value;
	      }
	      else if (!is_implant_aida && !is_implant_frs)
	      {
		stitch->_combine = true;
	      }
	      // printf("ordered -> !bad combine\n");
	      // Do not change stitch->_last_stamp, we are combining
	      // against the stamp of the first event in this new event.
	    }
	  else
	    {
	      // printf("ordered -> !bad !combine\n");
	    }
	}
    }
  /*
  printf ("%012lx (%d %d) %012lx (%ld)\n",
         timestamp,
         stitch->_combine,stitch->_badstamp, stitch->_last_stamp, (int64_t)(timestamp - stitch->_last_stamp));
   */

#ifdef USE_INPUTFILTER 
  // Only make the window wide at the start of stitching
  if (_conf._aida_new_stitch && src_event->_aida_extra && !stitch->_combine)
  {
    timestamp += src_event->_aida_length;
  }

  // Do not stitch special triggers
  if (src_event->_header._info.i_trigger >= 10 && src_event->_header._info.i_trigger <= 13)
  {
    stitch->_combine = false;
  }
#endif

  //if (timestamp > stitch->_last_stamp)
  if (!stitch->_implant[0] && !stitch->_implant[1])
    stitch->_last_stamp = timestamp;
  if (!stitch->_combine)
    {
      stitch->_has_stamp = true;
      stitch->_implant[0] = is_implant_aida;
      stitch->_implant[1] = is_implant_frs;
    }
  // printf("stitch->_last_stamp = timestamp\n");
}
#endif

template<typename T_event_base>
void ucesb_event_loop::unpack_event(T_event_base &eb)
{
  eb._unpack_fail._prev = eb._unpack_fail._this = eb._unpack_fail._next = NULL;

#if USE_THREADING || USE_MERGING
  FILE_INPUT_EVENT *src_event = (FILE_INPUT_EVENT *) eb._file_event;
#else
  FILE_INPUT_EVENT *src_event = &_file_event;
#endif

  if (_dump_request)
    {
      printf ("\nEvent%s%12u%s Trigger %s%2d%s\n\n",
	      CT_OUT(BOLD_BLUE),
	      eb._unpack.event_no,
	      CT_OUT(NORM_DEF_COL),
	      CT_OUT(BOLD_BLUE),
	      eb._unpack.trigger,
	      CT_OUT(NORM_DEF_COL));
    }

#ifdef USE_LMD_INPUT
  if (_conf._event_sizes)
    _event_sizes.account(src_event);
#endif

#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
  // printf ("---> %d\n",src_event->_nsubevents);
  // fflush(stdout);

  // It is actually perfectly valid to not have any subevents

  for (int subevent = 0; subevent < src_event->_nsubevents; subevent++)
    {
      // Hmm, before we start to think about fetching the data for the
      // subevent (which will start to do copying if it is in any way
      // in our (or lmd) buffers fragmented, we should check that it is not
      // a subevent that is just to be skipped

      // This could be fixed by arranging for a hash table (of some
      // sorts) for the subevents, giving a function pointer etc to do
      // the actual unpacking work, instead of the current method
      // (which scales horribly when there are many kinds of
      // subevents)

      // Also, with a hash table, we can directly fix up the
      // swap/scramble correct pointers...  (he, he)

      typedef __typeof__(*src_event->_subevents) subevent_t;

      subevent_t *subevent_info = &src_event->_subevents[subevent];

      char *start;
      char *end;

      src_event->get_subevent_data_src(subevent_info,start,end);

      if (eb.is_sticky())
	{
      // NOTE! This is for testing only!!!
      // TODO: Remove!!!
#ifdef STICKY_SUBEVENT_USER_FUNCTION
	  STICKY_SUBEVENT_USER_FUNCTION((unpack_sticky_event *) &eb._unpack,
					&subevent_info->_header,
					start, end, src_event->_swapping);
#endif
	}

      if (start == NULL)
	{
	  revoke_subevent(eb,&subevent_info->_header);
	  continue;
	}

#ifdef USE_LMD_INPUT
      int scramble;

#if defined(__sparc__) || defined(__powerpc__)
      scramble = 0;
#else
      scramble = 1;
#endif
      scramble ^= src_event->_swapping;
      scramble ^= _conf._scramble;

      if (src_event->_swapping)
	{
	  if (scramble)
	    {
	      __data_src<1,1> src(start,end);
	      unpack_subevent(eb,&subevent_info->_header,src,start);
	    }
	  else
	    {
	      __data_src<1,0> src(start,end);
	      unpack_subevent(eb,&subevent_info->_header,src,start);
	    }
	}
      else
	{
	  if (scramble)
	    {
	      __data_src<0,1> src(start,end);
	      unpack_subevent(eb,&subevent_info->_header,src,start);
	    }
	  else
	    {
	      __data_src<0,0> src(start,end);
	      unpack_subevent(eb,&subevent_info->_header,src,start);
	    }
	}
#endif
#ifdef USE_HLD_INPUT
      if (subevent_info->_swapping)
	{
	  __data_src<1,0> src(start,end);
	  unpack_subevent(eb,&subevent_info->_header,src,start);
	}
      else
	{
	  __data_src<0,0> src(start,end);
	  unpack_subevent(eb,&subevent_info->_header,src,start);
	}
#endif
#ifdef USE_RIDF_INPUT
      __data_src<0,0> src(start,end);
      unpack_subevent(eb,&subevent_info->_header,src,start);
#endif
    }
#else
  unpack_clean(eb);

#if defined(USE_PAX_INPUT) || defined(USE_GENF_INPUT) || \
    defined(USE_EBYE_INPUT_16)
  uint16 *start;
  uint16 *end;
#endif
#if defined(USE_EBYE_INPUT_32)
  uint32 *start;
  uint32 *end;
#endif

  src_event->get_data_src(start,end);

  if (src_event->_swapping)
    {
      __data_src<1> src(start,end);

      unpack_subevent(eb,&src_event->_header,src,start);
    }
  else
    {
      __data_src<0> src(start,end);

      unpack_subevent(eb,&src_event->_header,src,start);
    }
#endif

  level_dump(DUMP_LEVEL_UNPACK,"UNPACK",eb._unpack);
}

// Force instantiation
template
void ucesb_event_loop::unpack_event<event_base>(event_base &eb);
template
void ucesb_event_loop::unpack_event<sticky_event_base>(sticky_event_base &eb);

void ucesb_event_loop::force_event_data(event_base &eb
#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
					, source_event_hint_t *hints
#endif
					)
{
#if USE_THREADING || USE_MERGING
  FILE_INPUT_EVENT *src_event = (FILE_INPUT_EVENT *) eb._file_event;
#else
  FILE_INPUT_EVENT *src_event = &_file_event;
#endif

#ifdef USE_LMD_INPUT
  // First get the basic event info
  src_event->get_10_1_info();    // this may throw up (also)...
#endif

#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
  // Next thing is to localise the subevents inside the event
  src_event->locate_subevents(hints); // this may throw up...

  for (int subevent = 0; subevent < src_event->_nsubevents; subevent++)
    {
      typedef __typeof__(*src_event->_subevents) subevent_t;

      subevent_t *subevent_info = &src_event->_subevents[subevent];

      char *start;
      char *end;

      src_event->get_subevent_data_src(subevent_info,start,end);
    }
#else
#if defined(USE_PAX_INPUT) || defined(USE_GENF_INPUT) || \
    defined(USE_EBYE_INPUT_16)
  uint16 *start;
  uint16 *end;
#endif
#if defined(USE_EBYE_INPUT_32)
  uint32 *start;
  uint32 *end;
#endif

  src_event->get_data_src(start,end);
#endif
}

void init_sticky_idx()
{
#if STICKY_EVENT_IS_NONTRIVIAL
  _static_sticky_event._unpack.sticky_idx = 0;
  _static_sticky_event._unpack.__clean();
#endif
}

void set_sticky_idx(unpack_event_base& unpack)
{
#if STICKY_EVENT_IS_NONTRIVIAL
  unpack.sticky_idx = _static_sticky_event._unpack.sticky_idx;
#endif
}

void set_sticky_idx(unpack_sticky_event_base& unpack)
{
#if STICKY_EVENT_IS_NONTRIVIAL
  unpack.sticky_idx++;
#endif
}

void event_base::raw_cal_user_clean()
{
#ifndef USE_MERGING
  _raw.__clean();
  _cal.__clean();
#ifdef USER_STRUCT
  _user.__clean();
#endif
#endif
}

void sticky_event_base::raw_cal_user_clean()
{
}

void copy_eventno_sub_trig(raw_event_base &raw,
			   unpack_event_base& unpack,
			   uint32 sub_no,
			   bool last_multi_ev)
{
#if USING_MULTI_EVENTS
  raw.event_no     = unpack.event_no;
  raw.event_sub_no = sub_no;

  if (last_multi_ev)
    raw.trigger = unpack.trigger;
  else
    raw.trigger = 1;
#endif
}

void copy_eventno_sub_trig(dummy_container &raw,
			   unpack_event_base& unpack,
			   uint32 sub_no,
			   bool last_multi_ev)
{
}

int wrap_UNPACK_EVENT_USER_FUNCTION(unpack_event *unpack_ev)
{
  int multievents = 1;

#if defined(UNPACK_EVENT_USER_FUNCTION) || USING_MULTI_EVENTS
  multievents = UNPACK_EVENT_USER_FUNCTION(unpack_ev);
#endif

#if !USING_MULTI_EVENTS
  multievents = !!multievents;
#endif

  return multievents;
}

int wrap_UNPACK_EVENT_USER_FUNCTION(unpack_sticky_event *unpack_ev)
{
  return 1;
}

void wrap_UNPACK_EVENT_END_USER_FUNCTION(unpack_event *unpack_ev)
{
#if defined(UNPACK_EVENT_END_USER_FUNCTION)
  UNPACK_EVENT_END_USER_FUNCTION(unpack_ev);
#endif
}

void wrap_UNPACK_EVENT_END_USER_FUNCTION(unpack_sticky_event *unpack_ev)
{
}

void wrap_RAW_EVENT_USER_FUNCTION(unpack_event *unpack_ev,
                                  raw_event    *raw_ev
                                  MAP_MEMBERS_PARAM)
{
#ifdef RAW_EVENT_USER_FUNCTION
  RAW_EVENT_USER_FUNCTION(unpack_ev,
                          raw_ev
                          MAP_MEMBERS_ARG);
#endif
}

void wrap_RAW_EVENT_USER_FUNCTION(unpack_sticky_event *unpack_ev,
				  raw_sticky          *raw_ev
				  MAP_MEMBERS_PARAM)
{
}

void wrap_CAL_EVENT_USER_FUNCTION(unpack_event *unpack_ev,
				  raw_event    *raw_ev,
				  cal_event    *cal_ev
#ifdef USER_STRUCT
				  ,USER_STRUCT *user_ev
#endif
				  MAP_MEMBERS_PARAM)
{
#ifdef CAL_EVENT_USER_FUNCTION
  CAL_EVENT_USER_FUNCTION(unpack_ev,
			  raw_ev,
			  cal_ev
#ifdef USER_STRUCT
			  ,user_ev
#endif
			  MAP_MEMBERS_ARG);
#endif
}

void wrap_CAL_EVENT_USER_FUNCTION(unpack_sticky_event *unpack_ev,
				  raw_sticky          *raw_ev,
				  dummy_container     *cal_ev
#ifdef USER_STRUCT
				  ,dummy_container    *user_ev
#endif
				  MAP_MEMBERS_PARAM)
{
}

void wrap_paw_ntuple_event(event_base &eb_dummy)
{
#if defined(USE_CERNLIB) || defined(USE_ROOT) || defined(USE_EXT_WRITER)
  _paw_ntuple->event(PAW_NTUPLE_NORMAL_EVENT);
#endif
}

void wrap_paw_ntuple_event(sticky_event_base &eb_dummy)
{
#if defined(USE_CERNLIB) || defined(USE_ROOT) || defined(USE_EXT_WRITER)
  _paw_ntuple->event(PAW_NTUPLE_STICKY_EVENT);
#endif
}

template<typename T_event_base>
bool ucesb_event_loop::handle_event(T_event_base &eb,int *num_multi)
{
  int multievents = 1;

  set_sticky_idx(eb._unpack);

#if defined(USE_EXT_WRITER)
  if (!_ext_source)
#endif
    {
      multievents = wrap_UNPACK_EVENT_USER_FUNCTION(&eb._unpack);
    }

  try {
#ifndef USE_MERGING
#ifdef USE_LMD_INPUT
  if (_ts_align_hist)
    {
      uint64_t timestamp;
      ssize_t ts_align_index;

#if USE_THREADING || USE_MERGING
      FILE_INPUT_EVENT *src_event = (FILE_INPUT_EVENT *) eb._file_event;
#else
      FILE_INPUT_EVENT *src_event = &_file_event;
#endif

      bool good_stamp =
	get_timestamp(_ts_align_hist->get_style(), src_event,
		      &timestamp, &ts_align_index);

      if (!good_stamp)
	timestamp = 0;

      // TODO: if (!good_stamp) { do we want to account 0 at all? }
      _ts_align_hist->account(ts_align_index, timestamp);
    }
#endif
  for (int mev = 0; mev < multievents; mev++)
    {
      eb.raw_cal_user_clean();

#if defined(USE_EXT_WRITER)
      if (_ext_source)
	{
	  unpack_clean(eb);
	  _ext_source->unpack_event();

	  if (_dump_request)
	    {
	      printf ("\nEvent%s%12u%s Trigger %s%2d%s\n\n",
		      CT_OUT(BOLD_BLUE),
		      eb._unpack.event_no,
		      CT_OUT(NORM_DEF_COL),
		      CT_OUT(BOLD_BLUE),
		      eb._unpack.trigger,
		      CT_OUT(NORM_DEF_COL));
	    }

	  if (_conf._reverse)
	    {
	      level_dump(DUMP_LEVEL_RAW,"RAW",eb._raw);

#if USING_MULTI_EVENTS
	      map_members_info map_info;

	      map_info._multi_event_no = /*mev*/0;
	      map_info._event_type = 0;

	      do_raw_reverse_map(&eb._raw, map_info);
#else
	      do_raw_reverse_map(&eb._raw);
#endif

	      // Pack into event structures!

	      // do_unpack_map();
	    }

	  level_dump(DUMP_LEVEL_UNPACK,"UNPACK",eb._unpack);

	  if (_conf._reverse)
	    goto map_process_done;
	}
#endif

      /* Map(copy) data from unpack structure to raw event structure.
       */

#if USING_MULTI_EVENTS
      map_members_info map_info;

      map_info._multi_event_no = mev;
      map_info._event_type = 0;

      if (mev == 0)
	map_info._event_type |= MAP_MEMBER_TYPE_MULTI_FIRST;
      if (mev == multievents-1)
	map_info._event_type |= MAP_MEMBER_TYPE_MULTI_LAST;
      if (!(map_info._event_type & (MAP_MEMBER_TYPE_MULTI_FIRST |
				    MAP_MEMBER_TYPE_MULTI_LAST)))
	map_info._event_type |= MAP_MEMBER_TYPE_MULTI_OTHER;

      do_unpack_map(&eb._unpack, map_info);

      copy_eventno_sub_trig(eb._raw, eb._unpack,
			    mev+1,
			    mev == multievents-1);
#else
      do_unpack_map(&eb._unpack);
#endif

      wrap_RAW_EVENT_USER_FUNCTION(&eb._unpack,&eb._raw
#if USING_MULTI_EVENTS
				   ,map_info
#endif
				   );

      level_dump(DUMP_LEVEL_RAW,"RAW",eb._raw);

      do_calib_map(&eb._raw);

      wrap_CAL_EVENT_USER_FUNCTION(&eb._unpack,&eb._raw,&eb._cal
#ifdef USER_STRUCT
				   ,&eb._user
#endif
#if USING_MULTI_EVENTS
				   ,map_info
#endif
				   );

      level_dump(DUMP_LEVEL_CAL,"CAL",eb._cal);

#ifdef USER_STRUCT
      level_dump(DUMP_LEVEL_USER,"USER",eb._user);
#endif

    map_process_done:

#ifdef USE_CURSES
      if (_conf._watcher._command)
	{
#if USING_MULTI_EVENTS
	  watcher_one_event(&eb._unpack, map_info);
#else
	  watcher_one_event(&eb._unpack);
#endif
	}
#endif

#if USING_MULTI_EVENTS
      correlation_event(&eb._unpack, map_info);
#else
      correlation_event(&eb._unpack);
#endif

#if defined(USE_CERNLIB) || defined(USE_ROOT) || defined(USE_EXT_WRITER)
      if (_paw_ntuple)
	{
	  try {
#if defined(USE_LMD_INPUT)
	    if (_paw_ntuple->_raw_select)
	      {
		/* Set up the raw data, if any. */
#if USE_THREADING || USE_MERGING
		FILE_INPUT_EVENT *src_event =
		  (FILE_INPUT_EVENT *) eb._file_event;
#else
		FILE_INPUT_EVENT *src_event = &_file_event;
#endif
		_paw_ntuple->_raw_event->clear();
		_paw_ntuple->_raw_event->copy(src_event,
					      _paw_ntuple->_raw_select,
					      false, false);
	      }
#endif
	    /* Produce the event. */
	    wrap_paw_ntuple_event(eb);
	  } catch (error &e) {
	    wrap_UNPACK_EVENT_END_USER_FUNCTION(&eb._unpack);
	    *num_multi = mev;
	    return false;
	  }
	}
#endif
    }
#endif//!USE_MERGING
  } catch (error &e) {
    wrap_UNPACK_EVENT_END_USER_FUNCTION(&eb._unpack);
    throw;
  }

  wrap_UNPACK_EVENT_END_USER_FUNCTION(&eb._unpack);

  *num_multi = multievents;
  return true;
}

// Force instantiation
template
bool ucesb_event_loop::handle_event<event_base>(event_base &eb,int *num_multi);
template
bool ucesb_event_loop::handle_event<sticky_event_base>(sticky_event_base &eb,int *num_multi);


/*
unpack_event the_event;
raw_event the_raw_event;
*/

#if 0

#define EVENT_TASK_UNPACK            1
#define EVENT_TASK_UNPACK_USER       2
#define EVENT_TASK_UNPACK_MAP_RAW    3
#define EVENT_TASK_RAW_USER          4
#define EVENT_TASK_RAW_MAP_CAL       5
#define EVENT_TASK_CAL_USER          6
#define EVENT_TASK_NTUPLE            7
#define EVENT_TASK_RETIRE            8


void ucesb_event_loop::handle_event_tasks(int task)
{

  switch (task)
    {
    case EVENT_TASK_UNPACK:



#ifdef EVENT_TASK_UNPACK_USER
      // if the next task is a task of it's own, we stop here,
      // otherwise we go directly into it
      break;
    case EVENT_TASK_UNPACK_USER:
#endif








    }
}
#endif

void report_open_close(bool open, const char* filename,
		       uint64 size, uint32 events)
{
  // Call user function
#ifdef OPEN_CLOSE_USER_FUNCTION
  OPEN_CLOSE_USER_FUNCTION(open, _cur_filename,
			   _cur_size, _cur_events);
#endif
}
