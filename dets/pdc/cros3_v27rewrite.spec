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

// (ucesb) specification for a rewritten event from cros3

// There is no longer any headers from each ad16 board, and some of the
// data from the headers have been omitted (the thresholds - as they are
// static information).

// There are three data formats: threshold curves, leading edge packed,
// and time-over-threshold.  The both original raw data formats are
// compressed into each of the packed ones.


CROS3_REWRITE(ccb_id)
{
  MEMBER(WIRE_START_END data[0x8000/*256*128*/] ZERO_SUPPRESS);

  UINT32 h1
    {
      0_15:  data_size; // number of 32-bit data words following

      16:    threshold_curve;
      17:    leading_edge;
      18:    data;

      20_23: trigger_time;
      24_27: ccb_id = MATCH(ccb_id);
      28_31: event_counter;
    };

  if (h1.data)
    {
      UINT32 h2
	{
	  0_1:   read_out_mode;  // (CSR_TRC  1:0)
	  4:     pulser_enabled; // (CSR_TRC    4)
	  8_10:  statistics;     // (CSR_TRC 10:8) (don't-care for data)
	  11:    both_edges;     // (CSR_TRC   11)

	  12_19: slices;         // (CSR_DRC  7:0)
	  20_21: scale;          // (CSR_DRC  9:8)

	  31:    odd_length16;   // 16-bit data has odd length, (last entry 'missing')
	};

      if (h1.leading_edge)
	{
	  // First loop over the fully used 32-bit data words

	  list(0<=index<static_cast<uint32>(h1.data_size-h2.odd_length16))
	    {
	      UINT32 ch_data NOENCODE
		{
		  0_7:   start_slice1;
		  8_15:  wire1;
		  16_23: start_slice2;
		  24_31: wire2;

		  ENCODE(data[index*2  ],(wire=wire1,start=start_slice1,stop=0));
		  ENCODE(data[index*2+1],(wire=wire2,start=start_slice2,stop=0));
		}
	    }

	  // Then, if there is one more entry, take that also

	  if (h2.odd_length16)
	    {
	      UINT32 ch_data_odd NOENCODE
		{
		  0_7:   start_slice1;
		  8_15:  wire1;
		  16_23: start_slice2 = 0xff; // high bits are dummy filled
		  24_31: wire2        = 0x00; // high bits are dummy filled

		  ENCODE(data[(h1.data_size-1)*2],(wire=wire1,start=start_slice1,stop=0));
		}
	    }
	}
      else
	{
	  list(0<=index<h1.data_size)
	    {
	      UINT32 ch_data NOENCODE
		{
		  0_7:   start_slice;
		  8_15:  wire;
		  16_23: end_slice;
		  // 8 bits left-over

		  ENCODE(data[index],(wire=wire,start=start_slice,stop=end_slice));
		}
	    }
	}
    }
  else
    {
      if (h1.threshold_curve)
	{
	  UINT32 trc_h2
	    {
	      0_1:   read_out_mode;  // (CSR_TRC  1:0)
	      4:     pulser_enabled; // (CSR_TRC    4)
	      8_10:  statistics;     // (CSR_TRC 10:8)
	      11:    both_edges;     // (CSR_TRC   11)

	      12_19: threshold_start;
	      20_23: threshold_step;
	      24_28: boards;
	    };

	  list(0<=board<trc_h2.boards)
	    {
	      UINT32 trc_h3 NOENCODE
		{
		  0_7:   test_pulser_even;
		  8_15:  test_pulser_odd;

		  16_23: threshold_steps;

		  28_31: ad_id;
		};

	      list(0<=index_thr<trc_h3.threshold_steps)
		{
		  list(0<=index_wire2<8)
		    {
		      UINT32 ch_counts NOENCODE
			{
			  0_11:   counts1;
			  12_15:  wire1;
			  16_27:  counts2;
			  28_31:  wire2;
			}
		    }
		}
	    }
	}
      else
	{
	  UINT32 dummy_h2 NOENCODE
	    {
	      0_31: 0x0;
 	    };
	}
    }
}



