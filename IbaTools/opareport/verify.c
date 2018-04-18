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

#include "opareport.h"
#include <iba/stl_helper.h>
#include <limits.h>

// Fabric Topology Verification and related reports

void ShowProblem(Format_t format, int indent, int detail, const char* pformat, ...)
{
	va_list args;
	static char buffer[100];
	int cnt;

	if (detail >= 0) {
		va_start(args, pformat);
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s", indent, "");
			vprintf(pformat, args);
			printf("\n");
			break;
		case FORMAT_XML:
			cnt = vsnprintf(buffer, sizeof(buffer), pformat, args);
			ASSERT(cnt <= sizeof(buffer)-1);	/* make sure message fits */
			XmlPrintStr("Problem", buffer, indent);
			break;
		default:
			break;
		}
		va_end(args);
	}
}

// verify a port against its corresponding ExpectedLink->PortSelector
// Only valid to be called for ports with ExpectedLink
// returns FALSE if any discrepencies
// only does output if detail >= 0
boolean PortVerify(PortData *portp, Format_t format, int indent, int detail)
{
	ExpectedLink *elinkp = portp->elinkp;
	PortSelector *portselp = NULL;
	boolean ret=TRUE;

	if (elinkp->portp1 == portp) {
		portselp = elinkp->portselp1;
	} else if (elinkp->portp2 == portp) {
		portselp = elinkp->portselp2;
	}
	if (portselp) { // not specified in input topology xml file; accept any port on the other end of the link
		if (portselp->NodeGUID && portselp->NodeGUID != portp->nodep->NodeInfo.NodeGUID) {
			ret = FALSE;
			ShowProblem(format, indent, detail,
				"NodeGuid mismatch: expected: 0x%016"PRIx64" found: 0x%016"PRIx64,
				portselp->NodeGUID, portp->nodep->NodeInfo.NodeGUID);
		}
		if (portselp->NodeDesc
			&& 0 != strncmp(portselp->NodeDesc,
							(char*)portp->nodep->NodeDesc.NodeString,
							NODE_DESCRIPTION_ARRAY_SIZE)) {
			ret = FALSE;
			ShowProblem(format, indent, detail,
				"NodeDesc mismatch: expected: '%s' found: '%.*s'",
				portselp->NodeDesc, NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)portp->nodep->NodeDesc.NodeString);
		}
		if (portselp->gotPortNum && portselp->PortNum != portp->PortNum) {
			ret = FALSE;
			ShowProblem(format, indent, detail,
				"PortNum mismatch: expected: %3u found: %3u",
				portselp->PortNum, portp->PortNum);
		}
		if (portselp->PortGUID && portselp->PortGUID != portp->PortGUID) {
			ret = FALSE;
			ShowProblem(format, indent, detail, 
				"PortGuid mismatch: expected: 0x%016"PRIx64" found: 0x%016"PRIx64,
				portselp->PortGUID, portp->PortGUID);
		}
		if (portselp->NodeType && portselp->NodeType != portp->nodep->NodeInfo.NodeType) {
			ret = FALSE;
			ShowProblem(format, indent, detail, 
				"NodeType mismatch: expected: %s found: %s",
				StlNodeTypeToText(portselp->NodeType),
				StlNodeTypeToText(portp->nodep->NodeInfo.NodeType));
		}
	}
	/* to get here the port must have a neighbor and hence should be linkup
	 * but it could be quarantined or failing to move to Active
	 */
	if (portp->PortInfo.PortStates.s.PortState != IB_PORT_ACTIVE) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"Port not Active: PortState: %s",
			StlPortStateToText(portp->PortInfo.PortStates.s.PortState));
	}

	return ret;
}

// check attributes of link against input topology
// Only valid to be called for ports with ExpectedLink
// returns FALSE if link fails to verify
// only does output if detail >= 0
boolean LinkAttrVerify(ExpectedLink *elinkp, PortData *portp1, Format_t format, int indent, int detail)
{
	PortData *portp2 = portp1->neighbor;
	boolean ret = TRUE;

	if (elinkp->expected_rate && elinkp->expected_rate != portp1->rate) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"Link Rate mismatch: expected: %4s found: %4s",
			StlStaticRateToText(elinkp->expected_rate),
			StlStaticRateToText(portp1->rate));
	}

	if (elinkp->expected_mtu && elinkp->expected_mtu != MIN(portp1->PortInfo.MTU.Cap, portp2->PortInfo.MTU.Cap)) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"Link MTU mismatch: expected minimum MTU: %5s found MTU: %5s",
			IbMTUToText(elinkp->expected_mtu),
			IbMTUToText(MIN(portp1->PortInfo.MTU.Cap, portp2->PortInfo.MTU.Cap)));
	}
	return ret;
}

typedef enum {
	LINK_VERIFY_OK = 0,	// link matches input topology
	LINK_VERIFY_DIFF = 1, // link fully resolved but diff from input topology
	LINK_VERIFY_UNEXPECTED = 2, // link not found in input topology
	LINK_VERIFY_MISSING = 3, // link not found in fabric, but in input topology
	LINK_VERIFY_DUP = 4, // possible duplicate in input topology
	LINK_VERIFY_CONN = 5, // link partially resolved, wrong cabling
	LINK_VERIFY_MAX=5	// maximum value of the above
} LinkVerifyResult_t;

