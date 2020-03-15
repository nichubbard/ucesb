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

#include "structures.hh"

#include "user.hh"
#include "optimise.hh"

#include "corr_plot_dense.hh"

#include "convert_picture.hh"

#include "hirich_pads.h"

#include "huffman.c"

#define HIRICH_SIZE 64

// This is not the most well-separated thing to do to handle the data
// in some global variables in the user function, but for
// quick-n-dirty testing it does get the job done...

// These are an relic since the unpacking was in the user function
// also...

struct rich_pad_item
{
  uint16 _index;
  uint8  _value;
};

struct rich_pads
{
  int           _n;
  rich_pad_item _pads[256*16];
};

rich_pads pads;

// To store the x-y picture of the detector

struct rich_masked
{
 // +1 to have one uint32 entry empty at the end (wanted by the encoder)
  uint32 _mask[HIRICH_SIZE+1][(HIRICH_SIZE+(sizeof(uint32)*8)-1)/(sizeof(uint32)*8)];
  uint8  _value[HIRICH_SIZE][HIRICH_SIZE];
};

rich_masked masked;

/*
//dense_corr _corr_index;
//dense_corr _corr_index2;
dense_corr _corr_padsx;
dense_corr _corr_padsy;

int _count_indices[4096];
*/

void picture_rich(rich_pads *pads);

void compress_rich(const rich_masked &masked);

void user_function(unpack_event *event,
		   raw_event    *raw_event,
		   cal_event    *cal_event/*,
		   rich_user_struct *user_event*/)
{
  /*-------------------------------------------------------------------------*/

#if 1

  rich_pad_item *next_pad = pads._pads;

  // modules in use:   1 9 17 25 33 41 49 57 65 73 81 89 87 105 113 121

  // Fill in the relic structures (should be removed and EXT_HIRICH
  // used directly instead) The unpacking has however ensured that we
  // have no duplicates, so no need to check that.

  for (int mi = 0; mi < event->rich.rich._num_modules; mi++)
    {
      EXT_HIRICH_module *module = &event->rich.rich._modules[mi];

      // three lower bits of module number are unused (fixed at 1..)

      if ((module->header.module_no & 0x07) != 1)
	ERROR("Module number % 8 not 1, unexpected.");

      int effective_module_shifted = (module->header.module_no >> 3) << 8;

      for (int i = 0; i < module->data._num_items; i++)
	{
	  hirich_data_word &data = module->data._items[i];

	  next_pad->_index = effective_module_shifted | data.channel;
	  next_pad->_value = data.value;

	  next_pad++;
	}
    }

  pads._n = next_pad - pads._pads;

  /////////////////////////////////////////////////////////////////

  // copy the pad data into a data structure representing bit pattern
  // of pads that fired, and the values (in their own locations, so as
  // to not need to do any sorting (well, it's actually bucket sort
  // then...)

  memset(masked._mask,0,sizeof(masked._mask));

  rich_pad_item *pad = pads._pads;

  for (int i = pads._n; i; --i, pad++)
    {
      int x, y;

      if (UNLIKELY(pad->_index > 4095))
	ERROR("Pad index (%d) too high.\n");

      y = padXY[pad->_index][0] - 8;
      x = padXY[pad->_index][1] - 8;

      uint32 *mask = &masked._mask[y][x >> 5];
      uint32  bit  = 1 << (x & 0x1f);

      if (UNLIKELY(*mask & bit))
	WARNING("Pad indexed %d (x=%d,y=%d) again (new=%d,old=%d).",
		x,y,pad->_value,masked._value[y][x]);

      *mask |= bit;

      masked._value[y][x] = pad->_value;
    }

  /////////////////////////////////////////////////////////////////

  // picture_rich(&pads);

  compress_rich(masked);


#if 0
  // printf ("mul: %d\n",pads._n);

  char lines[80][81];

  memset(lines,' ',sizeof(lines));

  rich_pad_item *pad = pads._pads;

  int index[4096];
  int *end_index = index;

  int cpadx[4096];
  int *end_cpadx = cpadx;

  int cpady[4096];
  int *end_cpady = cpady;

  for (int i = pads._n; i; --i, pad++)
    {
      int x, y;
      /*
      x = pad->_index & 0x3f;
      y = pad->_index >> 6;
      */
      if (pad->_index > 4095)
	ERROR("pad index too high\n");

      y = padXY[pad->_index][0];
      x = padXY[pad->_index][1];

      /*
      if (x >= 40 && x < (40+32) &&
	  y >= 40 && y < (40+32))
	*(end_cpad++) = ((y-40) * 32) + (x - 40);
	*/
      // if (pad->_index < 256)

      if (pad->_value > 30)
	{
	  *(end_index++) = pad->_index;

	  if (x >= 24 && x < (24+16) &&
	      y >= 24 && y < (24+16))
	    {
	      *(end_cpadx++) = ((y-24) * 16) + (x-24);
	      *(end_cpady++) = ((x-24) * 16) + (y-24);
	    }

	}
      _count_indices[pad->_index]++;

      lines[y][x] = hexilog2(pad->_value);
    }
#endif
  /*
  qsort(index,end_index - index,sizeof(int),compare_values<int>);
  _corr_index.add(index,end_index);

  int *i2 = index;

  for (int j = 0; j < 4096; j += 128)
    {
      int *i2_end = i2;

      while (i2_end < end_index && *(i2_end+1) < j+128)
	{
	  *(i2_end) -= j;
	  i2_end++;
	}

      _corr_index2.add(i2,i2_end);

      i2 = i2_end;
    }
  */
  /*
  qsort(cpadx,end_cpadx - cpadx,sizeof(int),compare_values<int>);
  _corr_padsx.add(cpadx,end_cpadx);

  qsort(cpady,end_cpady - cpady,sizeof(int),compare_values<int>);
  _corr_padsy.add(cpady,end_cpady);
  */
#endif

}

