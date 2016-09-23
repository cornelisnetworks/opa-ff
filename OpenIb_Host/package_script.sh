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

set -x
{

export FF_BUILD_ARGS=$*

#Will need updated module version.
echo "export MODULEVERSION=${MODULEVERSION}" >> build.env

BASE_DIR=`pwd`
FILES_TO_TAR="-T tar_manifest_secondary"

# TODO can this be removed for cvsgitall1?
if [ `basename $(pwd)` != "OPENIB_FF" ]
then
	BASE_DIR=`readlink -f $(pwd)/..`
	FILES_TO_TAR="OpenIb_Host"
fi

FILES_TO_TAR=$FILES_TO_TAR" -T tar_manifest_primary"
MPIAPPS_FILES_TO_TAR="-T mpiapps_tar_manifest" 

RPMDIR="$BASE_DIR/rpmbuild"
#rm -rf $RPMDIR
mkdir -p $RPMDIR/{BUILD,SPECS,BUILDROOT,SOURCES,RPMS,SRPMS}

cp opa.spec.in opa.spec
sed -i "s/__RPM_VERSION/$RPM_VER/g" opa.spec
sed -i "s/__RPM_RELEASE/$RPM_REL%{?dist}/g" opa.spec

cp mpi-apps.spec.in mpi-apps.spec
sed -i "s/__RPM_VERSION/$RPM_VER/g" mpi-apps.spec
sed -i "s/__RPM_RELEASE/$RPM_REL%{?dist}/g" mpi-apps.spec

tar czf $RPMDIR/SOURCES/opa.tgz -C $BASE_DIR $FILES_TO_TAR --exclude-vcs --ignore-case --exclude="./rpmbuild" -X tar_excludes
tar czf $RPMDIR/SOURCES/opa-mpi-apps.tgz -C $BASE_DIR $MPIAPPS_FILES_TO_TAR --exclude-vcs --ignore-case

mv opa.spec $RPMDIR/SPECS/
mv mpi-apps.spec $RPMDIR/SPECS/
cd $RPMDIR
}
set +x
	
rpmbuild -ba --define "_topdir $RPMDIR" SPECS/opa.spec
rpmbuild -ba --define "_topdir $RPMDIR" SPECS/mpi-apps.spec


