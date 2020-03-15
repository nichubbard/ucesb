#!/bin/bash

cd ..

if [ x$1 = xclean ]; then
    make clean -j 10
    exit 0
fi

if [ x$1 = xmerge ]; then
    make clean -j 10
    CXXFLAGS="-g -fdiagnostics-color" USE_MERGING=1 NO_CERNLIB=1 NO_YACC_WERROR=1 make despec -j8
    cp despec/despec despec/merge
    make clean -j 10
fi

CXXFLAGS="-g -fdiagnostics-color" VERBOSE=1 USE_INPUTFILTER=1 NO_CERNLIB=1 NO_YACC_WERROR=1 make despec -j8
cp despec/despec despec/ucesb
