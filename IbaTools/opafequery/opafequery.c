/* BEGIN_ICS_COPYRIGHT5 ****************************************

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

 * ** END_ICS_COPYRIGHT5   ****************************************/
/* [ICS VERSION STRING: unknown] */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#define _GNU_SOURCE
#include <signal.h>

#ifdef __cplusplus
}
#endif
#include <iba/public/datatypes.h>

#include "fe_connections.h"
#include "fe_sa.h"
#include "fe_pa.h"
#include "fe_ssl.h"

#include "iba/ipublic.h"
#include "ibprint.h"

#include "iba/stl_sa.h"
#include "iba/stl_sd.h"
#include "iba/ib_mad.h"

#include "ixml_ib.h"

/* general related macro definitions */
#define APP_NAME    "opafequery"
#define	MAX_VFABRIC_NAME 64		// from fm_xml.h

#define min(a,b) (((a)<(b))?(a):(b))

/* FE login related macro definitions */
#define DEFAULT_HOST            "127.0.0.1" /* localhost */
#define VALID_HOST              g_ipAdr
#define VALID_PORT              3245

/* Request manager types */
#define Q_UNDEFINED_QUERY 		-1
#define Q_SA_QUERY 				1
#define Q_PA_QUERY  			2

/* Request type macro definitions */
#define Q_SA_CLASSPORTINFO		1
#define Q_SA_INFORM				2
#define Q_SA_NODE				3
#define Q_SA_SYSTEMGUID			4
#define Q_SA_NODEGUID			5
#define Q_SA_DESC				6
#define Q_SA_PORTGUID			7
#define Q_SA_LID				8
#define Q_SA_PORTINFO			9
#define Q_SA_SCSC				10
#define Q_SA_SWINFO				11
#define Q_SA_LINFDB				12
#define Q_SA_RANFDB				13
#define Q_SA_MCFDB				14
#define Q_SA_SMINFO				15
#define Q_SA_LINK				16
#define Q_SA_SERVICE			17
#define Q_SA_PKEY				18
#define Q_SA_PATH				19
#define Q_SA_VLARB				20	
#define Q_SA_MCMEMBER			21
#define Q_SA_TRACE				22	
#define Q_SA_SLSC				23
#define Q_SA_SCSL				24
#define Q_SA_SCVLNT				25
#define Q_SA_SCVLT				26
#define Q_SA_SCVLR				27
#define Q_SA_PGFDB				28
#define Q_SA_CABLEINFO			29
#define Q_SA_VFINFO				30
#define Q_SA_PORTGROUP			31
#define Q_SA_BUFCTRL			32
#define Q_SA_QUARANTINE			33
#define Q_SA_CONGINFO			34
#define Q_SA_SWCONG				35
#define Q_SA_SWPORTCONG			36
#define Q_SA_HFICONG			37
#define Q_SA_HFICONGCTRL		38
#define Q_SA_FABRICINFO			39
#define Q_LAST_SA 				39

#define Q_PA_GETGROUPLIST		40
#define Q_PA_GETGROUPINFO		41
#define Q_PA_GETGROUPCONFIG		42
#define Q_PA_GETPORTCOUNTERS	43
#define Q_PA_CLRPORTCOUNTERS	44
#define Q_PA_CLRALLPORTCOUNTERS	45
#define Q_PA_GETPMCONFIG		46
#define Q_PA_FREEZEIMAGE		47
#define Q_PA_RELEASEIMAGE		48
#define Q_PA_RENEWIMAGE			49
#define Q_PA_GETFOCUSPORTS		50
#define Q_PA_GETIMAGECONFIG		51
#define Q_PA_MOVEFREEZE			52
#define Q_PA_CLASSPORTINFO		53
#define Q_PA_GETVFLIST			54
#define Q_PA_GETVFINFO			55
#define Q_PA_GETVFCONFIG		56
#define Q_PA_GETVFPORTCOUNTERS	57
#define Q_PA_CLRVFPORTCOUNTERS	58
#define Q_PA_GETVFFOCUSPORTS	59
#define Q_LAST_PA 				59

/* Global variables */
PrintDest_t g_dest;
PrintDest_t g_dbgDest;
int g_verbose = 0;
uint8_t g_IB = 0; /* Perform query in legacy InfiniBand format */
uint8_t g_CSV = 0;  /* Output should use CSV style */
					/* only valid for OutputTypeVfInfoRecord */
					/* 1=absolute value for mtu and rate */
					/* 2=enum value for mtu and rate */

IB_GID g_sourceGid;
uint32_t g_gotSourceGid = 0;
uint16_t g_primaryPmLid= 0;
uint32_t g_nodeLid = 0;
uint8_t g_portNumber = 0;
uint32_t g_deltaFlag = 0;
uint32_t g_userCntrsFlag = 0;
uint32_t g_selectFlag = 0;
uint64_t g_imageNumber = 0;
int g_imageOffset = 0;
uint64_t g_moveImageNumber = 0;
int g_moveImageOffset = 0;
int g_focus = 0;
int g_start = 0;
int g_range= 0;
int g_feESM = 0;
uint32_t g_gotOutput= 0;
uint32_t g_gotGroup = 0;
uint32_t g_gotLid = 0;
uint32_t g_gotPort = 0;
uint32_t g_gotSelect	= 0;
uint32_t g_gotImgNum	= 0;
uint32_t g_gotImgOff	= 0;
uint32_t g_gotMoveImgNum	= 0;
uint32_t g_gotMoveImgOff	= 0;
uint32_t g_gotFocus = 0;
uint32_t g_gotStart = 0;
uint32_t g_gotRange = 0;
uint32_t g_gotvfName	= 0;
char g_groupName[STL_PM_GROUPNAMELEN];
char g_vfName[MAX_VFABRIC_NAME];
void *g_SDHandle = NULL;
char* g_sslParmsFile = "/etc/sysconfig/opa/opaff.xml";
sslParametersData_t g_sslParmsData;

static char g_ipAdr[16] = {DEFAULT_HOST};

static void *sslParmsXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr);
static void sslParmsXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid);

static IXML_FIELD sslSecurityFields[] = {
	{ tag:"SslSecurityEnabled", format:'u', IXML_FIELD_INFO(sslParametersData_t, sslSecurityEnabled) },
	{ tag:"SslSecurityDir", format:'s', IXML_FIELD_INFO(sslParametersData_t, sslSecurityDir) },
	{ tag:"SslSecurityFFCertificate", format:'s', IXML_FIELD_INFO(sslParametersData_t, sslSecurityFFCertificate) },
	{ tag:"SslSecurityFFPrivateKey", format:'s', IXML_FIELD_INFO(sslParametersData_t, sslSecurityFFPrivateKey) },
	{ tag:"SslSecurityFFCaCertificate", format:'s', IXML_FIELD_INFO(sslParametersData_t, sslSecurityFFCaCertificate) },
	{ tag:"SslSecurityFFCertChainDepth", format:'u', IXML_FIELD_INFO(sslParametersData_t, sslSecurityFFCertChainDepth) },
	{ tag:"SslSecurityFFDHParameters", format:'s', IXML_FIELD_INFO(sslParametersData_t, sslSecurityFFDHParameters) },
	{ tag:"SslSecurityFFCaCRLEnabled", format:'u', IXML_FIELD_INFO(sslParametersData_t, sslSecurityFFCaCRLEnabled) },
	{ tag:"SslSecurityFFCaCRL", format:'s', IXML_FIELD_INFO(sslParametersData_t, sslSecurityFFCaCRL) },
	{ NULL }
};

static IXML_FIELD sslTopLevelFields[] = {
	{ tag:"SslSecurityParameters", format:'K', subfields:sslSecurityFields, start_func:sslParmsXmlParserStart, end_func:sslParmsXmlParserEnd }, // structure
	{ NULL }
};

