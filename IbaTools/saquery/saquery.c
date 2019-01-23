/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015-2017, Intel Corporation

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
#include "ixml.h"

#define MAX_NUM_INPUT_ARGS 20

uint8           g_IB            = 0;	// perform query in legacy InfiniBand format
uint8           g_verbose       = 0;
int				g_exitstatus	= 0;
int				g_CSV			= 0;	// output should use CSV style
										// only valid for OutputTypeVfInfoRecord
										// 1=absolute value for mtu and rate
										// 2=enum value for mtu and rate
char *g_oob_address = "127.0.0.1";
uint16 g_oob_port = 3245;
char *g_sslParmsFile = "/etc/opa/opamgt_tls.xml";
struct omgt_ssl_params g_ssl_params = {0};
IB_GID g_src_gid = {{0}};
boolean g_isOOB = FALSE;
boolean g_isFEESM = FALSE;
boolean g_isOOBSSL = FALSE;

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
	DBGPRINT("Query: Input=%s (0x%x), Output=%s (0x%x)\n",
		iba_sd_query_input_type_msg(pQuery->InputType), pQuery->InputType,
		iba_sd_query_result_type_msg(pQuery->OutputType), pQuery->OutputType);

	if (!g_isOOB) {
		(void)omgt_port_get_port_guid(port, &g_src_gid.Type.Global.InterfaceID);
		(void)omgt_port_get_port_prefix(port, &g_src_gid.Type.Global.SubnetPrefix);
	}
	status = omgt_input_value_conversion(pQuery, old_input_value, g_src_gid);
	if (status) {
		fprintf(stderr, "%s: Error: InputValue conversion failed: Status %u\n"
			"Query: Input=%s (0x%x), Output=%s (0x%x)\n"
			"SourceGid: 0x%.16"PRIx64":%.16"PRIx64"\n", __func__, status,
			iba_sd_query_input_type_msg(pQuery->InputType), pQuery->InputType,
			iba_sd_query_result_type_msg(pQuery->OutputType), pQuery->OutputType,
			g_src_gid.AsReg64s.H, g_src_gid.AsReg64s.L);
		if (status == FERROR) {
			fprintf(stderr, "Source Gid (-x) must be supplied for Input/Output combination.\n");
		}
		g_exitstatus = 1;
		return;
	}
	// this call is synchronous
	status = omgt_query_sa(port, pQuery, &pQueryResults);

	g_exitstatus = PrintQueryResult(&g_dest, 0, &g_dbgDest, pQuery->OutputType,
		g_CSV, status, pQueryResults);

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
		{ "oob", required_argument, NULL, 'b' },
		{ "port", required_argument, NULL, 'p' },
		{ "source-gid", required_argument, NULL, 'x' },
		{ "feEsm", no_argument, NULL, 'E' },
		{ "ssl-params", required_argument, NULL, 'T' },

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
		{ "dgname", required_argument, NULL, 'D' },
		// output type
		{ "output", required_argument, NULL, 'o' },
		{ "timeout", required_argument, NULL, '!'},
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
	fprintf(stderr, "    -b/--oob address          - Out-of-Band address of node running the FE. Can be\n"
					"                                either hostname, IPv4, or IPv6 address.\n"
					"                                (default is \"%s\"\n", g_oob_address);
	fprintf(stderr, "    -p/--port port            - port, \n"
					"                                 In-band: numbered 1..n, 0=1st active (default\n"
					"                                     is 1st active)\n"
					"                                 Out-of-band: Port FE is listening on (default\n"
					"                                     is %u)\n", g_oob_port);
	fprintf(stderr, "    --timeout                 - timeout(response wait time) in ms, default is 1000ms\n");
	fprintf(stderr, "    -x/--source-gid src_gid   - Source GID of the local GID [required for most\n");
	fprintf(stderr, "                                Path and Trace Record Queries when Out-of-Band]\n");
	fprintf(stderr, "    -E/--feEsm                - ESM FE\n");
	fprintf(stderr, "    -T/--ssl-params file      - SSL/TLS parameters XML file\n"
					"                                (default is \"%s\")\n", g_sslParmsFile);
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
	fprintf(stderr, "    -D/--dgname name          - query by device group name/description\n");
	fprintf(stderr, "    -P/--guidpair 'guid guid' - query by a pair of port Guids\n");
	fprintf(stderr, "    -G/--gidpair 'gid gid'    - query by a pair of Gids\n");
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
	fprintf(stderr, "    scvlr         - list of SC to VLr table records\n");
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
	fprintf(stderr, "    cableinfo     - list of Cable Info records\n");
	fprintf(stderr, "    portgroup     - list of AR Port Group records\n");
	fprintf(stderr, "    portgroupfdb  - list of AR Port Group FWD records\n");
	fprintf(stderr, "    dglist        - list of Device Group Names\n");
	fprintf(stderr, "    dgmember      - list of Device Group records\n");	
	fprintf(stderr, "    dtree         - list of Device Tree records\n");
	fprintf(stderr ,"    swcost        - list of switch cost records\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage examples:\n");
	fprintf(stderr, "    opasaquery -o desc -t fi\n");
	exit(0);

}

