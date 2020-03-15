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

#include "param_arg.hh"
#include "parse_error.hh"

#include <map>

void param::dump(dumper &d)
{
  d.text(_name,true);
  if (_def)
    {
      d.text("=");
      _def->dump(d);
    }
}


void argument::dump(dumper &d)
{
  d.text(_name,true);
  d.text("=");
  _var->dump(d);
}

arguments::arguments(const argument_list* src_arg,
		     const param_list* src_param,
		     const arguments *prev)
{
  if (src_arg)
    {
      argument_list::const_iterator al;

      for (al = src_arg->begin(); al != src_arg->end(); ++al)
	{
	  const argument *arg = *al;
	  _arg_map.insert(arguments_map::value_type(arg->_name,arg->_var));
	}
    }
  if (src_param)
    {
      param_list::const_iterator pl;

      for (pl = src_param->begin(); pl != src_param->end(); ++pl)
	{
	  const param *param = *pl;
	  if (param->_def)
	    _param_def_map.insert(params_def_map::value_type(param->_name,
							     param->_def));
	}
    }
  _prev = prev;
}


void dump_param_args(const file_line &loc,
		     const param_list *params,
		     const argument_list *args,
		     dumper &d,
		     bool dump_member_args)
{
  // Loop over the parameter list, and find the corresponding arguments...
  // At the end, check that all arguments got assigned...

  // for easy finding, make a map of the arguments, and remove as they get used

  arguments a(args,NULL,NULL);

  param_list::const_iterator pl;

  for (pl = params->begin(); pl != params->end(); ++pl)
    {
      param *p = *pl;

      // Do we have an argument specified?

      arguments_map::iterator i;

      i = a._arg_map.find(p->_name);

      if (p->_member && !dump_member_args)
	{
	  if (i != a._arg_map.end())
	    a._arg_map.erase(i);

	  d.text_fmt("/*,%s:member*/",p->_name);
	  continue;
	}

      d.text_fmt(",/*%s*/",p->_name);
      if (i != a._arg_map.end())
	{
	  a._arg_map.erase(i);
	  i->second->dump(d);
	}
      else if (p->_def) // or do we have a default value?
	p->_def->dump(d);
      else
	ERROR_LOC(loc,"Parameter %s has no argument.",p->_name);
    }

  // if (params->size() == 0)
  // d.text_fmt(",/*no args*/");

  // Now, the map should be empty, of we have an error

  arguments_map::iterator i;

  if (a._arg_map.size())
    {
      for (i = a._arg_map.begin(); i != a._arg_map.end(); ++i)
	WARNING_LOC(loc,"No parameter for argument %s.",i->first);
      ERROR_LOC(loc,"Missing arguments.");
    }
}

void dump_match_args(const file_line &loc,
		     const param_list *params,
		     const argument_list *args,
		     dumper &d)
{
  // Loop over the parameter list, and find the corresponding arguments...
  // At the end, check that all arguments got assigned...

  // for easy finding, make a map of the arguments, and remove as they get used

  arguments a(args,NULL,NULL);

  param_list::const_iterator pl;

  bool has_cond = false;

  for (pl = params->begin(); pl != params->end(); ++pl)
    {
      param *p = *pl;

      // Do we have an argument specified?

      arguments_map::iterator i;

      i = a._arg_map.find(p->_name);

      if (i != a._arg_map.end())
	{
	  d.text_fmt("%s(VES10_1_%s==",has_cond ? "&&" : "",p->_name);
	  a._arg_map.erase(i);
	  i->second->dump(d);
	  d.text_fmt(")");
	  has_cond = true;
	}
    }
  if (!has_cond)
    d.text("true");
}