// check both fabric ports and link attributes against input topology
// only does output if detail >= 0
LinkVerifyResult_t LinkFabricVerify(PortData *portp, Format_t format, int indent, int detail)
{
	LinkVerifyResult_t ret = LINK_VERIFY_OK;

	// all checks which are based off of found ports/links in fabric
	if (portp->elinkp && portp->neighbor->elinkp) {
		ExpectedLink *elinkp = portp->elinkp;
		DEBUG_ASSERT(portp->elinkp == portp->neighbor->elinkp);
		// check both sides for expected characteristics
		if (! PortVerify(portp, format, indent, detail))
			ret = LINK_VERIFY_DIFF;
		if (! PortVerify(portp->neighbor, format, indent, detail))
			ret = LINK_VERIFY_DIFF;
		// check expected link characteristics
		if (! LinkAttrVerify(elinkp, portp, format, indent, detail))
			ret = LINK_VERIFY_DIFF;
	} else if (! portp->elinkp && ! portp->neighbor->elinkp) {
		ret = LINK_VERIFY_UNEXPECTED; // extra link
		ShowProblem(format, indent, detail, "Unexpected Link");
	} else {
		// only one side resolved
		DEBUG_ASSERT(0);	// we only set elinkp if both sides resolved
		ret = LINK_VERIFY_UNEXPECTED; // internal error
	}
	return ret;
}

// compare ExpectedLink against fabric
// LinkFabricVerify will have caught links which are different or extra
// this is focused on duplicate ExpectedLink or missing links
// only does output if detail >= 0
// side controls message output:
// 	0 - both ports and link info - for initial checks
// 	1 - elinkp->port 1 - when in process of outputting port 1 info
// 	2 - elinkp->port 2 - when in process of outputting port 2 info
// 	3 - link info only - when in process of outputting summary link info
LinkVerifyResult_t ExpectedLinkVerify(ExpectedLink *elinkp, uint8 side, Format_t format, int indent, int detail)
{
	LinkVerifyResult_t ret = LINK_VERIFY_OK;

	if (! elinkp->portp1 && ! elinkp->portp2) {
		if (side == 0 || side == 3)
			ShowProblem(format, indent, detail, "Missing Link");
		ret = LINK_VERIFY_MISSING;	// missing link
	} else if (elinkp->portp1 && elinkp->portp2) {
		if (elinkp->portp1->elinkp != elinkp) {
			// duplicate port, or incomplete/duplicate link in input topology
			ret = LINK_VERIFY_DUP;
			if (side == 0 || side == 1)
				ShowProblem(format, indent, detail, "Duplicate Port in input or incorrectly cabled");
		}
		if (elinkp->portp2->elinkp != elinkp) {
			// duplicate port, or incomplete/duplicate link in input topology
			ret = LINK_VERIFY_DUP;
			if (side == 0 || side == 2)
				ShowProblem(format, indent, detail, "Duplicate Port in input or incorrectly cabled");
		}
		if (ret == LINK_VERIFY_DUP && side == 3) {
			ShowProblem(format, indent, detail, "Duplicate Port in input or incorrectly cabled");
		}
	} else { /* elinkp->portp1 || elinkp->portp2 */
		// only 1 side resolved -> incorrectly cabled
		ret = LINK_VERIFY_CONN;
		if (detail >= 0 && (side == 0 || side == 3)) {
			if (elinkp->portp1) {
				if (elinkp->portp1->neighbor) {
					ShowProblem(format, indent, detail, "Incorrect Link, 2nd port found to be:");
					ShowLinkPortBriefSummary(elinkp->portp1->neighbor, "    ", 
							0, NULL, format, indent, 0);
				} else {
					ShowProblem(format, indent, detail, "Incorrect Link, 2nd port not found");
				}
			} else /* elinkp->portp2 */ {
				if (elinkp->portp2->neighbor) {
					ShowProblem(format, indent, detail, "Incorrect Link, 1st port found to be:");
					ShowLinkPortBriefSummary(elinkp->portp2->neighbor, "    ", 
							0, NULL, format, indent, 0);
				} else {
					ShowProblem(format, indent, detail, "Incorrect Link, 1st port not found");
				}
			}
		}
	}
	return ret;
}

void ShowLinkPortVerifySummaryCallback(uint64 context, PortData *portp,
									Format_t format, int indent, int detail)
{
	if (portp->elinkp && portp->neighbor->elinkp) {
		(void)PortVerify(portp, format, indent, detail);
	}
}

// show fabric link verify errors from portp to its neighbor
void ShowLinkVerifySummary(PortData *portp, Format_t format, int indent, int detail)
{
	ShowLinkFromBriefSummary(portp, 0, ShowLinkPortVerifySummaryCallback, format, indent, detail);

	ShowLinkToBriefSummary(portp->neighbor, "<-> ", FALSE, 0, ShowLinkPortVerifySummaryCallback, format, indent, detail);

	if (format == FORMAT_XML)
		indent +=4;
	if (portp->elinkp && portp->neighbor->elinkp
		&& (! PortVerify(portp, format, indent, -1)
			|| ! PortVerify(portp->neighbor, format, indent, -1)))
		ShowProblem(format, indent, detail, "Port Attributes Inconsistent");
	if (portp->elinkp && portp->neighbor->elinkp) {
		(void)LinkAttrVerify(portp->elinkp, portp, format, indent, detail);
	} else if (! portp->elinkp && ! portp->neighbor->elinkp) {
		ShowProblem(format, indent, detail, "Unexpected Link");
	}
	if (format == FORMAT_XML)
		printf("%*s</Link>\n", indent-4, "");
}

