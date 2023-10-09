// -*- C++ -*-

/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "land_vme.spec"
#include "gsi_tamex3.spec"
#include "gsi_tamex3_flexheader.spec"

tamex_subev_fh_data()
{
  select several
  {
    tamex[ 0] = TAMEX3_FH_SFP(sfp=0, card= 0);
    tamex[ 1] = TAMEX3_FH_SFP(sfp=0, card= 1);
    tamex[ 2] = TAMEX3_FH_SFP(sfp=0, card= 2);
    tamex[ 3] = TAMEX3_FH_SFP(sfp=0, card= 3);
    tamex[ 4] = TAMEX3_FH_SFP(sfp=0, card= 4);
    tamex[ 5] = TAMEX3_FH_SFP(sfp=0, card= 5);
    tamex[ 6] = TAMEX3_FH_SFP(sfp=0, card= 6);
    tamex[ 7] = TAMEX3_FH_SFP(sfp=0, card= 7);
    tamex[ 8] = TAMEX3_FH_SFP(sfp=0, card= 8);
    tamex[ 9] = TAMEX3_FH_SFP(sfp=0, card= 9);
    tamex[10] = TAMEX3_FH_SFP(sfp=0, card=10);
    tamex[11] = TAMEX3_FH_SFP(sfp=0, card=11);
    tamex[12] = TAMEX3_FH_SFP(sfp=0, card=12);
    tamex[13] = TAMEX3_FH_SFP(sfp=0, card=13);
    tamex[14] = TAMEX3_FH_SFP(sfp=0, card=14);
    tamex[15] = TAMEX3_FH_SFP(sfp=0, card=15);
    tamex[16] = TAMEX3_FH_SFP(sfp=0, card=16);
    tamex[17] = TAMEX3_FH_SFP(sfp=0, card=17);
    tamex[18] = TAMEX3_FH_SFP(sfp=0, card=18);
    tamex[19] = TAMEX3_FH_SFP(sfp=0, card=19);
    tamex[20] = TAMEX3_FH_SFP(sfp=0, card=20);
    tamex[21] = TAMEX3_FH_SFP(sfp=0, card=21);
    tamex[22] = TAMEX3_FH_SFP(sfp=0, card=22);
    tamex[23] = TAMEX3_FH_SFP(sfp=0, card=23);
    tamex[24] = TAMEX3_FH_SFP(sfp=0, card=24);
    tamex[25] = TAMEX3_FH_SFP(sfp=0, card=25);
    tamex[26] = TAMEX3_FH_SFP(sfp=0, card=26);
    tamex[27] = TAMEX3_FH_SFP(sfp=0, card=27);
    tamex[28] = TAMEX3_FH_SFP(sfp=0, card=28);
    tamex[29] = TAMEX3_FH_SFP(sfp=0, card=29);
    tamex[30] = TAMEX3_FH_SFP(sfp=0, card=30);
    tamex[31] = TAMEX3_FH_SFP(sfp=0, card=31);
    tamex[32] = TAMEX3_FH_SFP(sfp=0, card=32);
    tamex[33] = TAMEX3_FH_SFP(sfp=0, card=33);
    tamex[34] = TAMEX3_FH_SFP(sfp=0, card=34);
    tamex[35] = TAMEX3_FH_SFP(sfp=0, card=35);
    tamex[36] = TAMEX3_FH_SFP(sfp=0, card=36);
    tamex[37] = TAMEX3_FH_SFP(sfp=0, card=37);
    tamex[38] = TAMEX3_FH_SFP(sfp=0, card=38);
    tamex[39] = TAMEX3_FH_SFP(sfp=0, card=39);
    tamex[40] = TAMEX3_FH_SFP(sfp=0, card=40);
    tamex[41] = TAMEX3_FH_SFP(sfp=0, card=41);
    tamex[42] = TAMEX3_FH_SFP(sfp=0, card=42);
    tamex[43] = TAMEX3_FH_SFP(sfp=0, card=43);
    tamex[44] = TAMEX3_FH_SFP(sfp=0, card=44);
    tamex[45] = TAMEX3_FH_SFP(sfp=0, card=45);
    tamex[46] = TAMEX3_FH_SFP(sfp=0, card=46);
    tamex[47] = TAMEX3_FH_SFP(sfp=0, card=47);
    tamex[48] = TAMEX3_FH_SFP(sfp=0, card=48);
    tamex[49] = TAMEX3_FH_SFP(sfp=0, card=49);
  }
}

SUBEVENT(tamex_subev_fh)
{
  land_vme = LAND_STD_VME();
  select several
  {
    data = tamex_subev_fh_data();
  }
}

BARRIER()
{
  UINT32 barrier {
    0_31: 0xbabababa;
  }
}

