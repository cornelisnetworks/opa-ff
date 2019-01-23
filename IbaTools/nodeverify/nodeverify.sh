#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2018, Intel Corporation
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
MEMSIZE_EXPECTED=30601808   # size of memory (MemTotal) in k as shown in
                            #  /proc/meminfo
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
MPI_APPS=$HOME/mpi_apps/    # where to find mpi_apps for HPL test
MIN_FLOPS="115"             # minimum flops expected from HPL test
IPOIB_IF="ib0"              # IPoIB interface to check
IPOIB_MTU=65520             # IPoIB required MTU
outputdir=/root             # default outputdir is root -d $DIR overrides
CPU_DRIVER="intel_pstate"   # power scaling driver for CPU
CPU_GOVERNOR="performance"  # scaling governor for CPU
STREAM_MIN_BW=40000         # minimum Triad bandwidth for each run of the
                            # STREAM memory benchmark (MB/s)

# If desired, single node HPL can be run, using the parameters defined below.
#
# The goal of the single node HPL test is to check node stability and the
# consistency of performance between hosts, NOT to optimize performance.
# As such a small problem size, such as '16s' is recommended since it will run 
# faster and will still be equally good at discovering slow or unstable hosts.
# The given problem size should be run on a known good node and the result 
# can be used to select an appropriate MIN_FLOPS value for other hosts.  It 
# may be necessary to set MIN_FLOPS slightly below the observed result for 
# the known good host to account for minor variations in OS background 
# overhead, manufacturing variations, etc.

# When using HPL, the problem size will automatically be adjusted to match
# the number of cores on the host, but this can be overridden by setting
# the HPL_CORES variable.
# 
# The size can also be adjusted to consume a percentage of system memory
# by adjusting the HPL_PRESSURE variable up or down between 0.1 and 0.9.
# Note that increasing the value of HPL_PRESSURE will increast the amount
# of time the test will take to execute.

# In order to run the single node HPL test, each tested host must be able to
# ssh to locahost as root

# Set this if you don't want the problem size to match the number of cores on
# the target node.
HPL_CORES=

# By default the problem size will be scaled to consume roughly 30% of RAM on
# the target node.  This can be adjusted up or down between 0.0 and 0.9. Note
# that larger values will cause the test to run for a longer time.
HPL_PRESSURE=0.3

# can adjust default list of tests below
#TESTS="pcicfg pcispeed initscripts irqbalance hfi_pkt memsize cpu_consist cpu cstates turbo hton htoff ipoib hpl"
TESTS="pcicfg pcispeed initscripts memsize cpu hfi_pkt hton ipoib irqbalance turbo cstates vtd srp stream"