// header used before a series of links
void ShowExpectedLinkBriefSummaryHeader(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sRate MTU   NodeGUID          Port or PortGUID    Type Name\n", indent, "");
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

void ShowExpectedLinkPortVerifySummaryCallback(ExpectedLink *elinkp, uint8 side,
									Format_t format, int indent, int detail)
{
	(void)ExpectedLinkVerify(elinkp, side, format, indent, detail);
}

// show input link verify errors
void ShowExpectedLinkVerifySummary(ExpectedLink *elinkp, Format_t format, int indent, int detail)
{
	// top level information about link
	if (format == FORMAT_XML) {
		printf("%*s<Link id=\"0x%016"PRIx64"\">\n", indent, "", (uint64)(uintn)elinkp);
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
	ShowExpectedLinkPortSelBriefSummary("", elinkp, elinkp->portselp1,
					1, ShowExpectedLinkPortVerifySummaryCallback,
					format, indent, detail);

	// To Side (Port 2)
	ShowExpectedLinkPortSelBriefSummary("", elinkp, elinkp->portselp2,
					2, ShowExpectedLinkPortVerifySummaryCallback,
					format, indent, detail);

	// Summary information about Link itself
	if (detail && format != FORMAT_XML)
		ShowExpectedLinkBriefSummary(elinkp, format, indent+4, detail-1);
	(void)ExpectedLinkVerify(elinkp, 3, format, indent, detail);
	if (format == FORMAT_XML)
		printf("%*s</Link>\n", indent-4, "");
}

// Verify links in fabric against specified topology
void ShowVerifyLinksReport(Point *focus, report_t report, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 counts[LINK_VERIFY_MAX+1] = {0};
	uint32 port_errors = 0;
	uint32 link_errors = 0;
	uint32 fabric_checked = 0;
	uint32 input_checked = 0;
	char *xml_prefix = "";
	char *prefix = "";
	char *report_name = "";
	LinkVerifyResult_t res;

	switch (report) {
	default:	// should not happen, but just in case
		ASSERT(0);
	case REPORT_VERIFYLINKS:
		report_name = "verifylinks";
		xml_prefix = "";
		prefix = "";
		break;
	case REPORT_VERIFYEXTLINKS:
		report_name = "verifyextlinks";
		xml_prefix = "Ext";
		prefix = "External ";
		break;
	case REPORT_VERIFYFILINKS:
		report_name = "verifyfilinks";
		xml_prefix = "FI";
		prefix = "FI ";
		break;
	case REPORT_VERIFYISLINKS:
		report_name = "verifyislinks";
		xml_prefix = "IS";
		prefix = "Inter-Switch ";
		break;
	case REPORT_VERIFYEXTISLINKS:
		report_name = "verifyextislinks";
		xml_prefix = "ExtIS";
		prefix = "External Inter-Switch ";
		break;
	}
	// intro for report
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%sLinks Topology Verification\n", indent, "", prefix);
		break;
	case FORMAT_XML:
		printf("%*s<Verify%sLinks> <!-- %sLinks Topology Verification -->\n", indent, "", xml_prefix, prefix);
		indent+=4;
		break;
	default:
		break;
	}
	if (! g_topology_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: -T option not specified\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: -T option not specified -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	if (! (g_Fabric.flags & (FF_EXPECTED_LINKS|FF_EXPECTED_EXTLINKS))) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: no LinkSummary nor ExternalLinkSummary information provided\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: no LinkSummary nor ExternalLinkSummary information provided -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	if (0 == (report & (REPORT_VERIFYEXTLINKS|REPORT_VERIFYEXTISLINKS))
		&& ((g_Fabric.flags & (FF_EXPECTED_LINKS|FF_EXPECTED_EXTLINKS)) == FF_EXPECTED_EXTLINKS)) {
		fprintf(stderr, "opareport: Warning: %s requested, but only ExternalLinkSummary information provided\n", report_name);
	}

	ShowPointFocus(focus, (FIND_FLAG_FABRIC|FIND_FLAG_ELINK), format, indent, detail);

	// First we look at all the fabric links
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%sLinks Found with incorrect configuration:\n", indent, "", prefix);
		break;
	case FORMAT_XML:
		printf("%*s<!-- %sLinks Found with incorrect configuration -->\n", indent, "", prefix);
		break;
	default:
		break;
	}
	for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
		PortData *portp1, *portp2;

		portp1 = (PortData *)QListObj(p);
		// to avoid duplicated processing, only process "from" ports in link
		if (! portp1->from)
			continue;

		switch (report) {
		default:	// should not happen, but just in case
		case REPORT_VERIFYLINKS:
			// process all links
			break;
		case REPORT_VERIFYEXTLINKS:
			if (isInternalLink(portp1))
				continue;
			break;
		case REPORT_VERIFYFILINKS:
			if (! isFILink(portp1))
				continue;
			break;
		case REPORT_VERIFYISLINKS:
			if (! isISLink(portp1))
				continue;
			break;
		case REPORT_VERIFYEXTISLINKS:
			if (isInternalLink(portp1))
				continue;
			if (! isISLink(portp1))
				continue;
			break;
		}

		portp2 = portp1->neighbor;
		// We process only links whose PortData or resolved ExpectedLink
		// match the focus
		if (! ( ComparePortPoint(portp1, focus)
				|| ComparePortPoint(portp2, focus)
				|| (portp1->elinkp && CompareExpectedLinkPoint(portp1->elinkp, focus))
				|| (portp2->elinkp && CompareExpectedLinkPoint(portp2->elinkp, focus))))
			continue;
		fabric_checked++;
		// detail=-1 in LinkFabricVerify will suppress its output
		res = LinkFabricVerify(portp1, format, indent, -1);
		counts[res]++;
		if (res != LINK_VERIFY_OK) {
			if (detail) {
				if (port_errors) {
					if (format == FORMAT_TEXT) {
						printf("\n");	// blank line between links
					}
				} else {
					ShowLinkBriefSummaryHeader(format, indent, detail-1);
				}
				ShowLinkVerifySummary(portp1, format, indent, detail-1);
			}
			port_errors++;
		}
	}
	switch (format) {
	case FORMAT_TEXT:
		if (detail && port_errors)
			printf("\n");	// blank line between links
		printf("%*s%u of %u Fabric Links Checked\n", indent, "",
					fabric_checked, g_Fabric.LinkCount);
		break;
	case FORMAT_XML:
		XmlPrintDec("FabricLinksChecked", fabric_checked, indent);
		XmlPrintDec("TotalFabricLinks", g_Fabric.LinkCount, indent);
		break;
	default:
		break;
	}

	// now look at all the expected Links from the input topology
	switch (format) {
	case FORMAT_TEXT:
		printf("\n%*s%sLinks Expected but Missing, Duplicate in input or Incorrect:\n", indent, "", prefix);
		break;
	case FORMAT_XML:
		printf("%*s<!-- %sLinks Expected but Missing, Duplicate in input or Incorrect -->\n", indent, "", prefix);
		break;
	default:
		break;
	}
	for (p=QListHead(&g_Fabric.ExpectedLinks); p != NULL; p = QListNext(&g_Fabric.ExpectedLinks, p)) {
		ExpectedLink *elinkp = (ExpectedLink *)QListObj(p);

		// do our best to filter expected links
		// the is*ExpectedLink functions are purposely generously inclusive
		switch (report) {
		default:	// should not happen, but just in case
		case REPORT_VERIFYLINKS:
			// process all links
			break;
		case REPORT_VERIFYEXTLINKS:
			if (! isExternalExpectedLink(elinkp))
				continue;
			break;
		case REPORT_VERIFYFILINKS:
			if (! isFIExpectedLink(elinkp))
				continue;
			break;
		case REPORT_VERIFYISLINKS:
			if (! isISExpectedLink(elinkp))
				continue;
			break;
		case REPORT_VERIFYEXTISLINKS:
			if (! isExternalExpectedLink(elinkp))
				continue;
			if (! isISExpectedLink(elinkp))
				continue;
			break;
		}

		// We process only elinks whose resolved ports or ExpectedLink
		// match the focus
		if (! ( (elinkp->portp1 && ComparePortPoint(elinkp->portp1, focus))
				|| (elinkp->portp2 && ComparePortPoint(elinkp->portp2, focus))
				|| CompareExpectedLinkPoint(elinkp, focus)))
			continue;
		input_checked++;
		// detail=-1 in ExpectedLinkVerify will suppress its output
		res = ExpectedLinkVerify(elinkp, 0, format, indent, -1);
		counts[res]++;
		if (res != LINK_VERIFY_OK) {
			if (detail) {
				if (link_errors) {
					if (format == FORMAT_TEXT) {
						printf("\n");	// blank line between links
					}
				} else {
					ShowExpectedLinkBriefSummaryHeader(format, indent, detail-1);
				}
				ShowExpectedLinkVerifySummary(elinkp, format, indent, detail-1);
			}
			link_errors++;
		}
	}
	switch (format) {
	case FORMAT_TEXT:
		if (detail && link_errors)
			printf("\n");	// blank line between links
		printf("%*s%u of %u Input Links Checked\n", indent, "",
					input_checked, QListCount(&g_Fabric.ExpectedLinks));
		break;
	case FORMAT_XML:
		XmlPrintDec("InputLinksChecked", input_checked, indent);
		XmlPrintDec("TotalInputLinks", QListCount(&g_Fabric.ExpectedLinks), indent);
		break;
	default:
		break;
	}

	// final summary information
	switch (format) {
	case FORMAT_TEXT:
		printf("\n%*sTotal of %u Incorrect Links found\n", indent, "", port_errors+link_errors);
		printf("%*s%u Missing, %u Unexpected, %u Misconnected, %u Duplicate, %u Different\n", indent, "",
				counts[LINK_VERIFY_MISSING], counts[LINK_VERIFY_UNEXPECTED],
				counts[LINK_VERIFY_CONN], counts[LINK_VERIFY_DUP],
				counts[LINK_VERIFY_DIFF]);
		break;
	case FORMAT_XML:
		XmlPrintDec("TotalLinksIncorrect", port_errors+link_errors, indent);
		XmlPrintDec("Missing", counts[LINK_VERIFY_MISSING], indent);
		XmlPrintDec("Unexpected", counts[LINK_VERIFY_UNEXPECTED], indent);
		XmlPrintDec("Misconnected", counts[LINK_VERIFY_CONN], indent);
		XmlPrintDec("Duplicate", counts[LINK_VERIFY_DUP], indent);
		XmlPrintDec("Different", counts[LINK_VERIFY_DIFF], indent);
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
		printf("%*s</Verify%sLinks>\n", indent, "", xml_prefix);
		break;
	default:
		break;
	}
}

