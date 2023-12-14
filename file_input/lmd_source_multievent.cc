#include <algorithm>
#include <map>

#include "lmd_source_multievent.hh"
#include "lmd_input.hh"
#include "config.hh"
#include <inttypes.h>
#include <sstream>
#include <iomanip>

#define AIDA_PROCID 1
#define AIDA_TIME_SHIFT 14000
#define AIDA_CORRELATION_PULSER 1

// Allow timewarp messages to be an error or warning
//#define TIMEWARP ERROR
#define TIMEWARP WARNING

// "Optimise" AIDA output by discarding unnecessary SYNC items
//  and discarding tiny (useless) AIDA events
#define AIDA_OPTIMISE

// The following defs are automatic computations

aidaeb_watcher_stats* _AIDA_WATCHER_STATS = nullptr;

// someone thought it a good idea to use uint32 where xe ought to have used size_t.
#define WRTS_SIZE (uint32_t)sizeof(wrts_header)
//
// Debugging
#define MEMORY_REPORT 0

// This is the value of an AIDA correlation word for the pulser
#ifdef AIDA_CORRELATION_PULSER
static constexpr uint32_t AIDA_CORRELATION_EVENT = 0x80000000 | ((AIDA_CORRELATION_PULSER - 1) << 24) | (8 << 20);
#endif
static constexpr uint32_t AIDA_SCALER_EVENT = 0x80800000;
static constexpr uint32_t AIDA_SCALER_MASK = 0xC0FF0000;

#ifdef _ENABLE_TRACE
// Don't define this function if we don't use it :)
// Generate a nice text string from an AIDA data item
std::string print_aida(uint32_t word1, uint32_t word2);
#endif

// These suck but it is what it is
sint32 l_count = 0;
lmd_event_10_1_host input_event_header;

lmd_event *lmd_source_multievent::get_event()
{
get_event_retry:
  _TRACE("lmd_source_multievent::get_event()\n");

  if(!_conf._enable_eventbuilder)
    return lmd_source::get_event();

  _TRACE("the events queue has %lu entries\n", events.size());

  if (events.size() > 1000000) {
    ERROR("ucesb had over 1 million events in the backlog");
  }

  if (events.size() > 0)
  {
    // Get the first entry (sorted) to emit
    event_entry* first = events.top();
    _TRACE("First event in queue is %p:%16lx\n", first, first->timestamp);
    // Check first that the fragments should come out and try to complete it
    if (cur_aida != nullptr && cur_aida->timestamp < first->timestamp)
    {
      _TRACE(" loading more events to complete the AIDA fragment %16lx\n", cur_aida->timestamp);
      file_status_t s;
      while ((s = load_events()) != aida_event && s != eof && load_event_wr - cur_aida->timestamp < _conf._eventbuilder_window);
      if (cur_aida && cur_aida->fragment) {
        // Unfragment the event, store it and continue
        cur_aida->fragment = false;
        events.push(cur_aida);
        cur_aida = nullptr;
        goto get_event_retry;
      }
    }
    if (cur_dtas != nullptr && cur_dtas->timestamp < first->timestamp)
    {
       _TRACE(" loading more events to complete the DTAS fragment %16lx\n", cur_dtas->timestamp);
      file_status_t s;
      while ((s = load_events()) != dtas_event && s != eof && load_event_wr - cur_dtas->timestamp < _conf._eventbuilder_window);
      if (cur_dtas && cur_dtas->fragment) {
        // Unfragment the event, store it and continue
        cur_dtas->fragment = false;
        events.push(cur_dtas);
        cur_dtas = nullptr;
        goto get_event_retry;
      }
    }
    // Only emit if it's before the last MBS timestamp and the aida time shift (to ensure we know the order of things)
    if (first->timestamp < last_mbs_wr - AIDA_TIME_SHIFT)
    {
      _TRACE("Emitting event with timestamp %16lx\n", first->timestamp);
      // Event removed from queue as it's going to be emitted (or discarded)
      events.pop();
      // EMIT here
      if (first->timestamp >= emit_wr)
      {
        if (emit_skip)
          TIMEWARP("Recovered from timewarp but skipped %d event(s)", emit_skip);
        emit_skip = 0;
        emit_wr = first->timestamp;
        return first->emit();
      }
      else
      {
        if (!emit_skip)
            TIMEWARP("=> Not emitting timewarped event (%16lx before %16lx)", first->timestamp, emit_wr);
          emit_skip++;
          first->return_to_pool();
          lmd_source::release_events();
          goto get_event_retry;
      }
    }
    else
    {
      _TRACE("Not emitting event until we have had more MBS events (waiting for %16lx < %16lx)\n", first->timestamp, last_mbs_wr - AIDA_TIME_SHIFT);
    }
  }

  if (_conf._max_events == 1)
  {
    return NULL;
  }


  file_status_t status = load_events();
  if(status == other_event || status == aida_event || status == dtas_event)
  {
    _TRACE("Loaded an event so retrying to emit\n");
    goto get_event_retry;
  }
  else if (status == eof)
  {
    _TRACE("=> EOF reached, %lu / %lu\n", events.size(), 0UL);
    if (_input._input->_filename != _inputs.back()._name)
    {
      _TRACE("Returning EOF to move to next file\n");
      return NULL;
    }
    _TRACE("Nothing left, end of event input\n");
    // First unfragment the current built AIDA
    if (cur_aida && cur_aida->fragment)
    {
      _TRACE("Unfragmented final AIDA event");
      cur_aida->fragment = false;
      events.push(cur_aida);
      cur_aida = nullptr;
    }
    // First unfragment the current built DTAS
    if (cur_dtas && cur_dtas->fragment)
    {
      _TRACE("Unfragmented final DTAS event");
      cur_dtas->fragment = false;
      events.push(cur_dtas);
      cur_dtas = nullptr;
    }
    // Now empty the events quque
    if (events.size() > 0)
    {
#if MEMORY_REPORT
      INFO(0, "At EOF the event queue was %zu long", events.size());
#endif
      last_mbs_wr = 0x7fffffffffffffffL;
      goto get_event_retry;
    }
    // Finally return true EOF to end the procedure
    return NULL;
  }

  ERROR("Unknown event found");
  return NULL;
}

