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

#include "mc_def.hh"
#include "config.hh"

#include "file_line.hh"

#include "forked_child.hh"

#include "error.hh"

#include <stdlib.h>

#include <sys/stat.h>

def_node_list *all_mc_defs;

extern int lexer_read_fd;

bool parse_definitions();

void read_map_calib_info_file(const char *filename,bool must_exist)
{
  struct stat buf;

  if (stat(filename,&buf) == -1)
    {
      if (must_exist)
	{
	  ERROR("Mapping/calibration file '%s' does not exist.",filename);
	}
      else
	{
	  INFO("Optional map/calib file '%s' does not exist, is ok.",filename);
	}
      return;
    }

  INFO("Reading mapping/calibration file '%s'...",filename);

  forked_child fork;

  const char *argv[] = {
#if ((defined(__APPLE__) && defined(__MACH__)) || defined(__OpenBSD__))
	"g++", "-E", "-x", "c",
#else
	"cpp",
#endif
	"-DUNPACKER_IS_" stringify(UNPACKERNAME),
	/*arg,*/ filename, NULL };

  fork.fork(argv[0],argv,&lexer_read_fd,NULL);

  // read the information!

  parse_definitions();

  lexer_read_fd = -1;

  fork.wait(true);
}

void read_map_calib_info()
{
  // INFO("Reading calibration parameters...");

  all_mc_defs = new def_node_list;

  // First of all, we need to fire up the preprocessor which will chew
  // through the input files, and deliver them to...  Hmm, it actually
  // only eats one at a time...

  // we need to figure out where our executable came from... This is
  // probably not _the_ way to do it, but to get it running

  char *filename;

  filename = argv0_replace(GENDIR "/data_mapping.hh");
  read_map_calib_info_file(filename,true);
  free (filename);

  filename = argv0_replace("calibration.hh");
  read_map_calib_info_file(filename,false);
  free (filename);

  // process_map_calib_info("miffo.txt");

  config_calib_vect::iterator iter;

  for (iter = _conf_calib.begin(); iter != _conf_calib.end(); ++iter)
    {
      const char *file = *iter;

      read_map_calib_info_file(file,true);
    }

  // Dump the information that we have read...
}


void verify_units_match(const prefix_units_exponent *dest,
			const units_exponent *src,
			const file_line &loc,
			const char *descr,double &factor)
{
  bool match = match_units(dest,src,factor);

  if (!match)
    {
      char str_unit_dest[64];
      char str_unit_src[64];

      format_unit_str(src,str_unit_src,sizeof(str_unit_src));
      format_unit_str(dest,str_unit_dest,sizeof(str_unit_dest));

      ERROR_LOC(loc,
		"Unit mismatch: '%s' "
		"(unit '%s' of value does not match destination unit '%s').",
		descr,str_unit_src,str_unit_dest);
    }
}


void apply_map_item(const map_info *item)
{
  /*
  char str_src[64];
  char str_dest[64];

  item->_src->format(str_src,sizeof(str_src));
  item->_dest->format(str_dest,sizeof(str_dest));
  */
  // printf ("SIGNAL_MAPPING(%s,%s);\n",str_src,str_dest);

  // These should be, by design, as the items are not created
  // (enumerated) unless they are given pointers

  assert(item->_src->_set_dest);
  assert(item->_src->_addr);
  assert(item->_dest->_addr);

  if ((item->_src->_type & ENUM_TYPE_MASK) !=
      (item->_dest->_type & ENUM_TYPE_MASK))
    ERROR_LOC(item->_loc,"Mapping between incompatible types!");

  if (item->_dest->_type & ENUM_NO_INDEX_DEST)
    ERROR_LOC(item->_loc,
	      "Mapping destination is non-first index in unindexed array.  "
	      "(use (dummy)index 0/1)");

  // Ugly hack: we must discard the const qualifier on the src and
  // dest pointer

  if (!item->_src->_set_dest((void *) item->_src->_addr,
			     (void *) item->_dest->_addr,
			     item->_toggle_i /* applies to dest */))
    ERROR_LOC(item->_loc,"Mapping already specified for source item.");
}

void apply_calib_param(calib_param *item)
{
  assert(item->_src->_set_dest);
  assert(item->_src->_addr);
  assert(item->_dest->_addr);

  if (!item->_src->_set_dest(item,NULL,
			     item->_toggle_i /* applies to src */))
    ERROR_LOC(item->_loc,"Calib mapping already specified for source item.");
}