// verify a node against its corresponding ExpectedNode
// Only valid to be called for SMs with ExpectedNode
// returns FALSE if any discrepencies
// only does output if detail >= 0
boolean NodeVerify(NodeData *nodep, Format_t format, int indent, int detail)
{
	ExpectedNode *enodep = nodep->enodep;
	boolean ret=TRUE;

	ASSERT(enodep);
	if (enodep->NodeGUID && enodep->NodeGUID != nodep->NodeInfo.NodeGUID) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"NodeGuid mismatch: expected: 0x%016"PRIx64" found: 0x%016"PRIx64,
			enodep->NodeGUID, nodep->NodeInfo.NodeGUID);
	}
	if (enodep->NodeDesc
		&& 0 != strncmp(enodep->NodeDesc,
						(char*)nodep->NodeDesc.NodeString,
						NODE_DESCRIPTION_ARRAY_SIZE)) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"NodeDesc mismatch: expected: '%s' found: '%.*s'",
			enodep->NodeDesc, NODE_DESCRIPTION_ARRAY_SIZE,
			(char*)nodep->NodeDesc.NodeString);
	}
	if (enodep->NodeType && enodep->NodeType != nodep->NodeInfo.NodeType) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"NodeType mismatch: expected: %s found: %s",
			StlNodeTypeToText(enodep->NodeType),
			StlNodeTypeToText(nodep->NodeInfo.NodeType));
	}
	return ret;
}

