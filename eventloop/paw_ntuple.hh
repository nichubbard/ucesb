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

#ifndef __PAW_NTUPLE_HH__
#define __PAW_NTUPLE_HH__

#if defined(USE_CERNLIB) || defined(USE_ROOT) || defined(USE_EXT_WRITER)

class staged_ntuple;
class select_event;
class lmd_event_out;

#define PAW_NTUPLE_NORMAL_EVENT  0
#define PAW_NTUPLE_STICKY_EVENT  1
#define PAW_NTUPLE_NUM           2

class paw_ntuple
{
public:
  paw_ntuple();
  virtual ~paw_ntuple();

public:
  staged_ntuple *_staged[PAW_NTUPLE_NUM];

#if defined(USE_LMD_INPUT)
  // For raw data output:
  select_event *_raw_select;
  // The accumulated raw output data
  lmd_event_out *_raw_event;
#endif

public:
  void open_stage(const char *command,bool reading);
  void event(int kind); // write
  bool get_event(); // read I
  void unpack_event(); // read II
  void close();
};

paw_ntuple *paw_ntuple_open_stage(const char *command,bool reading);

#endif

#endif//__PAW_NTUPLE_HH__
