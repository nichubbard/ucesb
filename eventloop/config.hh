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

#ifndef __CONFIG_HH__
#define __CONFIG_HH__

#include <vector>

#include <stdint.h>

// Data source
struct config_input
{
  int _type;
  const char *_name;
};

struct config_output
{
  int _type;
  const char *_name;
};

struct time_slope
{
  time_slope(): proc(-1), ctrl(-1), crate(-1), tsid(-1), mult(-1), add(-1) {}
  int proc;
  int ctrl;
  int crate;
  int tsid;
  int mult;
  int add;
};

typedef std::vector<config_input> config_input_vect;

typedef std::vector<config_output> config_output_vect;

typedef std::vector<char *> config_calib_vect;

typedef std::vector<char *> config_command_vect;

struct config_opts
{
  // General info control
  int _debug;
  int _quiet;
  int _monitor_port;

  int _io_error_fatal;

  int _allow_errors;
  int _broken_files;

  int _print_buffer;
  int _print; // Print events
  int _data;  // Print event data
  int _reverse;

  int _show_members;
  int _event_sizes;
  int _show_calib;

  char const *_ts_align_hist_command;

  int64_t _max_events;
  int64_t _skip_events;
  int64_t _first_event;
  int64_t _last_event;
  int64_t _downscale;

#ifdef USE_LMD_INPUT
  int _scramble;
#endif
  uint64_t _input_buffer;

#ifdef USE_INPUTFILTER
  int _enable_eventbuilder;
  int _eventbuilder_procid;
  int _eventbuilder_wrid;
  int64_t _eventbuilder_window;
#endif

#ifdef USE_LMD_INPUT
  int _event_stitch_mode;
  int _event_stitch_value;
#endif

#ifdef USE_LMD_INPUT
  // Data dest
  struct
  {
    const char *_name; // NULL if inactive
  } _file_output_bad;
#endif

  struct
  {
    const char *_command; // NULL if inactive
  } _paw_ntuple;

  struct
  {
    const char *_command; // NULL if inactive
  } _watcher;

  struct
  {
    const char *_command; // NULL if inactive
  } _dump;

  int _num_threads;
  int _progress;

  int _files_open_ahead;

#ifdef USE_MERGING
  int _merge_concurrent_files;
  int _merge_event_mode;
#endif

  int _no_mmap;
};

extern config_opts _conf;

extern config_input_vect _inputs;

extern config_output_vect _outputs;

extern config_calib_vect _conf_calib;

extern config_command_vect _corr_commands;

#ifdef USE_LMD_INPUT
extern std::vector<time_slope> _conf_time_slope_vector;
#endif

#endif /* __CONFIG_HH__ */