tamex_subev_data()
{
  select several
  {
    barrier = BARRIER();
    padding = TAMEX3_PADDING();
    tamex[ 0] = TAMEX3_SFP(sfp=0, card= 0);
    tamex[ 1] = TAMEX3_SFP(sfp=0, card= 1);
    tamex[ 2] = TAMEX3_SFP(sfp=0, card= 2);
    tamex[ 3] = TAMEX3_SFP(sfp=0, card= 3);
    tamex[ 4] = TAMEX3_SFP(sfp=0, card= 4);
    tamex[ 5] = TAMEX3_SFP(sfp=0, card= 5);
    tamex[ 6] = TAMEX3_SFP(sfp=0, card= 6);
    tamex[ 7] = TAMEX3_SFP(sfp=0, card= 7);
    tamex[ 8] = TAMEX3_SFP(sfp=0, card= 8);
    tamex[ 9] = TAMEX3_SFP(sfp=0, card= 9);
    tamex[10] = TAMEX3_SFP(sfp=0, card=10);
    tamex[11] = TAMEX3_SFP(sfp=0, card=11);
    tamex[12] = TAMEX3_SFP(sfp=0, card=12);
    tamex[13] = TAMEX3_SFP(sfp=0, card=13);
    tamex[14] = TAMEX3_SFP(sfp=0, card=14);
    tamex[15] = TAMEX3_SFP(sfp=0, card=15);
    tamex[16] = TAMEX3_SFP(sfp=0, card=16);
    tamex[17] = TAMEX3_SFP(sfp=0, card=17);
    tamex[18] = TAMEX3_SFP(sfp=0, card=18);
    tamex[19] = TAMEX3_SFP(sfp=0, card=19);
    tamex[20] = TAMEX3_SFP(sfp=0, card=20);
    tamex[21] = TAMEX3_SFP(sfp=0, card=21);
    tamex[22] = TAMEX3_SFP(sfp=0, card=22);
    tamex[23] = TAMEX3_SFP(sfp=0, card=23);
    tamex[24] = TAMEX3_SFP(sfp=0, card=24);
    tamex[25] = TAMEX3_SFP(sfp=0, card=25);
    tamex[26] = TAMEX3_SFP(sfp=0, card=26);
    tamex[27] = TAMEX3_SFP(sfp=0, card=27);
    tamex[28] = TAMEX3_SFP(sfp=0, card=28);
    tamex[29] = TAMEX3_SFP(sfp=0, card=29);
    tamex[30] = TAMEX3_SFP(sfp=0, card=30);
    tamex[31] = TAMEX3_SFP(sfp=0, card=31);
    tamex[32] = TAMEX3_SFP(sfp=0, card=32);
    tamex[33] = TAMEX3_SFP(sfp=0, card=33);
    tamex[34] = TAMEX3_SFP(sfp=0, card=34);
    tamex[35] = TAMEX3_SFP(sfp=0, card=35);
    tamex[36] = TAMEX3_SFP(sfp=0, card=36);
    tamex[37] = TAMEX3_SFP(sfp=0, card=37);
    tamex[38] = TAMEX3_SFP(sfp=0, card=38);
    tamex[39] = TAMEX3_SFP(sfp=0, card=39);
    tamex[40] = TAMEX3_SFP(sfp=0, card=40);
    tamex[41] = TAMEX3_SFP(sfp=0, card=41);
    tamex[42] = TAMEX3_SFP(sfp=0, card=42);
    tamex[43] = TAMEX3_SFP(sfp=0, card=43);
    tamex[44] = TAMEX3_SFP(sfp=0, card=44);
    tamex[45] = TAMEX3_SFP(sfp=0, card=45);
    tamex[46] = TAMEX3_SFP(sfp=0, card=46);
    tamex[47] = TAMEX3_SFP(sfp=0, card=47);
    tamex[48] = TAMEX3_SFP(sfp=0, card=48);
    tamex[49] = TAMEX3_SFP(sfp=0, card=49);

    tamey[ 0] = TAMEX3_SFP(sfp=1, card= 0);
    tamey[ 1] = TAMEX3_SFP(sfp=1, card= 1);
    tamey[ 2] = TAMEX3_SFP(sfp=1, card= 2);
    tamey[ 3] = TAMEX3_SFP(sfp=1, card= 3);
    tamey[ 4] = TAMEX3_SFP(sfp=1, card= 4);
    tamey[ 5] = TAMEX3_SFP(sfp=1, card= 5);
    tamey[ 6] = TAMEX3_SFP(sfp=1, card= 6);
    tamey[ 7] = TAMEX3_SFP(sfp=1, card= 7);
    tamey[ 8] = TAMEX3_SFP(sfp=1, card= 8);
    tamey[ 9] = TAMEX3_SFP(sfp=1, card= 9);
    tamey[10] = TAMEX3_SFP(sfp=1, card=10);
    tamey[11] = TAMEX3_SFP(sfp=1, card=11);
    tamey[12] = TAMEX3_SFP(sfp=1, card=12);
    tamey[13] = TAMEX3_SFP(sfp=1, card=13);
    tamey[14] = TAMEX3_SFP(sfp=1, card=14);
    tamey[15] = TAMEX3_SFP(sfp=1, card=15);
    tamey[16] = TAMEX3_SFP(sfp=1, card=16);
    tamey[17] = TAMEX3_SFP(sfp=1, card=17);
    tamey[18] = TAMEX3_SFP(sfp=1, card=18);
    tamey[19] = TAMEX3_SFP(sfp=1, card=19);
    tamey[20] = TAMEX3_SFP(sfp=1, card=20);
    tamey[21] = TAMEX3_SFP(sfp=1, card=21);
    tamey[22] = TAMEX3_SFP(sfp=1, card=22);
    tamey[23] = TAMEX3_SFP(sfp=1, card=23);
    tamey[24] = TAMEX3_SFP(sfp=1, card=24);
    tamey[25] = TAMEX3_SFP(sfp=1, card=25);
    tamey[26] = TAMEX3_SFP(sfp=1, card=26);
    tamey[27] = TAMEX3_SFP(sfp=1, card=27);
    tamey[28] = TAMEX3_SFP(sfp=1, card=28);
    tamey[29] = TAMEX3_SFP(sfp=1, card=29);
    tamey[30] = TAMEX3_SFP(sfp=1, card=30);
    tamey[31] = TAMEX3_SFP(sfp=1, card=31);
    tamey[32] = TAMEX3_SFP(sfp=1, card=32);
    tamey[33] = TAMEX3_SFP(sfp=1, card=33);
    tamey[34] = TAMEX3_SFP(sfp=1, card=34);
    tamey[35] = TAMEX3_SFP(sfp=1, card=35);
    tamey[36] = TAMEX3_SFP(sfp=1, card=36);
    tamey[37] = TAMEX3_SFP(sfp=1, card=37);
    tamey[38] = TAMEX3_SFP(sfp=1, card=38);
    tamey[39] = TAMEX3_SFP(sfp=1, card=39);
    tamey[40] = TAMEX3_SFP(sfp=1, card=40);
    tamey[41] = TAMEX3_SFP(sfp=1, card=41);
    tamey[42] = TAMEX3_SFP(sfp=1, card=42);
    tamey[43] = TAMEX3_SFP(sfp=1, card=43);
    tamey[44] = TAMEX3_SFP(sfp=1, card=44);
    tamey[45] = TAMEX3_SFP(sfp=1, card=45);
    tamey[46] = TAMEX3_SFP(sfp=1, card=46);
    tamey[47] = TAMEX3_SFP(sfp=1, card=47);
    tamey[48] = TAMEX3_SFP(sfp=1, card=48);
    tamey[49] = TAMEX3_SFP(sfp=1, card=49);
  }
}

