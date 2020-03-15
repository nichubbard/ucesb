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

all:

include ../makefile_deps.inc

ifdef BUILD_LAND02
# TODO: this is to be handled in some better way!!!
CXXFLAGS += -DBUILD_LAND02
DEP_BASE_DIR=$$(HBOOK_BASE_DIR)
DEP_BASE_DIR_ESC=$$\(HBOOK_BASE_DIR\)
LU_COMMON_DIR=$(HBOOK_BASE_DIR)/../../lu_common
else
DEP_BASE_DIR=$$(UCESB_BASE_DIR)
DEP_BASE_DIR_ESC=$$\(UCESB_BASE_DIR\)
LU_COMMON_DIR=$(UCESB_BASE_DIR)/lu_common
endif

#####################################################################

CXXFLAGS += #-ansi -Wall -W -Wno-unused-function -Wno-unused-label \
	-Wno-unused-parameter -Wwrite-strings #-Wconversion \
	-Wno-non-template-friend  # -Wshadow -Wcast-qual
CFLAGS += -Wall -W -Wno-unused-function -Wno-unused-label \
	-Wno-unused-parameter -Wwrite-strings #-Wconversion \
	 -Wshadow -Wcast-qual

CFLAGS += -g -O3
CXXFLAGS += -g -O3
# OPTFLAGS += -DNDEBUG -O3
LDFLAGS += -g

#########################################################

FILE_CURSES_CONFIG=$(shell which ncurses5-config 2> /dev/null)

ifneq (,$(FILE_CURSES_CONFIG))
CURSES_CFLAGS = $(shell $(FILE_CURSES_CONFIG) --cflags)
CURSES_LIBS = $(shell $(FILE_CURSES_CONFIG) --libs)
else
# Try with some defaults:
CURSES_CFLAGS = 
CURSES_LIBS = -lcurses
endif

# And then to verify (also in case ncurses5-config was there, because
# sometimes installations are broken (no symbol-names / .a-files)

HAVE_CURSES := $(shell gcc -o /dev/null $(CURSES_CFLAGS) \
	$(LU_COMMON_DIR)/curses_test.c $(CURSES_LIBS) \
	2> /dev/null && echo -DHAVE_CURSES)

# If, then we are good
ifneq (,$(HAVE_CURSES))
USE_CURSES = 1
CXXFLAGS     += -DUSE_CURSES=$(USE_CURSES) $(CURSES_CFLAGS)
CXXLIBS      += $(CURSES_LIBS)
endif

#####################################################################

ifneq (,$(filter hbook_writer,$(MAKECMDGOALS)))
ifndef CERN_ROOT
# Try to localise cernlib on our own
CERNLIB_CERN_ROOT=$(shell which cernlib | sed -e "s,/bin/cernlib,," | grep -v cernlib)
ifneq (,$(CERNLIB_CERN_ROOT))
ifneq (,$(shell (test -e $(CERNLIB_CERN_ROOT)/include/cfortran/packlib.h || test -e $(CERNLIB_CERN_ROOT)/include/packlib.h) && echo exists))
CERN_ROOT=$(CERNLIB_CERN_ROOT)
endif
endif
endif

ifndef CERN_ROOT
ifdef USE_CERNLIB
$(error CERNLIB requested (via USE_CERNLIB), but cannot find 'cernlib' and CERN_ROOT is not set.  Perhaps '. cernlogin pro' or '. cernlogin.bash pro' will help.)
endif 
else # CERN_ROOT is defined!  rules to build hbook_writer
all: hbook_writer

CERN_INCLUDE_DIRS += $(addprefix $(CERN_ROOT),/include/cfortran/ /src/cfortran/)

# Use an ugly hack to try to find out if we want to force 32-bit build
# (we should really look at the size of libpacklib, but to find that
# is a pain.  Additionally, to find the .a file does not help, we need
# to go into that to get out the arch it was build for...  .so files
# do not always exist.  Looking at e.g. pawX11 is much easier.  OF course,
# this is not really what we want...

# We additionally only want to set the flag if we could need it, i.e.
# are usually compiling 64-bit

DEFAULT_SYSTEM_64:=$(shell cpp -dM /dev/null | grep __amd64__)

ifneq (,$(DEFAULT_SYSTEM_64))
FILE_PAWX11:=$(shell which pawX11)
ifneq (,$(FILE_PAWX11))
FILE_PAWX11_32:=$(shell file $(FILE_PAWX11) | grep "32-bit")
ifneq (,$(FILE_PAWX11_32))
ifneq (,$(findstring -m64,$(CXXFLAGS)))
$(error ROOT requested 64-bit build, CERNLIB about to request 32-bit build.  Cannot coexist like that.)
endif
FLAGS_CERNLIB_M32 = -m32
endif
endif
endif

