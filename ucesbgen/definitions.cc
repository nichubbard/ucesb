/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
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

#include "definitions.hh"
#include "parse_error.hh"
#include "unpack_code.hh"

#include "signal.hh"

def_node_list *all_definitions;

named_structure_map all_named_structures;
named_structure_map all_subevent_structures;

signal_map all_signals;
signal_multimap all_signals_no_ident;

signal_info_map all_signal_infos;

event_definition *the_event;
event_definition *the_sticky_event;

struct_definition *find_named_structure(const char *name)
{
  named_structure_map::iterator i;

  i = all_named_structures.find(name);

  if (i == all_named_structures.end())
    return NULL;

  return i->second;
}

struct_definition *find_subevent_structure(const char *name)
{
  named_structure_map::iterator i;

  i = all_subevent_structures.find(name);

  if (i == all_subevent_structures.end())
    return NULL;

  return i->second;
}

void insert_signal_to_all(signal_spec *signal)
{
  signal_map::iterator i;

  i = all_signals.find(signal->_name);

  if (i != all_signals.end())
    {
      signal_spec *found = i->second;
      
      /* It is ok if all tags match, except the toggle, which must not
       * match.
       */

      if (!(signal->_tag & SIGNAL_TAG_TOGGLE_MASK) ||
	  !(found->_tag & SIGNAL_TAG_TOGGLE_MASK))
	ERROR_LOC_PREV(signal->_loc, found->_loc,
		       "Several signals with name (without toggle): %s (0x%x, 0x%x)\n",
		       signal->_name, found->_tag, signal->_tag);

      if (signal->_tag & found->_tag & SIGNAL_TAG_TOGGLE_MASK)
	ERROR_LOC_PREV(signal->_loc, found->_loc,
		       "Several signals with name "
		       "(with overlapping toggle): %s\n",
		       signal->_name);
	
      if ((signal->_tag ^ found->_tag) & ~(SIGNAL_TAG_TOGGLE_MASK))
	ERROR_LOC_PREV(signal->_loc, found->_loc,
		       "Several signals with name (with toggle) "
		       "have mismatched tags: %s (0x%x, 0x%x)\n",
		       signal->_name, found->_tag, signal->_tag);

      /* We have additional toggles.  So insert them. */
      /*
	fprintf (stderr, "%p %p %p %p\n",
		 found->_ident[0],found->_ident[1],
		 signal->_ident[0],signal->_ident[1]);
      */
      for (int i = 0; i < MAX_NUM_TOGGLE; i++)
	if (signal->_ident[i])
	  {
	    assert(!found->_ident[i]);
	    found->_ident[i] = signal->_ident[i];
	  }

      found->_tag |= signal->_tag;
	
      return;
    }

  all_signals.insert(signal_map::value_type(signal->_name,
					    signal));
}

void insert_signal_to_no_ident(signal_spec *signal)
{
  all_signals_no_ident.insert(signal_multimap::value_type(signal->_name,
							  signal));
}

void insert_signal_info(signal_info *sig_info)
{
  all_signal_infos.insert(signal_info_map::value_type(sig_info->_name,
						      sig_info));
}

void mark_subevents_sticky(event_definition *evt, bool mark_sticky)
{
  const struct_decl_list *items = evt->_items;
  struct_decl_list::const_iterator i;

  for (i = items->begin(); i != items->end(); ++i)
    {
      struct_decl *decl = *i;

      if (decl->is_event_opt())
	continue;

      struct_definition *str_def;

      str_def = find_subevent_structure(decl->_ident);

      if (!str_def)
	ERROR_LOC(decl->_loc,"Failed to find subevent %s.",decl->_ident);

      if (mark_sticky)
	str_def->_opts |= STRUCT_DEF_OPTS_STICKY_SUBEV;
      else
	{
	  if (str_def->_opts & STRUCT_DEF_OPTS_STICKY_SUBEV)
	    ERROR_LOC(decl->_loc,
		      "Subevent %s used both as normal and sticky.",
		      decl->_ident);
	}
    } 
}

