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

#include "paw_ntuple.hh"

#include "event_base.hh"

#include "hbook.hh"
#include "staged_ntuple.hh"
#include "staging_ntuple.hh"
#include "writing_ntuple.hh"
#include "ntuple_item.hh"

#include "parse_util.hh"
#include "lmd_output.hh"

#include "detector_requests.hh"

#include "../common/strndup.hh"

#include <map>

#ifdef __APPLE__
#include <libgen.h>
#endif

struct ntuple_limit_item
{
  ntuple_item *_item;
  size_t       _omit_index;
};

typedef std::map<const void *,ntuple_limit_item> map_ntuple_limits;

struct enumerate_ntuple_info
{
  vect_ntuple_items *_listing;
  detector_requests *_requests;
  bool               _cwn;
  map_ntuple_limits  _limits;
  int                _level;
  bool               _detailed_only;
  int                _toggle_include;
  bool               _include_always;
  const char        *_block;
  const char        *_block_prefix;
};

void enumerate_member_paw_ntuple(const signal_id &id,
				 const enumerate_info &info,
				 void *extra)
{
  enumerate_ntuple_info *ntuple = (enumerate_ntuple_info *) extra;

  if (info._type & ENUM_NTUPLE_NEVER)
    return;
  else if (info._type & ENUM_NTUPLE_ALWAYS)
    {
      if (!ntuple->_include_always)
	return;
      // Do include
    }
  else if (ntuple->_include_always)
    return;
  else if (!(ntuple->_requests->
	     is_channel_requested(id,info._type & (ENUM_IS_LIST_LIMIT |
						   ENUM_IS_ARRAY_MASK),
				  ntuple->_level, ntuple->_detailed_only)))
    return;

  if (info._type & (ENUM_IS_TOGGLE_I |
		    ENUM_IS_TOGGLE_V) &&
      !(info._type & ntuple->_toggle_include))
    return;

  const zero_suppress_info *zzp_info = NULL;

  const void *const* ptr_offset =
    info._type & ENUM_HAS_PTR_OFFSET ? info._ptr_offset : NULL;

  if (ptr_offset)
    {
      WARNING("Cowardly refusing paw_ntuple item with ptr_offset.");
      // The ntuple stager/writer needs to be fixed to also handle
      // variables that may have an offset to their pointers...
      // For the time being, refuse those...
      return;
    }

  if (!(info._type & (ENUM_IS_LIST_LIMIT |
		      ENUM_IS_ARRAY_MASK)))
    zzp_info = get_ptr_zero_suppress_info((void*) info._addr,
					  ptr_offset,
					  false);

  // printf ("paw_ntuple item: "/*"%s"*/" (%p)  (zzp:%d)\n",
  //         /*name,*/info._addr,zzp_info ? zzp_info->_type : -1);

  const void  *limit_addr = NULL;
  uint         limit_index = (uint) -1;
  ntuple_item *limit_item = NULL;

  const uint32 *limit2_addr = NULL;
  uint          limit2_index = (uint) -1;
  ntuple_item  *limit_item2 = NULL;

  if (ntuple->_cwn && zzp_info)
    {
      // Item is part of an array, that has an controlling item (count
      // or bitmask)

      switch (zzp_info->_type)
	{
	case ZZP_INFO_NONE:
	  break; // actually not indexed (or no controlling item)
	case ZZP_INFO_FIXED_LIST:
	  ERROR("Unimplemented.");
	  limit_index = zzp_info->_fixed_list._index;
	  break;
	case ZZP_INFO_CALL_ARRAY_INDEX:
	  limit_addr = zzp_info->_array._limit_mask;
	  limit_index = zzp_info->_array._index;
	  break;
	case ZZP_INFO_CALL_LIST_INDEX:
	  // Find the name of the controlling variable!
	  // item->_index_var = ;
	  limit_addr = zzp_info->_list._limit;
	  limit_index = zzp_info->_list._index;
	  break;
	case ZZP_INFO_CALL_LIST_II_INDEX:
	  limit_addr = zzp_info->_list_ii._limit;
	  limit_index = zzp_info->_list_ii._index_x;
	  break;
        case ZZP_INFO_CALL_ARRAY_MULTI_INDEX:
	  limit_addr = zzp_info->_array._limit_mask;
	  limit_index = zzp_info->_array._index;
	  goto limit2_list_ii_index;
	case ZZP_INFO_CALL_ARRAY_LIST_II_INDEX:
	  limit_addr = zzp_info->_array._limit_mask;
	  limit_index = zzp_info->_array._index;
	  goto limit2_list_ii_index;
	case ZZP_INFO_CALL_LIST_LIST_II_INDEX:
	  limit_addr = zzp_info->_list._limit;
	  limit_index = zzp_info->_list._index;
	  goto limit2_list_ii_index;
	limit2_list_ii_index:
	  limit2_addr = zzp_info->_list_ii._limit;
	  limit2_index = zzp_info->_list_ii._index_x;
	  if (zzp_info->_type != ZZP_INFO_CALL_ARRAY_MULTI_INDEX)
	    ERROR("Unimplemented - no support for "
		  "zero suppression + multi entry in ntuples (%d).",
		  zzp_info->_type);
	  break;
	default:
	  ERROR("Unimplemented");
	  break;
	}
    }

  char prel_name[256];
  {
    signal_id id_fmt(id);
    id_fmt.format_paw(prel_name,sizeof(prel_name));
  }

  size_t omit_index = (size_t) -1;

  if (limit_addr)
    {
      // printf ("paw_ntuple limit_addr: %p\n",
      //	      limit_addr);

      // Find limiting item

      map_ntuple_limits::const_iterator i;

      i = ntuple->_limits.find(limit_addr);

      if (i == ntuple->_limits.end())
	ERROR("(Internal error?) Failed to find limiting item for '%s'...",
	      prel_name);

      omit_index = i->second._omit_index;
      limit_item = i->second._item;
    }

  size_t omit_index2 = (size_t) -1;

  if (limit2_addr)
    {
      // Find limiting item

      map_ntuple_limits::const_iterator i;

      i = ntuple->_limits.find(limit2_addr);

      if (i == ntuple->_limits.end())
	ERROR("(Internal error?) Failed to find limiting item 2 for '%s'...",
	      prel_name);

      omit_index2 = i->second._omit_index;
      limit_item2 = i->second._item;
    }

  char name[256];
  {
    signal_id id_fmt(id);

    // Rewrite the name to get rid of unwanted parts (_u32 and data)

    const sig_part &last = id._parts.back();

    if (last._type == SIG_PART_NAME &&
	strcmp(last._id._name,"u32") == 0)
      id_fmt._parts.pop_back();
    else if (last._type == SIG_PART_NAME &&
	     strcmp(last._id._name,"data") == 0)
      {
	if (zzp_info && zzp_info->_type != ZZP_INFO_NONE)
	  {
	    // It is in some kind of vector
	    id_fmt._parts.pop_back();
	  }
	else if (info._type & (ENUM_IS_LIST_LIMIT | ENUM_IS_ARRAY_MASK))
	  {
	    id_fmt._parts.pop_back();
	    id_fmt.push_back("n");
	  }
      }
    if (info._type & ENUM_IS_LIST_INDEX)
      id_fmt.push_back("i");

    strcpy(name,ntuple->_block_prefix);
    size_t off = strlen(name);

    id_fmt.format_paw(name+off,sizeof(name)-off,omit_index,omit_index2);

    //printf ("--- %s --- (%p, %d)\n", name, limit2_addr, limit2_index);
  }

  ntuple_item *item;
  int type = info._type & ENUM_TYPE_MASK;

  switch (type)
    {
    case ENUM_TYPE_DOUBLE:
      item = new ntuple_item(name,(double*) info._addr,ptr_offset);
      break;
    case ENUM_TYPE_FLOAT:
      item = new ntuple_item(name,(float*)  info._addr,ptr_offset);
      break;
    case ENUM_TYPE_DATA12:
    case ENUM_TYPE_DATA14:
      // needs full 16 bits, as it sometimes really is 12 bits+overflow+range
    case ENUM_TYPE_DATA16:
    case ENUM_TYPE_USHORT:
      item = new ntuple_item(name,(ushort*) info._addr,ptr_offset);
      break;
    case ENUM_TYPE_DATA8:
    case ENUM_TYPE_UCHAR:
      item = new ntuple_item(name,(unsigned char*) info._addr,ptr_offset);
      break;
    case ENUM_TYPE_DATA16PLUS:
      // Stores only value? Then could go to 16 bits.
    case ENUM_TYPE_DATA24:
      // could be cut down to 24 bits limits...
    case ENUM_TYPE_DATA32:
    case ENUM_TYPE_UINT:
      item = new ntuple_item(name,(uint*)   info._addr,ptr_offset);
      break;
    case ENUM_TYPE_INT:
      item = new ntuple_item(name,(int*)    info._addr,ptr_offset);
      break;
    case ENUM_TYPE_ULINT:
      item = new ntuple_item(name,(int*)    info._addr,ptr_offset);
      item->_flags |= NTUPLE_ITEM_IS_ARRAY_MASK;
      break;
    default:
      ERROR("Unhandled ENUM_TYPE (0x%x) for item '%s'.",type, name);
    }

  if (info._type & ENUM_IS_LIST_INDEX)
    {
      assert(item->_type == ntuple_item::INT);
      item->_type = ntuple_item::INT_INDEX_CUT;
    }

  //printf ("paw_ntuple item: %s (%p)  (zzp:%d)\n",
  //	  name,info._addr,zzp_info ? zzp_info->_type : -1);

  if (info._type & (ENUM_IS_LIST_LIMIT | ENUM_IS_ARRAY_MASK))
    {
      //printf ("paw_ntuple item limit: %s (%p) (%zd)\n",
      //	      name,info._addr,id._parts.size());

      ntuple_limit_item limit_item;

      limit_item._item = item;
      limit_item._omit_index = id._parts.size();

      ntuple->_limits.insert(map_ntuple_limits::value_type(info._addr,
							   limit_item));
    }

  if (limit_addr)
    {
      //printf ("paw_ntuple limit_addr: %s %p (%s : %d)\n",
      //	      name,limit_addr,
      //	      (const char *) limit_item->_name.c_str(),
      //	      limit_index);

      // item->_index_var = ;
      item->_index_var = limit_item->_name;
      item->_index     = (uint) limit_index;

      if (limit2_addr)
       {
         item->_ptr_limit2 = (const int*) limit2_addr; // <- const uint32 *
	 item->_max_limit2 = limit_item2->_limits._int._max;
         item->_index2     = limit2_index;
       }
    }

  /*
  if (info._cwn && cwn_index_postfix)
    {
      item->_index_var = ;
      item->_index     = id.get_index(0);
    }
  */
  if (info._type & ENUM_HAS_INT_LIMIT)
    {
      item->_flags |= NTUPLE_ITEM_HAS_LIMIT;
      item->_limits._int._min = info._min;
      item->_limits._int._max = info._max;
    }

  item->_block = ntuple->_block;

  if (info._type & ENUM_IS_LIST_LIMIT2)
    item->_flags |= NTUPLE_ITEM_OMIT;

  ntuple->_listing->push_back(item);

  //printf ("\n");
}