HBOOK_CXXFLAGS     += -DUSING_CERNLIB=1 \
	-Df2cFortran \
	$(addprefix -I,$(CERN_INCLUDE_DIRS)) \
	$(FLAGS_CERNLIB_M32)
HBOOK_CXXLINKFLAGS += $(FLAGS_CERNLIB_M32)
# libshift seems not needed, but gives problems on redhat at least
HBOOK_CXXLIBS      += $(shell $(CERN_ROOT)/bin/cernlib packlib | sed -e 's/-lshift//')

ifeq (,$(filter -lg2c -lgfortran,$(HBOOK_CXXLIBS)))
# cernlib did not tell if it wants -lg2c or -lgfortran

# If we manage to find an (absolute) path to libpacklib, then do nm on that,
# and see if we find 'gfortran' or 'g77' in the output
HBOOK_LIBPACKLIB = $(filter %packlib.a,$(shell $(CERN_ROOT)/bin/cernlib packlib))
ifeq (,$(HBOOK_LIBPACKLIB))
GUESS_LIBPACKLIB = $(CERN_ROOT)/lib/libpacklib.a
HBOOK_LIBPACKLIB = $(shell test -e $(GUESS_LIBPACKLIB) && echo $(GUESS_LIBPACKLIB))
endif
ifneq (,$(HBOOK_LIBPACKLIB))
NM_LIBPACKLIB_G77 = $(shell nm $(HBOOK_LIBPACKLIB) | grep _g77_)
NM_LIBPACKLIB_GFORTRAN = $(shell nm $(HBOOK_LIBPACKLIB) | grep _gfortran_)
ifeq (,$(NM_LIBPACKLIB_G77)$(NM_LIBPACKLIB_GFORTRAN))
$(error No clue if cernlib might want -lg2c or -lgfortran)
endif
ifneq (,$(NM_LIBPACKLIB_G77))
HBOOK_CXXLIBS += -lg2c
endif
ifneq (,$(NM_LIBPACKLIB_GFORTRAN))
HBOOK_CXXLIBS += -lgfortran
endif
endif

# And sometimes the cernlib given suggests g77, while only gfortran is
# available.  Then we are out of luck, as cernlib has pointed us to a
# bad place.  At least tell the user so.

HBOOK_FLAGS_ERR := $(shell gcc -o /dev/null maintest.c \
	$(HBOOK_CXXFLAGS) $(HBOOK_CXXLINKFLAGS) $(HBOOK_CXXLIBS) \
        2>&1)

ifneq (,$(HBOOK_FLAGS_ERR))
$(warning '. cernlogin 2005-lenny32-gfortran' helps on some 32 bit GSI machines.)
$(error Cannot compile with cernlib (using deduced flags+libs \
	'$(HBOOK_CXXFLAGS) $(HBOOK_CXXLINKFLAGS) $(HBOOK_CXXLIBS)'): \
	$(HBOOK_FLAGS_ERR)   See suggestions above..)
endif
endif

AUTO_DEPS += ext_hbook_writer.d

ext_hbook_writer.d: ext_file_writer.cc 
	@echo "  DEPS   $@"
	@$(CXX) $(CXXFLAGS) $(HBOOK_CXXFLAGS) -MM -MG $< | \
	  sed -e 's,\($(*F)\)\.o[ :]*,$*.o $@ : ,g' \
	> $@

ext_hbook_writer.o: ext_file_writer.cc ext_hbook_writer.d
	@echo "   CXX   $@"
	@$(CXX) $(CXXFLAGS) $(HBOOK_CXXFLAGS) -o $@ -c $<

hbook_writer: ext_hbook_writer.o hbook_writer.base.dep
	@echo "  LINK   $@"
	@$(CXX) $(LDFLAGS) $(HBOOK_CXXLINKFLAGS) -o $@ $< \
	  $(HBOOK_CXXLIBS) $(CXXLIBS) || \
	  echo "Linking $@ failed, perhaps not using CERNLIB" \
	  "installation correctly.  If not needed, try to make with" \
	  "NO_CERNLIB=1"

hbook_writer.dep: ext_hbook_writer.d
	@echo " FULLDEP $@"
	@echo "$@ hbook_writer: \\" > $@
	@cat ext_hbook_writer.d | sed -e 's,^.*:,,g' |\
	  sed -e 's,^#.*,,g' |\
	  sed -e 's,^.*:,,g' |\
	  $(SPACE_TO_NEWLINE) |\
	  sed -e 's,^,  ,g' |\
	  sed -e 's,$$,\\,g' |\
	  sed -e 's,\\\\$$,\\,g' | sort -u >> $@

