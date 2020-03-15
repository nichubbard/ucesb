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

#include "spec/spec.spec"

#include "xtst_vme.spec"

#define XTST_SUBEVENT_MEMBER_ZZP(type)                                     \
XTST_SUBEVENT_MEMBER_##type()                                              \
{                                                                          \
  MEMBER(type m##type       );                                             \
                                                                           \
  MEMBER(type m##type##_array[8]       );                                  \
  MEMBER(type m##type##_array_zzp[8]   ZERO_SUPPRESS);                     \
  MEMBER(type m##type##_array_zzpii[8] ZERO_SUPPRESS_LIST);                \
  MEMBER(type m##type##_array_zzpml[8] ZERO_SUPPRESS_MULTI(4));            \
                                                                           \
  MEMBER(type m##type##_array2[2][8]       );                              \
  MEMBER(type m##type##_array2_zzp[2][8]   ZERO_SUPPRESS);                 \
  MEMBER(type m##type##_array2_zzpii[2][8] ZERO_SUPPRESS_LIST);            \
  MEMBER(type m##type##_array2_zzpml[2][8] ZERO_SUPPRESS_MULTI(4));        \
                                                                           \
  /* We do not handle three levels of arrays! */                           \
  /*MEMBER(type m##type##_array3[2][4][8]       ZERO_SUPPRESS);*/          \
  /*MEMBER(type m##type##_array3_zzp[2][4][8]   ZERO_SUPPRESS);*/          \
  /*MEMBER(type m##type##_array3_zzpii[2][4][8] ZERO_SUPPRESS_LIST);*/     \
  /*MEMBER(type m##type##_array3_zzpml[2][4][8] ZERO_SUPPRESS_MULTI(4));*/ \
}

#define XTST_SUBEVENT_MEMBER_ZZP_B(type)				   \
XTST_SUBEVENT_MEMBER_##type##_B()                                          \
{                                                                          \
  MEMBER(type m##type##_array_zzpml[8] ZERO_SUPPRESS_MULTI(4));            \
}

#define XTST_SUBEVENT_MEMBER(type)                                         \
XTST_SUBEVENT_MEMBER_##type()                                              \
{                                                                          \
  MEMBER(type m##type       );                                             \
                                                                           \
  /* Cannot handle any kind of array for raw types, like uint32 */         \
  /*MEMBER(type m##type##_array[8]       );*/                              \
  /*MEMBER(type m##type##_array_zzp[8]   ZERO_SUPPRESS);*/                 \
  /*MEMBER(type m##type##_array_zzpii[8] ZERO_SUPPRESS_LIST);*/            \
  /*MEMBER(type m##type##_array_zzpml[8] ZERO_SUPPRESS_MULTI(4));*/        \
                                                                           \
  /*MEMBER(type m##type##_array2[2][8]       );*/                          \
  /*MEMBER(type m##type##_array2_zzp[2][8]   ZERO_SUPPRESS);*/             \
  /*MEMBER(type m##type##_array2_zzpii[2][8] ZERO_SUPPRESS_LIST);*/        \
  /*MEMBER(type m##type##_array2_zzpml[2][8] ZERO_SUPPRESS_MULTI(4));*/    \
                                                                           \
  /* We do not handle three levels of arrays! */                           \
  /*MEMBER(type m##type##_array3[2][4][8]       ZERO_SUPPRESS);*/          \
  /*MEMBER(type m##type##_array3_zzp[2][4][8]   ZERO_SUPPRESS);*/          \
  /*MEMBER(type m##type##_array3_zzpii[2][4][8] ZERO_SUPPRESS_LIST);*/     \
  /*MEMBER(type m##type##_array3_zzpml[2][4][8] ZERO_SUPPRESS_MULTI(4));*/ \
}

XTST_SUBEVENT_MEMBER_ZZP(DATA32)
XTST_SUBEVENT_MEMBER_ZZP(DATA24)
XTST_SUBEVENT_MEMBER_ZZP(DATA16)
XTST_SUBEVENT_MEMBER_ZZP(DATA16_OVERFLOW)
XTST_SUBEVENT_MEMBER_ZZP(DATA12)
XTST_SUBEVENT_MEMBER_ZZP(DATA12_OVERFLOW)
XTST_SUBEVENT_MEMBER_ZZP(DATA12_RANGE)
XTST_SUBEVENT_MEMBER_ZZP(DATA8)

XTST_SUBEVENT_MEMBER_ZZP_B(DATA32)

XTST_SUBEVENT_MEMBER(uint8)
XTST_SUBEVENT_MEMBER(uint16)
XTST_SUBEVENT_MEMBER(uint32)

SUBEVENT(XTST_SUBEVENT_MEMBER_ENCODE)
{
  member_DATA32 = XTST_SUBEVENT_MEMBER_DATA32();
  member_DATA24 = XTST_SUBEVENT_MEMBER_DATA24();
  member_DATA16 = XTST_SUBEVENT_MEMBER_DATA16();
  member_DATA16_OVERFLOW = XTST_SUBEVENT_MEMBER_DATA16_OVERFLOW();
  member_DATA12 = XTST_SUBEVENT_MEMBER_DATA12();
  member_DATA12_OVERFLOW = XTST_SUBEVENT_MEMBER_DATA12_OVERFLOW();
  member_DATA12_RANGE = XTST_SUBEVENT_MEMBER_DATA12_RANGE();

  member_uint32 = XTST_SUBEVENT_MEMBER_uint32();
  member_uint16 = XTST_SUBEVENT_MEMBER_uint16();
  member_uint8 = XTST_SUBEVENT_MEMBER_uint8();


}

SUBEVENT(XTST_SUBEVENT_MEMBER_ENCODE_A)
{
  d32 = XTST_SUBEVENT_MEMBER_DATA32();
}

SUBEVENT(XTST_SUBEVENT_MEMBER_ENCODE_B)
{
  d32 = XTST_SUBEVENT_MEMBER_DATA32_B();
}

ITEM64(num)
{
  MEMBER(DATA64 member64);

  UINT64 a
    {
    5_7: num = MATCH(num);
    0_1: 0;
    }

  UINT64 b
    {
    20_63: 0x123456;
    0_1  : 0;
    3_7  : v;

      ENCODE(member64,(value = v));
    }

  UINT32 c
    {
    20_31: 0x123;
    0_1  : 0;
    3_7  : v;

      ENCODE(member64,(value = v));
    }


}

ITEMQ(num)
{
  MEMBER(DATA32 chan);

  UINT64 w1
    {
    21: c21;
    }

  UINT64 w2
    {
    22: c22;
    }

  ENCODE(chan,(value=( ((w1.c21<<12)&0xF00) | (w2.c22) )));
}

SUBEVENT(BITS64)
{
  MEMBER(DATA64 member64);
  MEMBER(DATA64 array64[7]);

  MEMBER(DATA32 member32);

  UINT32 data32_1;
  UINT32 data32_2;

  MARK_COUNT(start);

  UINT64 data64_1;

  MARK_COUNT(end);
  CHECK_COUNT(data32_1,start,end,4,4);

  select several
  {
    i[0] = ITEM64(num=0);
    i[1] = ITEM64(num=1);
    i[2] = ITEM64(num=2);
  };
}

ITEM_ME2(geom)
{
  MEMBER(DATA16 data[32] ZERO_SUPPRESS);

  optional UINT32 padding NOENCODE
  {
    0_31: value = MATCH(0x32323232);
  }

  UINT32 header NOENCODE {
    16_23: geom = MATCH(geom);
  }

  MATCH_END;

  UINT32 data NOENCODE {
    16_23: geom = MATCH(geom);
  }
}

SUBEVENT(XTST_SUBEVENT_ME2)
{

  select several
  {
    i[0] = ITEM_ME2(geom=0);
    i[1] = ITEM_ME2(geom=1);
    i[2] = ITEM_ME2(geom=2);
  };
}

SUBEVENT(XTST_PLAIN)
{
  UINT32 u;
  UINT16 s;
  UINT8  c;

  // DATA32 d;
}

STICKY_ACTIVE()
{
  UINT32 separator { /* We encode this, tells if event is here. */
    0_31: 7;
  }

  UINT32 active;
  UINT32 mark;

  UINT32 corr;
}

WR_STAMP(id)
{
  UINT32 header NOENCODE
  {
    8_11: id = MATCH(id);
    16:   error;
  }
  UINT32 data0 NOENCODE
  {
    0_15: v;
    16_31: 0x03e1;
  };
  UINT32 data1 NOENCODE
  {
    0_15: v;
    16_31: 0x04e1;
  };
  UINT32 data2 NOENCODE
  {
    0_15: v;
    16_31: 0x05e1;
  };
  UINT32 data3 NOENCODE
  {
    0_15: v;
    16_31: 0x06e1;
  };
}

SUBEVENT(XTST_REGRESS)
{
  select several
  {
    wr[1] = WR_STAMP(id=1);
    wr[2] = WR_STAMP(id=2);
    wr[3] = WR_STAMP(id=3);
    wr[4] = WR_STAMP(id=4);
    wr[5] = WR_STAMP(id=5);
  }

  UINT32 separator1 NOENCODE
  {
    0_31: 0;
  }

  UINT32 seed;

  select several
  {
    v775mod[0] = VME_CAEN_V775(geom=1,crate=0x7f);
    v775mod[1] = VME_CAEN_V775(geom=2,crate=0x7e);
  }

  UINT32 separator2 NOENCODE
  {
    0_31: 0;
  }

  select several
  {
    v1290mod[0] = VME_CAEN_V1290_SHORT(geom=1);
    v1290mod[1] = VME_CAEN_V1290_SHORT(geom=2);
  }

  select several
  {
    sticky_active = STICKY_ACTIVE();
  }
}

SUBEVENT(XTST_REGRESSEXTRA)
{
  /* This subevent is not in the stream.  Only here to produce
   * extra data for the struct_writer.
   */
  UINT32 junk;
}

EVENT
{
  vme = XTST_VME(type=36,subtype=3100);

  bits64 = BITS64(type=64,subtype=0x40);

  me = XTST_SUBEVENT_MEMBER_ENCODE(type=10);

  mea = XTST_SUBEVENT_MEMBER_ENCODE_A(type=12);

  meb = XTST_SUBEVENT_MEMBER_ENCODE_B(type=13);

  me2 = XTST_SUBEVENT_ME2(type=11);

  plain = XTST_PLAIN(type=12);

  regress[ 0] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 0);
  regress[ 1] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 1);
  regress[ 2] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 2);
  regress[ 3] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 3);
  regress[ 4] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 4);
  regress[ 5] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 5);
  regress[ 6] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 6);
  regress[ 7] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 7);
  regress[ 8] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 8);
  regress[ 9] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate= 9);
  regress[10] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate=10);
  regress[11] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate=11);
  regress[12] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate=12);
  regress[13] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate=13);
  regress[14] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate=14);
  regress[15] = XTST_REGRESS(type=0x0cae,subtype=0x0cae,subcrate=15);

  regressextra = XTST_REGRESSEXTRA(type=0x0de0,subtype=0x0ad0);
}

