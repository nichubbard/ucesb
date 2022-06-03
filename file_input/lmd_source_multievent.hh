#ifndef _LMD_SOURCE_MULTIEVENT_H_
#define _LMD_SOURCE_MULTIEVENT_H_

#include <algorithm>
#include <deque>
#include <list>
#include <map>
#include <queue>

#include <stdint.h>

struct lmd_source_multievent;
struct lmd_event_multievent;

#include "lmd_input.hh"
#include "thread_buffer.hh"

#include <math.h>
#include "config.hh"

//#define BPLAST_DELAY_FIX 1
//#define FATIMA_DELAY_FIX 1
#define AIDA_REAL_IMPLANTS

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
      int max_fee = 0;
      for (auto& i : fee_dssd)
      {
        max_d = std::max(max_d, i.second);
        max_fee = std::max(max_fee, i.first);
      }
      dssd_counts_i.resize(copies);
      dssd_counts_d.resize(copies);
      scalers.resize(copies);
      for (size_t i = 0; i < copies; i++)
      {
        dssd_counts_i[i].resize(max_d);
        dssd_counts_d[i].resize(max_d);
        scalers[i].resize(max_fee);
        clear(i);
      }
  }

  inline void clear(size_t copy = 0)
  {
      std::fill(dssd_counts_i[copy].begin(), dssd_counts_i[copy].end(), 0);
      std::fill(dssd_counts_d[copy].begin(), dssd_counts_d[copy].end(), 0);
      std::fill(scalers[copy].begin(), scalers[copy].end(), 0);
  }

  inline std::vector<int64_t> const& implants(size_t copy = 0) const
  {
    return dssd_counts_i[copy];
  }

  inline std::vector<int64_t> const& decays(size_t copy = 0) const
  {
    return dssd_counts_d[copy];
  }

  inline std::vector<int64_t> const& scaler(size_t copy = 0) const
  {
    return scalers[copy];
  }

private:
  std::map<int, int> fee_dssd;
  std::vector<std::vector<int64_t>> dssd_counts_i;
  std::vector<std::vector<int64_t>> dssd_counts_d;
  std::vector<std::vector<int64_t>> scalers;
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

  inline void add_scaler(int fee)
  {
    for (size_t i = 0; i < copies; i++)
    {
      scalers[i][fee - 1]++;
    }
  }
};

extern aidaeb_watcher_stats* _AIDA_WATCHER_STATS;

/*
 * Removed
class multiplexer_data
{
public:
  struct multiplexer_entry
  {
    int64_t wr;
    int N;
  };

  multiplexer_data() : dirty(false) {}

  bool dirty;

  void clear() {
 //   std::fill(multiplexer_wrs.begin(), multiplexer_wrs.end(), multiplexer_entry{ 0, 0 });
//    std::fill(std::begin(multiplexer_wrs), std::end(multiplexer_wrs), multiplexer_entry{0, 0});
    if (dirty) {
      dirty = false;
      memset(&multiplexer_wrs[0], 0, sizeof(multiplexer_entry) * 48 *4);
    }
  }

  multiplexer_entry& operator()(size_t fee, size_t asic) {
    size_t indx = fee * 4 + asic;
//if (multiplexer_wrs.size() < 1 + indx) multiplexer_wrs.resize(indx + 1);
    return multiplexer_wrs[indx];
  }

private:
  multiplexer_entry multiplexer_wrs[48*4];
};
*/

// Pool managers

template <typename T>
T* pool_new(std::deque<T*>& queue)
{
  if (queue.empty())
  {
    T* set = new T;
    set->pool = &queue;
    return set;
  }
  else
  {
    T* cur = queue.front();
    queue.pop_front();
    return cur;
  }
}

template<typename T>
void pool_delete(std::deque<T*>& queue, T* entry)
{
  entry->reset();
  queue.push_back(entry);
}


// Generic event entry to go into the priorirty queue
// Emitted in order of lowest timestamp first
struct event_entry
{
public:
  int64_t timestamp;
  int64_t event_no;
  void* pool;
  virtual ~event_entry() {}
  virtual void reset() = 0;
  virtual lmd_event* emit() = 0;

  event_entry() : timestamp(0) {}

  friend bool operator>(event_entry const& lhs, event_entry const& rhs)
  {
    // If equal in timestamp sort by event_no, this only happens for trigger_events
    if (lhs.timestamp == rhs.timestamp) return lhs.event_no > rhs.event_no;
    return lhs.timestamp > rhs.timestamp;
  }

  virtual void return_to_pool() = 0;
};

