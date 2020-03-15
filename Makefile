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

# In case of problems compiling the parsers, ask them to be generated
# with bison instead of yacc (trouble on redhat / bsds) (uncomment
# below)
#YACC=bison  # needed on SLC
#export YACC #           SLC
#LEX=flex    # needed on cygwin
#export LEX  #           cygwin

UCESB_BASE_DIR:=$(shell pwd)
export UCESB_BASE_DIR

#CXX=g++-3.2
export CXX

GENDIR=gen

#########################################################

UNPACKERS=land xtst rpc2006 is446 is430_05 is445_08 labbet1 mwpclab \
	gamma_k8 hacky empty sid_genf madrid ebye i123 s107 tacquila \
	fa192mar09 is507 sampler ridf despec

UNPACKERS_is446=is446_toggle is446_tglarray

UNPACKERS_xtst=xtst_toggle

all: $(UNPACKERS) $(UNPACKERS_is446) $(UNPACKERS_xtst)

#########################################################
# Submakefiles that the programs depend on

include $(UCESB_BASE_DIR)/makefile_gendir.inc
include $(UCESB_BASE_DIR)/makefile_acc_def.inc
include $(UCESB_BASE_DIR)/makefile_ucesbgen.inc
include $(UCESB_BASE_DIR)/makefile_psdc.inc
include $(UCESB_BASE_DIR)/makefile_empty_file.inc
include $(UCESB_BASE_DIR)/makefile_tdas_conv.inc
include $(UCESB_BASE_DIR)/makefile_ext_file_writer.inc
include $(UCESB_BASE_DIR)/makefile_ext_writer_test.inc

DEPENDENCIES=$(UCESBGEN) $(PSDC) $(EMPTY_FILE) $(EXT_WRITERS) \
	$(EXT_WRITER_TEST)_tests

include $(UCESB_BASE_DIR)/makefile_hasrawapi.inc
ifneq (,$(HAS_RAWAPI))
include $(UCESB_BASE_DIR)/makefile_rfiocmd.inc
DEPENDENCIES+=$(RFCAT)
endif

$(EXT_WRITER_TEST): $(EXT_WRITERS)

#########################################################

.PHONY: builddeps
builddeps: $(DEPENDENCIES)

#########################################################

EXTTDIR = ext_test

