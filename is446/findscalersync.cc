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

// g++ -o findgood -O3 -Wall is446/findgood.cc


// for i in `ls /net/data1/is446/*.lmd | sed -e "s/.*\///" -e "s/.lmd//"` ; do echo $i ; is446/is446 /net/data1/is446/$i.lmd | grep "SC:" | gzip -c > /net/data1/htj/is446/$i.sc.gz ; done

// for i in `ls /net/data1/is446/*.lmd | sed -e "s/.*\///" -e "s/.lmd//"` ; do echo $i ; zcat /net/data1/htj/is446/$i.sc.gz | ./findscalersync | grep -v "//"  | gzip -c > /net/data1/htj/is446/$i.scsync.gz 2> /net/data1/htj/is446/$i.scsync.err ; cat /net/data1/htj/is446/$i.scsync.err ; done

#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <map>

struct ec_event
{
  int mbs_ec;
  int sca_ec;

  int sca0, sca1, sca2;

  int ebis;
  int clean;
};

typedef std::vector<ec_event> ecv;

ecv v;

typedef std::vector<int> int_v;

struct ebis_offsets
{
  int   mbs_ec;
  int_v offsets;

  int   best_off, num_best_off;
};

typedef std::vector<ebis_offsets> ebis_offsets_v;

typedef std::map<int,int> ii_map;

#define MAX_DESYNC 4

void agree_offset(ebis_offsets_v::iterator f,
		  ebis_offsets_v::iterator e)
{
  ebis_offsets_v::iterator i = f;

  ii_map offset_count;

  int n = 0;

  for ( ; ; ++i)
    {
      // printf ("(%d) ",i->mbs_ec);

      for (int_v::iterator j = i->offsets.begin();
	   j != i->offsets.end(); ++j)
	{
	  // printf ("[%d]",*j);

	  ii_map::iterator oci = offset_count.find(*j);

	  if (oci != offset_count.end())
	    (*oci).second++;
	  else
	    offset_count.insert(ii_map::value_type(*j,1));
	}

      n++;

      if (i == e)
	break;
    }

  int max_occur = 0;
  int max_off   = -1;

  for (ii_map::iterator i = offset_count.begin();
       i != offset_count.end(); ++i)
    {
      // printf ("[%d:%d]",i->first,i->second);

      if (i->second > max_occur)
	{
	  max_occur = i->second;
	  max_off   = i->first;
	}
      else if (i->second == max_occur)
	{
	  // two with same amount, no good!
	  max_off = -1;
	}
    }

  if (max_off != -1 && n >= 3 && max_occur >= 3 && max_occur >= n-1)
    {
      // printf ("[%d:%d/%d]",max_off,max_occur,n);

      i = f;

      for ( ; ; ++i)
	{
	  if (max_occur > i->num_best_off)
	    {
	      for (int_v::iterator j = i->offsets.begin();
		   j != i->offsets.end(); ++j)
		{
		  if (*j == max_off)
		    {
		      i->num_best_off = max_occur;
		      i->best_off     = max_off;
		      break;
		    }
		}
	    }

	  if (i == e)
	    break;
	}
    }

}


