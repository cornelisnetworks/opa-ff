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
if [ "$1" = "" ]
then
	nodedesc=SQUIRREL_STL_HFI1
else
	nodedesc="$1"
fi
if [ "$2" = "" ]
then
	nodedesc2=SQUIRREL_STL_HFI2
else
	nodedesc2="$2"
fi
#if [ "$3" = "" ]
#then
#	ioudesc='FVIC in Chassis 0x00117501da000159, Slot 8'
#else
#	ioudesc="$3"
#fi
# to make this easier to use, we trust saquery and opareport a little
nodeguid=`opasaquery -d "$nodedesc" -o nodeguid|head -1 2>/dev/null`
sysguid=`opasaquery -d "$nodedesc" -o systemguid|head -1 2>/dev/null`
portguid=`opasaquery -d "$nodedesc" -o portguid|head -1 2>/dev/null`
nodepat="`echo "$nodedesc"|cut -c1-$(( $(echo -n "$nodedesc"|wc -c) - 2))`*"
nodepatb="*`echo "$nodedesc"|cut -c3-100`"
nodepatc="*`echo "$nodedesc"|cut -c3-$(( $(echo -n "$nodedesc"|wc -c) - 2))`*"

nodeguid2=`opasaquery -d "$nodedesc2" -o nodeguid|head -1 2>/dev/null`
portguid2=`opasaquery -d "$nodedesc2" -o portguid|head -1 2>/dev/null`

#iocname=`opareport -q -o ious -F "node:$ioudesc" -d 2|grep 'ID String'|head -1|sed -e 's/^.*ID String: //' 2>/dev/null`
#iocguid=`opareport -q -o ious -F "node:$ioudesc" -d 2|grep IocSlot|head -1|sed -e 's/^.*0x/0x/' 2>/dev/null`
#iocpat="`echo "$iocname"|cut -c1-$(( $(echo -n "$iocname"|wc -c) - 2))`*"
#iocpatb="*`echo "$iocname"|cut -c3-100`"
#iocpatc="*`echo "$iocname"|cut -c3-$(( $(echo -n "$iocname"|wc -c) - 2))`*"

#ioctype='SRP'
subnet=0xfe80000000000000	# IBTA defined default subnet prefix
lid=1	# typically SM
topology=~/top.xml
badtopology=~/badtop.xml	# with some missing or inaccurate items

# missing fabric objects listed in badtopology
missfiportguid="0x0002b30101555555"
missfinodeguid="0x0002b30101555555"
missfinodedesc="MISSING_FI"
missfinodepat="*SSING_FI*"
missswnodeguid="0x0002b30102555555"
missswnodedesc="MISSING_SW"
missswnodepat="*SSING_SW"
misssmdetpat="*SSING*"
missrate="25g"
missmtu="2048"
missfinodedetpat="MISSING_N*"
misslabelpat="MISSING_LAB*"
misslengthpat="MISSLE*"
misscabledetpat="MISSING_DET*"
misslinkdetpat="MISSING_LDET*"
missportdetpat="MISSING_P*"

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
#echo
#echo "  ioudesc='$ioudesc'"
#echo "  iocname='$iocname'"
#echo "  iocpat='$iocpat'"
#echo "  iocpatb='$iocpatb'"
#echo "  iocpatc='$iocpatc'"
#echo "  iocguid='$iocguid'"
#echo "  ioctype='$ioctype'"

# run the command as given
# then if -x option was supplied use opaxmlindent to syntax check the XML
# it will be silent on success or report efforts if bad syntax
test_func()
{
$*
[[ "X$*" == X*\ -x\ * ]] && $*|opaxmlindent >/dev/null
}

set -x

