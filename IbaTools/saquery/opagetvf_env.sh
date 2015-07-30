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

# This script selects a VF based on the parameters given
# It will return exactly 1 VF, represented in a CSV format
# 			name:index:pkey:sl:mtu:rate
# if there is no matching VF, a non-zero exit code is returned
# this script is intended for use in mpirun and wrapper scripts to
# select a VF and aid in exported env variables

# convert opagetvf output to environment variables
# MTU and RATE represented as absolute values
opagetvf_env_func()
{
	# $1 = vf cvs string from opagetvf
	# $2 = PKEY env variable to set
	# $3 = SL env variable to set
	# $4 = MTU env variable to set
	# $5 = RATE env variable to set
	local pkey sl mtu rate
	[ -z "$1" ] && return 1

	pkey=$(echo "$1"|cut -d : -f 3)
	sl=$(echo "$1"|cut -d : -f 4)
	mtu=$(echo "$1"|cut -d : -f 5)
	rate=$(echo "$1"|cut -d : -f 6)

	[ ! -z "$2" ] && eval export $2=$pkey
	[ ! -z "$3" ] && eval export $3=$sl
	[ ! -z "$4" -a "$mtu" != "unlimited" ] && eval export $4=$mtu
	[ ! -z "$5" -a "$rate" != "unlimited" ] && eval export $5=$rate
        return 0
}

# query vfinfo and put into env variables
# MTU and RATE represented as absolute values
opagetvf_func()
{
	# $1 = arguments to opagetvf to select VF
	# $2 = PKEY env variable to set
	# $3 = SL env variable to set
	# $4 = MTU env variable to set
	# $5 = RATE env variable to set
	local vf

	vf=$(eval opagetvf $1)
        if [ $? -ne 0 ]
        then
            return 1
        else
            opagetvf_env_func "$vf" $2 $3 $4 $5
        fi
        return 0
}

# convert opagetvf output to environment variables
# MTU and RATE represented as enum values
opagetvf2_env_func()
{
	# $1 = vf cvs string from opagetvf
	# $2 = PKEY env variable to set
	# $3 = SL env variable to set
	# $4 = MTU env variable to set
	# $5 = RATE env variable to set
	local pkey sl mtu rate
	[ -z "$1" ] && return 1

	pkey=$(echo "$1"|cut -d : -f 3)
	sl=$(echo "$1"|cut -d : -f 4)
	mtu=$(echo "$1"|cut -d : -f 5)
	rate=$(echo "$1"|cut -d : -f 6)

	[ ! -z "$2" ] && eval export $2=$pkey
	[ ! -z "$3" ] && eval export $3=$sl
	[ ! -z "$4" -a "$mtu" != "0" ] && eval export $4=$mtu
	[ ! -z "$5" -a "$rate" != "0" ] && eval export $5=$rate
        return 0
}

# query vfinfo and put into env variables
# MTU and RATE represented as enum values
opagetvf2_func()
{
	# $1 = arguments to opagetvf to select VF
	# $2 = PKEY env variable to set
	# $3 = SL env variable to set
	# $4 = MTU env variable to set
	# $5 = RATE env variable to set
	local vf

	vf=$(eval opagetvf -e $1)
        if [ $? -ne 0 ]
        then
            return 1
        else
            opagetvf2_env_func "$vf" $2 $3 $4 $5
        fi
        return 0
}

iba_getpsm_func()
{
	# $1 = arguments to to select PSM multi-rails 

        rc=0
        # extract <unit:port> parameter values
        echo "$1"|tr \, \\n > .mrail_map_file.$$

        # validate parameter values       
        while read line
        do
            unit=${line%:*}
            port=${line#*:}

            # verify unit number
            if ! [[ $unit =~ ^[0-9]+$ ]]
            then
                rc=1
        	echo "iba_getpsm_func: Invalid unit number"
            fi
            # verify port number
            if ! [[ $port =~ ^[0-9]+$ ]];
            then
                rc=1
        	echo "iba_getpsm_func: Invalid port number"
            fi
        done <.mrail_map_file.$$

        # remove temp file        
        rm .mrail_map_file.$$

        return $rc
}