SIGNAL(POS1_1_T,vme.tdc0.data[0],DATA12);
SIGNAL(POS1_2_T,vme.tdc0.data[1],DATA12);
SIGNAL(POS1_3_T,vme.tdc0.data[2],DATA12);
SIGNAL(POS1_4_T,vme.tdc0.data[3],DATA12);
SIGNAL(POS2_1_T,vme.tdc0.data[4],DATA12);
SIGNAL(POS2_2_T,vme.tdc0.data[5],DATA12);
SIGNAL(POS2_3_T,vme.tdc0.data[6],DATA12);
SIGNAL(POS2_4_T,vme.tdc0.data[7],DATA12);
SIGNAL(POS2_1_E,vme.qdc0.data[4],DATA12);
SIGNAL(POS2_2_E,vme.qdc0.data[5],DATA12);
SIGNAL(POS2_3_E,vme.qdc0.data[6],DATA12);
SIGNAL(POS2_4_E,vme.qdc0.data[7],(DATA12,float));
SIGNAL(PIN1_E  ,vme.qdc0.data[8],DATA12);
SIGNAL(PIN2_E  ,vme.qdc0.data[9],(DATA12,float));
SIGNAL(GFI1_U_1_E,vme.qdc0.data[10],DATA12);
SIGNAL(GFI1_U_2_E,vme.qdc0.data[11],DATA12);
SIGNAL(GFI1_U_3_E,vme.qdc0.data[12],DATA12);
SIGNAL(GFI1_U_4_E,vme.qdc0.data[13],DATA12);
SIGNAL(GFI1_U_6_E,vme.qdc0.data[14],DATA12);
SIGNAL(GFI1_U_5_E,vme.qdc0.data[15],DATA12);
SIGNAL(GFI1_PM_T,vme.qdc0.data[16],DATA12);
SIGNAL(GFI1_PM_E,vme.qdc0.data[21],DATA12);
//SIGNAL(GFI1_PM_E,vme.qdc0.data[16],DATA12); // collision, found by ucesb...
SIGNAL(N1_1_1_T,vme.qdc0.data[17],DATA12);
SIGNAL(N1_1_2_T,vme.qdc0.data[18],DATA12);
SIGNAL(N1_1_1_E,vme.qdc0.data[22],DATA12);
SIGNAL(N1_2_1_T,vme.qdc0.data[19],DATA12);
SIGNAL(N2_2_1_T,vme.qdc0.data[20],DATA12);
SIGNAL(N10_20_2_T,,DATA12);
SIGNAL(N10_20_2_E,,/*(*/DATA12/*,float)*/);
SIGNAL(GFI1_U18E,,DATA12);
SIGNAL(GFI1_V16E,,DATA12);
SIGNAL(SST1_X640,,DATA12);
SIGNAL(SST1_Y384,,DATA12);

