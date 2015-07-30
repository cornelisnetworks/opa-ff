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

#[ICS VERSION STRING: unknown]
Name: opasadb
Version: 1.0.0
Release: 1%{?dist}
Summary: opasadb library and tools

Group: Applications/System
License: Copyright (c) 2014  Intel Corporation All Rights Reserved
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_builddir}
Vendor: Intel Corporation
AutoReqProv: no

%description
The %{name} is package that contains a library for easy access to the shared
memory SA cache exported by distributed SA and a few tools that provide
the capability to stress test the distributed SA, dump and query the SA
cache, and generate jobs.

%package devel
Summary: Headers file needed when building apps to use libopasadb.
Requires: %{name} = %{version}-%{release}
Group: System Environment/Daemons

%description devel
To make use of the shared-memory SA cache, an application can call the
functions exported by the libopasadb library.

%prep
%setup -q -n %{name}-%{version}

%build
#nothing to do because we copy pre-built files into SOURCES

%install
# simply copy the files
mkdir -p ${RPM_BUILD_ROOT}%{_bindir}
cp -pr opa_osd_dump ${RPM_BUILD_ROOT}%{_bindir}/
cp -pr opa_osd_exercise ${RPM_BUILD_ROOT}%{_bindir}/
cp -pr opa_osd_exercise_test.pl ${RPM_BUILD_ROOT}%{_bindir}/
cp -pr opa_osd_perf ${RPM_BUILD_ROOT}%{_bindir}/
cp -pr build_table.pl ${RPM_BUILD_ROOT}%{_bindir}/
cp -pr opa_osd_query ${RPM_BUILD_ROOT}%{_bindir}/
mkdir -p ${RPM_BUILD_ROOT}%{_libdir}
cp -pr lib%{name}.so.%{version} ${RPM_BUILD_ROOT}%{_libdir}/
mkdir -p ${RPM_BUILD_ROOT}%{_includedir}/infiniband
cp -pr opasadb_path.h ${RPM_BUILD_ROOT}%{_includedir}/infiniband/
cp -pr opasadb_route.h ${RPM_BUILD_ROOT}%{_includedir}/infiniband/
cp -pr opasadb_route2.h ${RPM_BUILD_ROOT}%{_includedir}/infiniband/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README
%{_libdir}/lib*.*
%{_bindir}/opa_osd_dump
%{_bindir}/opa_osd_exercise
%{_bindir}/opa_osd_exercise_test.pl
%{_bindir}/opa_osd_perf
%{_bindir}/build_table.pl
%{_bindir}/opa_osd_query

%files devel
%defattr(-,root,root,-)
%{_includedir}/infiniband/opasadb_path.h
%{_includedir}/infiniband/opasadb_route.h
%{_includedir}/infiniband/opasadb_route2.h




