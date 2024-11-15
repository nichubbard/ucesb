#include "config.hh"
#include "lmd_input.hh"
#include "structures.hh"

#include "user.hh"

#include <algorithm>
#include <ctime>
#include <curses.h> // needed for the COLOR_ below

#include "../watcher/watcher_channel.hh"
#include "../watcher/watcher_window.hh"

#include <map>
#include "../lu_common/colourtext.hh"

#include <atomic>
#include <sys/select.h>
#include <time.h>

#ifdef ZEROMQ
#include "ucesb.pb.h"
#include <zmqpp/zmqpp.hpp>
#undef OK
#include <google/protobuf/util/json_util.h>

#ifndef ZMQ_PORT
#define ZMQ_PORT "4242"
#endif

static web_monitor::UcesbReport report;
static zmqpp::context zmq_context;
static zmqpp::socket zmq_pubber(zmq_context, zmqpp::socket_type::pub);
extern std::atomic<int> _global_clients;
#endif

static int _events = 0;
int64_t __monitor_last = 0;
int64_t _monitor_now = 0;
// spil data
static bool _on_spill = false;
static uint64_t _last_spill = 0;
static uint64_t _spill_length = 0;
static uint64_t _extraction_time = 0;
static uint32_t _spill_counter = 0;

watcher_type_info web_monitor_watch_types[NUM_WATCH_TYPES] =
{
  { COLOR_GREEN,   "Physics" },
  { COLOR_YELLOW,  "Pulser" },
};

static constexpr uint64_t fast_scaler_refresh = 1E8; // nanoseconds 
static uint64_t last_fast_refresh = 0;
void zmq_calculate_scalers();
// Calculate the realtime in nanoseconds since 1970 (like WR but in UTC and much less precise)
static uint64_t realtime_ns()
{
  timespec v;
  clock_gettime(CLOCK_REALTIME, &v);
  uint64_t ts = static_cast<uint64_t>(v.tv_sec) * 1E9;
  ts += v.tv_nsec;
  return ts;
}

void web_monitor_watcher_event_info(watcher_event_info *info,
    unpack_event *event)
{
  info->_type = WEB_WATCH_TYPE_PHYSICS;
  bool pulse = false;

  if (event->trigger == 3)
  {
    info->_type = WEB_WATCH_TYPE_TCAL;
    pulse = true;
  }

  /* One can also override the _time and _event_no variables, altough
   * for LMD files formats this is filled out before (from the buffer
   * headers), so only needed if we do not like the information
   * contained in there.  Hmm _time from buffer not implemented yet...
   */

  // Since we have a time stamp (on some events), we'd rather use that
  // than the buffer timestamp, since the buffer timestamp is the time
  // of packaging, and not of event occuring
  //sprint()
  // clean it for events not having it
  info->_info &= ~WATCHER_DISPLAY_INFO_TIME;
  if (event->wr.size() > 0)
  {
    int64_t wr = event->wr[0].second;
    info->_time = (uint)(wr / (uint64_t)1e9);
    info->_info |= WATCHER_DISPLAY_INFO_TIME;
    _monitor_now = wr;
#ifdef ZEROMQ
    report.mutable_summary()->set_time(info->_time);
    report.mutable_summary()->set_wr(wr);
#endif
  }

#ifdef ZEROMQ
  report.mutable_summary()->set_event_no(info->_event_no);
  report.mutable_summary()->set_server(_inputs[0]._name);
#endif

  _events++;

#ifdef ZEROMQ
  uint64_t rt_now = realtime_ns();
  if (rt_now - last_fast_refresh > fast_scaler_refresh)
  {
    // Transmit scaler data again
    zmq_calculate_scalers();
    zmqpp::message message;
    std::string proto;
    report.SerializeToString(&proto);
    message << "stat" << proto;
    zmq_pubber.send(message);
    last_fast_refresh = rt_now;
  }
#endif  
}

void zmq_calculate_scalers()
{
#ifdef ZEROMQ
  //double dt = (_monitor_now - _monitor_last) / (double)1e9;
#endif
}

void web_monitor_watcher_init()
{
  extern watcher_window _watcher;
  _watcher._display_channels.clear();
  _watcher._present_channels.clear();
  init_pair(5, COLOR_RED, COLOR_BLACK);

  INFO("Web Monitor Example Watcher Initialised");

#ifdef ZEROMQ
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  INFO("Google Protobuf Version Verified OK");
  zmq_pubber.bind("tcp://*:" ZMQ_PORT);
  INFO("ZeroMQ PUB socket running on '%s'", "tcp://*:" ZMQ_PORT);

  std::atexit([]() {
      zmqpp::message message;
      message << "exit";
      zmq_pubber.send(message);
  });

  report.mutable_summary()->set_server(_inputs[0]._name);
  // Bit of a hack, look for trans server (only one used) and if active 
  // report hostname/port to ucesb protobuf
  for (auto& op : _outputs)
  {
    if (op._type == LMD_INPUT_TYPE_STREAM && strstr(op._name, "trans") != 0)
    {
      char hostname[64];
      char analserver[64 + 10];
      gethostname(hostname, 64);
      sprintf(analserver, "%s:%d", hostname, 6000);
      report.mutable_summary()->set_analserver(analserver);
    }
  }

  sleep(1);

  zmqpp::message message;
  std::string proto;
  report.SerializeToString(&proto);
  message << "init" << proto;
  zmq_pubber.send(message);
#endif
}

void web_monitor_watcher_display(watcher_display_info& info)
{
  if (info._line > info._max_line)
    return;

  wcolor_set(info._w, info._col_norm, NULL);
  wmove(info._w, info._line, 0);

  werase(info._w);

  whline(info._w, ACS_HLINE, 80);

  info._line += 1;

  if (_monitor_now == __monitor_last)
  {
    __monitor_last--;
  }

  double dt = (_monitor_now - __monitor_last) / (double)1e9;

  wrefresh(info._w);

#ifdef ZEROMQ
  zmq_calculate_scalers();
#endif

  extern watcher_window _watcher;

#ifdef ZEROMQ
  extern std::deque<std::pair<std::string, int>> errors;
  report.clear_logs();
  for (auto& e : errors)
  {
    auto log = report.add_logs();
    log->set_message(e.first);
    log->set_severity(web_monitor::LogEntry::LogSeverity::LogEntry_LogSeverity_NORMAL);
    if (e.second == FE_ERROR)
    {
      log->set_severity(web_monitor::LogEntry::LogSeverity::LogEntry_LogSeverity_ERROR);
    }
    if (e.second == FE_WARNING)
    {
      log->set_severity(web_monitor::LogEntry::LogSeverity::LogEntry_LogSeverity_WARNING);
    }
  }

  report.mutable_summary()->clear_triggers();
  for (int i = 0; i < NUM_WATCH_TYPES;  i++)
  {
    auto trig = report.mutable_summary()->add_triggers();
    trig->set_name(WATCH_TYPE_NAMES[i]._name);
    trig->set_rate(_watcher._type_count[i]);
  }
  report.mutable_summary()->set_clients(_global_clients);

  zmqpp::message message;
  std::string proto;
  report.SerializeToString(&proto);
  message << "stat" << proto;
  zmq_pubber.send(message);
#endif
}

void web_monitor_watcher_clear()
{
  __monitor_last = _monitor_now;

}

void web_monitor_watcher_keepalive(bool dead)
{
#ifdef ZEROMQ
  if (dead)
  {
    zmqpp::message message;
    std::string proto;
    report.SerializeToString(&proto);
    message << "dead" << proto;
    zmq_pubber.send(message);
  }
#endif
}