SIGNAL(TOGGLE 2: POS3_1_E,vme.qdc1.data[17],DATA12);
SIGNAL(TOGGLE 1: POS3_1_E,vme.qdc1.data[1],DATA12);

SIGNAL(TOGGLE 2: POS3_2_E,vme.qdc1.data[0],DATA12);

SIGNAL(TOGGLE 1:
       POS3_3_E,vme.qdc1.data[3],
       POS3_4_E,vme.qdc1.data[4],DATA12);
SIGNAL(TOGGLE 2:
       POS3_3_T,vme.tdc2.data[3],
       POS3_4_T,vme.tdc2.data[4],DATA12);
SIGNAL(TOGGLE 1:
       POS3_3_T,vme.tdc2.data[3+16],
       POS3_4_T,vme.tdc2.data[4+16],DATA12);

SIGNAL(N10_20_1_T,,DATA12 "ch");

SIGNAL(N10_20_1_T,,DATA12 "ch");

SIGNAL(N10_19_2_T,,(DATA12 "ch", float "#ns"));

SIGNAL(ZERO_SUPPRESS: N10_20);

SIGNAL(TGL1_1_E_1,vme.qdc1.data[10],DATA12);
SIGNAL(ZERO_SUPPRESS: TGL1_1_E_3);
SIGNAL(TOGGLE 2: TGL1_1_T_1,vme.tdc2.data[10],DATA12);
SIGNAL(ZERO_SUPPRESS: TGL1_1_T_3);
SIGNAL(TOGGLE 2: TGL1_1_F_1,vme.tdc2.data[11],(DATA12,float));
SIGNAL(ZERO_SUPPRESS: TGL1_1_F_3);

