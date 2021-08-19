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

/* External program for writing root files (trees).  (Or hbook ntuples).
 *
 * Kept very simple.  External for two reasons: a) not having to
 * make root/cernlib compile into the main executable.  b) file writing
 * gets a process of it's own.
 */

/* Communication:
 *
 * 0 (stdin):  one-byte commands/signals on stdin from calling process
 * 1 (stdout): one-byte responses/signals on stdout to calling process
 * 2 (stderr): as usual, where complaints go
 * 3 : file handle to a file descriptor for a shared memory file
 *     used as circular buffer.
 */

/* Shared memory performance:
 *
 * Testing shows that it actually is only marginally faster than plain
 * pipe communication.  Example:
 *
 * file_input/empty_file --lmd | empty/empty \
 *   --file=- --ntuple=UNPACK,STRUCT,NOSHM,dummy
 * hbook/struct_writer localhost --server=20000
 *
 * Gives ~9000 kHz on zxc (AMD Athlon(tm) II X3 455), while
 * without NOSHM (i.e. with SHM), we get ~5000 kHz.
 *
 * Test with more payload, i.e. land/land unpacker:
 *
 * file_input/empty_file --lmd | taskset -c 0,1,2 land/land \
 *   --file=- --ntuple=UNPACK,RAW,STRUCT,NOSHM,dummy
 * taskset -c 3 hbook/struct_writer localhost --server=20000
 *
 * Gives ~570 kHz on bridge2 (Xeon(R) CPU E31245 @ 3.30GHz), while
 * without NOSHM (i.e. with SHM), we get ~570 kHz.
 *
 * With no apparent advantage, it may make sense to rip it out, as
 * it just complicates the code.
 */

#define DO_EXT_NET_DECL
#include "ext_file_writer.hh"

#define __STDC_FORMAT_MACROS

#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <vector>
#include <map>
#include <set>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

#include "array_heap.h"

#ifndef BUILD_LAND02
#include "../common/strndup.hh"
#endif

const char *_argv0;

#ifdef BUILD_LAND02
#include "../optimise.hh"
#else
#include "../eventloop/optimise.hh"
#endif

#ifdef BUILD_LAND02
#include "../../lu_common/colourtext.cc"
#else
#include "../lu_common/colourtext.cc"
#endif

ext_write_config _config;

#include "ext_file_error.hh"

#if USING_CERNLIB
#define WARNING       MSG
#define ERROR     ERR_MSG
#define DONT_INCLUDE_ERROR_HH    // we provide that ourselves
#include "hbook.cc"
#endif
#if USING_ROOT
#include "TFile.h"
#include "TKey.h"
#include "TTree.h"
#include "TBranch.h"
#include "TLeaf.h"
#include "TH1.h"
#if ROOT_HAS_TTREE_COMPRESS_THREADS
#include "TTreeCompressThreads.h"
#endif
#endif
// to get at ext_data_write_event
#include "ext_data_client.c"

const char *get_buf_string(void **msg,uint32_t *left)
{
  external_writer_buf_string *str =
    (external_writer_buf_string *) *msg;

  if (sizeof(*str) > *left)
    ERR_MSG("SHM message has no space for string header.");

  uint32_t length = ntohl(str->_length);

  uint32_t advance = sizeof(*str) + (((length + 1) + 3) & ~3);

  if (advance > *left)
    ERR_MSG("SHM message has no space for string.");

  if (str->_string[length])
    ERR_MSG("SHM message string not zero-terminated.");

  *msg   = ((char*) str) + advance;
  *left -= advance;

  return str->_string;
}

uint32_t get_buf_uint32(void **msg,uint32_t *left)
{
  uint32_t *p =
    (uint32_t *) *msg;

  if (sizeof(*p) > *left)
    ERR_MSG("SHM message has no space for uint32.");

  uint32_t advance = sizeof(*p);

  *msg   = ((char*) p) + advance;
  *left -= advance;

  return ntohl(*p);
}

float get_buf_float(void **msg,uint32_t *left)
{
  union
  {
    float    _f;
    uint32_t _i;
  } value;

  value._i = get_buf_uint32(msg,left);
  return value._f;
}

uint32_t *get_buf_raw_ptr(void **msg,uint32_t *left,uint32_t u32_words)
{
  uint32_t *p =
    (uint32_t *) *msg;

  uint32_t advance = u32_words * sizeof(uint32_t);

  if (advance > *left)
    ERR_MSG("SHM message has no space for %d*uint32.", u32_words);

  *msg   = ((char*) p) + advance;
  *left -= advance;

  return p;
}

typedef ext_data_structure_item stage_array_item;

typedef std::map<uint32_t,stage_array_item> stage_array_item_map;
typedef std::vector<stage_array_item> stage_array_item_vector;

template<class T>
struct compare_ptrs_strcmp
{
  bool operator()(const T& s1,const T& s2) const
  {
    return strcmp(s1,s2) < 0;
  }
};

typedef std::map<const char *,uint32_t,
		 compare_ptrs_strcmp<const char*> > stage_array_name_map;

#define BUCKET_SORT_LEVELS  3
#define BUCKET_SORT_SHIFT   5
#define BUCKET_SORT_BITS   32 // (1 << BUCKET_SORT_SHIFT)

struct stage_array
{
  size_t _length;
  char*  _ptr;

  // char*  _merge_ptr;

#if STRUCT_WRITER
  uint32_t *_offset_value;

  int       _rewrite_max_bytes_per_item;

  uint32_t *_masks[BUCKET_SORT_LEVELS];
  int       _num_masks[BUCKET_SORT_LEVELS];

  stage_array_item_map _items;
  stage_array_name_map _names;
#else
  stage_array_item_vector _items_v;
#endif
};

#if USING_ROOT
typedef std::vector<TTree *> TTree_vector;
#endif

struct global_once
{
  uint32_t          _sort_u32_words;

  uint32_t          _max_offset_array_items;

#if USING_CERNLIB
  hbook            *_hfile;
#endif
#if USING_ROOT
#if ROOT_HAS_TTREE_COMPRESS_THREADS
  TTreeCompressThreads *_root_compress_threads;
#endif
  TFile            *_root_file;
  uint64_t          _num_read_entries;
#endif
  
  uint64_t          _num_events;
  uint64_t          _num_events_total;
  int               _num_hists;

public:
  global_once()
  {
    _sort_u32_words = (uint32_t) -1;
    _max_offset_array_items = 0;
#if USING_CERNLIB
    _hfile = NULL;
#endif
#if USING_ROOT
#if ROOT_HAS_TTREE_COMPRESS_THREADS
    _root_compress_threads = NULL;
#endif
    _root_file = NULL;
    _num_read_entries = 0;
#endif
    _num_events = 0;
    _num_events_total = 0;
    _num_hists = 0;
  }
};

global_once _g;

struct global_struct
{
  uint32_t    _hid;
  const char *_id;
  const char *_title;

  uint32_t    _max_raw_words;

#if USING_CERNLIB
  hbook_ntuple_cwn *_cwn;
#endif
#if USING_ROOT
  TTree_vector      _root_ntuples;
  TTree            *_root_ntuple;
#endif

  stage_array _stage_array;

  offset_array _offset_array;

  uint32_t _xor_sum;

  const char *_index_major;
  const char *_index_minor;

public:
  global_struct()
  {
    _hid = 0;
    _id = NULL;
    _title = NULL;
    _max_raw_words = 0;
#if USING_CERNLIB
    _cwn = NULL;
#endif
#if USING_ROOT
    /*_root_ntuples */;
    _root_ntuple = NULL;
#endif
    /* Cannot memset _stage_array, has non-POD objects. */
    _stage_array._length = 0;
    _stage_array._ptr = NULL;
    memset(&_offset_array, 0, sizeof (_offset_array));
    _xor_sum = 0;

    _index_major = NULL;
    _index_minor = NULL;
  }
};

typedef std::vector<global_struct *> global_struct_vector;

global_struct_vector _structures;
global_struct   *_cur_structure = NULL; /* the structure being set up */
global_struct  *_read_structure = NULL;

#if USING_CERNLIB || USING_ROOT
struct timeslice
{
  time_t _last_slicetime;
  time_t _last_autosave;
  int _autosave_lock_fail;
  int _autosave_lock_do;

  struct timeslice_name
  {
    const char *_dir;  // end with '/'
    const char *_base;
    const char *_ext;  // begin with '.'

    char *_curname;

    int   _list_fd;
  } _timeslice_name;

public:
  timeslice()
  {
    _last_slicetime = 0;
    _last_autosave = 0;
    _autosave_lock_fail = 0;
    _autosave_lock_do = 1;
  }
};

timeslice _ts;
#endif


void do_file_open(time_t slicetime)
{
#if USING_CERNLIB || USING_ROOT
  if (!_config._ftitle)
    _config._ftitle = "ext_writer";
  if (_config._outfile)
    {
      const char *filename = _config._outfile;
      char tmptm[32];

      if (_config._timeslice)
	{
	  _ts._last_slicetime = slicetime;

	  struct tm tmr;

	  _ts._timeslice_name._curname =
	    (char *) malloc(strlen(_ts._timeslice_name._dir)+32+   // date+'/'
			    strlen(_ts._timeslice_name._base)+32+  // '_'+date
			    strlen(_ts._timeslice_name._ext)+1+1); // '\n'+'\0'

	  if (!_ts._timeslice_name._curname)
	    ERR_MSG("Failure allocating string.");

	  strcpy(_ts._timeslice_name._curname,_ts._timeslice_name._dir);

	  if (_config._timeslice_subdir)
	    {
	      time_t dir_time =
		slicetime - slicetime % _config._timeslice_subdir;

	      gmtime_r(&dir_time,&tmr);
	      strftime(tmptm,sizeof(tmptm),"%Y%m%d_%H%M%S",&tmr);

	      strcat(_ts._timeslice_name._curname,tmptm);

	      // Let's try to always create the directory, e.g.
	      // in case the user helped by removing them...

	      int ret = mkdir(_ts._timeslice_name._curname,
			      S_IRWXU | S_IRWXG | S_IRWXO);

	      if (ret != 0 &&
		  errno != EEXIST)
		{
		  perror("mkdir");
		  ERR_MSG("Error making timeslice directory.");
		}
	      strcat(_ts._timeslice_name._curname,"/");
	    }

	  gmtime_r(&slicetime,&tmr);
	  strftime(tmptm,sizeof(tmptm),"%Y%m%d_%H%M%S",&tmr);

	  strcat(_ts._timeslice_name._curname,_ts._timeslice_name._base);
	  strcat(_ts._timeslice_name._curname,"_");
	  strcat(_ts._timeslice_name._curname,tmptm);
	  strcat(_ts._timeslice_name._curname,_ts._timeslice_name._ext);

	  filename = _ts._timeslice_name._curname;
	}

#if USING_CERNLIB
      if (strlen(_config._ftitle) > 16)
	ERR_MSG("HBOOK file title (%s) too long (max 16 chars).",
		_config._ftitle);
      _g._hfile = new hbook;
      _g._hfile->open(filename,_config._ftitle);
#endif
#if USING_ROOT
#if ROOT_HAS_TTREE_COMPRESS_THREADS
      if (!_g._root_compress_threads)
	{
	  // Count logical cores we are allowed to run on.
	  _g._root_compress_threads = new TTreeCompressThreads(5);
	}
#endif
      _g._root_file = new TFile(filename,"RECREATE",_config._ftitle);
      if (_g._root_file->IsZombie())
	ERR_MSG("Failure opening TFile %s.",filename);
#endif
      MSG("Opened %s (%s).",filename,_config._ftitle);
    }
  if (_config._infile)
    {
#if USING_CERNLIB
      ERR_MSG("HBOOK reading not implemented yet.");
#endif
#if USING_ROOT
      _g._root_file = new TFile(_config._infile,"READ");
      if (_g._root_file->IsZombie())
	ERR_MSG("Failure opening TFile %s.",_config._infile);
      MSG("Opened %s (%s).",_config._infile,_g._root_file->GetTitle());
#endif
    }
#endif
}

void request_file_open(void *msg,uint32_t *left)
{
  /* magic = */ get_buf_uint32(&msg,left);

  uint32_t sort_u32_words = get_buf_uint32(&msg,left);

  //_server_port = (int32_t) get_buf_uint32(&msg,left);

  //uint32_t generate_header = get_buf_uint32(&msg,left);

  //const char *filename = get_buf_string(&msg,left);
  //const char *ftitle = get_buf_string(&msg,left);

  if (_g._sort_u32_words != sort_u32_words &&
      _g._sort_u32_words != (uint32_t) -1)
    ERR_MSG("All sources must specify "
	    "the same number of sort words.  (got %d, had %d)",
	    sort_u32_words, _g._sort_u32_words);
  
  _g._sort_u32_words = sort_u32_words;

  do_file_open(time(NULL));
}

void do_book_ntuple(global_struct *s, uint32_t ntuple_index)
{
  uint32_t hid      = s->_hid;
  const char *id    = s->_id;
  const char *title = s->_title;

#if USING_CERNLIB || USING_ROOT
  if (_config._title)
    title = _config._title;
#if USING_CERNLIB
  if (!_g._hfile)
    ERR_MSG("Refusing to book a ntuple without an open file.");

  if (_config._id)
    {
      char *end;

      hid = strtol(_config._id,&end,10);

      if (end == _config._id || *end != 0)
	ERROR("HBOOK ntuple ID (%s) must be numeric.",_config._id);
    }
  if (strlen(title) > 255)
    ERROR("HBOOK ntuple title (%s) too long (max 255 chars).",
	  title);

  s->_cwn = new hbook_ntuple_cwn();
  s->_cwn->hbset_bsize(4096);
  s->_cwn->hbnt(hid,title," ");

  MSG("Booked %d : %s",hid,title);
#endif
#if USING_ROOT
  if (_config._id)
    id = _config._id;
  if (_config._outfile)
    {
      if (!_g._root_file)
	ERR_MSG("Refusing to book a ntuple(tree) without an open file.");

      if (ntuple_index != s->_root_ntuples.size() &&
	  !(ntuple_index == (uint32_t) -1 &&
	    s->_root_ntuples.size() == 1))
	ERR_MSG("Ntuple index (%d) out of order (!= %d).",
		ntuple_index,(int) s->_root_ntuples.size());
      if (_config._timeslice)
	if (ntuple_index != 0 && ntuple_index != (uint32_t) -1)
	  ERR_MSG("(Multiple) Ntuple index (%d) != 0 "
		  "not allowed with time-slicing.",
		  ntuple_index);

      TTree *root_ntuple = new TTree(id,title);
#if ROOT_HAS_TTREE_COMPRESS_THREADS
      if (_g._root_compress_threads)
	root_ntuple->SetCompressThreads(_g._root_compress_threads);
#endif

      root_ntuple->SetAutoSave(0);
      // SetAutoFlush seems not to be available everywhere.
      // In principle we do not want it, except to work around
      // broken OptimizeBaskets...
#if ROOT_VERSION_CODE >= ROOT_VERSION(5,26,0)
      root_ntuple->SetAutoFlush(0);
#endif

      if (ntuple_index == (uint32_t) -1)
	s->_root_ntuples[0] = root_ntuple;
      else
	s->_root_ntuples.push_back(root_ntuple);

      if (s->_root_ntuples.size() < 10)
	MSG("Tree %s (%s)",id,title);
      else if (s->_root_ntuples.size() == 10)
	MSG("(Further tree names suppressed...)");
    }
  else
    {
      if (!_g._root_file)
	ERR_MSG("Cannot get a ntuple(tree) without an open file.");

      TObject *obj_id = _g._root_file->Get(id);

      s->_root_ntuple = dynamic_cast<TTree*>(obj_id);

      if (!s->_root_ntuple)
	{
	  if (obj_id)
	    ERR_MSG("Found object with key (%s), but is not a tree.", id);

	  /* Search the file.  If there is only one tree, use that! */

	  TIter iter(_g._root_file->GetListOfKeys());
	  TKey *key;
	  while ((key = (TKey*) iter()))
	    {
	      if (strcmp(key->GetClassName(), "TTree") != 0)
		continue;
	      TString keyname = key->GetName();

	      if (s->_root_ntuple)
		ERR_MSG("Multiple trees in file, "
			"but none with expected key (%s)", id);

	      WARN_MSG("Tree with key (%s) not found, "
		       "using tree with key (%s).",
		       id, (const char *) keyname);

	      obj_id = _g._root_file->Get(keyname);

	      s->_root_ntuple = dynamic_cast<TTree*>(obj_id);

	      if (!s->_root_ntuple)
		ERR_MSG("Strange, item with key (%s) claimed to be a TTree, "
			"but cast failed.", id);
	    }
	  if (!s->_root_ntuple)
	    {
	      MSG("Tree with key (%s) not found in file, "
		  "and no other trees either!", id);
	    }
	}

  /* Find all TNamed objects in file with a key name ending in
   * "-describe".  To be propagated to the output file.
   */


      _g._num_read_entries = s->_root_ntuple->GetEntries();

      MSG("Got tree %s (%s), %" PRIu64 " entries.",
	  s->_root_ntuple->GetName(),s->_root_ntuple->GetTitle(),
	  _g._num_read_entries);
    }
#endif
#endif
}

void request_book_ntuple(void *msg,uint32_t *left)
{
  uint32_t struct_index = get_buf_uint32(&msg,left);
  uint32_t ntuple_index = get_buf_uint32(&msg,left);
  uint32_t hid = get_buf_uint32(&msg,left);
  uint32_t max_raw_words =
    (ntuple_index == 0 ? get_buf_uint32(&msg,left) : 0);
  const char *id = get_buf_string(&msg,left);
  const char *title = get_buf_string(&msg,left);
  const char *index_major = get_buf_string(&msg,left);
  const char *index_minor = get_buf_string(&msg,left);

  if (struct_index != _structures.size())
    ERR_MSG("Structure index not in order (got %d, expected %zd).",
	    struct_index, _structures.size());

  if (_cur_structure)
    {
      /* Finalisation is done by request_array_offsets. */
      ERR_MSG("Cannot book ntuple - structure in progress not finished.");
    }

  global_struct *s = new global_struct;

  if (!s)
    ERR_MSG("Failure allocating structure information.");

  _structures.push_back(s);
  _cur_structure = s;  

  s->_hid   = hid;
  s->_id    = strdup(id);
  s->_title = strdup(title);

  s->_index_major = strdup(index_major);
  s->_index_minor = strdup(index_minor);

  if (ntuple_index == 0)
    {
      s->_max_raw_words = max_raw_words;
    }

  do_book_ntuple(s, ntuple_index);
}

void request_hist_h1i(void *msg,uint32_t *left)
{
  uint32_t bins = get_buf_uint32(&msg,left);
  uint32_t hid = get_buf_uint32(&msg,left);
  double low  = get_buf_float(&msg,left);
  double high = get_buf_float(&msg,left);
  const char *id = get_buf_string(&msg,left);
  const char *title = get_buf_string(&msg,left);

#ifdef USING_CERNLIB
  hbook_histogram hist;

  hist.hbook1(hid,title,
	      bins,low,high);
#endif
#ifdef USING_ROOT
  TH1I *hist = new TH1I(id,title,
			bins,low,high);
#endif

  for (uint32_t b = 0; b < bins+2; b++)
    {
      uint32_t d = get_buf_uint32(&msg,left);

#ifdef USING_CERNLIB
      hist.hfill1(low+(b+.5)*(high-low)/bins,d);
#endif
#ifdef USING_ROOT
      hist->SetBinContent(b,d);
#endif
    }
#ifdef USING_CERNLIB
  hist.hrout();
#endif
#ifdef USING_ROOT
  if (!hist->Write())
    ERR_MSG("Writing histogram failed.  (disk full?)");
  delete hist;
#endif

  _g._num_hists++;

  if (_g._num_hists < 10)
    MSG("Hist H1I %s (%s) [%d,%.1f,%.1f]",id,title,bins,low,high);
  else if (_g._num_hists == 10)
    MSG("(Further histogram names suppressed...)");
}

void request_named_string(void *msg,uint32_t *left)
{
  const char *id = get_buf_string(&msg,left);
  const char *str = get_buf_string(&msg,left);

#ifdef USING_CERNLIB
  // Do not know how to write a string to a hbook file.  Ignoring!
#endif
#ifdef USING_ROOT
  TNamed *named = new TNamed(id, str);
  if (!named->Write())
    ERR_MSG("Writing named string failed.  (disk full?)");
  delete named;
#endif
}

void full_write(int fd,const void *buf,size_t count);

void merge_all_remaining()
{
  for (global_struct_vector::iterator iter = _structures.begin();
       iter != _structures.end(); ++iter)
    {
      global_struct *s = *iter;

      ext_merge_sort_all(&s->_offset_array,
			 s->_stage_array._length);
    }
}

void close_structure(global_struct *s, size_t *num_trees)
{
#if USING_CERNLIB
  if (s->_cwn)
    delete s->_cwn;
#endif
#if USING_ROOT
  *num_trees += s->_root_ntuples.size();
#endif
}

