#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
#
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

Usage()
{
	echo "build_tests.sh [-C]" >&2
	exit 2
}

Cflag=n
rerun=n
while getopts rC param
do
	case $param in
	r)	rerun=y;;
	C)	Cflag=y;;
	?)	Usage;;
	esac
done

set -x
wd=$PWD
testdir=$wd/tests_build

# TBD - maybe there can be a better way
# this copies the relevant directories to a place where we can do the
# build, this avoids clutter in the main tree and hence avoids clutter
# in the FF src.rpm, but it is creating extra copies of things which clutter
# any searches from the top of the tree
# perhaps we should rename testsdir to a dot filename or a different tree
if [ "$Cflag" != y ]
then
	# clobber old tree
	rm -rf $testdir
fi
mkdir -p $testdir
cp Makefile.tests $testdir/Makefile
cp *.project setenv $testdir
cd $TL_DIR
# -p will preserve timestamps (and ownership, etc) so that we will
# not rebuild unless we need to
cp -r -p Makerules MakeTools opamgt Xml Topology SrpTools InicTools Tests IbaTests IbaTools IbAccess MpiApps Fd $testdir
if [ "$Cflag" = y ]
then
	# if in-place FF build left builtbin, builtinclude and builtlibs, pick
	# them up to accelerate incremental builds
	for i in builtbin builtinclude builtlibs
	do
		if [ -e $i.$PRODUCT.$BUILD_CONFIG ]
		then
			cp -r -p $i.$PRODUCT.$BUILD_CONFIG $testdir
		fi
	done
fi


cd $testdir
export PROJ_FILE_DIR=.
set +x
. setenv
set -x
showenv
# TBD the code below should be moved back into the LINUX/Makefile as part of a
# package_tests target, then this can run make stage prepfiles package_tests
# and also avoid editing Makerules.project
sed -i 's/IntelOPA-Tools-FF/IntelOPA-Tests/' Makerules.project
make stage 2>&1 | tee make.res
#make prepfiles 2>&1 | tee make.res	# TBD should be enabled
if [ "x$RELEASE_TAG" = "x" ]
then
	STAGE_SUB_DIR=IntelOPA-Tests.$(MakeTools/patch_engineer_version.sh|cut -d"." -f1)
else
	STAGE_SUB_DIR=IntelOPA-Tests.$(format_releasetag $RELEASE_TAG)
fi
tarstage=$PWD/tarStage/$STAGE_SUB_DIR
mkdir -p $tarstage
if [ "x$PRODUCT" = "x" ]
then
	stagedir=stage.tests_build.$BUILD_CONFIG
else
	stagedir=stage.$PRODUCT.$BUILD_CONFIG
fi
tarbase=$(find $stagedir -name "IntelOPA-Tests*")
cd $tarbase
cd Tests
cp -r * $tarstage
cd ..
cp version $tarstage
cd $tarstage
cd ..
tar cfz $STAGE_SUB_DIR.tgz $STAGE_SUB_DIR
echo $PWD/$STAGE_SUB_DIR.tgz >>  $TL_DIR/../dist_files
echo $PWD/$STAGE_SUB_DIR.tgz >>  $TL_DIR/../packaged_files
