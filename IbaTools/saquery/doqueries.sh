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
nodeguid=0x0011750198$serial
sysguid=0x0011750198$serial
portguid=0x00117501a0$serial
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
#opasaquery -v -o gid # unsupported
opasaquery -v -o desc
opasaquery -v -o path
opasaquery -v -o node
opasaquery -v -o portinfo
opasaquery -v -o sminfo
opasaquery -v -o link
opasaquery -v -o service
opasaquery -v -o mcmember
opasaquery -v -o inform
#opasaquery -v -o trace # unsupported
opasaquery -v -o slvl
opasaquery -v -o swinfo
opasaquery -v -o linfdb
opasaquery -v -o ranfdb
opasaquery -v -o mcfdb
opasaquery -v -o vlarb
opasaquery -v -o pkey
opasaquery -v -o guids
opasaquery -v -o vfinfo
opasaquery -v -o vfinfocsv

# query by node type
opasaquery -v -t fi
opasaquery -v -t sw
opasaquery -v -o systemguid -t fi
opasaquery -v -o nodeguid -t fi
opasaquery -v -o portguid -t fi
opasaquery -v -o lid -t fi
#opasaquery -v -o gid -t fi # unsupported
opasaquery -v -o desc -t fi
opasaquery -v -o path -t fi
opasaquery -v -o node -t fi
#opasaquery -v -o portinfo -t fi # unsupported
#opasaquery -v -o sminfo -t fi # unsupported
#opasaquery -v -o link -t fi # unsupported
#opasaquery -v -o service -t fi # unsupported
#opasaquery -v -o mcmember -t fi # unsupported
#opasaquery -v -o inform -t fi # unsupported
#opasaquery -v -o trace -t fi # unsupported
#opasaquery -v -o slvl -t fi # unsupported
#opasaquery -v -o swinfo -t sw # unsupported
#opasaquery -v -o linfdb -t sw # unsupported
#opasaquery -v -o ranfdb -t sw # unsupported
#opasaquery -v -o mcfdb -t sw # unsupported
#opasaquery -v -o vlarb -t sw # unsupported
#opasaquery -v -o pkey -t sw # unsupported
#opasaquery -v -o guids -t sw # unsupported
#opasaquery -v -o vfinfo -t sw # unsupported
#opasaquery -v -o vfinfocsv -t sw # unsupported

# query by lid
opasaquery -v -l 1
opasaquery -v -l 1 -o systemguid
opasaquery -v -l 1 -o nodeguid
opasaquery -v -l 1 -o portguid
opasaquery -v -l 1 -o lid
#opasaquery -v -l 1 -o gid # unsupported
opasaquery -v -l 1 -o desc
opasaquery -v -l 1 -o path
opasaquery -v -l 1 -o node
opasaquery -v -l 1 -o portinfo
#opasaquery -v -l 1 -o sminfo # unsupported
#opasaquery -v -l 1 -o link # unsupported
opasaquery -v -l 0xc000 -o mcmember
#opasaquery -v -l 1 -o inform # unsupported
#opasaquery -v -l 1 -o trace # unsupported
opasaquery -v -l 1 -o slvl
opasaquery -v -l 1 -o swinfo
opasaquery -v -l 1 -o linfdb
opasaquery -v -l 1 -o ranfdb
opasaquery -v -l 1 -o mcfdb
opasaquery -v -l 1 -o vlarb
opasaquery -v -l 1 -o pkey
opasaquery -v -l 1 -o guids
#opasaquery -v -l 1 -o vfinfo # unsupported
#opasaquery -v -l 1 -o vfinfocsv # unsupported

