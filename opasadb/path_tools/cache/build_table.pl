#!/usr/bin/perl
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015, Intel Corporation
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# END_ICS_COPYRIGHT8   ****************************************

#[ICS VERSION STRING: unknown]
#
# To use this tool:
#
# /sbin/opasaquery -o path | ./build_table.pl >gidtable
#

use strict;
use Data::Dumper;

sub trim($)
{
    my $scratch = @_[0];

    $scratch =~ s/^\s+//;
    $scratch =~ s/\s+$//;

    return $scratch;
}

my $done;
until ( $done ) {
	my $line;
	my @arg;
	my @gid;
	my @lid;
	my $pkey;

	$line = readline STDIN;
	until ( $line =~ /SGID/ || eof) {
		$line = readline STDIN;
	}
	chomp;
	$line = trim($line);
	$line =~ s/0x/ /g;
	($arg[0], $gid[0], $gid[1]) = split(/:/, $line);

	$line = readline STDIN;
	until ( $line =~ /DGID/ || eof) {
		$line = readline STDIN;
	}
	chomp;
	$line = trim($line);
	$line =~ s/0x/ /g;
	($arg[0], $gid[2], $gid[3]) = split(/:/, $line);

	$line = readline STDIN;
	until ( $line =~ /LID/ || eof) {
		$line = readline STDIN;
	}
	chomp;
	$line = trim($line);
	($arg[0], $lid[0], $arg[1], $lid[1], 
		$arg[2], $arg[4], $arg[4], $pkey) = split(/ /, $line);

	$done = eof;
	print "$gid[0]$gid[1]:$gid[2]$gid[3]:$lid[0]:$lid[1]:$pkey\n" if ( ! $done)

}
