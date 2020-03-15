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

#include "online_chain.hh"

#if 0
void online_chain::select_files()
{
  // There were two choices, either let this function find the files
  // by traversing a directory structure, or to let it find the files
  // by consulting a file list (current option).

  // Traversing the directory structure would remove the need for a
  // file list, but would also likely be a bit slower when many files
  // come into play (perhaps not so important).

  // It is however very important to create intermediate summary files
  // that span stretches of the original small files, as the
  // performance of TChain for a large number of small files is rather
  // bad.  On a 2 GHz opteron, it seems to have an additional constant
  // of handling of about 35 files/s in addition to the actual
  // processing.  So the additional small program XXX should be run to
  // make combination files then usable by us.  (We will whenever
  // possible select the combined files).  Fortunately, merging files
  // is very easy with the CopyTree function.



}
#endif

TChain *online_chain::since(const char *basename,int seconds)
{

  return NULL;
}

TChain *online_chain::span(const char *basename,
			   const char *from,const char *to)
{






  return NULL;
}
