#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2017, Intel Corporation
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

# [ICS VERSION STRING: @(#) ./fastfabric/opaxlattopology 10_0_0_0_135 [09/08/14 23:46]

# Topology script to translate the CSV form (topology.csv) of a
# standard-format topology spread sheet for a fabric to one or more topology
# XML files (topology.0:0.xml) at the specified levels (top-level, rack group,
# rack, edge switch).  This script operates from the fabric directory and
# populates it.

# The following topology directories and files are generated:
#   FILE_LINKSUM         - Host-to-Edge, Edge-to-Core, Host-to-Core links;
#                           includes cable data
#   FILE_LINKSUM_NOCORE  - No Edge-to-Core or Host-to-Core links;
#                           includes cable data
#   FILE_LINKSUM_NOCABLE - Leaf-to-Spine links, no cable data
#   FILE_NODEFIS         - Host FIs, includes NodeDetails
#   FILE_NODESWITCHES    - Edge, Leaf and Spine switches
#   FILE_NODECHASSIS     - Core switches
#   FILE_NODESM          - Nodes running SM
#   FILE_NODELEAVES      - Leaf switches
#   FILE_TOPOLOGY_OUT    - Topology: FILE_LINKSUM + FILE_LINKSUM_NOCABLE +
#                           FILE_NODEFIS + FILE_NODESWITCHES
#   FILE_HOSTS           - 'hosts' file
#   FILE_CHASSIS         - 'chassis' file

# User topology CSV format:
#  Lines 1 - x    ignored
#  Line y         header line 1 (ignored)
#  Line n         header line 2
#  Lines n+1 - m  topology CSV
#  Line m+1       blank
#  Lines m+2 - z  core specification(s)

# Links Fields:
#  Minimal Fields Required
#  Source Rack Group: 01
#  Source Rack:       02
#  Source Name:       03
#  Source Name-2:     04
#  Source Port:       05
#  Source Type:       06
#  Dest Rack Group:   07
#  Dest Rack:         08
#  Dest Name:         09
#  Dest Name-2:       10
#  Dest Port:         11
#  Dest Type:         12
#  Cable Label:       13
#  Cable Length:      14
#  Cable Details:     15

#  Additional Fields (Optional)
#  Link Rate:         16
#  Link MTU:          17
#  Link Details:      18
#  Node Fields:       19 and above
#  Node/PortNum Field Calculated

# Core specifications (same line):
#  Core Name:X
#  Core Size:X (288 or 1152)
#  Core Group:X
#  Core Rack:X
#  Core Full:X (0 or 1)

FIELD_COUNT=18

## Defines:
XML_GENERATE="/usr/sbin/opaxmlgenerate"
XML_INDENT="/usr/sbin/opaxmlindent"
FILE_TOPOLOGY_LINKS="topology.csv"
FILE_LINKSUM_SWD06="/usr/share/opa/samples/linksum_swd06.csv"
FILE_LINKSUM_SWD24="/usr/share/opa/samples/linksum_swd24.csv"
FILE_LINKSUM="linksum.csv"
FILE_LINKSUM_NOCORE="linksum_nocore.csv"
FILE_LINKSUM_NOCABLE="linksum_nocable.csv"
FILE_NODEFIS="nodefis.csv"
FILE_NODESWITCHES="nodeswitches.csv"
FILE_NODELEAVES="nodeleaves.csv"
FILE_NODECHASSIS="nodechassis.csv"
FILE_NODESM="nodesm.csv"
FILE_CHASSIS="chassis"
FILE_HOSTS="hosts"
FILE_TOPOLOGY_OUT="topology.0:0.xml"
FILE_RESERVE="file_reserve"
FILE_TEMP=$(mktemp "opaxlattopo-1.XXXX")
FILE_TEMP2=$(mktemp "opaxlattopo-2.XXXX")
FILE_TOPOLOGY_TEMP="topology_temp.xml"
# Note: there are no real limits on numbers of groups, racks or switches;
#  these defines simply allow error messages before too much thrashing
#  takes place in cases where FILE_TOPOLOGY_LINKS has bad data
MAX_GROUPS=21
MAX_RACKS=501
MAX_SWITCHES=20001
OUTPUT_SWITCHES=1
OUTPUT_RACKS=2
OUTPUT_GROUPS=4
OUTPUT_EDGE_LEAF_LINKS=8
OUTPUT_SPINE_LEAF_LINKS=16
NODETYPE_HFI="FI"
NODETYPE_EDGE="SW"
NODETYPE_LEAF="CL"
NODETYPE_SPINE="CS"
NODETYPE_ENDPOINT="EN"
NODETYPE_SWITCH="SW"
DUMMY_CORE_NAME="ZzNAMEqQ"
CORE_GROUP="Core Group:"
CORE_RACK="Core Rack:"
CORE_NAME="Core Name:"
CORE_SIZE="Core Size:"
CORE_FULL="Core Full:"
CAT_CHAR_CORE=" "
HFI_SUFFIX_REGEX="hfi[1-9]_[0-9]+$"
HOST_HFI_REGEX="^[a-zA-Z0-9_-]+[ ]hfi[1-9]_[0-9]+$"
CAT_CHAR=" "

MTU_SW_SW_DEFAULT=${MTU_SW_SW:-10240}
MTU_SW_HFI_DEFAULT=${MTU_SW_HFI:-10240}

## Global variables:
hfi_suffix="hfi1_0"

# Parsing tokens:
declare -a t=()

t_srcgroup=""
t_srcrack=""
t_srcname=""
t_srcname2=""
t_srcport=""
t_srctype=""
t_dstgroup=""
t_dstrack=""
t_dstname=""
t_dstname2=""
t_dstport=""
t_dsttype=""
t_cablelabel=""
t_cablelength=""
t_cabledetails=""
t_linkdetails=""
output_section="all"

# Output CSV values:
rate=""
internal=""
nodedesc1=""
nodedetails1=""
nodetype1=""
nodedesc2=""
nodedetails2=""
nodetype2=""
link=""

# Operating variables:
cts_parse=0
ix=0
n_detail=0
fl_output_edge_leaf=1
fl_output_spine_leaf=1
n_verbose=2
indent=4
fl_clean=1
ix_srcgroup=0
ix_srcrack=0
ix_srcswitch=0
ix_dstgroup=0
ix_dstrack=0
ix_dstswitch=0
core_group=""
core_rack=""
core_name=""
core_size=
core_full=
rack=""
switch=""
leaves=""
additional_fields=0
portnum_field=0
ix_line=0
group_cnt=1
rack_cnt=1

# Check Bash Version for hash tables
if (( "${BASH_VERSINFO[0]}" < 4 ))
then
  echo "opaxlattopology: At least bash-4.0 is required to run this script" >&2
  exit 1
fi

# Hash table:
declare -A core_name_size
declare -A core_name_rack
declare -A core_name_group

# Arrays
tb_group[0]=""
tb_rack[0]=""
tb_switch[0]=""
core=()
partially_populated_core=()
spine_array=()
leaf_array=()
external_leaves=()
expected_sm=()
hfi=()

# Debug variables:
debug_0=
debug_1=
debug_2=
debug_3=
debug_4=
debug_5=
debug_6=
debug_7=
#echo "DEBUG-x.y: 0:$debug_0: 1:$debug_1: 2:$debug_2: 3:$debug_3: 4:$debug_4: 5:$debug_5: 6:$debug_6: 7:$debug_7:"

array_contains() {
    local array=("${!1}")
    local seeking=$2
    local found=1
    for element in "${array[@]}"; do
        if [[ "$element" == "$seeking" ]] ; then
            found=0
            break
        fi
    done
    return $found
}

misc_params=""

declare -A misc_sw_table
declare -A misc_fi_table
declare -A sw_portlist_table

declare -a misc_param_index

remove_file() {
  if [ -f $1 ]; then
   rm -f $1
  fi
}

clean_tempfiles() {
  if [ $fl_clean == 1 ]
    then
    remove_file "$FILE_TEMP"
    remove_file "$FILE_TEMP2"
    remove_file "$FILE_LINKSUM"
    remove_file "$FILE_LINKSUM_NOCORE"
    remove_file "$FILE_LINKSUM_NOCABLE"
    remove_file "$FILE_NODEFIS"
    remove_file "$FILE_NODESWITCHES"
    remove_file "$FILE_NODELEAVES"
    remove_file "$FILE_NODECHASSIS"
    remove_file "$FILE_NODESM"
  fi
}

topology_file_cleanup() {
  if [ -f $FILE_TOPOLOGY_TEMP ]; then
    remove_file "$FILE_TOPOLOGY_TEMP"
  fi
}

trap 'clean_tempfiles; topology_file_cleanup; exit 1' SIGINT SIGHUP SIGTERM
trap 'clean_tempfiles' EXIT

## Local functions:
functout=