/* Command line options, each has a short and long flag name */
struct option options[] = {
		/* general options */
		{ "verbose", no_argument, NULL, 'v' },
		{ "IP Adr", required_argument, NULL, 'a' },
		{ "IP Adr", required_argument, NULL, 'h' },
		{ "output", required_argument, NULL, 'o' },
		{ "sslParmsFile", no_argument, NULL, 'T' },

		/* Options for both SA and PA */
		{ "lid", required_argument, NULL, 'l' },

		/* SA options */
		{ "IB", no_argument, NULL, 'I' },
		{ "pkey", required_argument, NULL, 'k' },
		{ "vfindex", required_argument, NULL, 'i' },
		{ "serviceId", required_argument, NULL, 'S' },
		{ "SL", required_argument, NULL, 'L' },
		{ "type", required_argument, NULL, 't' },
		{ "sysguid", required_argument, NULL, 's' },
		{ "nodeguid", required_argument, NULL, 'n' },
		{ "portguid", required_argument, NULL, 'p' },
		{ "portgid", required_argument, NULL, 'u' },
		{ "mcgid", required_argument, NULL, 'm' },
		{ "desc", required_argument, NULL, 'd' },
		{ "guidpair", required_argument, NULL, 'P' },
		{ "gidpair", required_argument, NULL, 'G' },
		{ "guidlist", required_argument, NULL, 'B' },
		{ "gidlist", required_argument, NULL, 'A' },

		/* PA options */
		{ "groupName", required_argument, NULL, 'g' },
		{ "portNumber", required_argument, NULL, 'N' },
		{ "delta", required_argument, NULL, 'f' },
		{ "userCntrs", no_argument, NULL, 'U' },
		{ "select", required_argument, NULL, 'e' },
		{ "imgNum", required_argument, NULL, 'b' },
		{ "imgOff", required_argument, NULL, 'O' },
		{ "moveImgNum", required_argument, NULL, 'F' },
		{ "moveImgOff", required_argument, NULL, 'M' },
		{ "focus", required_argument, NULL, 'c' },
		{ "start", required_argument, NULL, 'w' },
		{ "range", required_argument, NULL, 'r' },
		{ "vfName", required_argument, NULL, 'V' },
		{ "help", no_argument, NULL, '$' },
		{ 0 }
};


static void *sslParmsXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	sslParametersData_t *sslParmsData = IXmlParserGetContext(state);
    memset(sslParmsData, 0, sizeof(sslParametersData_t));
    return sslParmsData;
}

static void sslParmsXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
}

/**
 * Prints the Usage information and exits
 */