void close_output()
{
  size_t num_trees = 0;
  uint64_t num_index_entries = 0;

  for (global_struct_vector::iterator iter = _structures.begin();
       iter != _structures.end(); ++iter)
    {
      global_struct *s = *iter;

      close_structure(s, &num_trees);
    }

  _g._num_events_total += _g._num_events;
#if USING_CERNLIB
  if (_g._hfile)
    {
      _g._hfile->close();
      delete _g._hfile;
      if (_config._timeslice) {
	MSG("Closed hbook (%lld events, %lld total).",
	    (long long int) _g._num_events,
	    (long long int) _g._num_events_total);
      } else
	MSG("Closed hbook (%lld events).",(long long int) _g._num_events);
    }
#endif
#if USING_ROOT
  if (_g._root_file)
    {
      for (global_struct_vector::iterator iter = _structures.begin();
	   iter != _structures.end(); ++iter)
	{
	  global_struct *s = *iter;

	  if (strcmp(s->_index_major,"") != 0)
	    {
	      const char *minor = s->_index_minor;

	      if (strcmp(s->_index_minor,"") == 0)
		minor = "0";

	      for (TTree_vector::iterator iter = s->_root_ntuples.begin();
		   iter != s->_root_ntuples.end(); ++iter)
		num_index_entries +=
		  (*iter)->BuildIndex(s->_index_major, minor);
	    }
	}
      if (_config._outfile)
	_g._root_file->Write();
      _g._root_file->Close();
      if (_g._root_file->TestBit(TFile::kWriteError))
	ERR_MSG("File writing failed.  (disk full?)");
      delete _g._root_file;
      char msg_total[64] = "";
      char msg_trees[64] = "";
      char msg_index[64] = "";
      char msg_hists[64] = "";
      if (_config._timeslice)
	sprintf (msg_total,", %lld total",
		 (long long int) _g._num_events_total);
      if (num_trees > 1)
	sprintf (msg_trees," in %d trees",(int) num_trees);
      if (num_index_entries)
	sprintf (msg_index,", %lld index-entries",
		 (long long int) num_index_entries);
      if (_g._num_hists)
	sprintf (msg_hists,", %d histograms",_g._num_hists);
      MSG("Closed file (%lld events%s%s%s%s).",
	  (long long int) _g._num_events,
	  msg_total,msg_trees,msg_index,msg_hists);
    }
#endif
#if USING_CERNLIB || USING_ROOT
  if (_ts._timeslice_name._curname)
    {
      strcat(_ts._timeslice_name._curname,"\n");

      // Handles errors itself
      full_write(_ts._timeslice_name._list_fd,
		 _ts._timeslice_name._curname,
		 strlen(_ts._timeslice_name._curname));
      fsync(_ts._timeslice_name._list_fd);

      free(_ts._timeslice_name._curname);
      _ts._timeslice_name._curname = NULL;
    }
#endif
#if STRUCT_WRITER
  ext_net_io_server_close();
  MSG("Done (%lld events, %.1f %cB, %.1f %cB to clients).     ",
      (long long int) _g._num_events,
      _net_stat._committed_size > 2000000000 ?
      (double) _net_stat._committed_size / 1000000000. :
      (double) _net_stat._committed_size / 1000000.,
      _net_stat._committed_size > 2000000000 ? 'G' : 'M',
      _net_stat._sent_size > 2000000000 ?
      (double) _net_stat._sent_size / 1000000000. :
      (double) _net_stat._sent_size / 1000000.,
      _net_stat._sent_size > 2000000000 ? 'G' : 'M');
#endif
  _g._num_events = 0;
}

void request_alloc_array(void *msg,uint32_t *left)
{
  global_struct *s = _cur_structure;

  if (!s)
    ERR_MSG("Cannot allocate array - no structure being defined.");

  uint32_t size = get_buf_uint32(&msg,left);

  if (s->_stage_array._length)
    ERR_MSG("Array already allocated.");

  // Since our compacter would break down in that it can produce
  // longer output than input.
  if (s->_stage_array._length >= 0x10000000)
    ERR_MSG("Refusing to handle event sizes >= 256 MB.");

  MSG("About to allocate array: %d B",size);

  s->_stage_array._length = size;
  s->_stage_array._ptr = (char*) malloc(size);

  if (!s->_stage_array._ptr)
    ERR_MSG("Failure allocating array with size %d.",size);

  memset(s->_stage_array._ptr,0,s->_stage_array._length);

  // TODO: This is only used when merging (time stitching).
  // For now: be lazy and always allocate it.
  // s->_stage_array._merge_ptr = (char*) malloc(size);
  // if (!s->_stage_array._merge_ptr)
  //   ERR_MSG("Failure allocating merge array with size %d.",size);

#if STRUCT_WRITER
  if (s->_stage_array._length < (1 << (4 + 2)))
    s->_stage_array._rewrite_max_bytes_per_item = 5;
  else if (s->_stage_array._length < (1 << (4 + 7 + 2)))
    s->_stage_array._rewrite_max_bytes_per_item = 6;
  else if (s->_stage_array._length < (1 << (4 + 7 + 7 + 2)))
    s->_stage_array._rewrite_max_bytes_per_item = 7;
  else if (s->_stage_array._length < (1 << (4 + 7 + 7 + 7 + 2)))
    s->_stage_array._rewrite_max_bytes_per_item = 8;
  else
    s->_stage_array._rewrite_max_bytes_per_item = 9;

  s->_stage_array._offset_value = (uint32_t*) malloc(size/* *2 */);

  if (!s->_stage_array._ptr)
    ERR_MSG("Failure allocating offset/value array with size %d.",size*2);

  // How many items do we have to mask?
  size_t masks = (size + 3) & ~3;

  for (int i = BUCKET_SORT_LEVELS-1; i >= 0; i--)
    {
      // Each mask item covers 32 items
      masks = (masks + 31) / 32;

      s->_stage_array._masks[i] = (uint32_t*) malloc(masks * sizeof(uint32_t));

      if (!s->_stage_array._masks[i])
	ERR_MSG("Failure allocating mask array with size %zd.",
		masks * sizeof(uint32_t));

      s->_stage_array._num_masks[i] = masks;
    }
#endif

  // MSG("Alloc array: %d",size);
}

#if STRUCT_WRITER
uint32_t calc_structure_xor_sum(stage_array &sa);
#endif

void request_array_offsets(void *msg,uint32_t *left)
{
  global_struct *s = _cur_structure;

  if (!s)
    ERR_MSG("Cannot allocate offsets - no structure being defined.");

  // First deal with the xor check.

  uint32_t *xor_sum_msg_p = (uint32_t *) msg;
  uint32_t xor_sum_msg = get_buf_uint32(&msg,left);

  uint32_t xor_sum = 0; // make compiler happy

#if STRUCT_WRITER
  if (1 /* writer check when we were in request_setup_done */ /*!writer*/)
    {
      xor_sum = calc_structure_xor_sum(s->_stage_array);

      if (xor_sum_msg && xor_sum_msg != xor_sum)
	ERR_MSG("Mismatch between received (0x%08x) "
		"and calculated xor_sum (0x%08x).",
		xor_sum_msg,xor_sum);

      // write the xor_sum into the chunk that goes to the network
      // it's in the first integer after the header

      *xor_sum_msg_p = htonl(xor_sum);

      /*
	fprintf (stderr, "set *xor_sum_msg_p : %08x\n", xor_sum);
      */

      s->_xor_sum = xor_sum;
    }
#endif

  // So, we are given the offset table.  Allocate space for it and
  // make a copy.

  if (s->_offset_array._length)
    ERR_MSG("Offset already allocated.");

  s->_offset_array._length = *left / sizeof(uint32_t);
  s->_offset_array._ptr = (uint32_t*) malloc(*left);
  s->_offset_array._static_items = 0;
  s->_offset_array._max_items = 0;

  s->_offset_array._poffset_ts_lo    = (uint32_t) -1;
  s->_offset_array._poffset_ts_hi    = (uint32_t) -1;
  s->_offset_array._poffset_ts_srcid = (uint32_t) -1;
  s->_offset_array._poffset_meventno = (uint32_t) -1;
  s->_offset_array._poffset_mrg_stat = (uint32_t) -1;
  s->_offset_array._poffset_mrg_mask = (uint32_t) -1;

  // MSG ("offsets: %d \n", _offset_array._length);

  if (!s->_offset_array._ptr)
    ERR_MSG("Failure allocating offset array with size %d.",*left);

  // Then verify that it's variable size array control items are
  // consistent.  (so we do not need to do that every event)

  uint32_t *o    = (uint32_t *) msg;
  uint32_t *oend = (uint32_t *) (((char*) msg) + *left);

  uint32_t *d    = s->_offset_array._ptr;

  uint32_t p_offset = 0;
  bool had_loop = false;

  bool expect_mind_loop2 = false;

  // We reset the entire stage array, to use that to verify that each
  // item is used once

  memset(s->_stage_array._ptr,0,s->_stage_array._length);

  // MSG("Offsets: %d Array: %d",*left,_stage_array._length);

  while (o < oend)
    {
      uint32_t mark   = *(d++) = ntohl(*(o++));
      uint32_t offset = *(d++) = ntohl(*(o++));

      // MSG("Offset entry @ %d : %d",(int) (o - (uint32_t *) msg)-1,offset);

      if ((mark & EXTERNAL_WRITER_MARK_CANARY_MASK) !=
	  EXTERNAL_WRITER_MARK_CANARY)
	ERR_MSG("Mark entry @ %zd has bad canary (%" PRIx32 ").",
		(o - (uint32_t *) msg)-2,mark);

      if (offset >= s->_stage_array._length)
	ERR_MSG("Offset entry @ %zd is outside (%" PRIu32 ") stage array "
		"(%zd).",
		(o - (uint32_t *) msg)-1,offset,s->_stage_array._length);
      if (offset & 3)
	ERR_MSG("Offset entry @ %zd is unaligned (%" PRIu32 ").",
		(o - (uint32_t *) msg)-1,offset);

      (*((uint32_t *) (s->_stage_array._ptr + offset)))++;
      s->_offset_array._static_items++;

      if ((mark & (EXTERNAL_WRITER_MARK_TS_LO |
		   EXTERNAL_WRITER_MARK_TS_HI |
		   EXTERNAL_WRITER_MARK_TS_SRCID |
		   EXTERNAL_WRITER_MARK_MEVENTNO |
		   EXTERNAL_WRITER_MARK_MRG_STAT |
		   EXTERNAL_WRITER_MARK_MRG_MASK)) &&
	  had_loop)
	{
	  /* The offset to the item in the raw (before stage) data
	   * could vary due to loops.  Offset would be jumping around.
	   */
	  ERR_MSG("Mark entry @ %zd has merge info (%08x) after loop item, "
		  "offset not fixed.",
		  (o - (uint32_t *) msg)-2, mark);
	}

      if (mark & EXTERNAL_WRITER_MARK_TS_LO)
	s->_offset_array._poffset_ts_lo = p_offset;
      if (mark & EXTERNAL_WRITER_MARK_TS_HI)
	s->_offset_array._poffset_ts_hi = p_offset;
      if (mark & EXTERNAL_WRITER_MARK_TS_SRCID)
	s->_offset_array._poffset_ts_srcid = p_offset;
      if (mark & EXTERNAL_WRITER_MARK_MEVENTNO)
	s->_offset_array._poffset_meventno = p_offset;
      if (mark & EXTERNAL_WRITER_MARK_MRG_STAT)
	s->_offset_array._poffset_mrg_stat = p_offset;
      if (mark & EXTERNAL_WRITER_MARK_MRG_MASK)
	s->_offset_array._poffset_mrg_mask = p_offset;

      if (!(mark & EXTERNAL_WRITER_MARK_LOOP) &&
	  (mark & (EXTERNAL_WRITER_MARK_ARRAY_IND1 |
		   EXTERNAL_WRITER_MARK_ARRAY_MIND)))
	ERR_MSG("Mark entry @ %zd has loop specialisation info (%08x) "
		"but is not loop control.",
		(o - (uint32_t *) msg)-2, mark);

      if ((mark & EXTERNAL_WRITER_MARK_LOOP) &&
	  (mark & (EXTERNAL_WRITER_MARK_ITEM_INDEX |
		   EXTERNAL_WRITER_MARK_ITEM_ENDNUM)))
	ERR_MSG("Mark entry @ %zd has array index item info (%08x) "
		"but is loop control.",
		(o - (uint32_t *) msg)-2, mark);

      if (expect_mind_loop2 &&
	  !(mark & EXTERNAL_WRITER_MARK_LOOP))
	{
	  ERR_MSG("Mark entry @ %zd expected to be 2nd loop after m-ind loop, "
		  "but is not loop control (%08x).",
		  (o - (uint32_t *) msg)-2, mark);
	}

      bool loop_mark_array_ind1 = !!(mark & EXTERNAL_WRITER_MARK_ARRAY_IND1);
      bool loop_mark_array_mind = !!(mark & EXTERNAL_WRITER_MARK_ARRAY_MIND);

      // MSG ("offset: %d %c",offset,(mark & EXTERNAL_WRITER_COMPACT_PACKED) ? '*' : ' ');

      p_offset++; /* No need to increment in loop, will vary. */

      if (mark & EXTERNAL_WRITER_MARK_LOOP)
	{
	  had_loop = true;

	  if (!(mark & EXTERNAL_WRITER_MARK_CLEAR_ZERO))
	    ERR_MSG("Offset ctrl entry @ %zd=0x%zx marker "
		    "says clean float (NaN).",
		    (o - (uint32_t *) msg)-1,(o - (uint32_t *) msg)-1);

	  if (o + 2 > oend)
	    ERR_MSG("Offset ctrl entry @ %zd=0x%zx overflows array (%d).",
		    (o - (uint32_t *) msg)-1,(o - (uint32_t *) msg)-1,
		    *left);

	  uint32_t max_loops = *(d++) = ntohl(*(o++));
	  uint32_t loop_size = *(d++) = ntohl(*(o++));

	  if ((mark & EXTERNAL_WRITER_MARK_ARRAY_MIND) &&
	      loop_size != 2)
	    ERR_MSG("Mark ctrl entry @ %zd=0x%zx specifies multi-index "
		    "loop, but loop_size (%d) != 2.",
		    (o - (uint32_t *) msg)-4,(o - (uint32_t *) msg)-4,
		    loop_size);

	  // MSG ("               %d * %d",max_loops,loop_size);

	  uint32_t items = max_loops * loop_size;

	  if (oend - o < 2 * items)
	    ERR_MSG("Offset ctrl entry @ %zd=0x%zx specifies controlled "
		    "entries (%" PRIu32 " * %" PRIu32 ") overflowing array (%"
		    PRIu32").",
		    (o - (uint32_t *) msg)-1,(o - (uint32_t *) msg)-1,
		    max_loops,loop_size,*left);

	  s->_offset_array._max_items += items;

	  if (loop_size > _max_loop_size)
	    _max_loop_size = loop_size;

	  for (int l = 0; l < max_loops; l++)
	    for (int i = 0; i < loop_size; i++)
	      {
		// Make sure there are no markers (although they would
		// be ignored)

		mark   = *(d++) = ntohl(*(o++));
		offset = *(d++) = ntohl(*(o++));

		if (offset >= s->_stage_array._length)
		  ERR_MSG("Offset entry @ %zd=0x%zx is outside "
			  "(%" PRIu32 ") stage array (%zd).",
			  (o - (uint32_t *) msg)-1,(o - (uint32_t *) msg)-1,
			  offset,
			  s->_stage_array._length);
		if (offset & 3)
		  ERR_MSG("Offset entry @ %zd=0x%zx is unaligned (%d).",
			  (o - (uint32_t *) msg)-1,(o - (uint32_t *) msg)-1,
			  offset);

		(*((uint32_t *) (s->_stage_array._ptr + offset)))++;

		if (mark & EXTERNAL_WRITER_MARK_LOOP)
		  ERR_MSG("Controlled offset array item (%d,%d) "
			  "@ %zd=0x%zx is marked "
			  "(0x%08x) as control item.",
			  l,i,
			  (o - (uint32_t *) msg)-2,(o - (uint32_t *) msg)-2,
			  mark);

		bool mark_item_index =
		  !!(mark & EXTERNAL_WRITER_MARK_ITEM_INDEX);
		bool mark_item_endnum =
		  !!(mark & EXTERNAL_WRITER_MARK_ITEM_ENDNUM);

		if (mark_item_index && (i != 0))
		  ERR_MSG("Controlled offset array item (%d,%d) "
			  "@ %zd=0x%zx is marked "
			  "(0x%08x) as index at non-0 index.",
			  l,i,
			  (o - (uint32_t *) msg)-2,(o - (uint32_t *) msg)-2,
			  mark);

		if (mark_item_endnum && (i != 1))
		  ERR_MSG("Controlled offset array item (%d,%d) "
			  "@ %zd=0x%zx is marked "
			  "(0x%08x) as endnum at non-1 index.",
			  l,i,
			  (o - (uint32_t *) msg)-2,(o - (uint32_t *) msg)-2,
			  mark);

		if ((loop_mark_array_ind1 ||
		     loop_mark_array_mind) && i == 0 &&
		    !mark_item_index)
		  ERR_MSG("Controlled offset array item (%d,%d) "
			  "@ %zd=0x%zx is not marked "
			  "(0x%08x) as index, "
			  "but at index 0 of ind1/m-ind loop.",
			  l,i,
			  (o - (uint32_t *) msg)-2,(o - (uint32_t *) msg)-2,
			  mark);

		if (loop_mark_array_mind && i == 1 &&
		    !mark_item_endnum)
		  ERR_MSG("Controlled offset array item (%d,%d) "
			  "@ %zd=0x%zx is not marked "
			  "(0x%08x) as index, "
			  "but at index 1 of ind1/m-ind loop.",
			  l,i,
			  (o - (uint32_t *) msg)-2,(o - (uint32_t *) msg)-2,
			  mark);
	      }
	}

      expect_mind_loop2 = loop_mark_array_mind;
    }

  if (expect_mind_loop2)
    ERR_MSG("Missing expected 2nd loop after m-ind loop.");

  s->_offset_array._max_items += s->_offset_array._static_items;

  uint32_t *pend = (uint32_t *) (s->_stage_array._ptr +
				 s->_stage_array._length);

  for (uint32_t *p = (uint32_t *) s->_stage_array._ptr; p < pend; p++)
    {
      if (*p != 1)
	ERR_MSG("Stage area item @ %zd is not used (%" PRIu32 ") exactly once.",
		(char*) p - s->_stage_array._ptr,*p);
    }

  *left = 0;

  if (s->_offset_array._max_items > _g._max_offset_array_items)
    _g._max_offset_array_items = s->_offset_array._max_items;

  if (_config._ts_merge_window &&
      s->_offset_array._poffset_ts_lo == (uint32_t) -1)
    ERR_MSG("Cannot merge (time_stitch) without TSTAMPLO.");
  if (_config._ts_merge_window &&
      s->_offset_array._poffset_ts_hi == (uint32_t) -1)
    ERR_MSG("Cannot merge (time_stitch) without TSTAMPHI.");
  if (_config._ts_merge_window &&
      s->_offset_array._poffset_ts_srcid == (uint32_t) -1)
    ERR_MSG("Cannot merge (time_stitch) without TSTAMPSRCID.");

  // OK, we are happy with the offset array

  // MSG("Offsets...");

  /*
  fprintf(stderr, "merge offsets: %d %d %d %d %d %d\n",
	  s->_offset_array._poffset_ts_lo,
	  s->_offset_array._poffset_ts_hi,
	  s->_offset_array._poffset_ts_srcid,
	  s->_offset_array._poffset_meventno,
	  s->_offset_array._poffset_mrg_stat,
	  s->_offset_array._poffset_mrg_mask);
  */

  /* This structure is done. */
  _cur_structure = NULL;
}

struct str_var_type
{
  const char *_Cname;
  const char *_hbook;
  const char *_root;
};

str_var_type _str_var_type[] = {
  { NULL,       NULL, NULL },
  { "int32_t ", "I", "I" },
  { "uint32_t", "I", "i" }, // hbook should be U
  // for uint16_t root is s and uint8_t root is b
  { "float   ", "R", "F" },
};

void do_create_branch(global_struct *s,
		      uint32_t offset,stage_array_item &item)
{
  if (!s->_stage_array._length)
    ERR_MSG("Cannot create branch using unallocated array.");

  if (offset > s->_stage_array._length ||
      offset+item._length > s->_stage_array._length)
    ERR_MSG("Cannot create branch with ptr [%" PRIu32 ",%" PRIu32
	    "[ outside array (%zd).",
	    offset,offset+item._length,s->_stage_array._length);

  if (item._var_type == 0 ||
      (item._var_type & EXTERNAL_WRITER_FLAG_TYPE_MASK) >=
      sizeof(_str_var_type)/sizeof(_str_var_type[0]) ||
      item._var_type & ~(EXTERNAL_WRITER_FLAG_TYPE_MASK |
			 EXTERNAL_WRITER_FLAG_HAS_LIMIT))
    ERR_MSG("Invalid variable type (0x%x).",item._var_type);

  void *ptr = s->_stage_array._ptr+offset;

  size_t branch_size = strlen(item._var_name)+
    (item._var_ctrl_name ? strlen(item._var_ctrl_name) : 0)+
    2/*type*/+1/*trail 0*/+2/*length/ctrl bracket ()*/+3/*limit [ , ]*/+2*11;
  char *branch = (char *) malloc(branch_size);

  if (!branch)
    ERR_MSG("Failure allocating string.");

#if USING_CERNLIB
  strcpy (branch,item._var_name);
  if (item._var_array_len != (uint32_t) -1)
    {
      if (item._var_ctrl_name)
	sprintf (branch+strlen(branch),"(%s)",item._var_ctrl_name);
      else
 	sprintf (branch+strlen(branch),"(%d)",item._var_array_len);
    }
  if (item._var_type & EXTERNAL_WRITER_FLAG_HAS_LIMIT)
    sprintf (branch+strlen(branch),
    "[%d,%d]",item._limit_min,item._limit_max);
  sprintf (branch+strlen(branch),":%s",
	   _str_var_type[item._var_type &
			 EXTERNAL_WRITER_FLAG_TYPE_MASK]._hbook);

  assert(strlen(branch) < branch_size); // < to include the trail 0

  // MSG ("OLD: %s NEW: %s",hbname_vars,branch);

  s->_cwn->hbname(item._block,ptr,branch/*hbname_vars*/);
#endif
#if USING_ROOT

  if (_config._outfile)
    {
      strcpy (branch,item._var_name);
      if (item._var_array_len != (uint32_t) -1)
	{
	  if (item._var_ctrl_name)
	    sprintf (branch+strlen(branch),"[%s]",item._var_ctrl_name);
	  else
	    sprintf (branch+strlen(branch),"[%d]",item._var_array_len);
	}
      /* // no limits in root
	 if (var_type & EXTERNAL_WRITER_FLAG_HAS_LIMIT)
	 sprintf (branch+strlen(branch),
	 "[%d,%d]",limit_min,limit_max);
      */
      sprintf (branch+strlen(branch),"/%s",
	       _str_var_type[item._var_type &
			     EXTERNAL_WRITER_FLAG_TYPE_MASK]._root);

      assert(strlen(branch) < branch_size); // < to include the trail 0

      // MSG ("OLD: %s NEW: %s",branch_vars,branch);

      for (TTree_vector::iterator iter = s->_root_ntuples.begin();
	   iter != s->_root_ntuples.end(); ++iter)
	(*iter)->Branch(item._var_name,ptr,branch/*branch_vars*/);
    }
  else
    {
      // We must verify that the branch exists, and have appropriate type!

      TBranch *br = s->_root_ntuple->GetBranch(item._var_name);

      if (!br)
	{
	  WARN_MSG("Requsted branch (%s) does not exist in tree.",
		   item._var_name);
	  /* Not such a disaster, just ignore it.  Zeroes will be
	   * delivered for each event...
	   */
	  return;
	}

      // So the branch is there.  Then, we expect the (only) leaf to be
      // the variable we want to get at!

      if (br->GetNleaves() != 1)
	ERR_MSG("Requsted branch (%s) does not have one leaf (%d).",
		item._var_name,br->GetNleaves());

      TLeaf *lf = br->GetLeaf(item._var_name);

      if (!lf)
	ERR_MSG("Requsted leaf (%s) does not exist in branch.",
		item._var_name);

      // Make sure it is counted only if we are counted
      TLeaf *lfcount = lf->GetLeafCount();

      // And, if we are counted, we should check that the count variable
      // has the right name.  (figuring out if it really is the variable
      // that we have in mind is (a lot) more tricky! Well, we just look
      // up the proposed count variable the normal way and check it!

      if (item._var_array_len == (uint32_t) -1)
	{
	  /* We are a singular item. */
	  if (lfcount)
	    ERR_MSG("Requested leaf (%s) variable in tree is counted, "
		    "but request is not.",item._var_name);
	  if (lf->GetLenStatic() != 1)
	    ERR_MSG("Requested leaf (%s) variable in tree is an array, "
		    "but request is not.",item._var_name);
	}
      else if (!item._var_ctrl_name)
	{
	  /* We are a an static array. */
	  if (lfcount)
	    ERR_MSG("Requested leaf (%s) variable in tree is counted, "
		    "but request is static array.",item._var_name);
	  if (lf->GetLenStatic() != item._var_array_len)
	    ERR_MSG("Requested leaf (%s) variable in tree "
		    "is static array [%d], but request is [%d].",
		    item._var_name,
		    lf->GetLenStatic(), item._var_array_len);
	}
      else
	{
	  /* We are a an dynamic array. */
	  if (!lfcount)
	    ERR_MSG("Requested leaf (%s) variable in tree is not counted, "
		    "but request is.",item._var_name);

	  TBranch *brc = s->_root_ntuple->GetBranch(item._var_ctrl_name);

	  if (!brc)
	    ERR_MSG("Requested branch (%s) (array control) "
		    "does not exist in tree.",
		    item._var_ctrl_name);

	  if (brc->GetNleaves() != 1)
	    ERR_MSG("Requested branch (%s) does not have one leaf (%d).",
		    item._var_ctrl_name,brc->GetNleaves());

	  TLeaf *lfc = brc->GetLeaf(item._var_ctrl_name);

	  if (!lfc)
	    ERR_MSG("Requested leaf (%s) (array control) "
		    "does not exist in branch.",
		    item._var_ctrl_name);

	  // And they better be the same

	  if (lfc != lfcount)
	    ERR_MSG("Requested leaf's (%s) array control (%s) found, "
		    "but mismatches leaf in tree's control variable (%s).",
		    item._var_name,item._var_ctrl_name,lfcount->GetName());

	  // Ok, so that matches
	}

      // Now that we found the sought variable, set the address!

      br->SetAddress(ptr);
    }
#endif

  //MSG("Created branch (%s,%s,%s,aid=%d,offset=%d,length=%d)",
  //    block,hbname_vars,branch_vars,aid,offset,length);
}

