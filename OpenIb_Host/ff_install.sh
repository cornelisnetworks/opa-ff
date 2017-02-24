#!/bin/bash

[ -z "${BUILDDIR}" ] && BUILDDIR="."
[ -z "${DESTDIR}" ] && DESTDIR="/"
[ -z "${LIBDIR}" ] && LIBDIR="/usr/lib"

if [ ! -f "${BUILDDIR}/RELEASE_PATH" ]; then
    echo "Wrong BUILDDIR? No such file ${BUILDDIR}/RELEASE_PATH"
    exit 1
fi

basic_tools_sbin="opacapture opafabricinfo opagetvf opagetvf_env
	opahfirev opapacketcapture opaportinfo oparesolvehfiport
	opasaquery opasmaquery opainfo opatmmtool"

basic_tools_sbin_sym="opapmaquery opaportconfig"

basic_tools_opt="setup_self_ssh usemem opaipcalc"

basic_mans="opacapture.1 opaconfig.1 opafabricinfo.1 opagetvf.1
	opagetvf_env.1 opahfirev.1 opainfo.1 opapacketcapture.1
	opapmaquery.1 opaportconfig.1 opaportinfo.1 oparesolvehfiport.1
	opasaquery.1 opasmaquery.1 opatmmtool.1"

ff_tools_opt="opaswquery opaswconfigure opaswfwconfigure
	opaswfwupdate opaswfwverify opaswping opaswreset"

ff_tools_exp="basic.exp chassis.exp chassis_configure.exp
	chassis_fmconfig.exp chassis_fmcontrol.exp chassis_fmgetconfig.exp
	chassis_getconfig.exp chassis_reboot.exp
	chassis_fmgetsecurityfiles.exp chassis_fmsecurityfiles.exp
	chassis_upgrade.exp common_funcs.exp configipoib.exp extmng.exp
	ff_function.exp ib.exp opa_to_xml.exp ibtools.exp install.exp
	ipoibping.exp load.exp mpi.exp mpiperf.exp mpiperfdeviation.exp
	network.exp proc_mgr.exp reboot.exp sacache.exp sm_control.exp
	switch_capture.exp switch_configure.exp switch_dump.exp
	switch_fwverify.exp switch_getconfig.exp switch_hwvpd.exp
	switch_info.exp switch_ping.exp switch_reboot.exp
	switch_upgrade.exp target.exp tools.exp upgrade.exp tclIndex
	tcl_proc comm12 front"

ff_tools_sbin="opacabletest opacheckload opaextracterror
	opaextractlink opaextractperf opaextractstat opaextractstat2
	opafindgood opafirmware opagenchassis opagenesmchassis
	opagenswitches opalinkanalysis opareport opareports
	opasorthosts opatop opaxlattopology opaxlattopology_cust
	opaxmlextract opaxmlfilter opaxmlgenerate opaxmlindent
	opaallanalysis opacaptureall opachassisanalysis opacmdall
	opadownloadall opaesmanalysis opafabricanalysis opafastfabric
	opahostsmanalysis opadisablehosts opadisableports opaenableports
	opaledports opaexpandfile opaextractbadlinks opaextractlids
	opaextractsellinks opaextractmissinglinks opaswenableall
	opaswdisableall opaverifyhosts opahostadmin opachassisadmin
	opaswitchadmin opapingall opascpall opasetupssh opashowallports
	opauploadall opapaquery opashowmc opafequery"

ff_tools_misc="ff_funcs opachassisip opagenswitcheshelper
	chassis_setup switch_setup opagetipaddrtype
	opafastfabric.conf.def show_counts"

ff_tools_fm="config_generate config_diff config_check config_convert"

ff_libs_misc="libqlgc_fork.so"

ff_mans="opaallanalysis.8 opacabletest.8 opacaptureall.8
	opachassisadmin.8 opachassisanalysis.8 opacheckload.8 opacmdall.8
	opadisablehosts.8 opadisableports.8 opadownloadall.8
	opaenableports.8 opaledports.8 opaesmanalysis.8 opaexpandfile.8
	opaextractbadlinks.8 opaextracterror.8 opaextractlids.8
	opaextractlink.8 opaextractperf.8 opaextractsellinks.8
	opaextractstat.8 opaextractstat2.8 opafabricanalysis.8
	opafastfabric.8 opafequery.8 opafindgood.8 opafmconfigcheck.8
	opafmconfigdiff.8 opagenchassis.8 opagenesmchassis.8
	opagenswitches.8 opagentopology.8 opahostadmin.8 opahostsmanalysis.8
	opalinkanalysis.8 opapaquery.8 opapingall.8 opareport.8
	opareports.8 opascpall.8 opasetupssh.8 opashowallports.8
	opasorthosts.8 opaswitchadmin.8 opatop.8 opauploadall.8
	opaverifyhosts.8 opaxlattopology.8 opaxlattopology_cust.8
	opashowmc.8 opaxmlextract.8 opaxmlfilter.8 opaxmlgenerate.8
	opaxmlindent.8 opaswdisableall.8 opaswenableall.8 opafirmware.8
	opaextractmissinglinks.8"

