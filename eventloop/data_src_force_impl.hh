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

#ifndef __DATA_SRC_FORCE_IMPL_H__
#define __DATA_SRC_FORCE_IMPL_H__

#define FORCE_IMPL_FCN(returns,fcn_name,...) \
  template returns fcn_name(__VA_ARGS__);

#if defined(USE_LMD_INPUT) || defined(USE_HLD_INPUT) || defined(USE_RIDF_INPUT)

#define FORCE_IMPL_DATA_SRC_FCN(returns,fcn_name) \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,0,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,1,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,0,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,1,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,0,1> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,1,1> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,0,1> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,1,1> &__buffer);

#define FORCE_IMPL_DATA_SRC_FCN_ARG(returns,fcn_name,...) \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,0,0> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,1,0> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,0,0> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,1,0> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,0,1> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,1,1> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,0,1> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,1,1> &__buffer,__VA_ARGS__);

#define FORCE_IMPL_DATA_SRC_FCN_HDR(returns,fcn_name) \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<0,0,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<0,1,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<1,0,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<1,1,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<0,0,1> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<0,1,1> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<1,0,1> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<1,1,1> &__buffer);

#else

#define FORCE_IMPL_DATA_SRC_FCN(returns,fcn_name) \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,1> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,1> &__buffer);

#define FORCE_IMPL_DATA_SRC_FCN_ARG(returns,fcn_name,...) \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,0> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,0> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<0,1> &__buffer,__VA_ARGS__); \
  FORCE_IMPL_FCN(returns,fcn_name,__data_src<1,1> &__buffer,__VA_ARGS__);

#define FORCE_IMPL_DATA_SRC_FCN_HDR(returns,fcn_name) \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<0,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<1,0> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<0,1> &__buffer); \
  FORCE_IMPL_FCN(returns,fcn_name,subevent_header *__header,__data_src<1,1> &__buffer);

#endif

#endif//__DATA_SRC_FORCE_IMPL_H__
