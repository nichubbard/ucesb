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

EXT_HBOOK_WRITER=$(UCESB_BASE_DIR)/hbook/hbook_writer
EXT_ROOT_WRITER=$(UCESB_BASE_DIR)/hbook/root_writer
EXT_STRUCT_WRITER=$(UCESB_BASE_DIR)/hbook/struct_writer

EXT_WRITERS=

#########################################################

# We only depend on (and compile) for cernlib, if we can find/set CERN_ROOT

ifndef NO_CERNLIB
ifndef CERN_ROOT
# Try to localise cernlib on our own
CERNLIB_CERN_ROOT=$(shell which cernlib | sed -e "s,/bin/cernlib,," | grep -v cernlib)
ifneq (,$(CERNLIB_CERN_ROOT))
ifneq (,$(shell (test -e $(CERNLIB_CERN_ROOT)/include/cfortran/packlib.h || test -e $(CERNLIB_CERN_ROOT)/include/packlib.h) && echo exists))
CERN_ROOT=$(CERNLIB_CERN_ROOT)
endif
endif
endif

ifdef CERN_ROOT
EXT_WRITERS += $(EXT_HBOOK_WRITER)
-include $(UCESB_BASE_DIR)/hbook/hbook_writer.base.dep
endif
endif

#########################################################

ifndef NO_ROOT
FILE_ROOT_CONFIG:=$(shell which root-config 2> /dev/null)
ifeq (,$(FILE_ROOT_CONFIG))
FILE_ROOT_CONFIG=$(shell which $(ROOTSYS)/bin/root-config 2> /dev/null)
endif
endif

ifneq (,$(FILE_ROOT_CONFIG))
EXT_WRITERS += $(EXT_ROOT_WRITER)
-include $(UCESB_BASE_DIR)/hbook/root_writer.base.dep
endif

#########################################################

EXT_WRITERS += $(EXT_STRUCT_WRITER)
-include $(UCESB_BASE_DIR)/hbook/struct_writer.base.dep

#########################################################

$(EXT_HBOOK_WRITER):
	@$(MAKE) -C $(UCESB_BASE_DIR)/hbook hbook_writer

$(EXT_ROOT_WRITER):
	@$(MAKE) -C $(UCESB_BASE_DIR)/hbook root_writer

$(EXT_STRUCT_WRITER):
	@$(MAKE) -C $(UCESB_BASE_DIR)/hbook struct_writer





