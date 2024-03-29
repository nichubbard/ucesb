# This file is part of UCESB - a tool for data unpacking and processing.
#
# Copyright (C) 2018  Haakan T. Johansson  <f96hajo@chalmers.se>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301  USA

ifndef QUIET
QUIET=@
endif

GENDIR=gen

OBJS    = empty_file.o

ALL_OBJS = $(OBJS) $(GENOBJS)

AUTO_DEPS = $(ALL_OBJS:%.o=%.d)

CXXLINK = $(CXX)

CXXLINKFLAGS += -rdynamic # for backtrace

CXXFLAGS += -g -I. -I../common # so autogenerated files find includes
CXXFLAGS += # -ansi , troubles with strdup and fileno
CXXFLAGS += -O3
CXXFLAGS += -DUCESB_SRC
CXXFLAGS += -I$(UCESB_BASE_DIR)/file_input \
	  -I$(UCESB_BASE_DIR)/file_input/lmd_struct \
	  -I$(UCESB_BASE_DIR)/eventloop \
	  -I$(UCESB_BASE_DIR)/acc_def \

CXXFLAGS += -ansi -Wall -W -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wwrite-strings -Werror -Wcast-qual # -Wconversion -Wshadow

UNAME_S:=$(shell uname -s)

ifneq ($(UNAME_S),Darwin)
  LIB_LRT:=-lrt
else
  LIB_LRT:=
endif

#########################################

include ../makefile_deps.inc
include ../makefile_filter_warnings.inc
include ../makefile_acc_def.inc
include ../makefile_gendir.inc

#####################################################################

# Default target

all: empty_file

# Cleanup

clean:
	rm -rf $(GENDIR)
	rm -f $(ALL_OBJS)
	rm -f $(AUTO_DEPS)
	rm -f empty_file.base.dep
	rm -f empty_file.dep
	rm -f empty_file

# Create the autogenerated files

%.o: %.cc %.d
	@echo "   CXX   $@"
	@$(CXX) $(CXXFLAGS) $(OPTFLAGS) -o $@ -c $<

%.d: %.cc
	@echo "  DEPS   $@"
	@$(CXX) $(CXXFLAGS) -MM -MG $< | \
	  sed -e 's,\($(*F)\)\.o[ :]*,$*.o $@ : ,g' \
	> $@

empty_file.dep: $(AUTO_DEPS)
	@echo " FULLDEP $@"
	@echo "$@ $(MAKECMDGOALS): \\" > $@
	@cat $(AUTO_DEPS) | sed -e 's,^.*:,,g' |\
          sed -e 's,^#.*,,g' |\
          sed -e 's,^.*:,,g' |\
	  $(SPACE_TO_NEWLINE) |\
	  sed -e 's,^,  ,g' |\
	  sed -e 's,$$,\\,g' |\
	  sed -e 's,\\\\$$,\\,g' | sort -u >> $@

#

empty_file.base.dep: empty_file.dep
	@echo " BASEDEP $@"
	@echo '$$(UCESB_BASE_DIR)/file_input/$(MAKECMDGOALS): \' > $@
	@cat $< | grep -v ' \\' | sed -e 's,  /, /,' | \
	  sed -e 's,  ,  $$\(UCESB_BASE_DIR\)/file_input/,g' | sort -u >> $@

#
# Link

empty_file: $(ALL_OBJS) empty_file.dep empty_file.base.dep
	@echo "  LINK   $@"
	@$(CXXLINK) $(CXXLINKFLAGS) $(ALL_OBJS) $(CXXLIBS) -o $@ $(LIB_LRT)

ifneq (clean,$(MAKECMDGOALS))
-include $(AUTO_DEPS) # dependency files (.d)
endif
