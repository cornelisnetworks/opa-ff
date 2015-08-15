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

%define basic_tools_opt opaswquery opaswconfigure opaswfwconfigure opaswfwupdate opaswfwverify opaswping opaswreset setup_self_ssh usemem opaipcalc 

%define basic_mans opacapture.1 opaconfig.1 opadisablehosts.1 opadisableports.1 opadownloadall.1 opahfirev.1 opapaquery.1 opapmaquery.1 opaportconfig.1 opaportinfo.1 oparesolvehfiport.1 opasaquery.1 opashowmc.1 opasmaquery.1

%define opasadb_bin opa_osd_dump opa_osd_exercise opa_osd_perf opa_osd_query

%define opasadb_header opasadb_path.h opasadb_route.h opasadb_route2.h


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

cd ../man/man1
cp -t $RPM_BUILD_ROOT%{_mandir}/man1 %basic_mans
cd ..

#Config files
cd ../config
cp -t $RPM_BUILD_ROOT%{_sysconfdir}/rdma dsap.conf

#Libraries installing
#cd ../builtlibs.OPENIB_FF.release
cd $(cat %{_builddir}/LIB_PATH)
cp -t $RPM_BUILD_ROOT%{_libdir} libopasadb.so.*
cp -t $RPM_BUILD_ROOT%{_libdir}/ibacm libdsap.so.*

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

#Final file listing for 'basic'
cat %{_builddir}/basic_sbin_file.list %{_builddir}/basic_opt_file.list %{_builddir}/basic_mans.list > %{_builddir}/basic_file.list

%clean
rm -rf $RPM_BUILD_ROOT

%post address-resolution -p /sbin/ldconfig
%postun address-resolution -p /sbin/ldconfig

%files basic-tools -f %{_builddir}/basic_file.list
%defattr(-,root,root,-)

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
