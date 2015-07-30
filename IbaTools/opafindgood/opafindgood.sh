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

# analyzes hosts in fabric and outputs some lists:
#	alive - list of pingable hosts
#	running - subset of alive which can be sshed to via opacmdall
#	active - list of hosts with 1 or more active ports
#	good - list of hosts which are running and active
#	bad - list of hosts which fail any of the above tests
# The intent is that the good list can be a candidate list of hosts for
# use in running MPI jobs to further use or test the cluster

trap "exit 1" SIGHUP SIGTERM SIGINT

# optional override of defaults
if [ -f /etc/sysconfig/opa/opafastfabric.conf ]
then
	. /etc/sysconfig/opa/opafastfabric.conf
fi

. /opt/opa/tools/opafastfabric.conf.def

TOOLSDIR=${TOOLSDIR:-/opt/opa/tools}
BINDIR=${BINDIR:-/usr/sbin}

. $TOOLSDIR/ff_funcs

punchlist=$FF_RESULT_DIR/punchlist.csv
del=';'
timestamp=$(date +"%Y/%m/%d %T")

Usage_full()
{
	echo "Usage: opafindgood [-RAQ] [-d dir] [-f hostfile] [-h 'hosts']" >&2
	echo "                    [-t portsfile] [-p ports] [-T timelimit]" >&2
	echo "              or" >&2
	echo "       opafindgood --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -R - skip the running test (ssh), recommended if password-less ssh not setup" >&2
	echo "   -A - skip the active test, recommended if OPA software or fabric is not up" >&2
	echo "   -Q - skip the quarantine test, recommended if OPA software or fabric is not up" >&2
	echo "   -d - directory in which to create alive, active, running, good and bad files" >&2
	echo "        default is $CONFIG_DIR/opa" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/opa/hosts" >&2
	echo "   -h hosts - list of hosts to ping" >&2
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
	echo "   -T timelimit - timelimit in seconds for host to respond to ssh" >&2
	echo "               default of 20 seconds" >&2
  	echo >&2
	echo "The files alive, running, active, good and bad are created in the selected" >&2
	echo "directory listing hosts passing each criteria" >&2
	echo "A punchlist of bad hosts is also appended to FF_RESULT_DIR/punchlist.csv" >&2
	echo "The good file can be used as input for an mpi_hosts." >&2
  	echo "It will list each good host exactly once" >&2
  	echo >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
	echo "   PORTS - list of ports, used in absence of -t and -p" >&2
	echo "   PORTS_FILE - file containing list of ports, used in absence of -t and -p" >&2
	echo "   FF_MAX_PARALLEL - maximum concurrent operations" >&2
	echo "example:">&2
	echo "   opafindgood" >&2
	echo "   opafindgood -f allhosts" >&2
	echo "   opafindgood -h 'arwen elrond'" >&2
	echo "   HOSTS='arwen elrond' opafindgood" >&2
	echo "   HOSTS_FILE=allhosts opafindgood" >&2
	echo "   opafindgood -p '1:1 1:2 2:1 2:2'" >&2
	exit 0
}

Usage()
{
	echo "Usage: opafindgood [-RAQ] [-d dir] [-f hostfile] [-h 'hosts']" >&2
	echo "                    [-t portsfile] [-p ports] [-T timelimit]" >&2
	echo "       opafindgood --help" >&2
	echo "              or" >&2
	echo "   --help - produce full help text" >&2
	echo "   -R - skip the running test (ssh), recommended if password-less ssh not setup" >&2
	echo "   -A - skip the active test, recommended if OPA software or fabric is not up" >&2
	echo "   -Q - skip the quarantine test, recommended if OPA software or fabric is not up" >&2
	echo "   -d - directory in which to create alive, active, running, good and bad files" >&2
	echo "        default is $CONFIG_DIR/opa" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/opa/hosts" >&2
	echo "" >&2
	echo "   See full help text for explanation of all options." >&2
  	echo >&2
	echo "The files alive, running, active, good and bad are created in the selected" >&2
	echo "directory listing hosts passing each criteria" >&2
	echo "A punchlist of bad hosts is also appended to FF_RESULT_DIR/punchlist.csv" >&2
	echo "The good file can be used as input for an mpi_hosts." >&2
  	echo "It will list each good host exactly once" >&2
  	echo >&2
	echo "example:">&2
	echo "   opafindgood" >&2
	echo "   opafindgood -f allhosts" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

skip_ssh=n
skip_active=n
skip_quarantine=n
dir=$CONFIG_DIR/opa
timelimit=20
while getopts d:f:h:t:p:QRAT: param
do
	case $param in
	d)	dir="$OPTARG";;
	h)	HOSTS="$OPTARG";;
	f)	HOSTS_FILE="$OPTARG";;
	p)	export PORTS="$OPTARG";;
	t)	export PORTS_FILE="$OPTARG";;
	R)	skip_ssh=y;;
	A)	skip_active=y;;
	Q)  skip_quarantine=y;;
	T)	export timelimit="$OPTARG";;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -gt 0 ]