void apply_calib_param(user_calib_param *item)
{
  assert (!item->_src);
  assert (item->_dest->_addr);

  // SET_CALIB_PARAM(float);

  if (item->_param->size() != 1)
    ERROR_LOC(item->_loc,"Wrong number of parameters.");

  double factor;

  verify_units_match(item->_dest->_unit,
		     item->_param[0][0]._unit,
		     item->_loc,"param",factor);

  if (item->_dest->_type == ENUM_TYPE_FLOAT) {
    *((float *) item->_dest->_addr) =
      (float) (item->_param[0][0]._value * factor);
    return;
  }
  if (item->_dest->_type == ENUM_TYPE_DOUBLE) {
    *((double *) item->_dest->_addr) =
      item->_param[0][0]._value * factor;
    return;
  }

  ERROR_LOC(item->_loc,"Unhandled type of calibration parameter destination "
	    "(%d).",
	    item->_dest->_type);

  /*
  assert(item->_src->_set_dest);
  assert(item->_src->_addr);
  assert(item->_dest->_addr);

  if (!item->_src->_set_dest(item,NULL,0))
    ERROR_LOC(item->_loc,"Mapping already specified for source item.");
  */
}

void process_map_calib_info()
{
  def_node_list::iterator i;

  for (i = all_mc_defs->begin(); i != all_mc_defs->end(); ++i)
    {
      def_node *info = *i;

      calib_param *calib_item =
	dynamic_cast<calib_param *>(info);

      if (calib_item)
	{
	  apply_calib_param(calib_item);
	  continue; // not to also get caught as a map_item
	}

      user_calib_param *user_calib_item =
	dynamic_cast<user_calib_param *>(info);

      if (user_calib_item)
	{
	  apply_calib_param(user_calib_item);
	  continue; // not to also get caught as a map_item
	}

      // Has to be last as it as base class for the above
      map_info *map_item =
	dynamic_cast<map_info *>(info);

      if (map_item)
	{
	  apply_map_item(map_item);
	  continue;
	}

    }
}

extern int yylineno;

const signal_id_info *get_signal_id_info(signal_id *id,int map_no)
{
  const signal_id_info *info = get_signal_id_info(*id,map_no);

  if (!info)
    {
      char str[128];

      id->format(str,sizeof(str));

      const char *map_str = NULL;

      switch (map_no)
	{
	case SID_MAP_UNPACK: map_str = "UNPACK";  break;
	case SID_MAP_RAW:    map_str = "RAW";     break;
	case SID_MAP_CAL:    map_str = "CAL";     break;
	case SID_MAP_USER:   map_str = "USER";    break;
	case SID_MAP_CALIB:  map_str = "CALIB";   break;

	case SID_MAP_STICKY | SID_MAP_UNPACK:
	  map_str = "sticky UNPACK";  break;
	case SID_MAP_STICKY | SID_MAP_RAW:
	  map_str = "sticky RAW";     break;  

	case SID_MAP_MIRROR_MAP | SID_MAP_UNPACK:
	  map_str = "UNPACK (map mirror)";  break;
	case SID_MAP_MIRROR_MAP | SID_MAP_RAW:
	  map_str = "RAW (map mirror)";     break;
	case SID_MAP_MIRROR_MAP | SID_MAP_CAL:
	  map_str = "CAL (map mirror)";     break;
	case SID_MAP_MIRROR_MAP | SID_MAP_USER:
	  map_str = "USER (map mirror)";    break;
	case SID_MAP_MIRROR_MAP | SID_MAP_CALIB:
	  map_str = "CALIB (map mirror)";   break;

	case SID_MAP_STICKY | SID_MAP_MIRROR_MAP | SID_MAP_UNPACK:
	  map_str = "sticky UNPACK (map mirror)";  break;

	case SID_MAP_MIRROR_MAP | SID_MAP_RAW | SID_MAP_REVERSE:
	  map_str = "RAW (reverse map mirror)"; break;

	default:             map_str = "unknown"; break;
	}

      print_lineno(stderr,yylineno);
      ERROR("Cannot find signal %s in map %s",str,map_str);
    }

  return info;
}

