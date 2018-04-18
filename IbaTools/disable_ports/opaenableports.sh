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

# reenable the specified set of ports

# optional override of defaults
if [ -f /etc/opa/opafastfabric.conf ]
then
	. /etc/opa/opafastfabric.conf
fi

. /usr/lib/opa/tools/opafastfabric.conf.def

. /usr/lib/opa/tools/ff_funcs

tempfile="$(mktemp)"
lidmap="$(mktemp --tmpdir lidmapXXXXXX)"
trap "rm -f $tempfile; rm -f $lidmap; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $tempfile; rm -f $lidmap" EXIT

Usage_full()
{
	echo "Usage: opaenableports [-h hfi] [-p port] < disabled.csv" >&2
	echo "              or" >&2
	echo "       opaenableports --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -h hfi - hfi, numbered 1..n, 0= -p port will be a system wide port num" >&2
	echo "            (default is 0)" >&2
	echo "   -p port - port, numbered 1..n, 0=1st active (default is 1st active)" >&2
	echo  >&2
	echo "The -h and -p options permit a variety of selections:" >&2
	echo "   -h 0       - 1st active port in system (this is the default)" >&2
	echo "   -h 0 -p 0  - 1st active port in system" >&2
	echo "   -h x       - 1st active port on HFI x" >&2
	echo "   -h x -p 0  - 1st active port on HFI x" >&2
	echo "   -h 0 -p y  - port y within system (irrespective of which ports are active)" >&2
	echo "   -h x -p y  - HFI x, port y" >&2
	echo  >&2
	echo "disabled.csv is an input file listing the ports to enable." >&2
	echo "It is of the form:" >&2
	echo "   NodeGUID;PortNum;NodeType;NodeDesc;Ignored" >&2
	echo "An input file such as this is generated in $CONFIG_DIR/opa/disabled*.csv" >&2
	echo "by opadisableports." >&2
	echo "for example:" >&2
	echo "   opaenableports < disabled.csv" >&2
	echo "   opaenableports < $CONFIG_DIR/opa/disabled:1:1.csv" >&2
	echo "   opaenableports -h 1 -p 1 < disabled.csv" >&2
	exit 0
}

Usage()
{
	echo "Usage: opaenableports [-h hfi] [-p port] < disabled.csv" >&2
	echo "              or" >&2
	echo "       opaenableports --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -h hfi - hfi, numbered 1..n, 0= -p port will be a system wide port num" >&2
	echo "            (default is 0)" >&2
	echo "   -p port - port, numbered 1..n, 0=1st active (default is 1st active)" >&2
	echo  >&2
	echo "disabled.csv is a file listing the ports to enable." >&2
	echo "It is of the form:" >&2
	echo "   NodeGUID;PortNum;NodeType;NodeDesc;Ignored" >&2
	echo "An input file such as this is generated in $CONFIG_DIR/opa/disabled*.csv" >&2
	echo "by opadisableports." >&2
	echo "for example:" >&2
	echo "   opaenableports < disabled.csv" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

res=0
hfi_input=0
port_input=0

while getopts h:p: param
do
	case $param in
	p)	port_input="$OPTARG";;
	h)	hfi_input="$OPTARG";;
	?)	Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -ge 1 ]
then
	Usage
fi

/usr/sbin/oparesolvehfiport $hfi_input $port_input &>/dev/null
if [ $? -ne 0 -o "$hfi_input" = "" -o "$port_input" = "" ]
then
	echo "opaenableports: Error: Invalid port specification: $hfi_input:$port_input" >&2
	exit 1
fi

hfi=$(/usr/sbin/oparesolvehfiport $hfi_input $port_input -o hfinum)
port=`echo $(/usr/sbin/oparesolvehfiport $hfi_input $port_input | cut -f 2 -d ':')`
port_opts="-h $hfi -p $port"

lookup_lid()
{
	local nodeguid="$1"
	local portnum="$2"
	local guid dport type desc lid

	grep "^$nodeguid;$portnum;" < $lidmap|while read guid dport type desc lid
	do
		echo -n $lid
	done
}
	

enable_ports()
{
	enabled=0
	failed=0
	skipped=0
	suffix=":$hfi:$port"

	# generate lidmap
	/usr/sbin/opaextractlids $port_opts > $lidmap
	if [ ! -s $lidmap ]
	then
		echo "opaenableports: Unable to determine fabric lids" >&2
		rm -f $lidmap
		return 1
	fi

	IFS=';'
	while read nodeguid dport type desc rest
	do
		lid=$(lookup_lid $nodeguid 0)
		if [ x"$lid" = x ]
		then
			echo "Skipping port: $desc:$dport: Device not found in fabric"
			skipped=$(( $skipped + 1))
		else
			echo "Enabling port: $desc:$dport"
			eval /usr/sbin/opaportconfig $port_opts -l $lid -m $dport enable

			if [ $? = 0 ]
			then
				logger -p user.err "Enabled port: $desc:$dport"
				if [ -e $CONFIG_DIR/opa/disabled$suffix.csv ]
				then
					# remove from disabled ports
					grep -v "^$nodeguid;$dport;" < $CONFIG_DIR/opa/disabled$suffix.csv > $tempfile
					mv $tempfile $CONFIG_DIR/opa/disabled$suffix.csv
				fi
				enabled=$(( $enabled + 1))
			else
				failed=$(( $failed + 1))
			fi
		fi
	done
	if [ $skipped -ne 0 ]
	then
		echo "For Skipped ports, either the device is now offline or the other end of the"
		echo "link was disabled and the device is no longer accessible in-band."
		echo "The end of the link previously disabled by opedisableports or opadisablehosts"
		echo "can be found in $CONFIG_DIR/opa/disabled$suffix.csv"
	fi
	if [ $failed -eq 0 ]
	then
		echo "Enabled: $enabled; Skipped: $skipped"
		return 0
	else
		echo "Enabled: $enabled; Skipped: $skipped; Failed: $failed"
		return 1
	fi
}

enable_ports "$hfi" "$port"
if [ $? -ne 0 ]
then
	res=1
fi

exit $res