then
	Usage
fi

check_host_args opafindgood

# just pass host list via environment for opapingall and opacmdall below
export HOSTS
unset HOSTS_FILE

check_ports_args opafindgood


append_punchlist()
# file with $1 set of tested hosts (in lowercase with no dups, unsorted)
# file with $2 set of passing hosts (in lowercase with no dups, unsorted)
# text for punchlist entry for failing hosts
{
	comm -23 <(sort "$1") <(sort "$2")| opasorthosts | while read host
	do
		echo "$timestamp$del$host$del$3"
	done >> $punchlist
}

# read stdin and convert hostnames to canonical lower case
# in field 1 of output, field2 is unmodified input
# fields are separated by a space
function to_canon()
{
	while read line
	do
		canon=$(ff_host_basename $line|ff_to_lc)
		echo "$canon $line"
	done|sort --ignore-case -t ' ' -k1,1
}

function mycomm12()
{
	$TOOLSDIR/comm12 $1 $2
}

echo "$(ff_var_filter_dups_to_stdout "$HOSTS"|wc -l) hosts will be checked"

good_meaning=	# indicates which tests a good host has passed
good_file=		# file (other than good) holding most recent good list

# ------------------------------------------------------------------------------
# ping test
opapingall -p|grep 'is alive'|sed -e 's/:.*//'|ff_filter_dups|opasorthosts > $dir/alive
append_punchlist <(ff_var_filter_dups_to_stdout "$HOSTS") $dir/alive "Doesn't ping"
good_meaning="alive"
good_file=$dir/alive
echo "$(cat $dir/alive|wc -l) hosts are pingable (alive)"

# ------------------------------------------------------------------------------
# ssh test
if [ "$skip_ssh" = n ]
then
	# -h '' to override HOSTS env var so -f is used (HOSTS would override -f)
	# use comm command to filter out hosts with unexpected hostnames
	# put in alphabetic order for "comm" command
	mycomm12 <(to_canon < $good_file) <(opacmdall -h '' -f $good_file -p -T $timelimit 'hostname -s' |grep -v 'hostname -s'|ff_filter_dups|to_canon) | opasorthosts > $dir/running
	append_punchlist $good_file $dir/running "Can't ssh"
	good_meaning="$good_meaning, running"
	good_file=$dir/running
	echo "$(cat $dir/running|wc -l) hosts are ssh'able (running)"
fi

# ------------------------------------------------------------------------------
# port active test
if [ "$skip_active" = n ]
then
	for hfi_port in $PORTS
	do
		hfi=$(expr $hfi_port : '\([0-9]*\):[0-9]*')
		port=$(expr $hfi_port : '[0-9]*:\([0-9]*\)')
		if [ "$hfi" = "" -o "$port" = "" ]
		then
			echo "opafindgood: Error: Invalid port specification: $hfi_port" >&2
			continue
		fi
		if [ "$port" -eq 0 ]
		then
			port_opts="-h $hfi"	# default port to 1st active
		else
			port_opts="-h $hfi -p $port"
		fi

		opasaquery $port_opts -t fi -o desc|sed -e 's/ HFI-[0-9]*$//'
	done | ff_filter_dups|opasorthosts > $dir/active
	# don't waste time reporting hosts which don't ping or can't ssh
	# they are probably down so no use double reporting them
	append_punchlist $good_file $dir/active "No active port"
	# put in alphabetic order for "comm" command
	mycomm12 <(to_canon < $good_file)  <(to_canon < $dir/active) | opasorthosts > $dir/good
	good_meaning="$good_meaning, active"
	#good_file=$dir/good	# can't really say this if next step would use it
	good_file=
	echo "$(cat $dir/active|wc -l) total hosts have FIs active on one or more fabrics (active)"
