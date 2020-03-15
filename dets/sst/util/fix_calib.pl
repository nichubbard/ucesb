#!/usr/bin/perl

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

use strict;
use warnings;

my @logval;
my $index = 0;

my $det = -1;

my @valid;

for (my $i = 0; $i < 1024; $i++) { $logval[$i] = "0"; $valid[$i] = 0; }

foreach my $argv (@ARGV)
{
    if ($argv =~ /det(\d+)/) { $det = $1; }
    elsif ($argv =~ /x(\d+)-(\d+)/) {
	for (my $i = $1*8; $i < ($2+1)*8; $i++) { $valid[$i] = 1; }
    }
    elsif ($argv =~ /y(\d+)-(\d+)/) {
	for (my $i = $1*8; $i < ($2+1)*8; $i++) { $valid[$i+640] = 1; }
    }
    else { die "Bad option: $argv"; }
}

while (my $line = <STDIN>)
{
    chomp $line;

    $line =~ s/^ +//;
    $line =~ s/ +$//;

    if (!($line =~ /\#/))
    {
	$logval[$index++] = $line;
    }
}

for (my $i = 0; $i < 1024; $i++)
{
    if ($logval[$i] ne "0")
    {
	my $logval = $logval[$i];
	my $val = 1.0/exp($logval);
	print sprintf ("%sCALIB_PARAM( SST[%d][%4d].E , OFFSET_SLOPE , 0.0 , %6.2f ); /* %4d : %s%3d : %6.2f */\n",
		       $valid[$i] ? "   " : "// ",$det,$i,
		       $val,$i,$i<640?"x":"y",($i < 640 ? $i : $i - 640) / 8,$logval);

    }
}


# CALIB_PARAM( SCI[0][0].T ,SLOPE_OFFSET,  2.4 , 6.7 );
