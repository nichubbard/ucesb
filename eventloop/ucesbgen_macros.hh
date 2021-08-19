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

#ifndef __UCESBGEN_MACROS_HH__
#define __UCESBGEN_MACROS_HH__

#include "error.hh"
#include "optimise.hh"

/*
 *
 */

#ifdef USE_LMD_INPUT
#include "lmd_event.hh"
#endif

#ifdef USE_HLD_INPUT
#include "hld_event.hh"
#endif

#ifdef USE_RIDF_INPUT
#include "ridf_event.hh"
#endif

// The UNLIKELY(...) should also be in the PEEK_FROM_BUFFER, but it
// seems many versions (random) of gcc then comes with spurious warnings
// about __match_peek possibly being used unitialized...

#define PEEK_FROM_BUFFER(loc,data_type,dest) {                      \
  if (/*UNLIKELY(*/!__buffer.peek_##data_type(&dest)/*)*/) {        \
    ERROR_U_LOC(loc,"Error while reading %s from buffer.",#dest);   \
  }                                                                 \
}

#define READ_FROM_BUFFER(loc,data_type,dest,account_id) {	   \
  if (UNLIKELY(!__buffer.get_##data_type(&dest))) {                \
    ERROR_U_LOC(loc,"Error while reading %s from buffer.",#dest);  \
  }                                                                \
  if (__buffer.is_account()) do_account_##data_type(account_id);   \
}

#define MATCH_READ_FROM_BUFFER(loc,data_type,dest,account_id) {	   \
  if (UNLIKELY(!__buffer.get_##data_type(&dest))) {                \
    ERROR_U_LOC(loc,"Error while reading %s from buffer.",#dest);  \
  }                                                                \
}

#define PEEK_FROM_BUFFER_FULL(loc,data_type,dest,dest_full,account_id) { \
  if (UNLIKELY(!__buffer.peek_##data_type(&dest_full))) {          \
    ERROR_U_LOC(loc,"Error while reading %s from buffer.",#dest);  \
  }                                                                \
}

#define READ_FROM_BUFFER_FULL(loc,data_type,dest,dest_full,account_id) { \
  if (UNLIKELY(!__buffer.get_##data_type(&dest_full))) {           \
    ERROR_U_LOC(loc,"Error while reading %s from buffer.",#dest);  \
  }                                                                \
  if (__buffer.is_account()) do_account_##data_type(account_id);   \
}

#define MATCH_READ_FROM_BUFFER_FULL(loc,data_type,dest,dest_full,account_id) {\
  if (UNLIKELY(!__buffer.get_##data_type(&dest_full))) {           \
    ERROR_U_LOC(loc,"Error while reading %s from buffer.",#dest);  \
  }                                                                \
}

#define CHECK_BITS_EQUAL(loc,value,constraint) {	      \
  if (UNLIKELY((value) != (constraint))) {		      \
    ERROR_U_LOC(loc,"%s should be 0x%llx, is 0x%llx.",#value, \
		(uint64) (constraint),(uint64) (value));      \
  }							      \
}

#define CHECK_UNNAMED_BITS_ZERO(loc,value,mask) {                            \
  if (UNLIKELY(((value) & (mask)) != 0)) {                                   \
    ERROR_U_LOC(loc,"Undefined parts of %s (mask 0x%llx) expected to be 0, " \
		    "are 0x%llx (masked 0x%llx).",#value,		     \
	      (uint64) (mask),(uint64) (value),(uint64) ((value) & (mask))); \
  }                                                                          \
}

#define CHECK_BITS_RANGE(loc,value,min,max) {                               \
  if (UNLIKELY((value) < (min) || (value) > (max))) {                       \
    ERROR_U_LOC(loc,"%s should be within 0x%llx..0x%llx, is 0x%llx.",       \
		    #value,(uint64) (min),(uint64) (max),(uint64) (value)); \
  }                                                                         \
}

#define CHECK_BITS_RANGE_MAX(loc,value,max) {                             \
  if (UNLIKELY((value) > (max))) {                                        \
    ERROR_U_LOC(loc,"%s should be within 0x%llx..0x%llx, is 0x%llx.",     \
		    #value,(uint64) (0),(uint64) (max),(uint64) (value)); \
  }                                                                       \
}

#define CHECK_WORD_COUNT(loc,value,start,stop,offset,multiplier) { \
  size_t __check_distance =                                        \
    (size_t) (((char*) __mark_##stop) - ((char*) __mark_##start)) + (offset); \
  if (UNLIKELY(((size_t) (value)) * (multiplier) != __check_distance)) { \
    ERROR_U_LOC(loc,"%s (= %d) times multiplier %d, is %d, "       \
                    "should be byte count from %s"                 \
                    " to %s + offset %d = %d.",                    \
		    #value,(value),				   \
      		    (multiplier),(value) * (multiplier),	   \
                    #start,#stop,                                  \
                    (offset),(int) __check_distance);              \
  }                                                                \
}

#define MATCH_BITS_EQUAL(loc,value,constraint) {      \
  if ((value) != (constraint)) {                      \
    return false;                                     \
  }                                                   \
}

#define MATCH_UNNAMED_BITS_ZERO(loc,value,mask) {     \
  if (((value) & (mask)) != 0) {                      \
    return false;                                     \
  }                                                   \
}

#define MATCH_BITS_RANGE(loc,value,min,max) {         \
  if ((value) < (min) || (value) > (max)) {           \
    return false;                                     \
  }                                                   \
}

#define MATCH_BITS_RANGE_MAX(loc,value,max) {         \
  if ((value) > (max)) {                              \
    return false;                                     \
  }                                                   \
}

