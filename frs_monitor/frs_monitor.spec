// -*- C++ -*-
// vi: filetype=cpp
#include "trloii_mb.spec"

// We're not a complete unpacker :)
DUMMY()
{
  UINT32 no NOENCODE;
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
    8_11: type;
    12_15: geo;
  }
  UINT16 header2 NOENCODE;

  list (0 <= i < header.nlw) {
    //UINT32 scaler NOENCODE {
      //0_25: value;
      //26: 0;
      //27_31: cid = MATCH(i);
      //ENCODE(scalers[i], (value=value));
    //}
    UINT32 scaler NOENCODE {
      0_31: value;
      ENCODE(scalers[i], (value=value));
    }
  }

  UINT32 trailer NOENCODE;
}

SUBEVENT(vulom) {
  wr_ts = WHITE_RABBIT();

  select optional {
    tpat_blt = TPAT_BLT();
  }

  trloii_trig_mux = TRLOII_TRIG_MUX();
  select optional {
    trloii_timing = TRLOII_TIMING();
  }

  select optional {
    trloii_src_scalers = TRLOII_SRC_SCALERS();
  }
}

SUBEVENT(frs_main_subev)
{
  vulom_scaler_marker = DUMMY();
  scaler = FRS_MVLC_SCALER();
  select several
  {
    dummy = DUMMY();
  }
}

SUBEVENT(frs_frs_subev)
{
  vulom_scaler_marker = DUMMY();
  scaler = FRS_MVLC_SCALER();
  select several
  {
    dummy = DUMMY();
  }
}

EVENT
{
  trloii_mvlc = vulom(procid=15, control=30);
  frs_frs = frs_frs_subev(type=10, subtype=1, procid=30);
  frs_main = frs_main_subev(type=10, subtype=1, procid=10);
  ignore_unknown_subevent;
}