SIGNAL(MAPA1_1,vme.tdc1.data[0],
       MAPA1_3,vme.tdc1.data[2],
       DATA12);

SIGNAL(MAPB1_1e,vme.tdc1.data[3],
       MAPB1_3e,vme.tdc1.data[5],
       DATA12);

SIGNAL(MAPC1_1e,vme.tdc1.data[6],
       MAPC3_1e,vme.tdc1.data[8],
       DATA12);

SIGNAL(MAPD1_4,vme.tdc1.data[9],
       MAPD1_4,vme.tdc1.data[9],
       DATA12);

SIGNAL(MAPS1,vme.sdm.data,
       MAPS1,vme.sdm.data,
       UINT32);

SIGNAL(plain_u, plain.u, UINT32); // UINT32 maps to uint32
SIGNAL(plain_s, plain.s, uint16);
SIGNAL(plain_c, plain.c, uint8);

// SIGNAL(EVENTNO, EVENTNO, UINT32);

MADC32(res,geo,channels)
{
  UINT32 header
  {
    0_11:  word_counts; // chn + eob
    12_14: resolution = MATCH(res);
    15:    unused1;
    16_23: id = MATCH(geo);
    24_29: 0b000000;
    30_31: 0b01;
  }

  if (header.resolution==0)
    {
      UINT32 madc32_data0{
      0_10:  adc_value;  // should be in the output structure (UINT16)
      11_13: 0b000;
      14:    unused;
      15:    0b0;
      16_20: channel = RANGE(0,channels);
      21_29: 0b000100000;
      30_31: 0b00;
      }
    }
  else if((header.resolution==1)||(header.resolution==2))
    {
      UINT32 madc32_data1{
      0_11:  adc_value;  // should be in the output structure (UINT16)
      12_13: 0b00;
      14:    unused;
      15:    0b0;
      16_20: channel = RANGE(0,channels);
      21_29: 0b000100000;
      30_31: 0b00;
      }
    }
  else if((header.resolution==3)||(header.resolution==4))
    {
      UINT32 madc32_data3{
      0_12: adc_value;   // should be in the output structure (UINT16)
      13:   0b0;
      14:    unused;
      15:    0b0;
      16_20: channel = RANGE(0,channels);
      21_29: 0b000100000;
      30_31: 0b00;
      }
    }
  UINT32 trailer
  {
    0_29:  evt_counter;
    30_31: 0b11;
  }
}




