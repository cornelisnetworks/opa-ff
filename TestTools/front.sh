#!/usr/bin/expect
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

# [ICS VERSION STRING: unknown]

##
## Running Test Commands in Shell
## ------------------------------
##
## Selected TCL procedures from the automated tests
## have been made available in the shell.  When run from the shell,
## the actual TCL procedure is still run, with some setup.
## Arguments for each command are identical to those of the TCL version.
## The result from the given command is returned on stdout, (possibly along
## with additional information preceeding it)
##
## The arguments are identical to those provided within TCL, however because
## of quoting differences, shell " or ' quotes should guard each argument
## For example:
##	In TCL: some_proc {{1} {my.file}}
##	In Sh:  some_proc "{1} {my.file}"
##
## The running of such commands in the shell assumes settl and TEST_CONFIG_FILE
## have been setup
##
## At present the following commands are available in the shell:
## some typical uses:

# This shell script is a simple interface for shell scripts to run the
# a tcl function from the automated test environment.
# typically it is invoked via tcl_proc, which also sets up common
# environment variables such as TCLLIBPATH

# This script can be linked to filenames correspnding to what ever TCL commands
# need to be available from the shell.
# alternately a tcl function name can be provided as the first argument to
# this script when this script is invoked as "front"
# such as:
#	front get_config CFG_HOSTS

if { ! [ info exists env(TL_DIR) ] || $env(TL_DIR) == "" } {
        puts stderr "$argv0: run settl first"
        exit 1
}

# This is a workaround for MAC, which does not support TCLLIBPATH
# it doesn't hurt on Linux.  ibtools.exp sources all our .exp scripts
if { [ file exists $env(TL_DIR)/TestTools/ibtools.exp ] } {
	# HostTests
	source $env(TL_DIR)/TestTools/ibtools.exp
} else {
	# Fast Fabric, TL_DIR is set to /usr/lib/opa/tools
	source $env(TL_DIR)/ibtools.exp
}

global env spawn_id expecting 
if { ! [ info exists env(TEST_SHOW_CONFIG) ] } {
	set env(TEST_SHOW_CONFIG) no
}

set spawn_id ""
set expecting ""
set_timeout 60

set cmd [file tail $argv0]
if { "$cmd" != "read_config" && [lindex $argv 0] != "read_config" } {
	read_config $env(TEST_CONFIG_FILE)
}

if { "$cmd" == "front" } {
	set res [ eval $argv ]
} else {
	set res [ eval $cmd $argv ]
}

if { "$res" != "" } {
	puts $res
}
