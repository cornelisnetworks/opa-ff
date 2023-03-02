#!/usr/bin/perl
# BEGIN_ICS_COPYRIGHT8 ****************************************
#
# Copyright (c) 2015-2020, Intel Corporation
# Copyright (c) 2020-2021, Cornelis Networks, Inc.
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
use strict;
#use Term::ANSIColor;
#use Term::ANSIColor qw(:constants);
#use File::Basename;
#use Math::BigInt;

	# Names of supported install components
	# must be listed in dependency order such that prereqs appear 1st
	# delta_debug must be last
my @delta_Components_rhel72 = ( "opa_stack", "ibacm", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles12_sp2 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel73 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles12_sp3 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles12_sp4 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles12_sp5 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel74 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel75 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel76 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel77 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel78 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel8 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel81 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel82 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel83 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel84 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel85 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel86 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_rhel9 = ( "opa_stack", "mpi_selector", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles15 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles15_sp1 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles15_sp2 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles15_sp3 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );
my @delta_Components_sles15_sp4 = ( "opa_stack", "intel_hfi",
		"opa_stack_dev",
		"delta_ipoib",
		"delta_debug", );

@Components = ( );
# RHEL7.2, ibacm is a full component with rpms to install
my @delta_SubComponents_older = ( "rdma_ndd", "delta_srp", "delta_srpt" );
# RHEL7.3 and newer AND SLES12.2 and newer
my @delta_SubComponents_newer = ( "ibacm", "rdma_ndd", "delta_srp", "delta_srpt" );
@SubComponents = ( );

# override some of settings in main_omnipathwrap_delta.pl
sub overrides()
{
	# The component list has slight variations per distro
	if ( "$CUR_VENDOR_VER" eq "ES72" ) {
		@Components = ( @delta_Components_rhel72 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES122" ) {
		@Components = ( @delta_Components_sles12_sp2 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES73" ) {
		@Components = ( @delta_Components_rhel73 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES123" ) {
		@Components = ( @delta_Components_sles12_sp3 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES124" ) {
		@Components = ( @delta_Components_sles12_sp4 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES125" ) {
		@Components = ( @delta_Components_sles12_sp5 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES74" ) {
		@Components = ( @delta_Components_rhel74 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES75" ) {
		@Components = ( @delta_Components_rhel75 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES76" ) {
		@Components = ( @delta_Components_rhel76 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES77" ) {
		@Components = ( @delta_Components_rhel77 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES78" || "$CUR_VENDOR_VER" eq "ES79" ) {
		@Components = ( @delta_Components_rhel78 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES8" ) {
		@Components = ( @delta_Components_rhel8 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES81" ) {
		@Components = ( @delta_Components_rhel81 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES82" ) {
		@Components = ( @delta_Components_rhel82 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES83" ) {
		@Components = ( @delta_Components_rhel83 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES84" ) {
		@Components = ( @delta_Components_rhel84 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES85" ) {
		@Components = ( @delta_Components_rhel85 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES86" ) {
		@Components = ( @delta_Components_rhel86 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES9" ) {
		@Components = ( @delta_Components_rhel9 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES15" ) {
		@Components = ( @delta_Components_sles15 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES151" ) {
		@Components = ( @delta_Components_sles15_sp1 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES152" ) {
		@Components = ( @delta_Components_sles15_sp2 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES153" ) {
		@Components = ( @delta_Components_sles15_sp3 );
	} elsif ( "$CUR_VENDOR_VER" eq "ES154" ) {
		@Components = ( @delta_Components_sles15_sp4 );
	} else {
		# unsupported OS
		@Components = ( );
	}

	# Sub components for autostart processing
	if ( "$CUR_VENDOR_VER" eq "ES72" ) {
		@SubComponents = ( @delta_SubComponents_older );
	} else {
		@SubComponents = ( @delta_SubComponents_newer );
	}

	# TBD remove this concept
	# no WrapperComponent (eg. opaconfig)
	$WrapperComponent = "";

	# set SrcDir for all components to .
	foreach my $comp ( @Components )
	{
        $ComponentInfo{$comp}{'SrcDir'} = ".";
	}
}
