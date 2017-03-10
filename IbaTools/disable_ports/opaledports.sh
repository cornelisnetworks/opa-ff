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

# optional override of defaults
if [ -f /etc/opa/opafastfabric.conf ]
then
	. /etc/opa/opafastfabric.conf
fi

. /usr/lib/opa/tools/opafastfabric.conf.def

. /usr/lib/opa/tools/ff_funcs

#Temporary lidmap setup
lidmap="$(mktemp --tmpdir lidmapXXXXXX)"
trap "rm -f $lidmap; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $lidmap" EXIT

Usage_full()
{
	echo "Usage: opaledports [-h hfi] [-p port] [-s|-d] [on|off] < portlist.csv" >&2
	echo "              or" >&2
	echo "       opaledports [-h hfi] [-p port] [-C]" >&2
	echo "              or" >&2
	echo "       opaledports --help" >&2
	echo  >&2
	echo "   --help - produce full help text" >&2
	echo "   -h hfi - hfi, numbered 1..n, 0= -p port will be a system wide port num" >&2
	echo "            (default is 0)" >&2 
	echo "   -p port - port, numbered 1..n, 0=1st active (default is 1st active)" >&2
	echo "   -s - Affect source side (first node) of link only." >&2
	echo "   -d - Affect dest side (second node) of link only." >&2
	echo "   -C - This option clears beaconing LED on all ports.">&2
	echo "   on|off - action to perform" >&2
	echo "            on - Turns on beaconing LED" >&2
	echo "            off - Turns off beaconing LED" >&2
	echo  >&2
	echo "portlist.csv is a file listing the links to process" >&2
	echo "It is of the form:" >&2
	echo "  NodeGUID;PortNum;NodeType;NodeDesc;NodeGUID;PortNum;NodeType;NodeDesc;Dontcare" >&2
	echo >&2
	echo "for example:" >&2
	echo "   echo \"0x001175010165ac1d;1;FI;phkpstl035 hfi1_0\"|opaledports on" >&2
	echo "   opaledports on < portlist.csv" >&2
	echo "   opaextractsellinks -F led:on | opaledports off" >&2
	echo "   opaledports -C" >&2
	exit 0
}

Usage()
{
	echo "Usage: opaledports [-h hfi] [-p port] [-s|-d] [on|off] < portlist.csv" >&2
	echo "              or" >&2
	echo "       opaledports [-h hfi] [-p port] [-C]" >&2
	echo "              or" >&2
	echo "       opaledports --help" >&2
	echo  >&2
	echo "   --help - produce full help text" >&2
	echo "   -h hfi - hfi, numbered 1..n, 0= -p port will be a system wide port num" >&2
	echo "            (default is 0)" >&2 
	echo "   -p port - port, numbered 1..n, 0=1st active (default is 1st active)" >&2
	echo "   -s - Affect source side (first node) of link only." >&2
	echo "   -d - Affect dest side (second node) of link only." >&2
	echo "   on|off - action to perform" >&2
	echo "            on - enabled beaconing LED" >&2
	echo "            off - disables beaconing LED" >&2
	echo  >&2
	echo "portlist.csv is a file listing the links to process" >&2
	echo "It is of the form:" >&2
	echo "  NodeGUID;PortNum;NodeType;NodeDesc;NodeGUID;PortNum;NodeType;NodeDesc;Dontcare" >&2
	echo  >&2
	echo "for example:" >&2
	echo "   opaledports on < portlist.csv" >&2
	exit 2
}

lookup_lid()
{
	local nodeguid="$1"
	local portnum="$2"
	local guid port type desc lid
	grep "^$nodeguid;$portnum;" < $lidmap|head -n 1| while read guid port type desc lid
	do
		echo -n $lid
	done        
}

generate_lidmap()
{

	# generate lidmap
	/usr/sbin/opaextractlids $port_opts > $lidmap
	if [ ! -s $lidmap ]
	then
		echo "opaledports: Unable to determine fabric lids" >&2
		rm -f $lidmap
		return 1
	fi
}