# query by system image guid
opasaquery -v  -s $sysguid
opasaquery -v -o systemguid  -s $sysguid
opasaquery -v -o nodeguid  -s $sysguid
opasaquery -v -o portguid  -s $sysguid
opasaquery -v -o lid  -s $sysguid
#opasaquery -v -o gid  -s $sysguid # unsupported
opasaquery -v -o desc  -s $sysguid
opasaquery -v -o path  -s $sysguid
opasaquery -v -o node  -s $sysguid
#opasaquery -v -o portinfo  -s $sysguid # unsupported
#opasaquery -v -o sminfo  -s $sysguid # unsupported
#opasaquery -v -o link  -s $sysguid # unsupported
#opasaquery -v -o service  -s $sysguid # unsupported
#opasaquery -v -o mcmember  -s $sysguid # unsupported
#opasaquery -v -o inform  -s $sysguid # unsupported
#opasaquery -v -o trace  -s $sysguid # unsupported
#opasaquery -v -o slvl  -s $sysguid # unsupported
#opasaquery -v -o swinfo  -s $sysguid # unsupported
#opasaquery -v -o linfdb  -s $sysguid # unsupported
#opasaquery -v -o ranfdb  -s $sysguid # unsupported
#opasaquery -v -o mcfdb  -s $sysguid # unsupported
#opasaquery -v -o vlarb  -s $sysguid # unsupported
#opasaquery -v -o pkey  -s $sysguid # unsupported
#opasaquery -v -o guids  -s $sysguid # unsupported
#opasaquery -v -o vfinfo  -s $sysguid # unsupported
#opasaquery -v -o vfinfocsv  -s $sysguid # unsupported

# query by system image guid of 0 (eg. nodes not supporting this optional field)
opasaquery -v  -s 0
opasaquery -v -o systemguid  -s 0
opasaquery -v -o nodeguid  -s 0
opasaquery -v -o portguid  -s 0
opasaquery -v -o lid  -s 0
#opasaquery -v -o gid  -s 0 # unsupported
opasaquery -v -o desc  -s 0
opasaquery -v -o path  -s 0
opasaquery -v -o node  -s 0
#opasaquery -v -o portinfo  -s 0 # unsupported
#opasaquery -v -o sminfo  -s 0 # unsupported
#opasaquery -v -o link  -s 0 # unsupported
#opasaquery -v -o service  -s 0 # unsupported
#opasaquery -v -o mcmember  -s 0 # unsupported
#opasaquery -v -o inform  -s 0 # unsupported
#opasaquery -v -o trace  -s 0 # unsupported
#opasaquery -v -o slvl  -s 0 # unsupported
#opasaquery -v -o swinfo  -s 0 # unsupported
#opasaquery -v -o linfdb  -s 0 # unsupported
#opasaquery -v -o ranfdb  -s 0 # unsupported
#opasaquery -v -o mcfdb  -s 0 # unsupported
#opasaquery -v -o vlarb  -s 0 # unsupported
#opasaquery -v -o pkey  -s 0 # unsupported
#opasaquery -v -o guids  -s 0 # unsupported
#opasaquery -v -o vfinfo  -s 0 # unsupported
#opasaquery -v -o vfinfocsv  -s 0 # unsupported

# query by node guid
opasaquery -v  -n $nodeguid
opasaquery -v -o systemguid  -n $nodeguid
opasaquery -v -o nodeguid  -n $nodeguid
opasaquery -v -o portguid  -n $nodeguid
opasaquery -v -o lid  -n $nodeguid
#opasaquery -v -o gid  -n $nodeguid # unsupported
opasaquery -v -o desc  -n $nodeguid
opasaquery -v -o path  -n $nodeguid
opasaquery -v -o node  -n $nodeguid
#opasaquery -v -o portinfo  -n $nodeguid # unsupported
#opasaquery -v -o sminfo  -n $nodeguid # unsupported
#opasaquery -v -o link  -n $nodeguid # unsupported
#opasaquery -v -o service  -n $nodeguid # unsupported
#opasaquery -v -o mcmember  -n $nodeguid # unsupported
#opasaquery -v -o inform  -n $nodeguid # unsupported
#opasaquery -v -o trace  -n $nodeguid # unsupported
#opasaquery -v -o slvl  -n $nodeguid # unsupported
#opasaquery -v -o swinfo  -n $nodeguid # unsupported
#opasaquery -v -o linfdb  -n $nodeguid # unsupported
#opasaquery -v -o ranfdb  -n $nodeguid # unsupported
#opasaquery -v -o mcfdb  -n $nodeguid # unsupported
#opasaquery -v -o vlarb  -n $nodeguid # unsupported
#opasaquery -v -o pkey  -n $nodeguid # unsupported
#opasaquery -v -o guids  -n $nodeguid # unsupported
#opasaquery -v -o vfinfo  -n $nodeguid # unsupported
#opasaquery -v -o vfinfocsv  -n $nodeguid # unsupported