void Usage(void)
{
	fprintf(stderr, "Usage: opafequery [-v] [-a ipAddr | -h hostName] [-E] [-T paramsfile]\n");
	fprintf(stderr, "                     -o type [SA options | PA options]\n");

	fprintf(stderr, "General Options:\n");
	fprintf(stderr, "    --help                    - produce this help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -a/--ipAdr                - IP address of node running the FE\n");
	fprintf(stderr, "    -h/--hostName             - host name of node running the FE\n");
	fprintf(stderr, "    -o/--output               - output type\n");
	fprintf(stderr, "    -E/--feEsm                - ESM FE\n");
	fprintf(stderr, "    -T/--sslParmsFile         - SSL/TLS parameters XML file\n");

	fprintf(stderr, "SA Specific Options:\n");
	fprintf(stderr, "    -I/--IB                   - issue query in legacy InfiniBand format\n");
	fprintf(stderr, "    -l/--lid lid              - query a specific lid\n");
	fprintf(stderr, "    -k/--pkey pkey            - query a specific pkey\n");
	fprintf(stderr, "    -i/--vfindex vfindex      - query a specific vfindex\n");
	fprintf(stderr, "    -S/--serviceId serviceId  - query a specific service id\n");
	fprintf(stderr, "    -L/--SL SL                - query by service level\n");
	fprintf(stderr, "    -t/--type type            - query by node type\n");
	fprintf(stderr, "    -s/--sysguid guid         - query by system image guid\n");
	fprintf(stderr, "    -n/--nodeguid guid        - query by node guid\n");
	fprintf(stderr, "    -p/--portguid guid        - query by port guid\n");
	fprintf(stderr, "    -u/--portgid gid          - query by port gid\n");
	fprintf(stderr, "    -m/--mcgid gid            - query by multicast gid\n");
	fprintf(stderr, "    -d/--desc name            - query by node name/description\n");
	fprintf(stderr, "    -P/--guidpair 'guid guid' - query by a pair of port Guids\n");
	fprintf(stderr, "    -G/--gidpair 'gid gid'    - query by a pair of Gids\n");
	fprintf(stderr, "    -B/--guidlist 'sguid ...;dguid ...'\n");
	fprintf(stderr, "                              - query by a list of port Guids\n");
	fprintf(stderr, "    -A/--gidlist 'sgid ...;dgid ...'\n");
	fprintf(stderr, "                              - query by a list of Gids\n");
	fprintf(stderr, "    -x/--sourcegid gid        - specify a source gid for certain queries\n");
	fprintf(stderr, "PA Specific Options:\n");
	fprintf(stderr, "    -g/--groupName            - group name for groupInfo query\n");
	fprintf(stderr, "    -l/--lid                  - lid of node for portCounters query\n");
	fprintf(stderr, "    -N/--portNumber           - port number for portCounters query\n");
	fprintf(stderr, "    -f/--delta                - delta flag for portCounters query - 0 or 1\n");
	fprintf(stderr, "    -U/--userCntrs            - user controlled counters flag for portCounters\n");
	fprintf(stderr, "                                query\n");
	fprintf(stderr, "    -e/--select               - 32-bit select flag for clearing port counters\n");
	fprintf(stderr, "                                select bits (0 is least signficant (rightmost))\n");
	fprintf(stderr, "       0 - XmitData               14 - MarkFECN\n");
	fprintf(stderr, "       1 - RcvData                15 - RcvConstraintErrors\n");
	fprintf(stderr, "       2 - XmitPkts               16 - RcvSwitchRelayErrors\n");
	fprintf(stderr, "       3 - RcvPkts                17 - XmitDiscards\n");
	fprintf(stderr, "       4 - MulticastXmitPkts      18 - XmitConstraintErrors\n");
	fprintf(stderr, "       5 - MulticastRcvPkts       19 - RcvRemotePhysicalErrors\n");
	fprintf(stderr, "       6 - XmitWait               20 - LocalLinkIntegrityErrors\n");
	fprintf(stderr, "       7 - CongDiscards           21 - RcvErrors\n");
	fprintf(stderr, "       8 - RcvFECN                22 - ExcessiveBufferOverruns\n");
	fprintf(stderr, "       9 - RcvBECN                23 - FMConfigErrors\n");
	fprintf(stderr, "      10 - XmitTimeCong           24 - LinkErrorRecovery\n");
	fprintf(stderr, "      11 - XmitWastedBW           25 - LinkDowned\n");
	fprintf(stderr, "      12 - XmitWaitData           26 - UncorrectableErrors\n");
	fprintf(stderr, "      13 - RcvBubble\n");
	fprintf(stderr, "    -c/--focus                - focus select value for getting focus ports\n");
	fprintf(stderr, "                                focus select values:\n");
	fprintf(stderr, "         0x00020001 - sorted by utilization - highest first\n");                  // STL_PA_SELECT_UTIL_HIGH         0x00020001
//	fprintf(stderr, "         0x00020081 - sorted by mulicast pkt rate - highest first\n");            // STL_PA_SELECT_UTIL_MC_HIGH      0x00020081
	fprintf(stderr, "         0x00020082 - sorted by packet rate - highest first\n");                  // STL_PA_SELECT_UTIL_PKTS_HIGH    0x00020082
	fprintf(stderr, "         0x00020101 - sorted by utilization - lowest first\n");                   // STL_PA_SELECT_UTIL_LOW          0x00020101
//	fprintf(stderr, "         0x00020102 - sorted by mulicast pkt rate - lowest first\n");             // STL_PA_SELECT_UTIL_MC_LOW       0x00020102
	fprintf(stderr, "         0x00030001 - sorted by integrity errors - highest first\n");             // STL_PA_SELECT_ERR_INTEG         0x00030001
	fprintf(stderr, "         0x00030002 - sorted by congestion errors - highest first\n");            // STL_PA_SELECT_ERR_CONG          0x00030002
	fprintf(stderr, "         0x00030003 - sorted by sma congestion errors - highest first\n");        // STL_PA_SELECT_ERR_SMA_CONG      0x00030003
	fprintf(stderr, "         0x00030004 - sorted by bubble errors - highest first\n");                // STL_PA_SELECT_ERR_BUBBLE        0x00030004
	fprintf(stderr, "         0x00030005 - sorted by security errors - highest first\n");              // STL_PA_SELECT_ERR_SEC           0x00030005
	fprintf(stderr, "         0x00030006 - sorted by routing errors - highest first\n");               // STL_PA_SELECT_ERR_ROUT          0x00030006
	fprintf(stderr, "    -w/--start                - start of window for focus ports - should always\n");
	fprintf(stderr, "                                be 0 for now\n");
	fprintf(stderr, "    -r/--range                - size of window for focus ports list\n");
	fprintf(stderr, "    -b/--imgNum               - 64-bit image number - may be used with,\n");
	fprintf(stderr, "                                groupInfo, groupConfig, portCounters (delta)\n");
	fprintf(stderr, "    -O/--imgOff               - image offset - may be used with groupInfo,\n");
	fprintf(stderr, "                                groupConfig, portCounters (delta)\n");
	fprintf(stderr, "    -F/--moveImgNum           - 64-bit image number - used with moveFreeze to\n");
	fprintf(stderr, "                                move a freeze image\n");
	fprintf(stderr, "    -M/--moveImgOff           - image offset - may be used with moveFreeze to\n");
	fprintf(stderr, "                                move a freeze image\n");
	fprintf(stderr, "    -V/--vfName               - VF name for vfInfo query\n");

	fprintf(stderr, "SA Output Types:\n");
	fprintf(stderr, "    saclassPortInfo           - class port info\n");
	fprintf(stderr, "    systemguid                - list of system image guids\n");
	fprintf(stderr, "    nodeguid                  - list of node guids\n");
	fprintf(stderr, "    portguid                  - list of port guids\n");
	fprintf(stderr, "    lid                       - list of lids\n");
	fprintf(stderr, "    desc                      - list of node descriptions/names\n");
	fprintf(stderr, "    path                      - list of path records\n");
	fprintf(stderr, "    node                      - list of node records\n");
	fprintf(stderr, "    portinfo                  - list of port info records\n");
	fprintf(stderr, "    sminfo                    - list of SM info records\n");
	fprintf(stderr, "    swinfo                    - list of switch info records\n");
	fprintf(stderr, "    link                      - list of link records\n");
	fprintf(stderr, "    scsc                      - list of SC to SC mapping table records\n");
	fprintf(stderr, "    slsc                      - list of SL to SC mapping table records\n");
	fprintf(stderr, "    scsl                      - list of SC to SL mapping table records\n");
	fprintf(stderr, "    scvlt                     - list of SC to VLt table records\n");
	fprintf(stderr, "    scvlnt                    - list of SC to VLnt table records\n");
	fprintf(stderr, "    vlarb                     - list of VL arbitration table records\n");
	fprintf(stderr, "    pkey                      - list of P-Key table records\n");
	fprintf(stderr, "    service                   - list of service records\n");
	fprintf(stderr, "    mcmember                  - list of multicast member records\n");
	fprintf(stderr, "    inform                    - list of inform info records\n");
	fprintf(stderr, "    linfdb                    - list of switch linear FDB records\n");
	fprintf(stderr, "    ranfdb                    - list of switch random FDB records\n");
	fprintf(stderr, "    mcfdb                     - list of switch multicast FDB records\n");
	fprintf(stderr, "    trace                     - list of trace records\n");
	fprintf(stderr, "    vfinfo                    - list of vFabrics\n");
	fprintf(stderr, "    vfinfocsv                 - list of vFabrics in CSV format\n");
	fprintf(stderr, "    vfinfocsv2                - list of vFabrics in CSV format with enums\n");
	fprintf(stderr, "    fabricinfo                - summary of fabric devices\n");
	fprintf(stderr, "    quarantine                - list of quarantined nodes\n");
	fprintf(stderr, "    conginfo                  - list of Congestion Info Records\n");
	fprintf(stderr, "    swcongset                 - list of Switch Congestion Settings\n");
	//fprintf(stderr, "    swportcong  - list of Switch Port Congestion Settings\n");  // Not Implemented in SM/SA
	fprintf(stderr, "    hficongset                - list of HFI Congestion Settings\n");
	fprintf(stderr, "    hficongcon                - list of HFI Congesting Control Settings\n");
	fprintf(stderr, "    bfrctrl                   - list of buffer control tables\n");
	fprintf(stderr ,"    cableinfo                 - list of Cable Info records\n");
	fprintf(stderr ,"    portgroup                 - list of AR Port Group records\n");
	fprintf(stderr ,"    portgroupfdb              - list of AR Port Group FWD records\n");

	fprintf(stderr, "PA Output Types:\n");
	fprintf(stderr, "    paclassPortInfo           - class port info\n");
	fprintf(stderr, "    groupList                 - list of PA groups\n");
	fprintf(stderr, "    groupInfo                 - summary statistics of a PA group - requires -g\n");
	fprintf(stderr, "                                option for groupName\n");
	fprintf(stderr, "    groupConfig               - configuration of a PA group - requires -g option\n");
	fprintf(stderr, "                                for groupName\n");
	fprintf(stderr, "    portCounters              - port counters of fabric port - requires -l (lid)\n");
	fprintf(stderr, "                                and -N (port) options, -f (delta) is optional\n");
	fprintf(stderr, "    clrPortCounters           - clear port counters of fabric port - requires -l\n");
	fprintf(stderr, "                                (lid), -N (port), and -e (select) options\n");
	fprintf(stderr, "    clrAllPortCounters        - clear all port counters in fabric\n");
	fprintf(stderr, "    pmConfig                  - retrieve PM configuration information\n");
	fprintf(stderr, "    freezeImage               - create freeze frame for image ID - requires -b\n");
	fprintf(stderr, "                                (imgNum)\n");
	fprintf(stderr, "    releaseImage              - release freeze frame for image ID - requires -b\n");
	fprintf(stderr, "                                (imgNum)\n");
	fprintf(stderr, "    renewImage                - renew lease for freeze frame for image ID -\n");
	fprintf(stderr, "                                requires -b (imgNum)\n");
	fprintf(stderr, "    moveFreeze                - move freeze frame from image ID to new image ID\n");
	fprintf(stderr, "                                - requires -b (imgNum) and -F (moveImgNum)\n");
	fprintf(stderr, "    focusPorts                - get sorted list of ports using utilization or\n");
	fprintf(stderr, "                                error values (from group buckets)\n");
	fprintf(stderr, "    imageInfo                 - get information about a PA image (timestamps,\n");
	fprintf(stderr, "                                etc.) - requires -b (imgNum)\n");
	fprintf(stderr, "    vfList                    - list of virtual fabrics\n");
	fprintf(stderr, "    vfInfo                    - summary statistics of a virtual fabric -\n");
	fprintf(stderr, "                                requires -V option for vfName\n");
	fprintf(stderr, "    vfConfig                  - configuration of a virtual fabric - requires -V\n");
	fprintf(stderr, "                                option for vfName\n");
	fprintf(stderr, "    vfPortCounters            - port counters of fabric port - requires -V\n");
	fprintf(stderr, "                                (vfName), -l (lid) and -N (port) options, -f\n");
	fprintf(stderr, "                                (delta) is optional\n");
	fprintf(stderr, "    vfFocusPorts              - get sorted list of virtual fabric ports using\n");
	fprintf(stderr, "                                utilization or error values (from VF buckets) -\n");
	fprintf(stderr, "                                requires -V (vfname)\n");
	fprintf(stderr, "    clrVfPortCounters         - clear VF port counters of fabric port - requires\n");
	fprintf(stderr, "                                -l (lid), -N (port), -e (select), and -V (vfname)\n");
	fprintf(stderr, "                                options\n");

	exit(2);
}

