all: despec count_scalers

count_scalers: count_scalers.c ext_h101.h despec
	 c++ -g -O3 -o count_scalers -I. -I../hbook count_scalers.c ../hbook/ext_data_client.o

ext_h101.h:
	./despec /dev/null --ntuple=UNPACK,STRUCT_HH,ext_h101.h

despec:
	./make.sh

clean:
	./make.sh clean
	-rm ext_h101.h count_scalers