# query by port guid
opasaquery -v  -g $portguid
opasaquery -v -o systemguid  -g $portguid
opasaquery -v -o nodeguid  -g $portguid
opasaquery -v -o portguid  -g $portguid
opasaquery -v -o lid  -g $portguid
#opasaquery -v -o gid  -g $portguid # unsupported
opasaquery -v -o desc  -g $portguid
opasaquery -v -o path  -g $portguid
opasaquery -v -o node  -g $portguid
#opasaquery -v -o portinfo  -g $portguid # unsupported
#opasaquery -v -o sminfo  -g $portguid # unsupported
#opasaquery -v -o link  -g $portguid # unsupported
opasaquery -v -o service  -g $portguid
opasaquery -v -o mcmember  -g $portguid # SM bug
opasaquery -v -o inform  -g $portguid
opasaquery -v -o trace  -g $portguid
#opasaquery -v -o slvl  -g $portguid # unsupported
#opasaquery -v -o swinfo  -g $portguid # unsupported
#opasaquery -v -o linfdb  -g $portguid # unsupported
#opasaquery -v -o ranfdb  -g $portguid # unsupported
#opasaquery -v -o mcfdb  -g $portguid # unsupported
#opasaquery -v -o vlarb  -g $portguid # unsupported
#opasaquery -v -o pkey  -g $portguid # unsupported
#opasaquery -v -o guids  -g $portguid # unsupported
#opasaquery -v -o vfinfo  -g $portguid # unsupported
#opasaquery -v -o vfinfocsv  -g $portguid # unsupported

# query by port gid
#opasaquery -v  -u $subnet:$portguid # unsupported
#opasaquery -v -o systemguid  -u $subnet:$portguid # unsupported
#opasaquery -v -o nodeguid  -u $subnet:$portguid # unsupported
#opasaquery -v -o portguid  -u $subnet:$portguid # unsupported
#opasaquery -v -o lid  -u $subnet:$portguid # unsupported
#opasaquery -v -o gid  -u $subnet:$portguid # unsupported
#opasaquery -v -o desc  -u $subnet:$portguid # unsupported
opasaquery -v -o path  -u $subnet:$portguid
#opasaquery -v -o node  -u $subnet:$portguid # unsupported
#opasaquery -v -o portinfo  -u $subnet:$portguid # unsupported
#opasaquery -v -o sminfo  -u $subnet:$portguid # unsupported
#opasaquery -v -o link  -u $subnet:$portguid # unsupported
opasaquery -v -o service  -u $subnet:$portguid # sm does not support
opasaquery -v -o mcmember  -u $subnet:$portguid
opasaquery -v -o inform  -u $subnet:$portguid
opasaquery -v -o trace  -u $subnet:$portguid
#opasaquery -v -o slvl  -u $subnet:$portguid # unsupported
#opasaquery -v -o swinfo  -u $subnet:$portguid # unsupported
#opasaquery -v -o linfdb  -u $subnet:$portguid # unsupported
#opasaquery -v -o ranfdb  -u $subnet:$portguid # unsupported
#opasaquery -v -o mcfdb  -u $subnet:$portguid # unsupported
#opasaquery -v -o vlarb  -u $subnet:$portguid # unsupported
#opasaquery -v -o pkey  -u $subnet:$portguid # unsupported
#opasaquery -v -o guids  -u $subnet:$portguid # unsupported
#opasaquery -v -o vfinfo  -u $subnet:$portguid # unsupported
#opasaquery -v -o vfinfocsv  -u $subnet:$portguid # unsupported