# Output usage information
usage_full()
{
  echo "Usage: opaxlattopology [-d level] [-v level] [-i level] [-K]" >&2
  echo "                          [-s hfi_suffix] [source [dest]]" >&2
  echo "           or" >&2
  echo "       opaxlattopology --help" >&2
  echo "       --help        -  produce full help text" >&2
  echo "       -d level      -  output detail level (default 0)" >&2
  echo "                        values are additive" >&2
  echo "                         1 - edge switch topology files" >&2
  echo "                         2 - rack topology files" >&2
  echo "                         4 - rack group topology files" >&2
# TBD - these options are disabled for now
#  echo "                     8 - DO NOT output edge-to-leaf links" >&2
#  echo "                    16 - DO NOT output spine-to-leaf links" >&2
  echo "       -v level      -  verbose level (0-8, default 2)" >&2
  echo "                         0 - no output" >&2
  echo "                         1 - progress output" >&2
  echo "                         2 - reserved" >&2
  echo "                         4 - time stamps" >&2
  echo "                         8 - reserved" >&2
  echo "       -i level      -  output indent level (default 4)" >&2
  echo "       -K            -  DO NOT clean temporary files" >&2
  echo "       -s hfi_suffix -  Used on Multi-Rail or Multi-Plane fabrics" >&2
  echo "                        Can be used to override the default hfi1_0." >&2
  echo "                        For Multi-Plane fabric, use the tool multiple" >&2
  echo "                        times with different hfi-suffix. For Multi-Rail" >&2
  echo "                        specify HostName as \"HostName HfiName\" in spreadsheet" >&2
  echo "       -o report     -  report type for output; by default, all the sections" >&2
  echo "                        are generated" >&2
  echo "                        Report Types:" >&2
  echo "                        brnodes  - Creates <Node> section xml for the csv input" >&2
  echo "                        links    - Creates <LinkSummary> section xml for the csv input" >&2
  echo "" >&2
  echo "   The following environment variables allow user-specified MTU" >&2
  echo "      MTU_SW_SW  -  If set will override default MTU on switch<->switch links" >&2
  echo "                       (default is 10240)" >&2
  echo "      MTU_SW_HFI -  If set will override default MTU on switch<->HFI links" >&2
  echo "                       (default is 10240)" >&2
  exit $1
}  # End of usage_full()

if [ x"$1" = "x--help" ]
then
  usage_full "0"
fi

# Convert general node types to standard node types
# Inputs:
#   $1 = general node type
#
# Outputs:
#   Standard node type
cvt_nodetype()
{

  local nodetype=$(echo "$1" | awk '{print toupper($0)}')
  case $nodetype in
  $NODETYPE_HFI)
    echo "$NODETYPE_HFI"
    ;;
  $NODETYPE_EDGE)
    echo "$NODETYPE_SWITCH"
    ;;
  $NODETYPE_LEAF)
    echo "$NODETYPE_SWITCH"
    ;;
  $NODETYPE_SPINE)
    echo "$NODETYPE_SWITCH"
    ;;
  $NODETYPE_ENDPOINT)
    echo "$NODETYPE_ENDPOINT"
    ;;
  *)
    echo ""
    ;;
  esac

}  # End of cvt_nodetype()

# Display progress information (to STDOUT)
# Inputs:
#   $1 - progress string
#
# Outputs:
#   none
display_progress()
{
  if [ $n_verbose -ge 1 ]
    then
    echo "$1"
    if [ $n_verbose -ge 4 ]
      then
      echo "  "`date`
    fi
  fi
}  # End of display_progress()

# Generate directory-level FILE_TOPOLOGY_OUT file from directory-level
# CSV files; consolidate directory-level CSV files.
# Inputs:
#   $1 = 1 - Use FILE_LINKSUM
#        0 - Use FILE_LINKSUM_NOCORE
#   $2 = 1 - Use FILE_LINKSUM_NOCABLE
#        0 - Do not use FILE_LINKSUM_NOCABLE
#   FILE_LINKSUM
#   FILE_LINKSUM_NOCORE (optional)
#   FILE_LINKSUM_NOCABLE (optional)
#   FILE_NODEFIS
#   FILE_NODESWITCHES
#   FILE_NODECHASSIS
#   FILE_NODESM
#
# Outputs:
#   FILE_TOPOLOGY_OUT
#   FILE_HOSTS
#   FILE_CHASSIS
gen_topology()
{
  local args=""
  if [ -f $FILE_LINKSUM ]
    then
    remove_file "$FILE_TEMP"
    mv $FILE_LINKSUM $FILE_TEMP
    sort -u $FILE_TEMP > $FILE_LINKSUM
  fi
  if [ -f $FILE_LINKSUM_NOCORE ]
    then
    remove_file "$FILE_TEMP"
    mv $FILE_LINKSUM_NOCORE $FILE_TEMP
    sort -u $FILE_TEMP > $FILE_LINKSUM_NOCORE
  fi
  if [ -f $FILE_LINKSUM_NOCABLE ]
    then
    remove_file "$FILE_TEMP"
    mv $FILE_LINKSUM_NOCABLE $FILE_TEMP
    sort -u $FILE_TEMP > $FILE_LINKSUM_NOCABLE
  fi

  if [ -f $FILE_NODEFIS ]
    then
    remove_file "$FILE_HOSTS"
    remove_file "$FILE_TEMP"
    remove_file "$FILE_TEMP2"
    mv $FILE_NODEFIS $FILE_TEMP
    sort -u $FILE_TEMP > $FILE_TEMP2

    # Read list of FI node data
    while IFS='' read -r node_in
    do
      # Extract node detail content from FI node data
      details=""
      if [[ "$node_in" =~ ";" ]]; then
        details=`echo "$node_in" | cut -d ';' -f 2 -`
      fi
      # Use node desc to lookup optional XML data (misc entries)
      node=${node_in/;*/}
      lookup_misc_table_entry "$node" "$NODETYPE_HFI" args
      # opaxmlgenerate is pedantic about the exact number of delimiters ';'
      if [ "$args" == "" ]; then
        echo "$node;$details" >> $FILE_NODEFIS
      else
        echo "$node;$details;$args" >> $FILE_NODEFIS
      fi
    done < $FILE_TEMP2
    cut -d ';' -f 1 $FILE_NODEFIS | sed -e "s/$CAT_CHAR$HFI_SUFFIX_REGEX//" > $FILE_HOSTS
  fi

  if [ -f $FILE_NODESWITCHES ]
    then
    remove_file "$FILE_TEMP"
    mv $FILE_NODESWITCHES $FILE_TEMP
    sort -u $FILE_TEMP > $FILE_TEMP2

    # Read list of SW node data
    while IFS='' read -r node_in
    do
      # Use node desc to lookup optional XML data (misc entries)
      node=${node_in/;*/}

      # Destination Node Descriptions might be blank if it is an endpoint (ignore them)
      if [[ "${node//[[:space:]]/}" == "" ]]; then
        continue
      fi

      lookup_misc_table_entry "$node" "$NODETYPE_EDGE" args

      if [ "$args" != "" ]; then
        echo "$node;$args" >> $FILE_NODESWITCHES
      else
        echo "$node" >> $FILE_NODESWITCHES
      fi
    done < $FILE_TEMP2
  fi
  if [ -f $FILE_NODECHASSIS ]
    then
    remove_file "$FILE_CHASSIS"
    remove_file "$FILE_TEMP"
    mv $FILE_NODECHASSIS $FILE_TEMP
    sort -u $FILE_TEMP > $FILE_NODECHASSIS
    cp -p $FILE_NODECHASSIS $FILE_CHASSIS
  fi
  if [ -f $FILE_NODESM ]
    then
    remove_file "$FILE_TEMP"
    mv $FILE_NODESM $FILE_TEMP
    sort -u $FILE_TEMP > $FILE_NODESM
  fi

  remove_file "$FILE_TOPOLOGY_TEMP"
  echo '<?xml version="1.0" encoding="utf-8" ?>' >> $FILE_TOPOLOGY_TEMP
  echo "<Report>" >> $FILE_TOPOLOGY_TEMP

  if [ "$output_section" == "all"  ] || [ "$output_section" == "links" ] ; then
    # Generate LinkSummary section
    echo "<LinkSummary>" >> $FILE_TOPOLOGY_TEMP
    if [ -s $FILE_LINKSUM -a $1 == 1 ]
      then
      $XML_GENERATE -X $FILE_LINKSUM -d \; -i 2 -h Link -g Rate -g MTU -g LinkDetails -g Internal -h Cable -g CableLength -g CableLabel -g CableDetails -e Cable -h Port -g PortNum -g NodeType -g NodeDesc -e Port -h Port -g PortNum -g NodeType -g NodeDesc -e Port -e Link >> $FILE_TOPOLOGY_TEMP
    elif [ -s $FILE_LINKSUM_NOCORE -a $1 == 0 ]
      then
      $XML_GENERATE -X $FILE_LINKSUM_NOCORE -d \; -i 2 -h Link -g Rate -g MTU -g LinkDetails -g Internal -h Cable -g CableLength -g CableLabel -g CableDetails -e Cable -h Port -g PortNum -g NodeType -g NodeDesc -e Port -h Port -g PortNum -g NodeType -g NodeDesc -e Port -e Link >> $FILE_TOPOLOGY_TEMP
    fi

    if [ -s $FILE_LINKSUM_NOCABLE -a $2 == 1 ]
      then
      # Note: <Cable> header not needed because cable data is null
      $XML_GENERATE -X $FILE_LINKSUM_NOCABLE -d \; -i 2 -h Link -g Rate -g MTU -g LinkDetails -g Internal -g CableLength -g CableLabel -g CableDetails -h Port -g PortNum -g NodeType -g NodeDesc -e Port -h Port -g PortNum -g NodeType -g NodeDesc -e Port -e Link >> $FILE_TOPOLOGY_TEMP
    fi
    echo "</LinkSummary>" >> $FILE_TOPOLOGY_TEMP
  fi

  if [ "$output_section" == "all"  ] || [ "$output_section" == "brnodes" ] ; then
    # Generate Nodes/FIs section
    echo "<Nodes>" >> $FILE_TOPOLOGY_TEMP
    echo "<FIs>" >> $FILE_TOPOLOGY_TEMP
    if [ -s $FILE_NODEFIS ]
      then
      $XML_GENERATE -X $FILE_NODEFIS -d \; -i 2 -h Node -g NodeDesc -g NodeDetails $misc_params -e Node >> $FILE_TOPOLOGY_TEMP
    fi
    echo "</FIs>" >> $FILE_TOPOLOGY_TEMP

    # Generate Switch Section
    generate_switch_section

    # Generate SM section
    echo "<SMs>" >> $FILE_TOPOLOGY_TEMP
    if [ -s $FILE_NODESM ]
      then
      $XML_GENERATE -X $FILE_NODESM -i 2 -h SM -g NodeDesc -g PortNum -e SM >> $FILE_TOPOLOGY_TEMP
    fi
    echo "</SMs>" >> $FILE_TOPOLOGY_TEMP
    echo "</Nodes>" >> $FILE_TOPOLOGY_TEMP
  fi

  echo "</Report>" >> $FILE_TOPOLOGY_TEMP

  # trim leading and trailing whitespace from tag contents and add indent
  $XML_INDENT -t $FILE_TOPOLOGY_TEMP -i $indent > $FILE_TEMP 2>/dev/null
  cat $FILE_TEMP > $FILE_TOPOLOGY_OUT

  # Clean temporary files
  clean_tempfiles
  topology_file_cleanup
}  # End of gen_topology

