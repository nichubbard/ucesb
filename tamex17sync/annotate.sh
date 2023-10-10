#!/bin/bash

IN_DIR=$1
OUT_DIR=$2

mkdir -p ${OUT_DIR}

for file in `ls ${IN_DIR}`
do
    echo $file

    convert \
	${IN_DIR}/$file \
	-gravity south \
	-background white \
	-extent $(identify -format '%[fx:W+0]x%[fx:H+30]' ${IN_DIR}/$file) \
	-pointsize 10 -fill blue -gravity NorthWest \
	-annotate 0 "$file" \
	${OUT_DIR}/$file

done

convert -delay 5 ${OUT_DIR}/*.png ${OUT_DIR}/movie.mp4