ff_iba_samples="hostverify.sh opatopology_FIs.txt opatopology_links.txt
	opatopology_SMs.txt opatopology_SWs.txt linksum_swd06.csv
	linksum_swd24.csv README.topology README.xlat_topology
	topology_cust.xlsx topology.xlsx allhosts-sample chassis-sample
	hosts-sample switches-sample ports-sample opaff.xml-sample
	mac_to_dhcp filterFile.txt triggerFile.txt opamon.conf-sample
	opamon.si.conf-sample opafastfabric.conf-sample
	opa_ca_openssl.cnf-sample opa_comp_openssl.cnf-sample
	opagentopology esm_chassis-sample"

help_doc="opatop_group_bw.hlp opatop_group_config.hlp
	opatop_group_err.hlp opatop_group_focus.hlp opatop_group_info_sel.hlp
	opatop_img_config.hlp opatop_pm_config.hlp opatop_port_stats.hlp
	opatop_summary.hlp opatop_vf_bw.hlp opatop_vf_info_sel.hlp
	opatop_vf_config.hlp"

opasadb_bin="opa_osd_dump opa_osd_exercise opa_osd_perf opa_osd_query"

opasadb_header="opasadb_path.h opasadb_route.h opasadb_route2.h"

opasadb_mans="opa_osd_dump.1 opa_osd_exercise.1 opa_osd_perf.1 opa_osd_query.1"

mpi_apps_files="Makefile mpi_hosts.sample README prepare_run
	select_mpi run_bw get_selected_mpi.sh get_mpi_cc.sh
	*.params gen_group_hosts gen_mpi_hosts mpi_cleanup
	stop_daemons hpl_dat_gen config_hpl2 run_hpl2 run_lat run_pmb run_imb
	run_lat2 run_bw2 run_bibw2 run_bcast2 run_app runmyapp mpicheck
	run_mpicheck run_deviation run_multibw run_mpi_stress run_osu
	run_cabletest run_allhfilatency run_nxnlatbw run_alltoall3 run_bcast3
	run_bibw3 run_bw3 run_lat3 run_mbw_mr3 run_multi_lat3 run_batch_script
	run_batch_cabletest hpl-count.diff groupstress deviation
	hpl-config/HPL.dat-* hpl-config/README"

shmem_apps_files="Makefile mpi_hosts.sample prepare_run README select_mpi
	run_barrier run_get_bibw run_get_bw run_get_latency run_put_bibw
	run_put_bw run_put_latency run_reduce run_hello run_alltoall
	run_rand shmem-hello.c"

release_string="IntelOPA-Tools-FF.$BUILD_TARGET_OS_ID.$MODULEVERSION"

#rm -rf $RPM_BUILD_ROOT
mkdir -p ${DESTDIR}/usr/bin
mkdir -p ${DESTDIR}/usr/sbin
mkdir -p ${DESTDIR}/usr/lib/opa/{tools,fm_tools,help,samples,src/mpi_apps,src/shmem_apps}
mkdir -p ${DESTDIR}$LIBDIR/ibacm
mkdir -p ${DESTDIR}/etc/rdma
mkdir -p ${DESTDIR}/etc/opa
mkdir -p ${DESTDIR}/etc/sysconfig/opa
mkdir -p ${DESTDIR}/usr/include/infiniband
mkdir -p ${DESTDIR}/usr/share/man/{man1,man8}
> ${BUILDDIR}/basic_file.list
> ${BUILDDIR}/ff_file.list


#Binaries and scripts installing (basic tools)
#cd builtbin.OPENIB_FF.release
cd $(cat ${BUILDDIR}/RELEASE_PATH)

cd bin
cp -t ${DESTDIR}/usr/sbin $basic_tools_sbin
cp -t ${DESTDIR}/usr/lib/opa/tools/ $basic_tools_opt
ln -s ./opaportinfo ${DESTDIR}/usr/sbin/opaportconfig
ln -s ./opasmaquery ${DESTDIR}/usr/sbin/opapmaquery
for i in $basic_tools_sbin $basic_tools_sbin_sym
do
	echo "/usr/sbin/$i" >> ${BUILDDIR}/basic_file.list
done
for i in $basic_tools_opt
do
	echo "/usr/lib/opa/tools/$i" >> ${BUILDDIR}/basic_file.list
done

