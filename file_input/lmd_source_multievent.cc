#include <algorithm>
#include <map>

#include "lmd_source_multievent.hh"
#include "lmd_input.hh"
#include "config.hh"
#include <inttypes.h>
#include <sstream>
#include <iomanip>

#define AIDA_PROCID 1

#define WRTS_SIZE (uint32_t)sizeof(wrts_header)
// someone thought it a good idea to use uint32 where xe ought to have used size_t.

#ifdef _ENABLE_TRACE
// Don't define this function if we don't use it :)
// Generate a nice text string from an AIDA data item
std::string print_aida(uint32_t word1, uint32_t word2);
#endif

lmd_event *lmd_source_multievent::get_event()
{
  _TRACE("lmd_source_multievent::get_event()\n");

  if(!_conf._enable_eventbuilder)
    return lmd_source::get_event();

  // Firstly dump AIDA events
  if (aida_events_dump.size() > 0)
  {
      aidaevent_entry* entry = aida_events_dump.front();
      aida_events_dump.pop_front();
      _TRACE("=> Return AIDA event (dump) (%16lx)\n", entry->timestamp);
      if (entry->timestamp >= emit_wr)
      {
        if (emit_skip)
          WARNING("Recovered from timewarp but skipped %d event(s)", emit_skip);
        emit_skip = 0;
        emit_wr = entry->timestamp;
        return emit_aida(entry);
      }
      else
      {
        _TRACE("=> Not emitting timewarped event (before %16lx)\n", emit_wr);
        emit_skip++;
        delete entry;
        return get_event();
      }
  }

  // Secondly start emitting other events if the queue is too large (in theory this could be 1, but you never know)
  if (trigger_event.size() > 10 && aida_events_merge.size() == 0)
  {
    _TRACE("=> Return other event (dump) (%16lx)\n", trigger_event.front().timestamp);
    emit_wr = trigger_event.front().timestamp;
    return emit_other();
  }

  // Merge events if possible
  if (aida_events_merge.size() > 0 && trigger_event.size() > 0)
  {
      aidaevent_entry* entry = aida_events_merge.front();
      if(entry->timestamp < trigger_event.front().timestamp)
      {
          if (entry->fragment)
          {
            _TRACE(" loading more events to complete the fragment %16lx\n", entry->timestamp);
            file_status_t s;
            while((s = load_events()) == other_event && load_event_wr - entry->timestamp < _conf._eventbuilder_window);
            if (s == unknown_event)
              ERROR("Unknown event found");
            if (!entry->fragment) { _TRACE(" unfragmenting AIDA event because an AIDA event didn't come in time\n"); }
            entry->fragment = false;
            return get_event();
          }

          aida_events_merge.pop_front();
          _TRACE("=> Return AIDA event (merge %16lx)\n", entry->timestamp);
          if (entry->timestamp >= emit_wr)
          {
            if (emit_skip)
              WARNING("Recovered from timewarp but skipped %d event(s)", emit_skip);
            emit_skip = 0;
            emit_wr = entry->timestamp;
            return emit_aida(entry);
          }
          else
          {
            emit_skip++;
            _TRACE("=> Not emitting timewarped event (before %16lx)\n", emit_wr);
            delete entry;
            return get_event();
          }
      }
      else
      {
        if (trigger_event.front().timestamp >= emit_wr)
        {
          _TRACE("=> Return other event (merge %16lx)\n", trigger_event.front().timestamp);
          if (emit_skip)
            WARNING("Recovered from timewarp but skipped %d event(s)", emit_skip);
          emit_skip = 0;
          emit_wr = trigger_event.front().timestamp;
          return emit_other();
        }
        else
        {
          emit_skip++;
          _TRACE("=> Not emitting timewarped event (before %16lx)\n", emit_wr);
          triggerevent_entry& entry = trigger_event.front();
          entry.event.release();
          free(entry.event._defrag_event._buf);
          free(entry.event._defrag_event_many._first);
          trigger_event.pop_front();
          return get_event();
        }
      }
  }

  if (_conf._max_events == 1)
  {
    return NULL;
  }


  file_status_t status = load_events();
  if(status == other_event || status == aida_event)
  {
    return get_event();
  }
  else if (status == eof)
  {
    _TRACE("=> EOF reached, %lu / %lu\n", aida_events_merge.size(), trigger_event.size());
    if (_input._input->_filename != _inputs.back()._name)
    {
      _TRACE("Returning EOF to move to next file\n");
      return NULL;
    }
    // Mark AIDA data for dumping
    if (aida_events_merge.size() > 0)
    {
      aida_events_dump = aida_events_merge;
      aida_events_merge.clear();
      return get_event();
    }
    else if (trigger_event.size() > 0)
    {
      return emit_other();
    }
    _TRACE("Nothing left, end of event input\n");
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
  _file_event.print_event(0, NULL);
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
        _TRACE("lmd_source_multievent::load_events(): WR subevent with ProcID: %d\n", se->_header.i_procid);
        _TRACE("lmd_source_multievent::load_events(): WR %lx\n", load_event_wr);
      }
    }

    if (se->_header.i_procid == _conf._eventbuilder_procid)
    {
      _TRACE("lmd_source_multievent::load_events(): identified an AIDA block... expanding\n");

      input_event_header = _file_event._header;

      aidaevent_entry* cur_aida;

      // We are to continue with a fragmented block
      if (aida_events_merge.size() > 0 && aida_events_merge.back()->fragment)
      {
        _TRACE(" found an AIDA fragment to continue with: %16lx\n", aida_events_merge.back()->fragment_wr);
        cur_aida = aida_events_merge.back();
        aida_events_merge.pop_back();
      }
      else
      {
        _TRACE(" new AIDA event %16lx\n", load_event_wr);
        cur_aida = new aidaevent_entry;
        cur_aida->_header = se->_header;
        cur_aida->timestamp = load_event_wr;
        cur_aida->fragment_wr = load_event_wr;
      }
      // Is there an old AIDA block? Mark it for dumping then build a new buffer
      if(aida_events_merge.size() > 0)
      {
        _TRACE(" clearing block\n");
        aida_events_dump = aida_events_merge;
        aida_events_merge.clear();
      }

      char *pb_start, *pb_end;
      _file_event.get_subevent_data_src(se, pb_start, pb_end);
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
              return unknown_event;
          }
          uint32_t highTS = word1 & 0x000FFFFF;
          uint32_t middleTS = middleTS_raw & 0x000FFFFF;
          load_event_wr = ((uint64_t)highTS << 48) | ((uint64_t)middleTS << 28) | word2;
          cur_aida->data.push_back(word1);
          cur_aida->data.push_back(word2);
          cur_aida->data.push_back(middleTS_raw);
          cur_aida->data.push_back(middleTS_low);
          //_TRACE(" event time: %16" PRIx64 "\n", load_event_wr);
          cur_aida->fragment_wr = load_event_wr;
          continue;
        }

        // PAUSE/RESUME markers also update the middle timestamp
        if ((word1 & 0xC0F00000) == 0x80200000 || (word1 & 0xC0F00000) == 0x80300000)
        {
            uint64_t highTS = (load_event_wr >> 48) & 0xFFFFF;
            uint64_t middleTS = word1 & 0x000FFFFF;
            load_event_wr = ((uint64_t)highTS << 48) | ((uint64_t)middleTS << 28) | word2;
        }

        if ((word1 & 0xF0000000) == 0xD0000000)
        {
          cur_aida->implant = true;
        }

        //_TRACE(" event time: %16" PRIx64 "\n", load_event_wr);

        // TODO: if earlier than MBS timestamp, just ignore the event as it's a partial fragment from the start
        if (load_event_wr < old_ts && old_ts != mbs_wr)
        {
          if(aida_skip++ == 0)
          {
            WARNING("AIDA Timewarp (%16" PRIx64 " before %16" PRIx64 ")", load_event_wr, old_ts);
          }
          continue;
        }

        if (aida_skip)
        {
          WARNING("AIDA timewarp is over, skipped %d AIDA event(s)", aida_skip);
        }
        aida_skip = 0;
        cur_aida->fragment_wr = load_event_wr;

        if (load_event_wr - old_ts > _conf._eventbuilder_window)
        {
          // End of Event
#if _AIDA_DUMP
          _TRACE ("end of event %16lx\n", cur_aida->timestamp);
#endif
          // If there is exactly one entry (2 words) and it's an ADC entry, discard it as it won't be front-back matched
          if (cur_aida->data.size() == 2 && (cur_aida->data[0] & 0xC0000000) == 0xC0000000)
          {
              _TRACE("Discarding tiny AIDA event %8x %8x\n", cur_aida->data[0], cur_aida->data[1]);
              delete cur_aida;
          }
          else
          {
            cur_aida->fragment = false;
            aida_events_merge.push_back(cur_aida);
          }
          cur_aida = new aidaevent_entry;
          cur_aida->_header = se->_header;
          cur_aida->timestamp = load_event_wr;
          cur_aida->fragment_wr = load_event_wr;
          _TRACE(" new AIDA event %16lx\n", load_event_wr);
        }

        cur_aida->data.push_back(word1);
        cur_aida->data.push_back(word2);

        old_ts = load_event_wr;
      }

      if (cur_aida->data.size() > 0)
      {
        _TRACE ("saving fragment %16lx\n", cur_aida->fragment_wr);
        aida_events_merge.push_back(cur_aida);
      }
      else
      {
        delete cur_aida;
      }

      _TRACE("AIDA Buffer now contains: %lu events\n", aida_events_merge.size());

      return aida_event;
    }
  }

  _TRACE("lmd_source_multievent::load_events(): identified an other block\n");

  trigger_event.emplace_back();
  triggerevent_entry& entry = trigger_event.back();

  entry.timestamp = load_event_wr;

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

    if (entry.event._nsubevents > 0)
    {
      return other_event;
    }
    else
    {
      _TRACE("First plastic event is only buffered\n");
      entry.event.release();
      free(entry.event._defrag_event._buf);
      free(entry.event._defrag_event_many._first);
      trigger_event.pop_back();
      return load_events();
    }
  }

