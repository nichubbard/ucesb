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

#include "event_loop.hh"
#include "config.hh"
#include "monitor.hh"
#include "error.hh"
#include "colourtext.hh"
#include "parse_util.hh"
#include "format_prefix.hh"

#include "mc_def.hh"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <math.h>


#include "util.hh"
#include "worker_thread.hh"
#include "watcher.hh"
#include "tstamp_alignment.hh"

#include "event_base.hh"

#include "thread_info_window.hh"
#include "thread_queue.hh"
#include "thread_block.hh"

#include "open_retire.hh"
#include "event_reader.hh"
#include "event_processor.hh"

#include "data_queues.hh"

#include "user.hh"

void signal_handler_ret(int signal)
{
  UNUSED(signal);
}

void usage()
{
  printf ("\n");
  printf ("Read and analyse data from DAQ (%s)\n",stringify(UNPACKERNAME));
  printf ("\n");
  printf ("%s options\n\n", main_argv0);

  //printf ("  --tcal=T,L     Perform time calibration\n"
  //	  "                 (pulses with period T ns, last at L ns)");

  printf ("  file://SRC        Read from file SRC (-=stdin).\n");
#ifdef USE_RFIO
  printf ("  rfio://HOST:SRC   Read from rfio file SRC from HOST.\n");
#else
  printf (" (rfio://)          No support for RFIO compiled in.\n");
#endif
#if defined(USE_LMD_INPUT)
  printf ("  event://HOST      Read from event server HOST.\n");
  printf ("  stream://HOST     Read from stream server HOST.\n");
  printf ("  trans://HOST      Read from transport HOST.\n");
  printf ("  --scramble        Toggle scrambling of data.\n");
#else
  printf (" (stream,event,trans://)  No MBS input support compiled in.\n");
#endif
  printf ("  --input-buffer=N  Input buffer size.\n");
#if defined(USE_EXT_WRITER) && !defined(USE_MERGING)
  printf ("  --in-tuple=LVL,DET,FILE  Read data from ROOT/STRUCT.\n");
  //printf ("  --reverse         Run mapping in reverse (only with --in-tuple).\n");
#else
  printf (" (--in-tuple)       No external reader support compiled in.\n");
#endif
#ifdef USE_INPUTFILTER
  printf ("  --aida             Enable AIDA event builder (splitting, sorting)\n");
  printf ("  --aida-window=N    Gap between AIDA items to break events. Default: 2200 (2.2 us)\n");
  printf ("  --aida-ids=P,W     The ProcID (P) and WR ID (W) for AIDA events. Default: 90, 700\n");
  printf ("  --aida-old-stitch  Use the older method of time-stitching AIDA (ignore length of AIDA event)\n");
  printf ("  --aida-skip-decays Don't output AIDA decay events (for near-line speedup)\n");
  printf ("  --dtas             Also split DTAS events\n");
#else
  printf (" (--aida)           No support for AIDA event building compiled in.\n");
#endif
#ifdef USE_MERGING
  printf ("  --merge=style,N   Merge events (in order) from N files, sort by style:\n");
  printf ("                    wr, titris, eventno, or user.  (use N>=2*num EB)\n");
#else
  printf (" (--merge)          No support for overlapping sources compiled in.\n");
#endif
#if defined(USE_LMD_INPUT)
  printf ("  --time-stitch=style,N   Combine events with timestamps with difference <= N,\n"
	  "                          style: wr, titris.\n");
  printf ("  --time-slope=[help],[filter],[mult],[add]\n"
          "                    Transform timestamps before they are evaluated.\n");
  printf ("  --tstamp-hist=[help],[style],[props]\n");
  printf ("                    Histogram of time stamp diffs, style: wr, titris.\n");
#endif
  printf ("  --calib=FILE      Extra input file with mapping/calibration parameters.\n");

  printf ("  --max-events=N    Limit number of events processed to N.\n");
  printf ("  --skip-events=N   Skip initial N events.\n");
  printf ("  --first-event=N   Skip initial events until event # N.\n");
  printf ("  --last-event=N    Stop processing at (before) event # N.\n");
  printf ("  --downscale=N     Only process every Nth event.\n");
#if 0
  printf ("  --rate=real|NHz   Process events at original or given rate.\n");
#endif
  printf ("  --print-buffer    Print buffer headers.\n");
  printf ("  --print           Print event headers.\n");
  printf ("  --data            Print event data.\n");
  printf ("  --debug           Print events causing errors.\n");
  printf ("  --colour=yes|no   Force colour and markup on or off.\n");
  printf ("  --event-sizes     Show average sizes of events and subevents.\n");
  printf ("  --data-sizes      Show data size usage by data members.\n");

#if defined(USE_EXT_WRITER)
  printf ("  --monitor[=PORT]  Status information server.\n");
#else
  printf (" (--monitor)        No information server compiled in.\n");
#endif
  printf ("  --quiet           Suppress harmless problem reports.\n");
  printf ("  --io-error-fatal  Any I/O error is fatal.\n");
  printf ("  --allow-errors    Allow errors.\n");
  printf ("  --broken-files    Allow errors again after bad files.\n");
  printf ("  --help            Print this usage information and quit.\n");

  // printf ("  --thresholds      Calculate thresholds.\n");

#if defined(USE_LMD_INPUT)
  printf ("  --output=OPT,FILE  Save events in LMD file (native/net/big/little,size=nM).\n");
  printf ("  --bad-events=FILE  Save events with unpack errors in LMD file.\n");
# if defined(USE_LMD_OUTPUT_TCP)
  printf ("  --server=OPT      Data server (stream:port,trans:port,size=nM,hold).\n");
# else
  printf (" (--server)         No support for data server.\n");
# endif
#else
  printf (" (--output,bad-events,server)  No support for writing data.\n");
#endif

#ifndef USE_MERGING
  printf ("  --show-members    Show members of all data structures.\n");
  printf ("  --show-calib      Show calibration parameters.\n");
#endif
#if defined(USE_CERNLIB) || defined (USE_ROOT) || defined(USE_EXT_WRITER)
  printf ("  --ntuple=LVL,DET,FILE  Dump data as "
# if !defined(USE_ROOT)
	  "PAW"
# elif !defined(USE_CERNLIB)
	  "ROOT"
# else
	  "PAW/ROOT"
# endif
	  " ntuple.\n");
#else
  printf (" (--ntuple)         No PAW/ROOT ntuples support compiled in.\n");
#endif
#ifdef USE_CURSES
  printf ("  --watcher=DET     Do ncurses-based data viewing.\n");
#else
  printf (" (--watcher)        No ncurses support compiled in.\n");
#endif
  printf ("  --corr=TRIG,DET,FILE  Create 2D correlation plot.\n");
  printf ("  --dump=LVL        Text dump of data from data structures.\n");
#ifdef USE_THREADING
  printf ("  --threads=N       Number of worker threads.\n");
  printf ("  --files-ahead=N   Number of files to buffer ahead.\n");
#else
  printf (" (--threads)        No threading support compiled in.\n");
  printf (" (--files-ahead)    No threading support compiled in.\n");
#endif
#ifdef USE_CURSES
  printf ("  --progress        Do ncurses-based thread monitoring.\n");
#else
  printf (" (--progress)       No ncurses support compiled in.\n");
#endif
#ifdef USAGE_COMMAND_LINE_OPTIONS
  printf ("\n");
  USAGE_COMMAND_LINE_OPTIONS();
#endif
  printf ("\n");
  printf ("Note: most options offer additional usage info when passed the argument 'help'.\n");
  printf ("\n");
  /*
  printf ("Parse order: all output options will be effective from start,\n"
	  "input (files) are treated in order.");
  */
}

status_monitor _status;

format_diff_info _ev_diff_info;
format_diff_info _evr_diff_info;
format_diff_info _mev_diff_info;
format_diff_info _mevr_diff_info;
format_diff_info _err_diff_info;

config_opts _conf;

// Data source
config_input_vect _inputs;

config_output_vect _outputs;

config_calib_vect _conf_calib;

config_command_vect _corr_commands;

#ifdef USE_LMD_INPUT
std::vector<time_slope> _conf_time_slope_vector;
#endif

thread_info _ti_info;

thread_info_window *_ti_info_window = NULL;

#ifdef USE_THREADING
open_retire     _open_retire_thread;
event_reader    _event_reader_thread;
event_processor _event_processor_threads[MAX_THREADS];
#else
# ifdef USE_PTHREAD
thread_block    _block_main;
# endif
#endif

int _got_sigint = 0;

void sigint_handler(int sig)
{
  _got_sigint++;

  if (_got_sigint > 1)
    {
      #ifdef USE_CURSES
      endwin();
      #endif
      printf ("Got many SIGINT requests, next will go through\n");
      signal(SIGINT,SIG_DFL);
    }
  else
    {
#ifdef USE_MERGING
      // So we do not overwrite the speed line...  Is nice to have :-)
      if (_conf._merge_concurrent_files > 1)
	fprintf (stderr, "\n");
#endif

      printf ("Termination request, setting max events to 1...\n");
      _conf._max_events = 1;
    }
}