void request_create_branch(void *msg,uint32_t *left)
{
  global_struct *s = _cur_structure;

  if (!s)
    ERR_MSG("Cannot create branch - no structure being defined.");

  uint32_t aid    = 0;
  uint32_t offset = get_buf_uint32(&msg,left);
  uint32_t length = get_buf_uint32(&msg,left);
  uint32_t var_array_len = get_buf_uint32(&msg,left);
  uint32_t var_type      = get_buf_uint32(&msg,left);
  uint32_t limit_min     = get_buf_uint32(&msg,left);
  uint32_t limit_max     = get_buf_uint32(&msg,left);

  const char *block = get_buf_string(&msg,left);
  //const char *branch_block = get_buf_string(&msg,left);
  //const char *hbname_vars = get_buf_string(&msg,left);
  //const char *branch_vars = get_buf_string(&msg,left);
  const char *var_name    = get_buf_string(&msg,left);
  const char *var_ctrl_name = get_buf_string(&msg,left);

  stage_array_item item;

#if !STRUCT_WRITER
  item._offset        = offset;
#endif
  item._length        = length;
  item._block         = strdup(block);
  //item._branch_block  = strdup(branch_block);
  //item._hbname_vars   = strdup(hbname_vars);
  //item._branch_vars   = strdup(branch_vars);
  item._var_name      = strdup(var_name);
  item._var_array_len = var_array_len;
  item._var_ctrl_name =
    strcmp(var_ctrl_name,"") == 0 ? NULL : strdup(var_ctrl_name);
  item._var_type      = var_type;
  item._limit_min     = limit_min;
  item._limit_max     = limit_max;
  /*
  printf ("%d %d %d %d %d %d .%s. .%s. .%s.\n",
	  offset, length,
	  var_array_len, var_type,
	  limit_min, limit_max,
	  block,
	  var_name, var_ctrl_name);
  fflush(stdout);
  */
#if STRUCT_WRITER
  if (item._var_array_len != -1 &&
      item._var_ctrl_name)
    {
      stage_array_name_map::iterator iter =
	s->_stage_array._names.find(item._var_ctrl_name);

      if (iter == s->_stage_array._names.end())
	ERR_MSG("Unable to find controlling item (%s) for %s.",
		item._var_ctrl_name,item._var_name);

      item._ctrl_offset = iter->second;
    }
  else
    item._ctrl_offset   = (uint32_t) -1;
#endif

  do_create_branch(s, offset,item);

#if STRUCT_WRITER
  s->_stage_array._items.insert(stage_array_item_map::value_type(offset,item));
  s->_stage_array._names.insert(stage_array_name_map::value_type(item._var_name,
								 offset));
#else
  s->_stage_array._items_v.push_back(item);
#endif
}

#if STRUCT_WRITER
uint32_t calc_structure_xor_sum(stage_array &sa)
{
  uint32_t xor_sum = 0x5a5a5a5a;

  for (stage_array_item_map::iterator iter = sa._items.begin();
       iter != sa._items.end(); ++iter)
    {
      stage_array_item &item = iter->second;
      uint32_t offset = iter->first;

      // Simple junk formula to avoid accidental use of non-compatible
      // structures
      int xor_shift = offset % 27;
      xor_sum ^= (item._var_type << xor_shift);
      xor_sum += item._var_array_len - (xor_sum >> 27);
      xor_sum ^= item._limit_min << 4;
      xor_sum ^= item._limit_max << 8;

      int i = offset % 31;

      for (const char *p = item._var_name; *p; p++, i = (i+1) % 23)
	xor_sum ^= ((uint32_t) tolower(*p)) << i;
      if (item._var_array_len != (uint32_t) -1 &&
	  item._var_ctrl_name)
	for (const char *p = item._var_ctrl_name; *p; p++, i = (i+22) % 23)
	  xor_sum ^= ((uint32_t) tolower(*p)) << i;
    }
  return xor_sum;
}

void generate_structure(FILE *fid,stage_array &sa,int indent,bool infomacro)
{
  if (_config._debug_header)
    {
      for (stage_array_item_map::iterator iter = sa._items.begin();
	   iter != sa._items.end(); ++iter)
	{
	  stage_array_item &item = iter->second;
	  uint32_t offset = iter->first;

	  fprintf (fid,
		   "/* %04x : %04x : %s[%d:%s]/%d : %s */%s\n",
		   offset,item._length,
		   item._var_name,item._var_array_len,
		   item._var_ctrl_name ? item._var_ctrl_name : "_",
		   item._var_type,
		   item._block,
		   infomacro ? " \\" : "");
	}

      fprintf (fid,
	       "\n");
    }

  uint32_t location = 0;
  const char *prev_block = "";

  for (stage_array_item_map::iterator iter = sa._items.begin();
       iter != sa._items.end(); ++iter)
    {
      stage_array_item &item = iter->second;
      uint32_t offset = iter->first;

      if (strcmp(prev_block,item._block))
	{
	  fprintf (fid,"%*s/* %s */%s\n",
		   indent,"",item._block,
		   infomacro ? " \\" : "");
	  prev_block = item._block;
	}

      if (offset < location)
	{
	  fprintf (fid,
		   "#error Next offset 0x%x < next location 0x%x\n",
		   offset,location);
	  MSG("Structure error, overlapping items, "
	      "%s has offset (0x%x) before end of previous item (0x%x).",
	      item._var_name,offset,location);
	}
      if (offset > location)
	{
	  if (!infomacro)
	    fprintf (fid,
		     "%*schar dummy_%x[%d];\n",
		     indent,"",location,offset-location);
	  location = offset;
	}


      if (!infomacro)
	{
	  fprintf (fid,"%*s",indent,"");
	  if (_config._debug_header)
	    fprintf (fid,"/* %04x %04x */ ",offset,item._length);
	  fprintf (fid,"%s %s",
		   _str_var_type[item._var_type &
				 EXTERNAL_WRITER_FLAG_TYPE_MASK]._Cname,
		   item._var_name);
	  if (item._var_array_len != (uint32_t) -1)
	    {
	      if (item._var_ctrl_name)
		fprintf (fid,"[%d EXT_STRUCT_CTRL(%s)]",
			 item._var_array_len,item._var_ctrl_name);
	      else
		fprintf (fid,"[%d]",
			 item._var_array_len);
	    }
	  fprintf (fid," /* [%d,%d] */",item._limit_min,item._limit_max);
	  fprintf (fid,";\n");
	}
      else
	{
	  const char *item_type = "UNKNOWN";

	  switch (item._var_type & EXT_DATA_ITEM_TYPE_MASK)
	    {
	    case EXT_DATA_ITEM_TYPE_INT32:   item_type = "INT32";   break;
	    case EXT_DATA_ITEM_TYPE_UINT32:  item_type = "UINT32";  break;
	    case EXT_DATA_ITEM_TYPE_FLOAT32: item_type = "FLOAT32"; break;
	    }

	  fprintf (fid,"%*s",indent,"");
	  if (_config._debug_header)
	    fprintf (fid,"/* %04x %04x */ ",offset,item._length);
	  int padlen = 32 - strlen(item._var_name);
	  if (padlen < 0)
	    padlen = 0;
	  fprintf (fid,
		   "EXT_STR_ITEM_INFO%4s(ok,si,offset,struct_t,printerr,\\\n"
		   "  %*.0s%s,%*.0s%s,\\\n"
		   "  %*.0s\"%s\"",
		   item._var_array_len != (uint32_t) -1 ? "_ZZP" :
		   (item._limit_max != -1 ? "_LIM" : ""),
		   19,"",item._var_name,
		   padlen,"",item_type,
		   18,"",item._var_name);
	  if (item._var_array_len != (uint32_t) -1)
	    {
	      if (item._var_ctrl_name)
		fprintf (fid,",%*.0s\"%s\"",
			 padlen,"",item._var_ctrl_name);
	      else
		fprintf (fid,",%d",item._var_array_len);
	    }
	  else if (item._limit_max != -1)
	    fprintf (fid,",%d",item._limit_max);
	  fprintf (fid,");%s\n",
		   infomacro ? " \\" : "");
	}
      location = offset + item._length;
    }
  if (sa._length < location)
    {
      fprintf (fid,
	       "#error Length of structure 0x%zx < next location 0x%x\n",
	       sa._length,location);
      MSG("Structure error, overflow, "
	  "end (0x%zx) before end of previous item (0x%x).",
	  sa._length,location);
    }
  if (sa._length > location)
    {
      if (!infomacro)
	fprintf (fid,
		 "%*schar dummy_%x[%zd];\n",
		 indent,"",location,sa._length-location);
    }
}

struct compare_str_less
{
  bool operator()(const char *s1,const char *s2) const
  {
    return strcmp(s1,s2) < 0;
  }
};

typedef std::set<const char *, compare_str_less> set_strings;

void unique_var_name(char *var_name,set_strings &used_names)
{
  // If we can find the name in the list, first try by appending
  // a '_'.  If that does not help, start to count numbers

  if (used_names.find(var_name) != used_names.end())
    {
      strcat(var_name,"_");

      char *add_at = var_name + strlen(var_name);

      for (int add = 1; used_names.find(var_name) != used_names.end(); add++)
	{
	  sprintf (add_at,"%d",add);
	}
    }

  used_names.insert(var_name);
}

void generate_structure_item(FILE *fid,
			     uint32_t offset,stage_array_item &item,
			     const char *base_name,int base_length,
			     int first_array_len,
			     int indent,set_strings &used_names)
{
  fprintf (fid,"%*s",indent,"");
  if (_config._debug_header)
    fprintf (fid,"/* %04x %04x */ ",offset,item._length);
  fprintf (fid,"%s ",
	   _str_var_type[item._var_type & EXTERNAL_WRITER_FLAG_TYPE_MASK]._Cname);
  char *var_name = new char[(base_name ? base_length : 0) +
			    (*item._var_name ? strlen(item._var_name) : 0) +
			    1 + 1 + // 1 for '\0', 1 for (possible) '_'
			    1 + 16]; // '_' + disambig

  if (base_name)
    sprintf (var_name,"%.*s",base_length,base_name);
  else
    var_name[0] = 0;
  strcat (var_name,*item._var_name ? item._var_name :
	  (base_name && *base_name ? "" : "_"));

  unique_var_name(var_name,used_names);

  fprintf (fid,"%s",var_name);

  if (first_array_len != -1)
    fprintf (fid,"[%d]",first_array_len);
  if (item._var_array_len != (uint32_t) -1)
    fprintf (fid,"[%d /* %s */]",
	     item._var_array_len,
	     item._var_ctrl_name ? item._var_ctrl_name : "_");
  fprintf (fid,";\n");
}

bool match_base_index(const char *base_name_start,int base_length,
		      const char *name,int match_index,
		      char **subname_ptr)
{
  if (strncmp(base_name_start,name,base_length) != 0)
    return false;

  const char *start_index = name + base_length;

  if (!*start_index || !isdigit(*start_index))
    return false;

  int index = strtol(start_index,subname_ptr,10);

  return index == match_index;
}

void generate_structure_onion(FILE *fid,stage_array_item_map &sa,
			      int indent,set_strings &used_names)
{
  const char *prev_block = "";

  for (stage_array_item_map::iterator iter = sa.begin();
       iter != sa.end(); )
    {
      stage_array_item &item = iter->second;
      uint32_t offset = iter->first;

      if (indent == 2 && strcmp(prev_block,item._block))
	{
	  fprintf (fid,"%*s/* %s */\n",indent,"",item._block);
	  prev_block = item._block;
	}

      // For every item, see if it has a name base, followed by a
      // number

      const char *start_index = item._var_name;

      while (*start_index && !isdigit(*start_index))
	start_index++;

      if (*start_index)
	{
	  char *end_index;

	  int index = strtol(start_index,&end_index,10);

	  // So, the name-base is (item._var_name,start_index(
	  // and the index is (start_index,end_index(

	  // If the index of this item is 1 (i.e. C-based 0) then, we
	  // may have a start of something that is structifiable

	  // For it to work as being structifiable, we must first find
	  // a bunch of entries like this with index all 1.  Then,
	  // after that, we must either have an end to this base.  Or
	  // it must continue all over again, with the same last names
	  // of the structures, but with index 2 and so on

	  // First item cannot have controlled array!

	  bool item1_ctrl = (item._var_array_len != (uint32_t) -1 &&
			     item._var_ctrl_name);

	  if (index == 1)
	    {
	      stage_array_item_map suba;

	      stage_array_item sub_item = item;

	      sub_item._var_name = end_index;
	      suba.insert(stage_array_item_map::value_type(0,sub_item));

	      int base_length = start_index - item._var_name;

	      stage_array_item_map::iterator iter2 = iter;

	      for (++iter2; iter2 != sa.end(); ++iter2)
		{
		  stage_array_item &item2 = iter2->second;
		  uint32_t offset2 = iter2->first;

		  char *end_index2;

		  if (!match_base_index(item._var_name,base_length,
					item2._var_name,1,&end_index2))
		      break;

		  // If it has a controlling item, that must also match
		  char *end_ctrl_index2 = NULL;
		  if (item1_ctrl)
		    {
		      // first item was controlled, all must be the
		      // same

		      if (item2._var_array_len == (uint32_t) -1)
			break;
		      if (item2._var_ctrl_name &&
			  strcmp(item._var_ctrl_name,item2._var_ctrl_name))
			break;
		    }
		  else if (item2._var_array_len != (uint32_t) -1 &&
			   item2._var_ctrl_name)
		    {
		      // First item was not controlled, we must then
		      // match the base
		      if (!match_base_index(item._var_name,base_length,
					    item2._var_ctrl_name,1,
					    &end_ctrl_index2))
			break;
		    }

		  // It continues the stretch of items...

		  sub_item = item2;

		  sub_item._var_name = end_index2;
		  if (end_ctrl_index2)
		    sub_item._var_ctrl_name = end_ctrl_index2;
		  suba.insert(stage_array_item_map::value_type(offset2-offset,
							       sub_item));
		}
	      /*
	      fprintf (fid,"// struct start candidate: %s .. %s\n",
		       item._var_name,iter2->second._var_name);
	      */
	      uint32_t array_size;

	      if (iter2 == sa.end())
		{
		  stage_array_item_map::iterator iter3 = iter2;
		  --iter3;
		  array_size = iter3->first + iter3->second._length - offset;
		}
	      else
		array_size = iter2->first - offset;

	      // Ok, so now we have the candidate stretch of items.
	      // See if they repeat with further indices

	      int max_index = 2;

	      uint32_t struct_offset = offset + array_size;

	      for ( ; ; max_index++, struct_offset += array_size)
		{
		  // We loop for every index through the subitem array
		  // and the next item in the array

		  stage_array_item_map::iterator sub_iter;

		  for (sub_iter = suba.begin();
		       sub_iter != suba.end(); ++sub_iter, ++iter2)
		    {
		      if (iter2 == sa.end())
			{//fprintf (fid,"f1\n");
			goto fail_item;} // premature end, not a complete item

		      stage_array_item &item2 = iter2->second;
		      uint32_t offset2 = iter2->first;

		      char *end_index2;

		      if (!match_base_index(item._var_name,base_length,
					    item2._var_name,max_index,
					    &end_index2))
			{//fprintf (fid,"f2\n");
			goto fail_item;} // name/index mismatch

		      // ok, so right base name, right index

		      // is the remainder of the name, and any array
		      // marking the same?  and at the correct offset?

		      stage_array_item &sub_item2 = sub_iter->second;
		      uint32_t sub_offset = sub_iter->first;

		      if (strcmp(sub_item2._var_name,end_index2) != 0)
			{//fprintf (fid,"f5\n");
			goto fail_array;}

		      if (sub_item2._var_type != item2._var_type)
			{//fprintf (fid,"f6\n");
			goto fail_array;}

		      if ((sub_item2._var_type &
			   EXTERNAL_WRITER_FLAG_HAS_LIMIT) &&
			  (sub_item2._limit_min != item2._limit_min ||
			   sub_item2._limit_max != item2._limit_max))
			{//fprintf (fid,"f6a\n");
			  goto fail_array;}


		      if (sub_item2._var_array_len != item2._var_array_len)
			{//fprintf (fid,"f7\n");
			goto fail_array;}

		      if (item1_ctrl)
			{
			  // first item was controlled, all must be the
			  // same

			  if (item2._var_array_len == (uint32_t) -1)
			    {//fprintf (fid,"f8a\n");
			    goto fail_array;}
			  if (item2._var_ctrl_name &&
			      strcmp(item._var_ctrl_name,item2._var_ctrl_name))
			    {//fprintf (fid,"f8b\n");
			    goto fail_array;}
			}
		      else if (sub_item2._var_array_len != (uint32_t) -1 &&
			       item2._var_ctrl_name)
			{
			  // First item was not controlled, we must then
			  // match the base
			  if (!match_base_index(item._var_name,base_length,
						item2._var_ctrl_name,max_index,
						NULL))
			    {//fprintf (fid,"f8c\n");
			    goto fail_array;}
			}

		      if (offset2 != struct_offset + sub_offset)
			{//fprintf (fid,"f9\n");
			goto fail_array;}

		      // ok, so the item is a match.  accept it
		    }

		  // We successfully matched all the items.  Try next index
		  continue;

		fail_item:
		  if (sub_iter != suba.begin())
		    {//fprintf (fid,"f10\n");
			goto fail_array;} // failed in middle of struct
		  // ok, we failed at start of struct
		  max_index--;
		  break;
		}
	      // We matched a bunch of indices...
	      /*
	      fprintf (fid,"// struct start candidate: %s / %d [%d]\n",
		       item._var_name,suba.size(),max_index);
	      */
	      if (suba.size() > 1)
		{
		  set_strings sub_used_names;

		  fprintf (fid,"%*sstruct",indent,"");
		  if (_config._debug_header)
		    fprintf (fid," /* %04x %2d*%04x (%zd) */",
			     offset,max_index,array_size,suba.size());
		  fprintf (fid," {\n");
		  generate_structure_onion(fid,suba,indent+2,sub_used_names);

		  char *var_name = new char[base_length +
					    1 + // '\0'
					    1 + 16]; // '_' + disambig

		  sprintf (var_name,"%.*s",base_length,item._var_name);

		  unique_var_name(var_name,used_names);

		  fprintf (fid,"%*s} %s[%d];\n",
			   indent,"",var_name,max_index);

		  // This should just delete all the pointers, but as
		  // the program quits after generating the header, we
		  // ignore the memory leak :)
		  // kill_strings(sub_used_names);
		}
	      else
		{
		  if (_config._debug_header)
		    {
		      fprintf (fid,"%*s",indent,"");
		      fprintf (fid,"/* %04x %2d*%04x (%zd) */",
			       offset,max_index,array_size,suba.size());
		      fprintf (fid,"\n");
		    }
		  stage_array_item_map::iterator sub_iter = suba.begin();
		  generate_structure_item(fid,
					  sub_iter->first,sub_iter->second,
					  item._var_name,base_length,max_index,
					  indent,used_names);
		}

	      iter = iter2;
	      continue;

	    fail_array:
	      ;
	    }
	}

      generate_structure_item(fid,offset,item,NULL,0,-1,indent,used_names);

      ++iter;
    }
}

