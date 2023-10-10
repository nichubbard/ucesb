#!/bin/bash

LMD_DIR=$1
PNG_DIR=$2

mkdir -p $2

for i in `seq 0 99`
do
    # Do ten files (runs) in parallell.

    for j in `seq 0 9`
    do
	./tamex17sync --input-buffer=200M \
		      ${LMD_DIR}/main0${i}${j}_0001.lmd \
		      --syncplot=${PNG_DIR}/main0${i}${j}_0001.png \
		      --max-events=1000000 \
	    &
    done

    # Want for this bunch of files to complete.
    wait 
done
