basic_tools_sbin="opacapture opafabricinfo opagetvf opagetvf_env opahfirev
	opapacketcapture opaportinfo oparesolvehfiport opasaquery opasmaquery
	opainfo"

basic_tools_sbin_sym="opapmaquery opaportconfig"

basic_tools_opt="setup_self_ssh usemem opaipcalc stream"

basic_mans="opacapture.1 opafabricinfo.1 opagetvf.1
	opagetvf_env.1 opahfirev.1 opainfo.1 opapacketcapture.1 opapmaquery.1
	opaportconfig.1 opaportinfo.1 oparesolvehfiport.1 opasaquery.1
	opasmaquery.1"

basic_configs="opamgt_tls.xml"

basic_samples="opamgt_tls.xml-sample"

ff_tools_opt="opaswquery opaswconfigure opaswfwconfigure opaswfwupdate
	opaswfwverify opaswping opaswreset"

ff_tools_exp="basic.exp chassis.exp chassis_configure.exp chassis_fmconfig.exp
	chassis_fmcontrol.exp chassis_fmgetconfig.exp chassis_getconfig.exp
	chassis_reboot.exp chassis_fmgetsecurityfiles.exp chassis_fmsecurityfiles.exp
	chassis_upgrade.exp common_funcs.exp configipoib.exp extmng.exp ff_function.exp
	ib.exp opa_to_xml.exp ibtools.exp install.exp ipoibping.exp load.exp mpi.exp
	mpiperf.exp mpiperfdeviation.exp network.exp proc_mgr.exp reboot.exp sacache.exp
	sm_control.exp switch_capture.exp switch_configure.exp switch_dump.exp
	switch_fwverify.exp switch_getconfig.exp switch_hwvpd.exp switch_info.exp
	switch_ping.exp switch_reboot.exp switch_upgrade.exp target.exp tools.exp
	upgrade.exp tclIndex tcl_proc comm12 front"

ff_tools_sbin="opacabletest opacheckload opaextracterror opaextractlink
	opaextractperf opaextractstat opaextractstat2 opafindgood opafirmware
	opagenchassis opagenesmchassis opagenswitches opalinkanalysis opareport
	opareports opasorthosts opatop opaxlattopology opaxmlextract
	opaxmlfilter opaxmlgenerate opaxmlindent opaallanalysis opacaptureall
	opachassisanalysis opacmdall opadownloadall opaesmanalysis opafabricanalysis
	opafastfabric opahostsmanalysis opadisablehosts opadisableports opaenableports
	opaledports opaexpandfile opaextractbadlinks opaextractlids opaextractsellinks
	opaextractmissinglinks opaswenableall opaswdisableall opaverifyhosts opahostadmin
	opachassisadmin opaswitchadmin opapingall opascpall opasetupssh opashowallports
	opauploadall opapaquery opashowmc opa2rm opaextractperf2 opamergeperf2"

ff_tools_misc="ff_funcs opachassisip opagenswitcheshelper chassis_setup
	switch_setup opagetipaddrtype opafastfabric.conf.def show_counts opacablehealthcron"

ff_tools_fm="config_generate config_diff config_check config_convert"

ff_libs_misc="libqlgc_fork.so"

ff_mans="opaallanalysis.8 opacabletest.8 opacaptureall.8 opachassisadmin.8
	opachassisanalysis.8 opacheckload.8 opacmdall.8 opadisablehosts.8
	opadisableports.8 opadownloadall.8 opaenableports.8 opaledports.8
	opaesmanalysis.8 opaexpandfile.8 opaextractbadlinks.8 opaextracterror.8
	opaextractlids.8 opaextractlink.8 opaextractperf.8 opaextractsellinks.8
	opaextractstat.8 opaextractstat2.8 opafabricanalysis.8 opafastfabric.8
	opafindgood.8 opafmconfigcheck.8 opafmconfigdiff.8 opagenchassis.8
	opagenesmchassis.8 opagenswitches.8 opagentopology.8 opahostadmin.8
	opahostsmanalysis.8 opalinkanalysis.8 opapaquery.8 opapingall.8 opareport.8
	opareports.8 opascpall.8 opasetupssh.8 opashowallports.8 opasorthosts.8
	opaswitchadmin.8 opatop.8 opauploadall.8 opaverifyhosts.8 opaxlattopology.8
	opashowmc.8 opaxmlextract.8 opaxmlfilter.8 opaextractperf2.8 opamergeperf2.8 
	opaxmlgenerate.8 opaxmlindent.8 opaswdisableall.8 opaswenableall.8
	opafirmware.8 opaextractmissinglinks.8 opa2rm.8"