SUBEVENT(tamex_subev)
{
  select several
    {
     land_vme = LAND_STD_VME();
    }
  data = tamex_subev_data();
}




EVENT
{
  tmxfh[0]  = tamex_subev_fh(type=102, subtype=10200, control=69);
  tmxfh[1]  = tamex_subev_fh(type=102, subtype=10200, control=70);
  tmxfh[2]  = tamex_subev_fh(type=102, subtype=10200, control=31);
  tmxfh[3]  = tamex_subev_fh(type=102, subtype=10200, control=32);

  tmxfh[4]  = tamex_subev_fh(type=102, subtype=10200, control=51);
  tmxfh[5]  = tamex_subev_fh(type=102, subtype=10200, control=52);
  tmxfh[6]  = tamex_subev_fh(type=102, subtype=10200, control=53);
  tmxfh[7]  = tamex_subev_fh(type=102, subtype=10200, control=54);

  tmxfh2[4] = tamex_subev_fh(type=102, subtype=10200, control=21);
  tmxfh2[5] = tamex_subev_fh(type=102, subtype=10200, control=22);
  tmxfh2[6] = tamex_subev_fh(type=102, subtype=10200, control=23);
  tmxfh2[7] = tamex_subev_fh(type=102, subtype=10200, control=24);

  tmx[0] = tamex_subev(type=102, subtype=10200, control= 3);
  tmx[1] = tamex_subev(type=102, subtype=10200, control= 8);

  tmx[2] = tamex_subev(type=102, subtype=10200, control=10);
  tmx[3] = tamex_subev(type=102, subtype=10200, control=11);

  ignore_unknown_subevent;
}
