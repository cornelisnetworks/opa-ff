#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2017, Intel Corporation
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

if [ -f /usr/lib/opa/tools/ff_funcs ]
then
	# optional override of defaults
	if [ -f /etc/opa/opafastfabric.conf ]
	then
		. /etc/opa/opafastfabric.conf
	fi

	. /usr/lib/opa/tools/opafastfabric.conf.def

	. /usr/lib/opa/tools/ff_funcs
	ff_available=y
else
	ff_available=n
fi

trap "exit 1" SIGHUP SIGTERM SIGINT

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
	echo "Fabric $hfi:$port Information:"
	if /usr/sbin/opasmaquery $port_opts -o pkey 2>/dev/null|grep 0xffff >/dev/null 2>&1
	then
		# Mgmt Node
		tmp=`/usr/sbin/opasaquery $port_opts -o sminfo`
		if [ "$?" -ne 0 ]
		then
			echo "ERROR: opasaquery failed"
			return 1
		fi
		echo "$tmp" | grep "Guid:"|while read trash lid trash guid trash state
		do
			tmp=`/usr/sbin/opasaquery $port_opts -g $guid -o desc`
			if [ "$?" -ne 0 ]
			then
				echo "ERROR: opasaquery failed"
				return 1
			fi
			tmp=`echo "$tmp"|head -1`
			echo "SM: $tmp Guid: $guid State: $state"
		done
		/usr/sbin/opasaquery $port_opts -o fabricinfo
		if [ "$?" -ne 0 ]
		then
			echo "ERROR: opasaquery failed"
			return 1
		fi
	else
		# Non-Mgmt Node
		tmp=`/usr/sbin/opaportinfo $port_opts`
		if [ "$?" -ne 0 ]
		then
			echo "ERROR: opaportinfo failed"
			return 1
		fi
		echo "$tmp" | grep "SMLID:"|while read trash lid trash smlid
		do
			tmp=`/usr/sbin/opasaquery $port_opts -l $smlid -o desc`
			if [ "$?" -ne 0 ]
			then
				echo "ERROR: opasaquery failed"
				return 1
			fi
			tmp=`echo "$tmp" | head -1`
			echo "Master SM: $tmp" 
		done

		tmp=`/usr/sbin/opasaquery $port_opts -t fi -o nodeguid`
		if [ "$?" -ne 0 ]
		then
			echo "ERROR: opasaquery failed"
			return 1
		fi
		tmp=`echo "$tmp" | sort -u | wc -l`
		echo "Number of HFIs: $tmp"
		tmp=`/usr/sbin/opasaquery $port_opts -t fi -o desc`
		if [ "$?" -ne 0 ]
		then
			echo "ERROR: opasaquery failed"
			return 1
		fi
		tmp=`echo "$tmp" | wc -l`
	 	echo "Number of HFI Ports: $tmp"
	fi
	echo "-------------------------------------------------------------------------------"
}

Usage_ff_full()
{
	echo "Usage: opafabricinfo [-t portsfile] [-p ports]" >&2
	echo "              or" >&2
	echo "       opafabricinfo --help" >&2
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
	echo "   opafabricinfo" >&2
	echo "   opafabricinfo -p '1:1 1:2 2:1 2:2'" >&2
	exit 0
}

Usage_basic_full()
{
	echo "Usage: opafabricinfo [-p ports]" >&2
	echo "              or" >&2
	echo "       opafabricinfo --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -p ports - list of local HFI ports used to access fabric(s) for analysis" >&2
	echo "              default is 1st active port" >&2
	echo "              This is specified as hfi:port" >&2
	echo "                 0:0 = 1st active port in system" >&2
	echo "                 0:y = port y within system" >&2
	echo "                 x:0 = 1st active port on HFI x" >&2
	echo "                 x:y = HFI x, port y" >&2
	echo "              The first HFI in the system is 1.  The first port on an HFI is 1." >&2
	echo " Environment:" >&2
	echo "   PORTS - list of ports, used in absence of -p" >&2
	echo "for example:" >&2
	echo "   opafabricinfo" >&2
	echo "   opafabricinfo -p '1:1 1:2 2:1 2:2'" >&2
	exit 0
}

Usage_ff()
{
	echo "Usage: opafabricinfo" >&2
	echo "              or" >&2
	echo "       opafabricinfo --help" >&2
	echo "   --help - produce full help text" >&2
	echo "for example:" >&2
	echo "   opafabricinfo" >&2
	exit 2
}

Usage_basic()
{
	Usage_ff
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

if [ x"$1" = "x--help" ]
then
	if [ "$ff_available" = y ]
	then
		Usage_ff_full
	else
		Usage_basic_full
	fi
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
else
	while getopts p: param
	do
		case $param in
		p)	export PORTS="$OPTARG";;
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
	check_ports_args opafabricinfo
else
	if [ "x$PORTS" = "x" ]
	then
		PORTS='0:0'
	fi
fi

for hfi_port in $PORTS
do
	hfi=$(expr $hfi_port : '\([0-9]*\):[0-9]*')
	port=$(expr $hfi_port : '[0-9]*:\([0-9]*\)')
	if [ "$hfi" = "" -o "$port" = "" ]
	then
		echo "opafabricinfo: Error: Invalid port specification: $hfi_port" >&2
		status=bad
		continue
	fi

	show_info "$hfi" "$port"
	if [ $? -ne 0 ]
	then
		status=bad
	fi
done

if [ "$status" != "ok" ]
then
	exit 1
else
	exit 0
fi