ff_iba_samples="hostverify.sh opatopology_FIs.txt opatopology_links.txt opatopology_SMs.txt
	opatopology_SWs.txt linksum_swd06.csv linksum_swd24.csv README.topology
	README.xlat_topology minimal_topology.xlsx detailed_topology.xlsx allhosts-sample chassis-sample
	hosts-sample switches-sample ports-sample mac_to_dhcp filterFile.txt triggerFile.txt
	opamon.conf-sample opamon.si.conf-sample opafastfabric.conf-sample
	opa_ca_openssl.cnf-sample opa_comp_openssl.cnf-sample opagentopology esm_chassis-sample"

help_doc="opatop_group_bw.hlp opatop_group_config.hlp opatop_group_ctg.hlp
	opatop_group_focus.hlp opatop_group_info_sel.hlp opatop_img_config.hlp
	opatop_pm_config.hlp opatop_port_stats.hlp opatop_summary.hlp opatop_vf_bw.hlp
	opatop_vf_info_sel.hlp opatop_vf_config.hlp"

opasadb_bin="opa_osd_dump opa_osd_exercise opa_osd_perf opa_osd_query opa_osd_query_many opa_osd_load"

opasadb_header="opasadb.h opasadb_path.h opasadb_route.h opasadb_route2.h"

opasadb_mans="opa_osd_dump.1 opa_osd_exercise.1 opa_osd_perf.1 opa_osd_query.1"

opamgt_headers="opamgt.h opamgt_pa.h opamgt_sa.h opamgt_sa_notice.h"

opamgt_iba_headers="ib_mad.h ib_sa_records.h ib_sd.h ib_sm_types.h
	ib_status.h ib_types.h stl_mad_types.h stl_pa_types.h
	stl_sa_types.h stl_sd.h stl_sm_types.h stl_types.h"

opamgt_iba_public_headers="datatypes.h datatypes_osd.h ibyteswap.h ibyteswap_osd.h
	ilist.h imath.h imemory.h imemory_osd.h ipackoff.h ipackon.h ispinlock.h
	ispinlock_osd.h statustext.h iethernet.h"

opamgt_examples="paquery.c saquery.c simple_sa_query.c simple_sa_notice.c simple_pa_query.c
	job_schedule.c show_switch_cost_matrix.c Makefile README"

mpi_apps_files="Makefile mpi_hosts.sample README prepare_run select_mpi run_bw
	get_selected_mpi.sh get_mpi_cc.sh *.params gen_group_hosts gen_mpi_hosts
	mpi_cleanup stop_daemons hpl_dat_gen config_hpl2 run_hpl2 run_lat run_imb run_lat2
	run_bw2 run_bibw2 run_bcast2 run_app runmyapp mpicheck run_mpicheck run_deviation
	run_multibw run_mpi_stress run_osu run_cabletest run_allhfilatency run_nxnlatbw
	run_alltoall3 run_bcast3 run_bibw3 run_bw3 run_lat3 run_mbw_mr3 run_multi_lat3
	run_batch_script run_batch_cabletest hpl-count.diff groupstress deviation
	hpl-config/HPL.dat-* hpl-config/README mpicc mpicxx mpif77"

opasnapconfig_bin=