lmd_source_multievent::file_status_t lmd_source_multievent::load_events()  ///////////////////////////////
{
  _TRACE("lmd_source_multievent::load_events()\n");

  lmd_subevent *se;

  _file_event.release();

  _TRACE("lmd_source::get_event()\n");

  // Read the next event from the input
  if (!lmd_source::get_event())
  return eof;

  _TRACE("Get 10:1 info\n");

  _file_event.get_10_1_info();
  if(_file_event._header._header.i_type != 10 || _file_event._header._header.i_subtype != 1)
  {
    _TRACE("=> return unknown_event\n");
    return unknown_event;
  }

  _TRACE("Locating subevents\n");

  _file_event.locate_subevents(&event_hint);

  #if _ENABLE_TRACE
  //_file_event.print_event(0, NULL);
  _TRACE(" Event: Number: %d, Trigger :%d\n", _file_event._header._info.l_count, _file_event._header._info.i_trigger);
  #endif

  if(_file_event._nsubevents == 0)
  {
    _TRACE("-> return unknown_events (_nsubevents = 0)\n");
    return unknown_event;
  }

  for(int i = 0; i < _file_event._nsubevents; i++)
  {
    se = &(_file_event._subevents[i]);
    _TRACE("  Subevent: Type: %d, Subtype: %d, ProcID: %d\n", se->_header._header.i_type,
    se->_header._header.i_subtype, se->_header.i_procid);

    // Try to find a WR Timestamp
    char *pb_start, *pb_end;
    _file_event.get_subevent_data_src(se, pb_start, pb_end);
    uint32_t *pl_start = reinterpret_cast<uint32_t*>(pb_start);
    int64_t mbs_wr = 0;

    // Empty subevents are destroyed (I think this is only for the FDR)
    if (pb_end - pb_start < 2)
      continue;

    if ((pl_start[1] & 0xFFFF0000) == 0x03e10000 && (pl_start[2] & 0xFFFF0000) == 0x04e10000
      && (pl_start[3] & 0xFFFF0000) == 0x05e10000 && (pl_start[4] & 0xFFFF0000) == 0x06e10000)
    {
      uint32_t procID = pl_start[0];
      if (procID != 0x200)
      {
        load_event_wr = pl_start[1] & 0xffff;
        load_event_wr |= (pl_start[2] & 0xffff) << 16;
        load_event_wr |= (uint64_t)(pl_start[3] & 0xffff) << 32;
        load_event_wr |= (uint64_t)(pl_start[4] & 0xffff) << 48;
        mbs_wr = load_event_wr;
        last_mbs_wr = load_event_wr;
        _TRACE("lmd_source_multievent::load_events(): WR subevent with ProcID: %d\n", se->_header.i_procid);
        _TRACE("lmd_source_multievent::load_events(): WR %lx\n", load_event_wr);
      }
    }

    if (se->_header.i_procid == _conf._eventbuilder_procid)
    {
      load_aida(se, pb_start, pb_end, mbs_wr);
      return aida_event;
    }

    if (_conf._enable_dtas && se->_header.i_procid == 37)
    {
      load_dtas(se, pb_start, pb_end, mbs_wr);
      return dtas_event;
    }
  }

  _TRACE("lmd_source_multievent::load_events(): identified an other block\n");

  triggerevent_entry* entry = pool_new(trigger_events_pool);
  entry->timestamp = load_event_wr;
  entry->event_no =  _file_event._header._info.l_count;

#if BPLAST_DELAY_FIX
  if (se->_header.i_procid == 80)
  {
    _TRACE("Fixing plastic by copying old data :)\n");

    if (plastic_buffer.event._nsubevents > 0)
    {
      _TRACE("Fixing plastic by copying old data 2\n");

      entry.event._header = plastic_buffer.event._header;
      entry.event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
      entry.event._swapping = plastic_buffer.event._swapping;
      entry.event._nsubevents = plastic_buffer.event._nsubevents;

      entry.event._subevents = (lmd_subevent*)entry.event._defrag_event.allocate(entry.event._nsubevents * sizeof(lmd_subevent));
      for (int i = 0; i < entry.event._nsubevents; i++)
      {
        size_t nsubev = SUBEVENT_DATA_LENGTH_FROM_DLEN(plastic_buffer.event._subevents[i]._header._header.l_dlen);
        entry.event._subevents[i]._header = plastic_buffer.event._subevents[i]._header;
        entry.event._subevents[i]._data = (char*)entry.event._defrag_event_many.allocate(nsubev);
        memcpy(entry.event._subevents[i]._data, plastic_buffer.event._subevents[i]._data, nsubev);

        _TRACE("Copying actual WR timestamp into MBS data\n");
        wrts_header wr(entry.timestamp);
        memcpy(&entry.event._subevents[0]._data[4], &wr.lower16, sizeof(wr) - sizeof(uint32_t));
      }
    }

    _TRACE("Copying plastic data to a buffer\n");
    plastic_buffer.event.release();
    plastic_buffer.event._header = _file_event._header;
    plastic_buffer.event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
    plastic_buffer.event._swapping = _file_event._swapping;
    plastic_buffer.event._nsubevents = _file_event._nsubevents;
    // allocate subevent array
    plastic_buffer.event._subevents = (lmd_subevent*)plastic_buffer.event._defrag_event.allocate(plastic_buffer.event._nsubevents * sizeof(lmd_subevent));
    // copy subevents over
    for (int i = 0; i < plastic_buffer.event._nsubevents; i++)
    {
      size_t nsubev = SUBEVENT_DATA_LENGTH_FROM_DLEN(_file_event._subevents[i]._header._header.l_dlen);
      plastic_buffer.event._subevents[i]._header = _file_event._subevents[i]._header;
      plastic_buffer.event._subevents[i]._data = (char*)plastic_buffer.event._defrag_event_many.allocate(nsubev);
      memcpy(plastic_buffer.event._subevents[i]._data, _file_event._subevents[i]._data, nsubev);
    }

    if (entry->event._nsubevents > 0)
    {
      return other_event;
    }
    else
    {
      _TRACE("First plastic event is only buffered\n");
      entry->reset();
      trigger_events_pool.push_back(entry);
      trigger_event.pop_back();
      return load_events();
    }
  }
#endif

#if FATIMA_DELAY_FIX
  if (se->_header.i_procid == 75)
  {
    _TRACE("Fixing fatima by copying old data :)\n");

    if (fatima_buffer.event._nsubevents > 0)
    {
      _TRACE("Fixing fatima by copying old data 2\n");

      entry.event._header = fatima_buffer.event._header;
      entry.event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
      entry.event._swapping = fatima_buffer.event._swapping;
      entry.event._nsubevents = fatima_buffer.event._nsubevents;

      entry.event._subevents = (lmd_subevent*)entry.event._defrag_event.allocate(entry.event._nsubevents * sizeof(lmd_subevent));
      for (int i = 0; i < entry.event._nsubevents; i++)
      {
        size_t nsubev = SUBEVENT_DATA_LENGTH_FROM_DLEN(fatima_buffer.event._subevents[i]._header._header.l_dlen);
        entry.event._subevents[i]._header = fatima_buffer.event._subevents[i]._header;
        entry.event._subevents[i]._data = (char*)entry.event._defrag_event_many.allocate(nsubev);
        memcpy(entry.event._subevents[i]._data, fatima_buffer.event._subevents[i]._data, nsubev);

        _TRACE("Copying actual WR timestamp into MBS data\n");
        wrts_header wr(entry.timestamp);
        memcpy(&entry.event._subevents[0]._data[4], &wr.lower16, sizeof(wr) - sizeof(uint32_t));
      }
    }

    _TRACE("Copying fatima data to a buffer\n");
    fatima_buffer.event.release();
    fatima_buffer.event._header = _file_event._header;
    fatima_buffer.event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
    fatima_buffer.event._swapping = _file_event._swapping;
    fatima_buffer.event._nsubevents = _file_event._nsubevents;
    // allocate subevent array
    fatima_buffer.event._subevents = (lmd_subevent*)fatima_buffer.event._defrag_event.allocate(fatima_buffer.event._nsubevents * sizeof(lmd_subevent));
    // copy subevents over
    for (int i = 0; i < fatima_buffer.event._nsubevents; i++)
    {
      size_t nsubev = SUBEVENT_DATA_LENGTH_FROM_DLEN(_file_event._subevents[i]._header._header.l_dlen);
      fatima_buffer.event._subevents[i]._header = _file_event._subevents[i]._header;
      fatima_buffer.event._subevents[i]._data = (char*)fatima_buffer.event._defrag_event_many.allocate(nsubev);
      memcpy(fatima_buffer.event._subevents[i]._data, _file_event._subevents[i]._data, nsubev);
    }

    if (entry->event._nsubevents > 0)
    {
      return other_event;
    }
    else
    {
      _TRACE("First fatima event is only buffered\n");
      entry->reset();
      trigger_events_pool.push_back(entry);
      trigger_event.pop_back();
      return load_events();
    }
  }
#endif

  // Copy the data over to ensure ownership of pointerss
  entry->event._header = _file_event._header;
  entry->event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
  entry->event._swapping = _file_event._swapping;
  entry->event._nsubevents = _file_event._nsubevents;
  // allocate subevent array
  entry->event._subevents = (lmd_subevent*)entry->event._defrag_event.allocate((size_t)entry->event._nsubevents * sizeof(lmd_subevent));
  // copy subevents over
  for (int i = 0; i < entry->event._nsubevents; i++)
  {
    size_t nsubev = SUBEVENT_DATA_LENGTH_FROM_DLEN(_file_event._subevents[i]._header._header.l_dlen);
    entry->event._subevents[i]._header = _file_event._subevents[i]._header;
    entry->event._subevents[i]._data = (char*)entry->event._defrag_event_many.allocate(nsubev);
    memcpy(entry->event._subevents[i]._data, _file_event._subevents[i]._data, nsubev);
  }

  events.push(entry);

  //_TRACE("Other Buffer now contains: %lu events\n", trigger_event.size());

  return other_event;
}

