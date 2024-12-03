TPAT_BLT() {
	UINT32 header NOENCODE {
		0_15: nwords;
		16_31: mvlc = MATCH(0xf500);
	}
	list(0 <= i < header.nwords) {
		UINT32 blabla NOENCODE;
	}
}


TRLOII_TRIG_MUX() {
	MEMBER(DATA32 before_deadtime[16]);
	MEMBER(DATA32 after_deadtime[16]);
	MEMBER(DATA32 after_reduction[16]);

	MEMBER(DATA32 inc_before_deadtime[32] NO_INDEX_LIST);
	MEMBER(DATA32 inc_after_deadtime[32] NO_INDEX_LIST);
	MEMBER(DATA32 inc_after_reduction[32] NO_INDEX_LIST);

	UINT32 header NOENCODE {
		0_7: npairs = MATCH(16);
		8_31: ind = MATCH(0xc0ffee);
	}

	list(0 <= i < header.npairs) {
		UINT32 w1 NOENCODE {
			0_31: data;
			ENCODE(before_deadtime[i], (value = data));
		}
		UINT32 w2 NOENCODE {
			0_31: data;
			ENCODE(after_deadtime[i], (value = data));
		}

		UINT32 w3 NOENCODE {
			0_31: data;
			ENCODE(after_reduction[i], (value = data));
		}
	}
}

MID_BARRIER() {
	UINT32 marker NOENCODE {
		0_31: w = MATCH(0xbeefbabe);
	}
}


#define LIST_SCALER(name, N) \
	MEMBER(DATA32 name[N]); \
	MEMBER(DATA32 inc_##name[32] NO_INDEX_LIST); \
	MEMBER(DATA32 full_##name[32] NO_INDEX_LIST); \
	list(0 <= i < N) { \
		UINT32 w NOENCODE { \
			0_31: data; \
			ENCODE(name[i], (value = data)); \
		} \
	}

TRLOII_SRC_SCALERS() {
	MEMBER(DATA32 time_increment_us); // Handled in user function.

	LIST_SCALER(ecl_in,                 16)
	LIST_SCALER(ecl_io_in,               8)
	LIST_SCALER(lemo_in,                 2)
	LIST_SCALER(wired_zero,              1)
	LIST_SCALER(wired_one,               1)
	LIST_SCALER(prng_poisson,            1)
	LIST_SCALER(pulser,                  4)
	LIST_SCALER(lmu_out,                 8)
	LIST_SCALER(gate_delay,              4)
	LIST_SCALER(edge_gate,               2)
	LIST_SCALER(downscale,               1)
	LIST_SCALER(all_or,                  2)
	LIST_SCALER(coincidence,             1)
	LIST_SCALER(input_coinc,             1)
	LIST_SCALER(multi_latch_alm_full,    4)
	LIST_SCALER(serial_tstamp_out,       1)
	LIST_SCALER(serial_tstamp_alm_full,  1)
	LIST_SCALER(serial_tstamp_desync,    1)
	LIST_SCALER(serial_signals_out,      3)
	LIST_SCALER(heimtime_out,            1)
	LIST_SCALER(accept_trig,            16)
	LIST_SCALER(encoded_trig,            4)
	LIST_SCALER(master_start,            1)
	LIST_SCALER(master_toggle,           2)
	LIST_SCALER(multi_scaler,            1)
	LIST_SCALER(deadtime,                1)
	LIST_SCALER(accept_pulse,            1)
	LIST_SCALER(trig_lmu_out_enabled_or, 1)
	LIST_SCALER(multi_trig_buf_alm_full, 1)
	LIST_SCALER(multi_scaler_alm_full,   1)
	LIST_SCALER(trimi_tdt,               1)
}

END_TAG() {
	UINT32 marker NOENCODE {
		0_31: w = MATCH(0xb16b00b5);
	}
}