hbook_writer.base.dep: hbook_writer.dep
	@echo " BASEDEP $@"
	@echo '$(DEP_BASE_DIR)/hbook/hbook_writer: \' > $@
	@cat $< | grep -v ' \\' | grep -v $(CERN_ROOT) | grep -v '/sw/include/' | sed -e 's,  ,  $(DEP_BASE_DIR_ESC)/hbook/,g' | sort -u >> $@

endif # CERN_ROOT
endif # $(MAKECMDGOALS) has hbook_writer

#####################################################################

ifneq (,$(filter root_writer struct_writer,$(MAKECMDGOALS)))
FILE_ROOT_CONFIG:=$(shell which root-config 2> /dev/null)
ifeq (,$(FILE_ROOT_CONFIG))
FILE_ROOT_CONFIG=$(shell which $(ROOTSYS)/bin/root-config 2> /dev/null)
endif

FILE_ROOTCINT:=$(shell which rootcint 2> /dev/null)
ifeq (,$(FILE_ROOTCINT))
FILE_ROOTCINT=$(shell which $(ROOTSYS)/bin/rootcint 2> /dev/null)
endif

ROOT_CXXFLAGS += $(shell $(FILE_ROOT_CONFIG) --cflags)
ROOT_CXXLINKFLAGS += $(shell $(FILE_ROOT_CONFIG) --ldflags)
ROOT_CXXLIBS      += $(shell $(FILE_ROOT_CONFIG) --libs)

ROOT_INC_DIR = $(shell $(FILE_ROOT_CONFIG) --incdir | sed -e 's,//,/,g')

ROOT_CXXFLAGS += $(shell test -e $(ROOT_INC_DIR)/TTreeCompressThreads.h && echo "-DROOT_HAS_TTREE_COMPRESS_THREADS=1")
endif

#####################################################################

ifneq (,$(filter root_writer,$(MAKECMDGOALS)))
ifeq (,$(FILE_ROOT_CONFIG))
ifdef USE_ROOT
$(error ROOT requested (via USE_ROOT), but 'root-config' cannot be found (even with ROOTSYS).)
endif
else # ROOT found!  rules to build root_writer
all: root_writer

ROOT_CXXFLAGS += -DUSING_ROOT=1

AUTO_DEPS += ext_root_writer.d

ext_root_writer.d: ext_file_writer.cc 
	@echo "  DEPS   $@"
	@$(CXX) $(CXXFLAGS) $(ROOT_CXXFLAGS) -MM -MG $< | \
	  sed -e 's,\($(*F)\)\.o[ :]*,$*.o $@ : ,g' \
	> $@

ext_root_writer.o: ext_file_writer.cc ext_root_writer.d
	@echo "   CXX   $@"
	@$(CXX) $(CXXFLAGS) $(ROOT_CXXFLAGS) -o $@ -c $<

root_writer: ext_root_writer.o root_writer.base.dep
	@echo "  LINK   $@"
	@$(CXX) $(LDFLAGS) $(ROOT_CXXLINKFLAGS) -o $@ $< \
	  $(ROOT_CXXLIBS) $(CXXLIBS)

root_writer.dep: ext_root_writer.d
	@echo " FULLDEP $@"
	@echo "$@ root_writer: \\" > $@
	@cat ext_root_writer.d | sed -e 's,^.*:,,g' |\
	  sed -e 's,^#.*,,g' |\
	  sed -e 's,^.*:,,g' |\
	  $(SPACE_TO_NEWLINE) |\
	  sed -e 's,^,  ,g' |\
	  sed -e 's,$$,\\,g' |\
	  sed -e 's,\\\\$$,\\,g' | sort -u >> $@

root_writer.base.dep: root_writer.dep
	@echo " BASEDEP $@"
	@echo '$(DEP_BASE_DIR)/hbook/root_writer: \' > $@
	@cat $< | grep -v ' \\' | grep -v $(ROOT_INC_DIR) | grep -v '/sw/include/' | sed -e 's,  ,  $(DEP_BASE_DIR_ESC)/hbook/,g' | sort -u >> $@

endif
endif # $(MAKECMDGOALS) has root_writer

#####################################################################

ifneq (,$(filter struct_writer,$(MAKECMDGOALS)))

all: struct_writer

STRUCT_CXXFLAGS += -DSTRUCT_WRITER=1
STRUCT_CXXFLAGS += 
STRUCT_CXXLINKFLAGS += 
STRUCT_CXXLIBS      += 

AUTO_DEPS += ext_struct_writer.d ext_struct_net_io.d

ext_struct_writer.d: ext_file_writer.cc 
	@echo "  DEPS   $@"
	@$(CXX) $(CXXFLAGS) $(STRUCT_CXXFLAGS) -MM -MG $< | \
	  sed -e 's,\($(*F)\)\.o[ :]*,$*.o $@ : ,g' \
	> $@

