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

# [ICS VERSION STRING: unknown]

# for queries we need 2 valid guids and a node description in the fabric
# for ease, guids can be specified as the last 6 digits of a SilverStorm Guid
# for a node in the fabric. or you can edit the assignments below appropriately
#OPA_REPORT='/usr/sbin/opareport -X snap.xml'
OPA_REPORT='/usr/sbin/opareport'
OPA_REPORT2='/usr/sbin/opareport'
if [ "$1" = "" ]
then
	nodedesc=duster
else
	nodedesc="$1"
fi
if [ "$2" = "" ]
then
	nodedesc2=cuda
else
	nodedesc2="$2"
fi
if [ "$3" = "" ]
then
	ioudesc='FVIC in Chassis 0x00117501da000159, Slot 8'
else
	ioudesc="$3"
fi
# to make this easier to use, we trust saquery and opareport a little
nodeguid=`opasaquery -d "$nodedesc" -o nodeguid|head -1 2>/dev/null`
sysguid=`opasaquery -d "$nodedesc" -o systemguid|head -1 2>/dev/null`
portguid=`opasaquery -d "$nodedesc" -o portguid|head -1 2>/dev/null`
nodepat="`echo "$nodedesc"|cut -c1-$(( $(echo -n "$nodedesc"|wc -c) - 2))`*"
nodepatb="*`echo "$nodedesc"|cut -c3-100`"
nodepatc="*`echo "$nodedesc"|cut -c3-$(( $(echo -n "$nodedesc"|wc -c) - 2))`*"

nodeguid2=`opasaquery -d "$nodedesc2" -o nodeguid|head -1 2>/dev/null`
portguid2=`opasaquery -d "$nodedesc2" -o portguid|head -1 2>/dev/null`

iocname=`opareport -q -o ious -F "node:$ioudesc" -d 2|grep 'ID String'|head -1|sed -e 's/^.*ID String: //' 2>/dev/null`
iocguid=`opareport -q -o ious -F "node:$ioudesc" -d 2|grep IocSlot|head -1|sed -e 's/^.*0x/0x/' 2>/dev/null`
iocpat="`echo "$iocname"|cut -c1-$(( $(echo -n "$iocname"|wc -c) - 2))`*"
iocpatb="*`echo "$iocname"|cut -c3-100`"
iocpatc="*`echo "$iocname"|cut -c3-$(( $(echo -n "$iocname"|wc -c) - 2))`*"

ioctype='VNIC'
subnet=0xfe80000000000000	# IBTA defined default subnet prefix
lid=1	# typically SM

# show what we concluded:
echo "Running reports using:"
echo "  nodedesc='$nodedesc'"
echo "  nodeguid='$nodeguid'"
echo "  sysguid='$sysguid'"
echo "  portguid='$portguid'"
echo "  nodepat='$nodepat'"
echo "  nodepatb='$nodepatb'"
echo "  nodepatc='$nodepatc'"
echo
echo "  nodedesc2='$nodedesc2'"
echo "  nodeguid2='$nodeguid2'"
echo "  portguid2='$portguid2'"
echo
echo "  ioudesc='$ioudesc'"
echo "  iocname='$iocname'"
echo "  iocpat='$iocpat'"
echo "  iocpatb='$iocpatb'"
echo "  iocpatc='$iocpatc'"
echo "  iocguid='$iocguid'"
echo "  ioctype='$ioctype'"

set -x

echo "Test basic reports"
$OPA_REPORT -o comps -d 10
$OPA_REPORT -o brcomps -d 10
$OPA_REPORT -o nodes -d 10
$OPA_REPORT -o brnodes -d 10
$OPA_REPORT -o ious -d 10
$OPA_REPORT -o links -d 10
$OPA_REPORT -o extlinks -d 10
$OPA_REPORT -o slowlinks -d 10
$OPA_REPORT -o slowconfiglinks -d 10
$OPA_REPORT -o slowconnlinks -d 10
$OPA_REPORT -o misconfiglinks -d 10
$OPA_REPORT -o misconnlinks -d 10
$OPA_REPORT -o otherports -d 10
$OPA_REPORT -o errors -d 10
$OPA_REPORT -o all -d 10
$OPA_REPORT2 -o route -S "node:$nodedesc:port:1" -D "node:$nodedesc2:port:2"
$OPA_REPORT2 -o route -S "node:$nodedesc" -D "node:$nodedesc2"
$OPA_REPORT -o none

# formats for "point"
echo "Test various Focus formats"
$OPA_REPORT -o nodes -F gid:$subnet:$portguid
$OPA_REPORT -o nodes -F lid:$lid
$OPA_REPORT -o nodes -F portguid:$portguid
$OPA_REPORT -o nodes -F nodeguid:$nodeguid
$OPA_REPORT -o nodes -F nodeguid:$nodeguid:port:1
$OPA_REPORT -o nodes -F iocguid:$iocguid
$OPA_REPORT -o nodes -F iocguid:$iocguid:port:1
$OPA_REPORT -o nodes -F systemguid:$sysguid
$OPA_REPORT -o nodes -F systemguid:$sysguid:port:1
$OPA_REPORT -o nodes -F "ioc:$iocname"
$OPA_REPORT -o nodes -F "ioc:$iocname:port:1"
$OPA_REPORT -o nodes -F "iocpat:$iocpat"
$OPA_REPORT -o nodes -F "iocpat:$iocpatb"
$OPA_REPORT -o nodes -F "iocpat:$iocpatc"
$OPA_REPORT -o nodes -F "iocpat:$iocpat:port:1"
$OPA_REPORT -o nodes -F "ioctype:$ioctype"
$OPA_REPORT -o nodes -F "ioctype:$ioctype:port:1"
$OPA_REPORT -o nodes -F "node:$nodedesc"
$OPA_REPORT -o nodes -F "node:$nodedesc:port:1"
$OPA_REPORT -o nodes -F "nodepat:$nodepat"
$OPA_REPORT -o nodes -F "nodepat:$nodepatb"
$OPA_REPORT -o nodes -F "nodepat:$nodepatc"
$OPA_REPORT -o nodes -F "nodepat:$nodepat:port:1"
$OPA_REPORT -o nodes -F "nodetype:CA"
$OPA_REPORT -o nodes -F "nodetype:CA:port:1"
$OPA_REPORT -o nodes -F "rate:10g"
$OPA_REPORT -o nodes -F "mtu:2048"
$OPA_REPORT -o nodes -F "portstate:active"
$OPA_REPORT -o nodes -F "portphysstate:linkup"
$OPA_REPORT -o nodes -F sm
$OPA_REPORT2 -o nodes -F "route:node:$nodedesc:port:1:node:$nodedesc2:port:2"
$OPA_REPORT2 -o nodes -F "route:node:$nodedesc:node:$nodedesc2"

# TBD other tests of options and flags