# query by mc gid
#opasaquery -v  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o systemguid  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o nodeguid  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o portguid  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o lid  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o gid  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o desc  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o path  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o node  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o portinfo  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o sminfo  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o link  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o service  -m 0xff02000000000000:0x0000000000000001 # unsupported
opasaquery -v -o mcmember  -m 0xff02000000000000:0x0000000000000001
#opasaquery -v -o inform  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o trace  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o slvl  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o swinfo  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o linfdb  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o ranfdb  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o mcfdb  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o vlarb  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o pkey  -m 0xff02000000000000:0x0000000000000001 # unsupported
#opasaquery -v -o guids  -m 0xff02000000000000:0x0000000000000001 # unsupported
opasaquery -v -o vfinfo  -m 0xff02000000000000:0x0000000000000001
opasaquery -v -o vfinfocsv  -m 0xff02000000000000:0x0000000000000001

# query by node desc
opasaquery -v  -d $nodedesc
opasaquery -v -o systemguid  -d $nodedesc
opasaquery -v -o nodeguid  -d $nodedesc
opasaquery -v -o portguid  -d $nodedesc
opasaquery -v -o lid  -d $nodedesc
#opasaquery -v -o gid  -d $nodedesc
opasaquery -v -o desc  -d $nodedesc
opasaquery -v -o path  -d $nodedesc
opasaquery -v -o node  -d $nodedesc
#opasaquery -v -o portinfo  -d $nodedesc # unsupported
#opasaquery -v -o sminfo  -d $nodedesc # unsupported
#opasaquery -v -o link  -d $nodedesc # unsupported
#opasaquery -v -o service  -d $nodedesc # unsupported
#opasaquery -v -o mcmember  -d $nodedesc # unsupported
#opasaquery -v -o inform  -d $nodedesc # unsupported
#opasaquery -v -o trace  -d $nodedesc # unsupported
#opasaquery -v -o slvl  -d $nodedesc # unsupported
#opasaquery -v -o swinfo  -d $nodedesc # unsupported
#opasaquery -v -o linfdb  -d $nodedesc # unsupported
#opasaquery -v -o ranfdb  -d $nodedesc # unsupported
#opasaquery -v -o mcfdb  -d $nodedesc # unsupported
#opasaquery -v -o vlarb  -d $nodedesc # unsupported
#opasaquery -v -o pkey  -d $nodedesc # unsupported
#opasaquery -v -o guids  -d $nodedesc # unsupported
opasaquery -v -o vfinfo  -d "Default"	# VF Name
opasaquery -v -o vfinfocsv  -d "Default"	# VF Name

# query by port guid pair
#opasaquery -v  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o systemguid  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o nodeguid  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o portguid  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o lid  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o gid  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o desc  -P "$portguid2 $portguid" # unsupported
opasaquery -v -o path  -P "$portguid2 $portguid"
#opasaquery -v -o node  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o portinfo  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o sminfo  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o link  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o service  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o mcmember  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o inform  -P "$portguid2 $portguid" # unsupported
opasaquery -v -o trace  -P "$portguid2 $portguid"
#opasaquery -v -o slvl  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o swinfo  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o linfdb  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o ranfdb  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o mcfdb  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o vlarb  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o pkey  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o guids  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o vfinfo  -P "$portguid2 $portguid" # unsupported
#opasaquery -v -o vfinfocsv  -P "$portguid2 $portguid" # unsupported

