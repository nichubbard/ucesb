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

#include "queue_selector.hh"


int queue_selector::next_queue()
{
  // until things work: simple round-robin

  _current++;

  return _current;

#if 0
  if (likely(!_drained))
    {
      _current_left--;

      if (likely(_current_left))
	return _current;
    }

  // Go over the queues and find out who is in most need of more events.
  // If we find anyone being empty, we'll enque events into that one, without
  // further looking into the other ones

  for (int i = 0; i < _queues; i++)
    {
      // Find out the status of the queue, how many events are in the queue

      int items = queue[i]->fill();

      if (unlikely(!items))
	{
	  _bunches >>= 3; // send fewer items per queue (restart the round robin)

	  _current_left = _bunches;

	  // but do not try to emit more elements than the queue has free slots!
	  // (avoid unnecessary stops)

	  int free_slots = queue[i]->slots() - items - 1 /* the one given now*/;

	  _current_left = (_bunches < free_slots) ? _bunches ? free_slots;
	  return _current = i;
	}

      // Compare the number of elements in the queue to the desired
      // fill factor, i.e. a queue that we do not want to give so much
      // work (because it also has other tasks)

      // Now for the 'queue' that fills into our own thread, there
      // will never be any entries, as we would process the events
      // directly, and send them along to the next stage...

      int factor = items / fraction[i];

      if (factor > min_factor)
	continue;

      min_factor = factor;

      // Now that


    }
#endif

}