#define NTUPLE_WRITER_UNPACK 0x0001
#define NTUPLE_WRITER_RAW    0x0002
#define NTUPLE_WRITER_CAL    0x0004
#define NTUPLE_WRITER_USER   0x0008

const char *request_level_str(int level)
{
  switch (level)
    {
    case NTUPLE_WRITER_UNPACK: return "UNPACK";
    case NTUPLE_WRITER_RAW:    return "RAW";
    case NTUPLE_WRITER_CAL:    return "CAL";
    case NTUPLE_WRITER_USER:   return "USER";
    case 0:                    return "any";
    default: assert (false);   return "*error*";
    }
}

void paw_ntuple_usage()
{
  printf ("\n");
  printf ("HBOOK/ROOT/STRUCT output (--ntuple) options:\n");
  printf ("\n");
  printf ("UNPACK,RAW,CAL,USER         Include requested level data.\n");
  printf ("[UNPACK|RAW|CAL|USER]:name  Include 'name'd member of level data.\n");
  printf ("UR,URC,URCUS        Prefix UNPACK,RAW,(CAL),(USER) level data with U,R,C,U.\n");
  printf ("TGLV                Include both toggle values.\n");
  printf ("NOTGLI              Do not include toggle index.\n");
  printf ("NOTRIGEVENTNO       Do not include TRIG, EVENTNO members.\n");
  printf ("CWN                 Produce columnwise HBOOK ntuple (default with .nt*/.hb*).\n");
  printf ("ROOT                Produce ROOT tree (default with .root).\n");
  printf ("STRUCT|SERVER       Run STRUCT server to send data.\n");
  printf ("STRUCT_HH           Produce header file for STRUCT server data.\n");
  printf ("port=N              Run STRUCT server on port N.\n");
  printf ("UPPER               Make all variable names upper case.\n");
  printf ("LOWER               Make all variable names lower case.\n");
  printf ("H2ROOT              Make all variable names like h2root.\n");
  printf ("id=ID               Set the ntuple ID.\n");
  printf ("title=TITLE         Set the tree TITLE.\n");
  printf ("ftitle=FTITLE       Set the File TITLE.\n");
  printf ("timeslice=N[:SUB]   Timeslice data, divide in SUBdirectories.\n");
  printf ("autosave[=N]        Autosave root file.\n");
#if defined(USE_LMD_INPUT)
  printf ("rawdata=N           Include raw data, max size N [ki|Mi|Gi].\n");
  printf ("incl=               Subevent inclusion.\n");
  printf ("excl=               Subevent exclusion.\n");
#endif
  printf ("NOSHM               Do not use shared memory communication.\n");
  printf ("GDB                 Run the external program via gdb (backtrace fault).\n");
  printf ("VALGRIND            Run the external program via valgrind.\n");
  printf ("\n");
}

