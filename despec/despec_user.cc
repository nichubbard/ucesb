
#include "structures.hh"

#include "user.hh"

#include <curses.h> // needed for the COLOR_ below

#include "../watcher/watcher_channel.hh"
#include "../watcher/watcher_window.hh"

#include <map>
#include "../lu_common/colourtext.hh"

static int _events = 0;
std::map<int, long> events;
std::map<int, long> pulses;
std::map<int, long> events_total;
time_t _despec_last = 0;
time_t _despec_now = 0;
std::map<int, int64_t> pulsers;
std::map<std::pair<int, int>, bool> sync_ok;
std::map<std::pair<int, int>, int> sync_bad;

std::map<int, std::string> names =
{
  { 0x100, "FRS" },
  { 0x400, "GALILEO" },
  { 0x500, "bPlas" },
  { 0x700, "AIDA" },
  { 0x1200, "FINGER" },
  { 0x1500, "FATIMA" },
};

std::vector<std::string> scalers =
{
  "GALILEO Free",
  "bPlas 2",
  "bPlas 1",
  "FATIMA Accepted",
  "Pulser",
  "AIDA 1",
  "AIDA 2",
  "AIDA 3",
  "SC41 L",
  "SC41 R",
  "FATIMA Free",
  "bPlas 1&2",
  "GALILEO Accepted"
};

std::vector<int> scaler_order =
{
  0,
  12,
  10,
  3,
  8,
  9,
  2,
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
            }
            else
            {
              if(++sync_bad[pair] > 20)
              {
                sync_ok[pair] = false;
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

  mvwprintw(info._w, info._line, 0, "%8s\t%4s\t%8s\t%10s\t%10s", "Detector", "ID", "Events", "Rate", "Pulser Evts");
  info._line++;

  int dt = (int)(_despec_now - _despec_last);

  char buf[256] = { '\0' };
  for(auto& i: events_total)
  {
    format_long_int(buf, events_total[i.first]);
    mvwprintw(info._w, info._line, 0, "%8s\t%4x\t%8s\t%8ld/s\t%8ld/s ", names[i.first].c_str(), i.first, buf, events[i.first] / dt, pulses[i.first] / dt);
    info._line += 1;
  }
  wrefresh(info._w);

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
}

void despec_watcher_clear()
{
  events.clear();
  pulses.clear();
  _despec_last = _despec_now;
  scalers_old = std::move(scalers_now);
  scalers_now.resize(16);
  //sync_ok.clear();
}