#define MATCH_WORD_COUNT(loc,value,start,stop,offset,multiplier) { }

#define CHECK_JUMP_BITS_EQUAL(loc,value,constraint,jump_target) {  \
  if ((value) != (constraint)) {                                   \
    goto jump_target;                                              \
  }                                                                \
}

#define CHECK_JUMP_UNNAMED_BITS_ZERO(loc,value,mask,jump_target) { \
  if (((value) & (mask)) != 0) {                                   \
    goto jump_target;                                              \
  }                                                                \
}

#define CHECK_JUMP_BITS_RANGE(loc,value,min,max,jump_target) {     \
  if ((value) < (min) || (value) > (max)) {                        \
    goto jump_target;                                              \
  }                                                                \
}

#define CHECK_JUMP_BITS_RANGE_MAX(loc,value,max,jump_target) {     \
  if ((value) > (max)) {                                           \
    goto jump_target;                                              \
  }                                                                \
}

#define CHECK_JUMP_WORD_COUNT(loc,value,start,stop,offset,multiplier,jump_target) { }

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define MATCH_DECL(loc,match,matchno,decltype,declname,__VA_ARGS__...) { \
  __data_src_t __tmp_buffer(__buffer);                    \
  if (decltype::__match(__tmp_buffer, ## __VA_ARGS__)) { \
    if (UNLIKELY(match)) {                               \
      ERROR_U_LOC(loc,"Two (or more) matches for select statement, previous #%d, this #%d (%s).",match,matchno,#declname); \
    }                                        \
    match = matchno;                         \
  }                                          \
}
#else
#define MATCH_DECL(loc,match,matchno,decltype,declname,...) { \
  __data_src_t __tmp_buffer(__buffer);                   \
  if (decltype::__match(__tmp_buffer, ## __VA_ARGS__)) { \
    if (UNLIKELY(match)) {                               \
      ERROR_U_LOC(loc,"Two (or more) matches for select statement, previous #%d, this #%d (%s).",match,matchno,#declname); \
    }                                        \
    match = matchno;                         \
  }                                          \
}
#endif

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define CHECK_SPURIOUS_MATCH_DECL(loc,spurious_match_label,decltype,__VA_ARGS__...) { \
  __data_src_t __tmp_buffer(__buffer);         \
  if (!decltype::__match(__tmp_buffer, ## __VA_ARGS__)) { \
    goto spurious_match_label;               \
  }                                          \
}
#else
#define CHECK_SPURIOUS_MATCH_DECL(loc,spurious_match_label,decltype,...) { \
  __data_src_t __tmp_buffer(__buffer);         \
  if (!decltype::__match(__tmp_buffer, ## __VA_ARGS__)) { \
    goto spurious_match_label;               \
  }                                          \
}
#endif

#define MATCH_DECL_QUICK(loc,match,matchno,declname,peek,mask,value) { \
  if (((peek) & (mask)) == (value)) {        \
    if (UNLIKELY(match)) {                   \
      ERROR_U_LOC(loc,"Two (or more) matches for select statement, previous #%d, this #%d (%s).",match,matchno,#declname); \
    }                                        \
    match = matchno;                         \
  }                                          \
}

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define VERIFY_MATCH_DECL_QUICK(loc,match,matchno,declname,peek,mask,value,decltype,__VA_ARGS__...) { \
  if (((peek) & (mask)) == (value)) {                      \
    __data_src_t __tmp_buffer(__buffer);                   \
    if (decltype::__match(__tmp_buffer, ## __VA_ARGS__)) { \
      if (UNLIKELY(match)) {                               \
        ERROR_U_LOC(loc,"Two (or more) matches for select statement, previous #%d, this #%d (%s).",match,matchno,#declname); \
      }                                                    \
      match = matchno;                                     \
    }                                                      \
  }                                                        \
}
#else
#define VERIFY_MATCH_DECL_QUICK(loc,match,matchno,declname,peek,mask,value,decltype,...) { \
  if (((peek) & (mask)) == (value)) {                      \
    __data_src_t __tmp_buffer(__buffer);                   \
    if (decltype::__match(__tmp_buffer, ## __VA_ARGS__)) { \
      if (UNLIKELY(match)) {                               \
        ERROR_U_LOC(loc,"Two (or more) matches for select statement, previous #%d, this #%d (%s).",match,matchno,#declname); \
      }                                                    \
      match = matchno;                                     \
    }                                                      \
  }                                                        \
}
#endif

/*
#define MATCH_DECL_ARRAY(loc,match,matchindex,matcharray) { \
  match = matcharray[matchindex];                           \
  if (UNLIKELY(match == -1)) {                              \
    ERROR_U_LOC(loc,"Two matches for select statement.");   \
  }                                                         \
}
*/

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define UNPACK_DECL(loc,decltype,declname,__VA_ARGS__...) { \
  try {                         \
    (declname).__unpack(__buffer, ## __VA_ARGS__);  \
  } catch (error &e) {          \
    print_error(e);             \
    ERROR_U_LOC(loc,"Unpack failure in substructure %s %s.",#decltype,#declname); \
  }                             \
}
#else
#define UNPACK_DECL(loc,decltype,declname,...) { \
  try {                         \
    (declname).__unpack(__buffer, ## __VA_ARGS__);  \
  } catch (error &e) {          \
    print_error(e);             \
    ERROR_U_LOC(loc,"Unpack failure in substructure %s %s.",#decltype,#declname); \
  }                             \
}
#endif

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define CHECK_MATCH_DECL(loc,decltype,declname,__VA_ARGS__...) { \
  if (UNLIKELY(!decltype::__match(__buffer, ## __VA_ARGS__))) {  \
    ERROR_U_LOC(loc,"Match failure in substructure %s %s.",#decltype,#declname); \
  }                                                              \
}
#else
#define CHECK_MATCH_DECL(loc,decltype,declname,...) {           \
  if (UNLIKELY(!decltype::__match(__buffer, ## __VA_ARGS__))) { \
    ERROR_U_LOC(loc,"Match failure in substructure %s %s.",#decltype,#declname); \
  }                                                             \
}
#endif

#define UNPACK_CHECK_NO_REVISIT(loc,decltype,declname,visit_array,visit_index) { \
  if (UNLIKELY(visit_array.get_set(visit_index))) {                              \
    ERROR_U_LOC(loc,"Duplicate, substructure %s %s"                              \
                    " has already been visited.",#decltype,#declname);           \
  }                                                                              \
}

#ifdef USE_LMD_INPUT
#define VES10_1_ITEM(x) (((lmd_subevent_10_1_host *) __header)->x)

#define VES10_1_type     VES10_1_ITEM(_header.i_type)
#define VES10_1_subtype  VES10_1_ITEM(_header.i_subtype)
#define VES10_1_control  VES10_1_ITEM(h_control)
#define VES10_1_subcrate VES10_1_ITEM(h_subcrate)
#define VES10_1_procid   VES10_1_ITEM(i_procid)
#endif//USE_LMD_INPUT

#ifdef USE_HLD_INPUT
#define VES10_1_ITEM(x) (((hld_subevent_header *) __header)->x)

#define VES10_1_decode_type  VES10_1_ITEM(_deconde._type)
#define VES10_1_id           VES10_1_ITEM(_id)
#endif//USE_HLD_INPUT

#ifdef USE_PAX_INPUT
#define PAX_EV_HEADER_ITEM(x) (((pax_event_header *) __header)->x)

#define VES10_1_type     PAX_EV_HEADER_ITEM(_type)
#endif//USE_PAX_INPUT

#ifdef USE_RIDF_INPUT
#define VES10_1_ITEM(mask, bit) \
    ((mask & ((ridf_subevent_header *) __header)->_id) >> bit)

#define VES10_1_rev  VES10_1_ITEM(0xfc000000, 26)
#define VES10_1_dev  VES10_1_ITEM(0x03f00000, 20)
#define VES10_1_fp   VES10_1_ITEM(0x000fc000, 14)
#define VES10_1_det  VES10_1_ITEM(0x00003f00,  8)
#define VES10_1_mod  VES10_1_ITEM(0x000000ff,  0)
#endif//USE_RIDF_INPUT

#define UNPACK_SUBEVENT_CHECK_NO_REVISIT(loc,decltype,declname,visit_index) { \
  if (UNLIKELY(__visited.get_set(visit_index))) {                             \
    ERROR_U_LOC(loc,"Duplicate, subevent %s %s "                              \
                    " has already been visited.",#decltype,#declname);        \
  }                                                                           \
}

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define MATCH_SUBEVENT_DECL(loc,match,matchno,match_cond,decl,__VA_ARGS__...) { \
  /*__data_src_t __tmp_buffer(__buffer);*/     \
  if (match_cond) {                          \
  /*if ((decl).__match(__tmp_buffer, ## __VA_ARGS__)) {*/ \
    if (UNLIKELY(match)) {                   \
      ERROR_U_LOC(loc,"Two (or more) matches for subevent, previous #%d, this #%d.",match,matchno); \
    }                                        \
    match = matchno;                         \
  }                                          \
}
#else
#define MATCH_SUBEVENT_DECL(loc,match,matchno,match_cond,decl,...) { \
  /*__data_src_t __tmp_buffer(__buffer);*/     \
  if (match_cond) {                          \
  /*if ((decl).__match(__tmp_buffer, ## __VA_ARGS__)) {*/ \
    if (UNLIKELY(match)) {                   \
      ERROR_U_LOC(loc,"Two (or more) matches for subevent, previous #%d, this #%d.",match,matchno); \
    }                                        \
    match = matchno;                         \
  }                                          \
}
#endif

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define UNPACK_SUBEVENT_DECL(loc,sticky,decltype,declname,__VA_ARGS__...) { \
  try {                         \
    if (sticky) { (declname).__clean(); }		\
    (declname).__unpack(__buffer, ## __VA_ARGS__);	\
  } catch (error &e) {          \
    print_error(e);             \
    ERROR_U_LOC((loc),"Unpack failure in subevent %s %s.",#decltype,#declname); \
  }                             \
  return (loc);                 \
}
#else
#define UNPACK_SUBEVENT_DECL(loc,sticky,decltype,declname,...) {	\
  try {                         \
    if (sticky) { (declname).__clean(); }	    \
    (declname).__unpack(__buffer, ## __VA_ARGS__);  \
  } catch (error &e) {          \
    print_error(e);             \
    ERROR_U_LOC((loc),"Unpack failure in subevent %s %s.",#decltype,#declname); \
  }                             \
  return (loc);                 \
}
#endif

#if defined __GNUC__ && __GNUC__ < 3 // 2.95 do not do iso99 variadic macros
#define REVOKE_SUBEVENT_DECL(loc,sticky,decltype,declname,__VA_ARGS__...) { \
  if (sticky) { (declname).__clean(); }		\
  return (loc);                 \
}
#else
#define REVOKE_SUBEVENT_DECL(loc,sticky,decltype,declname,...) {	\
  if (sticky) { (declname).__clean(); }		\
  return (loc);                 \
}
#endif

#include "data_src_force_impl.hh"

#endif//__UCESBGEN_MACROS_HH__


