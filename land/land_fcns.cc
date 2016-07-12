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

#include "structures.hh"

#include "land_triggers.hh"

#include "event_base.hh"
#include "worker_thread.hh"

uint32 LAND_CAMAC_SCALER::has_timestamp() const
{
  if (CURRENT_EVENT->_unpack.trigger == LAND_EVENT_SPILL_BEGIN ||
      CURRENT_EVENT->_unpack.trigger == LAND_EVENT_SPILL_END ||
      CURRENT_EVENT->_unpack.trigger == LAND_EVENT_START_ACQ ||
      CURRENT_EVENT->_unpack.trigger == LAND_EVENT_STOP_ACQ)
    return 1;
  return 0;
}