void Usage(void)
{
	fprintf(stderr, "Usage: opasaquery [-v] [-o type] [-l lid] [-t type] [-s guid] [-n guid]\n");
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
	fprintf(stderr, "Node Type:\n");
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
/* SSL Paramters XML parse code */
static void *sslParmsXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	struct omgt_ssl_params *sslParmsData = IXmlParserGetContext(state);
	memset(sslParmsData, 0, sizeof(struct omgt_ssl_params));
	return sslParmsData;
}
static void sslParmsXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
}
static IXML_FIELD sslSecurityFields[] = {
	{ tag:"SslSecurityEnabled", format:'u', IXML_FIELD_INFO(struct omgt_ssl_params, enable) },
	{ tag:"SslSecurityEnable", format:'u', IXML_FIELD_INFO(struct omgt_ssl_params, enable) },
	{ tag:"SslSecurityDir", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, directory) },
	{ tag:"SslSecurityFFCertificate", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, certificate) },
	{ tag:"SslSecurityFFPrivateKey", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, private_key) },
	{ tag:"SslSecurityFFCaCertificate", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, ca_certificate) },
	{ tag:"SslSecurityFFCertChainDepth", format:'u', IXML_FIELD_INFO(struct omgt_ssl_params, cert_chain_depth) },
	{ tag:"SslSecurityFFDHParameters", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, dh_params) },
	{ tag:"SslSecurityFFCaCRLEnabled", format:'u', IXML_FIELD_INFO(struct omgt_ssl_params, ca_crl_enable) },
	{ tag:"SslSecurityFFCaCRL", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, ca_crl) },
	{ NULL }
};
static IXML_FIELD sslTopLevelFields[] = {
	{ tag:"SslSecurityParameters", format:'K', subfields:sslSecurityFields, start_func:sslParmsXmlParserStart, end_func:sslParmsXmlParserEnd }, // structure
	{ NULL }
};
FSTATUS ParseSslParmsFile(const char *input_file, int g_verbose, struct omgt_ssl_params *sslParmsData)
{
	unsigned tags_found, fields_found;
	const char *filename=input_file;

	if (strcmp(input_file, "-") == 0) {
		filename="stdin";
		if (FSUCCESS != IXmlParseFile(stdin, "stdin", IXML_PARSER_FLAG_NONE, sslTopLevelFields, NULL, sslParmsData, NULL, NULL, &tags_found, &fields_found)) {
			return FERROR;
		}
	} else {
		if (FSUCCESS != IXmlParseInputFile(input_file, IXML_PARSER_FLAG_NONE, sslTopLevelFields, NULL, sslParmsData, NULL, NULL, &tags_found, &fields_found)) {
			return FERROR;
		}
	}
	if (tags_found != 1 || fields_found != 1) {
		fprintf(stderr, "Warning: potentially inaccurate input '%s': found %u recognized top level tags, expected 1\n", filename, tags_found);
	}

	return FSUCCESS;
}
/* END SSL Paramters XML parse code */

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
	case InputTypeDeviceGroup:
			typestr = "DeviceGroup";
			break;
        default:
			typestr = "parameter";
		}
	fprintf(stderr, "opasaquery: multiple selection criteria not supported, ignoring %s\n", typestr);
}


 