void do_sync (ecv::iterator f,ecv::iterator e)
{
  bool start0 = f->sca_ec == 0;

  (void) start0;

  int_v  clean_ec;
  ebis_offsets_v eov;

  printf ("//--------------------------------------------------\n");

  {
    ebis_offsets eo;

    eo.mbs_ec = f->mbs_ec;
    eo.best_off = -1;
    eo.num_best_off = -1;

    eov.push_back(eo);
  }

  for (ecv::iterator i = f; i != e; ++i)
    {
      ec_event &e = *i;

      if (e.clean)
	{
	  clean_ec.push_back(e.mbs_ec);

	  // printf ("C: %d\n",e.mbs_ec);
	}

      if (e.ebis && e.sca_ec != 0)
	{
	  ebis_offsets eo;

	  eo.mbs_ec = e.mbs_ec;
	  eo.best_off = -1;
	  eo.num_best_off = -1;

	  // printf ("E: %d :",e.mbs_ec);

	  int q = 0;

	  for (int_v::reverse_iterator j = clean_ec.rbegin();
	       j != clean_ec.rend() && q < MAX_DESYNC; ++j, q++)
	    {
	      eo.offsets.push_back(e.mbs_ec - *j);
	      // printf (" %d",e.mbs_ec - *j);
	    }
	  // printf ("\n");

	  eov.push_back(eo);
	}
    }

  {
    ebis_offsets eo;

    eo.mbs_ec = (e-1)->mbs_ec;
    eo.best_off = -1;
    eo.num_best_off = -1;

    eov.push_back(eo);
  }
  /*
  ebis_offsets_v::iterator eoi_p0 = eov.begin();
  ebis_offsets_v::iterator eoi_p1 = eov.begin();
  ebis_offsets_v::iterator eoi_p2 = eov.begin();
  ebis_offsets_v::iterator eoi_p3 = eov.begin();
  ebis_offsets_v::iterator eoi_p4 = eov.begin();
  */
  ebis_offsets_v::iterator eoi_pp[MAX_DESYNC];

  for (int i = 0; i < MAX_DESYNC; i++)
    eoi_pp[i] = eov.begin();

  for (ebis_offsets_v::iterator eoi = eov.begin();
       eoi != eov.end(); ++eoi)
    {
      // printf ("%d : ",eoi->mbs_ec);

      for (int i = MAX_DESYNC-1; i >= 1; i--)
	agree_offset(eoi_pp[i],eoi);

      /*
      agree_offset(eoi_p4,eoi);
      agree_offset(eoi_p3,eoi);
      agree_offset(eoi_p2,eoi);
      agree_offset(eoi_p1,eoi);
      */

      /*
      for (int_v::iterator j = eoi->offsets.begin();
	   j != eoi->offsets.end(); ++j)
	{
	  printf (" %d",*j);
	}
      */

      for (int i = MAX_DESYNC-2; i >= 0; i--)
	eoi_pp[i+1] = eoi_pp[i];

      eoi_pp[0] = eoi;
      /*
      eoi_p4 = eoi_p3;
      eoi_p3 = eoi_p2;
      eoi_p2 = eoi_p1;
      eoi_p1 = eoi_p0;
      eoi_p0 = eoi;
      */
	//printf ("\n");
    }

  // 22997235

  bool removed;
  do
    {
      removed = false;

      ebis_offsets_v::iterator eoi1 = eov.begin();

      for ( ; eoi1 != eov.end(); ++eoi1)
	{
	  ebis_offsets_v::iterator eoi3 = eoi1;

	  for (++eoi3; eoi3 != eov.end(); ++eoi3)
	    {
	      if (eoi3->best_off != -1)
		break;
	    }

	  if (eoi3 != eov.end() &&
	      eoi1->best_off != -1 &&
	      eoi3->best_off != -1 &&
	      eoi1->best_off > eoi3->best_off)
	    {
	      eoi1->best_off = -1;
	      eoi3->best_off = -1;
	      removed = true;
	      printf ("//r: %d\n",eoi1->mbs_ec);
	    }
	}
    }
  while (removed);

  ebis_offsets_v::iterator prev_eoi = eov.end();

  for (ebis_offsets_v::iterator eoi = eov.begin();
       eoi != eov.end(); ++eoi)
    {
      if (eoi->best_off == -1)
	{
	  // this has no assignment, find previous and next with
	  // assignment (if any)

	  ebis_offsets_v::iterator next_eoi = eoi;

	  for (++next_eoi; next_eoi != eov.end(); ++next_eoi)
	    {
	      if (next_eoi->best_off != -1)
		break;
	    }

	  // if previous and next exist, and they agree on assignment
	  // then accept it

	  if (prev_eoi != eov.end() &&
	      next_eoi != eov.end() &&
	      prev_eoi->best_off == next_eoi->best_off)
	    {
	      eoi->best_off     = prev_eoi->best_off;
	      eoi->num_best_off = 2;
	      continue;
	    }

	  // if we searching backwards, do not find the forward
	  // offest, then we accept the backward offset, similarly,
	  // searching forwards, if we do not find the backwards
	  // offset, we accept it

	  if (prev_eoi == eov.end())
	    {
	      // none before, accept coming, if confirmed within three
	      // steps

	      if (next_eoi == eov.end())
		continue;

	      ebis_offsets_v::iterator eoj = eoi;

	      for (int q = 0; q < 4; ++eoj, q++)
		{
		  for (int_v::iterator j = eoj->offsets.begin();
		       j != eoj->offsets.end(); ++j)
		    if (*j == next_eoi->best_off)
		      goto accept_single_forward;
		  if (eoj == next_eoi)
		    break;
		}
	      continue;
	    accept_single_forward:
	      eoi->best_off     = next_eoi->best_off;
	      eoi->num_best_off = 1;
	      continue;
	    }

	  if (next_eoi == eov.end())
	    {
	      // none after, accept previous, if confirmed within three
	      // steps

	      ebis_offsets_v::iterator eoj = eoi;

	      for (int q = 0; q < 4; --eoj, q++)
		{
		  for (int_v::iterator j = eoj->offsets.begin();
		       j != eoj->offsets.end(); ++j)
		    if (*j == prev_eoi->best_off)
		      goto accept_single_backward;
		  if (eoj == prev_eoi)
		    break;
		}
	      continue;
	    accept_single_backward:
	      eoi->best_off     = prev_eoi->best_off;
	      eoi->num_best_off = 1;
	      continue;
	      ;
	    }

	  // 22937805 22961131 22991558

	  // so we have both a previous and a next, and they are not
	  // the same.  can be before (after) us find a copy of the
	  // good offset known from after (before us), without and
	  // mentioning of the good ofter known before (after) us?

	  ebis_offsets_v::iterator eoj;

	  for (eoj = eoi; eoj != prev_eoi; --eoj)
	    {
	      for (int_v::iterator j = eoj->offsets.begin();
		   j != eoj->offsets.end(); ++j)
		if (*j == next_eoi->best_off)
		  {
		    // then, between eoj and next_eoi we may not
		    // find prev_eoi->best_off

		    for (++eoj; eoj != next_eoi; ++eoj)
		      {
			for (j = eoj->offsets.begin();
			     j != eoj->offsets.end(); ++j)
			  if (*j == prev_eoi->best_off)
			    goto fail_previous;
		      }
		    eoi->best_off     = next_eoi->best_off;
		    eoi->num_best_off = 0;
		    goto done;
		  }
	    }

	fail_previous:
	  for (eoj = eoi; eoj != next_eoi; ++eoj)
	    {
	      for (int_v::iterator j = eoj->offsets.begin();
		   j != eoj->offsets.end(); ++j)
		if (*j == prev_eoi->best_off)
		  {
		    // then, between eoj and next_eoi we may not
		    // find prev_eoi->best_off

		    for (--eoj; eoj != prev_eoi; --eoj)
		      {
			for (j = eoj->offsets.begin();
			     j != eoj->offsets.end(); ++j)
			  if (*j == next_eoi->best_off)
			    goto fail_next;
		      }
		    eoi->best_off     = prev_eoi->best_off;
		    eoi->num_best_off = 0;
		    goto done;
		  }
	    }
	fail_next:
	done:
	  ;



	  /*
	  ebis_offsets_v::iterator eoj = eoi;

	  for ( ; eoj != prev_eoi; --eoj)
	    {
	      for (int_v::iterator j = eoj->offsets.begin();
		   j != eoj->offsets.end(); ++j)
		if (*j == next_eoi->best_off)
		  goto fail_backward;
	    }

	  eoi->best_off     = prev_eoi->best_off;
	  eoi->num_best_off = 1;
	fail_backward:
	  ;

	  eoj = eoi;

	  for ( ; eoj != next_eoi; ++eoj)
	    {
	      for (int_v::iterator j = eoj->offsets.begin();
		       j != eoj->offsets.end(); ++j)
		if (*j == prev_eoi->best_off)
		  goto fail_forward;
	    }

	  eoi->best_off     = next_eoi->best_off;
	  eoi->num_best_off = 1;

	fail_forward:
	  ;
	  */
	}
      else
	prev_eoi = eoi;
    }

  /////////////////////////////////////
  //
  // Then, go through the data and calculate the TSHORT and TEBIS
  // for each trigger

  for (ebis_offsets_v::iterator eoi = eov.begin();
       eoi != eov.end(); ++eoi)
    {
      printf ("// %d : %2d/%2d :",
	      eoi->mbs_ec,eoi->best_off,eoi->num_best_off);

      for (int_v::iterator j = eoi->offsets.begin();
	   j != eoi->offsets.end(); ++j)
	{
	  printf (" %d",*j);
	}
      printf ("\n");
    }

  //

  ebis_offsets_v::iterator eoi_prev = eov.begin();
  ebis_offsets_v::iterator eoi = eoi_prev;

  ecv::iterator i_sca = f;

  int ebis_reference = -1;

  for (++eoi; eoi != eov.end(); ++eoi)
    {
      /*
      printf ("%d : %2d/%2d :",
	      eoi->mbs_ec,eoi->best_off,eoi->num_best_off);

      for (int_v::iterator j = eoi->offsets.begin();
	   j != eoi->offsets.end(); ++j)
	{
	  printf (" %d",*j);
	}
      printf ("\n");
      */

      if (eoi_prev->best_off != -1 &&
	  eoi_prev->best_off == eoi->best_off)
	{
	  // previous and this agree about offset

	  // first skip any events that may be too early

	  for ( ; i_sca != e && i_sca->mbs_ec < eoi_prev->mbs_ec; ++i_sca)
	    ;

	  // then handle our events, up including the last

	  for ( ; i_sca != e && i_sca->mbs_ec <= eoi->mbs_ec; ++i_sca)
	    {
	      printf ("%d ",
		      i_sca->mbs_ec-eoi_prev->best_off);

	      if (i_sca->ebis)
		ebis_reference = i_sca->sca0;

	      int tshort = i_sca->sca0;
	      int tebis  = i_sca->sca0 - ebis_reference;

	      if (ebis_reference == -1)
		tebis = -500;

	      printf ("%8d %6d",tshort,tebis);

	      printf ("\n");
	    }

	}
      else
	ebis_reference = -1;

      eoi_prev = eoi;
    }

  fprintf (stderr,"%08d-%08d : N%6d  E%6d  C%6d  %5.1f\n",
	   f->mbs_ec,(e-1)->mbs_ec,
	   (int)distance(f,e),(int)eov.size(),(int)clean_ec.size(),
	   (double)distance(f,e)/eov.size());

}



