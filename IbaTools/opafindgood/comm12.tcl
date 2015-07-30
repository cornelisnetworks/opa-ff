#!/usr/bin/tclsh
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

# This implements a variation of comm -12 file1 file2
# which is used by opafindgood


proc compare_files { fd1 fd2 } {
# compare input files fd1 and fd2
# each file is expected to have 2 fields separated by spaces
# files should be sorted case insensitively by first field
# first field is sorted canonical form of name
# second field is original input from user
# assumes there are no comment lines and no blank lines in input
# assumes there are no duplicate lines in input
# lines whose canonical forms match will have the fd1 second field output


	set done 0
	set line1 ""
	set line2 ""

	while { ! $done } {
		if { [ string equal "$line1" "" ] } {
			if { ! [ eof $fd1 ] } {
				gets $fd1 line1
			}
		}
		if { [ string equal "$line2" "" ] } {
			if { ! [ eof $fd2 ] } {
				gets $fd2 line2
			}
		}
		# cut field 1 at ; and just compare field 1 in each
		if { [ string equal "$line1" "" ] } {
			set done 1
			#if { [ string equal "$line2" "" ] } {
			#	set done 1
			#} else {
			#	puts "$line2"
			#	set line2 ""
			#}
		} elseif { [ string equal "$line2" "" ] } {
			set done 1
			#puts "$line1"
			#set line1 ""
		} else {
			set list1 [ concat $line1 ]
			set list2 [ concat $line2 ]
			# we have input from both files
			set res [ string compare -nocase [ lindex $list1 0] [ lindex $list2 0] ]
			if { $res <= -1 } {
				# line1 is less, read fd1 again
				#puts [ lindex $list1 1]
				set line1 ""
			} elseif { $res == 0 } {
				# equal, output and read from both
				puts [ lindex $list1 1]
				set line1 ""
				set line2 ""
			} else {
				# line1 is greater, read fd2 again
				#puts [ lindex $list2 1]
				set line2 ""
			}
		}
	}
}


if { $argc != 2 } {
	puts stderr "Usage: comm12 file1 file2"
	exit 2
}

if { [ catch {
	set fname1 [lindex $argv 0]
	set fname2 [lindex $argv 1]
	set fd1 [open $fname1 "r"]
	set fd2 [open $fname2 "r"]
	compare_files $fd1 $fd2

} res ] != 0 } {
	puts stderr "comm12: Error: $res"
	exit 1
}
exit 0
