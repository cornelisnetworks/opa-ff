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
# This tool must be run as root. 
# 
# It builds a table of fabric simulator HCA nodes, then passes them
# to opp_exercise.
#
# All command line arguments are simply passed to opp_exercise.
#
# /sbin/opareport -o lids -F nodetype:CA | ./build_table.pl >guidtable
#

use strict;
use Sys::Hostname;
use Data::Dumper;
use Getopt::Long;
use Cwd;

# Don't change these.
my $cwd = &Cwd::cwd();
my $report_opts = '';
my $ca_extract_opts = '-H -s SMs -s Switches -e LID -e PortGUID -e NodeDesc';
my $sm_extract_opts = '-H -e SM.PortGUID';
my $guid_table = "guidtable_$$";

sub trim($)
{
    my $scratch = @_[0];

    $scratch =~ s/^\s+//;
    $scratch =~ s/\s+$//;

    return $scratch;
}

my $args='';
my $mode_opt='';
my $simulator='';
my $duration=600;
my @sid= ();
 
my %mode = (
	"baseline" => "-x 0 -X 0 -D 0 -s 0",
	"porttoggle" => "-x 10 -X 0 -D 20",
	"smfailover" => "-x 10 -X 0 -D 20",
	"stresstest" => "-x 10 -X 60 -D 20"
);

GetOptions(
	"duration=i" => \$duration,
	"mode=s" => \$mode_opt, 
	"fsim=s" => \$simulator,
	"sid=s" => \@sid);

if (!($mode{$mode_opt})) {
	my $m;
	print "You must specify a mode.\n";
	print "Valid Modes are:\n";
	foreach $m (%mode) {
		print "$m\n";
	}
	die;
}

$args = "$args " . $mode{$mode_opt};

my $sidi;
foreach $sidi (@sid) {
	$args = "$args -S $sidi";
}

if ($mode_opt ne "baseline") {
	$args = "$args -s $duration";
}

my $host = hostname;
($host) = split(/\./,$host);

# We're playing a game here. We extract a list of the SMs and source ports 
# then use grep to remove them from the list of valid destination ports. 
# This prevents the tool from turning off the SM's ports.
open(LOCAL_FD,"/usr/sbin/opainfo -o info | grep GUID: |") || die "Could not get portinfo.";
open(OUTPUT_FD,">sms.$$") || die "Could not open output file.";

until ( eof LOCAL_FD ) {
	my $line;
	my $guid;
	my $dummy;

	$line = readline LOCAL_FD;
	# Subnet: 0xfe80000000000000  GUID: 0x00117500005a6e8a  GUID Cap:     5
	$line = trim($line);
	($dummy, $dummy, $dummy, $guid) = split(/ +/, $line);
	
	print OUTPUT_FD "$guid\n";
}
close(LOCAL_FD);
close(OUTPUT_FD);

system "opareport -x $report_opts >$$" || die "Failed to generate opareport.";
system "opaxmlextract $sm_extract_opts <$$ >>sms.$$" || die "Failed to generate list of SMs.\n";
system "opaxmlextract $ca_extract_opts <$$ | grep -v -f sms.$$ >$guid_table" || die "Failed to generate list of destinations.\n";

unlink "$$";
unlink "sms.$$";

my $arg;

foreach $arg (@ARGV) {
	$args = "$args $arg";
}

#system "cat $guid_table";
print "$cwd/opa_osd_exercise $args $guid_table\n";
system "$cwd/opa_osd_exercise $args $guid_table";
unlink $guid_table;