void map_definitions()
{
  if (!all_definitions)
    return;

  def_node_list::iterator i;

  for (i = all_definitions->begin(); i != all_definitions->end(); ++i)
    {
      def_node* node = *i;

      struct_definition *structure =
	dynamic_cast<struct_definition *>(node);

      if (structure)
	{
	  const struct_header_subevent *subevent =
	    dynamic_cast<const struct_header_subevent *>(structure->_header);

	  if (subevent)
	    {
	      if (all_subevent_structures.find(subevent->_name) !=
		  all_subevent_structures.end())
		ERROR_LOC_PREV(structure->_loc,
			       all_subevent_structures.find(subevent->_name)->second->_loc,
			       "Several subevent structures with name: %s\n",
			       subevent->_name);

	      all_subevent_structures.insert(named_structure_map::value_type(subevent->_name,
									     structure));
	    }
	  else
	    {
	      const struct_header_named *named =
		dynamic_cast<const struct_header_named *>(structure->_header);

	      if (named)
		{
		  if (all_named_structures.find(named->_name) !=
		      all_named_structures.end())
		    ERROR_LOC_PREV(structure->_loc,
				   all_named_structures.find(named->_name)->second->_loc,
				   "Several structures with name: %s\n",
				   named->_name);

		  all_named_structures.insert(named_structure_map::value_type(named->_name,
									      structure));
		}
	    }
	}

      signal_spec *signal =
	dynamic_cast<signal_spec *>(node);

      if (signal)
	{
	  signal_spec_range *signal_range =
	    dynamic_cast<signal_spec_range *>(signal);

	  bool has_ident = false;

	  for (int i = 0; i < MAX_NUM_TOGGLE; i++)
	    if (signal->_ident[i])
	      has_ident = true;	  

	  if (signal_range)
	    expand_insert_signal_to_all(signal_range);
	  else if (has_ident)
	    insert_signal_to_all(signal);
	  else
	    insert_signal_to_no_ident(signal);
	}

      signal_info *sig_info =
	dynamic_cast<signal_info *>(node);

      if (sig_info)
	{
	  insert_signal_info(sig_info);
	}

      event_definition *event =
	dynamic_cast<event_definition *>(node);

      if (event)
	{
	  if (event->_opts & EVENT_OPTS_STICKY)
	    {
	      if (the_sticky_event)
		ERROR_LOC_PREV(event->_loc,the_sticky_event->_loc,
			       "Several sticky event definitions.\n");
	      the_sticky_event = event;
	    }
	  else
	    {
	      if (the_event)
		ERROR_LOC_PREV(event->_loc,the_event->_loc,
			       "Several event definitions.\n");
	      the_event = event;
	    }
	}
    }

  if (!the_event)
    {
      // please add an EVENT to your .spec file!
      ERROR_LOC(file_line(_first_lineno_map ?
			  _first_lineno_map->_internal : 0),
		"There is no event definition (EVENT { }) "
		"in the specification.");
    }

  if (!the_sticky_event)
    {
      // Empty sticky event.  Since it does not have ignore, it will
      // dislike any actual sticky event.  But code can compile.
      the_sticky_event =
	new event_definition(file_line(),
			     new struct_decl_list);
      the_sticky_event->_opts |=
	EVENT_OPTS_STICKY | EVENT_OPTS_INTENTIONALLY_EMPTY;
    }

  mark_subevents_sticky(the_sticky_event, true);
  mark_subevents_sticky(the_event, false);
}

void generate_unpack_code()
{
  named_structure_map::iterator i;

  printf ("/**********************************************************\n"
          " * Generating unpacking code...\n"
          " */\n\n");

  for (i = all_named_structures.begin(); i != all_named_structures.end(); ++i)
    generate_unpack_code(i->second);

  for (i = all_subevent_structures.begin(); i != all_subevent_structures.end(); ++i)
    generate_unpack_code(i->second);

  if (the_event)
    generate_unpack_code(the_event);

  if (the_sticky_event)
    generate_unpack_code(the_sticky_event);

  printf ("/**********************************************************/\n");
}

void check_consistency()
{

}

/* Just so we can see that we parsed everything...
 */

void dump_definitions()
{
  print_header("INPUT_DEFINITION","All specifications as seen by the parser.");
  printf ("/**********************************************************\n"
          " * Dump of all structures:\n"
          " */\n\n");

  dumper_dest_file d_dest(stdout);
  dumper d(&d_dest);

  {
    named_structure_map::iterator i;

    for (i = all_named_structures.begin(); i != all_named_structures.end(); ++i)
      {
	i->second->dump(d);
	d.nl();
      }

    for (i = all_subevent_structures.begin(); i != all_subevent_structures.end(); ++i)
      {
	i->second->dump(d);
	d.nl();
      }
  }

  printf ("/**********************************************************\n"
          " * The event definition:\n"
          " */\n\n");

  if (the_event)
    {
      the_event->dump(d);
      d.nl();
    }

  printf ("/**********************************************************\n"
          " * The sticky_event definition:\n"
          " */\n\n");

  if (the_sticky_event &&
      !(the_sticky_event->_opts & EVENT_OPTS_INTENTIONALLY_EMPTY))
    {
      the_sticky_event->dump(d);
      d.nl();
    }

  printf ("/**********************************************************\n"
          " * Signal name mappings:\n"
          " */\n\n");

  {
    signal_map::iterator i;

    for (i = all_signals.begin(); i != all_signals.end(); ++i)
      i->second->dump(d);
  }

  {
    signal_multimap::iterator i;

    for (i = all_signals_no_ident.begin(); i != all_signals_no_ident.end(); ++i)
      i->second->dump(d);
  }

  {
    signal_info_map::iterator i;

    for (i = all_signal_infos.begin(); i != all_signal_infos.end(); ++i)
      i->second->dump(d);
  }

  printf ("/**********************************************************/\n");
  print_footer("INPUT_DEFINITION");
}
