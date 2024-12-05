#include "config.hh"
#include "lmd_input.hh"
#include "structures.hh"

#include "user.hh"

#include <algorithm>
#include <cstdint>
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

static frs_monitor::UcesbReport report;
static zmqpp::context zmq_context;
static zmqpp::socket zmq_pubber(zmq_context, zmqpp::socket_type::pub);
extern std::atomic<int> _global_clients;
#endif

static int _events = 0;
int64_t __monitor_last = 0;
int64_t _monitor_now = 0;
// per-wr tracking
std::map<int, long> events;
std::map<int, long> pulses;
std::map<int, long> events_total;
std::map<int, uint64_t> last_event;
// wr sync tracking
std::map<int, int64_t> pulsers;
std::map<std::pair<int, int>, bool> sync_ok;
std::map<std::pair<int, int>, int> sync_bad;
std::map<int, int> daq_sync;
// spil data
static bool _on_spill = false;
static uint64_t _last_spill = 0;
static uint64_t _spill_length = 0;
static uint64_t _extraction_time = 0;
static uint32_t _spill_counter = 0;
// frs trloii trigger mux
std::vector<uint32_t> frs_trloii_now(16 * 3);
std::vector<uint32_t> frs_trloii_old(16 * 3);
// frs trloii dump
std::vector<uint32_t> frs_trloii_all_now(93);
std::vector<uint32_t> frs_trloii_all_old(93);

// This contains data that can change more often
#include FRS_EXPERIMENT_H

