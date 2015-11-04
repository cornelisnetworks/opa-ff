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
#include <oib_utils_sa.h>
#include <ibprint.h>
#include <stl_print.h>
#include <infiniband/umad.h>

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

static struct oib_port *sa_oib_session;
PrintDest_t g_dest;
PrintDest_t g_dbgDest;

// perform the given query and display the results
// if portGuid is -1, 1st active port is used to issue query
//void do_query(EUI64 portGuid, QUERY *pQuery)
void do_query(struct oib_port *port, QUERY *pQuery)
{
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	DBGPRINT("Query: Input=%s (0x%x), Output=%s (0x%x)\n",
				   		iba_sd_query_input_type_msg(pQuery->InputType),
						pQuery->InputType,
					   	iba_sd_query_result_type_msg(pQuery->OutputType),
						pQuery->OutputType);

	// this call is synchronous
	status = oib_query_sa(port, pQuery, &pQueryResults);

	g_exitstatus = PrintQueryResult(&g_dest, 0, &g_dbgDest,
			   		pQuery->OutputType, g_CSV, status, pQueryResults);

	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		oib_free_query_result_buffer(pQueryResults);
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
	fprintf(stderr, "Usage: opasaquery [-v [-v] [-v]] [-I] [-h hfi] [-p port] [-o type]\n");
	fprintf(stderr, "                   [-l lid] [-t type] [-s guid] [-n guid] [-g guid]\n");
	fprintf(stderr, "                   [-k pkey] [-i vfIndex] [-S serviceId] [-L sl]\n");
	fprintf(stderr, "                   [-u gid] [-m gid] [-d name]\n");
	fprintf(stderr, "                   [-P 'guid guid'] [-G 'gid gid']\n");
	fprintf(stderr, "                   [-a 'sguid...;dguid...'] [-A 'sgid...;dgid...']\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opasaquery --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output. A second invocation will\n"
                    "                                activate openib debugging, a 3rd will\n"
 					"                                activate libibumad debugging.\n");
	fprintf(stderr, "    -I/--IB                   - issue query in legacy InfiniBand format\n");
	fprintf(stderr, "    -h/--hfi hfi              - hfi, numbered 1..n, 0= -p port will be a\n");
	fprintf(stderr, "                                system wide port num (default is 0)\n");
	fprintf(stderr, "    -p/--port port            - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "                                is 1st active)\n");
	fprintf(stderr, "    -l/--lid lid              - query a specific lid\n");
	fprintf(stderr, "    -k/--pkey pkey            - query a specific pkey\n");
	fprintf(stderr, "    -i/--vfindex vfindex      - query a specific vfindex\n");
	fprintf(stderr, "    -S/--serviceId serviceId  - query a specific service id\n");
	fprintf(stderr, "    -L/--SL SL                - query by service level\n");
	fprintf(stderr, "    -t/--type type            - query by node type\n");
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
	fprintf(stderr, "    -o/--output type          - output type for query (default is node)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Node Types:\n");
	fprintf(stderr, "    fi  - fabric interface\n");
	fprintf(stderr, "    sw  - switch\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Gids:\n");
	fprintf(stderr, "   specify 64 bit subnet and 64 bit interface id as:\n");
	fprintf(stderr, "   	subnet:interface\n");
	fprintf(stderr, "   such as: 0xfe80000000000000:0x00066a00a0000380\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Output Types:\n");
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
	fprintf(stderr, "    ranfdb        - list of switch random FDB records\n");
	fprintf(stderr, "    mcfdb         - list of switch multicast FDB records\n");
	fprintf(stderr, "    trace         - list of trace records\n");
	fprintf(stderr, "    vfinfo        - list of vFabrics\n");
	fprintf(stderr, "    vfinfocsv     - list of vFabrics in CSV format\n");
	fprintf(stderr, "    vfinfocsv2    - list of vFabrics in CSV format with enums\n");
	fprintf(stderr, "    fabricinfo    - summary of fabric devices\n");
	fprintf(stderr, "    quarantine    - list of quarantined nodes\n");
	fprintf(stderr, "    conginfo      - list of Congestion Info Records\n");
	fprintf(stderr, "    swcongset     - list of Switch Congestion Settings\n");
	fprintf(stderr, "    swportcong  - list of Switch Port Congestion Settings\n");
	fprintf(stderr, "    hficongset    - list of HFI Congestion Settings\n");
	fprintf(stderr, "    hficongcon    - list of HFI Congesting Control Settings\n");
	fprintf(stderr, "    bfrctrl       - list of buffer control tables\n");
	fprintf(stderr ,"    cableinfo     - list of Cable Info records\n");
	fprintf(stderr ,"    portgroup     - list of AR Port Group records\n");
	fprintf(stderr ,"    portgroupfdb  - list of AR Port Group FWD records\n");
	exit(0);
}

