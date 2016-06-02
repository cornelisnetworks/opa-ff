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
if [ "$1" = "" ]
then
	serial=000380
else
	serial="$1"
fi
if [ "$2" = "" ]
then
	serial2=000101
else
	serial2="$2"
fi
if [ "$3" = "" ]
then
	nodedesc=duster
else
	nodedesc="$3"
fi
nodeguid=0x0011750198$serial2
sysguid=0x0011750198$serial2
portguid=0x00117501a0$serial2
portguid2=0x00117501a0$serial2
subnet=0xfe80000000000000	# IBTA defined default subnet prefix

set -x

# sample of all valid queries
# note some combinations of inputs and outputs are not supported

# query all records of a given type
opasaquery -v
opasaquery -v -o systemguid
opasaquery -v -o nodeguid
opasaquery -v -o portguid
opasaquery -v -o lid
opasaquery -v -o desc
opasaquery -v -o path
opasaquery -v -o node
opasaquery -v -o portinfo
opasaquery -v -o sminfo
opasaquery -v -o swinfo
opasaquery -v -o link
opasaquery -v -o service
opasaquery -v -o mcmember
opasaquery -v -o inform
opasaquery -v -o scsc
opasaquery -v -o scsl
opasaquery -v -o scvlnt
opasaquery -v -o linfdb
opasaquery -v -o mcfdb
opasaquery -v -o vlarb
opasaquery -v -o pkey
opasaquery -v -o vfinfo
opasaquery -v -o vfinfocsv
opasaquery -v -o vfinfocsv2
opasaquery -v -o classportinfo
opasaquery -v -o fabricinfo
opasaquery -v -o quarantine
opasaquery -v -o conginfo
opasaquery -v -o swcongset
opasaquery -v -o swportcong
opasaquery -v -o hficongset
opasaquery -v -o hficongcon
opasaquery -v -o bfrctrl
opasaquery -v -o cableinfo
opasaquery -v -o portgroup
opasaquery -v -o portgroupfdb


# query by node type
opasaquery -v -t fi
opasaquery -v -t sw
opasaquery -v -o systemguid -t fi
opasaquery -v -o nodeguid -t fi
opasaquery -v -o portguid -t fi
opasaquery -v -o lid -t fi
opasaquery -v -o desc -t fi
opasaquery -v -o node -t fi

# query by lid
opasaquery -v -l 1
opasaquery -v -l 1 -o systemguid
opasaquery -v -l 1 -o nodeguid
opasaquery -v -l 1 -o portguid
opasaquery -v -l 1 -o lid
opasaquery -v -l 1 -o desc
opasaquery -v -l 1 -o path
opasaquery -v -l 1 -o node
opasaquery -v -l 1 -o portinfo
opasaquery -v -l 0xc000 -o mcmember
opasaquery -v -l 1 -o inform 
opasaquery -v -l 1 -o swinfo
opasaquery -v -l 1 -o linfdb
opasaquery -v -l 1 -o mcfdb
opasaquery -v -l 1 -o vlarb
opasaquery -v -l 1 -o pkey
opasaquery -v -l 1 -o scsc
opasaquery -v -l 1 -o slsc
opasaquery -v -l 1 -o scvlt
opasaquery -v -l 1 -o scvlnt
opasaquery -v -l 1 -o classportinfo
opasaquery -v -l 1 -o fabricinfo
opasaquery -v -l 1 -o quarantine
opasaquery -v -l 1 -o conginfo
opasaquery -v -l 1 -o swcongset
opasaquery -v -l 1 -o swportcong
opasaquery -v -l 1 -o hficongset
opasaquery -v -l 1 -o hficongcon
opasaquery -v -l 1 -o bfrctrl
opasaquery -v -l 1 -o cableinfo
opasaquery -v -l 1 -o portgroup
opasaquery -v -l 1 -o portgroupfdb


# query by system image guid
opasaquery -v  -s $sysguid
opasaquery -v -o systemguid  -s $sysguid
opasaquery -v -o nodeguid  -s $sysguid
opasaquery -v -o portguid  -s $sysguid
opasaquery -v -o lid  -s $sysguid
opasaquery -v -o desc  -s $sysguid
opasaquery -v -o node  -s $sysguid

