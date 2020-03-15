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

#include "select_event.hh"
#include "lmd_output.hh"

#include "error.hh"

#include "../common/strndup.hh"

#include <stddef.h>

struct subevent_name_info
{
  const char *_name;
  const char *_command;
};

subevent_name_info subevent_names[] =
{
#ifndef NO_SUBEVENT_NAMES
#include "gen/subevent_names.hh"
#endif
  { NULL, NULL },
};

// For the event itself, one may select on the trigger

// For the subevent, on may select on type, subtype , procid, subcrate, control


// incl=trig:6-8

// incl=type=5-9:subtype=5





#define REQUEST_TYPE_EVENT      0x01
#define REQUEST_TYPE_SUBEVENT   0x02

int select_event_request::add_item(const char *cmd)
{
  select_event_request_item item;

  // the item is string:value[-value]

  const char *equals = strchr(cmd,'=');

  if (!equals)
    ERROR("Malformed select request: %s",cmd);

  char *end;

  item._min =
    (int) strtol(equals+1,&end,strncmp(equals+1,"0x",2) == 0 ? 16 : 10);

  if (*end == '-')
    {
      item._max =
	(int) strtol(end+1,&end,strncmp(end+1,"0x",2) == 0 ? 16 : 10);

      if (*end != 0)
	ERROR("Malformed select request (bad max number): %s",cmd);
    }
  else if (*end == 0)
    item._max = item._min;
  else
    ERROR("Malformed select request (bad number): %s",cmd);

  // Ok, so we've got the numbers, now figure out what item it applies to

#define MATCH_SELECT_ITEM(name) \
  ((size_t) (equals - cmd) == strlen(name) && strncmp(cmd,name,strlen(name)) == 0)
#define MATCH_SELECT_ITEM_MEMBER(structure,member,size,type)  do { \
    item._offset = offsetof(structure,member);   \
    item._size   = size;                         \
    request_type = (type);                       \
  } while(0)

  int request_type = 0;

#ifdef USE_LMD_INPUT
  /**/ if (MATCH_SELECT_ITEM("trig") || MATCH_SELECT_ITEM("trigger"))
    MATCH_SELECT_ITEM_MEMBER(lmd_event_10_1_host,_info.i_trigger,-2,REQUEST_TYPE_EVENT);
  else if (MATCH_SELECT_ITEM("type"))
    MATCH_SELECT_ITEM_MEMBER(lmd_subevent_10_1_host,_header.i_type,-2,REQUEST_TYPE_SUBEVENT);
  else if (MATCH_SELECT_ITEM("subtype"))
    MATCH_SELECT_ITEM_MEMBER(lmd_subevent_10_1_host,_header.i_subtype,-2,REQUEST_TYPE_SUBEVENT);
  else if (MATCH_SELECT_ITEM("procid") || MATCH_SELECT_ITEM("id"))
    MATCH_SELECT_ITEM_MEMBER(lmd_subevent_10_1_host,i_procid,-2,REQUEST_TYPE_SUBEVENT);
  else if (MATCH_SELECT_ITEM("subcrate") || MATCH_SELECT_ITEM("crate"))
    MATCH_SELECT_ITEM_MEMBER(lmd_subevent_10_1_host,h_subcrate,-1,REQUEST_TYPE_SUBEVENT);
  else if (MATCH_SELECT_ITEM("control") || MATCH_SELECT_ITEM("ctrl"))
    MATCH_SELECT_ITEM_MEMBER(lmd_subevent_10_1_host,h_control,-1,REQUEST_TYPE_SUBEVENT);
#endif
#ifdef USE_RIDF_INPUT
  /**/ if (MATCH_SELECT_ITEM("dev") || MATCH_SELECT_ITEM("device"))
    MATCH_SELECT_ITEM_MEMBER(lmd_event_10_1_host,_info.i_dev,-2,REQUEST_TYPE_EVENT);
  else if (MATCH_SELECT_ITEM("fp") || MATCH_SELECT_ITEM("focalplane"))
    MATCH_SELECT_ITEM_MEMBER(lmd_subevent_10_1_host,_header.i_fp,-2,REQUEST_TYPE_SUBEVENT);
  else if (MATCH_SELECT_ITEM("det") || MATCH_SELECT_ITEM("detector"))
    MATCH_SELECT_ITEM_MEMBER(lmd_subevent_10_1_host,_header.i_det,-2,REQUEST_TYPE_SUBEVENT);
  else if (MATCH_SELECT_ITEM("mod") || MATCH_SELECT_ITEM("module"))
    MATCH_SELECT_ITEM_MEMBER(lmd_subevent_10_1_host,_header.i_mod,-2,REQUEST_TYPE_SUBEVENT);
