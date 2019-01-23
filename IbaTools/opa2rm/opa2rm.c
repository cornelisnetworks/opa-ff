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
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <arpa/inet.h>
#include <stl_helper.h>
#include <time.h>
#include <string.h>


#define MYTAG MAKE_MEM_TAG('o','2', 'r', 'm')

#define DBGPRINT(format, args...) \
	do { if (g_verbose) { fflush(stdout); fprintf(stderr, format, ##args); } } while (0)

#define PROGRESS_PRINT(newline, format, args...) \
	do { if (! g_quiet) { ProgressPrint(newline, format, ##args); }  } while (0)

#define PROGRESS_FREQ 1000

#define MIN_LIST_ITEMS 10	// for neighbor sw and fi DLISTs

// macro to always return a valid pointer for use in %s formats
#define OPTIONAL_STR(s) (s?s:"")

#define RANGE_IN_MIDDLE 0	// should ranges in middle of name be permitted

uint8			g_verbose		= 0;
uint8			g_quiet			= 0;	// omit progress output
#define MAX_FOCUS	10
char			*g_focus_arg[MAX_FOCUS];
Point			g_focus[MAX_FOCUS];
int				g_num_focus = 0;
char*			g_prefix = NULL;	// prefix for all FIs
char*			g_suffix = NULL;	// suffix for all FIs

// All the information about the fabric
FabricData_t	g_Fabric;

// In order to allow removal of spaces, sorting and compacting
// names into ranges (eg. compute[1-10]) we keep a NameData per
// name we want to show.  The NameData includes the reformated full name
// and the parsed elements of body, numeric range and end
typedef struct NameData_s {
	cl_map_item_t	NameListEntry;	// nameList, key is name
	char		name[NODE_DESCRIPTION_ARRAY_SIZE];	// full name

	// a name is broken into four parts, body is alphanumeric
	// start,end is a numeric part at the end of the body
	// prefix is a prefix we will add afterwards such as opa- for IPoIB on OPA
	// suffix is a suffix we will add afterwards such as -opa for IPoIB on OPA
	const char	*prefix;
	char		body[NODE_DESCRIPTION_ARRAY_SIZE];
	uint32		start, end;	// numeric range
	boolean		leadzero;	// did original name have leading zeros for number
	int8		numlen;		// number of characters in number in original name
	const char	*suffix;
} NameData_t;

// This will be attached to each switch node via ExpectedNode.context 
typedef struct SwitchLists_s {
	cl_qmap_t	fiNeighborNames;// filtered list of neighbor FIs NameData_t
	cl_qmap_t	swNeighborNames;// filtered list of neighbor SWs NameData_t
	uint8		tier;			// tier in tree of given switch
								// (distance from FIs)
	DLIST		swNeighbors;	// list of all neighbor SWs ExpectedNode*
} SwitchLists_t;

boolean CompareExpectedNodeFocus(ExpectedNode *enodep);
boolean CompareExpectedLinkFocus(ExpectedLink *elinkp);

// -------------- output of Expected* functions --------------------
// these next few functions are for error output and debug
// They allow dumping of the input topology file which was parsed
// output goes to stderr

// show 1 port selector in link data in brief form
void ShowExpectedLinkPortSelBriefSummary(ExpectedLink *elinkp, PortSelector *portselp,
			 uint8 side, int indent, int detail)
{
	DEBUG_ASSERT(side == 1 || side == 2);
	fprintf(stderr, "%*s%4s ", indent, "",
			(side == 1)?
				elinkp->expected_rate?
					StlStaticRateToText(elinkp->expected_rate)
					:""
			: "<-> ");
	if (side == 1 && elinkp->expected_mtu)
		fprintf(stderr, "%5s ", IbMTUToText(elinkp->expected_mtu));
	else
		fprintf(stderr, "     ");

	if (portselp) {
		if (portselp->NodeGUID)
			fprintf(stderr, "0x%016"PRIx64, portselp->NodeGUID);
		else
			fprintf(stderr, "                  ");
		if (portselp->gotPortNum)
			fprintf(stderr, " %3u               ",portselp->PortNum);
		else if (portselp->PortGUID)
			fprintf(stderr, " 0x%016"PRIx64, portselp->PortGUID);
		else
			fprintf(stderr, "                   ");
		if (portselp->NodeType)
			fprintf(stderr, " %s",
				StlNodeTypeToText(portselp->NodeType));
		else
			fprintf(stderr, "   ");
		if (portselp->NodeDesc)
			fprintf(stderr, " %.*s\n",
				NODE_DESCRIPTION_ARRAY_SIZE, portselp->NodeDesc);
		else
			fprintf(stderr, "\n");
		if (detail) {
			if (portselp->details) {
				fprintf(stderr, "%*sPortDetails: %s\n", indent+4, "", portselp->details);
			}
		}
	} else {
		fprintf(stderr, "unspecified\n");
	}
}


// show cable information for a link in multi-line format with field headings
void ShowExpectedLinkSummary(ExpectedLink *elinkp,
			int indent, int detail)
{
	// From Side (Port 1)
	ShowExpectedLinkPortSelBriefSummary(elinkp, elinkp->portselp1,
										1, indent, detail);
	// To Side (Port 2)
	ShowExpectedLinkPortSelBriefSummary(elinkp, elinkp->portselp2,
										2, indent, detail);
	if (elinkp->details) {
		fprintf(stderr, "%*sLinkDetails: %s\n", indent, "", elinkp->details);
	}
	if (elinkp->CableData.length || elinkp->CableData.label
		|| elinkp->CableData.details) {
		fprintf(stderr, "%*sCableLabel: %-*s  CableLen: %-*s\n",
			indent, "",
			CABLE_LABEL_STRLEN, OPTIONAL_STR(elinkp->CableData.label),
			CABLE_LENGTH_STRLEN, OPTIONAL_STR(elinkp->CableData.length));
		fprintf(stderr, "%*sCableDetails: %s\n",
			indent, "",
			OPTIONAL_STR(elinkp->CableData.details));
	}
}

// Verify links in fabric against specified topology
void ShowExpectedLinksReport(/*Point *focus,*/ int indent, int detail)
{
	LIST_ITEM *p;
	uint32 input_checked = 0;

	fprintf(stderr, "%*sLinks Topology Expected\n", indent, "");

	fprintf(stderr, "%*sRate MTU  NodeGUID          Port or PortGUID    Type Name\n", indent, "");
	if (detail && (g_Fabric.flags & FF_CABLEDATA)) {
		//fprintf(stderr, "%*sPortDetails\n", indent+4, "");
		//fprintf(stderr, "%*sLinkDetails\n", indent+4, "");
		fprintf(stderr, "%*sCable: %-*s %-*s\n", indent+4, "",
						CABLE_LABEL_STRLEN, "CableLabel",
						CABLE_LENGTH_STRLEN, "CableLen");
		fprintf(stderr, "%*s%s\n", indent+4, "", "CableDetails");
	}
	for (p=QListHead(&g_Fabric.ExpectedLinks); p != NULL; p = QListNext(&g_Fabric.ExpectedLinks, p)) {
		ExpectedLink *elinkp = (ExpectedLink *)QListObj(p);

		if (! elinkp)
			continue;
		if (! CompareExpectedLinkFocus(elinkp))
				continue;
		input_checked++;
		ShowExpectedLinkSummary(elinkp, indent, detail);
	}
	if (detail && input_checked)
		fprintf(stderr, "\n");	// blank line between links
	fprintf(stderr, "%*s%u of %u Input Links Shown\n", indent, "",
				input_checked, QListCount(&g_Fabric.ExpectedLinks));

	return;
}

// output brief summary of an expected IB Node
void ShowExpectedNodeBriefSummary(ExpectedNode *enodep, int indent, int detail)
{
	fprintf(stderr, "%*s", indent, "");
	if (enodep->NodeGUID)
		fprintf(stderr, "0x%016"PRIx64, enodep->NodeGUID);
	else
		fprintf(stderr, "                  ");
	if (enodep->NodeType)
		fprintf(stderr, " %s", StlNodeTypeToText(enodep->NodeType));
	else
		fprintf(stderr, "   ");
	if (enodep->NodeDesc)
		fprintf(stderr, " %s\n", enodep->NodeDesc);
	else
		fprintf(stderr, "\n");
	if (enodep->details)
		fprintf(stderr, "%*sNodeDetails: %s\n", indent+4, "", enodep->details);
}

// Verify nodes in fabric against specified topology
void ShowExpectedNodesReport(/*Point *focus,*/ uint8 NodeType, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 input_checked = 0;
	const char *NodeTypeText = StlNodeTypeToText(NodeType);
	QUICK_LIST *input_listp;

	switch (NodeType) {
	case STL_NODE_FI:
			input_listp = &g_Fabric.ExpectedFIs;
			break;
	case STL_NODE_SW:
			input_listp = &g_Fabric.ExpectedSWs;
			break;
	default:
			ASSERT(0);
			break;
	}

	// intro for report
	fprintf(stderr, "%*s%ss Topology Expected\n", indent, "", NodeTypeText);
	for (p=QListHead(input_listp); p != NULL; p = QListNext(input_listp, p)) {
		ExpectedNode *enodep = (ExpectedNode *)QListObj(p);

		if (! enodep)
			continue;
		input_checked++;
		ShowExpectedNodeBriefSummary(enodep, indent, detail);
	}
	if (detail && input_checked)
		fprintf(stderr, "\n");	// blank line between links
	fprintf(stderr, "%*s%u of %u Input %ss Shown\n", indent, "",
					input_checked, QListCount(input_listp), NodeTypeText);
	return;
}

// --------------------- NameData functions ----------------------------

// when using GUIDs as name, put whole value in body
void SetNameToGuid(NameData_t *namep, EUI64 guid)
{
	sprintf(namep->name, "0x%016"PRIx64, guid);
	namep->prefix = NULL;
	StringCopy(namep->body, namep->name, NODE_DESCRIPTION_ARRAY_SIZE);
	namep->start = 0;
	namep->end = 0;
	namep->leadzero = FALSE;
	namep->numlen = 0;
	namep->suffix = NULL;
}

// Name processing copy Modes:
// underscore - simple conversion of space to underscore
// trunc - take all chars after 1st space and remove
//				host405 hfi1_0 -> host405
//				can be useful for single and multi-rail hosts
//				can also be useful to produce a more concise slurm file
//				by treating large switches as a single black box entity
// flip - take all chars after 1st space and put at start of name
//				any additional spaces converted to underscore
//				opacore7 Leaf101 -> Leaf101_opacore7
//				can be useful so can sort and combine to get ranges
//				such as Leaf101_opacore[1-7]
// guid - use text representation of hex GUID
// TBD - future - could add multi-rail suffix addition or replace modes
//		- algortihmic suffix (hf1_0 -> -opa1, hfi1_1 -> -opa2)
typedef enum {
	NAME_MODE_UNDERSCORE = 1,
	NAME_MODE_FLIP = 2,
	NAME_MODE_TRUNC = 3,
	NAME_MODE_GUID = 4
} name_mode_t;


// translate and copy name into a NameData, separating out body and number
// and adding prefix and suffix
void CopyName(NameData_t *namep, const char *name, EUI64 guid, name_mode_t mode,
				const char *prefix, const char *suffix)
{
	int len = 0;
	int i;

	if (! name || mode == NAME_MODE_GUID) {
		DEBUG_ASSERT(guid);
		SetNameToGuid(namep, guid);
		return;
	}
	if (mode == NAME_MODE_UNDERSCORE) {
		// change spaces to underscore
		int l;
		char *d;
		StringCopy(namep->name, name, sizeof(namep->name));
		len = strnlen(namep->name, sizeof(namep->name));
		for (l=len, d=namep->name; l; d++, l--)
			if (isspace(*d))
				*d = '_';
	} else if (mode == NAME_MODE_TRUNC) {
		// truncate at 1st space
		int l;
		char *d;
		StringCopy(namep->name, name, sizeof(namep->name));
		len = strnlen(namep->name, sizeof(namep->name));
		for (l=len, d=namep->name; l; d++, l--) {
			if (isspace(*d)) {
				*d = '\0';
				break;
			}
		}
		len = (d - namep->name);
	} else if (mode == NAME_MODE_FLIP) {
		// put content after 1st space at start of string,
		// replacing space w/underscore
		int l;
		const char *s;
		char *d = namep->name;
		for (l=sizeof(namep->name), s=name; l && *s && ! isspace(*s); s++, l--)
			;
		if (l && *s) {
			// we found a space
			DEBUG_ASSERT(isspace(*s));
			s++; l--;
			// copy remaining characters to start of string
			for (; l && *s; s++, l--)
				if(!isspace(*s))
					*d++ = *s;
			*d++ = '_';
		}
		// now copy up to the 1st space into the remainder
		for (l=sizeof(namep->name) - (d-namep->name), s=name; l && *s && ! isspace(*s); s++, d++, l--)
			*d = *s;
		if (l)
			*d='\0';
		len = (d - namep->name);
	}

	namep->prefix = prefix;
	StringCopy(namep->body, namep->name, sizeof(namep->body));
	// assume no number
	namep->numlen = 0;
	namep->start = 0;
	namep->end = 0;
	if (len) {
		for (i=len; i> 0 && isdigit(namep->body[i-1]); i--)
			;
		if (i != len) {
			// we have a number, starting at body[i]
			if (FSUCCESS == StringToUint32(&namep->start, &namep->body[i], NULL, 10, FALSE)) {
				namep->numlen = len - i;
				namep->end = namep->start;
				namep->leadzero = (namep->numlen > 1 && namep->body[i] == '0');
				namep->body[i] = '\0';
			}
		}
	}
	namep->suffix = suffix;
	DBGPRINT("name=%s, prefix=%s body=%s, suffix=%s\n", namep->name,
			namep->prefix?namep->prefix:"", namep->body,
			namep->suffix?namep->suffix:"");
	DBGPRINT("numlen=%u, leadzero=%d, start=%u end=%u\n", namep->numlen,
			namep->leadzero, namep->start, namep->end);
}

void PrintName(const NameData_t *namep)
{
	if (namep->prefix)
		printf("%s", namep->prefix);
	printf("%.*s", NODE_DESCRIPTION_ARRAY_SIZE, namep->body);
	if (namep->numlen) {
		if (namep->leadzero)
			if (namep->start != namep->end)
				printf("[%0*u-%0*u]", namep->numlen, namep->start, namep->numlen, namep->end);
			else
				printf("%0*u", namep->numlen, namep->start);
		else
			if (namep->start != namep->end)
				printf("[%u-%u]", namep->start, namep->end);
			else
				printf("%u", namep->start);
	}
	if (namep->suffix)
		printf("%s", namep->suffix);
}

// is name2 numerically sequential and after name1
// if so we can consider combining them
boolean SequentialNames(const NameData_t *name1p, const NameData_t *name2p)
{
	// to optimize speed of compare, we do the faster checks 1st
	if (! name1p->numlen || ! name2p->numlen)
		return FALSE;	// only sequential if have numbers
	if (name1p->end+1 != name2p->start)
		return FALSE;	// not in sequence
	if (name1p->leadzero && name1p->numlen == name2p->numlen) {
		// ok, leading zeros in the lower numbered namep and constant length
	} else if (!name2p->leadzero && ! name2p->leadzero) {
		// ok, no leading zeros, all good
	} else {
		return FALSE;   // number representation inconsistent
	}
#if ! RANGE_IN_MIDDLE
	if (name1p->suffix || name2p->suffix)
		return FALSE;	// can't have range in middle of a name
#endif
	if (name1p->prefix) {
		if (!name2p->prefix || 0 != strncmp(name1p->prefix, name2p->prefix, NODE_DESCRIPTION_ARRAY_SIZE))
			return FALSE;	// different prefix
	} else if (name2p->prefix) {
		return FALSE;	// different prefix
	}
	if (0 != strncmp(name1p->body, name2p->body, NODE_DESCRIPTION_ARRAY_SIZE))
		return FALSE;   // different body

	if (name1p->suffix) {
		if (!name2p->suffix || 0 != strncmp(name1p->suffix, name2p->suffix, NODE_DESCRIPTION_ARRAY_SIZE))
			return FALSE;	// different suffix
	} else if (name2p->suffix) {
		return FALSE;	// different suffix
	}
	return TRUE;
}

// compare two names and return:
// -1 - name1 < name2
// 0 - name1 == name2
// 1 - name1 > name2
int CompareNameData(IN const uint64 name1, IN  const uint64 name2)
{
	const NameData_t *name1p = (const NameData_t*)name1;
	const NameData_t *name2p = (const NameData_t*)name2;
	int ret;

	DBGPRINT("CompareNameData: %s, %s\n", name1p->name, name2p->name);
	if (name1p->prefix) {
		if (!name2p->prefix) {
			return 1;
		} else {
			ret = strncmp(name1p->prefix, name2p->prefix, NODE_DESCRIPTION_ARRAY_SIZE);
			if (ret)
				return ret;
		}
	} else if (name2p->prefix) {
		return -1;
	}
	ret = strncmp(name1p->body, name2p->body, NODE_DESCRIPTION_ARRAY_SIZE);
	if (ret)
		return ret;
	if (! name1p->numlen || ! name2p->numlen) {
		// if only 1 has a number, put one without a number 1st
		if (name1p->numlen)
			return 1;
		else if (name2p->numlen)
			return -1;
	}
	// for RANGE_IN_MIDDLE we want items with same suffix together so we can
	// find best opportunities to combine, so sort by suffix prior to sorting
	// by numeric field
	// for ! RANGE_IN_MIDDLE, we want to sort by suffix after sort by numeric
	if (name1p->suffix) {
		if (!name2p->suffix) {
			ret = 1;
		} else {
			ret = strncmp(name1p->suffix, name2p->suffix, NODE_DESCRIPTION_ARRAY_SIZE);
			if (ret)
				return ret;
		}
	} else if (name2p->suffix) {
		ret = -1;
	}
#if RANGE_IN_MIDDLE
	if (ret)
		return ret;
#endif
	if (! name1p->numlen && ! name2p->numlen)
		return ret;
	// if we see two styles of format, we want to sort those with shorter
	// numeric fields first.  c0, c1, c000, c001 so we can find best
	// opportunities to combine sequential ranges
	if (name1p->leadzero || name2p->leadzero) {
		if (name1p->numlen < name2p->numlen)
			return -1;
		else if (name1p->numlen > name2p->numlen)
			return -1;
	}
	if (name1p->start < name2p->start)
		return -1;
	else if (name1p->start > name2p->start)
		return 1;
	else
		return ret;
}

// updates name1p to include name2p
// only valid if name1p and name2p are sequential
// before or after this update, name2p should be remove from list
void CombineNames(NameData_t *name1p, const NameData_t *name2p)
{
	DEBUG_ASSERT(SequentialNames(name1p, name2p));
	DBGPRINT("Combining: %s, %s\n", name1p->name, name2p->name);
	name1p->end = name2p->end;
}

// --------------------- NameData list functions ----------------------------

void InitNameList(cl_qmap_t *listp)
{
	cl_qmap_init(listp, CompareNameData);
}

// add name to list if not already on list
// the algorithm assumes duplicates are relatively infrequent so its simpler
// to just build the final NameData for comparison and throw it away on a
// duplicate. Alternatives would be more complex and have an extra copy
// of the name string for each non-dup
// rationale:
//	- hosts on a switch will have no dups
//	- ISLs will have dups limited to trunk size of link (1-4 typical)
FSTATUS AddNameList(cl_qmap_t *listp, const char* name, EUI64 guid, name_mode_t mode,
				const char *prefix, const char *suffix)
{
	NameData_t *namep;

	if (name)
		DBGPRINT("AddNameList: %s 0x%016"PRIx64"\n", name, guid);
	else
		DBGPRINT("AddNameList: NULL 0x%016"PRIx64"\n", guid);
	namep = (NameData_t *)MemoryAllocate2AndClear(sizeof(NameData_t), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! namep) {
		fprintf(stderr, "Out of memory\n");
		return FINSUFFICIENT_MEMORY;
	}
	CopyName(namep, name, guid, mode, prefix, suffix);
	if (cl_qmap_insert(listp, (uint64)(uintn)namep, &namep->NameListEntry) != &namep->NameListEntry) {
		// name already on list
		MemoryDeallocate(namep);
		return FSUCCESS;
	}
	DBGPRINT("AddNameList: created new entry\n");
	return FSUCCESS;
}

// take the sorted list and convert sequential names to a numeric range
// two styles of numeric ranges: name[0-10] and name[00-10] indicate if
// leading zeros were present in the original names
void CompactNameList(cl_qmap_t *listp)
{
	cl_map_item_t *p;

	DBGPRINT("compacting name list\n");
	for (p=cl_qmap_head(listp); p != cl_qmap_end(listp);) {
		NameData_t *name1p = PARENT_STRUCT(p, NameData_t, NameListEntry);
		NameData_t *name2p;
		cl_map_item_t *next= cl_qmap_next(p);
		if (next == cl_qmap_end(listp))
			break;
		DBGPRINT("compacting name list: p: %p, next: %p\n", p, next);
		name2p = PARENT_STRUCT(next, NameData_t, NameListEntry);
		if (SequentialNames(name1p, name2p)) {
			cl_qmap_remove_item(listp, next);
			CombineNames(name1p, name2p);
			MemoryDeallocate(name2p);
		} else {
			p = cl_qmap_next(p);
		}
	}
}

void FreeNameList(cl_qmap_t *listp)
{
	cl_map_item_t *p;

	for (p=cl_qmap_head(listp); p != cl_qmap_end(listp); p = cl_qmap_head(listp)) {
		NameData_t *namep = PARENT_STRUCT(p, NameData_t, NameListEntry);
		cl_qmap_remove_item(listp, p);
		MemoryDeallocate(namep);
	}
}

// --------------------- SwitchLists functions ----------------------------

static __inline cl_qmap_t *GetSwitchFiNeighNameList(ExpectedNode *enodep)
{
	return &((SwitchLists_t*)enodep->context)->fiNeighborNames;
}

static __inline cl_qmap_t *GetSwitchSwNeighNameList(ExpectedNode *enodep)
{
	return &((SwitchLists_t*)enodep->context)->swNeighborNames;
}

static __inline uint8 GetSwitchTier(ExpectedNode *enodep)
{
	return ((SwitchLists_t*)enodep->context)->tier;
}

static __inline DLIST *GetSwitchSwNeighList(ExpectedNode *enodep)
{
	return &((SwitchLists_t*)enodep->context)->swNeighbors;
}

FSTATUS AllocSwitchLists(ExpectedNode *enodep)
{
	enodep->context = MemoryAllocate2AndClear(sizeof(SwitchLists_t), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! enodep->context)
		return FINSUFFICIENT_MEMORY;
	InitNameList(GetSwitchFiNeighNameList(enodep));
	InitNameList(GetSwitchSwNeighNameList(enodep));
	ListInitState(&((SwitchLists_t*)enodep->context)->swNeighbors);
	if (! ListInit(&((SwitchLists_t*)enodep->context)->swNeighbors, MIN_LIST_ITEMS))
		goto fail;
	return FSUCCESS;

fail:
	MemoryDeallocate(enodep->context);
	return FINSUFFICIENT_MEMORY;
}

void FreeSwitchLists(ExpectedNode *enodep)
{
	if (enodep->context) {
		FreeNameList(GetSwitchFiNeighNameList(enodep));
		FreeNameList(GetSwitchSwNeighNameList(enodep));
		ListDestroy(&((SwitchLists_t*)enodep->context)->swNeighbors);
		MemoryDeallocate(enodep->context);
		enodep->context = NULL;
	}
}

FSTATUS AddSwitchFiNameList(ExpectedNode *enodep, const char* name, EUI64 guid, name_mode_t mode,
				const char *prefix, const char *suffix)
{
	if (! enodep->context) {
		FSTATUS status = AllocSwitchLists(enodep);
		if (FSUCCESS != status)
			return status;
	}
	return AddNameList(GetSwitchFiNeighNameList(enodep), name, guid, mode,
				prefix, suffix);
}

FSTATUS AddSwitchSwNameList(ExpectedNode *enodep, const char* name, EUI64 guid, name_mode_t mode,
				const char *prefix, const char *suffix)
{
	if (! enodep->context) {
		FSTATUS status = AllocSwitchLists(enodep);
		if (FSUCCESS != status)
			return status;
	}
	return AddNameList(GetSwitchSwNeighNameList(enodep), name, guid, mode,
				prefix, suffix);
}

FSTATUS SetSwitchTier(ExpectedNode *enodep, uint8 tier)
{
	if (! enodep->context) {
		FSTATUS status = AllocSwitchLists(enodep);
		if (FSUCCESS != status)
			return status;
	}
	((SwitchLists_t*)enodep->context)->tier = tier;
	return FSUCCESS;
}

FSTATUS AddSwitchSwNeighbor(ExpectedNode *enodep, ExpectedNode *swnodep)
{
	if (! enodep->context) {
		FSTATUS status = AllocSwitchLists(enodep);
		if (FSUCCESS != status)
			return status;
	}
	if (! ListInsertTail(&((SwitchLists_t*)enodep->context)->swNeighbors, swnodep))
		return FINSUFFICIENT_MEMORY;
	return FSUCCESS;
}

// analyze the expected links to determine the swNeighbors for each switch.
// TBD - Future - would be nice to be able to filter out selected hosts such as
// service nodes connected to core, but would need to figure out how to
// filter out edge switches left with no hosts so they aren't mistaken for
// core switches.  For now, SLURM uses with impure trees or other topologies
// should use the -o slurm option which will provide a brief report which does
// not list all the ISLs
void BuildSwitchNeighList(boolean get_isls)
{
	LIST_ITEM *q;
	LIST_ITEM *p;
	uint8 tier = 0;
	boolean found;

	if (! get_isls)
		return;

	// now populate swNeighbors for each switch and identify tier 1 switches
	for (q=QListHead(&g_Fabric.ExpectedLinks); q != NULL; q = QListNext(&g_Fabric.ExpectedLinks, q)) {
		ExpectedLink *elinkp = (ExpectedLink *)QListObj(q);

		if (! elinkp->portselp1 || ! elinkp->portselp1->enodep
			|| ! elinkp->portselp2 || ! elinkp->portselp2->enodep) {
			// Skipping Link, unresolved
			continue;
		}
		// figure out the types on both ends
		switch (elinkp->portselp1->enodep->NodeType) {
		case STL_NODE_FI:
			switch (elinkp->portselp2->enodep->NodeType) {
			case STL_NODE_FI: // FI<->FI
				// Skipping Link, unexpected FI<->FI link
				break;
			case STL_NODE_SW: // FI<->SW
				(void)SetSwitchTier(elinkp->portselp2->enodep, 1);
				break;
			default:	// should not happen, enodep should always have type
				// Skipping Link, unspecified NodeType
				break;
			}
			break;
		case STL_NODE_SW:
			switch (elinkp->portselp2->NodeType) {
			case STL_NODE_FI: // SW<->FI
				(void)SetSwitchTier(elinkp->portselp1->enodep, 1);
				break;
			case STL_NODE_SW: // SW<->SW
				(void)AddSwitchSwNeighbor(elinkp->portselp1->enodep, elinkp->portselp2->enodep);
				(void)AddSwitchSwNeighbor(elinkp->portselp2->enodep, elinkp->portselp1->enodep);
				break;
			default:	// should not happen, enodep should always have type
				// Skipping Link, unspecified NodeType
				break;
			}
			break;
		default:	// should not happen, enodep should always have type
			// Skipping Link, unspecified NodeType
			continue;
		}
	}

	// now we have tier 1 switches identified, need to walk up the tree to
	// identify higher tier switches (higher is closer to core and further from
	// FIs)
	// This algorithm is based on Topology/route.c:DetermineSwitchTiers except
	// this works with ExpectedNode while route.c works with NodeData
	//
	// switches connected to tier 1 switches are tier 2, etc
	
	// This algorithm works fine for pure trees, however for impure trees
	// it can yield unexpected results, for example a core switch with an HFI
	// will be considered a tier 1 switch.
	tier=2;
	do {
		found = FALSE;
		for (p=QListHead(&g_Fabric.ExpectedSWs); p != NULL; p = QListNext(&g_Fabric.ExpectedSWs, p)) {
			ExpectedNode *enodep = (ExpectedNode *)QListObj(p);
			LIST_ITERATOR i;
			if (GetSwitchTier(enodep) != tier-1)
				continue;
			for (i=ListHead(GetSwitchSwNeighList(enodep)); i != NULL;
						i = ListNext(GetSwitchSwNeighList(enodep),i)) {
				ExpectedNode* nswnodep = (ExpectedNode*)ListObj(i);
				if (! GetSwitchTier(nswnodep)) {
					SetSwitchTier(nswnodep, tier);
					found = TRUE;
				}
			}
		}
		tier++;
	} while (found);
	return;
}

// Analyze ExpectedLinks and build lists of neighbors in ExpectedSW's context
// pointer in preparation for output generation
// The algorithm here assumes:
//	- ExpectedSWs and ExpectedLinks are both supplied and consistent
//	- NodeDesc and/or NodeGUID is listed for each entry
//	- All entries in both have NodeType
// The above will all be true if the topology.xml was autogenerated by
// opaxlattopologory or opareport -o topology
void BuildAllSwitchLists(name_mode_t switch_name_mode, name_mode_t node_name_mode, boolean get_isls, boolean get_hfis)
{
	LIST_ITEM *q;
	uint32 input_checked = 0;
	uint32 bad_input = 0;
	uint32 fi_links = 0;
	uint32 isl_links = 0;
	uint32 bad_isl_links = 0;
	uint32 ix = 0;

	BuildSwitchNeighList(get_isls);

	PROGRESS_PRINT(TRUE, "Processing Links...");
	for (q=QListHead(&g_Fabric.ExpectedLinks); q != NULL; q = QListNext(&g_Fabric.ExpectedLinks, q)) {
		ExpectedLink *elinkp = (ExpectedLink *)QListObj(q);
		ExpectedNode *swenodep = NULL;
		ExpectedNode *nswenodep = NULL;
		ExpectedNode *fienodep = NULL;

		if ((ix++ % PROGRESS_FREQ) == 0) {
			PROGRESS_PRINT(FALSE, "Processed %6d of %6d Links...",
				ix, QListCount(&g_Fabric.ExpectedLinks));
		}

		if (! elinkp->portselp1 || ! elinkp->portselp1->enodep
			|| ! elinkp->portselp2 || ! elinkp->portselp2->enodep) {
			fprintf(stderr, "Skipping Link (topology file line %"PRIu64"), unresolved:\n", elinkp->lineno);
			ShowExpectedLinkSummary(elinkp, 4, 0);
			bad_input++;
			continue;
		}
		// figure out the types on both ends, want FI-SW and SW-SW links
		switch (elinkp->portselp1->enodep->NodeType) {
		case STL_NODE_FI:
			switch (elinkp->portselp2->enodep->NodeType) {
			case STL_NODE_FI:
				fprintf(stderr, "Skipping Link (topology file line %"PRIu64"), unexpected FI<->FI link:\n", elinkp->lineno);
				ShowExpectedLinkSummary(elinkp, 4, 0);
				bad_input++;
				continue;
			case STL_NODE_SW:
				fienodep = elinkp->portselp1->enodep;
				swenodep = elinkp->portselp2->enodep;
				break;
			default:
				fprintf(stderr, "Skipping Link (topology file line %"PRIu64"), unspecified NodeType:\n", elinkp->lineno);
				ShowExpectedLinkSummary(elinkp, 4, 0);
				bad_input++;
				continue;
			}
			break;
		case STL_NODE_SW:
			switch (elinkp->portselp2->enodep->NodeType) {
			case STL_NODE_FI:
				swenodep = elinkp->portselp1->enodep;
				fienodep = elinkp->portselp2->enodep;
				break;
			case STL_NODE_SW:
				swenodep = elinkp->portselp1->enodep;
				nswenodep = elinkp->portselp2->enodep;
				break;
			default:
				fprintf(stderr, "Skipping Link (topology file line %"PRIu64"), unspecified NodeType:\n", elinkp->lineno);
				ShowExpectedLinkSummary(elinkp, 4, 0);
				bad_input++;
				continue;
			}
			break;
		default:
			fprintf(stderr, "Skipping Link (topology file line %"PRIu64"), unspecified NodeType:\n", elinkp->lineno);
			ShowExpectedLinkSummary(elinkp, 4, 0);
			bad_input++;
			continue;
		}
		// now swenodep is a SW
		// if neighbor is a FI, fienodep is non-NULL
		// if neighbor is a SW, nswenodep is non-NULL
		DEBUG_ASSERT(swenodep);
		DEBUG_ASSERT(nswenodep || fienodep);

		if ( ! get_hfis && ! nswenodep)
				continue;	// skip FI-SW link
		if ( ! get_isls && ! fienodep)
				continue;	// skip SW-SW link
		if (! CompareExpectedLinkFocus(elinkp))
			continue;
		input_checked++;
		if (fienodep) {	// FI<->SW
			(void)AddSwitchFiNameList(swenodep, fienodep->NodeDesc, fienodep->NodeGUID, node_name_mode, g_prefix, g_suffix);
			fi_links++;
		} else {	// SW<->SW
			// only add downlink which goes from upper tier switch to lower tier
			// SLURM wants just one direction reported for each link, so
			// we only report downlinks in pure trees and warn if any links are
			// found which are not tier X to tier X-1 or tier X+1
			if (GetSwitchTier(swenodep) == GetSwitchTier(nswenodep)+1) {
				(void)AddSwitchSwNameList(swenodep, nswenodep->NodeDesc, nswenodep->NodeGUID, switch_name_mode, NULL, NULL);
				isl_links++;
			} else if (GetSwitchTier(nswenodep) == GetSwitchTier(swenodep)+1) {
				(void)AddSwitchSwNameList(nswenodep, swenodep->NodeDesc, swenodep->NodeGUID, switch_name_mode, NULL, NULL);
				isl_links++;
			} else {
				// This is a best attempt but will not catch all issues
				// for example a 3 tier fabric with an HFI on the core will
				// treat the core as a tier 1 which is connected to tier 2
				// switches and hence not reported as an issue
				fprintf(stderr, "Skipping Link (topology file line %"PRIu64"), not a pure tree:\n", elinkp->lineno);
				ShowExpectedLinkSummary(elinkp, 4, 0);
				bad_isl_links++;
			}
		}
	}
	PROGRESS_PRINT(TRUE, "Done Processing Links");
	fprintf(stderr, "%u of %u Input Links Selected, %u Bad Input Skipped\n",
				input_checked, QListCount(&g_Fabric.ExpectedLinks), bad_input);
	if (get_hfis)
		fprintf(stderr, "%u Input FI Links Shown\n",
					fi_links);
	if (get_isls)
		fprintf(stderr, "%u Input ISLs Shown, %u Bad ISLs Skipped\n",
					isl_links, bad_isl_links);
	return;
}

void FreeAllSwitchNamelists(void)
{
	LIST_ITEM *p;

	for (p=QListHead(&g_Fabric.ExpectedSWs); p != NULL; p = QListNext(&g_Fabric.ExpectedSWs, p)) {
		ExpectedNode *enodep = (ExpectedNode *)QListObj(p);

		FreeSwitchLists(enodep);
	}
}

// --------------------- Focus functions ----------------------------

// compare against all -F options provided
// if matches any one of the -F options, returns TRUE
boolean CompareExpectedNodeFocus(ExpectedNode *enodep)
{
	int i;

	if (! g_num_focus)
		return TRUE;	// match everything if no focus
	for (i=0; i < g_num_focus; i++)
		if (CompareExpectedNodePoint(enodep, &g_focus[i]))
			return TRUE;
	return FALSE;
}

// compare against all -F options provided
// if matches any one of the -F options, returns TRUE
boolean CompareExpectedLinkFocus(ExpectedLink *elinkp)
{
	int i;

	if (! g_num_focus)
		return TRUE;	// match everything if no focus
	for (i=0; i < g_num_focus; i++)
		if (CompareExpectedLinkPoint(elinkp, &g_focus[i]))
			return TRUE;
	return FALSE;
}

// --------------------- Output functions ----------------------------

// output a comment indicating when and how the output was generated
void PrintComment(const char* commentchar, int argc, char **argv)
{
	char datestr[80] = "";
	int i;
	time_t now;

	time(&now);
	Top_formattime(datestr, sizeof(datestr), now);
	printf("%s generated on %s\n", commentchar, datestr);
	printf("%s using:", commentchar);
	for (i=0; i<argc; i++)
		printf(" %s ", argv[i]);
	printf("\n");
}


// Output FastFabric hosts list
// The algorithm here assumes:
//	- ExpectedFIs is supplied
//	- NodeDesc is listed for each entry
//	- All entries have NodeType
//	- this host should be omitted as its the fastfabric host
// The above will all be true if the topology.xml was autogenerated by
// opaxlattopologory or opareport -o topology
void ShowFastFabricHosts(name_mode_t node_name_mode)
{
	LIST_ITEM *p;
	uint32 input_checked = 0;
	uint32 bad_input = 0;
	cl_qmap_t host_list;
	cl_map_item_t	*q;
	char myhostname[NODE_DESCRIPTION_ARRAY_SIZE];
	int i;

	ASSERT(node_name_mode != NAME_MODE_GUID);

	if (0 != gethostname(myhostname, sizeof(myhostname))) {
		// odd, just ignore filtering out our hostname
		myhostname[0] = '\0';
	} else {
		// if hostname is truncated, might lack a trailing NUL
		myhostname[NODE_DESCRIPTION_ARRAY_SIZE-1] = '\0';
	}
	// truncate any domain names
	for (i=0; i< sizeof(myhostname); i++) {
		if (myhostname[i] == '.') {
			myhostname[i] = '\0';
			break;
		}
	}
	DBGPRINT("myhostname=%.*s\n", NODE_DESCRIPTION_ARRAY_SIZE, myhostname);

	InitNameList(&host_list);
	for (p=QListHead(&g_Fabric.ExpectedFIs); p != NULL; p = QListNext(&g_Fabric.ExpectedFIs, p)) {
		ExpectedNode *enodep = (ExpectedNode *)QListObj(p);

		if (! enodep->NodeDesc) {
			fprintf(stderr, "Skipping FI (topology file line %"PRIu64"), no name\n", enodep->lineno);
			ShowExpectedNodeBriefSummary(enodep, 4, 0);
			bad_input++;
			continue;
		}
		// be paranoid, this is probably not possible since we are checking
		// ExpectedFIs list
		if (enodep->NodeType != STL_NODE_FI) {
			fprintf(stderr, "Skipping FI (topology file line %"PRIu64"), unspecified NodeType\n", enodep->lineno);
			ShowExpectedNodeBriefSummary(enodep, 4, 0);
			bad_input++;
			continue;	//unspecified type
		}

		if (! CompareExpectedNodeFocus(enodep))
			continue;
		input_checked++;
		// put hosts in a list, if there are multi-rail hosts
		// a given host will only be listed once
		if (FSUCCESS != AddNameList(&host_list, enodep->NodeDesc, 0, node_name_mode, g_prefix, g_suffix)) {
			// unexpected error
			continue;
		}
	}
	// CompactNameList(&host_list);	// FastFabric does not support ranges

	for (q=cl_qmap_head(&host_list); q != cl_qmap_end(&host_list); q = cl_qmap_next(q)) {
		NameData_t *namep = PARENT_STRUCT(q, NameData_t, NameListEntry);
		if (0 == strncmp(myhostname, namep->name, NODE_DESCRIPTION_ARRAY_SIZE))
			printf("#");	// comment out our host
		PrintName(namep);
		printf("\n");
	}
	FreeNameList(&host_list);
	fprintf(stderr, "%u of %u Input FIs Shown, %u Bad Input Skipped\n",
			input_checked, QListCount(&g_Fabric.ExpectedFIs), bad_input);

	return;
}

// Output SLURM topology file fake ISL single list of the form:
//     SwitchName=fake Switches=abc,def,ghi
// This output form allows for faster SLURM operation by assuming that once
// scheduling gets outside a single edge switch, its roughly equivalent latency
// This also supports impure tree and other topologies
void ShowSlurmFakeISLs(name_mode_t switch_name_mode)
{
	LIST_ITEM *p;
	ExpectedNode fake = {
		NodeType: STL_NODE_SW,
		NodeDesc: "fake",
		NodeGUID: 0x00066A0102FFFFFF
	};

	// build list of neighbors for "fake" core switch listing all switches
	// which have unfiltered FIs
	for (p=QListHead(&g_Fabric.ExpectedSWs); p != NULL; p = QListNext(&g_Fabric.ExpectedSWs, p)) {
		ExpectedNode *enodep = (ExpectedNode *)QListObj(p);
		cl_qmap_t *neigh_list;

		if (! enodep->context)	// switch has no neighbors
			continue;
		neigh_list = GetSwitchFiNeighNameList(enodep);
		if (cl_qmap_head(neigh_list) == cl_qmap_end(neigh_list))
			continue;	// switch has no FI neighbors
		(void)AddSwitchSwNameList(&fake, enodep->NodeDesc, enodep->NodeGUID, switch_name_mode, NULL, NULL);
	}

	if (! fake.context)
		return;	// there are no switches with FIs

	printf("# Fake Switch to Switch Connectivity\n");
	{
		ExpectedNode *enodep = &fake;
		int first=1;
		cl_qmap_t *neigh_list;
		cl_map_item_t *np;

		neigh_list = GetSwitchSwNeighNameList(enodep);
		// if using GUIDs for names, simply skip compact and will
		// get no ranges in output
		if (switch_name_mode != NAME_MODE_GUID)
			CompactNameList(neigh_list);
		for (np=cl_qmap_head(neigh_list); np != cl_qmap_end(neigh_list); np = cl_qmap_next(np)) {
			NameData_t *namep = PARENT_STRUCT(np, NameData_t, NameListEntry);
			if (first) {
				NameData_t nameData = { };
				printf("SwitchName=");
				CopyName(&nameData, enodep->NodeDesc, enodep->NodeGUID, switch_name_mode, NULL, NULL);
				PrintName(&nameData);
				printf(" Switches=");
				first=0;
			} else {
				printf(",");
			}
			PrintName(namep);
		}
		if (! first)
			printf("\n");
	}
	FreeSwitchLists(&fake);

	return;
}

// Output SLURM topology file ISL lists for each switch of the form:
//     SwitchName=xyz Switches=abc,def,ghi
void ShowSlurmISLs(name_mode_t switch_name_mode)
{
	LIST_ITEM *p;

	printf("# Switch to Switch Connectivity\n");
	for (p=QListHead(&g_Fabric.ExpectedSWs); p != NULL; p = QListNext(&g_Fabric.ExpectedSWs, p)) {
		ExpectedNode *enodep = (ExpectedNode *)QListObj(p);
		cl_qmap_t *neigh_list;
		int first=1;
		cl_map_item_t *np;

		if (! enodep->context)	// switch has no neighbors
			continue;
		neigh_list = GetSwitchSwNeighNameList(enodep);
		// if using GUIDs for names, simply skip compact and will
		// get no ranges in output
		if (switch_name_mode != NAME_MODE_GUID)
			CompactNameList(neigh_list);
		for (np=cl_qmap_head(neigh_list); np != cl_qmap_end(neigh_list); np = cl_qmap_next(np)) {
			NameData_t *namep = PARENT_STRUCT(np, NameData_t, NameListEntry);
			if (first) {
				NameData_t nameData = { };
				printf("SwitchName=");
				CopyName(&nameData, enodep->NodeDesc, enodep->NodeGUID, switch_name_mode, NULL, NULL);
				PrintName(&nameData);
				printf(" Switches=");
				first=0;
			} else {
				printf(",");
			}
			PrintName(namep);
		}
		if (! first)
			printf("\n");
	}

	return;
}

// Output SLURM topology file FI lists for each switch of the form:
//     SwitchName=xyz Nodes=abc,def,ghi
void ShowSlurmNodes(name_mode_t switch_name_mode, name_mode_t node_name_mode)
{
	LIST_ITEM *p;

	printf("# Switch to FI Connectivity\n");
	for (p=QListHead(&g_Fabric.ExpectedSWs); p != NULL; p = QListNext(&g_Fabric.ExpectedSWs, p)) {
		ExpectedNode *enodep = (ExpectedNode *)QListObj(p);
		cl_qmap_t *neigh_list;
		int first=1;
		cl_map_item_t *np;

		if (! enodep->context)	// switch has no FI neighbors
			continue;
		neigh_list = GetSwitchFiNeighNameList(enodep);
		// if using GUIDs for names, simply skip compact and will
		// get no ranges in output
		if (node_name_mode != NAME_MODE_GUID)
			CompactNameList(neigh_list);
		for (np=cl_qmap_head(neigh_list); np != cl_qmap_end(neigh_list); np = cl_qmap_next(np)) {
			NameData_t *namep = PARENT_STRUCT(np, NameData_t, NameListEntry);
			if (first) {
				NameData_t nameData = { };
				printf("SwitchName=");
				CopyName(&nameData, enodep->NodeDesc, enodep->NodeGUID, switch_name_mode, NULL, NULL);
				PrintName(&nameData);
				printf(" Nodes=");
				first=0;
			} else {
				printf(",");
			}
			PrintName(namep);
		}
		if (! first)
			printf("\n");
	}
	return;
}

// command line options, each has a short and long flag name
struct option options[] = {
		{ "verbose", no_argument, NULL, 'v' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "output", required_argument, NULL, 'o' },
		{ "guid", no_argument, NULL, 'g' },
		{ "underscore", no_argument, NULL, 'u' },
		{ "trunc", no_argument, NULL, 't' },
		{ "prefix", required_argument, NULL, 'p' },
		{ "suffix", required_argument, NULL, 's' },
		{ "strict", required_argument, NULL, 'S' },
		{ "check", required_argument, NULL, 'C' },
		{ "focus", no_argument, NULL, 'F' },
		{ "help", no_argument, NULL, '$' },	// use an invalid option character

		{ 0 }
};

void Usage_full(void)
{
	fprintf(stderr, "Usage: opa2rm [-v] [-q] -o output [-g|-u|-t] [-F point]\n"
					"                 [-p prefix] [-s suffix] topology_input\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opa2rm --help\n");
	fprintf(stderr, "    --help                    - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -q/--quiet                - disable progress reports\n");
	fprintf(stderr, "    -o/--output output        - output type\n");
	fprintf(stderr, "                                slurm - SLURM tree nodes\n");
	fprintf(stderr, "                                        Supports variety of topologies\n");
	fprintf(stderr, "                                slurmfull - SLURM fat tree nodes and ISLs\n");
	fprintf(stderr, "                                        Only supports pure trees\n");
	fprintf(stderr, "                                hosts - fastfabric hosts file\n");
	fprintf(stderr, "                                        omitting this host\n");
	fprintf(stderr, "    -g/--guid                 - output switch GUIDs instead of names\n");
	fprintf(stderr, "    -u/--underscore           - change spaces in switch names to underscores\n");
	fprintf(stderr, "    -t/--trunc                - merely truncate switch names at 1st space\n");
	fprintf(stderr, "                                This will treat large director switches as a\n");
	fprintf(stderr, "                                single big switch.\n");
	fprintf(stderr, "                                If none of -g, -u nor -t are specified\n");
	fprintf(stderr, "                                switch name suffixes after the first space\n");
	fprintf(stderr, "                                will be placed at the start of the name.\n");
	fprintf(stderr, "                                So 'core5 Leaf 101' becomes 'Leaf101_core5'\n");
	fprintf(stderr, "    -p/--prefix prefix        - prefix to prepend to all FI hostnames\n");
	fprintf(stderr, "    -s/--suffix suffix        - suffix to append to all FI hostnames\n");
	fprintf(stderr, "    -F/--focus point          - focus area for output\n");
	fprintf(stderr, "                                Limits scope of output to links\n");
	fprintf(stderr, "                                which match any of the given focus points.\n");
	fprintf(stderr, "                                May be specified up to %d times\n", MAX_FOCUS);
	fprintf(stderr, "    -C/--check                - perform more topology file validation.\n");
	fprintf(stderr, "                                Requires all links resolve against nodes,\n");
	fprintf(stderr, "                                all nodes connected to same fabric and\n");
	fprintf(stderr, "                                treats any resolution errors as fatal.\n");
	fprintf(stderr, "    -S/--strict               - perform strict topology file validation.\n");
	fprintf(stderr, "                                Performs all checks in -C, and\n");;
	fprintf(stderr, "                                requires all nodes list PortNum and\n");
	fprintf(stderr, "                                all nodes list ports used.\n");
	fprintf(stderr, "    topology_input            - topology_input file to use\n");
	fprintf(stderr, "                                '-' may be used to specify stdin\n");
// list only subset of formats applicable to ExpectedLink and useful here
// omit gid, portguid, nodeguid
	fprintf(stderr, "Point Syntax:\n");
	fprintf(stderr, "   node:value                 - value is node description (node name)\n");
	fprintf(stderr, "   node:value1:port:value2    - value1 is node description (node name)\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   nodepat:value              - value is glob pattern for node description (node\n");
	fprintf(stderr, "                                name)\n");
	fprintf(stderr, "   nodepat:value1:port:value2 - value1 is glob pattern for node description\n");
	fprintf(stderr, "                                (node name), value2 is port #\n");
	fprintf(stderr, "   nodetype:value             - value is node type (SW or FI)\n");
	fprintf(stderr, "   nodetype:value1:port:value2\n");
	fprintf(stderr, "                              - value1 is node type (SW or FI)\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   rate:value                 - value is string for rate (25g, 50g, 75g, 100g)\n");
	fprintf(stderr, "                                omits switch mgmt port 0\n");
	fprintf(stderr, "   mtucap:value               - value is MTU size (2048, 4096, 8192, 10240)\n");
	fprintf(stderr, "                                omits switch mgmt port 0\n");
	fprintf(stderr, "   labelpat:value             - value is glob pattern for cable label\n");
	fprintf(stderr, "   lengthpat:value            - value is glob pattern for cable length\n");
	fprintf(stderr, "   cabledetpat:value          - value is glob pattern for cable details\n");
	fprintf(stderr, "   linkdetpat:value           - value is glob pattern for link details\n");
	fprintf(stderr, "   portdetpat:value           - value is glob pattern for port details\n");
	fprintf(stderr, "                                to value\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "   opa2rm -o slurm topology.xml\n");
	fprintf(stderr, "   opa2rm -o slurm -p 'opa-' topology.xml\n");
	fprintf(stderr, "   opa2rm -o slurm -s '-opa' topology.xml\n");
	fprintf(stderr, "   opa2rm -o slurm -F 'nodepat:compute*' -F 'nodepat:opacore1 *' topology.xml\n");
	fprintf(stderr, "   opa2rm -o hosts -F 'nodedetpat:compute*' topology.xml\n");
	fprintf(stderr, "   opa2rm -o hosts topology.xml\n");
	exit(0);
}

void Usage(void)
{
	fprintf(stderr, "Usage: opa2rm [-v] [-q] -o output [-g|-u|-t] topology_input\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opa2rm --help\n");
	fprintf(stderr, "    --help                    - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -q/--quiet                - disable progress reports\n");
	fprintf(stderr, "    -o/--output output        - output type\n");
	fprintf(stderr, "                                slurm - SLURM tree nodes\n");
	fprintf(stderr, "                                        Supports variety of topologies\n");
	fprintf(stderr, "                                slurmfull - SLURM fat tree nodes and ISLs\n");
	fprintf(stderr, "                                        Only supports pure trees\n");
	fprintf(stderr, "                                hosts - fastfabric hosts file\n");
	fprintf(stderr, "                                        omitting this host\n");
	fprintf(stderr, "    -g/--guid                 - output switch GUIDs instead of names\n");
	fprintf(stderr, "    -u/--underscore           - instead of flipping any switch name suffix to\n");
	fprintf(stderr, "                                start, merely change spaces to underscore\n");
	fprintf(stderr, "    -t/--trunc                - instead of flipping any switch name suffix to\n");
	fprintf(stderr, "                                start, merely truncate at 1st space\n");
	fprintf(stderr, "    topology_input            - topology_input file to use\n");
	fprintf(stderr, "                                '-' may be used to specify stdin\n");
	exit(2);
}

typedef enum {	// bitmask indicating selected reports
	REPORT_NONE			=0x0,
	REPORT_SLURM		=0x1,
	REPORT_SLURMFULL	=0x2,
	REPORT_HOSTS		=0x4,
} report_t;


// convert a output type argument to the proper constant
report_t checkOutputType(const char* name)
{
	if (0 == strcmp(optarg, "slurm")) {
		return REPORT_SLURM;
	} else if (0 == strcmp(optarg, "slurmfull")) {
		return (REPORT_SLURMFULL);
	} else if (0 == strcmp(optarg, "hosts")) {
		return (REPORT_HOSTS);
	} else {
		fprintf(stderr, "opa2rm: Invalid Output Type: %s\n", name);
		Usage();
		// NOTREACHED
		return REPORT_NONE;
	}
}

int main(int argc, char ** argv)
{
	int                 c;
	report_t			report = REPORT_NONE;
	int					i;
	name_mode_t			switch_name_mode = NAME_MODE_FLIP;
	name_mode_t			node_name_mode = NAME_MODE_TRUNC;
	char*				topology_in_file	= NULL;	// input file being parsed
	int					exitstatus	= 0;
	TopoVal_t			validation = TOPOVAL_NONE;

	Top_setcmdname("opa2rm");
	g_quiet = ! isatty(2);  // disable progress if stderr is not tty

	// process command line arguments
	while (-1 != (c = getopt_long(argc,argv, "vqo:gutp:s:F:SC",
									options, &i)))
	{
		switch (c)
		{
			case '$':
				Usage_full();
				break;
			case 'v':
				g_verbose++;
				break;
			case 'q':
				g_quiet = 1;
				break;
			case 'o':
				report = (report_t) report | checkOutputType(optarg);
				break;
			case 'g':
				switch_name_mode = NAME_MODE_GUID;
				break;
			case 'u':
				switch_name_mode = NAME_MODE_UNDERSCORE;
				break;
			case 't':
				switch_name_mode = NAME_MODE_TRUNC;
				break;
			case 'p':
				g_prefix = optarg;
				break;
			case 's':
				g_suffix = optarg;
				break;
			case 'F':   // focus for report
				if (g_num_focus >= MAX_FOCUS) {
					fprintf(stderr, "opa2rm: -F option may only be specified %d times\n", MAX_FOCUS);
					Usage();
				}
				PointInit(&g_focus[g_num_focus]);
				g_focus_arg[g_num_focus++] = optarg;
				break;
			case 'S':
				validation = TOPOVAL_STRICT;
				break;
			case 'C':
				validation = TOPOVAL_SOMEWHAT_STRICT;
				break;
			default:
				fprintf(stderr, "opa2rm: Invalid option -%c\n", c);
				Usage();
				break;
		}
	} /* end while */
	// Name algorithms just check for NULL, so convert empty strings to NULL
	if (g_suffix && 0 == strlen(g_suffix))
		g_suffix = NULL;
	if (g_prefix && 0 == strlen(g_prefix))
		g_prefix = NULL;
	// slurm report generation requires cross referenced elinkp and enodep
	if (validation < TOPOVAL_LOOSE
		&& (report & (REPORT_SLURM|REPORT_SLURMFULL))) 
		validation = TOPOVAL_LOOSE;

	if (optind < argc) {
		topology_in_file = argv[optind++];
		if (!topology_in_file){
			fprintf(stderr, "opa2rm: Error: null input filename\n");
			exitstatus = 1;
			goto done;
		}
	} else {
		fprintf(stderr, "opa2rm: Missing topology_input argument\n");
		Usage();
	}
	if (optind < argc)
	{
		fprintf(stderr, "opa2rm: Unexpected extra arguments\n");
		Usage();
	}
	if (report == REPORT_NONE)
	{
		fprintf(stderr, "opa2rm: '-o output' option must be specified\n");
		Usage();
	}

	if (FSUCCESS != InitFabricData(&g_Fabric, FF_NONE)) {
		fprintf(stderr, "opa2rm: Unable to initialize fabric storage area\n");
		exitstatus = 1;
		goto done;
	}

	// parse topology input file
	if (FSUCCESS != Xml2ParseTopology(topology_in_file, g_quiet, &g_Fabric, 
						validation)) {
		exitstatus = 1;
		goto done;
	}
	if (g_verbose > 3)
		Xml2PrintTopology(stderr, &g_Fabric);	// for debug
	if (! (g_Fabric.flags & FF_EXPECTED_NODES)) {
		printf("opa2rm: Incomplete topology file: no Nodes information provided\n");
		exitstatus = 1;
		goto done;
	}
	if (! (g_Fabric.flags & (FF_EXPECTED_LINKS|FF_EXPECTED_EXTLINKS))) {
		printf("opa2rm: Incomplete topology file: no LinkSummary nor ExternalLinkSummary information provided\n");
		exitstatus = 1;
		goto done;
	}

	for (i=0; i < g_num_focus; i++) {
		char *p;
		FSTATUS status;
		uint8 find_flag =
				((report & (REPORT_SLURM|REPORT_SLURMFULL))?FIND_FLAG_ELINK:0)
				| ((report & (REPORT_HOSTS))?FIND_FLAG_ENODE:0);
		status = ParseFocusPoint(0, &g_Fabric, g_focus_arg[i], &g_focus[i],
									find_flag, &p, TRUE);
		if (FINVALID_PARAMETER == status || *p != '\0') {
			fprintf(stderr, "opa2rm: Invalid Point Syntax: '%s'\n", g_focus_arg[i]);
			fprintf(stderr, "opa2rm:                        %*s^\n", (int)(p-g_focus_arg[i]), "");
			Usage_full();
		}
		if (FSUCCESS != status) {
			fprintf(stderr, "opa2rm: Unable to resolve Point: '%s': %s\n", g_focus_arg[i], iba_fstatus_msg(status));
			exitstatus = 1;
			goto done;
		}
	}

	if (g_verbose > 1 ) {
		ShowExpectedLinksReport(0, g_verbose);
		ShowExpectedNodesReport(STL_NODE_FI, 0, g_verbose);
		ShowExpectedNodesReport(STL_NODE_SW, 0, g_verbose);
	}

	if (report & REPORT_HOSTS) {
		PrintComment("#", argc, argv);
		ShowFastFabricHosts(node_name_mode);
	}

	if (report & (REPORT_SLURM|REPORT_SLURMFULL)) {
		BuildAllSwitchLists(switch_name_mode, node_name_mode,
							 0 != (report&REPORT_SLURMFULL), 1);
		PrintComment("#", argc, argv);
		ShowSlurmNodes(switch_name_mode, node_name_mode);
		if (report & REPORT_SLURM) {
			printf("\n");
			ShowSlurmFakeISLs(switch_name_mode);
		}
		if (report & REPORT_SLURMFULL) {
			printf("\n");
			ShowSlurmISLs(switch_name_mode);
		}
		FreeAllSwitchNamelists();
	}

	DestroyFabricData(&g_Fabric);
done:

	for (i=0; i < g_num_focus; i++)
		PointDestroy(&g_focus[i]);

	if (exitstatus == 2)
		Usage();

	return exitstatus;
}
