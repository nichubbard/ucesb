
#include "structures.hh"

#include "user.hh"

#include <curses.h> // needed for the COLOR_ below

#include "../watcher/watcher_channel.hh"
#include "../watcher/watcher_window.hh"

#include <map>
#include "../lu_common/colourtext.hh"

#include "../file_input/lmd_source_multievent.hh"
#include <atomic>

#ifdef ZEROMQ
#include "ucesb.pb.h"
#include <zmqpp/zmqpp.hpp>
#undef OK
#include <google/protobuf/util/json_util.h>

static despec::UcesbReport report;
static zmqpp::context zmq_context;
static zmqpp::socket zmq_pubber(zmq_context, zmqpp::socket_type::pub);
extern std::atomic<int> _global_clients;
#endif

static int _events = 0;
std::map<int, long> events;
std::map<int, long> pulses;
std::map<int, long> events_total;
int64_t _despec_last = 0;
int64_t _despec_now = 0;
std::map<int, int64_t> pulsers;
std::map<std::pair<int, int>, bool> sync_ok;
std::map<std::pair<int, int>, int> sync_bad;
std::map<int, int> daq_sync;
// spil data
static bool _on_spill = false;
static time_t _last_spill = 0;

#define AIDA_IMPLANT_MAGIC 0x701

std::map<int, std::string> names =
{
  { 0x100 , "FRS" },
  { 0x400 , "HPGe" },
  { 0x500 , "bPlas" },
  { 0x700 , "AIDA" },
  { 0x1200, "FINGER" },
  { 0x1500, "FAT. VME" },
  { 0x1600, "FAT. TMX" },
};

std::vector<std::string> scalers =
{
  "bPlast Free",
  "bPlast Accepted",
  "FATIMA TAMEX Free",
  "FATIMA TAMEX Accepted",
  "FATIMA VME Free",
  "FATIMA VME Accepted",
  "Ge Free",
  "Ge Accepted",
  "bPlast Up",
  "bPlast Down",
  "bPlast AND",
  "SCI41 L",
  "SCI41 R",
  "SCI42 L",
  "SCI42 R"
};

std::vector<int> scaler_order =
{
  0, 1,
  2, 3,
  4, 5,
  6, 7,
  8, 9,
  10, -1,
  11, 12,
  13, 14
};

// 16 FATIMA scalers and 64 FRS scalers
static constexpr size_t SCALER_COUNT = 16 + 32 + 32;

std::vector<uint32_t> scalers_now(SCALER_COUNT);
std::vector<uint32_t> scalers_old(SCALER_COUNT);
std::vector<uint32_t> scalers_old_spill(SCALER_COUNT);

watcher_type_info despec_watch_types[NUM_WATCH_TYPES] =
{
  { COLOR_GREEN,   "Physics" },
  { COLOR_YELLOW,  "Pulser" },
};

std::map<int, int> aida_dssd_map =
{
  {1, 1},
  {2, 1},
  {3, 1},
  {4, 1},

  {5, 2},
  {6, 2},
  {7, 2},
  {8, 2},

  {9, 3},
  {10, 3},
  {11, 3},
  {12, 3},
};

