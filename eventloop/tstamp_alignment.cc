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
#include "event_loop.hh"
#include "colourtext.hh"

#ifdef USE_LMD_INPUT

ts_a_hist::ts_a_hist()
{
  _bins[1] = _bins[0] = NULL;
}

void ts_a_hist::add(int i,uint64_t val)
{
  uint bin = (uint) -1;
  
  if (val < (((uint64_t) 1) << 32))
    bin = _is_lin ? (uint) (_bin_num * val / _range) : ilog2((uint) val);
  if (bin != (uint) -1 && bin < _bin_num)
    _bins[i][bin]++;
}

void ts_a_hist::clear()
{
  for (size_t i = 0; i < 2; ++i)
    memset(_bins[i], 0, _bin_num * sizeof *_bins);
}

void ts_a_hist::init(int is_lin, uint range, size_t bin_num)
{
  _is_lin = is_lin;
  _range = range;
  _bin_num = bin_num;
  for (size_t i = 0; i < 2; ++i)
    {
      free(_bins[i]);
      _bins[i] = (uint *)calloc(_bin_num, sizeof *_bins[i]);
    }
}


void tstamp_alignment_histo::init_clear()
{
  _prev_stamp = 0;
}

void tstamp_alignment_histo::add_histos(size_t n, int is_lin, uint range,
    size_t bin_num)
{
  size_t n_old = _hists.size();
  _hists.resize(n);
  for (size_t i = n_old; i < n; ++i)
    _hists[i].init(is_lin, range, bin_num);
}


tstamp_alignment::tstamp_alignment(char const *a_command, int a_merge_style):
  _style(a_merge_style),
  _is_lin(0),
  _range(1000000),
  _bin_num(32)
{
  const char *cmd = a_command;
  for (; NULL != cmd;)
    {
      char const *req_end;
      for (req_end = cmd; *req_end != ',' && *req_end; req_end++);
      if (req_end == cmd)
        break;
      char *request = strndup(cmd,req_end-cmd);
      if (strcmp(request, "help") == 0) {
        free(request);
        usage();
        exit(EXIT_SUCCESS);
      }
      char const *p = request;
      int mode;
      if ((mode = get_time_stamp_mode(request)) != -1)
        {
          _style = mode;
          goto parse_time_stamp_hist_options_next;
        }
      if (strcmp(request, "lin") == 0)
        {
          _is_lin = 1;
          goto parse_time_stamp_hist_options_next;
        }
      if (strcmp(request, "log") == 0)
        {
          _is_lin = 0;
          goto parse_time_stamp_hist_options_next;
        }

#define TIME_HIST_COMPONENT(dst, src)					\
      do {								\
        char const *opt = #src"=";					\
        size_t optlen = strlen(opt);					\
        if (strncmp(p, opt, optlen) == 0) {				\
          p += optlen;							\
          char *end;							\
          int value = (int)strtod(p, &end);				\
          if (p == end || end[0] != '\0') {				\
            ERROR("Invalid number for \""#src"\" in "			\
		  "time stamp histogram option \"%s\".", a_command);	\
	  }								\
	  dst = value;							\
	  goto parse_time_stamp_hist_options_next;			\
	}								\
      } while (0)

      TIME_HIST_COMPONENT(_range, range);
      TIME_HIST_COMPONENT(_bin_num, bins);
      ERROR("Unknown time stamp histogram option \"%s\", in \"%s\".",
	    request,a_command);
parse_time_stamp_hist_options_next:
      free(request);
      cmd = req_end + (*req_end == ',');
    }
  if (_style <= 0)
    ERROR("No time stamp histogram style in \"%s\" or by merge style %d.",
	  a_command,a_merge_style);
}

ssize_t tstamp_alignment::get_index(const lmd_subevent *subevent_info,
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
	  (*i)->add_histos(index+1, _is_lin, _range, _bin_num);
	}

      // printf ("%d\n",index);

      return index;
    }
  else
    return iter->second;
}

void tstamp_alignment::account(ssize_t index, uint64_t stamp)
{
  if (index == -1)
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

int tstamp_alignment::get_style() const
{
  return _style;
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
              if (_is_lin)
                printf ("%*s%s*%s\n",
                        (int)_bin_num, "",
                        CT_OUT(BOLD_MAGENTA), CT_OUT(NORM));
              else
                printf ("%*s%s*%s\n",
                        16, "",
                        CT_OUT(BOLD_MAGENTA), CT_OUT(NORM));
	    }
	  else
	    {
	      ts_a_hist &hist =
		_vect_histo[iter_j->second]->_hists[iter_i->second];

              if (_is_lin)
                {
                  uint max = 0;
                  for (size_t k = 0; k < _bin_num; ++k)
                    for (size_t l = 0; l < 2; ++l)
                      max = std::max(max, hist._bins[l][k]);
                  for (size_t k = 0; k < _bin_num; ++k)
                    {
                      uint val = hist._bins[0][_bin_num - k - 1];
                      printf ("%c", hexilog2b1(val));
                    }
                  printf ("-");
                  for (size_t k = 0; k < _bin_num; ++k)
                    {
                      uint val = hist._bins[1][k];
                      printf ("%c", hexilog2b1(val));
                    }
                }
              else
                {
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

void tstamp_alignment::usage()
{
  printf ("\n");
  printf ("%s --tstamp-hist=[help],[props]\n", main_argv0);
  printf ("\n");
  printf ("Fill histograms between every pair of timestamped systems.\n");
  printf ("\n");
  printf (" [props] is a comma-separated combination of the following properties for the\n");
  printf (" histograms:\n");
  printf ("  wr|titris  - timestamp format.\n");
  printf ("  log        - logarithmic time scale, ignores properties below.\n");
  printf ("  lin        - linear time scale.\n");
  printf ("  range=num  - maximum negative and positive extent, default=1e6.\n");
  printf ("  bins=num   - number of bins on one side of 0, default=16 means 2x16+1 columns.\n");
  printf ("\n");
  printf ("Examples:\n");
  printf (" --tstamp-hist=log,wr\n");
  printf (" --tstamp-hist=lin,wr,range=1e3,bins=40\n");
  printf ("\n");
}

#endif//USE_LMD_INPUT