volatile int _update_progress = 0;

void sigalarm_handler(int sig)
{
  _update_progress = 1;
}


/********************************************************************/

#ifdef USE_LMD_INPUT
/* Hack: should not be here - clean me up! */

extern tstamp_alignment *_ts_align_hist;
#endif

/********************************************************************/

bool add_input_try_follow_link(const char *path,
			       config_input &input, bool match_opt_dashes)
{
  const char *post;

  /* As we might have been called recursively by ourselves, path
   * is not persistent, so need to do strdup. */

#define MATCH_PATH_PREFIX(prefix,post) (strncmp(path,prefix,strlen(prefix)) == 0 && *(post = path + strlen(prefix)) != '\0')

  if ((match_opt_dashes &&
       MATCH_PATH_PREFIX("--file=",post)) ||
      MATCH_PATH_PREFIX("file://",post)) {
    input._type = INPUT_TYPE_FILE;
    input._name = strdup(post);
    return true;
  }
#ifdef USE_RFIO
  if ((match_opt_dashes &&
       MATCH_PATH_PREFIX("--rfio=",post)) ||
	   MATCH_PATH_PREFIX("rfio://",post)) {
    input._type = INPUT_TYPE_RFIO;
    input._name = strdup(post);
    return true;
  }
  if (MATCH_PATH_PREFIX("gstore:",post/*dummy*/))
    {
      input._type = INPUT_TYPE_RFIO;
      input._name = strdup(path);
      return true;
    }
#endif
  if ((match_opt_dashes &&
       MATCH_PATH_PREFIX("--srm=",post)) ||
      MATCH_PATH_PREFIX("srm://",post)) {
    input._type = INPUT_TYPE_FILE_SRM;
    input._name = strdup(path);
    return true;
  }
#ifdef USE_LMD_INPUT
  if ((match_opt_dashes &&
       MATCH_PATH_PREFIX("--event=",post)) ||
      MATCH_PATH_PREFIX("event://",post)) {
    input._type = LMD_INPUT_TYPE_EVENT; // "MBS event server"
    input._name = strdup(post);
    return true;
  }
  if ((match_opt_dashes &&
       MATCH_PATH_PREFIX("--stream=",post)) ||
      MATCH_PATH_PREFIX("stream://",post)) {
    input._type = LMD_INPUT_TYPE_STREAM; // "MBS stream server"
    input._name = strdup(post);
    return true;
  }
  if ((match_opt_dashes &&
       MATCH_PATH_PREFIX("--trans=",post)) ||
      MATCH_PATH_PREFIX("trans://",post)) {
    input._type = LMD_INPUT_TYPE_TRANS; // "MBS transport"
    input._name = strdup(post);
    return true;
  }
#endif//USE_LMD_INPUT

  struct stat buf;

  /* Test if it is a file name.
   */

  if (stat(path,&buf) == 0)
    {
      input._type = INPUT_TYPE_FILE; // "file"
      input._name = strdup(path);
      return true;
    }

  /* Stat()ing the file has failed.  We try to follow the symbolic
   * links as far as possible.  If we find a dangling link, we try to
   * interpret it as a resource locator, like 'srm://...'
   */

  char *dest = NULL;
  ssize_t n;

  // printf ("Follow: %s\n", path);

  if (lstat(path, &buf) == -1)
    goto failure;

  dest = (char *) malloc((size_t) buf.st_size + 1);
  if (dest == NULL)
    goto failure;

  n = readlink(path, dest, (size_t) buf.st_size + 1);

  if (n < 0)
    goto failure;

  if (n > buf.st_size)
    goto failure;

  dest[buf.st_size] = 0;

  // printf ("Points to: %s\n", dest);

  if (add_input_try_follow_link(dest, input, false))
    {
      free(dest);
      return true;
    }

  WARNING("Symlink '%s' -> '%s'.", path, dest);

 failure:
  free(dest);
  return false;
}

/********************************************************************/

#ifdef USE_LMD_INPUT
int get_time_stamp_mode(const char *mode)
{
  if (strcmp(mode,"titris") == 0) {
    return TIMESTAMP_TYPE_TITRIS;
  }
  else if (strcmp(mode,"wr") == 0) {
    return TIMESTAMP_TYPE_WR;
  }

  return -1;
}

void parse_time_stitch_options(const char *command)
{
  const char *cmd = command;
  const char *req_end;

  while ((req_end = strchr(cmd,',')) != NULL)
    {
      char *request = strndup(cmd,(size_t) (req_end-cmd));
      int mode;

      if ((mode = get_time_stamp_mode(request)) != -1)
	_conf._event_stitch_mode = mode;
      else {
	ERROR("Unknown time stitch option: %s",request);
      }

      free(request);
      cmd = req_end+1;
    }

  // and handle the remainder

  _conf._event_stitch_value = atoi(cmd);
  if (!_conf._event_stitch_value)
    ERROR("Cannot have delta=0 time-stitch.");
  if (!_conf._event_stitch_mode)
    ERROR("Time stamp style not specified.");
}

#ifdef USE_INPUTFILTER
void parse_aida_ids(const char *command)
{
  const char *cmd = command;
  const char *req_end;

  while ((req_end = strchr(cmd,',')) != NULL)
    {
      char *request = strndup(cmd,(size_t) (req_end-cmd));

      _conf._eventbuilder_procid = atoi(request);


      free(request);
      cmd = req_end+1;
    }

  // and handle the remainder

  _conf._eventbuilder_wrid = (int)strtol(cmd, NULL, 16);
  if (!_conf._eventbuilder_wrid)
    ERROR("Cannot have AIDA WR ID = 0.");
  if (!_conf._eventbuilder_procid)
    ERROR("Cannot have AIDA PrcID = 0");
}
#endif


void parse_time_slope_usage()
{
  printf ("\n");
  printf ("%s --time-slope=[help],[filter],[mult],[add]\n", main_argv0);
  printf ("\n");
  printf ("Every timestamped 10/1 sub-event that matches [filter] will have its timestamp\n");
  printf ("transformed as \"t' = mult * t + add\" before being evaluated for merging or time\n");
  printf ("stitching. An error will be emitted if a sub-event matches several given\n");
  printf ("time-slopes.\n");
  printf ("\n");
  printf ("[filter] is a comma-separated list of the following:\n");
  printf (" proc=num  - matches against sub-event procid.\n");
  printf (" ctrl=num  - matches against sub-event control.\n");
  printf (" crate=num - matches against sub-event sub-crate.\n");
  printf (" tsid=num  - matches against timestamp ID in sub-event payload.\n");
  printf (" mult=num  - timestamp multiplier, 1 if omitted.\n");
  printf (" add=num   - timestamp additive offset, 0 if omitted.\n");
  printf ("Note that at least one of \"mult\" or \"add\" must be specified.\n");
  printf ("\n");
  printf ("Example:\n");
  printf (" --time-slope=proc=12,ctrl=5,tsid=0x200,add=100\n");
  printf ("For every 10/1 subevent with procid=12, control=5, timestamp ID=0x200 in the\n");
  printf ("payload, the timestamp will be transformed as \"t'=t+100\".\n");
  printf ("\n");
}

