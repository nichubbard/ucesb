#ifndef _LMD_SOURCE_MULTIEVENT_H_
#define _LMD_SOURCE_MULTIEVENT_H_

#include <algorithm>
#include <deque>
#include <list>
#include <map>

#include <stdint.h>

struct lmd_source_multievent;
struct lmd_event_multievent;

#include "lmd_input.hh"
#include "thread_buffer.hh"

#include <math.h>
#include "config.hh"

//#define BPLAST_DELAY_FIX 1
//#define FATIMA_DELAY_FIX 1

#define _ENABLE_TRACE 0
#define _AIDA_DUMP 0
#if _ENABLE_TRACE
#define _TRACE(...) fprintf(stderr, __VA_ARGS__)
#else
#define _TRACE(...)
#endif

class aidaeb_watcher_stats
{
public:
  aidaeb_watcher_stats(size_t _copies = 1) : copies(_copies) {}

  inline void load_map(std::map<int, int> new_map)
  {
      fee_dssd = new_map;
      int max_d = 0;
      for (auto& i : fee_dssd)
      {
        max_d = std::max(max_d, i.second);
      }
      dssd_counts_i.resize(copies);
      dssd_counts_d.resize(copies);
      for (size_t i = 0; i < copies; i++)
      {
        dssd_counts_i[i].resize(max_d);
        dssd_counts_d[i].resize(max_d);
        clear(i);
      }
  }

  inline void clear(size_t copy = 0)
  {
      std::fill(dssd_counts_i[copy].begin(), dssd_counts_i[copy].end(), 0);
      std::fill(dssd_counts_d[copy].begin(), dssd_counts_d[copy].end(), 0);
  }

  inline std::vector<int64_t> const& implants(size_t copy = 0) const
  {
    return dssd_counts_i[copy];
  }

  inline std::vector<int64_t> const& decays(size_t copy = 0) const
  {
    return dssd_counts_d[copy];
  }

private:
  std::map<int, int> fee_dssd;
  std::vector<std::vector<int64_t>> dssd_counts_i;
  std::vector<std::vector<int64_t>> dssd_counts_d;
  size_t copies;

  inline void add_internal(std::vector<std::vector<int64_t>>& vec, int fee)
  {
    if (fee_dssd.find(fee) != fee_dssd.end())
    {
      int dssd = fee_dssd[fee];
      for(size_t i = 0; i < copies; i++)
      {
        vec[i][dssd - 1]++;
      }
    }
  }

public:
  inline void add_i(int fee)
  {
      add_internal(dssd_counts_i, fee);
  }

  inline void add_d(int fee)
  {
      add_internal(dssd_counts_d, fee);
  }
};

extern aidaeb_watcher_stats* _AIDA_WATCHER_STATS;

class multiplexer_data
{
public:
  struct multiplexer_entry
  {
    uint64_t wr;
    int N;
  };

  multiplexer_data() {}

  void clear() { multiplexer_wrs.clear(); }

  multiplexer_entry& operator()(size_t fee, size_t asic) {
    size_t indx = fee * 4 + asic;
    if (multiplexer_wrs.size() < 1 + indx) multiplexer_wrs.resize(indx + 1);
    return multiplexer_wrs[indx];
  }

private:
  std::vector<multiplexer_entry> multiplexer_wrs;
};

struct aidaevent_entry
{
  //keep_buffer_wrapper *data_alloc;
  lmd_subevent_10_1_host _header;
  int64_t timestamp;
  std::vector<uint32_t> data;
  bool fragment;
  int64_t fragment_wr, implant_wr_s, implant_wr_e;
  int flags;
  // clean this up later if it works
  multiplexer_data multiplexer;

	aidaevent_entry() : timestamp(0), data(), fragment(true), implant_wr_s(0), flags(0)  { data.reserve(10000); reset(); }
	~aidaevent_entry(){}

  void reset() {
    timestamp = 0;
    data.clear();
    fragment = true;
    implant_wr_s = 0;
    flags = 0;
    multiplexer.clear();
  }

  bool implant() const {
    return (flags & 0x1) == 0x1;
  }

  //void* operator new(size_t bytes, keep_buffer_wrapper &alloc);
  //void operator delete(void *ptr);
};

typedef std::deque< aidaevent_entry* > aidaevent_queue;

struct triggerevent_entry
{
  lmd_event event;
  int64_t timestamp;
};

typedef std::deque< triggerevent_entry > triggerevent_queue;

struct lmd_source_multievent : public lmd_source
{
protected:
  enum file_status_t { aida_event, other_event, eof, unknown_event };

  int64_t load_event_wr;
  int64_t emit_wr;
  int emit_skip;
  int aida_skip;

  lmd_event_hint event_hint;
  lmd_event_10_1_host input_event_header;
  sint32 l_count;

  aidaevent_queue aida_events_pool;

  aidaevent_queue aida_events_merge;
  aidaevent_queue aida_events_dump;
  triggerevent_queue trigger_event;
#if BPLAST_DELAY_FIX
  triggerevent_entry plastic_buffer;
#endif
#if FATIMA_DELAY_FIX
  triggerevent_entry fatima_buffer;
#endif

  file_status_t load_events();

  lmd_event *emit_aida(aidaevent_entry* event);
  lmd_event *emit_other();

public:
  lmd_source_multievent();
  virtual lmd_event *get_event();
};

struct wrts_header
{
  uint32_t system_id;
  uint32_t lower16;
  uint32_t midlower16;
  uint32_t midupper16;
  uint32_t upper16;
public:
  wrts_header(uint64_t ts):
    system_id(_conf._eventbuilder_wrid),
    lower16(   0x03e10000 | (0xffff & (uint32_t)(ts    ))),
    midlower16(0x04e10000 | (0xffff & (uint32_t)(ts>>16))),
    midupper16(0x05e10000 | (0xffff & (uint32_t)(ts>>32))),
    upper16(   0x06e10000 | (0xffff & (uint32_t)(ts>>48)))
  {}
};

#endif
