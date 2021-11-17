all: despec count_scalers

count_scalers: count_scalers.c ext_h101.h
	 c++ -g -O3 -o count_scalers -I. -I../hbook count_scalers.c ../hbook/ext_data_client.o

ext_h101.h:
	./despec /dev/null --ntuple=UNPACK,STRUCT_HH,ext_h101.h

.PHONY: despec
despec:
	$(MAKE) -C .. USE_INPUTFILTER=1 NO_CERNLIB=1 NO_YACC_WERROR=1 despec

.PHONY: clean
clean:
	$(MAKE) -C .. -j 10 clean
	-rm ext_h101.h count_scalers
