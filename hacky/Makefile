# This file is part of UCESB - a tool for data unpacking and processing.
#
# Copyright (C) 2016  GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
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

# This makefile is needed when you make a copy of this directory
# (hacky/) and use it outside the ucesb/ structure (where the
# Makefile is within the ucesb/ directory itself.

#########################################################

# In case of problems compiling the parsers, ask them to be generated
# with bison instead of yacc (trouble on redhat / bsds) (uncomment
# below)

#YACC=bison
#export YACC

#########################################################

# If you want to always enforce the inclusion of the ntuple-generation
# and the watcher (daqscope) facility, you may use the following
# declarations.  (otherwise, they can also be done on the command line,
# prior to invoking make)

#USE_CERNLIB=1
#USE_CURSES=1

#export USE_CERNLIB
#export USE_CURSES

#########################################################

# We still need access to the ucesb/ directory, since the ucesb and
# psdc programs are there, and since the common source also is there

TREE_OR_HOME_SRC_DIR = $(realpath $(firstword $(wildcard $(addsuffix /$(1),$(shell pwd)/.. $(HOME)))))

UCESB_BASE_DIR:=$(call TREE_OR_HOME_SRC_DIR,ucesb)
export UCESB_BASE_DIR

#########################################################

#CXX=g++-3.2
export CXX

#########################################################

# Our target, rename as needed

all: hacky

#########################################################
# Submakefiles that the programs depend on

include $(UCESB_BASE_DIR)/makefile_ucesbgen.inc
include $(UCESB_BASE_DIR)/makefile_psdc.inc

DEPENDENCIES=$(UCESBGEN) $(PSDC)

#########################################################

.PHONY: hacky
hacky: $(DEPENDENCIES)
	@$(MAKE) -f $(UCESB_BASE_DIR)/makefile_unpacker.inc UNPACKER=$@

#########################################################

clean:
	$(MAKE) -f $(UCESB_BASE_DIR)/makefile_unpacker.inc UNPACKER=hacky clean

all-clean: clean
	rm -rf gen

