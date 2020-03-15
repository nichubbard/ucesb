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

#ifndef __EVENT_LOOP_HH__
#define __EVENT_LOOP_HH__

#include "lmd_input.hh"
#include "lmd_output.hh"
#include "lmd_output_tcp.hh"
#include "pax_input.hh"
#include "genf_input.hh"
#include "ebye_input.hh"
#include "hld_input.hh"
#include "ridf_input.hh"

#ifdef USE_INPUTFILTER
#include "lmd_source_multievent.hh"
#endif

#include "config.hh"
#include "thread_param.hh"

#include <set>
#include <vector>
#include <queue>
#include <functional>

/*---------------------------------------------------------------------------*/

#ifdef USE_LMD_INPUT
#define TIMESTAMP_TYPE_TITRIS             1
#define TIMESTAMP_TYPE_WR                 2
#endif

#ifdef USE_MERGING
class event_base;
union merge_event_order;

#define MERGE_EVENTS_ERROR_ORDER_BEFORE   1
#define MERGE_EVENTS_ERROR_ORDER_SAME     2

#ifdef USE_LMD_INPUT
#define MERGE_EVENTS_MODE_TITRIS_TIME     TIMESTAMP_TYPE_TITRIS
#define MERGE_EVENTS_MODE_WR_TIME         TIMESTAMP_TYPE_WR
#define MERGE_EVENTS_MODE_EVENTNO         3
#endif
#define MERGE_EVENTS_MODE_USER            4 // if MERGE_COMPARE_EVENTS_AFTER

struct source_event_base
{
  lmd_source *_src;
  event_base *_event;
  uint64_t    _timestamp; // for MERGE_EVENTS_MODE_TITRIS_TIME

  uint64_t    _events;           // for display
  uint64_t    _events_last_show; // for display

  size_t         _tstamp_align_index;

  const char *_name;      // for debug
};

void do_merge_prepare_event_info(source_event_base *x);
bool do_merge_compare_events_after(const source_event_base *x,
				   const source_event_base *y);
int do_check_merge_event_after(const merge_event_order *prev,
			       const source_event_base *x);
merge_event_order *
do_record_merge_event_info(merge_event_order *info,
			   const source_event_base *x);
void print_current_merge_order(const merge_event_order *prev);

typedef std::vector<source_event_base*> vect_source_event_base;

struct less_source_event_no //: 
//  public binary_function<source_event_base*, source_event_base*, bool> 
{
  bool operator()(source_event_base* x, source_event_base* y) 
  { 
    return do_merge_compare_events_after(x,y);
  }
};
#endif

#ifdef USE_LMD_INPUT
struct output_info
{
  lmd_output_buffered *_dest;
  // Something to select what subevents to use also...

  // The accumulated output data
  lmd_event_out _event;
};
#endif

#ifdef USE_LMD_INPUT
#define EVENT_STITCH_MODE_TITRIS_TIME   TIMESTAMP_TYPE_TITRIS
#define EVENT_STITCH_MODE_WR_TIME       TIMESTAMP_TYPE_WR
#endif

struct stitch_info
{
  uint64_t _last_stamp;
  bool     _combine;
  bool     _badstamp;
};

class paw_ntuple;

class ucesb_event_loop
{
public:
  ucesb_event_loop();
  ~ucesb_event_loop();

public:
#ifdef USE_LMD_INPUT
#ifdef USE_MERGING
  vect_source_event_base _sources;
  vect_source_event_base _sources_need_event;
  std::priority_queue<source_event_base*,
		      vect_source_event_base,
		      less_source_event_no> _sources_next_event;
#else
#ifdef USE_INPUTFILTER	
  lmd_source_multievent _source;
#else
  lmd_source _source;
#endif
#endif
  typedef lmd_event_hint source_event_hint_t;
  std::vector<output_info> _output;
  lmd_output_file *_file_output_bad;
#endif
#ifdef USE_PAX_INPUT
  pax_source  _source;
#endif
#ifdef USE_GENF_INPUT
  genf_source _source;
#endif
#ifdef USE_EBYE_INPUT
  ebye_source _source;
#endif
#ifdef USE_HLD_INPUT
  hld_source  _source;
  typedef hld_event_hint source_event_hint_t;
#endif
#ifdef USE_RIDF_INPUT
  ridf_source _source;
  typedef ridf_event_hint source_event_hint_t;
#endif
#ifdef USE_EXT_WRITER
  paw_ntuple *_ext_source;
#endif

#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
  source_event_hint_t _source_event_hint;
#endif

public:
  void preprocess();
  void postprocess();

public:
  void open_source(config_input &input,
		   input_buffer **file_input
		   PTHREAD_PARAM(thread_block *block_reader) );
#ifdef USE_MERGING
  void close_source(source_event_base*);
  void close_sources();
#else
  void close_source();
#endif
  void close_output();

protected:
  template<typename source_type>
  void open_source(source_type &source,
#if defined(USE_EXT_WRITER)
		   paw_ntuple *&ext_source,
#endif
		   config_input &input,
		   input_buffer **file_input
		   PTHREAD_PARAM(thread_block *block_reader) );
public:
  static void pre_unpack_event(event_base &eb
#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
			       , source_event_hint_t *hints
#endif
			       );

  static void stitch_event(event_base &eb,
			   stitch_info *stitch);

  static void unpack_event(event_base &eb);
  // the following is used before error printing, to ensure that
  // whatever data is available, is available unfragmented.
  static void force_event_data(event_base &eb
#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
			       , source_event_hint_t *hints
#endif
			       );

  bool handle_event(event_base &eb,int *num_multi);

public:
  bool get_ext_source_event(event_base &eb);
  void close_ext_source();
};

/*---------------------------------------------------------------------------*/

void report_open_close(bool open, const char* filename,
		       uint64 size, uint32 events);

#endif//__EVENT_LOOP_HH__