# Append to LINKSUM_NOCABLE file using parameter as input.  
# Inputs:
#   $1 = FILE_LINKSUM_SWD06 or FILE_LINKSUM_SWD24
# Outputs: FILE_LINKSUM_NOCABLE
generate_linksum_nocable()
{
  if ! [ -z "$1" ]
  then
    local IFS=";"
    cat $1 | sed -e "s/$DUMMY_CORE_NAME/$core_name/g" -e "s/$CAT_CHAR_CORE/$CAT_CHAR/g" | grep -E "$leaves" > $FILE_TEMP

    while read -a t
    do
      if [ "${t[7]}" == "$NODETYPE_SWITCH" ] && [ "${t[10]}" == "$NODETYPE_SWITCH" ]; then
        local IFS="|" link="${t[0]};${MTU_SW_SW};;${t[2]};${t[3]};${t[4]};${t[5]};${t[6]};${t[7]};${t[8]};${t[9]};${t[10]};${t[11]}"
        # Add Source Spine
        add_portnum_to_switchportlist "${t[6]}" "${t[8]}" "1"
        # Add Destination Leaf
        add_portnum_to_switchportlist "${t[9]}" "${t[11]}" "1"
      else
        local IFS="|" link="${t[0]};${MTU_SW_HFI};;${t[2]};${t[3]};${t[4]};${t[5]};${t[6]};${t[7]};${t[8]};${t[9]};${t[10]};${t[11]}"
      fi
      echo "$link" >> ${FILE_LINKSUM_NOCABLE}
      local IFS=";"
    done < $FILE_TEMP
  fi
}

# Process rack group name; check for non-null name and find in tb_group[].
# If present return tb_group[] index, otherwise make entry and return index.
# Note that tb_group[0] is always null and is the default rack group.
# Inputs:
#   $1 = rack group name (may be null)
#
# Outputs:
#         functout - index of rack group name, or 0
#   tb_rack[index] - name of rack group (written when new group)
proc_group()
{
  local val
  local ix

  val=0

  if [ $((n_detail & OUTPUT_GROUPS)) != 0 ] || [ $((n_detail & OUTPUT_RACKS)) != 0 ] || [ $((n_detail & OUTPUT_SWITCHES)) != 0 ]
    then
    if [ -n "$1" ]
      then
      # Check for group name already in tb_group[]
      for (( ix=1 ; $ix<$MAX_GROUPS ; ix=$((ix+1)) ))
      do
        if [ -n "${tb_group[$ix]}" ]
          then
          if [ "$1/" == "${tb_group[$ix]}" ]
            then
            val=$ix
            break
          fi
        # New group name, save in tb_group[] and make group directory
        else
          tb_group[$ix]="$1/"
          rm -f -r ${tb_group[$ix]}
          mkdir ${tb_group[$ix]}
          val=$ix
          (( group_cnt++ ))
          break
        fi
      
      done  # for (( ix=1 ; $ix<$MAX_GROUPS

      if [ $ix == $MAX_GROUPS ]
        then
        echo "Too many rack groups (>= $MAX_GROUPS)" >&2
        usage_full "2"
      fi

    else
      echo "Must have rack group name when outputting rack group (line:$ix_line)" >&2
      usage_full "2"
    fi  # End of if [ -n "$1" ]

  fi  # End of if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]

  functout=$val

}  # End of proc_group()

# Process rack name; check for non-null name and find in tb_rack[].
# If present return tb_rack[] index, otherwise make entry and return index.
# Note that tb_rack[0] is always null and is the default rack.
# Inputs:
#   $1 = rack name (may be null)
#   $2 = rack group name (may be null)
#
# Outputs:
#         functout - index of rack name, or 0
#   tb_rack[index] - name of rack (written when new rack)
proc_rack()
{
  local val
  local ix

  val=0

  if [ $((n_detail & OUTPUT_RACKS)) != 0 ] || [ $((n_detail & OUTPUT_SWITCHES)) != 0 ]
    then
    if [ -n "$1" ]
      then
      # Check for rack name already in tb_rack[]
      for (( ix=1 ; $ix<$MAX_RACKS ; ix=$((ix+1)) ))
      do
        if [ -n "${tb_rack[$ix]}" ]
          then
          if [ "$1/" == "${tb_rack[$ix]}" ]
            then
            val=$ix
            break
          fi
        # New rack name, save in tb_rack[] and make rack directory
        else
          tb_rack[$ix]="$1/"
          rm -f -r $2${tb_rack[$ix]}
          mkdir $2${tb_rack[$ix]}
          val=$ix
          (( rack_cnt++ ))
          break
        fi
      
      done  # for (( ix=1 ; $ix<$MAX_RACKS

      if [ $ix == $MAX_RACKS ]
        then
        echo "Too many racks (>= $MAX_RACKS)" >&2
        usage_full "2"
      fi

    else
      echo "Must have rack name when outputting rack (line:$ix_line)" >&2
      usage_full "2"
    fi  # End of if [ -n "$1" ]

  fi  # End of if [ $((n_detail & OUTPUT_RACKS)) != 0 ]

  functout=$val

}  # End of proc_rack()

# Process switch name; check for non-null name and find in tb_switch[].
# If present return tb_switch[] index, otherwise make entry and return index.
# Note that tb_switch[0] is always null and is the default switch.
# Inputs:
#   $1 = switch name (may be null)
#   $2 = rack group/rack name (may be null)
#
# Outputs:
#           functout - index of switch name, or 0
#   tb_switch[index] - name of switch (written when new switch)
proc_switch()
{
  local val
  local ix

  val=0

  if [ $((n_detail & OUTPUT_SWITCHES)) != 0 ]
    then
    if [ -n "$1" ]
      then
      # Check for switch name already in tb_switch[]
      for (( ix=1 ; $ix<$MAX_SWITCHES ; ix=$((ix + 1)) ))
      do
        if [ -n "${tb_switch[$ix]}" ]
          then
          if [ "$1/" == "${tb_switch[$ix]}" ]
            then
            val=$ix
            break
          fi
        # New switch name, save in tb_switch[] and make switch directory
        else
          tb_switch[$ix]="$1/"
          rm -f -r $2${tb_switch[$ix]}
          mkdir $2${tb_switch[$ix]}
          val=$ix
          break
        fi
      
      done  # for (( ix=1 ; $ix<$MAX_SWITCHES

      if [ $ix == $MAX_SWITCHES ]
        then
        echo "Too many switches (>= $MAX_SWITCHES)" >&2
        usage_full "2"
      fi

    else
      echo "Must have switch name when outputting switch (line:$ix_line)" >&2
      usage_full "2"
    fi  # End of if [ -n "$1" ]

  fi  # End of if [ $((n_detail & OUTPUT_RACKS)) != 0 ]

  functout=$val

}  # End of proc_switch()

