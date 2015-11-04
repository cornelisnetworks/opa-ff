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

# [ICS VERSION STRING: unknown]

use Socket;
use Getopt::Long;

sub broadcast_addr()
{
	my($address, $netmask) = @_;
	my($tempaddress);
	my($tempmask);
	my($rval);

	# compute computational address
	$tempaddress = inet_aton($address);
	if (!defined($tempaddress))
	{
		return undef;
	}
	$tempaddress = unpack("N",$tempaddress);

	# compute computational netmask
	$tempmask = inet_aton($netmask);
	if (!defined($tempmask))
	{
		return undef;
	}
	$tempmask = unpack("N",$tempmask);

	$rval = $tempmask & $tempaddress;
	$rval |= (~$tempmask & 0xfffffff);
	$rval = pack("N",$rval);
	$rval = inet_ntoa($rval);
	return $rval;
}

sub default_netmask()
{
	my($address) = @_;
	my($tempaddress);
	my($mask);
	my($rval);

	$tempaddress = inet_aton($address);
	if (!defined($tempaddress))
	{
		return undef;
	}
	$tempaddress = unpack("N",$tempaddress);
	if (($tempaddress & 0x80000000) == 0x00000000) {
		$mask = 0xff000000;
	} elsif (($tempaddress & 0x40000000) == 0x00000000) {
		$mask = 0xffff0000;
	} else {
		$mask = 0xffffff00;
	}
	$rval = pack("N",$mask);
	$rval = inet_ntoa($rval);
	return $rval;
}

sub network_number()
{
	my($address, $netmask) = @_;
	my($tempinetaddr);
	my($tempmask);
	my($rval);

	$tempinetaddr = inet_aton($address);
	if (!defined($tempinetaddr))
	{
		return undef;
	}
	$tempinetaddr = unpack("N",$tempinetaddr);

	# compute computational netmask
	$tempmask = inet_aton($netmask);
	if (!defined($tempmask))
	{
		return undef;
	}
	$tempmask = unpack("N",$tempmask);
	$tempinetaddr &= $tempmask;
	$rval = pack("N",$tempinetaddr);
	$rval = inet_ntoa($rval);
	return $rval;
}

sub print_usage() {
	print STDERR
"Usage: opaipcalc [OPTION...] ip_address [netmask]\n", 
"  -b, --broadcast     Display calculated broadcast address\n",
"  -m, --netmask       Display default netmask for IP (class A, B, or C)\n",
"  -n, --network       Display network address\n",
"Help options:\n",
"  -?, --help          Show this help message\n",
}

$ret = &GetOptions(\%longopts, 'broadcast|b', 'netmask|m', 'network|n');
if (!$ret || ($#ARGV < 0 || $#ARGV > 1))
{
	&print_usage;
	exit 1;
}

if ($#ARGV == 0) {
	if ($ARGV[0] =~ /([\d.]+)\/([\d.]+)/) {
		$address = $1;
		$mask = $2;
	} else {
		$address = $ARGV[0];
	}
} else {
	$address = $ARGV[0];
	$mask = $ARGV[1];
}

$mask = &default_netmask($address) unless defined($mask);
$network = &network_number($address,$mask);
if (!defined($network))
{
	print STDERR "Invalid address or mask\n";
	&print_usage();
	exit 1;
}
$broadcast = &broadcast_addr($address,$mask);
if (!defined($broadcast))
{
	print STDERR "Invalid address or mask\n";
	&print_usage();
	exit 1;
}

$usage=1;
foreach $option (sort keys %longopts) {
	$usage=0;
	SWITCH: {
		print ('NETWORK=', &network_number($address,$mask), "\n"), last SWITCH if $option eq 'network';
		print ('BROADCAST=', &broadcast_addr($address,$mask),"\n"), last SWITCH if $option eq 'broadcast';
		print ('NETMASK=', $mask,"\n"),  last SWITCH if $option eq 'netmask';
	}
}
if ($usage) {
	&print_usage;
	exit 1;
}

exit 0;

