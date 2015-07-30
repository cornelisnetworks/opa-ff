Name: opa
Version: 10.0.0.0
Release: 435
Summary: Intel Omni-Path basic tools and libraries for fabric managment.

Group: System Environment/Libraries
License: GPLv2/BSD 
Url: http://www.intel.com/
Source: opa.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if 0%{?suse_version} >= 1110
%debug_package
%endif

%description
Basic package

%package basic-tools
Summary: Managment level tools and scripts.
Group: System Environment/Libraries
AutoReq: no
Requires(post): libibmad, libibumad, libibverbs

%description basic-tools
Contains basic tools for fabric managment necessary on all compute nodes.

%package fastfabric
Summary: Management level tools and scripts.
Group: System Environment/Libraries
AutoReq: no
Requires: opa-basic-tools

%description fastfabric
Contains tools for managing fabric on a managment node.

%package address-resolution
Summary: Contains Address Resolution manager
Group: System Environment/Libraries
AutoReq: no
Requires: opa-basic-tools

%description address-resolution
This is to be filled out more concisely later.

%prep
#rm -rf %{_builddir}/*
#tar xzf %_sourcedir/%name.tgz
%setup -q -c

%build
if [ -d OpenIb_Host ]
then
	cd OpenIb_Host && ./ff_build.sh %{_builddir} $FF_BUILD_ARGS
else
	./ff_build.sh %{_builddir} $FF_BUILD_ARGS
fi


%install
%define basic_tools_sbin fabric_info opacapture opafequery opagetvf opagetvf_env opahfirev opapacketcapture opaportinfo oparesolvehfiport opasaquery opashowmc opasmaquery opainfo

%define basic_tools_sbin_sym opapmaquery opaportconfig

%define basic_tools_opt opaswquery opaswconfigure opaswfwconfigure opaswfwupdate opaswfwverify opaswping opaswreset setup_self_ssh bsd_ping usemem opaipcalc 

%define basic_mans opacapture.1 opaconfig.1 opadisablehosts.1 opadisableports.1 opadownloadall.1 opahfirev.1 opapaquery.1 opapmaquery.1 opaportconfig.1 opaportinfo.1 oparesolvehfiport.1 opasaquery.1 opashowmc.1 opasmaquery.1

%define ff_tools_exp basic.exp chassis.exp chassis_configure.exp chassis_fmconfig.exp chassis_fmcontrol.exp chassis_fmgetconfig.exp chassis_getconfig.exp chassis_reboot.exp chassis_upgrade.exp common_funcs.exp configipoib.exp extmng.exp ff_function.exp ib.exp opa_to_xml.exp ibtools.exp install.exp ipoibping.exp load.exp mpi.exp mpiperf.exp mpiperfdeviation.exp network.exp proc_mgr.exp reboot.exp sacache.exp sm_control.exp switch_capture.exp switch_configure.exp switch_dump.exp switch_fwverify.exp switch_getconfig.exp switch_hwvpd.exp switch_info.exp switch_ping.exp switch_reboot.exp switch_upgrade.exp target.exp tools.exp upgrade.exp tclIndex tcl_proc comm12 front

%define ff_tools_sbin opacabletest opacheckload opaextracterror opaextractlink opaextractperf opaextractstat opaextractstat2 opafindgood opafirmware opagenchassis opagenesmchassis opagenswitches opagentopology opalinkanalysis opareport opareports opasorthosts opatop opaxlattopology opaxlattopology_cust opaxmlextract opaxmlfilter opaxmlgenerate opaxmlindent opaallanalysis opacaptureall opachassisanalysis opacmdall opadownloadall opaesmanalysis opafabricanalysis opafastfabric opahostsmanalysis opadisablehosts opadisableports opaenableports opaexpandfile opaextractbadlinks opaextractlids opaextractsellinks opaswdisableall opaverifyhosts opatest opahostadmin opachassisadmin opaswitchadmin opapingall opascpall opasetupssh showallports opauploadall opapaquery

%define ff_tools_misc ff_funcs opachassisip opagenswitcheshelper chassis_setup switch_setup opagetipaddrtype opafastfabric.conf.def

%define ff_tools_fm config_generate config_diff config_check config_convert

%define ff_libs_misc libqlgc_fork.so

