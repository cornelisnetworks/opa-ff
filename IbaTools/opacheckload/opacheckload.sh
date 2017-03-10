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

# get loadavg on all hosts and show busiest or least busy hosts
# This can be used to make sure hosts are idle before starting a MPI benchmark
# or to see if a benchmark is running

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
	echo "Usage: opacheckload [-f hostfile] [-h 'hosts'] [-r] [-a|-n numprocs]" >&2
	echo "                      [-d uploaddir]" >&2
	echo "              or" >&2
	echo "       opacheckload --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -f hostfile - file with hosts to check," >&2
	echo "                 default is $CONFIG_DIR/opa/hosts" >&2
	echo "   -h hosts - list of hosts to check" >&2
	echo "   -r - reverse output, show least busy hosts, default is busiest hosts" >&2
	echo "   -n numprocs - show top numprocs hosts, default 10" >&2
	echo "   -a - show all hosts, default 10" >&2
	echo "   -d upload_dir - directory to upload loadavg to, default is uploads" >&2
	echo >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
	echo "   UPLOADS_DIR - directory to upload loadavg to, used in absence of -d" >&2
	echo "   FF_MAX_PARALLEL - maximum concurrent operations" >&2
	echo "example:">&2
	echo "   opacheckload" >&2
	echo "   opacheckload -h 'arwen elrond'" >&2
	echo "   HOSTS='arwen elrond' opacheckload" >&2
	exit 0
}

Usage()
{
	echo "Usage: opacheckload [-f hostfile] [-r] [-a|-n numprocs]" >&2
	echo "              or" >&2
	echo "       opacheckload --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -f hostfile - file with hosts to check," >&2
	echo "                 default is $CONFIG_DIR/opa/hosts" >&2
	echo "   -r - reverse output, show least busy hosts, default is busiest hosts" >&2
	echo "   -n numprocs - show top numprocs hosts, default 10" >&2
	echo "   -a - show all hosts, default 10" >&2
	echo "   -H - supress headers for script parsing" >&2
	echo >&2
	echo " Environment:" >&2
	echo "   FF_MAX_PARALLEL - maximum concurrent operations" >&2
	echo "example:">&2
	echo "   opacheckload" >&2
	echo "   opacheckload -f good" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

numprocs=10
ropt=-r
while getopts f:h:n:d:aHr param
do
	case $param in
	h)
		HOSTS="$OPTARG";;
	H)
		skip_headers=1;;
	f)
		HOSTS_FILE="$OPTARG";;
	n)
		numprocs="$OPTARG";;
	d)
		UPLOADS_DIR="$OPTARG";;
	a)
		numprocs="1000000";;	# more than ever expected to be found
	r)
		ropt="";;	# sort from lowest to highest, shows least busy
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))

if [ $# -ne 0 ]
then
	echo "opacheckload: extra arguments: $@" >&2
	Usage
fi

check_host_args opacheckload
# HOSTS now lists all the hosts, pass it along to the commands below via env
export HOSTS
unset HOSTS_FILE

# remove any stale data so we don't mistakenly report it below
for j in $HOSTS
do
	rm -f $UPLOADS_DIR/$j/loadavg
done

opacmdall -p 'cat /proc/loadavg > /tmp/loadavg' >/dev/null
opauploadall -p /tmp/loadavg loadavg >/dev/null
if [ -z $skip_headers ]; then
	echo "loadavg					host"
fi

for j in $HOSTS
do
	i=$UPLOADS_DIR/$j/loadavg
	if [ ! -e $i ]
	then
		echo "opacheckload: $j: Unable to get loadavg" >&2
	else
		l=`cat $i`
		echo "$l	$j"
	fi
done | sort -n $ropt|head -n $numprocs
