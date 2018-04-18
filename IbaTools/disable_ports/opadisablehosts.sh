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

# disable the specified set of hosts

if [ -f /etc/opa/opafastfabric.conf ]
then
	. /etc/opa/opafastfabric.conf
fi

. /usr/lib/opa/tools/opafastfabric.conf.def

. /usr/lib/opa/tools/ff_funcs

tempfile="$(mktemp)"
trap "rm -f $tempfile; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $tempfile" EXIT

Usage_full()
{
	echo "Usage: opadisablehosts [-h hfi] [-p port] reason host ..." >&2
	echo "              or" >&2
	echo "       opadisablehosts --help" >&2
	echo " " >&2
	echo "   --help         - produce full help text" >&2
	echo "   -h hfi         - hfi, numbered 1..n, 0= -p port will be" >&2
	echo "                    a system wide port num (default is 0)" >&2
	echo "   -p port        - port, numbered 1..n, 0=1st active" >&2
	echo "                    (default is 1st active)" >&2
	echo "   reason         - text description of reason hosts are being disabled," >&2
	echo "                    will be saved in reason field of output file" >&2
	echo  >&2
	echo "The -h and -p options permit a variety of selections:" >&2
	echo "   -h 0       - 1st active port in system (this is the default)" >&2
	echo "   -h 0 -p 0  - 1st active port in system " >&2
	echo "   -h x       - 1st active port on HFI x" >&2
	echo "   -h x -p 0  - 1st active port on HFI x" >&2
	echo "   -h 0 -p y  - port y within system (irrespective of which ports are active)" >&2
	echo "   -h x -p y  - HFI x, port y" >&2
	echo  >&2
	echo "Information about the links disabled will be written to a CSV file. By default">&2
	echo "this file is named $CONFIG_DIR/opa/disabled:hfi:port.csv where the hfi:port">&2
	echo "part of the file name is replaced by the HFI number and the port number being">&2
	echo "operated on (such as 1:1 or 2:1).  This CSV file can be used as input to">&2
	echo "opaenableports. It is of the form:" >&2
	echo "  NodeGUID;PortNum;NodeType;NodeDesc;NodeGUID;PortNum;NodeType;NodeDesc;Reason" >&2
	echo "For each listed link, the switch port closer to this is the one that has been" >&2
	echo "disabled." >&2
	echo  >&2
	echo "for example:" >&2
	echo "   opadisablehosts 'bad DRAM' compute001 compute045" >&2
	echo "   opadisablehosts -h 1 -p 2 'crashed' compute001 compute045" >&2
	exit 0
}

Usage()
{
	echo "Usage: opadisablehosts [-h hfi] [-p port] reason host ..." >&2
	echo "              or" >&2
	echo "       opadisablehosts --help" >&2
	echo " " >&2
	echo "   --help         - produce full help text" >&2
	echo "   -h hfi         - hfi, numbered 1..n, 0= -p port will be" >&2
	echo "                    a system wide port num (default is 0)" >&2
	echo "   -p port        - port, numbered 1..n, 0=1st active" >&2
	echo "                    (default is 1st active)" >&2
	echo "   reason         - text description of reason hosts are being disabled," >&2
	echo "                    will be saved in reason field of output file" >&2
	echo >&2
	echo "for example:" >&2
	echo "   opadisablehosts 'bad DRAM' compute001 compute045" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

reason=
hfi=0
port=0
while getopts h:p: param
do
	case $param in
	h)	hfi="$OPTARG";;
	p)	port="$OPTARG";;
	?)	Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -lt 1 ]
then
	Usage
fi
reason="$1"
shift

if [ $# -lt 1 ]
then
	Usage
fi

if [ "$port" -eq 0 ]
then
	port_opts="-h $hfi" # default port to 1st active
else
	port_opts="-h $hfi -p $port"
fi

# loop for each host
# this could be more efficient, but for a small list of hosts its ok
res=0
for i in "$@"
do
	echo "============================================================================="
	echo "Disabling host: $i..."
	/usr/sbin/opaextractsellinks $port_opts -F "nodepat:$i hfi*" > $tempfile
	if [ ! -s $tempfile ]
	then
		echo "opadisablehosts: Unable to find host: $i" >&2
		res=1
	else
		/usr/sbin/opadisableports -h $hfi -p $port "$reason" < $tempfile
	fi
	rm -f $tempfile
done
exit $res
