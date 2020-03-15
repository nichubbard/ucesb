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

#ifndef __RAW_TO_CAL_HH__
#define __RAW_TO_CAL_HH__

#include "util.hh"

#include <stdlib.h>

typedef void (*raw_to_tcal_convert_fcn)(void *,const void *);

class raw_to_tcal_base
{
public:
  raw_to_tcal_base()
  {
    _convert = NULL;
  }

  virtual ~raw_to_tcal_base() { }

  virtual void show() = 0;

public:
  raw_to_tcal_convert_fcn _convert;
};

template<typename T_src/*,typename T_dest*/>
class raw_to_cal :
  public raw_to_tcal_base
{
public:
  raw_to_cal()
  {
    _dest     = NULL;
    _zzp_info = NULL;
  }

  virtual ~raw_to_cal() { }

  virtual void show() = 0;

public:
  typedef T_src  src_t;
  //typedef T_dest dest_t;

public:
  void                     *_dest;
  const zero_suppress_info *_zzp_info;

public:
  template<typename T_dest>
  void set_dest(T_dest *dest, int toggle_i);
};

template<typename T_src>
class r2c_randomize
{
public:
  // virtual ~r2c_randomize() { }

public:
  template<typename T_dest>
  static double convert(const T_src *src)
  {
    double value = (double) (src->value);

    value += ((double) random() / (double) RAND_MAX) - 0.5;

    // printf ("(rnd %f -> %f)",(double) src->value,value);

    return value;
  }
};

template<typename T_src/*,typename T_dest*/>
class r2c_slope_offset
  : public raw_to_cal<T_src/*,T_dest*/>
{
public:
  virtual ~r2c_slope_offset() { }

public:
  double _slope;
  double _offset;

public:
  virtual void show()
  {
    printf ("(() * %8.4g) + %8.4g",_slope,_offset);
  }

public:
  template<typename T_dest>
  bool convert(const T_src *src,T_dest *dest)
  {
    double value;

    r2c_randomize<T_src> randomize;

    value = randomize.template convert<T_dest>(src);

    *dest = (T_dest) ((value * _slope) + _offset);
    // printf ("sl_off %f -> %f\n",(double) (src->value),(double) *dest);
    return true;
  }
};

template<typename T_src/*,typename T_dest*/>
class r2c_offset_slope
: public raw_to_cal<T_src/*,T_dest*/>
{
public:
  virtual ~r2c_offset_slope() { }

public:
  double _offset;
  double _slope;

public:
  virtual void show()
  {
    printf ("(() + %8.4g) * %8.4g",_offset,_slope);
  }

public:
  template<typename T_dest>
  bool convert(const T_src *src,T_dest *dest)
  {
    double value;

    r2c_randomize<T_src> randomize;

    value = randomize.template convert<T_dest>(src);

    *dest = (T_dest) ((value + _offset) * _slope);

    // printf ("off_sl %f [+ %f * %f] -> %f\n",
    // (double) (src->value),
    // _offset,_slope,(double) *dest);
    return true;
  }
};

template<typename T_src/*,typename T_dest*/>
class r2c_slope
: public raw_to_cal<T_src/*,T_dest*/>
{
public:
  virtual ~r2c_slope() { }

public:
  double _slope;

public:
  virtual void show()
  {
    printf ("() * %8.4g",_slope);
  }

public:
  template<typename T_dest>
  bool convert(const T_src *src,T_dest *dest)
  {
    double value;

    r2c_randomize<T_src> randomize;

    value = randomize.template convert<T_dest>(src);

    *dest = (T_dest) (value * _slope);

    // printf ("off_sl %f [+ %f * %f] -> %f\n",
    // (double) (src->value),
    // _offset,_slope,(double) *dest);
    return true;
  }
};

template<typename T_src/*,typename T_dest*/>
class r2c_offset
: public raw_to_cal<T_src/*,T_dest*/>
{
public:
  virtual ~r2c_offset() { }

public:
  double _offset;

public:
  virtual void show()
  {
    printf ("() + %8.4g",_offset);
  }

public:
  template<typename T_dest>
  bool convert(const T_src *src,T_dest *dest)
  {
    double value;

    r2c_randomize<T_src> randomize;

    value = randomize.template convert<T_dest>(src);

    *dest = (T_dest) (value + _offset);

    // printf ("off_sl %f [+ %f * %f] -> %f\n",
    // (double) (src->value),
    // _offset,_slope,(double) *dest);
    return true;
  }
};

template<typename T_src/*,typename T_dest*/>
class r2c_cut_below_oe
: public raw_to_cal<T_src/*,T_dest*/>
{
public:
  virtual ~r2c_cut_below_oe() { }

public:
  double _cut;

public:
  virtual void show()
  {
    printf ("(() > %6.3g ? )",_cut);
  }

public:
  template<typename T_dest>
  bool convert(const T_src *src,T_dest *dest)
  {
    T_dest value = (T_dest) (src->value);

    if (value > _cut)
      {
	*dest = value;
	return true;
      }
    return false;
  }
};