watcher_type_info frs_monitor_watch_types[NUM_WATCH_TYPES] =
{
  { COLOR_GREEN,   "Physics" },
  { COLOR_YELLOW,  "TRLOII" },
  { COLOR_YELLOW,  "WR Sync" },
  { COLOR_BLUE,    "BOS" },
  { COLOR_BLUE,    "EOS" },
  { COLOR_RED,     "Other" },
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

void frs_monitor_watcher_event_info(watcher_event_info *info,
    unpack_event *event)
{
  bool pulse = false;


  switch (event->trigger)
  {
    case 1:
      info->_type = FRS_WATCH_TYPE_PHYSICS;
      break;
    case 2:
      info->_type = FRS_WATCH_TYPE_TRLOII;
      break;
    case 3:
      info->_type = FRS_WATCH_TYPE_WR;
      pulse = true;
      break;
    case 4:
      info->_type = FRS_WATCH_TYPE_BOS;
      break;
    case 5:
      info->_type = FRS_WATCH_TYPE_EOS;
      break;
    default:
      info->_type = FRS_WATCH_TYPE_OTHER;
      break;
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

  // DAQ and Pulser Tracking
  for (uint i = 0; i < event->wr.size(); i++)
  {
    // Used to track if DAQ is "alive"
    last_event[event->wr[i].first] = _monitor_now;

    if (pulse)
    {
      int id = event->wr[i].first;
      int64_t wr = event->wr[i].second;
      // Statistics
      pulses[id]++;
      if (!events_total[id]) events_total[id] = 0;
      int64_t prev_ts = pulsers[id];
      if (prev_ts != 0)
      {
        // Check pulse times of all other subsystems
        // If the time between this and other system is < 30 us
        // the sync of this PAIR is good
        // otherwise if 20 pulses have gone and none have matched
        // the pair is BAD
        //
        // If the PAIR is good, both DAQs are good
        // If the PAIR is bad, a DAQ is bad if all its pairs are bad
        for (auto& i : pulsers)
        {
          if (i.first == id) continue;
          int64_t prev_them = i.second;
          int64_t time_since_other = wr - prev_them;
          if (wr > prev_them && prev_them != 0)
          {
            auto pair = std::make_pair(id, i.first);
            if (id > i.first) pair = std::make_pair(i.first, id);

            if (abs((int)time_since_other) < 30000)
            {
              sync_ok[pair] = true;
              sync_bad[pair] = 0;
              daq_sync[i.first] = 1;
              daq_sync[id] = 1;
            }
            else
            {
              if(++sync_bad[pair] > 20)
              {
                sync_ok[pair] = false;
                if (daq_sync[i.first] != 1) daq_sync[i.first] = 2;
                if (daq_sync[id] != 1) daq_sync[id] = 2;
              }
            }
          }
        }
      }
      pulsers[id] = wr;
    }
    else
    {
      // Non pulsers just accumulate statistics
      events[event->wr[i].first]++;
      events_total[event->wr[i].first]++;
    }
  }

  // No pulser for 2 minutes - downgrade to ? status again
  for (auto& i : pulsers)
  {
    if (i.second && i.second + 120e9 < _monitor_now)
    {
      // Reset ALL daq syncs for now, they should be redone next time?
      for (auto syncid : daq_sync) {
        daq_sync[syncid.first] = 0;
      }
      i.second = 0;
    }
  }

  // TRLOII Scalers
  //
  for (uint i = 0; i < 16; i++) {
    frs_trloii_now[i] = event->trloii_mvlc.trloii_trig_mux.before_deadtime[i];
    frs_trloii_now[16 + i] = event->trloii_mvlc.trloii_trig_mux.after_deadtime[i];
    frs_trloii_now[32 + i] = event->trloii_mvlc.trloii_trig_mux.after_reduction[i];
  }

#define LIST_SCALER(name, N) \
  for(uint i = 0; i < N; i++) { \
    frs_trloii_all_now[offs + i] = event->trloii_mvlc.trloii_src_scalers.name[i]; \
  }

  // TRLOII DUMP
  if (event->trigger == 2) {
    size_t offs = 0;
	LIST_SCALER(ecl_in,                 16)
	LIST_SCALER(ecl_io_in,               8)
	LIST_SCALER(lemo_in,                 2)
	LIST_SCALER(wired_zero,              1)
	LIST_SCALER(wired_one,               1)
	LIST_SCALER(prng_poisson,            1)
	LIST_SCALER(pulser,                  4)
	LIST_SCALER(lmu_out,                 8)
	LIST_SCALER(gate_delay,              4)
	LIST_SCALER(edge_gate,               2)
	LIST_SCALER(downscale,               1)
	LIST_SCALER(all_or,                  2)
	LIST_SCALER(coincidence,             1)
	LIST_SCALER(input_coinc,             1)
	LIST_SCALER(multi_latch_alm_full,    4)
	LIST_SCALER(serial_tstamp_out,       1)
	LIST_SCALER(serial_tstamp_alm_full,  1)
	LIST_SCALER(serial_tstamp_desync,    1)
	LIST_SCALER(serial_signals_out,      3)
	LIST_SCALER(heimtime_out,            1)
	LIST_SCALER(accept_trig,            16)
	LIST_SCALER(encoded_trig,            4)
	LIST_SCALER(master_start,            1)
	LIST_SCALER(master_toggle,           2)
	LIST_SCALER(multi_scaler,            1)
	LIST_SCALER(deadtime,                1)
	LIST_SCALER(accept_pulse,            1)
	LIST_SCALER(trig_lmu_out_enabled_or, 1)
	LIST_SCALER(multi_trig_buf_alm_full, 1)
	LIST_SCALER(multi_scaler_alm_full,   1)
	LIST_SCALER(trimi_tdt,               1)
  }

  _events++;

#ifdef ZEROMQ
  // Send some statistics to the 0MQ report
  report.mutable_summary()->set_event_no(info->_event_no);
  report.mutable_summary()->set_server(_inputs[0]._name);
#endif

#ifdef ZEROMQ
  // Send data every 100ms (sorta), although some data
  // is only done every second. Makes scalers look more responsive
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
  double dt = (_monitor_now - __monitor_last) / (double)1e9;

  report.clear_scalers();

  auto trloii_tpat_report = report.add_scalers();
  trloii_tpat_report->set_key("tpat");
  trloii_tpat_report->clear_scalers();
  for (size_t i = 0; i < 48; i++) {
    auto entry = trloii_tpat_report->add_scalers();
    entry->set_index(i);
    entry->set_rate((double)(frs_trloii_now[i] - frs_trloii_old[i]) / dt);
  }

  auto trloii_dump_report = report.add_scalers();
  trloii_dump_report->set_key("trloii");
  trloii_dump_report->clear_scalers();
  for (size_t i = 0; i < frs_trloii_all_now.size(); i++) {
    auto entry = trloii_dump_report->add_scalers();
    entry->set_index(i);
    entry->set_rate((double)(frs_trloii_all_now[i] - frs_trloii_all_old[i]) / dt);
  }
#endif
}

void frs_monitor_watcher_init()
{
  extern watcher_window _watcher;
  _watcher._display_channels.clear();
  _watcher._present_channels.clear();
  init_pair(5, COLOR_RED, COLOR_BLACK);

  // Create all expected subsystems so they're always shown
  for (auto i : expected)
  {
    events_total[i] = 0;
  }

  INFO("FRS Web Monitor Watcher Initialised");

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

// Used to format big numbers (event counters)
void format_long_int(char* buf, long i)
{
  int r = 0;
  while (i > 1e8)
  {
    i /= 1000;
    r += 1;
  }

  if (r)
  {
    char rs[] = { ' ', 'k', 'M', 'B', 'T', 'q', 'Q', 's', 'S', '?' };
    if (r > 9) r = 9;
    sprintf(buf, "%7ld%c", i, rs[r]);
  }
  else
  {
    sprintf(buf, "%8ld", i);
  }
}

void frs_monitor_watcher_display(watcher_display_info& info)
{
  if (info._line > info._max_line)
    return;

  wcolor_set(info._w, info._col_norm, NULL);
  wmove(info._w, info._line, 0);

  werase(info._w);

  whline(info._w, ACS_HLINE, 80);
  mvwaddstr(info._w, info._line, 1, "FRS DAQ Status");

  info._line += 1;

  if (_monitor_now == __monitor_last)
  {
    __monitor_last--;
  }
 
  mvwprintw(info._w, info._line, 0, "%8s\t%4s\t%8s    %10s    %10s    %12s", "System", "ID", "Events", "Rate", "Pulser", "Correlation");
  info._line++;

  double dt = (_monitor_now - __monitor_last) / (double)1e9;

  char buf[256] = { '\0' };
#ifdef ZEROMQ
  report.mutable_status()->clear_daq();
#endif
  // Loop through all WR IDs that have been seen
  for(auto& i: events_total)
  {
    // If the DAQ hasn't send a message in 2 minutes, say it's dead
    bool active = true;
    if (last_event[i.first] + 120e9 < _monitor_now)
    {
      active = false;
    }
    format_long_int(buf, events_total[i.first]);
    {
#ifdef ZEROMQ
      // Just shove all the data in the 0MQ buffer, this part is easy
      auto report_daq = report.mutable_status()->add_daq();
      report_daq->set_events(events_total[i.first]);
      report_daq->set_id(i.first);
      report_daq->set_subsystem(names[i.first]);
      report_daq->set_rate(events[i.first] / dt);
      report_daq->set_pulser(pulses[i.first] / dt);
      report_daq->set_active(active);
#endif
      // For the rarely looked at ncurses UI
      if (!active)
      {
        mvwprintw(info._w, info._line, 0, "%8s\t%4x\t%8s    %18s          ", names[i.first].c_str(), i.first, buf, "NO DATA");
      }
      else
      {
        mvwprintw(info._w, info._line, 0, "%8s\t%4x\t%8s    %8.0f/s    %8.0f/s    ", names[i.first].c_str(), i.first, buf, events[i.first] / dt, pulses[i.first] / dt);
      }
      // Check the DAQ Sync "enum"/flag
      if (daq_sync[i.first] == 1)
      {
        wcolor_set(info._w, 3, NULL);
        wprintw(info._w, "%12s", "OK");
#ifdef ZEROMQ
        report_daq->set_correlation(frs_monitor::DaqInformation::GOOD);
#endif
      }
      else if (daq_sync[i.first] == 2)
      {
        wcolor_set(info._w, 5, NULL);
        wprintw(info._w, "%12s", "BAD");
#ifdef ZEROMQ
        report_daq->set_correlation(frs_monitor::DaqInformation::BAD);
#endif
      }
      else
      {
        wcolor_set(info._w, 4, NULL);
        wprintw(info._w, "%12s", "N/A");
#ifdef ZEROMQ
        report_daq->set_correlation(frs_monitor::DaqInformation::UNKNOWN);
#endif
      }
      wcolor_set(info._w, 2, NULL);
    }
    info._line += 1;
  }
  wrefresh(info._w);

#ifdef ZEROMQ
  zmq_calculate_scalers();
#endif

  // Could ncurses some scalers here, like DESPEC
  // But probably no point, prefer web UI

  extern watcher_window _watcher;

#ifdef ZEROMQ
  extern std::deque<std::pair<std::string, int>> errors;
  report.clear_logs();
  for (auto& e : errors)
  {
    auto log = report.add_logs();
    log->set_message(e.first);
    log->set_severity(frs_monitor::LogEntry::LogSeverity::LogEntry_LogSeverity_NORMAL);
    if (e.second == FE_ERROR)
    {
      log->set_severity(frs_monitor::LogEntry::LogSeverity::LogEntry_LogSeverity_ERROR);
    }
    if (e.second == FE_WARNING)
    {
      log->set_severity(frs_monitor::LogEntry::LogSeverity::LogEntry_LogSeverity_WARNING);
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

// This happens every second (TIMEOUT) and used to reset the periodic stats
// (aka rates)
void frs_monitor_watcher_clear()
{
  events.clear();
  pulses.clear();
  frs_trloii_old = frs_trloii_now;
  frs_trloii_all_old = frs_trloii_all_now;
  __monitor_last = _monitor_now;

}

// A new callback from ucesb core (requires patches)
// if dead is true, ucesb hasn't received anything from its input
void frs_monitor_watcher_keepalive(bool dead)
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