typedef enum {
	NODE_VERIFY_OK = 0,	// node matches input topology
	NODE_VERIFY_DIFF = 1, // node fully resolved but diff from input topology
	NODE_VERIFY_UNEXPECTED = 2, // node not found in input topology
	NODE_VERIFY_MISSING = 3, // node not found in fabric, but in input topology
	NODE_VERIFY_DUP = 4, // possible duplicate in input topology
	NODE_VERIFY_MAX=4	// maximum value of the above
} NodeVerifyResult_t;

// check both fabric node against input topology
// only does output if detail >= 0
NodeVerifyResult_t NodeFabricVerify(NodeData *nodep, Format_t format, int indent, int detail)
{
	NodeVerifyResult_t ret = NODE_VERIFY_OK;

	// all checks which are based off of found node in fabric
	if (nodep->enodep) {
		if (! NodeVerify(nodep, format, indent, detail))
			ret = NODE_VERIFY_DIFF;
	} else {
		ret = NODE_VERIFY_UNEXPECTED; // extra node
		ShowProblem(format, indent, detail, "Unexpected %s",
			StlNodeTypeToText(nodep->NodeInfo.NodeType));
	}
	return ret;
}

// compare ExpectedNode against fabric
// NodeFabricVerify will have caught nodes which are different or extra
// this is focused on duplicate ExpectedNodes or missing nodes
// only does output if detail >= 0
NodeVerifyResult_t ExpectedNodeVerify(ExpectedNode *enodep, Format_t format, int indent, int detail)
{
	NodeVerifyResult_t ret = NODE_VERIFY_OK;

	if (! enodep->nodep) {
		ShowProblem(format, indent, detail, "Missing %s",
					StlNodeTypeToText(enodep->NodeType));
		ret = NODE_VERIFY_MISSING;	// missing link
	} else {
		if (enodep->nodep->enodep != enodep) {
			// duplicate node in input topology
			ret = NODE_VERIFY_DUP;
			ShowProblem(format, indent, detail, "Duplicate %s in input",
					StlNodeTypeToText(enodep->NodeType));
		}
	}
	return ret;
}

// show fabric node verify errors
void ShowNodeVerifySummary(NodeData *nodep, Format_t format, int indent, int detail)
{
	ShowNodeBriefSummary(nodep, NULL, FALSE, format, indent, 0);

	if (format == FORMAT_XML)
		indent +=4;
	if (nodep->enodep && ! NodeVerify(nodep, format, indent, detail-1)) {
		ShowProblem(format, indent, detail, "Node Attributes Inconsistent");
	} else {
		ShowProblem(format, indent, detail, "Unexpected %s",
			StlNodeTypeToText(nodep->NodeInfo.NodeType));
	}
	if (format == FORMAT_XML)
		printf("%*s</Node>\n", indent-4, "");
}

// show input node verify errors
void ShowExpectedNodeVerifySummary(ExpectedNode *enodep, Format_t format, int indent, int detail)
{
	ShowExpectedNodeBriefSummary("", enodep, "Node", FALSE, format, indent, detail-1);
	if (format == FORMAT_XML)
		indent+=4;
	(void)ExpectedNodeVerify(enodep, format, indent, detail);
	if (format == FORMAT_XML)
		printf("%*s</Node>\n", indent-4, "");
}

