#!/bin/bash
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
# This script provides a quick summary of fabric configuration
# it uses saquery to gather the information from the SM

trap "exit 1" SIGHUP SIGTERM SIGINT

TOOLSDIR=${TOOLSDIR:-/opt/opa/tools}
BINDIR=${BINDIR:-/usr/sbin}

# optional override of defaults
if [ -f /etc/sysconfig/opa/opafastfabric.conf ]
then
	. /etc/sysconfig/opa/opafastfabric.conf
fi

. /opt/opa/tools/opafastfabric.conf.def

if [ -f $TOOLDDIR/opafastfabric.conf ]
then
	. $TOOLSDIR/opafastfabric.conf
fi
if [ -f $TOOLSDIR/ff_funcs ]
then
	. $TOOLSDIR/ff_funcs
	ff_available=y
else
	ff_available=n
fi

# if ff is not available, we can only support a subset of the capability

show_info()
{
	# $1 = hfi
	# $2 = port
	if [ "$port" -eq 0 ]
	then
		port_opts="-h $hfi"	# default port to 1st active
	else
		port_opts="-h $hfi -p $port"
	fi
	if [ "$ff_available" = y ]
	then
		echo "Fabric $hfi:$port Information:"
	else
		echo "Fabric Information:"
	fi
	$BINDIR/opasaquery $port_opts -o sminfo|grep "Guid:"|while read trash lid trash guid trash state
	do
		echo "SM:" `$BINDIR/opasaquery $port_opts -g $guid -o desc|head -1` "Guid: $guid State: $state"
	done
	echo "Number of HFIs:" `$BINDIR/opasaquery $port_opts -t fi -o nodeguid|sort -u|wc -l`
	echo "Number of HFI Ports:" `$BINDIR/opasaquery $port_opts -t fi -o desc|wc -l`
	echo "Number of Switch Chips:" `$BINDIR/opasaquery $port_opts -t sw -o nodeguid|sort -u|grep -v 'No Records Returned'|wc -l`
	echo "Number of Links:" $(($($BINDIR/opasaquery $port_opts -o link|wc -l) / 2))
	# check for any slow or 1x links
	echo "Number of slow Ports (not 25Gb/lane):" `$BINDIR/opasaquery $port_opts -o portinfo|grep 'LinkSpeed'|grep -v 'Act: 25Gb'|wc -l`
	echo "Number of degraded Ports (not at 4x):" `$BINDIR/opasaquery $port_opts -o portinfo|grep 'LinkWidthDnGrd'|grep -v 'ActTx: 4  Rx: 4'|wc -l`
	echo "-------------------------------------------------------------------------------"
}

Usage_ff_full()
{
	echo "Usage: fabric_info [-t portsfile] [-p ports]" >&2
	echo "              or" >&2
	echo "       fabric_info --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -t portsfile - file with list of local HFI ports used to access" >&2
	echo "                  fabric(s) for analysis, default is $CONFIG_DIR/opa/ports" >&2
	echo "   -p ports - list of local HFI ports used to access fabric(s) for analysis" >&2
	echo "              default is 1st active port" >&2
	echo "              This is specified as hfi:port" >&2
	echo "                 0:0 = 1st active port in system" >&2
	echo "                 0:y = port y within system" >&2
	echo "                 x:0 = 1st active port on HFI x" >&2
	echo "                 x:y = HFI x, port y" >&2
	echo "              The first HFI in the system is 1.  The first port on an HFI is 1." >&2
	echo " Environment:" >&2
	echo "   PORTS - list of ports, used in absence of -t and -p" >&2
	echo "   PORTS_FILE - file containing list of ports, used in absence of -t and -p" >&2
	echo "for example:" >&2
	echo "   fabric_info" >&2
	echo "   fabric_info -p '1:1 1:2 2:1 2:2'" >&2
	exit 0
}

Usage_ff()
{
	echo "Usage: fabric_info" >&2
	echo "              or" >&2
	echo "       fabric_info --help" >&2
	echo "   --help - produce full help text" >&2
	echo "for example:" >&2
	echo "   fabric_info" >&2
	exit 2
}

Usage_basic()
{
	echo "Usage: fabric_info" >&2
	exit 2
}

Usage()
{
	if [ "$ff_available" = y ]
	then
		Usage_ff
	else
		Usage_basic
	fi
}

if [ "$ff_available" = y  -a  x"$1" = "x--help" ]
then
	Usage_ff_full
fi

status=ok
if [ "$ff_available" = y ]
then
	while getopts p:t: param
	do
		case $param in
		p)	export PORTS="$OPTARG";;
		t)	export PORTS_FILE="$OPTARG";;
		?)	Usage;;
		esac
	done
	shift $((OPTIND -1))
fi
if [ $# -ge 1 ]
then
	Usage
fi

if [ "$ff_available" = y ]
then
	check_ports_args fabric_info
else
	PORTS='0:0'
fi

for hfi_port in $PORTS
do
	hfi=$(expr $hfi_port : '\([0-9]*\):[0-9]*')
	port=$(expr $hfi_port : '[0-9]*:\([0-9]*\)')
	if [ "$hfi" = "" -o "$port" = "" ]
	then
		echo "fabric_info: Error: Invalid port specification: $hfi_port" >&2
		status=bad
		continue
	fi

	show_info "$hfi" "$port"
done

if [ "$status" != "ok" ]
then
	exit 1
else
	exit 0
fi
