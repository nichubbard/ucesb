
#include "structures.hh"

#include "user.hh"

#include <curses.h> // needed for the COLOR_ below

#include "../watcher/watcher_channel.hh"
#include "../watcher/watcher_window.hh"

#include <map>
#include "../lu_common/colourtext.hh"

#include "../file_input/lmd_source_multievent.hh"

static int _events = 0;
std::map<int, long> events;
std::map<int, long> pulses;
std::map<int, long> events_total;
time_t _despec_last = 0;
time_t _despec_now = 0;
std::map<int, int64_t> pulsers;
std::map<std::pair<int, int>, bool> sync_ok;
std::map<std::pair<int, int>, int> sync_bad;
std::map<int, int> daq_sync;

#define AIDA_IMPLANT_MAGIC 0x701

std::map<int, std::string> names =
{
  { 0x100 , "FRS" },
  { 0x400 , "GALILEO" },
  { 0x500 , "bPlas" },
  { 0x700 , "AIDA" },
  { 0x1200, "FINGER" },
  { 0x1500, "FAT. VME" },
  { 0x1600, "FAT. TMX" },
};

std::vector<std::string> scalers =
{
  "GALILEO Free",
  "bPlas 2",
  "---",
  "FATIMA Accepted",
  "Pulser",
  "AIDA 1",
  "AIDA 2",
  "AIDA 3",
  "SC41 L",
  "SC41 R",
  "FATIMA Free",
  "bPlas 1&2",
  "GALILEO Accepted",
  "bPlas 1",
};

std::vector<int> scaler_order =
{
  0,
  12,
  10,
  3,
  8,
  9,
  13,
  1,
  11,
  -1,
  5,
  6,
  7,
  -1,
  4
};

std::vector<uint32_t> scalers_now(16);
std::vector<uint32_t> scalers_old(16);

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

  if (event->frs_tpat.tpat.n == 1 && event->frs_tpat.tpat.tpat[0] & 0x200)
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
  if (event->sub.wr.ts_high[0])
  {
    info->_time = (uint)(((uint64_t)event->sub.wr.ts_high[0] << 32 | event->sub.wr.ts_low[0]) / (uint64_t)1e9);
    info->_info |= WATCHER_DISPLAY_INFO_TIME;
    _despec_now = info->_time;
  }

  for (uint i = 0; i < event->sub.dummy.scalars._num_items; i++)
  {
    scalers_now[i] = event->sub.dummy.scalars[i];
  }

  for (uint i = 0; i < event->sub.wr.ts_id._num_items; i++)
  {
    if (event->sub.wr.ts_id[i] == 0x200) continue;
    if (pulse)
    {
      int id = event->sub.wr.ts_id[i];
      int64_t wr = ((int64_t)event->sub.wr.ts_high[i] << 32 | event->sub.wr.ts_low[i]);
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
      events[event->sub.wr.ts_id[i]]++;
      events_total[event->sub.wr.ts_id[i]]++;

      if (event->is_aida && event->aida_implant)
      {
        events[AIDA_IMPLANT_MAGIC]++;
        events_total[AIDA_IMPLANT_MAGIC]++;
      }
    }
  }

  _events++;
}

