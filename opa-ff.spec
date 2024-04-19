Name: opa
Version: 10.12.1.0
Release: 1
Summary: Cornelis Omni-Path basic tools and libraries for fabric managment.

Group: System Environment/Libraries
License: GPLv2/BSD
Url: https://github.com/cornelisnetworks/opa-ff
# tarball created by:
# git clone https://github.com/cornelisnetworks/opa-ff.git
# cd opa-ff
# tar czf opa.tgz --exclude-vcs .
Source: opa.tgz
# The Cornelis(TM) OPX product line is only available on x86_64 platforms at this time.

%description
This package contains the tools necessary to manage an Cornelis(TM) Omni-Path Express fabric.
IFSComponent: Tools_FF 10.12.1.0.6

%package basic-tools
Summary: Managment level tools and scripts.
Group: System Environment/Libraries

Epoch: 1

%description basic-tools
Contains basic tools for fabric managment necessary on all compute nodes.
IFSComponent: Tools_FF 10.12.1.0.6

%package fastfabric
Summary: Management level tools and scripts.
Group: System Environment/Libraries

%description fastfabric
Contains tools for managing fabric on a managment node.
IFSComponent: Tools_FF 10.12.1.0.6

%package address-resolution
Summary: Contains Address Resolution manager
Group: System Environment/Libraries

%description address-resolution
This package contains the ibacm distributed SA provider (dsap) for name and address resolution on OPA platform.
It also contains the library and tools to access the shared memory database exported by dsap.
IFSComponent: Tools_FF 10.12.1.0.6

%package libopamgt
Summary: Omni-Path management API library
Group: System Environment/Libraries

%description libopamgt
This package contains the library necessary to build applications that interface with an Omni-Path FM.
IFSComponent: Tools_FF 10.12.1.0.6


%package libopamgt-devel
Summary: Omni-Path library development headers
Group: System Environment/Libraries

%description libopamgt-devel
This package contains the necessary headers for opamgt development.
IFSComponent: Tools_FF 10.12.1.0.6

%prep
#rm -rf %{_builddir}/*
#tar xzf %_sourcedir/%name.tgz
%setup -q -c

%build
cd OpenIb_Host
OPA_FEATURE_SET=opa10 ./ff_build.sh %{_builddir} $BUILD_ARGS


%install
BUILDDIR=%{_builddir} DESTDIR=%{buildroot} LIBDIR=/usr/lib DSAP_LIBDIR=%{_libdir} ./OpenIb_Host/ff_install.sh

%post address-resolution -p /sbin/ldconfig
%postun address-resolution -p /sbin/ldconfig

%preun fastfabric
cd /usr/src/opa/mpi_apps >/dev/null 2>&1
make -k clean >/dev/null 2>&1 || : # suppress all errors and return codes from the make clean.

%post libopamgt -p /sbin/ldconfig
%postun libopamgt -p /sbin/ldconfig

%preun libopamgt-devel
cd /usr/src/opamgt >/dev/null 2>&1
make -k clean >/dev/null 2>&1 || :

%files basic-tools
%{_sbindir}/opacapture
%{_sbindir}/opafabricinfo
%{_sbindir}/opagetvf
%{_sbindir}/opagetvf_env
%{_sbindir}/opahfirev
%{_sbindir}/opapacketcapture
%{_sbindir}/opaportinfo
%{_sbindir}/oparesolvehfiport
%{_sbindir}/opasaquery
%{_sbindir}/opasmaquery
%{_sbindir}/opainfo
%{_sbindir}/opapmaquery
%{_sbindir}/opaportconfig
/usr/lib/opa/tools/setup_self_ssh
/usr/lib/opa/tools/usemem
/usr/lib/opa/tools/opaipcalc
/usr/lib/opa/tools/stream
%{_mandir}/man1/opacapture.1.gz
%{_mandir}/man1/opafabricinfo.1.gz
%{_mandir}/man1/opagetvf.1.gz
%{_mandir}/man1/opagetvf_env.1.gz
%{_mandir}/man1/opahfirev.1.gz
%{_mandir}/man1/opainfo.1.gz
%{_mandir}/man1/opapacketcapture.1.gz
%{_mandir}/man1/opapmaquery.1.gz
%{_mandir}/man1/opaportconfig.1.gz
%{_mandir}/man1/opaportinfo.1.gz
%{_mandir}/man1/oparesolvehfiport.1.gz
%{_mandir}/man1/opasaquery.1.gz
%{_mandir}/man1/opasmaquery.1.gz
/usr/share/opa/samples/opamgt_tls.xml-sample
%config(noreplace) %{_sysconfdir}/opa/opamgt_tls.xml

