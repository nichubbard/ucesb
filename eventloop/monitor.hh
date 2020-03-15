
#ifndef __MONITOR_HH__
#define __MONITOR_HH__

#include <stdint.h>

#define EXT_WRITER_UCESB_MONITOR_DEFAULT_PORT  56579

struct status_monitor
{
  uint64_t _events;
  uint64_t _multi_events;
  uint64_t _errors;
};

extern status_monitor _status;

struct status_block
{
  status_monitor _copy[2];

  int      volatile _copy_seq[2][2];
  int      _copy_seq_done;

  int      _update_seq;

  volatile int _mon_update_seq;
};

extern status_block _status_block;

void mon_block_update(status_block *handle, status_monitor *data);

#define MON_CHECK_COPY_BLOCK(handle, data)		\
  do {							\
    int update_seq = (handle)->_mon_update_seq;		\
    if (update_seq != (handle)->_update_seq) {		\
      (handle)->_update_seq = update_seq;		\
      mon_block_update(handle,data);			\
    }							\
  } while (0)

void start_monitor_thread(int port);

#endif/*__MONITOR_HH__*/
