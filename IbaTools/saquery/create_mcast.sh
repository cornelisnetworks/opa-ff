#!/bin/bash
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

# test script, not in any build.  Helps to create many multicast groups
set -x
for k in 0 1 2 3 4 5 6 7 8 9 a b c d e f
do
for j in 0 1 2 3 4 5 6 7 8 9 a b c d e f
do
        for i in 0 1 2 3 4 5 6 7 8 9 a b c d e f
        do
                #route add -net 224.0.$j.$i netmask 255.255.255.255 gw 172.21.35.23
                #route add -net 224.0.$j.$i netmask 255.255.255.255 gw 172.21.35.23
                #ipmaddr add 00:ff:ff:ff:ff:12:60:1b:00:00:00:00:00:00:00:01:ff:00:0$k:$i$j dev ib0
                ipmaddr delete 00:ff:ff:ff:ff:12:60:1b:00:00:00:00:00:00:00:01:ff:00:0$k:$i$j dev ib0
                #ipmaddr add 00:ff:ff:ff:ff:12:60:1b:00:00:00:00:00:00:00:02:ff:00:0$k:$i$j dev ib0
                ipmaddr delete 00:ff:ff:ff:ff:12:60:1b:00:00:00:00:00:00:00:02:ff:00:0$k:$i$j dev ib0
                > /dev/null
        done
done
done
for k in 0 1 2 3
do
for j in 0
do
        for i in 0
        do
                #ipmaddr add 00:ff:ff:ff:ff:12:60:1b:00:00:00:00:00:00:00:02:ff:00:0$k:$i$j dev ib0
                #ipmaddr delete 00:ff:ff:ff:ff:12:60:1b:00:00:00:00:00:00:00:02:ff:00:0$k:$i$j dev ib0
                > /dev/null
        done
done
done