# Function to trim the trailing whitespace which
# a user may enter by mistake
trim_trailing_whitespace()
{
  echo "$(echo -e "${1}" | sed -e 's/[[:space:]]*$//')"
}

process_sm()
{
  for element in "${t[@]:1}"
  do
    if [ -n "$element" ]; then
      host=`echo "$element" | cut -d : -f 1`
      port=`echo "$element" | cut -s -d : -f 2`
      if [ -z "$port" ]; then
        if array_contains hfi[@] "$host" ; then
          port=1
        else
          port=0
        fi
      fi
      if array_contains hfi[@] "$host" && ! [[ "$host" =~ $HOST_HFI_REGEX ]] ; then
        expected_sm+=("${host}${CAT_CHAR}${hfi_suffix};$port")
      else
        expected_sm+=("$host;$port")
      fi
    fi
  done
  # Add expected sm to FILE_NODESM
  for element in "${expected_sm[@]}"
  do
    echo "$element" >> $FILE_NODESM
  done
}


# Join miscellaneous fields of global $t[] into a user specified delimited string
join_fields_into_string()
{
  local __res=$1
  local delimiter=$2
  local entry=""
  local string=""
  local args=""
  local i=""
  local start=$FIELD_COUNT
  local end=$((FIELD_COUNT+additional_fields))
  local IFS

  eval $__res="''"

  ((end--))
  for i in `seq $start $end`; do
    entry="${t[$i]}"
    if [ "$string" == "" ]; then
      string=$entry
    else
      string="${string}${delimiter}${entry}"
    fi
  done

  # Add a terminating delimeter (this helps parsers account for empty last column cells)
  if [ "$string" != "" ]; then
    string="${string}${delimiter}"
  fi

  eval $__res="'$string'"
}

parse_table_header()
{
  local IFS=","
  declare -a element_list
  declare -a subelement_list
  declare -a misc_hdr_list
  local misc_column_list=""
  local element=""
  local subelement=""
  local subelement_test=""
  local nested_element=""
  local entry=""
  local index_pos=-1
  misc_param_index=()

  join_fields_into_string misc_column_list ","

  # Excel will put a cell in quotes if it contains a comma during CSV generation
  if [[ "$misc_column_list" =~ "\"" ]]; then
    echo "Parse error no quoted strings in miscellaneous header cell: $misc_column_list" >&2
    exit 2
    return
  fi

  if [[ "$misc_column_list" =~ ";" ]]; then
    echo "Parse error no semicolons in miscellaneous header: $misc_column_list" >&2
    exit 2
    return
  fi

  for element in $misc_column_list; do
    if [[ "$element" == "" ]]; then
      echo "Parse error empty element in miscellaneous header" >&2
      exit 2
    fi

    if [[ $element =~ [[:space:]] ]]; then
      echo "Parse error whitespace in miscellaneous element '${element}'" >&2
      exit 2
    fi

    element_list+=($element)

    # check if entry is part of subelement
    if [[ "$element" =~ ":" ]]; then
       # sanity check subelement
       subelement=${element/:*/}
       if [[ "$subelement" == "" ]]; then
          echo "Parse error subelement name not specified '${element}'" >&2
          exit 2
       fi

       # sanity check nested element
       nested_element=`echo $element | cut -d ':' -f 2`
       if [[ "$nested_element" == "" ]]; then
          echo "Parse error nested element name not specified '${element}'" >&2
          exit 2
       fi
       subelement_list+=($subelement)
    fi
  done

  IFS=' '
  # sort subelements and remove duplicates
  subelement_list=($(echo ${subelement_list[@]} | tr ' ' '\n' | sort -u| tr '\n' ' '))

  # add the root element too beginning of list
  subelement_list=($(echo "__root ${subelement_list[@]}"))

  for subelement in "${subelement_list[@]}"; do
     for element in "${element_list[@]}"; do

       if [[ "$subelement" == "__root" ]]; then
         # a root element has is a element without : seperator
         if ! [[ "$element" =~ ":" ]]; then
           misc_hdr_list+=($element)
           continue
         fi
       fi

       # check if entry is a member of subelement
       subelement_test=${element/:*/}
       if [[ "$subelement" == "$subelement_test" ]]; then
         misc_hdr_list+=($element)
       fi
     done
  done

  # Build parameter list for opaxmlgenerate
  subelement="__root"
  args=""
  subelement_test=""
  nested_element=""

  for entry in "${misc_hdr_list[@]}"; do
    if [[ "$entry" =~ ":" ]]; then
       subelement_test=${entry/:*/}
       nested_element=`echo $entry | cut -d ':' -f 2`
       if [[ "$subelement" != "$subelement_test" ]]; then
          if [[ "$subelement" == "__root" ]]; then
            args="$args -h $subelement_test -g $nested_element"
          else
            args="$args -e $subelement -h $subelement_test -g $nested_element"
          fi
          subelement=$subelement_test;
       else
          args="$args -g $nested_element"
       fi
    else
      if [[ "$subelement" != "__root" ]]; then
        echo "Unexpected error root element" >&2
        exit 2
      fi
      args="$args -g $entry"
    fi
  done
  if [[ "$nested_element" != "" ]]; then
      args="$args -e $subelement"
  fi

  misc_params=$args

  # Now that columns are in a different order build an index to map the data set
  index_pos=-1
  for element in "${element_list[@]}"; do
     ((index_pos++))
     entry_pos=0
     for entry in "${misc_hdr_list[@]}"; do
        if [[ "$entry" == "$element" ]]; then
           misc_param_index[$index_pos]=$entry_pos
           break
        fi
        ((entry_pos++))
     done
  done
}

add_fields_to_misc_table()
{
  # save entry $3 to lookup table via node desc $1 and type $2
  local node_desc=$1
  local node_type=$2
  local arg=""
  local node_misc_entry=""
  local i=0
  local index_pos=0
  local cnt=0
  local IFS
  declare -a node_misc_entry_array=()
  declare -a sorted_node_misc_entry_array=()

  if [ "$node_desc" == "" ]; then
    return
  fi

  # Join node field arguments into a comma-delimited string
  join_fields_into_string node_misc_entry ","

  if [ "$node_misc_entry" == "" ]; then
    return
  fi

  # Excel will put a cell in quotes if it contains a comma during CSV generation
  if [[ "$node_misc_entry" =~ "\"" ]]; then
    echo "Parse error no quoted strings in cell $node_misc_entry" >&2
    exit 2
    return
  fi

  # Convert comma delimiter to semicolon delimiter for opaxmlgenerate
  IFS="," node_misc_entry_array=($node_misc_entry)

  cnt=${#node_misc_entry_array[@]}
  ((cnt--))

  # Sort input entries using the parameter index for opaxmlgenerate
  # Create a semicolon delmited string
  unset IFS

  if [[ "$cnt" -ge 0 ]]; then
    for i in `seq 0 $cnt`
    do
      index_pos=${misc_param_index[$i]}
      if [[ "$index_pos" == "" ]]; then
        echo "Unexpected error index not defined for position $i" >&2
        exit 2
      fi
      sorted_node_misc_entry_array[$index_pos]=${node_misc_entry_array[$i]}
    done

    node_misc_entry="${sorted_node_misc_entry_array[0]}"
    for i in `seq 1 $cnt`
    do
      node_misc_entry="$node_misc_entry;${sorted_node_misc_entry_array[$i]}"
    done
  fi

  # Remove whitespace in node description for associative array lookup (required by Bash)
  node_desc=${node_desc//[[:space:]]/_}

  case $node_type in
    $NODETYPE_EDGE )
      misc_sw_table[$node_desc]="$node_misc_entry"
    ;;
    $NODETYPE_LEAF )
      misc_sw_table[$node_desc]="$node_misc_entry"
    ;;
    $NODETYPE_SPINE )
      misc_sw_table[$node_desc]="$node_misc_entry"
    ;;
    $NODETYPE_HFI )
      misc_fi_table[$node_desc]="$node_misc_entry"
    ;;
    *)
      echo "Error invalid node type '${node_type}'" >&2
      exit 2
    ;;
  esac
}

