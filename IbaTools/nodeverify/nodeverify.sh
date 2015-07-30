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


# script to verify configuration and performance of an individual node
# this should be run using opaverifyhosts.  It can also be run on an individual
# node directly.

# The following specify expected minimal configuration and performance
MEMSIZE_EXPECTED=30601808   # size of memory in k as shown in /proc/meminfo
HFI_COUNT=1                 # expected Number of Intel HFIs
HFI_CPU_CORE[0]=1           # CPU core to use when testing HFI 0 PCI performance
HFI_CPU_CORE[1]=1           # CPU core to use when testing HFI 1 PCI performance
HFI_CPU_CORE[2]=1           # CPU core to use when testing HFI 2 PCI performance
HFI_CPU_CORE[3]=1           # CPU core to use when testing HFI 3 PCI performance
                            # On some servers, such as Sandy-Bridge, it is
                            # recommended to set the CPU CORE for each HFI to
                            # a core within the same CPU socket the HFI's PCI
                            # bus is connected to
PCI_MAXPAYLOAD=256          # expected value for PCI max payload
PCI_MINREADREQ=512          # expected value for PCI min read req size
PCI_MAXREADREQ=4096         # expected value for PCI max read req size
PCI_SPEED="8GT/s"           # expected value for PCI speed on Intel WFR HFI
PCI_WIDTH="x16"             # expected value for PCI width on Intel WFR HFI
MIN_HFI_PKT=2500          # minimum result from hfi_pkt_test WFR HFI
MPI_APPS=/opt/opa/src/mpi_apps/ # where to find mpi_apps for HPL test
MIN_FLOPS="115"             # minimum flops expected from HPL test
HPL_CORES=16                # how many cores per node to include in HPL test
HPL_CONFIG="${HPL_CORES}s"  # problem selection arguments for config_hpl2
                            # This can be a simple config selection: "16s"
                            # or a config selection and problem size: "16t 9000"
                            # if set to "", config_hpl2 will not be run

# can adjust default list of tests below
#TESTS="pcicfg pcispeed cstates initscripts hyperthreading hfi_pkt memsize hpl"
TESTS="pcicfg pcispeed cstates initscripts hyperthreading hfi_pkt memsize"


# If desired, single node HPL can be run.
# The goal of the single node HPL test is to check node stability and the
# consistency of performance between hosts, NOT to optimize performance.
# As such a small problem size, such as './config_hpl2 16s' is recommended
# since it will run faster and be equally good at discovering slow or
# unstable hosts.  The given problem size should be run on a known good
# node and the result can be used to select an appropriate MIN_FLOPS
# value for other hosts.  It may be necessary to set MIN_FLOPS slightly below
# the observed result for the known good host to account for minor variations
# in OS background overhead, manufacturing variations, etc.

# HPL_CONFIG can specify a problem size for config_hpl2, in which case
# config_hpl2 will be run at the start of the test to setup the hpl2 HPL.dat

# if HPL_CONFIG is "", then config_hpl2 will not be run and it is expected
# that the user has setup an appropriate HPL.dat file an appropriate single
# node HPL_CORES count and problem size.  HPL.dat files should run a single
# HPL computation across all selected cores.
# See MPI_APPS/hpl-config/ for examples.

# In order to run the single node HPL test, each tested host must be able to
# ssh to locahost as root


Usage()
{
	echo "Usage: ./hostverify.sh [-v] [test ...]" >&2
	echo "              or" >&2
	echo "       ./hostverify.sh --help" >&2
	echo "   -v - provide verbose output on stdout (by default only PASS/FAIL is output)" >&2
	echo "        detailed results are always output to /root/hostverify.res" >&2
	echo "   --help - produce full help text" >&2
	echo "	 test - one or more specific tests to run" >&2
	echo "This verifies basic node configuration and performance" >&2
	echo "Prior to using this, edit it to set proper" >&2
	echo "expectations for node configuration and performance" >&2
	echo >&2
	echo "The following tests are available:" >&2
	echo "  pcicfg - verify PCI max payload and max read request size settings" >&2
	echo "  pcispeed - verify PCI bus negotiated to PCIe Gen2 x8 speed" >&2
	echo "  cstates - verify CPU cstates are disabled" >&2
	echo "  initscripts - verify irqbalance, irq_balancer, powerd and cpuspeed" >&2
	echo "                init.d scripts are disabled" >&2
	echo "  hyperthreading - verify hyperthreading is disabled" >&2
	echo "  ipath_pkt - check PCI-HFI bus performance.  Requires HFI port is Active" >&2
	echo "  memsize - check total size of memory in system" >&2
	echo "  hpl - perform a single node HPL test," >&2
	echo "        useful to determine if all hosts perform consistently" >&2
	echo "  default - run all tests selected in TESTS" >&2
	echo >&2
	echo "Detailed output is written to stdout and appended to /root/hostverify.res" >&2
	echo "egrep 'PASS|FAIL' /root/hostverify.res for a brief summary" >&2
	echo >&2
	echo "A Intel HFI is required for pcicfg, pcispeed and ipath_pkt tests" >&2
	exit 0
}

