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

#include "signal_errors.hh"

#include <stdio.h>

#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#if defined(__GNUC__) && !defined(__CYGWIN__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(__svr4__)
#define HAS_EXECINFO
#include <execinfo.h>
#endif//__CYGWIN__

#ifdef __GNUC_PREREQ
#if __GNUC_PREREQ (3, 2)
#include <cxxabi.h>
#endif
#endif

////////////////////////////////////////////////////////////////////////

#ifndef countof
#define countof(array) (sizeof(array)/sizeof((array)[0]))
#endif

void
print_back_trace()
{
#ifndef HAS_EXECINFO
  fprintf (stderr,"Traceback not available for cygwin, and others.\n\n");
#else
  void *array[32];
  int size;

  char **traces;

  size = backtrace(array,(int) countof(array));
  traces = backtrace_symbols(array,size);

  fprintf (stderr,"\n");

  for (int i = 0; i < size; i++)
    {
      // try to demangle the name if possible

      // the output is on format path(fncname+0xhexoffset) [0xhexaddr]
      // so try simply to find last ( (since filename but no other part
      // could contain a '('), and then ending +, if so, try demangling,
      // otherwise just output

      char* first;
      char* last;

      if ((first = strrchr(traces[i],'(')) != NULL &&
          (last  = strchr(first,'+')))
        {
          // we have a name to demangle at first+1, until
          // last

          *first = '\0';
          *last = '\0';

          int status = -1;
          char *dem = 0;

#ifdef __GNUC_PREREQ
#if __GNUC_PREREQ (3, 2)
          if (strcmp(first+1,"main") != 0) // do not demangle main (becomes unsigned long ...?)
            dem = abi::__cxa_demangle(first+1, 0, 0, &status);
#endif
#endif

          fprintf (stderr,
                   "%s(%s+%s\n",
                   traces[i],
                   status == 0 ? dem : first+1,
                   last+1);

#ifdef __GNUC_PREREQ
#if __GNUC_PREREQ (3, 2)
          if (status == 0)
            free(dem);
#endif
#endif
        }
      else
        fprintf (stderr,"%s\n",traces[i]);
    }

  fprintf (stderr,"\n");

  free (traces);
#endif//__CYGWIN__
}

////////////////////////////////////////////////////////////////////////

const char* _cmdname;

extern "C" void handle_sigsegv(int signum)
{
  // reset signal handler so that next time segv happens (which it
  // will when we return), it does it's normal job (core dump)

  signal(signum,SIG_DFL);

  // Tell user what happened, and give context traceback
  // Also give some info about interpreting the core dump
  // with a debugger...

  fprintf (stderr,
           "\n\n"
          "*************************************\n"
          "***   Internal fault, got SIGNAL  ***\n"
          "*** The program has (1+) bugs :-( ***\n"
          "*\n");

  // and generate a back trace of what happened

  fprintf (stderr,
           "* Traceback of function calls leading to the disaster\n"
	   "* (usually the three first lines are of no interest,\n"
	   "* a more accurate traceback is given by a debugger):\n\n");

  print_back_trace();

  fprintf (stderr,
           "\n");
  fprintf (stderr,
           "* If everything is setup nicely, you will get a core dump,\n"
	   "* named 'core' in the current directory.\n"
	   "* With a debugger (e.g. gdb or ddd), you could figure out\n"
	   "* what went wrong (e.g. what line & function failed, if\n"
	   "* the program was compiled for debugging (-g)):\n"
	   "*\n"
	   "* Just issue: gdb %s core\n"
	   "* Or visual:  ddd %s core\n"
	   "* And then:   backtrace\n"
	   "*\n"
	   "* Good luck! Have a nice day...\n",_cmdname,_cmdname);
}

void setup_segfault_coredump(const char* cmdname)
{
  _cmdname = cmdname;

  // Use our signal handler

  signal(SIGSEGV,handle_sigsegv);
  /*
  signal(SIGBUS ,handle_sigsegv);
  signal(SIGILL ,handle_sigsegv);
  signal(SIGABRT,handle_sigsegv);
  */

  // enable core dumps (if not already allowed)
  /*
  rlimit rl;

  getrlimit(RLIMIT_CORE, &rl);
  // fprintf (stderr,"%d %d\n",(int) rl.rlim_cur,(int) rl.rlim_max);
  if (rl.rlim_cur <= 0 || rl.rlim_max <= 0)
    {
      // allow core files to be created
      rl.rlim_cur = rl.rlim_max = 200000000; // 200 megs core size max...
      setrlimit(RLIMIT_CORE, &rl);
    }
  // getrlimit(RLIMIT_CORE, &rl);
  // fprintf (stderr,"%d %d\n",(int) rl.rlim_cur,(int) rl.rlim_max);
  */
}