set_led()
{
	processed=0
	skipped=0
	failed=0
	IFS=';'
	while read nodeguid1 port1 type1 desc1 nodeguid2 port2 type2 desc2 rest
	do
		if [ "$dstonly" -eq 0 ]; then	#we are processing first port 
			if [ $type1 = "SW" ]; then
				lid1=$(lookup_lid $nodeguid1 0)
			else
				lid1=$(lookup_lid $nodeguid1 $port1)
			fi
			echo "$lid1;$nodeguid1;$port1;$type1;$desc1"
		fi
		if [ "$srconly" -eq 0 ]; then	#we are processing second port
			if [ -n "$nodeguid2" ]; then
				if [ $type2 = "SW" ]; then	
					lid2=$(lookup_lid $nodeguid2 0)
				else
					lid2=$(lookup_lid $nodeguid2 $port2)
				fi
				echo "$lid2;$nodeguid2;$port2;$type2;$desc2"
			fi
		fi
	done | 
	{
	while read lid guid port type desc 
	do
		if [ x"$lid" = x ]
		then
			echo "Skipping link: $desc:$port -> $ldesc:$lport"
			skipped=$(( $skipped + 1))
		else
			if [ "$enable_disable" == "on" ]; then
				echo "Turning on LED beaconing: $guid:$desc:$lid:$port"
				eval /usr/sbin/opaportconfig $port_opts -l $lid -m $port ledon
				if [ $? -ne 0 ]; then failed=$((failed+1)); fi
			else
				echo "Turning off LED beaconing: $guid:$desc:$lid:$port"
				eval /usr/sbin/opaportconfig $port_opts -l $lid -m $port ledoff
				if [ $? -ne 0 ]; then failed=$((failed+1)); fi
			fi
			processed=$(( $processed + 1))
		fi
	done
	if [ $failed -eq 0 ]
	then
		echo "Processed: $processed; Skipped: $skipped"
		return 0
	else
		echo "Processed: $processed; Skipped: $skipped; Failed: $failed"
		return 1
	fi
	}
}


if [ x"$1" = "x--help" ]
then
	Usage_full
fi

res=0
export srconly=0
export dstonly=0
export clearall=0
export hfi_input=0
export port_input=0

while getopts sdCh:p:t: param
do
	case $param in
	p)	export port_input="$OPTARG";;
	h)	export hfi_input="$OPTARG";;
	C)	export clearall=1;;
	s)	export srconly=1;;
	d)	export dstonly=1;;
	?)	Usage;;
	esac
done

/usr/sbin/oparesolvehfiport $hfi_input $port_input &>/dev/null
if [ $? -ne 0 -o "$hfi_input" = "" -o "$port_input" = "" ]
then
	echo "opaledports: Error: Invalid port specification: $hfi_input:$port_input" >&2
	exit 1
fi

hfi=$(/usr/sbin/oparesolvehfiport $hfi_input $port_input -o hfinum)
port=`echo $(/usr/sbin/oparesolvehfiport $hfi_input $port_input | cut -f 2 -d ':')`
port_opts="-h $hfi -p $port"

if [ "$clearall" -eq 1 ]; then
	if [ $srconly -eq 1 ] || [ $dstonly -eq 1 ]; then
		echo "ERROR: -d/-s incompatible with -C option" >&2
		Usage
		exit 1
	fi
	export clearall=0
	/usr/sbin/opaextractsellinks $port_opts -F led:on  | $0 $port_opts off
	exit $?
fi

if [ $srconly -eq 1 ] && [ $dstonly -eq 1 ]; then
	echo "ERROR: -d and -s options mutually exclusive.  Choose only one." >&2
	Usage
fi

shift $((OPTIND -1))

if [ $# -ge 1 ]
then
	enable_disable="$1"
	shift
fi

if [ $# -ge 1 ]
then
	Usage
fi

if [ "$enable_disable" != "on" ] && [ "$enable_disable" != "off" ]; then
  echo "ERROR: No action -C, on or off specified" >&2
  Usage
fi
  
suffix=":$hfi:$port"
generate_lidmap

echo "Processing fabric: ${suffix}..."
echo "--------------------------------------------------------"
set_led "$hfi" "$port"
if [ $? -ne 0 ]
then
	res=1
fi

exit $res
