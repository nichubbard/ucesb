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

#include "correlation.hh"

#include "typedef.hh"

#include "structures.hh"

#include "bitsone.hh"

#include "raw_data.hh"
#include "raw_data_correlation.hh"

#include "event_base.hh"

#include "../common/strndup.hh"

#include "user.hh"

#include "corr_plot_dense.hh"
#include "corr_plot_dense2.hh"

#ifdef USER_CORRELATION_STRUCT_INCLUDE
#include USER_CORRELATION_STRUCT_INCLUDE
#endif

struct correlation_list
{
public:
  correlation_list()
  {
    _buffer = NULL;
  }

  ~correlation_list()
  {
    free(_buffer);
  }
  // dense_corr *_corr;

  int *_buffer;
  int *_cur;

public:
  void init(int n)
  {
    int *np = (int *) realloc(_buffer,sizeof(int) * (size_t) n);

    if (!np)
      ERROR("Memory allocation error.");

    _buffer = np;
  }

  void reset() { _cur = _buffer; }

public:
  void add(int index) { *(_cur++) = index; }
};

#define STRUCT_MIRROR_FCNS_DECL(name)           \
 public:                                        \
  bool enumerate_correlations(const signal_id &id,enumerate_correlations_info *info); \
  void add_corr_members(const class name &src,correlation_list *list WATCH_MEMBERS_PARAM) const; \
  template<typename T,typename T_map> \
  void add_corr_members(const multi_chunks<T,T_map> &src,correlation_list *list WATCH_MEMBERS_PARAM) const \
  { src.add_corr_members(*this,list WATCH_MEMBERS_ARG); }
#define STRUCT_MIRROR_TYPE(type)         type##_correlation
#define STRUCT_MIRROR_TYPE_TOGGLE(type)  STRUCT_MIRROR_TYPE(type)
#define STRUCT_MIRROR_NAME(name)         name
#define STRUCT_MIRROR_STRUCT(type)       STRUCT_MIRROR_TYPE(type)
#define STRUCT_MIRROR_BASE(type)         STRUCT_MIRROR_TYPE(type)
#define STRUCT_MIRROR_TEMPLATE_ARG(arg)  arg##_correlation,arg
#define STRUCT_MIRROR_TEMPLATE_ARG_N(arg,array)  arg##_correlation array,arg array
#define STRUCT_MIRROR_TEMPLATE_ARG_TOGGLE(arg)  arg##_correlation,TOGGLE(arg)
#define STRUCT_MIRROR_TEMPLATE_ARG_TOGGLE_N(arg,array)  arg##_correlation array,TOGGLE(arg) array
#define STRUCT_MIRROR_ITEM_CTRL_BASE(name) bool name##_active
#define STRUCT_MIRROR_ITEM_CTRL(name)      bool name##_active
#define STRUCT_MIRROR_ITEM_CTRL_ARRAY(name,non_last_index,last_index) bitsone<last_index> name##_active non_last_index
#define STRUCT_ONLY_LAST_UNION_MEMBER    1

#include "gen/struct_mirror.hh"
#include "gen/raw_struct_mirror.hh"
#include "gen/cal_struct_mirror.hh"
#ifdef USER_STRUCT
#include "gen/user_struct_mirror.hh"
#endif

#undef  STRUCT_MIRROR_FCNS_DECL
#undef  STRUCT_MIRROR_TYPE
#undef  STRUCT_MIRROR_TYPE_TOGGLE
#undef  STRUCT_MIRROR_NAME
#undef  STRUCT_MIRROR_STRUCT
#undef  STRUCT_MIRROR_BASE
#undef  STRUCT_MIRROR_TEMPLATE_ARG
#undef  STRUCT_MIRROR_TEMPLATE_ARG_N
#undef  STRUCT_MIRROR_TEMPLATE_ARG_TOGGLE
#undef  STRUCT_MIRROR_TEMPLATE_ARG_TOGGLE_N
#undef  STRUCT_MIRROR_ITEM_CTRL_BASE
#undef  STRUCT_MIRROR_ITEM_CTRL
#undef  STRUCT_MIRROR_ITEM_CTRL_ARRAY
#undef  STRUCT_ONLY_LAST_UNION_MEMBER

