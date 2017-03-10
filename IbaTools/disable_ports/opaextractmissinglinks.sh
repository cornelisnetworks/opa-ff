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

# Run opareports and pipe output to opaxmlextract to extract
#  specified missing links

tempfile="$(mktemp)"
trap "rm -f $tempfile; exit 1" SIGHUP SIGTERM SIGINT
trap "rm -f $tempfile" EXIT

# Default values
topology_input=/etc/opa/topology.0:0.xml
otype=verifylinks

topology_args="-T $topology_input"
otype_args="-o $otype"

cmd=`basename $0`
Usage_full()
{
	echo >&2
	echo "Usage: ${cmd} [-T topology_input] [-o report] [--help]|[opareport options]" >&2
	echo "   --help - produce full help text" >&2
	echo "   -T     - topology file to verify against" >&2
	echo "            [Default: \"/etc/opa/topology.0:0.xml\"]" >&2
	echo "   -o     - output report type [Default: \"verifylinks\"]" >&2
	echo "      verifylinks - verify links against topology input" >&2
	echo "      verifyextlinks - verify links against topology input" >&2
	echo "          limit analysis to links external to systems" >&2
	echo "      verifyfilinks - verify links against topology input" >&2
	echo "          limit analysis to FI links" >&2
	echo "      verifyislinks - verify links against topology input" >&2
	echo "          limit analysis to inter-switch links" >&2
	echo "      verifyextislinks - verify links against topology input" >&2
	echo "          limit analysis to inter-switch links external to systems" >&2
	echo "   [opareport options] - options will be passed to opareport." >&2
	echo >&2
	echo "${cmd} is a front end to the opareport tool that generates" >&2
	echo "a report listing all or some of the links that are present in the supplied" >&2
	echo "topology file, but are missing in the fabric. The output is in a CSV format" >&2
	echo "suitable for importing into a spreadsheet or parsed by other scripts." >&2
	echo >&2
	echo "for example:" >&2
	echo "   List all the missing links in the fabric:" >&2
	echo "      ${cmd}" >&2
	echo >&2
	echo "   List all the missing links to a switch named \"coresw1\":" >&2
	echo "      ${cmd} -T topology.0:0.xml -F \"node:coresw1\"" >&2
	echo >&2
	echo "   List all the missing connections to end-nodes:" >&2
	echo "      ${cmd} -o verifyfilinks" >&2
	echo >&2
	echo "   List all the missing links on the 2nd HFI's fabric of a multi-plane fabric:" >&2
	echo "      ${cmd} -h 2 -T /etc/opa/topology.2:1.xml" >&2
	echo >&2
	echo "   List all the missing links between two switches:" >&2
	echo "      ${cmd} -o verifyislinks -T topology.0:0.xml" >&2
	echo >&2
	echo "See the man page for \"opareport\" for the full set of options." >&2
	echo >&2
	exit 0
}

Usage()
{
	echo >&2
	echo "Usage: ${cmd} [-T topology_input] [-o report] [--help]|[opareport options]" >&2
	echo "   --help - produce full help text" >&2
	echo "   -T     - topology file to verify against" >&2
	echo "            [Default: \"/etc/opa/topology.0:0.xml\"]" >&2
	echo "   -o     - output report type [Default: \"verifylinks\"]" >&2
	echo "      verifylinks, verifyextlinks, verifyfilinks, verifyislinks, verifyextislinks" >&2
	echo "   [opareport options] - options will be passed to opareport." >&2
	echo >&2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

# Check argument list
while getopts ":T:o:" param
do
	case "${param}" in
	T)	topology_input=${OPTARG}
		topology_args=
		;;
	o)
		otype=${OPTARG}
		otype_args=
		;;
	*)	;;
	esac
done

# Parse and verify otype while setting XML_PREFIX
case $otype in
verifyextislinks)XML_PREFIX=VerifyExtISLinks;;
verifyextlinks)XML_PREFIX=VerifyExtLinks;;
verifyislinks)XML_PREFIX=VerifyISLinks;;
verifyfilinks)XML_PREFIX=VerifyFILinks;;
verifylinks|verifyall)XML_PREFIX=VerifyLinks;;
*)
	echo "${cmd}: Please supply a supported output report type: $otype" >&2
	echo " [verifyextislinks, verifyextlinks, verifyislinks, verifyfilinks," >&2
	echo "  verifylinks, and verifyall]. Default is \"verifylinks\"" >&2
	Usage
	exit 1;;
esac

# Verify Topology file argument is supplied
if [ -z "$topology_input" ]; then
	echo "${cmd}: Topology file must be provided to verify against" >&2
	Usage
	exit 1
fi

# Verify Topology file exists
if [ ! -f "$topology_input" ]; then
	echo "${cmd}: Topology file not found: \"$topology_input\"" >&2
	Usage
	exit 1
fi

line1=
line2=
# we do this against a single fabric, options can select a local HFI and Port
/usr/sbin/opareport ${otype_args} ${topology_args} -x "$@" > $tempfile
if [ -s $tempfile ]
then
	IFS=';'
	cat $tempfile | /usr/sbin/opaxmlextract -H -d \; -e ${XML_PREFIX}.Link.Port.NodeGUID \
		-e ${XML_PREFIX}.Link.Port.PortNum -e ${XML_PREFIX}.Link.Port.NodeType \
		-e ${XML_PREFIX}.Link.Port.NodeDesc -e ${XML_PREFIX}.Link.Problem \
		| while read guid port type desc problem
		do
			if [ -z "$line1" -a -z "$problem" ]
			# if current line is 1st half of link and not a problem
			then
				# Port 1 if LINK: 'NodeGUID;PortNum;NodeType;NodeDesc;'
				line1="${guid};${port};${type};${desc};"

			elif [ -z "$line2" -a -z "$problem" ]
			# if current line is 2nd half of link and not a problem
			then
				# Port 2 if LINK: 'NodeGUID;PortNum;NodeType;NodeDesc;'
				line2="${guid};${port};${type};${desc};"

			elif [ "$problem" == "Missing Link" -a "$line1" -a "$line2" ]
			# if current line is the Missing Link Problem and other data exists
			then
				# echo LINK and clear temp variables
				echo "${line1}${line2}Missing Link"
				line1=
				line2=

			elif [ "$problem" ]
			# if current line is a Link.Problem, but not "Missing Link", then skip over
			then
				continue
			else
			# else None of the Link.Problems were "Missing Link",
			#      Reset temp variables and set current line to 1st port of link
				line1="${guid};${port};${type};${desc};"
				line2=
			fi
	done
	res=0
else
	echo "${cmd}: Unable to find specified missing links" >&2
	Usage
	res=1
fi
rm -f $tempfile
exit $res

