// -*- C++ -*-
// vi: filetype=cpp
#include "trloii_mb.spec"

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

SUBEVENT(vulom) {
  select optional {
    wr_ts = WHITE_RABBIT();
  }

  select optional {
    tpat_blt = TPAT_BLT();
  }

  trloii_trig_mux = TRLOII_TRIG_MUX();

  select optional {
    mid = MID_BARRIER();
  }

  select optional {
    trloii_src_scalers = TRLOII_SRC_SCALERS();
  }

  select optional {
    end = END_TAG();
  }
}

EVENT
{
  trloii_mvlc = vulom(procid=15, control=30);
  ignore_unknown_subevent;
}

