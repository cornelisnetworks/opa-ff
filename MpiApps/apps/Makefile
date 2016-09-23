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

# Makefile for MPI application/benchmark source for end user system
# targets:
# 	full 	- build all targets.
#   basic   - build all applications provivded in opa-fastfabric package
# 	quick 	- build commonly used apps.
# 	all		- synonym for quick
# 	clobber - remove all built files.
#
# 	The following targets build the matching applications. If the
# 	application name differs from the target name, it is listed
# 	in parens.
#
# 	GROUPSTRESS	
# 	DEVIATION
# 	LAT (latency)
#	BW (bandwidth)
#	MULTIBW (multi-pair bandwidth)
#	OSU2 (version 2 of osu_latency, osu_bw, osu_bcast, osu_bibw)
#	OSU2_MPI2 (version 2 of osu MPI2 benchmarks)
#	OSU38 (version 3.8 of the OSU benchmarks)
#	IMB/PMB (Pallas)
#	XHPL2 (hpl v2)
#

ifndef MPICH_PREFIX
export MPICH_PREFIX=$(shell . ./select_mpi 2>/dev/null; echo $$MPICH_PREFIX)
endif

ifndef MPICC
export MPICC=$(MPICH_PREFIX)/bin/mpicc 
endif
ifndef MPIF77
export MPIF77=$(MPICH_PREFIX)/bin/mpif77 
endif
ifndef MPI_INCLUDE_DIR
export MPI_INCLUDE_DIR=$(MPICH_PREFIX)/include
endif
ifndef MPI_LIB_DIR
export MPI_LIB_DIR=/lib
endif

all:: INCLUDES LIBS CMDS
.PHONY: all

CHECK: SHOWMPI
	@echo "Building MPI Check..."
	export HOME_DIR=$(PWD); \
	make -C mpicheck
	@echo
.PHONY: CHECK

GROUPSTRESS: SHOWMPI
	@echo "Building groupstress..."
	export HOME_DIR=$(PWD); \
	make -C groupstress
	@echo
.PHONY: GROUPSTRESS
DEVIATION: SHOWMPI
	@echo "Building deviation..."
	export HOME_DIR=$(PWD); \
	make -C deviation
	@echo
.PHONY: DEVIATION

NVIDIA: SHOWMPI
	@echo "Building nvidia test apps..."
	export HOME_DIR=$(PWD); \
	make -C nvidia
	@echo
.PHONY: NVIDIA

LAT: SHOWMPI
	@echo "Building OSU latency..."
	cd latency;\
	$(MPICC) -I$(MPI_INCLUDE_DIR) -o latency latency.c;
	@echo
.PHONY: LAT

BW: SHOWMPI
	@echo "Building OSU bandwidth..."
	cd bandwidth;\
	$(MPICC) -I$(MPI_INCLUDE_DIR) -o bw bw.c;
	@echo
.PHONY: BW

MULTIBW: SHOWMPI
	@echo "Building multi bandwidth..."
	cd mpi_multibw;\
	$(MPICC) -I$(MPI_INCLUDE_DIR) -o mpi_multibw mpi_multibw.c;
	@echo
.PHONY: MUTLIBW

OSU2: SHOWMPI
	@echo "Building OSU 2.0 benchmarks..."
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_latency osu_latency.c
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_bw osu_bw.c
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_bcast osu_bcast.c
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_bibw osu_bibw.c
	@echo
.PHONY: OSU2

OSU38: SHOWMPI
	@echo "Building OSU 3.8 benchmarks..."
	cd osu-micro-benchmarks-3.8-July12; ./configure CC=$(MPICC); make
	@echo
.PHONY: OSU38

OSU2_MPI2: SHOWMPI
	@echo "Building OSU 2.0 benchmarks for MPI2..."
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_acc_latency osu_acc_latency.c
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_get_latency osu_get_latency.c
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_put_latency osu_put_latency.c
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_get_bw osu_get_bw.c
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_put_bw osu_put_bw.c
	cd osu2; $(MPICC) -I$(MPI_INCLUDE_DIR) -o osu_put_bibw osu_put_bibw.c
	@echo
.PHONY: OSU2_MPI2