typedef struct InputFlags {
	QUERY_INPUT_TYPE flags[MAX_NUM_INPUT_ARGS];
} InputFlags_t;

#define END_INPUT_TYPE 0xFFFF
//Note: The last initialized element of every InputFlags_t array must be END_INPUT_TYPE
InputFlags_t NodeInput = {{InputTypeNoInput, InputTypeNodeType,InputTypeLid,InputTypeSystemImageGuid,InputTypeNodeGuid,InputTypePortGuid,
		InputTypeNodeDesc, END_INPUT_TYPE}};
InputFlags_t PathInput = {{InputTypeNoInput, InputTypeLid,InputTypePKey,InputTypePortGuid,InputTypePortGid,InputTypePortGuidPair,
		InputTypeGidPair,InputTypeServiceId,InputTypeSL,END_INPUT_TYPE}};
InputFlags_t ServiceInput = {{InputTypeNoInput,InputTypePortGuid,InputTypePortGid,InputTypeServiceId,END_INPUT_TYPE}};
InputFlags_t McmemberInput = {{InputTypeNoInput, InputTypeLid, InputTypePKey,InputTypePortGuid,InputTypePortGid,InputTypeMcGid,InputTypeSL,END_INPUT_TYPE}};
InputFlags_t InformInput = {{InputTypeNoInput,InputTypePortGuid,InputTypePortGid,InputTypeLid,END_INPUT_TYPE}};
InputFlags_t TraceInput =  {{InputTypePortGuid,InputTypePortGid,InputTypePortGuidPair,InputTypeGidPair,END_INPUT_TYPE}};
InputFlags_t VfinfoInput = {{InputTypeNoInput,InputTypePKey,InputTypeIndex,InputTypeMcGid,InputTypeServiceId,InputTypeSL,InputTypeNodeDesc,END_INPUT_TYPE}};
InputFlags_t MiscInput = {{InputTypeNoInput,InputTypeLid,END_INPUT_TYPE}};
InputFlags_t DgMemberInput = {{InputTypeNoInput,InputTypeLid, InputTypePortGuid, InputTypeNodeDesc,InputTypeDeviceGroup,END_INPUT_TYPE}};

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
    {"link",            OutputTypeStlLinkRecord,              NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"service",         NO_OUTPUT_TYPE,                       OutputTypeServiceRecord,    &ServiceInput, 0},
    {"mcmember",        NO_OUTPUT_TYPE,                       OutputTypeMcMemberRecord,   &McmemberInput, 0},
    {"inform",          OutputTypeStlInformInfoRecord,        OutputTypeInformInfoRecord, &InformInput, 0},
    {"trace",           OutputTypeStlTraceRecord,             NO_OUTPUT_TYPE,             &TraceInput, 0},
    {"scsc",            OutputTypeStlSCSCTableRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"slsc",            OutputTypeStlSLSCTableRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"scsl",            OutputTypeStlSCSLTableRecord,         NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"scvlt",           OutputTypeStlSCVLtTableRecord,        NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"scvlnt",          OutputTypeStlSCVLntTableRecord,       NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"scvlr",           OutputTypeStlSCVLrTableRecord,        NO_OUTPUT_TYPE,             &MiscInput, 0},
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
    {"dgmember",        OutputTypeStlDeviceGroupMemberRecord, NO_OUTPUT_TYPE,             &DgMemberInput, 0},
    {"dglist",          OutputTypeStlDeviceGroupNameRecord,   NO_OUTPUT_TYPE,             NULL, 0},
    {"dtree",           OutputTypeStlDeviceTreeMemberRecord,  NO_OUTPUT_TYPE,             &MiscInput, 0},
    {"swcost",          OutputTypeStlSwitchCostRecord,        NO_OUTPUT_TYPE,             &MiscInput, 0},
    //Last entry must be null, insert new attributes above here!
    {NULL, NO_OUTPUT_TYPE, NO_OUTPUT_TYPE, NULL, 0}
};

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
	{ "DeviceGroup", InputTypeDeviceGroup},
	//Last entry must be null, insert new inputs above here!
	{NULL, 0}
};