// Special comparator to compare the two event_entry pointers by their values (not addresses)
struct compare_entry
{
  bool operator()(event_entry const* lhs, event_entry const* rhs)
  {
    return *lhs > *rhs;
  }
};

struct aidaevent_entry;
typedef std::deque< aidaevent_entry* > aidaevent_queue;

struct aidaevent_entry : public event_entry
{
  //keep_buffer_wrapper *data_alloc;
  lmd_subevent_10_1_host _header;
  std::vector<uint32_t> data;
  bool fragment;
  int64_t fragment_wr, implant_wr_s, implant_wr_e;
  int flags;
  // clean this up later if it works
  //multiplexer_data multiplexer;
#ifdef AIDA_REAL_IMPLANTS
  bool pside_imp[2];
  bool nside_imp[2];
#endif

	aidaevent_entry() : data(), fragment(true), implant_wr_s(0), flags(0)  { data.reserve(10000); reset(); }
	~aidaevent_entry(){}

  virtual void reset() {
    timestamp = 0;
    data.clear();
    fragment = true;
    implant_wr_s = 0;
    flags = 0;
    //multiplexer.clear();
#ifdef AIDA_REAL_IMPLANTS
    pside_imp[0] = false;
    nside_imp[0] = false;
    pside_imp[1] = false;
    nside_imp[1] = false;
#endif
  }

  bool implant() const {
    return (flags & 0x1) == 0x1;
  }

  virtual lmd_event* emit();

  virtual void return_to_pool() {
    aidaevent_queue* q = reinterpret_cast<aidaevent_queue*>(this->pool);
    pool_delete(*q, this);
  }

  //void* operator new(size_t bytes, keep_buffer_wrapper &alloc);
  //void operator delete(void *ptr);
};

struct triggerevent_entry;
typedef std::deque< triggerevent_entry* > triggerevent_queue;

struct triggerevent_entry : public event_entry
{
  lmd_event event;

  virtual void reset() {
    timestamp = 0;
    event.release();
  }

  virtual lmd_event* emit();

  virtual void return_to_pool() {
    triggerevent_queue* q = reinterpret_cast<triggerevent_queue*>(this->pool);
    pool_delete(*q, this);
  }
};

struct dtasevent_entry;
typedef std::deque< dtasevent_entry* > dtasevent_queue;

struct dtasevent_entry : public event_entry
{
  lmd_subevent_10_1_host _header;
  std::vector<uint32_t> data;
  bool fragment;
  int64_t fragment_wr;
  bool pulser;

  dtasevent_entry() : data() { data.reserve(10000); reset(); }

  virtual void reset() {
    timestamp = 0;
    pulser = false;
    data.clear();
  }

  virtual lmd_event* emit();

  virtual void return_to_pool() {
    dtasevent_queue* q = reinterpret_cast<dtasevent_queue*>(this->pool);
    pool_delete(*q, this);
  }
};

struct lmd_source_multievent : public lmd_source
{
protected:
  enum file_status_t { aida_event, other_event, eof, unknown_event, dtas_event };

  int64_t load_event_wr;
  int64_t emit_wr;
  int64_t last_mbs_wr;
  int emit_skip;
  int aida_skip;
  int dtas_skip;

  lmd_event_hint event_hint;

  aidaevent_queue aida_events_pool;
  triggerevent_queue trigger_events_pool;
  dtasevent_queue dtas_events_pool;

  aidaevent_entry* cur_aida;
  dtasevent_entry* cur_dtas;

  //aidaevent_queue aida_events_merge;
  //aidaevent_queue aida_events_dump;
  //triggerevent_queue trigger_event;
  //dtasevent_queue dtas_events;
  std::priority_queue<event_entry*, std::vector<event_entry*>, compare_entry> events;
#if BPLAST_DELAY_FIX
  triggerevent_entry plastic_buffer;
#endif
#if FATIMA_DELAY_FIX
  triggerevent_entry fatima_buffer;
#endif

  file_status_t load_events();

  void load_aida(lmd_subevent *se, char* pb_start, char* pb_end, int64_t mbs_wr);
  void load_dtas(lmd_subevent *se, char* pb_start, char* pb_end, int64_t mbs_wr);
  void load_other();

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
  wrts_header(uint32_t id, uint64_t ts):
    system_id(id),
    lower16(   0x03e10000 | (0xffff & (uint32_t)(ts    ))),
    midlower16(0x04e10000 | (0xffff & (uint32_t)(ts>>16))),
    midupper16(0x05e10000 | (0xffff & (uint32_t)(ts>>32))),
    upper16(   0x06e10000 | (0xffff & (uint32_t)(ts>>48)))
  {}
};

#endif