void Usage(void)
{
	fprintf(stderr, "Usage: opasaquery [-v] [-o type] [-l lid] [-t type] [-s guid] [-n guid]\n");
	fprintf(stderr, "                   [-k pkey] \n");
	fprintf(stderr, "                   [-g guid] [-u gid] [-m gid] [-d name]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opasaquery --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -l/--lid lid              - query a specific lid\n");
	fprintf(stderr, "    -k/--pkey pkey            - query a specific pkey\n");
	fprintf(stderr, "    -t/--type type            - query by node type\n");
	fprintf(stderr, "    -s/--sysguid guid         - query by system image guid\n");
	fprintf(stderr, "    -n/--nodeguid guid        - query by node guid\n");
	fprintf(stderr, "    -g/--portguid guid        - query by port guid\n");
	fprintf(stderr, "    -m/--mcgid gid            - query by multicast gid\n");
	fprintf(stderr, "    -d/--desc name            - query by node name/description\n");
	fprintf(stderr, "    -o/--output type          - output type for query (default is node)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Node Types:\n");
	fprintf(stderr, "    fi  - fabric interface\n");
	fprintf(stderr, "    sw  - switch\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Gids:\n");
	fprintf(stderr, "   specify 64 bit subnet and 64 bit interface id as:\n");
	fprintf(stderr, "   	subnet:interface\n");
	fprintf(stderr, "   such as: 0xfe80000000000000:0x00066a00a0000380\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Output Types (abridged):\n");
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

typedef struct OutputStringMap {
	char *string;
	QUERY_RESULT_TYPE stl_type;
	QUERY_RESULT_TYPE ib_type;
	int csv;
} OutputStringMap_t;

#define NO_OUTPUT_TYPE 0xffff
OutputStringMap_t OutputTypeTable[] = {
//--input string--------StlOutputType-----------------------IbOutputType--------csv-//
	{"systemguid",      NO_OUTPUT_TYPE,                     OutputTypeSystemImageGuid, 0},
	{"classportinfo",   OutputTypeStlClassPortInfo,         OutputTypeClassPortInfo, 0},
	{"nodeguid",        NO_OUTPUT_TYPE,                     OutputTypeNodeGuid, 0},
	{"portguid",        OutputTypeStlPortGuid,              OutputTypePortGuid, 0},
	{"lid",             OutputTypeStlLid,                   NO_OUTPUT_TYPE, 0},
	{"desc",            OutputTypeStlNodeDesc,              OutputTypeNodeDesc, 0},
	{"path",            NO_OUTPUT_TYPE,                     OutputTypePathRecord, 0},
	{"node",            OutputTypeStlNodeRecord,            OutputTypeNodeRecord, 0},
	{"portinfo",        OutputTypeStlPortInfoRecord,        OutputTypePortInfoRecord, 0},
	{"sminfo",          OutputTypeStlSMInfoRecord,          OutputTypeSMInfoRecord, 0},
	{"link",            OutputTypeStlLinkRecord,            OutputTypeLinkRecord, 0},
#ifndef NO_STL_SERVICE_OUTPUT       // Don't output STL Service if defined
	{"service",         OutputTypeStlServiceRecord,         OutputTypeServiceRecord, 0},
#else
	{"service",         OutputTypeServiceRecord,            OutputTypeServiceRecord, 0},
#endif
#ifndef NO_STL_MCMEMBER_OUTPUT      // Don't output STL McMember (use IB format) if defined
	{"mcmember",        OutputTypeStlMcMemberRecord,        OutputTypeMcMemberRecord, 0},
#else
    {"mcmember",        OutputTypeMcMemberRecord,           OutputTypeMcMemberRecord, 0},
#endif
	{"inform",          OutputTypeStlInformInfoRecord,      OutputTypeInformInfoRecord, 0},
	{"trace",           OutputTypeStlTraceRecord,           NO_OUTPUT_TYPE, 0},
	{"scsc",            OutputTypeStlSCSCTableRecord,       NO_OUTPUT_TYPE, 0},
	{"slsc",            OutputTypeStlSLSCTableRecord,       NO_OUTPUT_TYPE, 0},
	{"scsl",            OutputTypeStlSCSLTableRecord,       NO_OUTPUT_TYPE, 0},
	{"scvlt",           OutputTypeStlSCVLtTableRecord,      NO_OUTPUT_TYPE, 0},
	{"scvlnt",          OutputTypeStlSCVLntTableRecord,     NO_OUTPUT_TYPE, 0},
	{"swinfo",          OutputTypeStlSwitchInfoRecord,      OutputTypeSwitchInfoRecord, 0},
	{"linfdb",          OutputTypeStlLinearFDBRecord,       NO_OUTPUT_TYPE, 0},
	{"ranfdb",          NO_OUTPUT_TYPE,                     OutputTypeRandomFDBRecord, 0},
	{"mcfdb",           OutputTypeStlMCastFDBRecord,        NO_OUTPUT_TYPE, 0},
	{"vlarb",           OutputTypeStlVLArbTableRecord,      NO_OUTPUT_TYPE, 0},
	{"pkey",            OutputTypeStlPKeyTableRecord,       NO_OUTPUT_TYPE, 0},
	{"vfinfo",          OutputTypeStlVfInfoRecord,          NO_OUTPUT_TYPE, 0},
	{"vfinfocsv",       OutputTypeStlVfInfoRecord,          NO_OUTPUT_TYPE, 1},
	{"vfinfocsv2",      OutputTypeStlVfInfoRecord,          NO_OUTPUT_TYPE, 2},
	{"fabricinfo",      OutputTypeStlFabricInfoRecord,      NO_OUTPUT_TYPE, 0},
	{"quarantine",      OutputTypeStlQuarantinedNodeRecord, NO_OUTPUT_TYPE, 0},
	{"conginfo",        OutputTypeStlCongInfoRecord,        NO_OUTPUT_TYPE, 0},
	{"swcongset",       OutputTypeStlSwitchCongRecord,      NO_OUTPUT_TYPE, 0},
	{"swportcong",      OutputTypeStlSwitchPortCongRecord,  NO_OUTPUT_TYPE, 0},
	{"hficongset",      OutputTypeStlHFICongRecord,         NO_OUTPUT_TYPE, 0},
	{"hficongcon",      OutputTypeStlHFICongCtrlRecord,     NO_OUTPUT_TYPE, 0},
	{"bfrctrl",         OutputTypeStlBufCtrlTabRecord,      NO_OUTPUT_TYPE, 0},
	{"cableinfo",       OutputTypeStlCableInfoRecord,       NO_OUTPUT_TYPE, 0},
	{"portgroup",       OutputTypeStlPortGroupRecord,       NO_OUTPUT_TYPE, 0},
	{"portgroupfdb",    OutputTypeStlPortGroupFwdRecord,    NO_OUTPUT_TYPE, 0},
	//Last entry must be null, insert new attributes above here!
	{NULL, NO_OUTPUT_TYPE, NO_OUTPUT_TYPE, 0}
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


int main(int argc, char ** argv)
{
    int                 c;
    uint8               hfi         = 1;
    uint8               port        = 0;
	int					index;
	QUERY				query;
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
				if (g_verbose>1) oib_set_dbg(stderr);
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
				if (FSUCCESS != StringToUint16(&query.InputValue.Lid, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid LID: %s\n", optarg);
					Usage();
				}
                break;
            case 'k':	// query by pkey
				multiInputCheck(query.InputType);
				query.InputType = InputTypePKey;
				if (FSUCCESS != StringToUint16(&query.InputValue.PKey, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid PKey: %s\n", optarg);
					Usage();
				}
				break;
            case 'i':	// query by vfindex
				multiInputCheck(query.InputType);
				query.InputType = InputTypeIndex;
				if (FSUCCESS != StringToUint16(&query.InputValue.vfIndex, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid vfIndex: %s\n", optarg);
					Usage();
				}
				break;
            case 'S':	// query by serviceId
				multiInputCheck(query.InputType);
				query.InputType = InputTypeServiceId;
				if (FSUCCESS != StringToUint64(&query.InputValue.ServiceId, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid ServiceId: %s\n", optarg);
					Usage();
				}
				break;
            case 'L':	// query by service level
				multiInputCheck(query.InputType);
				query.InputType = InputTypeSL;
				if (FSUCCESS != StringToUint8(&query.InputValue.SL, optarg, NULL, 0, TRUE)
					|| query.InputValue.SL > 15) {
					fprintf(stderr, "opasaquery: Invalid SL: %s\n", optarg);
					Usage();
				}
                break;
            case 't':	// query by node type
				multiInputCheck(query.InputType);
				query.InputType = InputTypeNodeType;
				query.InputValue.TypeOfNode = checkNodeType(optarg);
                break;
            case 's':	// query by system image guid
				multiInputCheck(query.InputType);
				query.InputType = InputTypeSystemImageGuid;
				if (FSUCCESS != StringToUint64(&query.InputValue.Guid, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GUID: %s\n", optarg);
					Usage();
				}
                break;
            case 'n':	// query by node guid
				multiInputCheck(query.InputType);
				query.InputType = InputTypeNodeGuid;
				if (FSUCCESS != StringToUint64(&query.InputValue.Guid, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GUID: %s\n", optarg);
					Usage();
				}
                break;
            case 'g':	// query by port guid
				multiInputCheck(query.InputType);
				query.InputType = InputTypePortGuid;
				if (FSUCCESS != StringToUint64(&query.InputValue.Guid, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GUID: %s\n", optarg);
					Usage();
				}
                break;
            case 'u':	// query by gid
				multiInputCheck(query.InputType);
				query.InputType = InputTypePortGid;
				if (FSUCCESS != StringToGid(&query.InputValue.Gid.AsReg64s.H,&query.InputValue.Gid.AsReg64s.L, optarg, NULL, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GID: %s\n", optarg);
					Usage();
				}
                break;
            case 'm':	// query by multicast gid
				multiInputCheck(query.InputType);
				query.InputType = InputTypeMcGid;
				if (FSUCCESS != StringToGid(&query.InputValue.Gid.AsReg64s.H,&query.InputValue.Gid.AsReg64s.L, optarg, NULL, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid GID: %s\n", optarg);
					Usage();
				}
                break;
            case 'd':	// query by node description
				multiInputCheck(query.InputType);
				query.InputType = InputTypeNodeDesc;
				query.InputValue.NodeDesc.NameLength = MIN(strlen(optarg), NODE_DESCRIPTION_ARRAY_SIZE);
				strncpy((char*)query.InputValue.NodeDesc.Name, optarg, NODE_DESCRIPTION_ARRAY_SIZE);
                break;
			case 'P':	// query by source:dest port guids
				{
					char *p;
					multiInputCheck(query.InputType);
					query.InputType = InputTypePortGuidPair;
					if (FSUCCESS != StringToUint64(&query.InputValue.PortGuidPair.SourcePortGuid, optarg, &p, 0, TRUE)
						|| ! p || *p == '\0') {
						fprintf(stderr, "opasaquery: Invalid GUID Pair: %s\n", optarg);
						Usage();
					}
					if (FSUCCESS != StringToUint64(&query.InputValue.PortGuidPair.DestPortGuid, p, NULL, 0, TRUE)) {
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
					if (FSUCCESS != StringToGid(&query.InputValue.GidPair.SourceGid.AsReg64s.H,&query.InputValue.GidPair.SourceGid.AsReg64s.L, optarg, &p, TRUE)
						|| ! p || *p == '\0') {
						fprintf(stderr, "opasaquery: Invalid GID Pair: %s\n", optarg);
						Usage();
					}
					if (FSUCCESS != StringToGid(&query.InputValue.GidPair.DestGid.AsReg64s.H,&query.InputValue.GidPair.DestGid.AsReg64s.L, p, NULL, TRUE)) {
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
						if (FSUCCESS != StringToUint64(&query.InputValue.PortGuidList.GuidList[i], p, &p, 0, TRUE)) {
							fprintf(stderr, "opasaquery: Invalid GUID List: %s\n", optarg);
							Usage();
						}
						i++;
						query.InputValue.PortGuidList.SourceGuidCount++;
					} while (p && *p != '\0' && *p != ';');
					if (p && *p != '\0')
					{
						p++; // skip the semi-colon.
						do {
							if (FSUCCESS != StringToUint64(&query.InputValue.PortGuidList.GuidList[i], p, &p, 0, TRUE)) {
								fprintf(stderr, "opasaquery: Invalid GUID List: %s\n", optarg);
								Usage();
							}
							i++;
							query.InputValue.PortGuidList.DestGuidCount++;
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
						if (FSUCCESS != StringToGid(&query.InputValue.GidList.GidList[i].AsReg64s.H,&query.InputValue.GidList.GidList[i].AsReg64s.L, p, &p, TRUE)) {
							fprintf(stderr, "opasaquery: Invalid GID List: %s\n", optarg);
							Usage();
						}
						i++;
						query.InputValue.GidList.SourceGidCount++;
					} while (p && *p != '\0' && *p != ';');
					if (p && *p != '\0')
					{
						p++; // skip the semi-colon
						do {
							if (FSUCCESS != StringToGid(&query.InputValue.GidList.GidList[i].AsReg64s.H,&query.InputValue.GidList.GidList[i].AsReg64s.L, p, &p, TRUE)) {
								fprintf(stderr, "opasaquery: Invalid GID List: %s\n", optarg);
								Usage();
							}
							i++;
							query.InputValue.GidList.DestGidCount++;
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

	if (NULL != outputTypeMap)
		query.OutputType = GetOutputType(outputTypeMap); 

	PrintDestInitFile(&g_dest, stdout);
	if (g_verbose)
		PrintDestInitFile(&g_dbgDest, stdout);
	else
		PrintDestInitNone(&g_dbgDest);

	if(oib_open_port_by_num(&sa_oib_session,hfi,port) != 0)
	{
		fprintf(stderr, "opasaquery: Could not open oib session.\n");
		return FERROR;
	}

	// perform the query and display output
	do_query(sa_oib_session, &query);

	oib_close_port(sa_oib_session);

	if (g_exitstatus == 2)
		Usage();

	return g_exitstatus;
}