cd ../opasadb
cp -t ${DESTDIR}/usr/bin $opasadb_bin
cp -t ${DESTDIR}/usr/include/infiniband $opasadb_header

cd ../bin
cp -t ${DESTDIR}/usr/lib/opa/tools/ $ff_tools_opt
for i in $ff_tools_opt
do
	echo "/usr/lib/opa/tools/$i" >> ${BUILDDIR}/ff_file.list
done

cd ../fastfabric
cp -t ${DESTDIR}/usr/sbin $ff_tools_sbin
cp -t ${DESTDIR}/usr/lib/opa/tools/ $ff_tools_misc
cp -t ${DESTDIR}/usr/lib/opa/help $help_doc
for i in $ff_tools_sbin
do
	echo "/usr/sbin/$i" >> ${BUILDDIR}/ff_file.list
done
for i in $ff_tools_misc
do
	echo "/usr/lib/opa/tools/$i" >> ${BUILDDIR}/ff_file.list
done
for i in $help_doc
do
	echo "/usr/lib/opa/help/$i" >> ${BUILDDIR}/ff_file.list
done

cd ../etc
cp -t ${DESTDIR}/usr/lib/opa/fm_tools/ $ff_tools_fm
ln -s /usr/lib/opa/fm_tools/config_check ${DESTDIR}/usr/sbin/opafmconfigcheck
ln -s /usr/lib/opa/fm_tools/config_diff ${DESTDIR}/usr/sbin/opafmconfigdiff
for i in $ff_tools_fm
do
	echo "/usr/lib/opa/fm_tools/$i" >> ${BUILDDIR}/ff_file.list
done
for i in opafmconfigcheck opafmconfigdiff
do
	echo "/usr/sbin/$i" >> ${BUILDDIR}/ff_file.list
done

cd ../fastfabric/samples
cp -t ${DESTDIR}/usr/lib/opa/samples $ff_iba_samples
for i in $ff_iba_samples
do
	echo "/usr/lib/opa/samples/$i" >> ${BUILDDIR}/ff_file.list
done
cd ..

cd ../fastfabric/tools
cp -t ${DESTDIR}/usr/lib/opa/tools/ $ff_tools_exp
cp -t ${DESTDIR}/usr/lib/opa/tools/ $ff_libs_misc
cp -t ${DESTDIR}/usr/lib/opa/tools/ osid_wrapper
cp -t ${DESTDIR}/etc/sysconfig/opa allhosts chassis esm_chassis hosts ports switches opaff.xml
for i in $ff_tools_exp $ff_libs_misc
do
	echo "/usr/lib/opa/tools/$i" >> ${BUILDDIR}/ff_file.list
done
cd ..

cd ../man/man1
cp -t ${DESTDIR}/usr/share/man/man1 $basic_mans
for i in $basic_mans
do
	echo "/usr/share/man/man1/${i}.gz" >> ${BUILDDIR}/basic_file.list
done
cp -t ${DESTDIR}/usr/share/man/man1 $opasadb_mans
cd ../man8
cp -t ${DESTDIR}/usr/share/man/man8 $ff_mans
for i in $ff_mans
do
	echo "/usr/share/man/man8/${i}.gz" >> ${BUILDDIR}/ff_file.list
done
cd ..

cd ../src/mpi/mpi_apps
tar -xzf mpi_apps.tgz -C ${DESTDIR}/usr/lib/opa/src/mpi_apps/
for i in $mpi_apps_files
do
	echo "/usr/lib/opa/src/mpi_apps/$i" >> ${BUILDDIR}/ff_file.list
done
cd ../../

cd ../src/shmem/shmem_apps
tar -xzf shmem_apps.tgz -C ${DESTDIR}/usr/lib/opa/src/shmem_apps/
for i in $shmem_apps_files
do
	echo "/usr/lib/opa/src/shmem_apps/$i" >> ${BUILDDIR}/ff_file.list
done
cd ../../

#Config files
cd ../config
cp -t ${DESTDIR}/etc/rdma dsap.conf
cp -t ${DESTDIR}/etc/sysconfig/opa opamon.conf opamon.si.conf

#Libraries installing
#cd ../builtlibs.OPENIB_FF.release
cd $(cat ${BUILDDIR}/LIB_PATH)
cp -t ${DESTDIR}${LIBDIR} libopasadb.so.*
cp -t ${DESTDIR}${LIBDIR}/ibacm libdsap.so.*

# Now that we've put everything in the buildroot, copy any default config files to their expected location for user
# to edit. To prevent nuking existing user configs, the files section of this spec file will reference these as noreplace
# config files.
cp ${DESTDIR}/usr/lib/opa/tools/opafastfabric.conf.def ${DESTDIR}/etc/sysconfig/opa/opafastfabric.conf