template<typename T_src/*,typename T_dest*/>
union r2c_union
{
  r2c_slope_offset<T_src/*,T_dest*/> _slope_offset;
  r2c_offset_slope<T_src/*,T_dest*/> _offset_slope;

};

template<typename T_r2c,typename T_src,typename T_dest>
void call_r2c_convert(void *r2c_ptr,const void *src_ptr)
{
  T_dest value;
  T_src *src = (T_src *) src_ptr;
  T_r2c *r2c = (T_r2c *) r2c_ptr;

  bool success = r2c->convert(src,&value);

  if (!success) // no result to store
    return;

  void *dest = r2c->_dest;
  const zero_suppress_info *zzp_info = r2c->_zzp_info;

  switch (zzp_info->_type)
    {
    case ZZP_INFO_NONE: // no zero supress item
      break;
    case ZZP_INFO_FIXED_LIST: // part of fixed list
      {
	ERROR("Is this really working correctly?");
	// It seems that _list and _array are wrong names?!?, there
	// is the _fixed_list
	/*
	size_t offset = (*zzp_info->_list._call)(zzp_info->_array._item,
						 zzp_info->_array._index);
	dest = (((char *) dest) + offset);
	*/
	break;
      }
    case ZZP_INFO_CALL_ARRAY_INDEX:
      (*zzp_info->_array._call)(zzp_info->_array._item,
				zzp_info->_array._index);
      break;
    case ZZP_INFO_CALL_ARRAY_MULTI_INDEX:
      {
	size_t offset = (*zzp_info->_array._call_multi)(zzp_info->_array._item,
							zzp_info->_array._index);
	dest = (((char *) dest) + offset);
	break;
      }
    case ZZP_INFO_CALL_LIST_INDEX:
      {
	size_t offset = (*zzp_info->_list._call)(zzp_info->_list._item,
						 zzp_info->_list._index);
	dest = (((char *) dest) + offset);
	// printf ("%d - %d\n",zzp_info->_array._index,offset);
	break;
      }
    case ZZP_INFO_CALL_ARRAY_LIST_II_INDEX:
      (*zzp_info->_array._call)(zzp_info->_array._item,
				zzp_info->_array._index);
      goto call_list_ii_index;
    case ZZP_INFO_CALL_LIST_LIST_II_INDEX:
      {
	size_t offset = (*zzp_info->_list._call)(zzp_info->_list._item,
						 zzp_info->_list._index);
	dest = (((char *) dest) + offset);
	goto call_list_ii_index;
      }
    case ZZP_INFO_CALL_LIST_II_INDEX:
    call_list_ii_index:
      {
	size_t offset = (*zzp_info->_list_ii._call_ii)(zzp_info->_list_ii._item);
	dest = (((char *) dest) + offset);
	// printf ("%d - %d\n",zzp_info->_array._index,offset);
	break;
      }
    default:
      ERROR("Internal error in calib data mapping! (type=%d)",zzp_info->_type);
      break;
    }
  *((T_dest *) dest) = value;
}

template<typename T_r2c,typename T_src,typename T_dest>
void/*raw_to_tcal_convert_fcn*/ new_raw_to_cal(T_r2c *r2c,
					       const T_src *src,
					       T_dest *dest)
{
  UNUSED(r2c);
  UNUSED(src);
  UNUSED(dest);

  r2c->set_dest(dest, 0 /* the toggle for calib params applies to the src */);
  r2c->_convert = call_r2c_convert<T_r2c,T_src,T_dest>;
}

template<typename T_src,typename T_dest>
raw_to_tcal_base *new_slope_offset(const T_src *src,T_dest *dest,double slope,double offset)
{
  r2c_slope_offset<T_src> *obj = new r2c_slope_offset<T_src>;

  obj->_slope  = slope;
  obj->_offset = offset;

  new_raw_to_cal(obj,src,dest);

  return obj;
}

template<typename T_src,typename T_dest>
raw_to_tcal_base *new_offset_slope(const T_src *src,T_dest *dest,double offset,double slope)
{
  r2c_offset_slope<T_src> *obj = new r2c_offset_slope<T_src>;

  obj->_offset = offset;
  obj->_slope  = slope;

  new_raw_to_cal(obj,src,dest);

  return obj;
}

template<typename T_src,typename T_dest>
raw_to_tcal_base *new_slope(const T_src *src,T_dest *dest,double slope)
{
  r2c_slope<T_src> *obj = new r2c_slope<T_src>;

  obj->_slope  = slope;

  new_raw_to_cal(obj,src,dest);

  return obj;
}

template<typename T_src,typename T_dest>
raw_to_tcal_base *new_offset(const T_src *src,T_dest *dest,double offset)
{
  r2c_offset<T_src> *obj = new r2c_offset<T_src>;

  obj->_offset = offset;

  new_raw_to_cal(obj,src,dest);

  return obj;
}

template<typename T_src,typename T_dest>
raw_to_tcal_base *new_cut_below_oe(const T_src *src,T_dest *dest,double cut)
{
  r2c_cut_below_oe<T_src> *obj = new r2c_cut_below_oe<T_src>;

  obj->_cut = cut;

  new_raw_to_cal(obj,src,dest);

  return obj;
}

#endif//__RAW_TO_CAL_HH__