/**
 * Returns a #def type relating to the type of request on the -o flag
 *
 * @param outputType Pointer to the string containing the -o flag in argv
 * @return The #def type represented by the input
 */
int getOutputType(const char* outputType)
{
	if (0 == strcasecmp(optarg, "saclassPortInfo")) {
		return Q_SA_CLASSPORTINFO;
	} else if (0 == strcmp(optarg, "inform")) {
		return Q_SA_INFORM;
	} else if (0 == strcmp(optarg, "node")) {
		return Q_SA_NODE;		
	} else if (0 == strcmp(optarg, "systemguid")) {
		return Q_SA_SYSTEMGUID;
	} else if (0 == strcmp(optarg, "nodeguid")) {
		return Q_SA_NODEGUID;
	} else if (0 == strcmp(optarg, "desc")) {
		return Q_SA_DESC;
	} else if (0 == strcmp(optarg, "portguid")) {
		return Q_SA_PORTGUID;
	} else if (0 == strcmp(optarg, "lid")) {
		return Q_SA_LID;
	} else if (0 == strcmp(optarg, "portinfo")) {
		return Q_SA_PORTINFO;
	} else if (0 == strcmp(optarg, "scsc")) {
		return Q_SA_SCSC;
	} else if (0 == strcmp(optarg, "swinfo")) {
		return Q_SA_SWINFO;
	} else if (0 == strcmp(optarg, "linfdb")) {
		return Q_SA_LINFDB;
	} else if (0 == strcmp(optarg, "ranfdb")) {
		return Q_SA_RANFDB;
	} else if (0 == strcmp(optarg, "mcfdb")) {
		return Q_SA_MCFDB;
	} else if (0 == strcmp(optarg, "sminfo")) {
		return Q_SA_SMINFO;
	} else if (0 == strcmp(optarg, "link")) {
		return Q_SA_LINK;
	} else if (0 == strcmp(optarg, "service")) {
		return Q_SA_SERVICE;
	} else if (0 == strcmp(optarg, "pkey")) {
		return Q_SA_PKEY;
	} else if (0 == strcmp(optarg, "path")) {
		return Q_SA_PATH;
	} else if (0 == strcmp(optarg, "vlarb")) {
		return Q_SA_VLARB;
	} else if (0 == strcmp(optarg, "mcmember")) {
		return Q_SA_MCMEMBER;
	} else if (0 == strcmp(optarg, "trace")) {
		return Q_SA_TRACE;
	} else if (0 == strcmp(optarg, "slsc")) {
		return Q_SA_SLSC;
	} else if (0 == strcmp(optarg, "scsl")) {
		return Q_SA_SCSL;
	} else if (0 == strcmp(optarg, "scvlnt")) {
		return Q_SA_SCVLNT;
	} else if (0 == strcmp(optarg, "scvlt")) {
		return Q_SA_SCVLT;
	} else if (0 == strcmp(optarg, "portgroupfdb")) {
		return Q_SA_PGFDB;
	} else if (0 == strcmp(optarg, "cableinfo")) {
		return Q_SA_CABLEINFO;
	} else if (0 == strcmp(optarg, "vfinfo")) {
		return Q_SA_VFINFO;
	} else if (0 == strcmp(optarg, "vfinfocsv")) {
		g_CSV=1;
		return Q_SA_VFINFO;
	} else if (0 == strcmp(optarg, "vfinfocsv2")) {
		g_CSV=2;
		return Q_SA_VFINFO;
	} else if (0 == strcmp(optarg, "portgroup")) {
		return Q_SA_PORTGROUP;
	} else if (0 == strcmp(optarg, "bfrctrl")) {
		return Q_SA_BUFCTRL;
	} else if (0 == strcmp(optarg, "fabricinfo")) {
		return Q_SA_FABRICINFO;
	} else if (0 == strcmp(optarg, "quarantine")) {
		return Q_SA_QUARANTINE;
	} else if (0 == strcmp(optarg, "conginfo")) {
		return Q_SA_CONGINFO;
	} else if (0 == strcmp(optarg, "swcongset")) {
		return Q_SA_SWCONG;
	} else if (0 == strcmp(optarg, "swportcong")) {
		return Q_SA_SWPORTCONG;
	} else if (0 == strcmp(optarg, "hficongset")) {
		return Q_SA_HFICONG;
	} else if (0 == strcmp(optarg, "hficongcon")) {
		return Q_SA_HFICONGCTRL;
	} else if (0 == strcasecmp(optarg, "groupList")) {
		return Q_PA_GETGROUPLIST;
	} else if (0 == strcasecmp(optarg, "groupInfo")) {
		return Q_PA_GETGROUPINFO;
	} else if (0 == strcasecmp(optarg, "groupConfig")) {
		return Q_PA_GETGROUPCONFIG;
	} else if (0 == strcasecmp(optarg, "portCounters")) {
		return Q_PA_GETPORTCOUNTERS;
	} else if (0 == strcasecmp(optarg, "clrPortCounters")) {
		return Q_PA_CLRPORTCOUNTERS;
	} else if (0 == strcasecmp(optarg, "clrAllPortCounters")) {
		return Q_PA_CLRALLPORTCOUNTERS;
	} else if (0 == strcasecmp(optarg, "pmConfig")) {
		return Q_PA_GETPMCONFIG;
	} else if (0 == strcasecmp(optarg, "freezeImage")) {
		return Q_PA_FREEZEIMAGE;
	} else if (0 == strcasecmp(optarg, "releaseImage")) {
		return Q_PA_RELEASEIMAGE;
	} else if (0 == strcasecmp(optarg, "renewImage")) {
		return Q_PA_RENEWIMAGE;
	} else if (0 == strcasecmp(optarg, "moveFreeze")) {
		return Q_PA_MOVEFREEZE;
	} else if (0 == strcasecmp(optarg, "focusPorts")) {
		return Q_PA_GETFOCUSPORTS;
	} else if (0 == strcasecmp(optarg, "imageInfo")) {
		return Q_PA_GETIMAGECONFIG;
	} else if (0 == strcasecmp(optarg, "paclassPortInfo")) {
		return Q_PA_CLASSPORTINFO;
	} else if (0 == strcasecmp(optarg, "vfList")) {
		return Q_PA_GETVFLIST;
	} else if (0 == strcasecmp(optarg, "vfInfo")) {
		return Q_PA_GETVFINFO;
	} else if (0 == strcasecmp(optarg, "vfConfig")) {
		return Q_PA_GETVFCONFIG;
	} else if (0 == strcasecmp(optarg, "vfPortCounters")) {
		return Q_PA_GETVFPORTCOUNTERS;
	} else if (0 == strcasecmp(optarg, "clrVfPortCounters")) {
		return Q_PA_CLRVFPORTCOUNTERS;
	} else if (0 == strcasecmp(optarg, "vfFocusPorts")) {
		return Q_PA_GETVFFOCUSPORTS;
	} else {
		fprintf(stderr, "opafequery: Invalid Output Type: %s\n", outputType);
		Usage();
		// NOTREACHED
		return 0;
	}
}

