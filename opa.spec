Name: opa
Version: 10.1.0.0
Release: 126%{?dist}
Summary: Intel Omni-Path basic tools and libraries for fabric management

License: GPLv2 or BSD 
Url: https://github.com/01org/opa-ff
# tarball created by:
# git clone -b v1.2 https://github.com/01org/opa-ff.git
# cd opa-ff
# tar czf opa-ff.tar.gz --exclude-vcs .
Source: opa.tgz
ExclusiveArch: x86_64
# The Intel OPA product line is only available on x86_64 platforms at this time.

%description
This package contains the tools necessary to manage an Intel Omni-Path Architecture fabric.

%package basic-tools
Summary: Management level tools and scripts.

Requires: rdma bc

BuildRequires: expat-devel, gcc-c++, openssl-devel, ncurses-devel, tcl-devel, libibumad-devel, libibverbs-devel, libibmad-devel
BuildRequires: ibacm-devel

%description basic-tools
Contains basic tools for fabric managment necessary on all compute nodes.

%package fastfabric
Summary: Management level tools and scripts.
Requires: opa-basic-tools%{?_isa} >= %{version}-%{release}

%description fastfabric
Contains tools for managing fabric on a managment node.

%package address-resolution
Summary: Contains Address Resolution manager
Requires: opa-basic-tools%{?_isa} >= %{version}-%{release}

%description address-resolution
This package contains the ibacm distributed SA provider (dsap) for name and address resolution on OPA platform.
It also contains the library and tools to access the shared memory database exported by dsap.

%prep
#rm -rf %{_builddir}/*
#tar xzf %_sourcedir/%name.tgz
%setup -q -c

%build
./ff_build.sh %{_builddir} $FF_BUILD_ARGS


%install
%define basic_tools_sbin opacapture opafabricinfo opagetvf opagetvf_env opahfirev opapacketcapture opaportinfo oparesolvehfiport opasaquery opasmaquery opainfo opatmmtool

%define basic_tools_sbin_sym opapmaquery opaportconfig

%define basic_tools_opt setup_self_ssh usemem opaipcalc 

%define basic_mans opacapture.1 opaconfig.1 opafabricinfo.1 opagetvf.1 opagetvf_env.1 opahfirev.1 opainfo.1 opapacketcapture.1 opapmaquery.1 opaportconfig.1 opaportinfo.1 oparesolvehfiport.1 opasaquery.1 opashowmc.1 opasmaquery.1 opatmmtool.1

%define ff_tools_opt opaswquery opaswconfigure opaswfwconfigure opaswfwupdate opaswfwverify opaswping opaswreset

%define ff_tools_exp basic.exp chassis.exp chassis_configure.exp chassis_fmconfig.exp chassis_fmcontrol.exp chassis_fmgetconfig.exp chassis_getconfig.exp chassis_reboot.exp chassis_fmgetsecurityfiles.exp chassis_fmsecurityfiles.exp chassis_upgrade.exp common_funcs.exp configipoib.exp extmng.exp ff_function.exp ib.exp opa_to_xml.exp ibtools.exp install.exp ipoibping.exp load.exp mpi.exp mpiperf.exp mpiperfdeviation.exp network.exp proc_mgr.exp reboot.exp sacache.exp sm_control.exp switch_capture.exp switch_configure.exp switch_dump.exp switch_fwverify.exp switch_getconfig.exp switch_hwvpd.exp switch_info.exp switch_ping.exp switch_reboot.exp switch_upgrade.exp target.exp tools.exp upgrade.exp tclIndex tcl_proc comm12 front

%define ff_tools_sbin opacabletest opacheckload opaextracterror opaextractlink opaextractperf opaextractstat opaextractstat2 opafindgood opafirmware opagenchassis opagenesmchassis opagenswitches opalinkanalysis opareport opareports opasorthosts opatop opaxlattopology opaxlattopology_cust opaxmlextract opaxmlfilter opaxmlgenerate opaxmlindent opaallanalysis opacaptureall opachassisanalysis opacmdall opadownloadall opaesmanalysis opafabricanalysis opafastfabric opahostsmanalysis opadisablehosts opadisableports opaenableports opaledports opaexpandfile opaextractbadlinks opaextractlids opaextractsellinks opaswenableall opaswdisableall opaverifyhosts opahostadmin opachassisadmin opaswitchadmin opapingall opascpall opasetupssh opashowallports opauploadall opapaquery opashowmc opafequery 

%define ff_tools_misc ff_funcs opachassisip opagenswitcheshelper chassis_setup switch_setup opagetipaddrtype opafastfabric.conf.def show_counts