void despec_watcher_event_info(watcher_event_info *info,
    unpack_event *event)
{
  info->_type = DESPEC_WATCH_TYPE_PHYSICS;
  bool pulse = false;

  if (event->trigger == 3)
  {
    info->_type = DESPEC_WATCH_TYPE_TCAL;
    pulse = true;
  }

  if (event->frs_tpat.tpat.n == 1 && event->frs_tpat.tpat.tpat[0].value & (1 << 8))
  {
    info->_type = DESPEC_WATCH_TYPE_TCAL;
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
    _despec_now = wr;
    report.mutable_summary()->set_time(info->_time);
    report.mutable_summary()->set_wr(wr);
  }

  for (uint i = 0; i < event->fatima.scaler.scalars._num_items; i++)
  {
    scalers_now[i] = event->fatima.scaler.scalars[i];
  }

  for (uint i = 0; i < event->frs_frs.scaler.scalers._num_items; i++)
  {
    scalers_now[16 + i] = event->frs_frs.scaler.scalers[i];
  }

  // START EXTR Scaler triggered, so we reset the spill array and say on spill
  if (scalers_now[16 + 8] - scalers_old_spill[16 + 8] > 0) {
    _on_spill = true;
    _last_spill = (uint)(_despec_now / (uint64_t)1e9);
    scalers_old_spill = scalers_now;
    _AIDA_WATCHER_STATS->clear(1);
  }

  // STOP EXTR Scaler triggered, spill off flag
  if (scalers_now[16 + 9] - scalers_old_spill[16 + 9] > 0) {
    _on_spill = false;
  }

  for (uint i = 0; i < event->frs_main.scaler.scalers._num_items; i++)
  {
    scalers_now[16 + 32 + i] = event->frs_main.scaler.scalers[i];
  }

  for (uint i = 0; i < event->wr.size(); i++)
  {
    if (event->wr[i].first == 0x200) continue;
    if (pulse)
    {
      int id = event->wr[i].first;
      int64_t wr = event->wr[i].second;
      pulses[id]++;
      int64_t prev_ts = pulsers[id];
      if (prev_ts != 0)
      {
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
      events[event->wr[i].first]++;
      events_total[event->wr[i].first]++;

      if (event->is_aida && event->aida_implant)
      {
        events[AIDA_IMPLANT_MAGIC]++;
        events_total[AIDA_IMPLANT_MAGIC]++;
      }
    }
  }

  report.mutable_summary()->set_event_no(info->_event_no);
  //_inputs
  report.mutable_summary()->set_server(_inputs[0]._name);
  report.mutable_summary()->set_onspill(_on_spill);
  report.mutable_summary()->set_lastspill(_last_spill);

  _events++;
}

void despec_watcher_init()
{
  extern watcher_window _watcher;
  _watcher._display_channels.clear();
  _watcher._present_channels.clear();
  init_pair(5, COLOR_RED, COLOR_BLACK);

  //extern aidaeb_watcher_stats* _AIDA_WATCHER_STATS;
  _AIDA_WATCHER_STATS = new aidaeb_watcher_stats(2);
  _AIDA_WATCHER_STATS->load_map(aida_dssd_map);

  INFO("DESPEC Initialised");

#ifdef ZEROM
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  INFO("Google Protobuf Version Verified OK");
  zmq_pubber.bind("tcp://*:4242");
  INFO("ZeroMQ PUB socket running on tcp://*:4242");

  std::atexit([]() {
      zmqpp::message message;
      message << "exit";
      zmq_pubber.send(message);
  });

  report.mutable_summary()->set_server(_inputs[0]._name);

  sleep(1);

  zmqpp::message message;
  std::string proto;
  report.SerializeToString(&proto);
  message << "init" << proto;
  zmq_pubber.send(message);
#endif
}

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

void despec_watcher_display(watcher_display_info& info)
{
  if (info._line > info._max_line)
    return;

  wcolor_set(info._w, info._col_norm, NULL);
  wmove(info._w, info._line, 0);

  werase(info._w);

  whline(info._w, ACS_HLINE, 80);
  mvwaddstr(info._w, info._line, 1, "DESPEC DAQ Status");
  //mvwprintw(info._w, info._line + 1, 0, "Events: %08d", _events);

  info._line += 1;

  if (_despec_now == _despec_last)
  {
    _despec_last--;
  }

  mvwprintw(info._w, info._line, 0, "%8s\t%4s\t%8s    %10s    %10s    %12s", "Detector", "ID", "Events", "Rate", "Pulser", "Correlation");
  info._line++;

  //int dt = (int)(_despec_now - _despec_last);
  double dt = (_despec_now - _despec_last) / (double)1e9;

  char buf[256] = { '\0' };
  report.mutable_status()->clear_daq();
  for(auto& i: events_total)
  {

    format_long_int(buf, events_total[i.first]);
    if (i.first == AIDA_IMPLANT_MAGIC)
    {
      continue;
      //mvwprintw(info._w, info._line, 0, "%8s%12s\t%8s    %8ld/s    ", "", "(Implants)", buf, events[i.first] / dt);
    }
    else
    {
      auto report_daq = report.mutable_status()->add_daq();
      report_daq->set_events(events_total[i.first]);
      report_daq->set_id(i.first);
      report_daq->set_subsystem(names[i.first]);
      report_daq->set_rate(events[i.first] / dt);
      report_daq->set_pulser(pulses[i.first] / dt);
      mvwprintw(info._w, info._line, 0, "%8s\t%4x\t%8s    %8.0f/s    %8.0f/s    ", names[i.first].c_str(), i.first, buf, events[i.first] / dt, pulses[i.first] / dt);
      if (daq_sync[i.first] == 1)
      {
        wcolor_set(info._w, 3, NULL);
        wprintw(info._w, "%12s", "OK");
        report_daq->set_correlation(despec::DaqInformation::GOOD);
      }
      else if (daq_sync[i.first] == 2)
      {
        wcolor_set(info._w, 5, NULL);
        wprintw(info._w, "%12s", "BAD");
        report_daq->set_correlation(despec::DaqInformation::BAD);
      }
      else
      {
        wcolor_set(info._w, 4, NULL);
        wprintw(info._w, "%12s", "N/A");
        report_daq->set_correlation(despec::DaqInformation::UNKNOWN);
      }
      wcolor_set(info._w, 2, NULL);
    }
    info._line += 1;
  }
  wrefresh(info._w);

  report.clear_scalers();

  info._line++;
  wmove(info._w, info._line, 0);
  whline(info._w, ACS_HLINE, 80);
  mvwaddstr(info._w, info._line, 1, "VME Scalers");

  auto fatvme_report = report.add_scalers();
  fatvme_report->set_key("fatima");
  fatvme_report->clear_scalers();
  for (size_t i = 0; i < 16; i++)
  {
    auto entry = fatvme_report->add_scalers();
    entry->set_index(i);
    entry->set_rate((double)(scalers_now[i] - scalers_old[i]) / dt);
    entry->set_spill(scalers_now[i] - scalers_old_spill[i]);
  }

  auto frs_report = report.add_scalers();
  frs_report->set_key("frs");
  frs_report->clear_scalers();
  for (size_t i = 0; i < 64; i++)
  {
    auto entry = frs_report->add_scalers();
    entry->set_index(i);
    entry->set_rate((double)(scalers_now[i + 16] - scalers_old[i + 16]) / dt);
    entry->set_spill(scalers_now[i + 16] - scalers_old_spill[i + 16]);
  }

  for (size_t j = 0; j < scaler_order.size(); j++)
  {
    int i = scaler_order[j];
    int col =0 ;
    if (j % 2 == 1) col = 40;
    else info._line++;
    if (i == -1) continue;
    mvwprintw(info._w, info._line, col, "%21s = %8.0f Hz",
        scalers[i].c_str(),
        (double)(scalers_now[i] - scalers_old[i]) / dt
        );
  }
  wrefresh(info._w);

  if(_conf._enable_eventbuilder)
  {
    info._line++;
    info._line++;
    wmove(info._w, info._line, 0);
    whline(info._w, ACS_HLINE, 80);
    mvwaddstr(info._w, info._line, 1, "AIDA Rates");
    info._line++;
    mvwprintw(info._w, info._line, 0, "%18s      Implants", "");
    mvwprintw(info._w, info._line, 40, "Decays", "");
    auto const& im_hz = _AIDA_WATCHER_STATS->implants(0);
    auto const& de_hz = _AIDA_WATCHER_STATS->decays(0);
    auto const& im_sp = _AIDA_WATCHER_STATS->implants(1);
    auto const& de_sp = _AIDA_WATCHER_STATS->decays(1);
    auto aida_report = report.add_scalers();
    aida_report->set_key("aida");
    aida_report->clear_scalers();
    for (int j = 0; j < 3; j++)
    {
      auto entry = aida_report->add_scalers();
      entry->set_index(2 * j);
      entry->set_rate((double)im_hz[j] / dt);
      entry->set_spill(im_sp[j]);
      entry = aida_report->add_scalers();
      entry->set_index(2 * j + 1);
      entry->set_rate((double)de_hz[j] / dt);
      entry->set_spill(de_sp[j]);
      info._line++;
      mvwprintw(info._w, info._line, 0, "%17s %d    %8.0f Hz",
          "DSSD",
          j + 1,
          (double)im_hz[j] / dt
          );
      mvwprintw(info._w, info._line, 40, "%8.0f Hz",
          (double)de_hz[j] / dt
          );
    }
  }
  else
  {
    info._line++;
    info._line++;
    mvwaddstr(info._w, info._line, 1, "AIDA Event Bulder is not enabled");
  }

  extern watcher_window _watcher;
  extern std::deque<std::pair<std::string, int>> errors;

  report.clear_logs();
  for (auto& e : errors)
  {
    auto log = report.add_logs();
    log->set_message(e.first);
    log->set_severity(despec::LogEntry::LogSeverity::LogEntry_LogSeverity_NORMAL);
    if (e.second == FE_ERROR)
    {
      log->set_severity(despec::LogEntry::LogSeverity::LogEntry_LogSeverity_ERROR);
    }
    if (e.second == FE_WARNING)
    {
      log->set_severity(despec::LogEntry::LogSeverity::LogEntry_LogSeverity_WARNING);
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
}

void despec_watcher_clear()
{
  events.clear();
  pulses.clear();
  _despec_last = _despec_now;
  scalers_old = scalers_now;
  //sync_ok.clear();
  _AIDA_WATCHER_STATS->clear(0);

}

void despec_watcher_keepalive(bool dead)
{
  if (dead)
  {
    zmqpp::message message;
    report.clear_status();
    report.clear_scalers();
    std::string proto;
    report.SerializeToString(&proto);
    message << "dead" << proto;
    zmq_pubber.send(message);
  }
}
