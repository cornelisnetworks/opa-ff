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

# verify hosts basic single node configuration and performance via hostverify.sh
# prior to using this, copy /usr/share/opa/samples/hostverify.sh to FF_HOSTVERIFY_DIR,
# and edit to set proper expectations for node configuration and performance, 

# optional override of defaults
if [ -f /etc/opa/opafastfabric.conf ]
then
	. /etc/opa/opafastfabric.conf
fi

. /usr/lib/opa/tools/opafastfabric.conf.def

. /usr/lib/opa/tools/ff_funcs

trap "exit 1" SIGHUP SIGTERM SIGINT

punchlist=$FF_RESULT_DIR/punchlist.csv
del=';' # TBD what will work best for import

Usage_full()
{
	echo "Usage: opaverifyhosts [-kc] [-f hostfile] [-u upload_file] [-d upload_dir]" >&2
	echo "                         [-h 'hosts'] [-T timelimit] [test ...]" >&2
	echo "              or" >&2
	echo "       opaverifyhosts --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -k - at start and end of verification, kill any existing hostverify" >&2
	echo "        or xhpl jobs on the hosts" >&2
	echo "   -c - copy hostverify.sh to hosts first, useful if you have edited it" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/opa/hosts" >&2
	echo "   -h hosts - list of hosts to ping" >&2
	echo "   -u upload_file - filename to upload hostverify.res to after verification" >&2
	echo "                    to allow backup and review of the detailed results" >&2
	echo "                    for each node" >&2
	echo "                    The default upload destination file is hostverify.res" >&2
	echo "                    If -u '' is specified, no upload will occur" >&2
	echo "   -d upload_dir - directory to upload result from each host to" >&2
	echo "                   default is uploads" >&2
	echo "   -T timelimit - timelimit in seconds for host to complete tests" >&2
	echo "                  default of 300 seconds (5 minutes)" >&2
	echo "   -F filename - filename of hostverify script to use. Default is $FF_HOSTVERIFY_DIR/hostverify.sh" >&2
	echo "	 test - one or more specific tests to run" >&2
	echo "	        see /usr/share/opa/samples/hostverify.sh for a list of available tests" >&2
	echo "This verifies basic node configuration and performance by running" >&2
	echo "FF_HOSTVERIFY_DIR/hostverify.sh on all specified hosts" >&2
	echo >&2
	echo "Prior to using this, copy /usr/share/opa/samples/hostverify.sh to FF_HOSTVERIFY_DIR" >&2
	echo "and edit to set proper expectations for node configuration and performance" >&2
	echo "Then be sure to use the -c option on first run for a given node" >&2
	echo "so that hostverify.sh gets copied to each node." >&2
	echo "FF_HOSTVERIFY_DIR is configured in /etc/opa/opafastfabric.conf" >&2
	echo >&2
	echo "A summary of results is appended to FF_RESULT_DIR/verifyhosts.res." >&2
	echo "A punchlist of failures is also appended to FF_RESULT_DIR/punchlist.csv" >&2
	echo "Only failures are shown on stdout" >&2
	echo >&2
	echo " Environment:" >&2
	echo "   HOSTS - list of hosts, used if -h option not supplied" >&2
	echo "   HOSTS_FILE - file containing list of hosts, used in absence of -f and -h" >&2
	echo "   UPLOADS_DIR - directory to upload to, used in absence of -d" >&2
	echo "   FF_MAX_PARALLEL - maximum concurrent operations" >&2
	echo "example:">&2
	echo "   opaverifyhosts -c" >&2
	echo "   opaverifyhosts -h 'arwen elrond'" >&2
	echo "   HOSTS='arwen elrond' opaverifyhosts" >&2
	exit 0
}

