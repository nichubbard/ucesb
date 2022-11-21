#!/usr/bin/perl

# This file is part of TDCPM - time dependent calibration parameters.
#
# Copyright (C) 2017  Haakan T. Johansson  <f96hajo@chalmers.se>
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

########################################################################
# Subroutines

sub find_structure_items();
sub create_struct_items_decl($$);
sub create_struct_items($$);
sub create_struct_items_void($$);

########################################################################

my $struct = (); # Name of the structure, without ..._t
my $header = 0;

foreach my $arg (@ARGV)
{
    if($arg =~ /^--struct=([a-zA-Z0-9_]+)$/)
    {
	$struct = $1;
    }
    elsif($arg =~ /^--header$/)
    {
	$header = 1;
    }
    else
    {
	die "Unhandled argument: $arg";
    }
}

# if (!$struct) { die "Structure name not specified"; }

########################################################################
# Read the input

my $fullinput = do { local $/; <STDIN> };

# Strip comments, well, simple ones.  Comment markers in strings
# will trip it over, but ... do not run this on such code then!

$fullinput =~ s,/\*.*?\*/,,sg;
$fullinput =~ s,//.*,,g;

# Remove preprocessor directives

$fullinput =~ s,#.*,,g;

# Turn the input into one long line.  Simplifies further parsing.

$fullinput =~ s,\n, ,sg;

# print $fullinput;

########################################################################
# Find all structures.

my %structs = ();
my @structs = ();

find_structure_items();

########################################################################

my $define_name = "__GEN_"."STRUCT"."_ITEMS_H__";

print <<"EndOfText";
/***********************************************************************
 *
 * This file is autogenerated by $0
 *
 * Editing is useless.
 *
 **********************************************************************/

/* The headers declaring '...' (TODO: insert structure names!)
 * must be included before this file!
 */

EndOfText

if ($header) { print <<"EndOfText";
\#ifndef $define_name
\#define $define_name

\#include <stddef.h>

EndOfText
} else { print <<"EndOfText";
\#include "tdcpm_struct_info.h"

EndOfText
}

print <<"EndOfText";
/**********************************************************************/

void tdcpm_declare_struct()
{
EndOfText

########################################################################

foreach $struct (@structs)
{
    create_struct_items_decl($struct, $structs{$struct});
}

foreach $struct (@structs)
{
    create_struct_items($struct, $structs{$struct});
    create_struct_items_void($struct, $structs{$struct});
}

########################################################################

print <<"EndOfText";
}

/**********************************************************************/
EndOfText

if ($header) { print <<"EndOfText";

\#endif/*$define_name*/
EndOfText
}

exit 0;

########################################################################