/**
 * Handles attempted multiple input search parameters by ignoring all but the first
 *
 * @param inputType The input type parameter attempting to be the search param
 */
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
	fprintf(stderr, "opafequery: multiple selection criteria not supported for SA requests, ignoring %s\n", typestr);
}

/**
 * Convert a node type argument to the proper constant
 *
 * @param name Pointer to the string representing the node type
 * @return A #def type representing the node type
 */
NODE_TYPE checkNodeType(const char* name)
{
	if (0 == strcasecmp(optarg, "ca")) {
		return STL_NODE_FI;
	} else if (0 == strcasecmp(optarg, "sw")) {
		return STL_NODE_SW;
	} else {
		fprintf(stderr, "opafequery: Invalid Node Type: %s\n", name);
		Usage();
		// NOTREACHED
		return STL_NODE_FI;
	}
}

/**
 * Handles signals generated by the user and exits cleanly
 *
 * @param sig The signal to handle
 */
static void signal_handler(int sig)
{
    fprintf(stderr, "%s: Warning, abort request received, terminating!\n", APP_NAME);
    exit(-1);
}

/**
 * Get type of manager this request goes to based on the query type
 * 
 * @param queryType The resolved input flag -o query type
 * @return A #def type relating to the manager, for example Q_SA_QUERY
 */
int fe_getManagerFromQueryType(int queryType)
{
	if(queryType > 0 && queryType <= Q_LAST_SA)
		return Q_SA_QUERY;
	else if(queryType > Q_LAST_SA && queryType <= Q_LAST_PA)
		return Q_PA_QUERY;
	else
		return Q_UNDEFINED_QUERY;
}

/**
 * Get the SA Query OutputType from the input -o query
 *
 * @param queryType The respolved input flag -o query type
 * @return The corresponding SA OutputType
 */
int fe_getSAOutputTypeFromQueryType(int queryType)
{
	switch(queryType){
	case Q_SA_CLASSPORTINFO:
		return OutputTypeStlClassPortInfo;
	case Q_SA_INFORM:
		if (g_IB) return OutputTypeInformInfoRecord;
		else return OutputTypeStlInformInfoRecord;
	case Q_SA_NODE:
		if(g_IB) return OutputTypeNodeRecord;
		else return OutputTypeStlNodeRecord;
	case Q_SA_SYSTEMGUID:
		return OutputTypeStlSystemImageGuid;
	case Q_SA_NODEGUID:
		return OutputTypeNodeGuid;
	case Q_SA_DESC:
		if(g_IB) return OutputTypeNodeDesc;
		else return OutputTypeStlNodeDesc;
	case Q_SA_PORTGUID:
		return OutputTypeStlPortGuid;
	case Q_SA_LID:
		return OutputTypeStlLid;
	case Q_SA_PORTINFO:
		if(g_IB) return OutputTypePortInfoRecord;
		else return OutputTypeStlPortInfoRecord;
	case Q_SA_SCSC:
		return OutputTypeStlSCSCTableRecord;
	case Q_SA_SWINFO:
		return OutputTypeStlSwitchInfoRecord;
	case Q_SA_LINFDB:
		return OutputTypeStlLinearFDBRecord;
	case Q_SA_RANFDB:
		return OutputTypeRandomFDBRecord;
	case Q_SA_MCFDB:
		return OutputTypeStlMCastFDBRecord;
	case Q_SA_SMINFO:
		return OutputTypeStlSMInfoRecord;
	case Q_SA_LINK:
		return OutputTypeStlLinkRecord;
	case Q_SA_SERVICE:
#ifndef NO_STL_SERVICE_OUTPUT       // Don't output STL Service if defined
		if (g_IB) return OutputTypeServiceRecord;
		else return OutputTypeStlServiceRecord;
#else
		return OutputTypeServiceRecord;
#endif
	case Q_SA_PKEY:
		return OutputTypeStlPKeyTableRecord;
	case Q_SA_PATH:
		return OutputTypePathRecord;
	case Q_SA_VLARB:
		return OutputTypeStlVLArbTableRecord;
	case Q_SA_MCMEMBER:
#ifndef NO_STL_MCMEMBER_OUTPUT       // Don't output STL McMember if defined
		if (g_IB) return OutputTypeMcMemberRecord;
		else return OutputTypeStlMcMemberRecord;
#else
		return OutputTypeMcMemberRecord;
#endif
	case Q_SA_TRACE:
		return OutputTypeStlTraceRecord;
	case Q_SA_SLSC:
		return OutputTypeStlSLSCTableRecord;
	case Q_SA_SCSL:
		return OutputTypeStlSCSLTableRecord;
	case Q_SA_SCVLNT:
		return OutputTypeStlSCVLntTableRecord;
	case Q_SA_SCVLT:
		return OutputTypeStlSCVLtTableRecord;
	case Q_SA_PGFDB:
		return OutputTypeStlPortGroupFwdRecord;
	case Q_SA_CABLEINFO:
		return OutputTypeStlCableInfoRecord;
	case Q_SA_VFINFO:
		return OutputTypeStlVfInfoRecord;
	case Q_SA_PORTGROUP:
		return OutputTypeStlPortGroupRecord;
	case Q_SA_BUFCTRL:
		return OutputTypeStlBufCtrlTabRecord;
	case Q_SA_FABRICINFO:
		return OutputTypeStlFabricInfoRecord;
	case Q_SA_QUARANTINE:
		return OutputTypeStlQuarantinedNodeRecord;
	case Q_SA_CONGINFO:
		return OutputTypeStlCongInfoRecord;
	case Q_SA_SWCONG:
		return OutputTypeStlSwitchCongRecord;
	case Q_SA_SWPORTCONG:
		return OutputTypeStlSwitchPortCongRecord;
    case Q_SA_HFICONG:
        return OutputTypeStlHFICongRecord;
    case Q_SA_HFICONGCTRL:
        return OutputTypeStlHFICongCtrlRecord;
	default:
		return OutputTypeStlNodeRecord;
	}
}