paw_ntuple::paw_ntuple()
{
  for (int i = 0; i < PAW_NTUPLE_NUM; i++)
    _staged[i] = NULL;
#if defined(USE_LMD_INPUT)
  _raw_select = NULL;
  _raw_event = NULL;
#endif
}

paw_ntuple::~paw_ntuple()
{
  close();
#if defined(USE_LMD_INPUT)
  delete _raw_select;
  delete _raw_event;
#endif
}

paw_ntuple *paw_ntuple_open_stage(const char *command,bool reading)
{
  /* First parse the command.
   *
   * It is a comma separated list of:
   * - Signal id requests, i.e. TFW, N1-5, DSSSD3, etc
   * - Level requests.
   * - Filename (has to be the last one, i.e. no comma after it.
   */

  detector_requests requests;
  detector_requests sticky_requests;
  int request_level = 0;
  int request_level_detailed = 0;
  int prefix_level = 0;
#if defined(USE_LMD_INPUT)
  uint64_t max_raw_size = 0;
#endif
  int toggle_include = ENUM_IS_TOGGLE_I;
  bool inc_trig_eventno = true;

  uint ntuple_type = 0;
  uint ntuple_opt = 0;

  char *id = NULL;     // of ntuple (root: name)
  char *title = NULL;  // of ntuple
  char *ftitle = NULL; // of file (hbook: file top)

  int struct_server_port = -1;

  int timeslice = 0;
  int timeslice_subdir = 0;
  int autosave = 0;

  paw_ntuple *ntuple = new paw_ntuple;

  if (!ntuple)
    ERROR("Memory allocation error (ntuple: ntuple).");

#if defined(USE_LMD_INPUT)
  ntuple->_raw_select = new select_event;

  if (!ntuple->_raw_select)
    ERROR("Memory allocation error (ntuple: select).");
#endif

  if (reading)
    ntuple_opt |= NTUPLE_OPT_READER_INPUT;

  const char *req_end;

  while ((req_end = strchr(command,',')) != NULL)
    {
      char *request = strndup(command,(size_t) (req_end-command));

      char *post;

#define MATCH_PREFIX(prefix,post) (strncmp(request,prefix,strlen(prefix)) == 0 && *(post = request + strlen(prefix)) != '\0')
#define MATCH_ARG(name) (strcmp(request,name) == 0)

      // printf ("Request: %s\n",request);

      if (MATCH_ARG("HELP") || MATCH_ARG("help"))
	{
	  paw_ntuple_usage();
	  exit(0);
	}
      else if (MATCH_ARG("UNPACK"))
	request_level |= NTUPLE_WRITER_UNPACK;
      else if (MATCH_ARG("RAW"))
	request_level |= NTUPLE_WRITER_RAW;
      else if (MATCH_ARG("CAL"))
	request_level |= NTUPLE_WRITER_CAL;
      else if (MATCH_ARG("USER"))
	request_level |= NTUPLE_WRITER_USER;
      else if (MATCH_ARG("NOTGLI"))
	toggle_include &= ~ENUM_IS_TOGGLE_I;
      else if (MATCH_ARG("TGLV"))
	toggle_include |= ENUM_IS_TOGGLE_V;
      else if (MATCH_ARG("NOTRIGEVENTNO"))
	inc_trig_eventno = false;
      else if (MATCH_PREFIX("UNPACK:",post))
	{
	  request_level_detailed |= NTUPLE_WRITER_UNPACK;
	  requests.add_detector_request(post,NTUPLE_WRITER_UNPACK);
	}
      else if (MATCH_PREFIX("RAW:",post))
	{
	  request_level_detailed |= NTUPLE_WRITER_RAW;
	  requests.add_detector_request(post,NTUPLE_WRITER_RAW);
	}
      else if (MATCH_PREFIX("CAL:",post))
	{
	  request_level_detailed |= NTUPLE_WRITER_CAL;
	  requests.add_detector_request(post,NTUPLE_WRITER_CAL);
	}
      else if (MATCH_PREFIX("USER:",post))
	{
	  request_level_detailed |= NTUPLE_WRITER_USER;
	  requests.add_detector_request(post,NTUPLE_WRITER_USER);
	}
      else if (MATCH_ARG("UR"))
	prefix_level |= (NTUPLE_WRITER_UNPACK | NTUPLE_WRITER_RAW);
      else if (MATCH_ARG("URC"))
	prefix_level |= (NTUPLE_WRITER_UNPACK | NTUPLE_WRITER_RAW |
			 NTUPLE_WRITER_CAL);
      else if (MATCH_ARG("URCUS"))
	prefix_level |= (NTUPLE_WRITER_UNPACK | NTUPLE_WRITER_RAW |
			 NTUPLE_WRITER_CAL |    NTUPLE_WRITER_USER);
      else if (MATCH_ARG("RWN")) {
	ERROR("Support for row-wise ntuples (RWN) has been removed.");
      }
      else if (MATCH_ARG("CWN"))
	ntuple_type |= NTUPLE_TYPE_CWN;
      else if (MATCH_ARG("ROOT"))
	ntuple_type |= NTUPLE_TYPE_ROOT;
      else if (MATCH_ARG("STRUCT_HH"))
	ntuple_type |= NTUPLE_TYPE_STRUCT_HH;
      else if (MATCH_ARG("STRUCT") || MATCH_ARG("SERVER"))
	ntuple_type |= NTUPLE_TYPE_STRUCT;
      else if (MATCH_PREFIX("PORT=",post) || MATCH_PREFIX("port=",post))
	struct_server_port = atoi(post);
      else if (MATCH_ARG("NOEXTERNAL")) {
	ERROR("Support for internal ntuple writing has been removed.");
      }
      else if (MATCH_ARG("UPPER"))
	ntuple_type |= NTUPLE_CASE_UPPER;
      else if (MATCH_ARG("LOWER"))
	ntuple_type |= NTUPLE_CASE_LOWER;
      else if (MATCH_ARG("H2ROOT"))
	ntuple_type |= NTUPLE_CASE_H2ROOT;
      else if (MATCH_PREFIX("ID=",post) || MATCH_PREFIX("id=",post))
	id = strdup(post);
      else if (MATCH_PREFIX("TITLE=",post) || MATCH_PREFIX("title=",post))
	title = strdup(post);
      else if (MATCH_PREFIX("FTITLE=",post) || MATCH_PREFIX("ftitle=",post))
	ftitle = strdup(post);
      else if (MATCH_PREFIX("TIMESLICE=",post) ||
	       MATCH_PREFIX("timeslice=",post))
	{
	  timeslice = atoi(post);
	  char *colon = strchr(post,':');
	  if (colon)
	    timeslice_subdir = atoi(colon+1);
	}
      else if (MATCH_ARG("AUTOSAVE") || MATCH_ARG("autosave"))
	autosave = 1;
      else if (MATCH_PREFIX("AUTOSAVE=",post) ||
	       MATCH_PREFIX("autosave=",post))
	autosave = atoi(post);
#if defined(USE_LMD_INPUT)
      else if (MATCH_PREFIX("rawdata=",post))
	max_raw_size = parse_size_postfix(post,"kMG","Rawdata",true);
      else if (MATCH_PREFIX("incl=",post))
	ntuple->_raw_select->parse_request(post,true);
      else if (MATCH_PREFIX("excl=",post))
	ntuple->_raw_select->parse_request(post,false);
#endif
      else if (MATCH_ARG("NOSHM"))
	ntuple_opt |= NTUPLE_OPT_WRITER_NO_SHM;
      else if (MATCH_ARG("GDB"))
	ntuple_opt |= NTUPLE_OPT_EXT_GDB;
      else if (MATCH_ARG("VALGRIND"))
	ntuple_opt |= NTUPLE_OPT_EXT_VALGRIND;
      else
	requests.add_detector_request(request,0);

      free(request);
      command = req_end+1;
    }

  const char *filename = command;

  if (strcmp(filename,"help") == 0)
    {
      paw_ntuple_usage();
      exit(0);
    }

  if (!(ntuple_type & NTUPLE_TYPE_MASK))
    {
      const char *last_slash = strrchr(filename,'/');
      if (!last_slash)
	last_slash = filename;

      if (strstr(last_slash,".nt") || strstr(last_slash,".hb"))
	ntuple_type |= NTUPLE_TYPE_CWN;
      if (strstr(last_slash,".root"))
	ntuple_type |= NTUPLE_TYPE_ROOT;

      if (strcmp(last_slash+strlen(last_slash)-2,".h") == 0 ||
	  strcmp(last_slash+strlen(last_slash)-3,".hh") == 0)
	ntuple_type |= NTUPLE_TYPE_STRUCT_HH;
    }
  if (!(ntuple_type & NTUPLE_TYPE_MASK))
    ERROR("Cannot determine / no ntuple type given.");

  if (!ftitle)
    ftitle = strdup("ucesb");
  if (!title)
    title = strdup("CWNtuple");

#if defined(USE_LMD_INPUT)
  if (ntuple->_raw_select->has_selection() &&
      !max_raw_size)
    ERROR("(sub)event selection in ntuple, but no maximum size given.");

  if (max_raw_size)
    {
      ntuple->_raw_event = new lmd_event_out;
      if (!ntuple->_raw_event)
	ERROR("Memory allocation error (ntuple: event_out).");
    }
  else
    {
      delete ntuple->_raw_select;
      ntuple->_raw_select = NULL;
    }
#endif

  requests.prepare();

  vect_ntuple_items listing;

  {
  enumerate_ntuple_info extra;

  extra._listing = &listing;
  extra._requests = &requests;
  extra._block = "";
  extra._level = 0;
  extra._toggle_include = toggle_include;

  if (!request_level && !request_level_detailed)
    {
#ifdef USER_STRUCT
      request_level |= NTUPLE_WRITER_USER;
#else
      request_level |= NTUPLE_WRITER_RAW;
#endif
    }

  extra._cwn = !!(ntuple_type & (NTUPLE_TYPE_CWN |
				 NTUPLE_TYPE_ROOT |
				 NTUPLE_TYPE_STRUCT_HH |
				 NTUPLE_TYPE_STRUCT));

  extra._include_always = inc_trig_eventno; /* true */

#define ENUMERATE_MEMBERS_UNCOND(data, level,		    \
				 block, block_prefix)	    \
  {							    \
    extra._level = (level);				    \
    extra._detailed_only = false;			    \
    extra._block = block;				    \
    extra._block_prefix =				    \
      (prefix_level & (level)) ? block_prefix : "";	    \
    data.enumerate_members(signal_id(),enumerate_info(),    \
			   enumerate_member_paw_ntuple,	    \
			   &extra);			    \
  }

  ENUMERATE_MEMBERS_UNCOND(_static_event._unpack,
			   NTUPLE_WRITER_UNPACK, "UNPACK", "U");
  ENUMERATE_MEMBERS_UNCOND(_static_event._raw,
			   NTUPLE_WRITER_UNPACK, "RAW", "R");

  extra._include_always = false;

#define ENUMERATE_MEMBERS(data, level, block, block_prefix) \
  if ((request_level | request_level_detailed) & (level))   \
    {							    \
      extra._level = (level);				    \
      extra._detailed_only = !(request_level & (level));    \
      extra._block = block;				    \
      extra._block_prefix =				    \
	(prefix_level & (level)) ? block_prefix : "";	    \
      data.enumerate_members(signal_id(),enumerate_info(),  \
			     enumerate_member_paw_ntuple,   \
			     &extra);			    \
    }

  ENUMERATE_MEMBERS(_static_event._unpack,
		    NTUPLE_WRITER_UNPACK, "UNPACK", "U");
  ENUMERATE_MEMBERS(_static_event._raw,
		    NTUPLE_WRITER_RAW, "RAW", "R");
  ENUMERATE_MEMBERS(_static_event._cal,
		    NTUPLE_WRITER_CAL, "CAL", "C");

#ifdef USER_STRUCT
  ENUMERATE_MEMBERS(_static_event._user,
		    NTUPLE_WRITER_USER, "USER", "US");
#endif

  for (uint i = 0; i < requests._requests.size(); i++)
    if (!requests._requests[i]._checked)
      {
	if (strcmp(requests._requests[i]._str,"EVENTNO") == 0 ||
	    strcmp(requests._requests[i]._str,"MEVENTNO") == 0 ||
	    strcmp(requests._requests[i]._str,"TRIGGER") == 0)
	  WARNING("NTuple request for item %s (level %s) "
		  "is no longer needed.  Item is always included.",
		  requests._requests[i]._str,
		  request_level_str(requests._requests[i]._level));
	else
	  ERROR("NTuple request for item %s (level %s) was not considered.  "
		"Does that signal exist?",
		requests._requests[i]._str,
		request_level_str(requests._requests[i]._level));
      }
  }

  sticky_requests.prepare();

  vect_ntuple_items sticky_listing;

  {
  enumerate_ntuple_info extra;

  extra._listing = &sticky_listing;
  extra._requests = &sticky_requests;
  extra._block = "";
  extra._level = 0;
  extra._toggle_include = toggle_include;

  extra._cwn = !!(ntuple_type & (NTUPLE_TYPE_CWN |
				 NTUPLE_TYPE_ROOT |
				 NTUPLE_TYPE_STRUCT_HH |
				 NTUPLE_TYPE_STRUCT));

  extra._include_always = inc_trig_eventno; /* true */

  ENUMERATE_MEMBERS_UNCOND(_static_sticky_event._unpack,
			   NTUPLE_WRITER_UNPACK, "UNPACK", "U");
  ENUMERATE_MEMBERS_UNCOND(_static_sticky_event._raw,
			   NTUPLE_WRITER_UNPACK, "RAW", "R");

  extra._include_always = false;

  ENUMERATE_MEMBERS_UNCOND(_static_sticky_event._unpack,
			   NTUPLE_WRITER_UNPACK, "UNPACK", "U");
  ENUMERATE_MEMBERS_UNCOND(_static_sticky_event._raw,
			   NTUPLE_WRITER_UNPACK, "RAW", "W");

  }


  int hid = 101;

  if (ntuple_type & NTUPLE_TYPE_CWN)
    {
      if (id)
	{
	  // The id must be numeric

	  char *end;

	  hid = (int) strtol(id,&end,10);

	  if (end == id || *end != 0)
	    ERROR("HBOOK ntuple ID (%s) must be numeric.",
		  id);
	}
      if (strlen(title) > 255)
	ERROR("HBOOK ntuple title (%s) too long (max 255 chars).",
	      title);
      if (strlen(ftitle) > 16)
	ERROR("HBOOK file title (%s) too long (max 16 chars).",
	      ftitle);
    }

  if (!id)
    id = strdup("h101");

  ///

  for (int i = 0; i < PAW_NTUPLE_NUM; i++)
    {
#ifndef STICKY_EVENT_IS_NONTRIVIAL
# define STICKY_EVENT_IS_NONTRIVIAL 0
#endif
      if (i == PAW_NTUPLE_STICKY_EVENT &&
	  (!STICKY_EVENT_IS_NONTRIVIAL ||
	   reading))
	continue;

      // printf ("tree: %d\n",i);

      staged_ntuple *staged = new staged_ntuple;

      if (!staged)
	ERROR("Memory allocation error (ntuple: staged).");

      const char *this_id = NULL;
      const char *this_title = NULL;
      const char *this_index_major = "";
      const char *this_index_minor = "";

      if (i == PAW_NTUPLE_STICKY_EVENT)
	{
	  this_id     = "hsticky";
	  this_title  = "CWNsticky";
	  this_index_major = "STIDX";
	}
      else
	{
	  this_id     = id;
	  this_title  = title;
	}
      staged->_struct_index = i;

      if (i == 0)
	staged->open_x(filename,ftitle,
		       ntuple_type, ntuple_opt,
		       struct_server_port,
		       timeslice, timeslice_subdir,
		       autosave
#if defined(USE_LMD_INPUT)
		       ,1
#endif
		       ); // open file/start the writer
      else
	staged->set_ext(ntuple->_staged[0]->get_ext());

      staged->stage_x(i == 0 ? listing : sticky_listing,
		      hid,
		      this_id, this_title,
		      this_index_major, this_index_minor,
		      i == 0 ?
		      (void*) &_static_event : (void*) &_static_sticky_event
#if defined(USE_LMD_INPUT)
		      ,(uint) ((max_raw_size + sizeof(uint)-1) / sizeof(uint))
#endif
		      );

      ntuple->_staged[i] = staged;
    }

  assert(ntuple->_staged[0]);

  ntuple->_staged[0]->stage_done();

  free(id);
  free(title);
  free(ftitle);

  return ntuple;
}

