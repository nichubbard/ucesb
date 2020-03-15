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

# DG 0   50   19  0.9333  0.0169  0.2940
# DE 0  258  259 -0.0846  0.0194  0.5109

my $dodet = $ARGV[0];

my %used_index;

sub insert_index($$);

my @DE;
my @DG;

while (my $line = <STDIN>)
{
    if ($line =~ s/DE *//)
    {
	my @val = split ' +',$line;

	# insert_index

	my $det = $val[0];
	my $i1  = $val[1];
	my $i2  = $val[2];

	my $mean   = $val[3];
	my $mean_e = $val[4];

	# print "$val[0] $val[1] $val[2]\n";

	if ($det == $dodet)
	{
	    # print "$val[0] $val[1] $val[2]\n";

	    insert_index($i1,1);
	    insert_index($i2,1);

	    push @DE , [ $i1, $i2, $mean, $mean_e ];
	}
    }
    elsif ($line =~ s/DG *//)
    {
	my @val = split ' +',$line;

	my $det = $val[0];
	my $i1  = $val[1];
	my $i2  = $val[2];

	my $mean   = $val[3];
	my $mean_e = $val[4];

	# print "$val[0] $val[1] $val[2]\n";

	if ($det == $dodet)
	{
	    # print "$val[0] $val[1] $val[2]\n";

	    for (my $i = 0; $i < 8; $i++)
	    {
		insert_index($i1 * 8 + $i,.101);
		insert_index($i2 * 8 + $i + 640,.101);

	    }
	    push @DG , [ $i1, $i2, $mean, $mean_e  ];
	}

    }
    else { die "Unknown: $line"; }
}

sub insert_index($$)
{
    my $i = shift;
    my $w = shift;

    my $before = 0;

    if ($used_index{$i}) { $before = $used_index{$i}; }

    $used_index{$i} = $before + $w;
}

my @used_index = sort {$a <=> $b} keys %used_index;
my $ti = 0;

my %hash_index;

my $output = "";

foreach my $used (@used_index)
{
    if ($used_index{$used} > 3.5)
    {
	$ti++;
	$hash_index{$used} = $ti;
	$output .= "perm($ti) = $used; # $used : $used_index{$used}\n";
    }
}

print "# Indices used: $ti\n";

my $eqnno = 0;

for my $de ( @DE )
{
    my @val = @$de;

    my $i1 = $val[0];
    my $i2 = $val[1];

    my $h1;
    my $h2;

    if (($h1 = $hash_index{$i1}) &&
	($h2 = $hash_index{$i2}))
    {
	$eqnno++;

	my $rhs    = $val[2];
	my $weight = 1/$val[3];

	my $w = sprintf("%6.2f",$weight);

	$output .= "a($eqnno,$h1) = $w; a($eqnno,$h2) = -$w; b($eqnno) = $rhs * $w; # $i1,$i2 -> $h1,$h2\n";
    }
}

my $min_i1 = 1024;
my $max_i1 = 0;
my $min_i2 = 1024;
my $max_i2 = 0;

my %dg_12;

for my $dg ( @DG )
{
    my @val = @$dg;

    my $i1 = $val[0];
    my $i2 = $val[1];

    if ($i1 < $min_i1) { $min_i1 = $i1; }
    if ($i1 > $max_i1) { $max_i1 = $i1; }
    if ($i2 < $min_i2) { $min_i2 = $i2; }
    if ($i2 > $max_i2) { $max_i2 = $i2; }

    $dg_12{"$i1,$i2"} = $dg;

    my @h1;
    my @h2;

    for (my $i = 0; $i < 8; $i++)
    {
	my $h = $hash_index{$i1 * 8 + $i};
	if ($h) { push @h1,$h; }

	$h = $hash_index{$i2 * 8 + $i + 640};
	if ($h) { push @h2,$h; }
    }

    if ($#h1 > 0 && $#h2 > 0)
    {
	$eqnno++;

	my $rhs    = $val[2];
	my $weight = 1/$val[3];

	my $w  = sprintf("%6.2f",$weight);
	my $w1 = sprintf("%6.2f",$weight/($#h1+1));
	my $w2 = sprintf("%6.2f",$weight/($#h1+1));

	my @o1;
	my @o2;

	foreach my $h (@h1) { push @o1,1; }
	foreach my $h (@h2) { push @o2,1; }

	$output .= "a($eqnno,[@h1]) = -$w1 .* [@o1];\n".
	    "a($eqnno,[@h2]) = $w2 .* [@o2]; b($eqnno) = $rhs * $w; # $i1,$i2\n";
    }

}

print "a = zeros($eqnno,$ti);\n";
print "b = zeros($eqnno,1);\n";
print "perm = zeros($ti,1);\n";

print $output;

print sprintf("#    ");
for (my $i2 = $min_i2; $i2 <= $max_i2; $i2++) { print sprintf("%5d",$i2); }
print "\n";

for (my $i1 = $min_i1; $i1 <= $max_i1; $i1++)
{
    print sprintf("# %2d ",$i1);
    for (my $i2 = $min_i2; $i2 <= $max_i2; $i2++)
    {
	my $ref = $dg_12{"$i1,$i2"};

	my $mean = "     ";

	if ($ref)
	{
	    my @val  = @$ref;

	    $mean = sprintf("%5.2f",$val[2]);
	}
	print $mean;
    }
    print "\n";
}

print "d = a\\b;\n";
print "sol=zeros(1,1024);\n";
print "sol(perm)=d;\n";
print "for i=1:1024;disp(sprintf(\"sol: %3d %f\",i,sol(i)));end;\n";
