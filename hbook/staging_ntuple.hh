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

#ifndef __STAGING_NTUPLE_H__
#define __STAGING_NTUPLE_H__

#include "ntuple_item.hh"

#include "writing_ntuple.hh"

#include "hbook.hh"

#include "external_writer.hh"

#include <vector>
#include <string>
#include <stdio.h>

typedef void(*ntuple_var_case_fcn)(char *);

struct type_indices
{
public:
  type_indices()
  {
    _slot_ind = 0;
    _dest_ind = 0;
  }

public:
  uint _slot_ind;
  uint _dest_ind;
};

class hbook_ntuple_cwn;
class TTree;

struct stage_ntuple_info
{
  external_writer  *ext;

  ntuple_var_case_fcn fix_case;

  const char *block;
  char       *base_ptr;

  char       *src_base;
};

class stage_ntuple_item
{
public:
  stage_ntuple_item(ntuple_item *item)
  {
    _item = item;
  }

  stage_ntuple_item(stage_ntuple_item *src)
  {
    _item = src->_item;
  }

  virtual ~stage_ntuple_item() { }

public:
  ntuple_item *_item;

public:
  void storage_size(indexed_item &write_ptrs,size_t &size);
  void init_cwn(indexed_item &write_ptrs,type_indices &indices,
		stage_ntuple_info &info,
		char *&ptr,const char *block);
};

// This class just to make dynamic_cast<> capable to distinguish from
// stage_ntuple_index_var.
class stage_ntuple_item_plain
  : public stage_ntuple_item
{
public:
  stage_ntuple_item_plain(ntuple_item *item)
    : stage_ntuple_item(item)
  {
  }

  virtual ~stage_ntuple_item_plain() { }
};

template<typename Tsni_ind, typename Tsni_vect>
class stage_ntuple_indexed_var;

template<typename Tsni_ind, typename Tsni_vect>
struct vect_stage_ntuple_indexed_var
{
  typedef std::vector<stage_ntuple_indexed_var<Tsni_ind,Tsni_vect> *> type;
};

struct sni_ind1
{
public:
  sni_ind1() { }
  sni_ind1(uint v1,uint dummy2 = 0) { UNUSED(dummy2); _1 = v1; }

public:
  void clear() { _1 = 0; }
  uint tot() const { return _1; }

  void iter(const sni_ind1 &lim) { UNUSED(lim); _1++; }
  bool iter_within(const sni_ind1 &lim) const { return _1 < lim._1; }

  bool outside(const sni_ind1 &lim) const { return _1 >= lim._1; }
  void expand_end(const sni_ind1 ind) {
    if (ind._1+1 > _1) _1 = ind._1+1;
  }

  uint lin_index(const sni_ind1 &lim) const { UNUSED(lim); return _1; }

  static bool is_2d() { return false; }

public:
  void dbg_fmt(char *str,size_t n) const { snprintf(str,n,"%d",_1); }

public:
  uint _1;
};

struct sni_ind2
{
public:
  sni_ind2() { }
  sni_ind2(uint v1,uint v2) { _1 = v1; _2 = v2; }

public:
  void clear() { _1 = _2 = 0; }
  uint tot() const { return _1 * _2; }

  void iter(const sni_ind2 &lim) { _2++; if (_2 >= lim._2) { _1++; _2 = 0; } }
  bool iter_within(const sni_ind2 &lim) const { return _1 < lim._1; }

  bool outside(const sni_ind2 &lim) const { return _1 >= lim._1 || _2 >= lim._2; }
  void expand_end(const sni_ind2 ind) {
    if (ind._1+1 > _1) _1 = ind._1+1;
    if (ind._2+1 > _2) _2 = ind._2+1;
  }

  uint lin_index(const sni_ind2 &lim) const { return _1 * lim._2 + _2; }

  static bool is_2d() { return true; }

public:
  void dbg_fmt(char *str,size_t n) const { snprintf(str,n,"(%d,%d)",_1,_2); }

public:
  uint _1;
  uint _2;
};