void write_structure_header(FILE *fid, global_struct *s,
			    const char *struct_name)
{
  // We do not put the typedefs in en else clause above, such that
  // if compiled on a platform where it is not true, the compiler
  // will cry.  Also, if our defines should get promoted to 64 bits,
  // the structure size will be wrong, and that's checked.

  fprintf (fid,
	   "\n"
	   "/********************************************************\n"
	   " *\n"
	   " * Plain structure (layout as ntuple/root file):\n"
	   " */\n"
	   "\n");

  fprintf (fid,
	   "typedef struct EXT_STR_%s_t\n"
	   "{\n",
	   struct_name);

  generate_structure(fid,s->_stage_array,2,false);

  fprintf (fid,
	   "\n"
	   "} EXT_STR_%s;\n",
	   struct_name);

  fprintf (fid,
	   "\n"
	   "/********************************************************\n"
	   " *\n"
	   " * Structure with multiple levels of arrays (partially)\n"
	   " * recovered (recommended):\n"
	   " */\n"
	   "\n");

  fprintf (fid,
	   "typedef struct EXT_STR_%s_onion_t\n"
	   "{\n",
	   struct_name);

  {
    set_strings used_names;

    generate_structure_onion(fid,s->_stage_array._items,2,used_names);

    // This should just delete all the pointers, but as the program
    // quits after generating the header, we ignore the memory leak :)
    // kill_strings(used_names);
  }

  fprintf (fid,
	   "\n"
	   "} EXT_STR_%s_onion;\n",
	   struct_name);

  fprintf (fid,
	   "\n"
	   "/*******************************************************/\n"
	   "\n"
	   "#define EXT_STR_%s_ITEMS_INFO(ok,si,offset,struct_t,printerr) "
	   /* */ "do { \\\n"
	   "  ok = 1; \\\n",
	   struct_name);

  generate_structure(fid,s->_stage_array,2,true);

  fprintf (fid,
	   "  \\\n"
	   "} while (0);\n");

  fprintf (fid,
	   "\n"
	   "/********************************************************\n"
	   " *\n"
	   " * For internal use by the network data reader:\n"
	   " * (version checks, etc)\n"
	   " */\n"
	   "\n");

  fprintf (fid,
	   "typedef struct EXT_STR_%s_layout_t\n"
	   "{\n",
	   struct_name);

  fprintf (fid,
	   "  uint32_t _magic;\n"
	   "  uint32_t _size_info;\n"
	   "  uint32_t _size_struct;\n"
	   "  uint32_t _size_struct_onion;\n"
	   "  uint32_t _pack_list_items;\n"
	   "\n"
	   "  uint32_t _num_items;\n"
	   "  struct {\n"
	   "    uint32_t _offset;\n"
	   "    uint32_t _size;\n"
	   "    uint32_t _xor;\n"
	   "    const char *_name;\n"
	   "  } _items[1];\n"
	   );

  fprintf (fid,
	   "  uint32_t _pack_list[%zd];",
	   s->_offset_array._length);

  fprintf (fid,
	   "\n"
	   "} EXT_STR_%s_layout;\n",
	   struct_name);

  fprintf (fid,
	   "\n"
	   "#define EXT_STR_%s_LAYOUT_INIT { \\\n",
	   struct_name);

  fprintf (fid,"  0x%08x, \\\n"
	   "  sizeof(EXT_STR_%s_layout), \\\n"
	   "  sizeof(EXT_STR_%s), \\\n"
	   "  sizeof(EXT_STR_%s_onion), \\\n"
	   "  %zd, \\\n"
	   "  %d, \\\n"
	   "  { \\\n",
	   EXTERNAL_WRITER_MAGIC,
	   struct_name,
	   struct_name,
	   struct_name,
	   s->_offset_array._length,
	   1);

  uint32_t xor_sum = calc_structure_xor_sum(s->_stage_array);

  fprintf (fid,"    { 0, sizeof(EXT_STR_%s), 0x%08x, \"%s\" }, \\\n",
	   struct_name,
	   xor_sum,
	   struct_name);

  fprintf (fid,"  }, \\\n"
	   "  { \\\n"
	   "   ");

  for (int i = 0; i < s->_offset_array._length; i++)
    {
      if ((i & 3) == 0 && i)
	fprintf (fid," \\\n   ");
      if ((i & 3) == 0)
	fprintf (fid,"/* %4d */", i);
      fprintf (fid," 0x%08x,",s->_offset_array._ptr[i]);
    }

  fprintf (fid," \\\n"
	   "  } \\\n"
	   "};\n");
}

void write_header()
{
  const char *name = _config._header_id;

  if (!name)
    {
      if (_structures.size() > 1)
	name = "all";
      else
	name = _structures[0]->_id;
    }

  char *header_guard =
    (char *) malloc(2+6+strlen(name)+1+strlen(_config._header)+2+1);

  if (!header_guard)
    ERR_MSG("Failure allocating memory for header_guard.");

  sprintf(header_guard,"__GUARD_%s_%s__",name,_config._header);

  for (char *p = header_guard; *p; p++)
    {
      if (*p == '.')
	*p = '_';
      if (*p == '/')
	*p = '_';
      *p = toupper(*p);
    }

  FILE *fid = fopen(_config._header,"wt");
  if (!fid)
    {
      perror("fopen");
      ERR_MSG("Failure opening %s for writing.",_config._header);
    }

  fprintf (fid,
	   "/********************************************************\n"
	   " *\n"
	   " * Structure for ext_data_fetch_event() filling.\n"
	   " *\n"
	   " * Do not edit - automatically generated.\n"
	   " */\n"
	   "\n"
	   "#ifndef %s\n"
	   "#define %s\n"
	   "\n"
	   "#ifndef __CINT__\n"
	   "# include <stdint.h>\n"
	   "#else\n"
	   "/* For CINT (old version trouble with stdint.h): */\n"
	   "# ifndef uint32_t\n"
	   "typedef unsigned int uint32_t;\n"
	   "typedef          int  int32_t;\n"
	   "# endif\n"
	   "#endif\n"
	   "#ifndef EXT_STRUCT_CTRL\n"
	   "# define EXT_STRUCT_CTRL(x)\n"
	   "#endif\n",
	   header_guard,
	   header_guard);

  for (global_struct_vector::iterator iter = _structures.begin();
       iter != _structures.end(); ++iter)
    {
      global_struct *s = *iter;

      if (_config._header_id_orig &&
	  strcmp(_config._header_id_orig, s->_id) != 0)
	continue;

      const char *struct_name = _config._header_id;

      if (!struct_name)
	struct_name = s->_id;

      write_structure_header(fid, s, struct_name);
    }

  fprintf (fid,
	   "\n"
	   "#endif/*%s*/\n"
	   "\n"
	   "/*******************************************************/\n",
	   header_guard);

  fclose(fid);

  free(header_guard);

  MSG ("Wrote structure header file %s.",_config._header);
}
#endif

#if STRUCT_WRITER
struct ext_data_client *_reader_client = NULL;
bool _client_written = false;
#endif

void request_setup_done(void *msg,uint32_t *left,int reader,int writer)
{
  if (_cur_structure)
    {
      /* Finalisation is done by request_array_offsets. */
      ERR_MSG("Premature setup done - structure in progress not finished.");
    }

  size_t num_trees = 0;

  for (global_struct_vector::iterator iter = _structures.begin();
       iter != _structures.end(); ++iter)
    {
      global_struct *s = *iter;
      
#if USING_ROOT
      num_trees += s->_root_ntuples.size();
#endif
    }

#if USING_ROOT
  if (num_trees >= 10)
    MSG("(... %d trees booked.)",(int) num_trees);
#else
  (void) num_trees;
#endif
#if STRUCT_WRITER
  _client_written = writer;

  if (_config._header)
    {
      if (writer)
	{
	  ERR_MSG("Data comes from client, "
		  "not enough information to generate header file.");
	}

      write_header();

      if (_config._port == 0 && !_config._stdout && !_config._forked)
	exit(0); // We're done
    }

  // If we are to run a server, start it

  if (_config._port != 0)
    {
      ext_net_io_server_bind(_config._port == -1 ?
			     EXTERNAL_WRITER_DEFAULT_PORT : _config._port);
    }

  // If we are to read data, then set the client up.  Use the client
  // reading functions.

  if (reader)
    {
      if (_structures.size() < 1)
	ERR_MSG("Cannot get event without structure defined.");

      _read_structure = _structures[0];

      global_struct *s = _read_structure;

      if (!_config._insrc)
	ERR_MSG("Input source not specified.");

      _reader_client = ext_data_connect(_config._insrc);

      if (!_reader_client)
	{
	  perror("ext_data_connect");
	  ERR_MSG("Error connecting to server '%s'.",_config._insrc);
	}

      MSG("Reading data from: %s",_config._insrc);

      struct ext_data_structure_layout slo;

      memset(&slo,0,sizeof(slo));

      slo._magic = EXTERNAL_WRITER_MAGIC;
      slo._size_info = sizeof(slo);
      slo._size_struct = slo._size_struct_onion = s->_stage_array._length;
      slo._pack_list_items = 0; // no pack list
      slo._num_items = 1;
      slo._items[0]._offset = 0;
      slo._items[0]._size = s->_stage_array._length;
      slo._items[0]._xor = s->_xor_sum;
      slo._items[0]._name = NULL;

      if (ext_data_setup(_reader_client,
			 &slo,sizeof(slo),
			 NULL, NULL,
			 s->_stage_array._length,
			 "", NULL) != 0)
	{
	  perror("ext_data_setup");
	  ERR_MSG("Failed to setup connection: %s",
		  ext_data_last_error(_reader_client));
	}

      // We are now ready to read events...
    }
#endif
}

#if STRUCT_WRITER
void dump_array_normal(global_struct *s)
{
  // Dump all entries...

  // 10 * 6 + 5 = 65 tokens, i.e. 14 left (from 79)
  // eating into the 10 available for the value itself, 24

  int cols =
    (_config._dump == EXT_WRITER_DUMP_FORMAT_NORMAL_WIDE) ? 4 : 6;
  int startcol = 80 - cols * 11 - 1;

  printf ("--- === --- === ---\n");

  for (stage_array_item_map::iterator iter = s->_stage_array._items.begin();
       iter != s->_stage_array._items.end(); ++iter)
    {
      stage_array_item &item = iter->second;
      uint32_t offset = iter->first;

      char tmp1[64];

      int items = 1;

      if (item._var_array_len != (uint32_t) -1)
	{
	  if (item._var_ctrl_name)
	    {
	      uint32_t *ctrl_p =
		(uint32_t *) (s->_stage_array._ptr + item._ctrl_offset);

	      items = *ctrl_p;

	      if (items == 0)
		continue;

	      snprintf (tmp1, sizeof(tmp1), "%s[%s]",
			item._var_name, item._var_ctrl_name);
	    }
	  else
	    {
	      items = item._var_array_len;

	      snprintf (tmp1, sizeof(tmp1), "%s[%d]",
			item._var_name, item._var_array_len);
	    }
	}
      else
	snprintf (tmp1, sizeof(tmp1), "%s", item._var_name);

      if (items == 0)
	printf ("%s =", tmp1);

      char *p = s->_stage_array._ptr + offset;

      for (int i = 0; i < items; i++, p += sizeof(uint32_t))
	{
	  char tmp2[64];

	  switch (item._var_type & EXTERNAL_WRITER_FLAG_TYPE_MASK)
	    {
	    case EXTERNAL_WRITER_FLAG_TYPE_INT32: {
	      snprintf (tmp2, sizeof(tmp2),
		       "%d",(int) *((int32_t *) p)); // PRId32
	      break;
	    }
	    case EXTERNAL_WRITER_FLAG_TYPE_UINT32: {
	      snprintf (tmp2, sizeof(tmp2),
		       "%u",(unsigned int) *((uint32_t *) p)); // PRIu32
	      break;
	    }
	    case EXTERNAL_WRITER_FLAG_TYPE_FLOAT32: {
	      float f = *((float *) p);
	      if (fabs(f) < 0.01 || fabs(f) > 1000000)
		snprintf (tmp2, sizeof(tmp2), "%.3e",f); // x.yyyE+02
	      else if (fabs(f) >= 9)
		snprintf (tmp2, sizeof(tmp2), "%.6g",f); // x.yyyyyy
	      else
		snprintf (tmp2, sizeof(tmp2), "%.6f",f); // x.yyyyyy
	      break;
	    }
	    }

	  if (i == 0)
	    {
	      size_t l1 = strlen(tmp1);
	      size_t l2 = strlen(tmp2);

	      if (l1 + 3 + l2 <= startcol + 11)
		printf ("%s = %*s",
			tmp1, (int) (startcol + 11 - l1 - 3), tmp2);
	      else
		printf ("%s =\n%*s %10s",
			tmp1, startcol, "", tmp2);
	    }
	  else
	    {
	      if (i && (i % 6) == 0)
		printf ("\n%*s", startcol, "");

	      printf (" %10s", tmp2);
	    }
	}

      printf ("\n");
    }
  fflush(stdout);
}

void dump_array_json(global_struct *s)
{
  // Dump all entries...

  bool pretty = _config._dump == EXT_WRITER_DUMP_FORMAT_HUMAN_JSON;
  
  printf ("{");
  if (pretty) printf("\n");

  stage_array_item_map::iterator last = --s->_stage_array._items.end();
  
  for (stage_array_item_map::iterator iter = s->_stage_array._items.begin();
       iter != s->_stage_array._items.end(); ++iter)
    {
      stage_array_item &item = iter->second;
      uint32_t offset = iter->first;

      int items = 1;

      bool is_array = item._var_array_len != (uint32_t) -1;

      if (is_array)
	{
	  if (item._var_ctrl_name)
	    {
	      uint32_t *ctrl_p =
		(uint32_t *) (s->_stage_array._ptr + item._ctrl_offset);

	      items = *ctrl_p;
	      if (items == 0)
		continue;
	    }
	  else
	    {
	      items = item._var_array_len;
	    }
	}

      printf("\"%s\":",item._var_name);

      if (pretty && strlen(item._var_name) < 30)
	printf("%*s", (int) (30-strlen(item._var_name)), "");

      char *p = s->_stage_array._ptr + offset;
      
      if (is_array) printf("[");
      for (int i = 0; i < items; i++, p += sizeof(uint32_t))
	{
	  switch (item._var_type & EXTERNAL_WRITER_FLAG_TYPE_MASK)
	    {
	    case EXTERNAL_WRITER_FLAG_TYPE_INT32: {
	      printf ("%d",(int) *((int32_t *) p)); // PRId32
	      break;
	    }
	    case EXTERNAL_WRITER_FLAG_TYPE_UINT32: {
	      printf ("%u",(unsigned int) *((uint32_t *) p)); // PRIu32
	      break;
	    }
	    case EXTERNAL_WRITER_FLAG_TYPE_FLOAT32: {
	      float f = *((float *) p);
	      printf("%g", f);
	      break;
	    }
	    }
	  if (is_array && i < items-1) printf(",");
	}
      if (is_array) printf("]");

      if (iter != last) printf(",");
      if (pretty) printf("\n");
    }
  printf("}\n");
  fflush(stdout);
}
#endif

int compare_raw_fill_list(const void *p1,const void *p2)
{
  int offset1 = ntohl(*((uint32_t*) p1)) & 0x00ffffff;
  int offset2 = ntohl(*((uint32_t*) p2)) & 0x00ffffff;

  return offset1 - offset2;
}

int _got_sigalarm_nonforked = 0;
#if USING_CERNLIB || USING_ROOT
int _got_sigalarm_timesliced = 0;
#endif
#if USING_ROOT
int _got_sigalarm_autosave = 0;
#endif

void sigalarm_handler(int sig)
{
  if (!_config._forked)
    _got_sigalarm_nonforked = 1;
#if USING_CERNLIB || USING_ROOT
  if (_config._timeslice)
    _got_sigalarm_timesliced = 1;
#endif
#if USING_ROOT
  if (_config._autosave)
    _got_sigalarm_autosave = 1;
#endif
}

int _got_sigint = 0;

void sigint_handler(int sig)
{
  _got_sigint++;

  if (_got_sigint > 2)
    signal(SIGINT,SIG_DFL);
}

#if STRUCT_WRITER
int _got_sigio = 0;

void sigio_handler(int sig)
{
  _got_sigio = 1;
}
#endif

class bucket_iter
{
public:
  bucket_iter(uint32_t bits,int index)
  {
    _bits  = bits;
    _index = index - 1;
  }

public:
  uint32_t _bits;
  uint32_t _index;

public:
  bool get_next()
  {
    _index++;

    if (!_bits)
      return false;

    while (!(_bits & 0x3f))
      {
	_bits >>= 6;
	_index += 6;
      }
    while (!(_bits & 1))
      {
	_bits >>= 1;
	_index += 1;
      }
    _bits >>= 1;
    return true;
  }

};


// Number of rounds needed for a radix sort:
//
// n/k where n are the number of bits that need to be sorted,
// k are the number of bits that get sorted each round
//
// Number of instructions used each round:
//
// K is the length of the offset array, generally K=2^k
// N is the length of the array to be sorted
// M is the size of each item
//
// Resetting offset array:  K writes
// Counting items into offset array:  N reads, N read-modify-write
// Making the pointer array (cumulative offset array): K reads, K writes
// Moving the items to the new array: M*N reads, M*N writes, N rmw
//
// Counting each memory access as the same, we have per round:
// K+N+N+K+M*N+M*N+N = 3*K+(5+2*M)*N
//
// We need n/k rounds, 2^n is usually much larger than N
//
// Setting, K = 2^k, we get  n/k*(3*2^k+(5+2*M)*N)
// and for the case here, M = 2, so
// n/k*(3*2^k+9*N)
//
// The variable that can be controlled is k.  N and n are given by the
// data.  As long as 9*N dominates, k should be made larger, as any
// increase in k that produces less rounds in total reduces the amount
// of work done.  Conversely, when the setting and resetting of the k
// array starts to dominate, k is too large.  With some special
// handling of the last round, some work can be saved on the k
// calculations, possibly saving one round of work.
//
// The last round can be covered by q=n-k*floor(n/k) bits of comparison,
// and even just qq=nn / 2^(k*floor(n/k)) items in the elements array.
// Here nn is the range of values, with 2^n >= nn and 2^(n-1) < nn.
// floor(n/k)*(3*2^k+(5+2*M)*N)+(3*(nn/2^(k*floor(n/k)))+(5+2*M)*N)
//
// Similar problems: SST sorting, sort 64 13-bit values.  N=64, n=13
// M=1 => 13/k*(3*2^k+7*64) = 13/k*(3*2^k+448)
// taking the effects of finite numbers into account:
// radix bits 6 and 7 => 2*7*64 + 3*(2^6+2^7) = 1472
// radix bits 4, 4, and 5 => 3*7*64 + 3*(2^4+2^4+2^5) = 1536
//
// if N is very small compared to n, then a radix sort cannot compete
// with more usual comparison sorts.  using quick-sort has the
// disadvantage if it being data-dependent and thus prone to
// branching.  a (potentially) branch-less alternative is odd-even
// merge-sort It becomes branch-less if the cpu has the necessary
// predicated instructions for moving data.  It needs to do (log_2
// N)^2 passes over the data, each pass over (almost) all N items.
// Each pass reads N items, does N/2 compares, and makes N writes,
// i.e. we have 2 * N * (log_2 N)^2 memory operations.  For the SST
// case, we would end up at 576 (543 in reality) * 4 = 2304 (2172)
// memory operations, which does not beat the radix sort.
//
// Another issue with the odd-even merge sort is that its stages (or
// rounds) are quite different, making the control loops short and
// thus increasing their relative overhead.

// Radix sort minimum.  Treat it as a continous function,
// differentiate wrt k: 3n*(2^k*(k*ln 2-1)-3*N)/k^2.
// Find zero - 3n*(2^k*(k*ln 2-1)-3*N)/k^2 == 0,
// 2^k*(k*ln 2-1)-3*N == 0
//
// The two (three) first of the three (four) loops in radix sort are
// independent, i.e. can be run in parallel.  This superficially only
// looks like it saves the N reads of the first loop.  But one also
// has to take the looping overhead into account.  The count per the
// first round are then (including one unit for loop overhead):
// K+K+N+N+N+K+K+M*N+M*N+N+N = 5*K+(7+2*M)*N And for subsequent
// (parallellised) rounds: K+N+N+K+M*N+M*N+N+N = 3*K+(6+2*M)*N

// Recalculate the SST effort with these numbers (assuming the 7-loops
// over K are unrolled to only have the 6-loop's overhead):
// radix bits 6 and 7 => (9+8)*64 + 5*2^6+3*2^7 = 1792
// radix bits 5, 4 and 4 => (9+8+8)*64 + 5*2^4+3*(2^4+2^5) = 1824
// (also in this case, one should run with just two rounds)



#define RADIX_K    6
#define RADIX_MASK ((1<<RADIX_K)-1)

void radix_sort(uint32_t *src,
		uint32_t *dest,
		size_t n,size_t koff)
{
  uint elements[RADIX_K];

  memset(elements,0,sizeof(elements));

  uint32_t *p = src;

  for (size_t i = 0; i < n; i++)
    {
      uint32_t offset = ntohl(*p);
      p += 2;

      int bucket = (offset >> koff) & RADIX_MASK;

      elements[bucket]++;
    }

  uint32_t *d[RADIX_K];

  d[0] = dest;

  for (int i = 0; i < RADIX_K-1; i++)
    d[i+1] = d[i] + elements[i]*2;

  assert(d[RADIX_K-1] + elements[RADIX_K-1]*2 == dest + n*2);

  p = src;

  for (size_t i = 0; i < n; i++)
    {
      uint32_t offset = ntohl(*p);

      int bucket = (offset >> koff) & RADIX_MASK;

      *(d[bucket]++) = *(p++);
      *(d[bucket]++) = *(p++);
    }



}







void request_keep_alive(ext_write_config_comm *comm,
			void *msg,uint32_t *left)
{
  comm->_raw_sort_u32 = get_buf_raw_ptr(&msg,left,_g._sort_u32_words);
  comm->_keep_alive_event = 1;
}

void prehandle_keep_alive(ext_write_config_comm *comm,
			  void *msg,uint32_t *left)
{
  comm->_raw_sort_u32 = get_buf_raw_ptr(&msg,left,_g._sort_u32_words);
  comm->_keep_alive_event = 1;
}

void prehandle_ntuple_fill(ext_write_config_comm *comm,
			   void *msg,uint32_t *left)
{
  comm->_raw_sort_u32 = get_buf_raw_ptr(&msg,left,_g._sort_u32_words);
  comm->_keep_alive_event = 0;
}