void lmd_source_multievent::load_aida(lmd_subevent *se, char* pb_start, char* pb_end, int64_t mbs_wr)
{
  _TRACE("lmd_source_multievent::load_events(): identified an AIDA block... expanding\n");

      input_event_header = _file_event._header;

      // We are to continue with a fragmented block
      if (cur_aida != nullptr && cur_aida->fragment)
      {
        _TRACE(" found an AIDA fragment to continue with: %16lx\n", cur_aida->fragment_wr);
      }
      else
      {
        _TRACE(" new AIDA event %16lx\n", load_event_wr);
        cur_aida = pool_new(aida_events_pool);
        cur_aida->_header = se->_header;
        cur_aida->timestamp = load_event_wr - AIDA_TIME_SHIFT;
        cur_aida->fragment_wr = load_event_wr;
      }

      uint32_t *pl_start = reinterpret_cast<uint32_t*>(pb_start);
      uint32_t *pl_end = reinterpret_cast<uint32_t*>(pb_end);

      // Advance past the extracted WR data
      uint32_t* pl_data = pl_start + 5;

      int64_t old_ts = cur_aida->fragment_wr;
      load_event_wr = old_ts;

      // bad hack: if the MBS & first AIDA WR match the MBS provides the full timestamp
      // otherwise it's probably a residual from the last MBS block and use the fragment's WR
      if ((pl_data[1] & 0x0fffffff) == (mbs_wr & 0x0fffffff))
      {
        load_event_wr = mbs_wr;
      }

      while(pl_data < pl_end)
      {
        uint32_t word1 = *pl_data++;
        uint32_t word2 = *pl_data++;

#if _AIDA_DUMP
        _TRACE(" aida event %08x %08x | %s\n", word1, word2, print_aida(word1, word2).c_str());
#endif

        load_event_wr &= ~0x0fffffff;
        load_event_wr |= (word2 & 0x0fffffff);

        // WR timestamp marker
        if ((word1 & 0xC0F00000) == 0x80500000)
        {
          uint32_t middleTS_raw = *pl_data++;
          uint32_t middleTS_low = *pl_data++;

#if _AIDA_DUMP
          _TRACE(" aida event %08x %08x | %s\n", middleTS_raw, middleTS_low, print_aida(middleTS_raw, middleTS_low).c_str());
#endif
          if ((middleTS_raw & 0xC0F00000) != 0x80400000)
          {
              ERROR("AIDA Corrupt WR Timestamp (High bits not followed by middle bits)");
              return;
          }
          uint32_t highTS = word1 & 0x000FFFFF;
          uint32_t middleTS = middleTS_raw & 0x000FFFFF;
          uint32_t old_highTS = (load_event_wr >> 48) & 0xFFFFF;
          uint32_t old_middleTS = (load_event_wr >> 28) & 0xFFFFF;
#ifdef AIDA_OPTIMISE
          if (highTS != old_highTS || middleTS != old_middleTS)
#endif
          {
            _TRACE(" pushing SYNC data because it actually changed\n");
            cur_aida->data.push_back(word1);
            cur_aida->data.push_back(word2);
            cur_aida->data.push_back(middleTS_raw);
            cur_aida->data.push_back(middleTS_low);
          }

          load_event_wr = (int64_t)(((uint64_t)highTS << 48) | ((uint64_t)middleTS << 28) | word2);
          //_TRACE(" event time: %16" PRIx64 "\n", load_event_wr);
          //cur_aida->fragment_wr = load_event_wr;
          continue;
        }

        // PAUSE/RESUME markers also update the middle timestamp
        if ((word1 & 0xC0F00000) == 0x80200000 || (word1 & 0xC0F00000) == 0x80300000)
        {
            uint64_t highTS = (load_event_wr >> 48) & 0xFFFFF;
            uint64_t middleTS = word1 & 0x000FFFFF;
            load_event_wr = (int64_t)(((uint64_t)highTS << 48) | ((uint64_t)middleTS << 28) | word2);
        }

        if ((word1 & 0xF0000000) == 0xD0000000)
        {
          cur_aida->flags |= 1;
          cur_aida->implant_wr_e = load_event_wr;
          int channelID = (word1 >> 16) & 0xFFF;
          int feeID = 1 + ((channelID >> 6) & 0x3F);
          channelID &= 0x3F;
#ifdef AIDA_REAL_IMPLANTS
          if (feeID == 1 || feeID == 3 || feeID == 5 || feeID == 6 || feeID == 7 || feeID == 8)
          {
            cur_aida->pside_imp[0] = true;
          }
          if (feeID == 2 || feeID == 4)
          {
            cur_aida->nside_imp[0] = true;
          }

          if (cur_aida->pside_imp[0] && cur_aida->nside_imp[0])
          {
            if (_AIDA_WATCHER_STATS)
            {
              _AIDA_WATCHER_STATS->add_i(1);
            }
            cur_aida->pside_imp[0] = false;
            cur_aida->nside_imp[0] = false;
          }
#else

          if (_AIDA_WATCHER_STATS)
          {
            _AIDA_WATCHER_STATS->add_i(feeID);
          }
#endif
          // Instead of tracking the multiplexer every event (mostly not used)
          // Go back and retrack the multiplexer at the start of an implant
          // Where it matters for aligning
          if (cur_aida->implant_wr_s == 0) {
            int N = 0;
            int64_t lWR = 0;
            int64_t tWR = cur_aida->timestamp;
            for (size_t j = 0; j < cur_aida->data.size(); j += 2)
            {
              uint32_t tdata = cur_aida->data[j];
              tWR &= ~0x0fffffff;
              tWR |= (cur_aida->data[j + 1] & 0x0fffffff);
              if ((tdata & 0xC0F00000) == 0x80500000)
              {
                uint32_t middleTS_raw = cur_aida->data[j + 2];
                uint32_t highTS = word1 & 0x000FFFFF;
                uint32_t middleTS = middleTS_raw & 0x000FFFFF;
                tWR = (int64_t)(((uint64_t)highTS << 48) | ((uint64_t)middleTS << 28) | cur_aida->data[j + 1]);
                j += 2;
                continue;
              }
              if ((tdata & 0xF0000000) == 0xC0000000)
              {
                int tchannelID = (tdata >> 16) & 0xFFF;
                int tfeeID = 1 + ((tchannelID >> 6) & 0x3F);
                tchannelID &= 0x3F;
                if (tfeeID == feeID && tchannelID / 16 == channelID / 16)
                {
                  if (tWR - lWR < 2100) {
                    N++;
                  }
                  else {
                    N = 0;
                  }
                  lWR = tWR;
                }
              }
            }
            cur_aida->implant_wr_s = load_event_wr - AIDA_TIME_SHIFT - (N * 2000);
            //_TRACE("correcting implant WR time by %d multiplexer cycles\n", m.N);
          }
        }
        if ((word1 & 0xF0000000) == 0xC0000000)
        {
          int channelID = (word1 >> 16) & 0xFFF;
          int feeID = 1 + ((channelID >> 6) & 0x3F);
          channelID &= 0x3F;
          if (_AIDA_WATCHER_STATS)
          {
            _AIDA_WATCHER_STATS->add_d(feeID);
          }
        }
#ifdef AIDA_CORRELATION_PULSER
        if ((word1 & 0xFFF00000) == AIDA_CORRELATION_EVENT)
        {
          cur_aida->flags |= 2;
        }
#endif
        if ((word1 & AIDA_SCALER_MASK) == AIDA_SCALER_EVENT)
        {
          int feeID = 1 + ((word1 >> 24) & 0x3F);
          if (_AIDA_WATCHER_STATS)
          {
            _AIDA_WATCHER_STATS->add_scaler(feeID);
          }
        }

        //_TRACE(" event time: %16" PRIx64 "\n", load_event_wr);

        // TODO: if earlier than MBS timestamp, just ignore the event as it's a partial fragment from the start
        if (load_event_wr < old_ts && old_ts != mbs_wr)
        {
          if(aida_skip++ == 0)
          {
            TIMEWARP("AIDA Timewarp (%16" PRIx64 " before %16" PRIx64 ")", load_event_wr, old_ts);
          }
          continue;
        }

        if (aida_skip)
        {
          TIMEWARP("AIDA timewarp is over, skipped %d AIDA event(s)", aida_skip);
        }
        aida_skip = 0;

        // UPDATE: Only ADC items can break apart events (INFO words don't really matter athis point)
        if ((word1 & 0xC0000000) == 0xC0000000 && load_event_wr - old_ts > _conf._eventbuilder_window)
        {
          // End of Event
#if _AIDA_DUMP
          _TRACE ("end of event (before last word) %16lx\n", cur_aida->timestamp);
#endif
          // If there is exactly one entry (2 words) and it's an ADC entry, discard it as it won't be front-back matched
#ifdef AIDA_OPTIMISE
          if (cur_aida->data.size() == 2 && (cur_aida->data[0] & 0xC0000000) == 0xC0000000)
          {
              _TRACE("Discarding tiny AIDA event %8x %8x\n", cur_aida->data[0], cur_aida->data[1]);
              pool_delete(aida_events_pool, cur_aida);
              cur_aida = nullptr;
          }
          else
#endif
          if (_conf._aida_skip_decays && !cur_aida->implant())
          {
              _TRACE("Discarding decay AIDA event");
              pool_delete(aida_events_pool, cur_aida);
              cur_aida = nullptr;
          }
          else
          {
            cur_aida->fragment = false;
            cur_aida->fragment_wr = old_ts;
            if (cur_aida->implant()) cur_aida->timestamp = cur_aida->implant_wr_s;
            //aida_events_merge.push_back(cur_aida);
            events.push(cur_aida);
            cur_aida = nullptr;
          }
          cur_aida = pool_new(aida_events_pool);
          cur_aida->_header = se->_header;
          cur_aida->timestamp = load_event_wr - AIDA_TIME_SHIFT;
          _TRACE(" new AIDA event %16lx\n", load_event_wr);
        }

        if ((word1 & 0xF0000000) == 0xC0000000)
        {
          cur_aida->fragment_wr = load_event_wr;
        }

        cur_aida->data.push_back(word1);
        cur_aida->data.push_back(word2);

        if ((word1 & 0xF0000000) == 0xC0000000)
        {
          old_ts = load_event_wr;
        }
      }

      if (cur_aida->data.size() > 0)
      {
        _TRACE ("saving fragment %16lx\n", cur_aida->fragment_wr);
        cur_aida->fragment = true;
      }
      else
      {
          ERROR("Why?");
          pool_delete(aida_events_pool, cur_aida);
          cur_aida = nullptr;
      }

      //_TRACE("AIDA Buffer now contains: %lu events\n", aida_events_merge.size());
}

