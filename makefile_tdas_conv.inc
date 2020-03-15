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

TDAS_CONV=$(UCESB_BASE_DIR)/file_input/tdas_conv

$(TDAS_CONV): $(GENDIR)/acc_auto_def/byteorder_include.h \
	$(GENDIR)/acc_auto_def/byteswap_include.h

$(TDAS_CONV): $(TDAS_CONV).cc
	@echo "  BUILD  $@"
	@$(CXX) -O3 -g -D__LAND02_CODE__ \
	  -I$(UCESB_BASE_DIR)/file_input \
	  -I$(UCESB_BASE_DIR)/file_input/lmd_struct \
	  -I$(UCESB_BASE_DIR)/eventloop \
	  -I$(UCESB_BASE_DIR)/threading \
	  -I$(UCESB_BASE_DIR)/common \
	  -I$(UCESB_BASE_DIR)/lu_common \
	  -I$(UCESB_BASE_DIR)/acc_def \
	  -I$(UCESB_BASE_DIR) \
	  -I$(GENDIR) \
	  -o $@ $<

.PHONY: tdas_conv
tdas_conv: $(TDAS_CONV)