#if defined(USE_LMD_INPUT)
void paw_ntuple_copy_raw(fill_raw_info *fill_raw)
{
  // printf ("FILL RAW\n");
  // for (uint32_t i = 0; i < fill_raw->_words; i++)
  //   fill_raw->_ptr[i] = 0xaa0000bb | (i << 8);

  char *ptr = (char *) fill_raw->_ptr;

  paw_ntuple *ntuple = (paw_ntuple *) fill_raw->_extra;
  lmd_event_out *event = ntuple->_raw_event;

  const buf_chunk_swap *chunk_cur = event->_chunk_start;

  // We always write in big-endican = network order
  const bool write_native = (__BYTE_ORDER == __BIG_ENDIAN);

  for ( ; chunk_cur < event->_chunk_end; chunk_cur++)
    {
      copy_to_buffer(ptr, chunk_cur->_ptr,
		     chunk_cur->_length,
                     !(write_native ^ chunk_cur->_swapping));

      ptr += chunk_cur->_length;
    }

  assert (ptr == (char *) (fill_raw->_ptr + fill_raw->_words));
}
#endif

void paw_ntuple::event(int kind)
{
#if defined(USE_LMD_INPUT)
  fill_raw_info fill_raw;

  if (_raw_event)
    {
      size_t length = _raw_event->get_length();

      length = (length + (sizeof (uint32_t) - 1)) / sizeof (uint32_t);

      if (length > _staged[kind]->_ext->_max_raw_words)
	ERROR("Too many words (%zd bytes > %zd bytes) "
	      "in raw data for ntuple (struct) output.",
	      length * sizeof (uint32_t),
	      _staged[kind]->_ext->_max_raw_words *
	      sizeof (uint32_t));

      fill_raw._words = (uint32_t) length;
      fill_raw._ptr = NULL;
      fill_raw._callback = paw_ntuple_copy_raw;
      fill_raw._extra = this;

      // printf ("RAW: %d\n",fill_raw._words);
    }
#endif

  _staged[kind]->event(kind == PAW_NTUPLE_NORMAL_EVENT ?
		       (void*) &_static_event : (void*) &_static_sticky_event
#if defined(USE_LMD_INPUT)
		       ,&_static_event._unpack.event_no
		       ,_raw_event ? &fill_raw : NULL
#endif
		       );
}

bool paw_ntuple::get_event()
{
  return _staged[PAW_NTUPLE_NORMAL_EVENT]->get_event();
}

void paw_ntuple::unpack_event()
{
  _staged[PAW_NTUPLE_NORMAL_EVENT]->unpack_event(&_static_event);
}

void paw_ntuple::close()
{
  if (_staged[0])
    _staged[0]->close();
  for (int i = 0; i < PAW_NTUPLE_NUM; i++)
    {
      delete _staged[i];
      _staged[i] = NULL;
    }
}
