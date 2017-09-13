/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#define _GNU_SOURCE
#include <getopt.h>

#include <iba/ibt.h>
#include <opamgt_sa_priv.h>
#include <ibprint.h>
#include <stl_print.h>
#include <infiniband/umad.h>

#define MAX_NUM_INPUT_ARGS 17

uint8           g_IB            = 0;	// perform query in legacy InfiniBand format
uint8           g_verbose       = 0;
int				g_exitstatus	= 0;
int				g_CSV			= 0;	// output should use CSV style
										// only valid for OutputTypeVfInfoRecord
										// 1=absolute value for mtu and rate
										// 2=enum value for mtu and rate

#ifdef DBGPRINT
#undef DBGPRINT
#endif
#define DBGPRINT(format, args...) \
	do { if (g_verbose) { fflush(stdout); fprintf(stderr, format, ##args); } } while (0)

static struct omgt_port *sa_omgt_session;
PrintDest_t g_dest;
PrintDest_t g_dbgDest;

// perform the given query and display the results
// if portGuid is -1, 1st active port is used to issue query
//void do_query(EUI64 portGuid, QUERY *pQuery)
void do_query(struct omgt_port *port, OMGT_QUERY *pQuery, QUERY_INPUT_VALUE *old_input_value)
{
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	IB_GID local_gid;
	DBGPRINT("Query: Input=%s (0x%x), Output=%s (0x%x)\n",
				   		iba_sd_query_input_type_msg(pQuery->InputType),
						pQuery->InputType,
					   	iba_sd_query_result_type_msg(pQuery->OutputType),
						pQuery->OutputType);

	(void)omgt_port_get_port_guid(port, &local_gid.Type.Global.InterfaceID);
	(void)omgt_port_get_port_prefix(port, &local_gid.Type.Global.SubnetPrefix);
	status = omgt_input_value_conversion(pQuery, old_input_value, local_gid);
	if (status) {
		fprintf(stderr, "%s: Error: InputValue conversion failed: Status %u\n"
			"Query: Input=%s (0x%x), Output=%s (0x%x)\n"
			"SourceGid: 0x%.16"PRIx64":%.16"PRIx64"\n", __func__, status,
			iba_sd_query_input_type_msg(pQuery->InputType), pQuery->InputType,
			iba_sd_query_result_type_msg(pQuery->OutputType), pQuery->OutputType,
			local_gid.AsReg64s.H, local_gid.AsReg64s.L);
		if (status == FERROR) {
			fprintf(stderr, "Source Gid must be supplied for Input/Output combination.\n");
		}
		return;
	}
	// this call is synchronous
	status = omgt_query_sa(port, pQuery, &pQueryResults);

	g_exitstatus = PrintQueryResult(&g_dest, 0, &g_dbgDest,
			   		pQuery->OutputType, g_CSV, status, pQueryResults);

	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
}

// command line options, each has a short and long flag name
struct option options[] = {
		// basic controls
		{ "verbose", no_argument, NULL, 'v' },
		{ "IB", no_argument, NULL, 'I' },
		{ "hfi", required_argument, NULL, 'h' },
		{ "port", required_argument, NULL, 'p' },

		// input data
		{ "lid", required_argument, NULL, 'l' },
		{ "pkey", required_argument, NULL, 'k' },
		{ "vfindex", required_argument, NULL, 'i' },
		{ "serviceId", required_argument, NULL, 'S' },
		{ "SL", required_argument, NULL, 'L' },
		{ "type", required_argument, NULL, 't' },
		{ "sysguid", required_argument, NULL, 's' },
		{ "nodeguid", required_argument, NULL, 'n' },
		{ "portguid", required_argument, NULL, 'g' },
		{ "portgid", required_argument, NULL, 'u' },
		{ "mcgid", required_argument, NULL, 'm' },
		{ "desc", required_argument, NULL, 'd' },
		{ "guidpair", required_argument, NULL, 'P' },
		{ "gidpair", required_argument, NULL, 'G' },
		{ "guidlist", required_argument, NULL, 'a' },
		{ "gidlist", required_argument, NULL, 'A' },

		// output type
		{ "output", required_argument, NULL, 'o' },
		{ "help", no_argument, NULL, '$' },	// use an invalid option character
		{ 0 }
};

void Usage_full(void)
{
	fprintf(stderr, "Usage: opasaquery [basic options] [SA options] -o type \n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opasaquery --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Basic Options:\n");
	fprintf(stderr, "    -v/--verbose              - verbose output. A second invocation will\n"
                    "                                activate openib debugging, a 3rd will\n"
 					"                                activate libibumad debugging.\n");
	fprintf(stderr, "    -I/--IB                   - issue query in legacy InfiniBand format\n");
	fprintf(stderr, "    -h/--hfi hfi              - hfi, numbered 1..n, 0= -p port will be a\n");
	fprintf(stderr, "                                system wide port num (default is 0)\n");
	fprintf(stderr, "    -p/--port port            - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "                                is 1st active)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "SA Options:\n");
	fprintf(stderr, "    -l/--lid lid              - query a specific lid\n");
	fprintf(stderr, "    -k/--pkey pkey            - query a specific pkey\n");
	fprintf(stderr, "    -i/--vfindex vfindex      - query a specific vfindex\n");
	fprintf(stderr, "    -S/--serviceId serviceId  - query a specific service id\n");
	fprintf(stderr, "    -L/--SL SL                - query by service level\n");
	fprintf(stderr, "    -t/--type nodetype        - query by node type\n");
	fprintf(stderr, "    -s/--sysguid guid         - query by system image guid\n");
	fprintf(stderr, "    -n/--nodeguid guid        - query by node guid\n");
	fprintf(stderr, "    -g/--portguid guid        - query by port guid\n");
	fprintf(stderr, "    -u/--portgid gid          - query by port gid\n");
	fprintf(stderr, "    -m/--mcgid gid            - query by multicast gid\n");
	fprintf(stderr, "    -d/--desc name            - query by node name/description\n");
	fprintf(stderr, "    -P/--guidpair 'guid guid' - query by a pair of port Guids\n");
	fprintf(stderr, "    -G/--gidpair 'gid gid'    - query by a pair of Gids\n");
	fprintf(stderr, "    -a/--guidlist 'sguid ...;dguid ...' - query by a list of port Guids\n");
	fprintf(stderr, "    -A/--gidlist 'sgid ...;dgid ...'    - query by a list of Gids\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "nodetype:\n");
	fprintf(stderr, "    fi  - fabric interface\n");
	fprintf(stderr, "    sw  - switch\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "gids:\n");
	fprintf(stderr, "   specify 64 bit subnet and 64 bit interface id as:\n");
	fprintf(stderr, "   	subnet:interface\n");
	fprintf(stderr, "   such as: 0xfe80000000000000:0x00117501a0000380\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "type: (default is node)\n");
	fprintf(stderr, "    classportinfo - classportinfo of the SA\n");
	fprintf(stderr, "    systemguid    - list of system image guids\n");
	fprintf(stderr, "    nodeguid      - list of node guids\n");
	fprintf(stderr, "    portguid      - list of port guids\n");
	fprintf(stderr, "    lid           - list of lids\n");
	//fprintf(stderr, "    gid         - list of gids\n");
	fprintf(stderr, "    desc          - list of node descriptions/names\n");
	fprintf(stderr, "    path          - list of path records\n");
	fprintf(stderr, "    node          - list of node records\n");
	fprintf(stderr, "    portinfo      - list of port info records\n");
	fprintf(stderr, "    sminfo        - list of SM info records\n");
	fprintf(stderr, "    swinfo        - list of switch info records\n");
	fprintf(stderr, "    link          - list of link records\n");
	fprintf(stderr, "    scsc          - list of SC to SC mapping table records\n");
	fprintf(stderr, "    slsc          - list of SL to SC mapping table records\n");
	fprintf(stderr, "    scsl          - list of SC to SL mapping table records\n");
	fprintf(stderr, "    scvlt         - list of SC to VLt table records\n");
	fprintf(stderr, "    scvlnt        - list of SC to VLnt table records\n");
	fprintf(stderr, "    vlarb         - list of VL arbitration table records\n");
	fprintf(stderr, "    pkey          - list of P-Key table records\n");
	fprintf(stderr, "    service       - list of service records\n");
	fprintf(stderr, "    mcmember      - list of multicast member records\n");
	fprintf(stderr, "    inform        - list of inform info records\n");
	fprintf(stderr, "    linfdb        - list of switch linear FDB records\n");
	fprintf(stderr, "    mcfdb         - list of switch multicast FDB records\n");
	fprintf(stderr, "    trace         - list of trace records\n");
	fprintf(stderr, "    vfinfo        - list of vFabrics\n");
	fprintf(stderr, "    vfinfocsv     - list of vFabrics in CSV format\n");
	fprintf(stderr, "    vfinfocsv2    - list of vFabrics in CSV format with enums\n");
	fprintf(stderr, "    fabricinfo    - summary of fabric devices\n");
	fprintf(stderr, "    quarantine    - list of quarantined nodes\n");
	fprintf(stderr, "    conginfo      - list of Congestion Info Records\n");
	fprintf(stderr, "    swcongset     - list of Switch Congestion Settings\n");
	fprintf(stderr, "    swportcong    - list of Switch Port Congestion Settings\n");
	fprintf(stderr, "    hficongset    - list of HFI Congestion Settings\n");
	fprintf(stderr, "    hficongcon    - list of HFI Congesting Control Settings\n");
	fprintf(stderr, "    bfrctrl       - list of buffer control tables\n");
	fprintf(stderr ,"    cableinfo     - list of Cable Info records\n");
	fprintf(stderr ,"    portgroup     - list of AR Port Group records\n");
	fprintf(stderr ,"    portgroupfdb  - list of AR Port Group FWD records\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage examples:\n");
	fprintf(stderr, "    opasaquery -o desc -t fi\n");
	fprintf(stderr, "    opasaquery -o portinfo -l 2\n");
	fprintf(stderr, "    opasaquery -o sminfo\n");
	fprintf(stderr, "    opasaquery -o pkey\n");

	exit(0);
}

void Usage(void)
{
	fprintf(stderr, "Usage: opasaquery [-v] [-o type] [-l lid] [-t nodetype] [-s guid] [-n guid]\n");
	fprintf(stderr, "                  [-k pkey] [-g guid] [-u gid] [-m gid] [-d name]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opasaquery --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -l/--lid lid              - query a specific lid\n");
	fprintf(stderr, "    -k/--pkey pkey            - query a specific pkey\n");
	fprintf(stderr, "    -t/--type nodetype        - query by node type\n");
	fprintf(stderr, "    -s/--sysguid guid         - query by system image guid\n");
	fprintf(stderr, "    -n/--nodeguid guid        - query by node guid\n");
	fprintf(stderr, "    -g/--portguid guid        - query by port guid\n");
	fprintf(stderr, "    -m/--mcgid gid            - query by multicast gid\n");
	fprintf(stderr, "    -d/--desc name            - query by node name/description\n");
	fprintf(stderr, "    -o/--output type          - output type for query (default is node)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "nodetype:\n");
	fprintf(stderr, "    fi  - fabric interface\n");
	fprintf(stderr, "    sw  - switch\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "gids:\n");
	fprintf(stderr, "   specify 64 bit subnet and 64 bit interface id as:\n");
	fprintf(stderr, "   	subnet:interface\n");
	fprintf(stderr, "   such as: 0xfe80000000000000:0x00117501a0000380\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Output types (abridged):\n");
	fprintf(stderr, "    systemguid  - list of system image guids\n");
	fprintf(stderr, "    nodeguid    - list of node guids\n");
	fprintf(stderr, "    portguid    - list of port guids\n");
	fprintf(stderr, "    lid         - list of lids\n");
	//fprintf(stderr, "    gid         - list of gids\n");
	fprintf(stderr, "    desc        - list of node descriptions/names\n");
	fprintf(stderr, "    node        - list of node records\n");
	fprintf(stderr, "    portinfo    - list of port info records\n");
	fprintf(stderr, "    sminfo      - list of SM info records\n");
	fprintf(stderr, "    swinfo      - list of switch info records\n");
	fprintf(stderr, "    service     - list of service records\n");
	fprintf(stderr, "    mcmember    - list of multicast member records\n");
	fprintf(stderr, "    vfinfo      - list of vFabrics\n");
	fprintf(stderr, "    vfinfocsv   - list of vFabrics in CSV format\n");
	fprintf(stderr, "    vfinfocsv2  - list of vFabrics in CSV format\n");
	fprintf(stderr, "    bfrctrl     - list of buffer control tables\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage examples:\n");
	fprintf(stderr, "    opasaquery -o desc -t fi\n");
	fprintf(stderr, "    opasaquery -o portinfo -l 2\n");
	fprintf(stderr, "    opasaquery -o sminfo\n");
	exit(2);
}

// convert a node type argument to the proper constant
NODE_TYPE checkNodeType(const char* name)
{
	if (0 == strcasecmp(optarg, "fi")) {
		return STL_NODE_FI;
	} else if (0 == strcasecmp(optarg, "sw")) {
		return STL_NODE_SW;
	} else {
		fprintf(stderr, "opasaquery: Invalid Node Type: %s\n", name);
		Usage();
		// NOTREACHED
		return STL_NODE_FI;
	}
}

void multiInputCheck(int inputType) {
	const char* typestr;
	switch (inputType) {
        case InputTypeNoInput:
			return;
        case InputTypePKey:
			typestr = "pkey";
			break;
        case InputTypeIndex:
			typestr = "index";
			break;
        case InputTypeServiceId:
			typestr = "serviceId";
			break;
        case InputTypeLid:
			typestr = "lid";
			break;
        case InputTypeSL:
			typestr = "SL";
			break;
        case InputTypeNodeType:
			typestr = "NodeType";
			break;
        case InputTypeSystemImageGuid:
			typestr = "SystemImageGuid";
			break;
        case InputTypeNodeGuid:
			typestr = "NodeGuid";
			break;
        case InputTypePortGuid:
			typestr = "PortGuid";
			break;
        case InputTypePortGid:
			typestr = "PortGid";
			break;
        case InputTypeMcGid:
			typestr = "McGid";
			break;
        case InputTypeNodeDesc:
			typestr = "NodeDesc";
			break;
        case InputTypePortGuidPair:
			typestr = "PortGuidPair";
			break;
        case InputTypeGidPair:
			typestr = "GidPair";
			break;
        case InputTypePortGuidList:
			typestr = "PortGuidList";
			break;
        case InputTypeGidList:
			typestr = "GidList";
			break;
        default:
			typestr = "parameter";
		}
	fprintf(stderr, "opasaquery: multiple selection criteria not supported, ignoring %s\n", typestr);
}


typedef struct InputFlags {
	QUERY_INPUT_TYPE flags[MAX_NUM_INPUT_ARGS];
} InputFlags_t;
//last element must be zero
InputFlags_t NodeInput = {{InputTypeNodeType,InputTypeLid,InputTypeSystemImageGuid,InputTypeNodeGuid,InputTypePortGuid,
		InputTypeNodeDesc,0,0,0,0,0,0,0,0,0,0,0}};
InputFlags_t PathInput = {{InputTypeLid,InputTypePKey,InputTypePortGuid,InputTypePortGid,InputTypePortGuidPair,
		InputTypeGidPair,InputTypeServiceId,InputTypeSL,InputTypePortGuidList,InputTypeGidList,0,0,0,0,0,0,0,}};
InputFlags_t ServiceInput = {{InputTypeLid,InputTypePortGuid,InputTypePortGid,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
InputFlags_t McmemberInput = {{InputTypeLid, InputTypePKey,InputTypePortGuid,InputTypePortGid,InputTypeMcGid,0,0,0,0,0,0,0,0,0,0,0,0}};
InputFlags_t InformInput = {{InputTypePortGuid,InputTypePortGid,InputTypeLid,0,0,0,0,0,0,0,0,0,0,0,0,0,0,}};
InputFlags_t TraceInput =  {{InputTypePortGuid,InputTypePortGid,InputTypePortGuidPair,InputTypeGidPair,0,0,0,0,0,0,0,0,0,0,0,0,0}};
InputFlags_t VfinfoInput = {{InputTypePKey,InputTypeIndex,InputTypeMcGid,InputTypeServiceId,InputTypeSL,InputTypeNodeDesc,0,0,0,0,0,0,0,0,0,0,0}};
InputFlags_t MiscInput = {{InputTypeLid,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

typedef struct OutputStringMap {
	char *string;
	QUERY_RESULT_TYPE stl_type;
	QUERY_RESULT_TYPE ib_type;
	InputFlags_t *valid_input_types;
	int csv;
} OutputStringMap_t;

#define NO_OUTPUT_TYPE 0xffff
OutputStringMap_t OutputTypeTable[] = {
//--input string--------StlOutputType-------------------------IbOutputType----------------InputFlags----csv-//
    {"systemguid",      OutputTypeStlSystemImageGuid,         NO_OUTPUT_TYPE,             &NodeInput, 0},
    {"classportinfo",   OutputTypeStlClassPortInfo,           OutputTypeClassPortInfo,    NULL, 0},
    {"nodeguid",        OutputTypeStlNodeGuid,                NO_OUTPUT_TYPE,             &NodeInput, 0},
    {"portguid",        OutputTypeStlPortGuid,                NO_OUTPUT_TYPE,             &NodeInput, 0},
    {"lid",             OutputTypeStlLid,                     NO_OUTPUT_TYPE,             &NodeInput, 0},
    {"desc",            OutputTypeStlNodeDesc,                NO_OUTPUT_TYPE,             &NodeInput, 0},
    {"path",            NO_OUTPUT_TYPE,                       OutputTypePathRecord,       &PathInput, 0},
    {"node",            OutputTypeStlNodeRecord,              OutputTypeNodeRecord,       &NodeInput, 0},
    {"portinfo",        OutputTypeStlPortInfoRecord,          OutputTypePortInfoRecord,   &MiscInput, 0},
    {"sminfo",          OutputTypeStlSMInfoRecord,            NO_OUTPUT_TYPE,             NULL, 0},
    {"link",            OutputTypeStlLinkRecord,              NO_OUTPUT_TYPE,             NULL, 0},
#ifndef NO_STL_SERVICE_OUTPUT       // Don't output STL Service if defined
    {"service",         OutputTypeStlServiceRecord,           OutputTypeServiceRecord,    &ServiceInput, 0},
#else
    {"service",         NO_OUTPUT_TYPE,                       OutputTypeServiceRecord,    &ServiceInput, 0},
#endif
#ifndef NO_STL_MCMEMBER_OUTPUT      // Don't output STL McMember (use IB format) if defined
    {"mcmember",        OutputTypeStlMcMemberRecord,          OutputTypeMcMemberRecord,   &McmemberInput, 0},
#else
    {"mcmember",        NO_OUTPUT_TYPE,                       OutputTypeMcMemberRecord,   &McmemberInput, 0},
#endif
    {"inform",          OutputTypeStlInformInfoRecord,        OutputTypeInformInfoRecord, &InformInput, 0},
    {"trace",           OutputTypeStlTraceRecord,             NO_OUTPUT_TYPE,             &TraceInput, 0},
    {"scsc",            OutputTypeStlSCSCTableRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"slsc",            OutputTypeStlSLSCTableRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"scsl",            OutputTypeStlSCSLTableRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"scvlt",           OutputTypeStlSCVLtTableRecord,        NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"scvlnt",          OutputTypeStlSCVLntTableRecord,       NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"swinfo",          OutputTypeStlSwitchInfoRecord,        NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"linfdb",          OutputTypeStlLinearFDBRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"mcfdb",           OutputTypeStlMCastFDBRecord,          NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"vlarb",           OutputTypeStlVLArbTableRecord,        NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"pkey",            OutputTypeStlPKeyTableRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"vfinfo",          OutputTypeStlVfInfoRecord,            NO_OUTPUT_TYPE,             &VfinfoInput, 0},
    {"vfinfocsv",       OutputTypeStlVfInfoRecord,            NO_OUTPUT_TYPE,             &VfinfoInput, 1},
    {"vfinfocsv2",      OutputTypeStlVfInfoRecord,            NO_OUTPUT_TYPE,             &VfinfoInput, 2},
    {"fabricinfo",      OutputTypeStlFabricInfoRecord,        NO_OUTPUT_TYPE,             NULL, 0},
    {"quarantine",      OutputTypeStlQuarantinedNodeRecord,   NO_OUTPUT_TYPE,             NULL, 0},
    {"conginfo",        OutputTypeStlCongInfoRecord,          NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"swcongset",       OutputTypeStlSwitchCongRecord,        NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"swportcong",      OutputTypeStlSwitchPortCongRecord,    NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"hficongset",      OutputTypeStlHFICongRecord,           NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"hficongcon",      OutputTypeStlHFICongCtrlRecord,       NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"bfrctrl",         OutputTypeStlBufCtrlTabRecord,        NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"cableinfo",       OutputTypeStlCableInfoRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"portgroup",       OutputTypeStlPortGroupRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"portgroupfdb",    OutputTypeStlPortGroupFwdRecord,      NO_OUTPUT_TYPE,             &MiscInput, 0},
    //Last entry must be null, insert new attributes above here!
    { NULL, NO_OUTPUT_TYPE, NO_OUTPUT_TYPE, NULL, 0 }
};

OutputStringMap_t *GetOutputTypeMap(const char* name) {

	int i =0;
	while(OutputTypeTable[i].string != NULL) {
		if (0 == strcmp(name, OutputTypeTable[i].string)) {
			return &OutputTypeTable[i];
		}
		i++;
	}

	fprintf(stderr, "opasaquery: Invalid Output Type: %s\n", name);
	Usage();
	// NOTREACHED
	return NULL;
}


// convert a output type argument to the proper constant
QUERY_RESULT_TYPE GetOutputType(OutputStringMap_t *in)
{
	QUERY_RESULT_TYPE res = NO_OUTPUT_TYPE;

	//by default use STL type unless there is no STL
	//specific type, or the user specified '-I" option
	res = in->stl_type;
	g_CSV = in->csv;
	if (g_IB  || res == NO_OUTPUT_TYPE)
		res = in->ib_type;

	if(res == NO_OUTPUT_TYPE) {
		if (g_IB)
			fprintf(stderr, "opasaquery: Invalid IB Output Type: %s\n", in->string);
		else
			fprintf(stderr, "opasaquery: Invalid Output Type: %s\n", in->string);
		Usage();
		// NOTREACHED
		return OutputTypeStlNodeRecord;
	}
	return res;
}

typedef struct InputStringMap {
	char *string;
	QUERY_INPUT_TYPE in_type;
} InputStringMap_t;

InputStringMap_t InputStrTable[] = {
		{ "No Input", InputTypeNoInput },
		{ "pkey", InputTypePKey },
        { "index", InputTypeIndex},
        { "serviceId", InputTypeServiceId },
        { "lid", InputTypeLid },
        { "SL", InputTypeSL },
        { "NodeType", InputTypeNodeType },
        { "SystemImageGuid", InputTypeSystemImageGuid },
        { "NodeGuid", InputTypeNodeGuid },
        { "PortGuid", InputTypePortGuid },
        { "PortGid", InputTypePortGid },
        { "McGid", InputTypeMcGid },
        { "NodeDesc", InputTypeNodeDesc },
        { "PortGuidPair", InputTypePortGuidPair },
        { "GidPair", InputTypeGidPair },
        { "PortGuidList", InputTypePortGuidList },
        { "GidList", InputTypeGidList },
		//Last entry must be null, insert new attributes above here!
		{NULL, 0}
};

FSTATUS CheckInputOutput(OMGT_QUERY *pQuery, OutputStringMap_t *in) {

	int i=0;
	int j=0;
	boolean found;

	if ( pQuery->InputType == InputTypeNoInput)
		return FSUCCESS;

	if (in->valid_input_types != NULL) {
		while ( (j < MAX_NUM_INPUT_ARGS) && (in->valid_input_types->flags[j] != 0) ){
			if (pQuery->InputType == in->valid_input_types->flags[j])
					return FSUCCESS;
			else j++;
			} //end while
		}
		else {
			fprintf(stderr, "opasaquery: This option (%s) does not require input. ",in->string);
			fprintf(stderr, "Ignoring input argument\n");
			pQuery->InputType = InputTypeNoInput;
			return FSUCCESS;
		}
// output not found for a given input... then...
	fprintf(stderr, "opasaquery: Invalid input-output pair\n");
	j=0;
	fprintf(stderr, "opasaquery: for the selected output option (%s), the following \n",in->string);
	fprintf(stderr, "input types are valid: ");
	while ((j < MAX_NUM_INPUT_ARGS) && (in->valid_input_types->flags[j] != 0)){
			// look for the string corresponding to the flag
		i=0;
		found=FALSE;
		while ((InputStrTable[i].string !=NULL) && !found){
			if (in->valid_input_types->flags[j] == InputStrTable[i].in_type) {
				fprintf(stderr,"%s ",InputStrTable[i].string);
				found = TRUE;
			}
			else
				i++;
			}
		j++;
	}

	fprintf(stderr, "\n");
	return FERROR;

}

int main(int argc, char ** argv)
{
    int                 c;
    uint8               hfi         = 0;
    uint8               port        = 0;
	int					index;
	OMGT_QUERY			query;
	QUERY_INPUT_VALUE old_input_values;
	OutputStringMap_t	*outputTypeMap = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields

	// default query for this application is for all node records
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypeStlNodeRecord;

	// process command line arguments
	while (-1 != (c = getopt_long(argc,argv, "vIh:p:l:k:i:t:s:n:g:u:m:d:P:G:a:A:o:S:L:", options, &index)))
    {
        switch (c)
        {
            case '$':
                Usage_full();
                break;
            case 'v':
                g_verbose++;
				if (g_verbose>2) umad_debug(g_verbose-2);
                break;
			case 'I':   // issue query in legacy InfiniBand format (IB)
				g_IB = 1;
				query.OutputType = OutputTypeNodeRecord;
				break;
            case 'h':	// hfi to issue query from
				if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid HFI Number: %s\n", optarg);
					Usage();
				}
                break;
            case 'p':	// port to issue query from
				if (FSUCCESS != StringToUint8(&port, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid Port Number: %s\n", optarg);
					Usage();
				}
                break;

            case 'l':	// query by lid
				multiInputCheck(query.InputType);
				query.InputType = InputTypeLid;
				if (FSUCCESS != StringToUint16(&old_input_values.Lid, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid LID: %s\n", optarg);
					Usage();
				}
                break;
            case 'k':	// query by pkey
				multiInputCheck(query.InputType);
				query.InputType = InputTypePKey;
				if (FSUCCESS != StringToUint16(&old_input_values.PKey, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid PKey: %s\n", optarg);
					Usage();
				}
				break;
            case 'i':	// query by vfindex
				multiInputCheck(query.InputType);
				query.InputType = InputTypeIndex;
				if (FSUCCESS != StringToUint16(&old_input_values.vfIndex, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid vfIndex: %s\n", optarg);
					Usage();
				}
				break;
            case 'S':	// query by serviceId
				multiInputCheck(query.InputType);
				query.InputType = InputTypeServiceId;
				if (FSUCCESS != StringToUint64(&old_input_values.ServiceId, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid ServiceId: %s\n", optarg);
					Usage();
				}
				break;
            case 'L':	// query by service level
				multiInputCheck(query.InputType);
				query.InputType = InputTypeSL;
				if (FSUCCESS != StringToUint8(&old_input_values.SL, optarg, NULL, 0, TRUE)
					|| old_input_values.SL > 15) {
					fprintf(stderr, "opasaquery: Invalid SL: %s\n", optarg);
					Usage();
				}
                break;
            case 't':	// query by node type
				multiInputCheck(query.InputType);
				query.InputType = InputTypeNodeType;
				old_input_values.TypeOfNode = checkNodeType(optarg);
                break;
            case 's':	// query by system image guid
				multiInputCheck(query.InputType);
				query.InputType = InputTypeSystemImageGuid;
				if (FSUCCESS != StringToUint64(&old_input_values.Guid, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GUID: %s\n", optarg);
					Usage();
				}
                break;
            case 'n':	// query by node guid
				multiInputCheck(query.InputType);
				query.InputType = InputTypeNodeGuid;
				if (FSUCCESS != StringToUint64(&old_input_values.Guid, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GUID: %s\n", optarg);
					Usage();
				}
                break;
            case 'g':	// query by port guid
				multiInputCheck(query.InputType);
				query.InputType = InputTypePortGuid;
				if (FSUCCESS != StringToUint64(&old_input_values.Guid, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GUID: %s\n", optarg);
					Usage();
				}
                break;
            case 'u':	// query by gid
				multiInputCheck(query.InputType);
				query.InputType = InputTypePortGid;
				if (FSUCCESS != StringToGid(&old_input_values.Gid.AsReg64s.H,&old_input_values.Gid.AsReg64s.L, optarg, NULL, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GID: %s\n", optarg);
					Usage();
				}
                break;
            case 'm':	// query by multicast gid
				multiInputCheck(query.InputType);
				query.InputType = InputTypeMcGid;
				if (FSUCCESS != StringToGid(&old_input_values.Gid.AsReg64s.H,&old_input_values.Gid.AsReg64s.L, optarg, NULL, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GID: %s\n", optarg);
					Usage();
				}
                break;
            case 'd':	// query by node description
				multiInputCheck(query.InputType);
				query.InputType = InputTypeNodeDesc;
				old_input_values.NodeDesc.NameLength = MIN(strlen(optarg), NODE_DESCRIPTION_ARRAY_SIZE);
				strncpy((char*)old_input_values.NodeDesc.Name, optarg, NODE_DESCRIPTION_ARRAY_SIZE);
                break;
			case 'P':	// query by source:dest port guids
				{
					char *p;
					multiInputCheck(query.InputType);
					query.InputType = InputTypePortGuidPair;
					if (FSUCCESS != StringToUint64(&old_input_values.PortGuidPair.SourcePortGuid, optarg, &p, 0, TRUE)
						|| ! p || *p == '\0') {
						fprintf(stderr, "opasaquery: Invalid GUID Pair: %s\n", optarg);
						Usage();
					}
					if (FSUCCESS != StringToUint64(&old_input_values.PortGuidPair.DestPortGuid, p, NULL, 0, TRUE)) {
						fprintf(stderr, "opasaquery: Invalid GUID Pair: %s\n", optarg);
						Usage();
					}
				}
                break;
			case 'G':	// query by source:dest port gids
				{
					char *p;
					multiInputCheck(query.InputType);
					query.InputType = InputTypeGidPair;
					if (FSUCCESS != StringToGid(&old_input_values.GidPair.SourceGid.AsReg64s.H,&old_input_values.GidPair.SourceGid.AsReg64s.L, optarg, &p, TRUE)
						|| ! p || *p == '\0') {
						fprintf(stderr, "opasaquery: Invalid GID Pair: %s\n", optarg);
						Usage();
					}
					if (FSUCCESS != StringToGid(&old_input_values.GidPair.DestGid.AsReg64s.H,&old_input_values.GidPair.DestGid.AsReg64s.L, p, NULL, TRUE)) {
						fprintf(stderr, "opasaquery: Invalid GID Pair: %s\n", optarg);
						Usage();
					}
				}
				break;
			case 'a':	// multipath query by src1:src2:...;dest1:dest2:... port guids
				{
					char *p =optarg;
					int i = 0;
					errno = 0;
					multiInputCheck(query.InputType);
					query.InputType = InputTypePortGuidList;
					do {
						if (FSUCCESS != StringToUint64(&old_input_values.PortGuidList.GuidList[i], p, &p, 0, TRUE)) {
							fprintf(stderr, "opasaquery: Invalid GUID List: %s\n", optarg);
							Usage();
						}
						i++;
						old_input_values.PortGuidList.SourceGuidCount++;
					} while (p && *p != '\0' && *p != ';');
					if (p && *p != '\0')
					{
						p++; // skip the semi-colon.
						do {
							if (FSUCCESS != StringToUint64(&old_input_values.PortGuidList.GuidList[i], p, &p, 0, TRUE)) {
								fprintf(stderr, "opasaquery: Invalid GUID List: %s\n", optarg);
								Usage();
							}
							i++;
							old_input_values.PortGuidList.DestGuidCount++;
						} while (p && *p != '\0');
					} else {
						Usage();
					}
				}
                break;
			case 'A':	// multipath query by src1:src2:...;dest1:dest2:... gids
				{
					char *p=optarg;
					int i = 0;
					errno = 0;
					multiInputCheck(query.InputType);
					query.InputType = InputTypeGidList;
					do {
						if (FSUCCESS != StringToGid(&old_input_values.GidList.GidList[i].AsReg64s.H,&old_input_values.GidList.GidList[i].AsReg64s.L, p, &p, TRUE)) {
							fprintf(stderr, "opasaquery: Invalid GID List: %s\n", optarg);
							Usage();
						}
						i++;
						old_input_values.GidList.SourceGidCount++;
					} while (p && *p != '\0' && *p != ';');
					if (p && *p != '\0')
					{
						p++; // skip the semi-colon
						do {
							if (FSUCCESS != StringToGid(&old_input_values.GidList.GidList[i].AsReg64s.H,&old_input_values.GidList.GidList[i].AsReg64s.L, p, &p, TRUE)) {
								fprintf(stderr, "opasaquery: Invalid GID List: %s\n", optarg);
								Usage();
							}
							i++;
							old_input_values.GidList.DestGidCount++;
						} while (p && *p != '\0');
					} else {
						Usage();
					}
				}
                break;
            case 'o':	// select output record desired
				outputTypeMap = GetOutputTypeMap(optarg);
                break;
            default:
                fprintf(stderr, "opasaquery: Invalid option -%c\n", c);
                Usage();
                break;
        }
    } /* end while */

	if (optind < argc)
	{
		Usage();
	}
	if (NULL != outputTypeMap){
		if  (CheckInputOutput(&query, outputTypeMap )!= 0)
			Usage();
		else
			query.OutputType = GetOutputType(outputTypeMap); 
		}

	PrintDestInitFile(&g_dest, stdout);
	if (g_verbose)
		PrintDestInitFile(&g_dbgDest, stdout);
	else
		PrintDestInitNone(&g_dbgDest);

	FSTATUS status;
	struct omgt_params params = {.debug_file = g_verbose > 1 ? stderr : NULL };
	status = omgt_open_port_by_num(&sa_omgt_session,hfi,port,&params);
	if (status != 0) {
		fprintf(stderr, "opasaquery: Failed to open port hfi %d:%d: %s\n", hfi, port, strerror(status));
		exit(1);
	}

	// perform the query and display output
	do_query(sa_omgt_session, &query, &old_input_values);

	omgt_close_port(sa_omgt_session);

	if (g_exitstatus == 2)
		Usage();

	return g_exitstatus;
}