void despec_watcher_init()
{
  extern watcher_window _watcher;
  _watcher._display_channels.clear();
  _watcher._present_channels.clear();
  init_pair(5, COLOR_RED, COLOR_BLACK);
  
  //extern aidaeb_watcher_stats* _AIDA_WATCHER_STATS;
  _AIDA_WATCHER_STATS = new aidaeb_watcher_stats;
  _AIDA_WATCHER_STATS->load_map(aida_dssd_map);
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

  int dt = (int)(_despec_now - _despec_last);

  char buf[256] = { '\0' };
  for(auto& i: events_total)
  {
    format_long_int(buf, events_total[i.first]);
    if (i.first == AIDA_IMPLANT_MAGIC)
    {
      mvwprintw(info._w, info._line, 0, "%8s%12s\t%8s    %8ld/s    ", "", "(Implants)", buf, events[i.first] / dt);
    }
    else
    {
      mvwprintw(info._w, info._line, 0, "%8s\t%4x\t%8s    %8ld/s    %8ld/s    ", names[i.first].c_str(), i.first, buf, events[i.first] / dt, (int)(((double)pulses[i.first] + 0.5) / dt));
      if (daq_sync[i.first] == 1)
      {
        wcolor_set(info._w, 3, NULL);
        wprintw(info._w, "%12s", "OK");
      }
      else if (daq_sync[i.first] == 2)
      {
        wcolor_set(info._w, 5, NULL);
        wprintw(info._w, "%12s", "BAD");
      }
      else
      {
        wcolor_set(info._w, 4, NULL);
        wprintw(info._w, "%12s", "N/A");
      }
      wcolor_set(info._w, 2, NULL);
    }
    info._line += 1;
  }
  wrefresh(info._w);

#if 0

  info._line += 1;
  wmove(info._w, info._line, 0);
  whline(info._w, ACS_HLINE, 80);
  mvwaddstr(info._w, info._line, 1, "Pulser Correlation");
  info._line += 1;

  wmove(info._w, info._line, 0);
  wprintw(info._w, "%8s | %8s | %12s", "Det. 1", "Det. 2", "Correlation");
  //wprintw(info._w, "%8d", info._line);
  info._line += 1;


  for (auto& i : sync_bad)
  {
    mvwprintw(info._w, info._line, 0, "%8s | %8s | ", names[i.first.first].c_str(), names[i.first.second].c_str());
    if (!sync_ok[i.first] && sync_bad[i.first] < 20)
    {
      wcolor_set(info._w, 4, NULL);
      wprintw(info._w, "%12s", "N/A");
    }
    else if (sync_ok[i.first])
    {
      wcolor_set(info._w, 3, NULL);
      wprintw(info._w, "%12s", "OK");
    }
    else
    {
      wcolor_set(info._w, 5, NULL);
      wprintw(info._w, "%12s", "BAD");
    }
    wcolor_set(info._w, 2, NULL);
    info._line++;
  }

#endif

  info._line++;
  wmove(info._w, info._line, 0);
  whline(info._w, ACS_HLINE, 80);
  mvwaddstr(info._w, info._line, 1, "VME Scalers");

  for (int j = 0; j < scaler_order.size(); j++)
  {
    int i = scaler_order[j];
    int col =0 ;
    if (j % 2 == 1) col = 40;
    else info._line++;
    if (i == -1) continue;
    mvwprintw(info._w, info._line, col, "%20s = %8.0f Hz",
        scalers[i].c_str(),
        (double)(scalers_now[i] - scalers_old[i]) / dt
        );
  }
  wrefresh(info._w);
  
  info._line++;
  info._line++;
  wmove(info._w, info._line, 0);
  whline(info._w, ACS_HLINE, 80);
  mvwaddstr(info._w, info._line, 1, "AIDA Rates");
  info._line++;
  mvwprintw(info._w, info._line, 0, "%18s      Implants", "");
  mvwprintw(info._w, info._line, 40, "Decays", "");
  auto const& im = _AIDA_WATCHER_STATS->implants();
  auto const& de = _AIDA_WATCHER_STATS->decays();
  for (int j = 0; j < 3; j++)
  {
    info._line++;
    mvwprintw(info._w, info._line, 0, "%17s %d    %8.0f Hz",
        "DSSD",
        j + 1,
        (double)im[j] / dt
        );
    mvwprintw(info._w, info._line, 40, "%8.0f Hz",
        (double)de[j] / dt
        );
  }
}

void despec_watcher_clear()
{
  events.clear();
  pulses.clear();
  _despec_last = _despec_now;
  scalers_old = std::move(scalers_now);
  scalers_now.resize(16);
  //sync_ok.clear();
  _AIDA_WATCHER_STATS->clear();
  
}