ext_struct_writer.o: ext_file_writer.cc ext_struct_writer.d
	@echo "   CXX   $@"
	@$(CXX) $(CXXFLAGS) $(STRUCT_CXXFLAGS) -o $@ -c $<

ext_struct_net_io.d: ext_file_net_io.cc 
	@echo "  DEPS   $@"
	@$(CXX) $(CXXFLAGS) $(STRUCT_CXXFLAGS) -MM -MG $< | \
	  sed -e 's,\($(*F)\)\.o[ :]*,$*.o $@ : ,g' \
	> $@

ext_struct_net_io.o: ext_file_net_io.cc ext_struct_net_io.d
	@echo "   CXX   $@"
	@$(CXX) $(CXXFLAGS) $(STRUCT_CXXFLAGS) -o $@ -c $<

struct_writer: ext_struct_writer.o ext_struct_net_io.o struct_writer.base.dep
	@echo "  LINK   $@"
	@$(CXX) $(LDFLAGS) $(STRUCT_CXXLINKFLAGS) -o $@ \
	  ext_struct_writer.o ext_struct_net_io.o $(STRUCT_CXXLIBS) $(CXXLIBS)

struct_writer.dep: ext_struct_writer.d ext_struct_net_io.d ext_data_client.d 
	@echo " FULLDEP $@"
	@echo "$@ struct_writer: \\" > $@
	@cat ext_struct_writer.d ext_struct_net_io.d ext_data_client.d | \
	  sed -e 's,^.*:,,g' |\
	  sed -e 's,^#.*,,g' |\
	  sed -e 's,^.*:,,g' |\
	  $(SPACE_TO_NEWLINE) |\
	  sed -e 's,^,  ,g' |\
	  sed -e 's,$$,\\,g' |\
	  sed -e 's,\\\\$$,\\,g' | sort -u >> $@

struct_writer.base.dep: struct_writer.dep
	@echo " BASEDEP $@"
	@echo '$(DEP_BASE_DIR)/hbook/struct_writer: \' > $@    # emacs-dummy: '
	@cat $< | grep -v ' \\' | grep -v '/sw/include/' | sed -e 's,  ,  $(DEP_BASE_DIR_ESC)/hbook/,g' | sort -u >> $@

#

AUTO_DEPS += ext_data_client.d

ext_data_client.d: ext_data_client.c
	@echo "  DEPS   $@"
	@$(CC) $(CFLAGS) -MM -MG $< | \
	  sed -e 's,\($(*F)\)\.o[ :]*,$*.o $@ : ,g' \
	> $@

ext_data_client.o: ext_data_client.c ext_data_client.d
	@echo "   CC    $@"
	@$(CC) $(CFLAGS) -fPIC -o $@ -c $<

struct_writer: ext_data_client.o

ifneq (,$(FILE_ROOT_CONFIG))

%_dict.cc: %.hh
	@echo "ROOTCINT $@"
	@rootcint -f $@ -c $<

# Would be ext_data_clnt% , but the % wants at least one letter...
ext_data_cln%.o: ext_data_cln%.cc
	@echo "   CXX   $@"
	@$(CXX) $(CXXFLAGS) -fPIC $(ROOT_CXXFLAGS) -o $@ -c $<

# Same rule as previous (share?)
online_chai%.o: online_chai%.cc
	@echo "   CXX   $@"
	@$(CXX) $(CXXFLAGS) -fPIC $(ROOT_CXXFLAGS) -o $@ -c $<

.PRECIOUS: ext_data_clnt_dict.cc ext_data_clnt_stderr_dict.cc online_chain_dict.cc

libext_data_clnt.so: ext_data_client.o ext_data_clnt.o \
	ext_data_clnt_dict.o ext_data_clnt_stderr_dict.o \
	online_chain.o online_chain_dict.o
	@echo "  LINK   $@"
	@$(CXX) -g -shared $(ROOT_CXXLINKFLAGS) -o $@ $+ \
	  $(ROOT_CXXLIBS)

struct_writer: libext_data_clnt.so

endif

endif # $(MAKECMDGOALS) has struct_writer

#####################################################################

clean:
	rm -f *.o *.d
	rm -f root_writer.base.dep hbook_writer.base.dep struct_writer.base.dep
	rm -f root_writer.dep hbook_writer.dep struct_writer.dep
	rm -f root_writer hbook_writer struct_writer
	rm -f root_reader.base.dep
	rm -f root_reader.dep
	rm -f root_reader
	rm -f ext_data_clnt*_dict.cc ext_data_clnt*_dict.h 
	rm -f online_chain_dict.cc online_chain_dict.h
	rm -f libext_data_clnt.so 

ifneq (clean,$(MAKECMDGOALS))
-include $(AUTO_DEPS) # dependency files (.d)
endif
