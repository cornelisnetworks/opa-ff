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
# setup password-less ssh on a single host so it can ssh to itself
# This is part of IntelOPA-Basic for use by opasetupssh in FastFabric

trap "exit 1" SIGHUP SIGTERM SIGINT

Usage()
{
	echo "Usage: setup_self_ssh [-U] [-i ipoib_hostname]" >&2
	echo "              or" >&2
	echo "       setup_self_ssh --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -U - only perform connect (to enter in local hosts knownhosts)" >&2
	echo "   -i ipoib_hostname - ipoib hostname for this host" >&2
	echo "         default is to skip setup for ipoib to self" >&2
	echo "example:">&2
	echo "   setup_self_ssh -i myhost-ib" >&2
	echo "   setup_self_ssh -U" >&2
	exit 2
}

if [ x"$1" = "x--help" ]
then
	Usage
fi

user=`id -u -n`
Uopt=n
ihost_ib=
while getopts Ui: param
do
	case $param in
	U)
		Uopt=y;;
	i)
		ihost_ib="$OPTARG";;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))
if [ $# -gt 0 ]
then
	Usage
fi

# connect to host via ssh
connect_to_host()
{
	# $1 = user
	# $2 = host

	# We use an alternate file to build up the new keys
	# this way parallel calls can let ssh itself handle file locking
	# then update_known_hosts can replace data in real known_hosts file
	ssh -o 'UserKnownHostsFile=~/.ssh/.known_hosts-ffnew' -o 'StrictHostKeyChecking=no' $1@$2 echo "$2: Connected"
}

# update known_hosts file with information from connect_to_host calls
update_known_hosts()
{
	if [ -e ~/.ssh/.known_hosts-ffnew ]
	then
		if [ -e ~/.ssh/known_hosts ]
		then
			(
			IFS=" ,	"
			while read name trash
			do
				# remove old entry from known hosts in case key changed
				if grep "^$name[, ]" < ~/.ssh/known_hosts > /dev/null 2>&1
				then
					grep -v "^$name[, ]" < ~/.ssh/known_hosts > ~/.ssh/.known_hosts-fftmp
					mv ~/.ssh/.known_hosts-fftmp ~/.ssh/known_hosts
				fi
			done < ~/.ssh/.known_hosts-ffnew
			)
		fi
		cat ~/.ssh/.known_hosts-ffnew >> ~/.ssh/known_hosts
		chmod go-w ~/.ssh/known_hosts

		rm -rf ~/.ssh/.known_hosts-ffnew ~/.ssh/.known_hosts-fftmp
	fi
}

# Generate public and private SSH key pairs
cd ~
mkdir -m 0700 -p ~/.ssh ~/.ssh2
if [ ! -f .ssh/id_rsa.pub -o ! -f .ssh/id_rsa ]
then
	ssh-keygen -t rsa -N '' -f ~/.ssh/id_rsa 
fi
if [ ! -f .ssh/id_dsa.pub -o ! -f .ssh/id_dsa ]
then
	ssh-keygen -t dsa -N '' -f ~/.ssh/id_dsa
fi
# recreate public key in Reflection format for ssh2
if [ ! -e .ssh2/ssh2_id_dsa.pub ]
then
	# older distros may not support this, ignore error
	ssh-keygen -O ~/.ssh/id_dsa.pub -o ~/.ssh2/ssh2_id_dsa.pub 2>/dev/null
fi

# configure ssh on the host(s)
rm -rf ~/.ssh/.known_hosts-ffnew ~/.ssh/.known_hosts-fftmp

#stty_settings=`stty -g`
ihost=`hostname | cut -f1 -d.`

# setup ssh to ourselves
if [ $Uopt = n ]
then
	echo "$ihost: Configuring localhost ssh..."
	rm -f ~/.ssh/.tmp_keys$$

	>> ~/.ssh/authorized_keys
	cat ~/.ssh/authorized_keys ~/.ssh/id_rsa.pub ~/.ssh/id_dsa.pub|sort -u > ~/.ssh/.tmp_keys$$
	mv ~/.ssh/.tmp_keys$$ ~/.ssh/authorized_keys

	>> ~/.ssh/authorized_keys2
	cat ~/.ssh/authorized_keys2 ~/.ssh/id_rsa.pub ~/.ssh/id_dsa.pub|sort -u > ~/.ssh/.tmp_keys$$
	mv ~/.ssh/.tmp_keys$$ ~/.ssh/authorized_keys2

	# set up ssh2 DSA authorization
	# older distros may not support this
	[ -f ~/.ssh2/ssh2_id_dsa.pub ] && !  grep '^Key ssh2_id_dsa.pub$' ~/.ssh2/authorization >/dev/null 2>&1 && echo "Key ssh2_id_dsa.pub" >> ~/.ssh2/authorization

	chmod go-w ~/.ssh/authorized_keys ~/.ssh/authorized_keys2
	test -f ~/.ssh2/authorization && chmod go-w ~/.ssh2/authorization
fi

echo "$ihost: Verifying localhost ssh..."
# make sure we can ssh ourselves so ipoibping test works
connect_to_host $user localhost
connect_to_host $user $ihost
# make sure we can ssh to ourselves over ipoib, so MPI can be run on master
if [ x"$ihost_ib" != "x" ]
then
	connect_to_host $user $ihost_ib
fi
echo "$ihost: Configured localhost ssh"

#stty $stty_settings
update_known_hosts