else
	cat $good_file > $dir/good
fi

# ------------------------------------------------------------------------------
# quarantined node test
if [ "$skip_quarantine" = n ]
then
	# Change the field delimiter so we don't multiprocess node descriptions with spaces
	OLDIFS=$IFS
	IFS=$(echo -en "\n\b")
	testi=1

	# Get the node type and desc to output only quarantined HFIs
	for qnode in $(opareport -q -o quarantinednodes -x | opaxmlextract -H -e QuarantinedNodes.QNode.NodeType -e QuarantinedNodes.QNode.NodeDesc)
	do
		if [ $(echo $qnode | cut -f 1 -d ';') == "FI" ] 
		then
			echo $qnode | cut -f 2 -d ';' | sed -e 's/ HFI-[0-9]*$//'
		fi
	done | ff_filter_dups | opasorthosts > $dir/quarantined

	IFS=$OLDIFS

	# Remove nodes from the "good" file that are quarantined. Have to use a temp file as comm doesn't like rewriting a file it's reading
	comm -23 <(sort $dir/good) <(sort $dir/quarantined) | opasorthosts > $dir/temp
	mv $dir/temp $dir/good

	# Add punchlist items for any quarantined nodes. We can't use the defined append_punchlist as quarantined items will likely
	# have already failed the active test. We use awk as opaxmlextract doesn't handle multiple quarantine reasons nicely.
	punchlistgen='
		# Use the NodeType tag as a record separator
		BEGIN { FS="\n"; RS="<NodeType>"; ORS="" }
		{
			# Strip XML tags and leading whitespace from NodeType
			gsub(/(<[^>]*>|^[ \t]+)/, "", $1)
			if( $1 == "FI" ) {
				# Strip XML tags and leading whitespace from NodeDesc
				gsub(/(<[^>]*>|^[ \t]+)/, "", $2)
				# Print timestamp, lower cased hostname, and start of quarantine reasons
				print timestamp
				$2 = tolower($2)
				print delim$2
				print delim"Quarantined: "
				for(i = 3; i < NF; i++) {
					# Strip XML tags and leading whitespace from each quarantine Reason
					gsub(/(<[^>]*>|^[ \t]+)/, "", $i)
					print $i
		
					if( i != NF-1 )
						print ", "
				}
				print "\n"
			}
		}
	'

	# Steps are as follows:
	# Output quarantine nodes | Limit to NodeDesc,NodeType,Reason | Format into punchlist style | 
	# Insert extra delim into NodeDesc for sorting | Sort on NodeDesc string then HFI num | Remove extra delim >> Output to punchlist
	opareport -q -o quarantinednodes -x | grep -E '(\<NodeDesc\>|\<NodeType\>|\<Reason\>)' | awk -v delim="$del" -v timestamp="$timestamp" "$punchlistgen"  | awk -v delim="$del" -v FS="$del" -v OFS="$del" '{gsub(/([0-9]*)$/, delim"&", $2); print $0}' | sort --ignore-case -k2,2 -k3,3n -t $del | awk -v delim="$del" -v FS="$del" '{print $1delim$2$3delim$4 }' >> $punchlist 

	echo "$(cat $dir/quarantined|wc -l) hosts have FIs that have failed authentication (quarantined)"
fi

# ------------------------------------------------------------------------------
# final output
echo "$(cat $dir/good|wc -l) hosts are $good_meaning (good)"
comm -23 <(ff_var_filter_dups_to_stdout "$HOSTS") <(sort $dir/good)| opasorthosts > $dir/bad
echo "$(cat $dir/bad|wc -l) hosts are bad (bad)"

exit 0
