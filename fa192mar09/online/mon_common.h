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

#ifndef __CINT__
#include "../../hbook/ext_data_clnt_stderr.hh"
#include "TCanvas.h"
#include "TStyle.h"
#include "TH1.h"
#include "TH2.h"
#endif

#include <vector>
#include <unistd.h>

void mon_common_setup()
{
#ifdef __CINT__
  gSystem->Load("../../hbook/ext_data_clnt.so");
#endif
}

#ifndef countof
#define countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

typedef std::vector<TH1*>         vect_TH1_ptr;
typedef std::vector<TVirtualPad*> vect_TVirtualPad_ptr;
typedef std::vector<TCanvas*>     vect_TCanvas_ptr;

vect_TH1_ptr         _hists;
vect_TVirtualPad_ptr _pads;
vect_TCanvas_ptr     _canvases;

#define NEW_CANVAS(var,name)             \
  TCanvas *var = new TCanvas(name,name); \
  _canvases.push_back(var);

#define PAD_CD_ADD(parent,no) { _pads.push_back(parent->cd(no)); }

const int trig_line_color[4] = { 3, 6, 5, 2};

#define TEMP_FMT_STR(tmpstr,fmt,...) \
  char tmpstr[64]; sprintf(tmpstr,fmt,__VA_ARGS__);

// create in reverse order to get physics drawn on top of calibrations
#define TH2I_TRIG_XIND(var,name,nind,mulind,nbiny,miny,maxy)	           \
  /* TH2I *var[4]; */                                                      \
  for (int __i = 4-1; __i >= 0; __i--) {                                   \
    char __tmp[64]; sprintf(__tmp,__i==3?"%s":"%s__%d",name,__i);          \
    var[__i] = new TH2I(__tmp,__tmp,mulind*nind,1,nind+1,nbiny,miny,maxy); \
    var[__i]->SetMarkerColor(trig_line_color[__i]);                        \
    /*var[__i]->SetMarkerStyle(7);  */                                         \
    var[__i]->Draw(__i != 3 ? "same" : "");			           \
    _hists.push_back(var[__i]);                                            \
  }

#define TH2I_TRIG_YIND(var,name,nind,mulind,nbinx,minx,maxx)	           \
  /* TH2I *var[4]; */                                                      \
  for (int __i = 4-1; __i >= 0; __i--) {                                   \
    char __tmp[64]; sprintf(__tmp,__i==3?"%s":"%s__%d",name,__i);          \
    var[__i] = new TH2I(__tmp,__tmp,nbinx,minx,maxx,mulind*nind,1,nind+1); \
    var[__i]->SetMarkerColor(trig_line_color[__i]);                        \
    /*var[__i]->SetMarkerStyle(7);   */                                        \
    var[__i]->Draw(__i != 3 ? "same" : "");			           \
    _hists.push_back(var[__i]);                                            \
  }

#define TH2I_TRIG(var,name,nbinx,minx,maxx,nbiny,miny,maxy)	       \
  /* TH2I *var[4]; */                                                  \
  for (int __i = 4-1; __i >= 0; __i--) {                               \
    char __tmp[64]; sprintf(__tmp,__i==3?"%s":"%s__%d",name,__i);      \
    var[__i] = new TH2I(__tmp,__tmp,nbinx,minx,maxx,nbiny,miny,maxy);  \
    var[__i]->SetMarkerColor(trig_line_color[__i]);                    \
    /*var[__i]->SetMarkerStyle(7);        */                               \
    var[__i]->Draw(__i != 3 ? "same" : "");			       \
    _hists.push_back(var[__i]);                                        \
  }

#define TH1I_TRIG(var,name,nbinx,minx,maxx)	                       \
  /* TH1I *var[4]; */                                                  \
  for (int __i = 4-1; __i >= 0; __i--) {                               \
    char __tmp[64]; sprintf(__tmp,__i==3?"%s":"%s__%d",name,__i);      \
    var[__i] = new TH1I(__tmp,__tmp,nbinx,minx,maxx);                  \
    var[__i]->SetLineColor(trig_line_color[__i]);                      \
    /*var[__i]->SetLineWidth(2);  */                                       \
    var[__i]->Draw(__i != 3 ? "same" : "");			       \
    _hists.push_back(var[__i]);                                        \
  }

int mon_tpat_histo(int tpat)
{
  if (tpat & 0x00ff) return 0; // physics
  if (tpat & 0x0f00) return 1; // offspill
  if (tpat & 0x2000) return 2; // tcal
  return 3;                    // others (0x1000 = clock, last BOS, EOS)
}

void mon_reset_hists()
{
  for (vect_TH1_ptr::iterator iter = _hists.begin();
       iter != _hists.end(); ++iter)
    (*iter)->Reset();
}

void mon_pads_modified()
{
  for (vect_TVirtualPad_ptr::iterator iter = _pads.begin();
       iter != _pads.end(); ++iter)
    (*iter)->Modified();
}

void mon_canvas_update()
{
  for (vect_TCanvas_ptr::iterator iter = _canvases.begin();
       iter != _canvases.end(); ++iter)
    {
      (*iter)->Modified();
      (*iter)->Update();
    }
}

void mon_pads_modified_update_reset_hists()
{
  mon_pads_modified();
  mon_canvas_update();
  mon_reset_hists();
}

void mon_pads_modified_update()
{
  mon_pads_modified();
  mon_canvas_update();
}

#define MON_SETUP            \
  mon_common_setup();        \
  ext_data_clnt_stderr conn; \
  uint32_t events = 0, last_update_events = 0; \
  uint32_t last_update_event;


#define MON_EVENT_LOOP_TOP(port,event_struct,event_struct_layout,event_struct_layout_init,event_var) \
  for ( ; ; ) {                                                 \
    if (conn.connect(HOSTNAME,port)) {                          \
      event_struct event_var;                                   \
      event_struct_layout event_layout = event_struct_layout_init; \
      if (conn.setup(&event_layout,sizeof(event_layout),        \
                     sizeof(event_var))) {                      \
        last_update_event = 0;                                  \
        for ( ; ; events++) {                                   \
	  conn.rand_fill(&event_var,sizeof(event_var));         \
	  if (!conn.fetch_event(&event_var,sizeof(event_var)))  \
	    break;

#define MON_EVENT_LOOP_UPDATE(event_var,trig_update,reset)    \
   /*Do the update on EOS */                                  \
 if (event_var.TRIGGER == /*7*/trig_update) {                 \
   static int countdown = 5;                                  \
   if (--countdown <= 0) {                                    \
     countdown = 5;                                           \
     if (reset) mon_pads_modified_update_reset_hists();       \
     else mon_pads_modified_update();                         \
     printf("Events: %d shown (this update: %d/%d, %.0f%% seen).     \r", \
          events,events - last_update_events,                 \
          event_var.EVENTNO - last_update_event,              \
          !last_update_event ? 0.0 :                          \
          100.0 * (events - last_update_events) /             \
          (event_var.EVENTNO - last_update_event));           \
     fflush(stdout);                                            \
     last_update_events = events;                               \
     last_update_event = event_var.EVENTNO;                     \
   }                                                          \
 }

#define MON_EVENT_LOOP_BOTTOM(event_var)                      \
        /*gSystem->ProcessEvents();                           */\
        }                                                     \
      }                                                       \
      conn.close();                                           \
    }                                                         \
    sleep(5); /* Wait with reconnect, to not flood server. */ \
  }