int main()
{

  while (!feof(stdin))
    {
      int n;
      ec_event e;

      char str_ebis[64];
      char str_nacc[64];
      char str_clr[64];

      memset(&e,0,sizeof(e));

      n = fscanf(stdin,
		 "SC:  %d %d %d %d %d %s %s %s\n",
		 &e.mbs_ec,
		 &e.sca0,&e.sca1,&e.sca2,
		 &e.sca_ec,
		 str_ebis,str_nacc,str_clr);

      if (str_ebis[0] == 'E')
	e.ebis = 1;

      if (str_clr[0] == 'C')
	e.clean = 1;

      if (n != 8)
	{
	  printf ("Cannot convert... (got %d)!",n);
	  exit(1);
	}

      v.push_back(e);
    }

  // go through the data, each time scaler is reset, we know we can
  // restart, so treat the data in that way...

  ecv::iterator i;

  ecv::iterator i_reset = v.begin();

  for (i = v.begin(); i != v.end(); ++i)
    {
      if (i->sca_ec == 0)
	{
	  do_sync(i_reset,i);

	  i_reset = i;
	}
    }

  do_sync(i_reset,v.end());

  for (unsigned int i = 0; i < v.size(); i++)
    {
      ec_event &e = v[i];

      printf ("//%8d %5d %d %d : \n",e.mbs_ec,e.sca_ec,e.ebis,e.clean);
    }

  return 0;
}
