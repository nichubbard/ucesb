all: count_scalers
	./make.sh

count_scalers: count_scalers.c ext_h101.h
	 c++ -g -O3 -o count_scalers -I. -I../hbook count_scalers.c ../hbook/ext_data_client.o

clean:
	./make.sh clean
