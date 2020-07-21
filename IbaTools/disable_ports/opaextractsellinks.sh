#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
#
# Copyright (c) 2015-2020, Intel Corporation
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

# Run opareports and pipe output to opaxmlextract to extract
#  specified links

tempfile="$(mktemp)"
trap "rm -f $tempfile; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $tempfile" EXIT

cmd=`basename $0`
Usage_full()
{
	echo >&2
	echo "Usage: ${cmd} [--help]|[opareport options]" >&2
	echo "   --help - produce full help text" >&2
	echo "   [opareport options] - options will be passed to opareport." >&2
	echo >&2
	echo "${cmd} is a front end to the opareport tool that generates" >&2
	echo "a report listing all or some of the links in the fabric in a CSV format" >&2
	echo "suitable for importing into a spreadsheet or parsed by other scripts." >&2
	echo >&2
	echo "for example:" >&2
	echo "   List all the links in the fabric:" >&2
	echo "      ${cmd}" >&2
	echo >&2
	echo "   List all the links to a switch named \"coresw1\":" >&2
	echo "      ${cmd} -F \"node:coresw1\"" >&2
	echo >&2
	echo "   List all the connections to end-nodes:" >&2
	echo "      ${cmd} -F \"nodetype:FI\"" >&2
	echo >&2
	echo "   List all the links on the 2nd HFI's fabric of a multi-plane fabric:" >&2
	echo "      ${cmd} -h 2" >&2
	echo >&2
	echo "See the man page for \"opareport\" for the full set of options." >&2
	echo "By design, the tool ignores \"-o/--output\" report option." >&2
	echo >&2
	exit 0
}

Usage()
{
	echo >&2
	echo "Usage: ${cmd} [--help]|[opareport options]" >&2
	echo "   --help - produce full help text" >&2
	echo "   [opareport options] - options will be passed to opareport." >&2
	echo >&2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

line1=
# we do this against a single fabric, options can select a local HFI and Port
/usr/sbin/opareport -o links -x -Q "$@" > $tempfile
if [ -s $tempfile ]
then
	cat $tempfile| /usr/sbin/opaxmlextract -H -d \; -e LinkSummary.Link.Port.NodeGUID -e LinkSummary.Link.Port.PortNum -e LinkSummary.Link.Port.NodeType -e LinkSummary.Link.Port.NodeDesc|while read line
	do
		if [ x"$line1" = x ]
		then
			# 1st port in given link
			line1="$line"
		else
			# 2nd port in given link, output a single line for given link
			echo "$line1;$line"
			line1=
		fi
	done
	res=0
else
	echo "${cmd}: Unable to find specified links" >&2
	Usage
	res=1
fi
rm -f $tempfile
exit $res