void lmd_source_multievent::load_dtas(lmd_subevent *se, char* pb_start, char* pb_end, int64_t mbs_wr)
{
  _TRACE("lmd_source_multievent::load_events(): identified a DTAS block... expanding\n");

  input_event_header = _file_event._header;

  // We are to continue with a fragmented block
  if (cur_dtas != nullptr && cur_dtas->fragment)
  {
    _TRACE(" found a DTAS fragment to continue with: %16lx\n", cur_dtas->fragment_wr);
    assert(false);
  }
  else
  {
    _TRACE(" new DTAS event %16lx\n", load_event_wr);
    cur_dtas = pool_new(dtas_events_pool);
    cur_dtas->_header = se->_header;
    cur_dtas->timestamp = load_event_wr;
    cur_dtas->fragment_wr = load_event_wr;
  }

  uint32_t *pl_start = reinterpret_cast<uint32_t*>(pb_start);
  uint32_t *pl_end = reinterpret_cast<uint32_t*>(pb_end);

  // Advance past the extracted WR data
  uint32_t* pl_data = pl_start + 5;

  int64_t old_ts = cur_dtas->fragment_wr;
  load_event_wr = old_ts;

  // We should assert these are the same as the MBS header
  uint32_t wr34 = *pl_data++;

  // Synchronisation pulser
  if (wr34 == 0x12345678)
  {
    if (cur_dtas->data.size() == 0)
    {
      _TRACE("Deleting empty event because of pulser\n");
      pool_delete(dtas_events_pool, cur_dtas);
      cur_dtas = nullptr;
    }
    _TRACE("Making special DTAS pulser event\n");    
    dtasevent_entry* pulser = pool_new(dtas_events_pool);
    pulser->_header = se->_header;
    pulser->timestamp = mbs_wr;
    pulser->fragment = false;
    pulser->fragment_wr = mbs_wr;
    pulser->pulser = true;
    events.push(pulser);
    return;
  }

  if ((mbs_wr >> 32) != wr34)
  {
    ERROR("DTAS WR header did not match MBS WR header %16lx %08x", (mbs_wr >> 32), wr34);
  }

  load_event_wr &= 0xFFFFFFFFULL;
  load_event_wr |= ((int64_t)wr34 << 32);

  while (pl_data < pl_end)
  {
    uint32_t dtas_block[4];
    dtas_block[0] = *pl_data++; // Header
    dtas_block[1] = *pl_data++; // Timestamp
    dtas_block[2] = *pl_data++; // Fine Time
    dtas_block[3] = *pl_data++; // Energy

#if _DTAS_DUMP
        _TRACE(" dtas event %08x %08x %08x %08x\n", dtas_block[0], dtas_block[1], dtas_block[2], dtas_block[3]);
#endif

    // Check the data is being interpreted properly
    if ((dtas_block[0] & 0xFF00FF00) != 0xBA00C000)
    {
      ERROR("Not a valid DTAS Block. Expected 0xBAxxC0xx got %08x", dtas_block[0]);
    }

    // Update the timestamp for the event
    load_event_wr &= ~0xFFFFFFFFULL;
    load_event_wr |= dtas_block[1];
    // Rollover correction (but don't try to increase if it's the first MBS event)
    if (load_event_wr < old_ts && load_event_wr != mbs_wr)
    {
      load_event_wr += 0x100000000ULL;
      // Don't increase the WR if it's more than 200 ms increase
      //if (load_event_wr - old_ts > 200000000LL) {
        //load_event_wr -= 0x100000000ULL;
      //}
    }

    // Timewarp detection
    if (load_event_wr < old_ts)
    {
      if (dtas_skip++ == 0)
      {
        TIMEWARP("DTAS Timewarp (%16" PRIx64 " before %16" PRIx64 ")", load_event_wr, old_ts);
      }
      continue;
    }
    if (dtas_skip)
    {
      TIMEWARP("DTAS timewarp is over, skipped %d DTAS event(s)", dtas_skip);
      dtas_skip = 0;
    }

    // If the event occurred after a gap, the old event is completed
    if (load_event_wr - old_ts > _conf._eventbuilder_window)
    {
      if (cur_dtas->data.size() > 0)
      {
        cur_dtas->fragment = false;
        cur_dtas->fragment_wr = old_ts;
        _TRACE(" completed DTAS event %16lx (%lu entries) moved to event queue\n", cur_dtas->timestamp, cur_dtas->data.size());
        events.push(cur_dtas);
        cur_dtas = pool_new(dtas_events_pool);
      }
      else
      {
        _TRACE(" resetting DTAS event because it was empty\n");
        cur_dtas->reset();
      }
      cur_dtas->_header = se->_header;
      cur_dtas->timestamp = load_event_wr;
      _TRACE(" new DTAS event %16lx\n", load_event_wr);
    }

    cur_dtas->fragment_wr = load_event_wr;
    cur_dtas->data.push_back(dtas_block[0]);
    cur_dtas->data.push_back(dtas_block[1]);
    cur_dtas->data.push_back(dtas_block[2]);    
    cur_dtas->data.push_back(dtas_block[3]);

    old_ts = load_event_wr;
  }

  if (cur_dtas->data.size() > 0)
  {
    //_TRACE ("saving fragment %16lx\n", cur_dtas->fragment_wr);
    //cur_dtas->fragment = true;

    cur_dtas->fragment = false;
    events.push(cur_dtas);
    cur_dtas = nullptr;
  }
  else
  {
    //assert(false);
    // Logically this never happens?
    pool_delete(dtas_events_pool, cur_dtas);
    cur_dtas = nullptr;
  }
}