uint32 pattern8[0x100];
uint32 pattern12[0x1000];
uint32 pattern16[0x10000];

uint32 freqval[0x100];
uint32 freqdiff[0x100];
uint32 freqdiff2[0x100];
uint32 freqdiffm[0x10000];

#define PICT_SIZE 80
#define PICT_SIDE 10

#define PICT_LINE_SIZE (PICT_SIZE*PICT_SIDE+(PICT_SIDE-1))
#define PICT_ARRAY_SIZE (PICT_LINE_SIZE)*(PICT_LINE_SIZE)

char *_pict;
int   _npict = 0;

void user_init()
{
  _pict = (char*) malloc(PICT_ARRAY_SIZE);

  //_corr_pads.clear(80*80);
  //_corr_pads.clear(256);
  //_corr_index.clear(4096);
  //_corr_index2.clear(128);

  //_corr_padsx.clear(80*80);
  //_corr_padsy.clear(80*80);
  //_corr_padsx.clear(16*16);
  //_corr_padsy.clear(16*16);

  //memset(_count_indices,0,sizeof (_count_indices));

  memset(pattern8 ,0,sizeof(pattern8 ));
  memset(pattern12,0,sizeof(pattern12));
  memset(pattern16,0,sizeof(pattern16));

  memset(freqval ,0,sizeof(freqval ));
  memset(freqdiff,0,sizeof(freqdiff));
  memset(freqdiff2,0,sizeof(freqdiff2));
  memset(freqdiffm,0,sizeof(freqdiffm));
}


struct pattern_freq
{
  int _freq;
  int _pattern;
};

template<int n,typename T>
void print_mask_freq(const char *label,T *counts)
{
  printf ("\n=== %s ===================\n\n",label);

  int N = 1 << n;

  pattern_freq pf[N];

  for (int i = 0; i < N; i++)
    {
      pf[i]._freq    = counts[i];
      pf[i]._pattern = i;
    }

  qsort(pf,N,sizeof(pattern_freq),compare_values<int>);

  for (int i = N-1; i >= 0; i--)
    {
      int pattern = pf[i]._pattern;
      int freq    = pf[i]._freq;
      int nbits = 0;

      printf ("%5d: %0*x: ",i,n/4,pattern);

      int bits = pattern;

      for (int j = 0; j < n; j++, bits >>= 1)
	{
	  nbits += (bits & 1);
	  putchar ('0' + (bits & 1));
	}

      printf (" %2d  %d\n",nbits,freq);
    }

}