Usage()
{
	echo "Usage: ./hostverify.sh [-v] [-d OUTPUTDIR] [test ...]" >&2
	echo "              or" >&2
	echo "       ./hostverify.sh --help" >&2
	echo "   -v - provide verbose output on stdout (by default only PASS/FAIL is output)" >&2
	echo "        detailed results are always output to OUTPUTDIR/hostverify.res" >&2
	echo "   -d OUTPUTDIR - write detailed results to OUTPUTDIR (defaults to /root)" >&2
	echo "   --help - produce full help text" >&2
	echo "	 test - one or more specific tests to run" >&2
	echo "This verifies basic node configuration and performance" >&2
	echo "Prior to using this, edit it to set proper" >&2
	echo "expectations for node configuration and performance" >&2
	echo >&2
	echo "The following tests are available:" >&2
	echo "  pcicfg - verify PCI max payload and max read request size settings" >&2
	echo "  pcispeed - verify PCI bus negotiated to PCIe Gen3 x16 speed" >&2
	echo "  initscripts - verify  powerd and cpuspeed init.d scripts are disabled," >&2
	echo "  hfi_pkt - check PCI-HFI bus performance.  Requires HFI port is Active" >&2
	echo "  memsize - check total size of memory in system" >&2
	echo "  vtd - verify that Intel VT-d is disabled" >&2
	echo "  hpl - perform a single node HPL test," >&2
	echo "        useful to determine if all hosts perform consistently" >&2
	echo "  pstates_on - check if Intel P-States are enabled and optimally configured" >&2
	echo "  pstates_off - check if Intel P-States are disabled" >&2
	echo "  driver_on - check if power scaling driver (default isintel_pstates) enabled" >&2
	echo "				generalized version of pstates_on" >&2
	echo "  driver_off - check if power scaling driver (default is intel_pstates)" >&2
	echo "				 generalized version of pstates_off" >&2 
	echo "  governor - verify proper governor enabled for CPU (default 'performance')" >&2
	echo "  cpu_pinned - verify CPU operating at max frequency" >&2
	echo "  cpu_unpinned - verify CPU not pinned to max operating frequency" >&2
	echo "  pmodules_off - verify other power management modules aren't loaded" >&2
	echo "  cpu - check if CPU parameters are configured for optimal performance" >&2
	echo "        includes pstates_on and governor tests" >&2
	echo "  cpu_consist - check if CPU parameters are configured for most consistent performance" >&2
	echo "                includes pstates_off, pmodules_off, governor, and cpu_pinned tests" >&2
	echo "  turbo - check if Intel Turbo Boost is enabled" >&2
	echo "  cstates - check if Intel C-States are enabled and optimally configured." >&2
	echo "  hton - verify that Hyper Threading is enabled" >&2
	echo "  htoff - verify that Hyper Threading is disabled" >&2
	echo "  ipoib - verify IPoIB is properly configured" >&2
	echo "  irqbalance - verify irqbalance is running with proper configuration" >&2
	echo "  srp - verify srp daemon is not running" >&2
	echo "  stream - verify memory bandwidth" >&2
	echo "  default - run all tests selected in TESTS" >&2
	echo >&2
	echo "Detailed output is written to stdout and appended to" >&2
	echo "  OUTPUTDIR/hostverify.res" >&2
	echo "egrep 'PASS|FAIL' OUTPUTDIR/hostverify.res for a brief summary" >&2
	echo >&2
	echo "A Intel HFI is required for pcicfg, pcispeed and hfi_pkt tests" >&2
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
while getopts vd: param
do
	case $param in
	v) filter=cat;;
	d) outputdir="$OPTARG";;
	?) Usage;;
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

USEMEM=/usr/lib/opa/tools/usemem	# where to find usemem tool
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

outdir="$(mktemp --tmpdir -d hostverify.XXXXXXXXXX)" || fail "Cannot mktemp --tmpdir -d hostverify.XXXXXXXXXX"

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

	# get OmniPath Intel stand-alone WFR HFI
	set -x
	"${lspci}" -n -d 0x8086:0x24f0 2>pcicfg.stderr | tee -a pcicfg.stdout || fail "Error running lspci"
	set +x
	[ -s pcicfg.stderr ] && fail "Error during run of lspci: $(cat pcicfg.stderr)"
	# also get OmniPath Intel integrated WFR HFI
	set -x
	"${lspci}" -n -d 0x8086:0x24f1 2>pcicfg.stderr | tee -a pcicfg.stdout || fail "Error running lspci"
	set +x
	[ -s pcicfg.stderr ] && fail "Error during run of lspci: $(cat pcicfg.stderr)"

	set -x
	pci_id=$(egrep -o -m1 "24f[01]" pcicfg.stdout)
	if [ -z ${pci_id} ]
	then
		fail "Error identifying HFI's on the PCI bus"
	fi
	
	
	"${lspci}" -vvv -d 0x8086:$pci_id 2>pcicfg.stderr | tee -a pcicfg.stdout || fail "Error running lspci"
	set +x

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

	# get OmniPath Intel stand-alone WFR HFI
	set -x
	"${lspci}" -n -d 0x8086:0x24f0 2>pcispeed.stderr | tee -a pcispeed.stdout || fail "Error running lspci"
	set +x
	[ -s pcispeed.stderr ] && fail "Error during run of lspci: $(cat pcispeed.stderr)"
	# also get OmniPath Intel integrated WFR HFI
	set -x
	"${lspci}" -n -d 0x8086:0x24f1 2>pcispeed.stderr | tee -a pcispeed.stdout || fail "Error running lspci"
	set +x
	[ -s pcispeed.stderr ] && fail "Error during run of lspci: $(cat pcispeed.stderr)"

	set -x
	pci_id=$(egrep -o -m1 "24f[01]" pcispeed.stdout)
	if [ -z ${pci_id} ]
	then
		fail "Error identifying HFI's on the PCI bus"
	fi
	
	"${lspci}" -vvv -d 0x8086:$pci_id 2>pcispeed.stderr | tee -a pcispeed.stdout || fail "Error running lspci"
	set +x

	hfi_count=$(cat pcispeed.stdout|grep 'Speed .*, Width .*' pcispeed.stdout|egrep -v 'LnkCap|LnkCtl|Supported'| wc -l)
	[ $hfi_count -ne $HFI_COUNT ] && { fail_msg "Incorrect number of Intel HFIs: Expect $HFI_COUNT  Got: $hfi_count"; failure=1; }
	[ $HFI_COUNT -eq 0 ] && pass

	cat pcispeed.stdout|egrep 'Speed .*, Width .*|^[0-9]' pcispeed.stdout|egrep -v 'LnkCap|LnkCtl|Supported'|
		{
		failure=0
		device="Unknown"
		while read cfgline
		do
			if  echo "$cfgline"|grep '^[0-9]' >/dev/null 2>/dev/null
			then
				device=$(echo "$cfgline"|cut -f1 -d ' ')
			else
				result=$(echo "$cfgline"|sed -e 's/.*\(Speed .*, Width [^,]*\).*/\1'/)

				expect="Speed $PCI_SPEED, Width $PCI_WIDTH"
				 [ "$result" = "$expect" ] ||  { fail_msg "HFI $device: Incorrect Speed or Width: Expect: $expect  Got: $result"; failure=1; }
				device="Unknown"
			fi
		done
		[ $failure -ne 0 ] && exit 1
		exit 0
		}
	[ $failure -ne 0 -o $? != 0 ] && exit 1	# catch any fail calls within while

	pass
}