#endif
  else
    ERROR("Malformed select request (unknown item): %s",cmd);

  _items.push_back(item);

  return request_type;
}




void select_event_requests::add_item(const select_event_request &item,bool incl,
				     const char *command)
{
  int flag = incl ? SELECT_FLAG_INCLUDE : SELECT_FLAG_EXCLUDE;

  if (_flags && !(_flags & flag))
    ERROR("Cannot mix include and exclude requests: %s",command);

  _flags |= flag;

  _items.push_back(item);
}

bool select_event_requests::has_selection()
{
  return _flags || !_items.empty();
}

select_event::select_event()
{
  _omit_empty_payload = false;
}

bool select_event::has_selection()
{
  return _omit_empty_payload ||
    _event.has_selection() ||
    _subevent.has_selection();
}

void select_event::parse_request(const char *command,bool incl)
{
  // if the request matches one of the predefined names, then
  // replace the command string with that one...
  // note: we may have several matching items.  In that case,
  // do it once per matching item

  bool matched_name = false;

  for (size_t i = 0; i < countof(subevent_names); i++)
    {
      if (subevent_names[i]._name &&
	  strcmp(subevent_names[i]._name,command) == 0)
	{
	  parse_request(subevent_names[i]._command,incl);
	  matched_name = true;
	}
    }

  if (matched_name)
    return;

  select_event_request r;

  int request_type = 0;

  // chop the request up into pieces, separated by colons

  const char *cmd = command;
  const char *req_end;

  while ((req_end = strchr(cmd,':')) != NULL)
    {
      char *request = strndup(cmd,(size_t) (req_end-cmd));

      request_type |= r.add_item(request);

      free(request);
      cmd = req_end+1;
    }

  // and handle the remainder

  request_type |= r.add_item(cmd);

  if ((request_type & REQUEST_TYPE_EVENT) &&
      (request_type & REQUEST_TYPE_SUBEVENT))
    ERROR("Select request cannot be for both event and subevent: %s",command);

  if (request_type & REQUEST_TYPE_EVENT)
    _event.add_item(r,incl,command);
  if (request_type & REQUEST_TYPE_SUBEVENT)
    _subevent.add_item(r,incl,command);
}


bool select_event_request::match(const void *ptr) const
{
  for (size_t i = 0; i < _items.size(); i++)
    {
      const select_event_request_item &item = _items[i];

      const char *p = ((const char *) ptr) + item._offset;

      int value;

      if (item._size == -1)
	value = *((const sint8 *) p);
      else if (item._size == -2)
	value = *((const sint16*) p);
      else if (item._size == -4)
	value = *((const sint32*) p);
      else
	ERROR("Iternal error.");

      if (value < item._min || value > item._max)
	return false;
    }
  return true;
}




bool select_event_requests::match(const void *ptr) const
{
  for (size_t i = 0; i < _items.size(); i++)
    {
      const select_event_request &item = _items[i];

      if (item.match(ptr))
	return true;
    }
  return false;
}

bool select_event_requests::accept(const void *ptr) const
{
  if (!_flags)
    return true;

  if (_flags & SELECT_FLAG_INCLUDE)
    return match(ptr);

  if (_flags & SELECT_FLAG_EXCLUDE)
    return !match(ptr);

  assert (false);
  return true;
}

bool select_event::accept_event(const lmd_event_10_1_host *header) const
{
  if (header->_header.i_type == LMD_EVENT_STICKY_TYPE &&
      header->_header.i_subtype == LMD_EVENT_STICKY_SUBTYPE)
    return true;
  
  return _event.accept(header);
}

bool select_event::accept_subevent(const lmd_subevent_10_1_host *header) const
{
  return _subevent.accept(header);
}

bool select_event::accept_final_event(lmd_event_out *event) const
{
  if (event->is_clear()) // No header, no nothing!
    return false;

  // Also sticky events are useless without payload, so no special
  // handling needed

  if (_omit_empty_payload)
    {
      if (!event->has_subevents())
	return false;
    }

  return true;
}
