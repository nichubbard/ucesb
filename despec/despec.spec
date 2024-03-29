// -*- C++ -*-
// vi: filetype=cpp
#include "trloii.spec"

DUMMY()
{
  UINT32 no NOENCODE;
}

// Dummy events or unpack a scalar
FATIMA_SCALER()
{
  MEMBER(DATA32 scalars[16] ZERO_SUPPRESS_LIST);

  UINT32 header NOENCODE
  {
    0_31: h = 0x7a001000;
  }

  list(0 <= index < 16)
  {
    UINT32 ch_data NOENCODE;
    ENCODE(scalars[index],(value=ch_data));
  }

  UINT32 trailer NOENCODE
  {
     0_31: t = 0x7c000000;
  }
}

FRS_MVLC_SCALER()
{
  MEMBER(DATA32 scalers[32] ZERO_SUPPRESS_LIST);
  UINT32 marker NOENCODE {
    0_15 : unk1;
    16_31: 0xf520;
  }
  UINT16 header {
    0_1: 0;
    2_7: nlw;
    8_11: type = MATCH(4);
    12_15: geo;
  }
  UINT16 header2 NOENCODE;

  list (0 <= i < header.nlw) {
    UINT32 scaler NOENCODE {
      0_31: value;
      ENCODE(scalers[i], (value=value));
    }
  }

  UINT32 trailer NOENCODE;
}

FRS_MAIN_SCALER()
{
  MEMBER(DATA32 scalers[32] ZERO_SUPPRESS_LIST);
  UINT32 no NOENCODE;

  if (no == 0xbabababa)
  {
    UINT32 sc NOENCODE;
    if ((sc & 0xFFFF0000) == 0x0c800000)
    {
      list (0 <= index < 32)
      {
	UINT32 ch_data NOENCODE;
	ENCODE(scalers[index], (value=ch_data));
      }
    }
  }
}

FRS_FRS_SCALER()
{
  MEMBER(DATA32 scalers[32] ZERO_SUPPRESS_LIST);
  UINT32 no NOENCODE;

  if ((no & 0xFFFF0000) == 0x04400000)
  {
    list (0 <= index < 32)
    {
      UINT32 ch_data NOENCODE;
      ENCODE(scalers[index], (value=ch_data));
    }
  }
}

WHITE_RABBIT()
{
  MEMBER(DATA32 ts_id[1000] NO_INDEX_LIST);
  MEMBER(DATA32 ts_low[1000] NO_INDEX_LIST);
  MEMBER(DATA32 ts_high[1000] NO_INDEX_LIST);

  UINT32 id NOENCODE;
  ENCODE(ts_id APPEND_LIST, (value=id));

  UINT32 ts1 NOENCODE
  {
    0_15: val;
    16_31: marker = MATCH(0x03e1);
  }
  UINT32 ts2 NOENCODE
  {
    0_15: val;
    16_31: marker = MATCH(0x04e1);
  }
  UINT32 ts3 NOENCODE
  {
    0_15: val;
    16_31: marker = MATCH(0x05e1);
  }
  UINT32 ts4 NOENCODE
  {
    0_15: val;
    16_31: marker = MATCH(0x06e1);
  }

  ENCODE(ts_low APPEND_LIST, (value=(ts2.val << 16 | ts1.val)));
  ENCODE(ts_high APPEND_LIST, (value=(ts4.val << 16 | ts3.val)));
}

SUBEVENT(fatima_vme)
{
  wr = WHITE_RABBIT();
  select optional
  {
    scaler = FATIMA_SCALER();
  }
  select several
  {
    dummy = DUMMY();
  }
}

SUBEVENT(tpat_subev)
{
  dummy = DUMMY();
  tpat = TRLOII_TPAT(id = 0xcf);
}

SUBEVENT(frs_main_subev)
{
  wr = WHITE_RABBIT();
  scaler = FRS_MVLC_SCALER();
  select several
  {
    dummy = DUMMY();
  }
}

SUBEVENT(frs_frs_subev)
{
  scaler = FRS_MVLC_SCALER();
  select several
  {
    dummy = DUMMY();
  }
}

EVENT
{
  //AIDA = WR_BLOCK(procid=90);
  //FATIMA = fatima_event(procid=70);
  //PLASTIC = WR_BLOCK(procid=80);
  //GALILEO = WR_BLOCK(procid=60);
  //FINGER = WR_BLOCK(procid=50);
  //FRS = WR_BLOCK(procid=10);
  revisit fatima = fatima_vme(procid=70, type=10, subtype=1);
  revisit frs_tpat = tpat_subev(type=36, subtype=3600, procid=10);
  revisit frs_frs = frs_frs_subev(type=10, subtype=1, procid=30);
  revisit frs_main = frs_main_subev(type=10, subtype=1, procid=10);
  ignore_unknown_subevent;
}

// Before 'ignore_unknown_subevent' was implemented, all
// this was needed...  (along with the now removed files
// empty_external.hh:  #include "external_data.hh"
// control.hh:         #define USER_EXTERNAL_UNPACK_STRUCT_FILE \
//                     "empty_external.hh"

/*

external EXTERNAL_DATA_SKIP();

SUBEVENT(ANY_SUBEVENT)
{
  // We'll skip the entire subevent...

  external skip = EXTERNAL_DATA_SKIP();
}

EVENT
{
  // handle any subevent
  // allow it several times

  revisit any = ANY_SUBEVENT();
}

*/