// Verify nodes in fabric against specified topology
void ShowVerifyNodesReport(Point *focus, uint8 NodeType, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 counts[NODE_VERIFY_MAX+1] = {0};
	uint32 fabric_errors = 0;
	uint32 input_errors = 0;
	uint32 fabric_checked = 0;
	uint32 input_checked = 0;
	NodeVerifyResult_t res;
	const char *NodeTypeText = StlNodeTypeToText(NodeType);
	QUICK_LIST *fabric_listp;
	QUICK_LIST *input_listp;

	switch (NodeType) {
	case STL_NODE_FI:
			fabric_listp = &g_Fabric.AllFIs;
			input_listp = &g_Fabric.ExpectedFIs;
			break;
	case STL_NODE_SW:
			fabric_listp = &g_Fabric.AllSWs;
			input_listp = &g_Fabric.ExpectedSWs;
			break;
	default:
			ASSERT(0);
			break;
	}

	// intro for report
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%ss Topology Verification\n", indent, "", NodeTypeText);
		break;
	case FORMAT_XML:
		printf("%*s<Verify%ss> <!-- %s Topology Verification -->\n", indent, "", NodeTypeText, NodeTypeText);
		indent+=4;
		break;
	default:
		break;
	}
	if (! g_topology_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: -T option not specified\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: -T option not specified -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	if (! (g_Fabric.flags & FF_EXPECTED_NODES)) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: no Nodes information provided\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: no Nodes information provided -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}

	ShowPointFocus(focus, (FIND_FLAG_FABRIC|FIND_FLAG_ENODE), format, indent, detail);

	// First we look at all the fabric nodes
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%ss Found with incorrect configuration:\n", indent, "", NodeTypeText);
		break;
	case FORMAT_XML:
		printf("%*s<!-- %ss Found with incorrect configuration -->\n", indent, "", NodeTypeText);
		break;
	default:
		break;
	}
	for (p=QListHead(fabric_listp); p != NULL; p = QListNext(fabric_listp, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);

		// We process only nodes whose NodeData or resolved ExpectedNode
		// match the focus
		if (! ( CompareNodePoint(nodep, focus)
				|| (nodep->enodep && CompareExpectedNodePoint(nodep->enodep, focus))))
			continue;
		fabric_checked++;
		// detail=-1 in NodeFabricVerify will suppress its output
		res = NodeFabricVerify(nodep, format, indent, -1);
		counts[res]++;
		if (res != NODE_VERIFY_OK) {
			if (detail) {
				if (fabric_errors) {
					if (format == FORMAT_TEXT) {
						printf("\n");	// blank line between links
					}
				} else {
					// use detail 0 so don't show Port stuff
					ShowNodeBriefSummaryHeadings(format, indent, 0 /*detail-1*/);
				}
				ShowNodeVerifySummary(nodep, format, indent, detail-1);
			}
			fabric_errors++;
		}
	}
	switch (format) {
	case FORMAT_TEXT:
		if (detail && fabric_errors)
			printf("\n");	// blank line between links
		printf("%*s%u of %u Fabric %ss Checked\n", indent, "",
					fabric_checked, QListCount(fabric_listp), NodeTypeText);
		break;
	case FORMAT_XML:
		switch (NodeType) {
		case STL_NODE_FI:
			XmlPrintDec("FabricFIsChecked", fabric_checked, indent);
			XmlPrintDec("TotalFabricFIs", QListCount(fabric_listp), indent);
			break;
		case STL_NODE_SW:
			XmlPrintDec("FabricSWsChecked", fabric_checked, indent);
			XmlPrintDec("TotalFabricSWs", QListCount(fabric_listp), indent);
			break;
		default:
			break;
		}
	default:
		break;
	}

	// now look at all the expected Nodes from the input topology
	switch (format) {
	case FORMAT_TEXT:
		printf("\n%*s%ss Expected but Missing or Duplicate in input:\n", indent, "", NodeTypeText);
		break;
	case FORMAT_XML:
		printf("%*s<!-- %ss Expected but Missing or Duplicate in input -->\n", indent, "", NodeTypeText);
		break;
	default:
		break;
	}
	for (p=QListHead(input_listp); p != NULL; p = QListNext(input_listp, p)) {
		ExpectedNode *enodep = (ExpectedNode *)QListObj(p);

		// We process only enodes whose resolved node or ExpectedNode
		// match the focus
		if (! ( (enodep->nodep && CompareNodePoint(enodep->nodep, focus))
				|| CompareExpectedNodePoint(enodep, focus)))
			continue;
		input_checked++;
		// detail=-1 in ExpectedNodeVerify will suppress its output
		res = ExpectedNodeVerify(enodep, format, indent, -1);
		counts[res]++;
		if (res != NODE_VERIFY_OK) {
			if (detail) {
				if (input_errors) {
					if (format == FORMAT_TEXT) {
						printf("\n");	// blank line between links
					}
				} else {
					// use detail 0 so don't show Port stuff
					ShowNodeBriefSummaryHeadings(format, indent, 0 /*detail-1*/);
				}
				ShowExpectedNodeVerifySummary(enodep, format, indent, detail-1);
			}
			input_errors++;
		}
	}
	switch (format) {
	case FORMAT_TEXT:
		if (detail && input_errors)
			printf("\n");	// blank line between links
		printf("%*s%u of %u Input %ss Checked\n", indent, "",
					input_checked, QListCount(input_listp), NodeTypeText);
		break;
	case FORMAT_XML:
		switch (NodeType) {
		case STL_NODE_FI:
			XmlPrintDec("InputFIsChecked", input_checked, indent);
			XmlPrintDec("TotalInputFIs", QListCount(input_listp), indent);
			break;
		case STL_NODE_SW:
			XmlPrintDec("InputSWsChecked", input_checked, indent);
			XmlPrintDec("TotalInputSWs", QListCount(input_listp), indent);
			break;
		default:
			break;
		}
	default:
		break;
	}

	// final summary information
	switch (format) {
	case FORMAT_TEXT:
		printf("\n%*sTotal of %u Incorrect %ss found\n", indent, "", fabric_errors+input_errors, NodeTypeText);
		printf("%*s%u Missing, %u Unexpected, %u Duplicate, %u Different\n", indent, "",
				counts[NODE_VERIFY_MISSING], counts[NODE_VERIFY_UNEXPECTED],
				counts[NODE_VERIFY_DUP],
				counts[NODE_VERIFY_DIFF]);
		break;
	case FORMAT_XML:
		switch (NodeType) {
		case STL_NODE_FI:
			XmlPrintDec("TotalFIsIncorrect", fabric_errors+input_errors, indent);
			break;
		case STL_NODE_SW:
			XmlPrintDec("TotalSWsIncorrect", fabric_errors+input_errors, indent);
			break;
		default:
			break;
		}
		XmlPrintDec("Missing", counts[NODE_VERIFY_MISSING], indent);
		XmlPrintDec("Unexpected", counts[NODE_VERIFY_UNEXPECTED], indent);
		XmlPrintDec("Duplicate", counts[NODE_VERIFY_DUP], indent);
		XmlPrintDec("Different", counts[NODE_VERIFY_DIFF], indent);
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
		printf("%*s</Verify%ss>\n", indent, "", NodeTypeText);
		break;
	default:
		break;
	}
}

