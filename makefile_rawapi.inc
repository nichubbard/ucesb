# -*- Makefile -*-

# This file is part of UCESB - a tool for data unpacking and processing.
#
# Copyright (C) 2016  Haakan T. Johansson  <f96hajo@chalmers.se>
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

# Provides RAWAPI_DIR
include $(UCESB_BASE_DIR)/makefile_hasrawapi.inc

RAWAPI_CFLAGS=-D Linux -D SYS_UNIX

CXXFLAGS += -I$(RAWAPI_DIR)/inc

RAWAPI_USE_LIB=1

ifeq ($(RAWAPI_USE_LIB),1)
###################################################
# When using libraries

DEFAULT_SYSTEM_64:=$(shell $(CPP) -x c -dM /dev/null | grep __amd64__)

ifneq (,$(DEFAULT_SYSTEM_64))
CXXLIBS += -L $(RAWAPI_DIR)/Linux64 -lrawapiclin64 -lrawservn64
else
CXXLIBS += -L $(RAWAPI_DIR)/Linux -lrawapiclin -lrawservn
endif

###################################################
else # !RAWAPI_USE_LIB

RAWAPI_SRCS=rawCliProcn.c rawProcUn.c rawProcn.c \
	rawapin.c rconnect.c

RAWAPI_OBJS=$(RAWAPI_SRCS:%.c=rawapi_%.o)

###################################################
# When compiling from sources

OBJS += $(RAWAPI_OBJS)

###################################################

rawapi_%.o: $(RAWAPI_DIR)/src/%.c rawapi_%.d
	@echo "   CXX   $@"
	@$(CXX) -I. \
	  $(RAWAPI_CFLAGS) \
	  $(call FILTER_WARNINGS,$@,$(CXXFLAGS)) $(OPTFLAGS) -o $@ -c $<

rawapi_%.d: $(RAWAPI_DIR)/src/%.c
	@echo "  DEPS   $@"
	@$(CXX) -I. \
	  $(RAWAPI_CFLAGS) \
	    $(call FILTER_WARNINGS,$@,$(CXXFLAGS)) -MM -MG $< | \
	  sed -e 's,\($(*F)\)\.o[ :]*,rawapi_$*.o $@ : ,g' > $@

###################################################
endif # RAWAPI_USE_LIB