%define ff_tools_fm config_generate config_diff config_check config_convert

%define ff_libs_misc libqlgc_fork.so

%define ff_mans opaallanalysis.8 opacabletest.8 opacaptureall.8 opachassisadmin.8 opachassisanalysis.8 opacheckload.8 opacmdall.8 opadisablehosts.8 opadisableports.8 opadownloadall.8 opaenableports.8 opaledports.8 opaesmanalysis.8 opaexpandfile.8 opaextractbadlinks.8 opaextracterror.8 opaextractlids.8 opaextractlink.8 opaextractperf.8 opaextractsellinks.8 opaextractstat.8 opaextractstat2.8 opafabricanalysis.8 opafastfabric.8 opafequery.8 opafindgood.8 opafmconfigcheck.8 opafmconfigdiff.8 opagenchassis.8 opagenesmchassis.8 opagenswitches.8 opagentopology.8 opahostadmin.8 opahostsmanalysis.8 opalinkanalysis.8 opapaquery.8 opapingall.8 opareport.8 opareports.8 opascpall.8 opasetupssh.8 opashowallports.8 opasorthosts.8 opaswitchadmin.8 opatop.8 opauploadall.8 opaverifyhosts.8 opaxlattopology.8 opaxlattopology_cust.8 opashowmc.8 opaxmlextract.8 opaxmlfilter.8 opaxmlgenerate.8 opaxmlindent.8 opaswdisableall.8 opaswenableall.8

%define ff_iba_samples hostverify.sh opatopology_FIs.txt opatopology_links.txt opatopology_SMs.txt opatopology_SWs.txt linksum_swd06.csv linksum_swd24.csv README.topology README.xlat_topology topology_cust.xlsx topology.xlsx allhosts-sample chassis-sample hosts-sample switches-sample ports-sample opaff.xml-sample mac_to_dhcp filterFile.txt triggerFile.txt opamon.conf-sample opamon.si.conf-sample opafastfabric.conf-sample opa_ca_openssl.cnf-sample opa_comp_openssl.cnf-sample opagentopology esm_chassis-sample 

%define help_doc opatop_group_bw.hlp opatop_group_config.hlp opatop_group_err.hlp opatop_group_focus.hlp opatop_group_info_sel.hlp opatop_img_config.hlp opatop_pm_config.hlp opatop_port_stats.hlp opatop_summary.hlp opatop_vf_bw.hlp opatop_vf_info_sel.hlp opatop_vf_config.hlp

%define opasadb_bin opa_osd_dump opa_osd_exercise opa_osd_perf opa_osd_query

%define opasadb_header opasadb_path.h opasadb_route.h opasadb_route2.h

%define opasadb_mans opa_osd_dump.1 opa_osd_exercise.1 opa_osd_perf.1 opa_osd_query.1

%define shmem_apps_files Makefile mpi_hosts.sample prepare_run README select_mpi run_barrier run_get_bibw run_get_bw run_get_latency run_put_bibw run_put_bw run_put_latency run_reduce run_hello run_alltoall run_rand shmem-hello.c

%define release_string IntelOPA-Tools-FF.$BUILD_TARGET_OS_ID.$MODULEVERSION

#rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_sbindir}
mkdir -p $RPM_BUILD_ROOT/opt/opa/{tools,fm_tools,help,samples,src/mpi_apps,src/shmem_apps}
mkdir -p $RPM_BUILD_ROOT%{_libdir}/ibacm
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/rdma
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/opa
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/opa
mkdir -p $RPM_BUILD_ROOT%{_includedir}/infiniband
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man8


#Binaries and scripts installing (basic tools)
#cd builtbin.OPENIB_FF.release
cd $(cat %{_builddir}/RELEASE_PATH)

cd bin
cp -t $RPM_BUILD_ROOT%{_sbindir} %basic_tools_sbin 
cp -t $RPM_BUILD_ROOT/opt/opa/tools/ %basic_tools_opt
ln -s ./opaportinfo $RPM_BUILD_ROOT%{_sbindir}/opaportconfig
ln -s ./opasmaquery $RPM_BUILD_ROOT%{_sbindir}/opapmaquery

cd ../opasadb
cp -t $RPM_BUILD_ROOT%{_bindir} %opasadb_bin
cp -t $RPM_BUILD_ROOT%{_includedir}/infiniband %opasadb_header

cd ../bin
cp -t $RPM_BUILD_ROOT/opt/opa/tools/ %ff_tools_opt

