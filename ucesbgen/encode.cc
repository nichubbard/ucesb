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

#include "encode.hh"
#include "dump_list.hh"

void encode_spec::dump(dumper &d) const
{
  d.text("ENCODE(");
  _name->dump(d);
  if (_flags & ES_APPEND_LIST)
    d.text(" APPEND_LIST");
  d.text(",");
  dump_list_paren(_args,d,"()");
  d.text(");");
}

void encode_cond::dump(dumper &d) const
{
  bool recursive = true;

  d.text("if(");
  _expr->dump(d);
  d.text(")");
  d.nl();
  if (recursive)
    dump_list_braces(_items,d);
  if (_items_else)
    {
      d.nl();
      d.text("else");
      d.nl();
      if (recursive)
	dump_list_braces(_items_else,d);
    }
}