sub parse_structure_item($$)
{
    my $item = shift;
    my $username = shift;

    if ($item =~ /^\s*$/) { }
    elsif ($item =~ /^\s*LWROC_MSG_HEADER\(\(\s*$/) {
    }
    elsif ($item =~ /^\s*LWROC_MSG_FOOTER\(\(\s*$/) {
    }
    elsif ($item =~ /
    ^\s*                                                   # beginning of line
    (((uint32_t|
       double)|
      ([_A-Za-z][_A-Za-z0-9]*))                            # item type
     \s+
    )
    ([_A-Za-z][_A-Za-z0-9]*)                               # item name
    (\s*\[\s*([_A-Za-z0-9\]\[\s]*?)
     \s*\]
    )?                                                     # array len
    \s*(TDCPM_UNIT\s*\(\s*\"([^\".]*)\"\s*\)\s*)?
    \s*$                                                   # end of line
    /x)
    {
	my $plain_type = $3;
	my $struct_type = $4;
	my $name = $5;
	my $arraylen_str = $7;
	my $unit = $9;

	if (!defined($username)) {
	    $username = $name;
	}

	if ($plain_type) {
	    # TODO: Needed?
	    $plain_type =~ s/const\s*//g;
	    $plain_type =~ s/\s//g;
	}

	my @arraylens = ();

	if ($arraylen_str)
	{
	    my @raw_arraylens = split /\s*\]\[\s*/, $arraylen_str;

	    foreach my $arraylen (@raw_arraylens)
	    {
		# print "[$arraylen]\n";

		if (!($arraylen =~ /
    ^                                                      # beginning of line
    ([0-9]+|                                               # number
     [_A-Za-z][_A-Za-z0-9]*                                # some macro name
    )
    $                                                      # end of line
    /x)) {
		    die "Malformed array length '$arraylen' for ".
			"member '$name' in '$struct'.";
		}

		push @arraylens, $arraylen;
	    }
	}

	if (defined($plain_type) && $plain_type eq "unhandled_item") {
	    print "/* TODO: remove unhandled_item items... */\n";
	} else {
	    my $type, my $typem;
	    my $type_struct;

	    if ($plain_type)
	    {
		$typem = $type = $plain_type; # mangled name
		$type_struct = $type;
	    }
	    if ($struct_type)
	    {
		$typem = $type = $struct_type;
		if ($typem =~ s/struct\s+(.)_t/$1/) {
		    print "xx\n";
		}
		$type_struct = "struct";
	    }

	    my $rec = {
		TYPE        => $type,
		TYPEM       => $typem,
		TYPE_STRUCT => $type_struct,
		NAME        => $name,
		USERNAME    => $username,
		ARRAYLENS   => \@arraylens,
		UNIT        => $unit,
	    };

	    return $rec;
	}
    }
    else { die "Malformed item: '$item'."; }

    return ();
}

########################################################################

sub find_structure_items()
{
    my @globals = ();

    # TODO: Extract until the ending matched brace, such that
    # (anonymous) substructures can be supported.

    while ($fullinput =~ s/(\s|^)TDCPM_STRUCT_DEF(\s+typedef)?\s+struct\s+([_A-Za-z][_A-Za-z0-9]*?)(_t)?\s+{(.*?)}//) {
	my $struct  = $3;
	my $content = $5;
	my @content = split /;|\)\)/,$content;

	my @items = ();

	# print "Found struct: $struct\n";

	if ($structs{$struct}) {
	    die "Multiple definitions of structure '$struct'."; }

	foreach my $item (@content)
	{
	    my $rec = parse_structure_item($item,());

	    if (defined($rec)) {
		push @items,$rec;
	    }
	}

	$structs{$struct} = \@items;
	push @structs, $struct;
    }

    while ($fullinput =~ s/(\s|^)TDCPM_STRUCT_INST\s*\(([_A-Za-z][_A-Za-z0-9]*?)\)\s+([^;.]+);//)
    {
	my $name     = $2;
	my $instance = $3;

	my $rec = parse_structure_item($instance,$name);

	if (defined($rec)) {
	    push @globals,$rec;
	}

	# print "Found instance: $instance\n";
    }

    my $struct = ".global";

    $structs{$struct} = \@globals;
    push @structs, $struct;
}

########################################################################

sub create_struct_items_decl($$)
{
    my $struct = shift;
    my $items_ref = shift;

    my $hasstrings = 0;

    my $li_struct = "li_$struct";

    if ($struct eq ".global") {
	$li_struct = "gi";
    } else {
	print "  tdcpm_struct_info *$li_struct;\n";
    }

    foreach my $item (@$items_ref)
    {
	my $li_item = "${li_struct}_$item->{NAME}";

	print "  tdcpm_struct_info_item *$li_item;\n";
    }
    print "\n";
}

sub create_struct_items($$)
{
    my $struct = shift;
    my $items_ref = shift;

    my $hasstrings = 0;

    my $li_struct = "li_$struct";

    if ($struct eq ".global") {
	$li_struct = "gi";
    } else {
	print "  $li_struct = TDCPM_STRUCT($struct, \"$struct\");\n\n";
    }

    foreach my $item (@$items_ref)
    {
	my $uctypem = uc($item->{TYPE_STRUCT});

	if ($item->{TYPE_STRUCT} eq "struct") {

	}

	my $li_item = "${li_struct}_$item->{NAME}";

	my $strname = $item->{USERNAME};
	my $unit = $item->{UNIT};

	$strname =~ s/^_//;
	if (!defined($unit)) { $unit = ""; }

	if ($struct eq ".global") {
	    print "  $li_item = TDCPM_STRUCT_INSTANCE(";
	    # print "$item->{TYPEM}, ";
	} else {
	    print "  $li_item = TDCPM_STRUCT_ITEM_$uctypem(";
	    print "$li_struct, $struct, ";
	}
	print "$item->{NAME}, \"$strname\"";
	if ($item->{TYPE_STRUCT} eq "struct") {
	    my $li_subitem = "li_$item->{TYPEM}";
	    print ", $li_subitem";
	} else {
	    print ", \"$unit\"";
	}
	print ");\n";

	my $arraylens_ref = $item->{ARRAYLENS};
	my $subarray = "";

	foreach my $arraylen (@$arraylens_ref)
	{
	    my $spacer = $li_item;
	    $spacer =~ s/./ /g;
	    $spacer =~ s/  //;
	    if ($struct eq ".global") {
		print "  /*$spacer*/ TDCPM_STRUCT_INSTANCE_ARRAY".
		    "($li_item, $item->{NAME}$subarray);\n";
	    } else {
		print "  /*$spacer*/ TDCPM_STRUCT_ITEM_ARRAY".
		    "($li_item, $struct, $item->{NAME}$subarray);\n";
	    }
	    $subarray .= "[0]";
	}
    }
    print "\n";
}

sub create_struct_items_void($$)
{
    my $struct = shift;
    my $items_ref = shift;

    my $hasstrings = 0;

    my $li_struct = "li_$struct";

    if ($struct eq ".global") {
	$li_struct = "gi";
    } else {
	print "  (void) $li_struct;\n";
    }

    foreach my $item (@$items_ref)
    {
	my $li_item = "${li_struct}_$item->{NAME}";

	print "  (void) $li_item;\n";
    }
    print "\n";
}

########################################################################
