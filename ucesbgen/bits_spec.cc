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

#include "bits_spec.hh"



void bits_cond_check::dump(dumper &d) const
{
  d.text("CHECK(");
  _check->dump(d);
  d.text(")");
}

void bits_cond_check::check(dumper &d,const char *prefix,const char *name,
			    const char *jump_target) const
{
  d.text_fmt("%s_BITS_EQUAL(%d,%s,",prefix,_loc._internal,name);
  _check->dump(d);
  if (jump_target)
    d.text_fmt(",%s",jump_target);
  d.text(");\n");
}

void bits_cond_check::get_match(dumper &/*d*/,const arguments *args,
				uint32 *fixed_mask,uint32 *fixed_value) const
{
  //d.text_fmt("EQUAL(");
  //_check->dump(d);
  //d.text_fmt(")");

  // now, if we manage to evaluate the variable, then we know something

  uint32 val;

  if (_check->eval(/*d,*/args,&val))
    {
      //d.text_fmt("%d",val);

      *fixed_mask  = (uint32) -1;
      *fixed_value = val;
    }
}

void bits_cond_match::dump(dumper &d) const
{
  d.text("MATCH(");
  _check->dump(d);
  d.text(")");
}


void bits_cond_range::dump(dumper &d) const
{
  d.text("RANGE(");
  _min->dump(d);
  d.text(",");
  _max->dump(d);
  d.text(")");
}

void bits_cond_range::check(dumper &d,const char *prefix,const char *name,
			    const char *jump_target) const
{
  // This is a hack to get the compiler silent, and not warn about
  // comparing unsigned variables for < 0

  uint32 min;

  if (_min->eval(/*d,*/NULL,&min) && min == 0)
    {
      d.text_fmt("%s_BITS_RANGE_MAX(%d,%s,",prefix,_loc._internal,name);
      _max->dump(d);
    }
  else
    {
      d.text_fmt("%s_BITS_RANGE(%d,%s,",prefix,_loc._internal,name);
      _min->dump(d);
      d.text(",");
      _max->dump(d);
    }
  if (jump_target)
    d.text_fmt(",%s",jump_target);
  d.text(");\n");
}

void bits_cond_range::get_match(dumper &d,const arguments *args,
				uint32 *fixed_mask,uint32 *fixed_value) const
{
  //d.text_fmt("RANGE(");
  //_min->dump(d);
  //d.text(",");
  //_max->dump(d);
  //d.text_fmt(")");

  // now, if we manage to evaluate the variable, then we know something

  uint32 min, max;

  if (_min->eval(/*d,*/args,&min) &&
      _max->eval(/*d,*/args,&max))
    {
      // d.text_fmt("[%d,%d]",min,max);

      // ok, so figure out what bits are fixed while taking all values
      // in the range [min,max] ..
      // hmm, actually quite easy, since adding 1 will touch the least
      // significant bit, all bits up to the most significant bit of max
      // will be touched, except for the highest bits where max and min
      // agree

      uint32 non_fixed = max ^ min;

      // for all bits in non_fixed which have a larger bit set to one, they
      // are also non-fixed

      for (size_t i = sizeof(non_fixed) * 8; i; --i)
	non_fixed |= (non_fixed >> 1);

      // so, now we know

      *fixed_mask  = ~non_fixed;
      *fixed_value = max & (~non_fixed);

      // d.text_fmt("->{0x%x}{%d,%d}",non_fixed,*fixed_mask,*fixed_value);
    }
}

void bits_cond_check_count::dump(dumper &d,const variable *name) const
{
  d.text_fmt("CHECK_COUNT(");
  if (name)
    {
      name->dump(d);
      d.text(",");
    }
  d.text_fmt("%s,%s,",_start,_stop);
  _offset->dump(d);
  d.text(",");
  _multiplier->dump(d);
  d.text(")");
}

void bits_cond_check_count::dump(dumper &d) const
{
  dump(d,NULL);
}

void bits_cond_check_count::check(dumper &d,const char *prefix,
				  const char *name,const variable *var,
				  const char *jump_target) const
{
  d.text_fmt("%s_WORD_COUNT(%d,",
	     prefix,_loc._internal);
  if (name)
    d.text_fmt("%s",name);
  else if (var)
    var->dump(d);
  d.text_fmt(",%s,%s,",_start,_stop);
  _offset->dump(d);
  d.text(",");
  _multiplier->dump(d);
  if (jump_target)
    d.text_fmt(",%s",jump_target);
  d.text(");\n");
}

void bits_cond_check_count::check(dumper &d,const char *prefix,const char *name,
				  const char *jump_target) const
{
  check(d,prefix,name,NULL,jump_target);
}

void bits_cond_check_count::get_match(dumper &d,const arguments *args,
				uint32 *fixed_mask,uint32 *fixed_value) const
{
}

void bits_spec::dump(dumper &d) const
{
  char buf[64];

  if (_min == _max)
    sprintf (buf,"   %2d: ",_min);
  else
    sprintf (buf,"%2d_%02d: ",_min,_max);
  d.text(buf);

  if (_name)
    d.text(_name);
  if (_cond)
    {
      if (_name)
	d.text(" = ");
      // Special case to not print CHECK, incase of an constant and no name...
      // URK, is this ugly?!?
      // This is just such that we can still parse waht we generate, since as the parser is right now
      // one must give all items a name for checking, except when checking a constant...
      if (!_name)
	{
	  const bits_cond_check *check = dynamic_cast<const bits_cond_check *>(_cond);
	  if (check)
	    {
	      const var_const *c = dynamic_cast<const var_const *>(check->_check);
	      if (c)
		{
		  c->dump(d);
		  goto dumped_cond;
		}
	    }
	}
      _cond->dump(d);
    dumped_cond:
      ;
    }
  d.text(";");
}

bool bits_spec_has_ident(const bits_spec_list *list,const char *name)
{
  bits_spec_list::const_iterator i;
  for (i = list->begin(); i != list->end(); ++i)
    {
      bits_spec *b = *i;

      // fprintf(stderr,"{%s--%s}",b->_name,name);

      if (b->_name == name)
	return true;
    }
  return false;
}

const char *prefix_ident::get_prefix(const char *name) const
{
  if (bits_spec_has_ident(_list,name))
    return _prefix;

  return "";
}