add_portnum_to_switchportlist()
{
  local portnum=$1
  local node_desc=$2
  local internal=$3
  local portlist=""
  local IFS

  if [ "$node_desc" == "" ]; then
    return
  fi

  # Remove whitespace in node description for associative array lookup (required by Bash)
  node_desc=${node_desc//[[:space:]]/_}
  portlist=${sw_portlist_table[$node_desc]}

  # Make sure internal parameter numerical value
  if [[ ${internal//[[:digit:]]/} ]]; then
    echo "Internal Error: internal parameter is non-numerical \"$internal\"" >&2
    exit 1
  fi

  # Make sure port number is numerical value
  if [[ ${portnum//[[:digit:]]/} ]]; then
    echo "Error port number is non-numerical \"$portnum\" (line:$ix_line)" >&2
    exit 2
  fi

  # Make sure this port is not already in the portlist
  IFS=','
  for port in $portlist
  do
    if [ "$port" == "$portnum" ]; then
       # check to make sure there are not duplicate entries in the CSV files
       # accept duplicate internal ports and drop them from list
       if [ "$internal" == "1" ]; then
         return
       else
         echo "Error port number \"$portnum\" is already in the list of ports processed for \"$node_desc\" (line:$ix_line)" >&2
         exit 2
       fi
    fi
  done

  if [ "$portlist" == "" ]; then
    portlist="$portnum"
  else
    portlist="${portlist},${portnum}"
  fi

  sw_portlist_table[$node_desc]="$portlist"
}

lookup_portnum_from_switchportlist()
{
  local node_desc=$1
  local __res=$2
  local portlist=""

  if [ "$node_desc" == "" ]; then
    return
  fi

  # Remove whitespace in node description for associative array lookup (required by Bash)
  node_desc=${node_desc//[[:space:]]/_}
  portlist=${sw_portlist_table[$node_desc]}

  eval $__res="'$portlist'"
}

generate_switch_section()
{
  local line=""
  local node_desc=""
  local node_desc_key=""
  local port_list=""
  declare -a port_array
  local portnum=""
  local switch_entry=""
  local portnum_entry=""
  local IFS

  # Generate Nodes/Switches section in XML
  echo "<Switches>" >> $FILE_TOPOLOGY_TEMP
  if [ -s $FILE_NODESWITCHES ]
    then
    while read -r line; do
        portnum_entry=""
        switch_entry=""
        node_desc=${line//;*}
        if [ "$node_desc" == "" ]; then
          echo "Internal Error: Node Description Empty" >&2
          exit 1
        fi

        # Sanity check Switch Node ports
        lookup_portnum_from_switchportlist "$node_desc" port_list
        if [ "$port_list" == "" ]; then
          echo "Internal Error: Switch defined (${node_desc}) with no ports" >&2
          exit 1
        fi

        # If Switch entry has additional parameters, generate XML entry with $XML_GENERATE
        unset IFS
        if [[ "$line" =~ ";" ]]; then
          switch_entry=`echo $line | $XML_GENERATE -X - -d \; -i 2 -h Node -g NodeDesc $misc_params`
        else
          # Otherwise manually create Switch entries
          switch_entry="<Node>"$'\n'
          switch_entry=${switch_entry}"<NodeDesc>${node_desc}</NodeDesc>"
          switch_entry=${switch_entry}$'\n'

          # Add required PortNum 0 definition
          if [ "$port_field" == "required" ]; then
            portnum_entry=${portnum_entry}$'\n'
            portnum_entry=${portnum_entry}"<Port>"
            portnum_entry=${portnum_entry}$'\n'
            portnum_entry=${portnum_entry}"<PortNum>0</PortNum>"
            portnum_entry=${portnum_entry}$'\n'
            portnum_entry=${portnum_entry}"</Port>"
          fi
        fi

        if [ "$port_field" == "required" ]; then
          # Sort Port number list to make output more readable
          IFS=','
          for portnum in $port_list; do
            port_array+=($portnum)
          done
          port_array=($(echo ${port_array[@]} | tr ' ' '\n' | sort -n -u| tr '\n' ' '))

          unset IFS
          # Insert Port number list to XML
          for portnum in ${port_array[@]}
          do
            portnum_entry=${portnum_entry}$'\n'
            portnum_entry=${portnum_entry}"<Port>"
            portnum_entry=${portnum_entry}$'\n'
            portnum_entry=${portnum_entry}"<PortNum>${portnum}</PortNum>"
            portnum_entry=${portnum_entry}$'\n'
            portnum_entry=${portnum_entry}"</Port>"
          done
        fi

        # Write entry to XML file
        echo $switch_entry >> $FILE_TOPOLOGY_TEMP
        echo $portnum_entry >> $FILE_TOPOLOGY_TEMP
        echo "</Node>" >> $FILE_TOPOLOGY_TEMP
    done < $FILE_NODESWITCHES
  fi
  echo "</Switches>" >> $FILE_TOPOLOGY_TEMP
}

lookup_misc_table_entry()
{
  # lookup entry via node desc $1 and type $2 and return by updating $3
  local node_desc=$1
  local node_type=$2
  local __res=$3
  local entry=""

  if [ "$node_desc" == "" ]; then
    return
  fi

  # Remove whitespace in node description for associative array lookup (required by Bash)
  node_desc=${node_desc//[[:space:]]/_}

  case $node_type in
    $NODETYPE_EDGE )
      entry=${misc_sw_table[$node_desc]}
    ;;
    $NODETYPE_LEAF )
      entry=${misc_sw_table[$node_desc]}
    ;;
    $NODETYPE_SPINE )
      entry=${misc_sw_table[$node_desc]}
    ;;
    $NODETYPE_HFI )
      entry=${misc_fi_table[$node_desc]}
    ;;
    *)
      echo "Error invalid node type '${node_type}'" >&2
      exit 2
    ;;
  esac

  eval $__res="'$entry'"
}

## Main function:

# Get options
while getopts d:i:Kv:s:o: option
do
  case $option in
  d)
    n_detail=$OPTARG
    if [ $((n_detail & OUTPUT_EDGE_LEAF_LINKS)) != 0 ]
      then
      fl_output_edge_leaf=0
    fi
    if [ $((n_detail & OUTPUT_SPINE_LEAF_LINKS)) != 0 ]
      then
      fl_output_spine_leaf=0
    fi
    ;;

  i)
    indent=$OPTARG
    ;;

  K)
    fl_clean=0
    ;;

  v)
    n_verbose=$OPTARG
    ;;

  o)
    output_section=$OPTARG
    if [ "$output_section" != "brnodes" ] && [ "$output_section" != "links" ] ; then
      echo "opaxlattopology -o: Invalid Argument - $output_section"
      usage_full "2"
      exit 1
    fi
    ;;

  s)
    hfi_suffix=$OPTARG
    if [[ "$hfi_suffix" =~ $HFI_SUFFIX_REGEX ]] ; then
        hfiNum=`echo "$hfi_suffix" | cut -d "_" -f2`
        # Change the name of FILE_TOPOLOGY_OUT as per the hfi name
        # This can be overwritten by specifying output file name through command line
        FILE_TOPOLOGY_OUT="topology.$((hfiNum+1)):0.xml"
    elif [[ "$hfi_suffix" == "" ]] ; then
        # Do nothing
        :
    else
        echo "opaxlattopology -s: Invalid Argument, should be like hfi1_0 or empty string"
        usage_full "2"
        exit 1
    fi
    ;;

  *)
    usage_full "2"
    ;;
  esac
done

shift $((OPTIND -1))