for opts in "" "-T $topology" "-x" "-T $topology -x"
do
echo "Test basic reports"
test_func $OPA_REPORT $opts -o comps -d 10
test_func $OPA_REPORT $opts -o brcomps -d 10
test_func $OPA_REPORT $opts -o nodes -d 10
test_func $OPA_REPORT $opts -o brnodes -d 10
test_func $OPA_REPORT $opts -o ious -d 10
test_func $OPA_REPORT $opts -o lids -d 10
test_func $OPA_REPORT $opts -o links -d 10
test_func $OPA_REPORT $opts -o extlinks -d 10
test_func $OPA_REPORT $opts -o slowlinks -d 10
test_func $OPA_REPORT $opts -o slowconfiglinks -d 10
test_func $OPA_REPORT $opts -o slowconnlinks -d 10
test_func $OPA_REPORT $opts -o misconfiglinks -d 10
test_func $OPA_REPORT $opts -o misconnlinks -d 10
test_func $OPA_REPORT $opts -o errors -d 10
test_func $OPA_REPORT $opts -o otherports -d 10
test_func $OPA_REPORT $opts -o linear -d 10
test_func $OPA_REPORT $opts -o mcast -d 10
test_func $OPA_REPORT $opts -o portusage -d 10
test_func $OPA_REPORT $opts -o pathusage -d 10
test_func $OPA_REPORT $opts -o treepathusage -d 10
test_func $OPA_REPORT $opts -o portgroups -d 10
test_func $OPA_REPORT $opts -o quarantinednodes -d 10
test_func $OPA_REPORT $opts -o validateroutes -d 10
test_func $OPA_REPORT $opts -o validatepgs -d 10
test_func $OPA_REPORT $opts -o validatecreditloops -d 10
test_func $OPA_REPORT $opts -o vfinfo -d 10
test_func $OPA_REPORT $opts -o vfmember -d 10
test_func $OPA_REPORT $opts -o all -d 10
test_func $OPA_REPORT $opts -o route -S "node:$nodedesc:port:1" -D "node:$nodedesc2:port:1"
test_func $OPA_REPORT $opts -o route -S "node:$nodedesc" -D "node:$nodedesc2"
test_func $OPA_REPORT $opts -o none

# formats for "point"
echo "Test various Focus formats"
test_func $OPA_REPORT $opts -o nodes -F gid:$subnet:$portguid
test_func $OPA_REPORT $opts -o nodes -F lid:$lid
test_func $OPA_REPORT $opts -o nodes -F lid:$lid:node
test_func $OPA_REPORT $opts -o nodes -F lid:$lid:port:1
test_func $OPA_REPORT $opts -o nodes -F portguid:$portguid
test_func $OPA_REPORT $opts -o nodes -F nodeguid:$nodeguid
test_func $OPA_REPORT $opts -o nodes -F nodeguid:$nodeguid:port:1
#test_func $OPA_REPORT $opts -o nodes -F iocguid:$iocguid
#test_func $OPA_REPORT $opts -o nodes -F iocguid:$iocguid:port:1
test_func $OPA_REPORT $opts -o nodes -F systemguid:$sysguid
test_func $OPA_REPORT $opts -o nodes -F systemguid:$sysguid:port:1
#test_func $OPA_REPORT $opts -o nodes -F "ioc:$iocname"
#test_func $OPA_REPORT $opts -o nodes -F "ioc:$iocname:port:1"
#test_func $OPA_REPORT $opts -o nodes -F "iocpat:$iocpat"
#test_func $OPA_REPORT $opts -o nodes -F "iocpat:$iocpatb"
#test_func $OPA_REPORT $opts -o nodes -F "iocpat:$iocpatc"
#test_func $OPA_REPORT $opts -o nodes -F "iocpat:$iocpat:port:1"
#test_func $OPA_REPORT $opts -o nodes -F "ioctype:$ioctype"
#test_func $OPA_REPORT $opts -o nodes -F "ioctype:$ioctype:port:1"
test_func $OPA_REPORT $opts -o nodes -F "node:$nodedesc"
test_func $OPA_REPORT $opts -o nodes -F "node:$nodedesc:port:1"
test_func $OPA_REPORT $opts -o nodes -F "nodepat:$nodepat"
test_func $OPA_REPORT $opts -o nodes -F "nodepat:$nodepatb"
test_func $OPA_REPORT $opts -o nodes -F "nodepat:$nodepatc"
test_func $OPA_REPORT $opts -o nodes -F "nodepat:$nodepat:port:1"
#test_func $OPA_REPORT $opts -o nodes -F "nodedetpat:$nodedetpat"
#test_func $OPA_REPORT $opts -o nodes -F "nodedetpat:$nodedetpat:port:1"
test_func $OPA_REPORT $opts -o nodes -F "nodetype:FI"
test_func $OPA_REPORT $opts -o nodes -F "nodetype:FI:port:1"
test_func $OPA_REPORT $opts -o nodes -F "rate:100g"
test_func $OPA_REPORT $opts -o nodes -F "mtucap:8192"
test_func $OPA_REPORT $opts -o nodes -F "portstate:active"
test_func $OPA_REPORT $opts -o nodes -F "portphysstate:linkup"
#test_func $OPA_REPORT $opts -o nodes -F "labelpat:$labelpat"
#test_func $OPA_REPORT $opts -o nodes -F "lengthpat:$lengthpat"
#test_func $OPA_REPORT $opts -o nodes -F "cabledetpat:$cabledetpat"
#test_func $OPA_REPORT $opts -o nodes -F "cabinflenpat:$cabinflenpat"
#test_func $OPA_REPORT $opts -o nodes -F "cabinfvendnamepat:$cabinfvendnamepat"
#test_func $OPA_REPORT $opts -o nodes -F "cabinfvendpnpat:$cabinfvendpnpat"
#test_func $OPA_REPORT $opts -o nodes -F "cabinfvendrevpat:$cabinfvendrevpat"
#test_func $OPA_REPORT $opts -o nodes -F "cabinftype:$cabinftype"
#test_func $OPA_REPORT $opts -o nodes -F "cabinfvendsnpat:$cabinfvendsnpat"
#test_func $OPA_REPORT $opts -o nodes -F "linkdetpat:$linkdetpat"
#test_func $OPA_REPORT $opts -o nodes -F "portdetpat:$portdetpat"
test_func $OPA_REPORT $opts -o nodes -F sm
#test_func $OPA_REPORT $opts -o nodes -F "smdetpat:$smdetpat"
test_func $OPA_REPORT $opts -o nodes -F "route:node:$nodedesc:port:1:node:$nodedesc2:port:1"
test_func $OPA_REPORT $opts -o nodes -F "route:node:$nodedesc:node:$nodedesc2"
test_func $OPA_REPORT $opts -o nodes -F "linkqual:5"
test_func $OPA_REPORT $opts -o nodes -F "linkqualLE:5"
test_func $OPA_REPORT $opts -o nodes -F "linkqualGE:4"

