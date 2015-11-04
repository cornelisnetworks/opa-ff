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

# This script selects a VF based on the parameters given
# It will return exactly 1 VF, represented in a CSV format
# 			name:index:pkey:sl:mtu:rate:option_flags
# if there is no matching VF, a non-zero exit code is returned
# this script is intended for use in mpirun and wrapper scripts to
# select a VF and aid in exported env variables

Usage_full()
{
	echo "Usage: opagetvf [-h hfi] [-p port] [-e]" >&2
	echo "                 [-d vfname|-S serviceId|-m mcgid|-i vfIndex|-k pkey|-L sl]" >&2
	echo "              or" >&2
	echo "       opagetvf --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -h hfi             - hfi to send via, default is 1st hfi" >&2
	echo "   -p port            - port to send via, default is 1st active port" >&2
	echo "   -e                 - output mtu and rate as enum values, 0=unspecified" >&2
	echo "   -d vfname          - query by VirtualFabric Name" >&2
	echo "   -S serviceId       - query by Application ServiceId" >&2
	echo "   -m gid             - query by Application Multicast GID" >&2
	echo "   -i vfindex         - query by VirtualFabric Index" >&2
	echo "   -k pkey            - query by VirtualFabric PKey" >&2
	echo "   -L SL              - query by VirtualFabric SL" >&2

	echo "for example:" >&2
	echo "   opagetvf -d 'Compute'" >&2
	echo "   opagetvf -h 2 -p 2 -d 'Compute'" >&2
	exit 0
}

Usage()
{
	echo "Usage: opagetvf [-e] [-d vfname|-S serviceId|-m mcgid]" >&2
	echo "              or" >&2
	echo "       opagetvf --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -e                 - output mtu and rate as enum values, 0=unspecified" >&2
	echo "   -d vfname          - query by VirtualFabric Name" >&2
	echo "   -S serviceId       - query by Application ServiceId" >&2
	echo "   -m gid             - query by Application Multicast GID" >&2

	echo "for example:" >&2
	echo "   opagetvf -d 'Compute'" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

query_args=""
output_arg=vfinfocsv
while getopts h:p:ed:S:m:i:k:L: param
do
	case $param in
	h)	query_args="$query_args -$param '$OPTARG'";;
	p)	query_args="$query_args -$param '$OPTARG'";;
	e)	output_arg=vfinfocsv2;;
	d)	query_args="$query_args -$param '$OPTARG'";;
	S)	query_args="$query_args -$param '$OPTARG'";;
	m)	query_args="$query_args -$param '$OPTARG'";;
	i)	query_args="$query_args -$param '$OPTARG'";;
	k)	query_args="$query_args -$param '$OPTARG'";;
	L)	query_args="$query_args -$param '$OPTARG'";;
	?)	Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -ge 1 ]
then
	Usage
fi

vfs=$(eval /usr/sbin/opasaquery -o $output_arg $query_args)
if [ $? != 0 ]
then
	echo "opagetvf: SA query failed" >&2
	exit 1
fi
if [ -z "$vfs" -o "$vfs" = "No Records Returned" ]
then
	echo "opagetvf: VirtualFabric Not Found: $query_args" >&2
    exit 1
fi

# if more than 1 VF was returned, filter out the default partition
# and return the 1st one after it
# if the 1st two are both for the default partition, return the 1st one
lines=$(echo "$vfs"|wc -l)
candidate_vf=$(echo "$vfs"|head -1)
candidate_pkey=$(echo "$candidate_vf"|cut -d ':' -f 3)
for line in $(seq 2 $lines)
do
	vf=$(echo "$vf"|head -$line|tail -1)
	[ -z "$vf" ] && continue  # should not happen
	pkey=$(echo "$vf"|cut -d ':' -f 3)

	# prefer a VF which we are a full member of
	if [ $(($candidate_pkey & 0x8000)) -ne 0 -a $(($pkey & 0x8000)) -eq 0 ]
	then
		continue
	fi
	if [ $(($candidate_pkey & 0x8000)) -eq 0 -a $(($pkey & 0x8000)) -ne 0 ]
	then
		candidate_vf="$vf"
		candidate_pkey="$pkey"
		continue
	fi

	# prefer a non-default partition
	if [ $(($candidate_pkey & 0x7fff)) -eq $((0x7fff)) \
		-a $(($pkey & 0x7fff)) -ne $((0x7fff)) ]
	then
		candidate_vf="$vf"
		candidate_pkey="$pkey"
		continue
	fi

	# otherwise, stick with our existing candidate
done

echo "$candidate_vf"
exit 0
