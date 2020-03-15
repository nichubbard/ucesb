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

#include "event_reader.hh"

#include "data_queues.hh"
#include "thread_buffer.hh"

#include "event_base.hh"

#include "config.hh"

// When running threaded, the event reader is put in it's own thread.

// The main reason is that file reading (map_range (both of file_mmap
// and pipe_buffer may block until data is available)).  Since most
// certainly do not want to handle that inside all the reading
// functions, we allow this thread to simply block, and hope someone
// else takes over.  Caveat: when running mmap()ed, there is of course
// no reason why the mmap call itself would block, we'll rather block
// when actually accessing the pages of data.  As long as the file
// reader access at least on byte in each case, the file reader thread
// (this one) will take the hit for each page that has not been paged
// in yet.  If the subevents get larger than 4096k, such that they
// cover an entire page, the blocking may actually happen inside the
// unpacker functions.  Such large subevents are seldom!  Let's anyhow
// not read those pages in, since there is no sense in faulting them
// into the page tables.  Chances are that the subevent is not
// requested (not so likely though)

// Since event reading also is expected to be a light-weight operation
// (in comparision to unpacking/mapping/analysis/ntuple-writing), it
// should be fairly harmless to let this thread wonder around the
// cpus...  It may then perhaps manage to do it's work on an cpu
// standing around waiting?


event_reader::~event_reader()
{

}


static int insert_queue = 0;


void event_reader::wait_for_unpack_queue_slot()
{
  // We must have a slot to insert an event, or we must wait
  for ( ; ; )
    {
      if (_unpack_event_queue.can_insert())
	return;

      TDBG("none available");

      _unpack_event_queue.request_insert_wakeup(&_block);
      if (_unpack_event_queue.can_insert())
	{
	  _unpack_event_queue.cancel_insert_wakeup();
	  // _block.cancel_wakeup();
	  return;
	}
      TDBG("waiting");
      _block.block();
    }
}


void *event_reader::worker()
{
  TDBG("");

  // Get the next file from the input queue, and process events

  for ( ; ; )
    {
      while (!_open_file_queue.can_remove())
	{
	  // No items available, go to sleep...

	  _open_file_queue.request_remove_wakeup(&_block);
	  // Try again...
	  if (_open_file_queue.can_remove())
	    {
	      _open_file_queue.cancel_remove_wakeup();
	      break;
	    }

	  // Now, we wait for someone to wake us up.

	  _block.block();
	}

      // There is data available in the queue

      ofq_item &item = _open_file_queue.next_remove();

      int info = item._info;
      data_input_source *source = item._source;

      // If there are any reclaim items, they are all messages.  There
      // may be NOTHING to reclaim that is bound to the file itself,
      // since we'll send the reclaim info along directly, and it thus
      // will be released now!

      if (item._reclaim)
	{
	  // Make sure the queue has space for an item...
	  wait_for_unpack_queue_slot();

	  // We may now use the next entry in the queue
	  eq_item &send_item      = _unpack_event_queue.next_insert(/*0*/(insert_queue++)%MAX_THREADS);

	  send_item._info         = EQ_INFO_MESSAGE;
	  send_item._event        = NULL; // there is no event payload
	  send_item._reclaim      = item._reclaim;
	  send_item._last_reclaim = item._last_reclaim; // not needed, but anyhow

	  // We're done preparing the item
	  _unpack_event_queue.insert();
	}

      // Remove the item from the input queue

      _open_file_queue.remove();

      // Enqueue an item through the pipes with the information about the file

      if (source)
	{
	  // Now, process all the events from this file

	  process_file(source);
	}

      if (info & OFQ_INFO_FLUSH)
	{
	  // We are to flush ALL the queues.  But for each fan_in/out
	  // queue, we only need to send the item through one queue.
	  // But ALL the fan_out queues must be woken up.  (if they
	  // have events in them...  as we are the event producer, we
	  // now if avail may be larger than done (only our thread can
	  // move avail forward)

	  // Make sure the queue has space for an item...
	  wait_for_unpack_queue_slot();

	  // We may now use the next entry in the queue
	  eq_item &send_item      = _unpack_event_queue.next_insert(/*0*/(insert_queue++)%MAX_THREADS);

	  send_item._info         = (info & (OFQ_INFO_FLUSH | OFQ_INFO_DONE));
	  send_item._event        = NULL; // there is no event payload
	  send_item._reclaim      = NULL;
	  send_item._last_reclaim = NULL; // not needed, but anyhow

	  // We're done preparing the item
	  _unpack_event_queue.insert();

	  // now, fire of the flushing

	  _unpack_event_queue.flush_avail();
	}
    }

  return NULL;
}



