#!/bin/sh

DATADIR=/net/data2/i123/IGISOL08/data/
NTUDIR=/net/data2/i123/ntuple/
FILES=`ls $DATADIR`

for i in $FILES
  do
  NTUNAME=$NTUDIR`echo $i | sed -e "s/.gz//"`.ntu
  nice -n 19 ./i123 $DATADIR$i --ntuple=RAW,$NTUNAME || rm $NTUNAME
  # bash32 h2root $NTUNAME
done

# cd /net/data2/i123/ntuple/
# ls *.ntu | xargs -n 1 h2root