%define ff_mans opacabletest.8 opacaptureall.8 opacmdall.8 opaexpandfile.8 opafastfabric.8 opalinkanalysis.8 opapingall.8 opareport.8 opasetupssh.8 opasorthosts.8 opaverifyhosts.8 opaxlattopology.8 opaxlattopology_cust.8

%define ff_iba_samples hostverify.sh opatopology_FIs.txt opatopology_links.txt opatopology_SMs.txt opatopology_SWs.txt linksum_swd06.csv linksum_swd24.csv README.topology README.xlat_topology topology_cust.xlsx topology.xlsx allhosts-sample chassis-sample hosts-sample switches-sample ports-sample opaff.xml-sample mac_to_dhcp filterFile.txt triggerFile.txt opamon.conf-sample opamon.si.conf-sample

%define help_doc opatop_group_bw.hlp opatop_group_config.hlp opatop_group_err.hlp opatop_group_focus.hlp opatop_group_info_sel.hlp opatop_img_config.hlp opatop_pm_config.hlp opatop_port_stats.hlp opatop_summary.hlp opatop_vf_bw.hlp opatop_vf_info_sel.hlp opatop_vf_config.hlp

%define opasadb_bin opa_osd_dump opa_osd_exercise opa_osd_perf opa_osd_query

%define opasadb_header opasadb_path.h opasadb_route.h opasadb_route2.h

%define mpi_apps_files Makefile mpi_hosts.sample README prepare_run select_mpi run_bw get_selected_mpi.sh get_mpi_cc.sh *.params gen_group_hosts gen_mpi_hosts mpi_cleanup stop_daemons config_hpl config_hpl2 run_hpl run_hpl2 run_lat run_pmb run_npb run_imb run_lat2 run_bw2 run_bibw2 run_bcast2 run_app runmyapp mpicheck run_mpicheck deviation run_deviation mpi_multibw run_multibw groupstress run_cabletest run_allhcalatency run_nxnlatbw run_alltoall3 run_bcast3 run_bibw3 run_bw3 run_lat3 run_mbw_mr3 run_multi_lat3 run_batch_script run_batch_cabletest ATLAS hpl-count.diff hpl hpl-2.0 bandwidth latency osu2 imb osu-micro-benchmarks-3.8-July12 NPB3.2.1 PMB2.2.1 PMB_License.doc mpi.param.np mpi.param.hpl mpi.param.pallas hpl-config/HPL.dat-1t hpl-config/HPL.dat-2t hpl-config/HPL.dat-4t hpl-config/HPL.dat-8t hpl-config/HPL.dat-16t hpl-config/HPL.dat-32t hpl-config/HPL.dat-64t hpl-config/HPL.dat-128t hpl-config/HPL.dat-256t hpl-config/HPL.dat-300t hpl-config/HPL.dat-320t hpl-config/HPL.dat-512t hpl-config/HPL.dat-1s hpl-config/HPL.dat-2s hpl-config/HPL.dat-4s hpl-config/HPL.dat-8s hpl-config/HPL.dat-16s hpl-config/HPL.dat-32s hpl-config/HPL.dat-64s hpl-config/HPL.dat-128s hpl-config/HPL.dat-256s hpl-config/HPL.dat-300s hpl-config/HPL.dat-320s hpl-config/HPL.dat-512s hpl-config/HPL.dat-1m hpl-config/HPL.dat-2m hpl-config/HPL.dat-4m hpl-config/HPL.dat-8m hpl-config/HPL.dat-16m hpl-config/HPL.dat-32m hpl-config/HPL.dat-64m hpl-config/HPL.dat-128m hpl-config/HPL.dat-256m hpl-config/HPL.dat-300m hpl-config/HPL.dat-320m hpl-config/HPL.dat-512m hpl-config/HPL.dat-1l hpl-config/HPL.dat-2l hpl-config/HPL.dat-4l hpl-config/HPL.dat-8l hpl-config/HPL.dat-16l hpl-config/HPL.dat-32l hpl-config/HPL.dat-64l hpl-config/HPL.dat-128l hpl-config/HPL.dat-256l hpl-config/HPL.dat-300l hpl-config/HPL.dat-320l hpl-config/HPL.dat-512l hpl-config/README



%define release_string IntelOPA-Tools-FF.$BUILD_TARGET_OS_ID.$MODULEVERSION

#rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_sbindir}
mkdir -p $RPM_BUILD_ROOT/opt/opa/{tools,fm_tools,help,samples,src/mpi_apps}
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
cp -t $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/opa allhosts chassis hosts ports switches opaff.xml
cd ..

cd ../man/man1
cp -t $RPM_BUILD_ROOT%{_mandir}/man1 %basic_mans
cd ../man8
cp -t $RPM_BUILD_ROOT%{_mandir}/man8 %ff_mans
cd ..


cd ../src/mpi/mpi_apps
tar -xzf mpi_apps.tgz -C $RPM_BUILD_ROOT/opt/opa/src/mpi_apps/
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
echo "/usr/share/man/man1/%{basic_mans}" > %{_builddir}/basic_mans.list
sed -i 's;[ ];\n/usr/share/man/man1/;g' %{_builddir}/basic_mans.list
sed -i 's;\.1;\.1*;g' %{_builddir}/basic_mans.list

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
echo "/usr/share/man/man8/%{ff_mans}" > %{_builddir}/ff_mans.list
sed -i 's;[ ];\n/usr/share/man/man8/;g' %{_builddir}/ff_mans.list
sed -i 's;\.8;\.8*;g' %{_builddir}/ff_mans.list

#Final file listing for 'basic'
cat %{_builddir}/basic_sbin_file.list %{_builddir}/basic_opt_file.list > %{_builddir}/basic_file.list %{_builddir}/basic_mans.list


#FF tools help doc
echo "/opt/opa/help/%{help_doc}" > %{_builddir}/ff_help_file.list
sed -i 's;[ ];\n/opt/opa/help/;g' %{_builddir}/ff_help_file.list 

#FF tools sbin
echo "%{_sbindir}/%{ff_tools_sbin}" > %{_builddir}/ff_sbin_file.list
sed -i 's;[ ];\n%{_sbindir}/;g' %{_builddir}/ff_sbin_file.list

#MPI_Apps
echo "/opt/opa/src/mpi_apps/%{mpi_apps_files}" > %{_builddir}/ff_mpi_apps.list
sed -i 's;[ ];\n/opt/opa/src/mpi_apps/;g' %{_builddir}/ff_mpi_apps.list 

#Final file listing for 'ff'
cat %{_builddir}/ff_mpi_apps.list %{_builddir}/ff_sbin_file.list %{_builddir}/ff_help_file.list %{_builddir}/ff_tools_exp.list %{_builddir}/ff_tools_misc.list %{_builddir}/ff_libs_misc.list %{_builddir}/ff_iba_samples.list %{_builddir}/ff_mans.list %{_builddir}/ff_tools_fm.list > %{_builddir}/ff_file.list


%clean
rm -rf $RPM_BUILD_ROOT

%post address-resolution -p /sbin/ldconfig
%postun address-resolution -p /sbin/ldconfig

%files basic-tools -f %{_builddir}/basic_file.list
%defattr(-,root,root,-)

%files fastfabric -f %{_builddir}/ff_file.list
%defattr(-,root,root,-)
%{_sysconfdir}/sysconfig/opa/opamon.si.conf
%config(noreplace) %{_sysconfdir}/sysconfig/opa/opafastfabric.conf
%config(noreplace) %{_sysconfdir}/sysconfig/opa/opamon.conf
%config(noreplace) %{_sysconfdir}/sysconfig/opa/allhosts
%config(noreplace) %{_sysconfdir}/sysconfig/opa/chassis
%config(noreplace) %{_sysconfdir}/sysconfig/opa/hosts
%config(noreplace) %{_sysconfdir}/sysconfig/opa/ports
%config(noreplace) %{_sysconfdir}/sysconfig/opa/switches
%config(noreplace) %{_sysconfdir}/sysconfig/opa/opaff.xml
%config(noreplace) /opt/opa/tools/osid_wrapper
%{_sbindir}/opafmconfigcheck
%{_sbindir}/opafmconfigdiff


%files address-resolution
%defattr(-,root,root,-)
#Everything under the bin directory belongs exclusively to opasadb at this time.
%{_bindir}/*
%{_libdir}/*
%{_includedir}/*
%config(noreplace) %{_sysconfdir}/rdma/dsap.conf

%changelog
* Fri Oct 10 2014 Erik E. Kahn <erik.kahn@intel.com> - 1.0.0-ifs
- Initial version
