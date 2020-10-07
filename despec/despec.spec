// -*- C++ -*-
#include "trloii.spec"

DUMMY()
{
  UINT32 no NOENCODE;
}

// Dummy events or unpack a scalar
FATIMA_DUMMY()
{
  MEMBER(DATA32 scalars[16] ZERO_SUPPRESS_LIST);
  UINT32 no NOENCODE;
  
  if (no == 0x7a001000)
  {
    list(0 <= index < 16)
    {
      UINT32 ch_data NOENCODE;
      ENCODE(scalars[index],(value=ch_data));
    }
  }
}

FRS_FRS_SCALER()
{
  MEMBER(DATA32 scalers[32] ZERO_SUPPRESS_LIST);
  UINT32 no NOENCODE;

  if ((no & 0xFFFF0000) == 0x04800000 || (no & 0xFFFF0000) == 0x44800000)
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

SUBEVENT(WR_BLOCK)
{
  select optional
  {
    wr = WHITE_RABBIT();
  }
  select several
  {
    dummy = FATIMA_DUMMY();
  }
}

SUBEVENT(tpat_subev)
{
  dummy = DUMMY();
  tpat = TRLOII_TPAT(id = 0xcf);
}

SUBEVENT(frs_frs_subev)
{
  select several
  {
	scaler = FRS_FRS_SCALER();
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
  revisit sub = WR_BLOCK(type=10, subtype=1);
  revisit frs_tpat = tpat_subev(type=36, subtype=3600, procid=10);
  revisit frs_frs = frs_frs_subev(type=12, subtype=1, procid=30);
  revisit frs_main = frs_frs_subev(type=12, subtype=1, procid=10);
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