#endif

  // Copy the data over to ensure ownership of pointerss
  entry.event._header = _file_event._header;
  entry.event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
  entry.event._swapping = _file_event._swapping;
  entry.event._nsubevents = _file_event._nsubevents;
  // allocate subevent array
  entry.event._subevents = (lmd_subevent*)entry.event._defrag_event.allocate(entry.event._nsubevents * sizeof(lmd_subevent));
  // copy subevents over
  for (int i = 0; i < entry.event._nsubevents; i++)
  {
    size_t nsubev = SUBEVENT_DATA_LENGTH_FROM_DLEN(_file_event._subevents[i]._header._header.l_dlen);
    entry.event._subevents[i]._header = _file_event._subevents[i]._header;
    entry.event._subevents[i]._data = (char*)entry.event._defrag_event_many.allocate(nsubev);
    memcpy(entry.event._subevents[i]._data, _file_event._subevents[i]._data, nsubev);
  }

  _TRACE("Other Buffer now contains: %lu events\n", trigger_event.size());

  return other_event;
}

lmd_event *lmd_source_multievent::emit_aida(aidaevent_entry* entry)
{
  _file_event.release();

  _file_event._header = input_event_header;
  _file_event._header._info.l_count = ++l_count;
  _file_event._header._info.i_trigger = 1;
  if (!entry->implant && entry->data.size() > 750 * 3 * 2) // 750 channels, 3 items (SYNC, SYNC, ADC), 2 32-bit words per item
    _file_event._header._info.i_trigger = 3;

  _file_event._nsubevents = 1;
  _file_event._subevents = (lmd_subevent*)_file_event._defrag_event.allocate(sizeof (lmd_subevent));

  _file_event._subevents[0]._header = entry->_header;
  _file_event._subevents[0]._data = (char*)_file_event._defrag_event_many.allocate(entry->data.size() * sizeof(uint32_t) + WRTS_SIZE);
  _file_event._subevents[0]._header._header.l_dlen = (int32_t)(entry->data.size() * sizeof(uint32_t) + WRTS_SIZE)/2 + 2;

  _file_event._header._header.l_dlen = (int32_t)DLEN_FROM_EVENT_DATA_LENGTH(entry->data.size() * sizeof(uint32_t) + WRTS_SIZE + sizeof(lmd_subevent));

  wrts_header wr(entry->timestamp);
  memcpy(_file_event._subevents[0]._data, &wr, sizeof(wr));
  memcpy(_file_event._subevents[0]._data + WRTS_SIZE, entry->data.data(), entry->data.size() * sizeof(uint32_t));
  delete entry;

  _file_event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
  return &_file_event;
}

