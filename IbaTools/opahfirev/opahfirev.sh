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

# [ICS VERSION STRING: @(#) ./bin/opahfirev 0mwhe.20151019 [10/19/15 15:19]
function nn_to_sockets()
{
#determine socket from the corresponding numa node

	local numa_node="$1"
	cmdout="$(lscpu -p=SOCKET,NODE 2>&1)"

	#parse output of lscpu
	while read -r line
	do
		if [ "${line:0:1}" == "#" ]
		then continue
		fi
		sktval="${line%%,*}"
		nodeval="${line##*,}"
		if [ "${nodeval}" -eq "${numa_node}" ]
		then
			echo $sktval 
			return 0
		fi
	done <<< "${cmdout}"
	return 1

}

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

hfis=`lspci -nn -D -d '8086:*' | egrep -e "24f[01]|Omni-Path" | grep -vi bridge | cut -d\  -f1`

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
		instance=""

		localbus_info=$(lspci -vv -s "${hfi}" | 
			grep LnkSta: | 
			sed -e 's/^[[:space:]]*LnkSta:[[:space:]]*//' | 
			cut -d, -f1-2)

		driver=${BASEDIR}/${hfi}/infiniband/
		if [ -e ${driver} ]
		then
			instance=`ls ${driver} 2>/dev/null`
		fi

		if [ -z ${instance} ]	
		then
			instance="Driver not Loaded"
		elif [ ! -e ${driver} -o ! -e ${driver}/${instance} ]
		then
			instance="Driver not Loaded"
		else
			hfinum=${instance#*_}
			hfinum=`echo "${hfinum} +1" | bc`
			eval 2>/dev/null read boardver < ${driver}/${instance}/boardversion
			eval 2>/dev/null read serial < ${driver}/${instance}/serial
			eval 2>/dev/null read guid < ${driver}/${instance}/node_guid
			eval 2>/dev/null read hw_rev < ${driver}/${instance}/hw_rev
			case "$hw_rev" in
			"0") 
				hw_string="A0";;
			"1") 
				hw_string=" A1";;
			"10") 
				hw_string="B0";;
			"11") 
				hw_string="B1";;
			#note Add new HW rev codes here. This list was taken from WFR HAS CceRevision
			esac

			/usr/sbin/opatmmtool -h ${hfinum} 2>/dev/null 1>/dev/null
			tmm=$?
			if [ $tmm -eq 0 ]
			then
				tmmver=`/usr/sbin/opatmmtool -h ${hfinum} fwversion | sed s/"Current Firmware Version="//`
			fi
		fi

		pci_id=`lspci -n -D -d 0x8086:* | grep ${hfi} | egrep -o 24f[01]`
		if [ -z "${pci_id}" ]
		then
			echo "Error identifying HFI's on the PCI bus">&2
			type="NA"
			pci_slot="NA"
		else
			if [ "$pci_id" = "24f0" ] 
			then
				type="Discrete"
				pci_slot="${hfi##*:}"
				pci_slot="${pci_slot%%.*}"
			else
				type="Integrated"
			fi
		fi

	
		nn=`cat /sys/bus/pci/devices/$hfi/numa_node`
		res=$?
		if [ "${res}" -ne 0 ]
		then
			nn="NA"
			sckt="NA"
		else
			sckt=$(nn_to_sockets ${nn})
			res=$?
			if [ "${res}" -ne 0 ]
			then
				sckt="NA"
			fi
		fi

		if [ "${instance}" = "Driver not Loaded" ]
			then
				hfi_id="_NA"
			else
				hfi_id=${instance#*_}
		fi

		echo "HFI:   $instance"
		echo "Board: $boardver"
		echo "SN:    $serial"
		if [ "${type}" = "Discrete" ]
		then
			echo "Location:$type  Socket:$sckt PCISlot:$pci_slot NUMANode:$nn  HFI$hfi_id"
		else
			echo "Location:$type  Socket:$sckt  NUMANode:$nn  HFI$hfi_id"
		fi
		echo "Bus:   ${localbus_info}"
		echo "GUID:  $guid"

		if [ -z $hw_string ]
		then
			echo "SiRev: $hw_rev"
		else
			echo "SiRev: $hw_string ($hw_rev)"
		fi

		if [ $tmm -eq 0 ]
		then
			echo "TMM:   $tmmver"
		fi
		echo "######################"
	done
fi
