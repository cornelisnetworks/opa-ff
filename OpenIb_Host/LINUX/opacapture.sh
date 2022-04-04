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

# [ICS VERSION STRING: unknown]

# Usage: opacapture output_file_name
# captures system information for CornelisOPX problem reporting

Usage_full()
{
	echo "Usage: opacapture [-d detail] output_tgz_file" >&2
	echo "              or" >&2
	echo "       opacapture --help" >&2
	echo "   --help - produce full help text" >&2
	echo "   -d detail - level of detail of capture" >&2
	echo "               1-Local 2-Fabric 3-Fabric+FDB 4-Analysis (default=1)" >&2
	echo "This will capture critical system information into a zipped tar file" >&2
	echo "The program will automatically append .tgz to the <output_tgz_file>" >&2
	echo "if it does not already have a .tgz suffix" >&2
	echo "The resulting tar file should be sent to Customer Support along with any" 2>&1
	echo "CornelisOPX problem report regarding this system" >&2
}

Usage()
{
	Usage_full
	exit 2
}

if [ `basename $0` = ics_capture ]
then
	echo "warning: ics_capture is depricated, use opacapture" >&2
fi

if [ x"$1" = "x--help" ]
then
	Usage_full
	exit 0
fi


if [ -f /usr/lib/opa/tools/ff_funcs ]
then
	. /usr/lib/opa/tools/ff_funcs
	ff_available=y
else
	ff_available=n
fi

if [ $ff_available = "y" ]
then	
	if [ -f $CONFIG_DIR/opa/opafastfabric.conf  ]
	then
		. $CONFIG_DIR/opa/opafastfabric.conf
	fi
	
	. /usr/lib/opa/tools/opafastfabric.conf.def
fi

detail=1

while getopts d: param
do
	case $param in
        d)
                detail="$OPTARG";;
	?)
		Usage;;
	esac
done
shift $((OPTIND -1))

