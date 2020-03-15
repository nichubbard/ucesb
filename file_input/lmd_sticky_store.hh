
#ifndef __LMD_STICKY_STORE_HH__
#define __LMD_STICKY_STORE_HH__

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

struct lmd_sticky_meta_event
{
  size_t _data_offset;
  size_t _data_length; // may include timestamp subevent
  uint32_t _num_sub;   // number of subevent metadata entries
  uint32_t _live_sub;  // decreased as subevents are revoked
};

struct lmd_sticky_meta_subevent
{
  size_t  _ev_offset;
  ssize_t _data_offset; // -1 for data not live any longer
  size_t  _data_length;
};

struct lmd_sticky_hash_subevent
{
  ssize_t _sub_offset; // -1 for revoked
};

class lmd_event_out;
class lmd_output_buffered;

class lmd_sticky_store
{
public:
  lmd_sticky_store();
  ~lmd_sticky_store();

protected:
  char   *_data;
  size_t  _data_end;
  size_t  _data_alloc;

  size_t  _data_revoked;

protected:
  char   *_meta;
  size_t  _meta_end;
  size_t  _meta_alloc;

  size_t  _meta_revoked;

protected:
  
  lmd_sticky_hash_subevent *_hash;
  size_t  _hash_used;
  size_t  _hash_size;

public:
  void insert(const lmd_event_out *event,
	      bool discard_revoke);

  void write_events(lmd_output_buffered *dest);

protected:
  void compact_data();
  void compact_meta();
  void populate_hash();

  void insert_hash(lmd_sticky_meta_subevent *sev);

  void verify_meta();

};

#endif//__LMD_STICKY_STORE_HH__