void request_ntuple_fill(ext_write_config_comm *comm,
			 void *msg,uint32_t *left,
			 external_writer_buf_header *header, uint32_t length,
			 bool from_merge)
{
  uint32_t *raw_sort_u32 = NULL;
  uint32_t *raw_ptr = NULL;
  uint32_t  raw_words = 0;

#if STRUCT_WRITER
  send_item_chunk *chunk = NULL;
#endif

#if USING_CERNLIB || USING_ROOT
  if (_got_sigalarm_timesliced)
    {
      // there is a race condition with the timer, but the only thing
      // that could happen is that we miss to do an update
      _got_sigalarm_timesliced = 0;

      time_t now = time(NULL);

      if (now < _ts._last_slicetime)
	{
	  // This is likely to create a lot of problems, as the files
	  // will not be in order.  Either we could just print a
	  // warning and say ok = 1 and hope for the best...  Hmmm.
	  // Let's print warnings every second instead.  User better
	  // notice and restart...  I.e. we will by ourselves not
	  // arrange for a new file until new time is larger than old
	  // file time.
	  MSG("Timeslicing problem - time went backwards.  "
	      "Will not create new files until old time passed.  "
	      "You probably want to act on stored files and restart.");
	}

      if (now >= _ts._last_slicetime + _config._timeslice)
	{
	  // Close current file
	  close_output();

	  // Open another file
	  do_file_open(now);

	  // Loop over all structures
	  for (global_struct_vector::iterator iter = _structures.begin();
	       iter != _structures.end(); ++iter)
	    {
	      global_struct *s = *iter;
	  
	      do_book_ntuple(s, -1);

	      // Create the items again
	      for (stage_array_item_vector::iterator iter =
		     s->_stage_array._items_v.begin();
		   iter != s->_stage_array._items_v.end(); ++iter)
		{
		  stage_array_item &item = *iter;

		  do_create_branch(s, item._offset,item);
		}
	    }

	  // Ready to store data in the new file...
	}
    }
#endif

  uint32_t *msgstart = (uint32_t *) msg;
  uint32_t msglen   = *left;

  // MSG("left %d.",*left);

  raw_sort_u32 = get_buf_raw_ptr(&msg,left,_g._sort_u32_words);

  if (comm)
    {
      comm->_raw_sort_u32 = raw_sort_u32;
      comm->_keep_alive_event = 0;
    }

  uint32_t struct_index = get_buf_uint32(&msg,left);
  uint32_t ntuple_index = get_buf_uint32(&msg,left);

  if (struct_index >= _structures.size())
    ERR_MSG("Structure index (%d) too large (>= %d).",
	    struct_index,(int) _structures.size());

  global_struct *s = _structures[struct_index];

  if (s->_max_raw_words)
    {
      raw_words = get_buf_uint32(&msg,left);
      raw_ptr = get_buf_raw_ptr(&msg,left,raw_words);
    }

  uint32_t marker = get_buf_uint32(&msg,left);
  uint32_t compact_marker = marker & (EXTERNAL_WRITER_COMPACT_PACKED |
				      EXTERNAL_WRITER_COMPACT_NONPACKED);

  if (compact_marker != EXTERNAL_WRITER_COMPACT_PACKED &&
      compact_marker != EXTERNAL_WRITER_COMPACT_NONPACKED)
    {
	ERR_MSG("Compact marker invalid (0x%08x) .", compact_marker);
    }

  // MSG("index %d marker %d left %d.",ntuple_index,marker,*left);

  if (!s->_stage_array._length)
    ERR_MSG("Cannot fill using unallocated array.");

  if (_config._ts_merge_window &&
      (marker & EXTERNAL_WRITER_COMPACT_PACKED))
    ERR_MSG("Cannot merge (time_stitch) from compacted array.");

  if (!(marker & EXTERNAL_WRITER_COMPACT_PACKED))
    {
      // Non-compacted array.

      if (*left & (sizeof(uint32_t)-1))
	ERR_MSG("Raw fill list malformed, size not multiple of %zd.",
		sizeof(uint32_t));

      ////////////////////////////////////////

#if STRUCT_WRITER
      if (_client_written)
	{
	  // the reader client did not send any unpack info
	  *left = 0;
	  goto statistics;
	}
#endif

      // The data mst be unpacked using the static information we have
      // about offsets.  Only the values are sent each time.

      uint32_t *o    = s->_offset_array._ptr;
      uint32_t *oend = s->_offset_array._ptr + s->_offset_array._length;

      uint32_t *pstart = (uint32_t *) msg;
      uint32_t plen   = *left;

      uint32_t *p    = pstart;
      uint32_t *pend = (uint32_t *) (((char*) pstart) + plen);

#if STRUCT_WRITER
      uint32_t *pp    = (uint32_t *) s->_stage_array._offset_value;
      uint32_t *ppp   = (uint32_t *) pp;
#endif

      // instead of checking the p vs. pend every time, we know the
      // number of static items, and check for that availability
      // first.  then every time we end up inside a controlled item,
      // we check that those items are also available.

      if (pend - p < s->_offset_array._static_items)
	ERR_MSG("Fill value array too short (%zd) "
		"for offset array static items (%" PRIu32 ").",
		pend - p,s->_offset_array._static_items);

      uint32_t *pcheck = p + s->_offset_array._static_items;

      // MSG ("---");

      while (o < oend)
	{
	  uint32_t mark   = *(o++);
	  uint32_t offset = *(o++);
	  uint32_t loop = mark & EXTERNAL_WRITER_MARK_LOOP;
	  //uint32_t coffset = ntohl(offset);
	  uint32_t value = ntohl(*(p++));

	  // MSG("%08x : %08x : %08x",offset,offset,value);

#if STRUCT_WRITER
	  *(ppp++) = offset;
	  /**(ppp++) = value;*/
	  *((uint32_t *) (s->_stage_array._ptr + offset)) = value;
#else
	  *((uint32_t *) (s->_stage_array._ptr + offset)) = value;
#endif

	  // MSG ("value: (0x%08x) 0x%08x %c",offset,value,mark ? '*' : ' ');

	  // was it an controlling variable?

	  if (loop)
	    {
	      uint32_t max_loops = *(o++);
	      uint32_t loop_size = *(o++);

	      // MSG ("               %d * %d",max_loops,loop_size);

	      uint32_t *onext = o + 2 * max_loops * loop_size;

	      if (value > max_loops)
		ERR_MSG("Fill ctrl item at offset %d "
			"has too large value (%d > %d).",
			offset,value,max_loops);

	      uint32_t items = value * loop_size;

	      // MSG ("               %d * %d => %d",value,loop_size,items);

	      // onext has already been checked in request_array_offsets()

	      if (pend - pcheck < items)
		ERR_MSG("Fill value array too short (%zd) "
			"for offset array controlled items (%" PRIu32 "*"
			"%" PRIu32 "=%" PRIu32 ").",
			pend - pcheck,value,loop_size,items);

	      pcheck += items;

	      // The value tells how many loops we want to do in the
	      // array, each of a specific size.

	      for (int i = items; i; i--)
		{
		  mark   = *(o++);
		  offset = *(o++);
		  //coffset = ntohl(*(p++));
		  value = ntohl(*(p++));

		  // MSG ("cvalue: (0x%08x) 0x%08x",offset,value);

		  // MSG("%08x : %08x : %08x",coffset,offset,value);

#if STRUCT_WRITER
		  *(ppp++) = offset;
		  /**(ppp++) = value;*/
		  *((uint32_t *) (s->_stage_array._ptr + offset)) = value;
#else
		  *((uint32_t *) (s->_stage_array._ptr + offset)) = value;
#endif
		}

	      // And then jump to the end of the loop in the offset array

	      o = onext;
	    }
	}

      if (p != pend)
      //if (pend - p != p - (uint32_t *) msg /* p != pend */)
	ERR_MSG("Fill value array not completely consumed (%d left, %d used).  "
		"Desynchronised with offset array.",
		(int) (pend - p), (int) (p - (uint32_t *) msg));

      *left = 0; // consumed all data
      // msg = pend; // not really needed... (as left is 0)

#if STRUCT_WRITER
      assert(pp <= s->_stage_array._offset_value + s->_stage_array._length);
#endif
      
      // fprintf (stderr,"merge: %d\n", _config._ts_merge_window);

      if (!from_merge &&
	  _config._ts_merge_window)
	{
	  ext_merge_insert_chunk(comm,
				 &s->_offset_array,
				 msgstart,
				 (uint32_t) ((char *) pstart -
					     (char *) msgstart),
				 (uint32_t) ((char *) pend -
					     (char *) pstart),
				 s->_stage_array._length);

	  /* After the event was inserted into the merge queue,
	   * we drop it.  It will eventually come back here with
	   * from_merge set.
	   */
	  return;
	}

      ////////////////////////////////////////

#if STRUCT_WRITER
      /* Do not create the compacted data if we are not writing
       * to network.  It is just expensive.
       */
      if (_config._port != 0 ||
	  _config._bitpack)
	{
      // uint32_t *_masks[BUCKET_SORT_LEVELS];
      // int       _num_masks[BUCKET_SORT_LEVELS];

      //struct rusage use1, use2, use3, use4, use5;

      //getrusage(RUSAGE_SELF,&use1);

      memset(s->_stage_array._masks[0],0,
	     s->_stage_array._num_masks[0] * sizeof(uint32_t));

      while (pp/* + 1*/ < ppp)
	{
	  uint32_t offset = *(pp++);
	  /*uint32_t value  = *(pp++);*/

	  if (offset + sizeof(uint32_t) > s->_stage_array._length)
	    ERR_MSG("Fill item offset (%" PRIu32 ") outside array (%zd).",
		    offset,s->_stage_array._length);

	  /**((uint32_t *) (s->_stage_array._ptr + offset)) = value;*/

	  uint32_t mask_item = (offset / sizeof(uint32_t));
	  uint32_t mask_shift = (BUCKET_SORT_LEVELS - 1) * BUCKET_SORT_SHIFT;

	  // fprintf (stderr,"o%5d i%5d :",offset,mask_item);

	  // We first expect the bits to already be set
	  int level = 0;
	  for ( ; level < BUCKET_SORT_LEVELS; level++)
	    {
	      uint32_t this_mask = mask_item >> mask_shift;
	      mask_shift -= BUCKET_SORT_SHIFT;

	      uint32_t mask_off = this_mask >> BUCKET_SORT_SHIFT;
	      uint32_t mask_ind = this_mask & (BUCKET_SORT_BITS - 1);

	      uint32_t *mask_ptr = s->_stage_array._masks[level] + mask_off;
	      uint32_t mask_bit = 1 << mask_ind;

	      if (!(*mask_ptr & mask_bit))
		{
		  *mask_ptr |= mask_bit;
		  // Was not set before, we'll have to initialize all
		  // following masks!, i.e. go to next loop
		  // fprintf (stderr," %4d/%2da",mask_off,mask_ind);
		  level++;
		  break;
		}
	      // fprintf (stderr," %4d/%2d ",mask_off,mask_ind);
	    }
	  // From here on, the masks were not initialised this event
	  for ( ; level < BUCKET_SORT_LEVELS; level++)
	    {
	      uint32_t this_mask = mask_item >> mask_shift;
	      mask_shift -= BUCKET_SORT_SHIFT;

	      uint32_t mask_off = this_mask >> BUCKET_SORT_SHIFT;
	      uint32_t mask_ind = this_mask & (BUCKET_SORT_BITS - 1);

	      uint32_t *mask_ptr = s->_stage_array._masks[level] + mask_off;
	      uint32_t mask_bit = 1 << mask_ind;

	      *mask_ptr = mask_bit; // initialise

	      // fprintf (stderr," %4d/%2di",mask_off,mask_ind);
	    }
	  // fprintf (stderr,"\n");
	}

      // First sort all the entries in the fill list

      //getrusage(RUSAGE_SELF,&use2);
      // qsort(p,(end-p)/2,2 * sizeof(uint32_t),compare_raw_fill_list);
      //getrusage(RUSAGE_SELF,&use3);

      //
#if 0
      fprintf (stderr,"%4d: ",(end-p)/2);

      int prev_offset = -8;
      uint32_t *pp = p;

      for (pp ; pp < end; pp += 2)
	{
	  char type = '-';

	  if (ntohl(pp[1]) == 0) type = '0';
	  else if (ntohl(pp[1]) == 0x7fc00000) type = 'X';
	  else if (!(ntohl(pp[1]) & 0xffffff00)) type = 'b';
	  else if (!(ntohl(pp[1]) & 0xffff0000)) type = 's';

	  int offset = ntohl(pp[0]) & 0x00ffffff;

	  if (offset != prev_offset + 4)
	    fprintf (stderr," ");
	  fprintf (stderr,"%c",type);

	  prev_offset = offset;
	}
      fprintf (stderr,"\n");
#endif

      // Then, go through the bit buckets, to see what elements are of
      // interest.  By design, bits in buckets are never set for
      // entries which are outside our array, so we need not worry
      // about looking at memory too far away.

      // New compacted format:
      //
      // Packed value, telling type of data, and number of bits in
      // offset and number of bits in the data (where applicable)

      // Type [2 bits]:
      // 0 - value is zero
      // 1 - value is NaN
      // 2 - value has described number of bits
      // 3 - value is set of bytes (nibbles)
      //
      // Offset [5 bits]:
      //
      // Telling how many bits are needed to encode the offset, 0 bits
      // meaning a zero offset.  32 and 31 bits are never needed as we
      // operate with offsets in multiples of 4 bytes.
      //
      // Size [5 bits]:
      //
      // Telling how many bits-1 are needed for the value.  I.e. 0
      // means 1 bit and 31 means 32 bits.  Used for type 2.
      //
      // Nibble mask [8 bits]:
      //
      // Telling which nibbles are stored.  Used for type 3.




      //ppp = p;

      // Compacted data is a stream of bytes (no need to byteswap)
      //
      // By default, next offset is next (32-bit) location.
      //
      // [1nnnnnnn]  offset specification, n = low bits [8..2]
      // [1nnnnnnn]  higher bits for offset, n = next bits [15..9]
      // [1nnnnnnn]  higher bits for offset, n = next bits [22..16]
      //
      // [0qqknnnn]  qq=type of value
      //             qq=0 -> 5 bit value, stored in knnnn
      //             qq=1 -> 13 bit value, knnnn are bits 12..8, next byte
      //                     has the low bits
      //             qq=2 -> full value, stored in the following 4 bytes,
      //                     starting with the high byte, nnnnn contains
      //                     an optional high offset
      //             qq=3 -> special, k=1 => 0, k=0 => nan, nnnn high offset

      // The worst case size for one entry is 9 bytes, if we need to
      // do a full store.  But that only happens if the offset needs
      // >= 28 bits, i.e. is >= 256 MB...

      // We allocate the rewrite memory directly in the net I/O
      // buffers (which are there even if there are no consumers).
      // That way, we save a memcpy operation.

      size_t max_length = s->_stage_array._rewrite_max_bytes_per_item *
	((ppp - s->_stage_array._offset_value)/* / 2*/);

      max_length = (max_length + 3) & ~3; // 4-byte alignment
      max_length += sizeof (external_writer_buf_header);
      max_length +=
	(_g._sort_u32_words +
	 (s->_max_raw_words ? 1 + raw_words : 0) +
	 3) * // 3: struct_index + ntuple_index + mark_dest
	sizeof (uint32_t);

      char *net_io_chunk =
	ext_net_io_reserve_chunk(max_length,false,&chunk);

      header = (external_writer_buf_header *) net_io_chunk;

      header->_request = htonl(EXTERNAL_WRITER_BUF_NTUPLE_FILL |
			       EXTERNAL_WRITER_REQUEST_HI_MAGIC);
      header->_length = (uint32_t) -1;

      // sizeof (external_writer_buf_header)

      uint32_t *sort_u32_dest = (uint32_t*) (header + 1);
      for (uint32_t i = 0; i < _g._sort_u32_words; i++)
	sort_u32_dest[i] = raw_sort_u32[i];

      uint32_t *struct_index_dest =
	sort_u32_dest + _g._sort_u32_words;

      *struct_index_dest = htonl(struct_index);

      uint32_t *ntuple_index_dest = struct_index_dest + 1;

      *ntuple_index_dest = htonl(ntuple_index);

      uint32_t *raw_dest = ntuple_index_dest + 1;

      if (s->_max_raw_words)
	{
	  *(raw_dest++) = htonl(raw_words);
	  memcpy(raw_dest, raw_ptr,
		 sizeof(uint32_t) * raw_words);

	  raw_dest += raw_words;
	}

      uint32_t *mark_dest = raw_dest;
      uint8_t *dest = (uint8_t *) (mark_dest + 1);

      uint32_t cur_offset = 0;

      for (int i = 0; i < s->_stage_array._num_masks[0]; i++)
	{
	  // fprintf (stderr,"[0:%d]",i);

	  bucket_iter iter0(s->_stage_array._masks[0][i],
			    i*BUCKET_SORT_BITS);

	  while (iter0.get_next())
	    {
	      // so, this bit (@index) is (was) set, go into the next bucket

	      // fprintf (stderr,"(1:%d)",iter0._index);

	      bucket_iter
		iter1(s->_stage_array._masks[1][iter0._index],
		      iter0._index*BUCKET_SORT_BITS);

	      while (iter1.get_next())
		{
		  // fprintf (stderr,"(2:%d)",iter1._index);

		  bucket_iter
		    iter2(s->_stage_array._masks[2][iter1._index],
			  iter1._index*BUCKET_SORT_BITS);

		  while (iter2.get_next())
		    {
		      // Item at index  iter2._index

		      //assert(ppp < end);

		      // uint32_t offset = ntohl(ppp[0]);
		      /*
		      fprintf (stderr,"%d %d\n",
			       iter2._index * 4,ntohl(ppp[0]));
		      */
		      uint32_t offset = iter2._index * sizeof (uint32_t);
		      uint32_t value  =
			*((uint32_t *) (s->_stage_array._ptr + offset));
		      /*
		      if (iter2._index * 4 != ntohl(ppp[0]) ||
			  value != ntohl(ppp[1]))
			ERR_MSG("%d - %d   | %x - %x\n",
				iter2._index * 4,ntohl(ppp[0]),
				value,ntohl(ppp[1]));
		      */
		      //assert (offset == ntohl(ppp[0]));
		      //assert (value  == ntohl(ppp[1]));
		      //ppp += 2;

		      // uint32_t offset = ntohl(p[0]);
		      // uint32_t value  = ntohl(p[1]);

		      ////////////////////////////////////////////
		      // Begin encoding this value

		      // uint8_t *odest = dest;

		      uint32_t store_offset = offset - cur_offset;

		      assert(!(store_offset & 3));
		      store_offset >>= 2;

		      assert(offset + sizeof(uint32_t) <=
			     s->_stage_array._length);
			     /*
		      if (offset + sizeof(uint32_t) > _stage_array._length)
			ERR_MSG("Fill item offset (%d) outside array (%d).",
				offset,_stage_array._length);
			     */
		      cur_offset = offset + sizeof(uint32_t);

		      if (value == 0)
			{
			  while (store_offset > 0x0f)
			    {
			      *(dest++) = 0x80 | (store_offset & 0x7f);
			      store_offset >>= 7;
			    }
			  *(dest++) = (3 << 5) | (1 << 4) | store_offset;
			}
		      else if (value == 0x7fc00000) // NAN
			{
			  while (store_offset > 0x0f)
			    {
			      *(dest++) = 0x80 | (store_offset & 0x7f);
			      store_offset >>= 7;
			    }
			  *(dest++) = (3 << 5) | store_offset;
			}
		      else if (!(value & ~0x0000001f)) // 5 bits
			{
			  while (store_offset)
			    {
			      *(dest++) = 0x80 | (store_offset & 0x7f);
			      store_offset >>= 7;
			    }
			  *(dest++) = (0 << 5) | value;
			}
		      else if (!(value & ~0x00001fff)) // 13 bits
			{
			  while (store_offset)
			    {
			      *(dest++) = 0x80 | (store_offset & 0x7f);
			      store_offset >>= 7;
			    }
			  *(dest++) = (1 << 5) | (uint8_t) (value >> 8);
			  *(dest++) = (uint8_t) value;
			}
		      else
			{
			  while (store_offset > 0x01f)
			    {
			      *(dest++) = 0x80 | (store_offset & 0x7f);
			      store_offset >>= 7;
			    }
			  *(dest++) = (2 << 5) | store_offset;
			  *(dest++) = (uint8_t) (value >> 24);
			  *(dest++) = (uint8_t) (value >> 16);
			  *(dest++) = (uint8_t) (value >> 8);
			  *(dest++) = (uint8_t) value;
			}

		      // fprintf (stderr," %d",dest-odest);

		      // End encoding this value
		      ////////////////////////////////////////////
		    }
		}
	    }
	}

      // fprintf (stderr,"-----------------------------------\n");

      //assert(ppp == end);

      //getrusage(RUSAGE_SELF,&use4);

      // If we did not exactly reach the end, add a dummy entry
      // that writes a zero to the last item.  This only happens
      // if we employ zero suppression, so it's anyhow not to be
      // used.  We do this such that the unpacking can verify that
      // at least all the offsets worked out together.

      if (cur_offset != s->_stage_array._length)
	{
	  uint32_t offset = s->_stage_array._length - sizeof(uint32_t);
	  uint32_t store_offset = offset - cur_offset;

	  assert(!(store_offset & 3));
	  store_offset >>= 2;

	  while (store_offset > 0x0f)
	    {
	      *(dest++) = 0x80 | (store_offset & 0x7f);
	      store_offset >>= 7;
	    }
	  *(dest++) = (3 << 5) | (1 << 4) | store_offset;
	}

      uint32_t compact_len = ((char *) dest - (char *) (mark_dest + 1));

      *mark_dest = htonl(EXTERNAL_WRITER_COMPACT_PACKED | compact_len);

      // Pad with zeros (to make comparisons work)

      memset(dest,0,(-((char *) dest - (char *) header)) & 3);

      uint32_t new_length =
	((((char *) dest - (char *) header) + 3) & ~3);
      /*
      MSG("rewrite: %d items (%d bpi), %d / nl: %d ml: %d",
	  (ppp - _stage_array._offset_value) / 2,
	  _stage_array._rewrite_max_bytes_per_item,
	  ((char *) dest - (char *) (mark_dest + 1)),
	  new_length,max_length);
      */
      assert(new_length <= max_length);
      header->_length = htonl(new_length);

      length = new_length;

      // note, we cannot commit the chunk yet, as we also intend to
      // use it as a source if writing to stdout.  And after
      // committing, it may be freed by the net I/O system.  (Well,
      // not really, as all free'ing is done by the allocation routine
      // - but anyhow, would invite for bugs should that change).

#if 0
      int ret = ext_data_write_bitpacked_event((char *) s->_stage_array._ptr,
					       s->_stage_array._length,
					       (uint8_t *) (mark_dest+1),
					       (uint8_t *) dest);

      if (ret != 0)
	ERR_MSG("Test unpack of compacted event failed (ret=%d).",ret);
#endif

      //getrusage(RUSAGE_SELF,&use5);
      /*
      static double bucket = 0;
      static double sort = 0;
      static double debckt = 0;
      static double pack = 0;
      double total;

      double t1, t2, t3, t4, t5;

      t1 = use1.ru_utime.tv_sec + 0.000001 * use1.ru_utime.tv_usec;
      t2 = use2.ru_utime.tv_sec + 0.000001 * use2.ru_utime.tv_usec;
      t3 = use3.ru_utime.tv_sec + 0.000001 * use3.ru_utime.tv_usec;
      t4 = use4.ru_utime.tv_sec + 0.000001 * use4.ru_utime.tv_usec;
      t5 = use5.ru_utime.tv_sec + 0.000001 * use5.ru_utime.tv_usec;

      bucket += (t2-t1);
      sort   += (t3-t2);
      debckt += (t4-t3);
      pack   += (t5-t4);
      total   = t5;

      if (_num_events % 10000 == 0)
	{
	  MSG("%6.2f(%4.1f%%) %6.2f(%4.1f%%) | %6.2f(%4.1f%%) %6.2f(%4.1f%%) / %6.2f",
	      bucket,bucket/total*100,
	      debckt,debckt/total*100,
	      sort,sort/total*100,
	      pack,pack/total*100,
	      total);
	}
      */

      /*
      printf ("---> %d %d\n",
	      ((char *) end) - ((char*) msg),header_dest->_length);
      */
	}
#endif
    }
  else
    {
      // Compacted data.

      uint32_t real_len = marker & 0x3fffffff;

      if (real_len > *left)
	ERR_MSG("Raw fill compacted data malformed, size %d > length %d.",
		real_len,*left);

      // Also for struct_writer we unpack, just to check that the data
      // is correct

      int ret = ext_data_write_bitpacked_event((char *) s->_stage_array._ptr,
					       s->_stage_array._length,
					       (uint8_t *) msg,
					       (uint8_t *) msg + real_len);

      if (ret != 0)
	ERR_MSG("Compacted event failed to unpack (ret=%d).",ret);

      *left = 0;
    }

#if USING_CERNLIB
  s->_cwn->hfnt();
#endif
#if USING_ROOT
  if (ntuple_index >= s->_root_ntuples.size())
    ERR_MSG("Ntuple index (%d) too large (>= %d).",
	    ntuple_index,(int) s->_root_ntuples.size());
  Int_t ret = s->_root_ntuples[ntuple_index]->Fill();
  if (ret < 0)
    ERR_MSG("Tree filling failed.  (disk full?)");
  if (_got_sigalarm_autosave)
    {
      _got_sigalarm_autosave = 0;

      time_t now = time(NULL);

      if (now >= _ts._last_autosave + _config._autosave)
	{
	  struct flock fl;
	  int fd = _g._root_file->GetFd();
	  int lock_ret = 0;

	  /* Locking was conjured up to read data from a somewhat reasonable
	   * ROOT file while it's being written.
	   */

	  /* Try to acquire an exclusive advisory lock.  If we have
	   * not acquired the lock within 1 s, the alarm timer will
	   * wake us up.  In that case, we postpone the autosave until
	   * later.  If the autosave has been postponed three times,
	   * print a warning.  (Likely some reader has gotten stuck.)
	   */

	  if (_ts._autosave_lock_do) {
	    fl.l_type = F_WRLCK;
	    fl.l_whence = SEEK_SET;
	    fl.l_start = 0;
	    fl.l_len = 1;

	    lock_ret = fcntl(fd, F_SETLKW, &fl);

	    if (lock_ret == -1)
	      {
	        if (errno == EINTR)
	          {
		    _ts._autosave_lock_fail++;

		    /* There is no way for us to remove the lock.
		     * So we report it until the problem has been solved.
		     *
		     * We could decide to ignore it, but that has the
		     * side-effect of protection not working as intended.
		     * Then choosing to not do updates.
		     */

		    if (_ts._autosave_lock_fail >= 3)
		      {
			MSG("Lock before AutoSave has failed multiple (%d) "
			  "times in a row.  Not doing AutoSave.",
			  _ts._autosave_lock_fail);
		      }
		  }
	        else
	          {
		    /* Locking failed badly, don't bother to lock anymore. */
	            perror("fcntl(F_SETLK, F_UNLCK)");
	            MSG("Unexpected error locking before AutoSave, will not "
		      "lock.");
	            _ts._autosave_lock_do = 0;
	          }
	      }
	  }

	  if (lock_ret != -1 || !_ts._autosave_lock_do)
	    {
	      if (_ts._autosave_lock_fail >= 3)
		MSG("Lock before AutoSave succeeded.");

	      _ts._autosave_lock_fail = 0;

	      /* We got the lock.  Use it */

	      _ts._last_autosave = now;

	      for (TTree_vector::iterator iter = s->_root_ntuples.begin();
		   iter != s->_root_ntuples.end(); ++iter)
		(*iter)->AutoSave("saveself,flushbaskets");

	      if (_ts._autosave_lock_do) {
	        /* Then unlock again. */

	        fl.l_type = F_UNLCK;

	      unlock:
	        lock_ret = fcntl(fd, F_SETLK, &fl);

	        if (lock_ret == -1)
		  {
		  // This should not happen!  (F_SETLK)
#if 0
		  if (errno == EINTR)
		    {
		      MSG("Unlock after AutoSave interrupted by signal.  "
			  "Trying again.");
		      /* Releasing a lock really should work! */
		      goto unlock;
		    }
#endif

		    perror("fcntl(F_SETLK, F_UNLCK)");
		    ERR_MSG("Unexpected error unlocking after AutoSave.");
		  }
		}
	    }
	}
    }
#endif
#if STRUCT_WRITER
  if (_config._dump == EXT_WRITER_DUMP_FORMAT_NORMAL ||
      _config._dump == EXT_WRITER_DUMP_FORMAT_NORMAL_WIDE)
    dump_array_normal(s);
  else if (_config._dump == EXT_WRITER_DUMP_FORMAT_HUMAN_JSON ||
	   _config._dump == EXT_WRITER_DUMP_FORMAT_COMPACT_JSON)
    dump_array_json(s);

  if (!chunk)
    {
      // We have not compacted the data, so emit as it is.

      char *net_io_chunk =
	ext_net_io_reserve_chunk(length,
				 false /* request <
					  EXTERNAL_WRITER_BUF_NTUPLE_FILL */,
				 &chunk);

      memcpy(net_io_chunk,header,length);
    }

  if (_config._stdout)
    full_write(STDOUT_FILENO,header,length);

  ext_net_io_commit_chunk(length,chunk);
#endif

 statistics:

  _g._num_events++;

  if (_got_sigalarm_nonforked)
    {
      // there is a race condition with the timer, but the only thing
      // that could happen is that we miss to do an update
      _got_sigalarm_nonforked = 0;

      fprintf(stderr,"%lld events",(long long int) _g._num_events);
#if STRUCT_WRITER
      fprintf(stderr,", (%.1f %cB, %.1f %cB to clients, now %d)",
	      _net_stat._committed_size > 2000000000 ?
	      (double) _net_stat._committed_size / 1000000000. :
	      (double) _net_stat._committed_size / 1000000.,
	      _net_stat._committed_size > 2000000000 ? 'G' : 'M',
	      _net_stat._sent_size > 2000000000 ?
	      (double) _net_stat._sent_size / 1000000000. :
	      (double) _net_stat._sent_size / 1000000.,
	      _net_stat._sent_size > 2000000000 ? 'G' : 'M',
	      _net_stat._cur_clients);
#endif
      fprintf(stderr,"         \r");
      fflush(stderr);
    }
}