Usage()
{
	echo "Usage: opaverifyhosts [-kc] [-f hostfile] [-u upload_file]" >&2
	echo "              or" >&2
	echo "       opaverifyhosts --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -k - at start and end of verification, kill any existing hostverify" >&2
	echo "        or xhpl jobs on the hosts" >&2
	echo "   -c - copy hostverify.sh to hosts first, useful if you have edited it" >&2
	echo "   -f hostfile - file with hosts in cluster, default is $CONFIG_DIR/opa/hosts" >&2
	echo "   -u upload_file - filename to upload hostverify.res to after verification" >&2
	echo "                    to allow backup and review of the detailed results" >&2
	echo "                    for each node" >&2
	echo "                    The default upload destination file is hostverify.res" >&2
	echo "                    If -u '' is specified, no upload will occur" >&2
	echo >&2
	echo "This verifies basic node configuration and performance by running" >&2
	echo "FF_HOSTVERIFY_DIR/hostverify.sh on all specified hosts" >&2
	echo >&2
	echo "Prior to using this, copy /usr/share/opa/samples/hostverify.sh to FF_HOSTVERIFY_DIR" >&2
	echo "and edit to set proper expectations for node configuration and performance" >&2
	echo "Then be sure to use the -c option on first run for a given node" >&2
	echo "so that hostverify.sh gets copied to each node." >&2
	echo "FF_HOSTVERIFY_DIR is configured in /etc/opa/opafastfabric.conf" >&2
	echo >&2
	echo "A summary of results is appended to FF_RESULT_DIR/verifyhosts.res." >&2
	echo "A punchlist of failures is also appended to FF_RESULT_DIR/punchlist.csv" >&2
	echo "Only failures are shown on stdout" >&2
	echo >&2
	echo "example:">&2
	echo "   opaverifyhosts" >&2
	echo "   opaverifyhosts -c" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi

append_punchlist()
# stdin has verifyhosts.res failure lines generated in this run
{
	(
	export IFS=':'
	opasorthosts | while read host failinfo
	do
		echo "$timestamp$del$host$del$failinfo"
	done >> $punchlist
	)
}

job_cleanup()
{
	# HOSTS is exported
	if [ "$do_kill" = y ]
	then
		# we don't log this
		echo "Killing hostverify and xhpl on hosts..."
		# we use patterns so the pkill doesn't kill this script or opacmdall itself
		# use an echo at end so exit status is good
		opacmdall -p -T 60 "pkill -9 -f -x 'host[v]erify.*.sh'; pkill -9 '[x]hpl'; echo -n"
	fi
}

do_copy=n
upload_file=hostverify.res
timelimit=300
do_kill=n
filename=$FF_HOSTVERIFY_DIR/hostverify.sh
while getopts kcf:h:d:u:T:F: param
do
	case $param in
	k)
		do_kill=y;;
	c)
		do_copy=y;;
	h)
		HOSTS="$OPTARG";;
	f)
		HOSTS_FILE="$OPTARG";;
	d)
		export UPLOADS_DIR="$OPTARG";;
	u)
		upload_file="$OPTARG";;
	T)
		timelimit="$OPTARG";;
	F)
		filename="$OPTARG";;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
check_host_args opaverifyhosts
# HOSTS now lists all the hosts, pass it along to the commands below via env
export HOSTS
unset HOSTS_FILE

job_cleanup

echo "=============================================================================" >> $FF_RESULT_DIR/verifyhosts.res
date >> $FF_RESULT_DIR/verifyhosts.res
echo "$(echo "$HOSTS"|tr -s ' ' '\n'|sed -e '/^$/d'|sort -u| wc -l) hosts will be verified" | tee -a $FF_RESULT_DIR/verifyhosts.res

if [ "$do_copy" = y ]
then
	echo "SCPing $filename to $FF_HOSTVERIFY_DIR/hostverify.sh ..."| tee -a $FF_RESULT_DIR/verifyhosts.res
	opascpall -p "$filename" "$FF_HOSTVERIFY_DIR/hostverify.sh" 2>&1|tee -a $FF_RESULT_DIR/verifyhosts.res
	date >> $FF_RESULT_DIR/verifyhosts.res
fi

timestamp=$(date +"%Y/%m/%d %T")
echo "Running $FF_HOSTVERIFY_DIR/hostverify.sh -d $FF_HOSTVERIFY_DIR $* ..."
resultlineno=$(cat $FF_RESULT_DIR/verifyhosts.res|wc -l)	# for punchlist
opacmdall -p -T $timelimit "bash $FF_HOSTVERIFY_DIR/hostverify.sh -d $FF_HOSTVERIFY_DIR $*" 2>&1|tee -a $FF_RESULT_DIR/verifyhosts.res|egrep 'FAIL'
# update punchlist using new failures
tail -n +$resultlineno $FF_RESULT_DIR/verifyhosts.res| egrep 'FAIL'|append_punchlist
date >> $FF_RESULT_DIR/verifyhosts.res

job_cleanup

# upload the result file from each host
if [ z"$upload_file" != z ]
then
	echo "Uploading $FF_HOSTVERIFY_DIR/hostverify.res to $UPLOADS_DIR/$upload_file ..."| tee -a $FF_RESULT_DIR/verifyhosts.res
	opauploadall -p $FF_HOSTVERIFY_DIR/hostverify.res $upload_file 2>&1|tee -a $FF_RESULT_DIR/verifyhosts.res
	date >> $FF_RESULT_DIR/verifyhosts.res
fi
