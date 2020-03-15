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

#include "open_retire.hh"
#include "thread_info.hh"

#include "data_queues.hh"

#include "decompress.hh"

#include "lmd_input.hh"

open_retire::open_retire()
{

}

open_retire::~open_retire()
{

}


void open_retire::init()
{
  worker_thread::init();

  _input_iter = _inputs.begin();

}


bool open_retire::open_file(int wakeup_this_file,
			    thread_block *block_reader)
{
  if (_input_iter == _inputs.end())
    {
      // Enqueue an end-of-files item
      send_flush(OFQ_INFO_DONE);
      return true; // no more files to open
   }

  TDBG("open file: %s",_input_iter->_name);

  ofq_item &send_item = _open_file_queue.next_insert();

  send_item._info    = OFQ_INFO_FILE;
  send_item._source  = new data_input_source;
  send_item._reclaim = NULL;
  // Any error(info messages goes to this queue item
  _wt._last_reclaim = &send_item._reclaim;

  bool success = true;

  try {
#if 0 /* Handled below with normal open(). */
#ifdef USE_RFIO
    if (_input_iter->_type == INPUT_TYPE_RFIO)
      {
	send_item._source->open_rfio(_input_iter->_name,
				     block_reader,
				     &_block,
				     wakeup_this_file);

	TDBG("rfio file opened %p",send_item._source->_input._input);
      }
    else
#endif
#endif
#ifdef USE_LMD_INPUT
    if (_input_iter->_type != INPUT_TYPE_FILE &&
	_input_iter->_type != INPUT_TYPE_RFIO &&
	_input_iter->_type != INPUT_TYPE_FILE_SRM)
      {
	send_item._source->connect(_input_iter->_name,
				   _input_iter->_type,
				   block_reader,
				   &_block,
				   wakeup_this_file);

	TDBG("connection opened %p",send_item._source->_input._input);

	_ti_info._file_input = send_item._source->_input._input;
      }
    else
#endif
      {
	send_item._source->open(_input_iter->_name,
				block_reader,
				&_block,
				wakeup_this_file,
				_conf._no_mmap,
				_input_iter->_type == INPUT_TYPE_RFIO,
				_input_iter->_type == INPUT_TYPE_FILE_SRM);

	TDBG("file opened %p",send_item._source->_input._input);

	_ti_info._file_input = send_item._source->_input._input;
      }
    // Enqueue the file

  } catch (error &e) {
    WARNING("Skipping this file...");
    // Make sure another file gets tried
    // Enqueue an file-open-error item
    delete send_item._source;
    send_item._source = NULL;
    send_item._info   = OFQ_INFO_MESSAGE;
    success = false;
  }

  // Stop giving error messages to queue item
  send_item._last_reclaim = _wt._last_reclaim;
  _wt._last_reclaim = NULL;

  // The item is now available for processing
  _open_file_queue.insert();

  // TODO: if the event reader was blocking, waiting for a file to become
  // available, tell him!

  ++_input_iter;

  return success;
}

void open_retire::send_flush(int info)
{
  ofq_item &send_item = _open_file_queue.next_insert();

  send_item._info    = OFQ_INFO_FLUSH | info;
  send_item._source  = NULL;
  send_item._reclaim = NULL;
  send_item._last_reclaim = NULL;

  _open_file_queue.insert();

  _open_file_queue.flush_avail();
}

void open_retire::close_file(data_input_source *source)
{
  if (_ti_info._file_input == source->_input._input)
    _ti_info._file_input = NULL;

  source->close();
}




void *open_retire::worker()
{
  // this thread is the main tread, so never spawned
  assert(false);
  return NULL;
}