//#define APPEND_CORRELATION(x) x##_correlation
//#define XXX(x) x

//XXX(USER_STRUCT)
//USER_STRUCT


// correlation_event_info _event_info;
/*
template<typename T>
void correlation_channel_wrap<T>::event(DATA12 value)
{
  // _data.collect_raw((uint) value.value,_event_info._type);
}

template<typename T>
void correlation_channel_wrap<T>::event(DATA16 value)
{

}

template<typename T>
void correlation_channel_wrap<T>::event(DATA24 value)
{

}

template<typename T>
void correlation_channel_wrap<T>::event(DATA32 value)
{

}

template<typename T>
void correlation_channel_wrap<T>::event(float  value)
{

}
*/
/*
template<typename T>
void correlation_channel_wrap<T>::event(uint32 value)
{

}
*/

template<typename T>
bool corr_item_nonzero(const T &src)
{
  return src != 0;
}

bool corr_item_nonzero(const rawdata64 &src) { return src.value != 0; }
bool corr_item_nonzero(const rawdata32 &src) { return src.value != 0; }
bool corr_item_nonzero(const rawdata24 &src) { return src.value != 0; }
bool corr_item_nonzero(const rawdata16 &src) { return src.value != 0; }
bool corr_item_nonzero(const rawdata16plus &src) { return src.value != 0; }
bool corr_item_nonzero(const rawdata12 &src) { return src.value != 0; }
bool corr_item_nonzero(const rawdata14 &src) { return src.value != 0; }
bool corr_item_nonzero(const rawdata8  &src) { return src.value != 0; }

template<typename T>
void add_corr_members(const T &src,const data_correlation<T> &corr,correlation_list *list WATCH_MEMBERS_PARAM)
{
  // TODO: make sure the predication is perfect, such that we need not
  // check corr._index

  if (corr._index != -1)
    {
      if (corr_item_nonzero(src))
	list->add(corr._index);
    }
}

template<typename T>
void data_correlation<T>::add_corr_members(const T &src,correlation_list *list WATCH_MEMBERS_PARAM) const
{
  ::add_corr_members(src,*this,list WATCH_MEMBERS_ARG);
}