bool ntuple_get_event(char *msg,char **end)
{
  // TODO: Move this check to a common place?
  // I.e. need not be done every time?

  global_struct *s = _read_structure;

  external_writer_buf_header *header = (external_writer_buf_header*) msg;

  // Now, fetch the event

#if USING_ROOT
  int ret;

  if (_g._num_events >= _g._num_read_entries)
    goto read_done;

  ret = s->_root_ntuple->GetEntry(_g._num_events);

  if (ret == 0)
    {
      MSG("Entry (event) %" PRIu64 " does not exist in ntuple(tree).",
	  _g._num_events);
      goto read_abort;
    }
  else if (ret == -1)
    {
      MSG("Error while reading entry (event) %" PRIu64 " from ntuple(tree).",
	  _g._num_events);
      goto read_abort;
    }
  _g._num_events++;
#endif
#if STRUCT_WRITER
  /* If we are bit-packed, we must unpack into the staging area
   * and then recover to the normal format.  (only happens if data
   * came from a server).  If we are in the normal format, just
   * copy.
   */

  struct external_writer_buf_header *header_in;
  uint32_t length_in;

  int ret = ext_data_fetch_event(_reader_client,
				 s->_stage_array._ptr,s->_stage_array._length,
				 &header_in,&length_in);

  switch (ret)
    {
    default: // should not happen
      assert(false);
    case -1:
      perror("ext_data_fetch_event");
      ERR_MSG("Failed to fetch event: %s",ext_data_last_error(_reader_client));
      break; // unreachable
    case 0:
    case 2:
      // The data is in a nice format already.  Just copy the full
      // packet.  (which is in network order! already) hehe

      if (length_in > *end - msg)
	{
	  ERR_MSG("Fetched message (0x%08x=%d) too long (invalid/corrupt) "
		  "(%" PRIu32 " > %zd)."/*" %08x %08x %08x %08x"*/,
		  ntohl(header_in->_request),
		  ntohl(header_in->_request) & EXTERNAL_WRITER_REQUEST_LO_MASK,
		  length_in, *end - msg/*,
		  ntohl(((uint32_t *) header_in)[0]),
		  ntohl(((uint32_t *) header_in)[1]),
		  ntohl(((uint32_t *) header_in)[2]),
		  ntohl(((uint32_t *) header_in)[3])*/);
	}

      memcpy(msg,header_in,length_in);
      *end = msg + length_in;

      return (ret == 2); // ret == 0 means we just copied the done/abort msg
    case 1:
      // The data got unpacked.  Just fall through to the packaging.
      break;
    }
#endif
  {
    uint32_t *start = (uint32_t *) (header + 1);

    start[0] = htonl(0); // struct_index (0, only one when reading)
    start[0] = htonl(0); // ntuple_index (0, only one when reading)
    start[1] = htonl(EXTERNAL_WRITER_COMPACT_NONPACKED);

    uint32_t *cur = start + 3;

    // This has filled our staging array.  Now walk our offset pointers
    // and copy the data.

    uint32_t *o    = s->_offset_array._ptr;
    uint32_t *oend = s->_offset_array._ptr + s->_offset_array._length;

    while (o < oend)
      {
	uint32_t mark   = *(o++);
	uint32_t offset = *(o++);
	uint32_t loop = mark & EXTERNAL_WRITER_MARK_LOOP;

	uint32_t value = *((uint32_t *) (s->_stage_array._ptr + offset));

	// Even if an error is detected, we'll not write the event
	// anyhow
	*(cur++) = htonl(value);

	if (loop)
	  {
	    // It's a loop control.  Make sure the value was within
	    // limits.

	    uint32_t max_loops = *(o++);
	    uint32_t loop_size = *(o++);

	    // This will put an end to things!
	    if (value > max_loops)
	      {
		// Don't do ERR_MSG, as we want to write all the
		// successfully handled events, and then end it with an
		// ABORT message.
		MSG("Fill ctrl item at offset %d"
		    " has too large value (%d > %d).",
		    offset,value,max_loops);
		goto read_abort;
	      }

	    uint32_t *onext = o + 2 * max_loops * loop_size;
	    uint32_t items = value * loop_size;

	    for (int i = items; i; i--)
	      {
		offset = (*(o++)) & 0x3fffffff;
		value = *((uint32_t *) (s->_stage_array._ptr + offset));

		*(cur++) = htonl(value);
	      }
	    o = onext;
	  }
      }

    // Done.

    header->_request = htonl(EXTERNAL_WRITER_BUF_NTUPLE_FILL |
			     EXTERNAL_WRITER_REQUEST_HI_MAGIC);
    header->_length = htonl(((char*) cur) - ((char*) msg));

    *end = (char *) cur;
    return true;
  }

 read_abort:
  header->_request = htonl(EXTERNAL_WRITER_BUF_ABORT |
			   EXTERNAL_WRITER_REQUEST_HI_MAGIC);
  goto read_header_only;

 read_done:
  header->_request = htonl(EXTERNAL_WRITER_BUF_DONE |
			   EXTERNAL_WRITER_REQUEST_HI_MAGIC);

 read_header_only:
  header->_length = htonl(sizeof(external_writer_buf_header));
  *end = (char *) (header + 1);
  return false;
}




void write_response(ext_write_config_comm *comm, char response)
{
  for ( ; ; )
    {
      int ret = write(comm->_pipe_out,&response,sizeof(response));

      if (ret == -1)
	{
	  if (errno == EINTR)
	    continue; // try again
	  perror("write");
	  ERR_MSG("Unexpected error writing to command pipe.");
	}
      if (!ret)
	ERR_MSG("Could not write to command pipe, eof.");
      break;
    }
}

void full_write(int fd,const void *buf,size_t count)
{
  for ( ; ; )
    {
      ssize_t nwrite = write(fd,buf,count);

      if (!nwrite)
	ERR_MSG("Reached unexpected end of file while writing.");

      if (nwrite == -1)
	{
	  if (errno == EINTR)
	    nwrite = 0;
	  else
	    {
	      perror("write");
	      ERR_MSG("Error writing file.");
	    }
	}

      count -= nwrite;

      if (!count)
	break;
      buf = ((const char*) buf)+nwrite;
    }
}

// Shared memory circular message buffer (from producer!)

struct shared_mem_circular
{
  char   *_ptr;
  size_t  _len;

  external_writer_shm_control *_ctrl;
  char  *_begin;
  char  *_end;
  char  *_cur;
  size_t _size;

  char  *_mem;
  char  *_read_end;
};

/*
shared_mem_circular _shmc = {
  NULL, 0,
  NULL, NULL, NULL, NULL, 0,
  NULL, NULL
};
*/

external_writer_buf_header *
request_resize_shm(ext_write_config_comm *comm,
		   void *msg,uint32_t *left,
		   external_writer_buf_header *header,
		   size_t *expand)
{
  shared_mem_circular *shmc = comm->_shmc;

  uint32_t new_len = get_buf_uint32(&msg,left);
  uint32_t magic = get_buf_uint32(&msg,left);

  if (magic != EXTERNAL_WRITER_MAGIC)
    ERR_MSG("Request to resize shared memory has bad magic.");

  if (new_len < shmc->_len)
    ERR_MSG("Request to decrease shared memory not allowed.");

  // check that the shared memory has the required size

  struct stat st;

  /*int r =*/ fstat(comm->_shm_fd, &st);

  // MSG("stat shared mem: %d - size: %d",r,st.st_size);

  if (st.st_size < new_len)
    ERR_MSG("Size of resized shared memory unexpectedly small %d < %d.",
	    (int) st.st_size,new_len);

  // ok, so do the remapping of the memory

  munmap(shmc->_ptr,shmc->_len);
  /*
  printf ("l: %016p p: %016p ctrl: %016p b: %016p cur: %016p e: %016p / %08x\n",
	  _len,_ptr,_ctrl,_begin,_cur,_end,_size);
  */
  *expand = new_len - shmc->_len;

  shmc->_len = new_len;

  shmc->_ptr = (char *) mmap(0, shmc->_len, (PROT_READ | PROT_WRITE),
			     MAP_SHARED, comm->_shm_fd, 0);

  if (shmc->_ptr == MAP_FAILED)
    ERR_MSG("Error mapping shared memory segment.");

  size_t cur_offset = shmc->_cur - shmc->_begin;
  size_t header_off = ((char*) header) - shmc->_begin;

  shmc->_ctrl = (external_writer_shm_control*) shmc->_ptr;

  shmc->_begin = (char*) (shmc->_ctrl + 1);
  shmc->_cur  = shmc->_begin + cur_offset;
  shmc->_end  = shmc->_ptr + shmc->_len;
  shmc->_size = shmc->_end - shmc->_begin;

  header = (external_writer_buf_header *) (shmc->_begin + header_off);
  /*
  printf ("l: %016p p: %016p ctrl: %016p b: %016p cur: %016p e: %016p / %08x\n",
	  _len,_ptr,_ctrl,_begin,_cur,_end,_size);
  */
  // so, we'll now use the newer bigger size

  // MSG("Resized SHM -> %d.",new_len);

  return header;
}

external_writer_buf_header *alloc_pipe(ext_write_config_comm *comm,
				       size_t size,
				       external_writer_buf_header *header)
{
  shared_mem_circular *shmc = comm->_shmc;

  assert (size >= shmc->_size);

  size_t off_cur = shmc->_cur - shmc->_mem;
  size_t avail   = shmc->_read_end - shmc->_cur;
  size_t header_off = ((char*) header) - shmc->_mem;

  shmc->_mem = (char*) realloc(shmc->_mem,size);
  if (!shmc->_mem)
    ERR_MSG("Failure allocating memory for pipe, %zd bytes.", size);
  shmc->_size = size;
  shmc->_end = shmc->_mem + shmc->_size;

  // reposition the pointers

  shmc->_cur = shmc->_mem + off_cur;
  shmc->_read_end = shmc->_cur + avail;
  header = (external_writer_buf_header *) (shmc->_mem + header_off);

  return header;
}

external_writer_buf_header *
request_resize_pipe(ext_write_config_comm *comm,
		    void *msg,uint32_t *left,
		    external_writer_buf_header *header)
{
  shared_mem_circular *shmc = comm->_shmc;

  uint32_t new_len = get_buf_uint32(&msg,left);
  uint32_t magic = get_buf_uint32(&msg,left);

  if (magic != EXTERNAL_WRITER_MAGIC)
    ERR_MSG("Request to resize pipe has bad magic.");

  if (new_len < shmc->_len)
    ERR_MSG("Request to decrease shared memory not allowed.");

  // We're simply getting a completely new pipe.  All old pointers useless.

  header = alloc_pipe(comm,new_len,header);

  // Send a message to the other end that we did the resize (may now
  // continue to write, into new buffer :-) ) This game is needed to
  // make the alignment still work, without having to shift pointers
  // at both places.

  // No need to send anything!
  // write_response(EXTERNAL_WRITER_RESPONSE_PIPE_RESIZED);

  return header;
}

void send_abort()
{
#if STRUCT_WRITER
  external_writer_buf_header header;

  header._request = htonl(EXTERNAL_WRITER_BUF_ABORT |
			  EXTERNAL_WRITER_REQUEST_HI_MAGIC);
  header._length  = htonl(sizeof(header));

  send_item_chunk *chunk;

  char *net_io_chunk =
    ext_net_io_reserve_chunk(sizeof(header),false,&chunk);

  memcpy(net_io_chunk,&header,sizeof(header));

  ext_net_io_commit_chunk(sizeof(header),chunk);

  if (_config._stdout)
    full_write(STDOUT_FILENO,&header,sizeof(header));
#endif
}

