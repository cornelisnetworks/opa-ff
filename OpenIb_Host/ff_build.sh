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

# This is the main build script used within the FF src rpm's %build operation

# Import the build environment
. ./build.env

export BUILD_PLATFORM="LINUX"

source ../MakeTools/funcs-ext.sh

settarget RISCV64
settl

export BUILD_TARGET_OS_VERSION=`uname -r`

# This is a user level file to build the basic project
# One must run target and setver before invoking this script.
# One should set the targeted stack by uncommenting one of the lines below.
export BUILD_WITH_STACK=${BUILD_WITH_STACK:-OPENIB}

# Simple script to perform builds for current system/OS type
export BUILD_TARGET=${BUILD_TARGET:-`uname -m`}
case $BUILD_TARGET in
i?86)	BUILD_TARGET=ia32;;
esac

export BUILD_CONFIG=${BUILD_CONFIG:-"debug"}
export PRODUCT=${PRODUCT:-OPENIB_FF}

# for FF the kernel rev is not important.  We simply use the kernel rev
# of the running kernel.  While BUILD_TARGET_OS_VERSION is needed by Makerules
# it will have no impact on what is actually built for FF
export BUILD_TARGET_OS_VERSION=${BUILD_TARGET_OS_VERSION:-`uname -r`}
setver $BUILD_TARGET_OS_VENDOR $BUILD_TARGET_OS_VERSION
export OPA_FEATURE_SET=opa10

MODULEVERSION=`$TL_DIR/MakeTools/format_releasetag.sh $RELEASE_TAG`
RELEASE_STRING=CornelisOPX-Tools-FF.$BUILD_TARGET_OS_ID.$MODULEVERSION
echo "stage.OPENIB_FF.$BUILD_CONFIG/$BUILD_TARGET_OS_VENDOR/$BUILD_TARGET/$RELEASE_STRING" > $1/RELEASE_PATH
echo "../bin/$BUILD_TARGET/$BUILD_PLATFORM_OS_VENDOR.$BUILD_PLATFORM_OS_VENDOR_VERSION/lib/$BUILD_CONFIG/" > $1/LIB_PATH
shift

set -x
{
	echo "Environment:"
	env
	echo "----------------------------------------------------------------------------"
	echo
	./rpm_runmake -B ${BUILD_CONFIG} $*
} 2>&1|tee build.res
set +x

# Check the results of the build for errors and unexpected warnings.
./check_results -r build.res build.err build.warn