cd ../fastfabric
cp -t $RPM_BUILD_ROOT%{_sbindir} %ff_tools_sbin
cp -t $RPM_BUILD_ROOT/opt/opa/tools/ %ff_tools_misc
cp -t $RPM_BUILD_ROOT/opt/opa/help %help_doc

cd ../etc
cp -t $RPM_BUILD_ROOT/opt/opa/fm_tools/ %ff_tools_fm
ln -s /opt/opa/fm_tools/config_check $RPM_BUILD_ROOT%{_sbindir}/opafmconfigcheck 
ln -s /opt/opa/fm_tools/config_diff $RPM_BUILD_ROOT%{_sbindir}/opafmconfigdiff

cd ../fastfabric/samples
cp -t $RPM_BUILD_ROOT/opt/opa/samples %ff_iba_samples
cd ..

cd ../fastfabric/tools
cp -t $RPM_BUILD_ROOT/opt/opa/tools/ %ff_tools_exp
cp -t $RPM_BUILD_ROOT/opt/opa/tools/ %ff_libs_misc
cp -t $RPM_BUILD_ROOT/opt/opa/tools/ osid_wrapper
cp -t $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/opa allhosts chassis esm_chassis hosts ports switches opaff.xml
cd ..

cd ../man/man1
cp -t $RPM_BUILD_ROOT%{_mandir}/man1 %basic_mans
cp -t $RPM_BUILD_ROOT%{_mandir}/man1 %opasadb_mans
cd ../man8
cp -t $RPM_BUILD_ROOT%{_mandir}/man8 %ff_mans
cd ..


cd ../src/shmem/shmem_apps
tar -xzf shmem_apps.tgz -C $RPM_BUILD_ROOT/opt/opa/src/shmem_apps/
cd ../../

#Config files
cd ../config
cp -t $RPM_BUILD_ROOT%{_sysconfdir}/rdma dsap.conf
cp -t $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/opa opamon.conf opamon.si.conf

#Libraries installing
#cd ../builtlibs.OPENIB_FF.release
cd $(cat %{_builddir}/LIB_PATH)
cp -t $RPM_BUILD_ROOT%{_libdir} libopasadb.so.*
cp -t $RPM_BUILD_ROOT%{_libdir}/ibacm libdsap.so.*

# Now that we've put everything in the buildroot, copy any default config files to their expected location for user
# to edit. To prevent nuking existing user configs, the files section of this spec file will reference these as noreplace
# config files.
cp $RPM_BUILD_ROOT/opt/opa/tools/opafastfabric.conf.def $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/opa/opafastfabric.conf

#Now we do a bunch of work to build the file listing of what belongs to each RPM

#Basic tools sbin
echo "%{_sbindir}/%{basic_tools_sbin} %{basic_tools_sbin_sym}" > %{_builddir}/basic_sbin_file.list
sed -i 's;[ ];\n%{_sbindir}/;g' %{_builddir}/basic_sbin_file.list 

#Basic tools opt
echo "/opt/opa/tools/%{basic_tools_opt}" > %{_builddir}/basic_opt_file.list
sed -i 's;[ ];\n/opt/opa/tools/;g' %{_builddir}/basic_opt_file.list 

#Basic man pages
echo "%{_mandir}/man1/%{basic_mans}" > %{_builddir}/basic_mans.list
sed -i 's;[ ];\n%{_mandir}/man1/;g' %{_builddir}/basic_mans.list
sed -i 's;\.1;\.1*;g' %{_builddir}/basic_mans.list

#FF tools opt
echo "/opt/opa/tools/%{ff_tools_opt}" > %{_builddir}/ff_opt_file.list
sed -i 's;[ ];\n/opt/opa/tools/;g' %{_builddir}/ff_opt_file.list

#FF exp files opt
echo "/opt/opa/tools/%{ff_tools_exp}" > %{_builddir}/ff_tools_exp.list
sed -i 's;[ ];\n/opt/opa/tools/;g' %{_builddir}/ff_tools_exp.list 

#FF misc files opt
echo "/opt/opa/tools/%{ff_tools_misc}" > %{_builddir}/ff_tools_misc.list
sed -i 's;[ ];\n/opt/opa/tools/;g' %{_builddir}/ff_tools_misc.list 

#FF libs misc
echo "/opt/opa/tools/%{ff_libs_misc}" > %{_builddir}/ff_libs_misc.list
sed -i 's;[ ];\n/opt/opa/tools/;g' %{_builddir}/ff_libs_misc.list 