if [ x"$1" = "x--help" ]
then
	Usage
fi

myfilter()
{
	# filter out the residual report from HPL
	egrep 'PASS|FAIL|SKIP'|fgrep -v '||Ax-b||_oo/(eps'
}

filter=myfilter
while getopts v param
do
	case $param in
	v) filter=cat;;
	?)	Usage;;
	esac
done
shift $((OPTIND -1))

tests="$TESTS"
if [ $# -ne 0 ]
then
	tests=
	for i in $*
	do
		if [ x"$i" == x"default" ]
		then
			tests="$tests $TESTS"
		else
			tests="$tests $i"
		fi
	done
fi

USEMEM=/opt/opa/tools/usemem	# where to find usemem tool
if [ -n "${debug}" ]; then
	set -x
	set -v
fi

export LANG=C
unset CDPATH

TEST=main

function fail_msg()
{
	set +x
	echo "`hostname -s`: FAIL $TEST: $1"
}

function pass_msg()
{
	set +x
	echo "`hostname -s`: PASS $TEST$1"
}

function fail()
{
	fail_msg "$1"
	exit 1
}

function pass()
{
	pass_msg "$1"
	exit 0
}

function skip()
{
	set +x
	echo "`hostname -s`: SKIP $TEST$1"
	exit 0
}

workdir="${workdir:=/var/tmp}"
mkdir -p "${workdir}" || fail "Can't mkdir ${workdir}"
cd "${workdir}" || fail "Can't cd ${workdir}"
outdir="$workdir/$(mktemp -d test.XXXXXXXXXX)"

# verify basic PCIe configuration
test_pcicfg()
{
	TEST=pcicfg
	echo "pcicfg ..."
	date

	failure=0
	cd "${outdir}" || fail "Can't cd ${outdir}"

	lspci="${lspci:-/sbin/lspci}"
	[ ! -x "${lspci}" ] && fail "Can't find lspci"

	# get Storm Lake Intel stand-alone WFR HFI
	set -x
	"${lspci}" -vvv -d 0x8086:0x24f0 2>pcicfg.stderr | tee -a pcicfg.stdout || fail "Error running lspci"
	set +x
	[ -s pcicfg.stderr ] && fail "Error during run of lspci: $(cat pcicfg.stderr)"
	# also get Storm Lake Intel integrated WFR HFI
	set -x
	"${lspci}" -vvv -d 0x8086:0x24f1 2>pcicfg.stderr | tee -a pcicfg.stdout || fail "Error running lspci"
	set +x
	[ -s pcicfg.stderr ] && fail "Error during run of lspci: $(cat pcicfg.stderr)"

	hfi_count=$(cat pcicfg.stdout|grep 'MaxPayload .* bytes, MaxReadReq .* bytes' | wc -l)
	[ $hfi_count -ne $HFI_COUNT ] && { fail_msg "Incorrect number of Intel HFIs: Expect $HFI_COUNT  Got: $hfi_count"; failure=1; }
	[ $HFI_COUNT -eq 0 ] && pass

	cat pcicfg.stdout|egrep 'MaxPayload .* bytes, MaxReadReq .* bytes|^[0-9]' |
		{
		failure=0
		device="Unknown"
		while read cfgline
		do
			if  echo "$cfgline"|grep '^[0-9]' >/dev/null 2>/dev/null
			then
				device=$(echo "$cfgline"|cut -f1 -d ' ')
			else
				result=$(echo "$cfgline"|sed -e 's/.*\(MaxPayload .* bytes, MaxReadReq .* bytes\).*/\1'/)
				payload=$(echo "$result"|cut -f2 -d ' ')
				readreq=$(echo "$result"|cut -f5 -d ' ')

				[ "$payload" -eq "$PCI_MAXPAYLOAD" ] || { fail_msg "HFI $device: Incorrect MaxPayload of $payload (expected $PCI_MAXPAYLOAD)."; failure=1; }

				[ "$readreq" -le "$PCI_MAXREADREQ" -a "$readreq" -ge "$PCI_MINREADREQ" ] || { fail_msg "HFI $device: MaxReadReq of $readreq out of expected range [$PCI_MINREADREQ,$PCI_MAXREADREQ]."; failure=1; }

				device="Unknown"
			fi
		done
		[ $failure -ne 0 ] && exit 1
		exit 0
		}
	[ $failure -ne 0 -o $? != 0 ] && exit 1	# catch any fail calls within while

	pass
}

# verify PCIe is negotiated at PCIe Gen3 x16
test_pcispeed()
{
	TEST=pcispeed
	echo "pcispeed ..."
	date

	failure=0
	cd "${outdir}" || fail "Can't cd ${outdir}"

	lspci="${lspci:-/sbin/lspci}"
	[ ! -x "${lspci}" ] && fail "Can't find lspci"

	# get Storm Lake Intel stand-alone WFR HFI
	set -x
	"${lspci}" -vvv -d 0x8086:0x24f0 2>pcispeed.stderr | tee -a pcispeed.stdout || fail "Error running lspci"
	set +x
	[ -s pcispeed.stderr ] && fail "Error during run of lspci: $(cat pcispeed.stderr)"
	# also get Storm Lake Intel integrated WFR HFI
	set -x
	"${lspci}" -vvv -d 0x8086:0x24f1 2>pcispeed.stderr | tee -a pcispeed.stdout || fail "Error running lspci"
	set +x
	[ -s pcispeed.stderr ] && fail "Error during run of lspci: $(cat pcispeed.stderr)"

	hfi_count=$(cat pcispeed.stdout|grep 'Speed .*, Width .*' pcispeed.stdout|egrep -v 'LnkCap|LnkCtl|Supported'| wc -l)
	[ $hfi_count -ne $HFI_COUNT ] && { fail_msg "Incorrect number of Intel HFIs: Expect $HFI_COUNT  Got: $hfi_count"; failure=1; }
	[ $HFI_COUNT -eq 0 ] && pass

	cat pcispeed.stdout|egrep 'Speed .*, Width .*|^[0-9]' pcispeed.stdout|egrep -v 'LnkCap|LnkCtl|Supported'|
		{
		failure=0
		device="Unknown"
		is_wfr_lite=0
		while read cfgline
		do
			if  echo "$cfgline"|grep '^[0-9]' >/dev/null 2>/dev/null
			then
				device=$(echo "$cfgline"|cut -f1 -d ' ')
				if echo "$cfgline"|grep 'InfiniBand' >/dev/null 2>/dev/null
				then
					is_wfr_lite=1
				fi
			else
				result=$(echo "$cfgline"|sed -e 's/.*\(Speed .*, Width [^,]*\).*/\1'/)

				expect="Speed $PCI_SPEED, Width $PCI_WIDTH"
				 [ "$result" = "$expect" ] || [ $is_wfr_lite -eq 1 ] || { fail_msg "HFI $device: Incorrect Speed or Width: Expect: $expect  Got: $result"; failure=1; }
				device="Unknown"
				is_wfr_lite=0
			fi
		done
		[ $failure -ne 0 ] && exit 1
		exit 0
		}
	[ $failure -ne 0 -o $? != 0 ] && exit 1	# catch any fail calls within while

	pass
}

# make sure irqbalance, irq_balancer, powerd and cpuspeed are disabled
test_initscripts()
{
	TEST="initscripts"
	echo "initscripts ..."
	date
	cd "${outdir}" || fail "Can't cd ${outdir}"

	> chkconfig.out
	for i in irqbalance irq_balancer cpuspeed powerd
	do
		set -x
		if [ -e /etc/init.d/$i ]
		then
			chkconfig --list $i 2>chkconfig.stderr|tee -a chkconfig.out || fail "Error during chkconfig $i"
			[ -s chkconfig.stderr ] && fail "Error during run of chkconfig: $(cat chkconfig.stderr)"
		fi
		set +x
	done

	result=`grep '[2-6]:on' chkconfig.out`
	[ -n "${result}" ] && fail "Undesirable service enabled: $result"

	#Now check systemd
	for i in irqbalance irq_balancer cpuspeed powerd
	do
		set -x
		if [ -n "`systemctl list-units | grep $i`" ] 
		then
			[ -n "`systemctl is-enabled $i | grep enabled`" ] && fail "Undesirable systemd service enabled: $i"
		fi
		set +x
	done

	pass
}

# make sure hyperthreading is disabled
test_hyperthreading()
{
	TEST="hyperthreading"
	echo "hyperthreading ..."
	date
	cd "${outdir}" || fail "Can't cd $outdir"

	# hyperthreading only applies to Intel CPUs,  N/A for AMD CPUs
	set -x
	if cat /proc/cpuinfo | grep Intel >/dev/null
	then
		result=$(cat /proc/cpuinfo|egrep 'cpu cores|siblings'|sort -u)
		set +x
	
		[ $(echo "$result"|cut -f2 -d :|sort -u|wc -l) -ne 1 ] && fail "Siblings != Cpu Cores: $result"
	fi
	set +x

	pass
}

# make sure C-states are disabled
test_cstates()
{
	TEST="C-states"
	echo "C-states ..."
	date
	cd "${outdir}" || fail "Can't cd ${outdir}"

	skipped=0
	# does the CPU and OS support power managemnets (C-states) info
	if ls /proc/acpi/processor/*/info
	then 
	# first check for power management on
	set -x
	cat /proc/acpi/processor/*/info 2>acpi.stderr | tee acpi.stdout || fail "Error catting acpi info"
	set +x
	[ -s acpi.stderr ] && fail "Error during cat of acpi info: $(cat acpi.stderr)"

	if cat acpi.stdout | grep 'power' | grep 'yes'
	then
		fail "Power Management (C-States) enabled"
	fi
	else
		echo "Power management (C-States) information not available for this system"
		skipped=`expr $skipped + 1`
	fi

	# does the CPU and OS support C-states access
	if ls /proc/acpi/processor/*/power
	then
		# next check for C-States configured
		set -x
		cat /proc/acpi/processor/*/power 2>acpi2.stderr | tee acpi2.stdout || fail "Error catting acpi power"
		set +x
		[ -s acpi2.stderr ] && fail "Error during cat of acpi power: $(cat acpi2.stderr)"

		# C1 is ok, its the active state, others should be disabled
		if cat acpi2.stdout | grep -v 'C1:' | grep 'C.:'
		then
			fail "Power Management (C-States) configured"
		fi
	else
		echo "C-States information not available for this system"
		skipped=`expr $skipped + 1`
	fi

	# in case all above files are not present check in sysfs
	if ls /sys/devices/system/cpu/cpu*/cpuidle
	then
		fail "Power Management (C-States) enabled"
	else
		skipped=`expr $skipped - 1`
	fi

	#We shall mark this as skipped only if both are conditions are skipped
	#else test shall be marked as passed.
	if [ $skipped = 2 ]
	then
		skip
	else
		pass
	fi
}

#test_nodeperf()
#{
#	TEST="nodeperf"
#	echo "nodeperf ..."
#	date
#	cd "${outdir}" || fail "Can't cd ${outdir}"
#
#	if [ -z "${nomemflush}" ]; then
#		set -x
#		"$USEMEM" 1048576 20737418240
#		set +x
#	fi
#
#	set -x
#	LD_LIBRARY_PATH="${rundir}/lib" "${rundir}/bin/nodeperf" 2>nodeperf.stderr | tee nodeperf.stdout || fail "Failed to run nodeperf"
#	set +x
#	[ -s nodeperf.stderr ] && cat nodeperf.stdout && fail "Error running nodeperf"
#
#	min="${min:-120000}"
#	[ -z "${min}" ] && fail "invalid min"
#
#	nodeperf=$(cat nodeperf.stdout| grep '0 of 1' | head -1 | cut -d ' ' -f 12)
#	[ -z "${nodeperf}" ] && fail "Unable to obtain nodeperf result"
#
#	result="$(echo "if ( ${nodeperf} >= ${min} ) { print \"PASS\n\"; } else { print \"FAIL\n\"; }" | bc -lq)" || fail "Unable to analyze nodeperf result"
#	[ "${result}" != "PASS" ] && fail "Performance of ${nodeperf} less than min ${min}"
#
#	pass
#}

#test_streams()
#{
#	TEST="streams"
#	echo "streams ..."
#	date
#	for run in 1 2 3; do
#		if [ -z "${nomemflush}" ]; then
#			set -x
#			"$USEMEM" 1048576 20737418240
#			set +x
#		fi
#
#		set -x
#		"${rundir}/bin/stream_omp" 2>stream_omp.stderr |tee stream_omp_${run}.stdout || fail
#		set +x
#		[ -s stream_omp.stderr ] && cat stream_omp.stderr && fail "Errors during run"
#	done
#
#	min="${min:-120000}"
#	[ -z "${min}" ] && fail "invalid min"
#
#	triadbw1=$(cat stream_omp_1.stdout| grep '^Triad:' | sed -e 's/  */ /g' | cut -d ' ' -f 2)
#	[ -z "${triadbw1}" ] && fail "Triad result not found"
#
#	triadbw2=$(cat stream_omp_2.stdout| grep '^Triad:' | sed -e 's/  */ /g' | cut -d ' ' -f 2)
#	[ -z "${triadbw2}" ] && fail "Triad result not found"
#
#	triadbw3=$(cat stream_omp_3.stdout| grep '^Triad:' | sed -e 's/  */ /g' | cut -d ' ' -f 2)
#	[ -z "${triadbw3}" ] && fail "Triad result not found"
#
#	#result="$(echo "if ( (${triadbw1} + ${triadbw2} + ${triadbw3}) >= ${min} ) { print \"PASS\n\"; } else { print \"FAIL\n\"; }" | bc -lq)" || fail "Unable to analyze BW"
#	gbw=0
#	for x in 1 2 3; do
#        # The test below requires integers and errors out with floats
#		[ $(eval echo '$'triadbw$x | cut -d. -f1) -ge 40000 ] && (( gbw++ ))
#	done
#	[ $gbw -lt 2 ] && result="FAIL" || result="PASS"
#	[ "${result}" != "PASS" ] && fail "One or more Triad numbers are below 40000"
#
#	pass
#}

# verify PCI-HFI bus performance
test_hfi_pkt()
{
	TEST="hfi_pkt"
	echo "hfi_pkt-test ..."
	date
	cd "${outdir}" || fail "Can't cd ${outdir}"

	lspci="${lspci:-/sbin/lspci}"
	[ ! -x "${lspci}" ] && fail "Can't find lspci"

	# get Storm Lake Intel stand-alone WFR HFI
	set -x
	"${lspci}" -vvv -d 0x8086:0x24f0 2>pciinfo.stderr | tee -a pciinfo.stdout || fail "Error running lspci"
	set +x
	[ -s pciinfo.stderr ] && fail "Error during run of lspci: $(cat pciinfo.stderr)"
	# also get Storm Lake Intel integrated WFR HFI
	set -x
	"${lspci}" -vvv -d 0x8086:0x24f1 2>pciinfo.stderr | tee -a pciinfo.stdout || fail "Error running lspci"
	set +x
	[ -s pciinfo.stderr ] && fail "Error during run of lspci: $(cat pciinfo.stderr)"

	if [ $HFI_COUNT -gt 0 ]
	then
		[ ! -s pciinfo.stdout ] && fail "No Intel HFI found"
	else
		[ -s pciinfo.stdout ] && fail "Unexpected Intel HFI found"
		pass
	fi

	hfi_pkt_test="${hfi_pkt_test:-/usr/bin/hfi1_pkt_test}"
	[ ! -x "${hfi_pkt_test}" ] && fail "Can't find hfi_pkt_test"

	[ -z "${MIN_HFI_PKT}" ] && fail "Invalid MIN_HFI_PKT"

	failure=0
	for hfi in $(seq 0 $(($HFI_COUNT-1)) )
	do
		if [ -z "${nomemflush}" ]; then
			# use 90% of memory to flush caches and convert from K to bytes
			set -x
			"$USEMEM" 1048576 $(($MEMSIZE_EXPECTED * 90))
			set +x
		fi

		if [ x"${HFI_CPU_CORE[$hfi]}" = x ]
		then
			fail_msg "HFI $hfi: HFI_CPU_CORE[$hfi] not specified"
			failure=1
			continue
		fi
		set -x
		taskset -c ${HFI_CPU_CORE[$hfi]} "${hfi_pkt_test}" -B -U $hfi 2>hfi_pkt_test.stderr | tee hfi_pkt_test.stdout || { fail_msg "HFI $hfi: Error running hfi_pkt_test"; failure=1; continue; }
		set +x
		[ -s hfi_pkt_test.stderr ] && { fail_msg "Error during run of hfi_pkt_test: HFI $hfi: $(cat hfi_pkt_test.stderr)"; failure=1; continue; }

		bufferbw="$(cat hfi_pkt_test.stdout | grep 'Buffer Copy test:' | head -1 | sed -e 's/  */ /g' | cut -d ' ' -f 4)"
		[ -z "${bufferbw}" ] && { fail_msg "HFI $hfi: Unable to get result"; failure=1; continue; }

		result="$(echo "if ( ${bufferbw} >= ${MIN_HFI_PKT} ) { print \"PASS\n\"; } else { print \"FAIL\n\"; }" | bc -lq)" || { fail_msg "Unable to analyze result: HFI $hfi: $bufferbw"; failure=1; continue; }
		[ "${result}" != "PASS" ] && { fail_msg "HFI $hfi: Result of ${bufferbw} less than MIN_HFI_PKT ${MIN_HFI_PKT}"; failure=1; continue; }

		pass_msg ": HFI $hfi: $bufferbw MB/s"
	done
	[ $failure -ne 0 ] && exit 1

	exit 0
}


# verify server memory size
test_memsize()
{
	TEST="memsize"
	echo "memsize test ..."
	date
	set -x
	mem=$(cat  /proc/meminfo | head -1 | cut -f 2 -d : |cut -f 1 -d k)
	set +x
	echo "memory size: $mem"
	if  [ $mem -lt $MEMSIZE_EXPECTED ]
	then
		fail "low memory: $mem expected $MEMSIZE_EXPECTED"
	else
		pass ": $mem"
	fi
}


# single node HPL
test_hpl()
{
	TEST="hpl"
	echo "HPL test ..."
	date
	cd "${MPI_APPS}" || fail "Can't cd ${MPI_APPS}"

	# generate $outdir/mpi_hosts_hpl using localhost
	for i in $(seq 1 $HPL_CORES)
	do
		echo localhost
	done > $outdir/mpi_hosts_hpl

	# configure HPL2's HPL.dat file to control test
	if [ x"$HPL_CONFIG" != x ]
	then
		set -x
		MPI_HOSTS=$outdir/mpi_hosts_hpl ./config_hpl2 -l $HPL_CONFIG || fail "Error configuring HPL"
	fi

	set -x
	MPI_HOSTS=$outdir/mpi_hosts_hpl MPI_TASKSET="-c 0-$(($HPL_CORES - 1))" ./run_hpl2 $HPL_CORES 2> $outdir/hpl.stderr | tee $outdir/hpl.stdout || fail "Error running HPL"
	set +x
	grep -v '^+' < $outdir/hpl.stderr > $outdir/hpl.errors
	[ -s $outdir/hpl.errors ] && fail "Error during run of HPL: $(cat $outdir/hpl.errors)"

	[ -z "${MIN_FLOPS}" ] && fail "Invalid MIN_FLOPS"

	flops="$(cat $outdir/hpl.stdout | grep 'WR11R2C4' | head -1 | sed -e 's/  */ /g' | cut -d ' ' -f 7|sed -e 's/e+/ * 10^/')"
	[ -z "${flops}" ] && fail "Unable to get result"

	result="$(echo "if ( ${flops} >= ${MIN_FLOPS} ) { print \"PASS\n\"; } else { print \"FAIL\n\"; }" | bc -lq)" || fail "Unable to analyze result: $flops"
	[ "${result}" != "PASS" ] && fail "Result of ${flops} less than MIN_FLOPS ${MIN_FLOPS}"

	pass ": $flops Flops"
}




(
	for i in $tests
	do
		date
		if ! type "test_$i"|grep 'is a function' >/dev/null 2>/dev/null
		then
			TEST="$i" fail_msg "Invalid Test Name"
		else
			( test_$i )
		fi
		date
		echo "======================================================"
	done

	rm -rf "${outdir}"
	
) 2>&1|tee -a /root/hostverify.res|$filter
