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

#include "variable.hh"
#include "param_arg.hh"
#include "bits_spec.hh"
#include "parse_error.hh"

#include <assert.h>

void const32::dump(dumper &d) const
{
  char buf[64];

  if (_value < 256)
    sprintf (buf,"%d",_value);
  else
    sprintf (buf,"0x%0*x",_value > 0xffff ? 8 : (2),_value);
  d.text(buf);
}

void variable::dump_bracket(dumper &d,const prefix_ident* prefix)
{
  d.text("[");
  dump(d,prefix);
  d.text("]");
}

void var_name::dump(dumper &d,const prefix_ident* prefix) const
{
  if (prefix)
    d.text(prefix->get_prefix(_name));
  d.text(_name);
}

bool var_name::eval(/*dumper &d,*/const arguments *args,uint32 *val) const
{
  if (!args)
    return false;

  // try to find a mapping in our arguments

  arguments_map::const_iterator i;

  i = args->_arg_map.find(_name);

  if (i != args->_arg_map.end())
    {
      //d.text_fmt("var_name(%s)",_name);
      return i->second->eval(/*d,*/args->_prev,val);
    }

  // if no mapping, then try the default parameters, if any

  params_def_map::const_iterator j;

  j = args->_param_def_map.find(_name);

  if (j != args->_param_def_map.end())
    {
      //d.text_fmt("const(%s)",_name);
      *val = j->second->_value;
      return true;
    }

  /*
  d.text_fmt("var_const(%08x)",_value._value);
  *val = _value._value;
  return true;
  */
  return false;
}

void var_sub::dump(dumper &d,const prefix_ident* prefix) const
{
  var_name::dump(d,prefix);
  if (_index != -1)
    d.text_fmt("[%d]",_index);
  d.text(".");
  _child->dump(d,NULL);
}

bool var_sub::eval(/*dumper &*//*d*//*,*/const arguments */*args*/,uint32 */*val*/) const
{
  return false;
}

void var_index::dump(dumper &d,const prefix_ident* prefix) const
{
  var_name::dump(d,prefix);
  if (_index2 != -1)
    d.text_fmt("[%d]",_index2);
  d.text_fmt("[%d]",_index);
}

bool var_index::eval(/*dumper &*//*d*//*,*/const arguments */*args*/,uint32 */*val*/) const
{
  return false;
}

void var_indexed::dump(dumper &d,const prefix_ident* prefix) const
{
  var_name::dump(d,prefix);
  if (_index2)
    _index2->dump_bracket(d,prefix);
  _index->dump_bracket(d,prefix);
}

bool var_indexed::eval(/*dumper &*//*d*//*,*/const arguments */*args*/,uint32 */*val*/) const
{
  return false;
}

void var_const::dump(dumper &d,const prefix_ident* /*prefix*/) const
{
  _value.dump(d);
}

bool var_const::eval(/*dumper &*//*d*//*,*/const arguments */*args*/,uint32 *val) const
{
  //d.text_fmt("var_const(%08x)",_value._value);
  *val = _value._value;
  return true;
}

void var_external::dump(dumper &d,const prefix_ident* prefix) const
{
  d.text_fmt("EXTERNAL %s",_name);
}

bool var_external::eval(/*dumper &*//*d*//*,*/const arguments */*args*/,uint32 */*val*/) const
{
  return false;
}

const char *var_expr_op_str[] =
  {
    "???", "-", "~",
    "!", "||", "&&",
    "+", "-", "*", "/", "%",
    "&", "|", "^",
    ">>", "<<",
    "<", ">",
    "<=", ">=",
    "==", "!="
  };

void var_expr::dump(dumper &d,const prefix_ident* prefix) const
{
  // d.text_fmt("EXTERNAL %s",_name);

  d.text("(");
  if (_lhs)
    _lhs->dump(d,prefix);
  d.text_fmt(" %s ",var_expr_op_str[_op]);
  if (_rhs)
    _rhs->dump(d,prefix);
  d.text(")");
}

