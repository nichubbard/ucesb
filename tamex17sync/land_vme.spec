// -*- C++ -*-

LAND_STD_VME()
{

  MEMBER(DATA32 error_bits);
 
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

      ENCODE(error_bits, (value=
		fail_general << 0 |
		fail_data_corrupt << 1 |
		fail_data_missing << 2 |
		fail_data_too_much << 3 |
		fail_event_counter_mismatch << 4 |
		fail_readout_error_driver << 5 |
		fail_unexpected_trigger << 6 |
		fail_firmware_mismatch << 7));
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
