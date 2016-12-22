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

# This script gathers up the FF and mpi_apps source code into tarballs
# and then makes .src and binary rpms from the tarballs and spec files

set -x
{

export FF_BUILD_ARGS=$*

FILES_TO_TAR=
# if in-place build left builtbin, builtinclude and builtlibs, pick them up
# to accelerate incremental builds
for i in builtbin builtinclude builtlibs builtinplace
do
	if [ -e $TL_DIR/$i.$PRODUCT.$BUILD_CONFIG ]
	then
		FILES_TO_TAR="$FILES_TO_TAR $i.$PRODUCT.$BUILD_CONFIG"
	fi
done

FILES_TO_TAR=$FILES_TO_TAR" -T tar_manifest"
MPIAPPS_FILES_TO_TAR="-T mpiapps_tar_manifest" 

RPMDIR="$TL_DIR/rpmbuild"
#rm -rf $RPMDIR
mkdir -p $RPMDIR/{BUILD,SPECS,BUILDROOT,SOURCES,RPMS,SRPMS}

cp opa.spec.in opa.spec
sed -i "s/__RPM_VERSION/$RPM_VER/g" opa.spec
sed -i "s/__RPM_RELEASE/$RPM_REL%{?dist}/g" opa.spec
./update_opa_spec.sh

cp mpi-apps.spec.in mpi-apps.spec
sed -i "s/__RPM_VERSION/$RPM_VER/g" mpi-apps.spec
sed -i "s/__RPM_RELEASE/$RPM_REL%{?dist}/g" mpi-apps.spec

tar cvzf $RPMDIR/SOURCES/opa.tgz -C $TL_DIR $FILES_TO_TAR --exclude-vcs --ignore-case --exclude="./rpmbuild" -X tar_excludes
tar cvzf $RPMDIR/SOURCES/opa-mpi-apps.tgz -C $TL_DIR $MPIAPPS_FILES_TO_TAR --exclude-vcs --ignore-case

mv opa.spec $RPMDIR/SPECS/
mv mpi-apps.spec $RPMDIR/SPECS/
cd $RPMDIR
}
	
rpmbuild -ba --define "_topdir $RPMDIR" SPECS/opa.spec
rpmbuild -ba --define "_topdir $RPMDIR" SPECS/mpi-apps.spec
