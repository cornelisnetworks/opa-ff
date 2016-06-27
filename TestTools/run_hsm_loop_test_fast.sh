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

#[ICS VERSION STRING: unknown]
#!/bin/bash

INJECT_NUM_PKTS=5
start_test=0
end_test=0
inject=0
redundancy=0
length=0
SM_DIAG_PATH=/usr/lib/opa-fm/bin/fm_cmd
MIN_ISL_REDUNDANCY=4
PATH_LENGTH=4

Usage()
{

	echo "Usage: $0 [-s] [-e] [-i num] [-r num] [-l len] [-d path_to_fm_cmd]"
	echo "		 -s : start loop test in fast mode with default $INJECT_NUM_PKTS packets or -i option can be used to specify number of packets."
	echo "		 -e : end loop test and turn off fast mode"
	echo "		 -i num : inject num packets. Can also be used after starting the test, to inject more packets."
	echo "		 -r num : Set loop test's MinISLRedundancy to num. This will include each ISL in num different loops."
	echo "		 -l len : Set loop test path length to len. Loop paths will be of length <=len"
	echo "		 -d path : Run fm_cmd binary at path, instead of the default $SM_DIAG_PATH"
	echo "		 -h : shows usage"

	exit 1
}

if [ $# -eq 0 ]; then
	Usage
	exit 1
fi

while getopts sehi:r:l:d: param
do
	case $param in
	s)
		start_test=1;;
	e)
		end_test=1;;
	i)
		inject=1;
		INJECT_NUM_PKTS="$OPTARG";;
	r)
		redundancy=1
		MIN_ISL_REDUNDANCY="$OPTARG";;
	l)
		length=1
		PATH_LENGTH="$OPTARG";;
	d)
		SM_DIAG_PATH="$OPTARG";;
	h)
		Usage;;

	?)
		Usage;;
	esac
done

shift $((OPTIND -1))

if [ ! -f $SM_DIAG_PATH ];then
	echo "$SM_DIAG_PATH not found. fm_cmd is required for running loop test"
	exit 1
fi

if [ ! -x $SM_DIAG_PATH ];then
	echo "$SM_DIAG_PATH is not an executable. fm_cmd is required for running loop test"
	exit 1
fi


if [ $start_test -eq 1 ]; then
	echo "Turning on Loop test Fast Mode..."
	if ! $SM_DIAG_PATH smLooptestFastMode 1; then
		echo "Could not turn ON loop test fast mode. Command used: $SM_DIAG_PATH smLooptestFastMode 1"
		exit 1
	fi

	echo "Starting Loop test with $INJECT_NUM_PKTS packets..."
	if ! $SM_DIAG_PATH smLooptestStart $INJECT_NUM_PKTS; then
		echo "Could not start loop test. Command used: $SM_DIAG_PATH smLooptestStart $INJECT_NUM_PKTS"
		exit 1
	fi
	exit 0
fi

if [ $end_test -eq 1 ]; then
	echo "Stopping Loop test..."
	if ! $SM_DIAG_PATH smLooptestStop; then
		echo "Could not stop loop test. Command used: $SM_DIAG_PATH smLooptestStop"
		exit 1
	fi
	echo "Turning OFF Loop test Fast Mode..."
	if ! $SM_DIAG_PATH smLooptestFastMode 0; then
		echo "Could not turn OFF loop test fast mode. Command used: $SM_DIAG_PATH smLooptestFastMode 0"
	fi
	exit 0
fi

if [ $inject -eq 1 ]; then
	echo "Injecting $NUM_PKTS to Loop test..."
	if ! $SM_DIAG_PATH smLooptestInjectPackets $INJECT_NUM_PKTS; then
		echo "Could not inject packets. Command used: $SM_DIAG_PATH smLooptestInjectPackets $INJECT_NUM_PKTS"
		exit 1
	fi
	exit 0
fi

if [ $redundancy -eq 1 ]; then
	echo "Setting Loop test MinISLRedundancy to $MIN_ISL_REDUNDANCY..."
	if ! $SM_DIAG_PATH smLooptestMinISLRedundancy $MIN_ISL_REDUNDANCY; then
		echo "Could not set MinISLRedundancy. Command used: $SM_DIAG_PATH smLooptestMinISLRedundancy $MIN_ISL_REDUNDANCY"	
		exit 1
	fi
	exit 0
fi

if [ $length -eq 1 ]; then
	echo "Setting Loop test Path Length to $PATH_LENGTH..."
	if ! $SM_DIAG_PATH smLooptestPathLength $PATH_LENGTH; then
		echo "Could not set path length. Command used: $SM_DIAG_PATH smLooptestPathLength $PATH_LENGTH"	
		exit 1
	fi
	exit 0
fi