template<typename T>
void data_correlation<T>::add_corr_members(const toggle_item<T> &src,correlation_list *list WATCH_MEMBERS_PARAM) const
{
  ::add_corr_members(src._item,*this,list WATCH_MEMBERS_ARG);
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n>
void raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::add_corr_item(const T &src,const T_correlation &corr,correlation_list *list WATCH_MEMBERS_PARAM)
{
  const Tsingle *p_src     = (const Tsingle *) &src;
  const Tsingle_correlation *p = (const Tsingle_correlation *) &corr;

  for (size_t i = sizeof(T)/sizeof(Tsingle); i; --i, ++p_src, ++p)
    {
      // ::watch_members(*p_src,*p WATCH_MEMBERS_ARG);
      p->add_corr_members(*p_src,list WATCH_MEMBERS_ARG);
    }
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n>
void raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::add_corr_members(const raw_array<Tsingle,T,n> &src,correlation_list *list WATCH_MEMBERS_PARAM) const
{
  for (int i = 0; i < n; i++)
    {
      // ::watch_members(src._items[i],_items[i] WATCH_MEMBERS_ARG);
      add_corr_item(src._items[i],_items[i],list WATCH_MEMBERS_ARG);
    }
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n>
void raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::add_corr_members(const raw_array_zero_suppress<Tsingle,T,n> &src,correlation_list *list WATCH_MEMBERS_PARAM) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = src._valid.next(iter)) >= 0)
    {
      // ::watch_members(src._items[i],_items[i] WATCH_MEMBERS_ARG);
      add_corr_item(src._items[i],_items[i],list WATCH_MEMBERS_ARG);
    }
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n>
template<int max_entries>
void raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::add_corr_members(const raw_array_multi_zero_suppress<Tsingle,T,n,max_entries> &src,correlation_list *list WATCH_MEMBERS_PARAM) const
{
  bitsone_iterator iter;
  ssize_t i;

  while ((i = src._valid.next(iter)) >= 0)
    {
      // ::watch_members(src._items[i],_items[i] WATCH_MEMBERS_ARG);
      for (uint j = 0; j < src._num_entries[i]; j++)
	add_corr_item(src._items[i][j],_items[i],list WATCH_MEMBERS_ARG);
    }
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n>
void raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::add_corr_members(const raw_list_zero_suppress<Tsingle,T,n> &src,correlation_list *list WATCH_MEMBERS_PARAM) const
{
  for (unsigned int i = 0; i < src._num_items; i++)
    {
      // ::watch_members(src._items[i]._item,_items[_items[i]._index] WATCH_MEMBERS_ARG);
      add_corr_item(src._items[i]._item,_items[src._items[i]._index],list WATCH_MEMBERS_ARG);
    }
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n>
void raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::add_corr_members(const raw_list_ii_zero_suppress<Tsingle,T,n> &src,correlation_list *list WATCH_MEMBERS_PARAM) const
{
  for (unsigned int i = 0; i < src._num_items; i++)
    {
      // ::watch_members(src._items[i]._item,_items[_items[i]._index] WATCH_MEMBERS_ARG);
      add_corr_item(src._items[i],_items[i],list WATCH_MEMBERS_ARG);
    }
}

#define FCNCALL_CLASS_NAME(name) name##_correlation
#define FCNCALL_NAME(name) \
  add_corr_members(const name &__src,correlation_list *list WATCH_MEMBERS_PARAM) const
#define FCNCALL_CALL_BASE() add_corr_members(__src,list WATCH_MEMBERS_ARG)
#define FCNCALL_CALL(member) add_corr_members(__src.member,list WATCH_MEMBERS_ARG) // show_members(signal_id(id,#member))
#define FCNCALL_CALL_TYPE(type,member) ::add_corr_members<type>(__src.member,member,list WATCH_MEMBERS_ARG) // ::show_members<type>(signal_id(id,#member))
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) ;
#define FCNCALL_SUBNAME(name)   ;
//#define FCNCALL_SUBINDEX(index) const signal_id &shadow_id = id; signal_id id(shadow_id,index);
//#define FCNCALL_SUBNAME(name)   const signal_id &shadow_id = id; signal_id id(shadow_id,name);
#define FCNCALL_MULTI_MEMBER(name) name
#define FCNCALL_MULTI_ARG(name) multi_##name
#define FCNCALL_CALL_CTRL_WRAP(ctrl,call) { if (ctrl##_active || 1) { call; } }
#define FCNCALL_CALL_CTRL_WRAP_ARRAY(ctrl_name,ctrl_non_last_index,ctrl_last_index,call) (call)
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/struct_fcncall.hh"
#include "gen/raw_struct_fcncall.hh"
#include "gen/cal_struct_fcncall.hh"
#ifdef USER_STRUCT
#include "gen/user_struct_fcncall.hh"
#endif

#undef  FCNCALL_CLASS_NAME
#undef  FCNCALL_NAME
#undef  FCNCALL_CALL_BASE
#undef  FCNCALL_CALL
#undef  FCNCALL_CALL_TYPE
#undef  FCNCALL_FOR
#undef  FCNCALL_SUBINDEX
#undef  FCNCALL_SUBNAME
#undef  FCNCALL_MULTI_MEMBER
#undef  FCNCALL_MULTI_ARG
#undef  FCNCALL_CALL_CTRL_WRAP
#undef  FCNCALL_CALL_CTRL_WRAP_ARRAY
#undef STRUCT_ONLY_LAST_UNION_MEMBER


typedef std::vector<correlation_list *> correlation_list_vect;

struct correlation_plot
{

public:
  const char *_command; // for info
  const char *_filename;

public:
  correlation_list_vect _lists;
  dense_corr            *_corr;
  dense_corr2           *_corr2;

  int  _list_n;
  int  _list_i;
  bool _wrapped_i;

  bool  _need_sort; // list may be unsorted

public:
  unpack_event_correlation *_unpack_event_correlation;
  raw_event_correlation    *_raw_event_correlation;
  //cal_event_correlation    *_cal_event_correlation;
#ifdef USER_STRUCT
  //APPEND_CORRELATION()   the_user_event_correlation;
#endif
};

#ifndef USE_MERGING
void correlation_one_event(correlation_plot *plot WATCH_MEMBERS_PARAM)
{
  correlation_list *list_i;   // The list that we fill this time
  correlation_list *list_old; // The oldest list (that we correlate against)
  // If there is only one list, list_i and list_i_old will be the same

  list_i = plot->_lists[(size_t) plot->_list_i];

  plot->_list_i++;
  if (plot->_list_i >= plot->_list_n)
    {
      plot->_list_i    = 0;
      plot->_wrapped_i = true;
    }

  list_old = plot->_lists[(size_t) plot->_list_i];

  /*
  memset(&_event_info,0,sizeof(_event_info));

#if defined(CORRELATION_EVENT_INFO_USER_FUNCTION)
  CORRELATION_EVENT_INFO_USER_FUNCTION(&_event_info,
				       &_static_event._unpack);
#endif
  */

  list_i->reset();

  //printf ("c1 %x %x\n",
  //	  (int) (size_t) plot->_unpack_event_correlation,
  //	  (int) (size_t) plot->_raw_event_correlation);

  if (plot->_unpack_event_correlation)
    plot->_unpack_event_correlation->
      /**/add_corr_members(_static_event._unpack,
			   list_i WATCH_MEMBERS_ARG);
  if (plot->_raw_event_correlation)
    plot->_raw_event_correlation->
      /**/add_corr_members(_static_event._raw,
			   list_i WATCH_MEMBERS_ARG);

  //the_cal_event_correlation   .watch_members(_event._cal   ,list);
#ifdef USER_STRUCT
  //the_user_event_correlation  .watch_members(_event._user  ,list);
#endif

  // sort the entries in the list.  (in principle, this we should only
  // do if the list may be unsorted, i.e. items might have been added
  // in dis-order)

  if (plot->_need_sort)
    {
      // TODO: consider using radix sort
      // (most useful events for plot have a small number of
      // correlations however)
      // printf ("%d ",plot->_list->_cur - plot->_list->_buffer);
      qsort(list_i->_buffer,
	    (size_t) (list_i->_cur - list_i->_buffer),
	    sizeof(int),compare_values<int>);
    }

  // printf ("c2 %d\n",plot->_list->_cur - plot->_list->_buffer);

  // Only use the lists for filling if we have wrapped.
  // Happens immediately for the one-list case.
  if (plot->_wrapped_i)
    {
      if (plot->_corr)
	plot->_corr->add_list_diag(list_i->_buffer,
				   list_i->_cur);
      if (plot->_corr2)
	plot->_corr2->add_lists_two(list_i->_buffer,
				    list_i->_cur,
				    list_old->_buffer,
				    list_old->_cur);
    }

  (void) list_old;

  // printf (".%d",plot->_list->_cur - plot->_list->_buffer);
}
#endif//!USE_MERGING










template<typename T>
bool enumerate_correlations(data_correlation<T> &correlation,const signal_id &id,enumerate_correlations_info *info)
{
  if (!info->_requests->is_channel_requested(id,false,0,false) ||
      correlation._index != -1)
    return correlation._index != -1; // we were perhaps selected before?

  correlation._index = info->_next_index;
  info->_next_index++;
  /*
  char name[256];
  {
    signal_id id_fmt(id);
    id_fmt.format_paw(name,sizeof(name));
  }
  */

  return true;
}

template<typename T>
bool data_correlation<T>::enumerate_correlations(const signal_id &id,enumerate_correlations_info *info)
{
  // ::watch_members(src,_items[i] WATCH_MEMBERS_ARG);
  return ::enumerate_correlations(*this,id,info);
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n>
bool raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::enumerate_correlations(const signal_id &id,
										  enumerate_correlations_info *info)
{
  bool active = false;

  for (int i = 0; i < n; i++)
    active |= _items[i].enumerate_correlations(signal_id(id,i),info);

  return active;
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n,int n1>
bool raw_array_correlation_1<Tsingle_correlation,Tsingle,T_correlation,T,n,n1>::enumerate_correlations(const signal_id &id,
										       enumerate_correlations_info *info)
{
  bool active = false;

  for (int i = 0; i < n; i++)
    for (int i1 = 0; i1 < n1; ++i1)
      active |= (raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::_items[i])[i1].
	enumerate_correlations(signal_id(signal_id(id,i),i1),info);

  return active;
}

template<typename Tsingle_correlation,typename Tsingle,typename T_correlation,typename T,int n,int n1,int n2>
bool raw_array_correlation_2<Tsingle_correlation,Tsingle,T_correlation,T,n,n1,n2>::enumerate_correlations(const signal_id &id,
										       enumerate_correlations_info *info)
{
  bool active = false;

  for (int i = 0; i < n; i++)
    for (int i1 = 0; i1 < n1; ++i1)
      for (int i2 = 0; i2 < n2; ++i2)
	active |= (raw_array_correlation<Tsingle_correlation,Tsingle,T_correlation,T,n>::_items[i])[i1][i2].
	  enumerate_correlations(signal_id(signal_id(signal_id(id,i),i1),i2),info);

  return active;
}




#define FCNCALL_CLASS_NAME(name) name##_correlation
#define FCNCALL_NAME(name) \
  enumerate_correlations(const signal_id &__id,enumerate_correlations_info *__info)
#define FCNCALL_CALL_BASE() enumerate_correlations(__id,__info)
#define FCNCALL_CALL(member) enumerate_correlations(__id,__info)
#define FCNCALL_CALL_TYPE(type,member) ::enumerate_correlations<type>(member,__id,__info)
#define FCNCALL_FOR(index,size) for (int index = 0; index < size; ++index)
#define FCNCALL_SUBINDEX(index) const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,index); bool &__shadow_active = __active; bool __active = false;
#define FCNCALL_SUBINDEX_END(index) __shadow_active |= __active;
#define FCNCALL_SUBNAME(name)   const signal_id &__shadow_id = __id; signal_id __id(__shadow_id,name);
//#define FCNCALL_SUBINDEX(index) const signal_id &shadow_id = id; signal_id id(shadow_id,index);
//#define FCNCALL_SUBNAME(name)   const signal_id &shadow_id = id; signal_id id(shadow_id,name);
#define FCNCALL_MULTI_MEMBER(name) name
#define FCNCALL_MULTI_ARG(name) name
#define FCNCALL_CALL_CTRL_WRAP(ctrl,call) ctrl##_active = (call)
#define FCNCALL_CALL_CTRL_WRAP_ARRAY(ctrl_name,ctrl_non_last_index,ctrl_last_index,call) __active |= ((call) ? (ctrl_name##_active ctrl_non_last_index.set((unsigned int) ctrl_last_index)) , true : false)
#define FCNCALL_RET_TYPE bool
#define FCNCALL_INIT     bool __active = false;
#define FCNCALL_RET      return __active;
#define STRUCT_ONLY_LAST_UNION_MEMBER 1

#include "gen/struct_fcncall.hh"
#include "gen/raw_struct_fcncall.hh"
#include "gen/cal_struct_fcncall.hh"
#ifdef USER_STRUCT
#include "gen/user_struct_fcncall.hh"
#endif

#undef  FCNCALL_CLASS_NAME
#undef  FCNCALL_NAME
#undef  FCNCALL_CALL_BASE
#undef  FCNCALL_CALL
#undef  FCNCALL_CALL_TYPE
#undef  FCNCALL_FOR
#undef  FCNCALL_SUBINDEX
#undef  FCNCALL_SUBNAME
#undef  FCNCALL_MULTI_MEMBER
#undef  FCNCALL_MULTI_ARG
#undef  FCNCALL_RET_TYPE
#undef  FCNCALL_INIT
#undef  FCNCALL_RET
#undef  FCNCALL_CALL_CTRL_WRAP
#undef  FCNCALL_CALL_CTRL_WRAP_ARRAY
#undef STRUCT_ONLY_LAST_UNION_MEMBER



typedef std::vector<correlation_plot *> correlation_plot_vect;

correlation_plot_vect _correlation_plots;

void correlation_usage()
{
  printf ("\n");
  printf ("Correlation (--corr) options:\n");
  printf ("\n");
  printf ("Use , to separate detectors; use : to start new group.\n");
  printf ("\n");
  printf ("mix=N               Correlate between events (1=self, 2=next, ...).\n");
  printf ("2d                  Make full square plot also for mix=1.\n");
  printf ("det=NAME            In case detector name collides with option.\n");
  printf ("\n");
}

correlation_plot *correlation_init(const char *command)
{
  correlation_plot *cp = new correlation_plot;

  cp->_command = command; // just for debugging...
  cp->_filename = NULL;
  cp->_list_n = 1;
  cp->_list_i = 0;
  cp->_wrapped_i = false;
  cp->_corr  = NULL;
  cp->_corr2 = NULL;
  cp->_need_sort = false; // must be set if we call the enumerate
			  // functions more than once!

  cp->_unpack_event_correlation = NULL;
  cp->_raw_event_correlation = NULL;

  bool corr2_plot = false;

  enumerate_correlations_info info;

  info._next_index = 0;

  for ( ; ; )
    {
      detector_requests requests;

      const char *req_end;

      while ((req_end = strpbrk(command,",:")) != NULL)
	{
	  char *request = strndup(command,(size_t) (req_end-command));

	  char *post;

#define MATCH_C_PREFIX(prefix,post) (strncmp(request,prefix,strlen(prefix)) == 0 && *(post = request + strlen(prefix)) != '\0')
#define MATCH_ARG(name) (strcmp(request,name) == 0)

	  // printf ("Request: %s\n",request);
	  /*
	    if (MATCH_ARG("UNPACK"))
	    request_level |= NTUPLE_WRITER_UNPACK;
	    else if (MATCH_ARG("RAW"))
	    request_level |= NTUPLE_WRITER_RAW;
	    else if (MATCH_ARG("CAL"))
	    request_level |= NTUPLE_WRITER_CAL;
	    else if (MATCH_ARG("USER"))
	    request_level |= NTUPLE_WRITER_USER;
	    else if (MATCH_ARG("RWN"))
	    _paw_ntuple._ntuple_type = NTUPLE_TYPE_RWN;
	    else if (MATCH_ARG("CWN"))
	    _paw_ntuple._ntuple_type = NTUPLE_TYPE_CWN;
	    else
	  */
	  if (MATCH_ARG("help"))
	    {
	      correlation_usage();
	      exit(0);
	    }
	  else if (MATCH_C_PREFIX("mix=",post))
	    cp->_list_n = atoi(post);
	  else if (MATCH_ARG("2d"))
	    corr2_plot = true;
	  else if (MATCH_C_PREFIX("DET=",post) ||
		   MATCH_C_PREFIX("det=",post) ||
		   (post = request))
	    {
	      requests.add_detector_request(post,0);
	    }

	  free(request);
	  command = req_end+1;

	  if (*req_end == ':')
	    {
	      // Since we'll run over the data more than once, we may
	      // need sorting...
	      cp->_need_sort = true;
	      break;
	    }
	}

      requests.prepare();

      info._requests = &requests;

      if (!cp->_unpack_event_correlation)
	cp->_unpack_event_correlation = new unpack_event_correlation;
      if (!cp->_raw_event_correlation)
	cp->_raw_event_correlation = new raw_event_correlation;

      if (!cp->_unpack_event_correlation->
	  /**/enumerate_correlations(signal_id(),&info) && 0)
	{
	  delete cp->_unpack_event_correlation;
	  cp->_unpack_event_correlation = NULL;
	}
      if (!cp->_raw_event_correlation->
	  /**/enumerate_correlations(signal_id(),&info) && 0)
	{
	  delete cp->_raw_event_correlation;
	  cp->_raw_event_correlation = NULL;
	}
      //the_cal_event_correlation   .enumerate_correlations(signal_id(),info);
#ifdef USER_STRUCT
      //the_user_event_correlation  .watch_members(_event._user   CORRELATION_MEMBERS_ARG);
#endif

      for (uint i = 0; i < requests._requests.size(); i++)
	if (!requests._requests[i]._checked)
	  ERROR("Correlation request for item %s was not considered.  "
		"Does that detector exist?",
		requests._requests[i]._str);

      if (!req_end)
	break;
    }

  if (strcmp(command,"help") == 0)
    {
      correlation_usage();
      exit(0);
    }

  if (cp->_list_n < 1)
    ERROR("Correlation plot must have mix (%d) >= 1.\n", cp->_list_n);

  if (cp->_list_n > 1)
    corr2_plot = true;

  cp->_filename = command;

  // now, how many items did we get?

  int n = info._next_index;

  if (!corr2_plot)
    {
      cp->_corr = new dense_corr();
      cp->_corr->clear((size_t) n);
    }
  else
    {
      cp->_corr2 = new dense_corr2();
      cp->_corr2->clear((size_t) n);
    }

  for (int i = 0; i < cp->_list_n; i++)
    {
      correlation_list *list = new correlation_list;

      list->init(n);

      cp->_lists.push_back(list);
    }

  return cp;
}

void correlation_init(const config_command_vect &commands)
{
  config_command_vect::const_iterator iter;

  for (iter = commands.begin(); iter != commands.end(); ++iter)
    {
      const char *command = *iter;

      correlation_plot *cp = correlation_init(command);

      _correlation_plots.push_back(cp);
    }
}


#ifndef USE_MERGING
void correlation_event(unpack_event *unpack_ev
		       WATCH_MEMBERS_PARAM)
{
  (void) unpack_ev;

#if defined(CORRELATION_EVENT_INFO_USER_FUNCTION)
  if (!CORRELATION_EVENT_INFO_USER_FUNCTION(&_static_event._unpack))
    return;
#endif

  correlation_plot_vect::iterator iter;

  for (iter = _correlation_plots.begin();
       iter != _correlation_plots.end(); ++iter)
    {
      correlation_plot *cp = *iter;

      correlation_one_event(cp WATCH_MEMBERS_ARG);
    }
}

void correlation_event(unpack_sticky_event *unpack_ev
		       WATCH_MEMBERS_PARAM)
{
  (void) unpack_ev;
}
#endif//!USE_MERGING

void correlation_exit()
{
  correlation_plot_vect::iterator iter;

  for (iter = _correlation_plots.begin();
       iter != _correlation_plots.end(); ++iter)
    {
      correlation_plot *cp = *iter;

      if (cp->_corr)
	cp->_corr->picture(cp->_filename);
      if (cp->_corr2)
	cp->_corr2->picture(cp->_filename);
   }
}

