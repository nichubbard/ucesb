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

# This script reads the main input file, given with --input=<filename>

# Within it, it locates all sections of text between:
# /* BEGIN_INCLUDE_FILE "filename" */
# ...
# until
# /* END_INCLUDE_FILE "filename" */

# It then looks up the file filename, and compares the contents.
# Basically, we want to copy the contents from the file filename into
# the input file.  But, the usage of this script is that the user most
# likely rather is editing the code snippets from within the input
# file instead, so we want to copy the contents into the file named
# filename.  To keep track of which version has changed, we use an
# md5sum of the contents (in both the file itself, and the chunk in
# the input file.  This md5sum should match the text (excluding the
# surrounding junk, and the md5sum itself.  If it does not match, then
# that copy is expected to have been changed, (is newer), and is the
# one that should be used.  If both copies have been changed, we
# declare confusion and refuse to operate further.

# The include filenames are made relative to the directory containing
# the input file.

# The use of the script is to be able to have the parser .y and lexer
# .lex files distributed in many files (although lex and bison) cannot
# handle includes.  We cannot use the C preprocessor to include the
# files, since then the error messages and line numbers will come from
# the concatenated file, and not the original source file.  This
# script is to make it possible to 'maintain' the source in several
# files, but such that all copying between them operates automatically
# (sort of).

# To insert a chunk of text the first time, write:
# /* NEW_INCLUDE_FILE "filename" */
# Where it is wanted in the input file.

# In case of double change, replacing BEGIN_INCLUDE_FILE with
# DISCARD_THIS_INCLUDE_FILE or DISCARD_FILE_INCLUDE_FILE can be used
# to tell which one should be kept.

my $inputfile = ();
my $depfile = ();

my $silentupdateinputfile = 0;

foreach my $arg (@ARGV)
{
    if ($arg =~ /^--input=(.+)$/)
    {
	$inputfile = $1;
    }
    elsif ($arg =~ /^--dependencies=(.+)$/)
    {
	$depfile = $1;
    }
    elsif ($arg =~ /^--silentupdateinputfile$/)
    {
	$silentupdateinputfile = 1;
    }
    else
    {
	die "Unrecognized option: $arg";
    }
}

$inputfile || die "No input file specified (--input=).";

use Digest::MD5  qw(md5_hex);

sub prompt($);

sub read_file($);
sub write_file($@);

sub relative_path($);

my @textin = read_file($inputfile);