# query by gid pair
#opasaquery -v  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o systemguid  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o nodeguid  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o portguid  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o lid  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o gid  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o desc  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
opasaquery -v -o path  -G "$subnet:$portguid2 $subnet:$portguid"
#opasaquery -v -o node  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o portinfo  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o sminfo  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o link  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o service  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o mcmember  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o inform  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
opasaquery -v -o trace  -G "$subnet:$portguid2 $subnet:$portguid"
#opasaquery -v -o slvl  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o swinfo  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o linfdb  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o ranfdb  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o mcfdb  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o vlarb  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o pkey  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o guids  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o vfinfo  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported
#opasaquery -v -o vfinfocsv  -G "$subnet:$portguid2 $subnet:$portguid" # unsupported

# query by port guid list
#opasaquery -v  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o systemguid  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o nodeguid  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o portguid  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o lid  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o gid  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o desc  -a "$portguid2;$portguid" # unsupported
opasaquery -v -o path  -a "$portguid2;$portguid"
#opasaquery -v -o node  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o portinfo  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o sminfo  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o link  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o service  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o mcmember  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o inform  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o trace  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o slvl  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o swinfo  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o linfdb  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o ranfdb  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o mcfdb  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o vlarb  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o pkey  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o guids  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o vfinfo  -a "$portguid2;$portguid" # unsupported
#opasaquery -v -o vfinfocsv  -a "$portguid2;$portguid" # unsupported

# query by gid list
#opasaquery -v  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o systemguid  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o nodeguid  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o portguid  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o lid  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o gid  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o desc  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
opasaquery -v -o path  -A "$subnet:$portguid2;$subnet:$portguid"
#opasaquery -v -o node  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o portinfo  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o sminfo  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o link  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o service  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o mcmember  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o inform  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o trace  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o slvl  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o swinfo  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o linfdb  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o ranfdb  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o mcfdb  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o vlarb  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o pkey  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o guids  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o vfinfo  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported
#opasaquery -v -o vfinfocsv  -A "$subnet:$portguid2;$subnet:$portguid" # unsupported

# query by SL
#opasaquery -v -L 0 # unsupported
#opasaquery -v -L 0 -o systemguid # unsupported
#opasaquery -v -L 0 -o nodeguid # unsupported
#opasaquery -v -L 0 -o portguid # unsupported
#opasaquery -v -L 0 -o lid # unsupported
#opasaquery -v -L 0 -o gid # unsupported
#opasaquery -v -L 0 -o desc # unsupported
#opasaquery -v -L 0 -o path # unsupported
#opasaquery -v -L 0 -o node # unsupported
#opasaquery -v -L 0 -o portinfo # unsupported
#opasaquery -v -L 0 -o sminfo # unsupported
#opasaquery -v -L 0 -o link # unsupported
#opasaquery -v -L 0 -o mcmember # unsupported
#opasaquery -v -L 0 -o inform # unsupported
#opasaquery -v -L 0 -o trace # unsupported
#opasaquery -v -L 0 -o slvl # unsupported
#opasaquery -v -L 0 -o swinfo # unsupported
#opasaquery -v -L 0 -o linfdb # unsupported
#opasaquery -v -L 0 -o ranfdb # unsupported
#opasaquery -v -L 0 -o mcfdb # unsupported
#opasaquery -v -L 0 -o vlarb # unsupported
#opasaquery -v -L 0 -o pkey # unsupported
#opasaquery -v -L 0 -o guids # unsupported
opasaquery -v -L 0 -o vfinfo
opasaquery -v -L 0 -o vfinfocsv