$(EXTTDIR)/ext_h99.h: $(EXT_WRITER_TEST)
	@echo "EXTWRTST $@"
	@mkdir -p $(EXTTDIR)
	@$(EXT_WRITER_TEST) --struct_hh=$@ \
	  > $@.out 2> $@.err || \
	  ( echo "Failure while running: '$(EXT_WRITER_TEST) --struct_hh=$@':" ; \
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr: ---"; cat $@.err ; \
	    echo "---------------" ; mv $@ $@.fail ; false)
#	#@rm $@.out $@.err

$(EXTTDIR)/ext_reader_h99_stderr: hbook/example/ext_data_reader_h99_stderr.c $(EXT_STRUCT_WRITER) $(EXTTDIR)/ext_h99.h
	@echo "  BUILD  $@"
	@$(CC) -W -Wall -Wconversion -g -O3 -o $@ -I$(EXTTDIR) -Ihbook \
	  -DEXT_H99 \
	  hbook/example/ext_data_reader_h99_stderr.c hbook/ext_data_client.o

$(EXTTDIR)/ext_reader_h99_stderr.runstamp: $(EXTTDIR)/ext_reader_h99_stderr
	@echo "  TEST   $@"
	@$(EXT_WRITER_TEST) --struct=- 2> $@.err2 | \
	  ./$< - > $@.out 2> $@.err || \
	  ( echo "* FAIL * ..." >> $@.out )
	@echo "---" >> $@.out
	@$(EXT_WRITER_TEST) --struct=- 2>> $@.err2 | \
	  ./$< --h99 - >> $@.out 2>> $@.err || \
	  ( echo "* FAIL * ..." >> $@.out )
	@echo "---" >> $@.out
	@$(EXT_WRITER_TEST) --struct=- 2>> $@.err2 | \
	  ./$< --h98 - >> $@.out 2>> $@.err || \
	  ( echo "* FAIL * ..." >> $@.out )
	@echo "---" >> $@.out
	@$(EXT_WRITER_TEST) --struct=- 2>> $@.err2 | \
	  ./$< --h99-h98 - >> $@.out 2>> $@.err || \
	  ( echo "* FAIL * ..." >> $@.out )
	@diff -u hbook/example/$(notdir $<).good $@.out || \
	  ( echo "--- Failure while running: " ; \
	    echo "$(EXT_WRITER_TEST) --struct=- | ./$< -" ;\
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr (ext_writer_test): ---"; cat $@.err2 ; \
	    echo "--- stderr ($@): ---"; cat $@.err ; \
	    echo "---------------" ; false)
#	#@rm $@.out $@.err $@.err2
	@touch $@

$(EXTTDIR)/ext_writer_test.root: $(EXT_WRITER_TEST)
	@echo " EXTWR_R $@"
	@mkdir -p $(EXTTDIR)
	@$(EXT_WRITER_TEST) --root=$@ \
	  > $@.out 2> $@.err || ( echo "* FAIL * ..." >> $@.out )
	@diff -u hbook/example/$(notdir $@).good $@.out || \
	  ( echo "--- Failure while running: " ; \
	    echo "$(EXT_WRITER_TEST) --root=$@" ;\
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr : ---"; cat $@.err ; \
	    echo "---------------" ; mv $@ $@.fail ; false)
#	#@rm $@.out $@.err
	@touch $@

#########################################################

.PHONY: $(EXT_WRITER_TEST)_tests
$(EXT_WRITER_TEST)_tests: $(EXTTDIR)/ext_h99.h \
	$(EXTTDIR)/ext_reader_h99_stderr.runstamp \

ifneq (,$(FILE_ROOT_CONFIG))
$(EXT_WRITER_TEST)_tests: $(EXTTDIR)/ext_writer_test.root
endif

#########################################################

.PHONY: empty_real
empty_real: $(DEPENDENCIES)
	@$(MAKE) -C empty -f ../makefile_unpacker.inc UNPACKER=empty

#########################################################

.PHONY: despec_real
despec_real: $(DEPENDENCIES)
	@$(MAKE) -C despec -f ../makefile_unpacker.inc UNPACKER=despec 

#########################################################

empty/empty: empty_real

EMPTY_EMPTY_FILE=--lmd --random-trig --events=10

$(EXTTDIR)/ext_h101.h: empty/empty $(EXT_STRUCT_WRITER)
	@echo "  EMPTY  $@"
	@mkdir -p $(EXTTDIR)
	@empty/empty /dev/null --ntuple=UNPACK,STRUCT_HH,$@ \
	  > $@.out 2> $@.err || \
	  ( echo "Failure while running: '$< /dev/null --ntuple=UNPACK,STRUCT_HH,$@':" ; \
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr: ---"; cat $@.err ; \
	    echo "---------------" ; mv $@ $@.fail ; false)
#	#@rm $@.out $@.err

# $(EXT_STRUCT_WRITER) make sure hbook/ext_data_client.o is built
$(EXTTDIR)/ext_reader_h101: hbook/example/ext_data_reader.c $(EXT_STRUCT_WRITER) $(EXTTDIR)/ext_h101.h
	@echo "  BUILD  $@"
	@$(CC) -W -Wall -Wconversion -g -O3 -o $@ -I$(EXTTDIR) -Ihbook \
	  hbook/example/ext_data_reader.c hbook/ext_data_client.o

$(EXTTDIR)/ext_reader_h101_items_info: hbook/example/ext_data_reader.c $(EXT_STRUCT_WRITER) $(EXTTDIR)/ext_h101.h
	@echo "  BUILD  $@"
	@$(CC) -W -Wall -Wconversion -g -O3 -o $@ -I$(EXTTDIR) -Ihbook \
	  -DUSE_ITEMS_INFO=1 \
	  hbook/example/ext_data_reader.c hbook/ext_data_client.o

$(EXTTDIR)/ext_reader_h101_stderr: hbook/example/ext_data_reader_stderr.c $(EXT_STRUCT_WRITER) $(EXTTDIR)/ext_h101.h
	@echo "  BUILD  $@"
	@$(CC) -W -Wall -Wconversion -g -O3 -o $@ -I$(EXTTDIR) -Ihbook \
	  hbook/example/ext_data_reader_stderr.c hbook/ext_data_client.o

$(EXTTDIR)/ext_reader_h101_cc: hbook/example/ext_data_reader_cc.cc $(EXT_STRUCT_WRITER) $(EXTTDIR)/ext_h101.h
	@echo "  BUILD  $@"
	@$(CXX) -W -Wall -Wconversion -g -O3 -o $@ -I$(EXTTDIR) -Ihbook \
	  hbook/example/ext_data_reader_cc.cc \
	  hbook/ext_data_client.o hbook/ext_data_clnt.o

$(EXTTDIR)/ext_writer_h101: hbook/example/ext_data_writer.c $(EXT_STRUCT_WRITER) $(EXTTDIR)/ext_h101.h
	@echo "  BUILD  $@"
	@$(CC) -W -Wall -Wconversion -g -O3 -o $@ -I$(EXTTDIR) -Ihbook \
	  hbook/example/ext_data_writer.c hbook/ext_data_client.o

$(EXTTDIR)/ext_reader_h%.runstamp: $(EXTTDIR)/ext_reader_h% $(EMPTY_FILE)
	@echo "  TEST   $@"
	@$(EMPTY_FILE) $(EMPTY_EMPTY_FILE) 2> $@.err3 | \
	  empty/empty --file=- --ntuple=UNPACK,STRUCT,- 2> $@.err2 | \
	  ./$< - > $@.out 2> $@.err || ( echo "* FAIL * ..." >> $@.out )
	@diff -u hbook/example/$(notdir $<).good $@.out || \
	  ( echo "--- Failure while running: " ; \
	    echo "$(EMPTY_FILE) $(EMPTY_EMPTY_FILE) | empty/empty --file=- --ntuple=UNPACK,STRUCT,- | ./$< -" ;\
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr (empty_file): ---"; cat $@.err3 ; \
	    echo "--- stderr (empty): ---"; cat $@.err2 ; \
	    echo "--- stderr ($@): ---"; cat $@.err ; \
	    echo "---------------" ; false)
#	#@rm $@.out $@.err $@.err2 $@.err3
	@touch $@

$(EXTTDIR)/ext_writer_h%.runstamp: $(EXTTDIR)/ext_writer_h% $(EMPTY_FILE)
	@echo "  TEST   $@"
	@./$< 10 2> $@.err2 | \
	  empty/empty --in-tuple=UNPACK,STRUCT,- > $@.out 2> $@.err || \
	  ( echo "* FAIL * ..." >> $@.out )
	@diff -u hbook/example/$(notdir $<).good $@.out || \
	  ( echo "--- Failure while running: " ; \
	    echo "./$< 10 | empty/empty --in-tuple=UNPACK,STRUCT,-" ; \
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr ($@): ---"; cat $@.err2 ; \
	    echo "--- stderr (empty): ---"; cat $@.err ; \
	    echo "---------------" ; false)
#	#@rm $@.out $@.err $@.err2
	@touch $@

#########################################################

.PHONY: empty
empty: empty_real
ifndef USE_MERGING # disabled for the time being (to be fixed...)
empty: $(EXTTDIR)/ext_reader_h101.runstamp \
	$(EXTTDIR)/ext_reader_h101_items_info.runstamp \
	$(EXTTDIR)/ext_reader_h101_stderr.runstamp \
	$(EXTTDIR)/ext_reader_h101_cc.runstamp \
	$(EXTTDIR)/ext_writer_h101.runstamp
endif

#########################################################

.PHONY: despec
despec: despec_real
ifndef USE_MERGING # disabled for the time being (to be fixed...)
despec: $(EXTTDIR)/ext_reader_h101.runstamp \
	$(EXTTDIR)/ext_reader_h101_items_info.runstamp \
	$(EXTTDIR)/ext_reader_h101_stderr.runstamp \
	$(EXTTDIR)/ext_writer_h101.runstamp
endif
#########################################################

.PHONY: xtst_real
xtst_real: $(DEPENDENCIES)
	@$(MAKE) -C xtst -f ../makefile_unpacker.inc UNPACKER=xtst

#########################################################

.PHONY: xtst_toggle
xtst_toggle: $(DEPENDENCIES)
	@$(MAKE) -C xtst -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

xtst/xtst: xtst_real

XTST_EMPTY_FILE=--lmd --random-trig --caen-v775=2 --caen-v1290=2 \
   --sticky-fraction=29 --events=1036
XTST_REGRESS=UNPACK,regress1,RAW:STCORR,ID=xtst_regress
XTST_REGRESS_MORE=UNPACK,regress1,regressextra,RAW:STCORR,ID=xtst_regress
XTST_REGRESS_LESS=UNPACK,regress1,RAW:STCORR,ID=xtst_regress

# We depend on empty (full ext_reader stuff, as it is simpler)
# Remove empty dependency, caused rebuild of empty on expensive arm/ppc target
$(EXTTDIR)/ext_xtst_regress.h: xtst/xtst $(EXT_STRUCT_WRITER) # $(EXTTDIR)/ext_reader_h101.runstamp
	@echo "  XTST   $@"
	@mkdir -p $(EXTTDIR)
	@xtst/xtst /dev/null \
	  --ntuple=$(XTST_REGRESS),STRUCT,- > $@.extstrdump \
	  2> $@.err || \
	  ( echo "Failure while running: '$< /dev/null --ntuple=$(XTST_REGRESS),-':" ; \
	    echo "--- stderr: ---"; cat $@.err ; \
	    echo "---------------" ; false)
	@hbook/struct_writer - --header=$@ < $@.extstrdump \
	  > $@.out2 2> $@.err2 || \
	  ( echo "Failure while running: 'hbook/struct_writer --header=$@ < $@.str':" ; \
	    echo "--- stdout: ---" ; cat $@.out2 ; \
	    echo "--- stderr: ---"; cat $@.err2 ; \
	    echo "---------------" ; mv $@ $@.fail ; false)
#	#@rm $@.out $@.err
#	#@rm $@.out2 $@.err2

# $(EXT_STRUCT_WRITER) make sure hbook/ext_data_client.o is built
$(EXTTDIR)/ext_reader_xtst_regress: hbook/example/ext_data_reader_xtst_regress.c $(EXT_STRUCT_WRITER) $(EXTTDIR)/ext_xtst_regress.h hbook/example/test_caen_v775_data.h hbook/example/test_caen_v1290_data.h
	@echo "  BUILD  $@"
	@$(CC) -W -Wall -Wconversion -g -O3 -o $@ -I$(EXTTDIR)/ -Ihbook \
	  hbook/example/ext_data_reader_xtst_regress.c hbook/ext_data_client.o

$(EXTTDIR)/ext_reader_xtst_regress.runstamp: $(EXTTDIR)/ext_reader_xtst_regress $(XTST_FILE)
	@echo "  TEST   $@"
	@$(EMPTY_FILE) $(XTST_EMPTY_FILE) 2> $@.err3 | \
	  xtst/xtst --file=- \
	    --ntuple=$(XTST_REGRESS),STRUCT,- 2> $@.err2 | \
	  ./$< - > $@.out 2> $@.err || echo "fail..."
	@diff -u hbook/example/$(notdir $<).good $@.out || \
	  ( echo "Failure while running: xtst_file | xtst | $@:" ; \
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr (xtst_file): ---"; cat $@.err3 ; \
	    echo "--- stderr (xtst): ---"; cat $@.err2 ; \
	    echo "--- stderr ($@): ---"; cat $@.err ; \
	    echo "---------------" ; false)
#	#@rm $@.out $@.err $@.err2 $@.err3
	@touch $@

$(EXTTDIR)/ext_reader_xtst_regress_more.runstamp: $(EXTTDIR)/ext_reader_xtst_regress $(XTST_FILE)
	@echo "  TEST   $@"
	@$(EMPTY_FILE) $(XTST_EMPTY_FILE) 2> $@.err3 | \
	  xtst/xtst --file=- \
	    --ntuple=$(XTST_REGRESS_MORE),STRUCT,- 2> $@.err2 | \
	  ./$< - > $@.out 2> $@.err || echo "fail..."
	@diff -u hbook/example/$(notdir $<)_more.good $@.out || \
	  ( echo "Failure while running: xtst_file | xtst | $@:" ; \
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr (xtst_file): ---"; cat $@.err3 ; \
	    echo "--- stderr (xtst): ---"; cat $@.err2 ; \
	    echo "--- stderr ($@): ---"; cat $@.err ; \
	    echo "---------------" ; false)
#	#@rm $@.out $@.err $@.err2 $@.err3
	@touch $@

$(EXTTDIR)/ext_reader_xtst_regress_less.runstamp: $(EXTTDIR)/ext_reader_xtst_regress $(XTST_FILE)
	@echo "  TEST   $@"
	@$(EMPTY_FILE) $(XTST_EMPTY_FILE) 2> $@.err3 | \
	  xtst/xtst --file=- \
	    --ntuple=$(XTST_REGRESS_LESS),STRUCT,- 2> $@.err2 | \
	  ./$< - > $@.out 2> $@.err || echo "fail..."
	@diff -u hbook/example/$(notdir $<)_less.good $@.out || \
	  ( echo "Failure while running: xtst_file | xtst | $@:" ; \
	    echo "--- stdout: ---" ; cat $@.out ; \
	    echo "--- stderr (xtst_file): ---"; cat $@.err3 ; \
	    echo "--- stderr (xtst): ---"; cat $@.err2 ; \
	    echo "--- stderr ($@): ---"; cat $@.err ; \
	    echo "---------------" ; false)
#	#@rm $@.out $@.err $@.err2 $@.err3
	@touch $@

#########################################################

.PHONY: xtst
xtst: xtst_real
ifndef USE_MERGING # disabled for the time being (to be fixed...)
xtst: $(EXTTDIR)/ext_reader_xtst_regress.runstamp \
	$(EXTTDIR)/ext_reader_xtst_regress_more.runstamp \
	$(EXTTDIR)/ext_reader_xtst_regress_less.runstamp
endif

#########################################################

.PHONY: hacky
hacky: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: sid_genf
sid_genf: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: sampler
sampler: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: madrid
madrid: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: is507
is507: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: tagtest
tagtest: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: labbet1
labbet1: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: mwpclab
mwpclab: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: gamma_k8
gamma_k8: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: is430_05
is430_05: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: is445_08
is445_08: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: is446
is446: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: is446_toggle
is446_toggle: $(DEPENDENCIES)
	@$(MAKE) -C $(firstword $(subst _, ,$@)) \
	  -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: is446_tglarray
is446_tglarray: $(DEPENDENCIES)
	@$(MAKE) -C $(firstword $(subst _, ,$@)) \
	  -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: rpc2006
rpc2006: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: land
land: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: ebye
ebye: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: i123
i123: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: s107
s107: $(DEPENDENCIES) $(TDAS_CONV)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: tacquila
tacquila: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: fa192mar09
fa192mar09: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

.PHONY: ridf
ridf: $(DEPENDENCIES)
	@$(MAKE) -C $@ -f ../makefile_unpacker.inc UNPACKER=$@

#########################################################

clean: clean-dir-ucesbgen clean-dir-psdc clean-dir-file_input \
	clean-dir-rfiocmd clean-dir-hbook \
	$(UNPACKERS:%=clean-unp-%) $(UNPACKERS_EXT:%=clean-unp-%) \
	$(UNPACKERS_is446:%=clean-unp-is446-%) \
	$(UNPACKERS_xtst:%=clean-unp-xtst-%)
	rm -rf gen/acc_auto_def gen/
	rm -f xtst/xtst.spec.d xtst/*.o xtst/*.d xtst/*.dep
	rm -f file_input/empty_file file_input/tdas_conv
	rm -f hbook/example/ext_writer_test
	rm -rf $(EXTTDIR)/
	@if (echo $(MAKEFLAGS) | grep -v -q j); then \
		echo "Hint: if cleaning is slow, do 'make clean -j 10'" ; \
	fi

clean-dir-%:
	$(MAKE) -C $* clean

clean-unp-%:
	$(MAKE) -C $* -f ../makefile_unpacker.inc UNPACKER=$* clean

clean-unp-is446-%:
	$(MAKE) -C is446 -f ../makefile_unpacker.inc UNPACKER=$* clean

clean-unp-xtst-%:
	$(MAKE) -C xtst -f ../makefile_unpacker.inc UNPACKER=$* clean

all-clean: clean
	rm -rf land/gen
	rm -rf xtst/gen
