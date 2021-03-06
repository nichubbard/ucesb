#!/bin/sh

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

# This was originally a perl script.
# But running a compiler inside a perl job was too much for some
# memory challenged machines...

INPUTFILE=
COMPILER=
COMPILEARGS=

set -e

while [ $1 ]
do
    case "$1" in
	--input)
	    if [ $2 ]; then
		INPUTFILE=$2; shift 2
	    else
		echo "$0: $1 missing argument" 1>&2 ; exit 1
	    fi ;;

	--compiler)
	    if [ $2 ]; then
		COMPILER=$2; shift 2
	    else
		echo "$0: $1 missing argument" 1>&2 ; exit 1
	    fi ;;

	--compileargs)
	    shift
	    COMPILEARGS=$@
	    break ;;

	*)
	    echo "$0: Invalid option: $1" 1>&2
	    exit 1;;
    esac
done

if [ ! $INPUTFILE ] ; then
    echo "No input file specified (--input FILE)." 1>&2 ; exit 1
fi

BASENAME=`echo $INPUTFILE | tr '[:lower:]' '[:upper:]' | \
    sed -e "s,.*/,," -e "s,\..*,,"`

# Careful: Here documents (inline text) created strange I/O problems
# with some systems.  Using echo instead.

echo "/*********************************************************************"
echo " *"
echo " * This file is autogenerated by $0"
echo " *"
echo " * Editing is useless."
echo " *"
echo " *********************************************************************"
echo " *"
echo " * Input file:    $INPUTFILE"
echo " * Basename:      $BASENAME"
echo " * Compiler:      $COMPILER"
echo " * Compiler args: $COMPILEARGS"
echo " *"
echo " ********************************************************************/"
echo
echo "#ifndef __AUTO_DEF__${BASENAME}_HH__"
echo "#define __AUTO_DEF__${BASENAME}_HH__"
echo

echo "/*"

OPTIONS=`grep ACC_DEF_${BASENAME}_ $INPUTFILE | \
    sed -e "s,.*ACC_DEF_${BASENAME}_\\([_a-zA-Z0-9]*\\),\1,"`

for opt in $OPTIONS
do
    echo " * Option: $opt"
done

echo " */"

# echo $OPTIONS

WORKING=
WORKING_OUTPUT_LENGTH=-1

# For each of the options, try to compile the input file

for opt in $OPTIONS
do
    COMPOPT=ACC_DEF_${BASENAME}_$opt

    EXITCODE=0
    OUTPUT=`$COMPILER -x c -c $INPUTFILE -o /dev/null -DACC_DEF_RUN -D$COMPOPT $COMPILEARGS 2>&1` || EXITCODE=$?
    OUTPUT_LENGTH=${#OUTPUT}

    echo "/*"
    echo " * -D$COMPOPT"
    echo
    echo "$OUTPUT"
    echo
    echo " * --> exit $EXITCODE (len $OUTPUT_LENGTH)"
    echo " */"

    if [ $EXITCODE -eq 0 ]
    then
	if [ ! $WORKING ]
	then
	    WORKING=$COMPOPT
	    WORKING_OUTPUT_LENGTH=$OUTPUT_LENGTH
	fi
	if [ $OUTPUT_LENGTH -lt $WORKING_OUTPUT_LENGTH ]
	then
	    WORKING=$COMPOPT
	    WORKING_OUTPUT_LENGTH=$OUTPUT_LENGTH
	fi
    fi
done

echo
echo "/********************************************************************/"
echo

if [ $WORKING ]
then
    echo "#define $WORKING 1";
else
    echo "/*** Did NOT find any working option. ***/";
fi

echo
echo "/********************************************************************/"
echo
echo "#endif/*__AUTO_DEF__${BASENAME}_HH__*/"

if [ ! $WORKING ]
then
    echo "No working option found." 1>&2
    exit 1
fi