PMB: SHOWMPI
	@echo "Building Pallas 2.4..."
	if [ -f $(MPICH_PREFIX)/lib/libmpich.a -o -f $(MPICH_PREFIX)/lib/shared/libmpich.so ] ; then export MPICH_LIB='-lmpich'; fi ; \
	export HOME_DIR=$(PWD); \
	make -C PMB2.2.1/SRC_PMB
	@echo
.PHONY: PMB

IMB: SHOWMPI
	@echo "Building Intel Micro Benchmarks 4.0.2..."
	cd imb/src; gmake MPI_HOME=$(MPICH_PREFIX) -f make_mpich
	@echo
.PHONY: PMB

XHPL2: SHOWMPI
	@echo "Building HPL 2.0 (Linpack)..."
	export HPL_HOME_DIR=$(PWD)/hpl-2.0;\
	make -C hpl-2.0 arch=ICS.`uname -s`.`./get_mpi_cc.sh`
	@echo
.PHONY: XHPL2

# PGFILE_TEST is not included in the stock mpi_apps package.
PGFILE: SHOWMPI
	@echo "Building pgfile test..."
	export HOME_DIR=$(PWD); make -C pgfile_test;
	@echo
.PHONY: PGFILE

# TACHYON is not included in the stock mpi_apps package.
TACHYON: SHOWMPI
	@echo "Building Tachyon demo..."
	rm -rf tachyon/compile/*
	if [ -f $(MPICH_PREFIX)/lib/libmpich.a -o -f $(MPICH_PREFIX)/lib/shared/libmpich.so ] ; then export MPICH_LIB='-lmpich'; fi 
	make -C tachyon/unix linux-vapi
	cp tachyon/compile/linux-vapi/tachyon tachyon/scenes
	@echo
.PHONY: TACHYON

# MANDEL is not included in the stock mpi_apps package.
MANDEL: SHOWMPI
	@echo "Building Mandel demo..."
	make -C mandel clean
	make -C mandel
	@echo
.PHONY: MANDEL

cleandepend clean:: clobber
.PHONY: cleandepend clean

clobber::
	rm -f latency/*.o latency/*.d latency/latency
	rm -f bandwidth/*.o bandwidth/*.d bandwidth/bw
	if [ -d "PMB2.2.1" ]; then make -C PMB2.2.1/SRC_PMB clean; fi
	if [ -f "imb/src/make_mpich" ]; then cd imb/src; gmake MPI_HOME=$(MPICH_PREFIX) -f make_mpich clean; fi
	if [ -d hpl-2.0 ]; then find hpl-2.0 -name ICS.`uname -s`.* | xargs rm -rf; rm -rf hpl-2.0/bin hpl-2.0/lib; fi
	if [ -d hpl-2.0 ]; then find hpl-2.0 -name xerbla.o | xargs rm -rf; fi
	if [ -d pgfile_test ]; then make -C pgfile_test clean; fi
	if [ -d tachyon ]; then rm -rf tachyon/compile/*; fi
	if [ -d mandel ]; then make -C mandel clean; rm -f mandel/*.d; fi
	make -C mpicheck clean;
	make -C deviation clean;
	make -C groupstress clean;
	rm -f mpi_multibw/mpi_multibw mpi_multibw/*.o
	rm -f osu2/*.o
	rm -f osu2/osu_latency osu2/osu_bw osu2/osu_bcast osu2/osu_bibw
	rm -f osu2/osu_acc_latency osu2/osu_get_latency osu2/osu_put_latency
	rm -f osu2/osu_get_bw osu2/osu_put_bw osu2/osu_put_bibw
	if [ -f "osu-micro-benchmarks-3.8-July12/Makefile" ]; then cd osu-micro-benchmarks-3.8-July12; make clean; fi
	rm -f .prefix
.PHONY: clobber

SHOWMPI:
	@echo "Using MPICH_PREFIX=$(MPICH_PREFIX)"
	echo "$(MPICH_PREFIX)" > .prefix
.PHONY: SHOWMPI

depend::
force::
INCLUDES::
# do the real work during LIBS
LIBS:: 
.PHONY: depend force INCLUDES LIBS

opa-base:: DEVIATION CHECK GROUPSTRESS
	@echo "build base sample applications"
.PHONY: opa-base

quick:: BW LAT MULTIBW OSU2 OSU38 IMB XHPL2 opa-base
	@echo "Built subset of sample applications"
.PHONY: quick

CMDS:: quick
	@echo "Built sample applications"
.PHONY: CMDS

full:: CMDS PMB
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