%files fastfabric
%{_sbindir}/opacabletest
%{_sbindir}/opacheckload
%{_sbindir}/opaextracterror
%{_sbindir}/opaextractlink
%{_sbindir}/opaextractperf
%{_sbindir}/opaextractstat
%{_sbindir}/opaextractstat2
%{_sbindir}/opafindgood
%{_sbindir}/opafirmware
%{_sbindir}/opagenchassis
%{_sbindir}/opagenesmchassis
%{_sbindir}/opagenswitches
%{_sbindir}/opalinkanalysis
%{_sbindir}/opareport
%{_sbindir}/opareports
%{_sbindir}/opasorthosts
%{_sbindir}/opatop
%{_sbindir}/opaxlattopology
%{_sbindir}/opaxmlextract
%{_sbindir}/opaxmlfilter
%{_sbindir}/opaxmlgenerate
%{_sbindir}/opaxmlindent
%{_sbindir}/opaallanalysis
%{_sbindir}/opacaptureall
%{_sbindir}/opachassisanalysis
%{_sbindir}/opacmdall
%{_sbindir}/opadownloadall
%{_sbindir}/opaesmanalysis
%{_sbindir}/opafabricanalysis
%{_sbindir}/opafastfabric
%{_sbindir}/opahostsmanalysis
%{_sbindir}/opadisablehosts
%{_sbindir}/opadisableports
%{_sbindir}/opaenableports
%{_sbindir}/opaledports
%{_sbindir}/opaexpandfile
%{_sbindir}/opaextractbadlinks
%{_sbindir}/opaextractlids
%{_sbindir}/opaextractsellinks
%{_sbindir}/opaextractmissinglinks
%{_sbindir}/opaswenableall
%{_sbindir}/opaswdisableall
%{_sbindir}/opaverifyhosts
%{_sbindir}/opahostadmin
%{_sbindir}/opachassisadmin
%{_sbindir}/opaswitchadmin
%{_sbindir}/opapingall
%{_sbindir}/opascpall
%{_sbindir}/opasetupssh
%{_sbindir}/opashowallports
%{_sbindir}/opauploadall
%{_sbindir}/opapaquery
%{_sbindir}/opashowmc
%{_sbindir}/opa2rm
%{_sbindir}/opaextractperf2
%{_sbindir}/opamergeperf2
%{_sbindir}/opafmconfigcheck
%{_sbindir}/opafmconfigdiff
/usr/lib/opa/tools/opaswquery
/usr/lib/opa/tools/opaswconfigure
/usr/lib/opa/tools/opaswfwconfigure
/usr/lib/opa/tools/opaswfwupdate
/usr/lib/opa/tools/opaswfwverify
/usr/lib/opa/tools/opaswping
/usr/lib/opa/tools/opaswreset
/usr/lib/opa/tools/ff_funcs
/usr/lib/opa/tools/opachassisip
/usr/lib/opa/tools/opagenswitcheshelper
/usr/lib/opa/tools/chassis_setup
/usr/lib/opa/tools/switch_setup
/usr/lib/opa/tools/opagetipaddrtype
/usr/lib/opa/tools/opafastfabric.conf.def
/usr/lib/opa/tools/show_counts
/usr/lib/opa/tools/opacablehealthcron
/usr/lib/opa/tools/basic.exp
/usr/lib/opa/tools/chassis.exp
/usr/lib/opa/tools/chassis_configure.exp
/usr/lib/opa/tools/chassis_fmconfig.exp
/usr/lib/opa/tools/chassis_fmcontrol.exp
/usr/lib/opa/tools/chassis_fmgetconfig.exp
/usr/lib/opa/tools/chassis_getconfig.exp
/usr/lib/opa/tools/chassis_reboot.exp
/usr/lib/opa/tools/chassis_fmgetsecurityfiles.exp
/usr/lib/opa/tools/chassis_fmsecurityfiles.exp
/usr/lib/opa/tools/chassis_upgrade.exp
/usr/lib/opa/tools/common_funcs.exp
/usr/lib/opa/tools/configipoib.exp
/usr/lib/opa/tools/extmng.exp
/usr/lib/opa/tools/ff_function.exp
/usr/lib/opa/tools/ib.exp
/usr/lib/opa/tools/opa_to_xml.exp
/usr/lib/opa/tools/ibtools.exp
/usr/lib/opa/tools/install.exp
/usr/lib/opa/tools/ipoibping.exp
/usr/lib/opa/tools/load.exp
/usr/lib/opa/tools/mpi.exp
/usr/lib/opa/tools/mpiperf.exp
/usr/lib/opa/tools/mpiperfdeviation.exp
/usr/lib/opa/tools/network.exp
/usr/lib/opa/tools/proc_mgr.exp
/usr/lib/opa/tools/reboot.exp
/usr/lib/opa/tools/sacache.exp
/usr/lib/opa/tools/sm_control.exp
/usr/lib/opa/tools/switch_capture.exp
/usr/lib/opa/tools/switch_configure.exp
/usr/lib/opa/tools/switch_dump.exp
/usr/lib/opa/tools/switch_fwverify.exp
/usr/lib/opa/tools/switch_getconfig.exp
/usr/lib/opa/tools/switch_hwvpd.exp
/usr/lib/opa/tools/switch_info.exp
/usr/lib/opa/tools/switch_ping.exp
/usr/lib/opa/tools/switch_reboot.exp
/usr/lib/opa/tools/switch_upgrade.exp
/usr/lib/opa/tools/target.exp
/usr/lib/opa/tools/tools.exp
/usr/lib/opa/tools/upgrade.exp
/usr/lib/opa/tools/tclIndex
/usr/lib/opa/tools/tcl_proc
/usr/lib/opa/tools/comm12
/usr/lib/opa/tools/front
/usr/lib/opa/tools/libqlgc_fork.so
/usr/share/opa/help/opatop_group_bw.hlp
/usr/share/opa/help/opatop_group_config.hlp
/usr/share/opa/help/opatop_group_ctg.hlp
/usr/share/opa/help/opatop_group_focus.hlp
/usr/share/opa/help/opatop_group_info_sel.hlp
/usr/share/opa/help/opatop_img_config.hlp
/usr/share/opa/help/opatop_pm_config.hlp
/usr/share/opa/help/opatop_port_stats.hlp
/usr/share/opa/help/opatop_summary.hlp
/usr/share/opa/help/opatop_vf_bw.hlp
/usr/share/opa/help/opatop_vf_info_sel.hlp
/usr/share/opa/help/opatop_vf_config.hlp
/usr/lib/opa/fm_tools/config_generate
/usr/lib/opa/fm_tools/config_diff
/usr/lib/opa/fm_tools/config_check
/usr/lib/opa/fm_tools/config_convert
/usr/share/opa/samples/hostverify.sh
/usr/share/opa/samples/opatopology_FIs.txt
/usr/share/opa/samples/opatopology_links.txt
/usr/share/opa/samples/opatopology_SMs.txt
/usr/share/opa/samples/opatopology_SWs.txt
/usr/share/opa/samples/linksum_swd06.csv
/usr/share/opa/samples/linksum_swd24.csv
/usr/share/opa/samples/README.topology
/usr/share/opa/samples/README.xlat_topology
/usr/share/opa/samples/minimal_topology.xlsx
/usr/share/opa/samples/detailed_topology.xlsx
/usr/share/opa/samples/allhosts-sample
/usr/share/opa/samples/chassis-sample
/usr/share/opa/samples/hosts-sample
/usr/share/opa/samples/switches-sample
/usr/share/opa/samples/ports-sample
/usr/share/opa/samples/mac_to_dhcp
/usr/share/opa/samples/filterFile.txt
/usr/share/opa/samples/triggerFile.txt
/usr/share/opa/samples/opamon.conf-sample
/usr/share/opa/samples/opamon.si.conf-sample
/usr/share/opa/samples/opafastfabric.conf-sample
/usr/share/opa/samples/opa_ca_openssl.cnf-sample
/usr/share/opa/samples/opa_comp_openssl.cnf-sample
/usr/share/opa/samples/opagentopology
/usr/share/opa/samples/esm_chassis-sample
%{_mandir}/man8/opaallanalysis.8.gz
%{_mandir}/man8/opacabletest.8.gz
%{_mandir}/man8/opacaptureall.8.gz
%{_mandir}/man8/opachassisadmin.8.gz
%{_mandir}/man8/opachassisanalysis.8.gz
%{_mandir}/man8/opacheckload.8.gz
%{_mandir}/man8/opacmdall.8.gz
%{_mandir}/man8/opadisablehosts.8.gz
%{_mandir}/man8/opadisableports.8.gz
%{_mandir}/man8/opadownloadall.8.gz
%{_mandir}/man8/opaenableports.8.gz
%{_mandir}/man8/opaledports.8.gz
%{_mandir}/man8/opaesmanalysis.8.gz
%{_mandir}/man8/opaexpandfile.8.gz
%{_mandir}/man8/opaextractbadlinks.8.gz
%{_mandir}/man8/opaextracterror.8.gz
%{_mandir}/man8/opaextractlids.8.gz
%{_mandir}/man8/opaextractlink.8.gz
%{_mandir}/man8/opaextractperf.8.gz
%{_mandir}/man8/opaextractsellinks.8.gz
%{_mandir}/man8/opaextractstat.8.gz
%{_mandir}/man8/opaextractstat2.8.gz
%{_mandir}/man8/opafabricanalysis.8.gz
%{_mandir}/man8/opafastfabric.8.gz
%{_mandir}/man8/opafindgood.8.gz
%{_mandir}/man8/opafmconfigcheck.8.gz
%{_mandir}/man8/opafmconfigdiff.8.gz
%{_mandir}/man8/opagenchassis.8.gz
%{_mandir}/man8/opagenesmchassis.8.gz
%{_mandir}/man8/opagenswitches.8.gz
%{_mandir}/man8/opagentopology.8.gz
%{_mandir}/man8/opahostadmin.8.gz
%{_mandir}/man8/opahostsmanalysis.8.gz
%{_mandir}/man8/opalinkanalysis.8.gz
%{_mandir}/man8/opapaquery.8.gz
%{_mandir}/man8/opapingall.8.gz
%{_mandir}/man8/opareport.8.gz
%{_mandir}/man8/opareports.8.gz
%{_mandir}/man8/opascpall.8.gz
%{_mandir}/man8/opasetupssh.8.gz
%{_mandir}/man8/opashowallports.8.gz
%{_mandir}/man8/opasorthosts.8.gz
%{_mandir}/man8/opaswitchadmin.8.gz
%{_mandir}/man8/opatop.8.gz
%{_mandir}/man8/opauploadall.8.gz
%{_mandir}/man8/opaverifyhosts.8.gz
%{_mandir}/man8/opaxlattopology.8.gz
%{_mandir}/man8/opashowmc.8.gz
%{_mandir}/man8/opaxmlextract.8.gz
%{_mandir}/man8/opaxmlfilter.8.gz
%{_mandir}/man8/opaextractperf2.8.gz
%{_mandir}/man8/opamergeperf2.8.gz
%{_mandir}/man8/opaxmlgenerate.8.gz
%{_mandir}/man8/opaxmlindent.8.gz
%{_mandir}/man8/opaswdisableall.8.gz
%{_mandir}/man8/opaswenableall.8.gz
%{_mandir}/man8/opafirmware.8.gz
%{_mandir}/man8/opaextractmissinglinks.8.gz
%{_mandir}/man8/opa2rm.8.gz
/usr/src/opa/mpi_apps/Makefile
/usr/src/opa/mpi_apps/mpi_hosts.sample
/usr/src/opa/mpi_apps/README
/usr/src/opa/mpi_apps/prepare_run
/usr/src/opa/mpi_apps/select_mpi
/usr/src/opa/mpi_apps/run_bw
/usr/src/opa/mpi_apps/get_selected_mpi.sh
/usr/src/opa/mpi_apps/get_mpi_cc.sh
/usr/src/opa/mpi_apps/*.params
/usr/src/opa/mpi_apps/gen_group_hosts
/usr/src/opa/mpi_apps/gen_mpi_hosts
/usr/src/opa/mpi_apps/mpi_cleanup
/usr/src/opa/mpi_apps/stop_daemons
/usr/src/opa/mpi_apps/hpl_dat_gen
/usr/src/opa/mpi_apps/config_hpl2
/usr/src/opa/mpi_apps/run_hpl2
/usr/src/opa/mpi_apps/run_lat
/usr/src/opa/mpi_apps/run_imb
/usr/src/opa/mpi_apps/run_lat2
/usr/src/opa/mpi_apps/run_bw2
/usr/src/opa/mpi_apps/run_bibw2
/usr/src/opa/mpi_apps/run_bcast2
/usr/src/opa/mpi_apps/run_app
/usr/src/opa/mpi_apps/runmyapp
/usr/src/opa/mpi_apps/mpicheck
/usr/src/opa/mpi_apps/run_mpicheck
/usr/src/opa/mpi_apps/run_deviation
/usr/src/opa/mpi_apps/run_multibw
/usr/src/opa/mpi_apps/run_mpi_stress
/usr/src/opa/mpi_apps/run_osu
/usr/src/opa/mpi_apps/run_cabletest
/usr/src/opa/mpi_apps/run_allhfilatency
/usr/src/opa/mpi_apps/run_nxnlatbw
/usr/src/opa/mpi_apps/run_alltoall3
/usr/src/opa/mpi_apps/run_bcast3
/usr/src/opa/mpi_apps/run_bibw3
/usr/src/opa/mpi_apps/run_bw3
/usr/src/opa/mpi_apps/run_lat3
/usr/src/opa/mpi_apps/run_mbw_mr3
/usr/src/opa/mpi_apps/run_multi_lat3
/usr/src/opa/mpi_apps/run_batch_script
/usr/src/opa/mpi_apps/run_batch_cabletest
/usr/src/opa/mpi_apps/hpl-count.diff
/usr/src/opa/mpi_apps/groupstress
/usr/src/opa/mpi_apps/deviation
/usr/src/opa/mpi_apps/hpl-config/HPL.dat-*
/usr/src/opa/mpi_apps/hpl-config/README
/usr/src/opa/mpi_apps/mpicc
/usr/src/opa/mpi_apps/mpicxx
/usr/src/opa/mpi_apps/mpif77
%{_sysconfdir}/opa/opamon.si.conf
# Replace opamon.si.conf, as it's a template config file.
%config(noreplace) %{_sysconfdir}/opa/opafastfabric.conf
%config(noreplace) %{_sysconfdir}/opa/opamon.conf
%config(noreplace) %{_sysconfdir}/opa/allhosts
%config(noreplace) %{_sysconfdir}/opa/chassis
%config(noreplace) %{_sysconfdir}/opa/esm_chassis
%config(noreplace) %{_sysconfdir}/opa/hosts
%config(noreplace) %{_sysconfdir}/opa/ports
%config(noreplace) %{_sysconfdir}/opa/switches
%config(noreplace) %{_sysconfdir}/cron.d/opa-cablehealth
%config(noreplace) /usr/lib/opa/tools/osid_wrapper


%files address-resolution
%{_bindir}/opa_osd_dump
%{_bindir}/opa_osd_exercise
%{_bindir}/opa_osd_perf
%{_bindir}/opa_osd_query
%{_bindir}/opa_osd_query_many
%{_bindir}/opa_osd_load
%{_libdir}/ibacm
%{_libdir}/libopasadb.so*
%{_includedir}/infiniband
%{_mandir}/man1/opa_osd_dump.1*
%{_mandir}/man1/opa_osd_exercise.1*
%{_mandir}/man1/opa_osd_perf.1*
%{_mandir}/man1/opa_osd_query.1*
%config(noreplace) %{_sysconfdir}/rdma/dsap.conf
%config(noreplace) %{_sysconfdir}/rdma/op_path_rec.conf
%{_sysconfdir}/rdma/opasadb.xml

%files libopamgt
/usr/lib/libopamgt.*


%files libopamgt-devel
%{_includedir}/opamgt
/usr/src/opamgt

%changelog
* Mon Feb 26 2018 Jijun Wang <jijun.wang@intel.com> - 10.8.0.0
- Added epoch for RHEL address-resolution, basic-tools and fastfabric
- Added component information in description for all rpms
* Thu Apr 13 2017 Scott Breyer <scott.j.breyer@intel.com> - 10.5.0.0
- Updates for spec file cleanup
* Fri Oct 10 2014 Erik E. Kahn <erik.kahn@intel.com> - 1.0.0-ifs
- Initial version
