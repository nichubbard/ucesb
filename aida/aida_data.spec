#define MAX_HITS 10000
#define MEMBERS \
	MEMBER(DATA8  module[MAX_HITS] NO_INDEX_LIST); \
	MEMBER(DATA8  channel[MAX_HITS] NO_INDEX_LIST); \
	MEMBER(DATA8  info[MAX_HITS] NO_INDEX_LIST); \
	MEMBER(DATA16 adc[MAX_HITS] NO_INDEX_LIST); \
	MEMBER(DATA32 ts_high[MAX_HITS] NO_INDEX_LIST); \
	MEMBER(DATA32 ts_med[MAX_HITS] NO_INDEX_LIST); \
	MEMBER(DATA32 ts_low[MAX_HITS] NO_INDEX_LIST);

#define PARAMS_DEF \
	module, \
	channel, \
	info, \
	adc, \
	ts_high, \
	ts_med, \
	ts_low

#define PARAMS \
	module=module, \
	channel=channel, \
	info=info, \
	adc=adc, \
	ts_low=ts_low, \
	ts_med=ts_med, \
	ts_high=ts_high


AIDA()
{
	MEMBERS

	select several {
		mod[0] = AIDA_MODULE(module_id=0, PARAMS);
		mod[1] = AIDA_MODULE(module_id=1, PARAMS);
		mod[2] = AIDA_MODULE(module_id=2, PARAMS);
		mod[3] = AIDA_MODULE(module_id=3, PARAMS);
		mod[4] = AIDA_MODULE(module_id=4, PARAMS);
		mod[5] = AIDA_MODULE(module_id=5, PARAMS);
		mod[6] = AIDA_MODULE(module_id=6, PARAMS);
		mod[7] = AIDA_MODULE(module_id=7, PARAMS);
		mod[8] = AIDA_MODULE(module_id=8, PARAMS);
		mod[9] = AIDA_MODULE(module_id=9, PARAMS);
		mod[10] = AIDA_MODULE(module_id=10, PARAMS);
		mod[11] = AIDA_MODULE(module_id=11, PARAMS);
		mod[12] = AIDA_MODULE(module_id=12, PARAMS);
		mod[13] = AIDA_MODULE(module_id=13, PARAMS);
		mod[14] = AIDA_MODULE(module_id=14, PARAMS);
		mod[15] = AIDA_MODULE(module_id=15, PARAMS);
		mod[16] = AIDA_MODULE(module_id=16, PARAMS);
		mod[17] = AIDA_MODULE(module_id=17, PARAMS);
		mod[18] = AIDA_MODULE(module_id=18, PARAMS);
		mod[19] = AIDA_MODULE(module_id=19, PARAMS);
	}
}


AIDA_MODULE(module_id, PARAMS_DEF)
{
	MEMBERS

	select several {
		i = AIDA_INFO(module_id=module_id, PARAMS);
		d = AIDA_ADC(module_id=module_id, PARAMS);
	}
}


AIDA_INFO(module_id, PARAMS_DEF)
{
	MEMBERS

	UINT32 wordA NOENCODE {
		0_19: info_field;
		20_23: info_code = RANGE(2,15);
		24_29: module_id = MATCH(module_id);
		30_31: 0x2;
	}

	UINT32 wordB NOENCODE {
		0_27: info_time;
		28_31: 0;
	}

	ENCODE(module APPEND_LIST, (value=static_cast<uint8_t>(module_id)));
	ENCODE(info APPEND_LIST, (value=wordA.info_code));
	ENCODE(ts_low APPEND_LIST, (value=wordB.info_time));

	/* Encode info data with channel == 255 (not used in readout) */

	ENCODE(channel APPEND_LIST, (value=255));

	/* Only ts_med transmitted */
	if (wordA.info_code == 2 || wordA.info_code == 4
	    || wordA.info_code == 7) {
		ENCODE(ts_med APPEND_LIST, (value=wordA.info_field));
		ENCODE(ts_high APPEND_LIST, (value=0));
		ENCODE(adc APPEND_LIST, (value=0));
	}
	/* Only ts_high transmitted */
	if (wordA.info_code == 5 || wordA.info_code == 8) {
		ENCODE(ts_high APPEND_LIST, (value=wordA.info_field));
		ENCODE(ts_med APPEND_LIST, (value=0));
		ENCODE(adc APPEND_LIST, (value=0));
	}
	/* AIDA FEE64 discriminator data */
	if (wordA.info_code == 6) {
		ENCODE(adc APPEND_LIST, (value=0));
		ENCODE(ts_med APPEND_LIST, (value=wordA.info_field));
		ENCODE(ts_high APPEND_LIST, (value=0));
	}
#if 0 // Not used for AIDA (only R3B ??)
	/* Only ts_med and ts_low from external source transmitted */
	if (wordA.info_code == 14) {
		ENCODE(ts_low APPEND_LIST, (value=0));
		ENCODE(ts_med APPEND_LIST, (value=wordA.info_field));
		ENCODE(ts_low APPEND_LIST, (value=wordB.info_time));
	}
	/* Only ts_high from external source transmitted */
	if (wordA.info_code == 15) {
		ENCODE(ts_low APPEND_LIST, (value=0));
		ENCODE(ts_med APPEND_LIST, (value=0));
		ENCODE(ts_high APPEND_LIST, (value=wordA.info_field));
	}
#endif
}


AIDA_ADC(module_id, PARAMS_DEF)
{
	MEMBERS

	UINT32 wordA NOENCODE {
		0_15: adc_value;
		16_21: channel_id = RANGE(0, 63);
		22_27: module_id = MATCH(module_id);
		28: adc_range;
		29: 0;
		30_31: 0x3;
	}

	UINT32 wordB NOENCODE {
		0_27: ts;
		28_31: 0;
	}

	ENCODE(module APPEND_LIST, (value = static_cast<uint8_t>(module_id)));
	ENCODE(adc APPEND_LIST, (value = wordA.adc_value));
	ENCODE(channel APPEND_LIST, (value = wordA.channel_id));
	ENCODE(ts_low APPEND_LIST, (value = wordB.ts));
	ENCODE(ts_med APPEND_LIST, (value = 0));
	ENCODE(ts_high APPEND_LIST, (value = 0));

	/* Info code is set to 255 for normal ADC values */

	ENCODE(info APPEND_LIST, (value = 255));
}