# TBD other tests of options and flags
done

for opts in "-T $topology" "-T $topology -x" "-T $badtopology" "-T $badtopology -x"
do
echo "Test verify reports"
test_func $OPA_REPORT $opts -o verifyfis -d 10
test_func $OPA_REPORT $opts -o verifysws -d 10
test_func $OPA_REPORT $opts -o verifynodes -d 10
test_func $OPA_REPORT $opts -o verifysms -d 10
test_func $OPA_REPORT $opts -o verifylinks -d 10
test_func $OPA_REPORT $opts -o verifyextlinks -d 10
test_func $OPA_REPORT $opts -o verifyfilinks -d 10
test_func $OPA_REPORT $opts -o verifyislinks -d 10
test_func $OPA_REPORT $opts -o verifyextislinks -d 10
test_func $OPA_REPORT $opts -o verifyall -d 10

# formats for "point"
echo "Test verify with various Focus formats"
test_func $OPA_REPORT $opts -o verifyall -F gid:$subnet:$portguid
test_func $OPA_REPORT $opts -o verifyall -F lid:$lid
test_func $OPA_REPORT $opts -o verifyall -F lid:$lid:node
test_func $OPA_REPORT $opts -o verifyall -F lid:$lid:port:1
test_func $OPA_REPORT $opts -o verifyall -F portguid:$portguid
test_func $OPA_REPORT $opts -o verifyall -F nodeguid:$nodeguid
test_func $OPA_REPORT $opts -o verifyall -F nodeguid:$nodeguid:port:1
#test_func $OPA_REPORT $opts -o verifyall -F iocguid:$iocguid
#test_func $OPA_REPORT $opts -o verifyall -F iocguid:$iocguid:port:1
test_func $OPA_REPORT $opts -o verifyall -F systemguid:$sysguid
test_func $OPA_REPORT $opts -o verifyall -F systemguid:$sysguid:port:1
#test_func $OPA_REPORT $opts -o verifyall -F "ioc:$iocname"
#test_func $OPA_REPORT $opts -o verifyall -F "ioc:$iocname:port:1"
#test_func $OPA_REPORT $opts -o verifyall -F "iocpat:$iocpat"
#test_func $OPA_REPORT $opts -o verifyall -F "iocpat:$iocpatb"
#test_func $OPA_REPORT $opts -o verifyall -F "iocpat:$iocpatc"
#test_func $OPA_REPORT $opts -o verifyall -F "iocpat:$iocpat:port:1"
#test_func $OPA_REPORT $opts -o verifyall -F "ioctype:$ioctype"
#test_func $OPA_REPORT $opts -o verifyall -F "ioctype:$ioctype:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "node:$nodedesc"
test_func $OPA_REPORT $opts -o verifyall -F "node:$nodedesc:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "nodepat:$nodepat"
test_func $OPA_REPORT $opts -o verifyall -F "nodepat:$nodepatb"
test_func $OPA_REPORT $opts -o verifyall -F "nodepat:$nodepatc"
test_func $OPA_REPORT $opts -o verifyall -F "nodepat:$nodepat:port:1"
#test_func $OPA_REPORT $opts -o verifyall -F "nodedetpat:$nodedetpat"
#test_func $OPA_REPORT $opts -o verifyall -F "nodedetpat:$nodedetpat:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "nodetype:FI"
test_func $OPA_REPORT $opts -o verifyall -F "nodetype:FI:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "rate:100g"
test_func $OPA_REPORT $opts -o verifyall -F "mtucap:8192"
test_func $OPA_REPORT $opts -o verifyall -F "portstate:active"
test_func $OPA_REPORT $opts -o verifyall -F "portphysstate:linkup"
#test_func $OPA_REPORT $opts -o verifyall -F "labelpat:$labelpat"
#test_func $OPA_REPORT $opts -o verifyall -F "lengthpat:$lengthpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabledetpat:$cabledetpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinflenpat:$cabinflenpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinfvendnamepat:$cabinfvendnamepat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinfvendpnpat:$cabinfvendpnpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinfvendrevpat:$cabinfvendrevpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinftype:$cabinftype"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinfvendsnpat:$cabinfvendsnpat"
#test_func $OPA_REPORT $opts -o verifyall -F "linkdetpat:$linkdetpat"
#test_func $OPA_REPORT $opts -o verifyall -F "portdetpat:$portdetpat"
test_func $OPA_REPORT $opts -o verifyall -F sm
#test_func $OPA_REPORT $opts -o verifyall -F "smdetpat:$smdetpat"
test_func $OPA_REPORT $opts -o verifyall -F "route:node:$nodedesc:port:1:node:$nodedesc2:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "route:node:$nodedesc:node:$nodedesc2"
test_func $OPA_REPORT $opts -o verifyall -F "linkqual:5"
test_func $OPA_REPORT $opts -o verifyall -F "linkqualLE:5"
test_func $OPA_REPORT $opts -o verifyall -F "linkqualGE:4"