void parse_time_slope_options(const char *command)
{
  time_slope ts;
  char const *p = command;
  for (;;) {
    char *end;
    if (strncmp(p, "help", 4) == 0) {
      parse_time_slope_usage();
      exit(EXIT_SUCCESS);
    }
#define TIME_SLOPE_COMPONENT(name)\
    do {\
      char const *opt = #name"=";\
      size_t optlen = strlen(opt);\
      if (strncmp(p, opt, optlen) == 0) {\
        p += optlen;\
        int base = 10;\
        if (strncmp(p, "0x", 2) == 0) {\
          base = 16;\
          p += 2;\
        }\
        int value = (int)strtol(p, &end, base);\
        if (p == end) {\
          ERROR("Invalid number for \""#name"\" in time-slope \"%s\".", command);\
        }\
        ts.name = value;\
        goto parse_time_slope_options_next;\
      }\
    } while (0)
    TIME_SLOPE_COMPONENT(proc);
    TIME_SLOPE_COMPONENT(ctrl);
    TIME_SLOPE_COMPONENT(crate);
    TIME_SLOPE_COMPONENT(tsid);
    TIME_SLOPE_COMPONENT(mult);
    TIME_SLOPE_COMPONENT(add);
    ERROR("Unknown time-slope option in \"%s\".", command);
parse_time_slope_options_next:
    p = end;
    if (*p == '\0') {
      break;
    }
    if (*p == ',') {
      ++p;
      continue;
    }
    ERROR("Garbage in time-slope \"%s\".", command);
  }
  if (ts.mult == -1 && ts.add == -1) {
    ERROR("Time-slope has no modifiers \"%s\".", command);
  }
  _conf_time_slope_vector.push_back(ts);
}
#endif

/********************************************************************/

#ifdef USE_MERGING
void parse_merge_options(const char *command)
{
  const char *cmd = command;
  const char *req_end;

  while ((req_end = strchr(cmd,',')) != NULL)
    {
      char *request = strndup(cmd,req_end-cmd);
      int mode;

      if (strcmp(request,"eventno") == 0) {
	_conf._merge_event_mode = MERGE_EVENTS_MODE_EVENTNO;
      }
      else if ((mode = get_time_stamp_mode(request)) != -1)
	_conf._merge_event_mode = mode;
#ifdef MERGE_COMPARE_EVENTS_AFTER
      else if (strcmp(request,"user") == 0) {
	_conf._merge_event_mode = MERGE_EVENTS_MODE_USER;
      }
#endif
      else {
	ERROR("Unknown merge option: %s",request);
      }

      free(request);
      cmd = req_end+1;
    }

  // and handle the remainder

  _conf._merge_concurrent_files = atoi(cmd);

  if (!_conf._merge_event_mode)
    ERROR("Merge style not specified.");
}
#endif

/********************************************************************/

int main(int argc, char **argv)
{
  //INTS4 ll,l_evts=0;

  main_argv0 = argv[0];

  memset (&_conf,0,sizeof(_conf));
  _conf._max_events = -1;
  _conf._first_event = -1;
  _conf._last_event = -1;

  if(argc == 1)
    {
      usage();
      exit(0);
    }

#ifdef USE_THREADING
  _open_retire_thread.thread_init();
  _wt.init();
#else
# ifdef USE_PTHREAD
#  ifndef HAVE_THREAD_LOCAL_STORAGE
  fe_init();
#  endif
# endif
#endif

  colourtext_init();
  /******************************************************************/

#if USE_INPUTFILTER
  // Default IDs
  _conf._eventbuilder_procid = 90;
  _conf._eventbuilder_wrid = 0x700;
  _conf._eventbuilder_window = 2200;
  _conf._aida_new_stitch = true;
  _conf._aida_skip_decays = false;
#endif
  for (int i = 1; i < argc; i++)
    {
      char *post;

#define MATCH_PREFIX(prefix,post) (strncmp(argv[i],prefix,strlen(prefix)) == 0 && *(post = argv[i] + strlen(prefix)) != '\0')
#define MATCH_ARG(name) (strcmp(argv[i],name) == 0)

      if (MATCH_ARG("--help")) {
	usage();
	exit(0);
      }
#ifdef USE_LMD_INPUT
      else if (MATCH_PREFIX("--output=",post)) {
	config_output output;
	output._type = INPUT_TYPE_FILE;
	output._name = post;
	_outputs.push_back(output);
      }
# ifdef USE_LMD_OUTPUT_TCP
      else if (MATCH_PREFIX("--server=",post)) {
        config_output output;
        output._type = LMD_INPUT_TYPE_STREAM;
        output._name = post;
        _outputs.push_back(output);
      }
      else if (MATCH_ARG("--server")) {
	config_output output;
	output._type = LMD_INPUT_TYPE_STREAM;
	output._name = "";
	_outputs.push_back(output);
      }
# endif//USE_LMD_OUTPUT_TCP
      else if (MATCH_PREFIX("--bad-events=",post)) {
	_conf._file_output_bad._name = post;
      }
      else if (MATCH_ARG("--scramble")) {
	_conf._scramble = 1;
      }
      else if (MATCH_PREFIX("--time-stitch=",post)) {
	parse_time_stitch_options(post);
      }
      else if (MATCH_PREFIX("--time-slope=",post)) {
	parse_time_slope_options(post);
      }
      else if (MATCH_PREFIX("--tstamp-hist=",post)) {
        _conf._ts_align_hist_command = post;
      }
#endif//USE_LMD_INPUT
      else if (MATCH_PREFIX("--input-buffer=",post)) {
	_conf._input_buffer =
	  parse_size_postfix(post,"kMG","Input buffer size",false);
      }
#ifdef USE_INPUTFILTER
      else if (MATCH_ARG("--aida")) {
         _conf._enable_eventbuilder = 1;
      }
      else if (MATCH_PREFIX("--aida-window=",post)) {
         _conf._eventbuilder_window = atoi(post);
      }
      else if (MATCH_PREFIX("--aida-ids=",post)) {
         parse_aida_ids(post);
      }
      else if (MATCH_ARG("--aida-old-stitch")) {
	       _conf._aida_new_stitch = false;
      }
      else if (MATCH_ARG("--aida-skip-decays")) {
         _conf._aida_skip_decays = true;
      }
      else if (MATCH_ARG("--dtas")) {
	_conf._enable_dtas = true;
      }
      // --aida-minimum-words=2 : Minimum ADC words in an AIDA event for it to be emitted
#endif
#ifdef USE_MERGING
      else if (MATCH_PREFIX("--merge=",post)) {
	parse_merge_options(post);
      }
#endif
      else if (MATCH_PREFIX("--ntuple=",post)) {
#if defined(USE_CERNLIB) || defined (USE_ROOT) || defined(USE_EXT_WRITER)
	_conf._paw_ntuple._command = post;
#else
	ERROR("Support for ROOT or CERNLIB (or external) not compiled in.  "
	      "Compile using 'make USE_ROOT=1' "
	      "and/or using 'make CERN_ROOT=/path/to/cernlib/ USE_CERNLIB=1' "
	      "or using 'make USE_EXT_WRITER=1'");
#endif
      }
      else if (MATCH_PREFIX("--in-tuple=",post)) {
#if defined(USE_EXT_WRITER) && !defined(USE_MERGING)
	config_input input;
	input._type = INPUT_TYPE_EXTERNAL;
	input._name = post;
	_inputs.push_back(input);
#else
	ERROR("Support for external writer/reader not compiled in.  "
	      "Compile using 'make USE_EXT_WRITER=1'");
#endif
      }
#if !defined(USE_MERGING)
      else if (MATCH_ARG("--reverse")) {
	_conf._reverse = 1;
      }
#endif
      else if (MATCH_PREFIX("--calib=",post)) {
	_conf_calib.push_back(post);
      }
      else if (MATCH_PREFIX("--max-events=",post)) {
	char *end;
	_conf._max_events = strtoul(post, &end, 10);
        if (*end != 0 || end == post)
          ERROR("Invalid number for --max-events.");
      }
      else if (MATCH_PREFIX("--skip-events=",post)) {
	char *end;
	_conf._skip_events = strtoul(post, &end, 10);
        if (*end != 0 || end == post)
          ERROR("Invalid number for --skip-events.");
      }
      else if (MATCH_PREFIX("--first-event=",post)) {
	char *end;
	_conf._first_event = strtoul(post, &end, 10);
        if (*end != 0 || end == post)
          ERROR("Invalid number for --first-event.");
      }
      else if (MATCH_PREFIX("--last-event=",post)) {
	char *end;
	_conf._last_event = strtoul(post, &end, 10);
        if (*end != 0 || end == post)
          ERROR("Invalid number for --last-event.");
      }
      else if (MATCH_PREFIX("--downscale=",post)) {
	char *end;
	_conf._downscale = strtoul(post, &end, 10);
        if (*end != 0 || end == post)
          ERROR("Invalid number for --downscale.");
      }
      else if (MATCH_ARG("--print-buffer")) {
	_conf._print_buffer = 1;
      }
#ifndef USE_MERGING
      else if (MATCH_ARG("--show-members")) {
	_conf._show_members = 1;
      }
      else if (MATCH_ARG("--show-calib")) {
	_conf._show_calib = 1;
      }
#endif
      else if (MATCH_ARG("--event-sizes")) {
	_conf._event_sizes = 1;
      }
      else if (MATCH_ARG("--data-sizes")) {
	_conf._account = 1;
      }
      else if (MATCH_ARG("--print")) {
	_conf._print = 1;
      }
      else if (MATCH_ARG("--data")) {
	_conf._data = 1;
      }
      else if (MATCH_ARG("--debug")) {
	_conf._debug = 1;
      }
      else if (MATCH_ARG("--quiet")) {
	_conf._quiet++;
      }
#if defined(USE_EXT_WRITER)
      else if (MATCH_ARG("--monitor")) {
	_conf._monitor_port = EXT_WRITER_UCESB_MONITOR_DEFAULT_PORT;
      }
      else if (MATCH_PREFIX("--monitor=",post)) {
	_conf._monitor_port = atoi(post);
      }
#endif
      else if (MATCH_PREFIX("--colour=",post)) {
	int force = 0;

	if (strcmp(post,"yes") == 0) {
#ifdef USE_CURSES	  
	  force = 1;
#else
	  ERROR("No colour support since ncurses not compiled in.  "
		"Compile using 'make USE_CURSES=1'");
#endif	
	} else if (strcmp(post,"no") == 0)
	  force = -1;
	else if (strcmp(post,"auto") != 0)
	  ERROR("Bad option '%s' for --colour=",post);

	colourtext_setforce(force);


      }
      else if (MATCH_ARG("--watcher")) {
#ifdef USE_CURSES
	_conf._watcher._command = ""; // non-NULL, i.e. active
#else
	ERROR("No watcher - support for ncurses not compiled in.  Compile using 'make USE_CURSES=1'");
#endif
      }
      else if (MATCH_PREFIX("--watcher=",post)) {
#ifdef USE_CURSES
	_conf._watcher._command = post;
#else
	ERROR("No watcher - support for ncurses not compiled in.  Compile using 'make USE_CURSES=1'");
#endif
      }
      else if (MATCH_PREFIX("--corr=",post)) {
	_corr_commands.push_back(post);
      }
      else if (MATCH_PREFIX("--dump=",post)) {
	_conf._dump._command = post;
      }
      else if (MATCH_ARG("--io-error-fatal")) {
	_conf._io_error_fatal = 1;
      }
      else if (MATCH_ARG("--allow-errors")) {
	_conf._allow_errors = 1;
      }
      else if (MATCH_ARG("--broken-files")) {
	_conf._broken_files = 1;
      }
      else if (MATCH_ARG("--no-mmap")) {
	_conf._no_mmap = 1;
      }
#ifdef USE_THREADING
      else if (MATCH_PREFIX("--threads=",post)) {
	_conf._num_threads = atoi(post);
      }
      else if (MATCH_PREFIX("--files-ahead=",post)) {
        _conf._files_open_ahead = atoi(post);
      }
#endif
#ifdef USE_CURSES
      else if (MATCH_ARG("--progress")) {
	_conf._progress = 1;
      }
#endif
      else if (MATCH_ARG("--thresholds")) {
	ERROR("--thresholds flag unimplemented.");
      }
#ifdef HANDLE_COMMAND_LINE_OPTION
      else if (HANDLE_COMMAND_LINE_OPTION(argv[i])) {
      }
#endif
      else {
	config_input input;

	if (add_input_try_follow_link(argv[i], input, true))
	  {
	    _inputs.push_back(input);
	  }
	else
	  {
	    ERROR("Unrecognized or invalid option (also not a file): '%s'.",
		  argv[i]);
	  }
      }
    }

#ifdef USE_INPUTFILTER
  if(_conf._enable_eventbuilder)
  {
    INFO("AIDA Event Builder enabled, ProcID = %d and WR ID = %x, Event window = %ld ns", _conf._eventbuilder_procid, _conf._eventbuilder_wrid, _conf._eventbuilder_window);
    if (_conf._aida_skip_decays)
    {
      INFO("Only outputting AIDA implants!");
    }
    if (_conf._enable_dtas)
    {
      INFO("DTAS Event Builder enabled, ProcID = %d and WR ID = %x, Event window = %ld ns", 37, 0x800, 2000);
    }
  }
  else if (_conf._enable_dtas)
  {
    ERROR("Cannot build DTAS events without AIDA!");
  }
#endif
  /******************************************************************/

  if (_conf._watcher._command &&
      _conf._progress)
    {
      // Disable the progress window if watcher requested
      _conf._progress = 0;
    }

#ifdef USE_MERGING
  if (_conf._merge_concurrent_files < 1)
    _conf._merge_concurrent_files = 1;
  else if (_conf._skip_events > 0)
    ERROR("You can only do one of --skip-events and --merge, not both!");
  else if (_conf._first_event >= 0)
    ERROR("You can only do one of --first-event and --merge, not both!");
  else if (_conf._last_event >= 0)
    ERROR("You can only do one of --last-event and --merge, not both!");
  else if (_conf._downscale > 0)
    ERROR("You can only do one of --downscale and --merge, not both!");
  if (!_conf._merge_event_mode)
    _conf._merge_event_mode = MERGE_EVENTS_MODE_EVENTNO;
#endif
  if (_conf._last_event >= 0 &&
      _conf._first_event > _conf._last_event)
    ERROR("--first-event must be <= --last-event!");

  /******************************************************************/

  if (_conf._reverse)
    for (config_input_vect::iterator input = _inputs.begin();
	 input != _inputs.end(); ++input)
      if (input->_type != INPUT_TYPE_EXTERNAL)
	ERROR("Only external (--in-tuple=) inputs allowed in reverse mode.");

  // Check that any rfio files exist and are in staged state

#ifdef USE_RAWAPI
  bool exist_media_status_ok = true;

  for (config_input_vect::iterator input = _inputs.begin();
       input != _inputs.end(); ++input)
    {
      if (input->_type == INPUT_TYPE_RFIO)
	{
	  forked_child remote_cat;

	  char *cattool = argv0_replace(RFCAT_PREFIX "rfcat");

	  /* TODO: we could handle the check of multiple files with
	   * one call...
	   */

	  const char *argv[4];

	  argv[0] = cattool;
	  argv[1] = "--check-media-status";
	  argv[2] = input->_name;
	  argv[3] = NULL;

	  int exit_status;

	  remote_cat.fork(cattool,argv,NULL,NULL);
	  remote_cat.wait(false, &exit_status);

	  if (exit_status & 2) /* not existing */
	    exist_media_status_ok = false;
	  if (exit_status & 4) /* not staged */
	    exist_media_status_ok = false;
	  if (exit_status & ~(2 | 4))
	    ERROR("Bad exit status (%d) from RFIO media check (%s, file %s).",
		  exit_status, cattool, input->_name);

	  free(cattool);
	}
    }
  if (!exist_media_status_ok)
    ERROR("Files to read with RFIO should (exist and) "
	  "be STAGED before usage, exit.");
#endif

  init_sticky_idx();

  /******************************************************************/

  //struct sigaction action;
  //memset(&action,0,sizeof(action));
  //action.sa_handler = signal_handler_ret;
  //sigemptyset(&action.sa_mask);
  //action.sa_flags   = 0;
  //sigaction(SIGUSR1,&action,NULL);

  struct sigaction action;
  memset(&action,0,sizeof(action));
  action.sa_handler = sigint_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags   = 0;
  sigaction(SIGINT,&action,NULL);

  // Copied from hbook/ext_file_writer.cc
  struct itimerval ival;
  memset(&ival,0,sizeof(ival));
  ival.it_interval.tv_usec = 250000; // 250000 us == 0.25 s
  ival.it_value.tv_usec    = 250000;
  setitimer(ITIMER_REAL,&ival,NULL);

  memset(&action,0,sizeof(action));
  action.sa_handler = sigalarm_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags   = 0;
  sigaction(SIGALRM,&action,NULL);

  // We don't want any SIGPIPE signals to kill us

  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGPIPE);
  sigprocmask(SIG_BLOCK,&sigmask,NULL);

  // We may now use this thread as a worker, i.e. allocate memory
  // from defrag buffers etc

  // The first file should be opened somewhere around here, such that
  // any unpacking/read-ahead etc goes in parallel with firing the
  // rest of the analysis up

#ifdef USE_THREADING
  _open_retire_thread.init();
  _event_reader_thread.init();
#else
#ifdef USE_PTHREAD
  _block_main.init();
#endif
#endif

  int threads = MAX_THREADS; // fix this
  int tasks   = 3; // extract, unpack, retire

#ifdef USE_THREADING
  _unpack_event_queue.init(threads);
  _retire_queue.init(threads);
#endif

#if !defined(NDEBUG) && DEBUG_THREADING
  debug_queues_status();
#endif

#ifdef USE_THREADING
  for (int i = 0; i < threads; i++)
    _event_processor_threads[i].init(i);
#endif

#if defined(USE_EXT_WRITER)
  if (_conf._monitor_port)
    start_monitor_thread(_conf._monitor_port);
#endif

  // Initialize the threading info structure (must be before first
  // file open)
  _ti_info.init(tasks,
		threads+2/*workers+extract+retire*/,
		threads);

  if (_conf._files_open_ahead < 1)
    _conf._files_open_ahead = 1;

  bool had_broken = false;

#ifdef USE_THREADING
  int files_to_open = -1;
  int files_opened = 0; // the one that will be done

  // This just opens a file
  while (!files_opened &&
	 _open_file_queue.can_insert())
    {
      if (_open_retire_thread.open_file(files_opened,
					&_event_reader_thread._block))
	files_opened++;
    }
  _event_reader_thread.spawn();
#endif

  // Get the UI going (if wanted)

#ifdef USE_THREADING
  _ti_info.set_task(0,"PreUnpack",
		    TI_TASK_SERIAL   | TI_TASK_QUEUE_SINGLE,
		    &_open_file_queue);
  _ti_info.set_task(1,"Unpack",
		    TI_TASK_PARALLEL | TI_TASK_QUEUE_FAN_OUT,
		    &_unpack_event_queue);
  _ti_info.set_task(2,"Retire",
		    TI_TASK_SERIAL   | TI_TASK_QUEUE_FAN_IN,
		    &_retire_queue);
#endif

#ifdef USE_THREADING
  _ti_info.set_thread(0,"Extr",&_event_reader_thread);
  for (int i = 0; i < threads; i++)
    {
      char buf[32];

      sprintf(buf,"Work%d",i);
      _ti_info.set_thread(i+1,buf,&_event_processor_threads[i]);
    }
  _ti_info.set_thread(threads+1,"Final",&_open_retire_thread);
#endif

  // _ti_info_window = NULL;

#if defined(USE_THREADING) && defined(USE_CURSES)
  if (_conf._progress)
    {
      _ti_info_window = new thread_info_window();
      _ti_info_window->init(&_ti_info);
      _ti_info_window->display();
    }
#endif

#ifdef USE_MERGING
  merge_event_order *prev_event_merge_order = NULL;
  uint32_t output_eventno = 0;
#endif

  if (_conf._ts_align_hist_command
#ifdef USE_MERGING
      || _conf._merge_concurrent_files
#endif
      )
    {
#ifdef USE_LMD_INPUT
      _ts_align_hist = new tstamp_alignment(_conf._ts_align_hist_command,
# ifdef USE_MERGING
          _conf._merge_event_mode
# else
          0
# endif
          );
#endif
    }

  // Do any global pre-processing, like reading calibration parameters
  // Note, the input stream is already opened and buffering, and we
  // (or better: another processor) is already filling the buffers
  // with events to process...

  {
    ucesb_event_loop loop;

#ifndef USE_THREADING

    memset(&_status, 0, sizeof (_status));

    uint64_t   errors_file = 0;

    uint64_t   last_show = 0;
    uint64_t   last_show_multi = 0;

#endif

    timeval last_show_time;

    gettimeofday(&last_show_time,NULL);
    last_show_time.tv_sec--;

    try {

      loop.preprocess();

    } catch (error &e) {
      WARNING("Error while starting up...");
      return 1;
    }



    // Fire up the worker threads

#ifdef USE_THREADING
    {
      cpu_set_t orig_affinity;
      int use_cpu = 0;

      sched_getaffinity(0,sizeof(orig_affinity),&orig_affinity);

      for (int i = 0; i < threads; i++)
	{
	  cpu_set_t thread_affinity;

	  CPU_ZERO(&thread_affinity);

	  // Each worker thread executes on a specific CPU
	  while (!CPU_ISSET(use_cpu,&orig_affinity))
	    {
	      if (use_cpu >= CPU_SETSIZE)
		{
		  WARNING("Could only find %d CPUs to run worker threads on.",
			  i);
		  goto no_more_worker_cpus;
		}
	      use_cpu++;
	    }
	  CPU_SET(use_cpu,&thread_affinity);
	  use_cpu++;

	  //sched_setaffinity(0,sizeof(thread_affinity),&thread_affinity);
	  _event_processor_threads[i].spawn();
	}
    no_more_worker_cpus:
      // Put us back to execute whereever the OS finds nice
      sched_setaffinity(0,sizeof(orig_affinity),&orig_affinity);
    }
#endif

#ifndef USE_THREADING

    /****************************************************************/
    // Open output

#ifdef USE_LMD_INPUT
    if (_conf._file_output_bad._name)
      {
	loop._file_output_bad =
	  parse_open_lmd_file(_conf._file_output_bad._name,false);

	char msg[81];
	snprintf (msg,sizeof(msg),
		  "Bad events found by UCESB/unpacker: %s",argv[0]);

	loop._file_output_bad->set_file_header(NULL,msg, "", "");
      }
    for (config_output_vect::iterator output = _outputs.begin();
	 output != _outputs.end() ; ++output)
	{
	  config_output &this_output = *output;

	  output_info info;

	  if (this_output._type == INPUT_TYPE_FILE)
	    info._dest = parse_open_lmd_file(this_output._name);
	  else
	    {
	      assert(this_output._type == LMD_INPUT_TYPE_STREAM);
#ifdef USE_LMD_OUTPUT_TCP
	      info._dest = parse_open_lmd_server(this_output._name);
#else
	      assert(false);
#endif
	    }
	  loop._output.push_back(info);
	}
#endif

#ifdef USE_LMD_INPUT
    bool check_new_file_header = true;

    stitch_info stitch;

    stitch._last_stamp = 0;
    stitch._has_stamp = false;
    // init in case we are not stitching:
    stitch._badstamp = false;
    stitch._combine = false;
#endif

    int skip_events_counter = 0;
    bool printed_skipped = false;
    bool printed_skipped_eventno = false;
    int downscale_counter = 0;
    int64_t prev_multi_events = 0;

    for (config_input_vect::iterator input = _inputs.begin();
	 input != _inputs.end()
#ifdef USE_MERGING
	   || loop._sources.size()
#endif
	   ; )
      {
	// Open input channel

	if (_conf._broken_files)
	  errors_file = 0;

#ifdef USE_MERGING
	while (input != _inputs.end() &&
	       (int) loop._sources.size() < _conf._merge_concurrent_files)
#else
	if (input != _inputs.end())
#endif
	  {
#ifndef USE_MERGING
	    // close previous file?
	    try {
	      loop.close_source();
	    } catch (error &e) {
	      had_broken = true;
	      if (_conf._io_error_fatal)
		goto no_more_files;
	      WARNING("Close reported errors, skipping this file...");
	      goto no_more_events;
	    }
#endif

	    config_input &this_input = *input;
	    ++input;

#ifdef USE_LMD_INPUT
	    check_new_file_header = true;
#endif


	    // more files to open
	    try {
	      loop.open_source(this_input,
			       &_ti_info._file_input
			       PTHREAD_ARG(&_block_main));

#ifdef USE_MERGING
	      _ti_info._file_input = NULL; /* got troubles when src object deleted.. */
#endif
	    } catch (error &e) {
	      had_broken = true;
	      if (_conf._io_error_fatal)
		goto no_more_files;
	      WARNING("Open reported errors, skipping this file...");
	      goto no_more_events;
	    }
	  }

	/************************************************************/
	// Event loop

	for( ; ; )
	  {
get_next_event:
#ifdef USE_MERGING
	    // For each source that we have no events ready, we must
	    // get the next event.  (if it's the last event, we'll
	    // close the source, and try to open another file (by
	    // breaking out)...)

	    while (!loop._sources_need_event.empty())
	      {
		source_event_base *seb = loop._sources_need_event.back();
		loop._sources_need_event.pop_back();

		typedef __typeof__(*seb->_src) source_type;
		source_type *source = seb->_src;

		typedef __typeof__(*seb->_event) event_type;
		event_type *event = seb->_event;

		typedef __typeof__(*seb->_sticky_event) sticky_event_type;
		sticky_event_type *sticky_event = seb->_sticky_event;

		assert (CURRENT_EVENT == NULL);
		_current_event = seb->_event; // For CURRENT_EVENT

		typedef __typeof__(seb->_src->_file_event) file_event_type;
		file_event_type *file_event = &source->_file_event;
#else
	    {
	    typedef __typeof__(loop._source) source_type;
	    source_type *source = &loop._source;

	    typedef __typeof__(_static_event) event_type;
	    event_type *event = &_static_event;

#ifdef USE_LMD_INPUT
	    typedef __typeof__(_static_sticky_event) sticky_event_type;
	    sticky_event_type *sticky_event = &_static_sticky_event;
#endif

	    assert (CURRENT_EVENT == &_static_event);

	    typedef __typeof__(_file_event) file_event_type;
	    file_event_type *file_event = &_file_event;
#endif
	    try {
#if defined(USE_EXT_WRITER)
	      if (loop._ext_source)
		{
		  if (!loop.get_ext_source_event(*event))
		    {
		      // Find a new file to open
		      goto no_more_events;
		    }
		}
	      else
#endif
		{
#ifdef USE_LMD_INPUT
		if (!_conf._event_stitch_mode)
		  source->release_events();
#endif

		if (!source->get_event())
		  {
#ifdef USE_MERGING
		    // This file is over.
		    loop.close_source(seb);
		    _current_event = NULL;
		    check_new_file_header = true;
#endif
		    // Find a new file to open
		    goto no_more_events;
		  }
		}
	    } catch (error &e) {
#ifdef USE_MERGING
	      loop.close_source(seb);
	      _current_event = NULL;
	      check_new_file_header = true;
#endif
	      had_broken = true;
	      if (_conf._io_error_fatal)
		goto no_more_files;
	      WARNING("Event reported errors, skipping this file...");
	      goto no_more_events;
	    }

            // Determine event skipping right after we have an event.
            bool do_skip = false;
            if (skip_events_counter < _conf._skip_events)
	      {
		skip_events_counter++;
		do_skip = true;
		if (!printed_skipped ||
		    (skip_events_counter % 1000) == 0 ||
		    skip_events_counter == _conf._skip_events)
		  {
		    fprintf(stderr, "Skipped events: %s%d%s\r",
			    CT_OUT(BOLD_GREEN),
			    skip_events_counter,
			    CT_OUT(NORM_DEF_COL));
		    printed_skipped = true;
		  }
	      }
            else
	      {
		downscale_counter++;
		if (downscale_counter < _conf._downscale)
		  do_skip = true;
	      }
            if (do_skip)
	      {
#ifdef USE_MERGING
		loop._sources_need_event.push_back(seb);
		_current_event = NULL;
#endif
		goto get_next_event;
	      }
            downscale_counter = 0;
            if (printed_skipped)
	      {
		// For keeping the "Skipped:" output.
		fprintf(stderr, "\n");
		printed_skipped = false;
	      }

	    /* note that ps_bufhe and ps_filhe might be NULL */

	    bool printed_data = false;

	    if (_conf._print
#if defined(USE_EXT_WRITER)
		&& !loop._ext_source
#endif
		)
	      {
#if !defined(USE_LMD_INPUT) && !defined(USE_HLD_INPUT) && !defined(USE_RIDF_INPUT)
		try {
		  loop.force_event_data(*event);
		} catch (error &e) { }
		_file_event.print_event(_conf._data,NULL);
		printed_data = _conf._data;
#endif
	      }

	    bool had_error = false;

	    try {
#if defined(USE_EXT_WRITER)
	      if (!loop._ext_source)
#endif
		{
#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
		  loop.pre1_unpack_event(file_event);
#if defined(USE_LMD_INPUT)
		  if (file_event->is_sticky())
		    loop.pre2_unpack_event(*sticky_event,
					   &loop._source_event_hint);
		  else
#endif
		    loop.pre2_unpack_event(*event,
					   &loop._source_event_hint);
#endif

	      if (_conf._print)
		{
		  // With LMD input we cannot print the event until it has
		  // been pre-unpacked, i.e. the subevents localised
#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
		  try {
		    loop.force_event_data(*event, &loop._source_event_hint);
		  } catch (error &e) { }
		  file_event->print_event(_conf._data,NULL);
		  printed_data = _conf._data;
#endif
		}

	      // Before we do unpacking...  If we are stitching events,
	      // then we want to investigate the time stamp

#if defined(USE_LMD_INPUT)
	      if (_conf._event_stitch_mode)
		loop.stitch_event(*event,&stitch);
#endif

#if defined(USE_LMD_INPUT)
	      if (file_event->is_sticky())
		{
		  if (_conf._account)
		    loop.unpack_event<sticky_event_base,1>(*sticky_event);
		  else
		    loop.unpack_event<sticky_event_base,0>(*sticky_event);
		}
	      else
#endif
		{
		  if (_conf._account)
		    loop.unpack_event<event_base,1>(*event);
		  else
		    loop.unpack_event<event_base,0>(*event);
		}
		}

	      int num_multi = 0;

	      bool write_ok = true;

#if defined(USE_LMD_INPUT)
	      if (file_event->is_sticky())
		write_ok = loop.handle_event(*sticky_event,&num_multi);
	      else
#endif
		write_ok = loop.handle_event(*event,&num_multi);

	      _status._multi_events += (uint64_t) num_multi;

	      if (!write_ok)
		goto no_more_files;

#ifdef USE_MERGING
	      do_merge_prepare_event_info(seb);
	      loop._sources_next_event.push(seb);
#endif
	    } catch (error &e) {
	      had_error = true;
	      errors_file++;
	      _status._errors++;
	      if (!_conf._allow_errors) {
		if (_conf._debug && !printed_data)
		  {
		    printf ("%sError in this event:%s\n",
			    CT_OUT(RED),CT_OUT(DEF_COL));
		    try {
		      loop.force_event_data(*event
#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
					    , &loop._source_event_hint
#endif
					    );
		    } catch (error &e) { }
		    file_event->print_event(1,&event->_unpack_fail);
		  }
		//printf ("\n");
		if (errors_file >= 10)
		  {
		    if (_conf._broken_files)
		      {
#ifdef USE_MERGING
			// This file is over.
			loop.close_source(seb);
			_current_event = NULL;
			check_new_file_header = true;
#endif
			// This is not an I/O error, it's a format fault
			WARNING("Too many (%" PRIu64 ") errors, next file...",
				_status._errors);
			goto no_more_events;
		      }
		    else
		      {
			WARNING("Too many (%" PRIu64 ") errors, aborting...",
				_status._errors);
			had_broken = true;
			goto no_more_files;
		      }
		  }
	      }
#ifdef USE_MERGING
	      // In any case, the event has not been added to the output
	      // list.  So from this file we need a new event.
	      loop._sources_need_event.push_back(seb);
#endif
	    }

#if defined(USE_LMD_INPUT)
	    if (had_error &&
		loop._file_output_bad
#if defined(USE_EXT_WRITER)
		&& !loop._ext_source
#endif
		)
	      {
		try {
		  static lmd_event_out bad_event_out;
		  bad_event_out.copy_all(file_event,false);
		  loop._file_output_bad->write_event(&bad_event_out);
		} catch (error &e) {
		  goto no_more_files;
		}
	      }
#else
	    UNUSED(had_error);
#endif
#ifdef USE_MERGING
	      _current_event = NULL;
#endif
	    }

	    {
#ifdef USE_MERGING
	      // if there are no more events, then we're done...

	      if (loop._sources_next_event.empty())
		{
		  // we should also be out of files...
		  assert(input == _inputs.end());
		  // and no more streams that want to find events
		  assert(loop._sources_need_event.empty());
		  goto no_more_events;
  		}

	      // Find out from which source we'd like to take the event
	      source_event_base *seb = loop._sources_next_event.top();
	      loop._sources_next_event.pop();

	      typedef __typeof__(*seb->_src) source_type;
	      source_type *source = seb->_src;

	      typedef __typeof__(*seb->_event) event_type;
	      event_type *event = seb->_event;

	      typedef __typeof__(seb->_src->_file_event) file_event_type;
	      file_event_type *file_event = &source->_file_event;

	      unpack_event *unpack_event = &event->_unpack;

	      //printf ("[%p] ec: %d \n",seb,event->_unpack.event_no);

	      // make sure the event we got is after the previous that was processed

	      int bad_order;

	      if (prev_event_merge_order &&
		  (bad_order = do_check_merge_event_after(prev_event_merge_order,seb)) != 0)
		{
		  switch (bad_order)
		    {
		    case MERGE_EVENTS_ERROR_ORDER_SAME:
		      WARNING("Duplicate events while merging, aborting...");
		      break;
		    case MERGE_EVENTS_ERROR_ORDER_BEFORE:
		      WARNING("Events not merged in order (--merge=N, N too small?), aborting...");
		      break;
		    default: assert (false); break;
		    }
		  goto no_more_files;
		}
	      // and record info for this event to check next one
	      prev_event_merge_order =
		do_record_merge_event_info(prev_event_merge_order,seb);

	      // This is gonna need a new event...
	      loop._sources_need_event.push_back(seb);
#else
	      typedef __typeof__(_file_event) file_event_type;
	      file_event_type *file_event = &_file_event;

	      unpack_event *unpack_event = &_static_event._unpack;
#endif

              // Now we have the event no, let's see if we should skip.
              if (_conf._first_event >= 0 &&
                  unpack_event->event_no < _conf._first_event)
		{
		  if (!printed_skipped_eventno ||
		      (unpack_event->event_no % 1000) == 0 ||
		      unpack_event->event_no + 1 == _conf._first_event)
		    {
		      fprintf(stderr, "Skipped event #: %s%d%s\r",
			      CT_OUT(BOLD_GREEN),
			      unpack_event->event_no,
			      CT_OUT(NORM_DEF_COL));
		      printed_skipped_eventno = true;
		    }
		  // This is not pretty...
		  _status._multi_events = 0;
		  goto get_next_event;
		}
              if (_conf._last_event >= 0 &&
                  unpack_event->event_no > _conf._last_event)
		{
		  // We don't want the ones from the current event.
		  _status._multi_events = prev_multi_events;
		  goto no_more_files;
		}
              prev_multi_events = _status._multi_events;
              if (printed_skipped_eventno)
		{
		  // For keeping the "Skipped event #:" output.
		  fprintf (stderr, "\n");
		  printed_skipped_eventno = false;
		}

#if defined(USE_LMD_INPUT)
	      try {
	      if (check_new_file_header)
		{
		  // Use the file header of the first file still
		  // active.  (note: this a bit contorted way, using a
		  // trigger variable check_new_file_header was more
		  // easy than trying to set it when opening new
		  // files.  Two reasons: that would have given last
		  // opened file (bad (strange) for merging), and also
		  // would then have required an early read_record)

		  s_filhe_extra_host *file_header = NULL;

#ifdef USE_MERGING
		  for (unsigned int i = 0; i < loop._sources.size(); i++)
		    {
		      source_event_base *seb = loop._sources[i];

		      typedef __typeof__(*seb->_src) source_type;
		      source_type *source = seb->_src;

#else
		    {
		      typedef __typeof__(loop._source) source_type;
		      source_type *source = &loop._source;
#endif

		      if (source->_file_header_seen)
			file_header = &source->_file_header;
		    }

		  char msg[81];
		  snprintf (msg,sizeof(msg),
			    "Processed by UCESB/unpacker: %s",argv[0]);

		  char msg_eb[81] = "";
		  char msg_ts[81] = "";

		  if (_conf._enable_eventbuilder)
		  {
		    snprintf(msg_eb, sizeof(msg_eb),
			      "DESPEC Event Building for AIDA%s",
			      _conf._enable_dtas ? ", DTAS" : "");
		  }

		  if (_conf._event_stitch_mode == TIMESTAMP_TYPE_WR)
		  {
		    snprintf (msg_ts, sizeof(msg),
			    "Time-stitched with WR, window = %d ns", _conf._event_stitch_value);
		  }

		  for (unsigned int i = 0; i < loop._output.size(); i++)
		    {
		      output_info &output = loop._output[i];
		      output._dest->set_file_header(file_header,msg, msg_eb, msg_ts);
		    }
		  check_new_file_header = false;
		}

	      if (_conf._event_stitch_mode &&
		  (!stitch._combine || stitch._badstamp))
		{
		  // Dump previous events!  Already formatted.

		  for (unsigned int i = 0; i < loop._output.size(); i++)
		    {
		      output_info &output = loop._output[i];

		      // Do not dump if there was nothing...

		      if (output._dest->_select.accept_final_event(&output._event))
			{
			  output._dest->event_no_seen(output._event._info.l_count);
			  output._dest->write_event(&output._event);
			}
		    }
		}

	      for (unsigned int i = 0; i < loop._output.size(); i++)
		{
		  output_info &output = loop._output[i];

		  if (!stitch._combine || stitch._badstamp)
		    output._event.clear();

		  if (!output._dest->_select.accept_event(file_event,
							  &file_event->_header))
		    continue;

#ifdef COPY_OUTPUT_FILE_EVENT
		  if (!COPY_OUTPUT_FILE_EVENT(&output._event,
					      file_event,
					      unpack_event,
					      &output._dest->_select,
					      stitch._combine))
		    continue;
#else
		  output._event.copy(file_event,&output._dest->_select,
				     stitch._combine);
		  UNUSED(unpack_event);
#endif
#ifdef USE_MERGING
		  if (!stitch._combine &&
		      _conf._merge_event_mode != MERGE_EVENTS_MODE_EVENTNO)
		    {
		      // New event number required.
		      output._event._info.l_count = ++output_eventno;
		    }
#endif
		  /* The event_no_seen function should really be
		   * called for all events that we even manage to
		   * partially unpack...  But it must still be called
		   * order...
		   */

		  if (!_conf._event_stitch_mode ||
		      stitch._badstamp)
		    {
		      if (output._dest->_select.accept_final_event(&output._event))
			{
			  output._dest->event_no_seen(output._event._info.l_count);
			  output._dest->write_event(&output._event);
			}

		      if (stitch._badstamp)
			output._event.clear();
		    }
		}
	      } catch (error &e) {
		goto no_more_files;
	      }

	      // printf ("%d %d\n", _conf._event_stitch_mode, stitch._combine);

#if defined(USE_LMD_INPUT)
	      if (_conf._event_stitch_mode &&
		  !stitch._combine)
		{
#ifdef USE_MERGING
		  for (vect_source_event_base::iterator iter =
			 loop._sources_need_event.begin();
		       iter != loop._sources_need_event.end(); ++iter)
		    {
		      source_event_base *seb = (*iter);

		      typedef __typeof__(*seb->_src) source_type;
		      source_type *source = seb->_src;
#else
		  {
		    typedef __typeof__(loop._source) source_type;
		    source_type *source = &loop._source;
#endif
		    source->release_events();
		  }
		}
#endif
#ifdef USE_MERGING
	      seb->_events++;
	      _ts_align_hist->account(seb->_tstamp_align_index,
				      seb->_timestamp);
#endif
#else
	      UNUSED(file_event);
	      UNUSED(unpack_event);
#endif
	    }
	    _status._events++;

            // Casting is never great, but, 63-bits for event counters...
	    if (_conf._max_events >= 0 &&
		_status._events >= (uint64_t)_conf._max_events)
	      goto no_more_files;

#ifndef USE_MERGING
#ifdef USE_CURSES
	    if (_conf._watcher._command)
	      watcher_event();
	    else
#endif
#endif
	      {
		if (_update_progress)
		  {
		    _update_progress=0;

		    timeval now;

		    gettimeofday(&now,NULL);

                    const static char spinner_symbols[] = "-\\|/";
                    const static size_t spinner_symbols_len =
		      strlen(spinner_symbols);
		    static int spinner_count = 0;
                    int spinner_current =
		      spinner_symbols[spinner_count %
				      spinner_symbols_len];
		    spinner_count++;

		    _ti_info.update();

		    if (_ti_info_window)
		      {
#ifdef USE_THREADING
			_ti_info_window->display();
#endif
		      }
		    else if (!_conf._quiet)
		      {
			double elapsed = (double) (now.tv_sec - last_show_time.tv_sec) +
			  1.0e-6 * (double) (now.tv_usec - last_show_time.tv_usec);

			if (elapsed > 0.05)
			  {
			    /* Do not update the time for the first events,
			     * to quickly slow progress >= 1. */
			    if (_status._events >= 100)
			      last_show_time = now;

			    double event_rate =
			      ((double) (_status._events - last_show)) /
			      elapsed;

			    // Come up with a new interval such that spinner
			    // rate corresponds to log unpack speed.

			    // At 1 Hz (and lower) we want 1 update/s.
			    // At 1 MHz, we want say 11 updates/s.

			    double update_freq = 1. * log(event_rate) / M_LN10;
			    double interval = 1. / update_freq;

			    if (interval > 1) // 999999 to not set seconds
			      ival.it_value.tv_usec = 999999;
			    else if (interval < 0.05)
			      ival.it_value.tv_usec = 20000;
			    else
			      ival.it_value.tv_usec =
				(int) (1.e6 * interval);

			    setitimer(ITIMER_REAL,&ival,NULL);

			    //printf ("ival: %d   \n",
			    //    (int) ival.it_value.tv_usec);

#ifdef USE_MERGING
			    if (_conf._merge_concurrent_files > 1)
			      {
				for (vect_source_event_base::iterator iter =
				       loop._sources.begin();
				     iter != loop._sources.end();
				     ++iter)
				  {
				    source_event_base *seb = (*iter);

				    double rate =
				      ((double) (seb->_events -
						 seb->_events_last_show)) /
				      elapsed;
				    seb->_events_last_show = seb->_events;

				    char r_str[64];

				    format_prefix(r_str, sizeof (r_str),
						  rate, 5,
						  &seb->_ev_diff_info);

				    fprintf (stderr,
					     "[%s%s%s/s] ",
					     CT_ERR(BOLD),
					     r_str,
					     CT_ERR(NORM));
				  }

				char ev_str[64];
				char evr_str[64];

				format_prefix(ev_str, sizeof (ev_str),
					      (double) _status._events, 7,
					      &_ev_diff_info);
				format_prefix(evr_str, sizeof (evr_str),
					      event_rate, 5,
					      &_evr_diff_info);

				fprintf(stderr,
					"%s%s%s  (%s%s%s/s)  ",
					CT_ERR(BOLD_GREEN),
					ev_str,
					CT_ERR(NORM_DEF_COL),
					CT_ERR(BOLD),
					evr_str,
					CT_ERR(NORM));

				if (prev_event_merge_order)
				  print_current_merge_order(prev_event_merge_order);

				fprintf(stderr," %c   \r", spinner_current);
			      }
			    else // !(_conf._merge_concurrent_files > 1)
#endif // USE_MERGING
			      {
                                double mevent_rate =
                                  ((double) (_status._multi_events-
                                             last_show_multi)) / elapsed;

				char ev_str[64];
				char evr_str[64];
				char mev_str[64];
				char mevr_str[64];
				char err_str[64];

				format_prefix(ev_str, sizeof (ev_str),
					      (double) _status._events, 7,
					      &_ev_diff_info);
				format_prefix(evr_str, sizeof (evr_str),
					      event_rate, 5,
					      &_evr_diff_info);
				format_prefix(mev_str, sizeof (mev_str),
					      (double) _status._multi_events, 7,
					      &_mev_diff_info);
				format_prefix(mevr_str, sizeof (mevr_str),
					      mevent_rate, 5,
					      &_mevr_diff_info);
				format_prefix(err_str, sizeof (err_str),
					      (double) _status._errors, 5,
					      &_err_diff_info);

				fprintf(stderr,"Processed: "
				    "%s%s%s (%s%s%s/s)  "
				    "%s%s%s (%s%s%s/s) "
				    "(%s%s%s errors) %c     \r",
				    CT_ERR(BOLD_GREEN),
				    ev_str,
				    CT_ERR(NORM_DEF_COL),
				    CT_ERR(BOLD),
				    evr_str,
				    CT_ERR(NORM),
				    CT_ERR(BOLD_BLUE),
				    mev_str,
				    CT_ERR(NORM_DEF_COL),
				    CT_ERR(BOLD),
				    mevr_str,
				    CT_ERR(NORM),
				    CT_ERR(BOLD_RED),
				    err_str,
				    CT_ERR(NORM_DEF_COL),
				    spinner_current);
			      }
			    unsigned int nlines = 0;
#if defined(USE_LMD_INPUT)
			    for (unsigned int i = 0; i < loop._output.size(); i++)
			      {
				output_info &output = loop._output[i];
				output._dest->print_status(elapsed);
				nlines++;
			      }
#endif
			    for (unsigned int i = 0; i < nlines; i++)
			      fprintf (stderr,"%s",CT_ERR(UP1LINE));
			    fflush(stderr);
			    // INFO_FLUSH;

			    last_show = _status._events;
			    last_show_multi = _status._multi_events;
			  }
		      }
		  }
#if defined(USE_EXT_WRITER)
		MON_CHECK_COPY_BLOCK(&_status_block, &_status);
#endif
	      }
	  }
      no_more_events:;
	loop.close_ext_source();
	/************************************************************/
      }
  no_more_files:;
    // Since all this closing can throw a lot (mostly due to badly
    // ended subprocesses), we catch to do all the closing!
    try {
      loop.close_ext_source();
    } catch (error &e) {
      WARNING("Error while closing sources...");
      return 1;
    }
#ifdef USE_MERGING
    try {
      loop.close_sources();
    } catch (error &e) {
      WARNING("Error while closing sources...");
      return 1;
     }
#else
    try {
      loop.close_source();
    } catch (error &e) {
      WARNING("Error while closing sources...");
      return 1;
     }
#endif
#ifdef USE_LMD_INPUT
    if (_conf._event_stitch_mode)
      {
	// Dump last event!  Already formatted.

	for (unsigned int i = 0; i < loop._output.size(); i++)
	  {
	    output_info &output = loop._output[i];

	    // Do not dump if there was nothing...

	    if (output._dest->_select.accept_final_event(&output._event))
	      {
		output._dest->event_no_seen(output._event._info.l_count);
		output._dest->write_event(&output._event);
	      }
	  }
      }
#endif
    try {
      loop.close_output();
    } catch (error &e) {
      WARNING("Error while closing output...");
      return 1;
    }

#ifdef USE_CURSES
    endwin();
#endif

    INFO("Events: "
	 ERR_GREEN "%" PRIu64 ERR_ENDCOL "   "
	 ERR_BLUE "%" PRIu64 ERR_ENDCOL "             ("
	 ERR_RED "%" PRIu64 ERR_ENDCOL " errors)                \n",
	 _status._events,_status._multi_events,_status._errors);
    try {
      loop.postprocess();
    } catch (error &e) {
      WARNING("Error while shutting down...");
      return 1;
    }

#endif

#ifdef USE_THREADING

    // Loop of main thread.  We are responsible for:
    // * Opening files.
    // * Retiring events.
    // * Monitoring (if enabled).

    // The reason we are opening the files is that we want that task
    // to be asyncronous to the file reading.

    // This is a very light-working thread.  May fiddle around on any
    // CPU it likes.  (wherever there are some cycles to spare)

    timeval show_interval;
    timeval wait_show;
    timeval next_show_time;

    show_interval.tv_sec  = 0;
    show_interval.tv_usec = 250000; // 250 ms, 4 Hz

    next_show_time = last_show_time;

    for ( ; ; )
      {
	_ti_info.update();

	////////////////////////////////////////////////////////////
	// Retire events

      retire_events:

	while (LIKELY(_retire_queue.can_remove()))
	  {
	    TDBG("retire event");

	    // There is an event to retire!

	    eq_item &item = _retire_queue.next_remove();

	    // Before we start to relaim the event, print it, if
	    // marked for printing.  (All event printing goes here,
	    // such that we need not store event print requests into
	    // the error/info queue, to be printed at reclaim time

	    int info = item._info;

	    if (UNLIKELY(info & (EQ_INFO_FILE_CLOSE |
				 EQ_INFO_FLUSH |
				 EQ_INFO_PRINT_EVENT)))
	      {
		// There is something particular to do...

		if (info & EQ_INFO_FILE_CLOSE)
		  {
		    // It contains a file item to close.

		    data_input_source *source =
		      (data_input_source *) item._event;

		    _open_retire_thread.close_file(source);
		  }
		else
		  {
		    // Since file headers and buffers preferably
		    // should be printed before the event data itself
		    // (and any error messages), we will (when needed)
		    // walk through the reclaim items and print those
		    // things.  (this will not free the reclaim
		    // items..)  (it is not a performance hit, since
		    // only when debugging, this will be requested)

		    if (info & EQ_INFO_PRINT_BUFFER_HEADERS)
		      {
			// print_format_reclaim_items(item._reclaim);



		      }

		    if (info & EQ_INFO_PRINT_EVENT)
		      {
			// Before we try to print the event, we must
			// make sure that any subevents (if present in
			// the data format) has been localized, and also
			// defragmented...

			// Set up the pointers for where to put the allocated meory
			_wt._last_reclaim       = item._last_reclaim;

			event_base *eb = (event_base *) item._event;

			try {
			  // we must setup the reclaim pointers, since
			  // we may need to allocate memory to hold
			  // (info about subevents, but also subevent
			  // data itself, if it was fragmented for some
			  // reason

			  // src_event->get_10_1_info();    // this may throw up (also)...
			  // src_event->locate_subevents(); // this may throw up...

			  ucesb_event_loop::force_event_data(*eb);

			} catch (error &e) {

			}
			// Quit delivering error messages here
			item._last_reclaim = _wt._last_reclaim;
			_wt._last_reclaim = NULL;

			// Now, we are set to print the event!
			// And it will be printed directly

			((FILE_INPUT_EVENT *) eb->_file_event)->print_event(!!(info & EQ_INFO_PRINT_EVENT_DATA));
		      }
		  }
	      }

	    // We execute the reclaim list.  This will also eject any
	    // error messages to the error output.

	    reclaim_items(item._reclaim);

	    // We're done, so discard the queue item

	    _retire_queue.remove();

	    if (UNLIKELY(info & EQ_INFO_DONE))
	      {
		// Done.  We're finished processing
		goto no_more_files;
	      }
	  }

	////////////////////////////////////////////////////////////
	// Is there a request to open a new file?

	// TODO: we must also check that there is space in the queue
	// to insert files...

	//fprintf (stderr,"Wanna open file: %d + %d >= %d\n",
	//	 files_to_open,_conf._files_open_ahead,files_opened);

	while (UNLIKELY(files_to_open + _conf._files_open_ahead >= files_opened))
	  {

	    if (_open_file_queue.can_insert())
	      {
		if (_open_retire_thread.open_file(files_opened,
						  &_event_reader_thread._block))
		  files_opened++;
	      }
	    else
	      {
		// We cannot insert a file right now.  Enqueue us to
		// be woken up when space has become available

		// We do the wakeup-request only here and not below
		// (before the block, since it should only be done
		// when a file should be enqueued.  (this is a rare
		// event)

		_open_file_queue.request_insert_wakeup(&_open_retire_thread._block);
		if (_open_file_queue.can_insert())
		  {
		    _open_file_queue.cancel_insert_wakeup();
		    continue; // try again
		  }
		// break out of open file-loop.  We'll be back later...
		break;
	      }
	  }

	////////////////////////////////////////////////////////////
	// Monitoring

       	timeval now;

	gettimeofday(&now,NULL);

	if (timercmp(&now,&last_show_time,<) ||
	    timercmp(&now,&next_show_time,>))
	  {
#if !defined(NDEBUG) && DEBUG_THREADING
	    debug_queues_status();
#endif

#if defined(USE_CURSES)
	    if (_ti_info_window)
	      _ti_info_window->display();
	    else
#endif
	      {
		// Update the one-line statistics

		fprintf(stderr,"File: %10d %10d %10d   \r",
			(int) _ti_info._input._ahead,
			(int) _ti_info._input._active,
			(int) _ti_info._input._free);
		fflush(stderr);
	      }

	    timeradd(&now,&show_interval,&next_show_time);
	    wait_show = show_interval;
	  }
	else
	  timersub(&next_show_time,&now,&wait_show);

	////////////////////////////////////////////////////////////

	// We reached the end of the loop.  Nothing more to do right
	// now.  Wait for someone to wake us up.

	// Tell the final stage before retirement that we went to
	// sleep, i.e. when they have retired some (hysteresis)
	// events, it is a good plan to wake us up.

	TDBG("nothing to retire, no file to open");

	//_open_retire_thread._block.request_wakeup();
	_retire_queue.request_remove_wakeup(&_open_retire_thread._block);
	if (_retire_queue.can_remove())
	  {
	    //_open_retire_thread._block.cancel_wakeup();
	    _retire_queue.cancel_remove_wakeup();
	    goto retire_events; // data became available
	  }

	// If we had files to open, but no queue slots, then we've
	// already told the enqueuer to wake us up, should a slot
	// become available

	TDBG("waiting");

	fd_set rfds;
	FD_ZERO(&rfds);

	int token = _open_retire_thread._block.block(-1,&rfds,&wait_show);

	if (token & TOKEN_OPEN_FILE)
	  {
	    int to_open = token & TOKEN_FILES_TO_OPEN_MASK;

	    // fprintf (stderr,"to_open: %d\n",to_open);

	    if (to_open > files_to_open)
	      files_to_open = to_open;
	  }
      }

 no_more_files:
    ;

#endif
  }

  return 0;
}