bool var_expr::eval(/*dumper &d,*/const arguments *args,uint32 *val) const
{
  uint32 lhs = 0, rhs = 0;

  if (_lhs)
    if (!_lhs->eval(/*d,*/args,&lhs))
      return false;

  if (_rhs)
    if (!_rhs->eval(/*d,*/args,&rhs))
      return false;

  switch (_op)
    {
    case VAR_OP_NEG: *val =     - rhs;  break;
    case VAR_OP_NOT: *val =     ~ rhs;  break;
    case VAR_OP_LNOT: *val =    ! rhs;  break;
    case VAR_OP_LOR:  *val = lhs || rhs;  break;
    case VAR_OP_LAND: *val = lhs && rhs;  break;
    case VAR_OP_ADD: *val = lhs + rhs;  break;
    case VAR_OP_SUB: *val = lhs - rhs;  break;
    case VAR_OP_MUL: *val = lhs * rhs;  break;
    case VAR_OP_DIV:
      if (rhs == 0)
	{
	  // Sorry, have no location information.
	  ERROR_LOC(_loc,
		    "Division by zero when evaluating constant expression.");
	}
      *val = lhs / rhs;
      break;
    case VAR_OP_REM:
      if (rhs == 0)
	{
	  // Sorry, have no location information.
	  ERROR_LOC(_loc,
		    "Remainder (division) by zero when evaluating constant expression.");
	}
      *val = lhs % rhs;
      break;
    case VAR_OP_AND: *val = lhs & rhs;  break;
    case VAR_OP_OR : *val = lhs | rhs;  break;
    case VAR_OP_XOR: *val = lhs ^ rhs;  break;
    case VAR_OP_SHR: *val = lhs >> rhs; break;
    case VAR_OP_SHL: *val = lhs << rhs; break;
    case VAR_OP_LS: *val = lhs < rhs; break;
    case VAR_OP_GR: *val = lhs > rhs; break;
    case VAR_OP_LE: *val = lhs <= rhs; break;
    case VAR_OP_GE: *val = lhs >= rhs; break;
    case VAR_OP_EQ: *val = lhs == rhs; break;
    case VAR_OP_NEQ: *val = lhs != rhs; break;
    default:
      assert(false); // internal error
      return false;
    }

  // d.text_fmt("{ %d %s %d -> %d}",lhs,var_expr_op_str[_op],rhs,*val);

  return true;
}

void var_cast::dump(dumper &d,const prefix_ident* prefix) const
{
  // d.text_fmt("EXTERNAL %s",_name);

  d.text_fmt("static_cast<%s>(",_type);
  _rhs->dump(d,prefix);
  d.text(")");
}

bool var_cast::eval(/*dumper &*//*d*//*,*/const arguments */*args*/,uint32 */*val*/) const
{
  return false;
}




bool difference(const var_name *first,
		const var_name *last,
		int &diff_index,
		int &diff_length)
{
  diff_index = -1;
  diff_length = 0;

  int index = 0;

  for ( ; ; )
    {
      if (first->_name != last->_name)
	return false;

      // it is either a
      // var_sub   (which can continue)
      // var_name  (is terminal)
      // var_index (is terminal)

      const var_sub *vs1 = dynamic_cast<const var_sub*>(first);
      const var_sub *vs2 = dynamic_cast<const var_sub*>(last);

      if (vs1 && vs2)
	{
	  if (vs1->_index != vs2->_index)
	    {
	      if (diff_index != -1)
		return false;

	      diff_index = index;
	      diff_length = vs2->_index - vs1->_index;
	    }

	  first = vs1->_child;
	  last  = vs2->_child;

	  index++;
	  continue;
	}

      if (vs1 || vs2)
	return false;

      const var_index *vi1 = dynamic_cast<const var_index*>(first);
      const var_index *vi2 = dynamic_cast<const var_index*>(last);

      if (vi1 && vi2)
	{
	  if (vi1->_index != vi2->_index)
	    {
	      if (diff_index != -1)
		return false;

	      diff_index = index + 0;
	      diff_length = vi2->_index - vi1->_index;
	    }

	  if (vi1->_index2 != vi2->_index2)
	    {
	      if (diff_index != -1)
		return false;

	      diff_index = index + 1;
	      diff_length = vi2->_index2 - vi1->_index2;
	    }

	  return true;
	}

      if (vi1 || vi2)
	return false;

      return true;
    }
}

const var_name *generate_indexed(const var_name *first,
				 int diff_index,
				 int diff_offset)
{
  const var_name *result;
  const var_name **assign = &result;
  int index = 0;

  for ( ; ; )
    {
      const var_sub *vs = dynamic_cast<const var_sub*>(first);

      if (vs)
	{
	  var_sub *vs_new = new var_sub(file_line(-1),
					vs->_name,NULL,vs->_index);

	  if (diff_index == index)
	    vs_new->_index  += diff_offset;

	  *assign = vs_new;
	  assign = &vs_new->_child;
	  first = vs->_child;
	  index++;
	  continue;
	}

      const var_index *vi = dynamic_cast<const var_index*>(first);

      if (vi)
	{
	  var_index *vi_new = new var_index(file_line(-1),
					    vi->_name,
					    vi->_index,
					    vi->_index2);

	  // fprintf (stderr,"%d %d  \n",vi->_index,vi->_index2);

	  *assign = vi_new;

	  if (diff_index == index+0)
	    vi_new->_index  += diff_offset;
	  if (diff_index == index+1)
	    vi_new->_index2 += diff_offset;

	  return result;
	}

      // it is a var_name

      *assign = new var_name(file_line(-1), first->_name);

      return result;
    }
}