echo "Test verify with various Focus formats on missing objects"
test_func $OPA_REPORT $opts -o verifyall -F gid:$subnet:$missfiportguid
#test_func $OPA_REPORT $opts -o verifyall -F gid:$subnet:$missswportguid
#test_func $OPA_REPORT $opts -o verifyall -F lid:$lid
#test_func $OPA_REPORT $opts -o verifyall -F lid:$lid:node
#test_func $OPA_REPORT $opts -o verifyall -F lid:$lid:port:1
test_func $OPA_REPORT $opts -o verifyall -F portguid:$missfiportguid
#test_func $OPA_REPORT $opts -o verifyall -F portguid:$missswportguid
test_func $OPA_REPORT $opts -o verifyall -F nodeguid:$missfinodeguid
test_func $OPA_REPORT $opts -o verifyall -F nodeguid:$missswnodeguid
test_func $OPA_REPORT $opts -o verifyall -F nodeguid:$missfinodeguid:port:1
test_func $OPA_REPORT $opts -o verifyall -F nodeguid:$missswnodeguid:port:1
#test_func $OPA_REPORT $opts -o verifyall -F iocguid:$iocguid
#test_func $OPA_REPORT $opts -o verifyall -F iocguid:$iocguid:port:1
#test_func $OPA_REPORT $opts -o verifyall -F systemguid:$sysguid
#test_func $OPA_REPORT $opts -o verifyall -F systemguid:$sysguid:port:1
#test_func $OPA_REPORT $opts -o verifyall -F "ioc:$iocname"
#test_func $OPA_REPORT $opts -o verifyall -F "ioc:$iocname:port:1"
#test_func $OPA_REPORT $opts -o verifyall -F "iocpat:$iocpat"
#test_func $OPA_REPORT $opts -o verifyall -F "iocpat:$iocpatb"
#test_func $OPA_REPORT $opts -o verifyall -F "iocpat:$iocpatc"
#test_func $OPA_REPORT $opts -o verifyall -F "iocpat:$iocpat:port:1"
#test_func $OPA_REPORT $opts -o verifyall -F "ioctype:$ioctype"
#test_func $OPA_REPORT $opts -o verifyall -F "ioctype:$ioctype:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "node:$missfinodedesc"
test_func $OPA_REPORT $opts -o verifyall -F "node:$missswnodedesc"
test_func $OPA_REPORT $opts -o verifyall -F "node:$missfinodedesc:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "node:$missswnodedesc:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "nodepat:$missfinodepat"
test_func $OPA_REPORT $opts -o verifyall -F "nodepat:$missswnodepat"
test_func $OPA_REPORT $opts -o verifyall -F "nodepat:$missfinodepat:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "nodepat:$missswnodepat:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "nodedetpat:$missfinodedetpat"
#test_func $OPA_REPORT $opts -o verifyall -F "nodedetpat:$missswnodedetpat"
#test_func $OPA_REPORT $opts -o verifyall -F "nodedetpat:$missfinodedetpat:port:1"
#test_func $OPA_REPORT $opts -o verifyall -F "nodedetpat:$missswnodedetpat:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "nodetype:FI"
test_func $OPA_REPORT $opts -o verifyall -F "nodetype:FI:port:1"
test_func $OPA_REPORT $opts -o verifyall -F "rate:$missrate"
test_func $OPA_REPORT $opts -o verifyall -F "mtucap:$missmtu"
#test_func $OPA_REPORT $opts -o verifyall -F "portstate:active"
#test_func $OPA_REPORT $opts -o verifyall -F "portphysstate:linkup"
test_func $OPA_REPORT $opts -o verifyall -F "labelpat:$misslabelpat"
test_func $OPA_REPORT $opts -o verifyall -F "lengthpat:$misslengthpat"
test_func $OPA_REPORT $opts -o verifyall -F "cabledetpat:$misscabledetpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinflenpat:$cabinflenpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinfvendnamepat:$cabinfvendnamepat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinfvendpnpat:$cabinfvendpnpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinfvendrevpat:$cabinfvendrevpat"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinftype:$cabinftype"
#test_func $OPA_REPORT $opts -o verifyall -F "cabinfvendsnpat:$cabinfvendsnpat"
test_func $OPA_REPORT $opts -o verifyall -F "linkdetpat:$misslinkdetpat"
test_func $OPA_REPORT $opts -o verifyall -F "portdetpat:$missportdetpat"
#test_func $OPA_REPORT $opts -o verifyall -F sm
test_func $OPA_REPORT $opts -o verifyall -F "smdetpat:$misssmdetpat"
#test_func $OPA_REPORT $opts -o verifyall -F "route:node:$nodedesc:port:1:node:$nodedesc2:port:1"
#test_func $OPA_REPORT $opts -o verifyall -F "route:node:$nodedesc:node:$nodedesc2"
#test_func $OPA_REPORT $opts -o verifyall -F "linkqual:5"
#test_func $OPA_REPORT $opts -o verifyall -F "linkqualLE:5"
#test_func $OPA_REPORT $opts -o verifyall -F "linkqualGE:4"

