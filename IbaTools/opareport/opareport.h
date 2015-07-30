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

#ifndef _OPAREPORT_H
#define _OPAREPORT_H

#include <iba/ibt.h>

#include <iba/ipublic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#define _GNU_SOURCE

#include <ixml_ib.h>
#include <topology.h>

#ifdef LINUX
#define CONFIG_FILE	"/etc/sysconfig/opa/opamon.conf"
#else
#error "unsupported OS"
#endif

#define MYTAG MAKE_MEM_TAG('i','r', 'e', 'p')

#ifdef __cplusplus
extern "C" {
#endif

/* output format */
typedef enum {
	FORMAT_TEXT,
	FORMAT_XML,
} Format_t;


extern char *g_name_marker;					// what to output when g_noname set
extern uint8			g_verbose;
extern int				g_quiet;			// omit progress output
extern int				g_noname;			// omit names
extern int				g_stats;			// get port stats
extern char*			g_topology_in_file;	// input file being parsed
extern EUI64			g_portGuid;			// local port to use to access fabric
extern IB_PORT_ATTRIBUTES	*g_portAttrib;	// attributes for our local port
extern int				g_exitstatus;	// TBD make this static for opareport.c
extern FabricData_t		g_Fabric;		// fabric we are analyzing

#define DBGPRINT(format, args...) \
	do { if (g_verbose) { fflush(stdout); fprintf(stderr, format, ##args); } } while (0)

#define PROGRESS_PRINT(newline, format, args...) \
	do { if (! g_quiet) { ProgressPrint(newline, format, ##args); }  } while (0)

// macro to always return a valid pointer for use in %s formats
#define OPTIONAL_STR(s) (s?s:"")

//extern void DisplayTraceRecord(STL_TRACE_RECORD *pTraceRecord, int indent);

extern void DisplaySeparator(void);
extern void ShowLinkBriefSummaryHeader(Format_t format, int indent, int detail);
typedef enum {
	LINKPORT_NOMTU=1,
	LINKPORT_MTU=2,
	LINKPORT_BLANKMTU=3
} LinkPortMtu_t;
typedef void LinkPortSummaryDetailCallback_t(uint64 context, PortData *portp, Format_t format, int indent, int detail);
extern void ShowLinkPortBriefSummary(PortData *portp, const char *prefix,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail);
extern void ShowCableSummary(STL_CABLE_INFO *pCable, Format_t format, 
			int indent, int detail);
// show cable information for a link in brief summary format
extern void ShowExpectedLinkBriefSummary(ExpectedLink *elinkp,
			Format_t format, int indent, int detail);
// show from side of a link, need to later call ShowLinkToBriefSummary
// useful when traversing trace route and don't have both sides of link handy
extern void ShowLinkFromBriefSummary(PortData *portp1,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail);
// show to side of a link, need to call ShowLinkFromBriefSummary before this
// useful when traversing trace route and don't have both sides of link handy
// portp2 can be NULL to "close" the From Summary without additional
// port information and no cable information
// This is useful when reporting trace routes which stay within a single port
extern void ShowLinkToBriefSummary(PortData *portp2, const char* toprefix, boolean close_link,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail);

extern void ShowNodeBriefSummaryHeadings(Format_t format, int indent, int detail);

// output brief summary of a IB Node
extern void ShowNodeBriefSummary(NodeData *nodep, Point *focus,
				boolean close_node, Format_t format, int indent, int detail);
extern void ShowTraceRecord(STL_TRACE_RECORD *pTraceRecord, Format_t format, int indent, int detail);

// output brief summary of an expected IB Node
extern void ShowExpectedNodeBriefSummary(ExpectedNode *enodep,
				boolean close_node, Format_t format, int indent, int detail);

extern void ShowVerifySMBriefSummary(SMData *smp,
				boolean close_sm, Format_t format, int indent, int detail);

extern void ShowVerifySMBriefSummaryHeadings(Format_t format, int indent, int detail);

extern void ShowExpectedSMBriefSummary(ExpectedSM *esmp,
				boolean close_sm, Format_t format, int indent, int detail);

extern void ShowPointFocus(Point* focus, Format_t format, int indent, int detail);

// Verify ports in fabric against specified topology
extern void ShowVerifyLinksReport(Point *focus, boolean extOnly, Format_t format, int indent, int detail);

// Verify nodes in fabric against specified topology
extern void ShowVerifyNodesReport(Point *focus, uint8 NodeType, Format_t format, int indent, int detail);

// Verify SMs in fabric against specified topology
extern void ShowVerifySMsReport(Point *focus, Format_t format, int indent, int detail);

extern void XmlPrintHex64(const char *tag, uint64 value, int indent);
extern void XmlPrintHex32(const char *tag, uint32 value, int indent);
extern void XmlPrintHex16(const char *tag, uint16 value, int indent);
extern void XmlPrintHex8(const char *tag, uint8 value, int indent);
extern void XmlPrintDec(const char *tag, unsigned value, int indent);
extern void XmlPrintHex(const char *tag, unsigned value, int indent);
extern void XmlPrintStrLen(const char *tag, const char* value, int len, int indent);
extern void XmlPrintStr(const char *tag, const char* value, int indent);
extern void XmlPrintOptionalStr(const char *tag, const char* value, int indent);
extern void XmlPrintLID(const char *tag, IB_LID value, int indent);
extern void XmlPrintPKey(const char *tag, IB_P_KEY value, int indent);
extern void XmlPrintGID(const char *tag, IB_GID value, int indent);
extern void XmlPrintNodeType(uint8 value, int indent);
extern void XmlPrintNodeDesc(const char *value, int indent);
extern void XmlPrintIocIDString(const char *value, int indent);
extern void XmlPrintServiceName(const uchar *value, int indent);
extern void XmlPrintRate(uint8 value, int indent);
extern void XmlPrintLinkWidth(const char* tag_prefix, uint8 value, int indent);
extern void XmlPrintLinkSpeed(const char* tag_prefix, uint16 value, int indent);
extern void XmlPrintLinkStartTag(const char* tag, PortData *portp, int indent);

#ifdef __cplusplus
};
#endif

#endif /* _OPAREPORT_H */
