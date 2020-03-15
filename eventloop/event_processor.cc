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

#include "event_processor.hh"

#include "data_queues.hh"
#include "thread_buffer.hh"

#include "event_base.hh"
#include "event_loop.hh"

#include "config.hh"

event_processor::~event_processor()
{

}

void event_processor::init(int index)
{
  worker_thread::init();

  _queues.init(index);
}

void event_processor::wait_for_output_queue_slot()
{
  // We must have a slot to insert an event, or we must wait
  for ( ; ; )
    {
      if (_queues._retire->can_insert())
	return;

      TDBG("none available");
      _queues._retire->request_insert_wakeup(&_block);
      if (_queues._retire->can_insert())
	{
	  _queues._retire->cancel_insert_wakeup();
	  return;
	}
      TDBG("waiting");
      _block.block();
    }
}

void event_processor::wait_for_input_queue_item()
{
  // We must have a slot to insert an event, or we must wait
  for ( ; ; )
    {
      if (_queues._unpack->can_remove())
	return;

      TDBG("none available");
      _queues._unpack->request_remove_wakeup(&_block);
      if (_queues._unpack->can_remove())
	{
	  _queues._unpack->cancel_remove_wakeup();
	  return;
	}
      TDBG("waiting");
      _block.block();
    }
}

void *event_processor::worker()
{
  TDBG("");

  // Take in events from our input queue, find a slot in the ouput
  // queue, and process the event


  // If there is no free slot in the output queue, it makes no sense
  // to ask for an event from the input.  Because if the output is
  // full and the input is empty, to most pressing problem is the lack
  // of an output slot, since without that we can anyhow not continue

  for ( ; ; )
    {
      wait_for_output_queue_slot();
      wait_for_input_queue_item();

      int next_queue;

      eq_item &recv_item = _queues._unpack->next_remove(&next_queue);
      eq_item &send_item = _queues._retire->next_insert(next_queue);

      // First just make a copy of the item to at least send it along.
      // *all* items are sent along, since they may contain reclaim
      // items (among other: messages) that are not to get lost

      send_item = recv_item;

      // Now that we have taken over the data from the input slot, we
      // may release it.
      _queues._unpack->remove();

      // Set up the pointers for where to put any error messages
      _wt._last_reclaim       = send_item._last_reclaim;

      // Is there any processing requested?
      if (send_item._info & EQ_INFO_PROCESS)
	{
	  try {

	    ////////////////////////////////////////////////////
	    // THIS IS TO BE MOVED INTO THE event_loop.cc

	    event_base *eb = (event_base *) send_item._event;

#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)
	    ucesb_event_loop::pre_unpack_event(*eb, &_hints);
#endif

	    ucesb_event_loop::unpack_event(*eb);
	    /*
	    int a, b;

	    for (int i = 0; i < 10000000; i++)
	      {
		a = a + b; b ^= i + a;
		a |= i;
	      }
	    */
	    ////////////////////////////////////////////////////

	    // process_event();
	  } catch (error &e) {

	    // If an error occured during processing, remove the
	    // PROCESS flag

	    send_item._info &= ~EQ_INFO_PROCESS;
	    send_item._info |=  EQ_INFO_DAMAGED;

	    // And possibly mark the event for printing

	    if (_conf._debug)
	      send_item._info |= (EQ_INFO_PRINT_EVENT | EQ_INFO_PRINT_EVENT_DATA);
	  }
	}

      // Quit delivering error messages here
      send_item._last_reclaim = _wt._last_reclaim;
      _wt._last_reclaim = NULL;

      int info = send_item._info;

      // We have produced all information we want.  Let someone
      // operate on this item.
      _queues._retire->insert();
      // You may no longer use send_item!!!

      if (info & EQ_INFO_FLUSH)
	{
	  // This must be after the insert(), because the insert may
	  // not flush if the queue did not get full enough to reach
	  // wakeup threshold.  But this will flush if there is any
	  // item left at all (e.g. send_item).
	  _queues._retire->flush_avail();
	}
    }

  return NULL;
}
