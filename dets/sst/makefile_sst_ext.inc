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

OBJS += siderem_ext.o sst_ext.o

DSPCODE_DIR :=$(call TREE_UCESB_OR_HOME_SRC_DIR_OR_DIE,landdaq_/vidsel/siderem)

SRC_DIRS += $(UCESB_BASE_DIR)/dets/sst

CXXFLAGS += -I$(DSPCODE_DIR)

SST_DET_ADD_PED = $(SST_DIR)/sst_get_add_ped.pl

$(GENDIR)/sst_ped_add.hh: sidped_daq.txt $(SST_DET_ADD_PED)
	@echo " GET_PED $@"
	@perl $(SST_DET_ADD_PED) < $< > $@