lmd_event *aidaevent_entry::emit()
{
  _TRACE("Emitting an AIDA event entry\n");
  _file_event.release();

  _file_event._header = input_event_header;
  _file_event._header._info.l_count = (uint32_t)++l_count;
  _file_event._header._info.i_trigger = 1;
#ifdef AIDA_CORRELATION_PULSER
  if (this->flags & 0x2)
    _file_event._header._info.i_trigger = 3;
#else
  if (!entry->implant() && entry->data.size() > 750 * 3 * 2) // 750 channels, 3 items (SYNC, SYNC, ADC), 2 32-bit words per item
    _file_event._header._info.i_trigger = 3;
#endif

  _file_event._nsubevents = 1;
  _file_event._subevents = (lmd_subevent*)_file_event._defrag_event.allocate(sizeof (lmd_subevent));

  _file_event._subevents[0]._header = this->_header;
  //if (entry->implant) _file_event._subevents[0]._header.i_procid = 95; // Special subevent mark for implant events
  _file_event._subevents[0]._data = (char*)_file_event._defrag_event_many.allocate(this->data.size() * sizeof(uint32_t) + WRTS_SIZE);
  _file_event._subevents[0]._header._header.l_dlen = (uint32_t)(this->data.size() * sizeof(uint32_t) + WRTS_SIZE)/2 + 2;

  _file_event._header._header.l_dlen = (uint32_t)DLEN_FROM_EVENT_DATA_LENGTH(this->data.size() * sizeof(uint32_t) + WRTS_SIZE + sizeof(lmd_subevent));

  // AIDA events contain extra state for ucesb
  _file_event._aida_extra = true;
  _file_event._aida_implant = this->implant();
  _file_event._aida_length = (int64_t)this->fragment_wr - AIDA_TIME_SHIFT - this->timestamp;
  if (this->implant()) {
    this->fragment_wr = this->implant_wr_e;
    //_file_event._aida_length = (int64_t)entry->implant_wr_s - entry->timestamp; // Only count length from start to implant for overlap... implant is instant
    _file_event._aida_length = 0; // Aida implants have 0 length
  }

  wrts_header wr(_conf._eventbuilder_wrid, (uint64_t)this->timestamp);
  memcpy(_file_event._subevents[0]._data, &wr, sizeof(wr));
  memcpy(_file_event._subevents[0]._data + WRTS_SIZE, this->data.data(), this->data.size() * sizeof(uint32_t));
  this->return_to_pool();

  _file_event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
  return &_file_event;
}

