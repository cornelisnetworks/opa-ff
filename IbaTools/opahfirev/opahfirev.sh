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

Usage()
{
	cat <<EOF >&2
This scans the system and reports hardware and firmware information about
all the HFIs in the system.
EOF
	exit 2
}

verbose=n
while getopts v param
do
	case $param in
	v)	verbose=y;;
	?)	Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -ge 1 ]
then
	Usage
fi

# used to be:
# when output was:
# hfis=`/sbin/lspci | egrep -i 'InfiniBand' | grep -vi bridge | cut -d\  -f1`
# 83:00.0 InfiniBand: QLogic Corp. IBA7322 QDR InfiniBand HFI (rev 02)
# now output is either:
# 83:00.0 InfiniBand: Intel Corporation Device 24f0
# 83:00.0 Network controller: Intel Corporation Device 24f0
hfis=`/sbin/lspci | egrep -wi 'InfiniBand|Intel Corporation Device 24f0' | grep -vi bridge | cut -d\  -f1`

if [ -z "$hfis" ]
then 
	echo "No HFIs found."
else
	for hfi in $hfis
	do
		echo "######################"
		echo `hostname` " - HFI $hfi"
		# prepend to pci address as needed
		if [ -e "/sys/bus/pci/drivers/ib_wfr_lite/0000:$hfi" ]
		then
			hfi_pci="0000:$hfi"
			#dir=/sys/bus/pci/drivers/ib_wfr_lite/$hfi_pci
			dir=/sys/class/infiniband
			instance=$(cd $dir; echo wfr_lite[0-9]*)
		elif [ -e "/sys/bus/pci/drivers/hfi1/0000:$hfi" ]
		then
			hfi_pci="0000:$hfi"
			dir=/sys/class/infiniband
			instance=$(cd $dir; echo hfi1_[0-9]*)
		fi

		if [ -e "/sys/class/infiniband/$instance" ]
		then
			dir=/sys/class/infiniband/$instance
			eval 2>/dev/null read boardver < $dir/boardversion
			eval 2>/dev/null read serial < $dir/serial
			eval 2>/dev/null read localbus_info < $dir/localbus_info
			eval 2>/dev/null read guid < $dir/node_guid
			echo "Board: $boardver"
			echo "SN:    $serial"
			echo "Bus:   $localbus_info"
			echo "GUID:  $guid"
		fi

		echo "######################"
	done
fi