if [ $# != 1 ]
then
	Usage
fi

if [ `id -u` -ne 0 ]
then
	echo "This must be run as user root" >&2
	Usage
fi

if [ x`expr "$1" : '\(/\).*'` != x'/' ]
then
	# relative path
	tar_file=`pwd`/$1
else
	# absolute path
	tar_file=$1
fi

# append .tgz suffix if not already present
if [ x$(expr "$tar_file" : '.*\(\.tgz\)') != x'.tgz' ]
then
	tar_file="$tar_file.tgz"
fi

dir=tmp/capture$$
rm -rf /$dir
mkdir /$dir

echo "Capture Info: Detail: $detail; Date: $(date)" >> /$dir/capture_info

echo "Getting software and firmware version information ..."
echo "[ICS VERSION STRING: unknown]" > /$dir/sw_version
uname -a > /$dir/os_version
# we use query format so we can get ARCH information
rpm --queryformat '[%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n]' -qa > /$dir/rpms.detailed
# get simple version just to be safe
rpm -qa > /$dir/rpms

sha256sum /usr/lib/opa-fm/runtime/sm > /$dir/sha256sums
sha256sum /usr/lib/opa-fm/runtime/fe >> /$dir/sha256sums

echo "Capturing FM binaries and debuginfo if available"
rpm -q opa-fm-debuginfo > /dev/null 2>&1
if [ $? -eq 0 ]; then
	debuginfofiles=$(rpm -ql opa-fm-debuginfo | xargs -I% bash -c "if [ -f % ]; then echo %; fi")
	tar -zcf /$dir/opa-fm-debuginfo.tgz $debuginfofiles > /dev/null 2>&1
	unset debuginfofiles
fi
tar -zcf /$dir/opa-fm-bins.tgz /usr/lib/opa-fm/runtime/ > /dev/null 2>&1

# Finding the PCI devices
for fw in `lspci -n | grep "8086:24f0"`
do
	# Just get the PCI info for now...
	echo "$fw" >> /$dir/fw_info 
done

opahfirev > /$dir/opahfirev 2>&1

echo "Capturing Firmware info if available"
type hfi1_eprom > /dev/null 2>&1
if [ $? -eq 0 ]; then
	hfi1_eprom -d all -V > /$dir/uefi_version 2>&1
fi

type opatmmtool > /dev/null 2>&1
if [ $? -eq 0 ]; then
	opatmmtool 2>/dev/null 1>/dev/null
	tmm=$?
	if [ $tmm -ne 3 ]
	then
		echo "Getting TMM information..."
		opatmmtool -v status > /$dir/f4status 2>&1
		opatmmtool -f /$dir/f4otpdump dumpotp 2>&1
	fi
fi

echo "Obtaining OS configuration ..."
# get library config
ldconfig -p > /$dir/ldconfig
# get current runlevel
who -r > /$dir/who_r 2>/dev/null	# not available on all OSs
runlevel > /$dir/runlevel	# not available on all OSs
# get service startup configuration
if [ $(command -v systemctl) ]; then
	systemctl list-unit-files > /$dir/chkconfig.systemd
fi
chkconfig --list > /$dir/chkconfig 2>/dev/null
ulimit -a > /$dir/ulimit
uptime > /$dir/uptime

echo "Obtaining dmesg logs ..."
dmesg -T > /$dir/dmesg

echo "Obtaining present process and module list ..."
lsmod > /$dir/lsmod 2>&1
depmod -a 2>&1
cp -p /lib/modules/`uname -r`/modules.dep /$dir/modules.dep
ps -welf > /$dir/ps 2>&1

echo "Obtaining module info for hfi1 ..."
modinfo hfi1 > /$dir/modinfo_hfi1 2>&1

echo "Obtaining PCI device list ..."
lspci -vvv -xxxx > /$dir/lspci 2>&1
ls -l /dev/ipath* /dev/hfi* /dev/infiniband > /$dir/lsdev

echo "Obtaining processor information ..."
cpucount=$(grep -c processor /proc/cpuinfo)
cpupower -c 0-$((cpucount - 1)) frequency-info > /$dir/cpu-frequency-info 2>&1
grep . /sys/devices/system/cpu/cpu*/cpufreq/scaling* > /$dir/cpu-scaling-info 2>&1
grep . /sys/devices/system/cpu/intel_pstate/* > /$dir/cpu-intel_pstate 2>&1
unset cpucount

lscpu > /$dir/lscpu 2>&1
lscpu --extended=CPU,CORE,SOCKET,NODE,BOOK,DRAWER,CACHE,POLARIZATION,ADDRESS,CONFIGURED > /$dir/lscpu-extended 2>&1

echo "Obtaining environment variables ..."
env > /$dir/env 2>&1

echo "Obtaining network interfaces ..."
ip addr show > /$dir/ifconfig 2>&1

echo "Obtaining DMI information ..."
dmidecode > /$dir/dmidecode 2>&1

echo "Obtaining Shared Memory information ..."
sysctl -a > /$dir/sysctl 2>&1
sysctl -a 2>/dev/null | grep kernel.shm > /$dir/shm 2>&1
ls -d /dev/shm >> /$dir/shm 2>&1
ls -lR /dev/shm >> /$dir/shm 2>&1

echo "Obtaining OmniPath information ..."

# concise port statistics
opainfo > /$dir/opainfo 2>&1
opainfo -o stats > /$dir/opainfo-stats 2>&1
opainfo -o info > /$dir/opainfo-info 2>&1

echo "Obtaining MPI configuration ..."
if type mpi-selector >/dev/null 2>/dev/null
then
	mpi-selector --list > /$dir/mpi-selector-list
	mpi-selector --system --query > /$dir/mpi-selector-system
	mpi-selector --user --query > /$dir/mpi-selector-user
fi

mkdir /$dir/proc
echo "Copying configuration and statistics for OPA drivers from /proc ..."
for proc_file in cmdline cpuinfo ksyms meminfo mtrr modules net/arp net/dev net/dev_mcast net/route net/rt_cache net/vlan pci interrupts devices filesystems iomem ioports slabinfo version uptime scsi iba driver/ics_dsc driver/ipoib driver/sdp driver/rds irq acpi/processor
do
	if [ -e /proc/$proc_file ]
	then
		cp -p -r /proc/$proc_file /$dir/proc
	fi
done
for proc_file in `ps -eo pid`
do
	if [ -e /proc/$proc_file/stack ]
	then
		mkdir -p /$dir/proc/$proc_file
		cp -p -r /proc/$proc_file/stack /$dir/proc/$proc_file
	fi
done

echo "Obtaining additional CPU info..."
cpupower frequency-info > /$dir/cpupower-freq-info

# Check if HFI driver debug data dir) is present; log only if present
HFI_DEBUGDIR="/sys/kernel/debug/hfi1"
if [ -d ${HFI_DEBUGDIR} ]
then
	#hfi1stats requires the existance of ${HFI_DEBUGDIR}
	echo "Obtaining HFI statistics ..."
	hfi1stats > /$dir/hfi1stats 2>&1

	mkdir -p /${dir}${HFI_DEBUGDIR}
	echo "Copying kernel debug information from ${HFI_DEBUGDIR}..."
	cp -p -r ${HFI_DEBUGDIR}/* /${dir}/${HFI_DEBUGDIR} 2>/dev/null
fi

# Check if IPOIB debug data dir is present; log if present
IPOIB_DEBUGDIR="/sys/kernel/debug/ipoib"
if [ -d ${IPOIB_DEBUGDIR} ]
then
	echo "Obtaining IPOIB debug information from ${IPOIB_DEBUGDIR}..."
	mkdir -p /${dir}${IPOIB_DEBUGDIR}
	echo "Copying IPOIB debug information from ${IPOIB_DEBUGDIR}..."
	cp -r ${IPOIB_DEBUGDIR}/* /${dir}/${IPOIB_DEBUGDIR} 2>/dev/null
fi

# Check if side channel security issue mitigation information files are
# present; log if present
SIDE_CHANNEL_MITIGATION_INFO="/sys/devices/system/cpu/vulnerabilities"
if [ -d ${SIDE_CHANNEL_MITIGATION_INFO} ]
then
	echo "Obtaining side channel security issue mitigation information from ${SIDE_CHANNEL_MITIGATION_INFO}"
	mkdir -p /${dir}${SIDE_CHANNEL_MITIGATION_INFO}
	echo "Copying side channel security issue mitigation information from ${SIDE_CHANNEL_MITIGATION_INFO}..."
	cp -r ${SIDE_CHANNEL_MITIGATION_INFO}/* /${dir}${SIDE_CHANNEL_MITIGATION_INFO} 2>/dev/null
fi

# Check if side channel security issue mitigation kernel configuration files are
# present; log if present
KERNEL_CONFIG_LOC=/sys/kernel/debug/x86
SIDE_CHANNEL_MITIGATION_KERNEL_CONFIG_FILES="ibpb_enabled ibrs_enabled pti_enabled retp_enabled"
for fname in ${SIDE_CHANNEL_MITIGATION_KERNEL_CONFIG_FILES}
do
	if [ -e ${KERNEL_CONFIG_LOC}/${fname} ]
	then
		echo "Obtaining kernel configuration file ${KERNEL_CONFIG_LOC}/${fname}"
		if [ ! -d /${dir}${KERNEL_CONFIG_LOC} ]
		then
			mkdir -p /${dir}${KERNEL_CONFIG_LOC}
		fi
		echo "Copying kernel configuration file ${KERNEL_CONFIG_LOC}/${fname}..."
		cp ${KERNEL_CONFIG_LOC}/${fname} /${dir}${KERNEL_CONFIG_LOC} 2>/dev/null
	fi
done

mkdir -p /$dir/sys/class
if [ -e /sys/class/infiniband ]
then
	echo "Copying configuration and statistics for ib_ drivers from /sys ..."
	cp -p -r /sys/class/*infiniband* /$dir/sys/class 2>/dev/null
	mkdir -p /$dir/sys/class/infiniband
	for f in /sys/class/infiniband/*
	do
		if [ -h $f ]
		then
			rm -f /$dir/$f
			cp -p -r $f/ /$dir/sys/class/infiniband/ 2>/dev/null
			if [ -h $f/device ]
			then
				dev=`basename $f`
				unit=`expr $dev : 'hfi1_\(.*\)'`
				if [ ! -z "$unit" ]
				then
					echo "   Getting statedump for $dev ..."
					echo -e "unit $unit\nstate save hw /$dir/opa${dev}.dump"|hfidiags -s - > /$dir/opa${dev}.dump.res 2>&1
				fi
			#	rm -f /$dir/$f/device
			#	mkdir -p /$dir/sys/class/infiniband/$dev/ 2>/dev/null
			#	cp -p -r $f/device/ /$dir/sys/class/infiniband/$dev/ 2>/dev/null
			fi
		fi
	done
fi

if [ -e /sys/class/scsi_host ]
then
	cp -p -r /sys/class/scsi_host /$dir/sys/class 2>/dev/null
fi
if [ -e /sys/class/scsi_device ]
then
	cp -p -r /sys/class/scsi_device /$dir/sys/class 2>/dev/null
fi

if [ -e /sys/class/net/ ]
then
	echo "Copying interface information for ipoib"
	mkdir -p /$dir/sys/class/net
	cp -r /sys/class/net/ /$dir/sys/class 2>/dev/null
	for f in /sys/class/net/*
	do
		if [ -h $f ]
		then
			rm -f /$dir/$f
			cp -r $f/ /$dir/sys/class/net/ 2>/dev/null
			if [ -h $f/device ]
			then
				iface=`basename $f`
				rm -f /$dir/$f/device
				mkdir -p /$dir/sys/class/net/$iface/ 2>/dev/null
				cp -r $f/device/ /$dir/sys/class/net/$iface/ 2>/dev/null
			fi
		fi
	done
fi

if [ -e /sys/module ]
then
	echo "Copying configuration and statistics for OPA from /sys/module ..."
	cp -p -r /sys/module /$dir/sys 2>/dev/null 
fi

if [ -f /usr/lib/opa-fm/bin/fm_capture ]
then
	echo "Gathering Host FM Information ..."
	(cd /$dir; /usr/lib/opa-fm/bin/fm_capture)
fi

if [ -f /usr/bin/opa_osd_dump ]
then
	echo "Gathering Distributed SA data..."
	opa_osd_dump > /$dir/opa_osd_dump 2>&1
fi

if [ $detail -ge 2 ]
then
    mkdir -p /$dir/fabric
    cd /$dir/fabric/
    if [ "$ff_available" = y ]
    then
        check_ports_args opacapture
    else
        PORTS='0:0'
        if [ $detail -ge 3 ]
        then
            echo "Warning: opacapture detail=$detail but FastFabric not available"
        fi
    fi

    if [ $detail -ge 3 -a "$ff_available" = y ]
    then
        echo "Gathering Fabric-Level Information with FDBs ..."
    else
        echo "Gathering Fabric-Level Information ..."
    fi

    for hfi_port in $PORTS
    do
        hfi=$(expr $hfi_port : '\([0-9]*\):[0-9]*')
        port=$(expr $hfi_port : '[0-9]*:\([0-9]*\)')
        if [ "$hfi" = "" -o "$port" = "" ]
        then
            echo "opacapture: Error: Invalid port specification: $hfi_port" >&2
            continue
		fi
		## fixup name so winzip won't complain
		hfi_port_dir=${hfi}_${port}

        # make hfi_port directory
        mkdir $hfi_port_dir

        # opasaquery doesn't require FF available
        /usr/sbin/opasaquery -h $hfi -p $port -o node > $hfi_port_dir/nodes 2>&1
        /usr/sbin/opafabricinfo -p $hfi_port > $hfi_port_dir/opafabricinfo 2>&1

        if [ "$ff_available" = y ]
        then
            router_opt=""

            if [ $port -eq 0 ]
            then
                 port_opt="-h $hfi"
            else
                 port_opt="-h $hfi -p $port"
            fi

            # determine if port is management enabled
            /usr/sbin/opasmaquery $port_opt -o pkey 2>/dev/null|grep -q 0xffff
            mgmt_disabled=$?

            # add router table information to snapshot report
            if [ $detail -gt 2 ]
            then
                router_opt="-r"
            fi

            if [ $mgmt_disabled -eq 0 ]
            then
                /usr/sbin/opareport $port_opt -o snapshot -s -V $router_opt > $hfi_port_dir/snapshot.xml 2> $hfi_port_dir/snapshot.xml.err
                /usr/sbin/opareport $port_opt -o snapshot -m -M -s -V $router_opt > $hfi_port_dir/snapshot_direct.xml 2> $hfi_port_dir/snapshot_direct.xml.err
                /usr/sbin/opareport -o links -X $hfi_port_dir/snapshot.xml > $hfi_port_dir/fabric_links 2>&1
                /usr/sbin/opareport -o comps -X $hfi_port_dir/snapshot.xml > $hfi_port_dir/fabric_comps 2>&1
                /usr/sbin/opareport -o errors -X $hfi_port_dir/snapshot.xml > $hfi_port_dir/fabric_errors 2>&1
                /usr/sbin/opareport -o extlinks -X $hfi_port_dir/snapshot.xml > $hfi_port_dir/fabric_extlinks 2>&1
                /usr/sbin/opareport -o slowlinks -X $hfi_port_dir/snapshot.xml > $hfi_port_dir/fabric_slowlinks 2>&1
                /usr/sbin/opareport -o vfmember -V -d 4  > $hfi_port_dir/fabric_vfmember 2>&1
            fi

            if [ $detail -gt 2 ]
            then
                echo "Gathering Multicast Membership ..."
                /usr/sbin/opashowmc -p $hfi:$port > $hfi_port_dir/fabric_showmc 2>&1
                # create cable health report directory and generate report
                if [ $ff_available = "y" ] && [ -e "${FF_CABLE_HEALTH_REPORT_DIR}" ]
                then
                    echo "Gathering Cable Health Report ..."
                    FF_CABLE_HEALTH_REPORT_DIR_WITH_HFI_PORT="${FF_CABLE_HEALTH_REPORT_DIR}/${hfi_port_dir}/"
                    mkdir -p ${FF_CABLE_HEALTH_REPORT_DIR_WITH_HFI_PORT}
                    /usr/sbin/opareport -h $hfi -p $port -o cablehealth 2>/dev/null>${FF_CABLE_HEALTH_REPORT_DIR_WITH_HFI_PORT}/cablehealth$(date "+%Y%m%d%H%M%S").csv
                fi
            fi
        fi
    done
fi

if [ $ff_available = "y" ] && [ -e "${FF_CABLE_HEALTH_REPORT_DIR}" ]
then
        echo "Copying all Cable Health Reports"
        cp -p -r ${FF_CABLE_HEALTH_REPORT_DIR} /$dir/ 2>/dev/null
fi

cd /
files="$dir"
for f in var/log/opa* var/log/ics_* var/log/messages* var/log/ksyms.* var/log/boot* etc/*release* etc/sysconfig/ipoib.cfg* etc/opa etc/modules.conf* etc/modprobe.conf* etc/sysconfig/network-scripts/ifcfg* etc/dapl/ibhosts etc/hosts etc/sysconfig/boot etc/sysconfig/firstboot etc/dat.conf etc/sysconfig/network/ifcfg* etc/infiniband etc/sysconfig/*config etc/security etc/opa-fm/opafm.xml etc/sysconfig/iview_fm.config var/log/fm* var/log/sm* var/log/bm* var/log/pm* var/log/fe* var/log/opensm* var/log/ipath* etc/rc.d/rc.local etc/modprobe.d boot/grub/menu.lst boot/grub/grub.conf boot/grub2/grub.cfg boot/grub2/grubenv boot/grub2/device.map etc/grub*.conf etc/udev* etc/opensm etc/sysconfig/opensm etc/rdma/* etc/modprobe.d/* etc/dracut.conf.d/* etc/nsswitch.conf etc/sysconfig/irqbalance
do
	if [ -e "$f" ]
	then
		files="$files $f"
	fi
done
if [ -e /usr/bin/opaxmlextract ]
then
	for f in $(opaxmlextract -H -e LogFile < /etc/opa-fm/opafm.xml 2>/dev/null)
	do
		case "$f" in
		/var/log/fm*|/var/log/sm*|/var/log/bm*|/var/log/pm*|/var/log/fe*)
			>/dev/null;;
		*)
			if [ -e "$f" ]
			then
				files="$files $(echo $f|sed -e 's|^/||')"
			fi;;
		esac
	done
fi

for f in usr/local/src/mpi_apps/core* usr/src/opa/mpi_apps/core* usr/src/opa/shmem_apps/core*
do
	if [ -e "$f" ]
	then
		files="$files $f"
	fi
done
if [ "$ff_available" = y -a -d "$FF_MPI_APPS_DIR" ]
then
	apps_dir=$(echo $FF_MPI_APPS_DIR|sed -e 's|^/||')	# drop leading /
	for f in $apps_dir/core*
	do
		if [ -e "$f" ]
		then
			files="$files $f"
		fi
	done
fi

if [ $detail -ge 4 -a "$ff_available" = y ]
then
	if [ ! -d $FF_ANALYSIS_DIR/latest ]
	then
		rm -f $FF_ANALYSIS_DIR/opaallanalysis
		mkdir -p $FF_ANALYSIS_DIR
		baseline_opt=""
		if [ ! -d $FF_ANALYSIS_DIR/baseline ]
		then
			echo "Performing Fabric Analysis Baseline ..."
			baseline_opt="-b"
		else
			echo "Performing Fabric Analysis ..."
		fi
		/usr/sbin/opaallanalysis $baseline_opt > $FF_ANALYSIS_DIR/opaallanalysis 2>&1
	else
		echo "Copying Fabric Analysis ..."
	fi
	files="$files $(echo $FF_ANALYSIS_DIR|sed -e 's|^/||')"
fi

echo "Creating tar file $tar_file ..."
tar --format=gnu -czf $tar_file $files
retval=$?
rm -rf /$dir

if [ $retval -ne 0 ]
then
	echo "tar encountered an issue while generating the tarball. Please verify the tarball was created successfully, files that have changed are acceptable." >&2
fi

echo "Done."
echo
echo "Please include $tar_file with any problem reports to Customer Support"

exit $retval