if ($depfile)
{
    # We should only create a dependency list

    print "$depfile:";

    while (@textin)
    {
	my $line = shift @textin;

	if ($line =~ /^\s*\/\*\s*BEGIN_INCLUDE_FILE\s*\"(.+)\"\s*\*\/\s*$/)
	{
	    my $includefile = relative_path($1);
	    print " \\\n  $includefile";
	}
    }
    print "\n";
    exit 0; # We're done
}

my @textout = ();
my @origtext = @textin;

# Go though the file.  Copy all non-include data directly.  For each
# item found, investigate it.

while (@textin)
{
    my $line = shift @textin;

    if ($line =~ /(NEW|BEGIN|DISCARD_THIS|DISCARD_FILE)_INCLUDE_FILE/)
    {
	if (!($line =~ /^\s*\/\*\s*(NEW|BEGIN|DISCARD_THIS|DISCARD_FILE)_INCLUDE_FILE\s*\"(.+)\"\s*\*\/\s*$/))
	{ die "Unrecognized begin include: $line"; }

	my $includefile = $2;
	my $newinclude = $1;

	my @includecontent = ();
	my $includeoldmd5sum = ();

	if ($newinclude eq "NEW")
	{
	    $includeoldmd5sum = md5_hex @includecontent;
	}
	else
	{
	    my $line = shift @textin;
	    if (!($line =~ /^\s*\/\*\s*MD5SUM_INCLUDE\s*([0-9a-fA-F]+)\s*\*\/\s*$/))
	    { die "Unrecognized md5sum include: $line"; }
	    $includeoldmd5sum = $1;

	    while (@textin)
	    {
		$line = shift @textin;
		if ($line =~ /END_INCLUDE_FILE/) { last; }
		push @includecontent,$line;
	    }
	    if (!($line =~ /^\s*\/\*\s*END_INCLUDE_FILE\s*\"(.+)\"\s*\*\/\s*$/))
	    { die "Unrecognized end include: $line"; }
	    if ($1 ne $includefile)
	    { die "begin/end include filename mismatch: $1 ne $includefile"; }
	}

	# So, we have found a chunk of text.  $includefile,
	# $includeoldmd5sum, @includecontent

	# First, we calculate the correct md5sum

	my $includemd5sum = md5_hex @includecontent;

	# print "Include: $includefile ($includeoldmd5sum -> $includemd5sum)\n";

	# Then we try to read the file indicated

	my @incf_content = read_file(relative_path($includefile));
	# Extract the (old) md5sum from the file
	my $incf_md5sum = md5_hex @incf_content;

	# print "Include: ----> ($includeoldmd5sum -> $incf_md5sum)\n";

	# Now, there are four possibilites.
	# None changed -> nothing to do (except to copy to output)
	# Chunk changed -> Copy to include file
	# Include file changed -> Copy to us
	# Both changed -> nervous breakdown.

	my $chunkchange = $includeoldmd5sum ne $includemd5sum;
	my $filechange  = $includeoldmd5sum ne $incf_md5sum;

	if ($chunkchange && $filechange &&
	    ($includemd5sum ne $incf_md5sum))
	{
	    if ($newinclude eq "DISCARD_THIS")
	    {
		# Discard changes in this chunk
		$chunkchange = 0;
	    }
	    elsif ($newinclude eq "DISCARD_FILE")
	    {
		# Discard changes in the file
		$filechange = 0;
	    }
	    else
	    {
		die "Both chunk ($includeoldmd5sum -> $includemd5sum) and file ( -> $incf_md5sum) changed for $includefile.  Manual intervention!!!  Or use DISCARD_THIS_INCLUDE_FILE or DISCARD_FILE_INCLUDE_FILE.";
	    }
	}

	if ($filechange)
	{
	    # We should incorporate the changes from the include file.  Easy:

	    $includemd5sum  = $incf_md5sum;
	    @includecontent = @incf_content;
	}
	if ($chunkchange &&
	    ($includemd5sum ne $incf_md5sum))
	{
	    # We should write the new content to the include file
	    @incf_content = @includecontent;

	    my $relincfile = relative_path($includefile);

	    write_file("${relincfile}.incnew",@incf_content);

	    my @args = ("diff","-u","${relincfile}","${relincfile}.incnew");
	    system(@args);

	    prompt("I'd like to update $relincfile? ");

	    @args = ("mv","${relincfile}.incnew","${relincfile}");
	    system(@args);
	}

	# We survived, so we are to write the information to our buffer

	# Since flex has major problems with comments starting in the
	# first column in the rules section, we indent our information.

	push @textout," /* BEGIN_INCLUDE_FILE \"$includefile\" */\n";
	push @textout," /* MD5SUM_INCLUDE $includemd5sum */\n";
	push @textout,@includecontent;
	push @textout," /* END_INCLUDE_FILE \"$includefile\" */\n";
    }
    elsif ($line =~ /END_INCLUDE_FILE|MD5SUM_INCLUDE/)
    {
	# We found something we would not expect, complain
	die "Unexpected include directive: $line";
    }
    else
    {
	push @textout,$line;
    }
}

# And finally, if the text changed, then we write it.

my $differ = ($#origtext != $#textout);

for (my $i = 0; $i <= $#origtext && $i <= $#textout; $i++)
{
    if ($origtext[$i] ne $textout[$i]) { $ differ = 1; }
}

if ($differ)
{
    write_file("${inputfile}.incnew",@textout);

    if (!$silentupdateinputfile)
    {
	my @args = ("diff","-u","${inputfile}","${inputfile}.incnew");
	system(@args);

	prompt("I'd like to update $inputfile? ");
    }

    my @args = ("mv","${inputfile}.incnew","${inputfile}");
    system(@args);
}

# We are done!

sub prompt($)
{
    my ($text) = @_;
    print STDERR "\n";
    print STDERR "(If compiling from within EMACS, you likely have to abort\n";
    print STDERR "and redo from the shell, to be able to answer.)\n";
    print STDERR "\n";
    print STDERR "$text [y/n]";
    my $answer = <STDIN>;
    chomp $answer;
    if ($answer ne "Y" && $answer ne "y")
    {
	die "Stopped by user.";
    }
}

sub read_file($)
{
    my ($filename) = @_;
    open INPUT, "< $filename" or die "Can't open $filename : $!";
    my @content = <INPUT>;
    close INPUT;
    return @content;
}

sub write_file($@)
{
    my ($filename,@content) = @_;
    open OUTPUT, "> $filename" or die "Can't open $filename : $!";
    print OUTPUT @content;
    close OUTPUT;
}

sub relative_path($)
{
    my ($filename) = @_;
    if ($filename =~ /^\//) { return $filename; } # begins with a slash
    my $input_path = "";
    if ($inputfile =~ /(.*\/)/) { $input_path = $1; }
    return $input_path.$filename;
}
