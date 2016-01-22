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

# [ICS VERSION STRING: @(#) ./bin/opahfirev 0mwhe.20151019 [10/19/15 15:19]

Usage()
{
	cat <<EOF >&2
This scans the system and reports hardware and firmware information about
all the HFIs in the system.
EOF
	exit 2
}

if [ $# -ge 1 ]
then
	Usage
fi

BASEDIR=/sys/bus/pci/devices

hfis=`/sbin/lspci -Dd '8086:*' | egrep -e "24f[01]|Omni-Path" | grep -vi bridge | cut -d\  -f1`

if [ -z "$hfis" ]
then 
	echo "No HFIs found."
else
	for hfi in $hfis
	do
		echo "######################"
		echo `hostname` " - HFI $hfi"

		boardver="UNKNOWN"
		serial="UNKNOWN"
		guid="UNKNOWN"
		tmmver="UNKNOWN"
		tmm=0

		localbus_info=$(lspci -vv -s "${hfi}" | 
			grep LnkSta: | 
			sed -e 's/^[[:space:]]*LnkSta:[[:space:]]*//' | 
			cut -d, -f1-2)

		driver=${BASEDIR}/${hfi}/infiniband/
		if [ -e ${driver} ]
		then
			instance=`ls ${driver} 2>/dev/null`
		fi

		if [ ! -e ${driver} -o ! -e ${driver}/${instance} ] 
		then
			instance="Driver not Loaded"
		else
			eval 2>/dev/null read boardver < ${driver}/${instance}/boardversion
			eval 2>/dev/null read serial < ${driver}/${instance}/serial
			eval 2>/dev/null read guid < ${driver}/${instance}/node_guid
			/usr/sbin/opatmmtool 2>/dev/null 1>/dev/null
			tmm=$?
			if [ $tmm -ne 3 ]
			then
				tmmver=`/usr/sbin/opatmmtool fwversion | sed s/"Current Firmware Version="//`
			fi
		fi

		echo "HFI:   $instance"
		echo "Board: $boardver"
		echo "SN:    $serial"
		echo "Bus:   ${localbus_info}"
		echo "GUID:  $guid"
		if [ $tmm -ne 3 ]
		then
			echo "TMM:   $tmmver"
		fi
		echo "######################"
	done
fi