# make sure powerd and cpuspeed are disabled
test_initscripts()
{
	TEST="initscripts"
	echo "initscripts ..."
	date
	cd "${outdir}" || fail "Can't cd ${outdir}"

	> chkconfig.out
	for i in cpuspeed powerd
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
	for i in cpuspeed powerd
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

test_irqbalance()
{
	TEST="irqbalance"
	echo "irqbalance ..."
	date

	set -x

	pid=$(pgrep irqbalance)	
	[ -z "$pid" ] && fail "irqbalance process not running"

	cmdline=$(ps -o command --no-heading -p $pid)
	echo "$cmdline" | grep -q "exact" || fail "irqbalance is NOT running with hint policy 'exact'"
	set +x

	pass
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

test_stream()
{
	TEST="stream"
	echo "stream ..."
	date

	stream="${stream:-/usr/lib/opa/tools/stream}"
	[ ! -x "${stream}" ] && fail "Can't find stream"

	for run in 1 2 3; do
		if [ -z "${nomemflush}" ]; then
			set -x
			"$USEMEM" 1048576 20737418240
			set +x
		fi

		set -x
		"${stream}" 2>stream.stderr |tee stream_${run}.stdout || fail
		set +x
		[ -s stream.stderr ] && cat stream.stderr && fail "Errors during run"
	done

	min="${min:-120000}"
	[ -z "${min}" ] && fail "invalid min"

	triadbw1=$(cat stream_1.stdout| grep '^Triad:' | sed -e 's/  */ /g' | cut -d ' ' -f 2)
	[ -z "${triadbw1}" ] && fail "Triad result not found"

	triadbw2=$(cat stream_2.stdout| grep '^Triad:' | sed -e 's/  */ /g' | cut -d ' ' -f 2)
	[ -z "${triadbw2}" ] && fail "Triad result not found"

	triadbw3=$(cat stream_3.stdout| grep '^Triad:' | sed -e 's/  */ /g' | cut -d ' ' -f 2)
	[ -z "${triadbw3}" ] && fail "Triad result not found"

	#result="$(echo "if ( (${triadbw1} + ${triadbw2} + ${triadbw3}) >= ${min} ) { print \"PASS\n\"; } else { print \"FAIL\n\"; }" | bc -lq)" || fail "Unable to analyze BW"
	gbw=0
	for x in 1 2 3; do
        # The test below requires integers and errors out with floats
		[ $(eval echo '$'triadbw$x | cut -d. -f1) -ge $STREAM_MIN_BW ] && (( gbw++ ))
	done
	[ $gbw -lt 2 ] && result="FAIL" || result="PASS"
	[ "${result}" != "PASS" ] && fail "One or more Triad numbers are below 40000"

	pass
}

# verify PCI-HFI bus performance
test_hfi_pkt()
{
	TEST="hfi_pkt"
	echo "hfi_pkt-test ..."
	date
	cd "${outdir}" || fail "Can't cd ${outdir}"

	lspci="${lspci:-/sbin/lspci}"
	setpci="${setpci:-/sbin/setpci}"

	[ ! -x "${lspci}" ] && fail "Can't find lspci"
	[ ! -x "${setpci}" ] && fail "Can't find setpci"

	# get OmniPath Intel stand-alone WFR HFI
	set -x
	"${lspci}" -n -d 0x8086:0x24f0 2>pciinfo.stderr | tee -a pciinfo.stdout || fail "Error running lspci"
	set +x
	[ -s pciinfo.stderr ] && fail "Error during run of lspci: $(cat pciinfo.stderr)"
	# also get OmniPath Intel integrated WFR HFI
	set -x
	"${lspci}" -n -d 0x8086:0x24f1 2>pciinfo.stderr | tee -a pciinfo.stdout || fail "Error running lspci"
	set +x
	[ -s pciinfo.stderr ] && fail "Error during run of lspci: $(cat pciinfo.stderr)"

	pci_id=$(egrep -o -m1 "24f[01]" pciinfo.stdout)
	if [ $HFI_COUNT -gt 0 ]
	then
		[ -z "$pci_id" ] && fail "No Intel HFI found"
	else
		[ ! -z "$pci_id" ] && fail "Unexpected Intel HFI found"
		pass
	fi


	hfi_pkt_test="${hfi_pkt_test:-/usr/bin/hfi1_pkt_test}"
	[ ! -x "${hfi_pkt_test}" ] && fail "Can't find hfi_pkt_test"

	failure=0
	declare -a pci_addr
	pci_addr=($(lspci -nd 0x8086:$pci_id | cut -d' ' -f1))

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

		pci_bus="$(echo ${pci_addr[$hfi]} | cut -d : -f1):"

		set -x
		taskset -c ${HFI_CPU_CORE[$hfi]} "${hfi_pkt_test}" -B -U $hfi 2>hfi_pkt_test.stderr | tee hfi_pkt_test.stdout || { fail_msg "HFI $hfi: Error running hfi_pkt_test"; failure=1; continue; }
		set +x
		[ -s hfi_pkt_test.stderr ] && { fail_msg "Error during run of hfi_pkt_test: HFI $hfi: $(cat hfi_pkt_test.stderr)"; failure=1; continue; }

		# Check the PCI-Express Device Status register (offset 0xA into PCIe capability structure).
		# Lower 4 bits used to indicate if any problems occoured on the bus PCIe bus.
		# Error reports range from simple retry to fatal PCIe errors. From our perspective, any of these should indicate
		# a problem with the card or the interface, so we declare a problem if this field is non-zero.
		# See: PCI-Express Base Spec v 3.1, sec 7.8.5.

		# First check the HFI device itself
		set -x
		"${setpci}" -s $pci_bus -d 0x8086:$pci_id CAP_EXP+0xa.w 2>pciiset.stderr | tee pciiset.stdout || fail "Error running setpci for HFI device 0x8086:$pci_id"
		set +x
		[ -s pciiset.stderr ] && fail "Error running setpci for HFI device 0x8086:$pci_id: $(cat pciiset.stderr)"

		result="0x"
		result="$result$(cat pciiset.stdout)"
		let "result&=0x000f"
		[ "${result}" != "0" ] && { fail_msg "HFI $hfi: PCIe bus problems detected during packet test!"; failure=1; continue; }

		# Now let's check the PCIe root port the HFI device is attatched to, as it may report errors on it's side independently
		# of the HFI.
		set -x
		pci_root_port="$(${lspci} -M 2> /dev/null | grep "$pci_bus Entered via" | cut -d' ' -f4)"
		set +x

		set -x
		"${setpci}" -s $pci_root_port CAP_EXP+0xa.w 2>pciiset.stderr | tee pciiset.stdout || fail "Error running setpci for PCIe root port $pci_root_port"
		set +x
		[ -s pciiset.stderr ] && fail "Error running setpci for PCIe root port $pci_root_port: $(cat pciiset.stderr)"

		result="0x"
		result="$result$(cat pciiset.stdout)"
		let "result&=0x000f"
		[ "${result}" != "0" ] && { fail_msg "HFI $hfi: PCIe bus problems detected during packet test!"; failure=1; continue; }

		pass_msg ": HFI $hfi: No PCIe bus issues detected during buffer test"
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


# verify correct VT-d setting
test_vtd()
{
	TEST="vtd"
	echo "Checking for correct VT-D setting ..."
	date
	dmar_interrupts=`grep -i DMAR /proc/interrupts|wc -l`
	if [ -e /sys/firmware/acpi/tables/DMAR ] && [ $dmar_interrupts -ne 0 ];
	then
		fail ": VT-D should be disabled."
	else
		pass ": VT-D appears to be disabled."
	fi
}


# Confirm CPU will not do frequency throttling or boosting. This test is good for running
# benchmarks  or jobs that require consistency. Such a configuration does not
# necessarily offer best CPU performance.
test_cpu_consist()
{
	TEST="cpu_consist"
	echo "CPU consistency test ..."
	date
	
	(test_pstates_off)
	(test_pmodules_off)
	(test_governor)	
	(test_cpu_pinned)
}

# Confirm CPU has enabled Intel P-States, favoring performance. 
test_cpu()
{
	TEST="cpu"
	echo "CPU test ..."
		
	(test_pstates_on)
	(test_governor)
}

# Return whether or not P-States enabled
function pstates_enabled()
{
	echo "pstates test ..."
	date

	set -x
	driver=`head -n 1 /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver`
	set +x

	return $([ "${driver}" = "intel_pstate" ])
}


# Confirm CPU has enabled P-States, favoring performance.
test_pstates_on()
{
	TEST="pstates_on"
	(pstates_enabled) || fail "intel_pstate disabled in kernel. Load module or check kernel cmdline."
	pass ": CPU is operating with recommended P-State configuration"
}

# Confirm CPU has disabled P-States
test_pstates_off()
{
	TEST="pstates_off"
	(pstates_enabled) && fail "intel_pstate enabled in kernel. Set intel_pstate=disable on kernel cmdline."
	pass ": CPU is operating with recommended P-State configuration"
}


# Return whether or not default driver loaded
function driver_loaded()
{
	echo "CPU default driver(${CPU_DRIVER}) test ..."
	date

	set -x
	driver=`head -n 1 /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver`
	set +x

	return $([ "${driver}" = ${CPU_DRIVER} ])
}

# Confirm desired CPU Driver loaded
test_driver_on()
{
	TEST="driver_on"
	driver_loaded || fail "CPU driver not set to ${CPU_DRIVER}."
	pass ": CPU is operating with recommend ${CPU_DRIVER} configuration"
}

# Confirm CPU Driver not loaded
test_driver_off()
{
	TEST="driver_off"
	driver_loaded && fail "CPU driver set to ${CPU_DRIVER}."
	pass ": CPU is operating with recommended ${CPU_DRIVER} configuration"
}

# Confirm CPU has no other power management module besides the desired loaded
test_pmodules_off()
{
	TEST="pmodules_off"
	echo "power management modules off test ..."
	date

	set -x
	mod_dir=/lib/modules/$(uname -r)/kernel/drivers/cpufreq/ 
	if [ -e ${mod_dir} ]
	then 
		modules=$(ls ${mod_dir} | egrep *.ko | while read line; do basename ${line} .ko; done)
	fi
	
	ldmodules=
	for mod in ${modules}
	do 
		tmp=$(lsmod | grep ${mod})
		[ -n "${tmp}" ] && [ ${mod} != ${CPU_DRIVER} ] && ldmodules=${mod}' '${ldmodules} 
	done

	if [ -n "${ldmodules}" ]
	then
		fail "${ldmodules}loaded in kernel."
	fi

	# check if pstates on if it is not the desired driver
	[ ${CPU_DRIVER} != "intel_pstate" ] && pstates_enabled && fail "intel_pstates enabled in kernel."

	pass ": No other power management kernel modules loaded."
	set +x
}

# Confirm CPU governor is set properly
test_governor()
{
	TEST="governor"
	echo "governor test ..."
	date
	
	set -x
	gov=`head -n 1 /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`
	if [ "${gov}" != ${CPU_GOVERNOR} ]
	then
		fail "cpupower governor is set to ${gov}. Should be ${CPU_GOVERNOR}."
	fi
	pass ": CPU governor set to ${CPU_GOVERNOR}"
	set +x
}


# Return whether or not CPU frequency pinned
lo_freq=
hi_freq=
max_freq=
function cpu_pinned()
{
	echo "CPU frequency test"
	date

	set -x
	result=$(cpupower -c 0 frequency-info -p | tail -1)
	lo_freq=$(echo ${result} | cut -f 1 -d ' ')
	hi_freq=$(echo ${result} | cut -f 2 -d ' ')
	max_freq=$(cpupower -c -0 frequency-info -l | tail -1 | cut -f 2 -d ' ')

	set +x
	
	if [ "$1" = "max" ]
	then
		return $([ $hi_freq -eq $lo_freq ] && [ $hi_freq -eq $max_freq ])
	else
		return $([ $hi_freq -eq $lo_freq ])
	fi
}

# Confirm CPU operating at max frequency
test_cpu_pinned()
{
	TEST="cpu_pinned"
	cpu_pinned "max" || fail "CPU frequency not pinned to ${max_freq} KHz. Current operating in range of ${lo_freq} - ${hi_freq} KHz."
	pass ": CPU is operating at max frequency of ${max_freq} KHz."
}

# Confirm CPU not pinned to specific frequency
test_cpu_unpinned()
{
	TEST="cpu_unpinned"
	cpu_pinned && fail "CPU operating frequency pinned to ${hi_freq} KHz."
	pass ": CPU operating frequency not pinned. Current operating in range of ${lo_freq} - ${hi_freq} KHz."
}

test_cstates()
{
	TEST="cstates"
	echo "cstates test ..."
	date
	set -x
	driver=$(cat /sys/devices/system/cpu/cpuidle/current_driver)
	[ "${driver}" != "intel_idle" ] && fail "intel_idle not enabled in kernel, C-State config non-optimal."
	max_cstate=$(cat /sys/module/intel_idle/parameters/max_cstate)
	[ ! $max_cstate -gt 0 ] && fail "C-States are currently disabled."
	set +x

	pass ": intel_idle is maintaining recommended C-States."
}

test_turbo()
{
	TEST="turbo"
	echo "Turbo test ..."
	date

	set -x
	result=$(cpupower frequency-info | sed -n "1,/boost state support/d;1,+1p")
	echo $result | grep -q "Supported: yes" || skip "Turbo not supported on platform"
	echo $result | grep -q "Active: yes" || fail "Turbo is not enabled"
	set +x

	pass ": Supported and enabled"
}

function check_ht()
{
	echo "Hyperthreading test ..."
	date

	lscpu="${lscpu:-/usr/bin/lscpu}"
	[ ! -x "${lscpu}" ] && fail "Can't find lscpu"

	set -x
	cpus="$(${lscpu} -p=cpu | tail -1)"
	cores="$(${lscpu} -p=core | tail -1)"
	set +x

	return $([ ! $cpus = $cores ])
}

test_hton()
{
	TEST="hton"
	check_ht || fail "HyperThreading is disabled"
	pass ": Hyperthreading is enabled"
}

test_htoff()
{
	TEST="htoff"
	check_ht && fail "HyperThreading is enabled"
	pass ": Hyperthreading is disabled"
}

test_ipoib()
{
	TEST="ipoib"
	echo "IPoIB test ..."
	date
	
	if [ ! -e /sys/class/net/$IPOIB_IF ]
	then
		fail "Given IPoIB interface $IPOIB_IF couldn't be found."
	fi

	set -x
	mtu=$(cat /sys/class/net/$IPOIB_IF/mtu)
	mode=$(cat /sys/class/net/$IPOIB_IF/mode)
	
	[ $mode != "connected" ] && fail "$IPOIB_IF is in '$mode' mode - should be in 'connected' mode"
	[ $mtu -lt $IPOIB_MTU ] && fail "$IPOIB_IF MTU of $mtu is less than required $IPOIB_MTU"
	set +x
	
	pass ": IPoIB properly configured"
}

# single node HPL
test_hpl()
{
	TEST="hpl"
	echo "HPL test ..."
	date
	cd "${MPI_APPS}" || fail "Can't cd ${MPI_APPS}"

	echo localhost > $outdir/mpi_hosts_hpl

	# configure HPL2's HPL.dat file to control test
	if [ -z "$HPL_CORES" ]; then
		HPL_CORES=`cat /proc/cpuinfo | grep processor | wc -l`
	fi
	set -x
	./hpl_dat_gen -d -n 1 -p $HPL_PRESSURE -c $HPL_CORES || fail "Error configuring HPL"
	set +x

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

test_srp()
{
        TEST="srp"
        echo "srp daemon..."
        date

        set -x

        pid=$(pgrep srp_daemon)
        [ -n "$pid" ] && fail "SRP process is running"

	autostart=$(lsmod | grep ib_srp)
	[ -n "$autostart" ] && fail "SRP service is configured to start at boot."

        set +x

        pass
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
	
) 2>&1|tee -a $outputdir/hostverify.res|$filter
