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

# This script provides a summary of multicast membership
# it uses saquery to gather the information from the SM

# optional override of defaults
if [ -f /etc/opa/opafastfabric.conf ]
then
	. /etc/opa/opafastfabric.conf
fi

. /usr/lib/opa/tools/opafastfabric.conf.def

. /usr/lib/opa/tools/ff_funcs

trap "exit 1" SIGHUP SIGTERM SIGINT

show_mc()
{
	# $1 = hfi
	# $2 = port
	# $3 = showname (y or n)
	if [ "$port" -eq 0 ]
	then
		port_opts="-h $hfi"	# default port to 1st active
	else
		port_opts="-h $hfi -p $port"
	fi
	groups=$MGID
	if [ "$groups" == "" ]; then
		echo "Fabric $hfi:$port Multicast Information:"
		groups=`/usr/sbin/opasaquery $port_opts -o mcmember 2>/dev/null | grep GID: | sed -e 's/GID: //'`
	else
		echo "Fabric $hfi:$port Multicast Information for group(s) : '$groups'"
	fi
	for mgid in $groups
	do
		/usr/sbin/opasaquery $port_opts -o mcmember -m $mgid 2> /dev/null |head -4|grep -v 'PortGid:'
		if [ "$?" != "0" ]
		then
			(>&2 echo "opasaquery error MGID '$mgid'; Port '$hfi:$port'")
			(>&2 echo "     Likely cause is non-existent, invalid, or not visible MGID: $mgid")
			continue
		fi
		if [ "$showname" = "y" ]
		then
			/usr/sbin/opasaquery $port_opts -o mcmember -m $mgid|grep 'PortGid:'|while read heading portgid rest
			do
				echo "$heading $portgid $rest"
				portguid=$(echo $portgid|cut -f 2 -d :)
				if [ $(($portguid)) -ne 0 ]
				then
					echo -n "  Name: "
					/usr/sbin/opasaquery $port_opts -o desc -g $(echo $portgid|cut -f 2 -d :)
				fi
			done
		else
			/usr/sbin/opasaquery $port_opts -o mcmember -m $mgid|grep 'PortGid:'
		fi
		echo
	done
}

Usage_full()
{
	echo "Usage: opashowmc [-v] [-t portsfile] [-p ports]" >&2
	echo "              or" >&2
	echo "       opashowmc --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -v - verbose output, show name for each member" >&2
	echo "   -m mgid - show membership of group <mgid> only" >&2
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
	echo "   opashowmc" >&2
	echo "   opashowmc -p '1:1 1:2 2:1 2:2'" >&2
	echo "   opashowmc -m 0xff12401b80010000:0x00000000ffffffff" >&2
	exit 0
}

Usage()
{
	echo "Usage: opashowmc [-v]" >&2
	echo "              or" >&2
	echo "       opashowmc --help" >&2
	echo "   -v - verbose output, show name for each member" >&2
	echo "   --help - produce full help text" >&2
	echo "for example:" >&2
	echo "   opashowmc" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

status=ok
showname=n
while getopts p:t:vm: param
do
	case $param in
	p)	export PORTS="$OPTARG";;
	t)	export PORTS_FILE="$OPTARG";;
	v)	showname=y;;
	m)	export MGID="$OPTARG";;
	?)	Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -ge 1 ]
then
	Usage
fi

check_ports_args opashowmc

for hfi_port in $PORTS
do
	hfi=$(expr $hfi_port : '\([0-9]*\):[0-9]*')
	port=$(expr $hfi_port : '[0-9]*:\([0-9]*\)')
	if [ "$hfi" = "" -o "$port" = "" ]
	then
		echo "opashowmc: Error: Invalid port specification: $hfi_port" >&2
		status=bad
		continue
	fi

	show_mc "$hfi" "$port" "$showname"
done

if [ "$status" != "ok" ]
then
	exit 1
else
	exit 0
fi