# query by serviceId
#opasaquery -v -S 1 # unsupported
#opasaquery -v -S 1 -o systemguid # unsupported
#opasaquery -v -S 1 -o nodeguid # unsupported
#opasaquery -v -S 1 -o portguid # unsupported
#opasaquery -v -S 1 -o lid # unsupported
#opasaquery -v -S 1 -o gid # unsupported
#opasaquery -v -S 1 -o desc # unsupported
#opasaquery -v -S 1 -o path # unsupported
#opasaquery -v -S 1 -o node # unsupported
#opasaquery -v -S 1 -o portinfo # unsupported
#opasaquery -v -S 1 -o sminfo # unsupported
#opasaquery -v -S 1 -o link # unsupported
#opasaquery -v -S 1 -o mcmember # unsupported
#opasaquery -v -S 1 -o inform # unsupported
#opasaquery -v -S 1 -o trace # unsupported
#opasaquery -v -S 1 -o slvl # unsupported
#opasaquery -v -S 1 -o swinfo # unsupported
#opasaquery -v -S 1 -o linfdb # unsupported
#opasaquery -v -S 1 -o ranfdb # unsupported
#opasaquery -v -S 1 -o mcfdb # unsupported
#opasaquery -v -S 1 -o vlarb # unsupported
#opasaquery -v -S 1 -o pkey # unsupported
#opasaquery -v -S 1 -o guids # unsupported
opasaquery -v -S 1 -o vfinfo
opasaquery -v -S 1 -o vfinfocsv

# query by VfIndex
#opasaquery -v -i 0 # unsupported
#opasaquery -v -i 0 -o systemguid # unsupported
#opasaquery -v -i 0 -o nodeguid # unsupported
#opasaquery -v -i 0 -o portguid # unsupported
#opasaquery -v -i 0 -o lid # unsupported
#opasaquery -v -i 0 -o gid # unsupported
#opasaquery -v -i 0 -o desc # unsupported
#opasaquery -v -i 0 -o path # unsupported
#opasaquery -v -i 0 -o node # unsupported
#opasaquery -v -i 0 -o portinfo # unsupported
#opasaquery -v -i 0 -o sminfo # unsupported
#opasaquery -v -i 0 -o link # unsupported
#opasaquery -v -i 0 -o mcmember # unsupported
#opasaquery -v -i 0 -o inform # unsupported
#opasaquery -v -i 0 -o trace # unsupported
#opasaquery -v -i 0 -o slvl # unsupported
#opasaquery -v -i 0 -o swinfo # unsupported
#opasaquery -v -i 0 -o linfdb # unsupported
#opasaquery -v -i 0 -o ranfdb # unsupported
#opasaquery -v -i 0 -o mcfdb # unsupported
#opasaquery -v -i 0 -o vlarb # unsupported
#opasaquery -v -i 0 -o pkey # unsupported
#opasaquery -v -i 0 -o guids # unsupported
opasaquery -v -i 0 -o vfinfo
opasaquery -v -i 0 -o vfinfocsv

# query by pkey
#opasaquery -v -k 0x7fff # unsupported
#opasaquery -v -k 0x7fff -o systemguid # unsupported
#opasaquery -v -k 0x7fff -o nodeguid # unsupported
#opasaquery -v -k 0x7fff -o portguid # unsupported
#opasaquery -v -k 0x7fff -o lid # unsupported
#opasaquery -v -k 0x7fff -o gid # unsupported
#opasaquery -v -k 0x7fff -o desc # unsupported
#opasaquery -v -k 0x7fff -o path # unsupported
#opasaquery -v -k 0x7fff -o node # unsupported
#opasaquery -v -k 0x7fff -o portinfo # unsupported
#opasaquery -v -k 0x7fff -o sminfo # unsupported
#opasaquery -v -k 0x7fff -o link # unsupported
#opasaquery -v -k 0x7fff -o mcmember # unsupported
#opasaquery -v -k 0x7fff -o inform # unsupported
#opasaquery -v -k 0x7fff -o trace # unsupported
#opasaquery -v -k 0x7fff -o slvl # unsupported
#opasaquery -v -k 0x7fff -o swinfo # unsupported
#opasaquery -v -k 0x7fff -o linfdb # unsupported
#opasaquery -v -k 0x7fff -o ranfdb # unsupported
#opasaquery -v -k 0x7fff -o mcfdb # unsupported
#opasaquery -v -k 0x7fff -o vlarb # unsupported
#opasaquery -v -k 0x7fff -o pkey # unsupported
#opasaquery -v -k 0x7fff -o guids # unsupported
opasaquery -v -k 0x7fff -o vfinfo
opasaquery -v -k 0x7fff -o vfinfocsv
