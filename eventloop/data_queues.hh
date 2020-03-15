/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#ifndef __DATA_QUEUES_HH__
#define __DATA_QUEUES_HH__

#include "thread_queue.hh"
#include "reclaim.hh"

//////////////////////////////////////////////////////////////////////

#define OFQ_INFO_MESSAGE          0x01000000
#define OFQ_INFO_FILE             0x10000000
#define OFQ_INFO_FLUSH            0x04000000
#define OFQ_INFO_DONE             0x08000000

class data_input_source;

struct ofq_item
{
  int                _info;

  data_input_source *_source;
  reclaim_item      *_reclaim;
  reclaim_item     **_last_reclaim;
};

//////////////////////////////////////////////////////////////////////

// The process_mask is superfluos, since the queue in which the event
// is currently waiting, tells what is to be done next

#define EQ_INFO_NEXT_PROCESS_MASK 0x000000ff // mask to tell what is next processing stage
#define EQ_INFO_PROCESS           0x00000100 // event is to continue processing
#define EQ_INFO_DAMAGED           0x00000200 // event was damaged (no further processing)

#define EQ_INFO_PRINT_EVENT       0x00010000 // event header should be printed
#define EQ_INFO_PRINT_EVENT_DATA  0x00020000 // event data should be printed

#define EQ_INFO_PRINT_BUFFER_HEADERS 0x00040000 // there are headers to print

#define EQ_INFO_MESSAGE           0x01000000 // we have no event, only a message item

#define EQ_INFO_FILE_CLOSE        0x02000000 // _event point to an input_buffer to close

#define EQ_INFO_FLUSH             (OFQ_INFO_FLUSH) // last item in queue, flush the output queue(s)!
#define EQ_INFO_DONE              (OFQ_INFO_DONE)  // done, there will be nothing further!

struct eq_item
{
  int                _info;

  // FILE_INPUT_EVENT *event;
  void *_event;
  reclaim_item      *_reclaim;
  reclaim_item     **_last_reclaim;
};

//////////////////////////////////////////////////////////////////////

#define MAX_THREADS           4

//////////////////////////////////////////////////////////////////////
// Queue for opened data files that should be processed

#define DATA_FILE_QUEUE_LEN   4

extern single_thread_queue<ofq_item,DATA_FILE_QUEUE_LEN> _open_file_queue;

//////////////////////////////////////////////////////////////////////
// Queue for events that are to be processed.

#define UNPACK_QUEUE_LEN     8192 // must be power of 2

extern fan_out_thread_queue<eq_item,UNPACK_QUEUE_LEN>    _unpack_event_queue;

//////////////////////////////////////////////////////////////////////
// Queue for events that are completely processed and may be retired.

#define RETIRE_QUEUE_LEN     8192 // must be power of 2

extern fan_in_thread_queue<eq_item,RETIRE_QUEUE_LEN>     _retire_queue;

//////////////////////////////////////////////////////////////////////

struct processor_thread_data_queues
{
  fan_out_thread_one_queue<eq_item,UNPACK_QUEUE_LEN> *_unpack;
  fan_in_thread_one_queue<eq_item,RETIRE_QUEUE_LEN>  *_retire;

public:
  void init(int index);
};

//////////////////////////////////////////////////////////////////////

#ifndef NDEBUG
void debug_queues_status();
#endif

#endif//__DATA_QUEUES_HH__