# query by system image guid of 0 (eg. nodes not supporting this optional field)
opasaquery -v  -s 0
opasaquery -v -o systemguid  -s 0
opasaquery -v -o nodeguid  -s 0
opasaquery -v -o portguid  -s 0
opasaquery -v -o lid  -s 0
opasaquery -v -o desc  -s 0
opasaquery -v -o node  -s 0

# query by node guid
opasaquery -v  -n $nodeguid
opasaquery -v -o systemguid  -n $nodeguid
opasaquery -v -o nodeguid  -n $nodeguid
opasaquery -v -o portguid  -n $nodeguid
opasaquery -v -o lid  -n $nodeguid
opasaquery -v -o desc  -n $nodeguid

# query by port guid
opasaquery -v  -g $portguid
opasaquery -v -o systemguid  -g $portguid
opasaquery -v -o nodeguid  -g $portguid
opasaquery -v -o portguid  -g $portguid
opasaquery -v -o lid  -g $portguid
opasaquery -v -o desc  -g $portguid
opasaquery -v -o path  -g $portguid
opasaquery -v -o node  -g $portguid
opasaquery -v -o service  -g $portguid
opasaquery -v -o mcmember  -g $portguid # SM bug
#opasaquery -v -I -o inform  -g $portguid  #IB legacy
opasaquery -v -o trace  -g $portguid

# query by port gid
opasaquery -v -o path  -u $subnet:$portguid
opasaquery -v -o service  -u $subnet:$portguid # sm does not support
opasaquery -v -o mcmember  -u $subnet:$portguid
#opasaquery -v -I -o inform  -u $subnet:$portguid #IB legacy
opasaquery -v -o trace  -u $subnet:$portguid

# query by mc gid
opasaquery -v -o mcmember  -m 0xff02000000000000:0x0000000000000001
opasaquery -v -o vfinfo  -m 0xff02000000000000:0x0000000000000001
opasaquery -v -o vfinfocsv  -m 0xff02000000000000:0x0000000000000001
opasaquery -v -o vfinfocsv2  -m 0xff02000000000000:0x0000000000000001

# query by node desc
opasaquery -v  -d $nodedesc
opasaquery -v -o systemguid  -d $nodedesc
opasaquery -v -o nodeguid  -d $nodedesc
opasaquery -v -o portguid  -d $nodedesc
opasaquery -v -o lid  -d $nodedesc
opasaquery -v -o desc  -d $nodedesc
opasaquery -v -o node  -d $nodedesc
opasaquery -v -o vfinfo  -d "Default"	# VF Name 
opasaquery -v -o vfinfocsv  -d "Default"	# VF Name 
opasaquery -v -o vfinfocsv2  -d "Default"	# VF Name 

# query by port guid pair
opasaquery -v -o path  -P "$portguid2 $portguid"
opasaquery -v -o trace  -P "$portguid2 $portguid"

# query by gid pair
opasaquery -v -o path  -G "$subnet:$portguid2 $subnet:$portguid"
opasaquery -v -o trace  -G "$subnet:$portguid2 $subnet:$portguid"

# query by port guid list
opasaquery -v -o path  -a "$portguid2;$portguid"

# query by gid list
opasaquery -v -o path  -A "$subnet:$portguid2;$subnet:$portguid"

# query by SL
opasaquery -v -L 0 -o path 
opasaquery -v -L 0 -o vfinfo
opasaquery -v -L 0 -o vfinfocsv
opasaquery -v -L 0 -o vfinfocsv2

# query by serviceId
opasaquery -v -S 1 -o path 
opasaquery -v -S 1 -o vfinfo
opasaquery -v -S 1 -o vfinfocsv
opasaquery -v -S 1 -o vfinfocsv2

# query by VfIndex
opasaquery -v -i 0 -o vfinfo
opasaquery -v -i 0 -o vfinfocsv
opasaquery -v -i 0 -o vfinfocsv2

# query by pkey
opasaquery -v -k 0x7fff -o path
opasaquery -v -k 0x7fff -o mcmember # unsupported
opasaquery -v -k 0x7fff -o vfinfo
opasaquery -v -k 0x7fff -o vfinfocsv
opasaquery -v -k 0x7fff -o vfinfocsv2

#new queries added in STL-2
#opasaquery -v -o bwarb
#opasaquery -v -o dglist
#opasaquery -v -o dgmember
#opasaquery -v -o dgmember -D All