static void PrintValidInputTypes(OutputStringMap_t *in) {
	
	int i = 0;
	int j = 0;

	fprintf(stderr, "opasaquery: for the selected output option (%s), the following \n",in->string);
	fprintf(stderr, "input types are valid: ");

	while (in->valid_input_types->flags[i] != END_INPUT_TYPE){
		// look for the string corresponding to the flag
		j=0;
		while (InputStrTable[j].string !=NULL){
			if (in->valid_input_types->flags[i] == InputStrTable[j].in_type) {
				fprintf(stderr,"%s ",InputStrTable[j].string);
				break;
			}	
			j++;

		}
		i++;
	}

	fprintf(stderr, "\n");
	return;
}

static FSTATUS CheckInputOutput(OMGT_QUERY *pQuery, OutputStringMap_t *in) {

	int i=0;

	if(in->valid_input_types == NULL) {
		if (pQuery->InputType != InputTypeNoInput) {	
			fprintf(stderr, "opasaquery: This option (%s) does not accept input. ",in->string);
			fprintf(stderr, "Ignoring input argument\n");
			pQuery->InputType = InputTypeNoInput;
			return FSUCCESS;
		}
		else {
			return FSUCCESS;
		}
	}

	while (in->valid_input_types->flags[i] != END_INPUT_TYPE){
		if (pQuery->InputType == in->valid_input_types->flags[i]){
			return FSUCCESS;
		}
		i++;
	} 

	// Invalid Input Type detected
	fprintf(stderr, "opasaquery: Invalid input-output pair\n");
	PrintValidInputTypes(in);
	return FERROR;
}
 

