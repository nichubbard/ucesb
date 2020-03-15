// -*- C++ -*-

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

LAND_STD_VME()
{
  UINT32 failure
    {
      0:  fail_general;
      1:  fail_data_corrupt;
      2:  fail_data_missing;
      3:  fail_data_too_much;
      4:  fail_event_counter_mismatch;
      5:  fail_readout_error_driver;
      6:  fail_unexpected_trigger;
      7:  fail_firmware_mismatch;

      22: has_multi_trlo_ii_counter0;
      23: has_clock_counter_stamp;
      24: has_continous_event_counter;
      25: has_update_qdc_iped_value;
      26: spurious_start_before_tcal;
      27: has_no_zero_suppression;
      28: has_multi_adctdc_counter0;
      29: has_multi_scaler_counter0;
      30: has_multi_event;
      31: has_time_stamp;
    }

  if (failure.has_continous_event_counter) {
    UINT32 continous_event_counter;
  }

  if (failure.has_time_stamp) {
    UINT32 time_stamp;
  }

  if (failure.has_clock_counter_stamp) {
    UINT32 clock_counter_stamp;
  }

  if (failure.has_update_qdc_iped_value) {
    UINT32 iped;
  }

  if (failure.has_multi_event) {
    UINT32 multi_events;
  }

  if (failure.has_multi_trlo_ii_counter0) {
    UINT32 multi_trlo_ii_counter0;
  }

  if (failure.has_multi_scaler_counter0) {
    UINT32 multi_scaler_counter0;
  }

  if (failure.has_multi_adctdc_counter0) {
    UINT32 multi_adctdc_counter0;
  }

}