lmd_event *dtasevent_entry::emit()
{
  _TRACE("Emitting a DTAS event entry\n");
   _file_event.release();

  _file_event._header = input_event_header;
  _file_event._header._info.l_count = (uint32_t)++l_count;
  _file_event._header._info.i_trigger = 1;

  _file_event._nsubevents = 1;
  _file_event._subevents = (lmd_subevent*)_file_event._defrag_event.allocate(sizeof (lmd_subevent));

  _file_event._subevents[0]._header = this->_header;
  _file_event._subevents[0]._data = (char*)_file_event._defrag_event_many.allocate(this->data.size() * sizeof(uint32_t) + WRTS_SIZE + sizeof(uint32_t));
  _file_event._subevents[0]._header._header.l_dlen = (uint32_t)(this->data.size() * sizeof(uint32_t) + WRTS_SIZE + sizeof(uint32_t))/2 + 2;

  _file_event._header._header.l_dlen = (uint32_t)DLEN_FROM_EVENT_DATA_LENGTH(this->data.size() * sizeof(uint32_t) + WRTS_SIZE + sizeof(uint32_t) + sizeof(lmd_subevent));

  _file_event._aida_extra = false;
  _file_event._aida_implant = false;
  _file_event._aida_length = 0;

  wrts_header wr(0x800, (uint64_t)this->timestamp);
  memcpy(_file_event._subevents[0]._data, &wr, sizeof(wr));
  uint32_t sausage = (uint32_t)(this->timestamp >> 32);

  // If the event is a pulser set trigger to 3 (pulser) and set the "WR" to 12345678 as before
  // The data() array is (should be) 0 but we can memcpy 0 bytes safely (as a no-op)
  if (pulser)
  {
    _file_event._header._info.i_trigger = 3;
    sausage = 0x12345678;
  }
  memcpy(_file_event._subevents[0]._data + WRTS_SIZE, &sausage, sizeof(uint32_t));
  memcpy(_file_event._subevents[0]._data + sizeof(uint32_t) + WRTS_SIZE, this->data.data(), this->data.size() * sizeof(uint32_t));
  this->return_to_pool();

  _file_event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
  return &_file_event;
}