FSTATUS fe_runPAQueryFromQueryType(int queryType, struct net_connection *connection)
{
	FSTATUS fstatus;

	switch (queryType)
	{
		case Q_PA_GETGROUPLIST:
			fstatus = fe_GetGroupList(connection);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get group list\n", APP_NAME);
			}
			break;

		case Q_PA_GETGROUPINFO:
			fstatus = fe_GetGroupInfo(connection, g_groupName, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get group info\n", APP_NAME);
			}
			break;

		case Q_PA_GETGROUPCONFIG:
			fstatus = fe_GetGroupConfig(connection, g_groupName, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get group configuration\n", APP_NAME);
			}
			break;

		case Q_PA_GETPORTCOUNTERS:
			fstatus = fe_GetPortCounters(connection, g_nodeLid, g_portNumber, g_deltaFlag, g_userCntrsFlag, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get port counters\n", APP_NAME);
			}
			break;

		case Q_PA_CLRPORTCOUNTERS:
			fstatus = fe_ClrPortCounters(connection, g_nodeLid, g_portNumber, g_selectFlag);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to clear port counters\n", APP_NAME);
			}
			break;

		case Q_PA_CLRALLPORTCOUNTERS:
			fstatus = fe_ClrAllPortCounters(connection, g_selectFlag);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to clear all port counters\n", APP_NAME);
			}
			break;

		case Q_PA_GETPMCONFIG:
			fstatus = fe_GetPMConfig(connection);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get PM configuration\n", APP_NAME);
			}
			break;

		case Q_PA_FREEZEIMAGE:
			fstatus = fe_FreezeImage(connection, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to freeze image\n", APP_NAME);
			}
			break;

		case Q_PA_RELEASEIMAGE:
			fstatus = fe_ReleaseImage(connection, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to release image\n", APP_NAME);
			}
			break;

		case Q_PA_RENEWIMAGE:
			fstatus = fe_RenewImage(connection, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to renew image\n", APP_NAME);
			}
			break;

		case Q_PA_MOVEFREEZE:
			fstatus = fe_MoveFreeze(connection, g_imageNumber, g_imageOffset, g_moveImageNumber, g_moveImageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to move freeze image\n", APP_NAME);
			}
			break;

		case Q_PA_GETFOCUSPORTS:
			fstatus = fe_GetFocusPorts(connection, g_groupName, g_focus, g_start, g_range, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get focus ports\n", APP_NAME);
			}
			break;

		case Q_PA_GETIMAGECONFIG:
			fstatus = fe_GetImageInfo(connection, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get image info\n", APP_NAME);
			}
			break;

		case Q_PA_CLASSPORTINFO:
			fstatus = fe_GetClassPortInfo(connection);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get class port info\n", APP_NAME);
			}
			break;

		case Q_PA_GETVFLIST:
			fstatus = fe_GetVFList(connection);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get vf list\n", APP_NAME);
			}
			break;

		case Q_PA_GETVFINFO:
			fstatus = fe_GetVFInfo(connection, g_vfName, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get VF info\n", APP_NAME);
			}
			break;

		case Q_PA_GETVFCONFIG:
			fstatus = fe_GetVFConfig(connection, g_vfName, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get vf configuration\n", APP_NAME);
			}
			break;

		case Q_PA_GETVFPORTCOUNTERS:
			fstatus = fe_GetVFPortCounters(connection, g_nodeLid, g_portNumber, g_deltaFlag, g_userCntrsFlag, g_vfName, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get vf port counters\n", APP_NAME);
			}
			break;

		case Q_PA_CLRVFPORTCOUNTERS:
			fstatus = fe_ClrVFPortCounters(connection, g_nodeLid, g_portNumber, g_selectFlag, g_vfName);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to clear vf port counters\n", APP_NAME);
			}
			break;

		case Q_PA_GETVFFOCUSPORTS:
			fstatus = fe_GetVFFocusPorts(connection, g_vfName, g_focus, g_start, g_range, g_imageNumber, g_imageOffset);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: failed to get VF focus ports\n", APP_NAME);
			}
			break;

		/* We should never end up here */
		default:
			fstatus = FERROR;
			break;
	}

	return fstatus;
}

FSTATUS fe_xml2ParseSslParmsFile(const char *input_file, int g_verbose, sslParametersData_t *sslParmsData)
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

/**
 * Process the query received from the input parameters and give it to the appropriate query handler
 *
 * @param queryType The resolved input flag -o query type
 * @param sa_query Pointer to the QUERY structure inputs built when issuing an SA query
 * @return FSTATUS return code
 */
FSTATUS fe_processQuery(int queryType, PQUERY saQuery)
{
	FSTATUS fstatus = FSUCCESS; /* Return status */
	int queryManager = fe_getManagerFromQueryType(queryType); /* The manager this request goes to */
	struct net_connection *connection = NULL; /* Our connection to the FE */
	struct login_attr attr; /* Login information */
	attr.host = VALID_HOST;
	attr.port = VALID_PORT;

	/* Connect to the FE */
	if(fe_oob_connect(&attr, &connection) != FSUCCESS)
		return FERROR;

	/* Process the request and send it to the appropriate manager */
	switch(queryManager){
	case Q_UNDEFINED_QUERY:
		/* This should never be reached */
		return FERROR;
	case Q_SA_QUERY:
		{
			PQUERY_RESULT_VALUES pQueryResults;

			saQuery->OutputType = fe_getSAOutputTypeFromQueryType(queryType);
			fstatus = fe_processSAQuery(saQuery, connection, &pQueryResults);

			PrintQueryResult(&g_dest, 0, &g_dbgDest, saQuery->OutputType, g_CSV, fstatus, pQueryResults);
		}
		break;
	case Q_PA_QUERY:
		fe_runPAQueryFromQueryType(queryType, connection);
		break;
	}

	/* Disconnect from the FE and return the error code (if any) */
	fe_oob_disconnect(connection);
	return fstatus;
}

/**
 * Main program loop
 *
 * @param argc Count of the command line arguments
 * @param argv Pointer to the list of command line arguments
 * @return Program return code
 */