bool handle_request(ext_write_config_comm *comm,
		    external_writer_buf_header *header)
{
  shared_mem_circular *shmc = comm->_shmc;

  // The header may be unusable after we've remapped the memory
  uint32_t request = ntohl(header->_request);
  uint32_t length  = ntohl(header->_length);

  if ((request & EXTERNAL_WRITER_REQUEST_HI_MASK) !=
      EXTERNAL_WRITER_REQUEST_HI_MAGIC)
    ERR_MSG("Bad request hi magic.");

  request &= EXTERNAL_WRITER_REQUEST_LO_MASK;
  
  uint32_t left = length - sizeof(*header);

  bool quit = false;
  size_t expand = 0;

  if (_config._dump_raw)
    {
      fprintf (stderr,"\nRR %d: %d - %zd = %d = 0x%x\n",
	       request, ntohl(header->_length),sizeof(*header),left,left);
      {
	uint32_t n = ntohl(header->_length) / sizeof (uint32_t);
	uint32_t *p = (uint32_t *) header;
	for (int i = 0; i < n; i++)
	  {
	    if (i % 8 == 0)
	      fprintf (stderr,"%4x: ", i);
	    fprintf (stderr,"%08x ", ntohl(*(p++)));
	    if (i % 8 == 7)
	      fprintf (stderr,"\n");
	  }
	fprintf (stderr,"\n");
	fflush(stderr);
      }
    }

  switch (request)
    {
    case EXTERNAL_WRITER_BUF_OPEN_FILE:
      request_file_open(header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_HIST_H1I:
      request_hist_h1i(header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_NAMED_STRING:
      request_named_string(header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_BOOK_NTUPLE:
      request_book_ntuple(header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_ALLOC_ARRAY:
      request_alloc_array(header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_CREATE_BRANCH:
      request_create_branch(header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_RESIZE:
      {
	assert(!shmc->_mem || !shmc->_ctrl); // not SHM and pipe

	if (!shmc->_ctrl && !shmc->_mem)
	  ERR_MSG("Cannot resize without SHM or pipe.");
	if (shmc->_ctrl)
	  {
	    // The SHM space has (will) be expanded
	    header = request_resize_shm(comm,header+1,&left,header,&expand);
	    // check that next pointer has the magix just before it
	    uint32_t *p_next = (uint32_t*) (shmc->_cur + length);
	    if (ntohl(p_next[-1]) != EXTERNAL_WRITER_MAGIC)
	      ERR_MSG("Bad alignment after SHM resize.");
	  }
	if (shmc->_mem)
	  header = request_resize_pipe(comm,header+1,&left,header);
	break;
      }
    case EXTERNAL_WRITER_BUF_ARRAY_OFFSETS:
      request_array_offsets(header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_SETUP_DONE:
    case EXTERNAL_WRITER_BUF_SETUP_DONE_RD:
    case EXTERNAL_WRITER_BUF_SETUP_DONE_WR:
      // May change the message inline
      request_setup_done(header+1,&left,
			 request == EXTERNAL_WRITER_BUF_SETUP_DONE_RD,
			 request == EXTERNAL_WRITER_BUF_SETUP_DONE_WR);
      break;
    case EXTERNAL_WRITER_BUF_NTUPLE_FILL:
      // May change the message inline (including header)
      request_ntuple_fill(comm,
			  header+1,&left,
			  header,length,
			  false);
      break;
    case EXTERNAL_WRITER_BUF_KEEP_ALIVE:
      request_keep_alive(comm,
			 header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_DONE:
      /* Dump all merge data we have before we copy the 'done/abort'
       * entry.
       */
      merge_all_remaining();
      /* Note: there is also an exit path internally on abort.  Since
       * that is an uncontrolled exit, we do not emit incomplete merges
       * for that one.  Consequently, we also do not merge remaining
       * for ABORT messages.
       */
      /*FALLTHROUGH*/
    case EXTERNAL_WRITER_BUF_ABORT:
      quit = true;
      break; // not very reachable
    default:
      ERR_MSG("Unknown request %d.",request);
    }

  /*
  printf ("\nRD %d: %d\n",
	  request, left);
  */

  if (left != 0)
    ERR_MSG("Message not completely used (req=%d,left=%d).",
	    request,left);

#if STRUCT_WRITER
  if (request != EXTERNAL_WRITER_BUF_NTUPLE_FILL)
    {
      // We no longer change the header (ntuple fill handles writing
      // by itself).  So just check that the length was not changed.
      assert (length == ntohl(header->_length));

      send_item_chunk *chunk = NULL;

      char *net_io_chunk =
	ext_net_io_reserve_chunk(length,
				 request < EXTERNAL_WRITER_BUF_NTUPLE_FILL,
				 &chunk);

      memcpy(net_io_chunk,header,length);

      if (_config._stdout)
	full_write(STDOUT_FILENO,header,length);

      ext_net_io_commit_chunk(length,chunk);
    }
#endif

  if (expand)
    {
      // Used for SHM resize

      // Consume the added space.  Trick needed such that producer does
      // not start to over-write our old data before resize is complete
      // (i.e. here)
      MFENCE; // don't tell we're done until here
      shmc->_ctrl->_done += expand;
    }

  if (!_config._forked && _got_sigint)
    {
      // We got Ctrl+C, and are not in forked mode (where we're
      // ignoring sigints).  Quit gracefully, asking clients to
      // disconnect.

      send_abort();
      return true;
    }

  return quit;
}

bool check_request(ext_write_config_comm *comm,
		   external_writer_buf_header *header)
{
  shared_mem_circular *shmc = comm->_shmc;

  /* Verify request against the request list of the first source. */

  // The header may be unusable after we've remapped the memory
  uint32_t request = ntohl(header->_request);
  uint32_t length  = ntohl(header->_length);

  if ((request & EXTERNAL_WRITER_REQUEST_HI_MASK) !=
      EXTERNAL_WRITER_REQUEST_HI_MAGIC)
    ERR_MSG("Bad request hi magic.");

  request &= EXTERNAL_WRITER_REQUEST_LO_MASK;
  
  uint32_t left = length - sizeof(*header);

  bool quit = false;
  size_t expand = 0;

  switch (request)
    {
   case EXTERNAL_WRITER_BUF_RESIZE:
      {
	assert(!shmc->_mem || !shmc->_ctrl); // not SHM and pipe

	if (!shmc->_ctrl && !shmc->_mem)
	  ERR_MSG("Cannot resize without SHM or pipe.");
	if (shmc->_ctrl)
	  {
	    // The SHM space has (will) be expanded
	    header = request_resize_shm(comm,header+1,&left,header,&expand);
	    // check that next pointer has the magix just before it
	    uint32_t *p_next = (uint32_t*) (shmc->_cur + length);
	    if (ntohl(p_next[-1]) != EXTERNAL_WRITER_MAGIC)
	      ERR_MSG("Bad alignment after SHM resize.");
	  }
	if (shmc->_mem)
	  header = request_resize_pipe(comm,header+1,&left,header);
	break;
      }

    case EXTERNAL_WRITER_BUF_DONE:
    case EXTERNAL_WRITER_BUF_ABORT:
      quit = true;
      break; // not very reachable
    }

  if (expand)
    {
      // Used for SHM resize

      // Consume the added space.  Trick needed such that producer does
      // not start to over-write our old data before resize is complete
      // (i.e. here)
      MFENCE; // don't tell we're done until here
      shmc->_ctrl->_done += expand;
    }

  /* TODO: handle sigint?  elsewhere? */

  return quit;
}

bool pre_handle_request(ext_write_config_comm *comm,
			external_writer_buf_header *header)
{
  // The header may be unusable after we've remapped the memory
  uint32_t request = ntohl(header->_request);
  uint32_t length  = ntohl(header->_length);

  if ((request & EXTERNAL_WRITER_REQUEST_HI_MASK) !=
      EXTERNAL_WRITER_REQUEST_HI_MAGIC)
    ERR_MSG("Bad request hi magic.");

  request &= EXTERNAL_WRITER_REQUEST_LO_MASK;
  
  uint32_t left = length - sizeof(*header);

  bool quit = false;

  switch (request)
    {
    case EXTERNAL_WRITER_BUF_NTUPLE_FILL:
      prehandle_ntuple_fill(comm,
			    header+1,&left);
      break;
    case EXTERNAL_WRITER_BUF_KEEP_ALIVE:
      prehandle_keep_alive(comm,
			   header+1,&left);
      break;
    }

  return quit;
}


int connect_server(const char *server)
{
  int rc;
  struct sockaddr_in serv_addr;
  struct hostent *h;
  char *hostname, *colon;
  unsigned short port = (unsigned short) EXTERNAL_WRITER_DEFAULT_PORT;
  int fd_server;

  /* If there is a colon in the hostname, interpret what follows
   * as a port number, overriding the default port.
   */
  hostname = strdup(server);
  colon = strchr(hostname,':');

  if (colon)
    {
      port = (unsigned short) atoi(colon+1);
      *colon = 0; // cut the hostname
    }

  /* Get server IP address. */
  h = gethostbyname(hostname);
  free(hostname);

  if(h == NULL)
    ERR_MSG("Unknown host '%s'.",server);

  MSG("Server '%s' known... (IP : %s).", h->h_name,
      inet_ntoa(*(struct in_addr *)h->h_addr_list[0]));

  /* Create the socket. */
  fd_server = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (fd_server == -1)
    {
      perror("socket");
      ERR_MSG("Cannot open socket");
    }

  /* Connect to the port. */
  serv_addr.sin_family = (sa_family_t) h->h_addrtype;
  memcpy((char *) &serv_addr.sin_addr.s_addr,
	 h->h_addr_list[0], h->h_length);
  serv_addr.sin_port = htons(port);

  rc = connect(fd_server,
	       (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (rc == -1)
    {
      perror("connect()");
      ERR_MSG("Cannot connect to port.");
    }

  /* We've connected to the port server, get the address.  We
   * employ a time-out for the portmap message, to not get stuck
   * here.
   */

  external_writer_portmap_msg portmap_msg;
  size_t got = 0;

  while (got < sizeof(portmap_msg))
    {
      fd_set readfds;
      int nfd;

      FD_ZERO(&readfds);
      FD_SET(fd_server,&readfds);
      nfd = fd_server;

      struct timeval timeout;

      timeout.tv_sec = 2;
      timeout.tv_usec = 0;

      int ret = select(nfd+1,&readfds,NULL,NULL,&timeout);

      if (ret == -1)
	{
	  if (errno == EINTR)
	    continue;
	  perror("select");
	  ERR_MSG("Unexpected I/O multiplexer error "
		  "receiving data port number.");
	}

      if (ret == 0) // can only happen on timeout
	ERR_MSG("Timeout receiving data port number.");

      size_t left = sizeof(portmap_msg) - got;

      ssize_t n = read(fd_server,((char*) &portmap_msg)+got,left);

      if (n == -1)
	{
	  if (errno == EINTR)
	    continue;
	  perror("read");
	  ERR_MSG("Error receiving data port number.");
	}
      if (n == 0)
	ERR_MSG("Failure receiving data port number.");

      got += (size_t) n;
    }

  for ( ; ; )
    {
      if (close(fd_server) == 0)
	break;
      if (errno == EINTR)
	continue;
      perror("close");
      ERR_MSG("Failure receiving data port number.");
    }

  if (portmap_msg._magic != htonl(~EXTERNAL_WRITER_MAGIC))
    ERR_MSG("Bad magic when receiving data port number.");

  MSG("Server '%s:%d' has data port %d.",
      h->h_name,port,ntohs(portmap_msg._port));

  // Now, connect to the data port!

  /* Create the socket. */
  fd_server = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (fd_server == -1)
    {
      perror("socket");
      ERR_MSG("Cannot open socket");
    }

  /* Connect to the port. */
  serv_addr.sin_family = (sa_family_t) h->h_addrtype;
  memcpy((char *) &serv_addr.sin_addr.s_addr,
	 h->h_addr_list[0], h->h_length);
  serv_addr.sin_port = portmap_msg._port;

  rc = connect(fd_server,
	       (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (rc == -1)
    {
      perror("connect()");
      ERR_MSG("Cannot connect to port.");
    }

  /* We have successfully connected. */

  return fd_server;
}

uint32_t pipe_has_full_message(ext_write_config_comm *comm)
{
  shared_mem_circular *shmc = comm->_shmc;

  // ok, so we got some data.  It is enough to be a header?

  if (shmc->_read_end - shmc->_cur < sizeof(external_writer_buf_header))
    return 0;

  // we can inspect the header

  external_writer_buf_header *header =
    (external_writer_buf_header *) shmc->_cur;

  uint32_t length = ntohl(header->_length);

  if (length & 3)
    {
      /*
      for (int i = 0; i < 32; i++)
	MSG("%08x: %08x %08x %08x %08x",
	    i*16,
	    *((uint32_t *) (shmc->_mem + i*16 +  0)),
	    *((uint32_t *) (shmc->_mem + i*16 +  4)),
	    *((uint32_t *) (shmc->_mem + i*16 +  8)),
	    *((uint32_t *) (shmc->_mem + i*16 + 12)));
      */
      ERR_MSG("Pipe message length unaligned (req=0x%08x=%d,len=%d).",
	      ntohl(header->_request),
	      ntohl(header->_request) & EXTERNAL_WRITER_REQUEST_LO_MASK,
	      length);
    }
  /*
  MSG ("%08x  h: %08x %08x (%d) (%lld)\n",
       shmc->_cur - shmc->_mem,
       ntohl(header->_request),
       ntohl(header->_length),
       shmc->_read_end - shmc->_cur,
       (long long int) _num_events);
  */
  /*
  for (int i = 0; i < 32; i++)
    MSG("%08x: %08x %08x %08x %08x",
	i*16,
	*((uint32_t *) (shmc->_mem + i*16 +  0)),
	*((uint32_t *) (shmc->_mem + i*16 +  4)),
	*((uint32_t *) (shmc->_mem + i*16 +  8)),
	*((uint32_t *) (shmc->_mem + i*16 + 12)));
  */
  if (shmc->_read_end - shmc->_cur >= length)
    {
      // The entire message fits in the currently available space
      return length;
    }
  return 0;
}

uint32_t pipe_get_message(ext_write_config_comm *comm)
{
  shared_mem_circular *shmc = comm->_shmc;

  for ( ; ; )
    {
      //MSG("have left: %d",shmc->_read_end - shmc->_cur);

      // Did we consume some entire pages?  If so, madvise them away,
      // such that the writer does not need to do copy-on-write when
      // it starts to write more info in pages that possibly have been
      // just mapped into our space
      /*
	size_t page_off = (_cur - _mem) & (0x1000 - 1);
	char *madvise_end = _cur - page_off;

	if (madvise_end != _madvise_start)
	  {
	    madvise(_madvise_start,madvise_end - _madvise_start,MADV_DONTNEED);
	    madvise_end = _madvise_start;
	  }
      */
      // When we end up here, the entire message was not available (yet)
      // either because we just have not received it, or because we
      // reached our own end of buffer

      if (shmc->_read_end == shmc->_end)
	{
	  // We must move the current message to our first page of
	  // memory (or if we only have one page, to the page start)

	  size_t left = shmc->_read_end - shmc->_cur;

	  if (shmc->_size >= 2*0x1000)
	    {
	      size_t page_off = (shmc->_cur - shmc->_mem) & (0x1000 - 1);
	      char *start = shmc->_mem + page_off;
	      if (shmc->_cur == start)
		ERR_MSG("Message too long to be handled by pipe buffer.");
	      //MSG("move to %d",page_off);
	      memmove(start,shmc->_cur,left);
	      shmc->_cur = start;
	    }
	  else
	    {
	      if (shmc->_cur == shmc->_mem)
		ERR_MSG("Message too long for pipe buffer.");
	      //MSG("move to start");
	      memmove(shmc->_mem,shmc->_cur,left);
	      shmc->_cur = shmc->_mem;
	    }
	  shmc->_read_end = shmc->_cur + left;
	  //_madvise_start = _mem;
	  //MSG("have left: %d",_read_end - _cur);
	}

      //MSG("about to read: have %d space %d",_read_end - _cur,_end - _read_end);

#if STRUCT_WRITER
      // Let the server run until there is data in the pipe

      while (!ext_net_io_select_clients(comm->_pipe_in,-1,
					false,false))
	;
#endif

      // We always try to read as much as possible from the pipe

      ssize_t n;

      n = read(comm->_pipe_in,
	       shmc->_read_end,shmc->_end - shmc->_read_end);

      if (n == 0)
	{
	  // This is the normal error if someone hits ctrl+C on the
	  // controlling program. Tell our clients things went sour.
	  send_abort();
	  close_output(); // close outputs gracefully
	  ERR_MSG("Unexpected end of input.");
	}

      if (n == -1)
	{
	  if (errno == EINTR)
	    continue; // try again
	  perror("read");
	  ERR_MSG("Unexpected error reading input.");
	}

      shmc->_read_end += n;

      // MSG("got %d bytes, have now: %d",n,shmc->_read_end - shmc->_cur);

      uint32_t length = pipe_has_full_message(comm);

      if (length)
	return length;
    }
}

void prepare_pipe_comm(ext_write_config_comm *comm)
{
  if (comm->_shm_fd != -1)
    {
      ERR_MSG("Cannot handle SHM with several input sources.");
    }
  if (comm->_pipe_in != -1)
    {
    }
  else if (strcmp(comm->_input,"-") == 0)
    {
      /* Read data from stdin. */
      comm->_pipe_in = STDIN_FILENO;
    }
  else
    {
      comm->_server_fd = connect_server(comm->_input);
      comm->_pipe_in = comm->_server_fd;
    }

  shared_mem_circular *shmc = comm->_shmc;

  alloc_pipe(comm,EXTERNAL_WRITER_MIN_SHARED_SIZE,NULL);




}

int comm_next_item_compare_less(ext_write_config_comm *comm1,
				ext_write_config_comm *comm2)
{
  for (uint32_t i = 0; i < _g._sort_u32_words; i++)
    {
      uint32_t v1 = ntohl(comm1->_raw_sort_u32[i]);
      uint32_t v2 = ntohl(comm2->_raw_sort_u32[i]);

      if (v1 < v2)
	return 1;
      if (v1 > v2)
	return 0;
    }

  /* They actually have the same event number.  Is one of them per
   * chance a progress-mark event (i.e. keep-alive).  Then it sorts
   * as being later. */

  if (comm1->_keep_alive_event < comm2->_keep_alive_event)
    return 1;

  return 0;
}

void pipe_communication(ext_write_config_comm *comm_head)
{
  size_t nsrc = 0;

  for (ext_write_config_comm *comm = comm_head;
       comm != NULL; comm = comm->_next)
    {
      nsrc++;
    }

  ext_write_config_comm **heap =
    (ext_write_config_comm **) malloc (nsrc * sizeof (ext_write_config_comm *));

  int isrc = 0;

  // Hmmm, a much simpler way to check that the setup messages from
  // all sources are the same is to read all sources in lock-step.
  // First actually is handled, all others just verified.

  for (ext_write_config_comm *comm = comm_head;
       comm != NULL; comm = comm->_next)
    {
      prepare_pipe_comm(comm);

      shared_mem_circular *shmc = comm->_shmc;

      for ( ; ; )
	{
	  // This only returns if at least one message is present!
	  uint32_t length = pipe_get_message(comm);

	  for ( ; ; )
	    {
	      // MSG("%p %p %p\n",shmc->_mem,shmc->_cur,shmc->_read_end);

	      // The entire message fits in the currently available space

	      external_writer_buf_header *header =
		(external_writer_buf_header *) shmc->_cur;

	      uint32_t request = ntohl(header->_request);

	      request &= EXTERNAL_WRITER_REQUEST_LO_MASK;

	      if (request == EXTERNAL_WRITER_BUF_NTUPLE_FILL)
		{
		  // Pre-parse the item, such that we have the sorting
		  // information.

		  pre_handle_request(comm,header);

		  HEAP_INSERT(ext_write_config_comm *,
			      heap, isrc, comm_next_item_compare_less,
			      comm);
		  goto done_setup_this;
		}

	      // act as if we consumed it before investigating it, in case
	      // it is a resize request

	      shmc->_cur += length;

	      if (comm == comm_head)
		{
		  if (handle_request(comm,header))
		    goto done_message;
		}
	      else
		{
		  if (check_request(comm,header))
		    goto done_message;
		}

	      if (request == EXTERNAL_WRITER_BUF_SETUP_DONE_RD)
		goto reader_loop;

#if STRUCT_WRITER
	      if (_got_sigio)
		{
		  // Some client (may) want to send some data.  Let it
		  // have a go.
		  _got_sigio = 0;
		  MFENCE;
		  ext_net_io_select_clients(-1,-1,false,true);
		}
#endif
	      // Try to handle more messages, as long as we have full ones.

	      if (!(length = pipe_has_full_message(comm)))
		break;
	    }
	}
    done_setup_this:
      ;
    }

  assert(isrc == nsrc);

  if (nsrc > 1 && (_g._sort_u32_words == 0 ||
		   _g._sort_u32_words == (uint32_t) -1))
    ERR_MSG("Multiple input sources, "
	    "but 0 words for sorting in input streams.");

  /* Multiple sources handling.  The overhead for single-source is
   * small, as we never move anything around in the heap.
   */

  for ( ; ; )
    {
      /* The source to pick an event from is in the first slot. */

      ext_write_config_comm *comm = heap[0];

      shared_mem_circular *shmc = comm->_shmc;

      external_writer_buf_header *header =
	(external_writer_buf_header *) shmc->_cur;

      uint32_t request = ntohl(header->_request);
      uint32_t length  = ntohl(header->_length);

      // act as if we consumed it before investigating it, in case
      // it is a resize request

      shmc->_cur += length;

      if (handle_request(comm,header))
	goto done_message;

      if (!(length = pipe_has_full_message(comm)))
	{
	  // We need to read more from this source.

	  length = pipe_get_message(comm);
	}

      /* Pre-parse the item, such that we have the sorting information. */

      header =
	(external_writer_buf_header *) shmc->_cur;

      pre_handle_request(comm,header);


      HEAP_MOVE_DOWN(ext_write_config_comm *,
		     heap, nsrc, comm_next_item_compare_less,
		     0);
    }




 done_message:
  // First close the server, since otherwise that may have to wait for
  // us to shut our outputs down...
  for (ext_write_config_comm *comm = comm_head;
       comm != NULL; comm = comm->_next)
    {
      if (comm->_server_fd != -1)
	{
	  // Dirty, not caring about errors.
	  close(comm->_server_fd);
	}
    }
  // Then close the outputs (whichever)
  close_output();
  exit(0);

 reader_loop:
  ext_write_config_comm *comm = comm_head;
  shared_mem_circular *shmc = comm->_shmc;

  // We are to read events instead, so our loop drives the event flow.
  if (comm->_pipe_out == -1)
    ERR_MSG("Cannot write events - no output pipe.");

  // First check that all data from the pipe was consumed (there might
  // have come more, which we will not verify)
  if (shmc->_read_end != shmc->_cur)
    ERR_MSG("Pipe buffer not empty after setup done->read message.");

#define WRITE_START shmc->_read_end

  shmc->_cur = shmc->_mem; // we start writing to the beginning of the buffer.
  WRITE_START = shmc->_mem; // tells where the writing should start

  size_t max_msg_size =
    sizeof(external_writer_buf_header) + 3 * sizeof(uint32_t) +
    _g._max_offset_array_items * sizeof(uint32_t);

  for ( ; ; )
    {
      // While there is space enough in the buffer, fetch more events.

      while (shmc->_end - shmc->_cur >= max_msg_size)
	{
	  char *end_msg = shmc->_cur + max_msg_size;

	  bool more_msg = ntuple_get_event(shmc->_cur,&end_msg);

	  //MSG("max_size: %d  avail: %d  this: %d",
	  //     max_msg_size,_end - _cur,end_msg - _cur);

	  assert (end_msg <= shmc->_end);

	  shmc->_cur = end_msg;

	  if (!more_msg)
	    goto read_done;
	}

      // So, time to write!

      full_write(comm->_pipe_out,WRITE_START,shmc->_cur - WRITE_START);

      // We got rid of the data, reset the pointers.

      if (shmc->_size >= 2*0x1000)
	{
	  size_t page_off = (shmc->_cur - shmc->_mem) & (0x1000 - 1);
	  WRITE_START = shmc->_mem + page_off;


	  shmc->_cur = WRITE_START;
	}
      else
	{
	  shmc->_cur = shmc->_mem;
	  WRITE_START = shmc->_mem;
	}

      if (shmc->_end - shmc->_cur < max_msg_size)
	ERR_MSG("Worst case message to long to be handled by pipe buffer.");
    }

 read_done:
  // We should send the done message, so that the consumer knows all
  // went as expected.

  full_write(comm->_pipe_out,WRITE_START,shmc->_cur - WRITE_START);

  close_output();
  exit(0);
}

void usage(char *cmdname)
{
  printf ("\n");
#if USING_CERNLIB
  printf ("Write HBOOK ntuple.\n\n");
#endif
#if USING_ROOT
  printf ("Write/read ROOT ntuple.\n\n");
#endif
#if STRUCT_WRITER
  printf ("Propagate structure data.\n\n");
#endif
#if USING_CERNLIB || USING_ROOT
  printf ("%s SERVER[:PORT]|- options\n\n",
	  cmdname);
#endif
#if STRUCT_WRITER
  printf ("%s SERVER[:PORT]|- options\n\n",cmdname);
#endif

#if USING_CERNLIB || USING_ROOT
  printf ("  --outfile=FILE     Write output to FILE.\n");
  printf ("  --infile=FILE      Read from input FILE.\n");
  printf ("  --ftitle=FTITLE    FTITLE of output file.\n");
  printf ("  --id=[OID]=ID      Override ID of ntuple written (write single struct OID).\n");
  printf ("  --title=TITLE      Override TITLE of ntuple written.\n");
  printf ("  --timeslice=N[:M]  Start a new file every N seconds.  Subdir every M seconds.\n");
  printf ("  --autosave=N       Autosave tree every N seconds.\n");
#endif
#ifdef STRUCT_WRITER
  printf ("  --insrc=SRV:PRT|-  Input src when reading data. (internal use only)\n");
  printf ("  --header=FILE      Write data structure declaration to FILE.\n");
  printf ("  --id=ID            Override ID of header written.\n");
  printf ("  --dump-raw         Dump raw protocol data.\n");
  printf ("  --debug-header     Litter header declaration with offsets and sizes.\n");
  printf ("  --server[=PORT]    Run a external data server (at PORT).\n");
  printf ("  --stdout           Write data to stdout.\n");
  printf ("  --dump[=FORMAT]    Make text dump of data.  (FORMAT: normal, wide, [compact_]json)\n");
  printf ("  --bitpack          Bitpack STRUCT data even if not using network server.\n");
#endif
  printf ("  --time-stitch=N    Combine events with timestamps with difference <= N.\n");
  printf ("  --colour=yes|no    Force colour and markup on or off.\n");
  printf ("  --forked=fd1,fd2   File descriptors for forked comm. (internal use only)\n");
  printf ("  --shm-forked=fd,fd1,fd2  Use shared memory communication. (internal use only)\n");
  printf ("  --help             This text.\n");
  printf ("\n");
}

ext_write_config_comm *alloc_comm_struct(ext_write_config_comm ***next_comm_ptr)
{
  ext_write_config_comm *comm =
    (ext_write_config_comm *) malloc (sizeof (ext_write_config_comm));
  shared_mem_circular *shmc =
    (shared_mem_circular *) malloc (sizeof (shared_mem_circular));

  if (!comm || !shmc)
    ERR_MSG("Failure allocating input structure.");

  memset(comm, 0, sizeof (ext_write_config_comm));
  memset(shmc, 0, sizeof (shared_mem_circular));

  comm->_shm_fd = -1;
  comm->_pipe_in = comm->_pipe_out = -1;
  comm->_server_fd = -1;
  comm->_shmc = shmc;

  **next_comm_ptr = comm;
  *next_comm_ptr = &comm->_next;

  return comm;
}

int main(int argc,char *argv[])
{
  colourtext_init();

  _argv0 = argv[0];

  // The entire program operates in a loop reading one-byte commands
  // from the controlling parent process

  // parse any arguments

  memset(&_config,0,sizeof(_config));

  ext_write_config_comm **next_comm_ptr = &_config._comms;

  if (argc == 1)
    {
      usage(argv[0]);
      exit(0);
    }

  for (int i = 1; i < argc; i++)
    {
      char *post;

      //MSG("cmdline: %s",argv[i]);

#define MATCH_PREFIX(prefix,post) (strncmp(argv[i],prefix,strlen(prefix)) == 0 && *(post = argv[i] + strlen(prefix)) != '\0')
#define MATCH_ARG(name) (strcmp(argv[i],name) == 0)

      if (MATCH_ARG("--help")) {
	usage(argv[0]);
	exit(0);
      }
      else if (MATCH_PREFIX("--shm-forked=",post)) {
	_config._forked = true;
	ext_write_config_comm *comm = alloc_comm_struct(&next_comm_ptr);
	if (sscanf(post,"%d,%d,%d",
		   &comm->_shm_fd,&comm->_pipe_in,&comm->_pipe_out) != 3)
	  ERR_MSG("Bad fd description '%s' for --shm-forked.", post);
      }
      else if (MATCH_PREFIX("--forked=",post)) {
	_config._forked = true;
	ext_write_config_comm *comm = alloc_comm_struct(&next_comm_ptr);
	if (sscanf(post,"%d,%d",&comm->_pipe_in,&comm->_pipe_out) != 2)
	  ERR_MSG("Bad fd description '%s' for --forked.", post);
      }
      else if (MATCH_PREFIX("--colour=",post)) {
	int force = 0;

	if (strcmp(post,"yes") == 0)
#ifdef USE_CURSES	  
	  force = 1;
#else
	ERR_MSG("No colour support since ncurses not compiled in.");
#endif
	else if (strcmp(post,"no") == 0)
	  force = -1;
	else if (strcmp(post,"auto") != 0)
	  ERR_MSG("Bad option '%s' for --colour=",post);

	colourtext_setforce(force);
      }
      else if (MATCH_PREFIX("--time-stitch=",post)) {
	_config._ts_merge_window = atoi(post);
      }
#if USING_CERNLIB || USING_ROOT
      else if (MATCH_PREFIX("--outfile=",post)) {
	_config._outfile = post;
      }
      else if (MATCH_PREFIX("--infile=",post)) {
	_config._infile = post;
      }
      else if (MATCH_PREFIX("--ftitle=",post)) {
	_config._ftitle = post;
      }
      else if (MATCH_PREFIX("--id=",post)) {
	_config._id = post;
      }
      else if (MATCH_PREFIX("--title=",post)) {
	_config._title = post;
      }
      else if (MATCH_PREFIX("--timeslice=",post)) {
	_config._timeslice = atoi(post);
	char *colon = strchr(post,':');
	if (colon)
	  _config._timeslice_subdir = atoi(colon+1);
      }
#endif
#if USING_ROOT
      else if (MATCH_PREFIX("--autosave=",post)) {
	_config._autosave = atoi(post);
      }
#endif
#ifdef STRUCT_WRITER
      else if (MATCH_PREFIX("--insrc=",post)) {
	_config._insrc = post;
      }
      else if (MATCH_PREFIX("--header=",post)) {
	_config._header = post;
      }
      else if (MATCH_PREFIX("--id=",post)) {
	char *equals = strchr(post,'=');
	if (equals)
	  {
	    _config._header_id_orig = strndup(post,equals-post);
	    _config._header_id = equals + 1;
	  }
	else
	  _config._header_id = post;
      }
      else if (MATCH_ARG("--dump-raw")) {
	_config._dump_raw = 1;
      }
      else if (MATCH_ARG("--debug-header")) {
	_config._debug_header = 1;
      }
      else if (MATCH_PREFIX("--server=",post)) {
	_config._port = atoi(post);
      }
      else if (MATCH_ARG("--server")) {
	_config._port = -1;
      }
      else if (MATCH_ARG("--stdout")) {
	_config._stdout = 1;
      }
      else if (MATCH_ARG("--bitpack")) {
	_config._bitpack = 1;
      }
      else if (MATCH_ARG("--dump")) {
	_config._dump = EXT_WRITER_DUMP_FORMAT_NORMAL;
      }
      else if (MATCH_PREFIX("--dump=",post)) {
	if (strcmp(post,"normal") == 0)
	  _config._dump = EXT_WRITER_DUMP_FORMAT_NORMAL;
	else if (strcmp(post,"wide") == 0)
	  _config._dump = EXT_WRITER_DUMP_FORMAT_NORMAL_WIDE;
	else if (strcmp(post,"json") == 0)
	  _config._dump = EXT_WRITER_DUMP_FORMAT_HUMAN_JSON;
	else if (strcmp(post,"compact_json") == 0)
	  _config._dump = EXT_WRITER_DUMP_FORMAT_COMPACT_JSON;
	else
	  ERR_MSG("Bad option '%s' for --dump=",post);
      }
#endif
      else {
	// Treat the option as a specifier for input data source
	ext_write_config_comm *comm = alloc_comm_struct(&next_comm_ptr);
	comm->_input = argv[i];
      }
    }

  if (_config._comms == NULL)
    ERR_MSG("No input source specified.");

#if USING_CERNLIB || USING_ROOT
  if (!_config._outfile && !_config._infile)
    ERR_MSG("Filename missing.");
  if (_config._outfile && _config._infile)
    ERR_MSG("Cannot both read and write file.");
  if (_config._infile && _config._timeslice)
    ERR_MSG("Cannot timeslice files to be read.");

  if (_config._infile)
    {
      char *writer = strstr(argv[0], "_writer");
      // We replace writer with reader in the executable name
      // if possible.  Should call pthread_setname_np too?
      if (writer)
	strncpy(writer, "_reader",7);

    }

  if (_config._timeslice)
    {
      const char *slash = strrchr(_config._outfile,'/');
      const char *afterslash;

      if (slash)
	{
	  afterslash = slash + 1;
	  _ts._timeslice_name._dir = strndup(_config._outfile,
					     afterslash - _config._outfile);
	}
      else
	{
	  afterslash = _config._outfile;
	  _ts._timeslice_name._dir = "";
	}

      const char *dot = strrchr(afterslash,'.');

      if (dot)
	{
	  _ts._timeslice_name._base = strndup(afterslash,dot - afterslash);
	  _ts._timeslice_name._ext  = strdup(dot);
	}
      else
	{
	  _ts._timeslice_name._base = strdup(afterslash);
	  _ts._timeslice_name._ext = "";
	}

      char *listname =
	(char *) malloc(strlen(_ts._timeslice_name._dir)+
			strlen(_ts._timeslice_name._base)+
			strlen(".txt")+1);

      if (!listname)
	ERR_MSG("Failure allocating string.");

      strcpy(listname,_ts._timeslice_name._dir);
      strcat(listname,_ts._timeslice_name._base);
      strcat(listname,".txt");

      _ts._timeslice_name._list_fd = open(listname,
					  O_RDWR | O_APPEND | O_CREAT,
					  S_IRWXU | S_IRWXG | S_IRWXO);

      if (_ts._timeslice_name._list_fd == -1)
	{
	  perror("open");
	  ERR_MSG("Failed to open list of timesliced files for appending.");
	}

      // NOTE: the _timeslice_name._list_fd is never properly closed,
      // (there are so many places where this program terminates).
      // however, when we have opened it, we anyhow want it to the
      // end, and for the time being, we just let the OS clean that up

      // close(_timeslice_name._list_fd);

      MSG("Timeslice (%d): %s%s%s_YYYYMMDD_HHMMSS%s (list: %s)",
	  _config._timeslice,
	  _ts._timeslice_name._dir,
	  _config._timeslice_subdir ? "YYYYMMDD_HHMMSS/" : "",
	  _ts._timeslice_name._base,_ts._timeslice_name._ext,
	  listname);

      free(listname);
    }
#endif

  struct sigaction action;
  memset(&action,0,sizeof(action));
  action.sa_handler = sigint_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags   = 0;
  sigaction(SIGINT,&action,NULL);

  if (!_config._forked
#if USING_CERNLIB || USING_ROOT
      || _config._timeslice
#endif
#if USING_ROOT
      || _config._autosave
#endif
      )
    {
      struct itimerval ival;

      memset(&ival,0,sizeof(ival));

      ival.it_interval.tv_sec = 1;
      ival.it_value.tv_sec    = 1;

      setitimer(ITIMER_REAL,&ival,NULL);

      memset(&action,0,sizeof(action));
      action.sa_handler = sigalarm_handler;
      sigemptyset(&action.sa_mask);
      action.sa_flags   = 0;
      sigaction(SIGALRM,&action,NULL);
    }

#if STRUCT_WRITER
  memset(&action,0,sizeof(action));
  action.sa_handler = sigio_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags   = 0;
  sigaction(SIGIO,&action,NULL);
#endif

  if (_config._comms->_shm_fd == -1)
    {
      pipe_communication(_config._comms);
      return 0;
    }

  if (_config._comms->_next)
    {
      // No fundamental problem, just not implemented.
      ERR_MSG("Cannot handle several input sources with SHM.");
    }

  // SHM communication

  ext_write_config_comm *comm = _config._comms;
  shared_mem_circular *shmc = comm->_shmc;

  // Check the size of the shared memory.  Should
  // be at least 4096 bytes (default)

  struct stat st;

  int r = fstat(comm->_shm_fd, &st);

  if (r != 0)
    {
      perror("stat");
      write_response(comm, EXTERNAL_WRITER_RESPONSE_SHM_ERROR);
      ERR_MSG("Cannot determine shared memory size.");
    }

  // MSG("stat shared mem: %d - size: %d",r,st.st_size);

  if (st.st_size < EXTERNAL_WRITER_MIN_SHARED_SIZE)
    {
      write_response(comm,EXTERNAL_WRITER_RESPONSE_SHM_ERROR);
      ERR_MSG("Size of shared memory unexpectedly small %d < %d.  Try NOSHM.",
	      (int) st.st_size,EXTERNAL_WRITER_MIN_SHARED_SIZE);
    }

  shmc->_len = EXTERNAL_WRITER_MIN_SHARED_SIZE;

  // Try to mmap our shared memory

  shmc->_ptr = (char *) mmap(0, shmc->_len, (PROT_READ | PROT_WRITE),
			     MAP_SHARED, comm->_shm_fd, 0);

  if (shmc->_ptr == MAP_FAILED)
    {
      write_response(comm,EXTERNAL_WRITER_RESPONSE_SHM_ERROR);
      ERR_MSG("Error mapping shared memory segment.");
    }

  shmc->_ctrl = (external_writer_shm_control*) shmc->_ptr;

  if (shmc->_ctrl->_magic != EXTERNAL_WRITER_MAGIC)
    ERR_MSG("Bad magic (%08x) in shared memory segment control.",
	    shmc->_ctrl->_magic);
  if (shmc->_ctrl->_len != shmc->_len)
    ERR_MSG("Shared memory segment control check size (%zd) "
	    "wrong (!= %zd).",shmc->_ctrl->_len,shmc->_len);

  shmc->_cur = shmc->_begin = (char*) (shmc->_ctrl + 1);
  shmc->_end  = shmc->_ptr + shmc->_len;
  shmc->_size = shmc->_end - shmc->_begin;

  for ( ; ; )
    {
      char cmd;
      int ret;

      // We must only try to get a token if have no data available.
      // Otherwise we might get stuck in a race condition with the
      // setting of _need_consumer_wakeup at the end of the loop.
      if (shmc->_ctrl->_done == shmc->_ctrl->_avail)
	{
#if STRUCT_WRITER
	  // Let the server run until there is data in the pipe

	  while (!ext_net_io_select_clients(comm->_pipe_in,-1,false,false))
	    ;
#endif

	  ret = read(comm->_pipe_in,&cmd,sizeof(cmd));

	  if (ret == 0)
	    {
	      // This is the normal error if someone hits ctrl+C on the
	      // controlling program. Tell our clients things went sour.
	      send_abort();
	      close_output(); // close outputs gracefully
	      ERR_MSG("Unexpected end of command pipe.");
	    }

	  if (ret == -1)
	    {
	      if (errno == EINTR)
		continue; // try again
	      perror("read");
	      ERR_MSG("Unexpected error reading command pipe.");
	    }

	  // MSG("Command: %d",cmd);

	  if (cmd != EXTERNAL_WRITER_CMD_SHM_WORK)
	    ERR_MSG("Unknown command on command pipe: %d",cmd);

	  // Now, eat messages as long as there are any
	}

      while (shmc->_ctrl->_done != shmc->_ctrl->_avail)
	{
	  external_writer_buf_header *header =
	    (external_writer_buf_header *) shmc->_cur;
	  size_t lin_left = shmc->_end - shmc->_cur;

	  uint32_t request = ntohl(header->_request);

	  request &= EXTERNAL_WRITER_REQUEST_LO_MASK;

	  if (request == EXTERNAL_WRITER_BUF_EAT_LIN_SPACE)
	    {
	      //printf ("WASTE: \n");
	      // waste the space at the end
	      shmc->_ctrl->_done += lin_left;
	      shmc->_cur = shmc->_begin;
	      continue;
	    }

	  if (lin_left < sizeof(external_writer_buf_header))
	    ERR_MSG("Too little linear SHM space left for header (req=%d).",
		    request);

	  // MSG("CONSUME: %08x %08x (%x) [%08x].",_ctrl->_done,_cur-(char*)_ctrl,header->_length,header->_request);

	  uint32_t length  = ntohl(header->_length);

	  if (length & 3)
	    ERR_MSG("SHM message length unaligned (req=%d,len=%d).",
		    request,length);
	  if (length > lin_left)
	    ERR_MSG("SHM message length longer than linear space left (req=%d,len=%d).",
		    request,length);

	  // The header may be unusable after we've remapped the memory

	  // So message header is ok.  Handle the message

	  if (handle_request(comm,header))
	    {
	      // quit
	      close_output();
	      // As we will exit, update to tell that we're done with
	      // the messages
	      shmc->_ctrl->_done += length;
	      write_response(comm,EXTERNAL_WRITER_RESPONSE_DONE);
	      exit(0);
	    }

	  if (request == EXTERNAL_WRITER_BUF_SETUP_DONE_RD)
	    {
	      // The writer is actually stuck, so no harm in updating _done
	      shmc->_ctrl->_done += length;
	      goto reader_loop;
	    }

	  // We are done with the message.  Consume it.
	  shmc->_cur += length;
	  if (shmc->_cur == shmc->_end)
	    shmc->_cur = shmc->_begin; // start over from beginning
	  assert(shmc->_cur + sizeof (uint32_t) <= shmc->_end);
	  MFENCE; // we cannot write that we're done until we wont use
		  // the data any longer
	  shmc->_ctrl->_done += length;

	  // Did we by chance clean up enough of the buffer, that we
	  // should wake the consumer up?

	  if (shmc->_ctrl->_need_producer_wakeup &&
	      (((int) shmc->_ctrl->_done) -
	       ((int) shmc->_ctrl->_wakeup_done)) >= 0)
	    {
	      shmc->_ctrl->_need_producer_wakeup = 0;
	      MFENCE;

#if STRUCT_WRITER
	      // Let the server run until it is allowed to write the
	      // response.

	      // If we got signalled, remove the marker
	      _got_sigio = 0;
	      MFENCE;
	      while (!ext_net_io_select_clients(-1,comm->_pipe_out,
						false,false))
		;
#endif
	      // As the responses we write
	      write_response(comm,EXTERNAL_WRITER_RESPONSE_WORK);
	    }

#if STRUCT_WRITER
	  if (_got_sigio)
	    {
	      // Some client (may) want to send some data.  Let it
	      // have a go.
	      _got_sigio = 0;
	      MFENCE;
	      ext_net_io_select_clients(-1,-1,false,true);
	    }
#endif
	}

      // If the protocol would involve sending lots of commands over
      // the pipe (STDIN) it would not be good to always set the
      // request to wake us up whenever more has been written to the
      // shared memory.  But, in general there are no commands sent.
      // And it anyhow is only checked whenever we run out of job to
      // do

      // some hysteresis, as we anyhow went to sleep
      shmc->_ctrl->_wakeup_avail = shmc->_ctrl->_done + (shmc->_size >> 4);
      MFENCE;
      shmc->_ctrl->_need_consumer_wakeup = 1;
      MFENCE;

      // Must check that data did not become available just at the
      // update, in which case the producer may miss to inform us
    }

 reader_loop:
  // We are to read events instead, so our loop drives the event flow.

  // At this point, the producer is standing around waiting for us to
  // start producing data.  It has forcefully put itself into waiting
  // for a _consumer_ wakeup token.  However, it did not set the
  // request, as that may still have been needed for us.  That way, it
  // will not interfer with the control segment further until we have
  // come here.
  //
  // - First verify that the control segment was in sync at this point
  //
  // - Then reinitialise the the avail and done variables.  Request
  //   the wakeup.
  //
  // - Then we are ready to go.  As we can not have removed the
  //   consumer wants a token flag, we must only make sure the
  //   wakeup_avail flag is at a right place.

  if (shmc->_ctrl->_avail != shmc->_ctrl->_done)
    ERR_MSG("SHM buffer not in sync after setup done->read message.");

  shmc->_ctrl->_avail = shmc->_ctrl->_done = 0;
  shmc->_cur = shmc->_begin;

  shmc->_ctrl->_wakeup_avail = shmc->_ctrl->_done + (shmc->_size >> 4);
  shmc->_ctrl->_need_consumer_wakeup = 1;

  size_t max_msg_size =
    sizeof(external_writer_buf_header) + 3 * sizeof(uint32_t) +
    _g._max_offset_array_items * sizeof(uint32_t);

  for ( ; ; )
    {
      // While the buffer has enough space available

      while (shmc->_ctrl->_avail + max_msg_size - shmc->_ctrl->_done <=
	     shmc->_size)
	{
	  // Is the linear space enough?

	  size_t lin_left = shmc->_end - shmc->_cur;

	  if (lin_left < max_msg_size)
	    {
	      if (lin_left != 0) // may have been due to just ending at the end
		{
		  assert ((shmc->_end - shmc->_cur) >=
			  (ssize_t) sizeof(uint32_t));
		  // put a marker that eats all the space
		  *((uint32_t*) shmc->_cur) =
		    htonl(EXTERNAL_WRITER_BUF_EAT_LIN_SPACE);
		  shmc->_ctrl->_avail += lin_left;
		}
	      shmc->_cur = shmc->_begin;
	      continue; // try again
	    }

	  char *end_msg = shmc->_cur + max_msg_size;

	  bool more_msg = ntuple_get_event(shmc->_cur,&end_msg);

	  //MSG("max_size: %d  avail: %d  this: %d",
	  //     max_msg_size,_end - _cur,end_msg - _cur);

	  assert (end_msg <= shmc->_end);

	  shmc->_ctrl->_avail += end_msg - shmc->_cur;
	  shmc->_cur = end_msg;

	  if (!more_msg)
	    goto read_done;

	  if (shmc->_ctrl->_need_consumer_wakeup &&
	      (((int) shmc->_ctrl->_avail) -
	       ((int) shmc->_ctrl->_wakeup_avail)) >= 0)
	    {
	      shmc->_ctrl->_need_consumer_wakeup = 0;
	      SFENCE;
	      write_response(comm,EXTERNAL_WRITER_RESPONSE_WORK_RD);
	    }
	}

      // Out of SHM buf space.  Send a token to the reader to get
      // cracking!  (If it wanted to be woken up).  As the last avail
      // addition may have been to eat linear space, we must check
      // again.

      if (shmc->_ctrl->_need_consumer_wakeup)
	{
	  shmc->_ctrl->_need_consumer_wakeup = 0;
	  SFENCE;
	  write_response(comm,EXTERNAL_WRITER_RESPONSE_WORK_RD);
	}

      // Tell that we want to get a token when some space has been
      // free'd.

      MFENCE;
      shmc->_ctrl->_wakeup_done =
	shmc->_ctrl->_avail - shmc->_size + max_msg_size + (shmc->_size >> 4);
      MFENCE;
      shmc->_ctrl->_need_producer_wakeup = 1;
      MFENCE;

      // If enough space was not freed in the meantime.  Then the
      // consumer was still active, so it will send a token at some
      // point.

      if (shmc->_ctrl->_avail + max_msg_size - shmc->_ctrl->_done > shmc->_size)
	{
	  // Wait for token!  I.e. read the control pipe...

	  char cmd;
	  int ret;

	  ret = read(comm->_pipe_in,&cmd,sizeof(cmd));

	  if (ret == 0)
	    {
	      // This is the normal error if someone hits ctrl+C on the
	      // controlling program. Tell our clients things went sour.
	      send_abort();
	      close_output(); // close outputs gracefully
	      ERR_MSG("Unexpected end of command pipe.");
	    }

	  if (ret == -1)
	    {
	      if (errno == EINTR)
		continue; // try again
	      perror("read");
	      ERR_MSG("Unexpected error reading command pipe.");
	    }

	  // MSG("Command: %d",cmd);

	  if (cmd != EXTERNAL_WRITER_CMD_SHM_WORK)
	    ERR_MSG("Unknown command on command pipe: %d",cmd);

	  // Now, eat messages as long as there are any
	}
    }

 read_done:
  // So, the last message inserted was the done message.  Just wake
  // the consumer up one last time (in case it went to sleep), and
  // then we are done.

  SFENCE;
  write_response(comm,EXTERNAL_WRITER_RESPONSE_WORK_RD);

  close_output(); // needed? - will at least tell the number of consumed events
  exit(0);
}