lmd_event* triggerevent_entry::emit()
{
    _TRACE("Emitting a trigger event entry\n");
    _file_event.release();

    _file_event._header = this->event._header;
    _file_event._header._info.l_count = (uint32_t)++l_count;

    _file_event._swapping = this->event._swapping;
    _file_event._nsubevents = this->event._nsubevents;
    // allocate subevent array
    _file_event._subevents = (lmd_subevent*)_file_event._defrag_event.allocate((size_t)_file_event._nsubevents * sizeof(lmd_subevent));
    // copy subevents over
    for (int i = 0; i < this->event._nsubevents; i++)
    {
      size_t nsubev = SUBEVENT_DATA_LENGTH_FROM_DLEN(this->event._subevents[i]._header._header.l_dlen);
      _file_event._subevents[i]._header = this->event._subevents[i]._header;
      _file_event._subevents[i]._data = (char*)_file_event._defrag_event_many.allocate(nsubev);
      memcpy(_file_event._subevents[i]._data, this->event._subevents[i]._data, nsubev);
    }

    _file_event._aida_extra = false;
    _file_event._aida_implant = false;
    _file_event._aida_length = 0;

    this->return_to_pool();
    //trigger_event.pop_front();

    _file_event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
    return &_file_event;
}

lmd_source_multievent::lmd_source_multievent() : load_event_wr(0), emit_wr(0), last_mbs_wr(0), emit_skip(0), aida_skip(0), dtas_skip(0), cur_aida(nullptr), cur_dtas(nullptr)
{
#if BPLAST_DELAY_FIX
  WARNING("bPlas WR Correction is ACTIVE");
  plastic_buffer.event._nsubevents = 0;
#endif
#if FATIMA_DELAY_FIX
  WARNING("FATIMA TAMEX WR Correction is ACTIVE");
  fatima_buffer.event._nsubevents = 0;
#endif
}

