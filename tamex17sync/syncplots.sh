#!/bin/bash

LMD_DIR=$1
PNG_DIR=$2

mkdir -p $2

for i in `seq -f "%04.0f" 0 999`
do
    # Do ten files (runs) in parallell.

    ./tamex17sync --input-buffer=200M \
		  ${LMD_DIR}/main${i}_0001.lmd \
		  --syncplot=${PNG_DIR}/main${i}_0001.png \
		  --max-events=1000000 \
	&

    while :
    do
        running=$(jobs | wc -l | xargs)
        if [ $running -le 20 ] ; then break ; else sleep .05 ; fi
    done

done

wait # Wait for remaining jobs
