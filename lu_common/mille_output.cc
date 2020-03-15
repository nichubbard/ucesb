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

#include "mille_output.hh"

#include "error.hh"

#include <stdlib.h>
#include <limits.h>

mille_record::mille_record()
{
  _array_used      = 0;
  _array_allocated = 0;
  _array_float = NULL;
  _array_int   = NULL;

  _num_eqn = 0;
}

mille_record::~mille_record()
{
  free(_array_float);
  free(_array_int);
}

void mille_record::reset()
{
  _num_eqn = 0;
  _array_used = 0;

  if (_array_allocated < 1)
    reallocate(1);

  // Setup start of record
  _array_float[0] = 0.0;
  _array_int[0]   = 0;
  _array_used = 1;
}

void mille_record::reallocate(size_t need_more)
{
  size_t new_size = _array_allocated;

  if (new_size < 16)
    new_size = 16;
  while (new_size < _array_used + need_more)
    new_size *= 2;

  float *f = (float *) realloc(_array_float,sizeof(float) * new_size);
  if (!f)
    ERROR("Failure reallocating mille record buffer.");
  _array_float = f;

  int *i = (int *) realloc(_array_int,sizeof(float) * new_size);
  if (!i)
    ERROR("Failure reallocating mille record buffer.");
  _array_int = i;

  _array_allocated = new_size;
}

void mille_record::add_der_lbl(int num,float *der,int *lbl)
{
  for (int i = 0; i < num; i++)
    if (der[i] != 0.0)
      {
	if (lbl[i] <= 0)
	  ERROR("Mille label %d is zero or negative (%d).",
		i,lbl[i]);

	_array_float[_array_used] = der[i];
	_array_int[_array_used]   = lbl[i];
	_array_used++;
      }
}

void mille_record::add_eqn(int num_local,float *der_local,int *lbl_local,
			   int num_global,float *der_global,int *lbl_global,
			   float measured,float sigma)
{
  size_t most_need = (size_t) (2 + num_local + num_global);

  if (_array_used + most_need > _array_allocated)
    reallocate(most_need);

  // write value to arrays
  _array_float[_array_used] = measured;
  _array_int[_array_used]   = 0;
  _array_used++;

  // write locals to arrays
  add_der_lbl(num_local,der_local,lbl_local);

  // write value to arrays
  _array_float[_array_used] = sigma;
  _array_int[_array_used]   = 0;
  _array_used++;

  // write globals to arrays
  add_der_lbl(num_global,der_global,lbl_global);

  _num_eqn++;
}

void mille_record::add_der_lbl(int num,mille_lbl_der *lbl_der)
{
  for (int i = 0; i < num; i++)
    if (lbl_der[i].der != 0.0)
      {
	if (lbl_der[i].lbl <= 0)
	  ERROR("Mille label %d is zero or negative (%d).",
		i,lbl_der[i].lbl);

	_array_float[_array_used] = lbl_der[i].der;
	_array_int[_array_used]   = lbl_der[i].lbl;
	_array_used++;
      }
}

void mille_record::add_eqn(int num_local,mille_lbl_der *lbl_der_local,
			   int num_global,mille_lbl_der *lbl_der_global,
			   float measured,float sigma)
{
  size_t most_need = (size_t) (2 + num_local + num_global);

  if (_array_used + most_need > _array_allocated)
    reallocate(most_need);

  // write value to arrays
  _array_float[_array_used] = measured;
  _array_int[_array_used]   = 0;
  _array_used++;

  // write locals to arrays
  add_der_lbl(num_local,lbl_der_local);

  // write value to arrays
  _array_float[_array_used] = sigma;
  _array_int[_array_used]   = 0;
  _array_used++;

  // write globals to arrays
  add_der_lbl(num_global,lbl_der_global);

  _num_eqn++;
}


mille_file::mille_file()
{
  _fid = NULL;
}

mille_file::~mille_file()
{
  close();
}

void mille_file::open(const char *filename)
{
  close();

  _fid = fopen(filename,"w");

  if (_fid == NULL)
    {
      perror("fopen");
      ERROR("Failed to open file '%s' for writing.",filename);
    }

  INFO(0,"Opened millepede output file '%s'.",filename);
}

void mille_file::close()
{
  if (_fid)
    if (fclose(_fid) != 0)
      perror("close");
  _fid = NULL;
}

void mille_file::write(const mille_record &record)
{
  // Don't write if there is no data!

  if (record._array_used <= 1)
    return;

  assert (record._array_used * 2 < INT_MAX);

  int nwrite = (int) (record._array_used * 2);

  if (fwrite(&nwrite,sizeof(int),1,_fid) != 1 ||
      fwrite(record._array_float,sizeof(float),record._array_used,_fid) !=
      /* */                                    record._array_used ||
      fwrite(record._array_int,sizeof(float),record._array_used,_fid) !=
      /* */                                  record._array_used)
    {
      perror("fwrite");
      ERROR("Failure writing record (array length %zd) to mille file.",
	    record._array_used);
    }
}
