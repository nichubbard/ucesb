# -*- Makefile -*-

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

# Makefile include for generating listings of dependencies
#
# $(BASENAME) is the name of the program,
#
# we then do $(BASENAME).dep on request

CPP_DEFINES_FREEBSD = $(shell $(CPP) -x c -dM /dev/null | grep __FreeBSD__)
CPP_DEFINES_NETBSD = $(shell $(CPP) -x c -dM /dev/null | grep __NetBSD__)
CPP_DEFINES_OPENBSD = $(shell $(CPP) -x c -dM /dev/null | grep __OpenBSD__)
CPP_DEFINES_SVR4 = $(shell $(CPP) -x c -dM /dev/null | grep __svr4__)
CPP_DEFINES_CYGWIN = $(shell $(CPP) -x c -dM /dev/null | grep __CYGWIN__)
CPP_DEFINES_APPLE = $(shell $(CPP) -x c -dM /dev/null | grep __APPLE__)

GCC_IS_3_3 = $(shell echo "__GNUC__ : __GNUC_MINOR__" | $(CXX) -E - | grep "3 : 3")
GCC_IS_4_3 = $(shell echo "__GNUC__ : __GNUC_MINOR__" | $(CXX) -E - | grep "4 : 3")
GCC_IS_4_4 = $(shell echo "__GNUC__ : __GNUC_MINOR__" | $(CXX) -E - | grep "4 : 4")

ifneq (,$(CPP_DEFINES_FREEBSD)$(CPP_DEFINES_NETBSD)$(CPP_DEFINES_SVR4))
SPACE_TO_NEWLINE = sed -e 'y, ,\n,'
else
SPACE_TO_NEWLINE = awk '{ gsub(/ /, "\n") ; print }'
endif

#$(warning BASENAME $(BASENAME))
#$(BASENAME).dep: $(AUTO_DEPS)
#	@echo " FULLDEP $@"
#	@echo "$@ $(MAKECMDGOALS): \\" > $@
#	@cat $(AUTO_DEPS) | sed -e 's,^.*:,,g' |\
#	  $(SPACE_TO_NEWLINE) |\
#	  sed -e 's,^,  ,g' |\
#	  sed -e 's,$$,\\,g' |\
#	  sed -e 's,\\\\$$,\\,g' | sort -u >> $@
#	@echo "  parser.y\\" >> $@
#	@echo "  lexer.lex\\" >> $@

