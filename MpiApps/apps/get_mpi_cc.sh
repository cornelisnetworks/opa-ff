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

#[ICS VERSION STRING: unknown]
#!/bin/bash
#
# Sets the MPICH_CC_TYPE variable to match the CC compiler being used 
# by the current mpicc command. 
#
# Usage:
#
# source get_mpi_cc.sh
#
# -- or --
#
# export MYVAR=`get_mpi_cc.sh`




if [ -e .prefix ]; then
	prefix=`cat .prefix`
elif [ ! -z "$MPICH_PREFIX" ]; then
	prefix="$MPICH_PREFIX"
else
	prefix=`which mpicc 2>/dev/null | sed 's:bin/mpicc::g'`
fi

$prefix/bin/mpicc -v 2>&1 | grep -w "icc" | grep -q -v "mpicc"; isintel=$?
$prefix/bin/mpicc -v 2>&1 | grep -w "gcc" | grep -q -v "icc"; isgcc=$?
$prefix/bin/mpicc -v 2>&1 | grep -w "pgi" | grep -q -v "icc"; ispgi=$?

#echo "intel = $isintel; pgi = $ispgi; gcc = $isgcc"


if [ $isgcc -eq 0 ]; then 
	export MPICH_CC_TYPE="gcc"
elif [ $ispgi -eq 0 ]; then
	export MPICH_CC_TYPE="pgi"
elif [ $isintel -eq 0 ]; then
	export MPICH_CC_TYPE="intel"
else
	export MPICH_CC_TYPE="UNKNOWN_MPI_CC"
fi

echo $MPICH_CC_TYPE