// verify a SM against its corresponding ExpectedSM
// Only valid to be called for nodes with ExpectedSM
// returns FALSE if any discrepencies
// only does output if detail >= 0
boolean SMVerify(SMData *smp, Format_t format, int indent, int detail)
{
	ExpectedSM *esmp = smp->esmp;
	boolean ret=TRUE;

	ASSERT(esmp);
	if (esmp->NodeGUID && esmp->NodeGUID != smp->portp->nodep->NodeInfo.NodeGUID) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"NodeGuid mismatch: expected: 0x%016"PRIx64" found: 0x%016"PRIx64,
			esmp->NodeGUID, smp->portp->nodep->NodeInfo.NodeGUID);
	}
	if (esmp->NodeDesc
		&& 0 != strncmp(esmp->NodeDesc,
						(char*)smp->portp->nodep->NodeDesc.NodeString,
						NODE_DESCRIPTION_ARRAY_SIZE)) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"NodeDesc mismatch: expected: '%s' found: '%.*s'",
			esmp->NodeDesc, NODE_DESCRIPTION_ARRAY_SIZE,
			(char*)smp->portp->nodep->NodeDesc.NodeString);
	}
	if (esmp->gotPortNum && esmp->PortNum != smp->portp->PortNum) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"PortNum mismatch: expected: %3u found: %3u",
			esmp->PortNum, smp->portp->PortNum);
	}
	if (esmp->PortGUID && esmp->PortGUID != smp->portp->PortGUID) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"PortGuid mismatch: expected: 0x%016"PRIx64" found: 0x%016"PRIx64,
			esmp->PortGUID, smp->portp->PortGUID);
	}
	if (esmp->NodeType && esmp->NodeType != smp->portp->nodep->NodeInfo.NodeType) {
		ret = FALSE;
		ShowProblem(format, indent, detail, 
			"NodeType mismatch: expected: %s found: %s",
			StlNodeTypeToText(esmp->NodeType),
			StlNodeTypeToText(smp->portp->nodep->NodeInfo.NodeType));
	}
	return ret;
}

typedef enum {
	SM_VERIFY_OK = 0,	// SM matches input topology
	SM_VERIFY_DIFF = 1, // SM fully resolved but diff from input topology
	SM_VERIFY_UNEXPECTED = 2, // SM not found in input topology
	SM_VERIFY_MISSING = 3, // SM not found in fabric, but in input topology
	SM_VERIFY_DUP = 4, // possible duplicate in input topology
	SM_VERIFY_MAX=4	// maximum value of the above
} SMVerifyResult_t;

// check both fabric node against input topology
// only does output if detail >= 0
SMVerifyResult_t SMFabricVerify(SMData *smp, Format_t format, int indent, int detail)
{
	SMVerifyResult_t ret = SM_VERIFY_OK;

	// all checks which are based off of found SM in fabric
	if (smp->esmp) {
		if (! SMVerify(smp, format, indent, detail))
			ret = SM_VERIFY_DIFF;
	} else {
		ret = SM_VERIFY_UNEXPECTED; // extra node
		ShowProblem(format, indent, detail, "Unexpected SM");
	}
	return ret;
}

// compare ExpectedSM against fabric
// SMFabricVerify will have caught nodes which are different or extra
// this is focused on duplicate ExpectedSMs or missing SMs
// only does output if detail >= 0
SMVerifyResult_t ExpectedSMVerify(ExpectedSM *esmp, Format_t format, int indent, int detail)
{
	SMVerifyResult_t ret = SM_VERIFY_OK;

	if (! esmp->smp) {
		ShowProblem(format, indent, detail, "Missing SM");
		ret = SM_VERIFY_MISSING;	// missing link
	} else {
		if (esmp->smp->esmp != esmp) {
			// duplicate node in input topology
			ret = SM_VERIFY_DUP;
			ShowProblem(format, indent, detail, "Duplicate SM in input");
		}
	}
	return ret;
}

// show fabric SM verify errors
void ShowSMVerifySummary(SMData *smp, Format_t format, int indent, int detail)
{
	ShowVerifySMBriefSummary(smp, FALSE, format, indent, 0);

	if (format == FORMAT_XML)
		indent +=4;
	if (smp->esmp && ! SMVerify(smp, format, indent, detail-1)) {
		ShowProblem(format, indent, detail, "SM Attributes Inconsistent");
	} else {
		ShowProblem(format, indent, detail, "Unexpected SM");
	}
	if (format == FORMAT_XML)
		printf("%*s</SM>\n", indent-4, "");
}

// show input SM verify errors
void ShowExpectedSMVerifySummary(ExpectedSM *esmp, Format_t format, int indent, int detail)
{
	ShowExpectedSMBriefSummary("", esmp, "SM", FALSE, format, indent, detail-1);
	if (format == FORMAT_XML)
		indent+=4;
	(void)ExpectedSMVerify(esmp, format, indent, detail);
	if (format == FORMAT_XML)
		printf("%*s</SM>\n", indent-4, "");
}

