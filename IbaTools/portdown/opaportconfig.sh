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

# opaportconfig arguments
# -s speed = new link speeds enabled (default is 0)
#        0 - no-op
#        1 - 2.5 Gb/s
#        2 - 5 Gb/s
#        4 - 10 Gb/s
#        to enable multiple speeds, use sum of desired speeds
#        255 - enable all supported
#
HFIPORTS="1:1"	# space separated list of HFI:PORT to act on
# other examples: (uncomment exactly 1)
#HFIPORTS="1:2"	# port 2 on HFI 1
#HFIPORTS="1:1 1:2"	# port 1 and 2 on HFI 1
#HFIPORTS="1:1 2:1"	# port 1 on HFI 1 and port 1 on HFI 2
# Below will force DDR
ARGS="-s 2"		# additional arguments to use for all HFIs/PORTs
# Other examples: (uncomment exactly 1)
#ARGS="-s 1"		# force SDR
#ARGS="-s 4"		# force QDR
#ARGS="-s 3"		# negotiate SDR or DDR
#ARGS="-s 7"		# negotiate SDR, DDR or QDR

# opaportconfig     use opaportconfig to force link speed
#
# chkconfig: 35 16 84
# description: Force HFI port speed
# processname:
### BEGIN INIT INFO
# Provides:       opaportconfig
# Required-Start: openibd
# Required-Stop:
# Default-Start:  2 3 5
# Default-Stop:	  0 1 2
# Description:    use opaportconfig to force link speed
### END INIT INFO

PATH=/bin:/sbin:/usr/bin:/usr/sbin

# just in case no functions script
echo_success() { echo "[  OK  ]"; }
echo_failure() { echo "[FAILED]"; }

my_rc_status_v()
{
	res=$?
	if [ $res -eq 0 ]
	then
		echo_success
	else
		echo_failure
	fi
	echo
	my_rc_status_all=$(($my_rc_status_all || $res))
}

if [ -f /etc/init.d/functions ]; then
    . /etc/init.d/functions
elif [ -f /etc/rc.d/init.d/functions ] ; then
    . /etc/rc.d/init.d/functions
elif [ -s /etc/rc.status ]; then
	. /etc/rc.status
	rc_reset
	my_rc_status_v()
	{
		res=$?
		[ $res -eq 0 ] || false
		rc_status -v
		my_rc_status_all=$(($my_rc_status_all || $res))
	}
fi

my_rc_status_all=0
my_rc_exit()     { exit $my_rc_status_all; }
my_rc_status()   { my_rc_status_all=$(($my_rc_status_all || $?)); }

cmd=$1

Usage()
{
	echo "Usage: $0 [start|stop|restart]" >&2
	echo " start       - configure port" >&2
	echo " stop        - Noop" >&2
	echo " restart     - restart (eg. stop then start) the Port config" >&2
	echo " status      - show present port configuration" >&2
	exit 2
}

start()
{
	local res hfi port hfiport

	echo -n "Starting opaportconfig: "
	res=0
	for hfiport in $HFIPORTS
	do
		hfi=$(echo $hfiport|cut -f1 -d:)
		port=$(echo $hfiport|cut -f2 -d:)
		echo -n "Setting HFI $hfi Port $port: "
 		/sbin/opaportconfig -h $hfi -p $port $ARGS >/dev/null
		res=$(($res || $?))
		[ $res -eq 0 ] || false
		my_rc_status_v
	done
	if [ $res -eq 0 ]
	then
		touch /var/lock/subsys/opaportconfig
	fi
	return $res
}

stop()
{
	local res

	echo -n "Stopping opaportconfig: "
	# nothing to do
	res=0
	my_rc_status_v
	rm -f /var/lock/subsys/opaportconfig
	return $res
}

status()
{
	local res hfi port hfiport

	echo -n "Checking port state: "
	res=0
	for hfiport in $HFIPORTS
	do
		hfi=$(echo $hfiport|cut -f1 -d:)
		port=$(echo $hfiport|cut -f2 -d:)
		echo -n "HFI $hfi "
		/sbin/opaportconfig -h $hfi -p $port -v|tail -15
		res=$(($res || $?))
		[ $res -eq 0 ] || false
		my_rc_status_v
	done
	return $res
}

case "$cmd" in
	start)
		start;;
	stop)
		stop;;
	restart)
		stop; start;;
	status)
		status;;
	*)
		Usage;;
esac

my_rc_exit
