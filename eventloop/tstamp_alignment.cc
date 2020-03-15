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

#include "tstamp_alignment.hh"
#include "colourtext.hh"

#ifdef USE_LMD_INPUT

void tstamp_alignment::init()
{

}

ts_a_hist::ts_a_hist()
{
  clear();
}

void ts_a_hist::clear()
{
  memset(_bins, 0, sizeof (_bins));
}


void ts_a_hist::add(int i,uint64_t val)
{
  if (val < (((uint64_t) 1) << 32))
    {
      unsigned int bin = ilog2((unsigned int) val);

      _bins[i][bin]++;
    }
}

void tstamp_alignment_histo::init_clear()
{
  _prev_stamp = 0;
}

void tstamp_alignment_histo::add_histos(size_t n)
{
  _hists.resize(n);
}


size_t tstamp_alignment::get_index(const lmd_subevent *subevent_info,
				   uint titris_branch_id)
{
  titris_subevent_ident ident;

  ident._info._titris_id = titris_branch_id;
  ident._info._subev = subevent_info->_header;

  set_titris_subevent_index::iterator iter = _map_index.find(ident);

  if (iter == _map_index.end())
    {
      ident._info._subev._header.l_dlen = 0; // insert something nice

      size_t index = _vect_histo.size();

      tstamp_alignment_histo *histo = new tstamp_alignment_histo;

      _vect_histo.push_back(histo);

      histo->init_clear();

      _map_index.insert(set_titris_subevent_index::value_type(ident,index));

      // All branch/id need a new histogram...

      for (vect_tstamp_alignment_histo::iterator i = _vect_histo.begin();
	   i != _vect_histo.end(); ++i)
	{
	  (*i)->add_histos(index+1);
	}

      // printf ("%d\n",index);

      return index;
    }
  else
    return iter->second;
}

void tstamp_alignment::account(size_t index, uint64_t stamp)
{
  if (index == (size_t) -1)
    return;

  // printf ("%d %zd\n",index, _vect_histo.size());

  // We have just ejected an event for branch/id @index
  // at timestamp @stamp

  // So, for every other known branch/id, we record how long
  // it was since those were seen

  // And, to get the data in the other direction, for every
  // other known branch/id, we record how long the time
  // is to us...  But only if we have not occured already
  // after they were seen the last time...

  size_t ids = _vect_histo.size();

  uint64_t prev_us = _vect_histo[index]->_prev_stamp;

  for (size_t other = 0; other < ids; other++)
    if (other != (size_t) index)
      {
	uint64_t prev_other = _vect_histo[other]->_prev_stamp;
	uint64_t time_since_other = stamp - prev_other;

	if (stamp > prev_other && prev_other != 0)
	  {
	    _vect_histo[other]->_hists[index].add(0, time_since_other);

	    if (prev_other > prev_us)
	      {
		_vect_histo[index]->_hists[other].add(1, time_since_other);
	      }
	  }
      }

  _vect_histo[index]->_prev_stamp = stamp;
}

void tstamp_alignment::show()
{
  // Loop over the branch/id and show what we have

  for (set_titris_subevent_index::iterator iter_i = _map_index.begin();
       iter_i != _map_index.end(); ++iter_i)
    {
      for (set_titris_subevent_index::iterator iter_j = _map_index.begin();
	   iter_j != _map_index.end(); ++iter_j)
	{
	  const titris_subevent_ident &ident_j = iter_j->first;

	  printf ("%s0x%04x%s %s%5d%s/%s%5d%s %s%5d%s:%s%3d%s:%s%3d%s  ",
		  CT_OUT(BOLD),ident_j._info._titris_id,CT_OUT(NORM),
		  CT_OUT(BOLD),ident_j._info._subev._header.i_type,CT_OUT(NORM),
		  CT_OUT(BOLD),ident_j._info._subev._header.i_subtype,CT_OUT(NORM),
		  CT_OUT(BOLD),ident_j._info._subev.i_procid,CT_OUT(NORM),
		  CT_OUT(BOLD),ident_j._info._subev.h_subcrate,CT_OUT(NORM),
		  CT_OUT(BOLD),ident_j._info._subev.h_control,CT_OUT(NORM));

	  if (iter_j == iter_i)
	    {
	      printf ("%*s%s*%s\n",
		      16, "",
		      CT_OUT(BOLD_MAGENTA), CT_OUT(NORM));
	    }
	  else
	    {
	      ts_a_hist &hist =
		_vect_histo[iter_j->second]->_hists[iter_i->second];

	      for (int k = 0; k < 32; k += 2)
		{
		  uint sum =
		    hist._bins[0][31-k] + hist._bins[0][31-(k+1)];
		  uint scaled = sum / (1 << (31-k));
		  if (!scaled && sum)
		    scaled = 1;

		  printf ("%c", hexilog2b1(scaled));
		}

	      printf ("-");

	      for (int k = 0; k < 32; k += 2)
		{
		  uint sum =
		    hist._bins[1][k] + hist._bins[1][(k+1)];
		  uint scaled = sum / (1 << (31-k));
		  if (!scaled && sum)
		    scaled = 1;

		  printf ("%c", hexilog2b1(scaled));
		}
	      printf ("\n");
	    }
	  /*
	    printf ("%d %d %d\n",ilog2(0),ilog2(1),ilog2(2));
	    printf ("%d %d %d\n",ilog2(31),ilog2(32),ilog2(33));
	  */
	}
      printf ("\n");
    }
}

#endif//USE_LMD_INPUT