void event_reader::process_file(data_input_source *source)
{
  // First, we make sure that the file reader reads a record,
  // and print the file header if needed...

  _reader.take_over(*source);
  _reader.new_file();

  // Since the reading of the first record may cause errors (or
  // generate information text), we must have a context for storing
  // such information

  {
    // Make sure the queue has space for an item...
    wait_for_unpack_queue_slot();

    // We may now use the next entry in the queue
    eq_item &send_item      = _unpack_event_queue.next_insert(/*0*/(insert_queue++)%MAX_THREADS);

    send_item._info         = EQ_INFO_MESSAGE;
    send_item._event        = NULL; // there is no event payload
    send_item._reclaim      = NULL;
    // Any error(info messages goes to this queue item
    _wt._last_reclaim       = &send_item._reclaim;

    TDBG("read first record");

    bool failed = false;

    try {
      if (!_reader.read_record())
	ERROR("No record in file.");

      /*
      if (_conf._print_buffer)
	loop._source.print_file_header();
      */
    } catch (error &e) {
      WARNING("Skipping this file...");
      failed = true;
    }

    // Stop giving error messages to queue item
    send_item._last_reclaim = _wt._last_reclaim;
    _wt._last_reclaim = NULL;

    // Insert the messages (if any) into the queue
    _unpack_event_queue.insert();

    if (failed)
      goto no_more_events;
  }
  // Loop over all events
  for ( ; ; )
    {
      wait_for_unpack_queue_slot();

      TDBG("extract event");

      // We may now use the next entry in the queue
      eq_item &send_item      = _unpack_event_queue.next_insert(/*0*/(insert_queue++)%MAX_THREADS);

      send_item._info         = 0;
      send_item._event        = NULL; // there is no event payload
      send_item._reclaim      = NULL;
      // Any error(info messages goes to this queue item
      _wt._last_reclaim       = &send_item._reclaim;

      try {
	event_base *eb = (event_base *)
	  _wt._defrag_buffer->allocate_reclaim(sizeof (event_base));

	memset(eb,0,sizeof(event_base));

	eb->_file_event = _reader.get_event();

	if (!eb->_file_event)
	  break; // we are done with this file.  (no event will be inserted...)

	// It does not really matter that we are after the
	// if-statement, but this way, the _event pointer is null when
	// there anyhow is nothing.  The buffer space will be
	// reclaimed in any case

	// And possibly mark the event for printing

	if (_conf._print)
	  {
	    if (_conf._data)
	      send_item._info |= (EQ_INFO_PRINT_EVENT | EQ_INFO_PRINT_EVENT_DATA);
	    else
	      send_item._info |= EQ_INFO_PRINT_EVENT;
	  }

	send_item._info  |= EQ_INFO_PROCESS;
	send_item._event = eb;
      } catch (error &e) {
	WARNING("Skipping this file...");
      }

      // Buffer header printing...

      // This should really be moved into the event unpackers, such
      // that _all_ buffer headers get printed
      /*
      if (_conf._print_buffer)
	{
	  if (n_buf_print != loop._source.buffer_no())
	    {
	      loop._source.print_buffer_header();
	      n_buf_print = loop._source.buffer_no();
	    }
	}
      */
      // Event printing (this is not exactly optimal to run in
      // threaded mode, since we'll remember the full event as text,
      // and print it at retirement.  But this is necessary to keep
      // the in-order execution.

      //if (_conf._print)
      //	loop._source.print_event(_conf._data);

      // We must decide into which queue we will insert the _next_
      // event (this since the fan_in queue gets the information one
      // event before where to look for the next

      // We'd like to evenly distribute the events over the queues,
      // but in such a way that we keep the memory localized, i.e. at least
      // try to give full pages to one thread

      // In case some queue unexpectedly drained, we'll do an
      // emergency sweep and fill (the next event) into that one instead of the
      // predetermined order.








      // Stop giving error messages to queue item
      send_item._last_reclaim = _wt._last_reclaim;
      _wt._last_reclaim = NULL;

      // The event is available for processing, insert it
      _unpack_event_queue.insert();
    }

 no_more_events:

#ifdef USE_THREADING
  // Now, if we have stopped reading the file prematurely, then we
  // must make sure that another file gets opened ASAP!
  _reader._input._input->request_next_file();
#endif

  TDBG("file processed");

  // Take back the source
  source->take_over(_reader);

  // We are done with this file
  // _reader.close();

  {
    // Make sure the queue has space for an item...
    wait_for_unpack_queue_slot();

    // We may now use the next entry in the queue
    eq_item &send_item      = _unpack_event_queue.next_insert(/*0*/(insert_queue++)%MAX_THREADS);

    send_item._info         = EQ_INFO_FILE_CLOSE;
    send_item._event        = source; // The source item to be removed
    send_item._reclaim      = NULL;
    send_item._last_reclaim = NULL; // there will be no error messages

    // Insert the messages (if any) into the queue
    _unpack_event_queue.insert();
  }
}