lmd_event *lmd_source_multievent::emit_other()
{
    _file_event.release();

    triggerevent_entry& entry = trigger_event.front();

    _file_event._header = entry.event._header;
    _file_event._header._info.l_count = ++l_count;

    _file_event._swapping = entry.event._swapping;
    _file_event._nsubevents = entry.event._nsubevents;
    // allocate subevent array
    _file_event._subevents = (lmd_subevent*)_file_event._defrag_event.allocate(_file_event._nsubevents * sizeof(lmd_subevent));
    // copy subevents over
    for (int i = 0; i < entry.event._nsubevents; i++)
    {
      size_t nsubev = SUBEVENT_DATA_LENGTH_FROM_DLEN(entry.event._subevents[i]._header._header.l_dlen);
      _file_event._subevents[i]._header = entry.event._subevents[i]._header;
      _file_event._subevents[i]._data = (char*)_file_event._defrag_event_many.allocate(nsubev);
      memcpy(_file_event._subevents[i]._data, entry.event._subevents[i]._data, nsubev);
    }

    entry.event.release();
    free(entry.event._defrag_event._buf);
    free(entry.event._defrag_event_many._first);
    trigger_event.pop_front();

    _file_event._status = LMD_EVENT_GET_10_1_INFO_ATTEMPT | LMD_EVENT_HAS_10_1_INFO | LMD_EVENT_LOCATE_SUBEVENTS_ATTEMPT;
    return &_file_event;
}

lmd_source_multievent::lmd_source_multievent() : emit_wr(0), emit_skip(0), aida_skip(0), l_count(0)
{
#if BPLAST_DELAY_FIX
  WARNING("bPlas WR Correction is ACTIVE");
  plastic_buffer.event._nsubevents = 0;
#endif
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