echo "Test verify with various Focus formats on not found objects"
# these will generate errors, so dont use test_func to verify XML
$OPA_REPORT $opts -o verifyall -F gid:$subnet:0xffffffffffffffff
#$OPA_REPORT $opts -o verifyall -F lid:$lid
#$OPA_REPORT $opts -o verifyall -F lid:$lid:node
#$OPA_REPORT $opts -o verifyall -F lid:$lid:port:1
$OPA_REPORT $opts -o verifyall -F portguid:0xffffffffffffffff
$OPA_REPORT $opts -o verifyall -F nodeguid:0xffffffffffffffff
$OPA_REPORT $opts -o verifyall -F nodeguid:0xffffffffffffffff:port:1
#$OPA_REPORT $opts -o verifyall -F iocguid:$iocguid
#$OPA_REPORT $opts -o verifyall -F iocguid:$iocguid:port:1
#$OPA_REPORT $opts -o verifyall -F systemguid:$sysguid
#$OPA_REPORT $opts -o verifyall -F systemguid:$sysguid:port:1
#$OPA_REPORT $opts -o verifyall -F "ioc:$iocname"
#$OPA_REPORT $opts -o verifyall -F "ioc:$iocname:port:1"
#$OPA_REPORT $opts -o verifyall -F "iocpat:$iocpat"
#$OPA_REPORT $opts -o verifyall -F "iocpat:$iocpatb"
#$OPA_REPORT $opts -o verifyall -F "iocpat:$iocpatc"
#$OPA_REPORT $opts -o verifyall -F "iocpat:$iocpat:port:1"
#$OPA_REPORT $opts -o verifyall -F "ioctype:$ioctype"
#$OPA_REPORT $opts -o verifyall -F "ioctype:$ioctype:port:1"
$OPA_REPORT $opts -o verifyall -F "node:NO_SUCH_NODE"
$OPA_REPORT $opts -o verifyall -F "node:NO_SUCH_NODE:port:1"
$OPA_REPORT $opts -o verifyall -F "nodepat:NO_SUCH_*"
$OPA_REPORT $opts -o verifyall -F "nodepat:NO_SUCH_*:port:1"
$OPA_REPORT $opts -o verifyall -F "nodedetpat:NO_SUCH_*"
$OPA_REPORT $opts -o verifyall -F "nodedetpat:NO_SUCH_*:port:1"
#$OPA_REPORT $opts -o verifyall -F "nodetype:FI"
#$OPA_REPORT $opts -o verifyall -F "nodetype:FI:port:1"
$OPA_REPORT $opts -o verifyall -F "rate:50g"
$OPA_REPORT $opts -o verifyall -F "mtucap:4096"
#$OPA_REPORT $opts -o verifyall -F "portstate:active"
#$OPA_REPORT $opts -o verifyall -F "portphysstate:linkup"
#$OPA_REPORT $opts -o verifyall -F "labelpat:$labelpat"
#$OPA_REPORT $opts -o verifyall -F "lengthpat:$lengthpat"
#$OPA_REPORT $opts -o verifyall -F "cabledetpat:$cabledetpat"
#$OPA_REPORT $opts -o verifyall -F "cabinflenpat:$cabinflenpat"
#$OPA_REPORT $opts -o verifyall -F "cabinfvendnamepat:$cabinfvendnamepat"
#$OPA_REPORT $opts -o verifyall -F "cabinfvendpnpat:$cabinfvendpnpat"
#$OPA_REPORT $opts -o verifyall -F "cabinfvendrevpat:$cabinfvendrevpat"
#$OPA_REPORT $opts -o verifyall -F "cabinftype:$cabinftype"
#$OPA_REPORT $opts -o verifyall -F "cabinfvendsnpat:$cabinfvendsnpat"
#$OPA_REPORT $opts -o verifyall -F "linkdetpat:$linkdetpat"
#$OPA_REPORT $opts -o verifyall -F "portdetpat:$portdetpat"
#$OPA_REPORT $opts -o verifyall -F sm
$OPA_REPORT $opts -o verifyall -F "smdetpat:NO_SUCH_*"
#$OPA_REPORT $opts -o verifyall -F "route:node:$nodedesc:port:1:node:$nodedesc2:port:1"
#$OPA_REPORT $opts -o verifyall -F "route:node:$nodedesc:node:$nodedesc2"
#$OPA_REPORT $opts -o verifyall -F "linkqual:5"
#$OPA_REPORT $opts -o verifyall -F "linkqualLE:5"
#$OPA_REPORT $opts -o verifyall -F "linkqualGE:4"
done