/*
NT_SUBDEVICE(id, n)
{
    UINT32 header
    {
        0_7: rid = MATCH(id);
        8_15: rn = MATCH(n);
    }

    MATCH_END; // Placing this here has no effect
}

NT_DEVICE(id)
{
  //header = SOME_HEADER()

    select several
    {
        subdevice[0] = NT_SUBDEVICE(id = id, n = 0);
        subdevice[1] = NT_SUBDEVICE(id = id, n = 1);
    }
}

SUBEVENT(NT_SUBEV)
{
    select several
    {
      device[0] = NT_DEVICE(id = 0);
      device[1] = NT_DEVICE(id = 1);
    }
}

SIGNAL(ZERO_SUPPRESS_MULTI(200): NNva_P1_B1_tc_T1);
SIGNAL(NNva_P60_B50_tc_T2, ,DATA12);

SIGNAL(ZERO_SUPPRESS_MULTI(200): NNvb_P1_B1_T1_tc);
SIGNAL(ZERO_SUPPRESS_MULTI(200): NNvb_P1_B1_T1_tf);
SIGNAL(NNvb_P60_B50_T2_tc, ,DATA12);
SIGNAL(NNvb_P60_B50_T2_tf, ,DATA12);
*/


SIGNAL(ZZP1_1_E_1,vme.qdc1.data[2],DATA12);
SIGNAL(ZERO_SUPPRESS: ZZP1_1_E_10);
SIGNAL(ZZP1_1_F_1,vme.tdc2.data[5],(DATA12,float));
SIGNAL(ZERO_SUPPRESS: ZZP1_1_F_10);
SIGNAL(ZZP1_1_D_1,vme.tdc2.data[6],(DATA12,double));
SIGNAL(ZERO_SUPPRESS: ZZP1_1_D_10);
SIGNAL(ZZP1_1_U_1,vme.tdc2.data[7],(DATA12,uint32));
SIGNAL(ZERO_SUPPRESS: ZZP1_1_U_10);
SIGNAL(ZZP1_1_L_1,vme.tdc2.data[8],(DATA12,uint64));
SIGNAL(ZERO_SUPPRESS: ZZP1_1_L_10);

SIGNAL(STCORR,regress[0].sticky_active.corr,uint32);

SUBEVENT(XTST_STICKY_CORR)
{
  UINT32 base;
}

STICKY_EVENT
{
  corr = XTST_STICKY_CORR(type=0x0cae,subtype=0x0caf);

  ignore_unknown_subevent;
}

STICKY(corrbase, corr.base, UINT32);
