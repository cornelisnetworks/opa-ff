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

# Call opagenchassis and iterate through the list of chassis to 
# generate esm_chassis file by finding out all chassis where embedded
# subnet manager is running. 

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
	echo "Usage: opagenesmchassis [-u user] [-S] [-t portsfile] [-p ports]" >&2
	echo "              or" >&2
	echo "       opagenesmchassis --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -u user - user to perform cmd as" >&2
	echo "           for chassis default is admin" >&2
	echo "   -S - securely prompt for password for user on chassis" >&2
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
	echo " Environment:" >&2
	echo "   FF_CHASSIS_ADMIN_PASSWORD - password for chassis, used in absence of -S" >&2
        echo "   PORTS - list of ports, used in absence of -t and -p" >&2
	echo "   PORTS_FILE - file containing list of ports, used in absence of -t and -p" >&2
	echo "for example:" >&2
	echo "   opagenesmchassis" >&2
	echo "   opagenesmchassis -S -p '1:1 1:2 2:1 2:2'" >&2
	exit 0
}

Usage()
{
	echo "Usage: opagenesmchassis [-u user] [-S]" >&2
	echo "              or" >&2
	echo "       opagenesmchassis --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -u user - user to perform cmd as" >&2
	echo "           for chassis default is admin" >&2
	echo "   -S - securely prompt for password for user on chassis" >&2
	echo "for example:" >&2
	echo "   opagenesmchassis" >&2
	echo "   opagenesmchassis -S " >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage_full
fi


status=ok
user=`id -u -n`
uopt=n
Sopt=n
while getopts p:t:u:S param
do
	case $param in
	p)	export PORTS="$OPTARG";;
	t)	export PORTS_FILE="$OPTARG";;
	u)	uopt=y
		user="$OPTARG";;
	S)	Sopt=y;;		
	?)	Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -ge 1 ]
then
	Usage
fi

check_ports_args opagenesmchassis

if [ "$uopt" = n ]
then
	user=admin
fi
for chassis in `/usr/sbin/opagenchassis`; 
do
	if [ "$Sopt" = y ]
	then
		chassis_cmd=`/usr/sbin/opacmdall -C -H $chassis -u $user -S 'smControl status' 2>&1` 
	else
		chassis_cmd=`/usr/sbin/opacmdall -C -H $chassis -u $user 'smControl status' 2>&1`
	fi

	if [ $? != 0 ]	
	then
		status=bad
	elif grep -q "FAILED" <<<$chassis_cmd
	then
		status=bad
	elif grep -q "not started" <<<$chassis_cmd
	then
		status=bad
	elif grep -q "stopped" <<<$chassis_cmd
	then
		status=bad
	elif grep -q "not licensed" <<<$chassis_cmd
	then
		status=ok
	else
		echo "$chassis" 
		status=ok
	fi
	
done

if [ "$status" != "ok" ]
then
	exit 1
else
	exit 0
fi
