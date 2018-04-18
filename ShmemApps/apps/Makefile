# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2017, Intel Corporation
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

# Makefile for SHMEM application/benchmark source for end user system
# targets:
#	full	- build all targets.
#	quick	- build commonly used apps.
#	all		- synonym for quick
#	clobber - remove all built files.
#
#	The following targets build the matching applications. If the
#	application name differs from the target name, it is listed
#	in parens.
#
#	HELLO
#

ifndef SHMEM_PREFIX
SOS_DEVEL_RPM=sandia-openshmem_gcc_hfi-devel
export SHMEM_PREFIX=$(shell rpm -ql $(SOS_DEVEL_RPM) |grep -e "bin/oshcc" |sed -e "s%/bin/oshcc%%")
ifeq ($(SHMEM_PREFIX),)
$(error SHMEM_PREFIX not set)
endif

endif


ifndef SHMEMCC
export SHMEMCC=$(SHMEM_PREFIX)/bin/oshcc
endif
ifndef SHMEM_INCLUDE_DIR
export SHMEM_INCLUDE_DIR=$(SHMEM_PREFIX)/include
endif
ifndef SHMEM_LIB_DIR
export SHMEM_LIB_DIR=$(SHMEM_PREFIX)/lib64
endif

all:: INCLUDES LIBS CMDS
.PHONY: all

HELLO: SHOWSHMEM
	@echo "Building SHMEM Hello..."
	$(SHMEMCC) -I$(SHMEM_INCLUDE_DIR) -o shmem-hello shmem-hello.c;
	@echo
.PHONY: HELLO

cleandepend clean:: clobber
.PHONY: cleandepend clean

clobber::
	rm -f shmem-hello shmem-hello.o
	rm -f .shmem_prefix
.PHONY: clobber

SHOWSHMEM:
	@echo "Using SHMEM_PREFIX=$(SHMEM_PREFIX)"
	echo "$(SHMEM_PREFIX)" > .shmem_prefix
.PHONY: SHOWSHMEM

depend::
force::
INCLUDES::
# do the real work during LIBS
LIBS::
.PHONY: depend force INCLUDES LIBS

quick:: HELLO
	@echo "Built subset of sample applications"
.PHONY: quick

CMDS:: HELLO
	@echo "Built sample application"
.PHONY: CMDS

full:: CMDS
	@echo "Built full set of sample applications"
.PHONY: full

STAGE::
INSTALL::

RUNTEST::
build_dsfiles::
clobber_dsfiles::
lint::
cflow::

.PHONY: STAGE INSTALL RUNTEST build_dsfiles clobber_dsfiles lint cflow
