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
# This script provides a wrapper for opareport which can be run
# against multiple fabrics via multiple local HFI ports

# optional override of defaults
if [ -f /etc/opa/opafastfabric.conf ]
then
	. /etc/opa/opafastfabric.conf
fi

. /usr/lib/opa/tools/opafastfabric.conf.def

. /usr/lib/opa/tools/ff_funcs

trap "exit 1" SIGHUP SIGTERM SIGINT

Usage_full()
{
	echo "Usage: opareports [-t portsfile] [-p ports] [-T topology_input]" >&2
        echo "                     [opareport arguments]" >&2
	echo "              or" >&2
	echo "       opareports --help" >&2
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
	echo "   -T topology_input - name of a topology input file to use." >&2
	echo "              Any %P markers in this filename will be replaced with the" >&2
	echo "              hfi:port being operated on (such as 0:0 or 1:2)" >&2
	echo "              default is $CONFIG_DIR/opa/topology.%P.xml" >&2
	echo "              if NONE is specified, will not use any topology_input files" >&2
	echo "              See opareport for more information on topology_input files" >&2
	echo "   opareport arguments - any of the other opareport arguments." >&2
	echo "   			The following options are not available: -h, -X" >&2
	echo "   			Note -p is interpreted as indicated above" >&2
	echo "   			When run against multiple fabrics, the following options are not" >&2
	echo "   			available: -x, -o snapshot" >&2
	echo "   			Beware: when run against multiple fabrics the -F option will" >&2
	echo "   			be applied to all fabrics." >&2
	echo " Environment:" >&2
	echo "   PORTS - list of ports, used in absence of -t and -p" >&2
	echo "   PORTS_FILE - file containing list of ports, used in absence of -t and -p" >&2
	echo "   FF_TOPOLOGY_FILE - file containing topology_input, used in absence of -T" >&2
	echo "for example:" >&2
	echo "   opareports" >&2
	echo "   opareports -p '1:1 1:2 2:1 2:2'" >&2
	echo "opareport arguments:" >&2
	/usr/sbin/opareport --help
	exit 2
}

Usage()
{
	echo "Usage: opareports [opareport arguments]" >&2
	echo "              or" >&2
	echo "       opareports --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   opareport arguments - any of the other opareport arguments." >&2
	echo "for example:" >&2
	echo "   opareports" >&2
	echo "opareport arguments:" >&2
	/usr/sbin/opareport -?
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

status=ok

TEMP=`getopt -a -n opareports -o 'p:t:vqo:d:PHmMNxT:sri:Cac:LF:S:D:QVA' -l 'verbose,quiet,output:,detail:,persist,hard,smadirect,pmadirect,noname,xml,topology:,stats,routes,interval:,clear,clearall,config:,limit,focus:,src:,dest:,quietfocus,vltables,allports' -- "$@"`
if [ $? != 0 ]
then
	Usage
fi
eval set -- "$TEMP"
xopt=n
report=
while true
do
	case "$1" in
		-p)	export PORTS="$2"; shift 2;;
		-t)	export PORTS_FILE="$2"; shift 2;;
		#-h|--hfi)	echo "$1 option not allowed"; Usage; shift 2;;
		-x|--xml)	xopt=y; opts="$opts $1"; shift;;
		-o|--output) opts="$opts $1 '$2'"; report="$report $2"; shift 2;;
		-d|--detail) opts="$opts $1 '$2'"; shift 2;;
		#-X|--infile)	echo "$1 option not allowed"; Usage; shift 2;;
		-T|--topology)	export FF_TOPOLOGY_FILE="$2"; shift 2;;
		-i|--interval)	opts="$opts $1 '$2'"; shift 2;;
		-c|--config)	opts="$opts $1 '$2'"; shift 2;;
		-F|--focus)	opts="$opts $1 '$2'"; shift 2;;
		-S|--src)	opts="$opts $1 '$2'"; shift 2;;
		-D|--dest)	opts="$opts $1 '$2'"; shift 2;;
		--) shift; break;;	# end of options
		*) opts="$opts $1"; shift;;	# catches all other options without args
	esac
done
for arg in "$@"
do
	# no additional arguments are allowed
	Usage
	opts="$opts $arg"
done

# process ports and count how many we have
check_ports_args opareports
ports=0
for hfi_port in $PORTS
do
	ports=$(($ports + 1))
done
if [ $ports -lt 1 ]
then
	# should not happen, but be safe
	ports=1
	PORTS="0:0"
fi

if [ $ports -gt 1 ]
then
	if [ "$xopt" = y ]
	then
		echo "opareports: -x/--xml option not allowed for > 1 port" >&2
		Usage_full
	fi
	if echo "$report"|grep snapshot >/dev/null 2>/dev/null
	then
		echo "opareports: snapshot report option not allowed for > 1 port" >&2
		Usage_full
	fi
fi

run_report()
{
	# $1 = hfi
	# $2 = port
	if [ "$port" -eq 0 ]
	then
		port_opts="-h $hfi"	# default port to 1st active
	else
		port_opts="-h $hfi -p $port"
	fi
	if [ $ports -gt 1 ]
	then
		echo "Fabric $hfi:$port Report:"
	fi
	resolve_topology_file opareports "$hfi:$port"
	topt=""
	if [ "$TOPOLOGY_FILE" != "" ]
	then
		topt="-T '$TOPOLOGY_FILE'"
	fi
	#echo "/usr/sbin/opareport $port_opts $topt $opts"
	eval /usr/sbin/opareport $port_opts $topt $opts
	if [ $? -ne 0 ]
	then
		status=bad
	fi
	if [ $ports -gt 1 ]
	then
		echo "-------------------------------------------------------------------------------"
	fi
}


for hfi_port in $PORTS
do
	hfi=$(expr $hfi_port : '\([0-9]*\):[0-9]*')
	port=$(expr $hfi_port : '[0-9]*:\([0-9]*\)')
	if [ "$hfi" = "" -o "$port" = "" ]
	then
		echo "opareports: Error: Invalid port specification: $hfi_port" >&2
		status=bad
		continue
	fi

	run_report "$hfi" "$port"
done

if [ "$status" != "ok" ]
then
	exit 1
else
	exit 0
fi