if [ $# -ge 1 ]; then
	FILE_TOPOLOGY_LINKS=$1
	shift;
fi
if [ $# -ge 1 ]; then 
	FILE_TOPOLOGY_OUT=$1
fi

# Parse FILE_TOPOLOGY_LINKS2
display_progress "Parsing $FILE_TOPOLOGY_LINKS"

remove_file "$FILE_LINKSUM"
remove_file "$FILE_LINKSUM_NOCORE"
remove_file "$FILE_LINKSUM_NOCABLE"
remove_file "$FILE_NODEFIS"
remove_file "$FILE_NODESWITCHES"
remove_file "$FILE_NODELEAVES"
remove_file "$FILE_NODECHASSIS"

# default rate
RATE_DEFAULT=${RATE_DEFAULT:-"100g"}

while IFS="," read -a t
do
  ((ix_line++))

  case $cts_parse in
  # Syncing to beginning of link data
  0)
    if [ "${t[0]}" == "Rack Group" ]
    then
      # Parse remaining optional (CSV formatted) column headers
      # Generate additional parameters for opaxmlgenerate
      # based on the optional column headers
      # Count additional optional fields
      additional_fields=${#t[@]}
      additional_fields=$((additional_fields-FIELD_COUNT))
      # This may happen if user is using minimal sample csv
      if [[ "$additional_fields" -lt "0" ]]; then
         additional_fields=0
         portnum_field=FIELD_COUNT
      else
        # Topology XMLs require a Node/Port/PortNum Field
        portnum_field=${#t[@]}
        if [[ "$additional_fields" -gt "0" ]] ; then
          t_additional=${t[@]:$FIELD_COUNT}
          if array_contains t_additional[@] "Port:LID" || array_contains t_additional[@] "Port:LMC" ; then
            port_field="required"
            t[$portnum_field]="Port:PortNum"
            ((additional_fields++))
          fi
        fi
      fi
      parse_table_header
      cts_parse=1
    fi
  ;;

  # Process link tokens
  1)
    if [ -n "${t[0]}" ]
      then
      t_srcgroup=`trim_trailing_whitespace "${t[0]}"`
    fi
    if [ -n "${t[1]}" ]
      then
      t_srcrack=`trim_trailing_whitespace "${t[1]}"`
    fi
    t_srcname=`trim_trailing_whitespace "${t[2]}"`
    t_srcname2=`trim_trailing_whitespace "${t[3]}"`
    if [ -n "${t[5]}" ]
      then
      t_srctype=`trim_trailing_whitespace "${t[5]}"`
    fi
    if [ -z "${t[4]}" -a "$t_srctype" == "$NODETYPE_HFI" ]
      then
      t_srcport=1
    else
      t_srcport=`trim_trailing_whitespace "${t[4]}"`
    fi

    if [ -n "${t[6]}" ]
      then
      t_dstgroup=`trim_trailing_whitespace "${t[6]}"`
    fi
    if [ -n "${t[7]}" ]
      then
      t_dstrack=`trim_trailing_whitespace "${t[7]}"`
    fi
    t_dstname=`trim_trailing_whitespace "${t[8]}"`
    t_dstname2=`trim_trailing_whitespace "${t[9]}"`
    if [ -n "${t[11]}" ]
      then
      t_dsttype=`trim_trailing_whitespace "${t[11]}"`
    fi
    if [ -z "${t[10]}" -a "$t_dsttype" == "$NODETYPE_HFI" ]
      then
      t_dstport=1
    else
      t_dstport=`trim_trailing_whitespace "${t[10]}"`
    fi

    if [ `cvt_nodetype "$t_srctype"` == "$NODETYPE_SWITCH" ]; then
      t[$portnum_field]=0
    else
      t[$portnum_field]=$t_srcport
    fi

    t_cablelabel=`trim_trailing_whitespace "${t[12]}"`
    t_cablelength=`trim_trailing_whitespace "${t[13]}"`
    t_cabledetails=`trim_trailing_whitespace "${t[14]}"`

    rate=`trim_trailing_whitespace "${t[15]}"`

    if [ "$rate" == "" ]; then
      rate=$RATE_DEFAULT
    fi
    if [ `cvt_nodetype "$t_srctype"` == "$NODETYPE_HFI" ]; then
      MTU_SW_HFI=`trim_trailing_whitespace "${t[16]}"`

      if [ "$MTU_SW_HFI" == "" ]; then
       MTU_SW_HFI=$MTU_SW_HFI_DEFAULT
      fi

      # Make sure MTU is numerical value
      if [[ "${MTU_SW_HFI//[[:digit:]]/}" != "" ]]; then
        echo "Error MTU is non-numerical \"$MTU_SW_HFI\"" >&2
        exit 2
      fi

    else
      MTU_SW_SW=`trim_trailing_whitespace "${t[16]}"`

      if [ "$MTU_SW_SW" == "" ]; then
       MTU_SW_SW=$MTU_SW_SW_DEFAULT
      fi

      # Make sure MTU is numerical value
      if [[ "${MTU_SW_SW//[[:digit:]]/}" != "" ]]; then
        echo "Error MTU is non-numerical \"$MTU_SW_SW\"" >&2
        exit 2
      fi

    fi
    t_linkdetails=`trim_trailing_whitespace "${t[17]}"`

    if [ "$t_srctype" == "$NODETYPE_SPINE" ]
      then
      internal="1"
    else
      internal="0"
    fi

    # Process group, rack and switch names
    if [ -n "$t_srcname" ]
      then
      proc_group "$t_srcgroup"
      ix_srcgroup=$functout
      proc_rack "$t_srcrack" "${tb_group[$ix_srcgroup]}"
      ix_srcrack=$functout
      proc_group "$t_dstgroup"
      ix_dstgroup=$functout
      proc_rack "$t_dstrack" "${tb_group[$ix_dstgroup]}"
      ix_dstrack=$functout
      if [ "$t_srctype" == "$NODETYPE_EDGE" ]
        then
        proc_switch "$t_srcname" "${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}"
        ix_srcswitch=$functout
      else
        ix_srcswitch=0
      fi
      if [ "$t_dsttype" == "$NODETYPE_EDGE" ]
        then
        proc_switch "$t_dstname" "${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}"
        ix_dstswitch=$functout
      else
        ix_dstswitch=0
      fi

      # Form consolidated output data
      nodedesc1="${t_srcname}"
      if [ "$t_srctype" == "$NODETYPE_HFI" ]
        then
        if ! [[ "$nodedesc1" =~ $HOST_HFI_REGEX ]]; then
          nodedesc1="${nodedesc1}${CAT_CHAR}${hfi_suffix}"
        fi
        nodedetails1="${t_srcname2}"
        hfi+=("$t_srcname")
      else
        nodedetails1=""
        if [ "$t_srctype" != "$NODETYPE_EDGE" ]
          then
          nodedesc1="${nodedesc1}${CAT_CHAR}${t_srcname2}"
        fi
      fi

      nodedesc2="${t_dstname}"
      if [ "$t_dsttype" != "$NODETYPE_EDGE" ]
        then
        nodedesc2="${nodedesc2}${CAT_CHAR}${t_dstname2}"
      fi

      nodetype1=`cvt_nodetype "$t_srctype"`
      if [ -z "$nodetype1" ]
      then
        echo "NodeType of "$t_srctype" is not valid. Valid types are FI, SW, CL, CS" >&2 
        usage_full "2"
      fi

      nodetype2=`cvt_nodetype "$t_dsttype"`
      if [ -z "$nodetype2" ]
      then
        echo "NodeType of "$t_dsttype" is not valid. Valid types are FI, SW, CL, CS" >&2
        usage_full "2"
      fi

      # Validate sources and destinations
      if [ "$t_dsttype" == "$NODETYPE_HFI" ]; then
        echo "Error: HFIs cannot be destination nodes (line:$ix_line)" >&2
        usage_full "2"
      fi

      if [[ "$t_srctype" == "$NODETYPE_HFI" ]] && [[ "$t_dsttype" != "$NODETYPE_EDGE" && "$t_dsttype" != "$NODETYPE_LEAF" ]]; then
          echo "Error: HFIs must connect to Edge/Leaf Switches (line:$ix_line)" >&2
          usage_full "2"
      fi

      if [ "$t_srctype" != "$NODETYPE_LEAF" ] && [ "$t_dsttype" == "$NODETYPE_SPINE" ]; then
        echo "Error: Only Leaf switches can connect to Spine switches (line:$ix_line)" >&2
        usage_full "2"
      fi

      if [ "$t_srctype" == "$NODETYPE_SPINE" ]; then
        echo "Error: Spine switches cannot be source nodes (line:$ix_line)" >&2
        usage_full "2"
      fi

      # Add miscellaneous content to lookup table (keyed by node desc and node type)
      # This will be used in XML output generation

      add_fields_to_misc_table "$nodedesc1" "$t_srctype"

      if [ "$nodetype1" == "$NODETYPE_SWITCH" ]; then
        # Add Source Port to PortList if Source Node is Switch
        add_portnum_to_switchportlist "$t_srcport" "$nodedesc1" "$internal"
      fi

      if [ "$nodetype2" == "$NODETYPE_SWITCH" ]; then
        # Add Destination Port to PortList if Destination Node is Switch
        add_portnum_to_switchportlist "$t_dstport" "$nodedesc2" "$internal"
      fi

      # Output CSV FILE_LINKSUM
      if [ $nodetype2 != "$NODETYPE_ENDPOINT" ]; then
        if [ $nodetype1 == "$NODETYPE_SWITCH" ] && [ $nodetype2 == "$NODETYPE_SWITCH" ]; then
          link="${rate};${MTU_SW_SW};${t_linkdetails};${internal};${t_cablelength};${t_cablelabel};${t_cabledetails};${t_srcport};${nodetype1};${nodedesc1};${t_dstport};${nodetype2};${nodedesc2}"
        else
          link="${rate};${MTU_SW_HFI};${t_linkdetails};${internal};${t_cablelength};${t_cablelabel};${t_cabledetails};${t_srcport};${nodetype1};${nodedesc1};${t_dstport};${nodetype2};${nodedesc2}"
        fi
        echo "${link}" >> ${FILE_LINKSUM}
      fi

      if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]
        then
        echo "${link}" >> ${tb_group[$ix_srcgroup]}${FILE_LINKSUM}
        if [ $ix_dstgroup != $ix_srcgroup ]
          then
          echo "${link}" >> ${tb_group[$ix_dstgroup]}${FILE_LINKSUM}
        fi
      fi

      if [ $((n_detail & OUTPUT_RACKS)) != 0 ]
        then
        echo "${link}" >> ${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}${FILE_LINKSUM}
        if [ $ix_dstrack != $ix_srcrack ]
          then
          echo "${link}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${FILE_LINKSUM}
        fi
      fi

      if [ $((n_detail & OUTPUT_SWITCHES)) != 0 ]
        then
        if [ "$t_srctype" == "$NODETYPE_EDGE" ]
          then
          echo "${link}" >> ${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}${tb_switch[$ix_srcswitch]}${FILE_LINKSUM}
        fi
        if [ "$t_dsttype" == "$NODETYPE_EDGE" ]
          then
          echo "${link}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${tb_switch[$ix_dstswitch]}${FILE_LINKSUM}
        fi
      fi

      # Output CSV FILE_LINKSUM_NOCORE
      if [ "$t_dsttype" != "$NODETYPE_LEAF" ]
        then
        echo "${link}" >> ${FILE_LINKSUM_NOCORE}
        if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]
          then
          echo "${link}" >> ${tb_group[$ix_srcgroup]}${FILE_LINKSUM_NOCORE}
          if [ $ix_dstgroup != $ix_srcgroup ]
            then
            echo "${link}" >> ${tb_group[$ix_dstgroup]}${FILE_LINKSUM_NOCORE}
          fi
        fi

        if [ $((n_detail & OUTPUT_RACKS)) != 0 ]
          then
          echo "${link}" >> ${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}${FILE_LINKSUM_NOCORE}
          if [ $ix_dstrack != $ix_srcrack ]
            then
            echo "${link}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${FILE_LINKSUM_NOCORE}
          fi
        fi

        if [ $((n_detail & OUTPUT_SWITCHES)) != 0 ]
          then
          if [ "$t_srctype" == "$NODETYPE_EDGE" ]
            then
            echo "${link}" >> ${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}${tb_switch[$ix_srcswitch]}${FILE_LINKSUM_NOCORE}
          fi
          if [ "$t_dsttype" == "$NODETYPE_EDGE" ]
            then
            echo "${link}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${tb_switch[$ix_dstswitch]}${FILE_LINKSUM_NOCORE}
          fi
        fi
      fi

      # Output CSV FILE_LINKSUM_NOCABLE
      #  Note: output only needed at top-level to bootstrap FILE_LINKSUM_SWD06
      #  and FILE_LINKSUM_SWD24
      if [ "$t_srctype" == "$NODETYPE_SPINE" ]
        then
        echo "${link}" >> ${FILE_LINKSUM_NOCABLE}
      fi

      # Output CSV nodedesc1
      if [ "$t_srctype" == "$NODETYPE_HFI" ]
        then
        echo "${nodedesc1};${nodedetails1}" >> ${FILE_NODEFIS}
        if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]
          then
          echo "${nodedesc1};${nodedetails1}" >> ${tb_group[$ix_srcgroup]}${FILE_NODEFIS}
        fi

        if [ $((n_detail & OUTPUT_RACKS)) != 0 ]
          then
          echo "${nodedesc1};${nodedetails1}" >> ${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}${FILE_NODEFIS}
        fi

        if [ $((n_detail & OUTPUT_SWITCHES)) != 0 ]
          then
          if [ "$t_dsttype" == "$NODETYPE_EDGE" ]
            then
            if [ "x${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}" == "x${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}" ]
              then
              echo "${nodedesc1};${nodedetails1}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${tb_switch[$ix_dstswitch]}${FILE_NODEFIS}
            fi
          fi
        fi
      else
        echo "${nodedesc1}" >> ${FILE_NODESWITCHES}
        if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]
          then
          echo "${nodedesc1}" >> ${tb_group[$ix_srcgroup]}${FILE_NODESWITCHES}
        fi

        if [ $((n_detail & OUTPUT_RACKS)) != 0 ]
          then
          echo "${nodedesc1}" >> ${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}${FILE_NODESWITCHES}
        fi

        if [ $((n_detail & OUTPUT_SWITCHES)) != 0 ]
          then
          echo "${nodedesc1}" >> ${tb_group[$ix_srcgroup]}${tb_rack[$ix_srcrack]}${tb_switch[$ix_srcswitch]}${FILE_NODESWITCHES}
        fi
      fi

      # Output CSV nodedesc2
      if [ "$nodetype2" != "$NODETYPE_ENDPOINT" ]; then
        echo "${nodedesc2}" >> ${FILE_NODESWITCHES}
      fi
      if [ "$t_dsttype" == "$NODETYPE_LEAF" ]
        then
        echo "${nodedesc2}" >> ${FILE_NODELEAVES}
        echo "${t_dstname}" >> ${FILE_NODECHASSIS}
        external_leaves+=("`echo "${nodedesc2}" | sed -e 's/[AB]$//'`")
      fi
      if [ $((n_detail & OUTPUT_GROUPS)) != 0 ] && [ "$nodetype2" != "$NODETYPE_ENDPOINT" ]
        then
        echo "${nodedesc2}" >> ${tb_group[$ix_dstgroup]}${FILE_NODESWITCHES}
        if [ "$t_dsttype" == "$NODETYPE_LEAF" ]
          then
          echo "${nodedesc2}" >> ${tb_group[$ix_dstgroup]}${FILE_NODELEAVES}
          echo "${t_dstname}" >> ${tb_group[$ix_dstgroup]}${FILE_NODECHASSIS}
        fi
      fi

      if [ $((n_detail & OUTPUT_RACKS)) != 0 ] && [ "$nodetype2" != "$NODETYPE_ENDPOINT" ]
        then
        echo "${nodedesc2}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${FILE_NODESWITCHES}
        if [ "$t_dsttype" == "$NODETYPE_LEAF" ]
          then
          echo "${nodedesc2}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${FILE_NODELEAVES}
          echo "${t_dstname}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${FILE_NODECHASSIS}
        fi
      fi

      if [ $((n_detail & OUTPUT_SWITCHES)) != 0 ] && [ "$nodetype2" != "$NODETYPE_ENDPOINT" ]
        then
        if [ "$t_dsttype" == "$NODETYPE_EDGE" ]
          then
          echo "${nodedesc2}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${tb_switch[$ix_dstswitch]}${FILE_NODESWITCHES}
        elif [ "$t_dsttype" == "$NODETYPE_LEAF" ]
          then
          echo "${nodedesc2}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${tb_switch[$ix_dstswitch]}${FILE_NODELEAVES}
          echo "${t_dstname}" >> ${tb_group[$ix_dstgroup]}${tb_rack[$ix_dstrack]}${tb_switch[$ix_dstswitch]}${FILE_NODECHASSIS}
        fi
      fi

    # End of link information
    else
      cts_parse=2
    fi
    ;;

    # Process core switch data
  2)
    if echo "${t[0]}" | grep -e "$CORE_NAME" > /dev/null 2>&1
      then
      core_name=`echo "${t[0]}" | cut -d ':' -f 2`
      # Make sure that core name do not have space char
      if [[ "$core_name" =~ [[:space:]] ]]
        then
        echo "opaxlattopology: Error - core name cannot have space char: $core_name" >&2
        exit 2
      fi
      core+=("$core_name")
      display_progress "Generating links for Core:$core_name"
      if echo "${t[1]}" | grep -e "$CORE_GROUP" > /dev/null 2>&1
        then
        core_group=`echo "${t[1]}" | cut -d ':' -f 2`
      elif [ $((n_detail & OUTPUT_GROUPS)) != 0 ]
        then
        echo "Must have rack group name when outputting rack group (line:$ix_line)"
        usage_full "2"
      else
        core_group=""
      fi
      # Make sure that core group do not have space char
      if [[ "$core_group" =~ [[:space:]] ]]
        then
        echo "opaxlattopology: Error - core group name cannot have space char: $core_group" >&2
        exit 2
      fi
      # Store core group
      core_name_group["$core_name"]="$core_group"
      if echo "${t[2]}" | grep -e "$CORE_RACK" > /dev/null 2>&1
        then
        core_rack=`echo "${t[2]}" | cut -d ':' -f 2`
      elif [ $((n_detail & OUTPUT_RACKS)) != 0 ]
        then
        echo "Must have rack name when outputting rack (line:$ix_line)"
        usage_full "2"
      else
        core_rack=""
      fi
      # Make sure that core rack do not have space char
      if [[ "$core_rack" =~ [[:space:]] ]]
        then
        echo "opaxlattopology: Error - core rack name cannot have space char: $core_rack" >&2
        exit 2
      fi
      # Store core rack
      core_name_rack["$core_name"]="$core_rack"
      if echo "${t[3]}" | grep -e "$CORE_SIZE" > /dev/null 2>&1
        then
        core_size=`echo ${t[3]} | cut -d ':' -f 2`
        if [ $core_size -ne 288 -a $core_size -ne 1152 ]
          then
          echo "Invalid $CORE_SIZE parameter (${t[3]})"
          usage_full "2"
        fi
      else
        echo "No $CORE_SIZE parameter"
        usage_full "2"
      fi
      # Store core size
      core_name_size["$core_name"]="$core_size"
      if echo "${t[4]}" | grep -e "$CORE_FULL" > /dev/null 2>&1
        then
        core_full=`echo ${t[4]} | cut -d ':' -f 2`
        if [ $core_full -ne 0 -a $core_full -ne 1 ]
          then
          echo "Invalid $CORE_FULL parameter (${t[4]})"
          usage_full "2"
        fi
      else
        core_full=0
      fi

      # Output CSV FILE_LINKSUM_NOCABLE
      if [ $core_full == 1 ]
        then
        leaves=""
      else
        partially_populated_core+=($core_name)
        leaves=`cat $FILE_NODELEAVES | tr '\012' '|' | sed -e 's/|$//'`
      fi
      if [ $core_size == 288 ]
        then
	generate_linksum_nocable $FILE_LINKSUM_SWD06
      else
	generate_linksum_nocable $FILE_LINKSUM_SWD24
      fi
      cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 10 | sort -u >> ${FILE_NODESWITCHES}
      cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | sort -u >> ${FILE_NODESWITCHES}
      cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | cut -d "$CAT_CHAR" -f 1 | sort -u >> ${FILE_NODECHASSIS}

      if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]
        then
        if [ $core_full == 1 ]
          then
          leaves=""
        else
          leaves=`cat $core_group/$FILE_NODELEAVES | tr '\012' '|' | sed -e 's/|$//'`
        fi
        cp ${FILE_LINKSUM_NOCABLE} "$core_group"/${FILE_LINKSUM_NOCABLE}
        cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 10 | sort -u >> "$core_group"/${FILE_NODESWITCHES}
        cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | sort -u >> "$core_group"/${FILE_NODESWITCHES}
        cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | cut -d "$CAT_CHAR" -f 1 | sort -u >> "$core_group"/${FILE_NODECHASSIS}
      fi

      if [ $((n_detail & OUTPUT_RACKS)) != 0 ]
        then
        if [ $core_full == 1 ]
          then
          leaves=""
        else
          leaves=`cat "$core_group"/"$core_rack"/$FILE_NODELEAVES | tr '\012' '|' | sed -e 's/|$//'`
        fi
        cp ${FILE_LINKSUM_NOCABLE} "$core_group"/"$core_rack"/${FILE_LINKSUM_NOCABLE}
        cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 10 | sort -u >> "$core_group"/"$core_rack"/${FILE_NODESWITCHES}
        cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | sort -u >> "$core_group"/"$core_rack"/${FILE_NODESWITCHES}
        cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | cut -d "$CAT_CHAR" -f 1 | sort -u >> "$core_group"/"$core_rack"/${FILE_NODECHASSIS}
      fi

    # End of core switch information
    elif [ "${t[0]}" == "SM" ]
      then
      # Process SM section
      process_sm
    else
      cts_parse=3
    fi
    ;;

    # Finding "Present Leaves" or "Omitted Spine" or "SM" tag. Process if "SM" tag is found
  3)
    if [ "${t[0]}" == "Present Leafs" ] || [ "${t[0]}" == "Present Leaves" ]
      then
      cts_parse=4
    elif [ "${t[0]}" == "Omitted Spines" ]
      then
      cts_parse=5
    elif [ "${t[0]}" == "SM" ]
      then
      # Process SM section
      process_sm
    else
      cts_parse=3
    fi
    ;;

    # Process Present Leaves
  4)
    if echo "${t[0]}" | grep -e "$CORE_NAME" > /dev/null 2>&1
      then
      core_name=`echo ${t[0]} | cut -d ':' -f 2`
      if ! array_contains core[@] $core_name
        then
        echo "opaxlattopology: Error - No Core details found for $core_name, specified in \"Present Leaves\" section">&2
        exit 2
      fi
      if array_contains partially_populated_core[@] $core_name
        then
        display_progress "Processing Leaves of partially populated Core:$core_name"
        # add external leaves to the leaf_array
        for element in "${external_leaves[@]}"
        do
          if [[ "$element" =~ "$core_name"" "L[0-9]+$ ]]
            then
            leaf_array+=("$element")
          fi
        done
        for element in "${t[@]:1}"
        do
          if [ -n "$element" ]; then
            leaf_array+=("$core_name ${element}")
          fi
        done
        # Adding present leaves to temporary files
        if [ ${#leaf_array[@]} != 0 ]
          then
          leaves=""
          for element in "${leaf_array[@]}"
          do
            leaves="$leaves$element|"
          done
          leaves=`echo $leaves | sed -e 's/|$//'`
          if [ ${core_name_size["$core_name"]} == 288 ]
            then
            generate_linksum_nocable $FILE_LINKSUM_SWD06
          else
            generate_linksum_nocable $FILE_LINKSUM_SWD24
          fi
          cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 10 | sort -u >> ${FILE_NODESWITCHES}
          cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | sort -u >> ${FILE_NODESWITCHES}
          if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]
            then
            cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 10 | sort -u >> "${core_name_group["$core_name"]}"/${FILE_NODESWITCHES}
            cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | sort -u >> "$core_group"/${FILE_NODESWITCHES}
          fi
          if [ $((n_detail & OUTPUT_RACKS)) != 0 ]
            then
            cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 10 | sort -u >> "${core_name_group["$core_name"]}"/"${core_name_rack["$core_name"]}"/${FILE_NODESWITCHES}
            cat ${FILE_LINKSUM_NOCABLE} | cut -d ';' -f 13 | sort -u >> "${core_name_group["$core_name"]}"/"${core_name_rack["$core_name"]}"/${FILE_NODESWITCHES}
          fi
        fi
        leaf_array=()
      else
          echo "opaxlattopology: Warning - Listed \"Present Leaves\" for Fully Populated Core. Ignoring this row"
      fi
    # End of Present Leaves information
    else
        cts_parse=3
    fi
   ;;

    # Process Omited Spine
  5)
    if echo "${t[0]}" | grep -e "$CORE_NAME" > /dev/null 2>&1
      then
      core_name=`echo ${t[0]} | cut -d ':' -f 2`
      if ! array_contains core[@] $core_name
        then
        echo "opaxlattopology: Error - No Core details found for $core_name, specified in \"Omitted Spines\" section" >&2
        exit 2
      fi
      if array_contains partially_populated_core[@] $core_name
        then
        display_progress "Processing spines of partially populated Core:$core_name"
        for element in "${t[@]:1}"
        do
          if [ -n "$element" ]; then
            spine_array+=("$core_name ${element}")
          fi
        done
        if [ ${#spine_array[@]} != 0 ]
          then
          for element in "${spine_array[@]}"
          do
            # Remove Spine from FILE_LINKSUM_NOCABLE and FILE_NODESWITCHES
            sed -i "/$element/d" ${FILE_LINKSUM_NOCABLE}
            sed -i "/$element/d" ${FILE_NODESWITCHES}
            if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]
              then
              sed -i "/$element/d" ${core_name_group["$core_name"]}/${FILE_LINKSUM_NOCABLE}
              sed -i "/$element/d" ${core_name_group["$core_name"]}/${FILE_NODESWITCHES}
            fi
            if [ $((n_detail & OUTPUT_RACKS)) != 0 ]
              then
              sed -i "/$element/d" ${core_name_group["$core_name"]}/${core_name_rack["$core_name"]}/${FILE_LINKSUM_NOCABLE}
              sed -i "/$element/d" ${core_name_group["$core_name"]}/${core_name_rack["$core_name"]}/${FILE_NODESWITCHES}
            fi
          done
        fi
        spine_array=()
      else
        echo "opaxlattopology: Warning - Listed \"Omitted Spines\" for Fully Populated Core. Ignoring this row"
      fi
    else
      cts_parse=3
    fi
   ;;

  esac  # end of case $cts_parse in
done < <( cat $FILE_TOPOLOGY_LINKS | tr -d '\015' )  # End of while read ... do

# Generate topology file(s)
display_progress "Generating $FILE_TOPOLOGY_OUT file(s)"

# Generate top-level topology file
gen_topology "$fl_output_edge_leaf" "$fl_output_spine_leaf"

if [ "$n_detail" != "0" ]; then
  # iterate through groups
  for (( iy=1 ; $iy<$group_cnt ; iy=$((iy+1)) )); do

    if [ -d "${tb_group[$iy]}" ]; then
      cd ${tb_group[$iy]}

      if [ $((n_detail & OUTPUT_GROUPS)) != 0 ]; then
        gen_topology "$fl_output_edge_leaf" "$fl_output_spine_leaf"
      fi

      # iterate through racks
      for (( ix=1 ; $ix<$rack_cnt ; ix=$((ix+1)) )); do
        if [ -d "${tb_rack[$ix]}" ]; then
          cd ${tb_rack[$ix]}

          if [ $((n_detail & OUTPUT_RACKS)) != 0 ]; then
            gen_topology "$fl_output_edge_leaf" "$fl_output_spine_leaf"
          fi

          # iterate through switches
          if [ $((n_detail & OUTPUT_SWITCHES)) != 0 ]; then
            for switch in *; do
              if [ -d $switch ]; then
                cd $switch
                gen_topology "$fl_output_edge_leaf" "$fl_output_spine_leaf"
                cd ..
              fi # switch directory create
            done  # next switch
          fi # switch detail

          cd .. # go up to group directory
        fi # end rack topology create
      done # next rack
      cd .. # go up to base directory
    fi # rack detail
  done # next group

fi # detail output

display_progress "Done"
exit 0
