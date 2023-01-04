/* This file is part of UCESB - a tool for data unpacking and processing.
 *
 * Copyright (C) 2022  Haakan T. Johansson  <f96hajo@chalmers.se>
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

#include "tdcpm_struct_info.h"

TDCPM_STRUCT_DEF
typedef struct calt_t
{
  double _k TDCPM_UNIT("ns/ch");
  double _m TDCPM_UNIT("ns");
} calt;

TDCPM_STRUCT_DEF
typedef struct cale_t
{
  double _k TDCPM_UNIT("10 MeV/ch");
  double _m TDCPM_UNIT("10 MeV");
} cale;

#define SEVEN 7

TDCPM_STRUCT_DEF
typedef struct cal2_t
{
  double _a;
  double _b[SEVEN];
  double _c [ 5 ][ 6 ] ;  /* Intentional spaces for parser test. */

  calt   _d;
  cale   _e[4];
  cale   _f[2][3];
} cal2;

TDCPM_STRUCT_INST(r) cal2 _cal_det_r;
TDCPM_STRUCT_INST(s) calt _cal_det_s[7];
TDCPM_STRUCT_INST(t) cal2 _cal_det_t[7][6];

void tdcpm_test_struct()
{
  tdcpm_struct_info      *li_calt;
  tdcpm_struct_info_item *li_calt_k;
  tdcpm_struct_info_item *li_calt_m;

  tdcpm_struct_info      *li_cale;
  tdcpm_struct_info_item *li_cale_k;
  tdcpm_struct_info_item *li_cale_m;

  tdcpm_struct_info      *li_cal2;
  tdcpm_struct_info_item *li_cal2_a;
  tdcpm_struct_info_item *li_cal2_b;
  tdcpm_struct_info      *li_cal2_b_i;
  tdcpm_struct_info_item *li_cal2_c;
  tdcpm_struct_info      *li_cal2_c_i;
  tdcpm_struct_info      *li_cal2_c_i2;
  
  tdcpm_struct_info_item *li_cal2_d;
  tdcpm_struct_info_item *li_cal2_e;
  tdcpm_struct_info      *li_cal2_e_i;
  tdcpm_struct_info_item *li_cal2_f;
  tdcpm_struct_info      *li_cal2_f_i;
  tdcpm_struct_info      *li_cal2_f_i2;

  tdcpm_struct_info_item *gi_cal_det_r;
  tdcpm_struct_info_item *gi_cal_det_s;
  tdcpm_struct_info_item *gi_cal_det_t;
  
  /* li_cal_det_a = TDCPM_LAYOUT_LEVEL_0(_cal_det_a, "A"); */

  li_calt = TDCPM_STRUCT(calt, "calt");
  
  li_calt_k = TDCPM_STRUCT_ITEM_DOUBLE(li_calt, calt, _k, "k", "ns/ch");
  li_calt_m = TDCPM_STRUCT_ITEM_DOUBLE(li_calt, calt, _m, "m", "ns");
  
  li_cale = TDCPM_STRUCT(cale, "cale");
  
  li_cale_k = TDCPM_STRUCT_ITEM_DOUBLE(li_cale, cale, _k, "k", "10 MeV/ch");
  li_cale_m = TDCPM_STRUCT_ITEM_DOUBLE(li_cale, cale, _m, "m", "10 MeV");
  
  li_cal2 = TDCPM_STRUCT(cal2, "cal2");
  
  li_cal2_a = TDCPM_STRUCT_ITEM_DOUBLE(li_cal2, cal2, _a, "a", "");
  li_cal2_b = TDCPM_STRUCT_ITEM_DOUBLE(li_cal2, cal2, _b, "b", "");
  /*       */ TDCPM_STRUCT_ITEM_ARRAY(li_cal2_b, cal2, _b);
  li_cal2_c = TDCPM_STRUCT_ITEM_DOUBLE(li_cal2, cal2, _c, "c", "");
  /*       */ TDCPM_STRUCT_ITEM_ARRAY(li_cal2_c, cal2, _c);
  /*       */ TDCPM_STRUCT_ITEM_ARRAY(li_cal2_c, cal2, _c[0]);

  li_cal2_d = TDCPM_STRUCT_ITEM_STRUCT(li_cal2, cal2, _d, "d", li_calt);
  li_cal2_e = TDCPM_STRUCT_ITEM_STRUCT(li_cal2, cal2, _e, "e", li_cale);
  /*       */ TDCPM_STRUCT_ITEM_ARRAY(li_cal2_e, cal2, _e);
  li_cal2_f = TDCPM_STRUCT_ITEM_STRUCT(li_cal2, cal2, _f, "f", li_cale);
  /*       */ TDCPM_STRUCT_ITEM_ARRAY(li_cal2_f, cal2, _f);
  /*       */ TDCPM_STRUCT_ITEM_ARRAY(li_cal2_f, cal2, _f[0]);

  (void) li_calt_k;
  (void) li_calt_m;

  (void) li_cale_k;
  (void) li_cale_m;

  (void) li_cal2_a;
  (void) li_cal2_b;
  (void) li_cal2_b_i;
  (void) li_cal2_c;
  (void) li_cal2_c_i;
  (void) li_cal2_c_i2;

  (void) li_cal2_d;
  (void) li_cal2_e;
  (void) li_cal2_e_i;
  (void) li_cal2_f;
  (void) li_cal2_f_i;
  (void) li_cal2_f_i2;
  
  gi_cal_det_r = TDCPM_STRUCT_INSTANCE(/*cal2,*/ _cal_det_r, "r", li_cal2);
  gi_cal_det_s = TDCPM_STRUCT_INSTANCE(/*calt,*/ _cal_det_s, "s", li_calt);
  TDCPM_STRUCT_INSTANCE_ARRAY(gi_cal_det_s, /*calt,*/ _cal_det_s);

  gi_cal_det_t = TDCPM_STRUCT_INSTANCE(/*cal2,*/ _cal_det_t, "t", li_cal2);
  TDCPM_STRUCT_INSTANCE_ARRAY(gi_cal_det_t, /*cal2,*/ _cal_det_t);
  TDCPM_STRUCT_INSTANCE_ARRAY(gi_cal_det_t, /*cal2,*/ _cal_det_t[0]);

  (void) gi_cal_det_r;
  (void) gi_cal_det_s;
  (void) gi_cal_det_t;
  
  /*
  TDCPM_LAYOUT_BASE(_cal_det_t, "t");

  TDCPM_LAYOUT_BASE(_cal_det_u, "u");
  */
  
  /*
  TDCPM_LAYOUT_ARRAY(base, _cal_det_b, _cal_det_b);

  TDCPM_LAYOUT_ARRAY(base, _cal_det_c);
  TDCPM_LAYOUT_ARRAY(base, _cal_det_c[0]);
  */

}

#include "test/generated/tdcpm_test_struct_decl.c"