// Verify SMs in fabric against specified topology
void ShowVerifySMsReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	cl_map_item_t *ip;
	uint32 counts[SM_VERIFY_MAX+1] = {0};
	uint32 fabric_errors = 0;
	uint32 input_errors = 0;
	uint32 fabric_checked = 0;
	uint32 input_checked = 0;
	SMVerifyResult_t res;

	// intro for report
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sSMs Topology Verification\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<VerifySMs> <!-- SM Topology Verification -->\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	if (! g_topology_in_file) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: -T option not specified\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: -T option not specified -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}
	if (! (g_Fabric.flags & FF_EXPECTED_NODES)) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: no Nodes information provided\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: no Nodes information provided -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}

	ShowPointFocus(focus, (FIND_FLAG_FABRIC|FIND_FLAG_ESM), format, indent, detail);

	// First we look at all the fabric SMs
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sSMs Found with incorrect configuration:\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<!-- SMs Found with incorrect configuration -->\n", indent, "");
		break;
	default:
		break;
	}
	for (ip=cl_qmap_head(&g_Fabric.AllSMs); ip != cl_qmap_end(&g_Fabric.AllSMs); ip = cl_qmap_next(ip)) {
		SMData *smp = PARENT_STRUCT(ip, SMData, AllSMsEntry);

		// We process only SMs whose SMData or resolved ExpectedSM
		// match the focus
		if (! ( CompareSmPoint(smp, focus)
				|| (smp->esmp && CompareExpectedSMPoint(smp->esmp, focus))))
			continue;
		fabric_checked++;
		// detail=-1 in SMFabricVerify will suppress its output
		res = SMFabricVerify(smp, format, indent, -1);
		counts[res]++;
		if (res != SM_VERIFY_OK) {
			if (detail) {
				if (fabric_errors) {
					if (format == FORMAT_TEXT) {
						printf("\n");	// blank line between links
					}
				} else {
					ShowVerifySMBriefSummaryHeadings(format, indent, detail-1);
				}
				ShowSMVerifySummary(smp, format, indent, detail-1);
			}
			fabric_errors++;
		}
	}
	switch (format) {
	case FORMAT_TEXT:
		if (detail && fabric_errors)
			printf("\n");	// blank line between links
		printf("%*s%u of %u Fabric SMs Checked\n", indent, "",
					fabric_checked, (unsigned)cl_qmap_count(&g_Fabric.AllSMs));
		break;
	case FORMAT_XML:
		XmlPrintDec("FabricSMsChecked", fabric_checked, indent);
		XmlPrintDec("TotalFabricSMs", (unsigned)cl_qmap_count(&g_Fabric.AllSMs), indent);
		break;
	default:
		break;
	}

	// now look at all the expected Nodes from the input topology
	switch (format) {
	case FORMAT_TEXT:
		printf("\n%*sSMs Expected but Missing or Duplicate in input:\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<!-- SMs Expected but Missing or Duplicate in input -->\n", indent, "");
		break;
	default:
		break;
	}
	for (p=QListHead(&g_Fabric.ExpectedSMs); p != NULL; p = QListNext(&g_Fabric.ExpectedSMs, p)) {
		ExpectedSM *esmp = (ExpectedSM *)QListObj(p);

		// We process only ExpectedSMs whose resolved SMData or ExpectedSM
		// match the focus
		if (! ( (esmp->smp && CompareSmPoint(esmp->smp, focus))
				|| CompareExpectedSMPoint(esmp, focus)))
			continue;
		input_checked++;
		// detail=-1 in ExpectedSMVerify will suppress its output
		res = ExpectedSMVerify(esmp, format, indent, -1);
		counts[res]++;
		if (res != SM_VERIFY_OK) {
			if (detail) {
				if (input_errors) {
					if (format == FORMAT_TEXT) {
						printf("\n");	// blank line between links
					}
				} else {
					ShowVerifySMBriefSummaryHeadings(format, indent, detail-1);
				}
				ShowExpectedSMVerifySummary(esmp, format, indent, detail-1);
			}
			input_errors++;
		}
	}
	switch (format) {
	case FORMAT_TEXT:
		if (detail && input_errors)
			printf("\n");	// blank line between links
		printf("%*s%u of %u Input SMs Checked\n", indent, "",
					input_checked, QListCount(&g_Fabric.ExpectedSMs));
		break;
	case FORMAT_XML:
		XmlPrintDec("InputSMsChecked", input_checked, indent);
		XmlPrintDec("TotalInputSMs", QListCount(&g_Fabric.ExpectedSMs), indent);
		break;
	default:
		break;
	}

	// final summary information
	switch (format) {
	case FORMAT_TEXT:
		printf("\n%*sTotal of %u Incorrect SMs found\n", indent, "", fabric_errors+input_errors);
		printf("%*s%u Missing, %u Unexpected, %u Duplicate, %u Different\n", indent, "",
				counts[SM_VERIFY_MISSING], counts[SM_VERIFY_UNEXPECTED],
				counts[SM_VERIFY_DUP],
				counts[SM_VERIFY_DIFF]);
		break;
	case FORMAT_XML:
		XmlPrintDec("TotalSMsIncorrect", fabric_errors+input_errors, indent);
		break;
		XmlPrintDec("Missing", counts[SM_VERIFY_MISSING], indent);
		XmlPrintDec("Unexpected", counts[SM_VERIFY_UNEXPECTED], indent);
		XmlPrintDec("Duplicate", counts[SM_VERIFY_DUP], indent);
		XmlPrintDec("Different", counts[SM_VERIFY_DIFF], indent);
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
		printf("%*s</VerifySMs>\n", indent, "");
		break;
	default:
		break;
	}
}