// free everything for a valgrind check
lmd_source_multievent::~lmd_source_multievent()
{
#if MEMORY_REPORT
  INFO(0, "Memory Report for Multievent Builder");
  INFO(0, "cur_aida size = %zu", cur_aida ? cur_aida->data.size() : 0);
  INFO(0, "cur_dtas size = %zu", cur_dtas ? cur_dtas->data.size() : 0);
  INFO(0, "event buffer size = %zu", events.size());
  INFO(0, "aida pool size = %zu", aida_events_pool.size());
  size_t recurse = 0;
  for (auto i : aida_events_pool) recurse += i->data.capacity();
  INFO(0, "aida pool recursive size = %zu", recurse);
  INFO(0, "trigger pool size = %zu", trigger_events_pool.size());
  recurse = 0;
  for (auto i : trigger_events_pool) recurse += i->event._defrag_event_many._allocated;
  INFO(0, "trigger pool recursive size = %zu", recurse);
  INFO(0, "dtas pool size = %zu", dtas_events_pool.size());
  recurse = 0;
  for (auto i : dtas_events_pool) recurse += i->data.capacity();
  INFO(0, "dtas pool recursive size = %zu", recurse);
#endif
  delete cur_aida;
  delete cur_dtas;
  for (; !events.empty(); events.pop()) delete events.top();
  for (auto i : aida_events_pool) delete i;
  for (auto i : trigger_events_pool) delete i;
  for (auto i : dtas_events_pool) delete i;
  aida_events_pool.clear();
  trigger_events_pool.clear();
  dtas_events_pool.clear();
}

#ifdef _ENABLE_TRACE
// Don't define this function if we don't use it :)
// Generate a nice text string from an AIDA data item
std::string print_aida(uint32_t word1, uint32_t word2)
{
  std::stringstream s;
  s << std::setfill('0');

  if((word1 & 0xC0000000) == 0xC0000000)
  {
    int channelID = (word1 >> 16) & 0xFFF;
    int feeID = 1 + ((channelID >> 6) & 0x3F);
    channelID &= 0x3F;
    int data = (word1 & 0xFFFF);
    int veto = (word1 >> 28) & 0x1;

    s << "ADC " << std::setw(2) << feeID << ":"
      << std::setw(2) << channelID << " " << (veto ? 'H' : 'L')
      << " " << std::setw(4) << std::hex << data << std::dec;
  }
  else if ((word1 & 0xC0000000) == 0x80000000)
  {
    int feeID = 1 + ((word1 >> 24) & 0x3F);
    int infoCode = (word1 >> 20) & 0x000F;
    int infoField = word1 & 0x000FFFFF;

    s << "INFO " << infoCode;

    if (infoCode == 2)
    {
      s << " PAUSE " << std::setw(2) << feeID << " "
        << std::hex << std::setw(5) << infoField << std::dec;
    }
    else if (infoCode == 3)
    {
      s << " RESUME " << std::setw(2) << feeID << " "
        << std::hex << std::setw(5) << infoField << std::dec;
    }
    else if (infoCode == 4)
    {
      s << " SYNC4828 " << std::setw(2) << feeID << " "
        << std::hex << std::setw(5) << infoField << std::dec;
    }
    else if (infoCode == 5)
    {
      s << " SYNC6348 " << std::setw(2) << feeID << " "
        << std::hex << std::setw(5) << infoField << std::dec;
    }
    else if (infoCode == 6)
    {
      int adc = ((infoField >> 16) & 0xF);
      int hits = infoField & 0xFFFF;
      s << " DISCRIM " << std::setw(2) << feeID << ":" << adc << " "
        << std::hex << std::setw(4) << hits << std::dec;
    }
  }
  else
  {
    s << "Unknown";
  }

  return s.str();
}

#endif