int main(int argc, char ** argv)
{
    int                 c;
    uint8               hfi         = 0;
    uint16              port        = 0;
	int					index;
	OMGT_QUERY			query;
	QUERY_INPUT_VALUE old_input_values;
	OutputStringMap_t	*outputTypeMap = NULL;
	int ms_timeout = OMGT_DEF_TIMEOUT_MS;

	memset(&query, 0, sizeof(query));	// initialize reserved fields

	// default query for this application is for all node records
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypeStlNodeRecord;

	// process command line arguments
	while (-1 != (c = getopt_long(argc,argv, "vIh:p:b:x:ET:l:k:i:t:s:n:g:u:m:d:P:G:o:S:L:D:H:", options, &index)))
    {
        switch (c)
        {
			case '$':
				Usage_full();
				break;
			case 'v':
				g_verbose++;
				if (g_verbose > 2) umad_debug(g_verbose - 2);
				break;
			case 'I':   // issue query in legacy InfiniBand format (IB)
				g_IB = 1;
				query.OutputType = OutputTypeNodeRecord;
				break;
			case 'h':   // hfi to issue query from
				if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid HFI Number: %s\n", optarg);
					Usage();
				}
				g_isOOB = FALSE;
				break;
			case 'p':   // port to issue query from
				if (FSUCCESS != StringToUint16(&port, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid Port Number: %s\n", optarg);
					Usage();
				}
				break;
			case '!':
				if (FSUCCESS != StringToInt32(&ms_timeout, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid timeout value: %s\n", optarg);
					Usage();
				}
				break;
			case 'b': // OOB address of FE
				g_oob_address = optarg;
				g_isOOB = TRUE;
				break;
			case 'E': // FE is ESM
				g_isFEESM = TRUE;
				g_isOOB = TRUE;
				break;
			case 'T': // FE Uses SSL
				g_sslParmsFile = optarg;
				g_isOOBSSL = TRUE;
				g_isOOB = TRUE;
				break;
			case 'x': // Source GID
				if (FSUCCESS != StringToGid(&g_src_gid.AsReg64s.H, &g_src_gid.AsReg64s.L, optarg, NULL, TRUE)) {
					fprintf(stderr, "opasaquery: Invalid Source Gid: %s\n", optarg);
					Usage();
				}
				break; 

            case 'l':	// query by lid
				multiInputCheck(query.InputType);
				query.InputType = InputTypeLid;
				if (FSUCCESS != StringToUint32(&old_input_values.Lid, optarg, NULL, 0, TRUE)) {
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
					|| old_input_values.SL > 31) {
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
			case 'D':	// query by device group name
				multiInputCheck(query.InputType);
				query.InputType = InputTypeDeviceGroup;
				old_input_values.DeviceGroup.NameLength = MIN(strlen(optarg), MAX_DG_NAME);
				memset((char *)old_input_values.DeviceGroup.Name, 0, MAX_DG_NAME);
				strncpy((char*)old_input_values.DeviceGroup.Name, optarg, MAX_DG_NAME-1);
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
 
	if (g_IB && query.InputType == InputTypeLid &&
		old_input_values.Lid > IB_MAX_UCAST_LID &&
		query.OutputType != OutputTypeMcMemberRecord) {

		fprintf(stderr, "opasaquery: lid value must be less than or equal to 0x%x for IB queries\n",IB_MAX_UCAST_LID);
		Usage();
	}

	if (g_IB && query.InputType == InputTypeSL)
		if (old_input_values.SL > 15)
		{
			fprintf(stderr, "opasaquery: SL value must be less than or equal to 15 for IB queries\n");
			Usage();
		}

	if (query.InputType == InputTypeLid &&
		query.OutputType == OutputTypeMcMemberRecord) {

		if (IS_MCAST32(old_input_values.Lid)) {
			// allow 32-bit as long as it only uses the IB MLID subset
			IB_LID mlid16 = MCAST32_TO_MCAST16(old_input_values.Lid);
			STL_LID mlid32 = MCAST16_TO_MCAST32(mlid16);
			if (mlid32 != old_input_values.Lid ||
				(mlid16 & LID_PERMISSIVE) == LID_PERMISSIVE) {

				fprintf(stderr, "opasaquery: MLID 0x%x is outside acceptable range"
					" (0xf0000000 to 0xf003ffe) for using 32-bit MLID format with"
					" IB MulticastMemberRecord query\n", old_input_values.Lid);
				Usage();
			}

			old_input_values.Lid = MCAST32_TO_MCAST16(old_input_values.Lid);
		} else if (!IS_MCAST16(old_input_values.Lid)) {
			fprintf(stderr, "opasaquery: must use either 16-bit or 32-bit MLID"
				" format for IB MulticastMemberRecord queries\n");
			Usage();
		}
	}


 	PrintDestInitFile(&g_dest, stdout);
 	if (g_verbose)
		PrintDestInitFile(&g_dbgDest, stdout);
	else
		PrintDestInitNone(&g_dbgDest);

	FSTATUS status;
	struct omgt_params params = {
		.debug_file = (g_verbose > 1 ? stderr : NULL),
		.error_file = stderr
	};
	if (g_isOOB) {
		if (g_sslParmsFile) {
			if ((status = ParseSslParmsFile(g_sslParmsFile, g_verbose, &g_ssl_params)) != FSUCCESS) {
				fprintf(stderr, "opasaquery: Must provide a valid SSL/TLS parameters XML file: %s\n", g_sslParmsFile);
				Usage();
			}
		}
		struct omgt_oob_input oob_input = {
			.host = g_oob_address,
			.port = (port ? port : g_oob_port),
			.is_esm_fe = g_isFEESM,
			.ssl_params = g_ssl_params
		};
		status = omgt_oob_connect(&sa_omgt_session, &oob_input, &params);
		if (status != 0) {
			fprintf(stderr, "opasaquery: Failed to open connection to FE %s:%u: %s\n",
				oob_input.host, oob_input.port, omgt_status_totext(status));
			exit(1);
		}
	} else {
		status = omgt_open_port_by_num(&sa_omgt_session, hfi, port, &params);
		if (status != 0) {
			fprintf(stderr, "opasaquery: Failed to open port hfi %d:%d: %s\n", hfi, port, omgt_status_totext(status));
			exit(1);
		}
	}

	//set timeout for SA operations
	omgt_set_timeout(sa_omgt_session, ms_timeout);

	// perform the query and display output
	do_query(sa_omgt_session, &query, &old_input_values);

	omgt_close_port(sa_omgt_session);

	if (g_exitstatus == 2)
		Usage();

	return g_exitstatus;
}
