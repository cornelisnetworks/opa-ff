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
	echo "Usage: opasorthosts < hostlist > mpi_hosts" >&2
	echo "              or" >&2
	echo "       opasorthosts --help" >&2
	echo "   --help - produce full help text" >&2
	echo "Sort the hostlist alphabetically (case insensitively) then numerically" >&2
	echo "hostnames may end in a numeric field which may optionally have leading zeros" >&2
	echo "example input:">&2
	echo "	osd04" >&2
	echo "	osd1" >&2
	echo "	compute20" >&2
	echo "	compute3" >&2
	echo "	mgmt1" >&2
	echo "	mgmt2" >&2
	echo "	login" >&2
	echo "resulting output:">&2
	echo "	compute3" >&2
	echo "	compute20" >&2
	echo "	login" >&2
	echo "	mgmt1" >&2
	echo "	mgmt2" >&2
	echo "	osd1" >&2
	echo "	osd04" >&2
	exit 2
}

if [ $# -ne 0 ]
then
	Usage
fi

# use Cntr-C as a temporary delimiter since it should never appear in a name
# sort first by the alphanumeric 1st part of name,
# then by any purely numeric last part
sed -e 's/\([0-9]*\)$/\1/'|sort --ignore-case -k1,1 -k2,2n -t ''|sed -e 's///'