void user_exit()
{
  free(_pict);

  //_corr_index.picture("rich_i.png");
  //_corr_index2.picture("rich_i128.png");
  //_corr_padsx.picture("rich_px.png");
  //_corr_padsy.picture("rich_py.png");
  /*
  for (int i = 0; i < 4096; i++)
    printf ("%4d: %10d\n",i,_count_indices[i]);
  */


  print_mask_freq<8>("pattern8",pattern8);
  print_mask_freq<12>("pattern12",pattern12);
  print_mask_freq<16>("pattern16",pattern16);

  print_mask_freq<8>("freqval" ,freqval);
  print_mask_freq<8>("freqdiff",freqdiff);

  optimize_huff(freqval,0x100);
  optimize_huff(freqdiff,0x100);
  optimize_huff(freqdiff2,0x100);
  optimize_huff(freqdiffm,0x10000);

  optimize_huff_length(pattern8,0x100,16);
  optimize_huff_length(pattern12,0x1000,20);
  optimize_huff_length(pattern16,0x10000,24);

}




void count_pattern(uint32 m)
{
  pattern8 [m &   0xff]++;
  pattern12[m &  0xfff]++;
  pattern16[m & 0xffff]++;
}


void picture_rich(rich_pads *pads)
{
  if (_npict == 0)
    {
      memset(_pict,0,PICT_ARRAY_SIZE);

      for (int i = 1; i < PICT_SIDE; i++)
	{
	  for (int j = 0; j < PICT_LINE_SIZE; j++)
	    {
	      *(_pict + (i * (PICT_SIZE + 1) - 1) + j * PICT_LINE_SIZE) = 128;
	      *(_pict + (i * (PICT_SIZE + 1) - 1) * PICT_LINE_SIZE + j) = 128;
	    }
	}
    }

  int px = _npict % PICT_SIDE;
  int py = _npict / PICT_SIDE;

  char *start = _pict +
    px * (PICT_SIZE + 1) +
    py * (PICT_SIZE + 1) * PICT_LINE_SIZE;

  rich_pad_item *pad = pads->_pads;

  for (int i = pads->_n; i; --i, pad++)
    {
      int x, y;

      if (pad->_index > 4095)
	continue;

      y = padXY[pad->_index][0] - 8;
      x = padXY[pad->_index][1] - 8;

      *(start + x + y * PICT_LINE_SIZE) = 15*ilog2(pad->_value);
    }

  _npict++;

  // printf ("%d / %d...\n",_npict,(PICT_SIDE * PICT_SIDE));

  if (_npict == (PICT_SIDE * PICT_SIDE))
    {
      char filename[256];
      static int rings = 0;

      sprintf (filename,"ring_%03d.png",rings++);

      convert_picture(filename,_pict,PICT_LINE_SIZE,PICT_LINE_SIZE);

      _npict = 0;
    }
}


void compress_rich(const rich_masked &masked)
{
  // calculate to occurrence of different bit-patterns
  // starting at every bit

  const uint *mask = &masked._mask[0][0];

  for (int i = 0; i < 128; i++, mask++)
    {
      uint64 m1 = ((uint64) *(mask)) | (((uint64) *(mask+1)) << 32);

      for (int j = 0; j < 32; j++)
	{
	  count_pattern((uint32) m1);
	  m1 >>= 1;
	}
    }

  // get the values into an array of only values

  uint8 values[4096];
  uint8 *val_end;

  mask = &masked._mask[0][0];

  const uint8 *v = &masked._value[0][0];

  val_end = values;

  for (uint i = 0; i < 64*2; i++)
    {
      uint32 m = *(mask++);

      for (int j = 32; j; --j, m >>= 1, v++)
	if (m & 1)
	  {
	    *(val_end++) = *v;
	  }
    }
  /*
  for (uint8 *i = values; i < val_end; i++)
    printf ("%4d",*i);
  printf ("\n\n");
  */
  uint8 diff[4096];
  uint8 *diff_end = diff;

  uint8 diff2[4096];
  uint8 *diff2_end = diff;

  uint8 prev = 0;
  uint8 prev2 = 0;

  for (uint8 *i = values; i < val_end; i++)
    {
      uint8 v = *i;

      freqval[v]++;
      freqdiff[(uint8) (v-prev)]++;

      freqdiff2[(uint8) (v-(prev+(((signed char)(prev-prev2))>>1)))]++;

      *(diff_end++) = v - prev;

      prev2 = prev;
      prev = v;
    }
  /*
  for (uint8 *i = diff; i < diff_end; i++)
    printf ("%4d",*i);
  printf ("\n\n");
  */

  {
    uint8 *i = diff;

    for ( ; i < diff_end-1; i += 2)
      freqdiffm[(*i) | (((int) *(i+1)) << 8)]++;

    if (i < diff_end)
      freqdiffm[*i]++;
  }

}
