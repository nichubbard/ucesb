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

#########################################################

MAKE_ACC_AUTO_DEF=$(UCESB_BASE_DIR)/lu_common/make_acc_auto_def.sh

$(GENDIR)/gen/acc_auto_def/%.h $(GENDIR)/acc_auto_def/%.h: $(UCESB_BASE_DIR)/acc_def/%.h $(MAKE_ACC_AUTO_DEF) | $(GENDIR)/gen
	@echo " AUTODEF $@"
	@mkdir -p $(dir $@)
	@$(MAKE_ACC_AUTO_DEF) \
	  --input $< --compiler $(CXX) \
	  --compileargs $(filter-out ,$(CXXFLAGS)) > $@.tmp
	@mv $@.tmp $@

#########################################################
