/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015-2018, Intel Corporation

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

#include "opareport.h"
#ifdef IB_STACK_OPENIB
#include <opamgt_priv.h>
#include <opamgt_sa_priv.h>
#endif
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <arpa/inet.h>
#include <stl_helper.h>
#include <umad.h>
#include <time.h>
#include <string.h>
#include "stl_print.h"

// Used for expanding various enumarations into text equivalents
#define SHOW_BUF_SIZE 81

// what to output when g_noname set
char *g_name_marker = "xxxxxxxxxx";

// amount to subtract from threshold before compare
// 0-> Greater (report error if > threshold)
// 1-> Equal (report error if >= threshold)
uint32 g_threshold_compare = 0;

/* indicates overall set of reports for Slow Link reports
 * also used to indicate which portion of the report is being done
 */
typedef enum {
	LINK_EXPECTED_REPORT = 1,
	LINK_CONFIG_REPORT =2,
	LINK_CONN_REPORT =3
} LinkReport_t;

uint8           g_verbose       = 0;
int				g_exitstatus	= 0;
int				g_persist		= 0;	// omit transient data like LIDs
int				g_hard			= 0;	// omit software configured items
int				g_noname		= 0;	// omit names
char*			g_snapshot_in_file	= NULL;	// input file being parsed
char*			g_topology_in_file	= NULL;	// input file being parsed
int				g_interval		= 0;	// interval for port stats in seconds
int				g_clearstats	= 0;	// clear port stats
int				g_clearallstats	= 0;	// clear all port stats
int				g_limitstats	= 0;	// limit stats to specific focus ports
STL_PORT_STATUS_RSP	  g_Thresholds;
STL_CLEAR_PORT_STATUS g_CounterSelectMask;
EUI64			g_portGuid		= -1;	// local port to use to access fabric
IB_PORT_ATTRIBUTES	*g_portAttrib = NULL;// attributes for our local port
int				g_quietfocus	= 0;	// do not include focus desc in report
int				g_max_lft       = 0;	// Size of largest switch LFT
int				g_quiet         = 0;	// omit progress output
uint32          g_begin         = 0;	// begin time for interval
uint32          g_end           = 0;	// end time for interval
int		g_use_scsc	= 0; // should validatecreditloops use scsc tables

// All the information about the fabric
FabricData_t g_Fabric;

void XmlPrintHex64(const char *tag, uint64 value, int indent)
{
	printf("%*s<%s>0x%016"PRIx64"</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintHex32(const char *tag, uint32 value, int indent)
{
	printf("%*s<%s>0x%08x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintHex16(const char *tag, uint16 value, int indent)
{
	printf("%*s<%s>0x%04x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintHex8(const char *tag, uint8 value, int indent)
{
	printf("%*s<%s>0x%02x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintDec(const char *tag, unsigned value, int indent)
{
	printf("%*s<%s>%u</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintDec64(const char *tag, uint64 value, int indent)
{
	printf("%*s<%s>%"PRIu64"</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintHex(const char *tag, unsigned value, int indent)
{
	printf("%*s<%s>0x%x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintStrLen(const char *tag, const char* value, int len, int indent)
{
	printf("%*s<%s>", indent, "",tag);
	/* print string taking care to translate special XML characters */
	for (;len && *value; --len, ++value) {
		if (*value == '&')
			printf("&amp;");
		else if(*value==' '){
                        if(strcmp(tag,"VendorPN")){
                                if(isalnum(*(value+1)))
                                        putchar((int)(unsigned)(unsigned char)*value);
                        }
                }
		else if (*value == '<')
			printf("&lt;");
		else if (*value == '>')
			printf("&gt;");
		else if (*value == '\'')
			printf("&apos;");
		else if (*value == '"')
			printf("&quot;");
		else if (iscntrl(*value)) {
			//table in asciitab.h indiciates character codes permitted in XML strings
			//Only 3 control characters below 0x1f are permitted:
			//0x9 (BT_S), 0xa (BT_LRF), and 0xd (BT_CR)
			if ((unsigned char)*value <= 0x08
				|| ((unsigned char)*value >= 0x0b
						 && (unsigned char)*value <= 0x0c)
				|| ((unsigned char)*value >= 0x0e
						 && (unsigned char)*value <= 0x1f)) {
				// characters which XML does not permit in character fields
				printf("!");
			} else {
				printf("&#x%x;", (unsigned)(unsigned char)*value);
			}
		} else if ((unsigned char)*value > 0x7f)
			// permitted but generate 2 characters back after parsing, so omit
			printf("!");
		else
			putchar((int)(unsigned)(unsigned char)*value);
	}
	printf("</%s>\n", tag);
}

void XmlPrintStr(const char *tag, const char* value, int indent)
{
	XmlPrintStrLen(tag, value, IB_INT32_MAX, indent);
}

void XmlPrintOptionalStr(const char *tag, const char* value, int indent)
{
	if (value)
		XmlPrintStrLen(tag, value, IB_INT32_MAX, indent);
}

void XmlPrintBool(const char *tag, unsigned value, int indent)
{
	if (value)
		XmlPrintStr(tag, "True", indent);
	else
		XmlPrintStr(tag, "False", indent);
}

void XmlPrintMLID(const char *tag, STL_LID value, int indent)
{
	// should never be less than 4 hex digits, upper bit should never be 0
	printf("%*s<%s>0x%04x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintLID(const char *tag, STL_LID value, int indent)
{
	printf("%*s<%s>0x%.*x</%s>\n", indent, "", tag, (value <= IB_MAX_UCAST_LID ? 4:8), value, tag);
}

void XmlPrintPKey(const char *tag, IB_P_KEY value, int indent)
{
	printf("%*s<%s>0x%04x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintGID(const char *tag, IB_GID value, int indent)
{
	printf("%*s<%s>0x%016"PRIx64":%016"PRIx64"</%s>\n",
				indent, "", tag,
				value.Type.Global.SubnetPrefix,
				value.Type.Global.InterfaceID, tag);
}

void XmlPrintNodeType(uint8 value, int indent)
{
	XmlPrintStr("NodeType", StlNodeTypeToText(value), indent);
	XmlPrintDec("NodeType_Int", value, indent);
}

void XmlPrintNodeDesc(const char *value, int indent)
{
	if (! g_noname)
		XmlPrintStrLen("NodeDesc", value, NODE_DESCRIPTION_ARRAY_SIZE, indent);
}

void XmlPrintIocIDString(const char *value, int indent)
{
	if (! g_noname)
		XmlPrintStrLen("IDString", value, IOC_IDSTRING_SIZE, indent);
}

void XmlPrintServiceName(const uchar *value, int indent)
{
	/* service names are critical, g_noname NA */
	XmlPrintStrLen("Name", (const char*)value, IOC_SERVICE_NAME_SIZE, indent);
}

void XmlPrintRate(uint8 value, int indent)
{
	XmlPrintStr("Rate", StlStaticRateToText(value), indent);
	XmlPrintDec("Rate_Int", value, indent);
}

void XmlPrintLinkWidth(const char* tag_prefix, uint8 value, int indent)
{
	char buf[64];
	XmlPrintStr(tag_prefix, StlLinkWidthToText(value, buf, sizeof(buf)), indent);
	printf("%*s<%s_Int>%u</%s_Int>\n", indent, "",tag_prefix, value, tag_prefix);
}

void XmlPrintLinkSpeed(const char* tag_prefix, uint16 value, int indent)
{
	char buf[64];
	XmlPrintStr(tag_prefix, StlLinkSpeedToText(value, buf, sizeof(buf)), indent);
	printf("%*s<%s_Int>%u</%s_Int>\n", indent, "",tag_prefix, value, tag_prefix);
}

// for predictable output order, should be called with the from port of the
// link record, with the exception of trace routes
void XmlPrintLinkStartTag(const char* tag, PortData *portp, int indent)
{
	//if (! portp->from)
	//	portp = portp->neighbor;

	printf("%*s<%s id=\"0x%016"PRIx64":%u\">\n", indent, "", tag,
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
}

void XmlPrintTagHeader(const char *tag, int indent) 
{ 
   printf("%*s<%s>\n", indent, "", tag);
}
void XmlPrintTagFooter(const char *tag, int indent) 
{ 
   printf("%*s</%s>\n", indent, "", tag); 
}

void XmlPrintGroupRecord (McGroupData *pMcGroupRecord, int indent, int detail)
{
	char buf[8];
	XmlPrintGID("MGID",pMcGroupRecord->MGID,indent+8);
	XmlPrintMLID("MLID",pMcGroupRecord->MLID, indent+8);
	XmlPrintPKey("P_Key", pMcGroupRecord->GroupInfo.P_Key, indent+8);
	XmlPrintDec("Mtu", GetBytesFromMtu(pMcGroupRecord->GroupInfo.Mtu), indent+8);
	XmlPrintRate(pMcGroupRecord->GroupInfo.Rate,indent+8);
	FormatTimeoutMult(buf, pMcGroupRecord->GroupInfo.PktLifeTime);
	XmlPrintStr("PktLifeTime", buf, indent+8);
	XmlPrintDec("PktLifeTime_Int", pMcGroupRecord->GroupInfo.PktLifeTime, indent+8);
	XmlPrintHex32("Q_Key", pMcGroupRecord->GroupInfo.Q_Key, indent+8);
	XmlPrintDec("SL", pMcGroupRecord->GroupInfo.u1.s.SL, indent+8);
	XmlPrintHex("HopLimit", pMcGroupRecord->GroupInfo.u1.s.HopLimit, indent+8);
	XmlPrintHex("FlowLabel", pMcGroupRecord->GroupInfo.u1.s.FlowLabel, indent+8);
	XmlPrintHex8("TClass", pMcGroupRecord->GroupInfo.TClass, indent+8);
}

void McMembershipXmlOutput(const char *tag, McMemberData *pMCGG, int indent)
{
	uint8 Memberstatus;

	Memberstatus = (pMCGG->MemberInfo.JoinSendOnlyMember<<2 |
					pMCGG->MemberInfo.JoinNonMember<<1|
					pMCGG->MemberInfo.JoinFullMember);

	XmlPrintDec(tag, Memberstatus, indent);
}

void DisplaySeparator(void)
{
	printf("-------------------------------------------------------------------------------\n");

}

void DisplayGroupRecord(McGroupData *pMcGroupRecord, int indent, int detail)
{
	char buf[8];
	printf("%*sMGID: 0x%016"PRIx64":0x%016"PRIx64"\n",
			indent, "",
			pMcGroupRecord->MGID.AsReg64s.H,
			pMcGroupRecord->MGID.AsReg64s.L);
	FormatTimeoutMult(buf, pMcGroupRecord->GroupInfo.PktLifeTime);
	printf("%*sMLID: 0x%08x PKey: 0x%04x Mtu: %5s Rate: %4s PktLifeTime: %s\n",
			indent, "",
			pMcGroupRecord->MLID,
			pMcGroupRecord->GroupInfo.P_Key,
			IbMTUToText(pMcGroupRecord->GroupInfo.Mtu),
			StlStaticRateToText(pMcGroupRecord->GroupInfo.Rate),
			buf);
	printf("%*sQKey: 0x%08x SL: %2d FlowLabel: 0x%05x  HopLimit: 0x%02x  TClass:  0x%02x\n",
			indent, "",
			pMcGroupRecord->GroupInfo.Q_Key,
			pMcGroupRecord->GroupInfo.u1.s.SL,
			pMcGroupRecord->GroupInfo.u1.s.FlowLabel,
			pMcGroupRecord->GroupInfo.u1.s.HopLimit,
			pMcGroupRecord->GroupInfo.TClass);
}

void ShowPathRecord(IB_PATH_RECORD *pPathRecord, Format_t format,
					int  indent, int detail)
{
	char buf[8];
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sSGID: 0x%016"PRIx64":%016"PRIx64"\n",
				indent, "",
				pPathRecord->SGID.Type.Global.SubnetPrefix,
				pPathRecord->SGID.Type.Global.InterfaceID);
		printf("%*sDGID: 0x%016"PRIx64":%016"PRIx64"\n",
				indent, "",
				pPathRecord->DGID.Type.Global.SubnetPrefix,
				pPathRecord->DGID.Type.Global.InterfaceID);
		printf("%*sSLID: 0x%.*x DLID: 0x%.*x Reversible: %s",
				indent, "", (pPathRecord->SLID <= IB_MAX_UCAST_LID ? 4:8), pPathRecord->SLID,
				(pPathRecord->DLID <= IB_MAX_UCAST_LID ? 4:8), pPathRecord->DLID,
				pPathRecord->Reversible?"Y":"N");
		// If this is from a snapshot, stop here - the remaining values cannot be
		// deduced.
		if (g_snapshot_in_file) {
			printf("\n");
			break;
		}

		printf(" PKey: 0x%04x\n", pPathRecord->P_Key);

		printf("%*sRaw: %s FlowLabel: 0x%05x HopLimit: 0x%02x TClass: 0x%02x\n",
				indent, "", pPathRecord->u1.s.RawTraffic?"Y":"N",
				pPathRecord->u1.s.FlowLabel, pPathRecord->u1.s.HopLimit,
				pPathRecord->TClass);
		FormatTimeoutMult(buf, pPathRecord->PktLifeTime);
		printf("%*sSL: %2d Mtu: %5s Rate: %4s PktLifeTime: %s Pref: %d\n",
				indent, "", pPathRecord->u2.s.SL, IbMTUToText(pPathRecord->Mtu),
				StlStaticRateToText(pPathRecord->Rate), buf,
				pPathRecord->Preference);
		break;
	case FORMAT_XML:
		// TBD does this get a unique ID?  Is PKey needed too
		//printf("%*s<PathRecord id=\"0x%016"PRIx64":%016"PRIx64"-%016"PRIx64":%016"PRIx64"-%04x-%04x\">\n", indent, "",
		//		pPathRecord->SGID.Type.Global.SubnetPrefix,
		//		pPathRecord->SGID.Type.Global.InterfaceID,
		//		pPathRecord->DGID.Type.Global.SubnetPrefix,
		//		pPathRecord->DGID.Type.Global.InterfaceID,
		//		pPathRecord->SLID, pPathRecord->DLID);
		printf("%*s<PathRecord>\n", indent, "");
		XmlPrintGID("SGID", pPathRecord->SGID, indent+4);
		XmlPrintGID("DGID", pPathRecord->DGID, indent+4);
		XmlPrintLID("SLID", pPathRecord->SLID, indent+4);
		XmlPrintLID("DLID", pPathRecord->DLID, indent+4);
		XmlPrintStr("Reversible", pPathRecord->Reversible?"Y":"N", indent+4);
		XmlPrintDec("Reversible_Int", pPathRecord->Reversible, indent+4);
		// If this is from a snapshot, stop here - the remaining values cannot be
		// deduced.
		if (g_snapshot_in_file) {
			printf("%*s</PathRecord>\n", indent, "");
			break;
		}
		XmlPrintPKey("PKey", pPathRecord->P_Key, indent+4);
		XmlPrintStr("Raw", pPathRecord->u1.s.RawTraffic?"Y":"N", indent+4);
		XmlPrintDec("Raw_Int", pPathRecord->u1.s.RawTraffic, indent+4);
		XmlPrintHex("FlowLabel", pPathRecord->u1.s.FlowLabel, indent+4);
		XmlPrintHex8("HopLimit", pPathRecord->u1.s.HopLimit, indent+4);
		XmlPrintHex8("TClass", pPathRecord->TClass, indent+4);
		XmlPrintDec("SL", pPathRecord->u2.s.SL, indent+4);
		XmlPrintDec("Mtu", GetBytesFromMtu(pPathRecord->Mtu), indent+4);
		XmlPrintRate(pPathRecord->Rate, indent+4);
		FormatTimeoutMult(buf, pPathRecord->PktLifeTime);
		XmlPrintStr("PktLifeTime", buf, indent+4);
		XmlPrintDec("PktLifeTime_Int", pPathRecord->PktLifeTime, indent+4);
		XmlPrintDec("Preference", pPathRecord->Preference, indent+4);
		printf("%*s</PathRecord>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowTraceRecord(STL_TRACE_RECORD *pTraceRecord, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		// DisplayTraceRecord(pTraceRecord, indent);
		printf("%*sIDGeneration: 0x%04x\n",
           indent, "", pTraceRecord->IDGeneration);
		printf("%*sNodeType: 0x%02x\n",
           indent, "", pTraceRecord->NodeType);
		printf("%*sNodeID: 0x%016"PRIx64" ChassisID: %016"PRIx64"\n",
           indent, "", pTraceRecord->NodeID, pTraceRecord->ChassisID);
		printf("%*sEntryPortID: 0x%016"PRIx64" ExitPortID: %016"PRIx64"\n",
           indent, "", pTraceRecord->EntryPortID, pTraceRecord->ExitPortID);
		printf("%*sEntryPort: 0x%02x ExitPort: 0x%02x\n",
           indent, "", pTraceRecord->EntryPort, pTraceRecord->ExitPort);
		break;
	case FORMAT_XML:
		// TBD id may not be unique, may need different id based on NodeType
		printf("%*s<TraceRecord>\n", indent, "");
		XmlPrintHex16("IDGeneration", pTraceRecord->IDGeneration, indent+4);
		XmlPrintNodeType(pTraceRecord->NodeType, indent+4);
		XmlPrintHex64("NodeID", pTraceRecord->NodeID, indent+4);
		XmlPrintHex64("ChassisID", pTraceRecord->ChassisID, indent+4);
		XmlPrintHex64("EntryPortID", pTraceRecord->EntryPortID, indent+4);
		XmlPrintHex64("ExitPortID", pTraceRecord->ExitPortID, indent+4);
		XmlPrintHex8("EntryPort", pTraceRecord->EntryPort, indent+4);
		XmlPrintHex8("ExitPort", pTraceRecord->ExitPort, indent+4);
		printf("%*s</TraceRecord>\n", indent, "");
		break;
	default:
		break;
	}
}

// header used before a series of links
void ShowLinkBriefSummaryHeader(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sRate NodeGUID          Port Type Name\n", indent, "");
		if (detail && (g_Fabric.flags & FF_CABLEDATA)) {
			//printf("%*sPortDetails\n", indent+4, "");
			//printf("%*sLinkDetails\n", indent+4, "");
			printf("%*sCable: %-*s %-*s\n", indent+4, "",
							CABLE_LABEL_STRLEN, "CableLabel",
							CABLE_LENGTH_STRLEN, "CableLen");
			printf("%*s%s\n", indent+4, "", "CableDetails");
		}
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

void ShowCableSummary(uint8_t *pCableData, Format_t format, int indent, int detail, uint8 portType)
{
	// CableInfo is organized in 128-byte pages but is stored in 64-byte half-pages
	// For portType STANDARD we use STL_CIB_STD_HIGH_PAGE_ADDR to STL_CIB_STD_END_ADDR
	// inclusive (128-255)
	// To avoid compiler warnings, data pointer is used for the data portion
	// of the STL_CABLE_INFO
	STL_CABLE_INFO_STD *pCableInfo = (STL_CABLE_INFO_STD *)pCableData;
	CableTypeInfoType cableTypeInfo;
	boolean cableLenValid;			// Copper cable length valid
	boolean qsfp_dd;
	char tempStr[STL_CIB_STD_MAX_STRING + 1] = {'\0'};
	char tempBuf[129];
	unsigned int i;

	if (pCableData)
		qsfp_dd = (*pCableData == STL_CIB_STD_QSFP_DD);
	else {
		fprintf(stderr, "ShowCableSummary: cableInfoData pointer is invalid\n");
		return;
	}

	if (qsfp_dd) {
		ShowCableSummaryDD(pCableData, format, indent, detail, portType);
		return;
	}

	StlCableInfoDecodeCableType(pCableInfo->dev_tech.s.xmit_tech, pCableInfo->connector, pCableInfo->ident, &cableTypeInfo);
	cableLenValid = cableTypeInfo.cableLengthValid;

	switch (format) {
	case FORMAT_TEXT:
		switch (portType) {
		case STL_PORT_TYPE_STANDARD:
			if (detail <= 6) {
				//line1
				memset(tempBuf, ' ', sizeof(tempBuf));
                        	i = 0;
                        	StringCopy(&tempBuf[i], cableTypeInfo.cableTypeShortDesc, strlen(cableTypeInfo.cableTypeShortDesc)+1);
							tempBuf[i + strlen(cableTypeInfo.cableTypeShortDesc)] = ',';
#if 0
							i = STL_CIB_LINE1_FIELD1;
							strncpy(&tempBuf[i], cableTypeInfo.cableTypeShortDesc, strlen(cableTypeInfo.cableTypeShortDesc));
							tempBuf[i + strlen(cableTypeInfo.cableTypeShortDesc)] = ',';
							tempBuf[i + 1 + strlen(cableTypeInfo.cableTypeShortDesc)] = ' ';
#endif
						 	i = STL_CIB_LINE1_FIELD2;
                            StlCableInfoValidCableLengthToText(pCableInfo->len_om4, cableLenValid, &tempBuf[i]);
                            tempBuf[i + strlen(&tempBuf[i])] = ' ';
						 	i = STL_CIB_LINE1_FIELD3;
                        	memcpy(&tempBuf[i], pCableInfo->vendor_name, sizeof(pCableInfo->vendor_name));
						 	i = STL_CIB_LINE1_FIELD4;
                        	strcpy(&tempBuf[i], "P/N ");
						 	i = STL_CIB_LINE1_FIELD5;
                        	memcpy(&tempBuf[i], pCableInfo->vendor_pn, sizeof(pCableInfo->vendor_pn));
						 	i = STL_CIB_LINE1_FIELD6;
                        	strcpy(&tempBuf[i], "Rev ");
						 	i = STL_CIB_LINE1_FIELD7;
                        	memcpy(&tempBuf[i], pCableInfo->vendor_rev, sizeof(pCableInfo->vendor_rev));
						 	i = STL_CIB_LINE1_FIELD8;
                        	tempBuf[i] = '\0';
                        	printf("%*s%s\n", indent, "", tempBuf);
                        	if (detail <=1)
                        	        break;
				//line2
				memset(tempBuf, ' ', sizeof(tempBuf));
                        	i = 0;
                        	strcpy(&tempBuf[i], StlCableInfoPowerClassToText(pCableInfo->ext_ident.s.pwr_class_low, pCableInfo->ext_ident.s.pwr_class_high));
                        	tempBuf[i + strlen(&tempBuf[i])] = ' ';
						 	i = STL_CIB_LINE2_FIELD2;
                        	strcpy(&tempBuf[i], "S/N ");
						 	i = STL_CIB_LINE2_FIELD3;
                        	memcpy(&tempBuf[i], pCableInfo->vendor_sn, sizeof(pCableInfo->vendor_sn));
						 	i = STL_CIB_LINE2_FIELD4;
                        	strcpy(&tempBuf[i], "Mfg ");
						 	i = STL_CIB_LINE2_FIELD5;
                        	StlCableInfoDateCodeToText(pCableInfo->date_code, &tempBuf[i]);
                        	printf("%*s%s\n", indent, "", tempBuf);
				//line3
				memset(tempBuf, ' ', sizeof(tempBuf));
                        	i = 0;
                           	sprintf(&tempBuf[i], "OUI 0x%02X%02X%02X", pCableInfo->vendor_oui[0], pCableInfo->vendor_oui[1], pCableInfo->vendor_oui[2]);
                           	printf("%*s%s\n", indent, "", tempBuf);
				break;
			} else {
				printf("%*sQSFP Interpreted CableInfo:\n", indent, "");
				printf("%*sIdentifier: 0x%x\n", indent+4, "", pCableInfo->ident);
				memcpy(tempBuf, cableTypeInfo.connectorType, sizeof(cableTypeInfo.connectorType));
				printf("%*sConnector: %s\n", indent+4, "", tempBuf);
				StlCableInfoCableTypeToTextLong(pCableInfo->dev_tech.s.xmit_tech, pCableInfo->connector, tempBuf);
				printf("%*sDeviceTech: %s\n", indent+4, "", tempBuf);
                StlCableInfoValidCableLengthToText(pCableInfo->len_om4, cableLenValid, tempBuf);
				printf("%*sOM4Length: %s\n", indent+4, "", tempBuf);
				memcpy(tempStr, pCableInfo->vendor_name, sizeof(pCableInfo->vendor_name));
				tempStr[sizeof(pCableInfo->vendor_name)] = '\0';
				printf("%*sVendorName: %s\n", indent+4, "", tempStr);
				printf( "%*sVendorOUI: 0x%02x%02x%02x\n", indent+4, "", pCableInfo->vendor_oui[0],
				pCableInfo->vendor_oui[1], pCableInfo->vendor_oui[2] );
				memcpy(tempStr, pCableInfo->vendor_pn, sizeof(pCableInfo->vendor_pn));
				tempStr[sizeof(pCableInfo->vendor_pn)] = '\0';
				printf("%*sVendorPN: %s\n", indent+4, "", tempStr);
				memcpy(tempStr, pCableInfo->vendor_rev, sizeof(pCableInfo->vendor_rev));
				tempStr[sizeof(pCableInfo->vendor_rev)] = '\0';
				printf("%*sVendorRev: %s\n", indent+4, "", tempStr);
				memcpy(tempBuf, StlCableInfoPowerClassToText(pCableInfo->ext_ident.s.pwr_class_low, pCableInfo->ext_ident.s.pwr_class_high),
					strlen(StlCableInfoPowerClassToText(pCableInfo->ext_ident.s.pwr_class_low, pCableInfo->ext_ident.s.pwr_class_high)));
				tempStr[strlen(StlCableInfoPowerClassToText(pCableInfo->ext_ident.s.pwr_class_low, pCableInfo->ext_ident.s.pwr_class_high))] = '\0';
				printf("%*sPoweClass: %s\n", indent+4, "", tempStr);
				memcpy(tempStr, pCableInfo->vendor_sn, sizeof(pCableInfo->vendor_sn));
				tempStr[sizeof(pCableInfo->vendor_sn)] = '\0';
				printf("%*sVendorSN: %s\n", indent+4, "", tempStr);
				StlCableInfoDateCodeToText(pCableInfo->date_code, tempBuf);
				printf("%*sDateCode: %s\n", indent+4, "", tempBuf);
			}
			break;
		case STL_PORT_TYPE_SI_PHOTONICS:
		default:
			//printf("%*sCableInfo: N/A for Port Type: %s \n", indent, "", StlPortTypeToText(portType));
			break;
		}
		break;
	case FORMAT_XML:
		switch (portType) {
		case STL_PORT_TYPE_STANDARD:
			printf("%*s<CableInfo>\n", indent, "");
			if (detail <= 6) {
				strncpy(tempBuf, cableTypeInfo.cableTypeShortDesc, strlen(cableTypeInfo.cableTypeShortDesc));
				XmlPrintStr("DeviceTechShort", tempBuf, indent+4);
				//maps to text line 1
				StlCableInfoValidCableLengthToText(pCableInfo->len_om4, cableLenValid, tempBuf);
				XmlPrintStr("OM4Length", tempBuf, indent+4);
				XmlPrintStr("Length", tempBuf, indent+4);

				XmlPrintStrLen("VendorName", (char*)pCableInfo->vendor_name, sizeof(pCableInfo->vendor_name), indent+4);
				XmlPrintStrLen("VendorPN", (char*)pCableInfo->vendor_pn, sizeof(pCableInfo->vendor_pn), indent+4);
				XmlPrintStrLen("VendorRev", (char*)pCableInfo->vendor_rev, sizeof(pCableInfo->vendor_rev), indent+4);
				XmlPrintStrLen("VendorSN", (char*)pCableInfo->vendor_sn, sizeof(pCableInfo->vendor_sn), indent+4);
				StlCableInfoDateCodeToText(pCableInfo->date_code, tempBuf);
				XmlPrintStrLen("DateCode", (char*)tempBuf, sizeof(tempBuf), indent+4);
				XmlPrintHex("VendorOUI", (pCableInfo->vendor_oui[0]<<16) + (pCableInfo->vendor_oui[1]<<8) + pCableInfo->vendor_oui[2], indent+4);
			} else {
				snprintf(tempBuf, 5, "0x%x", pCableInfo->ident);
				XmlPrintStr("Identifier", tempBuf, indent+4);
				memcpy(tempBuf, cableTypeInfo.connectorType, sizeof(cableTypeInfo.connectorType));
				XmlPrintStr("Connector", tempBuf, indent+4);
				StlCableInfoCableTypeToTextLong(pCableInfo->dev_tech.s.xmit_tech, pCableInfo->connector, tempBuf);
				XmlPrintStr("DeviceTech", tempBuf, indent+4);
	 			StlCableInfoOM4LengthToText(pCableInfo->len_om4, cableLenValid, sizeof(tempBuf), tempBuf);
				XmlPrintStr("OM4Length", tempBuf, indent+4);
				XmlPrintStr("Length", tempBuf, indent+4);

				XmlPrintStrLen("VendorName", (char*)pCableInfo->vendor_name, sizeof(pCableInfo->vendor_name), indent+4);
				XmlPrintStrLen("VendorPN", (char*)pCableInfo->vendor_pn, sizeof(pCableInfo->vendor_pn), indent+4);
				XmlPrintStrLen("VendorRev", (char*)pCableInfo->vendor_rev, sizeof(pCableInfo->vendor_rev), indent+4);
				strcpy(tempBuf, StlCableInfoPowerClassToText(pCableInfo->ext_ident.s.pwr_class_low, pCableInfo->ext_ident.s.pwr_class_high));
				XmlPrintStr("PowerClass", tempBuf, indent+4);
				XmlPrintStrLen("VendorSN", (char*)pCableInfo->vendor_sn, sizeof(pCableInfo->vendor_sn), indent+4);
				StlCableInfoDateCodeToText(pCableInfo->date_code, tempBuf);
				XmlPrintStrLen("DateCode", (char*)tempBuf, sizeof(tempBuf), indent+4);
				XmlPrintHex("VendorOUI", (pCableInfo->vendor_oui[0]<<16) + (pCableInfo->vendor_oui[1]<<8) + pCableInfo->vendor_oui[2], indent+4);
			}
			printf("%*s</CableInfo>\n", indent, "");
			break;
		case STL_PORT_TYPE_SI_PHOTONICS:
		default:
			break;
		}
		break;
	default:
		break;
	}	
}	// End of ShowCableSummary()

void ShowCableSummaryDD(uint8_t *pCableData, Format_t format, int indent, int detail, uint8 portType)
{
	// CableInfo is organized in 128-byte pages but is stored in 64-byte half-pages
	// For portType STANDARD we use STL_CIB_STD_HIGH_PAGE_ADDR to STL_CIB_STD_END_ADDR
	// inclusive (128-255)
	// To avoid compiler warnings, data pointer is used for the data portion
	// of the STL_CABLE_INFO
	STL_CABLE_INFO_UP0_DD *pCableInfo = (STL_CABLE_INFO_UP0_DD *)pCableData;
	CableTypeInfoType cableTypeInfo;
	char tempStr[STL_CIB_STD_MAX_STRING + 1] = {'\0'};
	char tempBuf[129];
	unsigned int i;
#define MAX_CABLE_LENGTH_STR_LEN 8		// ~2-3 digits (poss. decimal pt) plus 'm'

	StlCableInfoDecodeCableType(pCableInfo->cable_type, pCableInfo->connector, pCableInfo->ident, &cableTypeInfo);

	switch (format) {
	case FORMAT_TEXT:
		switch (portType) {
		case STL_PORT_TYPE_STANDARD:
			if (detail <= 6) {
				//line1
				memset(tempBuf, ' ', sizeof(tempBuf));
                        	i = 0;
                        	StringCopy(&tempBuf[i], cableTypeInfo.cableTypeShortDesc, strlen(cableTypeInfo.cableTypeShortDesc)+1);
							tempBuf[i + strlen(cableTypeInfo.cableTypeShortDesc)] = ',';
#if 0
							i = STL_CIB_LINE1_FIELD1;
							strncpy(&tempBuf[i], cableTypeInfo.cableTypeShortDesc, strlen(cableTypeInfo.cableTypeShortDesc));
							tempBuf[i + strlen(cableTypeInfo.cableTypeShortDesc)] = ',';
							tempBuf[i + 1 + strlen(cableTypeInfo.cableTypeShortDesc)] = ' ';
#endif
						 	i = STL_CIB_LINE1_FIELD2;
							StlCableInfoDDCableLengthToText(pCableInfo->cableLengthEnc, MAX_CABLE_LENGTH_STR_LEN, &tempBuf[i]);
                            tempBuf[i + strlen(&tempBuf[i])] = ' ';
						 	i = STL_CIB_LINE1_FIELD3;
                        	memcpy(&tempBuf[i], pCableInfo->vendor_name, sizeof(pCableInfo->vendor_name));
						 	i = STL_CIB_LINE1_FIELD4;
                        	strcpy(&tempBuf[i], "P/N ");
						 	i = STL_CIB_LINE1_FIELD5;
                        	memcpy(&tempBuf[i], pCableInfo->vendor_pn, sizeof(pCableInfo->vendor_pn));
						 	i = STL_CIB_LINE1_FIELD6;
                        	strcpy(&tempBuf[i], "Rev ");
						 	i = STL_CIB_LINE1_FIELD7;
                        	memcpy(&tempBuf[i], pCableInfo->vendor_rev, sizeof(pCableInfo->vendor_rev));
						 	i = STL_CIB_LINE1_FIELD8;
                        	tempBuf[i] = '\0';
                        	printf("%*s%s\n", indent, "", tempBuf);
                        	if (detail <=1)
                        	        break;
				//line2
				memset(tempBuf, ' ', sizeof(tempBuf));
                        	i = 0;
							snprintf(&tempBuf[i], 20, "%.2f W max", (float)pCableInfo->powerMax / 4.0);
                        	tempBuf[i + strlen(&tempBuf[i])] = ' ';
						 	i = STL_CIB_LINE2_FIELD2;
                        	strcpy(&tempBuf[i], "S/N ");
						 	i = STL_CIB_LINE2_FIELD3;
                        	memcpy(&tempBuf[i], pCableInfo->vendor_sn, sizeof(pCableInfo->vendor_sn));
						 	i = STL_CIB_LINE2_FIELD4;
                        	strcpy(&tempBuf[i], "Mfg ");
						 	i = STL_CIB_LINE2_FIELD5;
                        	StlCableInfoDateCodeToText(pCableInfo->date_code, &tempBuf[i]);
                        	printf("%*s%s\n", indent, "", tempBuf);
				//line3
				memset(tempBuf, ' ', sizeof(tempBuf));
                        	i = 0;
                           	sprintf(&tempBuf[i], "OUI 0x%02X%02X%02X", pCableInfo->vendor_oui[0], pCableInfo->vendor_oui[1], pCableInfo->vendor_oui[2]);
                           	printf("%*s%s\n", indent, "", tempBuf);
				break;
			} else {
				printf("%*sQSFP Interpreted CableInfo:\n", indent, "");
				printf("%*sIdentifier: 0x%x\n", indent+4, "", pCableInfo->ident);
				snprintf(tempBuf, 20, "%.2f W max", (float)pCableInfo->powerMax / 4.0);
				printf("%*sPowerMax: %s\n", indent+4, "", tempBuf);
				memcpy(tempBuf, cableTypeInfo.connectorType, sizeof(cableTypeInfo.connectorType));
				printf("%*sConnector: %s\n", indent+4, "", tempBuf);
				StlCableInfoCableTypeToTextLong(pCableInfo->cable_type, pCableInfo->connector, tempBuf);
				printf("%*sDeviceTech: %s\n", indent+4, "", tempBuf);
				StlCableInfoDDCableLengthToText(pCableInfo->cableLengthEnc, sizeof(tempBuf), tempBuf);
				printf("%*sCableLength: %s\n", indent+4, "", tempBuf);
				memcpy(tempStr, pCableInfo->vendor_name, sizeof(pCableInfo->vendor_name));
				tempStr[sizeof(pCableInfo->vendor_name)] = '\0';
				printf("%*sVendorName: %s\n", indent+4, "", tempStr);
				printf( "%*sVendorOUI: 0x%02x%02x%02x\n", indent+4, "", pCableInfo->vendor_oui[0],
				pCableInfo->vendor_oui[1], pCableInfo->vendor_oui[2] );
				memcpy(tempStr, pCableInfo->vendor_pn, sizeof(pCableInfo->vendor_pn));
				tempStr[sizeof(pCableInfo->vendor_pn)] = '\0';
				printf("%*sVendorPN: %s\n", indent+4, "", tempStr);
				memcpy(tempStr, pCableInfo->vendor_rev, sizeof(pCableInfo->vendor_rev));
				tempStr[sizeof(pCableInfo->vendor_rev)] = '\0';
				printf("%*sVendorRev: %s\n", indent+4, "", tempStr);
				memcpy(tempStr, pCableInfo->vendor_sn, sizeof(pCableInfo->vendor_sn));
				tempStr[sizeof(pCableInfo->vendor_sn)] = '\0';
				printf("%*sVendorSN: %s\n", indent+4, "", tempStr);
				StlCableInfoDateCodeToText(pCableInfo->date_code, tempBuf);
				printf("%*sDateCode: %s\n", indent+4, "", tempBuf);
			}
			break;
		case STL_PORT_TYPE_SI_PHOTONICS:
		default:
			//printf("%*sCableInfo: N/A for Port Type: %s \n", indent, "", StlPortTypeToText(portType));
			break;
		}
		break;
	case FORMAT_XML:
		switch (portType) {
		case STL_PORT_TYPE_STANDARD:
			printf("%*s<CableInfo>\n", indent, "");
			if (detail <= 6) {
                StringCopy(tempBuf, cableTypeInfo.cableTypeShortDesc, strlen(cableTypeInfo.cableTypeShortDesc)+1);
				XmlPrintStr("DeviceTechShort", tempBuf, indent+4);
				//maps to text line 1
				StlCableInfoDDCableLengthToText(pCableInfo->cableLengthEnc, sizeof(tempBuf), tempBuf);
				XmlPrintStr("CableLength", tempBuf, indent+4);
				XmlPrintStr("Length", tempBuf, indent+4);
				XmlPrintStrLen("VendorName", (char*)pCableInfo->vendor_name, sizeof(pCableInfo->vendor_name), indent+4);
				XmlPrintStrLen("VendorPN", (char*)pCableInfo->vendor_pn, sizeof(pCableInfo->vendor_pn), indent+4);
				XmlPrintStrLen("VendorRev", (char*)pCableInfo->vendor_rev, sizeof(pCableInfo->vendor_rev), indent+4);
				snprintf(tempBuf, 20, "%.2f W max", (float)pCableInfo->powerMax / 4.0);
				XmlPrintStr("PowerMax", tempBuf, indent+4);
				XmlPrintStrLen("VendorSN", (char*)pCableInfo->vendor_sn, sizeof(pCableInfo->vendor_sn), indent+4);
				StlCableInfoDateCodeToText(pCableInfo->date_code, tempBuf);
				XmlPrintStrLen("DateCode", (char*)tempBuf, sizeof(tempBuf), indent+4);
				XmlPrintHex("VendorOUI", (pCableInfo->vendor_oui[0]<<16) + (pCableInfo->vendor_oui[1]<<8) + pCableInfo->vendor_oui[2], indent+4);
			} else {
				StlCableInfoCableTypeToTextShort(pCableInfo->cable_type, pCableInfo->connector, tempBuf);
				snprintf(tempBuf, 5, "0x%x", pCableInfo->ident);
				XmlPrintStr("Identifier", tempBuf, indent+4);
				snprintf(tempBuf, 20, "%.2f W max", (float)pCableInfo->powerMax / 4.0);
				XmlPrintStr("PowerMax", tempBuf, indent+4);
				memcpy(tempBuf, cableTypeInfo.connectorType, sizeof(cableTypeInfo.connectorType));
				XmlPrintStr("Connector", tempBuf, indent+4);
				StlCableInfoCableTypeToTextLong(pCableInfo->cable_type, pCableInfo->connector, tempBuf);
				XmlPrintStr("DeviceTech", tempBuf, indent+4);
				//maps to text line 1
				StlCableInfoDDCableLengthToText(pCableInfo->cableLengthEnc, sizeof(tempBuf), tempBuf);
				XmlPrintStr("CableLength", tempBuf, indent+4);
				XmlPrintStr("Length", tempBuf, indent+4);
				XmlPrintStrLen("VendorName", (char*)pCableInfo->vendor_name, sizeof(pCableInfo->vendor_name), indent+4);
				XmlPrintHex("VendorOUI", (pCableInfo->vendor_oui[0]<<16) + (pCableInfo->vendor_oui[1]<<8) + pCableInfo->vendor_oui[2], indent+4);
				XmlPrintStrLen("VendorPN", (char*)pCableInfo->vendor_pn, sizeof(pCableInfo->vendor_pn), indent+4);
				XmlPrintStrLen("VendorRev", (char*)pCableInfo->vendor_rev, sizeof(pCableInfo->vendor_rev), indent+4);
				XmlPrintStrLen("VendorSN", (char*)pCableInfo->vendor_sn, sizeof(pCableInfo->vendor_sn), indent+4);
				StlCableInfoDateCodeToText(pCableInfo->date_code, tempBuf);
				XmlPrintStrLen("DateCode", (char*)tempBuf, sizeof(tempBuf), indent+4);
			}
			printf("%*s</CableInfo>\n", indent, "");
			break;
		case STL_PORT_TYPE_SI_PHOTONICS:
		default:
			//printf("%*s<CableInfo>\n", indent, "");
			//XmlPrintStr("PortTypeNotSupported", StlPortTypeToText(portType), indent+4 );
			//printf("%*s</CableInfo>\n", indent, "");
			break;
		}
		break;
	default:
		break;
	}	
}	// End of ShowCableSummaryDD()

// show 1 port in a link in brief 1 line form
void ShowLinkPortBriefSummary(PortData *portp, const char *prefix,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%4s ", indent, "", prefix);
		
		printf("0x%016"PRIx64" %3u %2s   %.*s\n",
			portp->nodep->NodeInfo.NodeGUID,
			portp->PortNum,
			StlNodeTypeToText(portp->nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp->nodep->NodeDesc.NodeString);
		if (portp->nodep->enodep && portp->nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", portp->nodep->enodep->details);
		}
		if (detail) {
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				printf("%*sPortDetails: %s\n", indent+4, "", portselp->details);
			}
			
		}
		if (detail > 1 && portp->pCableInfoData)
			ShowCableSummary(portp->pCableInfoData, FORMAT_TEXT, indent+4, detail-1, portp->PortInfo.PortPhysConfig.s.PortType);

		break;
	case FORMAT_XML:
		// MTU is output as part of LinkFrom directly in <Link> tag
		printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintHex64("NodeGUID",
				portp->nodep->NodeInfo.NodeGUID, indent+4);
		if (portp->PortGUID)
			XmlPrintHex64("PortGUID", portp->PortGUID, indent+4);
		XmlPrintDec("PortNum", portp->PortNum, indent+4);
		XmlPrintNodeType(portp->nodep->NodeInfo.NodeType,
						indent+4);
		XmlPrintNodeDesc((char*)portp->nodep->NodeDesc.NodeString, indent+4);
		if (portp->nodep->enodep && portp->nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", portp->nodep->enodep->details, indent+4);
		}
		if (detail) {
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				XmlPrintOptionalStr("PortDetails", portselp->details, indent+4);
			}
			
		}

		break;
	default:
		break;
	}
	if (callback && detail)
		(*callback)(context, portp, format, indent+4, detail-1);
	if (format == FORMAT_XML)
		printf("%*s</Port>\n", indent, "");
}

// show 1 port in a link in multi-line form with heading per field
void ShowLinkPortSummary(PortData *portp, const char *prefix,
			Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%4s Name: %.*s\n",
			indent, "", prefix,
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp->nodep->NodeDesc.NodeString);
		printf("%*sNodeGUID: 0x%016"PRIx64" Type: %s PortNum: %3u\n",
			indent+4, "",
			portp->nodep->NodeInfo.NodeGUID,
			StlNodeTypeToText(portp->nodep->NodeInfo.NodeType),
			portp->PortNum);
		if (portp->nodep->enodep && portp->nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", portp->nodep->enodep->details);
		}
		if (detail) {
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				printf("%*sPortDetails: %s\n", indent+4, "", portselp->details);
			}
		}
		break;
	case FORMAT_XML:
		ShowLinkPortBriefSummary(portp, prefix, 
			0, NULL, format, indent, detail);
		break;
	default:
		break;
	}
}

// show cable information for a link in brief summary format
void ShowExpectedLinkBriefSummary(ExpectedLink *elinkp,
			Format_t format, int indent, int detail)
{
	if (! elinkp)
		return;
	switch (format) {
	case FORMAT_TEXT:
		if (elinkp->details) {
			printf("%*sLinkDetails: %s\n", indent, "", elinkp->details);
		}
		if (elinkp->CableData.length || elinkp->CableData.label
			|| elinkp->CableData.details) {
			// TBD should g_noname suppress some of this?
			printf("%*sCable: %-*s %-*s\n",
				indent, "",
				CABLE_LABEL_STRLEN, OPTIONAL_STR(elinkp->CableData.label),
				CABLE_LENGTH_STRLEN, OPTIONAL_STR(elinkp->CableData.length));
			if (elinkp->CableData.details)
				printf("%*s%s\n", indent, "", elinkp->CableData.details);
		}
		break;
	case FORMAT_XML:
		indent-= 4;	// hack to fix indent level
		if (elinkp->details) {
			XmlPrintOptionalStr("LinkDetails", elinkp->details, indent);
		}
		if (elinkp->CableData.length || elinkp->CableData.label
			|| elinkp->CableData.details) {
			printf("%*s<Cable>\n", indent, "");
			XmlPrintOptionalStr("CableLabel", elinkp->CableData.label, indent+4);
			XmlPrintOptionalStr("CableLength", elinkp->CableData.length, indent+4);
			XmlPrintOptionalStr("CableDetails", elinkp->CableData.details, indent+4);
			printf("%*s</Cable>\n", indent, "");
		}
		break;
	default:
		break;
	}
}

// show cable information for a link in multi-line format with field headings
void ShowExpectedLinkSummary(ExpectedLink *elinkp,
			Format_t format, int indent, int detail)
{
	if (! elinkp)
		return;
	ASSERT(elinkp->portp1 && elinkp->portp1->neighbor == elinkp->portp2);
	switch (format) {
	case FORMAT_TEXT:
		if (elinkp->details) {
			printf("%*sLinkDetails: %s\n", indent, "", elinkp->details);
		}
		if (elinkp->CableData.length || elinkp->CableData.label
			|| elinkp->CableData.details) {
			printf("%*sCableLabel: %-*s  CableLen: %-*s\n",
				indent, "",
				CABLE_LABEL_STRLEN, OPTIONAL_STR(elinkp->CableData.label),
				CABLE_LENGTH_STRLEN, OPTIONAL_STR(elinkp->CableData.length));
			printf("%*sCableDetails: %s\n",
				indent, "",
				OPTIONAL_STR(elinkp->CableData.details));
		}
		break;
	case FORMAT_XML:
		ShowExpectedLinkBriefSummary(elinkp, format, indent, detail);
		break;
	default:
		break;
	}
}

// show from side of a link, need to later call ShowLinkToBriefSummary
// useful when traversing trace route and don't have both sides of link handy
void ShowLinkFromBriefSummary(PortData *portp1,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail)
{
	if (format == FORMAT_XML) {
		XmlPrintLinkStartTag("Link", portp1, indent);
		indent+=4;
		XmlPrintRate(portp1->rate, indent);
		XmlPrintDec("Internal", isInternalLink(portp1)?1:0, indent);
		if (detail)
			ShowExpectedLinkBriefSummary(portp1->elinkp, format, indent+4, detail-1);
	}
	ShowLinkPortBriefSummary(portp1, StlStaticRateToText(portp1->rate), 
						context, callback, format, indent, detail);
}

// show to side of a link, need to call ShowLinkFromBriefSummary before this
// useful when traversing trace route and don't have both sides of link handy
// portp2 can be NULL to "close" the From Summary without additional
// port information and no cable information
// This is useful when reporting trace routes which stay within a single port
void ShowLinkToBriefSummary(PortData *portp2, const char* toprefix, boolean close_link,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail)
{
	if (format == FORMAT_XML)
		indent +=4;
	// portp2 should not be NULL, but code this defensively
	if (portp2) {
		ShowLinkPortBriefSummary(portp2, toprefix, 
							context, callback, format, indent, detail);
		DEBUG_ASSERT(portp2->elinkp == portp2->neighbor->elinkp);
		if (detail && format != FORMAT_XML)
			ShowExpectedLinkBriefSummary(portp2->elinkp, format, indent+4, detail-1);
		else if ((detail > 1 && portp2->pCableInfoData) && format==FORMAT_XML)
                        ShowCableSummary(portp2->pCableInfoData, FORMAT_XML, indent, detail-1, portp2->PortInfo.PortPhysConfig.s.PortType);
	}
	if (format == FORMAT_XML && close_link)
		printf("%*s</Link>\n", indent-4, "");
}

// show both sides of a link, portp1 should be the "from" port
void ShowLinkBriefSummary(PortData *portp1, const char* toprefix, Format_t format, int indent, int detail)
{
	ShowLinkFromBriefSummary(portp1, 0, NULL, format, indent, detail);
	ShowLinkToBriefSummary(portp1->neighbor, toprefix, TRUE, 0, NULL, format, indent, detail);
}

/*
 * There are 6 cases for routes:
 * 1. FI - FI
 * 2. FI to self
 * 3. SW Port 0 to FI
 * 4. FI to SW Port 0
 * 5. SW Port 0 to SW Port 0
 * 6. SW Port 0 to self
 *
 * Two self consistent Perspectives of these cases:
 *
 * Perspective 1:  Show all "Links" along the route
 *   - every Link is a connection between 2 devices
 *   - every Link involves 2 Ports on different devices
 *   - never show SW Port 0 in a route
 *   - never show any ports for a "talk to self" route
 *   - similarly -F route:... would only select ports which -o route would show
 *
 * Perspective 2: Show all "Ports" along the route
 *   - route is a list of Ports (not Links)
 *   - show every port, including port 0 at start and/or end
 *   - for "talk to self" routes, show just the 1 port involved
 *   - similarly -F route:... would select all ports involved in the route
 *
 * The code below implements Perspective 1.  Some code in #if 0 and some
 * comments discuss possible approaches to implement perspective 2.
 * If the future, perspective 2 could become runtime if flags based on a
 * new parameter to this function.
 */

/* obtain and output the trace route information for the given path between
 * the given pair of ports.  The ports are provided to aid in tranversing
 * the PortData and NodeData records and as an easy way to verify the
 * concistency of the trace route query results against our previous
 * port, node and link record queries.
 */
FSTATUS ShowTraceRoute(EUI64 portGuid, PortData *portp1, PortData *portp2,
					IB_PATH_RECORD *pathp,
					Format_t format, int indent, int detail)
{
	FSTATUS status;
	STL_TRACE_RECORD	*pTraceRecords = NULL;
	uint32 NumTraceRecords;
	int i = -1;
	uint32 links = 0;
	PortData *p = portp1;
	int p_shown = 0;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	struct omgt_port *omgt_port_session = NULL;

	if (format == FORMAT_XML) {
		printf("%*s<Route>\n", indent, "");
		indent+=4;
	}
	ShowPathRecord(pathp, format, indent, detail);

	if (portp1 == portp2) {
		/* special case, internal loopback */
#if 0	// enable for perspective 2
		//as coded below this results a <Link> with just 1 <Port>
		//For perspective 2, should instead output a list of Ports
		//(one port in this case) and show MTU and Rate per Port
		if (detail) {
			// detail=0 avoids link info and cable info, LinkTo needed to close
			// XML output's <Link> tag
			//ShowLinkBriefSummaryHeader(format, indent+4, 0);
			//ShowLinkFromBriefSummary(portp1, 0, NULL, format, indent+4, 0);
			//ShowLinkToBriefSummary(NULL, "->  ", TRUE, 0, NULL, format, indent+4, 0);
			show port portp1 in report
		}
		output "1 Ports Traversed"
#else
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s0 Links Traversed\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<LinksTraversed>0</LinksTraversed>\n", indent, "");
			break;
		default:
			break;
		}
#endif
		status = FSUCCESS;
		goto done;
	}
	if (portp1->neighbor == portp2) {
		/* special case, single link traversed */
		// Since portp1 has a neighbor, neither port is SW Port 0
#if 0	// enable for perspective 2
		if (detail) {
			show port portp1 in report
			show port portp2 in report
		}
		output "2 Ports Traversed"
#else
		if (detail) {
			ShowLinkBriefSummaryHeader(format, indent+4, detail-1);
			ShowLinkBriefSummary(portp1, "->  ", format, indent+4, detail-1);
		}
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s1 Links Traversed\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<LinksTraversed>1</LinksTraversed>\n", indent, "");
			break;
		default:
			break;
		}
#endif
		status = FSUCCESS;
		goto done;
	}


	if (!(g_Fabric.flags & FF_SMADIRECT) && ! g_snapshot_in_file) {
		struct omgt_params params = {.debug_file = g_verbose > 2 ? stdout : NULL};
		status = omgt_open_port_by_guid(&omgt_port_session, portGuid, &params);
		if (status != FSUCCESS) {
			fprintf(stderr, "Unable to open fabric interface.\n");
			goto done;
		} else {
			status = GetTraceRoute(omgt_port_session, pathp, &pQueryResults);
			if (FSUCCESS != status) {
				g_exitstatus = 1;
				goto done;
			}
		}
		NumTraceRecords = ((STL_TRACE_RECORD_RESULTS*)pQueryResults->QueryResult)->NumTraceRecords;
		pTraceRecords = ((STL_TRACE_RECORD_RESULTS*)pQueryResults->QueryResult)->TraceRecords;
	} else {
		status = GenTraceRoutePath(&g_Fabric, pathp, &pTraceRecords, &NumTraceRecords);
		if (FSUCCESS != status) {
			if (status == FUNAVAILABLE) {
				fprintf(stderr, "opareport: Routing Tables not available in snapshot\n");
				g_exitstatus = 1;
			} else if (status == FNOT_DONE) {
				switch (format) {
				case FORMAT_TEXT:
					printf("%*sRoute Incomplete\n", indent, "");
					break;
				case FORMAT_XML:
					printf("%*s<LinksTraversed>0</LinksTraversed> <!-- Route Incomplete -->\n", indent, "");
					break;
				default:
					break;
				}
				// don't fail just because some routes are incomplete
				status = FSUCCESS;
			} else {
				fprintf(stderr, "opareport: Unable to determine route: (status=0x%x): %s\n", status, iba_fstatus_msg(status));
				g_exitstatus = 1;
			}
			goto done;
		}
	}

	ASSERT(NumTraceRecords > 0);
	//printf("%*s%d Hops\n", indent, "", NumTraceRecords-1);

	/* the first Trace record should be the exit from portp1, however
	 * not all versions of the SM report this record
	 */
	if (detail)
		ShowLinkBriefSummaryHeader(format, indent+4, detail-1);
	if (pTraceRecords[0].NodeType != portp1->nodep->NodeInfo.NodeType) {
		/* workaround SM bug, did not report initial exit port */
		// assume portp1 is not a Switch Port 0
		p = portp1->neighbor;
		if (! p) {
			DBGPRINT("incorrect 1st trace record\n");
			goto badroute;
		}
		if (detail) {
#if 0	// enable for perspective 2
			show port portp1 in report
#else
			ShowLinkFromBriefSummary(portp1, 0, NULL, format, indent+4, detail-1);
#endif
		}
	}
	for (i=0; i< NumTraceRecords; i++) {
		if (g_verbose)
			ShowTraceRecord(&pTraceRecords[i], format, indent+4, detail-1);
		if (p != portp1) {
			if (detail) {
#if 0	// enable for perspective 2
				show port p in report
#else
				ShowLinkToBriefSummary(p, "->  ", TRUE, 0, NULL, format, indent+4, detail-1);
#endif
			}
			links++;
			p_shown = 1;
		}
		if (pTraceRecords[i].NodeType != STL_NODE_FI) {
#if 0	// enable for perspective 2
			if (i==0 && p == portp1) // must be starting at switch Port 0
				output portp1 to report
#endif
			p = FindNodePort(p->nodep, pTraceRecords[i].ExitPort);
			if (! p) {
				DBGPRINT("SW port not found\n");
				goto badroute;
			}
			if (0 == p->PortNum) {
				/* Switch Port 0 thus must be final port */
				if (i+1 != NumTraceRecords) {
					DBGPRINT("final switch port 0 error\n");
					goto badroute;
				}
#if 0	// enable for perspective 2
				if (detail)
					show port p in report
#endif
				break;
			}

			if (detail) {
#if 0	// enable for perspective 2
				show port p in report
#else
				ShowLinkFromBriefSummary(p, 0, NULL, format, indent+4, detail-1);
#endif
			}
			if (p == portp2) {
				// this should not happen.  If we reach portp2 as the exit
				// port of a switch, that implies portp2 must be port 0 of
				// the switch which the test above should have caught
				// but it doesn't hurt to have this redundant test here to be
				// safe.
				/* final port must be Switch Port 0 */
				if (i+1 != NumTraceRecords) {
					DBGPRINT("final switch port 0 error\n");
					goto badroute;
				}
			} else {
				p = p->neighbor;
				if (! p) {
					DBGPRINT("incorrect neighbor port\n");
					goto badroute;
				}
				p_shown = 0;
			}
		} else if (i == 0) {
			/* since we caught FI to FI case above, SM must have given us
			 * initial Node in path
			 */
			if (detail) {
#if 0	// enable for perspective 2
				show port portp1 in report
#else
				ShowLinkFromBriefSummary(portp1, 0, NULL, format, indent+4, detail-1);
#endif
			}
			/* unfortunately spec says Exit and Entry Port are 0 for FI, so
			 * can't verify consistency with portp1
			 */
			p = portp1->neighbor;
			if (! p) {
				DBGPRINT("1st port with no neighbor\n");
				goto badroute;
			}
			p_shown = 0;
		} else if (i+1 != NumTraceRecords) {
			DBGPRINT("extra unexpected trace records\n");
			goto badroute;
		}
	}
	if (! p_shown) {
		/* workaround SM bug, did not report final hop in route */
		if (detail) {
#if 0	// enable for perspective 2
			show port p in report
#else
			ShowLinkToBriefSummary(p, "->  ", TRUE, 0, NULL, format, indent+4, detail-1);
#endif
		}
	}
	if (p != portp2) {
		DBGPRINT("ended at wrong port\n");
		goto badroute;
	}
#if 0	// enable for perspective 2
	show Ports Traversed
#else
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Links Traversed\n", indent, "", links);
		break;
	case FORMAT_XML:
		printf("%*s<LinksTraversed>%u</LinksTraversed>\n", indent, "", links);
		break;
	default:
		break;
	}
#endif

done:
	if (format == FORMAT_XML) {
		indent-=4;
		printf("%*s</Route>\n", indent, "");
	}

	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	if (omgt_port_session != NULL)
		omgt_close_port(omgt_port_session);
	if (g_snapshot_in_file && pTraceRecords)
		MemoryDeallocate(pTraceRecords);
	return status;

badroute:
	status = FSUCCESS;	// might as well process what we can
	fprintf(stderr, "%*sRoute reported by SM inconsistent with Trace Route\n", indent+4, "");
	if (g_verbose && i+1 < NumTraceRecords) {
		if (format == FORMAT_TEXT)
			printf("%*sRemainder of Route:\n", indent+4, "");
		// Don't repeat records we already output above
		for (i=i+1; i< NumTraceRecords; i++)
			ShowTraceRecord(&pTraceRecords[i], format, indent+8, detail-1);
	}
	goto done;
}

/* show trace routes for all paths between 2 given ports */
FSTATUS ShowPortsTraceRoutes(EUI64 portGuid, PortData *portp1, PortData *portp2, Format_t format, int indent, int detail)
{
	FSTATUS status;

	PQUERY_RESULT_VALUES pQueryResults = NULL;
	uint32 NumPathRecords;
	IB_PATH_RECORD *pPathRecords = NULL;
	struct omgt_port *omgt_port_session = NULL;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sRoutes between ports:\n", indent, "");
		ShowLinkPortBriefSummary(portp1, "    ", 0, NULL, format, indent, 0);
		ShowLinkPortBriefSummary(portp2, "and ", 0, NULL, format, indent, 0);
		break;
	case FORMAT_XML:
		printf("%*s<PortRoutes>\n", indent, "");
		ShowLinkPortBriefSummary(portp1, "    ", 0, NULL, format, indent+4, 0);
		ShowLinkPortBriefSummary(portp2, "and ", 0, NULL, format, indent+4, 0);
		break;
	default:
		break;
	}

	if (! g_snapshot_in_file) {
		struct omgt_params params = {.debug_file = g_verbose > 2 ? stdout : NULL};
		status = omgt_open_port_by_guid(&omgt_port_session, portGuid, &params);
		if (FSUCCESS != status)
			goto done;
		status = GetPaths(omgt_port_session, portp1, portp2, &pQueryResults);
		if (FSUCCESS != status)
			goto done;
		NumPathRecords = ((PATH_RESULTS*)pQueryResults->QueryResult)->NumPathRecords;
		pPathRecords = ((PATH_RESULTS*)pQueryResults->QueryResult)->PathRecords;
	} else {
		status = GenPaths(&g_Fabric, portp1, portp2, &pPathRecords, &NumPathRecords);
		if (FSUCCESS != status)
			goto done;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Paths\n", indent, "", NumPathRecords);
		break;
	case FORMAT_XML:
		printf("%*s<Paths>%d</Paths>\n", indent+4, "", NumPathRecords);
		break;
	default:
		break;
	}
	if (detail) {
		int i;

		for (i=0; i< NumPathRecords; i++) {
			ShowTraceRoute(portGuid, portp1, portp2, &pPathRecords[i], format, indent+4, detail-1);
		}
	}

done:
	if (format == FORMAT_XML)
		printf("%*s</PortRoutes>\n", indent, "");

	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	if (omgt_port_session != NULL)
		omgt_close_port(omgt_port_session);
	if (g_snapshot_in_file && pPathRecords)
		MemoryDeallocate(pPathRecords);

	return status;
}

/* show trace routes for all paths between given port and node */
FSTATUS ShowPortNodeTraceRoutes(EUI64 portGuid, PortData *portp1, NodeData *nodep2, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;

	for (p=cl_qmap_head(&nodep2->Ports); p != cl_qmap_end(&nodep2->Ports); p = cl_qmap_next(p)) {
		PortData *portp2= PARENT_STRUCT(p, PortData, NodePortsEntry);
		ShowPortsTraceRoutes(portGuid, portp1, portp2, format, indent, detail);
	}
	return FSUCCESS;
}

/* show trace routes for all paths between given port and point */
FSTATUS ShowPortPointTraceRoutes(EUI64 portGuid, PortData *portp1, Point *point2, Format_t format, int indent, int detail)
{
	switch (point2->Type) {
	case POINT_TYPE_PORT:
		return ShowPortsTraceRoutes(portGuid, portp1, point2->u.portp, format, indent, detail);
	case POINT_TYPE_PORT_LIST:
		{
		LIST_ITERATOR i;
		DLIST *pList = &point2->u.portList;

		for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
			PortData *portp = (PortData*)ListObj(i);
			ShowPortsTraceRoutes(portGuid, portp1, portp, format, indent, detail);
		}
		}
		return FSUCCESS;
	case POINT_TYPE_NODE:
		return ShowPortNodeTraceRoutes(portGuid, portp1, point2->u.nodep, format, indent, detail);
	case POINT_TYPE_NODE_LIST:
		{
		LIST_ITERATOR i;
		DLIST *pList = &point2->u.nodeList;

		for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
			NodeData *nodep = (NodeData*)ListObj(i);
			ShowPortNodeTraceRoutes(portGuid, portp1, nodep, format, indent, detail);
		}
		}
		return FSUCCESS;
	case POINT_TYPE_IOC:
		return ShowPortNodeTraceRoutes(portGuid, portp1, point2->u.iocp->ioup->nodep, format, indent, detail);
	case POINT_TYPE_IOC_LIST:
		{
		LIST_ITERATOR i;
		DLIST *pList = &point2->u.nodeList;

		for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
			IocData *iocp = (IocData*)ListObj(i);
			ShowPortNodeTraceRoutes(portGuid, portp1, iocp->ioup->nodep, format, indent, detail);
		}
		}
		return FSUCCESS;
	case POINT_TYPE_SYSTEM:
		{
		cl_map_item_t *p;
		SystemData *systemp = point2->u.systemp;

		for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
			NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
			ShowPortNodeTraceRoutes(portGuid, portp1, nodep, format, indent, detail);
		}
		return FSUCCESS;
		}
	default:
		return FINVALID_PARAMETER;
	}
}

/* show trace routes for all paths between given node and point */
FSTATUS ShowNodePointTraceRoutes(EUI64 portGuid, NodeData *nodep1, Point *point2, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;

	for (p=cl_qmap_head(&nodep1->Ports); p != cl_qmap_end(&nodep1->Ports); p = cl_qmap_next(p)) {
		PortData *portp1 = PARENT_STRUCT(p, PortData, NodePortsEntry);
		ShowPortPointTraceRoutes(portGuid, portp1, point2, format, indent, detail);
	}
	return FSUCCESS;
}

/* show trace routes for all paths between 2 given points */
FSTATUS ShowPointsTraceRoutes(EUI64 portGuid, Point *point1, Point *point2, Format_t format, int indent, int detail)
{
	switch (point1->Type) {
	case POINT_TYPE_PORT:
		return ShowPortPointTraceRoutes(portGuid, point1->u.portp, point2, format, indent, detail);
	case POINT_TYPE_PORT_LIST:
		{
		LIST_ITERATOR i;
		DLIST *pList = &point1->u.portList;

		for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
			PortData *portp = (PortData*)ListObj(i);
			ShowPortPointTraceRoutes(portGuid, portp, point2, format, indent, detail);
		}
		}
		return FSUCCESS;
	case POINT_TYPE_NODE:
		return ShowNodePointTraceRoutes(portGuid, point1->u.nodep, point2, format, indent, detail);
	case POINT_TYPE_NODE_LIST:
		{
		LIST_ITERATOR i;
		DLIST *pList = &point1->u.nodeList;

		for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
			NodeData *nodep = (NodeData*)ListObj(i);
			ShowNodePointTraceRoutes(portGuid, nodep, point2, format, indent, detail);
		}
		}
		return FSUCCESS;
	case POINT_TYPE_IOC:
		return ShowNodePointTraceRoutes(portGuid, point1->u.iocp->ioup->nodep, point2, format, indent, detail);
	case POINT_TYPE_IOC_LIST:
		{
		LIST_ITERATOR i;
		DLIST *pList = &point1->u.iocList;

		for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
			IocData *iocp = (IocData*)ListObj(i);
			ShowNodePointTraceRoutes(portGuid, iocp->ioup->nodep, point2, format, indent, detail);
		}
		}
		return FSUCCESS;
	case POINT_TYPE_SYSTEM:
		{
		cl_map_item_t *p;
		SystemData *systemp = point1->u.systemp;

		for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
			NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
			ShowNodePointTraceRoutes(portGuid, nodep, point2, format, indent, detail);
		}
		return FSUCCESS;
		}
	default:
		return FINVALID_PARAMETER;
	}
}

void ShowPointNodeBriefSummary(const char* prefix, NodeData *nodep, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%s0x%016"PRIx64" %s %.*s\n",
			indent, "", prefix,
			nodep->NodeInfo.NodeGUID,
			StlNodeTypeToText(nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
				g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString);
		if (nodep->enodep && nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", nodep->enodep->details);
		}
		break;
	case FORMAT_XML:
		printf("%*s<Node id=\"0x%016"PRIx64"\">\n", indent, "",
					nodep->NodeInfo.NodeGUID);
		XmlPrintHex64("NodeGUID", nodep->NodeInfo.NodeGUID,
						indent+4);
		XmlPrintNodeType(nodep->NodeInfo.NodeType, indent+4);
		XmlPrintNodeDesc(
				(char*)nodep->NodeDesc.NodeString, indent+4);
		if (nodep->enodep && nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", nodep->enodep->details, indent+4);
		}
		printf("%*s</Node>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowPointPortBriefSummary(const char* prefix, PortData *portp, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		if (portp->PortGUID)
			printf("%*s%s%3u 0x%016"PRIx64"\n",
				indent, "", prefix,
				portp->PortNum,
				portp->PortGUID);
		else
			printf("%*s%s%3u\n",
				indent, "", prefix,
				portp->PortNum);
		ShowPointNodeBriefSummary("in Node: ", portp->nodep, format,
								indent+4, detail);
		break;
	case FORMAT_XML:
		printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintDec("PortNum", portp->PortNum, indent+4);
		if (portp->PortGUID)
			XmlPrintHex64("PortGUID", portp->PortGUID, indent+4);
		ShowPointNodeBriefSummary("in Node: ", portp->nodep, format,
								indent+4, detail);
		printf("%*s</Port>\n", indent, "");
		break;
	default:
		break;
	}
}

// show 1 port selector in link data in brief form
// designed to be called for side 1 then side 2
void ShowExpectedLinkPortSelBriefSummary(const char* prefix,
			ExpectedLink *elinkp, PortSelector *portselp,
			uint8 side, ExpectedLinkSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail)
{
	DEBUG_ASSERT(side == 1 || side == 2);
	switch (format) {
	case FORMAT_TEXT:
		{
		int prefix_len = strlen(prefix);
		if (side == 1) {
			printf("%*s%s%4s ", indent, "", prefix,
					elinkp->expected_rate?
						StlStaticRateToText(elinkp->expected_rate)
						:"    ");
		} else {
			printf("%*s%*s%4s ", indent, "", prefix_len, "",
					"<-> ");
		}
		if (side == 1 && elinkp->expected_mtu)
			printf("%5s ",
				IbMTUToText(elinkp->expected_mtu));
		else
			printf("      ");
		if (portselp) {
			if (portselp->NodeGUID)
				printf("0x%016"PRIx64, portselp->NodeGUID);
			else
				printf("                  ");
			if (portselp->gotPortNum)
				printf(" %3u               ",portselp->PortNum);
			else if (portselp->PortGUID)
				printf(" 0x%016"PRIx64, portselp->PortGUID);
			else
				printf("                   ");
			if (portselp->NodeType)
				printf(" %s",
					StlNodeTypeToText(portselp->NodeType));
			else
				printf("   ");
			if (portselp->NodeDesc)
				printf(" %.*s\n",
					NODE_DESCRIPTION_ARRAY_SIZE, g_noname?g_name_marker:portselp->NodeDesc);
			else
				printf("\n");
			if (detail) {
				if (portselp->details) {
					// TBD should g_noname suppress some of this?
					printf("%*sPortDetails: %s\n", indent+4+prefix_len, "", portselp->details);
				}
			}
		}
		}
		break;
	case FORMAT_XML:
		// MTU is output as part of LinkFrom directly in <Link> tag
		if (portselp) {
			printf("%*s<Port id=\"%u\">\n", indent, "", side);
			if (portselp->NodeGUID)
				XmlPrintHex64("NodeGUID", portselp->NodeGUID, indent+4);
			if (portselp->gotPortNum)
				XmlPrintDec("PortNum", portselp->PortNum, indent+4);
			if (portselp->PortGUID)
				XmlPrintHex64("PortGUID", portselp->PortGUID, indent+4);
			if (portselp->NodeType)
				XmlPrintNodeType(portselp->NodeType, indent+4);
			if (portselp->NodeDesc)
				XmlPrintNodeDesc(portselp->NodeDesc, indent+4);
			if (detail) {
				if (portselp->details) {
					// TBD should g_noname suppress some of this?
					XmlPrintOptionalStr("PortDetails", portselp->details, indent+4);
				}
			}
		}
		break;
	default:
		break;
	}
	if (callback && detail)
		(*callback)(elinkp, side, format, indent+4, detail-1);
	if (format == FORMAT_XML)
		printf("%*s</Port>\n", indent, "");
}

void ShowPointExpectedLinkBriefSummary(const char* prefix, ExpectedLink *elinkp, Format_t format, int indent, int detail)
{
	// top level information about link
	if (format == FORMAT_XML) {
		printf("%*s<InputLink id=\"0x%016"PRIx64"\">\n", indent, "", (uint64)(uintn)elinkp);
		indent+=4;
		if (elinkp->expected_rate)
	 		XmlPrintRate(elinkp->expected_rate, indent);
		if (elinkp->expected_mtu)
			XmlPrintDec("MTU",
				GetBytesFromMtu(elinkp->expected_mtu), indent);
		XmlPrintDec("Internal", elinkp->internal, indent);
		if (detail)
			ShowExpectedLinkBriefSummary(elinkp, format, indent+4, detail-1);
	}

	// From Side (Port 1)
	ShowExpectedLinkPortSelBriefSummary(prefix, elinkp, elinkp->portselp1,
					1, NULL, format, indent, detail);

	// To Side (Port 2)
	ShowExpectedLinkPortSelBriefSummary(prefix, elinkp, elinkp->portselp2,
					2, NULL, format, indent, detail);

	// Summary information about Link itself
	if (detail && format != FORMAT_XML)
		ShowExpectedLinkBriefSummary(elinkp, format, indent+4, detail-1);
	if (format == FORMAT_XML)
		printf("%*s</InputLink>\n", indent-4, "");
}

void ShowPointBriefSummary(Point* point, uint8 find_flag, Format_t format, int indent, int detail)
{
	ASSERT(PointValid(point));
	if (find_flag & FIND_FLAG_FABRIC) {
		switch (point->Type) {
		case POINT_TYPE_NONE:
			break;
		case POINT_TYPE_PORT:
			ShowPointPortBriefSummary("Port: ", point->u.portp, format, indent, detail);
			break;
		case POINT_TYPE_PORT_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u.portList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Ports:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<Ports>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				PortData *portp = (PortData*)ListObj(i);
				ShowPointPortBriefSummary("", portp, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</Ports>\n",
						indent, "");
			}
			break;
			}
		case POINT_TYPE_NODE:
			ShowPointNodeBriefSummary("Node: ", point->u.nodep, format, indent, detail);
			break;
		case POINT_TYPE_NODE_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u.nodeList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Nodes:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<Nodes>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				NodeData *nodep = (NodeData*)ListObj(i);
				ShowPointNodeBriefSummary("", nodep, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</Nodes>\n", indent, "");
			}
			break;
			}
		case POINT_TYPE_IOC:
		// TBD extract this and list code into common function
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sIoc: %3u 0x%016"PRIx64" %.*s\n",
					indent, "",
					point->u.iocp->IocSlot,
					point->u.iocp->IocProfile.IocGUID,
					IOC_IDSTRING_SIZE,
					g_noname?g_name_marker:(char*)point->u.iocp->IocProfile.IDString);
				break;
			case FORMAT_XML:
				printf("%*s<Ioc id=\"0x%016"PRIx64"\">\n", indent, "",
							point->u.iocp->IocProfile.IocGUID);
				XmlPrintDec("Slot", point->u.iocp->IocSlot, indent+4);
				XmlPrintHex64("IocGUID", point->u.iocp->IocProfile.IocGUID, indent+4);
				XmlPrintIocIDString(
						(char*)point->u.iocp->IocProfile.IDString, indent+4);
				break;
			default:
				break;
			}
			ShowPointNodeBriefSummary("in Node: ", point->u.iocp->ioup->nodep,
										format, indent+4, detail);
			if (format == FORMAT_XML) { 
				printf("%*s</Ioc>\n", indent, "");
			}
			break;
		case POINT_TYPE_IOC_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u.iocList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u IOCs:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<IOCs>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				IocData *iocp = (IocData*)ListObj(i);
				switch (format) {
				case FORMAT_TEXT:
					printf("%*s%3u 0x%016"PRIx64" %.*s\n",
						indent+4, "",
						iocp->IocSlot,
						iocp->IocProfile.IocGUID,
						IOC_IDSTRING_SIZE,
						g_noname?g_name_marker:(char*)iocp->IocProfile.IDString);
					break;
				case FORMAT_XML:
					printf("%*s<Ioc id=\"0x%016"PRIx64"\">\n", indent+4, "",
							iocp->IocProfile.IocGUID);
					XmlPrintDec("Slot", iocp->IocSlot, indent+8);
					XmlPrintHex64("IocGUID", iocp->IocProfile.IocGUID, indent+8);
					XmlPrintIocIDString(
						(char*)iocp->IocProfile.IDString, indent+8);
					break;
				default:
					break;
				}
				ShowPointNodeBriefSummary("in Node: ", iocp->ioup->nodep,
										format, indent+8, detail);
				if (format == FORMAT_XML) {
					printf("%*s</Ioc>\n", indent+4, "");
				}
			}
			if (format == FORMAT_XML) {
				printf("%*s</IOCs>\n",
						indent, "");
			}
			break;
			}
		case POINT_TYPE_SYSTEM:
			{
			SystemData *systemp = point->u.systemp;
			cl_map_item_t *p;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*sSystem: 0x%016"PRIx64"\n",
					indent, "",
					systemp->SystemImageGUID);
				break;
			case FORMAT_XML:
				printf("%*s<System id=\"0x%016"PRIx64"\">\n", indent, "",
						systemp->SystemImageGUID?systemp->SystemImageGUID:
						PARENT_STRUCT(cl_qmap_head(&systemp->Nodes), NodeData, SystemNodesEntry)->NodeInfo.NodeGUID);
				XmlPrintHex64("SystemImageGUID", systemp->SystemImageGUID, indent+4);
				break;
			default:
				break;
			}
			for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
				NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
				ShowPointNodeBriefSummary("Node: ", nodep, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</System>\n", indent, "");
			}
			break;
			}
		}
	}
	if (find_flag & FIND_FLAG_ENODE) {
		switch (point->EnodeType) {
		case POINT_ENODE_TYPE_NONE:
			break;
		case POINT_ENODE_TYPE_NODE:
			ShowExpectedNodeBriefSummary("Input Node: ", point->u2.enodep, "InputNode", TRUE, format, indent, detail);
			break;
		case POINT_ENODE_TYPE_NODE_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u2.enodeList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Input Nodes:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<InputNodes>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				ExpectedNode *enodep = (ExpectedNode*)ListObj(i);
				ShowExpectedNodeBriefSummary("", enodep, "InputNode", TRUE, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</InputNodes>\n", indent, "");
			}
			break;
			}
		}
	}
	if (find_flag & FIND_FLAG_ESM) {
		switch (point->EsmType) {
		case POINT_ESM_TYPE_NONE:
			break;
		case POINT_ESM_TYPE_SM:
			ShowExpectedSMBriefSummary("Input SM: ", point->u3.esmp, "InputSM", TRUE, format, indent, detail);
			break;
		case POINT_ESM_TYPE_SM_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u3.esmList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Input SMs:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<InputSMs>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				ExpectedSM *esmp = (ExpectedSM*)ListObj(i);
				ShowExpectedSMBriefSummary("", esmp, "InputSM", TRUE, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</InputSMs>\n", indent, "");
			}
			break;
			}
		}
	}
	if (find_flag & FIND_FLAG_ELINK) {
		switch (point->ElinkType) {
		case POINT_ELINK_TYPE_NONE:
			break;
		case POINT_ELINK_TYPE_LINK:
			ShowPointExpectedLinkBriefSummary("Input Link: ", point->u4.elinkp, format, indent, detail);
			break;
		case POINT_ELINK_TYPE_LINK_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u4.elinkList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Input Links:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<InputLinks>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				ExpectedLink *elinkp = (ExpectedLink*)ListObj(i);
				ShowPointExpectedLinkBriefSummary("", elinkp, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</InputLinks>\n", indent, "");
			}
			break;
			}
		}
	}
}

void ShowPointFocus(Point* focus, uint8 find_flag, Format_t format, int indent, int detail)
{
	if (! focus || g_quietfocus)
		return;
	if (PointValid(focus)) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sFocused on:\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<Focus>\n", indent, "");
			break;
		default:
			break;
		}
		ShowPointBriefSummary(focus, find_flag, format, indent+4, detail);
		if (format == FORMAT_XML)
			printf("%*s</Focus>\n", indent, "");
	}
	if (format == FORMAT_TEXT)
		printf("\n");
}

// output verbose summary of a IO Service Entry
void ShowIoServiceSummary(IOC_SERVICE *srvp, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sName: %.*s\n", indent, "", IOC_SERVICE_NAME_SIZE, srvp->Name);
		printf("%*sId: 0x%016"PRIx64"\n", indent, "", srvp->Id);
		break;
	case FORMAT_XML:
		printf("%*s<IocService id=\"%.*s\">\n", indent, "", IOC_SERVICE_NAME_SIZE, srvp->Name);
		XmlPrintServiceName(srvp->Name, indent+4);
		printf("%*s<Id>0x%016"PRIx64"</Id>\n", indent+4, "", srvp->Id);
		printf("%*s</IocService>\n", indent, "");
		break;
	default:
		break;
	}
}

// output verbose summary of a IOC
void ShowIocSummary(IocData *iocp, Format_t format, int indent, int detail)
{
	IOC_PROFILE *pIocProfile = &iocp->IocProfile;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sIocSlot: %3u GUID: 0x%016"PRIx64"\n",
				indent, "", iocp->IocSlot, pIocProfile->IocGUID);
		printf("%*sID String: %.*s\n",
				indent+4, "",
				IOC_IDSTRING_SIZE,
				g_noname?g_name_marker:(char*)pIocProfile->IDString);
		printf("%*sIO Class: 0x%x SubClass: 0x%x Protocol: 0x%x Protocol Ver: 0x%x\n",
				indent+4, "",
				pIocProfile->IOClass,
				pIocProfile->IOSubClass,
				pIocProfile->Protocol,
				pIocProfile->ProtocolVer);
		printf("%*sVendorID: 0x%x DeviceID: 0x%x Rev: 0x%x\n",
				indent+4, "",
				pIocProfile->ven.v.VendorId,
				pIocProfile->DeviceId,
				pIocProfile->DeviceVersion);
		if (detail) {
			printf("%*sSubsystem: VendorID: 0x%x DeviceID: 0x%x\n",
				indent+4, "",
				pIocProfile->sub.s.SubSystemVendorID,
				pIocProfile->SubSystemID);
			printf("%*sCapability: 0x%02x: %s%s%s%s%s%s%s%s\n",
				indent+4, "",
				pIocProfile->ccm.CntlCapMask,
				pIocProfile->ccm.ctlrCapMask.ST?"ST ":"",
				pIocProfile->ccm.ctlrCapMask.SF?"SF ":"",
				pIocProfile->ccm.ctlrCapMask.RT?"RT ":"",
				pIocProfile->ccm.ctlrCapMask.RF?"RF ":"",
				pIocProfile->ccm.ctlrCapMask.WT?"WT ":"",
				pIocProfile->ccm.ctlrCapMask.WF?"WF ":"",
				pIocProfile->ccm.ctlrCapMask.AT?"AT ":"",
				pIocProfile->ccm.ctlrCapMask.AF?"AF ":"");
			printf("%*sSend Depth: %u Size: %u; RDMA Read Depth: %u RDMA Size: %u\n",
				indent+4, "",
				pIocProfile->SendMsgDepth,
				pIocProfile->SendMsgSize,
				pIocProfile->RDMAReadDepth,
				pIocProfile->RDMASize);
		}
		printf("%*s%u Services%s\n", indent+4, "", pIocProfile->ServiceEntries, detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Ioc id=\"0x%016"PRIx64"\">\n", indent, "",
						pIocProfile->IocGUID);
		XmlPrintDec("IocSlot", iocp->IocSlot, indent+4);
		XmlPrintHex64("IocGUID", pIocProfile->IocGUID, indent+4);
		XmlPrintIocIDString((char*)pIocProfile->IDString, indent+4);
		XmlPrintHex("IOClass", pIocProfile->IOClass, indent+4);
		XmlPrintHex("IOSubClass", pIocProfile->IOSubClass, indent+4);
		XmlPrintHex("Protocol", pIocProfile->Protocol, indent+4);
		XmlPrintHex("ProtocolVer", pIocProfile->ProtocolVer, indent+4);
		XmlPrintHex("VendorID", pIocProfile->ven.v.VendorId, indent+4);
		XmlPrintHex("DeviceId", pIocProfile->DeviceId, indent+4);
		XmlPrintHex("DeviceVersion", pIocProfile->DeviceVersion, indent+4);
		if (detail) {
			XmlPrintHex("SubSystemVendorID", 
				pIocProfile->sub.s.SubSystemVendorID, indent+4);
			XmlPrintHex("SubSystemID", 
				pIocProfile->SubSystemID, indent+4);
			XmlPrintHex8("CapabilityMask", pIocProfile->ccm.CntlCapMask,
				indent+4);
			printf("%*s<Capability>%s%s%s%s%s%s%s%s</Capability>\n",
				indent+4, "",
				pIocProfile->ccm.ctlrCapMask.ST?"ST ":"",
				pIocProfile->ccm.ctlrCapMask.SF?"SF ":"",
				pIocProfile->ccm.ctlrCapMask.RT?"RT ":"",
				pIocProfile->ccm.ctlrCapMask.RF?"RF ":"",
				pIocProfile->ccm.ctlrCapMask.WT?"WT ":"",
				pIocProfile->ccm.ctlrCapMask.WF?"WF ":"",
				pIocProfile->ccm.ctlrCapMask.AT?"AT ":"",
				pIocProfile->ccm.ctlrCapMask.AF?"AF ":"");
			XmlPrintDec("SendMsgDepth", pIocProfile->SendMsgDepth, indent+4);
			XmlPrintDec("SendMsgSize", pIocProfile->SendMsgSize, indent+4);
			XmlPrintDec("RDMAReadDepth", pIocProfile->RDMAReadDepth, indent+4);
			XmlPrintDec("RDMASize", pIocProfile->RDMASize, indent+4);
		}
		printf("%*s<ServicesCount>%u</ServicesCount>\n", indent+4, "", pIocProfile->ServiceEntries);
		break;
	default:
		break;
	}
	if (detail) {
		unsigned i;
		if (format == FORMAT_XML) {
			printf("%*s<Services>\n", indent+4, "");
		}

		for (i=0; i < pIocProfile->ServiceEntries; i++) {
			ShowIoServiceSummary(&iocp->Services[i], format, indent+8, detail-1);
		}
		if (format == FORMAT_XML) {
			printf("%*s</Services>\n", indent+4, "");
		}
	}
	if (format == FORMAT_XML) {
		printf("%*s</Ioc>\n", indent, "");
	}
}

// output verbose summary of a IOU
void ShowIouSummary(IouData *ioup, Point *focus, Format_t format, int indent, int detail)
{
	IOUnitInfo *pIouInfo = &ioup->IouInfo;
	switch (format) {
	case FORMAT_TEXT:
		if (g_persist || g_hard)
			printf("%*sMax IOCs: %3u Change ID: xxxxx DiagDeviceId: %u Rom: %u\n",
				indent, "", pIouInfo->MaxControllers,
				pIouInfo->DiagDeviceId, pIouInfo->OptionRom);
		else
			printf("%*sMax IOCs: %3u Change ID: %5u DiagDeviceId: %u Rom: %u\n",
				indent, "", pIouInfo->MaxControllers, pIouInfo->Change_ID,
				pIouInfo->DiagDeviceId, pIouInfo->OptionRom);
		break;
	case FORMAT_XML:
		printf("%*s<Iou id=\"0x%016"PRIx64"\">\n", indent, "",
				ioup->nodep->NodeInfo.NodeGUID);
		XmlPrintDec("MaxIOCs", pIouInfo->MaxControllers, indent+4);
		if (! (g_persist || g_hard))
			XmlPrintDec("ChangeID", pIouInfo->Change_ID, indent+4);
		XmlPrintDec("DiagDeviceId", pIouInfo->DiagDeviceId, indent+4);
		XmlPrintDec("OptionRom", pIouInfo->OptionRom, indent+4);
		break;
	default:
		break;
	}
	if (detail) {
		LIST_ITEM *p;

		for (p=QListHead(&ioup->Iocs); p != NULL; p = QListNext(&ioup->Iocs, p)) {
			IocData *iocp = (IocData *)QListObj(p);
			if (! CompareIocPoint(iocp, focus))
				continue;
			ShowIocSummary(iocp, format, indent+4, detail-1);
		}
	}
	if (format == FORMAT_XML) {
		printf("%*s</Iou>\n", indent, "");
	}
}

// output verbose summary of SL-to-SC table
void ShowSLSCTable(NodeData *nodep, PortData *portp, Format_t format, int indent, int detail)
{
	STL_SLSCMAP *pSLSC = portp->pQOS->SL2SCMap;
	int i;

	if (format == FORMAT_XML) {
		printf("%*s<SLtoSCMap>\n", indent, "");
		indent+=4;
	} else {
		printf("%*sSLtoSC:\n", indent, "");
		printf("%*sSL: 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15\n",
			indent, "");
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sSC: ", indent, "");
		for (i = 0; i < STL_MAX_SLS; i++) {
			if (i == 16) {
				printf("\n%*sSL: 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31\n",
					indent, "");
				printf("%*sSC: ", indent, "");
			}
			printf("%02u ", pSLSC->SLSCMap[i].SC);
		}
		printf("\n");
		break;
	case FORMAT_XML:
		for (i=0; i<STL_MAX_SLS; i++) 
			printf("%*s<SC SL=\"%u\">%u</SC>\n", indent, "", i, pSLSC->SLSCMap[i].SC);
			break;
	default:
		break;
	}	// End of switch (format)


	switch (format) {
	case FORMAT_TEXT:
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</SLtoSCMap>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowSLSCTable

// output verbose summary of SC-to-SL table
void ShowSCSLTable(NodeData *nodep, PortData *portp, Format_t format, int indent, int detail)
{
	STL_SCSLMAP *pSCSL = portp->pQOS->SC2SLMap;
	int i;

	if (format == FORMAT_XML) {
		printf("%*s<SCtoSLMap>\n", indent, "");
		indent+=4;
	} else {
		printf("%*sSCtoSL:\n", indent, "");
		printf("%*sSC: 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15\n",
			indent, "");
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sSL: ", indent, "");
		for (i = 0; i < STL_MAX_SCS; i++) {
			if (i == 16) {
				printf("\n%*sSC: 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31\n",
					indent, "");
				printf("%*sSL: ", indent, "");
			}
			printf("%02u ", pSCSL->SCSLMap[i].SL);
		}
		printf("\n");
		break;
	case FORMAT_XML:
		for (i=0; i<STL_MAX_SCS; i++) 
			printf("%*s<SL SC=\"%u\">%u</SC>\n", indent, "", i, pSCSL->SCSLMap[i].SL);
			break;
	default:
		break;
	}	// End of switch (format)


	switch (format) {
	case FORMAT_TEXT:
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</SCtoSLMap>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowSCSLTable

// output verbose summary of SC-to-SC table
void ShowSCSCTable(NodeData *nodep, PortData *portp, int tab, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	int i;

	if (format == FORMAT_XML) {
		if (tab == 0) printf("%*s<SCtoSCMap>\n", indent, "");
		else printf("%*s<SCtoSCExtMap>\n", indent, "");
		indent+=4;
	} else {
		if (tab == 0) printf("%*sSCtoSC:\n", indent, "");
		else printf("%*sSCtoSCExt:\n", indent, "");
	}

	for (p=QListHead(&portp->pQOS->SC2SCMapList[tab]); p != NULL; p = QListNext(&portp->pQOS->SC2SCMapList[tab], p))
	{
		PortMaskSC2SCMap *pSC2SC = (PortMaskSC2SCMap *)QListObj(p);
		ASSERT(nodep->NodeInfo.NodeType == STL_NODE_SW);

		char outports[nodep->NodeInfo.NumPorts*3];
		FormatStlPortMask(outports, pSC2SC->outports, nodep->NodeInfo.NumPorts, nodep->NodeInfo.NumPorts*3);
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s\n", indent, "", outports);
			printf("%*s        SC: 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15\n",
					indent, "");
			printf("%*s       SC': ", indent, "");
			for (i = 0; i < STL_MAX_SCS; i++) {
				if (i == 16) {
					printf("\n%*s        SC: 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31\n",
						indent, "");
					printf("%*s       SC': ", indent, "");
				}
				printf("%02u ", pSC2SC->SC2SCMap->SCSCMap[i].SC);
			}
			printf("\n");
			break;
		case FORMAT_XML:
			printf("%*s<OutputPorts ports=\"%s\">\n", indent, "", &outports[7]); // don't want to print the 1st 7 chars of the string
			indent+=4;
			for (i=0; i<STL_MAX_SCS; i++)
				printf("%*s<SC SC=\"%u\">%u</SC>\n", indent, "", i, pSC2SC->SC2SCMap->SCSCMap[i].SC);
			indent-=4;
			printf("%*s</OutputPorts>\n", indent, "");
			break;
		default:
			break;
		}	// End of switch (format)

	}	// End of for ( p=QListHead(&portp->pQOS->SC2SCMapList)

	switch (format) {
	case FORMAT_TEXT:
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</SCtoSCMap>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowSCSCTable

// output verbose summary of VL arbitration table
void ShowVLArbTable(PortData *portp, Format_t format, int indent, int detail)
{
	// Adjust these to tweak the width of the print out.
	const int VLWT_PER_LINE=6;
	const int VL_PER_LINE=4;

	int t, i, j;
	STL_VLARB_TABLE *pVLArb = portp->pQOS->u.VLArbTable;

	int vlarb;
	int res = getVLArb(portp, &vlarb);

	if (res) {
		fprintf(stderr, "Error-%s: failed to determine if VLArb is in use.\n", __func__);
		return;
	}

	if (!vlarb)
		return;

	if (format == FORMAT_TEXT) {
		for (t=0; t<STL_VLARB_PREEMPT_MATRIX; t++) {
			if (t==STL_VLARB_LOW_ELEMENTS) printf("%*sVL Arbitration Low:\n", indent, "");
			else if (t==STL_VLARB_HIGH_ELEMENTS) printf("%*sVL Arbitration High:\n", indent, "");
			else printf("%*sVL Arbitration Preempt:\n", indent, "");
		
			for (i=0, j=0; i<VLARB_TABLE_LENGTH; i++) {
				if (j==0) {
					printf("%*s", indent+4, "");
					j++;
				}
				if (pVLArb[t].Elements[i].Weight) {
					printf("%02d/%03d ", 
							pVLArb[t].Elements[i].s.VL, 
							pVLArb[t].Elements[i].Weight); 
					j=j+1;
				}
				if (j==VLWT_PER_LINE) {
					j=0; 
					printf("\n");
				}
			}
			if (j!=0) printf("\n");
		}

		printf("%*sVL Preemption Matrix:\n", indent, "");
		for (i=0; i<STL_MAX_VLS; i+=VL_PER_LINE) {
			printf("%*s", indent+4, "");
			for (j=0; j<VL_PER_LINE; j++) {
				printf("%02d:%08x ", i+j, pVLArb[STL_VLARB_PREEMPT_MATRIX].Matrix[i+j]);
			}
			printf("\n");
		}
	} else if (format == FORMAT_XML) {
		for (t=0; t < STL_VLARB_PREEMPT_MATRIX; t++) {
			if (t==0) printf("%*s<VLArbitrationLow>\n", indent, "");
			else if (t==1) printf("%*s<VLArbitrationHigh>\n", indent, "");
			else printf("%*s<VLArbitrationPreempt>\n", indent, "");
		
			for (i=0; i<VLARB_TABLE_LENGTH; i++) {
				if (pVLArb[t].Elements[i].Weight) {
					printf("%*s<Weight VL=\"%u\">%u</Weight>\n", indent+4, "",
						pVLArb[t].Elements[i].s.VL, 
						pVLArb[t].Elements[i].Weight); 
				}
			}
			if (t==0) printf("%*s</VLArbitrationLow>\n", indent, "");
			else if (t==1) printf("%*s</VLArbitrationHigh>\n", indent, "");
			else printf("%*s</VLArbitrationPreempt>\n", indent, "");
		}

		printf("%*s<VLPreemptionMatrix>\n", indent, "");
		for (i=0; i<STL_MAX_VLS; i++) {
			printf("%*s<Element VL=\"%u\">%u</Element>\n", indent+4, "",
					i, pVLArb[STL_VLARB_PREEMPT_MATRIX].Matrix[i]);
		}
		printf("%*s</VLPreemptionMatrix>\n", indent, "");
	}

}	// End of ShowVLArbTable

// output verbose summary of P-Key table
void ShowPKeyTable(NodeData *nodep, PortData *portp, Format_t format, int indent, int detail)
{
#define PKEY_PER_LINE 6

	int ix, ix_line, last=0;
	STL_PKEY_ELEMENT *pPKey = portp->pPartitionTable;
	int ix_capacity = PortPartitionTableSize(portp);

	switch (format) {
	case FORMAT_XML:
		printf("%*s<PKeyTable>\n", indent, "");
		indent+=4;
		break;
	case FORMAT_TEXT:
	default:
		break;
	}

	if (pPKey && ix_capacity)
	{
		for (ix = 0; ix < ix_capacity; ix++ )
		{
			if (pPKey[ix].AsReg16 & 0x7FFF)
				last = ix;
		}
		for (ix = 0; ix <= last; )
		{
			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%12s", indent, "", !ix ? "P_Key Table:" : "");
				for ( ix_line = PKEY_PER_LINE;
						ix_line > 0 && ix <= last; ix++) {
					printf("0x%04X ", pPKey[ix].AsReg16);
					ix_line--;
				}
				printf("\n");
				break;
			case FORMAT_XML:
				XmlPrintHex16("PKey", pPKey[ix].AsReg16, indent);
				ix++;
				break;
			default:
				break;
			}	// End of switch (format)
		}
	}
	switch (format) {
	case FORMAT_XML:
		indent-=4;
		printf("%*s</PKeyTable>\n", indent, "");
		break;
	case FORMAT_TEXT:
	default:
		break;
	}

}	// End of ShowPKeyTable

// output verbose summary of PortCounters
void ShowPortCounters(STL_PortStatusData_t *pPortStatus, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sPerformance: Transmit\n",
			indent, "");
		printf("%*s    Xmit Data   %20"PRIu64" MB (%"PRIu64" Flits)\n",
			indent, "",
			pPortStatus->PortXmitData/FLITS_PER_MB,
			pPortStatus->PortXmitData);
		printf("%*s    Xmit Pkts   %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortXmitPkts);
		printf("%*s    MC Xmt Pkts %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortMulticastXmitPkts);
		printf("%*sPerformance: Receive\n",
			indent, "");
		printf("%*s    Rcv Data    %20"PRIu64" MB (%"PRIu64" Flits)\n",
			indent, "",
			pPortStatus->PortRcvData/FLITS_PER_MB,
			pPortStatus->PortRcvData);
		printf("%*s    Rcv Pkts    %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortRcvPkts);
		printf("%*s    MC Rcv Pkts %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortMulticastRcvPkts);
		printf("%*sPerformance: Congestion\n",
			indent, "");
		printf("%*s    Congestion Discards   %20"PRIu64"\n",
			indent, "",
			pPortStatus->SwPortCongestion);
		printf("%*s    Rcv FECN              %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortRcvFECN);
		printf("%*s    Rcv BECN              %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortRcvBECN);
		printf("%*s    Mark FECN             %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortMarkFECN);
		printf("%*s    Xmit Time Congestion  %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortXmitTimeCong);
		printf("%*s    Xmit Wait             %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortXmitWait);
		printf("%*sPerformance: Bubbles\n",
			indent, "");
		printf("%*s    Xmit Wasted BW        %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortXmitWastedBW);
		printf("%*s    Xmit Wait Data        %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortXmitWaitData);
		printf("%*s    Rcv Bubble            %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortRcvBubble);

		printf("%*sErrors: Signal Integrity\n",
			indent, "");
		printf("%*s    Link Qual Indicator   %20u (%s)\n",
			indent, "",
			pPortStatus->lq.s.LinkQualityIndicator,
			StlLinkQualToText(pPortStatus->lq.s.LinkQualityIndicator));
		printf("%*s    Uncorrectable Errors  %20u\n",	//8 bit
			indent, "",
			pPortStatus->UncorrectableErrors);
		printf("%*s    Link Downed           %20u\n",	// 32 bit
			indent, "",
			pPortStatus->LinkDowned);
		printf("%*s    Rcv Errors            %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortRcvErrors);
		printf("%*s    Exc. Buffer Overrun   %20"PRIu64"\n",
			indent, "",
			pPortStatus->ExcessiveBufferOverruns);
		printf("%*s    FM Config Errors      %20"PRIu64"\n",
			indent, "",
			pPortStatus->FMConfigErrors);
		printf("%*s    Link Error Recovery   %20u\n",	// 32 bit
			indent, "",
			pPortStatus->LinkErrorRecovery);
		printf("%*s    Local Link Integ Err  %20"PRIu64"\n",
			indent, "",
			pPortStatus->LocalLinkIntegrityErrors);
		printf("%*s    Rcv Rmt Phys Err      %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortRcvRemotePhysicalErrors);
		printf("%*sErrors: Security\n",
			indent, "");
		printf("%*s    Xmit Constraint       %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortXmitConstraintErrors);
		printf("%*s    Rcv Constraint        %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortRcvConstraintErrors);
		printf("%*sErrors: Routing and Other\n",
			indent, "");
		printf("%*s    Rcv Sw Relay Err      %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortRcvSwitchRelayErrors);
		printf("%*s    Xmit Discards         %20"PRIu64"\n",
			indent, "",
			pPortStatus->PortXmitDiscards);

		break;
	case FORMAT_XML:
		printf("%*s<Performance>\n", indent, "");
		// Data movement
		XmlPrintDec64("XmitDataMB", pPortStatus->PortXmitData/FLITS_PER_MB, indent+4);
		printf("%*s<XmitData>%"PRIu64"</XmitData> <!-- in Flits -->\n",
			indent+4, "", pPortStatus->PortXmitData);
		XmlPrintDec64("RcvDataMB", pPortStatus->PortRcvData/FLITS_PER_MB, indent+4);
		XmlPrintDec64("XmitPkts", pPortStatus->PortXmitPkts, indent+4);
		printf("%*s<RcvData>%"PRIu64"</RcvData> <!-- in Flits -->\n",
			indent+4, "", pPortStatus->PortRcvData);
		XmlPrintDec64("RcvPkts", pPortStatus->PortRcvPkts, indent+4);
		XmlPrintDec64("MulticastXmitPkts", pPortStatus->PortMulticastXmitPkts, indent+4);
		XmlPrintDec64("MulticastRcvPkts", pPortStatus->PortMulticastRcvPkts, indent+4);
		// Signal Integrity and Node/Link Stability
		XmlPrintDec("LinkQualityIndicator", pPortStatus->lq.s.LinkQualityIndicator, indent+4);
		XmlPrintDec("UncorrectableErrors", pPortStatus->UncorrectableErrors, indent+4);	// 8 bit
		XmlPrintDec("LinkDowned", pPortStatus->LinkDowned, indent+4);	// 32 bit
		XmlPrintDec64("RcvErrors", pPortStatus->PortRcvErrors, indent+4);
		XmlPrintDec64("ExcessiveBufferOverruns", pPortStatus->ExcessiveBufferOverruns, indent+4);
		XmlPrintDec64("FMConfigErrors", pPortStatus->FMConfigErrors, indent+4);
		XmlPrintDec("LinkErrorRecovery", pPortStatus->LinkErrorRecovery, indent+4);	// 32 bit
		XmlPrintDec64("LocalLinkIntegrityErrors", pPortStatus->LocalLinkIntegrityErrors, indent+4);
		XmlPrintDec64("RcvRemotePhysicalErrors", pPortStatus->PortRcvRemotePhysicalErrors, indent+4);
		// Security
		XmlPrintDec64("XmitConstraintErrors", pPortStatus->PortXmitConstraintErrors, indent+4);
		XmlPrintDec64("RcvConstraintErrors", pPortStatus->PortRcvConstraintErrors, indent+4);
		// Routing or Down nodes still being sent to
		XmlPrintDec64("RcvSwitchRelayErrors", pPortStatus->PortRcvSwitchRelayErrors, indent+4);
		XmlPrintDec64("XmitDiscards", pPortStatus->PortXmitDiscards, indent+4);
		// Congestion
		XmlPrintDec64("CongDiscards", pPortStatus->SwPortCongestion, indent+4);
		XmlPrintDec64("RcvFECN", pPortStatus->PortRcvFECN, indent+4);
		XmlPrintDec64("RcvBECN", pPortStatus->PortRcvBECN, indent+4);
		XmlPrintDec64("MarkFECN", pPortStatus->PortMarkFECN, indent+4);
		XmlPrintDec64("XmitTimeCong", pPortStatus->PortXmitTimeCong, indent+4);
		XmlPrintDec64("XmitWait", pPortStatus->PortXmitWait, indent+4);
		// Bubbles
		XmlPrintDec64("XmitWastedBW", pPortStatus->PortXmitWastedBW, indent+4);
		XmlPrintDec64("XmitWaitData", pPortStatus->PortXmitWaitData, indent+4);
		XmlPrintDec64("RcvBubble", pPortStatus->PortRcvBubble, indent+4);
		printf("%*s</Performance>\n", indent, "");
		break;
	default:
		break;
	}
}

// output verbose summary of an STL Port
void ShowPortSummary(PortData *portp, Format_t format, int indent, int detail)
{
	STL_PORT_INFO *pPortInfo = &portp->PortInfo;
	char buf1[SHOW_BUF_SIZE], buf2[SHOW_BUF_SIZE], buf3[SHOW_BUF_SIZE], buf4[SHOW_BUF_SIZE];

	switch (format) {
	case FORMAT_TEXT:
		if (portp->PortGUID)
			if (g_persist || g_hard)
				printf("%*sPortNum: %3u LID: xxxxxxxxxx GUID: 0x%016"PRIx64"\n",
					indent, "", portp->PortNum, portp->PortGUID);
			else
				printf("%*sPortNum: %3u LID: 0x%.*x GUID: 0x%016"PRIx64"\n",
					indent, "", portp->PortNum,
					(portp->EndPortLID <= IB_MAX_UCAST_LID ? 4:8),
					portp->EndPortLID, portp->PortGUID);
		else
			printf("%*sPortNum: %3u\n",
				indent, "", portp->PortNum);
		{
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				printf("%*sPortDetails: %s\n", indent+4, "", portselp->details);
			}
		}
		if (portp->neighbor) {
			ShowLinkPortSummary(portp->neighbor, "Neighbor: ", format, indent+4, detail);
			if (detail-1)
				ShowExpectedLinkSummary(portp->neighbor->elinkp, format, indent+8, detail-1);
		}
		if (detail) {
			if (g_hard) {
				printf( "%*sLocalPort: %-3d PortState: xxxxxx           PhysState: xxxxxxxx\n",
					indent+4, "", pPortInfo->LocalPortNum);
				printf( "%*sNeighborNormal: x\n", indent+4, "");
			}
			else {
				uint8 ldr = 0;
				if (pPortInfo->LinkDownReason)
					ldr = pPortInfo->LinkDownReason;
				else if (pPortInfo->NeighborLinkDownReason)
					ldr = pPortInfo->NeighborLinkDownReason;

				// SM will have cleared ldr for ports in Armed and Active
				if (pPortInfo->PortStates.s.PortState == IB_PORT_INIT
					&& pPortInfo->s3.LinkInitReason) {
					printf( "%*sLocalPort: %-3d PortState: %-6s (%-13s) PhysState: %-8s\n",
							indent+4, "",
							pPortInfo->LocalPortNum,
							IbPortStateToText(pPortInfo->PortStates.s.PortState),
							StlLinkInitReasonToText(pPortInfo->s3.LinkInitReason),
							StlPortPhysStateToText(pPortInfo->PortStates.s.PortPhysicalState));
				} else if (pPortInfo->PortStates.s.PortState == IB_PORT_DOWN && ldr && ! g_persist) {
					printf( "%*sLocalPort: %-3d PortState: %-6s (%-13s) PhysState: %-8s\n",
							indent+4, "",
							pPortInfo->LocalPortNum,
							IbPortStateToText(pPortInfo->PortStates.s.PortState),
							StlLinkDownReasonToText(ldr),
							StlPortPhysStateToText(pPortInfo->PortStates.s.PortPhysicalState));
				} else {
					printf( "%*sLocalPort: %-3d PortState: %-6s                 PhysState: %-8s\n",
						indent+4, "",
						pPortInfo->LocalPortNum,
						IbPortStateToText(pPortInfo->PortStates.s.PortState),
						StlPortPhysStateToText(pPortInfo->PortStates.s.PortPhysicalState));
				}
				if (pPortInfo->PortStates.s.PortPhysicalState == STL_PORT_PHYS_OFFLINE
					|| pPortInfo->PortStates.s.PortPhysicalState == IB_PORT_PHYS_DISABLED)
					
    				printf("%*sOfflineDisabledReason: %-14s\n",
    					indent+4, "",
    					StlPortOfflineDisabledReasonToText(pPortInfo->PortStates.s.OfflineDisabledReason));
				printf( "%*sIsSMConfigurationStarted: %-5s NeighborNormal: %-5s\n",
						indent+4, "",
						pPortInfo->PortStates.s.IsSMConfigurationStarted?"True":"False",
						pPortInfo->PortStates.s.NeighborNormal?"True":"False");
			}
			printf("%*sPortType: %-3s\n", indent+4, "", StlPortTypeToText(pPortInfo->PortPhysConfig.s.PortType));
			printf("%*sBundleNextPort: %-3u         BundleLane: %-3u\n", indent+4, "",
				pPortInfo->BundleNextPort,
				pPortInfo->BundleLane);
			if (g_hard) {
				printf( "%*sLID:    xxxxxxxxxx              LMC: x               Subnet: xxxxxxxxxxxxxxxxxx\n",
					indent+4, "");
				printf( "%*sSMLID:  xxxxxxxxxx   SMSL: xx   RespTimeout: xxxxxxx",
					indent+4, "");
				printf( "  SubnetTimeout: xxxxxxx\n");
			} else if (g_persist) {
				printf( "%*sLID:    xxxxxxxxxx              LMC: %u               Subnet: 0x%016"PRIx64"\n",
					indent+4, "",
					pPortInfo->s1.LMC,
					pPortInfo->SubnetPrefix);
				FormatTimeoutMult(buf1, pPortInfo->Resp.TimeValue);
				printf( "%*sSMLID:  xxxxxxxxxx   SMSL: %2u   RespTimeout: %s",
					indent+4, "",
					pPortInfo->s2.MasterSMSL, buf1);
				FormatTimeoutMult(buf1, pPortInfo->Subnet.Timeout);
				printf( "  SubnetTimeout: %s\n", buf1);
			} else {
				if (pPortInfo->LID == STL_LID_PERMISSIVE)
					printf( "%*sLID:    DontCare            LMC: %u               Subnet: 0x%016"PRIx64"\n",
						indent+4, "",
						pPortInfo->s1.LMC,
						pPortInfo->SubnetPrefix);
				else
					printf( "%*sLID:    0x%.*x              LMC: %u               Subnet: 0x%016"PRIx64"\n",
						indent+4, "",
						(pPortInfo->LID <= IB_MAX_UCAST_LID ? 4:8),
						pPortInfo->LID, pPortInfo->s1.LMC,
						pPortInfo->SubnetPrefix);
				FormatTimeoutMult(buf1, pPortInfo->Resp.TimeValue);
				printf( "%*sSMLID:  0x%.*x   SMSL: %2u   RespTimeout: %s",
					indent+4, "",(pPortInfo->MasterSMLID <= IB_MAX_UCAST_LID ? 4:8),
					pPortInfo->MasterSMLID, pPortInfo->s2.MasterSMSL, buf1);
				FormatTimeoutMult(buf1, pPortInfo->Subnet.Timeout);
				printf( "  SubnetTimeout: %s\n", buf1);
			}
			if (g_hard)
				printf( "%*sM_KEY:  xxxxxxxxxxxxxxxxxx  Lease: xxxxxxx          Protect: xxxxxxxx\n",
					indent+4, "");
			else
				printf( "%*sM_KEY:  0x%016"PRIx64"  Lease: %5u s          Protect: %s\n",
					indent+4, "",
					pPortInfo->M_Key, pPortInfo->M_KeyLeasePeriod,
					IbMKeyProtectToText(pPortInfo->s1.M_KeyProtectBits));
			printf("%*sMTU  Supported: (0x%x) %s bytes\n",
					indent+4, "", 
					pPortInfo->MTU.Cap, 
					IbMTUToText(pPortInfo->MTU.Cap));
			if (! g_hard)
			{
				FormatStlVLStalls(buf1, pPortInfo, sizeof(buf1));
				printf("%*sVLStallCount (per VL): %s\n", indent+4, "", buf1);
			
				printf("%*sMTU  Active by VL:\n", indent+4, "");
				printf("%*s00:%5s 01:%5s 02:%5s 03:%5s 04:%5s 05:%5s 06:%5s 07:%5s\n",
					indent+4, "",
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 0)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 1)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 2)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 3)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 4)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 5)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 6)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 7)));
				printf("%*s08:%5s 09:%5s 10:%5s 11:%5s 12:%5s 13:%5s 14:%5s 15:%5s\n",
					indent+4, "",
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 8)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 9)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 10)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 11)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 12)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 13)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 14)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 15)));
				printf("%*s16:%5s 17:%5s 18:%5s 19:%5s 20:%5s 21:%5s 22:%5s 23:%5s\n",
					indent+4, "",
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 16)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 17)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 18)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 19)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 20)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 21)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 22)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 23)));
				printf("%*s24:%5s 25:%5s 26:%5s 27:%5s 28:%5s 29:%5s 30:%5s 31:%5s\n",
					indent+4, "",
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 24)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 25)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 26)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 27)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 28)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 29)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 30)),
					IbMTUToText(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, 31)));
			}
			if (g_hard)
				printf( "%*sLinkWidth: Active:    xxxx  Supported:    %7s  Enabled:    xxxxxxx\n",
					indent+4, "",
					StlLinkWidthToText(pPortInfo->LinkWidth.Supported, buf1, sizeof(buf1)));
			else
				printf( "%*sLinkWidth: Active:    %4s  Supported:    %7s  Enabled:    %7s\n",
					indent+4, "",
					StlLinkWidthToText(pPortInfo->LinkWidth.Active, buf1, sizeof(buf1)),
					StlLinkWidthToText(pPortInfo->LinkWidth.Supported, buf2, sizeof(buf2)),
					StlLinkWidthToText(pPortInfo->LinkWidth.Enabled, buf3, sizeof(buf3)));
			if (g_hard)
				printf( "%*sLinkWidthDnGrade: Active: Tx: xx Rx: xx Supported:    %7s  Enabled:    xxxxxxx\n",
					indent+4, "",
					StlLinkWidthToText(pPortInfo->LinkWidthDowngrade.Supported, buf1, sizeof(buf1)));
			else
				printf( "%*sLinkWidthDnGrade: ActiveTx: %2.2s Rx: %2.2s Supported:    %7s  Enabled:    %7s\n",
					indent+4, "",
					StlLinkWidthToText(pPortInfo->LinkWidthDowngrade.TxActive, buf1, sizeof(buf1)),
					StlLinkWidthToText(pPortInfo->LinkWidthDowngrade.RxActive, buf2, sizeof(buf2)),
					StlLinkWidthToText(pPortInfo->LinkWidthDowngrade.Supported, buf3, sizeof(buf3)),
					StlLinkWidthToText(pPortInfo->LinkWidthDowngrade.Enabled, buf4, sizeof(buf4)));
			if (g_hard)
				printf("%*sPortLinkMode: Active: xxxx Supported: %-16s Enabled: xxxx\n",
					indent+4, "",
					StlPortLinkModeToText(pPortInfo->PortLinkMode.s.Supported, buf1, sizeof(buf1)));
			else
				printf("%*sPortLinkMode: Active: %-8s Supported: %-16s Enabled: %-8s\n",
					indent+4, "",
					StlPortLinkModeToText(pPortInfo->PortLinkMode.s.Active, buf1, sizeof(buf1)),
					StlPortLinkModeToText(pPortInfo->PortLinkMode.s.Supported, buf2, sizeof(buf2)),
					StlPortLinkModeToText(pPortInfo->PortLinkMode.s.Enabled, buf3, sizeof(buf3)));
			if (g_hard)
				printf( "%*sLinkSpeed: Active: xxxxxxx  Supported: %10s  Enabled: xxxxxxxxxx\n",
					indent+4, "",
					StlLinkSpeedToText(pPortInfo->LinkSpeed.Supported, buf1, sizeof(buf1)));
			else
				printf( "%*sLinkSpeed: Active: %7s  Supported: %10s  Enabled: %10s\n",
					indent+4, "",
					StlLinkSpeedToText(pPortInfo->LinkSpeed.Active, buf1, sizeof(buf1)),
					StlLinkSpeedToText(pPortInfo->LinkSpeed.Supported, buf2, sizeof(buf2)),
					StlLinkSpeedToText(pPortInfo->LinkSpeed.Enabled, buf3, sizeof(buf3)));

			printf("%*sSM_TrapQP: 0x%x  SA_QP: 0x%x IPAddr Prim/Sec: %s / %s\n",
				indent+4, "",
				pPortInfo->SM_TrapQP.s.QueuePair, pPortInfo->SA_QP.s.QueuePair,
				inet_ntop(AF_INET6, pPortInfo->IPAddrIPV6.addr, buf1, sizeof(buf1)),
				inet_ntop(AF_INET, pPortInfo->IPAddrIPV4.addr, buf2, sizeof(buf2)));

			if (g_hard) {
				printf( "%*sVLs:       Active:    xxxx  Supported:       %2u+1  HOQLife: xxxxxxxxx\n",
					indent+4, "",
					pPortInfo->VL.s2.Cap);
			} else {
				printf( "%*sVLs:       Active:    %2u+1  Supported:       %2u+1  \n",
					indent+4, "",
					pPortInfo->s4.OperationalVL,
					pPortInfo->VL.s2.Cap);
#if 0
				if (pPortInfo->XmitQ.HOQLife > IB_LIFETIME_MAX)
				{
					strncpy(buf1, "Infinite", 9);
				} else {
					FormatTimeoutMult(buf1, pPortInfo->XmitQ.HOQLife);
				}
				printf( "%s\n", buf1);
#else
				int i;
				printf("%*sHOQLife (Per VL):", indent+8, "");
				for (i = 0; i < STL_MAX_VLS; i++) {
					if (!(i % 5)) printf("\n%*s", indent+8, "");
					printf("VL%2d: 0x%x ", i, (uint32)pPortInfo->XmitQ[i].HOQLife);
				}
				printf("\n\n");
#endif
			}

			if (pPortInfo->CapabilityMask3.s.VLSchedulingConfig == VL_SCHED_MODE_VLARB) {
				if (g_hard) {
					printf( "%*sVL Arb Cap:  High:    %4u        Low:       %4u  HiLimit: xxxx PreemptLimit xxxx\n",
						indent+4, "", pPortInfo->VL.ArbitrationHighCap,
						pPortInfo->VL.ArbitrationLowCap);

				} else {
					printf( "%*sVL Arb Cap:  High:    %4u        Low:       %4u  HiLimit: %4u PreemptLimit %4u\n",
						indent+4, "", pPortInfo->VL.ArbitrationHighCap,
						pPortInfo->VL.ArbitrationLowCap, pPortInfo->VL.HighLimit,
						pPortInfo->VL.PreemptingLimit);
				}
			}

			printf("%*sVLFlowControlDisabledMask: 0x%08x\n", indent+4, "", pPortInfo->FlowControlMask);
			printf("%*sNeighborMode  MgmtAllowed: %3s  FwAuthenBypass: %3s  NeighborNodeType: %s\n",
				indent+4, "",
				pPortInfo->PortNeighborMode.MgmtAllowed?"Yes":"No",
				pPortInfo->PortNeighborMode.NeighborFWAuthenBypass?"On":"Off",
				OpaNeighborNodeTypeToText(pPortInfo->PortNeighborMode.NeighborNodeType));
			FormatStlCapabilityMask(buf1, pPortInfo->CapabilityMask);
			printf( "%*sCapability 0x%08x: %s\n",
				indent+4, "",
				pPortInfo->CapabilityMask.AsReg32, buf1);
			FormatStlCapabilityMask3(buf1, pPortInfo->CapabilityMask3, sizeof(buf1));
			printf("%*sCapability3 0x%04x: %s\n",
				indent+4, "",
				pPortInfo->CapabilityMask3.AsReg16, buf1);

			if (g_persist || g_hard)
				printf( "%*sViolations: M_Key: xxxxx P_Key: xxxxx Q_Key: xxxxx\n",
					indent+4, "");
			else
				printf( "%*sViolations: M_Key: %5u P_Key: %5u Q_Key: %5u\n",
					indent+4, "",
					pPortInfo->Violations.M_Key, pPortInfo->Violations.P_Key,
					pPortInfo->Violations.Q_Key);
			if (g_hard)
				printf("%*sPortMode ActiveOptimize xxxxx PassThrough: xxxxx VLMarker: xxxxx\n",
					indent+4, "");
			else
				printf("%*sPortMode ActiveOptimize: %3s PassThrough: %3s VLMarker: %3s\n",
					indent+4, "",
                    pPortInfo->PortMode.s.IsActiveOptimizeEnabled?"On":"Off",
					pPortInfo->PortMode.s.IsPassThroughEnabled?"On":"Off",
					pPortInfo->PortMode.s.IsVLMarkerEnabled?"On":"Off");
			if (g_hard) {
				printf("%*sFlitCtrlInterleave Distance Max: %2u Enabled: xxxxx\n",
					indent, "", pPortInfo->FlitControl.Interleave.s.DistanceSupported);
				printf("%*sMaxNestLevelTxEnabled: xxxx MaxNestLevelRxSupported: %u\n",
					indent+4, "",
					pPortInfo->FlitControl.Interleave.s.MaxNestLevelRxSupported);
			} else {
				printf("%*sFlitCtrlInterleave Distance Max: %2u Enabled: %2u\n",
					indent+4, "", pPortInfo->FlitControl.Interleave.s.DistanceSupported,
					pPortInfo->FlitControl.Interleave.s.DistanceEnabled);
				printf("%*sMaxNestLevelTxEnabled: %u MaxNestLevelRxSupported: %u\n",
					indent+4, "",
					pPortInfo->FlitControl.Interleave.s.MaxNestLevelTxEnabled,
					pPortInfo->FlitControl.Interleave.s.MaxNestLevelRxSupported);
			}
			if (g_hard) {
				printf("%*sSmallPktLimit: xxxx MaxSmallPktLimit: 0x%02x PreemptionLimit: xxxx\n",
					indent+4, "",
					pPortInfo->FlitControl.Preemption.MaxSmallPktLimit);
			} else {
				printf("%*sSmallPktLimit: 0x%02x MaxSmallPktLimit: 0x%02x PreemptionLimit: 0x%02x\n",
					indent+4, "",
					pPortInfo->FlitControl.Preemption.SmallPktLimit,
					pPortInfo->FlitControl.Preemption.MaxSmallPktLimit,
					pPortInfo->FlitControl.Preemption.PreemptionLimit);
				// Convert Flits to bytes and display in decimal
				printf("%*sFlitCtrlPreemption MinInital: %"PRIu64" MinTail: %"PRIu64" LargePktLim: 0x%02x\n",
					indent+4, "",
					(uint64_t)(pPortInfo->FlitControl.Preemption.MinInitial * BYTES_PER_FLIT),
					(uint64_t)(pPortInfo->FlitControl.Preemption.MinTail * BYTES_PER_FLIT),
					pPortInfo->FlitControl.Preemption.LargePktLimit);
			}
			if (!g_hard) {
				if (pPortInfo->CapabilityMask3.s.IsMAXLIDSupported)
					printf("%*sMaxLID: %u\n",
						indent+4, "", pPortInfo->MaxLID);
			}
			if (g_hard)
				printf("%*sBufferUnits: VL15Init 0x%04x; VL15CreditRate xxxxxx; CreditAck 0x%x; BufferAlloc 0x%x\n",
					indent+4, "",
					pPortInfo->BufferUnits.s.VL15Init,
					pPortInfo->BufferUnits.s.CreditAck,
					pPortInfo->BufferUnits.s.BufferAlloc);
			else
				printf("%*sBufferUnits: VL15Init 0x%04x; VL15CreditRate 0x%02x; CreditAck 0x%x; BufferAlloc 0x%x\n",
					indent+4, "",
					pPortInfo->BufferUnits.s.VL15Init,
					pPortInfo->BufferUnits.s.VL15CreditRate,
					pPortInfo->BufferUnits.s.CreditAck,
					pPortInfo->BufferUnits.s.BufferAlloc);

			if (! g_hard)
			{
				FormatStlPortErrorAction(buf1, pPortInfo, SHOW_BUF_SIZE);
				printf("%*sPortErrorActions: 0x%x: %s\n", indent+4, "", pPortInfo->PortErrorAction.AsReg32, buf1);
			}

			if (!g_persist && !g_hard)
				printf("%*sReplayDepth Buffer 0x%02x; Wire 0x%02x\n", indent+4, "",
					pPortInfo->ReplayDepth.BufferDepth, pPortInfo->ReplayDepth.WireDepth);
			else
				printf("%*sReplayDepth Buffer 0x%02x; Wire xxxx\n", indent+4, "",
					pPortInfo->ReplayDepth.BufferDepth);
			if (g_hard)
				printf( "%*sDiagCode: xxxxxx\n", indent+4, "");
			else
				printf( "%*sDiagCode: 0x%04x\n",
					indent+4, "",
					pPortInfo->DiagCode.AsReg16);
			printf("%*sOverallBufferSpace: 0x%04x\n", indent+4, "", 
				pPortInfo->OverallBufferSpace);
			if (g_hard)
				printf( "%*sP_Key Enforcement: In: xxx Out: xxx\n",
					indent+4, "");
			else
				printf( "%*sP_Key Enforcement: In: %3s Out: %3s\n",
					indent+4, "",
					pPortInfo->s3.PartitionEnforcementInbound?"On":"Off",
					pPortInfo->s3.PartitionEnforcementOutbound?"On":"Off");
			if ( portp->nodep && portp->pQOS &&
					(detail > 1) && !g_persist && !g_hard ) {
				if ( portp->pQOS->SL2SCMap ) {
					ShowSLSCTable(portp->nodep, portp, format, indent+4, detail-2);
				}
				if ( portp->pQOS->SC2SLMap ) {
					ShowSCSLTable(portp->nodep, portp, format, indent+4, detail-2);
				}
				int i = 0;
				for(i=0; i<SC2SCMAPLIST_MAX; i++) {
					if ( !(QListIsEmpty(&portp->pQOS->SC2SCMapList[i])) )  {
						ShowSCSCTable(portp->nodep, portp, i, format, indent+4, detail-2);
					}
				}
			}
			if (portp->pQOS && (detail > 1) && !g_persist && !g_hard) {
				ShowVLArbTable(portp, format, indent+4, detail-2);
			}
			if ( portp->nodep && portp->pPartitionTable && (detail > 1) &&
					!g_persist && !g_hard ) {
				ShowPKeyTable(portp->nodep, portp, format, indent+4, detail-2);
			}
			if (portp->pPortStatus && detail > 1 && ! g_persist && ! g_hard) {
				ShowPortCounters(portp->pPortStatus, format, indent+4, detail-2);
			}
		} else {
			if (g_hard)
				printf("%*sWidth: xxxx Speed: xxxxxxx Downgraded? xxx\n",
					indent+4, "");
			else
				printf("%*sWidth: %4s Speed: %7s Downgraded? %s\n",
					indent+4, "",
					StlLinkWidthToText(pPortInfo->LinkWidth.Active, buf1, sizeof(buf1)),
					StlLinkSpeedToText(pPortInfo->LinkSpeed.Active, buf2, sizeof(buf2)),
					((pPortInfo->LinkWidth.Active == pPortInfo->LinkWidthDowngrade.TxActive) &&
					 (pPortInfo->LinkWidth.Active == pPortInfo->LinkWidthDowngrade.RxActive))? 
						" No": "Yes");
		}

		if (detail && portp->pCableInfoData)
			ShowCableSummary(portp->pCableInfoData, FORMAT_TEXT, indent+4, detail, pPortInfo->PortPhysConfig.s.PortType);
		break;
	case FORMAT_XML:
		printf("%*s<PortInfo id=\"0x%016"PRIx64":%u\">\n", indent, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintDec("PortNum", portp->PortNum, indent+4);
		if (portp->PortGUID) {
			if (! (g_persist || g_hard))
				XmlPrintLID("LID", portp->EndPortLID,
							indent+4);
			XmlPrintHex64("GUID", portp->PortGUID, indent+4);
		}
		{
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				XmlPrintOptionalStr("PortDetails", portselp->details, indent+4);
			}
		}
		if (portp->neighbor) {
			printf("%*s<Neighbor>\n", indent+4, "");
			ShowLinkPortSummary(portp->neighbor, "Neighbor: ", format, indent+8, detail);
			if (detail-1)
				ShowExpectedLinkSummary(portp->neighbor->elinkp, format, indent+12, detail-1);
			printf("%*s</Neighbor>\n", indent+4, "");
		}
		if (detail) {
			if (g_hard) {
				// noop
				XmlPrintDec("LocalPort", pPortInfo->LocalPortNum, indent+4);
				XmlPrintStr("PortType", StlPortTypeToText(pPortInfo->PortPhysConfig.s.PortType), indent+4);
				XmlPrintHex("PortType_Int", pPortInfo->PortPhysConfig.s.PortType, indent+4);
			} else {
				XmlPrintDec("LocalPort", pPortInfo->LocalPortNum, indent+4);
				XmlPrintStr("PortState",
					IbPortStateToText(pPortInfo->PortStates.s.PortState), indent+4);
				XmlPrintDec("PortState_Int",
					pPortInfo->PortStates.s.PortState, indent+4);
				if (pPortInfo->PortStates.s.PortState == IB_PORT_INIT) {
					XmlPrintStr("InitReason",
						StlLinkInitReasonToText(pPortInfo->s3.LinkInitReason), indent+4);
					XmlPrintDec("InitReason_Int",
						pPortInfo->s3.LinkInitReason, indent+4);
				}
				XmlPrintStr("PortType", StlPortTypeToText(pPortInfo->PortPhysConfig.s.PortType), indent+4);
				XmlPrintHex("PortType_Int", pPortInfo->PortPhysConfig.s.PortType, indent+4);

				XmlPrintStr("PhysState",
					StlPortPhysStateToText(pPortInfo->PortStates.s.PortPhysicalState),
					indent+4);
				XmlPrintDec("PhysState_Int",
					pPortInfo->PortStates.s.PortPhysicalState, indent+4);
				XmlPrintStr("IsSMConfigurationStarted", pPortInfo->PortStates.s.IsSMConfigurationStarted?"True":"False", indent+4);
				XmlPrintStr("NeighborNormal", pPortInfo->PortStates.s.NeighborNormal?"True":"False", indent+4);
				if (pPortInfo->PortStates.s.PortPhysicalState == STL_PORT_PHYS_OFFLINE
					|| pPortInfo->PortStates.s.PortPhysicalState == IB_PORT_PHYS_DISABLED) {
				XmlPrintStr("OfflineDisabledReason",
    				StlPortOfflineDisabledReasonToText(pPortInfo->PortStates.s.OfflineDisabledReason),
					indent+4);
				XmlPrintDec("OfflineDisabledReason_Int",
    				pPortInfo->PortStates.s.OfflineDisabledReason, indent+4);
				}
			}
			if (g_hard) {
				// noop
			} else {
				if (! g_persist)
					XmlPrintLID("LID", pPortInfo->LID, indent+4);
				XmlPrintDec("LMC", pPortInfo->s1.LMC, indent+4);
				XmlPrintHex64("SubnetPrefix", pPortInfo->SubnetPrefix, indent+4);
				if (! g_persist)
					XmlPrintLID("SMLID", pPortInfo->MasterSMLID, indent+4);
				XmlPrintDec("SMSL", pPortInfo->s2.MasterSMSL, indent+4);
				FormatTimeoutMult(buf1, pPortInfo->Resp.TimeValue);
				XmlPrintStr("RespTimeout", buf1, indent+4);
				XmlPrintDec("RespTimeout_Int", pPortInfo->Resp.TimeValue,
								indent+4);
				FormatTimeoutMult(buf1, pPortInfo->Subnet.Timeout);
				XmlPrintStr("SubnetTimeout", buf1, indent+4);
				XmlPrintDec("SubnetTimeout_Int", pPortInfo->Subnet.Timeout,
								indent+4);
			}
			if (g_hard) {
				// noop
			} else {
				XmlPrintHex64("M_KEY", pPortInfo->M_Key, indent+4);
				printf("%*s<Lease>%u</Lease> <!-- seconds -->\n",
					indent+4, "",
					pPortInfo->M_KeyLeasePeriod);
				XmlPrintStr("Protect",
						IbMKeyProtectToText(pPortInfo->s1.M_KeyProtectBits),
						indent+4);
				XmlPrintDec("Protect_Int",
						pPortInfo->s1.M_KeyProtectBits, indent+4);
			}

			XmlPrintDec("MTUSupported",
					GetBytesFromMtu(pPortInfo->MTU.Cap), indent+4);
			if (! g_hard)
			{
				int i;
				char indxStr[5];
				printf("%*s<ActiveMTU>\n", indent+4, "");
				for (i = 0; i < STL_MAX_VLS; ++i)
				{
					snprintf(indxStr, sizeof(indxStr), "VL%u", i);
					XmlPrintDec(indxStr, GetBytesFromMtu(GET_STL_PORT_INFO_NeighborMTU(pPortInfo, i)), indent+8);
				}
				printf("%*s</ActiveMTU>\n", indent+4, "");
				
			}
			if (! g_hard)
			{
				int i;
				char indxStr[5];
				printf("%*s<VLStall>\n", indent+4, "");
				for (i = 0; i < STL_MAX_VLS; ++i)
				{
					snprintf(indxStr, sizeof(indxStr), "VL%u", i);
					XmlPrintDec(indxStr, pPortInfo->XmitQ[i].VLStallCount, indent+8);
				}	
				printf("%*s</VLStall>\n", indent+4, "");
			}

			if (! g_hard) {
				XmlPrintLinkWidth("LinkWidthActive",
					pPortInfo->LinkWidth.Active, indent+4);
			}
			XmlPrintLinkWidth("LinkWidthSupported",
					pPortInfo->LinkWidth.Supported, indent+4);
			if (! g_hard) {
				XmlPrintLinkWidth("LinkWidthEnabled",
					pPortInfo->LinkWidth.Enabled, indent+4);
			}
			if (! g_hard) {
				XmlPrintLinkWidth("LinkWidthDnGradeTxActive",
					pPortInfo->LinkWidthDowngrade.TxActive, indent+4);
				XmlPrintLinkWidth("LinkWidthDnGradeRxActive",
					pPortInfo->LinkWidthDowngrade.RxActive, indent+4);
			}
			XmlPrintLinkWidth("LinkWidthDnGradeSupported",
					pPortInfo->LinkWidthDowngrade.Supported, indent+4);
			if (! g_hard) {
				XmlPrintLinkWidth("LinkWidthDnGradeEnabled",
					pPortInfo->LinkWidthDowngrade.Enabled, indent+4);
			}
			if (! g_hard) {
				XmlPrintLinkSpeed("LinkSpeedActive",
					pPortInfo->LinkSpeed.Active, indent+4);
			}
			XmlPrintLinkSpeed("LinkSpeedSupported",
					pPortInfo->LinkSpeed.Supported, indent+4);
			if (! g_hard) {
				XmlPrintLinkSpeed("LinkSpeedEnabled",
					pPortInfo->LinkSpeed.Enabled, indent+4);
			}
			
			XmlPrintStr("PortLinkModeSupported", 
				StlPortLinkModeToText(pPortInfo->PortLinkMode.s.Supported, buf1,
				sizeof(buf1)), indent+4);	
				
			if (! g_hard) {	
				XmlPrintStr("PortLinkModeActive", 
					StlPortLinkModeToText(pPortInfo->PortLinkMode.s.Active, buf1,
					sizeof(buf1)), indent+4);	
				XmlPrintStr("PortLinkModeEnabled",
					StlPortLinkModeToText(pPortInfo->PortLinkMode.s.Enabled, buf1, 
					sizeof(buf1)), indent+4);	
			}
			
			XmlPrintDec("SM_TrapQP", pPortInfo->SM_TrapQP.s.QueuePair, indent+4);
			XmlPrintDec("SA_QP", pPortInfo->SA_QP.s.QueuePair, indent+4);
			XmlPrintStr("IPV6", inet_ntop(AF_INET6, pPortInfo->IPAddrIPV6.addr, buf1, sizeof(buf1)), indent+4);
			XmlPrintStr("IPV4", inet_ntop(AF_INET, pPortInfo->IPAddrIPV4.addr, buf2, sizeof(buf2)), indent+4);

			if (! g_hard) {
				printf( "%*s<VLsActive>%u+1</VLsActive>\n",
					indent+4, "",
					pPortInfo->s4.OperationalVL);
				XmlPrintDec("VLsActive_Int", 
					pPortInfo->s4.OperationalVL, indent+4);
			}
			printf( "%*s<VLsSupported>%u+1</VLsSupported>\n",
					indent+4, "",
					pPortInfo->VL.s2.Cap);
			XmlPrintDec("VLsSupported_Int", 
					pPortInfo->VL.s2.Cap, indent+4);

			if (! g_hard)
			{
				int i;
				char indxStr[5];
				printf("%*s<HOQLife>\n", indent+4, "");
				for (i = 0; i < STL_MAX_VLS; ++i)
				{
					snprintf(indxStr, sizeof(indxStr), "VL%u", i);
					if (pPortInfo->XmitQ[i].HOQLife > IB_LIFETIME_MAX)
					{
						strncpy(buf1, "Infinite", 9);
					} else {
						FormatTimeoutMult(buf1, pPortInfo->XmitQ[i].HOQLife);
					}
					XmlPrintStr(indxStr, buf1, indent+8);
				}	
				printf("%*s</HOQLife>\n", indent+4, "");
				printf("%*s<HOQLife_Int>\n", indent+4, "");
				for (i = 0; i < STL_MAX_VLS; ++i)
				{
					snprintf(indxStr, sizeof(indxStr), "VL%u", i);
					if (pPortInfo->XmitQ[i].HOQLife > IB_LIFETIME_MAX)
					{
						strncpy(buf1, "Infinite", 9);
					} else {
						FormatTimeoutMult(buf1, pPortInfo->XmitQ[i].HOQLife);
					}
					XmlPrintDec(indxStr, pPortInfo->XmitQ[i].HOQLife, indent+8);
				}	
				printf("%*s</HOQLife_Int>\n", indent+4, "");
			}

			if (pPortInfo->CapabilityMask3.s.VLSchedulingConfig == VL_SCHED_MODE_VLARB) {
				XmlPrintDec("VLArbHighCap", pPortInfo->VL.ArbitrationHighCap,
							indent+4);

				XmlPrintDec("VLArbLowCap", pPortInfo->VL.ArbitrationLowCap,
							indent+4);
				if (! g_hard) {
					XmlPrintDec("VLArbHighLimit", pPortInfo->VL.HighLimit,
							indent+4);
					XmlPrintDec("VLArbPreemptLimit", pPortInfo->VL.PreemptingLimit,
							indent+4);
				}
			}



			XmlPrintHex32("VLFlowControlDisabledMask", pPortInfo->FlowControlMask, indent+4);
			XmlPrintStr("NeighborModeMgmntAllowed", pPortInfo->PortNeighborMode.MgmtAllowed?"Yes":"No", indent+4);
			XmlPrintStr("NeighborModeFWAuthenBypass", pPortInfo->PortNeighborMode.NeighborFWAuthenBypass?"Yes":"No", indent+4);
			XmlPrintStr("NeighborModeNeighborNodeType",
				OpaNeighborNodeTypeToText(pPortInfo->PortNeighborMode.NeighborNodeType),
				indent+4);

			FormatStlCapabilityMask(buf1, pPortInfo->CapabilityMask);
			XmlPrintHex32("CapabilityMask",
				pPortInfo->CapabilityMask.AsReg32,
				indent+4);
			XmlPrintStr("Capability", buf1, indent+4);
			XmlPrintHex16("CapabilityMask3", pPortInfo->CapabilityMask3.AsReg16, indent+4);
			FormatStlCapabilityMask3(buf1, pPortInfo->CapabilityMask3, sizeof(buf1));
			XmlPrintStr("Capability3", buf1, indent+4);
			if (g_persist || g_hard) {
				// noop
			} else {
				XmlPrintDec("ViolationsM_Key",
					pPortInfo->Violations.M_Key, indent+4);
				XmlPrintDec("ViolationsP_Key",
					pPortInfo->Violations.P_Key, indent+4);
				XmlPrintDec("ViolationsQ_Key",
					pPortInfo->Violations.Q_Key, indent+4);
			}
			if (g_hard) {
				// noop
			} else {
				XmlPrintHex16( "DiagCode", pPortInfo->DiagCode.AsReg16, indent+4);
			}
			
			XmlPrintHex16("OverallBufferSpace", pPortInfo->OverallBufferSpace, indent+4);
			if (! g_hard && ! g_persist) {
				XmlPrintHex8("LinkDownReason", pPortInfo->LinkDownReason, indent+4);
				XmlPrintHex8("NeighborLinkDownReason", pPortInfo->NeighborLinkDownReason, indent+4);
			}
			if (! g_hard ) {
				XmlPrintStr("ActiveOptimize", pPortInfo->PortMode.s.IsActiveOptimizeEnabled?"On":"Off", indent+4);
				XmlPrintStr("PassThrough", pPortInfo->PortMode.s.IsPassThroughEnabled?"On":"Off", indent+4);
				XmlPrintStr("VLMarker", pPortInfo->PortMode.s.IsVLMarkerEnabled?"On":"Off", indent+4);
			}
			
			XmlPrintDec("FlitCtrlInterleaveDistanceMax", pPortInfo->FlitControl.Interleave.s.DistanceSupported,indent+4);
			if (! g_hard) {
				XmlPrintDec("MaxNestLevelRxSupported", pPortInfo->FlitControl.Interleave.s.MaxNestLevelRxSupported,indent+4);
				XmlPrintDec("FlitControlInterleaveDistance", pPortInfo->FlitControl.Interleave.s.DistanceEnabled, indent+4);
			}

			XmlPrintDec("SmallPktLimitMax", pPortInfo->FlitControl.Preemption.MaxSmallPktLimit, indent+4);
			if (! g_hard) {
				XmlPrintDec("SmallPktLimit", pPortInfo->FlitControl.Preemption.SmallPktLimit, indent+4);
				XmlPrintDec("PreemptionLimit", pPortInfo->FlitControl.Preemption.PreemptionLimit, indent+4);
				// Convert Flits to bytes and display in decimal
				XmlPrintDec("MinInitial", pPortInfo->FlitControl.Preemption.MinInitial, indent+4);
				// Convert Flits to bytes and display in decimal
				XmlPrintDec("MinTail", (pPortInfo->FlitControl.Preemption.MinTail * BYTES_PER_FLIT), indent+4);
				XmlPrintDec("LargePktLimit", (pPortInfo->FlitControl.Preemption.LargePktLimit * BYTES_PER_FLIT), indent+4);
			}

			if (! g_hard ) {
				if(pPortInfo->CapabilityMask3.s.IsMAXLIDSupported)
					XmlPrintLID("MaxLID", pPortInfo->MaxLID, indent+4);

			}

			if (! g_hard )
				XmlPrintHex32("PortErrorAction", pPortInfo->PortErrorAction.AsReg32, indent+4);
		
			XmlPrintHex16("VL15Init", pPortInfo->BufferUnits.s.VL15Init, indent+4);
			XmlPrintHex8("CreditAck", pPortInfo->BufferUnits.s.CreditAck, indent+4);
			XmlPrintHex8("BufferAlloc", pPortInfo->BufferUnits.s.BufferAlloc, indent+4);
			XmlPrintHex8("ReplayDepthBuffer", pPortInfo->ReplayDepth.BufferDepth, indent+4);
			if (!g_persist && !g_hard)
				XmlPrintHex8("ReplayDepthWire", pPortInfo->ReplayDepth.WireDepth, indent+4);
			
			if (! g_hard) {
				XmlPrintHex8("VL15CreditRate", pPortInfo->BufferUnits.s.VL15CreditRate, indent+4);
			}
			if (! g_hard) {
				XmlPrintStr( "P_KeyEnforcementInbound",
					pPortInfo->s3.PartitionEnforcementInbound?"On":"Off",
					indent+4);
				XmlPrintDec( "P_KeyEnforcementInbound_Int",
					pPortInfo->s3.PartitionEnforcementInbound,
					indent+4);
				XmlPrintStr("P_KeyEnforcementOutbound",
					pPortInfo->s3.PartitionEnforcementOutbound?"On":"Off",
					indent+4);
				XmlPrintDec("P_KeyEnforcementOutbound_Int",
					pPortInfo->s3.PartitionEnforcementOutbound,
					indent+4);
			}
			if ( portp->nodep && portp->pQOS && 
					(detail > 1) && !g_persist && !g_hard ) {
				if ( portp->pQOS->SL2SCMap ) {
					ShowSLSCTable(portp->nodep, portp, format, indent+8, detail-2);
				}
				if ( portp->pQOS->SC2SLMap ) {
					ShowSCSLTable(portp->nodep, portp, format, indent+8, detail-2);
				}
				if ( !(QListIsEmpty(&portp->pQOS->SC2SCMapList[0])) )  {
					ShowSCSCTable(portp->nodep, portp, 0, format, indent+8, detail-2);
				}
				if ( !(QListIsEmpty(&portp->pQOS->SC2SCMapList[1])) )  {
					ShowSCSCTable(portp->nodep, portp, 1, format, indent+8, detail-2);
				}
			}
			if (portp->pQOS && (detail > 1) && !g_persist && !g_hard) {
				ShowVLArbTable(portp, format, indent+8, detail-2);
			}
			if ( portp->nodep && portp->pPartitionTable && (detail > 1) &&
					!g_persist && !g_hard ) {
				ShowPKeyTable(portp->nodep, portp, format, indent+8, detail-2);
			}
			if (portp->pPortStatus && detail > 1 && ! g_persist && ! g_hard) {
				ShowPortCounters(portp->pPortStatus, format, indent+8, detail-2);
			}
		} else {
			if (! g_hard) {
				XmlPrintLinkWidth("LinkWidthActive",
					pPortInfo->LinkWidth.Active, indent+4);
				XmlPrintLinkSpeed("LinkSpeedActive",
					pPortInfo->LinkSpeed.Active, indent+4);
			}
		}
		if (detail && portp->pCableInfoData)
			ShowCableSummary(portp->pCableInfoData, FORMAT_XML, indent+4, detail, pPortInfo->PortPhysConfig.s.PortType);
		printf("%*s</PortInfo>\n", indent, "");
		break;
	default:
		break;
	}
}	// End of ShowPortSummary()

// output verbose summary of a IB Node
void ShowNodeSummary(NodeData *nodep, Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	char buf1[SHOW_BUF_SIZE], buf2[SHOW_BUF_SIZE];

	switch (format) {
	case FORMAT_TEXT:
		// omit fields which are port specific:
		// LID, PortGUID, LocalPortNum
		// TBD - is PartitionCap per Port or per Node?
		printf("%*sName: %.*s\n",
					indent, "",
					STL_NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString);
		printf("%*sNodeGUID: 0x%016"PRIx64" Type: %s\n",
					indent+4, "", nodep->NodeInfo.NodeGUID,
					StlNodeTypeToText(nodep->NodeInfo.NodeType));
		if (nodep->enodep && nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", nodep->enodep->details);
		}
		printf("%*sPorts: %d PartitionCap: %d SystemImageGuid: 0x%016"PRIx64"\n",
					indent+4, "", nodep->NodeInfo.NumPorts,
					nodep->NodeInfo.PartitionCap,
					nodep->NodeInfo.SystemImageGUID);
		printf("%*sBaseVer: %d SmaVer: %d VendorID: 0x%x DeviceID: 0x%x Rev: 0x%x\n",
					indent+4, "", nodep->NodeInfo.BaseVersion,
					nodep->NodeInfo.ClassVersion,
					nodep->NodeInfo.u1.s.VendorID,
					nodep->NodeInfo.DeviceID,
					nodep->NodeInfo.Revision);
		if (nodep->pSwitchInfo) {
			STL_SWITCHINFO_RECORD *pSwitchInfoRecord = nodep->pSwitchInfo;
			STL_SWITCH_INFO *pSwitchInfo = &pSwitchInfoRecord->SwitchInfoData;
			if (g_persist || g_hard)
				printf("%*sLID: xxxxxxxxxx", indent+4, "");
			else
				printf("%*sLID: 0x%.*x", indent+4, "", (pSwitchInfoRecord->RID.LID <= IB_MAX_UCAST_LID ? 4:8),
					pSwitchInfoRecord->RID.LID);
			printf( " LinearFDBCap: %5u ",
				pSwitchInfo->LinearFDBCap );
			if (g_persist || g_hard)
				printf("LinearFDBTop: xxxxxxxxxx");
			else
				printf( "LinearFDBTop: 0x%08x",
					pSwitchInfo->LinearFDBTop );
			printf( " MCFDBCap: %5u\n",
				pSwitchInfo->MulticastFDBCap );
			printf("%*sPartEnfCap: %5u ", indent+4, "",	pSwitchInfo->PartitionEnforcementCap);
			if (g_persist || g_hard) {
				printf("U1: 0x%02x PortStateChange: x  SwitchLifeTime x\n",
					pSwitchInfo->u1.AsReg8);
				printf("%*sU2: 0x%02x: %s\n",
					indent+4, "",
					pSwitchInfo->u2.AsReg8,
					pSwitchInfo->u2.s.EnhancedPort0?"E0 ": "");
			} else { 
				printf("U1: 0x%02x PortStateChange: %1u  SwitchLifeTime %2u\n",
					pSwitchInfo->u1.AsReg8,
					pSwitchInfo->u1.s.PortStateChange, 
					pSwitchInfo->u1.s.LifeTimeValue);
				printf("%*sU2: 0x%02x: %s\n",
					indent+4, "",
					pSwitchInfo->u2.AsReg8,
					pSwitchInfo->u2.s.EnhancedPort0?"E0 ": "");
			}


			if (! (g_persist || g_hard)) {
				printf("%*sAR: 0x%04x: %s%s%s%sF%u T%u\n",
					indent+4, "",
					pSwitchInfo->AdaptiveRouting.AsReg16,
					pSwitchInfo->AdaptiveRouting.s.Enable?"On ": "",
					pSwitchInfo->AdaptiveRouting.s.Pause?"Pause ": "",
					pSwitchInfo->AdaptiveRouting.s.Algorithm?((pSwitchInfo->AdaptiveRouting.s.Algorithm==2)?"RGreedy ":"Greedy "): "Random ",
					pSwitchInfo->AdaptiveRouting.s.LostRoutesOnly?"LostOnly ": "",
					pSwitchInfo->AdaptiveRouting.s.Frequency,
					pSwitchInfo->AdaptiveRouting.s.Threshold);
			} else {
				printf("%*sAR: xxxxxx: x\n", indent+4, "");
			}

			printf("%*sCapabilityMask: 0x%04x: %s%s\n",
					indent+4, "",
					pSwitchInfo->CapabilityMask.AsReg16,
					pSwitchInfo->CapabilityMask.s.IsAddrRangeConfigSupported?"ARC ":"",
					pSwitchInfo->CapabilityMask.s.IsAdaptiveRoutingSupported?"AR ":"");

			if (! (g_persist || g_hard)) {
				printf("%*sRouting Mode: Supported: 0x%x Enabled 0x%x\n",
					indent+4, "",
					pSwitchInfo->RoutingMode.Supported,
					pSwitchInfo->RoutingMode.Enabled);
			} else {
				printf("%*sRouting Mode: Supported: 0x%x Enabled x\n", 
					indent+4, "",
					pSwitchInfo->RoutingMode.Supported);
			}
			{
				printf("%*sIPAddrIPV6:  %s IPAddrIPV4: %s\n",
					indent+4, "",
					inet_ntop(AF_INET6, 
						pSwitchInfo->IPAddrIPV6.addr, 
						buf1, sizeof(buf1)),
					inet_ntop(AF_INET, 
						pSwitchInfo->IPAddrIPV4.addr, 
						buf2, sizeof(buf2)));
			}
		}
	
		printf("%*s%u Connected Ports%s\n", indent, "",
				CountInitializedPorts(&g_Fabric, nodep), detail?":":"");
		break;
	case FORMAT_XML:
		// omit fields which are port specific:
		// LID, PortGUID, LocalPortNum
		// TBD - is PartitionCap per Port or per Node?
		printf("%*s<Node id=\"0x%016"PRIx64"\">\n", indent, "",
					nodep->NodeInfo.NodeGUID);
		XmlPrintNodeDesc((char*)nodep->NodeDesc.NodeString, indent+4);
		XmlPrintNodeType(nodep->NodeInfo.NodeType, indent+4);
		if (nodep->enodep && nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", nodep->enodep->details, indent+4);
		}
		XmlPrintDec("BaseVer", nodep->NodeInfo.BaseVersion, indent+4);
		XmlPrintDec("SmaVer", nodep->NodeInfo.ClassVersion, indent+4);
		XmlPrintDec("NumPorts", nodep->NodeInfo.NumPorts, indent+4);
		XmlPrintHex64("SystemImageGUID", nodep->NodeInfo.SystemImageGUID, indent+4);
		XmlPrintHex64("NodeGUID", nodep->NodeInfo.NodeGUID, indent+4);
		XmlPrintDec("PartitionCap", nodep->NodeInfo.PartitionCap, indent+4);
		XmlPrintHex("DeviceID", nodep->NodeInfo.DeviceID, indent+4);
		XmlPrintHex("Revision", nodep->NodeInfo.Revision, indent+4);
		XmlPrintHex("VendorID", nodep->NodeInfo.u1.s.VendorID, indent+4);

		if (nodep->pSwitchInfo) {
			STL_SWITCHINFO_RECORD *pSwitchInfoRecord = nodep->pSwitchInfo;
			STL_SWITCH_INFO *pSwitchInfo = &pSwitchInfoRecord->SwitchInfoData;
			if (! (g_persist || g_hard))
				XmlPrintLID("LID", pSwitchInfoRecord->RID.LID, indent+4);
			XmlPrintDec("LinearFDBCap", pSwitchInfo->LinearFDBCap, indent+4);
			XmlPrintDec("MulticastFDBCap", pSwitchInfo->MulticastFDBCap, indent+4);
			if (! (g_persist || g_hard))
				XmlPrintDec("LinearFDBTop", pSwitchInfo->LinearFDBTop, indent+4);

			{
				XmlPrintStr("IPAddrIPV6",
					inet_ntop(AF_INET6, 
						pSwitchInfo->IPAddrIPV6.addr, 
						buf1, sizeof(buf1)),
					indent+4);
				XmlPrintStr("IPAddrIPV4",
					inet_ntop(AF_INET, 
						pSwitchInfo->IPAddrIPV4.addr, 
						buf1, sizeof(buf1)),
					indent+4);
			}
			XmlPrintHex8("U1",pSwitchInfo->u1.AsReg8,indent+4);
			if ( ! (g_persist || g_hard)) {
				XmlPrintDec("PortStateChange",
					pSwitchInfo->u1.s.PortStateChange,
					indent+4);
				XmlPrintDec("SwitchLifeTime",
					pSwitchInfo->u1.s.LifeTimeValue,
					indent+4);
			}
			XmlPrintDec("PartitionEnforcementCap",
					pSwitchInfo->PartitionEnforcementCap,
					indent+4);
			XmlPrintHex8("RoutingModeSupported",
					pSwitchInfo->RoutingMode.Supported, indent+4);
			if (!g_hard && !g_persist) {
				XmlPrintHex8("RoutingModeEnabled",
					pSwitchInfo->RoutingMode.Enabled, indent+4);
			}
			XmlPrintHex8("U2",
					pSwitchInfo->u2.AsReg8, indent+4);
			printf("%*s<Capability>%s</Capability>\n",
					indent+4, "",
					pSwitchInfo->u2.s.EnhancedPort0?"E0 ": "");
			if ( ! (g_persist || g_hard)) {
				XmlPrintHex8("AdaptiveRouting",
					pSwitchInfo->AdaptiveRouting.AsReg16, indent+4);
				XmlPrintDec("LostRoutesOnly",
					pSwitchInfo->AdaptiveRouting.s.LostRoutesOnly, indent+4);
				XmlPrintDec("Pause",
					pSwitchInfo->AdaptiveRouting.s.Pause, indent+4);
				XmlPrintDec("Enable",
					pSwitchInfo->AdaptiveRouting.s.Enable, indent+4);
				XmlPrintDec("Algorithm",
					pSwitchInfo->AdaptiveRouting.s.Algorithm, indent+4);
				XmlPrintDec("Frequency",
					pSwitchInfo->AdaptiveRouting.s.Frequency, indent+4);
				XmlPrintDec("Threshold",
					pSwitchInfo->AdaptiveRouting.s.Threshold, indent+4);
			}

			if (pSwitchInfo->CapabilityMask.AsReg16) {
				// only output if there are some vendor capabilities
				XmlPrintHex16("StlCapabilityMask",
					pSwitchInfo->CapabilityMask.AsReg16, indent+4);
				printf("%*s<StlCapability>%s</StlCapability>\n",
					indent+4, "",
					pSwitchInfo->CapabilityMask.s.IsAdaptiveRoutingSupported?"AR ": "");
			}
		}
	
		XmlPrintDec("ConnectedPorts", CountInitializedPorts(&g_Fabric, nodep),
						indent+4);
		break;
	default:
		break;
	}
	if (detail) {
		for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_next(p)) {
			PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
			if (! ComparePortPoint(portp, focus))
				continue;
			ShowPortSummary(portp, format, indent+4, detail-1);
		}
	}
	if (nodep->ioup) {
		/* IOU report is brief, do it at top level detail for Node */
		ShowIouSummary(nodep->ioup, focus, format, indent, detail);
	}
	if (format == FORMAT_XML) {
		printf("%*s</Node>\n", indent, "");
	}
}	// End of ShowNodeSummary()

// output verbose summary of a IB System
void ShowSystemSummary(SystemData *systemp, Point *focus,
						Format_t format, int indent, int detail)
{
	cl_map_item_t *p;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sSystemImageGUID: 0x%016"PRIx64"\n", indent, "", systemp->SystemImageGUID);
		printf("%*s%u Connected Nodes%s\n", indent, "", (unsigned)cl_qmap_count(&systemp->Nodes), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<System id=\"0x%016"PRIx64"\">\n", indent, "",
					systemp->SystemImageGUID?systemp->SystemImageGUID:
					PARENT_STRUCT(cl_qmap_head(&systemp->Nodes), NodeData, SystemNodesEntry)->NodeInfo.NodeGUID);
		XmlPrintHex64("SystemImageGUID", systemp->SystemImageGUID, indent+4);
		XmlPrintDec("ConnectedNodes", (unsigned)cl_qmap_count(&systemp->Nodes),
						indent+4);
		break;
	default:
		break;
	}
	if (detail) {
		for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
			NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
			if (! CompareNodePoint(nodep, focus))
				continue;
			ShowNodeSummary(nodep, focus, format, indent+4, detail-1);
		}
	}
	if (format == FORMAT_XML) {
		printf("%*s</System>\n", indent, "");
	}
}

// output verbose summary of IB SMs
void ShowSMSummary(SMData *smp, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		if (g_hard || g_persist)
			printf("%*sState: xxxxxxxxxxx Name: %.*s\n",
					indent, "",
					NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)smp->portp->nodep->NodeDesc.NodeString);
		else
			printf("%*sState: %-11s Name: %.*s\n",
					indent, "",
					IbSMStateToText(smp->SMInfoRecord.SMInfo.u.s.SMStateCurrent),
					NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)smp->portp->nodep->NodeDesc.NodeString);
		printf("%*sNodeGUID: 0x%016"PRIx64" Type: %s\n",
					indent+4, "", smp->portp->nodep->NodeInfo.NodeGUID,
					StlNodeTypeToText(smp->portp->nodep->NodeInfo.NodeType));
		if (smp->portp->nodep->enodep && smp->portp->nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", smp->portp->nodep->enodep->details);
		}
		if (smp->esmp && smp->esmp->details) {
			printf("%*sSMDetails: %s\n", indent+4, "", smp->esmp->details);
		}
		if (detail) {
			if (g_hard || g_persist)
				printf("%*sPortNum: %3u LID: xxxxxxxxxx PortGUID: 0x%016"PRIx64"\n",
						indent+4, "", smp->portp->PortNum,
						smp->SMInfoRecord.SMInfo.PortGUID);
			else
				printf("%*sPortNum: %3u LID: 0x%.*x PortGUID: 0x%016"PRIx64"\n",
						indent+4, "", smp->portp->PortNum,
						(smp->SMInfoRecord.RID.LID <= IB_MAX_UCAST_LID ? 4:8),
						smp->SMInfoRecord.RID.LID, smp->SMInfoRecord.SMInfo.PortGUID);
			if (g_hard)
				printf("%*sSM_Key: xxxxxxxxxxxxxxxxxx Priority: xxx ActCount: xxxxxxxxxx\n",
						indent+4, "");
			else if (g_persist)
				printf("%*sSM_Key: xxxxxxxxxxxxxxxxxx Priority: %3d ActCount: xxxxxxxxxx\n",
						indent+4, "",
						smp->SMInfoRecord.SMInfo.u.s.Priority);
			else
				printf("%*sSM_Key: 0x%016"PRIx64" Priority: %3d ActCount: 0x%08x\n",
						indent+4, "",
						smp->SMInfoRecord.SMInfo.SM_Key,
						smp->SMInfoRecord.SMInfo.u.s.Priority,
						smp->SMInfoRecord.SMInfo.ActCount);
		}
		break;
	case FORMAT_XML:
		printf("%*s<SM id=\"0x%016"PRIx64":%u\">\n", indent, "",
					smp->portp->nodep->NodeInfo.NodeGUID,
					smp->portp->PortNum);
		if (! (g_hard || g_persist)) {
			XmlPrintStr("State",
					IbSMStateToText(smp->SMInfoRecord.SMInfo.u.s.SMStateCurrent),
					indent+4);
			XmlPrintDec("State_Int",
					smp->SMInfoRecord.SMInfo.u.s.SMStateCurrent,
					indent+4);
		}
		XmlPrintNodeDesc(
					(char*)smp->portp->nodep->NodeDesc.NodeString, indent+4);
		XmlPrintHex64("NodeGUID",
					smp->portp->nodep->NodeInfo.NodeGUID,
					indent+4);
		XmlPrintNodeType(smp->portp->nodep->NodeInfo.NodeType,
					indent+4);
		if (smp->portp->nodep->enodep && smp->portp->nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", smp->portp->nodep->enodep->details, indent+4);
		}
		if (smp->esmp && smp->esmp->details) {
			XmlPrintOptionalStr("SMDetails", smp->esmp->details, indent+4);
		}
		if (detail) {
			XmlPrintDec("PortNum", smp->portp->PortNum, indent+4);
			if (! (g_hard || g_persist))
				XmlPrintLID("LID",
						smp->SMInfoRecord.RID.LID, indent+4);
			XmlPrintHex64("PortGUID",
						smp->SMInfoRecord.SMInfo.PortGUID, indent+4);
			if (g_hard) {
				// noop
			} else if (g_persist)
				XmlPrintDec("Priority",
						smp->SMInfoRecord.SMInfo.u.s.Priority, indent+4);
			else {
				XmlPrintHex64("SM_Key",
						smp->SMInfoRecord.SMInfo.SM_Key, indent+4);
				XmlPrintDec("Priority",
						smp->SMInfoRecord.SMInfo.u.s.Priority, indent+4);
				XmlPrintHex32("ActCount",
						smp->SMInfoRecord.SMInfo.ActCount, indent+4);
				}
		}
		printf("%*s</SM>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowAllSMSummary(Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	uint32 count = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected SMs in Fabric%s\n", indent, "", (unsigned)cl_qmap_count(&g_Fabric.AllSMs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<SMs>\n", indent, "");
		XmlPrintDec("ConnectedSMCount", (unsigned)cl_qmap_count(&g_Fabric.AllSMs),
						indent+4);
		break;
	default:
		break;
	}

	for (p=cl_qmap_head(&g_Fabric.AllSMs); p != cl_qmap_end(&g_Fabric.AllSMs); p = cl_qmap_next(p)) {
		SMData *smp = PARENT_STRUCT(p, SMData, AllSMsEntry);
		if (! CompareSmPoint(smp, focus))
			continue;
		if (detail)
			ShowSMSummary(smp, format, indent+4, detail-1);
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching SMs Found\n", indent, "", count);
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSMs", count, indent+4);
		printf("%*s</SMs>\n", indent, "");
		break;
	default:
		break;
	}
}

// output verbose summary of all IB Components (Systems, Nodes, Ports, SMs)
void ShowComponentReport(Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	uint32 count = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sComponent Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<ComponentSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Systems%s\n", indent, "", (unsigned)cl_qmap_count(&g_Fabric.AllSystems), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Systems>\n", indent, "");
		XmlPrintDec("ConnectedSystems", (unsigned)cl_qmap_count(&g_Fabric.AllSystems),
						indent+4);
		break;
	default:
		break;
	}
	for (p=cl_qmap_head(&g_Fabric.AllSystems); p != cl_qmap_end(&g_Fabric.AllSystems); p = cl_qmap_next(p)) {
		SystemData *systemp = PARENT_STRUCT(p, SystemData, AllSystemsEntry);
		if (! CompareSystemPoint(systemp, focus))
			continue;
		if (detail)
			ShowSystemSummary(systemp, focus, format, indent+4, detail-1);
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Systems Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSystems", count, indent+4);
		printf("%*s</Systems>\n", indent, "");
		break;
	default:
		break;
	}
	ShowAllSMSummary(focus, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</ComponentSummary>\n", indent, "");
		break;
	default:
		break;
	}
}

// output verbose summary of all IB Node Types
void ShowNodeTypeReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sNode Type Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<NodeTypeSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected FIs in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllFIs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<FIs>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedFICount", (unsigned)QListCount(&g_Fabric.AllFIs), indent);
		break;
	default:
		break;
	}
	count = 0;
	for (p=QListHead(&g_Fabric.AllFIs); p != NULL; p = QListNext(&g_Fabric.AllFIs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		if (! CompareNodePoint(nodep, focus))
			continue;
		if (detail)
			ShowNodeSummary(nodep, focus, format, indent+4, detail-1);
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching FIs Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingFIs", count, indent+4);
		indent-=4;
		printf("%*s</FIs>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Switches in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Switches>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs),
						indent);
		break;
	default:
		break;
	}
	count = 0;
	for (p=QListHead(&g_Fabric.AllSWs); p != NULL; p = QListNext(&g_Fabric.AllSWs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		if (! CompareNodePoint(nodep, focus))
			continue;
		if (detail)
			ShowNodeSummary(nodep, focus, format, indent+4, detail-1);
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Switches Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSwitches", count, indent+4);
		indent-=4;
		printf("%*s</Switches>\n", indent, "");
		break;
	default:
		break;
	}

	ShowAllSMSummary(focus, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</NodeTypeSummary>\n", indent, "");
		break;
	default:
		break;
	}
}

// output verbose summary of all IOUs
void ShowAllIOUReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sIOU Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<IOUSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u IOUs in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllIOUs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<IOUs>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedIOUCount", (unsigned)QListCount(&g_Fabric.AllIOUs), indent);
		break;
	default:
		break;
	}

	for (p=QListHead(&g_Fabric.AllIOUs); p != NULL; p = QListNext(&g_Fabric.AllIOUs, p)) {
		IouData *ioup = (IouData *)QListObj(p);
		if (! CompareIouPoint(ioup, focus))
			continue;
		if (detail)
			ShowNodeSummary(ioup->nodep, focus, format, indent+4, detail-1);
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching IOUs Found\n", indent, "", count);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingIOUs", count, indent);
		indent-=4;
		printf("%*s</IOUs>\n", indent, "");
		indent-=4;
		printf("%*s</IOUSummary>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowOtherPortSummaryHeader(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sNodeGUID          Port Type Name\n", indent, "");
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

// show a non-connected port in a node
void ShowOtherPortSummary(NodeData *nodep, uint8 portNum,
			Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s0x%016"PRIx64" %3u %s %.*s\n",
			indent, "",
			nodep->NodeInfo.NodeGUID,
			portNum,
			StlNodeTypeToText(nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString);
		if (nodep->enodep && nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", nodep->enodep->details);
		}
		break;
	case FORMAT_XML:
		printf("%*s<OtherPort id=\"0x%016"PRIx64":%u\">\n", indent, "",
				nodep->NodeInfo.NodeGUID, portNum);
		XmlPrintHex64("NodeGUID",
				nodep->NodeInfo.NodeGUID, indent+4);
		XmlPrintDec("PortNum", portNum, indent+4);
		XmlPrintNodeType(nodep->NodeInfo.NodeType,
						indent+4);
		XmlPrintNodeDesc((char*)nodep->NodeDesc.NodeString, indent+4);
		if (nodep->enodep && nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", nodep->enodep->details, indent+4);
		}
		break;
	default:
		break;
	}
	if (format == FORMAT_XML)
		printf("%*s</OtherPort>\n", indent, "");
}

// show a non-connected port in a node
void ShowNodeOtherPortSummary(NodeData *nodep,
			Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	uint8 port;

	for (port=1, p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports);)
	{
		PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
		if (portp->PortNum == 0)
		{
			p = cl_qmap_next(p);
			continue;	/* skip switch port 0 */
		}
		if (portp->PortNum != port) {
			ShowOtherPortSummary(nodep, port, format, indent, detail);
		} else {
			if (! IsPortInitialized(portp->PortInfo.PortStates))
				ShowOtherPortSummary(nodep, port, format, indent, detail);
			p = cl_qmap_next(p);
		}
		port++;
	}
	/* output remaining ports after last connected port */
	for (; port <= nodep->NodeInfo.NumPorts; port++)
		ShowOtherPortSummary(nodep, port, format, indent, detail);
}

// output verbose summary of IB ports not connected to this fabric
void ShowOtherPortsReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 node_count;
	uint32 port_count;
	uint32 other_port_count;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sOther Ports Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<OtherPortsSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected FIs in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllFIs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<FIs>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedFICount", (unsigned)QListCount(&g_Fabric.AllFIs), indent);
		break;
	default:
		break;
	}
	node_count = 0;
	port_count = 0;
	other_port_count = 0;
	for (p=QListHead(&g_Fabric.AllFIs); p != NULL; p = QListNext(&g_Fabric.AllFIs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		uint32 initialized_port_count;

		if (! CompareNodePoint(nodep, focus))
			continue;
		initialized_port_count = CountInitializedPorts(&g_Fabric, nodep);
		port_count += initialized_port_count;
		if (initialized_port_count >= nodep->NodeInfo.NumPorts)
			continue;
		// for FIs otherports will include ports connected to other fabrics
		if (node_count == 0 && detail)
			ShowOtherPortSummaryHeader(format, indent+4, detail-1);
		other_port_count += nodep->NodeInfo.NumPorts - initialized_port_count;
		if (detail)
			ShowNodeOtherPortSummary(nodep, format, indent+4, detail-1);
		node_count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching FIs Found\n", indent, "", node_count);
		printf("%*s%u Connected FI Ports\n", indent, "", port_count);
		printf("%*s%u Other FI Ports\n", indent, "", other_port_count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingFIs", node_count, indent+4);
		XmlPrintDec("ConnectedFIPorts", port_count, indent+4);
		XmlPrintDec("OtherCAPorts", other_port_count, indent+4);
		indent-=4;
		printf("%*s</FIs>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Switches in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Switches>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs),
						indent);
		break;
	default:
		break;
	}
	node_count = 0;
	port_count = 0;
	other_port_count = 0;
	for (p=QListHead(&g_Fabric.AllSWs); p != NULL; p = QListNext(&g_Fabric.AllSWs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		uint32 initialized_port_count;

		if (! CompareNodePoint(nodep, focus))
			continue;
		initialized_port_count = CountInitializedPorts(&g_Fabric, nodep);
		/* don't count switch port 0 */
		if (initialized_port_count) {
			PortData *portp = PARENT_STRUCT(cl_qmap_head(&nodep->Ports), PortData, NodePortsEntry);
			if (portp->PortNum == 0 && IsPortInitialized(portp->PortInfo.PortStates))
				initialized_port_count--;
		}
		port_count += initialized_port_count;
		if (initialized_port_count >= nodep->NodeInfo.NumPorts)
			continue;
		if (node_count == 0 && detail)
			ShowOtherPortSummaryHeader(format, indent+4, detail-1);
		other_port_count += nodep->NodeInfo.NumPorts - initialized_port_count;
		if (detail)
			ShowNodeOtherPortSummary(nodep, format, indent+4, detail-1);
		node_count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Switches Found\n", indent, "", node_count);
		printf("%*s%u Connected Switch Ports\n", indent, "", port_count);
		printf("%*s%u Other Switch Ports\n", indent, "", other_port_count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSwitches", node_count, indent+4);
		XmlPrintDec("ConnectedSwitchPorts", port_count, indent+4);
		XmlPrintDec("OtherSwitchPorts", other_port_count, indent+4);
		indent-=4;
		printf("%*s</Switches>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</OtherPortsSummary>\n", indent, "");
		break;
	default:
		break;
	}
}

// undocumented report on sizes
void ShowSizesReport(void)
{
	printf("sizeof(SystemData)=%u\n", (unsigned)sizeof(SystemData));
	printf("sizeof(NodeData)=%u\n", (unsigned)sizeof(NodeData));
	printf("sizeof(PortData)=%u\n", (unsigned)sizeof(PortData));
	printf("sizeof(QOSData)=%u (up to 1 per port)\n", (unsigned)sizeof(QOSData));
	printf("sizeof(STL_PortStatusData_t)=%u (up to 1 per port)\n", (unsigned)sizeof(STL_PortStatusData_t));
	printf("sizeof(STL_PKEY_ELEMENT)=%u (up to 1 per port per pkey)\n", (unsigned)sizeof(STL_PKEY_ELEMENT));
	printf("sizeof(IouData)=%u\n", (unsigned)sizeof(IouData));
	printf("sizeof(IocData)=%u\n", (unsigned)sizeof(IocData));
	printf("sizeof(IOC_SERVICE)=%u\n", (unsigned)sizeof(IOC_SERVICE));
	printf("sizeof(SMData)=%u\n", (unsigned)sizeof(SMData));
	printf("sizeof(STL_SWITCHINFO_RECORD)=%u (up to 1 per switch)\n", (unsigned)sizeof(STL_SMINFO_RECORD));
	printf("sizeof(IB_PATH_RECORD)=%u (up to 1 per port)\n", (unsigned)sizeof(IB_PATH_RECORD));
	printf("sizeof(STL_NODE_RECORD)=%u\n", (unsigned)sizeof(STL_NODE_RECORD));
	printf("sizeof(STL_PORTINFO_RECORD)=%u\n", (unsigned)sizeof(STL_PORTINFO_RECORD));
	printf("sizeof(STL_LINK_RECORD)=%u\n", (unsigned)sizeof(STL_LINK_RECORD));
}

// output brief summary of a IB Port
void ShowPortBriefSummary(PortData *portp, Format_t format, int indent, int detail)
{
	char buf1[SHOW_BUF_SIZE], buf2[SHOW_BUF_SIZE];

	switch (format) {
	case FORMAT_TEXT:
		if (portp->PortGUID)
			if (g_hard || g_persist)
				printf("%*s%3u xxxxxx 0x%016"PRIx64,
					indent, "", portp->PortNum,
					portp->PortGUID);
			else
				printf("%*s%3u 0x%04x 0x%016"PRIx64,
					indent, "", portp->PortNum,
					portp->EndPortLID, portp->PortGUID);
		else
			printf("%*s%3u                          ",
				indent, "", portp->PortNum);
		if (g_hard)
			printf(" xxxx xxxxxxx\n");
		else
			printf(" %4s %7s\n",
				StlLinkWidthToText(portp->PortInfo.LinkWidth.Active, buf1, sizeof(buf1)),
				StlLinkSpeedToText(portp->PortInfo.LinkSpeed.Active, buf2, sizeof(buf2)));
		break;
	case FORMAT_XML:
		printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintDec("PortNum", portp->PortNum, indent+4);
		if (portp->PortGUID) {
			if (! (g_hard || g_persist)) {
				XmlPrintLID("LID",
					portp->EndPortLID, indent+4);
				if (portp->PortInfo.s1.LMC)
					XmlPrintDec("LMC",
						portp->PortInfo.s1.LMC, indent+4);
                       	}
			XmlPrintHex64("PortGUID", portp->PortGUID, indent+4);
		}
		if (g_hard) {
			// noop
		} else {
			XmlPrintLinkWidth("LinkWidthActive",
				portp->PortInfo.LinkWidth.Active, indent+4);
			XmlPrintLinkSpeed("LinkSpeedActive",
				portp->PortInfo.LinkSpeed.Active, indent+4);
		}
		printf("%*s</Port>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowPortBriefSummaryHeadings(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sPort LID   PortGUID           Width Speed\n", indent, "");
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

static void PrintXmlNodeSummaryBrief(NodeData *nodep, int indent)
{
	printf("%*s<Node id=\"0x%016"PRIx64"\">\n", indent, "",	nodep->NodeInfo.NodeGUID);
	XmlPrintHex64("NodeGUID",nodep->NodeInfo.NodeGUID, indent+4);
	XmlPrintNodeType(nodep->NodeInfo.NodeType, indent+4);
	XmlPrintNodeDesc((char*)nodep->NodeDesc.NodeString, indent+4);
	if (nodep->enodep && nodep->enodep->details) {
		XmlPrintOptionalStr("NodeDetails", nodep->enodep->details, indent+4);
	}
}

static void PrintBriefNodePorts(NodeData *nodep, Point *focus, Format_t format,
				int indent, int detail)
{
	cl_map_item_t *p;

	for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_next(p)) {
		PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
		if (! ComparePortPoint(portp, focus))
			continue;
		ShowPortBriefSummary(portp, format, indent+4, detail-1);
	}
}
// output brief summary of a IB Node
void ShowNodeBriefSummary(NodeData *nodep, Point *focus,
				boolean close_node, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s0x%016"PRIx64" %s %.*s\n",
				indent, "", nodep->NodeInfo.NodeGUID,
				StlNodeTypeToText(nodep->NodeInfo.NodeType),
				NODE_DESCRIPTION_ARRAY_SIZE,
				g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString);
		if (nodep->enodep && nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", nodep->enodep->details);
		}
		break;
	case FORMAT_XML:
		PrintXmlNodeSummaryBrief(nodep, indent);
		break;
	default:
		break;
	}
	if (detail)
		PrintBriefNodePorts(nodep, focus, format, indent, detail);

	if (close_node && format == FORMAT_XML) {
		printf("%*s</Node>\n", indent, "");
	}
}

void ShowTopologyNodeBriefSummary(NodeData *nodep, Point *focus, int indent, int detail)
{
	PrintXmlNodeSummaryBrief(nodep, indent);
	// Print node/port xml tag only for opareport option -d3 or greater detail level
	if (detail >= 2)
		PrintBriefNodePorts(nodep, focus, FORMAT_XML, indent, detail);
	printf("%*s</Node>\n", indent, "");
}

// output brief summary of an expected IB Node
void ShowExpectedNodeBriefSummary(const char *prefix, ExpectedNode *enodep,
				const char *xml_tag, boolean close_node, Format_t format,
				int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%s", indent, "", prefix);
		if (enodep->NodeGUID)
			printf("0x%016"PRIx64, enodep->NodeGUID);
		else
			printf("%*s", 18, "");
		if (enodep->NodeType)
			printf(" %s", StlNodeTypeToText(enodep->NodeType));
		else
			printf("   ");
		if (enodep->NodeDesc)
			printf(" %.*s\n", NODE_DESCRIPTION_ARRAY_SIZE,
						g_noname?g_name_marker:enodep->NodeDesc);
		else
			printf("\n");
		if (enodep->details)
			printf("%*sNodeDetails: %s\n", indent+4, "", enodep->details);
		break;
	case FORMAT_XML:
		printf("%*s<%s id=\"0x%016"PRIx64"\">\n", indent, "",
					xml_tag, (uint64)(uintn)enodep);
		if (enodep->NodeGUID)
			XmlPrintHex64("NodeGUID", enodep->NodeGUID, indent+4);
		if (enodep->NodeType)
			XmlPrintNodeType(enodep->NodeType, indent+4);
		if (enodep->NodeDesc)
			XmlPrintNodeDesc(enodep->NodeDesc, indent+4);
		if (enodep->details)
			XmlPrintOptionalStr("NodeDetails", enodep->details, indent+4);
		break;
	default:
		break;
	}
	if (close_node && format == FORMAT_XML) {
		printf("%*s</%s>\n", indent, "", xml_tag);
	}
}

void ShowNodeBriefSummaryHeadings(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sNodeGUID         Type Name\n", indent, "");
		if (detail)
			ShowPortBriefSummaryHeadings(format, indent+4, detail-1);
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

// output brief summary of a IB System
void ShowSystemBriefSummary(SystemData *systemp, Point *focus,
						Format_t format, int indent, int detail)
{
	cl_map_item_t *p;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s0x%016"PRIx64"\n", indent, "", systemp->SystemImageGUID);
		break;
	case FORMAT_XML:
		printf("%*s<BriefSystem id=\"0x%016"PRIx64"\">\n", indent, "",
					systemp->SystemImageGUID?systemp->SystemImageGUID:
					PARENT_STRUCT(cl_qmap_head(&systemp->Nodes), NodeData, SystemNodesEntry)->NodeInfo.NodeGUID);
		XmlPrintHex64("SystemImageGUID", systemp->SystemImageGUID, indent+4);
		break;
	default:
		break;
	}
	if (detail) {
		for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
			NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
			if (! CompareNodePoint(nodep, focus))
				continue;
			ShowNodeBriefSummary(nodep, focus, TRUE, format, indent+4, detail-1);
		}
	}
	if (format == FORMAT_XML) {
		printf("%*s</BriefSystem>\n", indent, "");
	}
}

void ShowSystemBriefSummaryHeadings(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sSystemImage GUID\n", indent, "");
		if (detail)
			ShowNodeBriefSummaryHeadings(format, indent+4, detail-1);
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

void ShowSMBriefSummary(SMData *smp, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		if (g_hard || g_persist)
			printf("%*sxxxxxxxxxxx 0x%016"PRIx64" %.*s\n",
					indent, "",
					smp->portp->nodep->NodeInfo.NodeGUID,
					NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)smp->portp->nodep->NodeDesc.NodeString);
		else
			printf("%*s%-11s 0x%016"PRIx64" %.*s\n",
					indent, "",
					IbSMStateToText(smp->SMInfoRecord.SMInfo.u.s.SMStateCurrent),
					smp->portp->nodep->NodeInfo.NodeGUID,
					NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)smp->portp->nodep->NodeDesc.NodeString);
		if (smp->portp->nodep->enodep && smp->portp->nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", smp->portp->nodep->enodep->details);
		}
		if (smp->esmp && smp->esmp->details) {
			printf("%*sSMDetails: %s\n", indent+4, "", smp->esmp->details);
		}
		break;
	case FORMAT_XML:
		printf("%*s<SM id=\"0x%016"PRIx64":%u\">\n", indent, "",
					smp->portp->nodep->NodeInfo.NodeGUID,
					smp->portp->PortNum);
		if ( ! (g_hard || g_persist)) {
			XmlPrintStr("SMState",
					IbSMStateToText(smp->SMInfoRecord.SMInfo.u.s.SMStateCurrent),
					indent+4);
			XmlPrintDec("SMState_Int",
					smp->SMInfoRecord.SMInfo.u.s.SMStateCurrent, indent+4);
		}
		XmlPrintHex64("NodeGUID",
					smp->portp->nodep->NodeInfo.NodeGUID,
					indent+4);
		XmlPrintNodeDesc(
					(char*)smp->portp->nodep->NodeDesc.NodeString, indent+4);
		if (smp->portp->nodep->enodep && smp->portp->nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", smp->portp->nodep->enodep->details, indent+4);
		}
		if (smp->esmp && smp->esmp->details) {
			XmlPrintOptionalStr("SMDetails", smp->esmp->details, indent+4);
		}
		// we output this additional information to aid topology SM verify
		XmlPrintDec("PortNum", smp->portp->PortNum, indent+4);
		if (smp->portp->PortGUID) {
			XmlPrintHex64("PortGUID", smp->portp->PortGUID, indent+4);
		}
		XmlPrintNodeType(smp->portp->nodep->NodeInfo.NodeType,
						indent+4);
		printf("%*s</SM>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowSMBriefSummaryHeadings(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sState       GUID               Name\n", indent, "");
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

void ShowVerifySMBriefSummary(SMData *smp,
				boolean close_sm, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s0x%016"PRIx64" %3u", indent, "",
					smp->portp->nodep->NodeInfo.NodeGUID,
					smp->portp->PortNum);
		if (smp->portp->PortGUID)
			printf(" 0x%016"PRIx64, smp->portp->PortGUID);
		else
			printf("                   ");
		printf(" %s %.*s\n",
					StlNodeTypeToText(smp->portp->nodep->NodeInfo.NodeType),
					NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)smp->portp->nodep->NodeDesc.NodeString);
		if (smp->portp->nodep->enodep && smp->portp->nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", smp->portp->nodep->enodep->details);
		}
		if (smp->esmp && smp->esmp->details) {
			printf("%*sSMDetails: %s\n", indent+4, "", smp->esmp->details);
		}
		break;
	case FORMAT_XML:
		printf("%*s<SM id=\"0x%016"PRIx64":%u\">\n", indent, "",
					smp->portp->nodep->NodeInfo.NodeGUID,
					smp->portp->PortNum);
		XmlPrintHex64("NodeGUID",
					smp->portp->nodep->NodeInfo.NodeGUID,
					indent+4);
		XmlPrintDec("PortNum", smp->portp->PortNum, indent+4);
		if (smp->portp->PortGUID) {
			XmlPrintHex64("PortGUID", smp->portp->PortGUID, indent+4);
		}
		XmlPrintNodeType(smp->portp->nodep->NodeInfo.NodeType,
						indent+4);
		XmlPrintNodeDesc(
					(char*)smp->portp->nodep->NodeDesc.NodeString, indent+4);
		if (smp->portp->nodep->enodep && smp->portp->nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", smp->portp->nodep->enodep->details, indent+4);
		}
		if (smp->esmp && smp->esmp->details) {
			XmlPrintOptionalStr("SMDetails", smp->esmp->details, indent+4);
		}
		if (close_sm)
			printf("%*s</SM>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowExpectedSMBriefSummary(const char *prefix, ExpectedSM *esmp,
				const char *xml_tag, boolean close_sm, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%s", indent, "", prefix);
		if (esmp->NodeGUID)
			printf("0x%016"PRIx64, esmp->NodeGUID);
		else
			printf("                  ");
		if (esmp->gotPortNum)
			printf(" %3u", esmp->PortNum);
		else
			printf("    ");
		if (esmp->PortGUID)
			printf(" 0x%016"PRIx64, esmp->PortGUID);
		else
			printf("                  ");
		if (esmp->NodeType)
			printf(" %s", StlNodeTypeToText(esmp->NodeType));
		else
			printf("   ");
		if (esmp->NodeDesc)
			printf(" %s\n", g_noname?g_name_marker:esmp->NodeDesc);
		else
			printf("\n");
		if (esmp->smp && esmp->smp->portp->nodep->enodep && esmp->smp->portp->nodep->enodep->details)
			printf("%*sNodeDetails: %s\n", indent+4, "", esmp->smp->portp->nodep->enodep->details);
		if (esmp->details)
			printf("%*sSMDetails: %s\n", indent+4, "", esmp->details);
		break;
	case FORMAT_XML:
		printf("%*s<%s id=\"0x%016"PRIx64"\">\n", indent, "",
					xml_tag, (uint64)(uintn)esmp);
		if (esmp->NodeGUID)
			XmlPrintHex64("NodeGUID", esmp->NodeGUID, indent+4);
		if (esmp->gotPortNum)
			XmlPrintDec("PortNum", esmp->PortNum, indent+4);
		if (esmp->NodeType)
			XmlPrintNodeType(esmp->NodeType, indent+4);
		if (esmp->NodeDesc)
			XmlPrintNodeDesc(esmp->NodeDesc, indent+4);
		if (esmp->smp && esmp->smp->portp->nodep->enodep && esmp->smp->portp->nodep->enodep->details)
			XmlPrintOptionalStr("NodeDetails", esmp->smp->portp->nodep->enodep->details, indent+4);
		if (esmp->details)
			XmlPrintOptionalStr("SMDetails", esmp->details, indent+4);
		if (esmp->PortGUID) {
			XmlPrintHex64("PortGUID", esmp->PortGUID, indent+4);
		}
		if (close_sm)
			printf("%*s</%s>\n", indent, "", xml_tag);
		break;
	default:
		break;
	}
}

void ShowVerifySMBriefSummaryHeadings(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sNodeGUID          Port PortGUID         Type Name\n", indent, "");
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

// output brief summary of IB SMs
void ShowAllSMBriefSummary(Point *focus, Format_t format, int indent, int detail)
{
	uint32 count = 0;
	cl_map_item_t *p;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected SMs in Fabric%s\n", indent, "", (unsigned)cl_qmap_count(&g_Fabric.AllSMs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<SMs>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSMCount", (unsigned)cl_qmap_count(&g_Fabric.AllSMs), indent);
		break;
	default:
		break;
	}
	for (p=cl_qmap_head(&g_Fabric.AllSMs); p != cl_qmap_end(&g_Fabric.AllSMs); p = cl_qmap_next(p)) {
		SMData *smp = PARENT_STRUCT(p, SMData, AllSMsEntry);
		if (! CompareSmPoint(smp, focus))
			continue;
		if (detail) {
			if (! count)
				ShowSMBriefSummaryHeadings(format, indent, detail-1);
			ShowSMBriefSummary(smp, format, indent, detail-1);
		}
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching SMs Found\n", indent, "", count);
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSMs", count, indent+4);
		indent-=4;
		printf("%*s</SMs>\n", indent, "");
		break;
	default:
		break;
	}
}

// output verbose summary of all IB Components (Systems, Nodes, Ports)
void ShowComponentBriefReport(Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	uint32 count = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sComponent Brief Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<Components>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Systems in Fabric%s\n", indent, "", (unsigned)cl_qmap_count(&g_Fabric.AllSystems), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Systems>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSystemCount", (unsigned)cl_qmap_count(&g_Fabric.AllSystems), indent);
		break;
	default:
		break;
	}
	for (p=cl_qmap_head(&g_Fabric.AllSystems); p != cl_qmap_end(&g_Fabric.AllSystems); p = cl_qmap_next(p)) {
		SystemData *systemp = PARENT_STRUCT(p, SystemData, AllSystemsEntry);
		if (! CompareSystemPoint(systemp, focus))
			continue;
		if (detail) {
			if (! count)
				ShowSystemBriefSummaryHeadings(format, indent, detail-1);
			ShowSystemBriefSummary(systemp, focus, format, indent, detail-1);
		}
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Systems Found\n", indent, "", count);
		printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSystems", count, indent);
		indent-=4;
		printf("%*s</Systems>\n", indent, "");
		break;
	default:
		break;
	}
	ShowAllSMBriefSummary(focus, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</Components>\n", indent, "");
		break;
	default:
		break;
	}
}

// output brief summary of all IB Node Types
void ShowNodeTypeBriefReport(Point *focus, Format_t format, report_t report, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sNode Type Brief Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<Nodes>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected FIs in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllFIs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<FIs>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedFICount", (unsigned)QListCount(&g_Fabric.AllFIs), indent);
		break;
	default:
		break;
	}
	count = 0;
	for (p=QListHead(&g_Fabric.AllFIs); p != NULL; p = QListNext(&g_Fabric.AllFIs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		if (! CompareNodePoint(nodep, focus))
			continue;
		if (detail) {
			if (! count)
				ShowNodeBriefSummaryHeadings(format, indent, detail-1);
			if (report == REPORT_TOPOLOGY)
				ShowTopologyNodeBriefSummary(nodep, focus, indent, detail-1);
			else
				ShowNodeBriefSummary(nodep, focus, TRUE, format, indent, detail-1);
		}
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching FI Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingFIs", count, indent);
		indent-=4;
		printf("%*s</FIs>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Switches in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Switches>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}
	count = 0;
	for (p=QListHead(&g_Fabric.AllSWs); p != NULL; p = QListNext(&g_Fabric.AllSWs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		if (! CompareNodePoint(nodep, focus))
			continue;
		if (detail) {
			if (! count)
				ShowNodeBriefSummaryHeadings(format, indent, detail-1);
			if (report == REPORT_TOPOLOGY)
				ShowTopologyNodeBriefSummary(nodep, focus, indent, detail-1);
			else
				ShowNodeBriefSummary(nodep, focus, TRUE, format, indent, detail-1);
		}
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Switches Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSwitches", count, indent);
		indent-=4;
		printf("%*s</Switches>\n", indent, "");
		break;
	default:
		break;
	}

	ShowAllSMBriefSummary(focus, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</Nodes>\n", indent, "");
		break;
	default:
		break;
	}
}

static _inline
boolean PortCounterBelowThreshold(uint32 value, uint32 threshold)
{
	return (threshold && value < threshold);
}

static _inline
boolean PortCounterExceedsThreshold(uint32 value, uint32 threshold)
{
	return (threshold && value > threshold - g_threshold_compare);
}

static _inline
boolean PortCounterExceedsThreshold64(uint64 value, uint64 threshold)
{
	return (threshold && value > threshold - g_threshold_compare);
}

// check the last port counters against the new vs threshold
// returns: TRUE - one or more counters exceed threshold
//			FALSE - all counters below threshold
static boolean PortCountersExceedThreshold(PortData *portp)
{
	STL_PortStatusData_t *pPortStatus = portp->pPortStatus;

	if (! pPortStatus)
		return FALSE;

#define EXCEEDS_THRESHOLD(field) \
			PortCounterExceedsThreshold(pPortStatus->field, g_Thresholds.field)
#define EXCEEDS_THRESHOLD64(field) \
			PortCounterExceedsThreshold64(pPortStatus->field, g_Thresholds.field)
#define BELOW_THRESHOLD_LQI(field) \
			PortCounterBelowThreshold(pPortStatus->lq.s.field, g_Thresholds.lq.s.field)
#define EXCEEDS_THRESHOLD_NLD(field) \
			PortCounterExceedsThreshold((pPortStatus->lq.field >> 4), (g_Thresholds.lq.field >> 4))

			// Data movement
	return EXCEEDS_THRESHOLD64(PortXmitData)
			|| EXCEEDS_THRESHOLD64(PortRcvData)
			|| EXCEEDS_THRESHOLD64(PortXmitPkts)
			|| EXCEEDS_THRESHOLD64(PortRcvPkts)
			|| EXCEEDS_THRESHOLD64(PortMulticastXmitPkts)
			|| EXCEEDS_THRESHOLD64(PortMulticastRcvPkts)
			// Signal Integrity and Node/Link Stability
			|| BELOW_THRESHOLD_LQI(LinkQualityIndicator)
			|| EXCEEDS_THRESHOLD(UncorrectableErrors)
			|| EXCEEDS_THRESHOLD(LinkDowned)
			|| EXCEEDS_THRESHOLD64(PortRcvErrors)
			|| EXCEEDS_THRESHOLD64(FMConfigErrors)
			|| EXCEEDS_THRESHOLD64(ExcessiveBufferOverruns)
			|| EXCEEDS_THRESHOLD(LinkErrorRecovery)
			|| EXCEEDS_THRESHOLD64(LocalLinkIntegrityErrors)
			|| EXCEEDS_THRESHOLD64(PortRcvRemotePhysicalErrors)
			|| EXCEEDS_THRESHOLD_NLD(AsReg8)
			// Security
			|| EXCEEDS_THRESHOLD64(PortXmitConstraintErrors)
			|| EXCEEDS_THRESHOLD64(PortRcvConstraintErrors)
			// Routing or Down nodes still being sent to
			|| EXCEEDS_THRESHOLD64(PortRcvSwitchRelayErrors)
			|| EXCEEDS_THRESHOLD64(PortXmitDiscards)
			// Congestion
			|| EXCEEDS_THRESHOLD64(SwPortCongestion)
			|| EXCEEDS_THRESHOLD64(PortRcvFECN)
			|| EXCEEDS_THRESHOLD64(PortRcvBECN)
			|| EXCEEDS_THRESHOLD64(PortMarkFECN)
			|| EXCEEDS_THRESHOLD64(PortXmitTimeCong)
			|| EXCEEDS_THRESHOLD64(PortXmitWait)
			// Bubbles
			|| EXCEEDS_THRESHOLD64(PortXmitWastedBW)
			|| EXCEEDS_THRESHOLD64(PortXmitWaitData)
			|| EXCEEDS_THRESHOLD64(PortRcvBubble);
#undef EXCEEDS_THRESHOLD
#undef EXCEEDS_THRESHOLD64
#undef BELOW_THRESHOLD_LQI
#undef EXCEEDS_THRESHOLD_NLD
}

void ShowPortCounterBelowThreshold(const char* field, uint32 value, uint32 threshold, Format_t format, int indent, int detail)
{
	if (PortCounterBelowThreshold(value, threshold))
	{
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s: %u Below Threshold: %u\n",
				indent, "", field, value, threshold);
			break;
		case FORMAT_XML:
			// old format
			printf("%*s<%s>\n", indent, "", field);
			XmlPrintDec("Value", value, indent+4);
			XmlPrintDec("LowerThreshold", threshold, indent+4);
			printf("%*s</%s>\n", indent, "", field);
			// new format
			printf("%*s<%sValue>%u</%sValue>\n", indent, "", field, value, field);
			printf("%*s<%sThreshold>%u</%sThreshold>\n", indent, "", field, threshold, field);
			break;
		default:
			break;
		}
	}
}

void ShowPortCounterExceedingThreshold(const char* field, uint32 value, uint32 threshold, Format_t format, int indent, int detail)
{
	if (PortCounterExceedsThreshold(value, threshold))
	{
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s: %u Exceeds Threshold: %u\n",
				indent, "", field, value, threshold);
			break;
		case FORMAT_XML:
			// old format
			printf("%*s<%s>\n", indent, "", field);
			XmlPrintDec("Value", value, indent+4);
			XmlPrintDec("Threshold", threshold, indent+4);
			printf("%*s</%s>\n", indent, "", field);
			// new format
			printf("%*s<%sValue>%u</%sValue>\n", indent, "", field, value, field);
			printf("%*s<%sThreshold>%u</%sThreshold>\n", indent, "", field, threshold, field);
			break;
		default:
			break;
		}
	}
}

void ShowPortCounterExceedingThreshold64(const char* field, uint64 value, uint64 threshold, Format_t format, int indent, int detail)
{
	if (PortCounterExceedsThreshold64(value, threshold))
	{
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s: %"PRIu64" Exceeds Threshold: %"PRIu64"\n",
				indent, "", field, value, threshold);
			break;
		case FORMAT_XML:
			// old format
			printf("%*s<%s>\n", indent, "", field);
			XmlPrintDec64("Value", value, indent+4);
			XmlPrintDec64("Threshold", threshold, indent+4);
			printf("%*s</%s>\n", indent, "", field);
			// new format
			printf("%*s<%sValue>%"PRIu64"</%sValue>\n", indent, "", field, value, field);
			printf("%*s<%sThreshold>%"PRIu64"</%sThreshold>\n", indent, "", field, threshold, field);
			break;
		default:
			break;
		}
	}
}

void ShowPortCounterExceedingMbThreshold64(const char* field, uint64 value, uint64 threshold, Format_t format, int indent, int detail)
{
	if (PortCounterExceedsThreshold(value, threshold))
	{
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s: %"PRIu64" MB Exceeds Threshold: %u MB\n",
				indent, "", field, value/FLITS_PER_MB, (unsigned int)(threshold/FLITS_PER_MB));
			break;
		case FORMAT_XML:
			// old format
			printf("%*s<%s>\n", indent, "", field);
			XmlPrintDec64("ValueMB", value/FLITS_PER_MB, indent+4);
			XmlPrintDec64("ThresholdMB", threshold/FLITS_PER_MB, indent+4);
			printf("%*s</%s>\n", indent, "", field);
			// new format
			printf("%*s<%sValueMB>%"PRIu64"</%sValueMB>\n", indent, "", field, value/FLITS_PER_MB, field);
			printf("%*s<%sThresholdMB>%u</%sThresholdMB>\n", indent, "", field, (unsigned int)(threshold/FLITS_PER_MB), field);
			break;
		default:
			break;
		}
	}
}

void ShowLinkPortErrorSummary(PortData *portp, Format_t format, int indent, int detail)
{
	STL_PortStatusData_t *pPortStatus = portp->pPortStatus;

	if (! pPortStatus)
		return;

#define SHOW_BELOW_LQI_THRESHOLD(field, name) \
			ShowPortCounterBelowThreshold(#name, pPortStatus->lq.s.field, g_Thresholds.lq.s.field, format, indent, detail)
#define SHOW_EXCEEDING_THRESHOLD(field, name) \
			ShowPortCounterExceedingThreshold(#name, pPortStatus->field, g_Thresholds.field, format, indent, detail)
#define SHOW_EXCEEDING_THRESHOLD64(field, name) \
			ShowPortCounterExceedingThreshold64(#name, pPortStatus->field, g_Thresholds.field, format, indent, detail)
#define SHOW_EXCEEDING_MB_THRESHOLD(field, name) \
			ShowPortCounterExceedingMbThreshold64(#name, pPortStatus->field, g_Thresholds.field, format, indent, detail)
#define SHOW_EXCEEDING_NLD_THRESHOLD(field, name) \
			ShowPortCounterExceedingThreshold(#name, (pPortStatus->lq.field >> 4), (g_Thresholds.lq.field >> 4), format, indent, detail)
	// Data movement
	SHOW_EXCEEDING_MB_THRESHOLD(PortXmitData, XmitData);
	SHOW_EXCEEDING_MB_THRESHOLD(PortRcvData, RcvData);
	SHOW_EXCEEDING_THRESHOLD64(PortXmitPkts, XmitPkts);
	SHOW_EXCEEDING_THRESHOLD64(PortRcvPkts, RcvPkts);
	SHOW_EXCEEDING_THRESHOLD64(PortMulticastXmitPkts, MulticastXmitPkts);
	SHOW_EXCEEDING_THRESHOLD64(PortMulticastRcvPkts, MulticastRcvPkts);
	// Signal Integrity and Node/Link Stability
	SHOW_BELOW_LQI_THRESHOLD(LinkQualityIndicator, LinkQualityIndicator);
	SHOW_EXCEEDING_THRESHOLD(LinkDowned, LinkDowned);
	SHOW_EXCEEDING_THRESHOLD(UncorrectableErrors, UncorrectableErrors);
	SHOW_EXCEEDING_THRESHOLD64(FMConfigErrors, FMConfigErrors);
	SHOW_EXCEEDING_THRESHOLD64(PortRcvErrors, RcvErrors);
	SHOW_EXCEEDING_THRESHOLD64(ExcessiveBufferOverruns, ExcessiveBufferOverruns);
	SHOW_EXCEEDING_THRESHOLD(LinkErrorRecovery, LinkErrorRecovery);
	SHOW_EXCEEDING_THRESHOLD64(LocalLinkIntegrityErrors, LocalLinkIntegrityErrors);
	SHOW_EXCEEDING_THRESHOLD64(PortRcvRemotePhysicalErrors, RcvRemotePhysicalErrors);
	SHOW_EXCEEDING_NLD_THRESHOLD(AsReg8, NumLanesDown);
	// Security
	SHOW_EXCEEDING_THRESHOLD64(PortXmitConstraintErrors, XmitConstraintErrors);
	SHOW_EXCEEDING_THRESHOLD64(PortRcvConstraintErrors, RcvConstraintErrors);
	// Routing or Down nodes still being sent to
	SHOW_EXCEEDING_THRESHOLD64(PortRcvSwitchRelayErrors, RcvSwitchRelayErrors);
	SHOW_EXCEEDING_THRESHOLD64(PortXmitDiscards, XmitDiscards);
	// Congestion
	SHOW_EXCEEDING_THRESHOLD64(SwPortCongestion, CongDiscards);
	SHOW_EXCEEDING_THRESHOLD64(PortRcvFECN, RcvFECN);
	SHOW_EXCEEDING_THRESHOLD64(PortRcvBECN, RcvBECN);
	SHOW_EXCEEDING_THRESHOLD64(PortMarkFECN, MarkFECN);
	SHOW_EXCEEDING_THRESHOLD64(PortXmitTimeCong, XmitTimeCong);
	SHOW_EXCEEDING_THRESHOLD64(PortXmitWait, XmitWait);
	// Bubbles
	SHOW_EXCEEDING_THRESHOLD64(PortXmitWastedBW, XmitWastedBW);
	SHOW_EXCEEDING_THRESHOLD64(PortXmitWaitData, XmitWaitData);
	SHOW_EXCEEDING_THRESHOLD64(PortRcvBubble, RcvBubble);
#undef SHOW_BELOW_LQI_THRESHOLD
#undef SHOW_EXCEEDING_THRESHOLD
#undef SHOW_EXCEEDING_THRESHOLD64
#undef SHOW_EXCEEDING_MB_THRESHOLD
#undef SHOW_EXCEEDING_NLD_THRESHOLD
}

// returns TRUE if thresholds are configured
boolean ShowThresholds(Format_t format, int indent, int detail)
{
	boolean didoutput = FALSE;
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sConfigured Thresholds:\n",indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<ConfiguredThresholds>\n",indent, "");
		break;
	default:
		break;
	}
#define SHOW_THRESHOLD(field, name) \
	do { if (g_Thresholds.field) { switch (format) { case FORMAT_TEXT: printf("%*s%-30s %lu\n", indent+4, "", #name, (uint64)g_Thresholds.field); break; case FORMAT_XML: printf("%*s<%s>%lu</%s>\n", indent+4, "", #name, (uint64)g_Thresholds.field, #name); break; default: break; } didoutput = TRUE; } }  while (0)

#define SHOW_THRESHOLD_LQI(field, name) \
	do { if (g_Thresholds.lq.s.field) { switch (format) { case FORMAT_TEXT: printf("%*s%-30s %lu\n", indent+4, "", #name, (uint64)g_Thresholds.lq.s.field); break; case FORMAT_XML: printf("%*s<%s>%lu</%s>\n", indent+4, "", #name, (uint64)g_Thresholds.lq.s.field, #name); break; default: break; } didoutput = TRUE; } }  while (0)

#define SHOW_MB_THRESHOLD(field, name) \
	do { if (g_Thresholds.field) { switch (format) { case FORMAT_TEXT: printf("%*s%-30s %lu MB\n", indent+4, "", #name, (uint64)g_Thresholds.field/FLITS_PER_MB); break; case FORMAT_XML: printf("%*s<%sMB>%lu</%sMB>\n", indent+4, "", #name, (uint64)g_Thresholds.field/FLITS_PER_MB, #name); break; default: break; } didoutput = TRUE; } }  while (0)

#define SHOW_THRESHOLD_NLD(field, name) \
	do { if (g_Thresholds.lq.field >> 4) { switch (format) { case FORMAT_TEXT: printf("%*s%-30s %lu\n", indent+4, "", #name, (uint64)(g_Thresholds.lq.field >> 4)); break; case FORMAT_XML: printf("%*s<%s>%lu</%s>\n", indent+4, "", #name, (uint64)(g_Thresholds.lq.field >> 4), #name); break; default: break; } didoutput = TRUE; } }  while (0)

	// Data movement
	SHOW_MB_THRESHOLD(PortXmitData, XmitData);
	SHOW_MB_THRESHOLD(PortRcvData, RcvData);
	SHOW_THRESHOLD(PortXmitPkts, XmitPkts);
	SHOW_THRESHOLD(PortRcvPkts, RcvPkts);
	SHOW_THRESHOLD(PortMulticastXmitPkts, MulticastXmitPkts);
	SHOW_THRESHOLD(PortMulticastRcvPkts, MulticastRcvPkts);

	// Signal Integrity and Node/Link Stability
	SHOW_THRESHOLD_LQI(LinkQualityIndicator, LinkQualityIndicator);
	SHOW_THRESHOLD(UncorrectableErrors, UncorrectableErrors);
	SHOW_THRESHOLD(LinkDowned, LinkDowned);
	SHOW_THRESHOLD(PortRcvErrors, RcvErrors);
	SHOW_THRESHOLD(ExcessiveBufferOverruns, ExcessiveBufferOverruns);
	SHOW_THRESHOLD(FMConfigErrors, FMConfigErrors);
	SHOW_THRESHOLD(LinkErrorRecovery, LinkErrorRecovery);
	SHOW_THRESHOLD(LocalLinkIntegrityErrors, LocalLinkIntegrityErrors);
	SHOW_THRESHOLD(PortRcvRemotePhysicalErrors, RcvRemotePhysicalErrors);
	SHOW_THRESHOLD_NLD(AsReg8, NumLanesDown);

	// Security
	SHOW_THRESHOLD(PortXmitConstraintErrors, XmitConstraintErrors);
	SHOW_THRESHOLD(PortRcvConstraintErrors, RcvConstraintErrors);

	// Routing or Down nodes still being sent to
	SHOW_THRESHOLD(PortRcvSwitchRelayErrors, RcvSwitchRelayErrors);
	SHOW_THRESHOLD(PortXmitDiscards, XmitDiscards);

	// Congestion
	SHOW_THRESHOLD(SwPortCongestion, CongDiscards);
	SHOW_THRESHOLD(PortRcvFECN, RcvFECN);
	SHOW_THRESHOLD(PortRcvBECN, RcvBECN);
	SHOW_THRESHOLD(PortMarkFECN, MarkFECN);
	SHOW_THRESHOLD(PortXmitTimeCong, XmitTimeCong);
	SHOW_THRESHOLD(PortXmitWait, XmitWait);

	// Bubbles
	SHOW_THRESHOLD(PortXmitWastedBW, XmitWastedBW);
	SHOW_THRESHOLD(PortXmitWaitData, XmitWaitData);
	SHOW_THRESHOLD(PortRcvBubble, RcvBubble);

	switch (format) {
	case FORMAT_TEXT:
		if (! didoutput)
			printf("%*sNone\n", indent+4, "");
		break;
	case FORMAT_XML:
		printf("%*s</ConfiguredThresholds>\n",indent, "");
		break;
	default:
		break;
	}

#undef SHOW_THRESHOLD
#undef SHOW_MB_THRESHOLD
#undef SHOW_THRESHOLD_LQI
#undef SHOW_THRESHOLD_NLD
	return didoutput;
}

// returns TRUE if thresholds are configured for any counters
boolean NeedClearCounters(void)
{
	boolean needclear = FALSE;

#define CHECK_COUNTER(field) \
	do { needclear |= (0 != g_CounterSelectMask.CounterSelectMask.s.field); } while(0)

	// Data movement
	CHECK_COUNTER(PortXmitData);
	CHECK_COUNTER(PortRcvData);
	CHECK_COUNTER(PortXmitPkts);
	CHECK_COUNTER(PortRcvPkts);
	CHECK_COUNTER(PortMulticastXmitPkts);
	CHECK_COUNTER(PortMulticastRcvPkts);

	// Signal Integrity and Node/Link Stability
	// LinkQualityIndicator N/A since readonly can't clear
	CHECK_COUNTER(UncorrectableErrors);
	CHECK_COUNTER(LinkDowned);
	CHECK_COUNTER(PortRcvErrors);
	CHECK_COUNTER(ExcessiveBufferOverruns);
	CHECK_COUNTER(FMConfigErrors);
	CHECK_COUNTER(LinkErrorRecovery);
	CHECK_COUNTER(LocalLinkIntegrityErrors);
	CHECK_COUNTER(PortRcvRemotePhysicalErrors);

	// Security
	CHECK_COUNTER(PortXmitConstraintErrors);
	CHECK_COUNTER(PortRcvConstraintErrors);

	// Routing or Down nodes still being sent to
	CHECK_COUNTER(PortRcvSwitchRelayErrors);
	CHECK_COUNTER(PortXmitDiscards);

	// Congestion
	CHECK_COUNTER(SwPortCongestion);
	CHECK_COUNTER(PortRcvFECN);
	CHECK_COUNTER(PortRcvBECN);
	CHECK_COUNTER(PortMarkFECN);
	CHECK_COUNTER(PortXmitTimeCong);
	CHECK_COUNTER(PortXmitWait);

	// Bubbles
	CHECK_COUNTER(PortXmitWastedBW);
	CHECK_COUNTER(PortXmitWaitData);
	CHECK_COUNTER(PortRcvBubble);
#undef CHECK_COUNTER

	return needclear;
}

// returns TRUE if thresholds are configured or clearall
boolean ShowClearedCounters(Format_t format, int indent, int detail, boolean clearall)
{
	boolean didoutput = FALSE;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sConfigured Counters to Clear:\n",indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<ClearedCounters>\n",indent, "");
		break;
	default:
		break;
	}
#define SHOW_COUNTER(field, name) \
	do { if (clearall || g_CounterSelectMask.CounterSelectMask.s.field) { switch (format) { case FORMAT_TEXT: printf("%*s%-30s\n", indent+4, "", #name); break; case FORMAT_XML: printf("%*s<%s></%s>\n", indent+4, "", #name, #name); break; default: break; } didoutput = TRUE; } }  while (0)

	// Data movement
	SHOW_COUNTER(PortXmitData, XmitData);
	SHOW_COUNTER(PortRcvData, RcvData);
	SHOW_COUNTER(PortXmitPkts, XmitPkts);
	SHOW_COUNTER(PortRcvPkts, RcvPkts);
	SHOW_COUNTER(PortMulticastXmitPkts, MulticastXmitPkts);
	SHOW_COUNTER(PortMulticastRcvPkts, MulticastRcvPkts);
	// Signal Integrity and Node/Link Stability
	// LinkQualityIndicator N/A since readonly can't clear
	SHOW_COUNTER(UncorrectableErrors, UncorrectableErrors);
	SHOW_COUNTER(LinkDowned, LinkDowned);
	SHOW_COUNTER(PortRcvErrors, RcvErrors);
	SHOW_COUNTER(ExcessiveBufferOverruns, ExcessiveBufferOverruns);
	SHOW_COUNTER(FMConfigErrors, FMConfigErrors);
	SHOW_COUNTER(LinkErrorRecovery, LinkErrorRecovery);
	SHOW_COUNTER(LocalLinkIntegrityErrors, LocalLinkIntegrityErrors);
	SHOW_COUNTER(PortRcvRemotePhysicalErrors, RcvRemotePhysicalErrors);
	// Security
	SHOW_COUNTER(PortXmitConstraintErrors, XmitConstraintErrors);
	SHOW_COUNTER(PortRcvConstraintErrors, RcvConstraintErrors);
	// Routing or Down nodes still being sent to
	SHOW_COUNTER(PortRcvSwitchRelayErrors, RcvSwitchRelayErrors);
	SHOW_COUNTER(PortXmitDiscards, XmitDiscards);
	// Congestion
	SHOW_COUNTER(SwPortCongestion, CongDiscards);
	SHOW_COUNTER(PortRcvFECN, RcvFECN);
	SHOW_COUNTER(PortRcvBECN, RcvBECN);
	SHOW_COUNTER(PortMarkFECN, MarkFECN);
	SHOW_COUNTER(PortXmitTimeCong, XmitTimeCong);
	SHOW_COUNTER(PortXmitWait, XmitWait);
	// Bubbles
	SHOW_COUNTER(PortXmitWastedBW, XmitWastedBW);
	SHOW_COUNTER(PortXmitWaitData, XmitWaitData);
	SHOW_COUNTER(PortRcvBubble, RcvBubble);

	switch (format) {
	case FORMAT_TEXT:
		if (! didoutput)
			printf("%*sNone\n", indent+4, "");
		break;
	case FORMAT_XML:
		printf("%*s</ClearedCounters>\n",indent, "");
		break;
	default:
		break;
	}

#undef SHOW_COUNTER
	return didoutput;
}

void ShowSlowLinkPortSummaryHeader(LinkReport_t report, Format_t format, int indent, int detail)
{
	ShowLinkBriefSummaryHeader(format, indent, detail);
	if (detail) {
		switch (format) {
		case FORMAT_TEXT:
		 	switch (report) {
			case LINK_EXPECTED_REPORT:
				printf("%*s Active                              Enabled\n", indent+4, "");
				printf("%*s Lanes, Used(Tx), Used(Rx), Rate,    Lanes,    DownTo,  Rates\n", indent+4, "");
			break;
			case LINK_CONFIG_REPORT:
				printf("%*s Enabled                        Supported\n", indent+4, "");
				printf("%*s Lanes,   Used,   Rate,         Lanes,    DownTo,  Rates\n", indent+4, "");
				break;
			case LINK_CONN_REPORT:
				printf("%*s Supported\n", indent+4, "");
				printf("%*s Lanes,       DownTo,      Rate\n", indent+4, "");
				break;
			}
			DisplaySeparator();
			break;
		case FORMAT_XML:
			break;
		default:
			break;
		}
	}
}

void ShowSlowLinkReasonSummary(LinkReport_t report, PortData *portp, Format_t format, int indent, int detail)
{
	char buf1[SHOW_BUF_SIZE], buf2[SHOW_BUF_SIZE], buf3[SHOW_BUF_SIZE], buf4[SHOW_BUF_SIZE];
    char buf5[SHOW_BUF_SIZE], buf6[SHOW_BUF_SIZE], buf7[SHOW_BUF_SIZE];

	switch (format) {
	case FORMAT_TEXT:
		switch (report) {
		case LINK_EXPECTED_REPORT:
			printf("%*s %-6s %-9s %-9s %-8s %-9s %-8s %-12s\n", indent, "",
				StlLinkWidthToText(portp->PortInfo.LinkWidth.Active, buf1, sizeof(buf1)),
                StlLinkWidthToText(portp->PortInfo.LinkWidthDowngrade.TxActive, buf5, sizeof(buf5)),
				StlLinkWidthToText(portp->PortInfo.LinkWidthDowngrade.RxActive, buf7, sizeof(buf7)),
				StlLinkSpeedToText(portp->PortInfo.LinkSpeed.Active, buf2, sizeof(buf2)),
				StlLinkWidthToText(portp->PortInfo.LinkWidth.Enabled, buf3, sizeof(buf3)),
                StlLinkWidthToText(portp->PortInfo.LinkWidthDowngrade.Enabled, buf6, sizeof(buf6)),
				StlLinkSpeedToText(portp->PortInfo.LinkSpeed.Enabled, buf4, sizeof(buf4)));
			break;
		case LINK_CONFIG_REPORT:
			printf("%*s %-8s %-7s %-13s %-9s %-8s %-12s\n", indent, "",
				StlLinkWidthToText(portp->PortInfo.LinkWidth.Enabled, buf1, sizeof(buf1)),
                StlLinkWidthToText(portp->PortInfo.LinkWidthDowngrade.Enabled, buf5, sizeof(buf5)),
				StlLinkSpeedToText(portp->PortInfo.LinkSpeed.Enabled, buf2, sizeof(buf2)),
				StlLinkWidthToText(portp->PortInfo.LinkWidth.Supported, buf3, sizeof(buf3)),
                StlLinkWidthToText(portp->PortInfo.LinkWidthDowngrade.Supported, buf6, sizeof(buf6)),
				StlLinkSpeedToText(portp->PortInfo.LinkSpeed.Supported, buf4, sizeof(buf4)));
			break;
		case LINK_CONN_REPORT:
			printf("%*s %-12s %-12s %-12s\n", indent, "",
				StlLinkWidthToText(portp->PortInfo.LinkWidth.Supported, buf1, sizeof(buf1)),
                StlLinkWidthToText(portp->PortInfo.LinkWidthDowngrade.Supported, buf5, sizeof(buf5)),
				StlLinkSpeedToText(portp->PortInfo.LinkSpeed.Supported, buf2, sizeof(buf2)));
			break;
		}
		break;
	case FORMAT_XML:
		switch (report) {
		case LINK_EXPECTED_REPORT:
			XmlPrintLinkWidth("LinkWidthActive",
				portp->PortInfo.LinkWidth.Active, indent);
			XmlPrintLinkWidth("LinkWidthDnGradeTxActive",
				portp->PortInfo.LinkWidthDowngrade.TxActive, indent);
			XmlPrintLinkWidth("LinkWidthDnGradeRxActive",
				portp->PortInfo.LinkWidthDowngrade.RxActive, indent);
			XmlPrintLinkSpeed("LinkSpeedActive",
				portp->PortInfo.LinkSpeed.Active, indent);
			XmlPrintLinkWidth("LinkWidthEnabled",
				portp->PortInfo.LinkWidth.Enabled, indent);
			XmlPrintLinkWidth("LinkWidthDnGradeEnabled",
				portp->PortInfo.LinkWidthDowngrade.Enabled, indent);
			XmlPrintLinkSpeed("LinkSpeedEnabled",
				portp->PortInfo.LinkSpeed.Enabled, indent);
			break;
		case LINK_CONFIG_REPORT:
			XmlPrintLinkWidth("LinkWidthEnabled",
				portp->PortInfo.LinkWidth.Enabled, indent);
			XmlPrintLinkWidth("LinkWidthDnGradeEnabled",
				portp->PortInfo.LinkWidthDowngrade.Enabled, indent);
			XmlPrintLinkSpeed("LinkSpeedEnabled",
				portp->PortInfo.LinkSpeed.Enabled, indent);
			XmlPrintLinkWidth("LinkWidthSupported",
				portp->PortInfo.LinkWidth.Supported, indent);
			XmlPrintLinkWidth("LinkWidthDnGradeSupported",
				portp->PortInfo.LinkWidthDowngrade.Supported, indent);
			XmlPrintLinkSpeed("LinkSpeedSupported",
				portp->PortInfo.LinkSpeed.Supported, indent);
			break;
		case LINK_CONN_REPORT:
			XmlPrintLinkWidth("LinkWidthSupported",
				portp->PortInfo.LinkWidth.Supported, indent);
			XmlPrintLinkWidth("LinkWidthDnGradeSupported",
				portp->PortInfo.LinkWidthDowngrade.Supported, indent);
			XmlPrintLinkSpeed("LinkSpeedSupported",
				portp->PortInfo.LinkSpeed.Supported, indent);
			break;
		}
		break;
	default:
		break;
	}
}

void ShowSlowLinkReasonSummaryCallback(uint64 context, PortData *portp,
									Format_t format, int indent, int detail)
{
	ShowSlowLinkReasonSummary((LinkReport_t)context, portp, format, indent, detail);
}

void ShowSlowLinkSummary(LinkReport_t report, PortData *portp1, Format_t format, int indent, int detail)
{
	ShowLinkFromBriefSummary(portp1,
					(uint64)(uintn)report, ShowSlowLinkReasonSummaryCallback,
					format, indent, detail);
	ShowLinkToBriefSummary(portp1->neighbor, "<-> ", TRUE,
					(uint64)(uintn)report, ShowSlowLinkReasonSummaryCallback,
					format, indent, detail);
}

void ShowLinkPortErrorSummaryCallback(uint64 context, PortData *portp,
									Format_t format, int indent, int detail)
{
	ShowLinkPortErrorSummary(portp, format, indent, detail);
}

// show link errors from portp1 to its neighbor
void ShowLinkErrorSummary(PortData *portp1, Format_t format, int indent, int detail)
{
	ShowLinkFromBriefSummary(portp1, 0, ShowLinkPortErrorSummaryCallback, format, indent, detail);

	ShowLinkToBriefSummary(portp1->neighbor, "<-> ", TRUE, 0, ShowLinkPortErrorSummaryCallback, format, indent, detail);
}

// output summary of Links for given report
void ShowLinksReport(Point *focus, report_t report, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count = 0;
	char *xml_prefix = "";
	char *prefix = "";

	switch (report) {
	default:	// should not happen, but just in case
		ASSERT(0);
	case REPORT_LINKS:
		xml_prefix = "";
		prefix = "";
		break;
	case REPORT_EXTLINKS:
		xml_prefix = "Ext";
		prefix = "External ";
		break;
	case REPORT_FILINKS:
		xml_prefix = "FI";
		prefix = "FI ";
		break;
	case REPORT_ISLINKS:
		xml_prefix = "IS";
		prefix = "Inter-Switch ";
		break;
	case REPORT_EXTISLINKS:
		xml_prefix = "ExtIS";
		prefix = "External Inter-Switch ";
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%sLink Summary\n", indent, "", prefix);
		break;
	case FORMAT_XML:
		printf("%*s<%sLinkSummary>\n", indent, "", xml_prefix);
		indent+=4;
		break;
	default:
		break;
	}

	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		switch (report) {
		default:	// should not happen, but just in case
		case REPORT_LINKS:
			printf("%*s%u Links in Fabric%s\n", indent, "", g_Fabric.LinkCount, detail?":":""); break;
		case REPORT_EXTLINKS:
			printf("%*s%u External Links in Fabric%s\n", indent, "", g_Fabric.ExtLinkCount, detail?":":""); break;
		case REPORT_FILINKS:
			printf("%*s%u FI Links in Fabric%s\n", indent, "", g_Fabric.FILinkCount, detail?":":""); break;
		case REPORT_ISLINKS:
			printf("%*s%u Inter-Switch Links in Fabric%s\n", indent, "", g_Fabric.ISLinkCount, detail?":":""); break;
		case REPORT_EXTISLINKS:
			printf("%*s%u External Inter-Switch Links in Fabric%s\n", indent, "", g_Fabric.ExtISLinkCount, detail?":":""); break;
		}
		break;
	case FORMAT_XML:
		switch (report) {
		default:	// should not happen, but just in case
		case REPORT_LINKS:
			XmlPrintDec("LinkCount", g_Fabric.LinkCount, indent); break;
		case REPORT_EXTLINKS:
			XmlPrintDec("ExternalLinkCount", g_Fabric.ExtLinkCount, indent); break;
		case REPORT_FILINKS:
			XmlPrintDec("FILinkCount", g_Fabric.FILinkCount, indent); break;
		case REPORT_ISLINKS:
			XmlPrintDec("ISLinkCount", g_Fabric.ISLinkCount, indent); break;
		case REPORT_EXTISLINKS:
			XmlPrintDec("ExternalISLinkCount", g_Fabric.ExtISLinkCount, indent); break;
		}
		break;
	default:
		break;
	}

	if (detail)
		ShowLinkBriefSummaryHeader(format, indent, detail-1);
	for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
		PortData *portp1 = (PortData *)QListObj(p);
		// to avoid duplicated processing, only process "from" ports in link
		if (! portp1->from)
			continue;

		switch (report) {
		default:	// should not happen, but just in case
		case REPORT_LINKS:
			break;	// always show
		case REPORT_EXTLINKS:
			if (isInternalLink(portp1))
				continue;
			break;
		case REPORT_FILINKS:
			if (! isFILink(portp1))
				continue;
			break;
		case REPORT_ISLINKS:
			if (! isISLink(portp1))
				continue;
			break;
		case REPORT_EXTISLINKS:
			if (isInternalLink(portp1))
				continue;
			if (! isISLink(portp1))
				continue;
			break;
		}

		if (! ComparePortPoint(portp1, focus) && ! ComparePortPoint(portp1->neighbor, focus))
			continue;
		count++;
		if (detail)
			ShowLinkBriefSummary(portp1, "<-> ", format, indent, detail-1);
	}

	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Links Found\n", indent, "", count);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingLinks", count, indent);
		indent-=4;
		printf("%*s</%sLinkSummary>\n", indent, "", xml_prefix);
		break;
	default:
		break;
	}
}

// output summary of all slow IB Links
// detail = 0,1 -> link running < best enabled speed/width
// detail = 2 -> links running < best supported speed/width
// detail = >2 -> links running < max supported speed/width
// one_report indicates if only a single vs stacked reports
//	(stacked means separate sections for each previous part of report)
void ShowSlowLinkReport(LinkReport_t report, boolean one_report, Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	int loops;
	int firstloop = 1;
	int loop;
	char *xmltag="";

	switch (report) {
	default:
	case LINK_EXPECTED_REPORT:
		loops = 1;
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sLinks running slower than expected Summary\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<LinksExpected> <!-- Links running slower than expected Summary -->\n", indent, "");
			xmltag="LinksExpected";
			break;
		default:
			break;
		}
		break;
	case LINK_CONFIG_REPORT:
		loops = 2;
		if (one_report) {
			firstloop = 2;
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks configured slower than supported Summary\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<LinksConfig> <!-- Links configured slower than supported Summary -->\n", indent, "");
				xmltag="LinksConfig";
				break;
			default:
				break;
			}
		} else {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks running slower than supported Summary\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<LinksRunningSupported> <!-- Links running slower than supported Summary -->\n", indent, "");
				xmltag="LinksRunningSupported";
				break;
			default:
				break;
			}
		}
		break;
	case LINK_CONN_REPORT:
		loops = 3;
		if (one_report) {
			firstloop = 3;
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks connected with mismatched supported speeds Summary\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<LinksMismatched> <!-- Links connected with mismatched supported speeds Summary -->\n", indent, "");
				xmltag="LinksMismatched";
				break;
			default:
				break;
			}
		} else {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks running slower than faster port Summary\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<LinksRunningSlower> <!-- Links running slower than faster port Summary -->\n", indent, "");
				xmltag="LinksRunningSlower";
				break;
			default:
				break;
			}
		}
		break;
	}
	if (format == FORMAT_XML)
		indent+=4;
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	if (format == FORMAT_XML)
		indent-=4;

	for (loop = firstloop; loop <= loops; ++loop) {
		uint32 badcount = 0;
		uint32 checked = 0;
		LinkReport_t loop_report;

		switch (loop) {
		case 1:
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks running slower than expected:\n", indent, "");
				break;
			case FORMAT_XML:
				/* already output above */
				break;
			default:
				break;
			}
			loop_report = LINK_EXPECTED_REPORT;
			break;
		case 2:
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks configured to run slower than supported:\n", indent, "");
				break;
			case FORMAT_XML:
				if (loop != firstloop) {
					printf("</%s>\n", xmltag);
					printf("%*s<LinksConfig> <!-- Links configured slower than supported Summary -->\n", indent, "");
					xmltag="LinksConfig";
				}
				break;
			default:
				break;
			}
			loop_report = LINK_CONFIG_REPORT;
			if (g_hard) {
				switch (format) {
				case FORMAT_TEXT:
					printf("%*sReport skipped: -H option specified\n", indent, "");
					break;
				case FORMAT_XML:
					printf("%*s<!-- Report skipped: -H option specified -->\n", indent, "");
					break;
				default:
					break;
				}
				continue;
			}
			break;
		case 3:
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks connected with mismatched speed potential:\n", indent, "");
				break;
			case FORMAT_XML:
				if (loop != firstloop) {
					printf("</%s>\n", xmltag);
					printf("%*s<LinksMismatched> <!-- Links connected with mismatched supported speeds Summary -->\n", indent, "");
					xmltag="LinksMismatched";
				}
				break;
			default:
				break;
			}
			loop_report = LINK_CONN_REPORT;
			break;
		default:
			ASSERT(0);
			continue;
			break;
		}
		if (format == FORMAT_XML)
			indent+=4;
		for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
			PortData *portp1, *portp2;
			STL_PORT_INFO *pi1, *pi2;

			portp1 = (PortData *)QListObj(p);
			// to avoid duplicated processing, only process "from" ports in link
			if (! portp1->from)
				continue;
			portp2 = portp1->neighbor;
			if (! ComparePortPoint(portp1, focus) && ! ComparePortPoint(portp2, focus))
				continue;
			checked++;
			pi1 = &portp1->PortInfo;
			pi2 = &portp2->PortInfo;

			// If the data is invalid, declare the test failed
			if ( /* Active speed, width, widthDowngrade should all be a single bit value */
				 /* - else is invalid data */
				 (pi1->LinkSpeed.Active != StlBestLinkSpeed(pi1->LinkSpeed.Active)) || 
				 (pi2->LinkSpeed.Active != StlBestLinkSpeed(pi2->LinkSpeed.Active)) ||
				 (pi1->LinkWidth.Active != StlBestLinkWidth(pi1->LinkWidth.Active)) ||
				 (pi2->LinkWidth.Active != StlBestLinkWidth(pi2->LinkWidth.Active)) ||
				 (pi1->LinkWidthDowngrade.TxActive != StlBestLinkWidth(pi1->LinkWidthDowngrade.TxActive)) ||
				 (pi2->LinkWidthDowngrade.TxActive != StlBestLinkWidth(pi2->LinkWidthDowngrade.TxActive)) ||
				 (pi1->LinkWidthDowngrade.RxActive != StlBestLinkWidth(pi1->LinkWidthDowngrade.RxActive)) ||
				 (pi2->LinkWidthDowngrade.RxActive != StlBestLinkWidth(pi2->LinkWidthDowngrade.RxActive)) ) {
				printf(" The speed, width, or widthdowngrade value retrieved is not valid. \n");
				goto show;
			}

			// Links running slower than expected (not at highest supported speed that is enabled)
			if (firstloop <= 1) {
				if ( 
					 /* Active speed should match highest speed enabled on both ports */
					 (pi1->LinkSpeed.Active == StlExpectedLinkSpeed(
						pi1->LinkSpeed.Enabled, pi2->LinkSpeed.Enabled)) &&
					 (pi2->LinkSpeed.Active == StlExpectedLinkSpeed(
						pi1->LinkSpeed.Enabled, pi2->LinkSpeed.Enabled)) &&

					 /* Actual width (the downgrade width) should match highest width enabled on both ports */				
					 (pi1->LinkWidthDowngrade.TxActive == StlExpectedLinkWidth(
						pi1->LinkWidthDowngrade.Enabled, pi2->LinkWidthDowngrade.Enabled)) &&
					 (pi2->LinkWidthDowngrade.TxActive == StlExpectedLinkWidth(
						pi1->LinkWidthDowngrade.Enabled, pi2->LinkWidthDowngrade.Enabled)) &&
					 (pi1->LinkWidthDowngrade.RxActive == StlExpectedLinkWidth(
						pi1->LinkWidthDowngrade.Enabled, pi2->LinkWidthDowngrade.Enabled)) &&
					 (pi2->LinkWidthDowngrade.RxActive == StlExpectedLinkWidth(
						pi1->LinkWidthDowngrade.Enabled, pi2->LinkWidthDowngrade.Enabled)) &&
					 /* Active width should match highest width enabled on both ports */
					 (pi1->LinkWidth.Active == StlExpectedLinkWidth(
						pi1->LinkWidth.Enabled, pi2->LinkWidth.Enabled)) &&
					 (pi2->LinkWidth.Active == StlExpectedLinkWidth(
						pi1->LinkWidth.Enabled, pi2->LinkWidth.Enabled)) &&

					 /* And then finally, Actual width (the downgrade width) should match the active width */
					 (pi1->LinkWidthDowngrade.TxActive == pi1->LinkWidth.Active) &&
					 (pi2->LinkWidthDowngrade.TxActive == pi2->LinkWidth.Active) &&
					 (pi1->LinkWidthDowngrade.RxActive == pi1->LinkWidth.Active) &&
					 (pi2->LinkWidthDowngrade.RxActive == pi2->LinkWidth.Active) ) {
					/* active matches the best enabled, cable is good */
					if (loop == 1)
						continue;
				} else {
					/* bad cable, active doesn't match best enabled */
					if (loop > 1)
						continue;	/* already reported on loop 1 */
					else
						goto show;
				}
			}

			// links configured to run slower than expected (not configured to highest speed supported)
			if (firstloop <= 2) {
				if (
					/* The highest supported speed should be what is configured as enabled */
					(StlBestLinkSpeed(pi1->LinkSpeed.Enabled) == StlExpectedLinkSpeed( 
					   pi1->LinkSpeed.Supported, pi2->LinkSpeed.Supported)) && 
					(StlBestLinkSpeed(pi2->LinkSpeed.Enabled) == StlExpectedLinkSpeed( 
					   pi1->LinkSpeed.Supported, pi2->LinkSpeed.Supported)) && 

					/* The highest supported widthdowngrade should be what is configured as enabled */
					(StlBestLinkWidth(pi1->LinkWidthDowngrade.Enabled) == StlExpectedLinkWidth( 
					   pi1->LinkWidthDowngrade.Supported, pi2->LinkWidthDowngrade.Supported)) && 
					(StlBestLinkWidth(pi2->LinkWidthDowngrade.Enabled) == StlExpectedLinkWidth( 
					   pi1->LinkWidthDowngrade.Supported, pi2->LinkWidthDowngrade.Supported)) && 

					/* The highest supported width should be what is configured as enabled */
					(StlBestLinkWidth(pi1->LinkWidth.Enabled) == StlExpectedLinkWidth(
					   pi1->LinkWidth.Supported, pi2->LinkWidth.Supported)) && 
					(StlBestLinkWidth(pi2->LinkWidth.Enabled) == StlExpectedLinkWidth(
					   pi1->LinkWidth.Supported, pi2->LinkWidth.Supported)) ) {

					/* configured matches the best supported, config is good */
					if (loop == 2)
						continue;
				} else {
					/* bad config, active doesn't match best supported */
					if (loop > 2)
						continue;	/* already reported on loop 2 */
					else
						goto show;
				}
			}

			// Link connected with mismatched speed potential (bi-directional link not symetric)
			if (firstloop <= 3) {
				if ( 
					/* Bidirectional speed and width should match */
					(StlBestLinkSpeed(pi1->LinkSpeed.Supported) == StlBestLinkSpeed(pi2->LinkSpeed.Supported)) &&
					(StlBestLinkWidth(pi1->LinkWidthDowngrade.Supported) == StlBestLinkWidth(pi2->LinkWidthDowngrade.Supported)) &&
					(StlBestLinkWidth(pi1->LinkWidth.Supported) == StlBestLinkWidth(pi2->LinkWidth.Supported)) ) {

					/* match, connection choice is good */
					if (loop == 3)
						continue;
				} else {
					/* bad config, active doesn't match best supported */
					if (loop > 3)
						continue;	/* already reported on loop 3 */
					else
						goto show;
				}
			}

			/* bad connection choice, active doesn't match best supported */
show:
			if (detail) {
				if (! badcount)
					ShowSlowLinkPortSummaryHeader(loop_report, format, indent, detail-1);
				ShowSlowLinkSummary(loop_report, portp1, format, indent, detail-1);
			}
			badcount++;
		}
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%u of %u Links Checked, %u Errors found\n", indent, "",
						checked, g_Fabric.LinkCount, badcount);
			break;
		case FORMAT_XML:
			XmlPrintDec("LinksChecked", checked, indent);
			XmlPrintDec("TotalLinks", g_Fabric.LinkCount, indent);
			XmlPrintDec("LinksWithErrors", badcount, indent);
			break;
		default:
			break;
		}
		if (format == FORMAT_XML)
			indent-=4;
	}
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		printf("</%s>\n", xmltag);
		break;
	default:
		break;
	}
}

void ShowRoutesReport(EUI64 portGuid, Point *point1, Point *point2, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sRoutes Summary Between:\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<RoutesSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	switch (format) {
	case FORMAT_TEXT:
		ShowPointBriefSummary(point1, FIND_FLAG_FABRIC, format, indent+4, detail);
		printf("and ");
		ShowPointBriefSummary(point2, FIND_FLAG_FABRIC, format, indent, detail);
		break;
	case FORMAT_XML:
		ShowPointBriefSummary(point1, FIND_FLAG_FABRIC, format, indent, detail);
		ShowPointBriefSummary(point2, FIND_FLAG_FABRIC, format, indent, detail);
		break;
	default:
		break;
	}
	if (format == FORMAT_TEXT)
		printf("\n");
	if (g_hard || g_persist) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: -H or -P option specified\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: -H or -P option specified -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}

	(void)ShowPointsTraceRoutes(portGuid, point1, point2, format, indent, detail);

done:
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</RoutesSummary>\n", indent, "");
		break;
	default:
		break;
	}
}

static FSTATUS ValidateCLTimeGetCallback(uint64_t *address, pthread_mutex_t *lock) 
{ 
   uint64_t	usecs; 
   struct	timespec now; 
   
   // the system routine clock_gettime is not thread safe, so
   // must request global lock.   
   pthread_mutex_lock(lock); 
   if (address == 0) {
      pthread_mutex_unlock(lock); 
      return (FINVALID_PARAMETER);
   }
   clock_gettime(CLOCK_MONOTONIC, &now);
   pthread_mutex_unlock(lock); 
   
   usecs = now.tv_sec; 
   usecs *= 1000000; 
   usecs += (now.tv_nsec / 1000); 
   // round up anything greater than 0.5 usecs
   if (now.tv_nsec % 1000 > 500) 
      usecs++; 
   
   *address = usecs; 
   
   return (FSUCCESS);
}

static void ValidateCLRouteCallback(PortData *portp1, PortData *portp2, void *context) 
{ 
   FSTATUS status; 
   
   Format_t format = FORMAT_TEXT; 
   int indent = 0; 
   int detail = 0; 
   PQUERY_RESULT_VALUES pQueryResults = NULL; 
   uint32 NumPathRecords; 
   IB_PATH_RECORD *pPathRecords = NULL; 
   ValidateCreditLoopRoutesContext_t *cp = (ValidateCreditLoopRoutesContext_t *) context;
   struct omgt_port *omgt_port_session = NULL;
   
   if (cp) {
      format = cp->format; 
      indent = cp->indent; 
      detail = cp->detail;
   }
   
   switch (format) {
   case FORMAT_TEXT:
      printf("%*sRoutes Summary Between:\n", indent, ""); 
      break; 
   case FORMAT_XML:
      printf("%*s<RoutesSummary>\n", indent, ""); 
      indent += 4; 
      break; 
   default:
      break;
   }
   switch (format) {
   case FORMAT_TEXT:
      ShowPointPortBriefSummary("Port: ", portp1, format, indent, detail); 
      printf("and "); 
      ShowPointPortBriefSummary("Port: ", portp2, format, indent, detail); 
      break; 
   case FORMAT_XML:
      ShowPointPortBriefSummary("Port: ", portp1, format, indent, detail); 
      ShowPointPortBriefSummary("Port: ", portp2, format, indent, detail); 
      break; 
   default:
      break;
   }
   if (format == FORMAT_TEXT) 
      printf("\n"); 
   
   if (g_hard || g_persist) {
      switch (format) {
      case FORMAT_TEXT:
         printf("%*sReport skipped: -H or -P option specified\n", indent, ""); 
         break; 
      case FORMAT_XML:
         printf("%*s<!-- Report skipped: -H or -P option specified -->\n", indent, ""); 
         break; 
      default:
         break;
      }
      goto done;
   }
   if (!(g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
      switch (format) {
      case FORMAT_TEXT:
         printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, ""); 
         break; 
      case FORMAT_XML:
         printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, ""); 
         break; 
      default:
         break;
      }
      goto done;
   }
   
   switch (format) {
   case FORMAT_TEXT:
      printf("%*sRoutes between ports:\n", indent, ""); 
      ShowLinkPortBriefSummary(portp1, "    ", 0, NULL, format, indent, 0); 
      ShowLinkPortBriefSummary(portp2, "and ", 0, NULL, format, indent, 0); 
      break; 
   case FORMAT_XML:
      printf("%*s<PortRoutes>\n", indent, ""); 
      ShowLinkPortBriefSummary(portp1, "    ", 0, NULL, format, indent + 4, 0); 
      ShowLinkPortBriefSummary(portp2, "and ", 0, NULL, format, indent + 4, 0); 
      break; 
   default:
      break;
   }
   
   if (!g_snapshot_in_file) {
	   struct omgt_params params = {.debug_file = g_verbose > 2 ? stdout : NULL};
	   status = omgt_open_port_by_guid(&omgt_port_session, g_portGuid, &params);
	   if (FSUCCESS != status) 
		   goto done; 
	   status = GetPaths(omgt_port_session, portp1, portp2, &pQueryResults); 
	   if (FSUCCESS != status) 
		   goto done; 
	   NumPathRecords = ((PATH_RESULTS *)pQueryResults->QueryResult)->NumPathRecords; 
	   pPathRecords = ((PATH_RESULTS *)pQueryResults->QueryResult)->PathRecords;
   } else {
      status = GenPaths(&g_Fabric, portp1, portp2, &pPathRecords, &NumPathRecords); 
      if (FSUCCESS != status) 
         goto done;
   }
   
   switch (format) {
   case FORMAT_TEXT:
      printf("%*s%d Paths\n", indent, "", NumPathRecords); 
      break; 
   case FORMAT_XML:
      printf("%*s<Paths>%d</Paths>\n", indent + 4, "", NumPathRecords); 
      break; 
   default:
      break;
   }
   if (detail) {
      int i; 
      
      for (i = 0; i < NumPathRecords; i++) {
         ShowTraceRoute(g_portGuid, portp1, portp2, &pPathRecords[i], format, indent + 4, detail - 1);
      }
   }
   
done:
   if (format == FORMAT_XML) 
      printf("%*s</PortRoutes>\n", indent, ""); 
   
   if (pQueryResults) 
      omgt_free_query_result_buffer(pQueryResults); 
   if (omgt_port_session != NULL)
	   omgt_close_port(omgt_port_session);
   if (g_snapshot_in_file && pPathRecords) 
      MemoryDeallocate(pPathRecords); 
   
   switch (format) {
   case FORMAT_TEXT:
      DisplaySeparator(); 
      break; 
   case FORMAT_XML:
      indent -= 4; 
      printf("%*s</RoutesSummary>\n", indent, ""); 
      break; 
   default:
      break;
   }
}

static void ValidateCLFabricSummaryCallback(FabricData_t *fabricp, const char *name,
                                            uint32 totalPaths, uint32 totalBadPaths,
                                            void *context) 
{
   ValidateCreditLoopRoutesContext_t *cp = (ValidateCreditLoopRoutesContext_t *) context;
   int indent = cp->indent;
   char title[100]={0};
    
   if (!cp->detail) 
      return; 
   
   switch (cp->format) {
   case FORMAT_TEXT:
      /*printf("%*s%s summary: %d devices, %d HFIs, %d switches, %d connections, %d routing decisions, %d analyzed routes, %d incomplete routes\n", 
             indent, "", 
             name, (int)cl_qmap_count(&fabricp->map_guid_to_ib_device), QListCount(&fabricp->FIs), QListCount(&fabricp->Switches), 
             fabricp->ConnectionCount, fabricp->RouteCount, totalPaths, totalBadPaths);*/ // DEBUG_CODE --- ??? 
      printf("%*s%s summary: %d devices, %d HFIs, %d switches,\n", 
             indent, "", 
             name, (int)cl_qmap_count(&fabricp->map_guid_to_ib_device), QListCount(&fabricp->FIs), QListCount(&fabricp->Switches)); 
      printf("%*s\t%d connections, %d routing decisions,\n", 
             indent, "", 
             fabricp->ConnectionCount, fabricp->RouteCount); 
      printf("%*s\t%d analyzed routes, %d incomplete routes\n", 
             indent, "", 
             totalPaths, totalBadPaths); 
      break; 
   case FORMAT_XML:
      snprintf(title, sizeof(title), "CreditLoop%sSummary", name);
      XmlPrintTagHeader(title, indent);
      indent+= 4;    
      XmlPrintDec("Devices", cl_qmap_count(&fabricp->map_guid_to_ib_device), indent); 
      XmlPrintDec("HFIs", QListCount(&fabricp->FIs), indent); 
      XmlPrintDec("Switches", QListCount(&fabricp->Switches), indent); 
      XmlPrintDec("Connections", fabricp->ConnectionCount, indent); 
      XmlPrintDec("RoutingDecisions", fabricp->RouteCount, indent);
      XmlPrintDec("AnalyzedRoutes", totalPaths, indent);        
      XmlPrintDec("IncompleteRoutes", totalBadPaths, indent);        
      snprintf(title, sizeof(title), "CreditLoop%sSummary", name);
      XmlPrintTagFooter(title, indent-4);
      break; 
   default:
      break;
   } 
}

static void ValidateCLDataSummaryCallback(clGraphData_t *graphp, const char *name, void *context) 
{
   ValidateCreditLoopRoutesContext_t *cp = (ValidateCreditLoopRoutesContext_t *) context;
   int indent = cp->indent;
   char title[100]={0};
    
   if (!cp->detail) 
      return; 
   
   switch (cp->format) {
   case FORMAT_TEXT:
      printf("%*s%s summary: %d vertices and %d arcs\n",
             indent, "",
             name, graphp->NumActiveVertices, (int)cl_qmap_count(&graphp->Arcs));
      break; 
   case FORMAT_XML:
      snprintf(title, sizeof(title), "CreditLoop%sSummary", name);
      printf("\n");
      XmlPrintTagHeader(title, indent);
      indent+= 4;    
      XmlPrintDec("Vertices", graphp->NumActiveVertices, indent); 
      XmlPrintDec("Arcs", cl_qmap_count(&graphp->Arcs), indent); 
      snprintf(title, sizeof(title), "CreditLoop%sSummary", name);
      XmlPrintTagFooter(title, indent-4);
      break; 
   default:
      break;
   } 
}

static void ValidateCLRouteSummaryCallback(uint32 routesPresent,
                                           uint32 routesMissing,
                                           uint32 hopsHistogramEntries,
                                           uint32 *hopsHistogram,
                                           void *context) 
{
   ValidateCreditLoopRoutesContext_t *cp = (ValidateCreditLoopRoutesContext_t *) context;
   int indent = cp->indent;
   uint32 hh; 
    
   if (!cp->detail) 
      return; 
   
   switch (cp->format) {
   case FORMAT_TEXT:
      printf("%*sRouting summary: %d routes present, %d routes missing\n",
             indent, "", routesPresent, routesMissing); 
      
      //PYTHON: for h in hopsHistogram :
      for (hh = 0; hh < hopsHistogramEntries; hh++) {
         if (hopsHistogram[hh]) 
            printf("%*sHops summary: %d routes have %d hops\n", indent, "", hopsHistogram[hh], hh);
      }
      break; 
   case FORMAT_XML:
      printf("\n");
      XmlPrintTagHeader("CreditLoopRoutingSummary", indent);
      indent+= 4;
          
      XmlPrintTagHeader("RoutingSummary", indent);
      XmlPrintDec("RoutesPresent", routesPresent, indent+4); 
      XmlPrintDec("RoutesMissing", routesMissing, indent+4); 
      XmlPrintTagFooter("RoutingSummary", indent);
 
      //PYTHON: for h in hopsHistogram :
      for (hh = 0; hh < hopsHistogramEntries; hh++) {
         if (hopsHistogram[hh]) {
            printf("\n");
            XmlPrintTagHeader("HopsSummary", indent);
            XmlPrintDec("Routes", hopsHistogram[hh], indent+4); 
            XmlPrintDec("Hops", hh, indent+4); 
            XmlPrintTagFooter("HopsSummary", indent);
         }
      }
      XmlPrintTagFooter("CreditLoopRoutingSummary", indent-4);
      break; 
   default:
      break;
   }
}

static void ValidateCLLinkSummaryCallback(uint32 id, const char *name, uint32 cycle, uint8 header, int indent, void *context) 
{ 
   ValidateCreditLoopRoutesContext_t *cp = (ValidateCreditLoopRoutesContext_t *) context;

   switch (cp->format) {
   case FORMAT_TEXT:
      if (header)
         printf("%*sLINK %d (%s) is in a CYCLE of %d:\n", indent, "", id, name, cycle);
      break; 
      
   case FORMAT_XML:
      if (header) {
         printf("\n");
         XmlPrintTagHeader("LinkSummary", indent);
         XmlPrintDec("LinkCycle", cycle, indent+4); 
         XmlPrintDec("LinkId", id, indent+4); 
         XmlPrintStr("LinkName", name, indent+4); 
      } else {
         XmlPrintTagFooter("LinkSummary", indent);
      }
      break; 
      
   default:
      break;
   }
}

static void ValidateCLLinkStepSummaryCallback(uint32 id, const char *name, uint32 step, uint8 header, int indent, void *context) 
{ 
   ValidateCreditLoopRoutesContext_t *cp = (ValidateCreditLoopRoutesContext_t *) context;

   switch (cp->format) {
   case FORMAT_TEXT:
      if (header) {
         if (cp->detail >= 4)
             printf("%*s----------------------------------------\n", indent, ""); 
         printf("%*s%d: LINK %d (%s)\n", indent, "", step, id, name);
      }
      break; 
      
   case FORMAT_XML:
      if (header) {
         printf("\n");
         XmlPrintTagHeader("LinkStepSummary", indent);
         XmlPrintDec("LinkStep", step, indent+4); 
         XmlPrintDec("LinkId", id, indent+4); 
         XmlPrintStr("LinkName", name, indent+4); 
      } else {
         XmlPrintTagFooter("LinkStepSummary", indent);
      }
      break; 
      
   default:
      break;
   }
}

static void ShowCLPathRecord(IB_PATH_RECORD *pPathRecord, Format_t format, int indent) 
{ 
   char buf[8];    
   
   switch (format) {
   case FORMAT_TEXT:
      printf("%*sSGID: 0x%016"PRIx64":%016"PRIx64"\n", 
             indent, "", 
             pPathRecord->SGID.Type.Global.SubnetPrefix, 
             pPathRecord->SGID.Type.Global.InterfaceID); 
      printf("%*sDGID: 0x%016"PRIx64":%016"PRIx64"\n", 
             indent, "", 
             pPathRecord->DGID.Type.Global.SubnetPrefix, 
             pPathRecord->DGID.Type.Global.InterfaceID); 
      printf("%*sSLID: 0x%.*x DLID: 0x%.*x Reversible: %s PKey: 0x%04x\n",
             indent, "", (pPathRecord->SLID <= IB_MAX_UCAST_LID ? 4:8), pPathRecord->SLID,
	     (pPathRecord->DLID <= IB_MAX_UCAST_LID ? 4:8), pPathRecord->DLID,
             pPathRecord->Reversible ? "Y" : "N", pPathRecord->P_Key);
      printf("%*sRaw: %s FlowLabel: 0x%05x HopLimit: 0x%02x TClass: 0x%02x\n",
             indent, "", pPathRecord->u1.s.RawTraffic ? "Y" : "N",
             pPathRecord->u1.s.FlowLabel, pPathRecord->u1.s.HopLimit,
             pPathRecord->TClass);
      FormatTimeoutMult(buf, pPathRecord->PktLifeTime);
      printf("%*sSL: %2d Mtu: %5s Rate: %4s PktLifeTime: %s Pref: %d\n", 
             indent, "", pPathRecord->u2.s.SL, IbMTUToText(pPathRecord->Mtu), 
             StlStaticRateToText(pPathRecord->Rate), buf, 
             pPathRecord->Preference); 
      break;
       
   case FORMAT_XML:
      XmlPrintTagHeader("PathRecord", indent);
      XmlPrintGID("SGID", pPathRecord->SGID, indent + 4); 
      XmlPrintGID("DGID", pPathRecord->DGID, indent + 4); 
      XmlPrintLID("SLID", pPathRecord->SLID, indent + 4); 
      XmlPrintLID("DLID", pPathRecord->DLID, indent + 4); 
      XmlPrintStr("Reversible", pPathRecord->Reversible ? "Y" : "N", indent + 4); 
      XmlPrintDec("Reversible_Int", pPathRecord->Reversible, indent + 4); 
      XmlPrintPKey("PKey", pPathRecord->P_Key, indent + 4); 
      XmlPrintStr("Raw", pPathRecord->u1.s.RawTraffic ? "Y" : "N", indent + 4); 
      XmlPrintDec("Raw_Int", pPathRecord->u1.s.RawTraffic, indent + 4); 
      XmlPrintHex("FlowLabel", pPathRecord->u1.s.FlowLabel, indent + 4); 
      XmlPrintHex8("HopLimit", pPathRecord->u1.s.HopLimit, indent + 4); 
      XmlPrintHex8("TClass", pPathRecord->TClass, indent + 4); 
      XmlPrintDec("SL", pPathRecord->u2.s.SL, indent + 4); 
      XmlPrintDec("Mtu", GetBytesFromMtu(pPathRecord->Mtu), indent + 4); 
      XmlPrintRate(pPathRecord->Rate, indent + 4); 
      FormatTimeoutMult(buf, pPathRecord->PktLifeTime); 
      XmlPrintStr("PktLifeTime", buf, indent + 4); 
      XmlPrintDec("PktLifeTime_Int", pPathRecord->PktLifeTime, indent + 4); 
      XmlPrintDec("Preference", pPathRecord->Preference, indent + 4); 
      XmlPrintTagFooter("PathRecord", indent);
      break;
       
   default:
      break;
   }
}

static void ShowCLTraceRecord(FabricData_t *fabricp, STL_TRACE_RECORD *pTraceRecord, uint32 record, Format_t format, int indent) 
{ 
   NodeData *nodep; 
   
   switch (format) {
   case FORMAT_TEXT:
      printf("%*sTrace Record[%d]", indent, "", record); 
      indent += 4;      
      printf("%*sIDGeneration: 0x%04x\n", 
             indent, "", pTraceRecord->IDGeneration); 
      printf("%*sNodeType: 0x%02x\n", 
             indent, "", pTraceRecord->NodeType); 
      printf("%*sNodeID: 0x%016"PRIx64" ChassisID: %016"PRIx64"\n", 
             indent, "", pTraceRecord->NodeID, pTraceRecord->ChassisID); 
      printf("%*sEntryPortID: 0x%016"PRIx64" ExitPortID: %016"PRIx64"\n", 
             indent, "", pTraceRecord->EntryPortID, pTraceRecord->ExitPortID); 
      printf("%*sEntryPort: 0x%02x ExitPort: 0x%02x\n", 
             indent, "", pTraceRecord->EntryPort, pTraceRecord->ExitPort); 
      if ((nodep = FindNodeGuid(fabricp, pTraceRecord->NodeID))) 
         printf("%*sNodeDesc: %s\n", indent, "", nodep->NodeDesc.NodeString); 
      break;
       
   case FORMAT_XML:
      XmlPrintTagHeader("TraceRecord", indent);
      // TraceRecordId not a standard trace record field.  The field was
      // added to assist the person analyzing this data.
      XmlPrintDec("TraceRecordId", record, indent + 4); 
      XmlPrintHex16("IDGeneration", pTraceRecord->IDGeneration, indent + 4); 
      XmlPrintNodeType(pTraceRecord->NodeType, indent + 4); 
      XmlPrintHex64("NodeID", pTraceRecord->NodeID, indent + 4); 
      XmlPrintHex64("ChassisID", pTraceRecord->ChassisID, indent + 4); 
      XmlPrintHex64("EntryPortID", pTraceRecord->EntryPortID, indent + 4); 
      XmlPrintHex64("ExitPortID", pTraceRecord->ExitPortID, indent + 4); 
      XmlPrintHex8("EntryPort", pTraceRecord->EntryPort, indent + 4); 
      XmlPrintHex8("ExitPort", pTraceRecord->ExitPort, indent + 4); 
      XmlPrintTagFooter("TraceRecord", indent);
      break;
       
   default:
      break;
   }
}

static void ValidateCLPathSummaryCallback(FabricData_t *fabricp, clConnData_t *connp, int indent, void *context) 
{
   FSTATUS status; 
   ValidateCreditLoopRoutesContext_t *cp = (ValidateCreditLoopRoutesContext_t *) context;
   int i, xmlFmt = (cp->format == 1) ? 1 : 0;   
   uint32 NumTraceRecords; 
   STL_TRACE_RECORD	*pTraceRecords = NULL; 
   PQUERY_RESULT_VALUES pQueryResults = NULL; 
   struct omgt_port *omgt_port_session = NULL;

   //      
   // display path record associated with the route       
   if (xmlFmt)
      XmlPrintTagHeader("Route", indent);
   else
      printf("%*sPath Record\n", indent, ""); 
   ShowCLPathRecord(&connp->PathInfo.path, cp->format, indent+4);
    
   //    
   // retrieve trace records associated with the route       
   if (!((fabricp->flags & FF_ROUTES) && g_snapshot_in_file)) {
	   struct omgt_params params = {.debug_file = g_verbose > 2 ? stdout : NULL};
	   if(omgt_open_port_by_guid(&omgt_port_session, g_portGuid, &params))
		  return;
	   if (GetTraceRoute(omgt_port_session, &connp->PathInfo.path, &pQueryResults)) {
		   omgt_close_port(omgt_port_session);
		   return;
	   }
	   NumTraceRecords = ((STL_TRACE_RECORD_RESULTS *)pQueryResults->QueryResult)->NumTraceRecords; 
	   pTraceRecords = ((STL_TRACE_RECORD_RESULTS *)pQueryResults->QueryResult)->TraceRecords;
   } else {
      if ((status = GenTraceRoutePath(fabricp, &connp->PathInfo.path, &pTraceRecords, &NumTraceRecords))) {
         if (status == FUNAVAILABLE) {
            fprintf(stderr, "opareport: Routing Tables not available in snapshot\n"); 
            goto done;
         } else if (status == FNOT_DONE) {
            if (!xmlFmt)
               printf("%*sRoute Incomplete\n", indent, ""); 
            else
               printf("%*s<LinksTraversed>0</LinksTraversed> <!-- Route Incomplete -->\n", indent, ""); 
            // don't fail just because some routes are incomplete
         } else {
            fprintf(stderr, "opareport: Unable to determine route: (status=0x%x): %s\n", status, iba_fstatus_msg(status));
            if (status == FINSUFFICIENT_MEMORY)
               exit(0); 
         }
         goto done;
      }
   }

   //    
   // display trace records associated with the route       
   printf("\n");
   if (xmlFmt) 
      XmlPrintDec("TraceRecords", NumTraceRecords, indent+4); 
   else
      printf("%*sTrace Records (%d)\n", indent, "", NumTraceRecords);       
   for (i = 0; i < NumTraceRecords;/* i++*/) {
      ShowCLTraceRecord(fabricp, &pTraceRecords[i], i, cp->format, indent+4);
      if (++i < NumTraceRecords)
         printf("\n");
   }
    
   if (xmlFmt)
      XmlPrintTagFooter("Route", indent);
   else
      printf("\n");

done:
   if (pQueryResults) 
	  omgt_free_query_result_buffer(pQueryResults); 
   if (omgt_port_session != NULL)
	   omgt_close_port(omgt_port_session);
}

// output summary of all IB Links with errors > threshold
void ShowLinkErrorReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count = 0;
	uint32 checked = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sLinks with errors %s threshold Summary\n", indent, "",
				g_threshold_compare?">=":">");
		break;
	case FORMAT_XML:
		printf("%*s<LinkErrors> <!-- Links with errors %s threshold Summary -->\n", indent, "",
				g_threshold_compare?">=":">");
		indent+=4;
		break;
	default:
		break;
	}
	if (g_hard || g_persist) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: -H or -P option specified\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: -H or -P option specified -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	} else if (! (g_Fabric.flags & FF_STATS) && ! g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: -s nor -X option not specified\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: -s nor -X option not specified -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	} else if (! (g_Fabric.flags & FF_STATS) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -s option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -s option -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	if (g_limitstats) {
		switch (format) {
		case FORMAT_TEXT:
			printf("Check limited, will not check neighbor ports\n");
			break;
		case FORMAT_XML:
			printf("<!-- Check limited, will not check neighbor ports -->\n");
			break;
		default:
			break;
		}
	}
	if (! ShowThresholds(format, indent, detail)) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: no thresholds configured\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: no thresholds configured -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}

	for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
		PortData *portp1, *portp2;

		portp1 = (PortData *)QListObj(p);
		// to avoid duplicated processing, only process "from" ports in link
		if (! portp1->from)
			continue;
		portp2 = portp1->neighbor;
		// if g_limitstats is set, we will not have PortCounters for
		// ports outside our focus, so we will not report nor check them
		if (! ComparePortPoint(portp1, focus) && ! ComparePortPoint(portp2, focus))
			continue;
		checked++;
		if (PortCountersExceedThreshold(portp1) || PortCountersExceedThreshold(portp2))
		{
			if (detail) {
				if (count) {
					if (format == FORMAT_TEXT) {
						printf("\n");	// blank line between links
					}
				} else {
					ShowLinkBriefSummaryHeader(format, indent, detail-1);
				}
				ShowLinkErrorSummary(portp1, format, indent, detail-1);
			}
			count++;
		}
	}
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u of %u Links Checked, %u Errors found\n", indent, "",
					checked, g_Fabric.LinkCount, count);
		break;
	case FORMAT_XML:
		XmlPrintDec("LinksChecked", checked, indent);
		XmlPrintDec("TotalLinks", g_Fabric.LinkCount, indent);
		XmlPrintDec("LinksWithErrors", count, indent);
		break;
	default:
		break;
	}

done:
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</LinkErrors>\n", indent, "");
		break;
	default:
		break;
	}
}

/* clear all PortCounters on all ports in fabric
 */
FSTATUS ClearAllPortCountersAndShow(EUI64 portGuid, Point *focus, boolean clearall, Format_t format, boolean quiet)
{
	uint32 node_count=0;
	uint32 port_count=0;
	uint32 nrsp_node_count=0;
	uint32 nrsp_port_count=0;
	int indent = 0;

	if (! quiet) {
		switch (format) {
		case FORMAT_TEXT:
			printf("Clearing Port Counters\n");
			break;
		case FORMAT_XML:
			printf("<ClearCounters>\n");
			indent+=4;
			break;
		default:
			break;
		}
		ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, 0);
		if (g_limitstats) {
			switch (format) {
			case FORMAT_TEXT:
				printf("Clear limited, will not clear neighbor ports\n");
				break;
			case FORMAT_XML:
				printf("<!-- Clear limited, will not clear neighbor ports -->\n");
				break;
			default:
				break;
			}
		}
		if (! ShowClearedCounters(format, indent, 10, clearall)) {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sClear skipped: no thresholds configured\n", 0, "");
				break;
			case FORMAT_XML:
				printf("%*s<!-- Clear skipped: no thresholds configured -->\n", 0, "");
				break;
			default:
				break;
			}
			goto done;
		}
	} else if (! clearall && ! NeedClearCounters()) {
		goto done;
	}

	(void)ClearAllPortCounters(portGuid, g_portAttrib->GIDTable[0], &g_Fabric, focus,
							clearall?0xffffffff:g_CounterSelectMask.CounterSelectMask.AsReg32,
							g_limitstats, g_quiet, &node_count, &port_count,
							&nrsp_node_count, &nrsp_port_count);
	if (! quiet) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sCleared %u Ports on %u Nodes\n", indent, "", port_count, node_count);
			if (nrsp_port_count)
				printf("%*sUnable to clear %u Ports on %u Nodes\n", indent, "", nrsp_port_count, nrsp_node_count);
			break;
		case FORMAT_XML:
			XmlPrintDec("ClearedPorts", port_count, indent);
			XmlPrintDec("ClearedNodes", node_count, indent);
			if (nrsp_port_count) {
				XmlPrintDec("NoRespPorts", nrsp_port_count, indent);
				XmlPrintDec("NoRespNodes", nrsp_node_count, indent);
			}
			indent-=4;
			printf("</ClearCounters>\n");
			break;
		default:
			break;
		}
	} else {
		PROGRESS_PRINT(TRUE, "Cleared %u Ports on %u Nodes\n", port_count, node_count);
		if (nrsp_port_count)
			PROGRESS_PRINT(TRUE, "Unable to clear %u Ports on %u Nodes\n", nrsp_port_count, nrsp_node_count);
	}

	done:
	return FSUCCESS;	// TBD
}

// output summary of all LIDs
void ShowAllLIDReport(Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *pMap;
	NodeData *nodep;
	PortData *portp;
	PortSelector* portselp;
	uint8 lmc_range = 0;				// (2 ** lmc) - 1
	uint32 ct_lid;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sLID Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<LIDSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u LID(s) in Fabric%s\n", indent, "",g_Fabric.lidCount, detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("FabricLIDCount", g_Fabric.lidCount, indent);
		break;
	default:
		break;
	}
	// Report LID Summary
	ct_lid = 0;
	for ( pMap=cl_qmap_head(&g_Fabric.u.AllLids);
			pMap != cl_qmap_end(&g_Fabric.u.AllLids);
			pMap = cl_qmap_next(pMap) ) {
		portp = PARENT_STRUCT(pMap, PortData, AllLidsEntry);
		portselp = GetPortSelector(portp);
		nodep = portp->nodep;

		if (!ComparePortPoint(portp, focus))
			continue;
		lmc_range = (1 << portp->PortInfo.s1.LMC) - 1;

		if (detail) {
			if (!ct_lid) {
				switch (format) {
				case FORMAT_TEXT:
					printf("%*s   LID(Range) NodeGUID          Port Type Name\n", indent, "");
					if (g_topology_in_file)
						printf("%*s              NodeDetails          PortDetails\n", indent, "");
					break;
				case FORMAT_XML:
					break;
				default:
					break;
				}
			}
			switch (format) {
			case FORMAT_TEXT:
				printf( "%*s0x%.*x", indent, "", (portp->PortInfo.LID <= IB_MAX_UCAST_LID ? 4:8),
					portp->PortInfo.LID);
				if (!lmc_range)
					printf("       ");
				else
					printf( "-0x%.*x", ((portp->PortInfo.LID + lmc_range) <= IB_MAX_UCAST_LID ? 4:8),
						portp->PortInfo.LID + lmc_range );
				printf( " 0x%016"PRIx64" %3u %s %s\n",
					nodep->NodeInfo.NodeGUID, portp->PortNum,
					StlNodeTypeToText(nodep->NodeInfo.NodeType),
					(char*)nodep->NodeDesc.NodeString );
				if (nodep->enodep && nodep->enodep->details)
					printf( "%*s              %s\n",
						indent, "", nodep->enodep->details );
				if (portselp && portselp->details)
					printf( "%*s                                   %s\n",
						indent, "", portselp->details );
				break;
			case FORMAT_XML:
				if (!ct_lid)
					printf("%*s<LIDs>\n", indent, "");
				printf( "%*s<Value LID=\"0x%.*x\">\n", indent+4, "",
					(portp->PortInfo.LID <= IB_MAX_UCAST_LID ? 4:8),
					portp->PortInfo.LID );
				if (lmc_range)
					XmlPrintLID( "EndLID",
						portp->PortInfo.LID + lmc_range, indent+8 );
				XmlPrintHex64( "NodeGUID",
						nodep->NodeInfo.NodeGUID, indent+8 );
				XmlPrintDec("PortNum", portp->PortNum, indent+8);
				if (portselp && portselp->details)
					XmlPrintStr("PortDetails", portselp->details, indent+8);
				XmlPrintStr( "NodeType",
					StlNodeTypeToText(nodep->NodeInfo.NodeType),
					indent+8 );
				XmlPrintStr( "NodeDesc",
					(char*)nodep->NodeDesc.NodeString, indent+8 );
				if (nodep->enodep && nodep->enodep->details)
					XmlPrintStr("NodeDetails", nodep->enodep->details, indent+8);
				printf("%*s</Value>\n", indent+4, "");
				break;
			default:
				break;
			}
		}	// End of if (detail)
		ct_lid = ct_lid + lmc_range + 1;
	}	// End of for ( pMap=cl_qmap_head(&g_Fabric.AllLids)


	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Reported LID(s)\n", indent, "", ct_lid);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		if (detail && ct_lid)
			printf("%*s</LIDs>\n", indent, "");
		XmlPrintDec("ReportedLIDCount", ct_lid, indent);
		indent-=4;
		printf("%*s</LIDSummary>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowAllLIDReport()

// output linear FDB
void ShowLinearFDBReport(Point *focus, Format_t format, int indent, int detail)
{
	int ix_lid;
	LIST_ITEM *pList;
	cl_map_item_t *pMap;
	NodeData *nodep;
	SwitchData *switchp;
	PortData *portp;
	PortSelector* portselp;
	uint32 ct_node = 0;
	uint32 ct_lid;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sLinear FDB Tables\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<LinearFDBs>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u Connected Switches in Fabric%s\n", indent, "",
			(unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}

	// Report Linear FDB
	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {
		nodep = QListObj(pList);
		switchp = nodep->switchp;

		if (!CompareNodePoint(nodep, focus))
			continue;

		if (!switchp || !switchp->LinearFDB)
			continue;

		if (detail) {
			if (!ct_node) {
				ShowNodeBriefSummaryHeadings(format, indent, 0);
				switch (format) {
				case FORMAT_TEXT:
					printf("%*s      Egress      Neighbor\n", indent+4, "");
					printf("%*s  LID Port       Port   Name\n", indent+4, "");
					if (g_topology_in_file)
						printf("%*s         PortDetails NodeDetails\n", indent+4, "");
					break;
				case FORMAT_XML:
					break;
				default:
					break;
				}
			}
			ShowNodeBriefSummary(nodep, focus, FALSE, format, indent, 0);
			ct_lid = 0;
			for (ix_lid = 0; ix_lid < switchp->LinearFDBSize; ix_lid++) {
				if (STL_LFT_PORT_BLOCK(switchp->LinearFDB, ix_lid) == 0xFF)
					continue;

				switch (format) {
				case FORMAT_TEXT:
					printf( "%*s0x%04x %3u", indent+4, "", (uint32)ix_lid, STL_LFT_PORT_BLOCK(switchp->LinearFDB, ix_lid) );
					break;
				case FORMAT_XML:
					if (!ct_lid++)
						printf("%*s<LinearFDB>\n", indent+4, "");
					printf( "%*s<Value LID=\"0x%.*x\">\n", indent+8, "", (ix_lid <= IB_MAX_UCAST_LID ? 4:8),
					(uint16)ix_lid );
					XmlPrintDec("EgressPort", STL_LFT_PORT_BLOCK(switchp->LinearFDB, ix_lid), indent+12);
					break;
				default:
					break;
				}
				for ( pMap=cl_qmap_head(&nodep->Ports);
						pMap != cl_qmap_end(&nodep->Ports);
						pMap = cl_qmap_next(pMap) ) {
					portp = PARENT_STRUCT(pMap, PortData, NodePortsEntry);
					if (!portp)
						continue;

					if (portp->PortNum == STL_LFT_PORT_BLOCK(switchp->LinearFDB, ix_lid)) {
						portselp = GetPortSelector(portp);
						switch (format) {
						case FORMAT_TEXT:
							if (portp->neighbor) {
								printf( "       %3u %s",
									portp->neighbor->PortNum, g_noname?g_name_marker:
									(char*)portp->neighbor->nodep->NodeDesc.NodeString );
								if ( portp->neighbor->nodep->enodep &&
										portp->neighbor->nodep->enodep->details )
									printf( "\n%*s                     %s",
										indent+4, "", portp->neighbor->nodep->enodep->details );
							}

							if (portselp && portselp->details)
								printf( "\n%*s         %s",
									indent+4, "", portselp->details );
							break;
						case FORMAT_XML:
							if (portselp && portselp->details)
								XmlPrintStr("PortDetails", portselp->details, indent+12);
							if (portp->neighbor) {
								XmlPrintDec("NeighborPort", portp->neighbor->PortNum, indent+12);
								XmlPrintStr("NeighborNodeDesc",
									g_noname?g_name_marker:
									(char *)portp->neighbor->nodep->NodeDesc.NodeString, indent+12);
								if ( portp->neighbor->nodep->enodep &&
										portp->neighbor->nodep->enodep->details )
									XmlPrintStr("NeighborNodeDetails", portp->neighbor->nodep->enodep->details, indent+12);
							}
							printf("%*s</Value>\n", indent+8, "");
							break;
						default:
							break;
						}
						break;

					}	// End of if (portp->PortNum == switchp->LinearFDB[ix_lid])

				}	// End of for ( pMap=cl_qmap_head(&nodep->Ports)

				switch (format) {
				case FORMAT_TEXT:
					printf("\n");
					break;
				case FORMAT_XML:
					break;
				default:
					break;
				}

			}	// End of for (ix_lid = 0; ix_lid < switchp->LinearFDBSize

			switch (format) {
			case FORMAT_XML:
				if (ct_lid)
					printf("%*s</LinearFDB>\n", indent+4, "");
				printf("%*s</Node>\n", indent, "");
				break;
			default:
				break;
			}

		}	// End of if (detail)

		ct_node++;

	}	// End of for (pList=QListHead(&g_Fabric.AllSWs); pList != NULL

done:
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Reported Switch(es)\n", indent, "", ct_node);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedSwitchCount", (unsigned)ct_node, indent);
		indent-=4;
		printf("%*s</LinearFDBs>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowLinearFDBReport()

// Used by ShowPGReport.
static uint32 ones64(uint64 x)
{
	x -= ((x >> 1) & 0x5555555555555555ULL);
	x = (((x >> 2) & 0x3333333333333333ULL) + (x & 0x3333333333333333ULL));
	x = (((x >> 4) + x) & 0x0f0f0f0f0f0f0f0fULL);
	x += (x >> 8);
	x += (x >> 16);
	x += (x >> 32);
	return(x & 0x0000003f);
}

// Information about port groups
void ShowPGReport(Point *focus, Format_t format, int indent, int detail)
{
	int ix_lid;
	LIST_ITEM *pList;
	NodeData *nodep;
	SwitchData *switchp;
	uint16 pgCount;
	uint32 ct_node = 0;

	/* Statistics block */
	uint32 overallSwitchCount	= 0;
	uint32 overallGroupCount	= 0;
	uint32 overallMaxGroupSize	= 0;
	uint32 overallMinGroupSize	= 256;
	uint32 overallSumGroupSize	= 0;
	uint32 overallMaxGroupCount = 0;
	uint32 overallMinGroupCount = 256;
	uint32 overallMaxLidsGroup  = 0;
	uint32 overallMinLidsGroup  = 0xffffffff;
	uint32 overallSumLidsGroup  = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sPort Groups\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<PortGroups>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}

	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}

	overallSwitchCount = QListCount(&g_Fabric.AllSWs);
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u Connected Switches in Fabric%s\n", indent, "",
			overallSwitchCount, detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("ConnectedSwitchCount", overallSwitchCount, indent);
		break;
	default:
		break;
	}

	// Summary data about Port Group Tables
	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {

		/* Statistics block */
		uint32 maxGroupSize  = 0;
		uint32 minGroupSize  = 256;
		uint32 sumGroupSize  = 0;
		uint32 maxLidsGroup  = 0;
		uint32 minLidsGroup  = 0xffffffff;
		uint32 sumLidsGroup  = 0;
		uint32 LidsGroup[256];

		memset(LidsGroup,0,sizeof(LidsGroup));

		nodep = QListObj(pList);
		switchp = nodep->switchp;

		if (!CompareNodePoint(nodep, focus))
			continue;

		if (!switchp || !switchp->PortGroupElements || !switchp->PortGroupFDB)
			continue;

		if (nodep->pSwitchInfo && 
			nodep->pSwitchInfo->SwitchInfoData.AdaptiveRouting.s.Enable &&
			nodep->pSwitchInfo->SwitchInfoData.PortGroupTop != 0) {
			pgCount = nodep->pSwitchInfo->SwitchInfoData.PortGroupTop;
		} else {
			pgCount = 0;
		}

		overallGroupCount   += pgCount;
		overallMaxGroupCount = MAX(overallMaxGroupCount, pgCount);
		overallMinGroupCount = MIN(overallMinGroupCount, pgCount);

		switch (format) {
		case FORMAT_XML:
			printf("%*s<PortGroupSummary>\n", indent, "");
			break;
		default:
			break;
		}

		if (!ct_node) {
			ShowNodeBriefSummaryHeadings(format, indent, 0);
		}

		if (format == FORMAT_TEXT) {
			printf("\n");
		}

		ShowNodeBriefSummary(nodep, focus, FALSE, format, indent, 0);
		if (format == FORMAT_TEXT && detail > 2) {
			printf("%*s          Group : Ports\n",indent+4,"");
		}

		if (pgCount == 0) {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*s(no groups)\n", indent+4, "");
				break;
			case FORMAT_XML:
				break;
			default:
				break;
			}
		} else {
			for (ix_lid = 0; 
				switchp->PortGroupElements && ix_lid < pgCount;
				ix_lid++) {
				uint32 bitCount = ones64((uint64_t)switchp->PortGroupElements[ix_lid]);

				maxGroupSize = MAX(maxGroupSize, bitCount);
				minGroupSize = MIN(minGroupSize, bitCount);
				sumGroupSize += bitCount;
					
				if (detail > 2) {
					uint64_t msk;
					int j;
					if (format == FORMAT_TEXT) {
						printf("%*s            %3d : ", indent+4, "", 
							ix_lid);
						if (switchp->PortGroupElements[ix_lid]==0) {
							printf("None");
						} else for (msk = 1, j = 0;j < 64;msk<<= 1, j++) {
							if (switchp->PortGroupElements[ix_lid] & msk) {
								printf("%d, ", j+1);
							}
						}
						printf("\n");
					} else if (format == FORMAT_XML) {
						printf( "%*s<Value ID=\"%d\">\n", indent+4, "", 
							ix_lid);
						XmlPrintHex64("PortGroup", 
							switchp->PortGroupElements[ix_lid], 
							indent+8);
						printf("%*s</Value>\n", indent+4, "");
					} 
				}
			}

			overallMaxGroupSize = MAX(maxGroupSize,overallMaxGroupSize);
			overallMinGroupSize = MIN(minGroupSize,overallMinGroupSize);
			overallSumGroupSize += sumGroupSize;

			if (detail > 2 && format == FORMAT_TEXT) {
				printf("\n");
			}
			switch (format) {
			case FORMAT_TEXT:
				printf("%*s     Num Groups : %d\n", indent+4, "", pgCount);
				printf("%*sMax Ports/Group : %d\n", indent+4, "", maxGroupSize);
				printf("%*sMin Ports/Group : %d\n", indent+4, "", minGroupSize);
				printf("%*sAvg Ports/Group : %.1f\n", indent+4, "", 
					(float)sumGroupSize/(float)pgCount);
				break;
			case FORMAT_XML:
				XmlPrintDec("NumGroups", pgCount, indent+4);
				XmlPrintDec("MaxGroupSize",maxGroupSize, indent+4);
				XmlPrintDec("MinGroupSize",minGroupSize, indent+4);
				break;
			}	
		}

		if (pgCount && switchp->PortGroupFDB) {
			uint8 g; 
			
			for (ix_lid = 1; ix_lid < switchp->LinearFDBSize; ix_lid++) {
				g = STL_PGFT_PORT_BLOCK(switchp->PortGroupFDB,ix_lid);
				LidsGroup[g]++;
			}
		
			for (g=0; g < pgCount; g++) {
				sumLidsGroup += LidsGroup[g];
				maxLidsGroup = MAX(maxLidsGroup,LidsGroup[g]);
				minLidsGroup = MIN(minLidsGroup,LidsGroup[g]);
			}

			overallSumLidsGroup += sumLidsGroup;
			overallMaxLidsGroup = MAX(maxLidsGroup,overallMaxLidsGroup);
			overallMinLidsGroup = MIN(minLidsGroup,overallMinLidsGroup);

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s Max Lids/Group : %d\n", indent+4, "", maxLidsGroup);
				printf("%*s Min Lids/Group : %d\n", indent+4, "", minLidsGroup);
				printf("%*s Avg Lids/Group : %.1f\n", indent+4, "", 
					(float)sumLidsGroup/(float)pgCount);
				break;
			case FORMAT_XML:
				XmlPrintDec("MaxLidsGroup",maxLidsGroup, indent+4);
				XmlPrintDec("MinLidsGroup",minLidsGroup, indent+4);
				break;
			}	

			if (detail > 2) {
				uint32_t currGroup = 0xFFFF;
				STL_LID startLid = 0;

				switch (format) {
				case FORMAT_TEXT:
					printf("%*s----  Lid Range  ---- Group #\n", indent+4, "");
					break;
				case FORMAT_XML:
					printf("%*s<PortGroupForwardingTable>\n", indent+4, "");
					break;
				default:
					break;
				}
			
				for (ix_lid = 1; ix_lid < switchp->LinearFDBSize; ix_lid++) {
					g = STL_PGFT_PORT_BLOCK(switchp->PortGroupFDB,ix_lid);
					if (format == FORMAT_XML) {
						if (g == 0xFF)
						continue;

						printf( "%*s<Value LID=\"0x%.*x\">\n", indent+8, "",
							(ix_lid <= IB_MAX_UCAST_LID ? 4:8), ix_lid );
						XmlPrintHex("PortGroup", g, indent+12);
						printf("%*s</Value>\n", indent+8, "");
					} else {
						if (g == currGroup)
						continue;
						if (currGroup != 0xFFFF) {
							if (currGroup != 0xFF) {
								printf("%*s0x%08x - %08x %3u\n", indent+4, "",
									startLid, ix_lid-1, currGroup);
							} else {
								printf("%*s0x%08x - %08x (none)\n", 
									indent+4, "", startLid, ix_lid-1);
							}
						}
						currGroup = g;
						startLid = ix_lid;
					}
				}	

				switch (format) {
				case FORMAT_XML:
					printf("%*s</PortGroupForwardingTable>\n", indent+4, "");
					break;
				default:
					if (currGroup != 0xFF) {
						printf("%*s0x%08x - %08x %3u\n", indent+4, "",
							startLid, ix_lid-1, currGroup);
					} else {
						printf("%*s0x%08x - %08x (none)\n", indent+4, "", 
							startLid, ix_lid-1);
					}
					break;
				}
			}
		}

		switch (format) {
		case FORMAT_XML:
			printf("%*s</Node>\n", indent, "");
			printf("%*s</PortGroupSummary>\n", indent, "");
			break;
		default:
			break;
		}

		ct_node++;

	}	// End of for (pList=QListHead(&g_Fabric.AllSWs); pList != NULL

	if (overallGroupCount != 0) {
		switch (format) {
		case FORMAT_TEXT:
			printf("\n%*sSummary:\n", indent, "");
			printf("%*s     Num Groups : %d\n", indent+4, "", 
				overallGroupCount);
			printf("%*sMax Ports/Group : %d\n", indent+4, "", 
				overallMaxGroupSize);
			printf("%*sMin Ports/Group : %d\n", indent+4, "", 
				overallMinGroupSize);
			printf("%*sAvg Ports/Group : %.1f\n", indent+4, "", 
				(float)overallSumGroupSize/(float)overallGroupCount);
			printf("%*s Max Lids/Group : %d\n", indent+4, "", 
				overallMaxLidsGroup);
			printf("%*s Min Lids/Group : %d\n", indent+4, "", 
				overallMinLidsGroup);
			printf("%*s Avg Lids/Group : %.1f\n", indent+4, "", 
				(float)overallSumLidsGroup/(float)overallGroupCount);
			break;
		case FORMAT_XML:
			break;
		}	
	} else if (format == FORMAT_TEXT) {
		printf("\n%*sNo port groups found.\n",indent,"");
	}

done:
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Reported Switch(es)\n", indent, "", ct_node);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedSwitchCount", (unsigned)ct_node, indent);
		indent-=4;
		printf("%*s</PortGroups>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowPGReport()

// Used by PGRouteHop to print out the current node when a adaptive routing 
// error is detected. Since PGRouteHop is recursive, this has the effect
// of printing out each hop in the failed route.
//
// nodep - the current node. 
// lid - the lid of the current node.
// length - the recursive depth/hop count/path length at this point.
static void POP_NODEDATA(NodeData *nodep, STL_LID lid, 
	uint32_t length, Format_t format, int indent) 
{
	switch(format) {
	case FORMAT_XML:
		printf("%*s<Hop Value=\"%d\">\n",indent+4,"", length);
		XmlPrintHex64( "NodeGUID", nodep->NodeInfo.NodeGUID, indent+8 );
		printf("%*s</Hop>\n",indent+4,"");
		break;
	default:
		printf("%*sHop %d: 0x%016"PRIx64", (LID 0x%x)\n",indent+4,"", \
			length, nodep->NodeInfo.NodeGUID, lid); \
		break;
	}
}

// Note that 128 is an arbitrary limit, but it will be able to 
// cope with a 2D mesh/torus that's 64 switches in each dimension,
// which is more than twice the # of LIDs available in STL gen1.
#define MAX_HOPS 128

// Used by ShowValidatePGReport. 
// olid = originating lid (original slid)
// dlid = destination lid
// nodep = current hop
// length = hop count so far.
// mhops = maximum # of hops we are allowed to traverse.
// lftonly = ignore the port group forwarding table
//
// Returns the total hopcount or -1.
//
static int32_t PGRouteHop(STL_LID olid, NodeData *nodep, STL_LID dlid,
	int32_t length, int32_t mhops, int32_t lftonly,
	Format_t format, int indent)
{
	PortData *portp; 
	SwitchData *switchp;
	uint8_t ep;
	uint8_t pg;
	NodeData *nnodep;
	PortData *nportp;
	uint32_t i;
	int32_t pl1, pl2;
	STL_LID slid;
	STL_PORTMASK pgm;
	
	// Add the link that was traversed to get here:
	length++;

	// If we've already tested this dlid from this switch, 
	// don't do it again.
	if (((uint8_t*)nodep->context)[dlid] != 0) {
		return length+((uint8_t*)nodep->context)[dlid];
	}

	portp = FindNodePort(nodep,0);
	if (!portp) {
		printf("%*sUnable to find port 0\n", indent, "");
		return -1;
	}
	slid = portp->PortInfo.LID;
	switchp = nodep->switchp;

	if (dlid == slid) {
		// Inability to route to yourself is checked at the 
		// top level, so we can just return success here.
		return length;
	}
				
	// Have we exceeded the maximum # of hops permitted?
	if (length > mhops) { 
		switch(format) {
		case FORMAT_XML:
			printf("%*s<ARError Value=\"HopsExceeded\">\n",indent,"");
			XmlPrintDec("SLID",olid,indent+4);
			XmlPrintDec("DLID",dlid,indent+4);
			break;
		default:
			printf("%*sERROR: Path from 0x%x to 0x%x exceeds normal path length or max hops.\n",
				indent, "", olid, dlid);
			break;
		}
		POP_NODEDATA(nodep, slid, length, format, indent);
		return -1;
	}

	ep = STL_LFT_PORT_BLOCK(switchp->LinearFDB,dlid); 
	pg = STL_PGFT_PORT_BLOCK(switchp->PortGroupFDB,dlid); 

	//
	// if LFT[dlid] == 0xff then we think this dlid is unused,
	// which is inconsistent with the parent switch.
	//
	if (ep == 0xff) {
		printf("%*sNo path to LID 0x%x\n",
			indent, "", dlid);
		POP_NODEDATA(nodep, slid, length, format, indent);
		return -1;
	}

	// Find our LFT neighbor node.
	portp = FindNodePort(nodep,ep);
	if (!portp) {
		printf("%*sUnable to find port %d\n",
			indent, "", ep);
		return -1;
	}
	nportp = portp->neighbor;
	nnodep = nportp->nodep;

	// If our neighbor isn't a switch, then the route better
	// terminate and PGFT[dlid] should be 0xff.
	if (nnodep->NodeInfo.NodeType != STL_NODE_SW) {
		STL_LID mask = ~0 << nportp->PortInfo.s1.LMC;
		if ((dlid & mask) != (nportp->PortInfo.LID & mask)) {
			switch(format) {
			case FORMAT_XML:
				printf("%*s<ARError Value=\"BadTermination\">\n",indent,"");
				XmlPrintDec("SLID",olid,indent+4);
				XmlPrintDec("DLID",dlid,indent+4);
				break;
			default:
				printf("%*sERROR: Path from 0x%x to 0x%x terminates at the wrong "
					"device:\n",
					indent, "", olid, dlid);
				break;
			}
			POP_NODEDATA(nnodep, nportp->PortInfo.LID, length+1, format, indent);
			POP_NODEDATA(nodep, slid, length, format, indent);
			return -1;
		} else if (pg != 0xff) {
			switch(format) {
			case FORMAT_XML:
				printf("%*s<ARError Value=\"BadMembership\">\n",indent,"");
				XmlPrintDec("SLID",olid,indent+4);
				XmlPrintDec("DLID",dlid,indent+4);
				break;
			default:
				printf("%*sERROR: LFT Path from 0x%x to 0x%x terminates but is also "
					"in the PGFT: (1)\n",
					indent, "", olid, dlid);
				break;
			}
			POP_NODEDATA(nnodep, nportp->PortInfo.LID, length+1, format, indent);
			POP_NODEDATA(nodep, slid, length, format, indent);
			return -1;
		}
		// Everything checks out. Include the egress link in our count.
		((uint8_t*)nodep->context)[dlid]=1;
		return length+1; //success.
	}

	// Test the LFT route for this DLID. There is another report that
	// does this, but we want the hop count to compare the PGFT entries 
	// against.
	pl1 = PGRouteHop(slid, nnodep, dlid, length, mhops, 1, format, indent);
	if (pl1 < 0) {
		POP_NODEDATA(nodep, slid, length, format, indent);
		return pl1;
	} else if (lftonly != 0) {
		// done.
		return pl1;
	}

	// If the DLID is in the PGFT, then test the route from this switch to
	// DLID via each port in the matching port group.
	if (pg != 0xff) for (i=1, pgm = switchp->PortGroupElements[pg];
		pgm != 0; pgm>>=1, i++) {
		if (pgm & 1 && i != ep) {
			// We only test a port if (a) it is a member of the port group and
			// (b) it is not the lft egress port (we already checked that).
			// TODO Can we avoid revisiting the same node in the tier0 case?
			portp = FindNodePort(nodep,i);
			if (!portp) {
				printf("%*sUnable to find port %d\n",
					indent, "", i);
				return -1;
			}
			nportp = portp->neighbor;
			nnodep = nportp->nodep;

			if (nnodep->NodeInfo.NodeType != STL_NODE_SW) {
				STL_LID mask = ~0 << nportp->PortInfo.s1.LMC;
				if ((dlid & mask) != (nportp->PortInfo.LID & mask)) {
					switch(format) {
					case FORMAT_XML:
						printf("%*s<ARError Value=\"BadTermination\">\n",indent,"");
						XmlPrintDec("SLID",olid,indent+4);
						XmlPrintDec("DLID",dlid,indent+4);
						break;
					default:
						printf("%*sERROR: AR Path from 0x%x to 0x%x terminates at the "
							"wrong device:\n", indent, "", olid, dlid);
						break;
					}
				} else {
					// Even if this is the right device, we still have 
					// a problem.
					switch(format) {
					case FORMAT_XML:
						printf("%*s<ARError Value=\"BadMembership\">\n",indent,"");
						XmlPrintDec("SLID",olid,indent+4);
						XmlPrintDec("DLID",dlid,indent+4);
						break;
					default:
						printf("%*sERROR: AR Path from 0x%x to 0x%x terminates but LFT "
							"path does not: (2)\n",
							indent, "", olid, dlid);
						break;
					}
				}
				POP_NODEDATA(nnodep, nportp->PortInfo.LID, length+1, format, 
					indent);
				POP_NODEDATA(nodep, slid, length, format, indent);
				return -1;
			}

			// Calculate the length of the path from here to dlid
			// via nnodep. It should be the same as the LFT path length.
			pl2 = PGRouteHop(slid, nnodep, dlid, length, mhops, 0, format, indent);
			if (pl2 < 0) {
				// There was a problem further along. Just pop our
				// position in the path and return.
				POP_NODEDATA(nodep, slid, length, format, indent);
				return -1;
			} else if (pl2 != pl1) {
				// This path was either shorter or longer than
				// the linear path. Either way, that's a problem.
				switch(format) {
				case FORMAT_XML:
					printf("%*s<ARError Value=\"InconsistentHopCount\">\n",indent,"");
					XmlPrintDec("SLID",olid,indent+4);
					XmlPrintDec("DLID",dlid,indent+4);
					XmlPrintDec("PL1",pl1-1,indent+4);
					XmlPrintDec("PL2",pl2-1,indent+4);
					break;
				default:
					printf("%*sERROR: Paths from 0x%016"PRIx64", (LID 0x%x)"
						" to LID 0x%x have inconsistent hop counts: %d vs %d\n",
						indent, "",
						nodep->NodeInfo.NodeGUID, slid, dlid, pl1-1, pl2-1);
						break;
				}
				POP_NODEDATA(nnodep, nportp->EndPortLID, length, 
					format, indent);
				return -1;
			}
		}
	}
	
	((uint8_t*)nodep->context)[dlid]=pl1 - length;
	return pl1;
}

// Used by ShowValidatePGReport.
static int compare_masks(const void *a, const void *b)
{
	STL_PORTMASK *pm1 = (STL_PORTMASK*)a;
	STL_PORTMASK *pm2 = (STL_PORTMASK*)b;

	if (*pm1<*pm2) return -1;
	if (*pm1>*pm2) return 1;
	return 0;
}

void ShowValidatePGReport(Format_t format, int indent, int detail)
{
	LIST_ITEM *pList;
	uint32_t routeCount = 0;
	uint32_t ct_node = 0;

	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		return;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sValidate Port Groups\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<ValidatePortGroups>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u LID(s) in Fabric%s\n", indent, "",
			(unsigned)cl_qmap_count(&g_Fabric.u.AllLids), detail?":":"" );
		printf("%*s%u Connected HFIs in Fabric%s\n", indent, "", 
			(unsigned)QListCount(&g_Fabric.AllFIs), detail?":":"");
		printf( "%*s%u Connected Switch(es) in Fabric%s\n", indent, "",
			(unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("FabricLIDCount", 
			(unsigned)cl_qmap_count(&g_Fabric.u.AllLids), indent);
		XmlPrintDec("ConnectedHFICount", 
			(unsigned)QListCount(&g_Fabric.AllFIs), indent);
		XmlPrintDec("ConnectedSwitchCount", 
			(unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}

	// Allocate buffers for optimizing the search.
	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {
		NodeData *nodep = (NodeData*)QListObj(pList);
		STL_SWITCHINFO_RECORD *switchp = nodep->pSwitchInfo;

		assert(switchp);

		nodep->context = MemoryAllocate2AndClear(
			switchp->SwitchInfoData.LinearFDBTop,
			IBA_MEM_FLAG_PREMPTABLE, MYTAG);

		assert(nodep->context);
	}

	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {
		NodeData *nodep = (NodeData*)QListObj(pList);
		SwitchData *switchp = nodep->switchp;
		STL_PORTMASK pgt[MAX_PGT_ELEMENTS];
		STL_PORTMASK pgo;
		int i;
		int pgCount = 0;
		int dupPGFound = 0;

		if (!ct_node) {
			ShowNodeBriefSummaryHeadings(format, indent, 0);
			ct_node++;
		}
		ShowNodeBriefSummary(nodep, NULL, FALSE, format, indent, 0);

		if (nodep->pSwitchInfo &&
			nodep->pSwitchInfo->SwitchInfoData.AdaptiveRouting.s.Enable &&
			nodep->pSwitchInfo->SwitchInfoData.PortGroupTop != 0) {
			pgCount = nodep->pSwitchInfo->SwitchInfoData.PortGroupTop;
		} else if (nodep->pSwitchInfo && nodep->pSwitchInfo->SwitchInfoData.AdaptiveRouting.s.Enable == 0) {
			if (format == FORMAT_XML) printf("%*s</Node>\n", indent, "");
			else printf("%*sAdaptive Routing Disabled.\n", indent+4, "");
			continue;
		} else if (nodep->pSwitchInfo && nodep->pSwitchInfo->SwitchInfoData.PortGroupTop == 0) {
			if (format == FORMAT_XML) printf("%*s</Node>\n", indent, "");
			else printf("%*sNo port groups defined.\n", indent+4, "");
			continue;
		} else {
			if (format != FORMAT_XML) printf("%*sNo switch info.\n", indent+4, "");
		}

		// The first test is to check for duplicate port groups.
		memcpy(pgt, switchp->PortGroupElements, 
			pgCount*sizeof(STL_PORTMASK));
		
		qsort(pgt,pgCount,sizeof(STL_PORTMASK),compare_masks);

		pgo=pgt[0];
		for (i=1;i<pgCount; i++) {
			if (pgo == pgt[i]) {
				switch (format) {
				case FORMAT_XML:
					XmlPrintHex64("DuplicatePortGroup",(EUI64)pgo, indent+4);
					break;
				default:
					printf("%*sDuplicate port groups with value 0x%016llx.\n",
						indent, "",
						(long long unsigned int)pgo);
					break;
				}
				dupPGFound = 1;
			}
			pgo = pgt[i];
		}
		if (dupPGFound) {
			if (format != FORMAT_XML) {
				printf("%*sPort Groups:\n",indent,"");
			} else {
				printf("%*s<PortGroups>\n",indent+4,"");
			}
			for (i=0; i<pgCount; i++) {
				switch (format) {
				case FORMAT_XML:
					printf("%*s<PortGroup Id=%d>\n",indent+8,"",i);
					XmlPrintHex64( "Value",
						switchp->PortGroupElements[i], indent+12);
					printf("%*s</PortGroup>\n",indent+8,"");
					break;
				default:
					printf("%*sPG %3d : 0x%016llx\n",indent+4,"",i,
						(long long unsigned int)switchp->PortGroupElements[i]);
					break;
				}
			}
			if (format == FORMAT_XML) {
				printf("%*s</PortGroups>\n",indent+4,"");
			}
		}

		//
		// Now validate all routes from this switch.
		//
		{
			PortData *portp = FindNodePort(nodep,0);
			if (!portp) {
				printf("%*sUnable to find port 0\n",
					indent, "");
				return;
			}				
			STL_LID slid = portp->PortInfo.LID;
			STL_LID dlid;
			uint32_t lidCount;
			uint32_t arOkay;
			uint32_t arCount;

			if (nodep->pSwitchInfo && 
				nodep->pSwitchInfo->SwitchInfoData.LinearFDBTop != 0) {
				lidCount = nodep->pSwitchInfo->SwitchInfoData.LinearFDBTop;
			} else {
				lidCount = switchp->LinearFDBSize;
			}

			if (detail>2) {
				switch (format) {
				case FORMAT_XML:
					printf("%*s<AdaptiveRoutes>\n",indent+4,"");
					break;
				default:
					printf("%*sRoute List:\n", indent, "");
					printf("%*sGuid               - Lid        - Hops - Alts\n", indent+4, "");
					break;
				}
			}

			// Check all DLIDs, even the unused ones.
			for(dlid = 1; dlid < lidCount; dlid++) {
				uint8_t ep = STL_LFT_PORT_BLOCK(switchp->LinearFDB,dlid); 
				uint8_t pg = STL_PGFT_PORT_BLOCK(switchp->PortGroupFDB,dlid); 
				NodeData *nnodep;
				PortData *nportp;
				int32_t pl1, pl2;
				STL_PORTMASK pgm;

				arOkay = 1;
				arCount = 0;
	
				//
				// Make sure the switch correctly routes to itself.
				// LFT[dlid] should be zero and PGFT[dlid] should be 0xff.
				//
				if (dlid == slid) {
					if (ep != 0) switch (format) {
					case FORMAT_XML:
						printf("%*s<ARError Value=\"Port0Error\" />\n",
							indent,"");
						break;
					default:
						printf("%*sERROR: Switch cannot route to itself.\n",
							indent, "");
						break;
					}
					if (pg != 0xff) switch (format) {
					case FORMAT_XML:
						printf("%*s<ARError Value=\"Port0ARError\" />\n",
							indent,"");
						break;
					default:
						printf("%*sERROR: Switch is in its own port group "
							"forwarding table (LID 0x%x).\n",
							indent, "", dlid);
					}
					continue;
				}
				
				//
				// if the dlid is not in use, it should not be in the PGFT.
				//
				if (ep == 0xff) {
					if (pg != 0xff) switch (format) {
					case FORMAT_XML:
						printf("%*s<ARError Value=\"BadDLID\" />\n",
							indent,"");
						break;
					default:
						printf("%*sERROR: LID 0x%x is in the PGFT but not the LFT.\n",
							indent, "", dlid);
					}
					continue; // dlid is not in use.
				} else if (ep == 0) {
					continue; // FIXME MWHEINZ extra LID going to management card? LMC?
				}

				// Find our LFT neighbor node.
				portp = FindNodePort(nodep,ep);
				if (!portp) {
					printf("%*sUnable to find port %d\n",
						indent, "", ep);
					return;
				}
				nportp = portp->neighbor;
				nnodep = nportp->nodep;

				// If our neighbor isn't a switch, then the route better
				// terminate and PGFT[dlid] should be 0xff.
				if (nnodep->NodeInfo.NodeType != STL_NODE_SW) {
					STL_LID mask = ~0 << nportp->PortInfo.s1.LMC;
					if ((dlid & mask) != (nportp->PortInfo.LID & mask)) {
						switch(format) {
						case FORMAT_XML:
							printf("%*s<ARError Value=\"BadTermination\">\n",
								indent,"");
							XmlPrintDec("DLID",dlid,indent+4);
							printf("%*s</ARError>\n",indent,"");
							break;
						default:
							printf("%*sPath to 0x%x terminates at the "
							"wrong device:\n", indent, "", dlid);
							break;
						}
					} else if (pg != 0xff) {
						switch(format) {
						case FORMAT_XML:
							printf("%*s<ARError Value=\"BadMembership\">\n",indent,"");
							XmlPrintDec("SLID",slid,indent+4);
							XmlPrintDec("DLID",dlid,indent+4);
							break;
						default:
							printf("%*sERROR: LFT path from 0x%x to 0x%x terminates but is also "
								"in the PGFT. (3)\n",
								indent, "", slid, dlid);
							break;
						}
					}
					pl1 = 1;
				} else {

					// Test the LFT route for this DLID. There is another report 
					// that does this, but we want the hop count to compare the 
					// PGFT entries against.
					pl1 = PGRouteHop(slid, nnodep, dlid, 0, MAX_HOPS, 1, format, indent);
					if (pl1 < 0) {
						if (format==FORMAT_XML) {
							printf("%*s</ARError>\n",indent,"");
						}
						continue; 
					}

					// If the DLID is in the PGFT, then test the route from this 
					// switch to DLID via each port in the matching port group.
					if (pg != 0xff) for (i=1, pgm = switchp->PortGroupElements[pg];
						pgm != 0; pgm>>=1, i++) {
						if (pgm & 1) {
							// FIXME MWHEINZ can we avoid revisiting the same node in the tier0 case?
							portp = FindNodePort(nodep,i);
							if (!portp) {
								printf("%*sUnable to find port %d\n",
									indent, "", i);
								return;
							}
							nportp = portp->neighbor;
							nnodep = nportp->nodep;

							if (nnodep->NodeInfo.NodeType != STL_NODE_SW) {
								STL_LID mask = ~0 << nportp->PortInfo.s1.LMC;
								if ((dlid & mask) != (nportp->PortInfo.LID & mask)) switch(format) {
								case FORMAT_XML:
									printf("%*s<ARError Value=\"BadTermination\">\n",
										indent,"");
									XmlPrintDec("DLID",dlid,indent+4);
									printf("%*s</ARError>\n",indent,"");
									break;
								default:
									printf("%*sERROR: AR Path from 0x%x to 0x%x "
										"terminates at the wrong device.\n",
										indent, "", slid, dlid);
								} else {
									// Even if this is the right device, we still have 
									// a problem.
									switch(format) {
									case FORMAT_XML:
										printf("%*s<ARError Value=\"BadMembership\">\n",indent,"");
										XmlPrintDec("SLID",slid,indent+4);
										XmlPrintDec("DLID",dlid,indent+4);
										break;
									default:
										printf("%*sERROR: AR Path from 0x%x to 0x%x terminates but LFT "
											"path does not. (4)\n",
											indent, "", slid, dlid);
										break;
									}
								}
								continue;
							}

							// Calculate the length of the path from here to dlid 
							// via nnodep. It should be the same as the LFT path 
							// length.
							pl2 = PGRouteHop(slid, nnodep, dlid, 0, pl1, 0, format, indent);
							if (pl2 < 0) {
								POP_NODEDATA(nnodep, 
									nportp->EndPortLID, 1, 
									format, indent);
								// PGRouteHop detected an error.
								if (format==FORMAT_XML) {
									printf("%*s</ARError>\n",indent,"");
								}
								break; // stop looking...
							} else if (pl2 != pl1) {
								// This path was shorter or longer than the
								// linear path. Either way, that's a problem.
								switch(format) {
								case FORMAT_XML:
									printf("%*s<ARError Value=\"InconsistentHopCount\">\n",
										indent,"");
									XmlPrintDec("SLID",slid,indent+4);
									XmlPrintDec("DLID",dlid,indent+4);
									XmlPrintDec("PL1",pl1-1,indent+4);
									XmlPrintDec("PL2",pl2-1,indent+4);
									break;
								default:
									printf("%*sERROR: Paths from 0x%016"PRIx64", (LID 0x%x)"
										" to LID 0x%x have inconsistent hop counts: %d vs %d\n",
										indent, "",
										nodep->NodeInfo.NodeGUID, slid, dlid, pl1-1, pl2-1);
										break;
								}
								arOkay = 0;
								break;
							}
							arCount++;
						}
						routeCount++;
					}
					((uint8_t*)nodep->context)[dlid]=pl1;
				}
				if (detail>2 && arOkay) {
						PortData *destPortp = FindLid(&g_Fabric,dlid);
						EUI64 portGuid = (destPortp)?destPortp->PortGUID:0x0ll;
						
						switch (format) {
						case FORMAT_XML:
							printf("%*s<Route>\n",indent+4,"");
							XmlPrintHex("DLID",dlid, indent+8);
							XmlPrintHex("PortGUID", portGuid, indent+8);
							XmlPrintHex("HopCount",pl1, indent+8);
							XmlPrintDec("Alternates", arCount, indent+8);
							printf("%*s</Route>\n",indent+4,"");
							break;
						default:
							printf("%*s0x%016llx - 0x%08x -  %2d  -  %2d\n",
								indent+4, "", (long long unsigned int)portGuid, 
								dlid, pl1-1, arCount);
							break;
						}
				}
			}
			if (detail>2) switch (format) {
			case FORMAT_XML:
				printf("%*s</AdaptiveRoutes>\n",indent+4,"");
				break;
			case FORMAT_TEXT:
				break;
			}
		}
		if (format == FORMAT_XML) {
			printf("%*s</Node>\n", indent, "");
		}
	} // for ( pList=QListHead(&g_Fabric.AllSWs); 

	switch (format) {
	case FORMAT_XML:
		XmlPrintDec("RoutesTested",routeCount,indent+4);
		printf("%*s</ValidatePortGroups>\n", indent, "");
		break;
	default:
		printf("%*sAlternate Routes Tested: %d\n", indent, "", routeCount);
		break;
	}

}

FSTATUS ShowMcGroups(FabricData_t *fabricp, Format_t format, int detail, int indent)
{
	LIST_ITEM *n1, *p1;

	if (detail >= 0)
	  switch (format) {
	  case FORMAT_TEXT:
		if (fabricp->NumOfMcGroups == 1)
			printf("Number of MC Group: %d\n", fabricp->NumOfMcGroups);
		else
			printf("Number of MC Groups: %d\n", fabricp->NumOfMcGroups);
		if (detail > 0 ) printf("\n");
		break;
	  case FORMAT_XML:
		printf("%*s<NumMcGroups>%d</NumMcGroups>\n",indent+4,"",fabricp->NumOfMcGroups);
		break;
	  default:
		break;
	  }

// collecting information about MC Groups
	for (n1 = QListHead (&fabricp->AllMcGroups); n1 != NULL; n1 = QListNext(&fabricp->AllMcGroups, n1)) {
		McGroupData *pmcgroup = (McGroupData *)QListObj(n1);
		//for this PortGID get member group information
		// do not get info on empty groups
		McMemberData *pMCGM = (McMemberData *)QListObj(QListHead(&pmcgroup->AllMcGroupMembers));

		if ((pMCGM->MemberInfo.RID.PortGID.AsReg64s.H ==0) && (pMCGM->MemberInfo.RID.PortGID.AsReg64s.L==0))
			continue;
		if (detail > 0 )
			switch (format) {
			case FORMAT_TEXT:
				DisplayGroupRecord (pmcgroup, indent, detail);
				printf("Number of Group Members: %d\n", pmcgroup->NumOfMembers);
				break;
			case FORMAT_XML:
				printf("%*s<%s id=\"0x%016"PRIx64":0x%016"PRIx64"\">\n", indent+4, "", "MulticastGroup",
						pmcgroup->MGID.AsReg64s.H, pmcgroup->MGID.AsReg64s.L);
				printf("%*s<NumMcGMembers>%d</NumMcGMembers>\n",indent+8,"", pmcgroup->NumOfMembers);
				XmlPrintGroupRecord (pmcgroup, indent, detail);
				break;
			default:
				break;
			}

		if (detail > 1 ) {
			//if the list is not empty)

			for (p1=QListHead(&pmcgroup->AllMcGroupMembers); p1 != NULL; p1 = QListNext(&pmcgroup->AllMcGroupMembers, p1)) {
				McMemberData *pMCGG = (McMemberData *)QListObj(p1);

				if ((pMCGG->MemberInfo.RID.PortGID.AsReg64s.H !=0) || (pMCGG->MemberInfo.RID.PortGID.AsReg64s.L!=0)) {
					switch (format) { // do not print "zero" members
					case FORMAT_TEXT:
						printf("PortGid: 0x%016"PRIx64":0x%016"PRIx64" Membership: %s%s%s\n",
							pMCGG->MemberInfo.RID.PortGID.AsReg64s.H,
							pMCGG->MemberInfo.RID.PortGID.AsReg64s.L,
							pMCGG->MemberInfo.JoinFullMember ? "Full " : "",
							pMCGG->MemberInfo.JoinNonMember ? "Non " : "",
							pMCGG->MemberInfo.JoinSendOnlyMember ? "Sendonly " : "");
						break;
					case FORMAT_XML:
						printf("%*s<%s id=\"0x%016"PRIx64":0x%016"PRIx64"\">\n", indent+8, "", "McMembers",
							pMCGG->MemberInfo.RID.PortGID.AsReg64s.H,  pMCGG->MemberInfo.RID.PortGID.AsReg64s.L);
						XmlPrintGID("GID",pMCGG->MemberInfo.RID.PortGID,indent+12);
						XmlPrintTagHeader("Membership", indent+12);
						XmlPrintStr("Full", pMCGG->MemberInfo.JoinFullMember? "1":"0", indent+16);
						XmlPrintStr("NonMember", pMCGG->MemberInfo.JoinNonMember? "1":"0", indent+16);
						XmlPrintStr("SendOnly", pMCGG->MemberInfo.JoinSendOnlyMember? "1":"0", indent+16);
						XmlPrintTagFooter("Membership", indent+12);
						McMembershipXmlOutput("Membership_Int", pMCGG,indent+12);
						break;
					default:
						break;
					}// case end

					// this list is sorted/keyed by NodeGUID
					PortData *portp=FindPortGuid(fabricp, pMCGG->MemberInfo.RID.PortGID.AsReg64s.L);
					if (portp !=NULL) {
						switch (format) {
							case FORMAT_TEXT:
								printf("Name: %.*s \n",  NODE_DESCRIPTION_ARRAY_SIZE,
									g_noname?g_name_marker:(char*)portp->nodep->NodeDesc.NodeString);
								break;
							case FORMAT_XML:
								XmlPrintNodeDesc((char*)portp->nodep->NodeDesc.NodeString, indent+12);
							// print end tag
								break;
							default:
								break;
						}// end case
					} //end if
					else {
						switch (format) {
						case FORMAT_TEXT:
							printf("Name: Not available\n");
							break;
						default:
							break;
						}
					}

					if (format == FORMAT_XML)
						printf("%*s</McMembers>\n", indent+8, "");
				}
			}	// end for p1
		}//end if detail


		if (detail > 0)
			switch (format) {
			case FORMAT_TEXT:
				printf("\n");
				break;
			case FORMAT_XML:
				printf("%*s</MulticastGroup>\n", indent+4, "");
				break;
			}
	} //end for n1


	return FSUCCESS;

} //end of ShowMcGroups



//output multicast groups
void ShowMulticastGroupsReport(Format_t format, int indent, int detail)
{
	FSTATUS status;


	if (g_hard) {
		fprintf(stderr, "opareport -H -o mcgroups: No report provided for -H\n" );
		g_exitstatus=1;
	}

	switch (format) {
	case FORMAT_TEXT:
			printf("%*sMulticast Group Summary\n",indent, "");
		break;
	case FORMAT_XML:
			printf("%*s<MCGroupSummary>\n",indent,"");
		indent+=4;
		break;
	default:
		break;
	}

	status = ShowMcGroups(&g_Fabric,format, detail, indent);

	if (status != FSUCCESS) {
		fprintf(stderr, "opareport -o mcgroups: Unable to find multicast group members (status=0x%x): %s\n", status, iba_fstatus_msg(status));
		g_exitstatus=1;
	}

	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</MCGroupSummary>\n",indent, "");
		break;
	default:
		break;
	}

	return;
} //end of ShowMulticastGroupsReport

// output multicast FDB
void ShowMulticastFDBReport(Point *focus, Format_t format, int indent, int detail)
{
	int ix_lid, ix_port, ix_pos;
	LIST_ITEM *pList;
	NodeData *nodep;
	SwitchData *switchp;
	STL_PORTMASK **pPortMask;
	uint64 portMask;
	uint32 ct_node = 0;
	uint32 ct_lid, ct_port;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sMulticast FDB Tables\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<MulticastFDBs>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u Connected Switches in Fabric%s\n", indent, "",
			(unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}

	// Report Multicast FDB
	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {
		nodep = QListObj(pList);
		switchp = nodep->switchp;

		if (!CompareNodePoint(nodep, focus))
			continue;

		if (!switchp || !switchp->MulticastFDB[0])
			continue;

		if (detail) {
			if (!ct_node) {
				ShowNodeBriefSummaryHeadings(format, indent, 0);
				switch (format) {
				case FORMAT_TEXT:
					printf("%*s  LID Ports\n", indent+4, "");
					break;
				case FORMAT_XML:
					break;
				default:
					break;
				}
			}
			ShowNodeBriefSummary(nodep, focus, FALSE, format, indent, 0);
			ct_lid = 0;
			const size_t NumMcfdbEntries = switchp->MulticastFDBSize - STL_LID_MULTICAST_BEGIN;

			for ( ix_lid = 0, pPortMask = switchp->MulticastFDB; ix_lid < NumMcfdbEntries; ix_lid++ ) {

				for ( ix_pos = 0; ix_pos < STL_NUM_MFT_POSITIONS_MASK; ix_pos++) {

					portMask = pPortMask[ix_pos][ix_lid];
					if (!portMask)
						continue;

					switch (format) {
					case FORMAT_TEXT:
						printf("%*s0x%08x", indent+4, "", (uint32)(ix_lid + STL_LID_MULTICAST_BEGIN));
						break;
					case FORMAT_XML:
						if (!ix_pos && !ct_lid++)
							printf("%*s<MulticastFDB>\n", indent+4, "");
						printf( "%*s<Value pos=\"%d\" LID=\"0x%08x\">", indent+8, "",
							ix_pos, (ix_lid + STL_LID_MULTICAST_BEGIN) );
						break;
					default:
						break;
					}

					switch (format) {
					case FORMAT_TEXT:
						ct_port = 0;
						for (ix_port = 0; ix_port < STL_PORT_MASK_WIDTH; portMask >>= 1, ix_port++) {
							if (!(portMask & 1))
								continue;

							printf(" %02u", (uint32)(ix_pos*STL_PORT_MASK_WIDTH)+ix_port);
							if (++ct_port%20 == 0)
								printf("\n%*s      ", indent+4, "");
						}	// End of for (ix_port = 0; ix_port < STL_PORT_MASK_WIDTH
						break;
					case FORMAT_XML:
						printf("0x%"PRIx64, portMask);
						break;
					default:
						break;
					}

					switch (format) {
					case FORMAT_TEXT:
						printf("\n");
						break;
					case FORMAT_XML:
						printf("</Value>\n");
						break;
					default:
						break;
					}
				}	// End of for ( ix_pos = 0; ix_pos < STL_NUM_MFT_POSITIONS_MASK
			}	// End of for ( ix_lid = 0; pPortMask = switchp->MulticastFDB

			switch (format) {
			case FORMAT_XML:
				if (ct_lid)
					printf("%*s</MulticastFDB>\n", indent+4, "");
				printf("%*s</Node>\n", indent, "");
				break;
			default:
				break;
			}

		}	// End of if (detail)

		ct_node++;

	}	// End of for (pList=QListHead(&g_Fabric.AllSWs); pList != NULL

done:
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Reported Switch(es)\n", indent, "", ct_node);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedSwitchCount", (unsigned)ct_node, indent);
		indent-=4;
		printf("%*s</MulticastFDBs>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowMulticastFDBReport()


// output port usage in linear FDB
void ShowPortUsageReport(Point *focus, Format_t format, int indent, int detail)
{
	int ix_lid, ix_port, ix_lmc;
	LIST_ITEM *pList;
	cl_map_item_t *pMap, *pMap_2;
	NodeData *nodep;
	SwitchData *switchp;
	PortData *portp;
	uint32 ct_node = 0;
	uint32 ct_port;
	uint16 tb_nodeData[g_max_lft+1];
	uint32 tb_usageCaAll[256];
	uint32 tb_usageCaBase[256];
	uint32 tb_usageSwitch[256];

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sPort Usage\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<PortUsage>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u Connected Switch(es) in Fabric%s\n", indent, "",
			(unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}

	// Report Port Usage in Linear FDB
	// Make list of node type and lmc by LID
	memset(tb_nodeData, 0xFF, (g_max_lft+1) * sizeof(uint16));
	for ( pMap=cl_qmap_head(&g_Fabric.AllNodes);
			pMap != cl_qmap_end(&g_Fabric.AllNodes);
			pMap = cl_qmap_next(pMap) ) {
		nodep = PARENT_STRUCT(pMap, NodeData, AllNodesEntry);

		for ( pMap_2=cl_qmap_head(&nodep->Ports); pMap_2 != cl_qmap_end(&nodep->Ports);
				pMap_2 = cl_qmap_next(pMap_2) ) {
			portp = PARENT_STRUCT(pMap_2, PortData, NodePortsEntry);

			if ( portp->PortInfo.LID &&
					(portp->PortInfo.LID <= g_max_lft) ) {
				for ( ix_lmc = (1 << portp->PortInfo.s1.LMC) - 1;
						ix_lmc >= 0; ix_lmc-- )
					tb_nodeData[portp->PortInfo.LID + ix_lmc] =
						(ix_lmc << 8) + nodep->NodeInfo.NodeType;
			}

		}	// End of for ( pMap_2=cl_qmap_head(&nodep->Ports)

	}	// End of for ( pMap=cl_qmap_head(&g_Fabric.AllNodes)

	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {
		nodep = QListObj(pList);
		switchp = nodep->switchp;

		if (! CompareNodePoint(nodep, focus))
			continue;

		if (!switchp || !switchp->LinearFDB)
			continue;

		if (detail) {
			ShowNodeBriefSummaryHeadings(format, indent, 0);
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sPort Total FI-All  FI-Base Switch\n", indent+4, "");
				ShowNodeBriefSummary(nodep, focus, FALSE, format, indent, 0);
				break;
			case FORMAT_XML:
				ShowNodeBriefSummary(nodep, focus, FALSE, format, indent, 0);
				break;
			default:
				break;
			}

			memset(tb_usageCaAll, 0, 256 * sizeof(uint32));
			memset(tb_usageCaBase, 0, 256 * sizeof(uint32));
			memset(tb_usageSwitch, 0, 256 * sizeof(uint32));

			for (ix_lid = 0; ix_lid < switchp->LinearFDBSize; ix_lid++) {
				if ( STL_LFT_PORT_BLOCK(switchp->LinearFDB, ix_lid) == 0 || STL_LFT_PORT_BLOCK(switchp->LinearFDB, ix_lid) == 0xFF )
					continue;

				ix_lmc = (tb_nodeData[ix_lid] >> 8) & 0xFF;
				ix_port = STL_LFT_PORT_BLOCK(switchp->LinearFDB, ix_lid);
				switch (tb_nodeData[ix_lid] & 0xFF) {
				case STL_NODE_FI:
					tb_usageCaAll[ix_port]++;

					if (!ix_lmc)
						tb_usageCaBase[ix_port]++;
					break;

				case STL_NODE_SW:
					tb_usageSwitch[ix_port]++;
					break;

				default:
					break;

				}	// End of switch (tb_nodeData[ix_lid] & 0xFF)

			}	// End of for (ix_lid = 0; ix_lid < switchp->LinearFDBSize

			ct_node++;
			ct_port = 0;
			for (ix_port = 0; ix_port < 256; ix_port++) {
				if ( !tb_usageCaAll[ix_port] && !tb_usageCaBase[ix_port] &&
						!tb_usageSwitch[ix_port])
					continue;

				switch (format) {
				case FORMAT_TEXT:
					printf( "%*s%3u  %5u  %5u    %5u  %5u\n", indent+4, "",
						(uint32)ix_port, tb_usageCaAll[ix_port] +
						tb_usageSwitch[ix_port],
						tb_usageCaAll[ix_port], tb_usageCaBase[ix_port],
						tb_usageSwitch[ix_port]);
					break;
				case FORMAT_XML:
					if (!ct_port++)
						printf("%*s<Ports>\n", indent+4, "");
					printf("%*s<Value Port=\"%d\">\n", indent+8, "", ix_port);
					XmlPrintDec( "TotalLid", tb_usageCaAll[ix_port] +
						tb_usageSwitch[ix_port], indent+12 );
					if (tb_usageCaAll[ix_port])
						XmlPrintDec("CaAllLid", tb_usageCaAll[ix_port], indent+12);
					if (tb_usageCaBase[ix_port])
						XmlPrintDec("CaBaseLid", tb_usageCaBase[ix_port], indent+12);
					if (tb_usageSwitch[ix_port])
						XmlPrintDec("SwitchLid", tb_usageSwitch[ix_port], indent+12);
					printf("%*s</Value>\n", indent+8, "");
					break;
				default:
					break;
				}

			}	// End of for (ix_port = 0; ix_port < 256; ix_port++)

			switch (format) {
			case FORMAT_XML:
				if (ct_port)
					printf("%*s</Ports>\n", indent+4, "");
				printf("%*s</Node>\n", indent, "");
				break;
			default:
				break;
			}

		}	// End of if (detail)

	}	// End of for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL

done:
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Reported Switch(es)\n", indent, "", ct_node);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedSwitchCount", (unsigned)ct_node, indent);
		indent-=4;
		printf("%*s</PortUsage>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowPortUsageReport()

struct ShowPathContext {
	Format_t format;
	int indent;
	int detail;
};

void ReportCallback(PortData *portp1, PortData *portp2, STL_LID dlid, boolean isBaseLid, boolean flag /* TRUE=uplink or recv */, void *context)
{
	struct ShowPathContext *ShowPathContext = (struct ShowPathContext*)context;
	int indent = ShowPathContext->indent;

	switch (ShowPathContext->format) {
	case FORMAT_TEXT:
		// output portp1 
		// output -> dlid portp2
		printf("%*s          0x%016"PRIx64" %3u %s %.*s\n",
			indent, "",
			portp1->nodep->NodeInfo.NodeGUID,
			portp1->PortNum,
			StlNodeTypeToText(portp1->nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp1->nodep->NodeDesc.NodeString);
		printf("%*s-> 0x%08x 0x%016"PRIx64" %3u %s %.*s\n",
			indent, "", dlid,
			portp2->nodep->NodeInfo.NodeGUID,
			portp2->PortNum,
			StlNodeTypeToText(portp2->nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp2->nodep->NodeDesc.NodeString);
		break;
	case FORMAT_XML:
		printf("%*s<Path>\n", indent, "");

			printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent+4, "",
				portp1->nodep->NodeInfo.NodeGUID, portp1->PortNum);
			XmlPrintHex64("NodeGUID",
					portp1->nodep->NodeInfo.NodeGUID, indent+8);
			if (portp1->PortGUID)
				XmlPrintHex64("PortGUID", portp1->PortGUID, indent+8);
			XmlPrintDec("PortNum", portp1->PortNum, indent+8);
			XmlPrintNodeType(portp1->nodep->NodeInfo.NodeType,
						indent+8);
			XmlPrintNodeDesc((char*)portp1->nodep->NodeDesc.NodeString, indent+8);
			printf("%*s</Port>\n", indent+4, "");

			printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent+4, "",
				portp2->nodep->NodeInfo.NodeGUID, portp2->PortNum);
			XmlPrintLID("DLID", dlid, indent+8);
			XmlPrintHex64("NodeGUID",
					portp2->nodep->NodeInfo.NodeGUID, indent+8);
			if (portp2->PortGUID)
				XmlPrintHex64("PortGUID", portp2->PortGUID, indent+8);
			XmlPrintDec("PortNum", portp2->PortNum, indent+8);
			XmlPrintNodeType(portp2->nodep->NodeInfo.NodeType,
						indent+8);
			XmlPrintNodeDesc((char*)portp2->nodep->NodeDesc.NodeString, indent+8);
			printf("%*s</Port>\n", indent+4, "");
		printf("%*s</Path>\n", indent, "");
		break;
	default:
		break;
	} 
}

// Standard Deviation is computed as:
// SQRT(S0 * S2 - (S1*S1))/S0
// Where:
//     S0 = sum(data^0) = N
//     S1 = sum(data^1) = sum(data)          [held in Total]
//     S2 = sum(data^2)                      [held in Total2]
struct statistics_s {
	uint32 min;	// must initialize to IB_UINT32_MAX
	uint32 max;
	uint32 total;	// total of datum
	uint64 total2;	// total of datum^2
	uint32 avg;
	float stddev;
};

// add a single data point to the statistical variables
static _inline
void add_datum(struct statistics_s *stat, uint32 datum)
{
	stat->min = MIN(datum, stat->min);
	stat->max = MAX(datum, stat->max);
	stat->total += datum;
	stat->total2 += ((uint64)datum * (uint64)datum);
}

// add data points tabulated for stats2 to the running stat statistics
static _inline
void add_datums(struct statistics_s *stat, struct statistics_s *stat2)
{
	stat->min = MIN(stat2->min, stat->min);
	stat->max = MAX(stat2->max, stat->max);
	stat->total += stat2->total;
	stat->total2 += stat2->total2;
}

// compute summary statistics
static _inline
void compute_statistics(struct statistics_s *stat, uint32 n)
{
	if (n) {
		stat->avg = stat->total / n;
		stat->stddev = sqrt(n * stat->total2
						- ((uint64)(stat->total)*(uint64)(stat->total)))/n;
	} else {
		stat->min = 0;	// make output a litle cleaner
	}
}

// print text format of statistics
static _inline
void print_text_statistics(int indent, const char* title, struct statistics_s *stat)
{
	printf("%*s%s Total: %u  Avg: %u  StdDev: %u\n", indent, "", title,
				stat->total, stat->avg, (unsigned)stat->stddev);
	printf("%*s Min: %u  Max: %u\n", (int)(indent+strlen(title)-1), "",
				stat->min, stat->max);
}


// print XML format of statistics
static _inline
void print_xml_statistics(int indent, const char* title, struct statistics_s *stat)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "%sTotal", title);
	XmlPrintDec(buf, (unsigned)stat->total, indent);
	snprintf(buf, sizeof(buf), "%sAvg", title);
	XmlPrintDec(buf, (unsigned)stat->avg, indent);
	snprintf(buf, sizeof(buf),"%sStdDev", title);
	XmlPrintDec(buf, (unsigned)stat->stddev, indent);
	snprintf(buf, sizeof(buf), "%sMin", title);
	XmlPrintDec(buf, (unsigned)stat->min, indent);
	snprintf(buf, sizeof(buf), "%sMax", title);
	XmlPrintDec(buf, (unsigned)stat->max, indent);
}

// output path usage in linear FDBs
void ShowPathUsageReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *pList;
	NodeData *nodep;
	SwitchData *switchp;
	uint32 ct_node = 0;
	uint32 ct_port;
	FSTATUS status;
	uint32 totalPorts = 0;
	uint32 totalPaths;
	uint32 badPaths;
	struct ShowPathContext ShowPathContext = { format:format, detail:detail };

	struct statistics_s fabricRecvBasePaths = { min:IB_UINT32_MAX };
	struct statistics_s fabricXmitBasePaths = { min:IB_UINT32_MAX };

	struct statistics_s fabricRecvNonBasePaths = { min:IB_UINT32_MAX };
	struct statistics_s fabricXmitNonBasePaths = { min:IB_UINT32_MAX };

	struct statistics_s fabricRecvAllPaths = { min:IB_UINT32_MAX };
	struct statistics_s fabricXmitAllPaths = { min:IB_UINT32_MAX };

	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		return;
	}
	status = TabulateCARoutes(&g_Fabric, &totalPaths, &badPaths, FALSE);
	if (status != FSUCCESS) {
		fprintf(stderr, "opareport: -o pathusage: Unable to tabulate routes (status=0x%x): %s\n", status, iba_fstatus_msg(status));
		g_exitstatus = 1;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sPath Usage\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<PathUsage>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u Connected Switch(es) in Fabric%s\n", indent, "",
			(unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}

	// Report Path Usage in Linear FDB
	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {
		cl_map_item_t *p;

		struct statistics_s swRecvBasePaths = { min:IB_UINT32_MAX };
		struct statistics_s swXmitBasePaths = { min:IB_UINT32_MAX };

		struct statistics_s swRecvNonBasePaths = { min:IB_UINT32_MAX };
		struct statistics_s swXmitNonBasePaths = { min:IB_UINT32_MAX };

		struct statistics_s swRecvAllPaths = { min:IB_UINT32_MAX };
		struct statistics_s swXmitAllPaths = { min:IB_UINT32_MAX };

		nodep = QListObj(pList);
		switchp = nodep->switchp;

		if (! CompareNodePoint(nodep, focus))
			continue;

		if (!switchp || !switchp->LinearFDB)
			continue;

		if (detail) {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sNodeGUID         Type Name\n", indent, "");
				if (detail > 1)
					printf("%*sPort Rcv: FI-All    FI-Base  FI-NonBase  Xmt: FI-All    FI-Base  FI-NonBase\n", indent+4, "");
				if (detail > 2)
					printf("%*s    DLID  NodeGUID         Port Type Name\n", indent+8, "");
				printf("%*s0x%016"PRIx64" %s %s\n", indent, "",
					nodep->NodeInfo.NodeGUID,
					StlNodeTypeToText(nodep->NodeInfo.NodeType),
					(char*)nodep->NodeDesc.NodeString);
				break;
			case FORMAT_XML:
				printf( "%*s<Node id=\"0x%016"PRIx64"\">\n", indent, "",
						nodep->NodeInfo.NodeGUID );
				XmlPrintHex64( "NodeGUID",
						nodep->NodeInfo.NodeGUID, indent+4 );
				XmlPrintStr( "NodeType",
					StlNodeTypeToText(nodep->NodeInfo.NodeType),
					indent+4 );
				XmlPrintStr( "NodeDesc",
					(char*)nodep->NodeDesc.NodeString, indent+4 );
				break;
			default:
				break;
			}
			indent += 4;
		}

		ct_node++;
		ct_port = 0;
		for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_next(p)) {
			PortData *portp= PARENT_STRUCT(p, PortData, NodePortsEntry);
			uint32 recvNonBasePaths;
			uint32 xmitNonBasePaths;

			if (! portp->PortNum)
				continue;	// skip switch port 0

			// only report on ISLs,  FI-SW links are boring
			if (! portp->neighbor
				|| portp->neighbor->nodep->NodeInfo.NodeType != STL_NODE_SW)
				continue;

			if (! ComparePortPoint(portp, focus))
				continue;

			totalPorts++;
			ct_port++;
			recvNonBasePaths = portp->analysisData.routes.recvAllPaths - portp->analysisData.routes.recvBasePaths;
			xmitNonBasePaths = portp->analysisData.routes.xmitAllPaths - portp->analysisData.routes.xmitBasePaths;

			add_datum(&swRecvBasePaths, portp->analysisData.routes.recvBasePaths);
			add_datum(&swXmitBasePaths, portp->analysisData.routes.xmitBasePaths);

			add_datum(&swRecvNonBasePaths, recvNonBasePaths);
			add_datum(&swXmitNonBasePaths, xmitNonBasePaths);

			add_datum(&swRecvAllPaths, portp->analysisData.routes.recvAllPaths);
			add_datum(&swXmitAllPaths, portp->analysisData.routes.xmitAllPaths);

			if (detail > 1) {
				switch (format) {
				case FORMAT_TEXT:
					printf( "%*s%3u  %10u  %10u  %10u  %10u  %10u  %10u\n", indent, "",
						(uint32)portp->PortNum,
						portp->analysisData.routes.recvAllPaths, portp->analysisData.routes.recvBasePaths,
							recvNonBasePaths,
						portp->analysisData.routes.xmitAllPaths, portp->analysisData.routes.xmitBasePaths,
							xmitNonBasePaths);
					if (detail > 2) {
						ShowPathContext.indent = indent+4;
						(void)ReportCARoutes(&g_Fabric, portp, ReportCallback,
									&ShowPathContext, FALSE);
					}
					break;
				case FORMAT_XML:
					if (1 == ct_port)
						printf("%*s<Ports>\n", indent, "");
					printf("%*s<Value Port=\"%d\">\n", indent+4, "", portp->PortNum);
					XmlPrintDec("CaRecvAllPath", portp->analysisData.routes.recvAllPaths, indent+8);
					XmlPrintDec("CaRecvBaseLid", portp->analysisData.routes.recvBasePaths, indent+8);
					XmlPrintDec("CaRecvNonBaseLid", recvNonBasePaths, indent+8);
					XmlPrintDec("CaXmitAllPath", portp->analysisData.routes.xmitAllPaths, indent+8);
					XmlPrintDec("CaXmitBaseLid", portp->analysisData.routes.xmitBasePaths, indent+8);
					XmlPrintDec("CaXmitNonBaseLid", xmitNonBasePaths, indent+8);
					if (detail > 2) {
						ShowPathContext.indent = indent+8;
						(void)ReportCARoutes(&g_Fabric, portp, ReportCallback,
									&ShowPathContext, FALSE);
					}
					printf("%*s</Value>\n", indent+4, "");
					break;
				default:
					break;
				}
			}

		}	// End of for all ports

		if (detail > 1 && ct_port && format == FORMAT_XML)
			printf("%*s</Ports>\n", indent, "");

		// if there are no paths through switch, add_datums is no net change
		add_datums(&fabricRecvBasePaths, &swRecvBasePaths);
		add_datums(&fabricXmitBasePaths, &swXmitBasePaths);

		add_datums(&fabricRecvNonBasePaths, &swRecvNonBasePaths);
		add_datums(&fabricXmitNonBasePaths, &swXmitNonBasePaths);

		add_datums(&fabricRecvAllPaths, &swRecvAllPaths);
		add_datums(&fabricXmitAllPaths, &swXmitAllPaths);

		compute_statistics(&swRecvBasePaths, ct_port);
		compute_statistics(&swXmitBasePaths, ct_port);

		compute_statistics(&swRecvNonBasePaths, ct_port);
		compute_statistics(&swXmitNonBasePaths, ct_port);

		compute_statistics(&swRecvAllPaths, ct_port);
		compute_statistics(&swXmitAllPaths, ct_port);

		if (detail) {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%d Reported Port(s)\n", indent, "", ct_port);
				if (ct_port) {
					print_text_statistics(indent, "Recv All Paths:     ", &swRecvAllPaths);
					print_text_statistics(indent, "Recv Base Paths:    ", &swRecvBasePaths);
					print_text_statistics(indent, "Recv Non-Base Paths:", &swRecvNonBasePaths);
					print_text_statistics(indent, "Xmit All Paths:     ", &swXmitAllPaths);
					print_text_statistics(indent, "Xmit Base Paths:    ", &swXmitBasePaths);
					print_text_statistics(indent, "Xmit Non-Base Paths:", &swXmitNonBasePaths);
				}
				indent-=4;
				break;
			case FORMAT_XML:
				XmlPrintDec("ReportedPortCount", (unsigned)ct_port, indent);
				if (ct_port) {
					print_xml_statistics(indent, "RecvAllPaths", &swRecvAllPaths);
					print_xml_statistics(indent, "RecvBasePaths", &swRecvBasePaths);
					print_xml_statistics(indent, "RecvNonBasePaths", &swRecvNonBasePaths);
					print_xml_statistics(indent, "XmitAllPaths", &swXmitAllPaths);
					print_xml_statistics(indent, "XmitBasePaths", &swXmitBasePaths);
					print_xml_statistics(indent, "XmitNonBasePaths", &swXmitNonBasePaths);
				}
				indent-=4;
				printf("%*s</Node>\n", indent, "");
				break;
			default:
				break;
			}
		}	// End of if (detail)

	}	// End of for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL

	if (totalPorts) {
		compute_statistics(&fabricRecvBasePaths, totalPorts);
		compute_statistics(&fabricXmitBasePaths, totalPorts);

		compute_statistics(&fabricRecvNonBasePaths, totalPorts);
		compute_statistics(&fabricXmitNonBasePaths, totalPorts);

		compute_statistics(&fabricRecvAllPaths, totalPorts);
		compute_statistics(&fabricXmitAllPaths, totalPorts);
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Reported Switch(es)\n", indent, "", ct_node);
		printf("%*s%d Reported Port(s)\n", indent, "", totalPorts);
		printf("%*s%d Incomplete Route(s)\n", indent, "", badPaths);
		if (totalPorts) {
			print_text_statistics(indent, "Recv All Paths:     ", &fabricRecvAllPaths);
			print_text_statistics(indent, "Recv Base Paths:    ", &fabricRecvBasePaths);
			print_text_statistics(indent, "Recv Non-Base Paths:", &fabricRecvNonBasePaths);
			print_text_statistics(indent, "Xmit All Paths:     ", &fabricXmitAllPaths);
			print_text_statistics(indent, "Xmit Base Paths:    ", &fabricXmitBasePaths);
			print_text_statistics(indent, "Xmit Non-Base Paths:", &fabricXmitNonBasePaths);
		}
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedSwitchCount", (unsigned)ct_node, indent);
		XmlPrintDec("ReportedPortCount", (unsigned)totalPorts, indent);
		XmlPrintDec("IncompleteRoutes", (unsigned)badPaths, indent);
		if (totalPorts) {
			print_xml_statistics(indent, "RecvAllPaths", &fabricRecvAllPaths);
			print_xml_statistics(indent, "RecvBasePaths", &fabricRecvBasePaths);
			print_xml_statistics(indent, "RecvNonBasePaths", &fabricRecvNonBasePaths);
			print_xml_statistics(indent, "XmitAllPaths", &fabricXmitAllPaths);
			print_xml_statistics(indent, "XmitBasePaths", &fabricXmitBasePaths);
			print_xml_statistics(indent, "XmitNonBasePaths", &fabricXmitNonBasePaths);
		}
		break;
	default:
		break;
	}
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</PathUsage>\n", indent, "");
		break;
	default:
		break;
	}
}	// End of ShowPathUsageReport()

// output path usage in linear FDBs
void ShowTreePathUsageReport(Point *focus, Format_t format, int indent,
							int detail)
{
	LIST_ITEM *pList;
	NodeData *nodep;
	SwitchData *switchp;
	uint32 ct_node = 0;
	uint32 ct_port = 0;
	FSTATUS status;
	uint32 totalPorts = 0;
	uint32 totalPaths;
	uint32 badPaths;
	struct ShowPathContext ShowPathContext = { format:format, detail:detail };

	uint32 fabricUplinkCount = 0;
	uint32 fabricDownlinkCount = 0;
	
	struct statistics_s fabricDownlinkBasePaths = { min:IB_UINT32_MAX };
	struct statistics_s fabricUplinkBasePaths = { min:IB_UINT32_MAX };

	struct statistics_s fabricDownlinkNonBasePaths = { min:IB_UINT32_MAX };
	struct statistics_s fabricUplinkNonBasePaths = { min:IB_UINT32_MAX };

	struct statistics_s fabricDownlinkAllPaths = { min:IB_UINT32_MAX };
	struct statistics_s fabricUplinkAllPaths = { min:IB_UINT32_MAX };

	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		return;
	}
	status = TabulateCARoutes(&g_Fabric, &totalPaths, &badPaths, TRUE);
	if (status != FSUCCESS) {
		fprintf(stderr, "opareport: -o treepathusage: Unable to tabulate routes (status=0x%x): %s\n", status, iba_fstatus_msg(status));
		g_exitstatus = 1;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sTree Path Usage\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<TreePathUsage>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u Connected Switch(es) in Fabric%s\n", indent, "",
			(unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}

	// Report Tree Path Usage in Linear FDB
	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {
		cl_map_item_t *p;
		uint32 swUplinkCount = 0;
		uint32 swDownlinkCount = 0;
		struct statistics_s swDownlinkBasePaths = { min:IB_UINT32_MAX };
		struct statistics_s swUplinkBasePaths = { min:IB_UINT32_MAX };

		struct statistics_s swDownlinkNonBasePaths = { min:IB_UINT32_MAX };
		struct statistics_s swUplinkNonBasePaths = { min:IB_UINT32_MAX };

		struct statistics_s swDownlinkAllPaths = { min:IB_UINT32_MAX };
		struct statistics_s swUplinkAllPaths = { min:IB_UINT32_MAX };

		nodep = QListObj(pList);
		switchp = nodep->switchp;

		if (! CompareNodePoint(nodep, focus))
			continue;

		if (!switchp || !switchp->LinearFDB)
			continue;

		if (detail) {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sNodeGUID         Type Tier Name\n", indent, "");
				if (detail > 1)
					printf("%*sPort Up: FI-All     FI-Base  FI-NonBase Down: FI-All    FI-Base  FI-NonBase\n", indent+4, "");
				if (detail > 2)
					printf("%*s    DLID  NodeGUID         Port Type Name\n", indent+8, "");
				printf("%*s0x%016"PRIx64" %s %4u %s\n", indent, "",
					nodep->NodeInfo.NodeGUID,
					StlNodeTypeToText(nodep->NodeInfo.NodeType),
					nodep->analysis,
					(char*)nodep->NodeDesc.NodeString);
				break;
			case FORMAT_XML:
				printf( "%*s<Node id=\"0x%016"PRIx64"\">\n", indent, "",
						nodep->NodeInfo.NodeGUID );
				XmlPrintHex64( "NodeGUID",
						nodep->NodeInfo.NodeGUID, indent+4 );
				XmlPrintStr( "NodeType",
					StlNodeTypeToText(nodep->NodeInfo.NodeType),
					indent+4 );
				XmlPrintDec( "Tier", nodep->analysis, indent+4 );
				XmlPrintStr( "NodeDesc",
					(char*)nodep->NodeDesc.NodeString, indent+4 );
				break;
			default:
				break;
			}
			indent += 4;
		}

		ct_node++;
		ct_port = 0;
		for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_next(p)) {
			PortData *portp= PARENT_STRUCT(p, PortData, NodePortsEntry);
			uint32 downlinkNonBasePaths;
			uint32 uplinkNonBasePaths;

			if (! portp->PortNum)
				continue;	// skip switch port 0

			// only report on ISLs,  FI-SW links are boring
			if (! portp->neighbor
				|| portp->neighbor->nodep->NodeInfo.NodeType != STL_NODE_SW)
				continue;

			if (! ComparePortPoint(portp, focus))
				continue;

			totalPorts++;
			ct_port++;
			if (portp->analysisData.fatTreeRoutes.uplinkAllPaths)
				swUplinkCount++;
			if (portp->analysisData.fatTreeRoutes.downlinkAllPaths)
				swDownlinkCount++;
			downlinkNonBasePaths = portp->analysisData.fatTreeRoutes.downlinkAllPaths - portp->analysisData.fatTreeRoutes.downlinkBasePaths;
			uplinkNonBasePaths = portp->analysisData.fatTreeRoutes.uplinkAllPaths - portp->analysisData.fatTreeRoutes.uplinkBasePaths;

			add_datum(&swDownlinkBasePaths, portp->analysisData.fatTreeRoutes.downlinkBasePaths);
			add_datum(&swUplinkBasePaths, portp->analysisData.fatTreeRoutes.uplinkBasePaths);

			add_datum(&swDownlinkNonBasePaths, downlinkNonBasePaths);
			add_datum(&swUplinkNonBasePaths, uplinkNonBasePaths);

			add_datum(&swDownlinkAllPaths, portp->analysisData.fatTreeRoutes.downlinkAllPaths);
			add_datum(&swUplinkAllPaths, portp->analysisData.fatTreeRoutes.uplinkAllPaths);
			if (detail > 1) {
				switch (format) {
				case FORMAT_TEXT:
					printf( "%*s%3u  %10u  %10u  %10u  %10u  %10u  %10u\n", indent, "",
						(uint32)portp->PortNum,
						portp->analysisData.fatTreeRoutes.uplinkAllPaths, portp->analysisData.fatTreeRoutes.uplinkBasePaths,
							uplinkNonBasePaths,
						portp->analysisData.fatTreeRoutes.downlinkAllPaths, portp->analysisData.fatTreeRoutes.downlinkBasePaths,
							downlinkNonBasePaths);
					if (detail > 2) {
						ShowPathContext.indent = indent+4;
						(void)ReportCARoutes(&g_Fabric, portp, ReportCallback,
									&ShowPathContext, TRUE);
					}
					break;
				case FORMAT_XML:
					if (1 == ct_port)
						printf("%*s<Ports>\n", indent, "");
					printf("%*s<Value Port=\"%d\">\n", indent+4, "", portp->PortNum);
					XmlPrintDec("CaUplinkAllPath", portp->analysisData.fatTreeRoutes.uplinkAllPaths, indent+8);
					XmlPrintDec("CaUplinkBaseLid", portp->analysisData.fatTreeRoutes.uplinkBasePaths, indent+8);
					XmlPrintDec("CaUplinkNonBaseLid", uplinkNonBasePaths, indent+8);
					XmlPrintDec("CaDownlinkAllPath", portp->analysisData.fatTreeRoutes.downlinkAllPaths, indent+8);
					XmlPrintDec("CaDownlinkBaseLid", portp->analysisData.fatTreeRoutes.downlinkBasePaths, indent+8);
					XmlPrintDec("CaDownlinkNonBaseLid", downlinkNonBasePaths, indent+8);
					if (detail > 2) {
						ShowPathContext.indent = indent+8;
						(void)ReportCARoutes(&g_Fabric, portp, ReportCallback,
									&ShowPathContext, TRUE);
					}
					printf("%*s</Value>\n", indent+4, "");
					break;
				default:
					break;
				}
			}

		}	// End of for all ports

		if (detail > 1 && ct_port && format == FORMAT_XML)
			printf("%*s</Ports>\n", indent, "");

		// if there are no uplinks, add_datums is no net change
		fabricUplinkCount += swUplinkCount;
		add_datums(&fabricUplinkBasePaths, &swUplinkBasePaths);
		add_datums(&fabricUplinkNonBasePaths, &swUplinkNonBasePaths);
		add_datums(&fabricUplinkAllPaths, &swUplinkAllPaths);

		// if there are no downlinks, add_datums is no net change
		fabricDownlinkCount += swDownlinkCount;
		add_datums(&fabricDownlinkBasePaths, &swDownlinkBasePaths);
		add_datums(&fabricDownlinkNonBasePaths, &swDownlinkNonBasePaths);
		add_datums(&fabricDownlinkAllPaths, &swDownlinkAllPaths);

		compute_statistics(&swUplinkBasePaths, swUplinkCount);
		compute_statistics(&swUplinkNonBasePaths, swUplinkCount);
		compute_statistics(&swUplinkAllPaths, swUplinkCount);

		compute_statistics(&swDownlinkBasePaths, swDownlinkCount);
		compute_statistics(&swDownlinkNonBasePaths, swDownlinkCount);
		compute_statistics(&swDownlinkAllPaths, swDownlinkCount);

		if (detail) {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%d Reported Port(s)\n", indent, "", ct_port);
				printf("%*s%d Incomplete Route(s)\n", indent, "", badPaths);
				if (ct_port) {
					printf("%*s%u Uplink Port(s)  %u Downlink Port(s)\n", indent, "", swUplinkCount, swDownlinkCount);
					if (swUplinkCount) {
						print_text_statistics(indent, "Up All Paths:     ", &swUplinkAllPaths);
						print_text_statistics(indent, "Up Base Paths:    ", &swUplinkBasePaths);
						print_text_statistics(indent, "Up Non-Base Paths:", &swUplinkNonBasePaths);
					}
					if (swDownlinkCount) {
						print_text_statistics(indent, "Down All Paths:     ", &swDownlinkAllPaths);
						print_text_statistics(indent, "Down Base Paths:    ", &swDownlinkBasePaths);
						print_text_statistics(indent, "Down Non-Base Paths:", &swDownlinkNonBasePaths);
					}
				}
				indent-=4;
				break;
			case FORMAT_XML:
				XmlPrintDec("ReportedPortCount", (unsigned)ct_port, indent);
				XmlPrintDec("IncompleteRoutes", (unsigned)badPaths, indent);
				if (ct_port) {
					XmlPrintDec("UplinkPortCount", (unsigned)swUplinkCount, indent);
					XmlPrintDec("DownlinkPortCount", (unsigned)swDownlinkCount, indent);
					// always output, even if count is 0, simplifies scripts
					print_xml_statistics(indent, "UplinkAllPaths", &swUplinkAllPaths);
					print_xml_statistics(indent, "UplinkBasePaths", &swUplinkBasePaths);
					print_xml_statistics(indent, "UplinkNonBasePaths", &swUplinkNonBasePaths);
					print_xml_statistics(indent, "DownlinkAllPaths", &swDownlinkAllPaths);
					print_xml_statistics(indent, "DownlinkBasePaths", &swDownlinkBasePaths);
					print_xml_statistics(indent, "DownlinkNonBasePaths", &swDownlinkNonBasePaths);
				}
				indent-=4;
				printf("%*s</Node>\n", indent, "");
				break;
			default:
				break;
			}
		}	// End of if (detail)

	}	// End of for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL

	compute_statistics(&fabricUplinkBasePaths, fabricUplinkCount);
	compute_statistics(&fabricUplinkNonBasePaths, fabricUplinkCount);
	compute_statistics(&fabricUplinkAllPaths, fabricUplinkCount);
	
	compute_statistics(&fabricDownlinkBasePaths, fabricDownlinkCount);
	compute_statistics(&fabricDownlinkNonBasePaths, fabricDownlinkCount);
	compute_statistics(&fabricDownlinkAllPaths, fabricDownlinkCount);

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Reported Switch(es)\n", indent, "", ct_node);
		printf("%*s%d Reported Port(s)\n", indent, "", totalPorts);
		if (totalPorts) {
			printf("%*s%u Uplink Port(s)  %u Downlink Port(s)\n", indent, "", fabricUplinkCount, fabricDownlinkCount);
			if (fabricUplinkCount) {
				print_text_statistics(indent, "Up All Paths:     ", &fabricUplinkAllPaths);
				print_text_statistics(indent, "Up Base Paths:    ", &fabricUplinkBasePaths);
				print_text_statistics(indent, "Up Non-Base Paths:", &fabricUplinkNonBasePaths);
			}
			if (fabricDownlinkCount) {
				print_text_statistics(indent, "Down All Paths:     ", &fabricDownlinkAllPaths);
				print_text_statistics(indent, "Down Base Paths:    ", &fabricDownlinkBasePaths);
				print_text_statistics(indent, "Down Non-Base Paths:", &fabricDownlinkNonBasePaths);
			}
		}
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedSwitchCount", (unsigned)ct_node, indent);
		XmlPrintDec("ReportedPortCount", (unsigned)totalPorts, indent);
		if (totalPorts) {
			XmlPrintDec("UplinkPortCount", (unsigned)fabricUplinkCount, indent);
			XmlPrintDec("DownlinkPortCount", (unsigned)fabricDownlinkCount, indent);
			// always output, even if count is 0, simplifies scripts
			print_xml_statistics(indent, "UplinkAllPaths", &fabricUplinkAllPaths);
			print_xml_statistics(indent, "UplinkBasePaths", &fabricUplinkBasePaths);
			print_xml_statistics(indent, "UplinkNonBasePaths", &fabricUplinkNonBasePaths);
			print_xml_statistics(indent, "DownlinkAllPaths", &fabricDownlinkAllPaths);
			print_xml_statistics(indent, "DownlinkBasePaths", &fabricDownlinkBasePaths);
			print_xml_statistics(indent, "DownlinkNonBasePaths", &fabricDownlinkNonBasePaths);
		}
		break;
	default:
		break;
	}
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</TreePathUsage>\n", indent, "");
		break;
	default:
		break;
	}
}	// End of ShowTreePathUsageReport()

struct ValidateRoutesContext {
	Format_t format;
	int indent;
	int detail;
};

void ValidateRouteCallback(PortData *portp1, PortData *portp2, STL_LID dlid, boolean isBaseLid, uint8 SL, void *context)
{
	struct ValidateRoutesContext *ValidateRoutesContext = (struct ValidateRoutesContext*)context;
	int indent = ValidateRoutesContext->indent;

	if (! ValidateRoutesContext->detail)
		return;

	switch (ValidateRoutesContext->format) {
	case FORMAT_TEXT:
		// output portp1 
		// output -> dlid portp2
		printf("%*s          0x%016"PRIx64" %3u  ",
			indent, "",
			portp1->nodep->NodeInfo.NodeGUID,
			portp1->PortNum);
			if (g_use_scsc) printf("%2u", SL);
			printf("   %s  %.*s\n",
			StlNodeTypeToText(portp1->nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp1->nodep->NodeDesc.NodeString);
		printf("%*s-> 0x%08x 0x%016"PRIx64" %3u   ",
			indent, "", dlid,
			portp2->nodep->NodeInfo.NodeGUID,
			portp2->PortNum);
			if (g_use_scsc) printf("  ");
			printf("  %s  %.*s\n",
			StlNodeTypeToText(portp2->nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp2->nodep->NodeDesc.NodeString);
		if (ValidateRoutesContext->detail >= 2) {
			printf("%*sIncompletePath:\n", indent, "");
			printf("%*sNodeGUID         Port Type ", indent+4, "");
			if (g_use_scsc) printf("VL ");
			printf("Name\n");
		} else {
			printf("\n");
		}
		break;
	case FORMAT_XML:
		printf("%*s<Path>\n", indent, "");

			printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent+4, "",
				portp1->nodep->NodeInfo.NodeGUID, portp1->PortNum);
			XmlPrintHex64("NodeGUID",
					portp1->nodep->NodeInfo.NodeGUID, indent+8);
			if (portp1->PortGUID)
				XmlPrintHex64("PortGUID", portp1->PortGUID, indent+8);
			XmlPrintDec("PortNum", portp1->PortNum, indent+8);
			XmlPrintNodeType(portp1->nodep->NodeInfo.NodeType,
						indent+8);
			XmlPrintNodeDesc((char*)portp1->nodep->NodeDesc.NodeString, indent+8);
			printf("%*s</Port>\n", indent+4, "");

			printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent+4, "",
				portp2->nodep->NodeInfo.NodeGUID, portp2->PortNum);
			XmlPrintLID("DLID", dlid, indent+8);
			XmlPrintHex64("NodeGUID",
					portp2->nodep->NodeInfo.NodeGUID, indent+8);
			if (portp2->PortGUID)
				XmlPrintHex64("PortGUID", portp2->PortGUID, indent+8);
			XmlPrintDec("PortNum", portp2->PortNum, indent+8);
			XmlPrintNodeType(portp2->nodep->NodeInfo.NodeType,
						indent+8);
			XmlPrintNodeDesc((char*)portp2->nodep->NodeDesc.NodeString, indent+8);
			printf("%*s</Port>\n", indent+4, "");
			if (g_use_scsc) XmlPrintDec("SL", SL, indent+4);
		if (ValidateRoutesContext->detail >= 2)
			printf("%*s<IncompletePath>\n", indent+4, "");
		else
			printf("%*s</Path>\n", indent, "");
		break;
	default:
		break;
	} 
}

struct MCRoutesContext {
	Format_t format;
	int indent;
	int detail;
	MCROUTESTATUS status;
};

void PrintMCRouteMembers(McNodeLoopInc *LoopIncp, void *context)
{
	struct MCRoutesContext *MCRoutesContext = (struct MCRoutesContext*)context;
	int indent = MCRoutesContext->indent;

	if (! MCRoutesContext->detail)
		return;

	switch (MCRoutesContext->format) {
	case FORMAT_TEXT:
		printf("0x%016"PRIx64"\t%s\t%.*s\t%3u\t%3u\n",
			LoopIncp->pPort->nodep->NodeInfo.NodeGUID,
			StlNodeTypeToText(LoopIncp->pPort->nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)LoopIncp->pPort->nodep->NodeDesc.NodeString,
			LoopIncp->entryPort,
			LoopIncp->exitPort);

		break;
	case FORMAT_XML:
		printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent+4, "",
				LoopIncp->pPort->nodep->NodeInfo.NodeGUID, LoopIncp->pPort->PortNum);
		XmlPrintHex64("NodeGUID",
				LoopIncp->pPort->nodep->NodeInfo.NodeGUID, indent+8);
		if (LoopIncp->pPort->PortGUID)
				XmlPrintHex64("PortGUID", LoopIncp->pPort->PortGUID, indent+8);
		XmlPrintNodeType(LoopIncp->pPort->nodep->NodeInfo.NodeType,
				indent+8);
		XmlPrintNodeDesc((char*)LoopIncp->pPort->nodep->NodeDesc.NodeString, indent+8);
		XmlPrintDec("EntryPort", LoopIncp->entryPort, indent+8);
		XmlPrintDec("ExitPort", LoopIncp->exitPort, indent+8);
		printf("%*s</Port>\n", indent+4, "");
		break;
	default:
		break;
	}
}


void PrintEndMCRoute(void *context)
{
	struct MCRoutesContext *MCRoutesContext = (struct MCRoutesContext*)context;
	int indent = MCRoutesContext->indent+4;

	if (MCRoutesContext->detail <= 1)
		return;

	switch (MCRoutesContext->format) {
		case FORMAT_TEXT:
			printf("\n");
			break;
		case FORMAT_XML:
			printf("%*s</McPath>\n", indent-4, "");
			break;
		default:
			break;
		}
	return;
}

void PrintInitMCRoute(uint32 count,void *context)
{
	struct MCRoutesContext *MCRoutesContext = (struct MCRoutesContext*)context;
	int indent = MCRoutesContext->indent+4;
	MCROUTESTATUS mcstatus = MCRoutesContext->status;
	char statusstr[80], statusxml[80];

	if (MCRoutesContext->detail <= 1)
		return;

	if (count == 0) {
		if (MCRoutesContext->format== FORMAT_XML)
			printf("%*s<McPath>\n", indent-4, "");
	}
	else {
		switch (mcstatus) {
		case MC_NO_TRACE:
			strncpy(statusstr,"Unable to trace route",sizeof(statusstr));
			strncpy(statusxml,"UnableToTraceRoute",sizeof(statusxml));
			break;
		case MC_NOT_FOUND:
			strncpy(statusstr,"No start point",sizeof(statusstr));
			strncpy(statusxml,"NoStartPoint",sizeof(statusxml));
			break;
		case MC_UNAVAILABLE:
			strncpy(statusstr,"No MFT Route Table",sizeof(statusstr));
			strncpy(statusxml,"NoMFTRouteTable",sizeof(statusxml));
			break;
		case MC_LOOP:
			strncpy(statusstr,"Found Loop",sizeof(statusstr));
			strncpy(statusxml,"FoundLoop",sizeof(statusxml));
			break;
		case MC_NOGROUP:
			strncpy(statusstr,"End-node does not belong to McGroup",sizeof(statusstr));
			strncpy(statusxml,"EndNodeNoGroup",sizeof(statusxml));
			break;
		default:
			strcpy(statusstr,"");
			strcpy(statusxml,"");
			break;
		}

		switch (MCRoutesContext->format) {
		case FORMAT_TEXT:
			printf("%s. Num. of paths: %d\n",statusstr,count);
			if (MCRoutesContext->detail >= 2) {
				printf(" NodeGUID\t\tType\tName\tEntry Port\tExitPort\n");
			} else
				printf("\n");
			break;
		case FORMAT_XML:
			printf("%*s<%s>%d</%s>\n", indent-4, "",statusxml,count,statusxml);
			break;
		default:
			break;
		}
	}

}

void ValidateRouteCallback2(PortData *portp, uint8 vl, void *context)
{
	struct ValidateRoutesContext *ValidateRoutesContext = (struct ValidateRoutesContext*)context;
	int indent = ValidateRoutesContext->indent+4;

	if (ValidateRoutesContext->detail <= 1)
		return;

	if (! portp) {
		// special case, indicates end of path
		switch (ValidateRoutesContext->format) {
		case FORMAT_TEXT:
			printf("\n");
			break;
		case FORMAT_XML:
			printf("%*s</IncompletePath>\n", indent, "");
			printf("%*s</Path>\n", indent-4, "");
			break;
		default:
			break;
		}
		return;
	}

	switch (ValidateRoutesContext->format) {
	case FORMAT_TEXT:
		// output portp1 
		// output -> dlid portp2
		printf("%*s0x%016"PRIx64" %3u %s ",
			indent, "",
			portp->nodep->NodeInfo.NodeGUID,
			portp->PortNum,
			StlNodeTypeToText(portp->nodep->NodeInfo.NodeType));
		if (g_use_scsc) printf(" %2d ", vl);
		printf("%.*s\n",
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp->nodep->NodeDesc.NodeString);
		break;
	case FORMAT_XML:
		printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent+4, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintHex64("NodeGUID",
					portp->nodep->NodeInfo.NodeGUID, indent+8);
		if (portp->PortGUID)
			XmlPrintHex64("PortGUID", portp->PortGUID, indent+8);
		XmlPrintDec("PortNum", portp->PortNum, indent+8);
		XmlPrintNodeType(portp->nodep->NodeInfo.NodeType,
						indent+8);
		if (g_use_scsc) XmlPrintDec("VL", vl, indent+8);
		XmlPrintNodeDesc((char*)portp->nodep->NodeDesc.NodeString, indent+8);
		printf("%*s</Port>\n", indent+4, "");
		break;
	default:
		break;
	} 
}

// Validate all routes in linear FDBs
void ShowValidateRoutesReport(Format_t format, int indent, int detail)
{
	FSTATUS status;
	uint32 totalPaths;
	uint32 badPaths;

	struct ValidateRoutesContext ValidateRoutesContext = { format:format, detail:detail };

	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		return;
	}
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sValidate Routes\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<ValidateRoutes>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u LID(s) in Fabric%s\n", indent, "",
			(unsigned)cl_qmap_count(&g_Fabric.u.AllLids), detail?":":"" );
		printf("%*s%u Connected FIs in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllFIs), detail?":":"");
		printf( "%*s%u Connected Switch(es) in Fabric%s\n", indent, "",
			(unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("FabricLIDCount", (unsigned)cl_qmap_count(&g_Fabric.u.AllLids), indent);
		XmlPrintDec("ConnectedFICount", (unsigned)QListCount(&g_Fabric.AllFIs), indent);
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}

	if (detail) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sIncomplete Paths:\n", indent, "");
			if (g_use_scsc) printf("%*s    DLID  NodeGUID          Port  SL Type Name\n", indent, "");
			else printf("%*s    DLID  NodeGUID             Port Type Name\n", indent, "");
			break;
		case FORMAT_XML:
			printf( "%*s<IncompletePaths>\n", indent, "");
			indent+=4;
			break;
		default:
			break;
		}
	}

	ValidateRoutesContext.indent = indent;
	status = ValidateAllRoutes(&g_Fabric, g_portGuid, &totalPaths, &badPaths,
					ValidateRouteCallback, &ValidateRoutesContext,
					detail >=2 ?ValidateRouteCallback2:NULL,
					&ValidateRoutesContext, ((g_Fabric.flags & FF_QOSDATA) && g_use_scsc));
	if (status != FSUCCESS) {
		fprintf(stderr, "opareport: -o validateroutes: Unable to validate routes (status=0x%x): %s\n", status, iba_fstatus_msg(status));
		g_exitstatus = 1;
	}

	if (detail) {
		switch (format) {
		case FORMAT_TEXT:
			break;
		case FORMAT_XML:
			indent-=4;
			printf( "%*s</IncompletePaths>\n", indent, "");
			break;
		default:
			break;
		}
	}


	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Analyzed Routes\n", indent, "", totalPaths);
		printf("%*s%d Incomplete Route(s)\n", indent, "", badPaths);
		break;
	case FORMAT_XML:
		XmlPrintDec("AnalyzedRoutes", (unsigned)totalPaths, indent);
		XmlPrintDec("IncompleteRoutes", (unsigned)badPaths, indent);
		break;
	default:
		break;
	}
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</ValidateRoutes>\n", indent, "");
		break;
	default:
		break;
	}
}	// End of ShowValidateRoutesReport()

void PrintHeadGroup(STL_LID mlid, void *context)
{
	struct MCRoutesContext *MCRoutesContext = (struct MCRoutesContext*)context;
	int indent = MCRoutesContext->indent;
	int NOM=0;
	LIST_ITEM *p;

	if (! MCRoutesContext->detail)
		return;

	//Search number of members for MLID mcgroup
	for ( p= QListHead(&g_Fabric.AllMcGroups); p!=NULL; p=QListNext(&g_Fabric.AllMcGroups,p)){
		McGroupData *pMCGD = (McGroupData *)QListObj(p);
		if (pMCGD->MLID == mlid) {
			NOM = pMCGD->NumOfMembers;
			break;
		}
	}

	if (NOM !=0) {
		switch (MCRoutesContext->format) {
			case FORMAT_TEXT:
				printf("MC Group 0x%04x\n", mlid);
				printf("Number of Members:%d \n", NOM);
				break;
			case FORMAT_XML:
				XmlPrintMLID("MLID",mlid,indent+4);
				printf("%*s<MCGroupMembers>%d</MCGroupMembers>\n", indent+4, "",NOM);
				break;
		}
	}

	return;
}


// Validate MC routes in multicast tables MCDBs
void ShowValidateMCRoutesReport(Format_t format, int indent, int detail)
{
	FSTATUS status;
	uint32 totalPaths, badPaths=0, listcount=0;
	LIST_ITEM *p, *q;
	int i;
	struct MCRoutesContext MCRoutesContext = { format:format, detail:detail, status:MC_NO_TRACE };

	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		return;
	}

	if (QListCount(&g_Fabric.AllSWs) ==0) {
		printf("Cannot Validate MC Routes: No Switches Connected\n");
		return;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sValidate Multicast Routes\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<ValidateMCRoutes>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	///////////////////////////////////////////////////////////////////////
	//Get MCGroups

	switch (format) { // do not print "zero" members
		case FORMAT_TEXT:
			printf(" %d Multicast groups\n", g_Fabric.NumOfMcGroups);
			break;
		case FORMAT_XML:
			printf("%*s<MCGroup>%d</MCGroup>\n", indent, "",g_Fabric.NumOfMcGroups);
			break;
		default:
			break;
	}// case end

	MCRoutesContext.indent = indent;

	status = ValidateAllMCRoutes(&g_Fabric, &totalPaths);

	if (status != FSUCCESS) {
		fprintf(stderr, "opareport: -o validatemcroutes: Unable to validate multicast routes (status=0x%x): %s\n", status, iba_fstatus_msg(status));
		FreeValidateMCRoutes(&g_Fabric);
		g_exitstatus = 1;
		return;
	}

	// Display all MC routes with problems:
	for (i=0;i<MAXMCROUTESTATUS;i++) {
		MCRoutesContext.status = g_Fabric.AllMcLoopIncRoutes[i].status;
		listcount = QListCount(&g_Fabric.AllMcLoopIncRoutes[i].AllMcRouteStatus);
		badPaths+=listcount;

		if (listcount <= 0) // do not print "zero" members
			continue;

		PrintInitMCRoute(listcount,&MCRoutesContext); // print init
		if (QListCount(&g_Fabric.AllMcLoopIncRoutes[i].AllMcRouteStatus) <= 0)
			continue;

		for (p = QListHead(&g_Fabric.AllMcLoopIncRoutes[i].AllMcRouteStatus); p!= NULL;
			p = QListNext( &g_Fabric.AllMcLoopIncRoutes[i].AllMcRouteStatus,p) ) {

			McLoopInc *pmcloop = (McLoopInc *) QListObj(p);
			if (pmcloop == NULL)
				continue;

			PrintInitMCRoute(0,&MCRoutesContext); // print MC route members
			PrintHeadGroup(pmcloop->mlid,&MCRoutesContext);
			for (q = QListHead(&pmcloop->AllMcNodeLoopIncR ); q!= NULL;
				q = QListNext(&pmcloop->AllMcNodeLoopIncR,q)) {

				McNodeLoopInc *pmcnodeloop = (McNodeLoopInc *) QListObj(q);
				PrintMCRouteMembers(pmcnodeloop, &MCRoutesContext); // print node
			} // end for q
			PrintEndMCRoute(&MCRoutesContext); // print closure
		}// end for p
	}

	switch (format) {
		case FORMAT_TEXT:
			printf(" Total Analyzed MC Routes from Entry Switch to HFI: %d\n MC Bad Paths %d\n",totalPaths, badPaths);
			break;
		case FORMAT_XML:
			printf("%*s<AnalyzedMCRoutes>%d</AnalyzedMCRoutes>\n", indent, "",totalPaths);
			printf("%*s<BadMCRoutes>%d</BadMCRoutes>\n", indent, "",badPaths);
			indent-=4;
			break;
		default:
			break;
	}// case end

	switch (format) {
		case FORMAT_TEXT:
			printf("\n");
			DisplaySeparator();
			break;
		case FORMAT_XML:
			printf("%*s</ValidateMCRoutes>\n", indent, "");
			indent-=4;
			break;
		default:
			break;
	}

	// delete MC routes structure
	// deallocate memory for the MC route
	FreeValidateMCRoutes(&g_Fabric);

}	// End of ShowValidateMCRoutesReport()


// Validate all routes for credit loops
void ShowValidateCreditLoopsReport(Format_t format, int indent, int detail) 
{ 
   FSTATUS status; 
   ValidateCreditLoopRoutesContext_t ValidateCreditLoopRoutesContext = { format:format, detail:detail }; 
   
   if (!(g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
      switch (format) {
      case FORMAT_TEXT:
         printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, ""); 
         break; 
      case FORMAT_XML:
         printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, ""); 
         break; 
      default:
         break;
      }
      return;
   }      
   
   switch (format) {
   case FORMAT_TEXT:
      printf("%*sValidate Credit Loop Routes\n", indent, ""); 
      break; 
   case FORMAT_XML:
      printf("\n"); 
      printf("%*s<ValidateCreditLoopRoutes>\n", indent, ""); 
      indent += 4; 
      break; 
   default:
      break;
   }
   
   ValidateCreditLoopRoutesContext.indent = indent; 
   ValidateCreditLoopRoutesContext.quiet = g_quiet; 
   status = ValidateAllCreditLoopRoutes(&g_Fabric, g_portGuid, 
                                        ValidateCLRouteCallback,
                                        ValidateCLFabricSummaryCallback,
                                        ValidateCLDataSummaryCallback,
                                        ValidateCLRouteSummaryCallback,
                                        ValidateCLLinkSummaryCallback,
                                        ValidateCLLinkStepSummaryCallback,
                                        ValidateCLPathSummaryCallback,
                                        ValidateCLTimeGetCallback,
                                        &ValidateCreditLoopRoutesContext, 
                                        ((g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file),
					((g_Fabric.flags & FF_QOSDATA) && g_use_scsc));
   if (status != FSUCCESS) {
      fprintf(stderr, "opareport: -o validateroutes: Unable to validate credit loop routes (status=0x%x): %s\n", status, iba_fstatus_msg(status)); 
      g_exitstatus = 1;
   }
   switch (format) {
   case FORMAT_TEXT:
      DisplaySeparator(); 
      break; 
   case FORMAT_XML:
      indent -= 4; 
      printf("%*s</ValidateCreditLoopRoutes>\n", indent, ""); 
      break; 
   default:
      break;
   }
}   // End of ShowValidateCreditLoopsReport()

// output LID usage in linear FDB (undocumented)
void ShowLIDUsageReport(Point *focus, Format_t format, int indent, int detail)
{
	int ix_lid;
	LIST_ITEM *pList;
	NodeData *nodep;
	SwitchData *switchp;
	uint32 ct_node = 0;
	uint32 ct_lid = 0;
	uint16 usageLinearFDBSize = 0;
	uint32 tb_usageLID[g_max_lft+1];

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sLID Usage\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<LIDUsage>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	if (! (g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -r option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -r option -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u Connected Switch(es) in Fabric%s\n", indent, "",
			(unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}

	// Report LID usage in Linear FDB
	memset(tb_usageLID, 0, (g_max_lft+1) * sizeof(uint32));

	for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL;
			pList = QListNext(&g_Fabric.AllSWs, pList) ) {
		nodep = QListObj(pList);
		switchp = nodep->switchp;

		if (! CompareNodePoint(nodep, focus))
			continue;

		if (!switchp || !switchp->LinearFDB )
			continue;

		if (detail) {
			if (!ct_node) {
				ShowNodeBriefSummaryHeadings(format, indent, 0);
				switch (format) {
				case FORMAT_TEXT:
					printf("%*s  LID Count\n", indent+4, "");
				case FORMAT_XML:
					break;
				default:
					break;
				}
			}
			ShowNodeBriefSummary(nodep, focus, TRUE, format, indent, 0);
			if (switchp->LinearFDBSize > usageLinearFDBSize)
				usageLinearFDBSize = switchp->LinearFDBSize;

			for (ix_lid = 0; ix_lid < switchp->LinearFDBSize; ix_lid++) {
				if (STL_LFT_PORT_BLOCK(switchp->LinearFDB, ix_lid) == 0xFF)
					continue;

				tb_usageLID[ix_lid]++;

			}	// End of for (ix_lid = 0; ix_lid < switchp->LinearFDBSize

		}	// End of if (detail)

		ct_node++;

	}	// End of for ( pList=QListHead(&g_Fabric.AllSWs); pList != NULL

	for (ix_lid = 0; ix_lid < usageLinearFDBSize; ix_lid++)
		if (tb_usageLID[ix_lid])
			switch (format) {
			case FORMAT_TEXT:
				printf( "%*s0x%.*x %u\n", indent+4, "", (ix_lid <= IB_MAX_UCAST_LID ? 4:8) , (uint32)ix_lid,
					tb_usageLID[ix_lid] );
				break;
			case FORMAT_XML:
				if (!ct_lid++)
					printf("%*s<LIDs>\n", indent, "");
				printf( "%*s<Value LID=\"0x%.*x\">%u</Value>\n", indent+4, "", (ix_lid <= IB_MAX_UCAST_LID ? 4:8),
					(uint32)ix_lid, tb_usageLID[ix_lid] );
				break;
			default:
				break;
			}

done:
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Reported Switch(es)\n", indent, "", ct_node);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		if (ct_lid)
			printf("%*s</LIDs>\n", indent, "");
		XmlPrintDec("ReportedSwitchCount", (unsigned)ct_node, indent);
		indent-=4;
		printf("%*s</LIDUsage>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowLIDUsageReport()

// output vFabric information
void ShowVFInfoReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	int cnt = 0;

	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);

	switch (format) {
	case FORMAT_TEXT:
		printf("%*svFabrics:\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<vFabrics>\n", indent, "");
		indent += 4;
		break;
	default:
		break;
	}

	PrintDest_t print;
	PrintDestInitFile(&print, stdout);

	// Report vFabric records
	if (detail) {
		for (p = QListHead(&g_Fabric.AllVFs); p; p = QListNext(&g_Fabric.AllVFs, p), cnt++)
		{
			VFData_t *pVFData = (VFData_t *)QListObj(p);
			STL_VFINFO_RECORD *pR = &pVFData->record;
			char buf[8];

			switch (format) {
			case FORMAT_TEXT:
				if (cnt)
					printf("\n");

				PrintStlVfInfoRecord_detail(&print, indent, detail, pR, 0);
				break;

			case FORMAT_XML:
				printf("%*s<vFabric>\n", indent, "");
				indent += 4;
				XmlPrintDec("Index", pR->vfIndex, indent);
				XmlPrintStr("Name", (char *)pR->vfName, indent);
				// ServiceID and MGID are always zero when SA query asks for
				// all VFs
				//XmlPrintHex64("ServiceID", pR->ServiceID, indent);
				//printf( "%*s<MGID>0x%016"PRIx64":0x%016"PRIx64"</MGID>\n",
				//	indent, "", pR->MGID.AsReg64s.H, pR->MGID.AsReg64s.L );
				XmlPrintPKey("PKey", pR->pKey, indent);
				XmlPrintDec("SL", pR->s1.slBase, indent);


				if (pR->slMulticastSpecified)
					XmlPrintDec("MulticastSL", pR->slMulticast, indent);

				printf( "%*s<Select>%s%s</Select>\n", indent, "",
					(pR->s1.selectFlags & STL_VFINFO_REC_SEL_PKEY_QUERY) ? "PKEY " : "",
					(pR->s1.selectFlags & STL_VFINFO_REC_SEL_SL_QUERY) ? "SL " : "" );
				XmlPrintHex8("Select_Hex", pR->s1.selectFlags, indent);
				if (detail >1) {
					// get the value of Packet Lifetime Multiplier
					snprintf(buf, sizeof(buf), "%d", 1<<pR->s1.pktLifeTimeInc);
					XmlPrintStr( "PktLifeTimeMult",
						pR->s1.pktLifeSpecified ? buf : "unspecified",
						indent );
					if (pR->s1.mtuSpecified)
						XmlPrintDec("MaxMtu", GetBytesFromMtu(pR->s1.mtu), indent);
					else
						XmlPrintStr("MaxMtu", "unlimited", indent);
					XmlPrintStr( "MaxRate",
						pR->s1.rateSpecified ? StlStaticRateToText(pR->s1.rate) : "unlimited",
						indent );
					printf( "%*s<Options>%s%s%s</Options>\n", indent, "",
						(pR->optionFlags & STL_VFINFO_REC_OPT_SECURITY) ? "Security " : "",
						(pR->optionFlags & STL_VFINFO_REC_OPT_QOS) ? "QoS " : "",
						(pR->optionFlags & STL_VFINFO_REC_OPT_FLOW_DISABLE) ? "FlowCtrlDisable " : "" );
					XmlPrintHex8("Options_Hex", pR->optionFlags, indent);
	
					printf("%*s<QOS>\n", indent, "");
					indent += 4;
					if (pR->optionFlags & STL_VFINFO_REC_OPT_QOS)
					{
						XmlPrintDec("Bandwidth_Percent", pR->bandwidthPercent, indent);
						XmlPrintBool("Priority", pR->priority, indent);
					}
					indent -= 4;
					printf("%*s</QOS>\n", indent, "");
					XmlPrintDec("PreemptionRank", pR->preemptionRank, indent);
					FormatTimeoutMult(buf, pR->hoqLife);
					XmlPrintStr("HoQLife", buf, indent);
					XmlPrintDec("HoQLife_Int", pR->hoqLife, indent);
				}
				break;
	
			default:
				break;
			}	// End of switch (format)
	
			// we need QOSDATA to have the SL2SC and PKey tables which are
			// used by isVFMember
			if (detail > 2 && (g_Fabric.flags & FF_QOSDATA)) {
				LIST_ITEM *q;
				cl_map_item_t *r;
				if (format == FORMAT_TEXT)
					printf("%*s     NodeGUID          Port Type Name\n", indent, "");
				// show all endpoints (FIs and switch port 0) which are in
				// the given vFabric
				for (q=QListHead(&g_Fabric.AllFIs); q != NULL; q = QListNext(&g_Fabric.AllFIs, q)) {
					NodeData *nodep = (NodeData *)QListObj(q);
					for (r=cl_qmap_head(&nodep->Ports); r != cl_qmap_end(&nodep->Ports); r = cl_qmap_next(r)) {
						PortData *portp = PARENT_STRUCT(r, PortData, NodePortsEntry);

						if (! ComparePortPoint(portp, focus))
							continue;
						if (isVFMember(portp, pVFData))
							ShowLinkPortBriefSummary(portp, "",  0, NULL,
										format, indent, 0);
					}
				}
				//for all SWs
				for (q=QListHead(&g_Fabric.AllSWs); q != NULL; q = QListNext(&g_Fabric.AllSWs, q)) {
					NodeData *nodep = (NodeData *)QListObj(q);
					PortData *portp = FindNodePort(nodep,0);
					if (!portp)
						continue;
					if (! ComparePortPoint(portp, focus))
						continue;
					if (isVFMember(portp, pVFData))
						ShowLinkPortBriefSummary(portp, "",  0, NULL,
									format, indent, 0);
				}
			}
			switch (format) {
			case FORMAT_TEXT:
				break;
			case FORMAT_XML:
				indent -= 4;
				printf("%*s</vFabric>\n", indent, "");
				break;
			default:
				break;
			}
		}	// End of for each VF
	}

	switch (format) {
	case FORMAT_TEXT:
		if (cnt)
			printf("\n");
		printf("%*s%u VFs\n", indent, "", QListCount(&g_Fabric.AllVFs));
		DisplaySeparator();
		break;
	case FORMAT_XML:
		XmlPrintDec("Count", QListCount(&g_Fabric.AllVFs), indent);
		indent -= 4;
		printf("%*s</vFabrics>\n", indent, "");
		break;
	default:
		break;
	}
}	// End of ShowVFInfoReport()


void ShowBCTForPortText(PortData *port, Format_t format, int indent, int detail)
{
	PortData *rport = port->neighbor;
	uint16_t bytesPerAU = 0;
	uint32_t remLim = 0;
	uint8_t vl;

	indent+=4;

	if (rport) {
		printf("%*sRemote Port 0x%016"PRIx64" %u %s %.*s (LID %u)\n", indent, "",
			rport->nodep->NodeInfo.NodeGUID,
			rport->PortNum,
			StlNodeTypeToText(rport->nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)rport->nodep->NodeDesc.NodeString,
			rport->EndPortLID);
		bytesPerAU = 8 * (1 << rport->PortInfo.BufferUnits.s.BufferAlloc);
		remLim = rport->PortInfo.OverallBufferSpace;
	}

	printf("%*sBufferControlTable\n", indent, "");

	indent +=4;

	if (bytesPerAU)
		printf("%*sOverallBufferSpace   (AU/B):  %6u/%8u\n", indent, "",
				remLim, remLim * bytesPerAU);

	printf("%*sTx Buffer Depth     (LTP/B):  %6u/%8u\n", indent, "",
			port->PortInfo.ReplayDepth.BufferDepth,
			port->PortInfo.ReplayDepth.BufferDepth * BYTES_PER_LTP);

	if (!g_persist && !g_hard)
		printf("%*sWire Depth          (LTP/B):  %6u/%8u\n", indent, "",
				port->PortInfo.ReplayDepth.WireDepth,
				port->PortInfo.ReplayDepth.WireDepth * BYTES_PER_LTP);
	else
		printf("%*sWire Depth          (LTP/B):  xxxxxx/xxxxxxxx\n", indent, "");

	printf("%*sTxOverallSharedLimit (AU/B):  %6u/", indent, "",
			port->pBufCtrlTable->TxOverallSharedLimit);
	if (bytesPerAU)
		printf("%8u\n", port->pBufCtrlTable->TxOverallSharedLimit * bytesPerAU);
	else
		printf(" N/A\n");

	indent += 4;

	printf("%*sVL | Dedicated  (   Bytes) |  Shared  (   Bytes) |  MTU\n", indent, "");

	for (vl = 0; vl < STL_MAX_VLS; vl++) {
		if (port->pBufCtrlTable->VL[vl].TxDedicatedLimit == 0 && port->pBufCtrlTable->VL[vl].TxSharedLimit == 0)
			continue;

		uint32_t dBytes = 0;
		uint32_t sBytes = 0;
		uint16_t mtu = 0;

		if (bytesPerAU) {
			dBytes = port->pBufCtrlTable->VL[vl].TxDedicatedLimit * bytesPerAU;
			sBytes = port->pBufCtrlTable->VL[vl].TxSharedLimit * bytesPerAU;
		}

		if (vl%2 == 0) {
			mtu = port->PortInfo.NeighborMTU[vl/2].s.VL0_to_MTU;
		} else {
			mtu = port->PortInfo.NeighborMTU[vl/2].s.VL1_to_MTU;
		}
		mtu = GetBytesFromMtu(mtu);

		printf("%*s%2d |    %6u  (%8u) |  %6u  (%8u) | %6u\n", indent, "",
				vl,
				port->pBufCtrlTable->VL[vl].TxDedicatedLimit, dBytes,
				port->pBufCtrlTable->VL[vl].TxSharedLimit, sBytes,
				mtu);
	}
}

void ShowBCTForPortXML(PortData *port, Format_t format, int indent, int detail)
{
	PortData *rport = port->neighbor;
	uint16_t bytesPerAU = 0;
	uint32_t remLim = 0;
	uint8_t vl;

	indent+=4;

	if (rport) {
		printf("%*s<RemotePort id=\"0x%016"PRIx64":%u\"/>\n", indent, "",
				rport->nodep->NodeInfo.NodeGUID, rport->PortNum);
		bytesPerAU = 8 * (1 << rport->PortInfo.BufferUnits.s.BufferAlloc);
		remLim = rport->PortInfo.OverallBufferSpace;
	}

	printf("%*s<BufferControlTable>\n", indent, "");

	indent += 4;

	printf("%*s<ReplayDepthBufferDepth>%u</ReplayDepthBufferDepth>\n", indent, "",
			port->PortInfo.ReplayDepth.BufferDepth);
	printf("%*s<ReplayDepthBufferDepthBytes>%u</ReplayDepthBufferDepthBytes>\n", indent, "",
			port->PortInfo.ReplayDepth.BufferDepth * BYTES_PER_LTP);

	if (!g_persist && !g_hard) {
		printf("%*s<ReplayDepthWireDepth>%u</ReplayDepthWireDepth>\n", indent, "",
				port->PortInfo.ReplayDepth.WireDepth);
		printf("%*s<ReplayDepthWireDepthBytes>%u</ReplayDepthWireDepthBytes>\n", indent, "",
				port->PortInfo.ReplayDepth.WireDepth * BYTES_PER_LTP);
	}

	if (bytesPerAU) {
		printf("%*s<OverallBufferSpace>%u</OverallBufferSpace>\n", indent, "", remLim);
		printf("%*s<OverallBufferSpaceBytes>%u</OverallBufferSpaceBytes>\n", indent, "",
				remLim * bytesPerAU);
	}

	printf("%*s<TxOverallSharedLimit>%u</TxOverallSharedLimit>\n", indent, "",
			port->pBufCtrlTable->TxOverallSharedLimit);
	if (bytesPerAU)
		printf("%*s<TxOverallSharedLimitBytes>%u</TxOverallSharedLimitBytes>\n", indent, "",
				port->pBufCtrlTable->TxOverallSharedLimit * bytesPerAU);

	for (vl = 0; vl < STL_MAX_VLS; vl++)
	{
		if (port->pBufCtrlTable->VL[vl].TxDedicatedLimit == 0 && port->pBufCtrlTable->VL[vl].TxSharedLimit == 0)
			continue;

		printf("%*s<VLLimit vl=\"%d\">\n", indent, "", vl);

		indent += 4;

		printf("%*s<TxDedicatedLimit>%u</TxDedicatedLimit>\n", indent, "",
			port->pBufCtrlTable->VL[vl].TxDedicatedLimit);
		if (bytesPerAU)
			printf("%*s<TxDedicatedLimitBytes>%u</TxDedicatedLimitBytes>\n", indent, "",
				port->pBufCtrlTable->VL[vl].TxDedicatedLimit * bytesPerAU);
		printf("%*s<TxSharedLimit>%u</TxSharedLimit>\n", indent, "",
			port->pBufCtrlTable->VL[vl].TxSharedLimit);
		if (bytesPerAU)
			printf("%*s<TxSharedLimitBytes>%u</TxSharedLimitBytes>\n", indent, "",
				port->pBufCtrlTable->VL[vl].TxSharedLimit * bytesPerAU);

		indent -= 4;

		printf("%*s</VLLimit>\n", indent, "");
	}
	indent -= 4;

	printf("%*s</BufferControlTable>\n", indent, "");
}

void ShowAllBCTReports(Point *focus, Format_t format, int indent, int detail)
{
	int ct_port = 0;
	LIST_ITEM *p;

	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
		case FORMAT_TEXT:
			printf("%*sBufferControlTable Report\n", indent, "");
			break;
		case FORMAT_XML:
			indent+=4;
			printf("%*s<BufferControlTableReport>\n", indent, "");
			break;
		default:
			break;
	}

	indent+=4;

	if (g_snapshot_in_file && !(g_Fabric.flags & FF_BUFCTRLTABLE)) {
		switch (format) {
			case FORMAT_TEXT:
				printf("%*sReport skipped: provided snapshot was created without -V option\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<!-- Report skipped: provided snapshot was created without -V option -->\n", indent, "");
				break;
			default:
				break;
		}
		goto done;
	}

	for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
		PortData *port = (PortData *)QListObj(p);

		if (!ComparePortPoint(port, focus))
			continue;

		if (port->nodep->NodeInfo.NodeType == STL_NODE_SW && port->PortNum == 0)
			continue;

		if (! port->pBufCtrlTable)
			continue;

		switch (format) {
			case FORMAT_TEXT:
				printf("%*sPort 0x%016"PRIx64" %u %s %.*s (LID %u)\n", indent, "",
					port->nodep->NodeInfo.NodeGUID,
					port->PortNum,
					StlNodeTypeToText(port->nodep->NodeInfo.NodeType),
					NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)port->nodep->NodeDesc.NodeString,
					port->EndPortLID);

				ShowBCTForPortText(port, format, indent, detail);

				break;
			case FORMAT_XML:
				printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent, "",
						port->nodep->NodeInfo.NodeGUID, port->PortNum);
				XmlPrintHex64("NodeGUID",
						port->nodep->NodeInfo.NodeGUID, indent+4);
				if (port->PortGUID)
					XmlPrintHex64("PortGUID", port->PortGUID, indent+4);
				XmlPrintDec("PortNum", port->PortNum, indent+4);
				XmlPrintNodeType(port->nodep->NodeInfo.NodeType,
								indent+4);
				XmlPrintNodeDesc((char*)port->nodep->NodeDesc.NodeString, indent+4);

				ShowBCTForPortXML(port, format, indent, detail);

				printf("%*s</Port>\n", indent, "");
				break;
			default:
				break;
		}
		ct_port++;
	}

done:
	switch (format) {
	case FORMAT_TEXT:
		indent -= 4;
		printf("%*s%d Reported Port(s)\n", indent, "", ct_port);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedPortCount", (unsigned)ct_port, indent);
		indent -= 4;
		printf("%*s</BufferControlTableReport>\n", indent, "");
		break;
	default:
		break;
	}
}

void CheckVFAllocation(PortData *port, int indent, int format, int detail)
{
	// caller checks FF_QOSDATA, but play it safe
	if (detail <= 3 || !(g_Fabric.flags & FF_QOSDATA))
		return;

	// we need QOSDATA to have the SL2SC and PKey tables which are used by
	// isVFMember
	int sl, sc, vl, ded, share;
	STL_VFINFO_RECORD *pR;
	LIST_ITEM *p;

	uint8_t vfsPerVl[STL_MAX_VLS] = {0};
	uint8_t slsPerVl[STL_MAX_VLS] = {0};
	uint32_t vlSlsMap[STL_MAX_VLS] = {0};
	for (p = QListHead(&g_Fabric.AllVFs); p; p = QListNext(&g_Fabric.AllVFs, p)) {
		VFData_t *pVFData = (VFData_t *)QListObj(p);
		STL_VFINFO_RECORD *pR = &pVFData->record;
		// check that every VF that the port is a member of has a VL assigned to it
		if (!isVFMember(port, pVFData))
			continue;

		sl = pR->s1.slBase;
		sc = port->pQOS->SL2SCMap->SLSCMap[sl].SC;
		vl = port->pQOS->SC2VLMaps[Enum_SCVLt].SCVLMap[sc].VL;
		if (vl == 15) {
			// no VL allocated for this VF
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sNo VL allocated for VF: %s\n", indent, "", pR->vfName);
				break;
			case FORMAT_XML:
				printf("%*s<VFNoVLAllocated>%s</VFNoVLAllocated>", indent, "", pR->vfName);
				break;
			default:
				break;
			}
			continue;
		}

		// check that every VF that the port is a member of has buffers
		// allocated to it
		// skip port 0 since it doesn't get configured for buffers
		// and will confuse user by saying no buffers allocated
		if ((g_Fabric.flags & FF_BUFCTRLTABLE) && port->pBufCtrlTable
			&& port->PortNum) {
			ded = port->pBufCtrlTable->VL[vl].TxDedicatedLimit;
			share = port->pBufCtrlTable->VL[vl].TxSharedLimit;
			if (!ded && !share) {
				switch (format) {
				case FORMAT_TEXT:
					printf("%*sNo buffers allocated for VF: %s\n", indent, "", pR->vfName);
					break;
				case FORMAT_XML:
					printf("%*s<VFNoBufferAllocated>%s</VFNoBufferAllocated>", indent, "", pR->vfName);
					break;
				default:
					break;
				}
			}
		}

		int slSet[2] = {
			pR->s1.slBase,
			(pR->slMulticastSpecified? pR->slMulticast: -1)
		};

		// if qos is enabled, make note that this VL has a vfabric mapped
		// to it, so we can check for contracted links
		if (pR->optionFlags & STL_VFINFO_REC_OPT_QOS) {
			uint32_t vls = 0;

			int i;
			for (i = 0; i < 2; ++i) {
				sl = slSet[i];
				if (sl == -1) continue;

				sc = port->pQOS->SL2SCMap->SLSCMap[sl].SC;
				vl = port->pQOS->SC2VLMaps[Enum_SCVLt].SCVLMap[sc].VL;
				if (vl == 15) continue;


				if (!(vlSlsMap[vl] & (1 << sl))) {
					// Only count VFs that reach this VL via an SL
					// that hasn't been shared before
					if (!(vls & (1 << vl))) {
						// Only count this (VF,VL) pair once
						++vfsPerVl[vl];
						vls |= 1 << vl;
					}

					// Only count this (SL,VL) pair once
					slsPerVl[vl]++;
					vlSlsMap[vl] |= (1 << sl);
				}
			}
		}
	}

	// check for contracted links
	for (vl = 0; vl < STL_MAX_VLS; vl++) {
		if (slsPerVl[vl]>1){
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sContracted link: %d unique SLs in %d unique VFs (not counting shared SLs) mapped to VL %d\n", indent, "", slsPerVl[vl], vfsPerVl[vl], vl);
				break;
			case FORMAT_XML:
				printf("%*s<ContractedLink VL=\"%d\">%d</ContractedLink>\n", indent, "", vl, slsPerVl[vl]);
				break;
			default:
				break;
			}
		}
	}

	// check that every allocated buffer is mapped to a VF
	// skip port 0 since it doesn't get configured for buffers
	// and will confuse user by saying no buffers allocated
	if ((g_Fabric.flags & FF_BUFCTRLTABLE) && port->pBufCtrlTable
		&& port->PortNum) {

		for (vl=0; vl<STL_MAX_VLS; vl++) {
			// skip VL 15
			if (vl==15)
				continue;
			ded = port->pBufCtrlTable->VL[vl].TxDedicatedLimit;
			share = port->pBufCtrlTable->VL[vl].TxSharedLimit;
			if (!ded && !share)
				continue;
			// this VL has buffers dedicated to it
			// first need to find out if there is an SC for this VL
			boolean match_found = FALSE;
			for (sc=0; sc<STL_MAX_SCS; sc++) {
				if (vl == port->pQOS->SC2VLMaps[Enum_SCVLt].SCVLMap[sc].VL) {
					match_found = TRUE;
					break;
				}
			}
			if (match_found) {
				// now find the Sl for this SC
				match_found = FALSE;
				for (sl=0; sl<STL_MAX_SLS; sl++) {
					if (sc == port->pQOS->SL2SCMap->SLSCMap[sl].SC) {
						match_found = TRUE;
						break;
					}
				}
				if (match_found) {
					// now find the vfabric
					match_found = FALSE;
					for (p = QListHead(&g_Fabric.AllVFs); p; p = QListNext(&g_Fabric.AllVFs, p)) {
						pR = &((VFData_t *)QListObj(p))->record;
						if (sl == pR->s1.slBase ||
							(pR->slMulticastSpecified && sl == pR->slMulticast)) {
							match_found = TRUE;
							break;
						}
					}
				}
			}
			// we never found a match
			if (!match_found) {
				switch (format) {
				case FORMAT_TEXT:
					printf("%*sBuffers allocated to VL: %d, but it is not mapped to any VirtualFabric\n",indent, "", vl);
					break;
				case FORMAT_XML:
					printf("%*s<VLExtraBuffersAllocated>%d</VLExtraBuffersAllocated>\n", indent, "", vl);
					break;
				default:
					break;
				}
			}
		}
	}

	if (port->neighbor) {
		// check that SCVLt matches SCVLnt of the neighbor
		for (sc=0; sc<STL_MAX_VLS; sc++) {
			int vl1 = port->pQOS->SC2VLMaps[Enum_SCVLt].SCVLMap[sc].VL;
			int vl2 = port->neighbor->pQOS->SC2VLMaps[Enum_SCVLnt].SCVLMap[sc].VL;
			if (vl1!=vl2) {
				switch (format) {
				case FORMAT_TEXT:
					printf("%*sSCVLt/SCVLnt mapping mismatch:\n", indent, "");
					printf("%*s%s Port %d: SC %d -> VL %d\n", indent+4, "",
						port->nodep->NodeDesc.NodeString, port->PortNum, sc, vl1);
					printf("%*sNeighbor %s Port %d: SC %d -> VL %d\n", indent+4, "",
						port->neighbor->nodep->NodeDesc.NodeString, port->neighbor->PortNum, sc, vl2);
					break;
				case FORMAT_XML:
					printf("%*s<SCVLMismatch>\n", indent, "");
					printf("%*s<Local SC=%d>%d</Local>\n", indent+4, "",
						sc, vl1);
					printf("%*s<Neighbor SC=%d>%d</Neighbor>\n", indent+4, "",
						sc, vl2);
					printf("%*s</SCVLMismatch>\n", indent, "");
					break;
				default:
					break;
				}
			}
		}
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sVF Allocation Validated\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowPortVFMembershipText(PortData *port, int indent, int detail)
{
	int sc, vl, ded, share;
	LIST_ITEM *p;

	// we need QOSDATA to have the SL2SC and PKey tables which are used by
	// isVFMember
	if (! (g_Fabric.flags & FF_QOSDATA))	// caller checks, should be true
		return;
	printf("%*sVF Membership:\n", indent, "");
	indent+=4;
	printf("%*sVF Name\tVF Index\tBaseSL\tBaseSC\tBaseVL", indent, "");
	printf("\t\tMcastSL\tMcastSC\tMcastVL");

	if ((g_Fabric.flags & FF_BUFCTRLTABLE) && port->pBufCtrlTable
		&& detail>2 && port->PortNum)
		printf("\tDedicated\tShared");
	printf("\n");
	// look over every vFabric
	for (p = QListHead(&g_Fabric.AllVFs); p; p = QListNext(&g_Fabric.AllVFs, p)) {
		VFData_t *pVFData = (VFData_t *)QListObj(p);
		STL_VFINFO_RECORD *pR = &pVFData->record;
		if (!isVFMember(port, pVFData))
			continue;
		printf("%*s%-4s\t%d", indent, "", pR->vfName, pR->vfIndex);


		uint8_t sls[2] = {
			pR->s1.slBase,
			(pR->slMulticastSpecified? pR->slMulticast: pR->s1.slBase)
		};

		int i;
		for (i = 0; i < 2; ++i) {
			int sl = sls[i];
			sc = port->pQOS->SL2SCMap->SLSCMap[sl].SC;
			vl = port->pQOS->SC2VLMaps[Enum_SCVLt].SCVLMap[sc].VL;

			if (i > 0 && sl == pR->s1.slBase)
				printf("\t\tN/A\tN/A\tN/A");
			else
				printf("\t\t%d\t%d\t%d", sl, sc, vl);
		}

		if ((g_Fabric.flags & FF_BUFCTRLTABLE) && port->pBufCtrlTable
			&& detail>2 && port->PortNum) {
			ded = port->pBufCtrlTable->VL[vl].TxDedicatedLimit;
			share = port->pBufCtrlTable->VL[vl].TxSharedLimit;
			printf("\t%d\t\t%d", ded, share);
		}
		printf("\n"); 
	}
	printf("\n");
	CheckVFAllocation(port, indent, FORMAT_TEXT, detail);
}

void ShowPortVFMembershipXML(PortData *port, int indent, int detail)
{
	int sc, vl, ded, share;
	LIST_ITEM *p;

	// we need QOSDATA to have the SL2SC and PKey tables which are used by
	// isVFMember
	if (! (g_Fabric.flags & FF_QOSDATA))	// caller checks, should be true
		return;
	printf("%*s<VFMembership>\n", indent, "");
	indent+=4;
	
	for (p = QListHead(&g_Fabric.AllVFs); p; p = QListNext(&g_Fabric.AllVFs, p)) {
		VFData_t *pVFData = (VFData_t *)QListObj(p);
		STL_VFINFO_RECORD *pR = &pVFData->record;
		if (!isVFMember(port, pVFData)) 
			continue;

		printf("%*s<VirtualFabric id=\"%d\">\n", indent, "", pR->vfIndex);
		indent+=4;
		XmlPrintStr("Name", (char *)pR->vfName, indent);
		XmlPrintDec("Index", pR->vfIndex, indent);
		XmlPrintDec("BaseSL", pR->s1.slBase, indent);
		sc = port->pQOS->SL2SCMap->SLSCMap[pR->s1.slBase].SC;
		vl = port->pQOS->SC2VLMaps[Enum_SCVLt].SCVLMap[sc].VL;
		XmlPrintDec("BaseSC", sc, indent);
		XmlPrintDec("VL", vl, indent);


		if (pR->slMulticastSpecified) {
			sc = port->pQOS->SL2SCMap->SLSCMap[pR->slMulticast].SC;
			vl = port->pQOS->SC2VLMaps[Enum_SCVLt].SCVLMap[sc].VL;
			XmlPrintDec("MulticastSL", pR->slMulticast, indent);
			XmlPrintDec("MulticastSC", sc, indent);
			XmlPrintDec("MulticastVL", vl, indent);
		}

		if ((g_Fabric.flags & FF_BUFCTRLTABLE) && port->pBufCtrlTable
			&& detail>2 && port->PortNum) {
			ded = port->pBufCtrlTable->VL[vl].TxDedicatedLimit;
			share = port->pBufCtrlTable->VL[vl].TxSharedLimit;
			XmlPrintDec("DedicatedBuffer", ded, indent);
			XmlPrintDec("SharedBuffer", share, indent);
		}
		indent-=4;
		printf("%*s</VirtualFabric>\n", indent, "");
	}
	indent-=4;
	printf("%*s</VFMembership>\n", indent, "");
	CheckVFAllocation(port, indent, FORMAT_XML, detail);
}

void ShowVFMemberReport(Point *focus, Format_t format, int indent, int detail)
{
	int ct_port = 0;
	LIST_ITEM *p;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sVF Membership Report\n", indent, "");
		break;
	case FORMAT_XML:
		indent +=4;
		printf("%*s<VFMembershipReport>\n", indent, "");
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);

	// we need QOSDATA to have the SL2SC and PKey tables which are used by
	// isVFMember
	if (! (g_Fabric.flags & FF_QOSDATA) && g_snapshot_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: provided snapshot was created without -V option\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: provided snapshot was created without -V option -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}

	for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
		PortData *port = (PortData *)QListObj(p);
		PortData *neighbor = (PortData *)port->neighbor;

		// VF Membership is based on SL2SC tables so N/A to ISLs
		if (port->nodep->NodeInfo.NodeType == STL_NODE_SW && port->PortNum) {
			if (! (neighbor && neighbor->nodep->NodeInfo.NodeType == STL_NODE_FI))
				continue;
		}

		if (!ComparePortPoint(port, focus))
			continue;

		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%.*s\n", indent, "",
				   NODE_DESCRIPTION_ARRAY_SIZE,
				   g_noname?g_name_marker:(char*)port->nodep->NodeDesc.NodeString );
			printf("%*sLID %u\t%s\tNodeGUID 0x%016"PRIx64"\n", indent, "", 
				   port->EndPortLID,
				   StlNodeTypeToText(port->nodep->NodeInfo.NodeType),
				   port->nodep->NodeInfo.NodeGUID );
			printf("%*sPort %u\n", indent, "", port->PortNum);

			if (neighbor) {
				indent+=4;
				printf("%*sNeighbor Node: %.*s\n", indent, "",
				   	NODE_DESCRIPTION_ARRAY_SIZE,
				   	g_noname?g_name_marker:(char*)neighbor->nodep->NodeDesc.NodeString );
				printf("%*sLID %u\t%s\tNodeGUID 0x%016"PRIx64"\n", indent, "",
				   	neighbor->EndPortLID,
				   	StlNodeTypeToText(neighbor->nodep->NodeInfo.NodeType),
				   	neighbor->nodep->NodeInfo.NodeGUID);
				printf("%*sPort %u\n", indent, "", neighbor->PortNum);
				indent-=4;
			}
			if (port->nodep->NodeInfo.NodeType == STL_NODE_SW && port->PortNum) {
				if (neighbor && neighbor->nodep->NodeInfo.NodeType == STL_NODE_FI) {
					// if the node is a switch and the neighbor is a CA,
					// use the mappings from the CA
					ShowPortVFMembershipText(neighbor, indent, detail);
				}
				// if the node is a switch and the neighbor is a switch
				// then there is no VF membership
			} else {
				// node is a CA or switch port 0
				ShowPortVFMembershipText(port, indent, detail);
			}
			printf("\n");
			break;
		case FORMAT_XML:
			indent+=4;
			printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent, "",
				   port->nodep->NodeInfo.NodeGUID, port->PortNum);
			indent+=4;
			XmlPrintHex64("NodeGUID",
						  port->nodep->NodeInfo.NodeGUID, indent);
			XmlPrintDec("PortNum", port->PortNum, indent);
			XmlPrintNodeType(port->nodep->NodeInfo.NodeType, indent);
			XmlPrintNodeDesc((char*)port->nodep->NodeDesc.NodeString, indent);
			if (port->nodep->NodeInfo.NodeType == STL_NODE_SW && port->PortNum) {
				if (neighbor && neighbor->nodep->NodeInfo.NodeType == STL_NODE_FI) {
					// if the node is a switch and the neighbor is a CA,
					// use the mappings from the CA
					ShowPortVFMembershipXML(neighbor, indent, detail);
				}
				// if the node is a switch and the neighbor is a switch
				// then there is no VF membership
			} else {
				// node is a CA or switch port 0
				ShowPortVFMembershipXML(port, indent, detail);
			}
			indent-=4;
			printf("%*s</Port>\n", indent, "");
			indent-=4;
			break;
		default:
			break;
		}
		ct_port++;
	}

	switch (format) {
	case FORMAT_TEXT:
		indent -= 4;
		printf("%*s%d Reported Port(s)\n", indent, "", ct_port);
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedPortCount", (unsigned)ct_port, indent+4);
		break;
	default:
		break;
	}
done:
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		printf("%*s</VFMembershipReport>\n", indent, "");
		break;
	default:
		break;
	}
}

static void printQuarantinedNodeRecord(int indent, const STL_QUARANTINED_NODE_RECORD *pQuarantinedNodeRecord)
{
	int violStrLen = 256;
	char violationString[violStrLen];
	int previousViolation = 0;
	memset(violationString, 0, sizeof(violationString));

	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_SPOOF_GENERIC) {
		strncat(violationString, "NodeGUID/NodeType Spoofing", violStrLen - strlen(violationString));
		previousViolation = 1;
	}
	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_TOPO_NODE_DESC) {
		strncat(violationString, previousViolation ? ", NodeDesc Mismatch" : "NodeDesc Mismatch", 
					violStrLen - strlen(violationString));
		previousViolation = 1;
	}
	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_TOPO_NODE_GUID) {
		strncat(violationString, previousViolation ? ", NodeGUID Mismatch" : "NodeGUID Mismatch", 
					violStrLen - strlen(violationString));
		previousViolation = 1;
	}
	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_TOPO_PORT_GUID) {
		strncat(violationString, previousViolation ? ", PortGUID Mismatch" : "PortGUID Mismatch", 
					violStrLen - strlen(violationString));
		previousViolation = 1;
	}
	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_TOPO_UNDEFINED_LINK) {
		strncat(violationString, previousViolation ? ", Undefined Link" : "Undefined Link", 
					violStrLen - strlen(violationString));
		previousViolation = 1;
	}
	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_SMALL_MTU_SIZE) {
		strncat(violationString, previousViolation ? ", Small MTU Size" : "Small MTU Size", 
					violStrLen - strlen(violationString));
		previousViolation = 1;
	}
	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_VL_COUNT) {
		strncat(violationString, previousViolation ? ", Incorrect VL Count" : "Incorrect VL Count", 
					violStrLen - strlen(violationString));
		previousViolation = 1;
	}

	printf("%*sConnected to Port %d of (LID: 0x%x, NodeGUID: 0x%016" PRIx64 ")\n", indent, "", pQuarantinedNodeRecord->trustedPortNum, pQuarantinedNodeRecord->trustedLid, pQuarantinedNodeRecord->trustedNodeGUID);
	printf("%*s    Offending Node Actual NodeGUID: 0x%016" PRIx64 "\n", indent, "", pQuarantinedNodeRecord->trustedNeighborNodeGUID);
	printf("%*s    Violation(s): %s\n", indent, "", violationString);

	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_TOPO_NODE_DESC) {
		printf("%*s    Expected Node Description: %.*s\n", indent, "", STL_NODE_DESCRIPTION_ARRAY_SIZE, pQuarantinedNodeRecord->expectedNodeInfo.nodeDesc.NodeString);
	}
	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_TOPO_NODE_GUID) {
		printf("%*s    Expected NodeGUID: 0x%016" PRIx64 "\n", indent, "", pQuarantinedNodeRecord->expectedNodeInfo.nodeGUID);
	}
	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_TOPO_PORT_GUID) {
		printf("%*s    Expected PortGUID: 0x%016" PRIx64 "\n", indent, "", pQuarantinedNodeRecord->expectedNodeInfo.portGUID);
	}

	if(pQuarantinedNodeRecord->quarantineReasons & STL_QUARANTINE_REASON_SPOOF_GENERIC) {
		printf("%*s    <SPOOFED NODE REPORTED INFORMATION (May be falsified)>\n", indent, "");
	} else {
		printf("%*s    Received node Information:\n", indent, "");
	}

	printf("%*s        Node Description: %.*s\n", indent, "", NODE_DESCRIPTION_ARRAY_SIZE, pQuarantinedNodeRecord->NodeDesc.NodeString);
	printf("%*s        Type: %s Ports: %d PortNum: %d PartitionCap: %d\n", indent, "", StlNodeTypeToText(pQuarantinedNodeRecord->NodeInfo.NodeType), pQuarantinedNodeRecord->NodeInfo.NumPorts, pQuarantinedNodeRecord->NodeInfo.u1.s.LocalPortNum, pQuarantinedNodeRecord->NodeInfo.PartitionCap);
	printf("%*s        NodeGUID: 0x%016" PRIx64 " PortGUID: 0x%016" PRIx64 "\n", indent, "", pQuarantinedNodeRecord->NodeInfo.NodeGUID, pQuarantinedNodeRecord->NodeInfo.PortGUID);
	printf("%*s        SystemImageGuid: 0x%016" PRIx64 " BaseVersion: %d SmaVersion: %d\n", indent, "", pQuarantinedNodeRecord->NodeInfo.SystemImageGUID, pQuarantinedNodeRecord->NodeInfo.BaseVersion, pQuarantinedNodeRecord->NodeInfo.ClassVersion);
	printf("%*s        VendorID: 0x%x DeviceId: 0x%x Revision: 0x%x\n", indent, "", pQuarantinedNodeRecord->NodeInfo.u1.s.VendorID, pQuarantinedNodeRecord->NodeInfo.DeviceID, pQuarantinedNodeRecord->NodeInfo.Revision);
}

void ShowQuarantineNodeReport(Point *focus, Format_t format, int indent, int detail)
{
	int ix;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;
	STL_QUARANTINED_NODE_RECORD_RESULTS *pQNRR;
	STL_QUARANTINED_NODE_RECORD	*pR;
	struct omgt_port *omgt_port_session = NULL;

	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);

	struct omgt_params params = {.debug_file = g_verbose > 2 ? stdout : NULL};
	if(omgt_open_port_by_guid(&omgt_port_session, g_portGuid, &params) != FSUCCESS)
		return;
	if ( !(( pQueryResults =
			GetAllQuarantinedNodes(omgt_port_session, &g_Fabric, focus, g_quiet) )) )
		return;

	pQNRR = (STL_QUARANTINED_NODE_RECORD_RESULTS *)pQueryResults->QueryResult;
	pR = pQNRR->QuarantinedNodeRecords;
	if (!pQNRR->NumQuarantinedNodeRecords) { /* no quarantined nodes found. */
		return;
	}
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sQuarantinedNodes:\n", indent, "");
		break;

	case FORMAT_XML:
		indent +=4;
		printf("%*s<QuarantinedNodes>\n", indent, "");
		indent +=4;
		break;

	default:
		break;
	}

	// dump quarantined records data
	for (ix = 0; ix < pQNRR->NumQuarantinedNodeRecords; ix++, pR++)
	{
		switch (format) {
		case FORMAT_TEXT:
			if (ix > 0)
				DisplaySeparator();
			printQuarantinedNodeRecord(indent, pR);
			break;

		case FORMAT_XML:
			printf("%*s<QNode>\n", indent, "");
			indent += 4;
			XmlPrintHex64("TrustedNodeGUID", pR->trustedNeighborNodeGUID, indent);
			XmlPrintDec("TrustedPortNum", pR->trustedPortNum, indent);
			XmlPrintLID("TrustedLID", pR->trustedLid, indent);
			XmlPrintHex64("NodeGUID", pR->NodeInfo.NodeGUID, indent);
			XmlPrintHex64("PortGUID", pR->NodeInfo.PortGUID, indent);
			XmlPrintNodeType(pR->NodeInfo.NodeType, indent);
			XmlPrintNodeDesc((char*)pR->NodeDesc.NodeString, indent);
			XmlPrintDec("BaseVer", pR->NodeInfo.BaseVersion, indent);
			XmlPrintDec("SmaVer", pR->NodeInfo.ClassVersion, indent);
			XmlPrintDec("NumPorts", pR->NodeInfo.NumPorts, indent);
			XmlPrintHex64("SystemImageGUID", pR->NodeInfo.SystemImageGUID, indent);
			XmlPrintDec("PartitionCap", pR->NodeInfo.PartitionCap, indent);
			XmlPrintHex("DeviceID", pR->NodeInfo.DeviceID, indent);
			XmlPrintHex("Revision", pR->NodeInfo.Revision, indent);
			XmlPrintHex("VendorID", pR->NodeInfo.u1.s.VendorID, indent);

			if(pR->quarantineReasons) {
				int expectedInfo = 0;

				printf("%*s<QuarantineReasons>\n", indent, "");
				indent += 4;

				if(pR->quarantineReasons & STL_QUARANTINE_REASON_SPOOF_GENERIC) {
					printf("%*s<Reason>NodeGUID/NodeType Spoofing</Reason>\n", indent, "");
				}
				if(pR->quarantineReasons & STL_QUARANTINE_REASON_TOPO_NODE_DESC) {
					printf("%*s<Reason>NodeDesc Mismatch</Reason>\n", indent, "");
					expectedInfo = 1;
				}
				if(pR->quarantineReasons & STL_QUARANTINE_REASON_TOPO_NODE_GUID) {
					printf("%*s<Reason>NodeGUID Mismatch</Reason>\n", indent, "");
					expectedInfo = 1;
				}
				if(pR->quarantineReasons & STL_QUARANTINE_REASON_TOPO_PORT_GUID) {
					printf("%*s<Reason>PortGUID Mismatch</Reason>\n", indent, "");
					expectedInfo = 1;
				}
				if(pR->quarantineReasons & STL_QUARANTINE_REASON_TOPO_UNDEFINED_LINK) {
					printf("%*s<Reason>Undefined Link</Reason>\n", indent, "");
				}
				if(pR->quarantineReasons & STL_QUARANTINE_REASON_SMALL_MTU_SIZE) {
					printf("%*s<Reason>Small MTU Size</Reason>\n", indent, "");
				}
				if(pR->quarantineReasons & STL_QUARANTINE_REASON_VL_COUNT) {
					printf("%*s<Reason>Incorrect VL Count</Reason>\n", indent, "");
				}

				indent -= 4;
				printf("%*s</QuarantineReasons>\n", indent, "");

				if(expectedInfo) {
					printf("%*s<ExpectedInfo>\n", indent, "");
					indent += 4;
					
					if(pR->quarantineReasons & STL_QUARANTINE_REASON_TOPO_NODE_DESC) {
						XmlPrintStrLen("ExpectedNodeDesc", (char*) pR->expectedNodeInfo.nodeDesc.NodeString, STL_NODE_DESCRIPTION_ARRAY_SIZE, indent);
					}
					if(pR->quarantineReasons & STL_QUARANTINE_REASON_TOPO_NODE_GUID) {
						XmlPrintHex64("ExpectedNodeGUID", pR->expectedNodeInfo.nodeGUID, indent);
					}
					if(pR->quarantineReasons & STL_QUARANTINE_REASON_TOPO_PORT_GUID) {
						XmlPrintHex64("ExpectedPortGUID", pR->expectedNodeInfo.portGUID, indent);
					}

					indent -= 4;
					printf("%*s</ExpectedInfo>\n", indent, "");
				}


			}
			
			indent -= 4;
			printf("%*s</QNode>\n", indent, "");
		default:
			break;
		}	// End of switch (format)

	}	// End of for (ix = 0; ix < pQNRR->NumQuarantinedNodeRecords; ix++, pR++)

	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;

	case FORMAT_XML:
		indent -= 4;
		printf("%*s</QuarantinedNodes>\n", indent, "");
		break;

	default:
		break;
	}

	if (pQueryResults != NULL)
		omgt_free_query_result_buffer(pQueryResults);
	if (omgt_port_session != NULL)
		omgt_close_port(omgt_port_session);
}

static void printDGMemberRecord(int indent, const STL_DEVICE_GROUP_MEMBER_RECORD *pRecord)
{
	printf("%*s    LID: 0x%.*x\n", indent, "", (pRecord->LID <= IB_MAX_UCAST_LID ? 4:8) , pRecord->LID);
	printf("%*s    PortNum: %d\n", indent, "", pRecord->Port);
	printf("%*s    PortGUID: 0x%016" PRIx64 "\n", indent, "", pRecord->GUID);
	printf("%*s    NodeDesc: %s\n\n", indent, "", pRecord->NodeDescription.NodeString);
}

void ShowDGMemberReport(Point *focus, Format_t format, int indent, int detail)
{
	int										ix, printDGName=1;
	int										groupIndex, memberIndex;
	PQUERY_RESULT_VALUES					pQueryResults = NULL;
	STL_DEVICE_GROUP_MEMBER_RECORD_RESULTS *pDGMR;
	STL_DEVICE_GROUP_MEMBER_RECORD			*pR;
	struct omgt_port 						*omgt_port_session = NULL;

	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);

	struct omgt_params params = {.debug_file = g_verbose > 2 ? stdout : NULL};
	if(omgt_open_port_by_guid(&omgt_port_session, g_portGuid, &params) != FSUCCESS)
		return;
	if ( !(( pQueryResults =
			GetAllDeviceGroupMemberRecords(omgt_port_session, &g_Fabric, focus, g_quiet) )) )
		return;

	pDGMR = (STL_DEVICE_GROUP_MEMBER_RECORD_RESULTS *)pQueryResults->QueryResult;
	pR = pDGMR->Records;
	if (!pDGMR->NumRecords) { /* no device group member records were found. */
		return;
	}
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sDG Membership Report\n", indent, "");
		break;
	case FORMAT_XML:
		indent +=4;
		printf("%*s<DGMembershipReport>\n", indent, "");
		indent +=4;
		break;
	default:
		break;
	}

	groupIndex=0;
	memberIndex=0;
	// dump device group member record data
	for (ix = 0; ix < pDGMR->NumRecords; ix++, pR++)
	{
		switch (format) {
		case FORMAT_TEXT:
			if (printDGName) {
				++groupIndex;
				if (ix > 0)
					DisplaySeparator();
				printf("%*sDevice Group: %s\n", indent, "", pR->DeviceGroupName);
			}
			printDGMemberRecord(indent, pR);
			printDGName=0;
			// detect last record or last member of device group
			if (ix < (pDGMR->NumRecords-1) &&
				strncmp((char*)&pR->DeviceGroupName, (char*)&((pR+1)->DeviceGroupName), MAX_DG_NAME)) {
				printDGName=1;
			}
			break;

		case FORMAT_XML:
			if (printDGName) {
				printf("%*s<DGGroup id=\"%d\">\n", indent, "", groupIndex);
				++groupIndex;
				memberIndex=0;
				XmlPrintStrLen("Name", (char*)pR->DeviceGroupName, sizeof(pR->DeviceGroupName), indent);
				indent += 4;
			}
			printf("%*s<DGMember id=\"%d\">\n", indent, "", memberIndex);
			++memberIndex;
			XmlPrintLID("LID", pR->LID, indent+4);
			XmlPrintDec("PortNum", pR->Port, indent+4);
			XmlPrintHex64("PortGUID", pR->GUID, indent+4);
			XmlPrintNodeDesc((char*)pR->NodeDescription.NodeString, indent+4);
			printf("%*s</DGMember>\n", indent, "");
			printDGName=0;
			// detect last record or last member of device group
			if ((ix == pDGMR->NumRecords-1) ||
				(ix < (pDGMR->NumRecords-1) &&
				strncmp((char*)&pR->DeviceGroupName, (char*)&((pR+1)->DeviceGroupName), MAX_DG_NAME))) {
				indent -= 4;
				printf("%*s</DGGroup>\n", indent, "");
				printDGName=1;
			}
		default:
			break;
		}	// End of switch (format)

	}	// End of for (ix = 0; ix < pDGMR->NumRecords; ix++, pR++)

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%d Reported Device Group(s)\n", indent+4, "", groupIndex);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		XmlPrintDec("ReportedDeviceGroupCount", (unsigned)groupIndex, indent);
		indent -= 4;
		printf("%*s</DGMembershipReport>\n", indent, "");
		break;
	default:
		break;
	}

	if (pQueryResults != NULL)
		omgt_free_query_result_buffer(pQueryResults);
	if (omgt_port_session != NULL)
		omgt_close_port(omgt_port_session);
}

// command line options, each has a short and long flag name
struct option options[] = {
		// basic controls
		{ "verbose", no_argument, NULL, 'v' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "hfi", required_argument, NULL, 'h' },
		{ "port", required_argument, NULL, 'p' },
		{ "output", required_argument, NULL, 'o' },
		{ "detail", required_argument, NULL, 'd' },
		{ "persist", no_argument, NULL, 'P' },
		{ "hard", no_argument, NULL, 'H' },
		{ "noname", no_argument, NULL, 'N' },
		{ "stats", no_argument, NULL, 's' },
		{ "interval", no_argument, NULL, 'i' },
		{ "clear", no_argument, NULL, 'C' },
		{ "clearall", no_argument, NULL, 'a' },
		{ "smadirect", no_argument, NULL, 'm' },
		{ "pmadirect", no_argument, NULL, 'M' },
		{ "routes", no_argument, NULL, 'r' },
		{ "limit", no_argument, NULL, 'L' },
		{ "config", no_argument, NULL, 'c' },
		{ "focus", no_argument, NULL, 'F' },
		{ "src", no_argument, NULL, 'S' },
		{ "dest", no_argument, NULL, 'D' },
		{ "xml", no_argument, NULL, 'x' },
		{ "infile", no_argument, NULL, 'X' },
		{ "topology", no_argument, NULL, 'T' },
		{ "quietfocus", no_argument, NULL, 'Q' },
		{ "vltables", no_argument, NULL, 'V' },
		{ "allports", no_argument, NULL, 'A' },
		{ "begin", required_argument, NULL, 'b'},
		{ "end", required_argument, NULL, 'e'},
		{ "help", no_argument, NULL, '$' },	// use an invalid option character

		{ 0 }
};

void Usage_full(void)
{
	fprintf(stderr, "Usage: opareport [-v][-q] [-h hfi] [-p port] [-o report] [-d detail]\n"
	                "                    [-P|-H] [-N] [-x] [-X snapshot_input] [-T topology_input]\n"
	                "                    [-s] [-r] [-V] [-i seconds] [-b date_time] [-e date_time]\n"
	                "                    [-C] [-a] [-m] [-M] [-A] [-c file] [-L] [-F point]\n"
	                "                    [-S point] [-D point] [-Q]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opareport --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -q/--quiet                - disable progress reports\n");
	fprintf(stderr, "    -h/--hfi hfi              - hfi, numbered 1..n, 0= -p port will be a\n");
	fprintf(stderr, "                                system wide port num (default is 0)\n");
	fprintf(stderr, "    -p/--port port            - port, numbered 1..n, 0=1st active\n");
	fprintf(stderr, "                                (default is 1st active)\n");
	fprintf(stderr, "    -o/--output report        - report type for output\n");
	fprintf(stderr, "    -d/--detail level         - level of detail 0-n for output, default is 2\n");
	fprintf(stderr, "    -P/--persist              - only include data persistent across reboots\n");
	fprintf(stderr, "    -H/--hard                 - only include permanent hardware data\n");
	fprintf(stderr, "    -N/--noname               - omit node and IOC names\n");
	fprintf(stderr, "    -x/--xml                  - output in xml\n");
	fprintf(stderr, "    -X/--infile snapshot_input\n");
	fprintf(stderr, "                              - generate report using data in snapshot_input\n");
	fprintf(stderr, "                                snapshot_input must have been generated via\n");
	fprintf(stderr, "                                previous -o snapshot run.\n");
	fprintf(stderr, "                                When used, -s, -i, -C and -a options are ignored\n");
	fprintf(stderr, "                                '-' may be used to specify stdin\n");
	fprintf(stderr, "    -T/--topology topology_input\n");
	fprintf(stderr, "                              - use topology_input file to augment and\n");
	fprintf(stderr, "                                verify fabric information.  When used various\n");
	fprintf(stderr, "                                reports can be augmented with information\n");
	fprintf(stderr, "                                not available electronically (such as cable\n");
	fprintf(stderr, "                                labels).\n");
	fprintf(stderr, "                                '-' may be used to specify stdin\n");
	fprintf(stderr, "    -s/--stats                - get performance stats for all ports\n");
	fprintf(stderr, "    -i/--interval seconds     - obtain performance stats over interval seconds\n");
	fprintf(stderr, "                                clears all stats, waits interval seconds,\n");
	fprintf(stderr, "                                then generates report.  Implies -s\n");
	fprintf(stderr, "    -b/--begin date_time      - obtain past performance stats over interval\n");
	fprintf(stderr, "                                beginning at date_time. Implies -s\n");
	fprintf(stderr, "    -e/--end date_time        - obtain past performance stats over interval\n");
	fprintf(stderr, "                                ending at date_time. Implies -s\n");
	fprintf(stderr, "                                For both -b and -e, date_time may be a time\n");
	fprintf(stderr, "                                entered as HH:MM[:SS] or date as mm/dd/YYYY,\n");
	fprintf(stderr, "                                dd.mm.YYYY, YYYY-mm-dd or date followed by time\n");
	fprintf(stderr, "                                e.g. \"2016-07-04 14:40\". Relative times are\n");
	fprintf(stderr, "                                taken as \"x [second|minute|hour|day](s) ago.\"\n");
	fprintf(stderr, "    -C/--clear                - clear performance stats for all ports\n");
	fprintf(stderr, "                              - only stats with error thresholds are cleared\n");
	fprintf(stderr, "                              - clear occurs after generating report\n");
	fprintf(stderr, "    -a/--clearall             - clear all performance stats for all ports\n");
	fprintf(stderr, "    -m/--smadirect            - access fabric information directly from SMA\n");
	fprintf(stderr, "    -M/--pmadirect            - access performance stats via direct PMA\n");
	fprintf(stderr, "    -A/--allports             - also get PortInfo for down switch ports.\n");
	fprintf(stderr, "                                uses direct SMA to get this data.\n");
	fprintf(stderr, "                                If used with -M will also get PMA stats\n");
	fprintf(stderr, "                                for down switch ports.\n");
	fprintf(stderr, "    -c/--config file          - error thresholds config file, default is\n");
	fprintf(stderr, "                                %s\n", CONFIG_FILE);
	fprintf(stderr, "    -L/--limit                - For port error counters check (-o errors)\n");
	fprintf(stderr, "                                and port counters clear (-C or -i) with -F\n");
	fprintf(stderr, "                                limit operation to exact specified focus.\n");
	fprintf(stderr, "                                Normally the neighbor of each selected\n");
	fprintf(stderr, "                                port would also be checked/cleared\n");
	fprintf(stderr, "                                Does not affect other reports\n");
	fprintf(stderr, "    -F/--focus point          - focus area for report\n");
	fprintf(stderr, "                                Limits output to reflect a subsection of\n");
	fprintf(stderr, "                                the fabric. May not work with all reports.\n");
	fprintf(stderr, "                                (For example, route, mcgroups, and the verify*\n");
	fprintf(stderr, "                                reports may ignore the option or not generate\n");
	fprintf(stderr, "                                useful results.)\n");
	fprintf(stderr, "    -S/--src point            - source for trace route, default is local port\n");
	fprintf(stderr, "    -D/--dest point           - destination for trace route\n");
	fprintf(stderr, "    -Q/--quietfocus           - do not include focus description in report\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0                      - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0                 - 1st active port in system\n");
	fprintf(stderr, "    -h x                      - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0                 - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y                 - port y within system (irrespective of which\n");
	fprintf(stderr, "                                ports are active)\n");
	fprintf(stderr, "    -h x -p y                 - HFI x, port y\n");
	fprintf(stderr, "Snapshot specific Options:\n");
	fprintf(stderr, "    -r/--routes               - get routing tables for all switches\n");
	fprintf(stderr, "    -V/--vltables             - get the P-Key tables for all nodes and\n");
	fprintf(stderr, "                                QOS VL-related tables for all ports\n");
	fprintf(stderr, "Report Types:\n");
	fprintf(stderr, "    comps                     - summary of all systems and SMs in fabric\n");
	fprintf(stderr, "    brcomps                   - brief summary of all systems and SMs in fabric\n");
	fprintf(stderr, "    nodes                     - summary of all node types and SMs in fabric\n");
	fprintf(stderr, "    brnodes                   - brief summary of all node types and SMs in\n");
	fprintf(stderr, "                                fabric\n");
	fprintf(stderr, "    ious                      - summary of all IO Units in fabric\n");
	fprintf(stderr, "    lids                      - summary of all LIDs in fabric\n");
	fprintf(stderr, "    links                     - summary of all links\n");
	fprintf(stderr, "    extlinks                  - summary of links external to systems\n");
	fprintf(stderr, "    filinks                   - summary of links to FIs\n");
	fprintf(stderr, "    islinks                   - summary of inter-switch links\n");
	fprintf(stderr, "    extislinks                - summary of inter-switch links external to systems\n");
	fprintf(stderr, "    slowlinks                 - summary of links running slower than expected\n");
	fprintf(stderr, "    slowconfiglinks           - summary of links configured to run slower than\n");
	fprintf(stderr, "                                supported includes slowlinks\n");
	fprintf(stderr, "    slowconnlinks             - summary of links connected with mismatched speed\n");
	fprintf(stderr, "                                potential includes slowconfiglinks\n");
	fprintf(stderr, "    misconfiglinks            - summary of links configured to run slower than\n");
	fprintf(stderr, "                                supported\n");
	fprintf(stderr, "    misconnlinks              - summary of links connected with mismatched speed\n");
	fprintf(stderr, "                                potential\n");
	fprintf(stderr, "    errors                    - summary of links whose errors exceed counts in\n");
        fprintf(stderr, "                                config file\n");
	fprintf(stderr, "    otherports                - summary of ports not connected to this fabric\n");
	fprintf(stderr, "    linear                    - summary of linear forwarding data base (FDB) for\n");
        fprintf(stderr, "                                each Switch\n");
	fprintf(stderr, "    mcast                     - summary of multicast FDB for each Switch\n");
	fprintf(stderr, "    mcgroups                  - summary of multicast groups\n");
	fprintf(stderr, "    portusage                 - summary of ports referenced in linear FDB for\n");
        fprintf(stderr, "                                each Switch, broken down by NodeType of DLID\n");
	fprintf(stderr, "    pathusage                 - summary of number of FI to FI paths routed\n");
	fprintf(stderr, "                                through each switch port\n");
	fprintf(stderr, "    treepathusage             - analysis of number of FI to FI paths routed\n");
	fprintf(stderr, "                                through each switch port for a fat tree\n");
	fprintf(stderr, "    portgroups                - summary of adaptive routing port groups for each\n");
	fprintf(stderr, "                                Switch\n");
	fprintf(stderr, "    quarantinednodes          - summary of quarantined nodes\n");
	fprintf(stderr, "    validateroutes            - validate all routes in the fabric\n");
	fprintf(stderr, "    validatevlroutes          - validate all routes in the fabric using SLSC,\n");
	fprintf(stderr, "                                SCSC, and SCVL tables\n");
	fprintf(stderr, "    validatepgs               - validate all port groups in the fabric\n");
	fprintf(stderr, "    validatecreditloops       - validate topology configuration of the fabric to\n");
	fprintf(stderr, "                                identify any existing credit loops\n");
	fprintf(stderr, "    validatevlcreditloops     - validate topology configuration of the fabric\n");
	fprintf(stderr, "                                including SLSC, SCSC, and SCVL tables to identify\n");
	fprintf(stderr, "                                any existing credit loops\n");
	fprintf(stderr, "    validatemcroutes          - validate multicast routes of the fabric to\n");
	fprintf(stderr, "                                identify loops in multicast forwarding tables and\n");
	fprintf(stderr, "                                detect MFT-multicast membership inconsistencies.\n");
	fprintf(stderr, "    vfinfo                    - summary of vFabric information\n");
	fprintf(stderr, "    vfmember                  - summary of vFabric membership information\n");
	fprintf(stderr, "    verifyfis                 - compare fabric (or snapshot) FIs to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "    verifysws                 - compare fabric (or snapshot) Switches to\n");
	fprintf(stderr, "                                supplied topology and identify differences and\n");
	fprintf(stderr, "                                omissions\n");
	fprintf(stderr, "    verifynodes               - verifyfis and verifysws reports\n");
	fprintf(stderr, "    verifysms                 - compare fabric (or snapshot) SMs to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "    verifylinks               - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "    verifyextlinks            - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "                                limit analysis to links external to systems\n");
	fprintf(stderr, "    verifyfilinks             - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "                                limit analysis to links to FIs\n");
	fprintf(stderr, "    verifyislinks             - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "                                limit analysis to inter-switch links\n");
	fprintf(stderr, "    verifyextislinks          - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "                                limit analysis to inter-switch links external to\n");
	fprintf(stderr, "                                systems\n");
	fprintf(stderr, "    verifyall                 - verifyfis, verifysws, verifysms and verifylinks\n");
	fprintf(stderr, "                                reports\n");
	fprintf(stderr, "    all                       - comp, nodes, ious, links, extlinks,\n");
	fprintf(stderr, "                                slowconnlinks, and errors reports\n");
	fprintf(stderr, "    route                     - trace route between -S and -D points\n");
	fprintf(stderr, "    bfrctrl                   - report Buffer Control Tables for all ports\n");
	fprintf(stderr, "    snapshot                  - output snapshot of fabric state for later use as\n");
	fprintf(stderr, "                                snapshot_input implies -x.  May not be combined\n");
	fprintf(stderr, "                                with other reports. When selected, -F, -P, -H,\n");
	fprintf(stderr, "                                -N options are ignored\n");
	fprintf(stderr, "    topology                  - output the topology of the fabric for later use\n");
	fprintf(stderr, "                                as topology_input implies -x.  May not be\n");
	fprintf(stderr, "                                combined with other reports\n");
	fprintf(stderr, "                                Use with detail level 3 or more to get Port element\n");
	fprintf(stderr, "                                under Node in output xml\n");
	fprintf(stderr, "    none                      - no report, useful if just want to clear stats\n");
	fprintf(stderr, "Point Syntax:\n");
	fprintf(stderr, "   gid:value                  - value is numeric port gid of form: subnet:guid\n");
	fprintf(stderr, "   lid:value                  - value is numeric lid\n");
	fprintf(stderr, "   lid:value:node             - value is numeric lid, selects node with given\n");
	fprintf(stderr, "                                lid\n");
	fprintf(stderr, "   lid:value:port:value2      - value is numeric lid of node, value2 is port #\n");
	fprintf(stderr, "   portguid:value             - value is numeric port guid\n");
	fprintf(stderr, "   nodeguid:value             - value is numeric node guid\n");
	fprintf(stderr, "   nodeguid:value1:port:value2\n");
        fprintf(stderr, "                              - value1 is numeric node guid, value2 is port #\n");
	fprintf(stderr, "   iocguid:value              - value is numeric IOC guid\n");
	fprintf(stderr, "   iocguid:value1:port:value2 - value1 is numeric IOC guid, value2 is port #\n");
	fprintf(stderr, "   systemguid:value           - value is numeric system image guid\n");
	fprintf(stderr, "   systemguid:value1:port:value2\n");
        fprintf(stderr, "                              - value1 is numeric system image guid\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   ioc:value                  - value is IOC Profile ID String (IOC Name)\n");
	fprintf(stderr, "   ioc:value1:port:value2     - value1 is IOC Profile ID String (IOC Name)\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   iocpat:value               - value is glob pattern for IOC Profile ID String\n");
        fprintf(stderr, "                                (IOC Name)\n");
	fprintf(stderr, "   iocpat:value1:port:value2  - value1 is glob pattern for IOC Profile ID String\n");
	fprintf(stderr, "                                (IOC Name), value2 is port #\n");
	fprintf(stderr, "   ioctype:value              - value is IOC type (SRP or OTHER)\n");
	fprintf(stderr, "   ioctype:value1:port:value2 - value1 is IOC type (SRP or OTHER)\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   node:value                 - value is node description (node name)\n");
	fprintf(stderr, "   node:value1:port:value2    - value1 is node description (node name)\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   nodepat:value              - value is glob pattern for node description (node\n");
	fprintf(stderr, "                                name)\n");
	fprintf(stderr, "   nodepat:value1:port:value2 - value1 is glob pattern for node description\n");
	fprintf(stderr, "                                (node name), value2 is port #\n");
	fprintf(stderr, "   nodedetpat:value           - value is glob pattern for node details\n");
	fprintf(stderr, "   nodedetpat:value1:port:value2\n");
        fprintf(stderr, "                              - value1 is glob pattern for node details,\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   nodetype:value             - value is node type (SW, FI or RT)\n");
	fprintf(stderr, "   nodetype:value1:port:value2\n");
        fprintf(stderr, "                              - value1 is node type (SW, FI or RT)\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   rate:value                 - value is string for rate (25g, 50g, 75g, 100g)\n");
	fprintf(stderr, "                                omits switch mgmt port 0\n");
	fprintf(stderr, "   portstate:value            - value is string for state (down, init, armed,\n");
	fprintf(stderr, "                                active, notactive, initarmed)\n");
	fprintf(stderr, "   portphysstate:value        - value is string for phys state (polling,\n");
	fprintf(stderr, "                                disabled, training, linkup, recovery, offline,\n");
	fprintf(stderr, "                                test)\n");
	fprintf(stderr, "   mtucap:value               - value is MTU size (2048, 4096, 8192, 10240)\n");
	fprintf(stderr, "                                omits switch mgmt port 0\n");
	fprintf(stderr, "   labelpat:value             - value is glob pattern for cable label\n");
	fprintf(stderr, "   lengthpat:value            - value is glob pattern for cable length\n");
	fprintf(stderr, "   cabledetpat:value          - value is glob pattern for cable details\n");
	fprintf(stderr, "   cabinflenpat:value         - value is glob pattern for cable info length\n");
	fprintf(stderr, "   cabinfvendnamepat:value    - value is glob pattern for cable info vendor name\n");
	fprintf(stderr, "   cabinfvendpnpat:value      - value is glob pattern for cable info vendor PN\n");
	fprintf(stderr, "   cabinfvendrevpat:value     - value is glob pattern for cable info vendor rev\n");
	fprintf(stderr, "   cabinfvendsnpat:value      - value is glob pattern for cable info vendor SN\n");
	fprintf(stderr, "   cabinftype:value           - value is either 'optical', 'passive_copper', \n");
	fprintf(stderr, "                                'active_copper' or 'unknown' \n");
	fprintf(stderr, "   linkdetpat:value           - value is glob pattern for link details\n");
	fprintf(stderr, "   portdetpat:value           - value is glob pattern for port details\n");
	fprintf(stderr, "   sm                         - master SM\n");
	fprintf(stderr, "   smdetpat:value             - value is glob pattern for sm details\n");
	fprintf(stderr, "   route:point1:point2        - all ports along the routes between the 2 given\n");
	fprintf(stderr, "                                points\n");
	fprintf(stderr, "   led:value                  - value is either 'on' or 'off' for LED port beacon\n");
	fprintf(stderr, "   linkqual:value             - ports with a link quality equal to value\n");
	fprintf(stderr, "   linkqualLE:value           - ports with a link quality less than or equal to\n");
	fprintf(stderr, "                                value\n");
	fprintf(stderr, "   linkqualGE:value           - ports with a link quality greater than or equal\n");
	fprintf(stderr, "                                to value\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "   opareport -o comps -d 3\n");
	fprintf(stderr, "   opareport -o errors -o slowlinks\n");
	fprintf(stderr, "   opareport -o nodes -F portguid:0x00066a00a000447b\n");
	fprintf(stderr, "   opareport -o nodes -F nodeguid:0x001175019800447b:port:1\n");
	fprintf(stderr, "   opareport -o nodes -F nodeguid:0x001175019800447b\n");
	fprintf(stderr, "   opareport -o nodes -F 'node:duster hfi1_0'\n");
	fprintf(stderr, "   opareport -o nodes -F 'node:duster hfi1_0:port:1'\n");
	fprintf(stderr, "   opareport -o nodes -F 'nodepat:d*'\n");
	fprintf(stderr, "   opareport -o nodes -F 'nodepat:d*:port:1'\n");
	fprintf(stderr, "   opareport -o nodes -F 'nodedetpat:compute*'\n");
	fprintf(stderr, "   opareport -o nodes -F 'nodedetpat:compute*:port:1'\n");
	fprintf(stderr, "   opareport -o nodes -F nodetype:FI\n");
	fprintf(stderr, "   opareport -o nodes -F nodetype:FI:port:1\n");
	fprintf(stderr, "   opareport -o nodes -F lid:1\n");
	fprintf(stderr, "   opareport -o nodes -F led:on\n");
	fprintf(stderr, "   opareport -o nodes -F led:off\n");
	fprintf(stderr, "   opareport -o nodes -F lid:1:node\n");
	fprintf(stderr, "   opareport -o nodes -F lid:1:port:2\n");
	fprintf(stderr, "   opareport -o nodes -F gid:0xfe80000000000000:0x00066a00a000447b\n");
	fprintf(stderr, "   opareport -o nodes -F systemguid:0x001175019800447b\n");
	fprintf(stderr, "   opareport -o nodes -F systemguid:0x001175019800447b:port:1\n");
	fprintf(stderr, "   opareport -o nodes -F iocguid:0x00117501300001e0\n");
	fprintf(stderr, "   opareport -o nodes -F iocguid:0x00117501300001e0:port:2\n");
	fprintf(stderr, "   opareport -o nodes -F 'ioc:Chassis 0x00066A005000010C, Slot 2, IOC 1'\n");
	fprintf(stderr, "   opareport -o nodes -F 'ioc:Chassis 0x00066A005000010C, Slot 2, IOC 1:port:2'\n");
	fprintf(stderr, "   opareport -o nodes -F 'iocpat:*Slot 2*'\n");
	fprintf(stderr, "   opareport -o nodes -F 'iocpat:*Slot 2*:port:2'\n");
	fprintf(stderr, "   opareport -o nodes -F ioctype:SRP\n");
	fprintf(stderr, "   opareport -o nodes -F ioctype:SRP:port:2\n");
	fprintf(stderr, "   opareport -o extlinks -F rate:100g\n");
	fprintf(stderr, "   opareport -o extlinks -F portstate:armed\n");
	fprintf(stderr, "   opareport -o extlinks -F portphysstate:linkup\n");
	fprintf(stderr, "   opareport -o extlinks -F 'labelpat:S1345*'\n");
	fprintf(stderr, "   opareport -o extlinks -F 'lengthpat:11m'\n");
	fprintf(stderr, "   opareport -o extlinks -F 'cabledetpat:*hitachi*'\n");
	fprintf(stderr, "   opareport -o extlinks -F 'linkdetpat:*core ISL*'\n");
	fprintf(stderr, "   opareport -o extlinks -F 'portdetpat:*mgmt*'\n");
	fprintf(stderr, "   opareport -o links -F mtucap:2048\n");
	fprintf(stderr, "   opareport -o nodes -F sm\n");
	fprintf(stderr, "   opareport -o nodes -F 'smdetpat:primary*'\n");
	fprintf(stderr, "   opareport -o nodes -F 'route:node:duster hfi1_0:node:cuda hfi1_0'\n");
	fprintf(stderr, "   opareport -o nodes -F \\\n");
	fprintf(stderr, "       'route:node:duster hfi1_0:port:1:node:cuda hfi1_0:port:2'\n");
	fprintf(stderr, "   opareport -s -o snapshot > file\n");
	fprintf(stderr, "   opareport -o topology > topology.xml\n");
	fprintf(stderr, "   opareport -o errors -X file\n");
	fprintf(stderr, "   opareport -s --begin \"2 days ago\"\n");
	fprintf(stderr, "   opareport -s --begin \"12:30\" --end \"14:00\"\n");
	exit(0);
}

void Usage(void)
{
	fprintf(stderr, "Usage: opareport [-v][-q] [-o report] [-d detail] [-x] [-s] [-i seconds] [-C]\n");
	fprintf(stderr, "                 [-b date_time] [-e date_time]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opareport --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -q/--quiet                - disable progress reports\n");
	fprintf(stderr, "    -o/--output report        - report type for output\n");
	fprintf(stderr, "    -d/--detail level         - level of detail 0-n for output, default is 2\n");
	fprintf(stderr, "    -x/--xml                  - output in xml\n");
	fprintf(stderr, "    -s/--stats                - get performance stats for all ports\n");
	fprintf(stderr, "    -i/--interval seconds     - obtain performance stats over interval seconds\n");
	fprintf(stderr, "                                clears all stats, waits interval seconds,\n");
	fprintf(stderr, "                                then generates report.  Implies -s\n");
	fprintf(stderr, "    -b/--begin date_time      - obtain past performance stats over interval\n");
	fprintf(stderr, "                                beginning at date_time. Implies -s\n");
	fprintf(stderr, "    -e/--end date_time        - obtain past performance stats over interval\n");
	fprintf(stderr, "                                ending at date_time. Implies -s\n");
	fprintf(stderr, "    -C/--clear                - clear performance stats for all ports\n");
	fprintf(stderr, "                              - only stats with error thresholds are cleared\n");
	fprintf(stderr, "                              - clear occurs after generating report\n");
	fprintf(stderr, "Report Types (abridged):\n");
	fprintf(stderr, "    comps                     - summary of all systems and SMs in fabric\n");
	fprintf(stderr, "    brcomps                   - brief summary of all systems and SMs in fabric\n");
	fprintf(stderr, "    nodes                     - summary of all node types and SMs in fabric\n");
	fprintf(stderr, "    brnodes                   - brief summary of all node types and SMs in\n");
	fprintf(stderr, "                                fabric\n");
	fprintf(stderr, "    ious                      - summary of all IO Units in fabric\n");
	fprintf(stderr, "    lids                      - summary of all LIDs in fabric\n");
	fprintf(stderr, "    links                     - summary of all links\n");
	fprintf(stderr, "    extlinks                  - summary of links external to systems\n");
	fprintf(stderr, "    slowlinks                 - summary of links running slower than expected\n");
	fprintf(stderr, "    slowconfiglinks           - summary of links configured to run slower than\n");
	fprintf(stderr, "                                supported includes slowlinks\n");
	fprintf(stderr, "    slowconnlinks             - summary of links connected with mismatched speed\n");
	fprintf(stderr, "                                potential includes slowconfiglinks\n");
	fprintf(stderr, "    misconfiglinks            - summary of links configured to run slower than\n");
	fprintf(stderr, "                                supported\n");
	fprintf(stderr, "    misconnlinks              - summary of links connected with mismatched speed\n");
	fprintf(stderr, "                                potential\n");
	fprintf(stderr, "    errors                    - summary of links whose errors exceed counts in\n");
	fprintf(stderr, "                                config file\n");
	fprintf(stderr, "    otherports                - summary of ports not connected to this fabric\n");
	fprintf(stderr, "    all                       - comp, nodes, ious, links, extlinks,\n");
	fprintf(stderr, "                                slowconnlinks, and error reports\n");
	fprintf(stderr, "    none                      - no report, useful if just want to clear stats\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "   opareport -o comps -d 3\n");
	fprintf(stderr, "   opareport -o errors -o slowlinks\n");
	exit(2);
}

int parse(const char* filename)
{
	FILE *fp = NULL;
	char param[80];
	unsigned long long threshold;
	int ret;
	char buffer[81];
	int skipping = 0;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "opareport: Can't open %s: %s\n", filename, strerror(errno));
		return -1;
	}
	while (NULL != fgets(buffer, sizeof(buffer), fp))
	{
		// just ignore long lines
		if (buffer[strlen(buffer)-1] != '\n') {
			skipping=1;
			continue;
		}
		if (skipping) {
			skipping = 0;
			continue;
		}
		ret = sscanf(buffer, "%70s\n", param);
		if (ret != 1)
			continue;	// blank line
		if ( param[0]=='#')
			continue; // ignore comments
		if (strcmp(param, "Threshold") == 0) {
			char compare[21];
			ret = sscanf(buffer,"%70s %20s\n", param, compare);
			if (ret != 2) {
				fprintf(stderr, "opareport: Invalid Config Line: %s, ignoring\n", buffer);
				continue;
			}
			if (strcmp(compare, "Greater") == 0) {
				g_threshold_compare = 0;
			} else if (strcmp(compare, "Equal") == 0) {
				g_threshold_compare = 1;
			} else {
				fprintf(stderr, "opareport: Invalid Threshold: %s, ignoring\n", compare);
			}
			continue;
		}
		ret = sscanf(buffer, "%70s %llu\n", param, &threshold);
		if (ret == 2) {
			if (param[0]=='#') {
				// ignore comments
			} else if (strcmp(param,"LinkQualityIndicator") == 0) {
				if (threshold > 5) {
					fprintf(stderr, "opareport: LinkQualityIndicator max threshold setting is 5, ignoring: %llu\n", threshold);
				} else {
					g_Thresholds.lq.s.LinkQualityIndicator = threshold;
					/* can't be cleared. */
				}
			} else if (strcmp(param,"NumLanesDown") == 0) {
				if (threshold > 4) {
					fprintf(stderr, "opareport: NumLanesDown max threshold setting is 4, ignoring: %llu\n", threshold);
				} else {
					g_Thresholds.lq.AsReg8 |= (threshold << 4);
				}
#define PARSE_THRESHOLD(field, name, max) \
	if (strcmp(param, #name) == 0) { \
		if (threshold > (max)) { \
			fprintf(stderr, "opareport: " #name " max threshold is %u, ignoring: %llu\n", (max), threshold); \
		} else { \
			g_Thresholds.field = threshold; \
			if (threshold) { \
				g_CounterSelectMask.CounterSelectMask.s.field = 1; \
			} \
		} \
	}
#define PARSE_THRESHOLD64(field, name) \
	if (strcmp(param, #name) == 0) { \
			g_Thresholds.field = threshold; \
			if (threshold) { \
				g_CounterSelectMask.CounterSelectMask.s.field = 1; \
			} \
		}
#define PARSE_MB_THRESHOLD(field, name) \
	if (strcmp(param, #name) == 0) { \
		if (threshold > ((1ULL << 63)-1)/(FLITS_PER_MB/2)) { \
			fprintf(stderr, "opareport: " #name " max threshold is %llu, ignoring: %llu\n", ((1ULL << 63)-1)/(FLITS_PER_MB/2), threshold); \
		} else { \
			threshold = threshold * FLITS_PER_MB; \
			g_Thresholds.field = threshold; \
			if (threshold) { \
				g_CounterSelectMask.CounterSelectMask.s.field = 1; \
			} \
		} \
	}
			// Data movement
			} else PARSE_MB_THRESHOLD(PortXmitData, XmitData)
			else PARSE_MB_THRESHOLD(PortRcvData, RcvData)
			else PARSE_THRESHOLD64(PortXmitPkts, XmitPkts)
			else PARSE_THRESHOLD64(PortRcvPkts, RcvPkts)
			else PARSE_THRESHOLD64(PortMulticastXmitPkts, MulticastXmitPkts)
			else PARSE_THRESHOLD64(PortMulticastRcvPkts, MulticastRcvPkts)
			// Signal Integrity and Node/Link Stability
			// LinkQualityIndicator parsed above
			else PARSE_THRESHOLD(UncorrectableErrors, UncorrectableErrors, 255)
			else PARSE_THRESHOLD(LinkDowned, LinkDowned, UINT_MAX)
			else PARSE_THRESHOLD64(PortRcvErrors, RcvErrors)
			else PARSE_THRESHOLD64(ExcessiveBufferOverruns, ExcessiveBufferOverruns)
			else PARSE_THRESHOLD64(FMConfigErrors, FMConfigErrors)
			else PARSE_THRESHOLD(LinkErrorRecovery, LinkErrorRecovery, UINT_MAX)
			else PARSE_THRESHOLD64(LocalLinkIntegrityErrors, LocalLinkIntegrityErrors)
			else PARSE_THRESHOLD64(PortRcvRemotePhysicalErrors, RcvRemotePhysicalErrors)
			// Security
			else PARSE_THRESHOLD64(PortXmitConstraintErrors, XmitConstraintErrors)
			else PARSE_THRESHOLD64(PortRcvConstraintErrors, RcvConstraintErrors)
			// Routing or Down nodes still being sent to
			else PARSE_THRESHOLD64(PortRcvSwitchRelayErrors, RcvSwitchRelayErrors)
			else PARSE_THRESHOLD64(PortXmitDiscards, XmitDiscards)
			// Congestion
			else PARSE_THRESHOLD64(SwPortCongestion, CongDiscards)
			else PARSE_THRESHOLD64(PortRcvFECN, RcvFECN)
			else PARSE_THRESHOLD64(PortRcvBECN, RcvBECN)
			else PARSE_THRESHOLD64(PortMarkFECN, MarkFECN)
			else PARSE_THRESHOLD64(PortXmitTimeCong, XmitTimeCong)
			else PARSE_THRESHOLD64(PortXmitWait, XmitWait)
			// Bubbles
			else PARSE_THRESHOLD64(PortXmitWastedBW, XmitWastedBW)
			else PARSE_THRESHOLD64(PortXmitWaitData, XmitWaitData)
			else PARSE_THRESHOLD64(PortRcvBubble, RcvBubble)
#undef PARSE_THRESHOLD
#undef PARSE_MB_THRESHOLD
			else {
				fprintf(stderr, "opareport: Invalid parameter: %s, ignoring\n", param);
			}
		} else {
			fprintf(stderr, "opareport: Invalid Config Line: %s, ignoring\n", buffer);
		}
	}
	fclose(fp);
	return 0;
}

// convert a output type argument to the proper constant
report_t checkOutputType(const char* name)
{
	if (0 == strcmp(optarg, "comps")) {
		return REPORT_COMP;
	} else if (0 == strcmp(optarg, "brcomps")) {
		return REPORT_BRCOMP;
	} else if (0 == strcmp(optarg, "nodes")) {
		return REPORT_NODES;
	} else if (0 == strcmp(optarg, "brnodes")) {
		return REPORT_BRNODES;
	} else if (0 == strcmp(optarg, "ious")) {
		return REPORT_IOUS;
	} else if (0 == strcmp(optarg, "links")) {
		return REPORT_LINKS;
	} else if (0 == strcmp(optarg, "extlinks")) {
		return REPORT_EXTLINKS;
	} else if (0 == strcmp(optarg, "filinks")) {
		return REPORT_FILINKS;
	} else if (0 == strcmp(optarg, "islinks")) {
		return REPORT_ISLINKS;
	} else if (0 == strcmp(optarg, "extislinks")) {
		return REPORT_EXTISLINKS;
	} else if (0 == strcmp(optarg, "slowlinks")) {
		return REPORT_SLOWLINKS;
	} else if (0 == strcmp(optarg, "slowconfiglinks")) {
		return REPORT_SLOWCONFIGLINKS;
	} else if (0 == strcmp(optarg, "slowconnlinks")) {
		return REPORT_SLOWCONNLINKS;
	} else if (0 == strcmp(optarg, "misconfiglinks")) {
		return REPORT_MISCONFIGLINKS;
	} else if (0 == strcmp(optarg, "misconnlinks")) {
		return REPORT_MISCONNLINKS;
	} else if (0 == strcmp(optarg, "errors")) {
		return REPORT_ERRORS;
	} else if (0 == strcmp(optarg, "otherports")) {
		return REPORT_OTHERPORTS;
	} else if (0 == strcmp(optarg, "verifylinks")) {
		return REPORT_VERIFYLINKS;
	} else if (0 == strcmp(optarg, "verifyextlinks")) {
		return REPORT_VERIFYEXTLINKS;
	} else if (0 == strcmp(optarg, "verifyfilinks")) {
		return REPORT_VERIFYFILINKS;
	} else if (0 == strcmp(optarg, "verifyislinks")) {
		return REPORT_VERIFYISLINKS;
	} else if (0 == strcmp(optarg, "verifyextislinks")) {
		return REPORT_VERIFYEXTISLINKS;
	} else if (0 == strcmp(optarg, "verifynodes")) {
		return REPORT_VERIFYFIS|REPORT_VERIFYSWS;
	} else if (0 == strcmp(optarg, "verifyfis")) {
		return REPORT_VERIFYFIS;
	} else if (0 == strcmp(optarg, "verifysws")) {
		return REPORT_VERIFYSWS;
	} else if (0 == strcmp(optarg, "verifysms")) {
		return REPORT_VERIFYSMS;
	} else if (0 == strcmp(optarg, "verifyall")) {
		/* verifylinks is a superset of verifyextlinks, verifyfilinks, */
		/* verifyislinks, verifyextislinks */
		return REPORT_VERIFYFIS|REPORT_VERIFYSWS|REPORT_VERIFYLINKS|REPORT_VERIFYSMS;
	} else if (0 == strcmp(optarg, "route")) {
		return REPORT_ROUTE;
	} else if (0 == strcmp(optarg, "none")) {
		return REPORT_SKIP;
	} else if (0 == strcmp(optarg, "sizes")) {
		return REPORT_SIZES;
	} else if (0 == strcmp(optarg, "snapshot")) {
		return REPORT_SNAPSHOT;
	} else if (0 == strcmp(optarg, "lids")) {
		return REPORT_LIDS;
	} else if (0 == strcmp(optarg, "linear")) {
		return REPORT_LINEARFDBS;
	} else if (0 == strcmp(optarg, "mcast")) {
		return REPORT_MCASTFDBS;
	} else if (0 == strcmp(optarg, "mcgroups")) {
		return REPORT_MCGROUPS;
	} else if (0 == strcmp(optarg, "vfinfo")) {
		return REPORT_VFINFO;
	} else if (0 == strcmp(optarg, "validatemcroutes")) {
		return REPORT_VALIDATEMCROUTES;
	} else if (0 == strcmp(optarg, "portusage")) {
		return REPORT_PORTUSAGE;
	} else if (0 == strcmp(optarg, "lidusage")) {
		return REPORT_LIDUSAGE;
	} else if (0 == strcmp(optarg, "pathusage")) {
		return REPORT_PATHUSAGE;
	} else if (0 == strcmp(optarg, "treepathusage")) {
		return REPORT_TREEPATHUSAGE;
	} else if (0 == strcmp(optarg, "validateroutes")) {
		return REPORT_VALIDATEROUTES;
	} else if (0 == strcmp(optarg, "validatevlroutes")) {
		return REPORT_VALIDATEVLROUTES;
        } else if (0 == strcmp(optarg, "validatecreditloops")) {
		return REPORT_VALIDATECREDITLOOPS;
	} else if (0 == strcmp(optarg, "validatevlcreditloops")) {
		return REPORT_VALIDATEVLCREDITLOOPS;
	} else if (0 == strcmp(optarg, "bfrctrl")) {
		return REPORT_BUFCTRLTABLES;
	} else if (0 == strcmp(optarg, "portgroups")) {
		return REPORT_PORTGROUPS;
	} else if (0 == strcmp(optarg, "validatepgs")) {
		return REPORT_VERIFYPGS;
	} else if (0 == strcmp(optarg, "vfmember")) {
		return REPORT_VFMEMBER;
	} else if (0 == strcmp(optarg, "quarantinednodes")) {
		return REPORT_QUARANTINE_NODES;
	} else if (0 == strcmp(optarg, "topology")) {
		return REPORT_TOPOLOGY;
	} else if (0 == strcmp(optarg, "all")) {
		/* note we omit brcomp and brnodes since comp and nodes is superset */
		/* similarly links is a suprset of filinks, islinks, extislinks */
		/* omit snapshot */
		return REPORT_COMP|REPORT_NODES|REPORT_IOUS|REPORT_LINKS
				|REPORT_EXTLINKS|REPORT_SLOWCONNLINKS|REPORT_ERRORS;
	} else {
		fprintf(stderr, "opareport: Invalid Output Type: %s\n", name);
		Usage();
		// NOTREACHED
		return 0;
	}
}

int main(int argc, char ** argv)
{
    FSTATUS             fstatus;
    int                 c;
    uint8               hfi         = 0;
    uint8               port        = 0;
	boolean				gothfi=FALSE, gotport=FALSE;
	Format_t			format = FORMAT_TEXT;
    int					detail = 2;
	int					index;
	report_t			report = REPORT_NONE;
	int					stats = 0;	// get port stats
	int					routes = 0;	// get routing FDB
	int					fl_vlqos = 0;	// get QOS VL-related tables
	int					bfrctrl = 0;	// get Buffer Control Tables
	int					mcgroups = 0;	// get multicast group members
	char *config_file = CONFIG_FILE;
	char *route_src = NULL;
	char *route_dest = NULL;
	char *focus_arg = NULL;
	Point				focus;
	uint32 temp;
	FabricFlags_t		sweepFlags = FF_NONE;
	uint8				find_flag = FIND_FLAG_FABRIC;	// always check fabric

	Top_setcmdname("opareport");
	PointInit(&focus);
	g_quiet = ! isatty(2);	// disable progress if stderr is not tty
	
	// process command line arguments
	while (-1 != (c = getopt_long(argc,argv, "vVAqh:p:o:d:PHNsrRi:CamK:MLc:S:D:F:xX:T:Qb:e:",
									options, &index)))
    {
        switch (c)
        {
			case '$':
				Usage_full();
				// NOTREACHED
				break;
            case 'v':
				g_verbose++;
				if (g_verbose > 3) umad_debug(g_verbose-2);
                break;
            case 'q':
				g_quiet = 1;
				break;
            case 'h':	// hfi to issue query from
				if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opareport: Invalid HFI Number: %s\n", optarg);
					Usage();
					// NOTREACHED
				}
				gothfi=TRUE;
                break;
            case 'p':	// port to issue query from
				if (FSUCCESS != StringToUint8(&port, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opareport: Invalid Port Number: %s\n", optarg);
					Usage();
					// NOTREACHED
				}
				gotport=TRUE;
                break;
            case 'o':	// select output record desired
				report = (report_t) report | checkOutputType(optarg);
				if (report & REPORT_ERRORS)
					stats = 1;
				if (report & (REPORT_MCGROUPS | REPORT_SNAPSHOT | REPORT_VALIDATEMCROUTES))
					mcgroups = 1;
				if ( report & ( REPORT_LINEARFDBS | REPORT_MCASTFDBS    |
						REPORT_PORTUSAGE | REPORT_LIDUSAGE |
						REPORT_PATHUSAGE | REPORT_TREEPATHUSAGE |
						REPORT_VALIDATEROUTES | REPORT_VALIDATECREDITLOOPS | REPORT_VALIDATEMCROUTES |
						REPORT_PORTGROUPS | REPORT_VERIFYPGS |
						REPORT_VALIDATEVLCREDITLOOPS | REPORT_VALIDATEVLROUTES ) )
					routes = 1;
				if (report & REPORT_BUFCTRLTABLES)
					bfrctrl = 1;
				if (report & (REPORT_VALIDATEVLCREDITLOOPS | REPORT_VALIDATEVLROUTES )) {
					fl_vlqos = 1;
					g_use_scsc = 1;
				}
				if (report & REPORT_VFMEMBER)
					fl_vlqos = 1;
				if (report & (REPORT_VERIFYFIS|REPORT_VERIFYSWS))
					find_flag |= FIND_FLAG_ENODE;
				if (report & REPORT_VERIFYSMS)
					find_flag |= FIND_FLAG_ESM;
				if (report & (REPORT_VERIFYLINKS|REPORT_VERIFYEXTLINKS
							  |REPORT_VERIFYFILINKS|REPORT_VERIFYISLINKS
							  |REPORT_VERIFYEXTISLINKS))
					find_flag |= FIND_FLAG_ELINK;
                break;
            case 'd':	// detail level
				if (FSUCCESS != StringToUint32(&temp, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opareport: Invalid Detail Level: %s\n", optarg);
					Usage();
					// NOTREACHED
				}
				detail = (int)temp;
                break;
            case 'P':	// persistent data only
                g_persist = 1;
                break;
            case 'H':	// hardware data only
                g_hard = 1;
                break;
            case 'N':	// omit names
                g_noname = 1;
                break;
            case 's':	// get performance stats
                stats = 1;
                break;
            case 'i':	// get performance stats over interval
				if (FSUCCESS != StringToUint32(&temp, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opareport: Invalid Interval: %s\n", optarg);
					Usage();
					// NOTREACHED
				}
                g_interval = (int)temp;
                break;
			case 'C':	// clear all monitored performance stats
				g_clearstats = 1;
				break;
			case 'a':	// clear all performance stats
				g_clearallstats = 1;
				break;
			case 'm':	// access fabric information through direct SMA
				sweepFlags |= FF_SMADIRECT;
				break;
			case 'M':	// access performance stats through direct PMA
				sweepFlags |= FF_PMADIRECT;
				break;
			case 'A':	// get PortInfo for all switch ports, including down ones
				sweepFlags |= FF_DOWNPORTINFO;
				break;
            case 'r':	// get routing FDBs
                routes = 1;
                break;
			case 'L':	// limit to specific ports
				g_limitstats = 1;
				break;
			case 'c':	// config file for thresholds in errors report
				config_file = optarg;
				break;
			case 'S':	// source for trace route
				if (route_src) {
					fprintf(stderr, "opareport: -S option may only be specified once\n");
					Usage();
					// NOTREACHED
				}
				route_src = optarg;
				break;
			case 'D':	// dest for trace route
				if (route_dest) {
					fprintf(stderr, "opareport: -D option may only be specified once\n");
					Usage();
					// NOTREACHED
				}
				route_dest = optarg;
				break;
			case 'F':	// focus for report
				if (focus_arg) {
					fprintf(stderr, "opareport: -F option may only be specified once\n");
					Usage();
					// NOTREACHED
				}
				focus_arg = optarg;
				break;
            case 'V':	// get QOS VL-related tables
                fl_vlqos = 1;
                bfrctrl = 1;
                break;
			case 'x':	// output in xml
				format = FORMAT_XML;
				break;
			case 'X':	// snapshot_input in xml
				g_snapshot_in_file = optarg;
				break;
			case 'T':	// topology_input in xml
				g_topology_in_file = optarg;
				break;
			case 'Q':	// do not include focus description in report
				g_quietfocus = 1;
				break;
			case 'b':
				if (FSUCCESS != StringToDateTime(&temp, optarg)) {
					fprintf(stderr, "opareport: Invalid Date/Time: %s\n", optarg);
					Usage();
					// NOTREACHED
				}
				g_begin = temp;
				break;
			case 'e':
				if (FSUCCESS != StringToDateTime(&temp, optarg)) {
					fprintf(stderr, "opareport: Invalid Date/Time: %s\n", optarg);
					Usage();
					// NOTREACHED
				}
				g_end = temp;
				break;
            default:
                fprintf(stderr, "opareport: Invalid option -%c\n", c);
                Usage();
                // NOTREACHED
                break;
        }
    } /* end while */

	if (optind < argc)
	{
		Usage();
		// NOTREACHED
	}

	// check for incompatible reports
	if (report & REPORT_TOPOLOGY) {
		if((report != REPORT_TOPOLOGY)){
			fprintf(stderr, "opareport: -o topology cannot be run with other reports\n");
			Usage();
			// NOTREACHED
		} else {
			format = FORMAT_XML;
		}
	}
	if ((report & REPORT_SNAPSHOT) && (report != REPORT_SNAPSHOT)) {
		fprintf(stderr, "opareport: -o snapshot cannot be run with other reports\n");
		Usage();
		// NOTREACHED
	}

	// check for missing required arguments
	if ((report & REPORT_ROUTE) && route_dest == NULL) {
		fprintf(stderr, "opareport: -o route require -D option\n");
		Usage();
		// NOTREACHED
	}

	// check for incompatible arguments
	if (g_begin && g_end && (g_begin > g_end)){
		fprintf(stderr, "opareport: begin time must be before end time\n");
		Usage();
		// NOTREACHED
	}	

	// Warn for extraneous arguments and ignore them
	if ((route_dest || route_src) && ! (report & REPORT_ROUTE)) {
		fprintf(stderr, "opareport: -S or -D option given without -o route, ignoring -S and -D\n");
		route_src = NULL;
		route_dest = NULL;
	}

	if (focus_arg) {
		char *name = "report";
		int suppress = 0;
		if (report & REPORT_ROUTE) { suppress = 1; name = "route"; }
		if (report & REPORT_SNAPSHOT) { suppress = 1; name = "snapshot"; }
		if (report & REPORT_MCGROUPS) { suppress = 1; name = "mcgroups"; }
		if (report & REPORT_VALIDATEROUTES) { suppress = 1; name = "validateroutes"; }
		if (report & REPORT_VALIDATECREDITLOOPS) { suppress = 1; name = "validatecreditloops"; }
		if (report & REPORT_VALIDATEVLCREDITLOOPS) { suppress = 1; name = "validatevlcreditloops"; }
		if (report & REPORT_VALIDATEMCROUTES) { suppress = 1; name = "validatemcroutes"; }
		if (report & REPORT_VERIFYPGS) { suppress = 1; name = "validatepgs"; }
		if (report & REPORT_SKIP) { suppress = 1; name = "none"; }

		if (suppress) {
			fprintf(stderr,"opareport: %s does not support -F option.\n", name);
			fprintf(stderr,"           -F ignored for all reports.\n");
			focus_arg = NULL;
		}
	}

	if (report == REPORT_SNAPSHOT && g_limitstats) {
		fprintf(stderr, "opareport: -L ignored for -o snapshot\n");
		g_limitstats = 0;
	}
	
	if (g_limitstats && ! focus_arg) {
		fprintf(stderr, "opareport: -L ignored when -F not specified\n");
		g_limitstats = 0;
	}
	if ((report & REPORT_SNAPSHOT) && (g_noname || g_hard || g_persist)) {
		fprintf(stderr, "opareport: -N, -H and -P ignored for -o snapshot\n");
		g_noname = 0;
		g_hard = 0;
		g_persist = 0;
	}

	if ((report & REPORT_MCGROUPS ) && (sweepFlags & FF_SMADIRECT)) {
		fprintf(stderr, "opareport: -m ignored for -o mcgroups\n");
	}

	if ((report & REPORT_VALIDATEMCROUTES ) && (sweepFlags & FF_SMADIRECT)) {
		fprintf(stderr, "opareport: -m ignored for -o validatemcroutes\n");
	}

	//In a live cluster, implicitly fetch the routing tables when route report or route focus is requested with -m option
	if (!g_snapshot_in_file && (sweepFlags & FF_SMADIRECT)&& ((report & REPORT_ROUTE ) || ( focus_arg && NULL != ComparePrefix(focus_arg, "route:"))))  
		routes = 1;
	

	if (g_snapshot_in_file && (g_interval || g_clearstats || g_clearallstats || g_begin || g_end)) {
		fprintf(stderr, "opareport: -i, -C, -a, -b, and -e ignored for -X\n");
		g_interval = 0;
		g_clearstats = 0;
		g_clearallstats = 0;
		g_begin = 0;
		g_end = 0;
	}
	if (g_snapshot_in_file) {
		if (sweepFlags & FF_SMADIRECT)
			fprintf(stderr, "opareport: -m ignored for -X\n");
		if (sweepFlags & FF_PMADIRECT)
			fprintf(stderr, "opareport: -M ignored for -X\n");
		if (sweepFlags & FF_DOWNPORTINFO)
			fprintf(stderr, "opareport: -A ignored for -X\n");
		sweepFlags &= ~(FF_SMADIRECT|FF_PMADIRECT|FF_DOWNPORTINFO);
	}
	if (g_snapshot_in_file && stats) {
		if (! (report & REPORT_ERRORS)) {
			// -s must have been explicitly specified
			fprintf(stderr, "opareport: -s ignored for -X\n");
		}
		stats = 0;
	}
	if (g_snapshot_in_file && (fl_vlqos || bfrctrl)) {
		if (! (report & (REPORT_BUFCTRLTABLES|REPORT_VFINFO|REPORT_VFMEMBER))) {
			// -V must have been explicitly specified
			fprintf(stderr, "opareport: -V ignored for -X\n");
		}
	}
	if (g_snapshot_in_file && routes) {
		if ( ! ( report & ( REPORT_LINEARFDBS | REPORT_MCASTFDBS |
				REPORT_PORTUSAGE | REPORT_LIDUSAGE | REPORT_PATHUSAGE |
				REPORT_TREEPATHUSAGE | REPORT_VALIDATEROUTES | REPORT_VALIDATECREDITLOOPS |
				REPORT_VALIDATEVLROUTES | REPORT_VALIDATEVLCREDITLOOPS | REPORT_VALIDATEMCROUTES) ) ) {
			// -r must have been explicitly specified
			fprintf(stderr, "opareport: -r ignored for -X\n");
		}
	}
	if (g_limitstats && ! (report & (REPORT_ERRORS|REPORT_SNAPSHOT)) && ! g_clearstats && ! g_clearallstats && ! g_interval) {
		fprintf(stderr, "opareport: -L ignored without -C, -a, -i, -o errors nor -o snapshot\n");
		g_limitstats = 0;
	}
	if (g_interval && (g_hard || g_persist)) {
		fprintf(stderr, "opareport: -i ignored with -H or -P\n");
		g_interval = 0;
	}

	if ((report & REPORT_VFINFO) && detail > 2)
		fl_vlqos = 1;
	// check for unignored arguments which imply need to get stats
	if (! g_snapshot_in_file && focus_arg && NULL != ComparePrefix(focus_arg, "linkqual")) 
		stats = 1; // using the linkqual focus option implies -s
	if (g_interval || g_begin | g_end)
		stats = 1;

	// now that we have final value for "stats" option, make sure consistent
	if ((sweepFlags & FF_DOWNPORTINFO) && ! (sweepFlags & FF_PMADIRECT)
		&& (stats || g_interval || g_clearstats || g_clearallstats)) {
		fprintf(stderr, "opareport: Use of -A requires performance stats be gathered directly (via -M)\n");
		Usage();
		// NOTREACHED
	}
	if (g_interval && focus_arg && NULL != ComparePrefix(focus_arg, "linkqual")) {
		fprintf(stderr, "opareport: -i option not permitted in conjunction with -F linkqual, linkqualLE\n     nor linkqualGE\n");
		Usage();
		// NOTREACHED
	}

	if (report == REPORT_NONE)
		report = REPORT_BRNODES;

	// Initialize Sweep Verbose option, for -X still used for Focus processing
	fstatus = InitSweepVerbose(g_verbose?stderr:NULL);
	if (fstatus != FSUCCESS) {
		fprintf(stderr, "opareport: Initialize Verbose option (status=0x%x): %s\n", fstatus, iba_fstatus_msg(fstatus));
		g_exitstatus = 1;
		goto done;
	}

	// figure out which local port we will use to gather data
	if (g_snapshot_in_file) {
		if (gotport || gothfi)
			fprintf(stderr, "opareport: -p and -h ignored for -X\n");
	} else {
#ifdef IB_STACK_IBACCESS
		// we must initialize user mode iba library first
		fstatus = iba_init();
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opareport: iba_init failed (status=0x%x): %s\n", fstatus, iba_fstatus_msg(fstatus));
			g_exitstatus = 1;
			goto done;
		}
#endif

		// find portGuid for hfi/port specified
		{
			uint32 caCount, portCount;

			fstatus = iba_get_portguid(hfi, port, NULL, &g_portGuid, NULL, &g_portAttrib,
									&caCount, &portCount);

			if (FNOT_FOUND == fstatus) {
				fprintf(stderr, "opareport: %s\n",
					iba_format_get_portguid_error(hfi, port, caCount, portCount));
				g_exitstatus = 1;
				goto done;
			} else if (FSUCCESS != fstatus) {
				fprintf(stderr, "opareport: iba_get_portguid Failed: %s\n",
								iba_fstatus_msg(fstatus));
				g_exitstatus = 1;
				goto done;
			}
			DBGPRINT("USING SUBNET PREFIX 0x%016"PRIx64"\n", g_portAttrib->GIDTable[0].Type.Global.SubnetPrefix);
		}
	}

	// get thresholds config file
	if ((report & REPORT_ERRORS) || g_clearstats) {
		if (0 != parse(config_file)) {
			g_exitstatus = 1;
			goto done;
		}
	}

	// get the fabric data
	if (g_snapshot_in_file) {
		if (FSUCCESS != Xml2ParseSnapshot(g_snapshot_in_file, g_quiet, &g_Fabric, FF_NONE, 0)) {
			g_exitstatus = 1;
			goto done;
		}
	} else {
		if (FSUCCESS != InitMad(g_portGuid, g_verbose?stderr:NULL)) {
			g_exitstatus = 1;
			goto done;
		}
		if (FSUCCESS != Sweep(g_portGuid, &g_Fabric, sweepFlags, SWEEP_ALL, g_quiet)) {
			g_exitstatus = 1;
			goto done;
		}
	}

	// parse topology input file and cross reference to fabric data
	if (g_topology_in_file) {
		if (FSUCCESS != Xml2ParseTopology(g_topology_in_file, g_quiet, &g_Fabric, TOPOVAL_NONE)) {
			g_exitstatus = 1;
			goto done_fabric;
		}
		//if (g_verbose)
		//	Xml2PrintTopology(stdout, &g_Fabric);	// for debug
	}

	// we can't do a linkqual focus until after the port counters have been collected
	// nor can we do route focus with -m until FDBs are collected. So will handle route focus with and without -m later.
	if (focus_arg && (NULL == ComparePrefix(focus_arg, "linkqual")) && (NULL == ComparePrefix(focus_arg, "route"))) {
		char *p;
		FSTATUS status;
		
		status = ParseFocusPoint(g_snapshot_in_file?0:g_portGuid,
						&g_Fabric, focus_arg, &focus, find_flag, &p, TRUE);
		if (FINVALID_PARAMETER == status || (FSUCCESS == status && *p != '\0')) {
			fprintf(stderr, "opareport: Invalid Point Syntax: '%s'\n", focus_arg);
			fprintf(stderr, "opareport:                        %*s^\n", (int)(p-focus_arg), "");
			PointDestroy(&focus);
			Usage_full();
			// NOTREACHED
		}
		if (FSUCCESS != status) {
			fprintf(stderr, "opareport: Unable to resolve Point: '%s': %s\n", focus_arg, iba_fstatus_msg(status));
			g_exitstatus = 1;
			goto done_fabric;
		}
	}

	// output the desired reports
	if (g_interval) {
		(void)ClearAllPortCountersAndShow(g_portGuid, &focus, TRUE, format, report == REPORT_SNAPSHOT);
		PROGRESS_PRINT(TRUE, "Waiting %d seconds", g_interval);
		sleep(g_interval);
	}

	if (!g_snapshot_in_file && stats && ! g_hard && ! g_persist) {
		if (FSUCCESS != GetAllPortCounters(g_portGuid, g_portAttrib->GIDTable[0],
				&g_Fabric, &focus, g_limitstats, g_quiet, g_begin, g_end)) {
			g_exitstatus = 1;
			report &= ~REPORT_ERRORS;
			fprintf(stderr, "opareport: Failed to Get Port Counters\n");
		}
	}

	// get other optional fabric data
	// now that the port counters have been collected, we can do the link quality focus
	if (focus_arg && NULL != ComparePrefix(focus_arg, "linkqual")) {
		if (!(g_Fabric.flags & FF_STATS) && g_snapshot_in_file ) {
			fprintf(stderr, "opareport: '-F linkqual' with snapshot must be created with -s option\n");
			Usage();
			// NOTREACHED
		}
		char *p;
		FSTATUS status;
		// use FIND_FLAG_FABRIC, no use checking anything other than fabric
		status = ParseFocusPoint(g_snapshot_in_file?0:g_portGuid,
						&g_Fabric, focus_arg, &focus, FIND_FLAG_FABRIC, &p, TRUE);
		if (FINVALID_PARAMETER == status || (FSUCCESS == status && *p != '\0')) {
			fprintf(stderr, "opareport: Invalid Point Syntax: '%s'\n", focus_arg);
			fprintf(stderr, "opareport:                        %*s^\n", (int)(p-focus_arg), "");
			PointDestroy(&focus);
			Usage_full();
			// NOTREACHED
		}
		if (FSUCCESS != status) {
			fprintf(stderr, "opareport: Unable to resolve Point: '%s': %s\n", focus_arg, iba_fstatus_msg(status));
			g_exitstatus = 1;
			goto done_fabric;
		}
	}

	if ((!g_snapshot_in_file && routes && ! g_hard && ! g_persist) || (g_snapshot_in_file && (report & REPORT_PORTUSAGE))) {
		if (!g_snapshot_in_file && (FSUCCESS != GetAllFDBs(g_portGuid, &g_Fabric, &focus, g_quiet))) {
			g_exitstatus = 1;
			goto done_fabric;
		}
		/* Traverse the switches and find the largest LFT */
		LIST_ITEM *p;
		for (p=QListHead(&g_Fabric.AllSWs); p != NULL; p = QListNext(&g_Fabric.AllSWs, p)) {
			NodeData *nodep;
			SwitchData *switchp;

			nodep = QListObj(p);
			switchp = nodep->switchp;
			if (!switchp || !switchp->LinearFDB) continue;
			if (switchp->LinearFDBSize > g_max_lft) g_max_lft = switchp->LinearFDBSize;
		}
	}

	// now that the FDBs  have been fetched for -m option, , we can do the route focus
	if (focus_arg && NULL != ComparePrefix(focus_arg, "route")) {
		char *p;
		FSTATUS status;
		if ( !(g_Fabric.flags & FF_ROUTES) && g_snapshot_in_file ) {
			fprintf(stderr, "opareport: '-F route:' with snapshot must be created with -r option\n");
			Usage();
			// NOTREACHED
		}
		status = ParseFocusPoint(g_snapshot_in_file?0:g_portGuid,
						&g_Fabric, focus_arg, &focus, find_flag, &p, TRUE);
		if (FINVALID_PARAMETER == status || (FSUCCESS == status && *p != '\0')) {
			fprintf(stderr, "opareport: Invalid Point Syntax: '%s'\n", focus_arg);
			fprintf(stderr, "opareport:                        %*s^\n", (int)(p-focus_arg), "");
			PointDestroy(&focus);
			Usage_full();
			// NOTREACHED
		}
		if (FSUCCESS != status) {
			fprintf(stderr, "opareport: Unable to resolve Point: '%s': %s\n", focus_arg, iba_fstatus_msg(status));
			g_exitstatus = 1;
			goto done_fabric;
		}
	}

	if (!g_snapshot_in_file && fl_vlqos && ! g_hard && ! g_persist) {
		if (FSUCCESS != GetAllPortVLInfo(g_portGuid, &g_Fabric, &focus, g_quiet, &g_use_scsc)) {
			g_exitstatus = 1;
			goto done_fabric;
		}
	}

	if (!g_snapshot_in_file && bfrctrl && ! g_hard && ! g_persist) {
		if (FSUCCESS != GetAllBCTs(g_portGuid, &g_Fabric, &focus, g_quiet)) {
			g_exitstatus = 1;
			goto done_fabric;
		}
	}

	if (!g_snapshot_in_file && mcgroups && ! g_hard && ! g_persist) {
		if (FSUCCESS != GetAllMCGroups(g_portGuid, &g_Fabric, &focus, g_quiet)) {
			g_exitstatus = 1;
			goto done_fabric;
		}
	}

	if (format == FORMAT_XML && ! (report & REPORT_SNAPSHOT)) {
// TBD - use IXml functions for XML output
		char datestr[80] = "";
		int i;

		printf("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
		Top_formattime(datestr, sizeof(datestr), g_Fabric.time);
		printf("<Report date=\"%s\" unixtime=\"%ld\" options=\"", datestr, g_Fabric.time);
		for (i=1; i<argc; i++)
			printf("%s%s", i>1?" ":"", argv[i]);
		printf("\" >\n");
	}
	if (report & REPORT_COMP)
		ShowComponentReport(&focus, format, 0, detail);
	if (report & REPORT_BRCOMP)
		ShowComponentBriefReport(&focus, format, 0, detail);
	if (report & REPORT_NODES)
		ShowNodeTypeReport(&focus, format, 0, detail);
	if (report & REPORT_BRNODES)
		ShowNodeTypeBriefReport(&focus, format, REPORT_BRNODES, 0, detail);
	if (report & REPORT_IOUS)
		ShowAllIOUReport(&focus, format, 0, detail);
	if (report & REPORT_LINKS)
		ShowLinksReport(&focus, REPORT_LINKS, format, 0, detail);
	if (report & REPORT_EXTLINKS)
		ShowLinksReport(&focus, REPORT_EXTLINKS, format, 0, detail);
	if (report & REPORT_FILINKS)
		ShowLinksReport(&focus, REPORT_FILINKS, format, 0, detail);
	if (report & REPORT_ISLINKS)
		ShowLinksReport(&focus, REPORT_ISLINKS, format, 0, detail);
	if (report & REPORT_EXTISLINKS)
		ShowLinksReport(&focus, REPORT_EXTISLINKS, format, 0, detail);
	if (report & REPORT_SLOWLINKS)
		ShowSlowLinkReport(LINK_EXPECTED_REPORT, FALSE, &focus, format, 0, detail);
	if (report & REPORT_SLOWCONFIGLINKS)
		ShowSlowLinkReport(LINK_CONFIG_REPORT, FALSE, &focus, format, 0, detail);
	if (report & REPORT_SLOWCONNLINKS)
		ShowSlowLinkReport(LINK_CONN_REPORT, FALSE, &focus, format, 0, detail);
	if (report & REPORT_MISCONFIGLINKS)
		ShowSlowLinkReport(LINK_CONFIG_REPORT, TRUE, &focus, format, 0, detail);
	if (report & REPORT_MISCONNLINKS)
		ShowSlowLinkReport(LINK_CONN_REPORT, TRUE, &focus, format, 0, detail);
	if (report & REPORT_ERRORS)
		ShowLinkErrorReport(&focus, format, 0, detail);
	if (report & REPORT_OTHERPORTS)
		ShowOtherPortsReport(&focus, format, 0, detail);
	if (report & REPORT_VERIFYFIS)
		ShowVerifyNodesReport(&focus, STL_NODE_FI, format, 0, detail);
	if (report & REPORT_VERIFYSWS)
		ShowVerifyNodesReport(&focus, STL_NODE_SW, format, 0, detail);
	if (report & REPORT_VERIFYSMS)
		ShowVerifySMsReport(&focus, format, 0, detail);
	if (report & REPORT_VERIFYLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYLINKS, format, 0, detail);
	if (report & REPORT_VERIFYEXTLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYEXTLINKS, format, 0, detail);
	if (report & REPORT_VERIFYFILINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYFILINKS, format, 0, detail);
	if (report & REPORT_VERIFYISLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYISLINKS, format, 0, detail);
	if (report & REPORT_VERIFYEXTISLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYEXTISLINKS, format, 0, detail);
	if (report & REPORT_TOPOLOGY) {
		ShowNodeTypeBriefReport(&focus, format, REPORT_TOPOLOGY, 0, detail);
		ShowLinksReport(&focus, REPORT_LINKS, format, 0, detail);
	}
	if (report & REPORT_ROUTE) {
		Point point1, point2;
		int skip = 0;

		PointInit(&point1);
		PointInit(&point2);

		if (route_src) {
			char *p;
			if (FSUCCESS != (fstatus = ParsePoint(&g_Fabric, route_src, &point1, FIND_FLAG_FABRIC, &p)) || *p != '\0') {
				PointDestroy(&point1);
				PointDestroy(&point2);
				if (FINVALID_PARAMETER == fstatus || (FSUCCESS == fstatus && *p != '\0')) {
					fprintf(stderr, "opareport: Invalid Source Point Syntax: '%s'\n", route_src);
					fprintf(stderr, "opareport:                               %*s^\n", (int)(p-route_src), "");
				}

				fprintf(stderr, "opareport: Unable to resolve Source Point: '%s': %s\n", route_src, iba_fstatus_msg(fstatus));

				switch (format) {
				case FORMAT_TEXT:
					printf("%*sReport skipped: Invalid source point syntax: %s:%s\n",0,"",
						route_src, iba_fstatus_msg(fstatus));
					break;
				case FORMAT_XML:
					printf("%*s<!-- Report skipped: Invalid source point syntax: %s:%s -->\n",0,"",
						route_src, iba_fstatus_msg(fstatus));
					break;
				default:
					break;
				}

				goto done_route;
			}
		} else {
			PortData *portp1 = FindPortGuid(&g_Fabric, g_portGuid);
			if (! portp1) {
				fprintf(stderr, "opareport: Local Port GUID Not Found: 0x%016"PRIx64"\n", g_portGuid);
				fprintf(stderr, "opareport: Skipping trace route report\n");
				skip = 1;
			} else {
				point1.Type = POINT_TYPE_SYSTEM;
				point1.u.systemp = portp1->nodep->systemp;
				if (! point1.u.systemp) {
					fprintf(stderr, "opareport: System for Local Port GUID Not Found: 0x%016"PRIx64"\n", g_portGuid);
					fprintf(stderr, "opareport: Skipping trace route report\n");
					skip = 1;
				}
			}
		}
		if ((!skip) && route_dest) {
			char *p;
			if (FSUCCESS != (fstatus = ParsePoint(&g_Fabric, route_dest, &point2, FIND_FLAG_FABRIC, &p)) || *p != '\0') {
				PointDestroy(&point1);
				PointDestroy(&point2);
				if (FINVALID_PARAMETER == fstatus || (FSUCCESS == fstatus && *p != '\0')) {
					fprintf(stderr, "opareport: Invalid Dest Point Syntax: '%s'\n", route_dest);
					fprintf(stderr, "opareport:                             %*s^\n", (int)(p-route_dest), "");
				}

				switch (format) {
				case FORMAT_TEXT:
					printf("%*sReport skipped: Invalid destination point syntax: %s:%s\n",0,"",
						route_dest, iba_fstatus_msg(fstatus));
					break;
				case FORMAT_XML:
					printf("%*s<!-- Report skipped: Invalid destination point syntax: %s:%s -->\n",0,"",
						route_dest, iba_fstatus_msg(fstatus));
					break;
				default:
					break;
				}

				goto done_route;
			}

			ShowRoutesReport(g_portGuid, &point1, &point2, format, 0, detail);
		}
		done_route:
		PointDestroy(&point1);
		PointDestroy(&point2);
	}
	if (report & REPORT_SIZES)
		ShowSizesReport();
	if (report == REPORT_SNAPSHOT) {
		SnapshotOutputInfo_t info;

		info.fabricp = &g_Fabric;
		info.argc = argc;
		info.argv = argv;

		Xml2PrintSnapshot(stdout, &info);
	}

	if (report & REPORT_LIDS)
		ShowAllLIDReport(&focus, format, 0, detail);

	if (report & REPORT_LINEARFDBS)
		ShowLinearFDBReport(&focus, format, 0, detail);

	if (report & REPORT_MCASTFDBS)
		ShowMulticastFDBReport(&focus, format, 0, detail);


	if (report & REPORT_MCGROUPS)
		ShowMulticastGroupsReport(format, 0, detail);
	
	if (report & REPORT_PORTUSAGE)
		ShowPortUsageReport(&focus, format, 0, detail);

	if (report & REPORT_PATHUSAGE)
		ShowPathUsageReport(&focus, format, 0, detail);

	if (report & REPORT_TREEPATHUSAGE)
		ShowTreePathUsageReport(&focus, format, 0, detail);

	if (report & REPORT_VALIDATEROUTES || report & REPORT_VALIDATEVLROUTES) {
		ShowValidateRoutesReport(format, 0, detail);
	}

	if (report & REPORT_VALIDATECREDITLOOPS || report & REPORT_VALIDATEVLCREDITLOOPS) {
		ShowValidateCreditLoopsReport(format, 0, detail);
	}
 
	if (report & REPORT_VALIDATEMCROUTES) {
		ShowValidateMCRoutesReport(format, 0, detail);
	}

	if (report & REPORT_VFINFO)
		ShowVFInfoReport(&focus, format, 0, detail);

	if (report & REPORT_PORTGROUPS)
		ShowPGReport(&focus, format, 0, detail);

	if (report & REPORT_VERIFYPGS)
		ShowValidatePGReport(format, 0, detail);

	// Undocumented LID usage report
	if (report & REPORT_LIDUSAGE)
		ShowLIDUsageReport(&focus, format, 0, detail);

	if (report & REPORT_BUFCTRLTABLES)
		ShowAllBCTReports(&focus, format, 0, detail);

	if (report & REPORT_VFMEMBER) 
		ShowVFMemberReport(&focus, format, 0, detail);


	if (report & REPORT_QUARANTINE_NODES) 
		ShowQuarantineNodeReport(&focus, format, 0, detail);

	if (g_clearstats || g_clearallstats)
		(void)ClearAllPortCountersAndShow(g_portGuid, &focus, g_clearallstats, format, report == REPORT_SNAPSHOT);
	if (format == FORMAT_XML && ! (report & REPORT_SNAPSHOT)) {
		printf("</Report>\n");
	}
done_fabric:
	DestroyFabricData(&g_Fabric);
done:
	PointDestroy(&focus);
	DestroyMad();

	if (g_exitstatus == 2) {
		Usage();
		// NOTREACHED
	}

	return g_exitstatus;
}