int main (int argc, char *argv[])
{
    FSTATUS fstatus = FERROR;
    int c, index;
	char outputType[64];
	int queryType = 0;
	uint32_t temp32;
	uint8 temp8;

	/* Allocate an SA query in case we need it */
	QUERY query;
	memset(&query, 0, sizeof(query));
	query.InputType = InputTypeNoInput;
	query.OutputType = OutputTypeStlNodeRecord;


    /* Process command line arguments */
	while (-1 != (c = getopt_long(argc, argv, "va:h:o:l:Ik:i:S:L:t:s:n:p:u:m:d:P:G:a:A:g:N:f:Ue:c:w:r:b:O:F:M:x:V:T:E", options, &index))) {
        switch (c) {
		/* (General) Verbose flag */
        case '$':
            Usage();
            break;
        case 'v':
            g_verbose = 1;
            break;

		/* (General) IP Address */
        case 'a':
            memset(g_ipAdr, 0, sizeof(g_ipAdr));
            memcpy(g_ipAdr, optarg, min(strlen(optarg),sizeof(g_ipAdr)-1));
            break;

		/* (General) Hostname */
        case 'h':
            {
                struct hostent * hp = gethostbyname(optarg);
                struct in_addr * sin_addr;

                if (hp == NULL) {
                    fprintf(stderr, "%s: Unknown host\n", APP_NAME);
                    Usage();
                }
                sin_addr = (struct in_addr *)hp->h_addr;
                memcpy(g_ipAdr, inet_ntoa(*sin_addr), min(strlen(inet_ntoa(*sin_addr)),sizeof(g_ipAdr)-1));
            }
            break;

		/* (General) Output type */
        case 'o':
            memcpy(outputType, optarg, 64);
            if ((queryType = getOutputType(outputType)) <= 0) {
                Usage();
            }
            g_gotOutput = 1;
            break;


		/* (SA,PA) Lid */
        case 'l':
            if (FSUCCESS != StringToUint32(&temp32, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Lid Number: %s\n", APP_NAME, optarg);
                Usage();
            }
			multiInputCheck(query.InputType);
			query.InputType = InputTypeLid;
			query.InputValue.Lid = (uint16_t)temp32;
            g_nodeLid = temp32;
            g_gotLid = TRUE;
            break;

		/* (SA) IB Flag */
		case 'I':
			g_IB = 1;
			break;

		/* (SA) pkey */
		case 'k':
			multiInputCheck(query.InputType);
			query.InputType = InputTypePKey;
			if (FSUCCESS != StringToUint16(&query.InputValue.PKey, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opafequery: Invalid PKey: %s\n", optarg);
				Usage();
			}
			break;

		/* (SA) vfindex */
		case 'i':
			multiInputCheck(query.InputType);
			query.InputType = InputTypeIndex;
			if (FSUCCESS != StringToUint16(&query.InputValue.vfIndex, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opafequery: Invalid vfIndex: %s\n", optarg);
				Usage();
			}
			break;

		/* (SA) serviceId */
		case 'S':
			multiInputCheck(query.InputType);
			query.InputType = InputTypeServiceId;
			if (FSUCCESS != StringToUint64(&query.InputValue.ServiceId, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opafequery: Invalid ServiceId: %s\n", optarg);
				Usage();
			}
			break;

		/* (SA) SL */
		case 'L':
			multiInputCheck(query.InputType);
			query.InputType = InputTypeSL;
			if (FSUCCESS != StringToUint8(&query.InputValue.SL, optarg, NULL, 0, TRUE)
				|| query.InputValue.SL > 15) {
				fprintf(stderr, "opafequery: Invalid SL: %s\n", optarg);
				Usage();
			}
			break;

		/* (SA) type */
		case 't':
			multiInputCheck(query.InputType);
			query.InputType = InputTypeNodeType;
			query.InputValue.TypeOfNode = checkNodeType(optarg);
			break;

		/* (SA) sysguid */
		case 's':
			multiInputCheck(query.InputType);
			query.InputType = InputTypeServiceId;
			if (FSUCCESS != StringToUint64(&query.InputValue.ServiceId, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opafequery: Invalid ServiceId: %s\n", optarg);
				Usage();
			}
			break;

		/* (SA) nodeguid */
		case 'n':
			multiInputCheck(query.InputType);
			query.InputType = InputTypeNodeGuid;
			if (FSUCCESS != StringToUint64(&query.InputValue.Guid, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opafequery: Invalid GUID: %s\n", optarg);
				Usage();
			}
            break;

		/* (SA) portguid */
		case 'p':
			multiInputCheck(query.InputType);
			query.InputType = InputTypePortGuid;
			if (FSUCCESS != StringToUint64(&query.InputValue.Guid, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opafequery: Invalid GUID: %s\n", optarg);
				Usage();
			}
			break;

		/* (SA) portgid */
		case 'u':
			multiInputCheck(query.InputType);
			query.InputType = InputTypePortGid;
			if (FSUCCESS != StringToGid(&query.InputValue.Gid.AsReg64s.H,&query.InputValue.Gid.AsReg64s.L, optarg, NULL, TRUE)) {
				fprintf(stderr, "opafequery: Invalid GID: %s\n", optarg);
				Usage();
			}
			break;

		/* (SA) mcgid */
		case 'm':
			multiInputCheck(query.InputType);
			query.InputType = InputTypeMcGid;
			if (FSUCCESS != StringToGid(&query.InputValue.Gid.AsReg64s.H,&query.InputValue.Gid.AsReg64s.L, optarg, NULL, TRUE)) {
				fprintf(stderr, "opafequery: Invalid GID: %s\n", optarg);
				Usage();
			}
            break;

		/* (SA) desc */
		case 'd':
		multiInputCheck(query.InputType);
			query.InputType = InputTypeNodeDesc;
			query.InputValue.NodeDesc.NameLength = MIN(strlen(optarg), STL_NODE_DESCRIPTION_ARRAY_SIZE);
			strncpy((char*)query.InputValue.NodeDesc.Name, optarg, STL_NODE_DESCRIPTION_ARRAY_SIZE);
            break;

		/* (SA) guidpair */
		case 'P':
			{
				char *p;
				multiInputCheck(query.InputType);
				query.InputType = InputTypePortGuidPair;
				if (FSUCCESS != StringToUint64(&query.InputValue.PortGuidPair.SourcePortGuid, optarg, &p, 0, TRUE)
					|| ! p || *p == '\0') {
					fprintf(stderr, "opafequery: Invalid GUID Pair: %s\n", optarg);
					Usage();
				}
				if (FSUCCESS != StringToUint64(&query.InputValue.PortGuidPair.DestPortGuid, p, NULL, 0, TRUE)) {
					fprintf(stderr, "opafequery: Invalid GUID Pair: %s\n", optarg);
					Usage();
				}
			}
            break;

		/* (SA) gidpair */
		case 'G':
			{
				char *p;
				multiInputCheck(query.InputType);
				query.InputType = InputTypeGidPair;
				if (FSUCCESS != StringToGid(&query.InputValue.GidPair.SourceGid.AsReg64s.H,&query.InputValue.GidPair.SourceGid.AsReg64s.L, optarg, &p, TRUE)
					|| ! p || *p == '\0') {
					fprintf(stderr, "opafequery: Invalid GID Pair: %s\n", optarg);
					Usage();
				}
				if (FSUCCESS != StringToGid(&query.InputValue.GidPair.DestGid.AsReg64s.H,&query.InputValue.GidPair.DestGid.AsReg64s.L, p, NULL, TRUE)) {
					fprintf(stderr, "opafequery: Invalid GID Pair: %s\n", optarg);
					Usage();
				}
			}
			break;

		/* (SA) guidlist */
		case 'B':
			{
				char *p =optarg;
				int i = 0;
				errno = 0;
				multiInputCheck(query.InputType);
				query.InputType = InputTypePortGuidList;
				do {
					if (FSUCCESS != StringToUint64(&query.InputValue.PortGuidList.GuidList[i], p, &p, 0, TRUE)) {
						fprintf(stderr, "opafequery: Invalid GUID List: %s\n", optarg);
						Usage();
					}
					i++;
					query.InputValue.PortGuidList.SourceGuidCount++;
				} while (p && *p != '\0' && *p++ != ';');
				if (p && *p != '\0')
				{
					do {
						if (FSUCCESS != StringToUint64(&query.InputValue.PortGuidList.GuidList[i], p, &p, 0, TRUE)) {
							fprintf(stderr, "opafequery: Invalid GUID List: %s\n", optarg);
							Usage();
						}
						i++;
						query.InputValue.PortGuidList.DestGuidCount++;
					} while (p && *p++ != '\0');
				} else {
					Usage();
				}
			}
			break;

		/* (SA) gidlist */
		case 'A':
			{
				char *p=optarg;
				int i = 0;
				errno = 0;
				multiInputCheck(query.InputType);
				query.InputType = InputTypeGidList;
				do {
					if (FSUCCESS != StringToGid(&query.InputValue.GidList.GidList[i].AsReg64s.H,&query.InputValue.GidList.GidList[i].AsReg64s.L, p, &p, TRUE)) {
						fprintf(stderr, "opafequery: Invalid GID List: %s\n", optarg);
						Usage();
					}
					i++;
					query.InputValue.GidList.SourceGidCount++;
				} while (p && *p != '\0' && *p++ != ';');
				if (p && *p != '\0')
				{
					do {
						if (FSUCCESS != StringToGid(&query.InputValue.GidList.GidList[i].AsReg64s.H,&query.InputValue.GidList.GidList[i].AsReg64s.L, p, &p, TRUE)) {
							fprintf(stderr, "opafequery: Invalid GID List: %s\n", optarg);
							Usage();
						}
						i++;
						query.InputValue.GidList.DestGidCount++;
					} while (p && *p++ != '\0');
				} else {
					Usage();
				}
			}
			break;

		/* (SA) Source GID */
		case 'x':
			{
				char *p = optarg;

				if(FSUCCESS != StringToGid(&g_sourceGid.AsReg64s.H, &g_sourceGid.AsReg64s.L, p, &p, TRUE)) {
					fprintf(stderr, "opafequery: Invalid Source GID: %s\n", optarg);
					Usage();
				} else {
					g_gotSourceGid = 1;
				}
			}
			break;

		/* (PA) Group Name */
        case 'g':
            memcpy(g_groupName, optarg, 64);
            g_gotGroup = 1;
            break;

		/* (PA) Port Number */
        case 'N':
            if (FSUCCESS != StringToUint8(&temp8, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Port Number: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_portNumber = temp8;
            g_gotPort = TRUE;
            break;

		/* (PA) Delta flag */
        case 'f':
            if (FSUCCESS != StringToUint32(&g_deltaFlag, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Delta Flag: %s\n", APP_NAME, optarg);
                Usage();
            }
            break;

		/* (PA) User Counters flag */
        case 'U':
            g_userCntrsFlag = 1;
            break;

		/* (PA) Select flag */
        case 'e':
            if (FSUCCESS != StringToUint32(&g_selectFlag, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Select Flag: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_gotSelect = TRUE;
            break;


		/* (PA) Image number */
        case 'b':
            if (FSUCCESS != StringToUint64(&g_imageNumber, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Image Number: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_gotImgNum = TRUE;
            break;

		/* (PA) Image offset */
        case 'O':
            if (FSUCCESS != StringToInt32(&g_imageOffset, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Image Offset: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_gotImgOff = TRUE;
            break;

		/* (PA) Move image number */
        case 'F':
            if (FSUCCESS != StringToUint64(&g_moveImageNumber, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Move Image Number: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_gotMoveImgNum = TRUE;
            break;

		/* (PA) Move image offset */
        case 'M':
            if (FSUCCESS != StringToInt32(&g_moveImageOffset, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Move Image Offset: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_gotMoveImgOff = TRUE;
            break;

		/* (PA) Focus */
        case 'c':
            if (FSUCCESS != StringToInt32(&g_focus, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Focus Number: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_gotFocus = TRUE;
            break;

		/* (PA) Start */
        case 'w':
            if (FSUCCESS != StringToInt32(&g_start, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Start Number: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_gotStart = TRUE;
            break;

		/* (PA) Range */
        case 'r':
            if (FSUCCESS != StringToInt32(&g_range, optarg, NULL, 0, TRUE)) {
                fprintf(stderr, "%s: Invalid Range Number: %s\n", APP_NAME, optarg);
                Usage();
            }
            g_gotRange = TRUE;
            break;

		/* (PA) Virtual Fabric Name */
		case 'V':
			memcpy(g_vfName, optarg, 64);
			g_gotvfName = TRUE;
			break;

        /* SSL/TLS XML input file */
        case 'T':
            g_sslParmsFile = optarg;
            break;

        /* SSL/TLS ESM peer */
        case 'E':
            g_feESM = 1;
            break;

		/* (General) Help & Usage */
        case '?':
            Usage();

        default:
            fprintf(stderr, "%s: Invalid Option %c\n", APP_NAME, c);
            Usage();
            break;
        }
    } /* end while */

    if (optind < argc) {
        fprintf(stderr, "%s: invalid argument %s\n", APP_NAME, argv[optind]);
        Usage();
    }

    /* Check parameter consistency */
    if (!g_gotOutput) {
        fprintf(stderr, "%s: Must provide an output type\n", APP_NAME);
        Usage();
    }

    if ((queryType == Q_PA_GETGROUPINFO) && (!g_gotGroup)) {
        fprintf(stderr, "%s: Must provide a group name with output type groupInfo\n", APP_NAME);
        Usage();
    }

    if ((queryType == Q_PA_GETGROUPCONFIG) && (!g_gotGroup)) {
        fprintf(stderr, "%s: Must provide a group name with output type groupConfig\n", APP_NAME);
        Usage();
    }

    if ((queryType == Q_PA_GETFOCUSPORTS) && (!g_gotGroup)) {
        fprintf(stderr, "%s: Must provide a group name with output type focusPorts\n", APP_NAME);
        Usage();
    }

    if ((queryType == Q_PA_GETPORTCOUNTERS) && ((!g_gotLid) || (!g_gotPort))) {
        fprintf(stderr, "%s: Must provide a lid and port with output type portCounters\n", APP_NAME);
        Usage();
    }

	if ((queryType == Q_PA_CLRPORTCOUNTERS) && ((!g_gotLid) || (!g_gotPort) || (!g_gotSelect))) {
		fprintf(stderr, "%s: Must provide a lid, port, and select flag with output type clrPortCounters\n", APP_NAME);
		Usage();
	}

	if ((queryType == Q_PA_CLRALLPORTCOUNTERS) && !g_gotSelect) {
		fprintf(stderr, "%s: Must provide a select flag with output type clrAllPortCounters\n", APP_NAME);
		Usage();
	}

    if (((queryType == Q_PA_FREEZEIMAGE) || (queryType == Q_PA_RELEASEIMAGE) || (queryType == Q_PA_RENEWIMAGE) || (queryType == Q_PA_MOVEFREEZE))   &&
        (!g_gotImgNum)) {
        fprintf(stderr, "%s: Must provide an imgNum with freezeImage/releaseImage/renewImage/moveFreeze\n", APP_NAME);
        Usage();
    }

    if ((queryType == Q_PA_MOVEFREEZE) && !g_gotMoveImgNum) {
        fprintf(stderr, "%s: Must provide a moveImgNum with output type moveFreeze\n", APP_NAME);
        Usage();
    }

    if (g_deltaFlag > 1) {
        fprintf(stderr, "%s: delta value must be 0 or 1\n", APP_NAME);
        Usage();
    }

    if (g_userCntrsFlag && (g_deltaFlag || g_gotImgOff)) {
        fprintf(stderr, "%s: delta value and image offset must be 0 when querying User Counters\n", APP_NAME);
        Usage();
    }

	if ((queryType == Q_PA_GETVFINFO) && (!g_gotvfName)) {
		fprintf(stderr, "%s: Must provide a VF name with output type vfInfo\n", APP_NAME);
		Usage();
	}

	if ((queryType == Q_PA_GETVFCONFIG) && (!g_gotvfName)) {
		fprintf(stderr, "%s: Must provide a VF name with output type vfConfig\n", APP_NAME);
		Usage();
	}

	if ((queryType == Q_PA_GETVFPORTCOUNTERS) && (!g_gotvfName)) {
		fprintf(stderr, "%s: Must provide a VF name with output type vfPortCounters\n", APP_NAME);
		Usage();
	}

    if ((queryType == Q_PA_GETVFPORTCOUNTERS) && ((!g_gotLid) || (!g_gotPort))) {
        fprintf(stderr, "%s: Must provide a lid and port with output type vfPortCounters\n", APP_NAME);
        Usage();
    }

	if ((queryType == Q_PA_CLRVFPORTCOUNTERS) && (!g_gotvfName)) {
		fprintf(stderr, "%s: Must provide a VF name with output type clrVfPortCounters\n", APP_NAME);
		Usage();
	}

	if ((queryType == Q_PA_CLRVFPORTCOUNTERS) && ((!g_gotLid) || (!g_gotPort) || (!g_gotSelect))) {
		fprintf(stderr, "%s: Must provide a lid, port, and select flag with output type clrVfPortCounters\n", APP_NAME);
		Usage();
	}

	if (g_sslParmsFile) {
		if (FSUCCESS != fe_xml2ParseSslParmsFile(g_sslParmsFile, g_verbose, &g_sslParmsData)) {
            fprintf(stderr, "%s: Must provide a valid SSL/TLS parameters XML file\n", APP_NAME);
            Usage();
		}
	}

    /* Initialize signal handlers for orderly shutdown */
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, signal_handler);

    /* initialize global for ibprint output handling */
    PrintDestInitFile(&g_dest, stdout);
	if (g_verbose)
		PrintDestInitFile(&g_dbgDest, stdout);
	else
		PrintDestInitNone(&g_dbgDest);

	/* Process the query */
	fstatus = fe_processQuery(queryType, &query);

	/* Print success and exit */
    fprintf(stderr, "%s completed: %s\n", APP_NAME, (fstatus == FSUCCESS) ? "OK" : "FAILED");
    exit(0);
}