template<typename Tsni_ind, typename Tsni_vect>
class stage_ntuple_index_var
  : public stage_ntuple_item
{
public:
  stage_ntuple_index_var(stage_ntuple_item *src,Tsni_ind length)
    : stage_ntuple_item(src)
  {
    _length_max = length;
    _length_used.clear();
  }

  virtual ~stage_ntuple_index_var() { }

public:
  typename vect_stage_ntuple_indexed_var<Tsni_ind,Tsni_vect>::type _vars;
  Tsni_ind _length_max;
  Tsni_ind _length_used;

public:
  indexed_item _write_ptrs;

public:
  void storage_size(/*indexed_item &write_ptrs,*/size_t &size);

  template<typename Tindexed_item>
  void init_cwn_vars(Tindexed_item &write_ptrs,
		     stage_ntuple_info &info,
		     char *&ptr,const char *block);
  void init_cwn_ctrl_var(stage_ntuple_info &info,
			 char *&ptr,const char *block);

  void init_cwn(index_item &write_ptrs,
		/*indexed_item &write_ptrs,type_indices &indices,*/
		stage_ntuple_info &info,
		char *&ptr,const char *block);
  template<typename Tarray_item>
  void init_cwn(Tarray_item &write_ptrs,
		/*indexed_item &write_ptrs,type_indices &indices,*/
		stage_ntuple_info &info,
		char *&ptr,const char *block);
};

struct sni_vect1
{
public:
  void resize(const sni_ind1 &ind)
  {
    _items.resize(ind._1,NULL);
  }
  size_t size() { return _items.size(); }

  void set(const sni_ind1 &ind,ntuple_item *item) { _items[ind._1] = item; }
  ntuple_item *get(const sni_ind1 &ind) { return _items[ind._1]; }
  ntuple_item *first_item() { return _items[0]; }

public:
  std::vector<ntuple_item *> _items;
};

struct sni_vect2
{
public:
  void resize(const sni_ind2 &ind)
  {
    _items.resize(ind._1);
    for (uint i = 0; i < ind._1; i++)
      _items[i].resize(ind._2,NULL);
  }
  size_t size() { return _items.size() * _items[0].size(); }

  void set(const sni_ind2 &ind,ntuple_item *item) { _items[ind._1][ind._2] = item; }
  ntuple_item *get(const sni_ind2 &ind) { return _items[ind._1][ind._2]; }
  ntuple_item *first_item() { return _items[0][0]; }

public:
  std::vector<std::vector<ntuple_item *> > _items;
};

template<typename Tsni_ind, typename Tsni_vect>
class stage_ntuple_indexed_var
{
public:
  stage_ntuple_indexed_var(std::string &name,Tsni_ind length,
			   stage_ntuple_index_var<Tsni_ind,Tsni_vect> *index)
  {
    _name = name;
    _items.resize(length);
    _index = index;
  }

public:
  std::string _name;

  stage_ntuple_index_var<Tsni_ind,Tsni_vect> *_index;

public:
  Tsni_vect _items;

public:
  void storage_size(indexed_item &write_ptrs,size_t &size,Tsni_ind length_used);
 template<typename Tindexed_item>
 void init_cwn(Tindexed_item &write_ptrs,type_indices &indices,
	       Tsni_ind index,
	       const char *index_var,
	       Tsni_ind length_used,
	       stage_ntuple_info &info,
	       char *&ptr,const char *block,
	       bool first_item);
};

struct stage_ntuple_block_item
{
public:
  stage_ntuple_block_item(stage_ntuple_item_plain *item)
  {
    _item = item;
  }

  stage_ntuple_block_item(stage_ntuple_index_var<sni_ind1,sni_vect1> *index_var)
  {
    _item = index_var;
  }

  stage_ntuple_block_item(stage_ntuple_index_var<sni_ind2,sni_vect2> *index_var)
  {
    _item = index_var;
  }

public:
  stage_ntuple_item *_item;
};

typedef std::vector<stage_ntuple_block_item> vect_stage_ntuple_block_item;

class stage_ntuple_block
{
public:
  stage_ntuple_block(std::string &block)
  {
    _block = block;
  }

public:
  std::string _block;

public:
  vect_stage_ntuple_block_item _items;

public:
  void storage_size(indexed_item &write_ptrs,
		    size_t &size,
		    uint32_t &index_vars,uint32_t &array_vars,
		    /*int &index2_vars,*/uint32_t &array2_vars);
  void init_cwn(indexed_item &write_ptrs,type_indices &indices,
		index_item *&index_write_ptrs,
		array_item *&array_write_ptrs,
		/*index2_item *&index2_write_ptrs,*/
		array2_item *&array2_write_ptrs,
		stage_ntuple_info &info,
		char *&ptr);
};

typedef std::vector<stage_ntuple_block *> vect_stage_ntuple_blocks;

#endif//__STAGING_NTUPLE_H__

