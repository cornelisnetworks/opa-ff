#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
#
# Copyright (c) 2024, Tactical Computing Labs, LLC
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

#[ICS VERSION STRING: unknown]

id=$(./get_id_and_versionid.sh | cut -f1 -d' ')
versionid=$(./get_id_and_versionid.sh | cut -f2 -d' ')

if [ "$id" = "rhel" -o "$id" = "centos" -o "$id" = "rocky" -o "$id" = "ubuntu" ]
then
	GE_8_0=$(echo "$versionid >= 8.0" | bc)
	sed -i "s/__RPM_REQ/Requires: atlas/" mpi-apps.spec
	if [ $GE_8_0 = 1 ]
	then
		sed -i "s/__RPM_DBG/%global debug_package %{nil}/" mpi-apps.spec
	else
		sed -i "/__RPM_DBG/,+1d" mpi-apps.spec
	fi
elif [ "$id" = "fedora" ]
then
	sed -i "s/__RPM_REQ/Requires: atlas/" mpi-apps.spec
	sed -i "s/__RPM_DBG/%global debug_package %{nil}/" mpi-apps.spec
elif [ "$id" = "sles" ]
then
	sed -i "/__RPM_REQ/,+1d" mpi-apps.spec
	sed -i "/__RPM_DBG/,+1d" mpi-apps.spec
else
	echo ERROR: Unsupported distribution: $id $versionid
	exit 1
fi

exit 0