#FF iba samples
echo "/opt/opa/samples/%{ff_iba_samples}" > %{_builddir}/ff_iba_samples.list
sed -i 's;[ ];\n/opt/opa/samples/;g' %{_builddir}/ff_iba_samples.list 

#FF tools to FM configuration
echo "/opt/opa/fm_tools/%{ff_tools_fm}" > %{_builddir}/ff_tools_fm.list
sed -i 's;[ ];\n/opt/opa/fm_tools/;g' %{_builddir}/ff_tools_fm.list 

#FF man pages
echo "%{_mandir}/man8/%{ff_mans}" > %{_builddir}/ff_mans.list
sed -i 's;[ ];\n%{_mandir}/man8/;g' %{_builddir}/ff_mans.list
sed -i 's;\.8;\.8*;g' %{_builddir}/ff_mans.list

#Final file listing for 'basic'
cat %{_builddir}/basic_sbin_file.list %{_builddir}/basic_opt_file.list > %{_builddir}/basic_file.list %{_builddir}/basic_mans.list


#FF tools help doc
echo "/opt/opa/help/%{help_doc}" > %{_builddir}/ff_help_file.list
sed -i 's;[ ];\n/opt/opa/help/;g' %{_builddir}/ff_help_file.list 

#FF tools sbin
echo "%{_sbindir}/%{ff_tools_sbin}" > %{_builddir}/ff_sbin_file.list
sed -i 's;[ ];\n%{_sbindir}/;g' %{_builddir}/ff_sbin_file.list

#ShmemApps
echo "/opt/opa/src/shmem_apps/%{shmem_apps_files}" > %{_builddir}/ff_shmem_apps.list
sed -i 's;[ ];\n/opt/opa/src/shmem_apps/;g' %{_builddir}/ff_shmem_apps.list 

#Final file listing for 'ff'
cat %{_builddir}/ff_shmem_apps.list %{_builddir}/ff_sbin_file.list %{_builddir}/ff_help_file.list %{_builddir}/ff_tools_exp.list %{_builddir}/ff_tools_misc.list %{_builddir}/ff_libs_misc.list %{_builddir}/ff_iba_samples.list %{_builddir}/ff_mans.list %{_builddir}/ff_tools_fm.list %{_builddir}/ff_opt_file.list > %{_builddir}/ff_file.list



%post address-resolution -p /sbin/ldconfig
%postun address-resolution -p /sbin/ldconfig

%preun fastfabric
cd /opt/opa/src/mpi_apps >/dev/null 2>&1
make -k clean >/dev/null 2>&1 || : # suppress all errors and return codes from the make clean.

%files basic-tools -f %{_builddir}/basic_file.list

%files fastfabric -f %{_builddir}/ff_file.list
%{_sysconfdir}/sysconfig/opa/opamon.si.conf
%config(noreplace) %{_sysconfdir}/sysconfig/opa/opafastfabric.conf
%config(noreplace) %{_sysconfdir}/sysconfig/opa/opamon.conf
%config(noreplace) %{_sysconfdir}/sysconfig/opa/allhosts
%config(noreplace) %{_sysconfdir}/sysconfig/opa/chassis
%config(noreplace) %{_sysconfdir}/sysconfig/opa/esm_chassis
%config(noreplace) %{_sysconfdir}/sysconfig/opa/hosts
%config(noreplace) %{_sysconfdir}/sysconfig/opa/ports
%config(noreplace) %{_sysconfdir}/sysconfig/opa/switches
%config(noreplace) %{_sysconfdir}/sysconfig/opa/opaff.xml
%config(noreplace) /opt/opa/tools/osid_wrapper
%{_sbindir}/opafmconfigcheck
%{_sbindir}/opafmconfigdiff


%files address-resolution
%{_bindir}/opa_osd_dump
%{_bindir}/opa_osd_exercise
%{_bindir}/opa_osd_perf
%{_bindir}/opa_osd_query
%{_libdir}/ibacm
%{_libdir}/libopasadb.so.*
%{_includedir}/infiniband
%{_mandir}/man1/opa_osd_dump.1*
%{_mandir}/man1/opa_osd_exercise.1*
%{_mandir}/man1/opa_osd_perf.1*
%{_mandir}/man1/opa_osd_query.1*
%config(noreplace) %{_sysconfdir}/rdma/dsap.conf

%changelog
* Thu Jun 2 2016 Scott Breyer <scott.j.breyer@intel.com> - 10.1.0-ifs
- Update to latest from build 10.1.0.0.145 (FF 10.1.0.0.126)
