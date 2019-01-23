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
#include <string.h>
#include <getopt.h>

#include <iba/ibt.h>
#include <iba/ipublic.h>
#include <topology.h>
#include <opamgt_priv.h>

#define _GNU_SOURCE

#define EXIT_STATUS_OK 0
#define EXIT_STATUS_BADOPTIONS 1
#define EXIT_STATUS_FAILED_PARSE 2
#define EXIT_STATUS_FAILED_OPEN_PORT 3
#define EXIT_STATUS_NO_LOCAL_NODE 4
#define EXIT_STATUS_FAILED_IDENTITY_VERIFICATION 5
#define EXIT_STATUS_FAILED_SMP_DISTRIBUTION 6
#define EXIT_STATUS_FAILED_ACTIVATION 7
#define EXIT_STATUS_BAD_FILENAME 8

#define PROGRAM_ATTR_PORTINFO    (0x00000001)
#define PROGRAM_ATTR_VLARB       (0x00000002)
#define PROGRAM_ATTR_PKEY        (0x00000004)
#define PROGRAM_ATTR_LFT 	     (0x00000008)
#define PROGRAM_ATTR_MFT         (0x00000020)
#define PROGRAM_ATTR_SLSC        (0x00000040)
#define PROGRAM_ATTR_SCSC        (0x00000080)
#define PROGRAM_ATTR_SWITCHINFO  (0x00000100)
#define PROGRAM_ATTR_SCSL 		 (0x00000200)
#define PROGRAM_ATTR_BFRCTRL	 (0x00000400)
#define PROGRAM_ATTR_SCVLR	 	 (0x00002000)
#define PROGRAM_ATTR_SCVLT	 	 (0x00004000)
#define PROGRAM_ATTR_SCVLNT	 	 (0x00008000)
#define PROGRAM_ATTR_ALL         (0x80000000)

#define DR_PATH_SIZE 64

// command line options, each has a short and long flag name
struct option options[] = {
		// basic controls
		{ "help", no_argument, NULL, '$' },	// use an invalid option character
		{ "verbose", no_argument, NULL, 'v' },
		{ "hfi", required_argument, NULL, 'h' },
		{ "port", required_argument, NULL, 'p' },
		{ "prog", no_argument, NULL, 'P' },
		{ "act", no_argument, NULL, 'A' },
		{ "attr", required_argument, NULL, 'a' },
		{ "attempts", required_argument, NULL, 'c' },
		{ "directed", no_argument, NULL, 'd' },
		{ "strict", no_argument, NULL, 's' },
		{ "bail", no_argument, NULL, 'b' },
		{ "parsable", no_argument, NULL, 'g' },
		{ 0 }
};

static __inline__ void logFailedSMP(NodeData* node, char* attribute, uint8_t parsableOutput) {
	if(!node || !attribute) 
		return;

	if(parsableOutput)
		fprintf(stderr, "error;attribute;%s;%s;0x%016"PRIx64"\n", attribute, node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
	else
		fprintf(stderr, "ERROR: Failed %s to %s (NodeGUID: 0x%016"PRIx64")\n", attribute, node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
}

void Output_Usage(void)
{
	fprintf(stderr, "Usage: opasnapconfig [OPTION] ... SNAPSHOT\n");
	fprintf(stderr, "Parse information from provided snapshot file and issue packets to program\n");
	fprintf(stderr, "and activate a fabric.\n");
	fprintf(stderr, "    --help                     - produce full help text\n");
	fprintf(stderr, "    -v/--verbose               - verbose output\n");
	fprintf(stderr, "    -h/--hfi hfi               - hfi, numbered 1..n, 0= -p port will be a\n");
	fprintf(stderr, "                                 system wide port num (default is 0)\n");
	fprintf(stderr, "    -p/--port port             - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "                                 is 1st active)\n");
	fprintf(stderr, "    -P/--prog                  - perform only attribute programming, skip\n");
	fprintf(stderr, "                                 activation phase\n");
	fprintf(stderr, "    -A/--act                   - perform only activation phase, skip\n");
	fprintf(stderr, "                                 attribute programming\n");
	fprintf(stderr, "    -a/--attr attr             - enables selected attribute programming.\n");
	fprintf(stderr, "                                 can be specified multiple times for\n");
	fprintf(stderr, "                                 multiple attributes (default is all)\n");
	fprintf(stderr, "    -c/--attempts attempts     - number of times to attempt sending a packet\n");
	fprintf(stderr, "                                 before moving on (default is 2)\n");
	fprintf(stderr, "    -d/--directed              - perform all attribute programming using only\n");
	fprintf(stderr, "                                 directed route routing. recommended for use\n");
	fprintf(stderr, "                                 with the -P option\n");
	fprintf(stderr, "    -s/--strict                - when checking node identity validate only\n");
	fprintf(stderr, "                                 NodeGUID (default: match NodeGUID first,\n");
	fprintf(stderr, "                                 match NodeDesc second)\n");
	fprintf(stderr, "    -b/--bail                  - exit at the first error encountered\n");
	fprintf(stderr, "    -g/--parsable              - machine parsable output of failures during\n");
	fprintf(stderr, "                                 fabric configuration\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Attribute Types:\n");
	fprintf(stderr, "    portinfo   - program each port's PortInfo\n");
	fprintf(stderr, "    vlarb      - program each port's VL Arbitration Table\n");
	fprintf(stderr, "    pkey       - program each port's PKey Table\n");
	fprintf(stderr, "    lft        - program each node's Linear Forwarding Table\n");
	fprintf(stderr, "    mft        - program each node's Multicast Forwarding Table\n");
	fprintf(stderr, "    slsc       - program each node's SLtoSC Mapping Table\n");
	fprintf(stderr, "    scsl       - program each node's SCtoSL Mapping Table\n");
	fprintf(stderr, "    scsc       - program each node's SCtoSC Mapping Table\n");
	fprintf(stderr, "    switchinfo - program each switch's SwitchInfo\n");
	fprintf(stderr, "    bfrctrl    - program each port's Buffer Control Table\n");
	fprintf(stderr, "    scvlr      - program each port's SCtoVLr Mapping Table\n");
	fprintf(stderr, "    scvlt      - program each port's SCtoVLt Mapping Table\n");
	fprintf(stderr, "    scvlnt     - program each port's SCtoVLnt Mapping Table\n");
	fprintf(stderr, "    all        - program all attribute information (default)\n");
}

void Usage_full(void)
{
	Output_Usage();
	exit(EXIT_STATUS_OK);
}

void Usage(void)
{
	Output_Usage();
	exit(EXIT_STATUS_BADOPTIONS);
}

uint32_t attributeToFlag(const char *attribute) {
	if(0 == strcasecmp(attribute, "portinfo"))
		return PROGRAM_ATTR_PORTINFO;
	else if(0 == strcasecmp(attribute, "vlarb"))
		return PROGRAM_ATTR_VLARB;
	else if(0 == strcasecmp(attribute, "pkey"))
		return PROGRAM_ATTR_PKEY;
	else if(0 == strcasecmp(attribute, "lft"))
		return PROGRAM_ATTR_LFT;
	else if(0 == strcasecmp(attribute, "mft"))
		return PROGRAM_ATTR_MFT;
	else if(0 == strcasecmp(attribute, "slsc"))
		return PROGRAM_ATTR_SLSC;
	else if(0 == strcasecmp(attribute, "scsc"))
		return PROGRAM_ATTR_SCSC;
	else if(0 == strcasecmp(attribute, "switchinfo"))
		return PROGRAM_ATTR_SWITCHINFO;
	else if(0 == strcasecmp(attribute, "scsl"))
		return PROGRAM_ATTR_SCSL;
	else if(0 == strcasecmp(attribute, "bfrctrl"))
		return PROGRAM_ATTR_BFRCTRL;
	else if(0 == strcasecmp(attribute, "scvlr"))
		return PROGRAM_ATTR_SCVLR;
	else if(0 == strcasecmp(attribute, "scvlt"))
		return PROGRAM_ATTR_SCVLT;
	else if(0 == strcasecmp(attribute, "scvlnt"))
		return PROGRAM_ATTR_SCVLNT;
	else if(0 == strcasecmp(attribute, "all"))
		return PROGRAM_ATTR_ALL;
	else {
		fprintf(stderr, "opasnapconfig: Invalid attribute type: %s\n", attribute);
		Usage();
	}

	return PROGRAM_ATTR_ALL;
}

static __inline__ void attributeFlagsToString(uint32_t attributes, char* buf, uint32_t buflen) {
	int remainingLength = buflen - 1;
	char* attrString;

	if(attributes & PROGRAM_ATTR_ALL) {
		snprintf(buf, remainingLength, "All");
		return;
	}

	if((attributes & PROGRAM_ATTR_PORTINFO) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_PORTINFO;
		attrString = (attributes) ? "portinfo, " : "portinfo";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_VLARB) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_VLARB;
		attrString = (attributes) ? "vlarb, " : "vlarb";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_PKEY) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_PKEY;
		attrString = (attributes) ? "pkey, " : "pkey";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_LFT) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_LFT;
		attrString = (attributes) ? "lft, " : "lft";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_MFT) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_MFT;
		attrString = (attributes) ? "mft, " : "mft";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_SLSC) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_SLSC;
		attrString = (attributes) ? "slsc, " : "slsc";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_SCSL) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_SCSL;
		attrString = (attributes) ? "scsl, " : "scsl";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_SCSC) && remainingLength) {
		//attributes &= ~PROGRAM_ATTR_SCSC;
		//attrString = (attributes) ? "scsc, " : "scsc";
		//
		//strncat(buf, attrString, remainingLength);
		//remainingLength -= strlen(attrString);
		fprintf(stdout, "SCSC Mapping Table currently unsupported\n");
	}
	if((attributes & PROGRAM_ATTR_SWITCHINFO) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_SWITCHINFO;
		attrString = (attributes) ? "switchinfo, " : "switchinfo";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_BFRCTRL) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_BFRCTRL;
		attrString = (attributes) ? "bfrctrl, " : "bfrctrl";
		
		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_SCVLR) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_SCVLR;
		attrString = (attributes) ? "scvlr, " : "scvlr";

		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_SCVLT) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_SCVLT;
		attrString = (attributes) ? "scvlt, " : "scvlt";

		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
	if((attributes & PROGRAM_ATTR_SCVLNT) && remainingLength) {
		attributes &= ~PROGRAM_ATTR_SCVLNT;
		attrString = (attributes) ? "scvlnt, " : "scvlnt";

		strncat(buf, attrString, remainingLength);
		remainingLength -= strlen(attrString);
	}
}

NodeData* locateLocalNode(struct omgt_port* oibPort, FabricData_t* fabric, uint8_t strict, uint8_t parsableOutput)
{
	STL_NODE_INFO localNodeInfo;
	STL_NODE_DESCRIPTION localNodeDesc;
	uint8_t path[DR_PATH_SIZE];
	Point results;
	NodeData* localNode;

	memset(&localNodeInfo, 0, sizeof(STL_NODE_INFO));
	memset(&localNodeDesc, 0, sizeof(STL_NODE_DESCRIPTION));
	memset(path, 0, sizeof(path));
	PointInit(&results);

	if(SmaGetNodeInfo(oibPort, 0, 0, path, &localNodeInfo) != FSUCCESS) {
		if(parsableOutput)
			fprintf(stderr, "error;attribute;Get(NodeInfo);Local Node;none;any\n");
		else
			fprintf(stderr, "ERROR: Unable to get local NodeInfo\n");
		return NULL;
	}

	if(SmaGetNodeDesc(oibPort, 0, 0, path, &localNodeDesc) != FSUCCESS) {
		if(parsableOutput)
			fprintf(stderr, "error;attribute;Get(NodeDesc);Local Node;none;any\n");
		else
			fprintf(stderr, "ERROR: Unable to get local NodeDesc\n");
		return NULL;
	}

	if((localNode = FindNodeGuid(fabric, localNodeInfo.NodeGUID)) != NULL) {
		// Mark the local node as visited so we don't revisit it
		localNode->visited = 1;
		localNode->valid = 1;
		return localNode;
	}

	if (strict || FindNodeNamePoint(fabric, (char*)localNodeDesc.NodeString, &results, FIND_FLAG_FABRIC, 1) != FSUCCESS)
		return NULL;

	if (results.Type == POINT_TYPE_NODE_LIST) {
		if(parsableOutput)
			fprintf(stderr, "error;snapshot;locateLocalNode;Local node listed multiple times\n");
		else
			fprintf(stderr, "ERROR: Found local node listed multiple times in snapshot\n");
		return NULL;
	}

	if (results.Type != POINT_TYPE_NODE)
		return NULL;

	localNode = results.u.nodep;
	// Mark the local node as visited so we don't revisit it
	localNode->visited = 1;
	localNode->valid = 1;
	return localNode;
}

FSTATUS verifyNodeIdentityRecurse(struct omgt_port* oibPort, NodeData* nodeToVerify, uint8_t* path, uint8_t strict, uint8_t bail, uint8_t parsableOutput) {
	cl_map_item_t *port;
	PortData* portData;
	STL_NODE_INFO nodeInfo;
	STL_NODE_DESCRIPTION nodeDesc;
	uint8_t gotNodeInfo = 0;
	uint8_t gotNodeDesc = 0;

	if(!oibPort || !nodeToVerify || !path)
		return FINVALID_PARAMETER;
	
	if(nodeToVerify->visited)
		return FSUCCESS;

	memset(&nodeInfo, 0, sizeof(STL_NODE_INFO));
	memset(&nodeDesc, 0, sizeof(STL_NODE_DESCRIPTION));

	if(SmaGetNodeInfo(oibPort, 0, 0, path, &nodeInfo) != FSUCCESS) {
		logFailedSMP(nodeToVerify, "Get(NodeInfo)", parsableOutput);
		if(bail)
			return FERROR;
	} else {
		gotNodeInfo = 1;
	}

	if(SmaGetNodeDesc(oibPort, 0, 0, path, &nodeDesc) != FSUCCESS) {
		logFailedSMP(nodeToVerify, "Get(NodeDesc)", parsableOutput);
		if(bail)
			return FERROR;
	} else {
		gotNodeDesc = 1;
	}

	nodeToVerify->visited = 1;

	if(gotNodeInfo && nodeInfo.NodeGUID == nodeToVerify->NodeInfo.NodeGUID) {
		nodeToVerify->valid = 1;
	}

	if(!strict && !nodeToVerify->valid && gotNodeDesc && 
		strncmp((char*)nodeDesc.NodeString, (char*)nodeToVerify->NodeDesc.NodeString, STL_NODE_DESCRIPTION_ARRAY_SIZE) == 0) {
		nodeToVerify->valid = 1;
	}

	if(nodeToVerify->valid) {
		memcpy(nodeToVerify->path, path, sizeof(nodeToVerify->path));
	} else {
		if(parsableOutput)
			fprintf(stderr, "error;incorrectnode;%s0x%016"PRIx64";%s;0x%016"PRIx64"\n", nodeToVerify->NodeDesc.NodeString, nodeToVerify->NodeInfo.NodeGUID,
																				gotNodeDesc ? (char*)nodeDesc.NodeString : "none", gotNodeInfo ? nodeInfo.NodeGUID : 0);
		else {
			fprintf(stderr, "ERROR: Skipping programming of incorrect node\n");
			fprintf(stderr, "    Expected: %s (NodeGUID: 0x%016"PRIx64")\n", nodeToVerify->NodeDesc.NodeString, nodeToVerify->NodeInfo.NodeGUID);
			fprintf(stderr, "    Found: %s (NodeGUID: 0x%016"PRIx64")\n", gotNodeDesc ? (char*)nodeDesc.NodeString : "none", gotNodeInfo ? nodeInfo.NodeGUID : 0);
		}

		if(bail)
			return FERROR;
	}

	for (port = cl_qmap_head(&nodeToVerify->Ports); port != cl_qmap_end(&nodeToVerify->Ports); port = cl_qmap_next(port)) {
		portData = PARENT_STRUCT(port, PortData, NodePortsEntry);

		if(portData->neighbor != NULL && portData->neighbor->nodep != NULL && portData->PortNum != 0 && path[0] < DR_PATH_SIZE-2) {
			FSTATUS status;
			uint8_t newPath[DR_PATH_SIZE];
			memcpy(newPath, path, sizeof(newPath));
			// Update the path port number and hop count
			newPath[0]++;
			newPath[newPath[0]] = portData->PortNum;

			status = verifyNodeIdentityRecurse(oibPort, portData->neighbor->nodep, newPath, strict, bail, parsableOutput);
			if(status != FSUCCESS && bail){
				return FERROR;
			}
		}
	}
	
	return FSUCCESS;
}

FSTATUS checkAllNodesVerified(FabricData_t* fabric, uint8_t parsableOutput) {
	FSTATUS status = FSUCCESS;
	LIST_ITEM* listNode;
	NodeData* node;
//	It is enough to verify all switches are visited since the logic in verifyNodeIdentityRecurse will ensure all FIs are always visited
//	as long as the neighbor switch is visited.
	for(listNode = QListHead(&fabric->AllSWs); listNode != NULL; listNode = QListNext(&fabric->AllSWs, listNode)) {
		node = (NodeData*)QListObj(listNode);

		if(!node || !node->valid)
			continue;

		if(!node->visited) {
			if(parsableOutput)
				fprintf(stderr, "error;routing;%s;0x%016"PRIx64";node not verified \n", node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
			else
				fprintf(stderr, "ERROR:Node %s (NodeGUID: 0x%016"PRIx64") not verified \n", node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
			status = FINVALID_SETTING;
		}
	}
	return status;
}

static FSTATUS DistributePortInfo(struct omgt_port* oibPort, NodeData *node, uint8_t directedOnly, uint8_t bail, uint8_t parsableOutput)
{
	if(!node || !node->valid)
		return FSUCCESS;

	cl_map_item_t *clPort;
	for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
		PortData *port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

		if(!port)
			continue;

		port->PortInfo.PortStates.s.PortState = IB_PORT_NOP;
		port->PortInfo.PortStates.s.PortPhysicalState = IB_PORT_PHYS_NOP;
		if (node->pSwitchInfo) {
			// If we're programming a switch, make sure Flit Preemption values are set.
			if (port->PortInfo.FlitControl.Preemption.MinInitial == 0)
				port->PortInfo.FlitControl.Preemption.MinInitial = 8;
			if (port->PortInfo.FlitControl.Preemption.MinTail == 0)
				port->PortInfo.FlitControl.Preemption.MinTail = 8;
		}

		if(SmaSetPortInfo(oibPort, 0, 0, node->path, port->PortNum, &port->PortInfo) != FSUCCESS) {
			logFailedSMP(node, "Set(PortInfo)", parsableOutput);

			if(bail)
				return FERROR;
		}
	}

	return FSUCCESS;
}

static FSTATUS updateFabricPortInfo(struct omgt_port* oibPort, FabricData_t* fabric, uint8_t directedOnly, uint8_t bail, uint8_t parsableOutput)
{
	cl_map_item_t *clNode, *clPort;
	NodeData* node;
	PortData* port;
	STL_PORT_INFO portInfo;
	FSTATUS fstatus;

	for (clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
		node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

		if (!node || !node->valid)
			continue;

		for (clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
			port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

			if (!port)
				continue;

			memset(&portInfo, 0, sizeof(portInfo));

			fstatus = SmaGetPortInfo(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL, port->PortNum, 1, &portInfo);

			if (fstatus == FSUCCESS) {
				memcpy(&port->PortInfo, &portInfo, sizeof(portInfo));
			} else {
				logFailedSMP(node, "Get(PortInfo)", parsableOutput);

				if (bail)
					return FERROR;
			}
		}
	}

	return FSUCCESS;
}

FSTATUS distributeSMPs(struct omgt_port* oibPort, FabricData_t* fabric, uint32_t attributesToProgram, uint8_t directedOnly, uint8_t bail, uint8_t parsableOutput)
{
	FSTATUS status = FSUCCESS;
	LIST_ITEM* listNode;
	cl_map_item_t *clNode, *clPort;
	NodeData* node;
	PortData* port;
	SC2VLUpdateType sc2vlUpdateType;
	int block;

	if(!oibPort || !fabric)
		return FINVALID_PARAMETER;


	// Distribute SwitchInfo
	if(attributesToProgram & (PROGRAM_ATTR_SWITCHINFO | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending SwitchInfo(s)...\n");

		for(listNode = QListHead(&fabric->AllSWs); listNode != NULL; listNode = QListNext(&fabric->AllSWs, listNode)) {
			node = (NodeData*)QListObj(listNode);

			if(!node || !node->valid || !node->pSwitchInfo)
				continue;

			if(SmaSetSwitchInfo(oibPort, 0, 0, node->path, &node->pSwitchInfo->SwitchInfoData) != FSUCCESS) {
				logFailedSMP(node, "Set(SwitchInfo)", parsableOutput);

				if(bail)
					return FERROR;
			}
		}
	}

	// Distribute LFTs
	if(attributesToProgram & (PROGRAM_ATTR_LFT | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending LFT(s)...\n");

		for(listNode = QListHead(&fabric->AllSWs); listNode != NULL; listNode = QListNext(&fabric->AllSWs, listNode)) {
			node = (NodeData*)QListObj(listNode);

			if(!node || !node->valid || !node->switchp || !node->switchp->LinearFDB || !node->pSwitchInfo || node->pSwitchInfo->SwitchInfoData.RoutingMode.Enabled != STL_ROUTE_LINEAR)
				continue;

			for(block = 0; block <= node->switchp->LinearFDBSize / MAX_LFT_ELEMENTS_BLOCK; block++) {
				if(SmaSetLinearFDBTable(oibPort, 0, 0, node->path, block, &node->switchp->LinearFDB[block]) != FSUCCESS) {
					logFailedSMP(node, "Set(LFT)", parsableOutput);

					if(bail)
						return FERROR;
				}
			}
		}
	}


	// Distribute PortInfo
	if(attributesToProgram & (PROGRAM_ATTR_PORTINFO | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending PortInfo(s)...\n");

		// First Set(PortInfo), program switches first followed by FIs
		// This is to avoid transient SLID security errors when IPoIB tries to reregister
		// with new LID before LID has been changed on switch port side.
		LIST_ITEM *n;
		for (n = QListHead(&fabric->AllSWs); n != NULL; n = QListNext(&fabric->AllSWs, n)) {
			node = (NodeData*)QListObj(n);
			if (FSUCCESS != DistributePortInfo(oibPort, node, directedOnly, bail, parsableOutput)) {
				if (bail)
					return FERROR;
			}
		}

		for (n = QListHead(&fabric->AllFIs); n != NULL; n = QListNext(&fabric->AllFIs, n)) {
			node = (NodeData*)QListObj(n);
			if (FSUCCESS != DistributePortInfo(oibPort, node, directedOnly, bail, parsableOutput)) {
				if (bail)
					return FERROR;
			}
		}
	}

	// Distribute PKey Tables
	// NOTE: Currently PKey distribution only supports a single block (32 entries) worth of PKey information
	if(attributesToProgram & (PROGRAM_ATTR_PKEY | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending PKey Table(s)...\n");

		for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
			node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

			if(!node || !node->valid)
				continue;

			for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
				port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

				if(!port || !port->pPartitionTable)
					continue;

				STL_PARTITION_TABLE pkeyTable;
				size_t cpyCount = sizeof(STL_PKEY_ELEMENT) * (node->pSwitchInfo ?
					node->pSwitchInfo->SwitchInfoData.PartitionEnforcementCap :
					node->NodeInfo.PartitionCap);
				cpyCount = MIN(cpyCount, sizeof(pkeyTable));

				memset(&pkeyTable, 0, sizeof(pkeyTable));
				memcpy(&pkeyTable, port->pPartitionTable, cpyCount);
				if(SmaSetPartTable(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL, port->PortNum, 0, &pkeyTable) != FSUCCESS) {
					logFailedSMP(node, "Set(PKeyTable)", parsableOutput);

					if(bail)
						return FERROR;
				}
			}
		}
	}

	// Distribute VLArb Tables
	if(attributesToProgram & (PROGRAM_ATTR_VLARB | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending VLArb Table(s)...\n");

		for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
			node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

			if(!node || !node->valid)
				continue;

			int vlArb = 0;
			if (node->NodeInfo.NodeType == STL_NODE_SW) {
				cl_map_item_t *it = cl_qmap_get(&node->Ports, 0);

				if (it != cl_qmap_end(&node->Ports)) {
					PortData *sp0 = PARENT_STRUCT(it, PortData, NodePortsEntry);
					vlArb = sp0->PortInfo.CapabilityMask3.s.VLSchedulingConfig == STL_VL_SCHED_MODE_VLARB;
				} else {
					fprintf(stderr,
						"Warning-%s: cannot get switch port 0 for node %s,"
						"0x%016"PRIx64". Assuming VLARB Supported = %d.\n",
						__func__, node->NodeDesc.NodeString, node->NodeInfo.NodeGUID, vlArb);
				}
			}

			for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
				int part;
				port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

				if (node->NodeInfo.NodeType != STL_NODE_SW)
					vlArb = port->PortInfo.CapabilityMask3.s.VLSchedulingConfig == STL_VL_SCHED_MODE_VLARB;

				if(!port || !port->pQOS || !vlArb || port->PortInfo.VL.s2.Cap <= 1)
					continue;

				for(part = 0; part < STL_VLARB_NUM_SECTIONS; part++) {
					// Skip Preemption matrices if not supported
					if (part >= STL_VLARB_PREEMPT_ELEMENTS && !(port->PortInfo.PortMode.s.IsVLMarkerEnabled ||
						port->PortInfo.FlitControl.Interleave.s.MaxNestLevelTxEnabled))
						continue;

					if(SmaSetVLArbTable(oibPort, directedOnly ? 0 : port->EndPortLID, 0,
						directedOnly ? node->path : NULL, port->PortNum, part,
						&port->pQOS->u.VLArbTable[part]) != FSUCCESS) {
						logFailedSMP(node, "Set(VLarbTable)", parsableOutput);

						if(bail)
							return FERROR;
					}
				}
			}
		}
	}


	// Distribute MFTs
	if(attributesToProgram & (PROGRAM_ATTR_MFT | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending MFT(s)...\n");

		for(listNode = QListHead(&fabric->AllSWs); listNode != NULL; listNode = QListNext(&fabric->AllSWs, listNode)) {
			node = (NodeData*)QListObj(listNode);
			uint8_t pos;
			uint32_t element;

			if(!node || !node->valid || !node->switchp || !node->pSwitchInfo
				|| !(node->pSwitchInfo->SwitchInfoData.MulticastFDBTop & STL_LID_MULTICAST_MASK))
				continue;

			for (pos = 0; pos < STL_NUM_MFT_POSITIONS_MASK; ++pos) {
				STL_MULTICAST_FORWARDING_TABLE mftBlk = {{0}};
				uint8_t nonEmpty = 0;

				if (!(node->switchp->MulticastFDB[pos]))
					continue;

				for (element = 0; element <= (node->pSwitchInfo->SwitchInfoData.MulticastFDBTop & STL_LID_MULTICAST_MASK); ++element) {
					
					if ((mftBlk.MftBlock[element % STL_NUM_MFT_ELEMENTS_BLOCK] = node->switchp->MulticastFDB[pos][element]))
						nonEmpty = 1;
					
					if (nonEmpty && ((( (element + 1) % STL_NUM_MFT_ELEMENTS_BLOCK == 0) && (element > 0))
						|| (element == (node->pSwitchInfo->SwitchInfoData.MulticastFDBTop & STL_LID_MULTICAST_MASK)) )) {

						if(SmaSetMulticastFDBTable(oibPort, directedOnly ? 0 : node->pSwitchInfo->RID.LID, 0, directedOnly ? node->path : NULL,
													(element / STL_NUM_MFT_ELEMENTS_BLOCK), pos,
													(STL_MULTICAST_FORWARDING_TABLE*)&mftBlk) != FSUCCESS) {
							logFailedSMP(node, "Set(MFT)", parsableOutput);

							if(bail)
								return FERROR;
						}
						mftBlk = (STL_MULTICAST_FORWARDING_TABLE){{0}};
						nonEmpty = 0;
					}
				}
			}
		}
	}

	// Distribute SLSC Mapping Tables
	if(attributesToProgram & (PROGRAM_ATTR_SLSC | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending SLSC Mapping Table(s)...\n");

		for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
			node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

			if(!node || !node->valid)
				continue;

			for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
				port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

				if(!port || !port->pQOS || !port->pQOS->SL2SCMap || (node->NodeInfo.NodeType == STL_NODE_SW && port->PortNum != 0))
					continue;

				if(SmaSetSLSCMappingTable(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL,  port->pQOS->SL2SCMap) != FSUCCESS) {
					logFailedSMP(node, "Set(SLSCMappingTable)", parsableOutput);

					if(bail)
						return FERROR;
				}
			}
		}
	}

	// Distribute SCSL Mapping Tables
	if(attributesToProgram & (PROGRAM_ATTR_SCSL | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending SCSL Mapping Table(s)...\n");

		for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
			node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

			if(!node || !node->valid)
				continue;

			for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
				port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

				if(!port || !port->pQOS || !port->pQOS->SC2SLMap || (node->NodeInfo.NodeType == STL_NODE_SW && port->PortNum != 0))
					continue;

				if(SmaSetSCSLMappingTable(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL,  port->pQOS->SC2SLMap) != FSUCCESS) {
					logFailedSMP(node, "Set(SCSLMappingTable)", parsableOutput);

					if(bail)
						return FERROR;
				}
			}
		}
	}

	// Distribute SCSC Mapping Tables
	if(attributesToProgram & (PROGRAM_ATTR_SCSC | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending SCSC Mapping Table(s)...\n");

		for(listNode = QListHead(&fabric->AllSWs); listNode != NULL; listNode = QListNext(&fabric->AllSWs, listNode)) {
			node = (NodeData*)QListObj(listNode);

			if(!node || !node->valid)
				continue;

			for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
				port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

				if(!port || !port->pQOS || !QListIsEmpty(&port->pQOS->SC2SCMapList[0]))
					continue;

				// NOTE: Implement SC2SC Table once STL2 format is finalized
			}
		}
	}

	// Distribute Buffer Control Tables
	if(attributesToProgram & (PROGRAM_ATTR_BFRCTRL | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending Buffer Control Table(s)...\n");

		for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
			node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

			if(!node || !node->valid)
				continue;

			for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
				port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

				if(!port || !port->pBufCtrlTable)
					continue;

				if(SmaSetBufferControlTable(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL, port->PortNum, port->PortNum, port->pBufCtrlTable) != FSUCCESS) {
					logFailedSMP(node, "Set(BufCtrlTable)", parsableOutput);

					if(bail)
						return FERROR;
				}
			}
		}
	}


	// We need to know the current state of each port to check whether it is allowed to update SC2VL tables or not.
	// In case PortInfo is one of attributes to program we can skip this step, as all information is already collected.
	if ((attributesToProgram & (PROGRAM_ATTR_SCVLR | PROGRAM_ATTR_SCVLT | PROGRAM_ATTR_SCVLNT))
		&& !(attributesToProgram & PROGRAM_ATTR_PORTINFO)) {

		if (updateFabricPortInfo(oibPort, fabric, directedOnly, bail, parsableOutput) != FSUCCESS) {
			if (bail)
				return FERROR;
		}
	}

	// Distribute SCVLr Tables
	if(attributesToProgram & (PROGRAM_ATTR_SCVLR | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending SCVLr Table(s)...\n");

		for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
			node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

			if(!node || !node->valid)
				continue;

			// SCVLr is switch-wide, so send one Set(SCVLr) with
			// AttrMod.AllPorts = 1 if SCVLr is same for all ports
			if (node->NodeInfo.NodeType == STL_NODE_SW) {
				STL_SCVLMAP *firstMap = NULL;
				int allSame = 1;

				for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
					port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);
					if (!port || !port->pQOS || !getIsVLrSupported(node, port))
						continue;

					if (!firstMap)
						firstMap = &port->pQOS->SC2VLMaps[Enum_SCVLr];
					else
						allSame = memcmp(firstMap, &port->pQOS->SC2VLMaps[Enum_SCVLr], sizeof(STL_SCVLMAP)) == 0;

					if (!allSame) {
						if (!parsableOutput) {
							fprintf(stderr, "Error: Cannot set SCVLr on switch %s 0x%016"PRIx64
								", switch per-port SCVLr tables must be the same.\n",
								node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
						} else {
							fprintf(stderr, "error;snapshot;%s0x%016"PRIx64
								";switch per-port SCVLr tables must be the same\n",
								node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
						}

						if (bail)
							return FERROR;

						break;
					}
				}

				if (allSame && firstMap) {
					port = FindNodePort(node, 0);
					if (!port) {
						if (!parsableOutput) {
							fprintf(stderr, "Error: Unable to find port 0 on switch %s 0x%016"PRIx64"\n",
								node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
						} else {
							fprintf(stderr, "error;application;%s0x%016"PRIx64
								";unable to find port 0\n",
								node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
						}
						return FERROR;
					}
					sc2vlUpdateType = getSC2VLUpdateType(node, port, Enum_SCVLr);
					if (sc2vlUpdateType == SC2VL_UPDATE_TYPE_NONE) {
						if (!parsableOutput) {
							fprintf(stderr, "Error: Cannot set SCVLr on switch %s 0x%016"PRIx64
								", switch ports must be in state Init or support asynchronous update\n",
								node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
						} else {
							fprintf(stderr, "error;application;%s0x%016"PRIx64
								";switch ports must be in state Init or support asynchronous update\n",
								node->NodeDesc.NodeString, node->NodeInfo.NodeGUID);
						}
						if (bail)
							return FERROR;
						continue;
					}

					if (SmaSetSCVLMappingTable(oibPort,
												directedOnly ? 0 : port->EndPortLID,
												0,
												directedOnly ? node->path : NULL,
												sc2vlUpdateType == SC2VL_UPDATE_TYPE_ASYNC,
												1,
												0,
												firstMap,
												STL_MCLASS_ATTRIB_ID_SC_VLR_MAPPING_TABLE) != FSUCCESS) {
						logFailedSMP(node, "Set(SCVLrMappingTable)", parsableOutput);

						if (bail)
							return FERROR;
					}
				}
			} else {
				for (clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports);clPort = cl_qmap_next(clPort)) {
					port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

					if (!port || !port->pQOS || !getIsVLrSupported(node, port))
						continue;

					sc2vlUpdateType = getSC2VLUpdateType(node, port, Enum_SCVLr);
					if (sc2vlUpdateType == SC2VL_UPDATE_TYPE_NONE) {
						if (!parsableOutput) {
							fprintf(stderr, "Error: Cannot set SCVLr on port %d in state %s"
								", port must be in state Init or support asynchronous update\n",
								port->PortNum, StlPortStateToText(port->PortInfo.PortStates.s.PortState));
						} else {
							fprintf(stderr, "error;application;cannot set SCVLr on port %d in state %s"
								";port must be in state Init or support asynchronous update\n",
								port->PortNum, StlPortStateToText(port->PortInfo.PortStates.s.PortState));
						}
						if (bail)
							return FERROR;
						continue;
					}

					if (SmaSetSCVLMappingTable(oibPort,
												directedOnly ? 0 : port->EndPortLID,
												0,
												directedOnly ? node->path : NULL,
												sc2vlUpdateType == SC2VL_UPDATE_TYPE_ASYNC,
												0,
												port->PortNum,
												&port->pQOS->SC2VLMaps[Enum_SCVLr],
												STL_MCLASS_ATTRIB_ID_SC_VLR_MAPPING_TABLE) != FSUCCESS) {
						logFailedSMP(node, "Set(SCVLrMappingTable)", parsableOutput);

						if (bail)
							return FERROR;
					}
				}
			}
		}
	}

	// Distribute SCVLt Tables
	if(attributesToProgram & (PROGRAM_ATTR_SCVLT | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending SCVLt Table(s)...\n");

		for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
			node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

			if(!node || !node->valid)
				continue;

			for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
				port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

				if(!port || !port->pQOS)
					continue;

				sc2vlUpdateType = getSC2VLUpdateType(node, port, Enum_SCVLt);
				if (sc2vlUpdateType == SC2VL_UPDATE_TYPE_NONE) {
					if (!parsableOutput) {
						fprintf(stderr, "Error: Cannot set SCVLt on port %d in state %s"
							", port must be in state Init or support asynchronous update\n",
							port->PortNum, StlPortStateToText(port->PortInfo.PortStates.s.PortState));
					} else {
						fprintf(stderr, "error;application;cannot set SCVLt on port %d in state %s"
							";port must be in state Init or support asynchronous update\n",
							port->PortNum, StlPortStateToText(port->PortInfo.PortStates.s.PortState));
					}
					if (bail)
						return FERROR;
					continue;
				}

				if (SmaSetSCVLMappingTable(oibPort,
											directedOnly ? 0 : port->EndPortLID,
											0,
											directedOnly ? node->path : NULL,
											sc2vlUpdateType == SC2VL_UPDATE_TYPE_ASYNC,
											0,
											port->PortNum,
											&port->pQOS->SC2VLMaps[Enum_SCVLt],
											STL_MCLASS_ATTRIB_ID_SC_VLT_MAPPING_TABLE) != FSUCCESS) {
					logFailedSMP(node, "Set(SCVLtMappingTable)", parsableOutput);

					if (bail)
						return FERROR;
				}
			}
		}
	}

	// Distribute SCVLnt tables
	if(attributesToProgram & (PROGRAM_ATTR_SCVLNT | PROGRAM_ATTR_ALL)) {
		if(!parsableOutput)
			fprintf(stdout, "Sending SCVLnt Table(s)...\n");

		for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
			node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

			if(!node || !node->valid)
				continue;

			for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
				port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

				if(!port || !port->pQOS || (node->NodeInfo.NodeType == STL_NODE_SW && port->PortNum == 0))
					continue;

				sc2vlUpdateType = getSC2VLUpdateType(node, port, Enum_SCVLnt);
				if (sc2vlUpdateType == SC2VL_UPDATE_TYPE_NONE) {
					if (!parsableOutput) {
						fprintf(stderr, "Error: Cannot set SCVLnt on port %d in state %s"
							", port must be in state Init or support asynchronous update\n",
							port->PortNum, StlPortStateToText(port->PortInfo.PortStates.s.PortState));
					} else {
						fprintf(stderr, "error;application;cannot set SCVLnt on port %d in state %s"
							";port must be in state Init or support asynchronous update\n",
							port->PortNum, StlPortStateToText(port->PortInfo.PortStates.s.PortState));
					}
					continue;
				}

				// asynchronous update is taken by the SMA
				if (sc2vlUpdateType == SC2VL_UPDATE_TYPE_ASYNC)
					continue;

				if (SmaSetSCVLMappingTable(oibPort,
											directedOnly ? 0 : port->EndPortLID,
											0,
											directedOnly ? node->path : NULL,
											sc2vlUpdateType == SC2VL_UPDATE_TYPE_ASYNC,
											0,
											port->PortNum,
											&port->pQOS->SC2VLMaps[Enum_SCVLnt],
											STL_MCLASS_ATTRIB_ID_SC_VLNT_MAPPING_TABLE) != FSUCCESS) {
					logFailedSMP(node, "Set(SCVLntMappingTable)", parsableOutput);

					if (bail)
						return FERROR;
				}
			}
		}
	}


	if(!parsableOutput)
		fprintf(stdout, "Fabric Programming Completed\n");

	return status;
}

FSTATUS activateFabric(struct omgt_port* oibPort, FabricData_t* fabric, uint8_t directedOnly, uint8_t bail, uint8_t parsableOutput) {
	FSTATUS status = FSUCCESS;
	cl_map_item_t *clNode, *clPort;
	NodeData* node;
	PortData* port;
	STL_PORT_INFO portInfo;

	if(!parsableOutput)
		fprintf(stdout, "Arming Ports...\n");

	for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
		node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

		if(!node || !node->valid)
			continue;

		for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
			port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

			if(!port)
				continue;

			if(!port->neighbor) {
				/* exclude switch port 0 */
				if (!(node->NodeInfo.NodeType == STL_NODE_SW && port->PortNum == 0))
					continue;
			}

			if(port->neighbor && !port->neighbor->nodep->valid)
				continue;

			memset(&portInfo, 0, sizeof(portInfo));

			if(SmaGetPortInfo(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL, port->PortNum, 1, &portInfo) != FSUCCESS) {
				logFailedSMP(node, "Get(PortInfo)", parsableOutput);

				if(bail)
					return FERROR;
			}

			if(portInfo.PortStates.s.PortState != IB_PORT_INIT)
				continue;

			// Set the "No chang'e" attributes
			portInfo.LinkSpeed.Enabled = 0;
			portInfo.LinkWidth.Enabled = 0;
			portInfo.PortStates.s.PortPhysicalState = IB_PORT_PHYS_NOP;
			portInfo.s4.OperationalVL = 0;
			portInfo.PortStates.s.PortState = IB_PORT_ARMED;

			if(SmaSetPortInfo(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL, port->PortNum, &portInfo) != FSUCCESS) {
				logFailedSMP(node, "Set(PortInfo)", parsableOutput);

				if(bail)
					return FERROR;
			}
		}
	}

	if(!parsableOutput)
		fprintf(stdout, "Activating Ports...\n");

	for(clNode = cl_qmap_head(&fabric->AllNodes); clNode != cl_qmap_end(&fabric->AllNodes); clNode = cl_qmap_next(clNode)) {
		node = PARENT_STRUCT(clNode, NodeData, AllNodesEntry);

		if(!node || !node->valid)
			continue;

		for(clPort = cl_qmap_head(&node->Ports); clPort != cl_qmap_end(&node->Ports); clPort = cl_qmap_next(clPort)) {
			port = PARENT_STRUCT(clPort, PortData, NodePortsEntry);

			if(!port)
				continue;

			if(!port->neighbor) {
				/* exclude switch port 0 */
				if (!(node->NodeInfo.NodeType == STL_NODE_SW && port->PortNum == 0))
					continue;
			}

			if(port->neighbor && !port->neighbor->nodep->valid)
				continue;

			memset(&portInfo, 0, sizeof(portInfo));

			if(SmaGetPortInfo(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL, port->PortNum, 0, &portInfo) != FSUCCESS) {
				logFailedSMP(node, "Get(PortInfo)", parsableOutput);

				if(bail)
					return FERROR;
			}

			if(portInfo.PortStates.s.PortState != IB_PORT_ARMED)
				continue;

			// Set the "No change" attributes
			portInfo.LinkSpeed.Enabled = 0;
			portInfo.LinkWidth.Enabled = 0;
			portInfo.PortStates.s.PortPhysicalState = IB_PORT_PHYS_NOP;
			portInfo.s4.OperationalVL = 0;
			portInfo.PortStates.s.PortState = IB_PORT_ACTIVE;

			if(SmaSetPortInfo(oibPort, directedOnly ? 0 : port->EndPortLID, 0, directedOnly ? node->path : NULL, port->PortNum, &portInfo) != FSUCCESS) {
				logFailedSMP(node, "Set(PortInfo)", parsableOutput);

				if(bail)
					return FERROR;
			}
		}
	}

	return status;
}

int main(int argc, char ** argv)
{
    int c;
	int	index;
	FSTATUS status = 0;
	int exitStatus = 0;
	struct omgt_port* oibPort;
	FabricData_t fabric = { 0 };
	char attributeString[100];
	memset(attributeString, 0, 100);
	NodeData* localNode = NULL;
	cl_map_item_t *mapPort;
	PortData* portData;

	uint8_t filenameOptind = 0;
	uint8_t verbose = 0;
	uint8_t hfi = 0;
	uint8_t port = 0;
	uint8_t programOnly = 0;
	uint8_t activateOnly = 0;
	uint32_t attributesToProgram = PROGRAM_ATTR_ALL;
	uint8_t sendAttempts = 2;
	uint8_t directedOnly = 0;
	uint8_t strictMatching = 0;
	uint8_t bailOnError = 0;
	uint8_t parsableOutput = 0;

	Top_setcmdname("opasnapconfig");
	
	// process command line arguments
	while (-1 != (c = getopt_long(argc,argv, "vh:p:PAa:c:dsbg",
									options, &index)))
	{
		switch (c)
		{
			case '$':
				Usage_full();
				break;
			case 'v':
				verbose++;
				setTopologyMadVerboseFile(stdout);
				if (verbose > 3) umad_debug(verbose - 3);
				break;
			case 'h':
				if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasnapconfig: Invalid HFI Number: %s\n", optarg);
					Usage();
				}
				break;
			case 'p':
				if (FSUCCESS != StringToUint8(&port, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasnapconfig: Invalid Port Number: %s\n", optarg);
					Usage();
				}
				break;
			case 'P':
				if(activateOnly) {
					fprintf(stderr, "opasnapconfig: Cannot specify both -P and -A flag together\n");
					Usage();
				}
				programOnly = 1;
				break;
			case 'A':
				if(programOnly) {
					fprintf(stderr, "opasnapconfig: Cannot specify both -P and -A flag together\n");
					Usage();
				}
				activateOnly = 1;
				break;
			case 'a':
				attributesToProgram &= ~PROGRAM_ATTR_ALL;
				attributesToProgram |= attributeToFlag(optarg);
				break;
			case 'c':
				if (FSUCCESS != StringToUint8(&sendAttempts, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opasnapconfig: Invalid Number of Attempts: %s\n", optarg);
					Usage();
				}
				setTopologyMadRetryCount(sendAttempts);
				break;
			case 'd':
				directedOnly = 1;
				break;
			case 's':
				strictMatching = 1;
				break;
			case 'b':
				bailOnError = 1;
				break;
			case 'g':
				parsableOutput = 1;
				break;
			default:
				fprintf(stderr, "opasnapconfig: Invalid option -%c\n", c);
				Usage();
				break;
		}
	} /* end while */

	// Check the and make sure there is exactly one input snapshot file
	if (optind == argc - 1)
	{
		filenameOptind = optind;
	} else if (optind == argc){
		fprintf(stderr, "opasnapconfig: no input snapshot file specified\n");
		Usage();
	} else {
		fprintf(stderr, "opasnapconfig: multiple input snapshot files unsupported\n");
		Usage();
	}

	if (!(*argv[filenameOptind])) {
		fprintf(stderr, "opasnapconfig: Error: null input filename\n");
		exitStatus = EXIT_STATUS_BAD_FILENAME;
		Usage();
	}

	// Output run summary info
	if(!parsableOutput) {
		fprintf(stdout, "Input file: %s\n", argv[filenameOptind]);
		fprintf(stdout, "Programming Phase: %s\n", activateOnly ? "No" : "Yes");
		fprintf(stdout, "Activation Phase: %s\n", programOnly ? "No" : "Yes");
		attributeFlagsToString(attributesToProgram, attributeString, 100);
		fprintf(stdout, "Selected Attributes: %s\n", attributeString);

		// Parse snapshot file
		fprintf(stdout, "\n");
		fprintf(stdout, "Parsing snapshot file... ");
	}

	if (FSUCCESS != Xml2ParseSnapshot(argv[filenameOptind], 1, &fabric, FF_NONE, 0)) {
		if(parsableOutput)
			fprintf(stderr, "error;application;Xml2ParseSnapshot;Failed to parse snapshot\n");
		else 
			fprintf(stderr, "ERROR: Failed to parse input snapshot file\n");

		exitStatus = EXIT_STATUS_FAILED_PARSE;
		goto done;
	}
	if(!parsableOutput)
		fprintf(stdout, "Done\n");

	// Open a port to the fabric
	struct omgt_params params = {.debug_file = (verbose > 2 ? stdout : NULL)};
	status = omgt_open_port_by_num (&oibPort, hfi, port, &params);
	if (status == OMGT_STATUS_NOT_DONE) {
		// Wildcard search for either/or port & hfi yielded no ACTIVE ports.
		if (!port) {
			if(!hfi) {
				// System wildcard search for Active port failed.
				// Fallback and query default hfi:1, port:1
				if(!parsableOutput)
					fprintf(stderr, "No Active port found in system. Trying default hfi:1 port:1\n");
				hfi = 1;
				port = 1;
				status = omgt_open_port_by_num(&oibPort, hfi, port, &params);
			} else {
				if(parsableOutput)
					fprintf(stderr, "error;application;openLocalPort;No active port found on hfi:%d\n", hfi);
				else
					fprintf(stderr, "No Active port found on hfi:%d\n", hfi);
				exit (EXIT_STATUS_FAILED_OPEN_PORT);
			}
		}
	}
	if (status != 0) {
		if(parsableOutput)
			fprintf(stderr, "error;application;openLocalPort;Failed to open port hfi:%d:%d: %s\n", hfi, port, strerror(status));
		else
			fprintf(stderr, "Failed to open port hfi %d:%d: %s\n", hfi, port, strerror(status));

		exit(EXIT_STATUS_FAILED_OPEN_PORT);
	}

	// Perform pre-programming prep
	localNode = locateLocalNode(oibPort, &fabric, strictMatching, parsableOutput);
	if(localNode == NULL) {
		if(parsableOutput)
			fprintf(stderr, "error;snapshot;locateLocalNode;Failed to properly locate local node in snapshot file\n");
		else
			fprintf(stderr, "ERROR: Failed to properly locate local node in snapshot file. Unable to calculate packet routes.\n");

		exitStatus = EXIT_STATUS_NO_LOCAL_NODE;
		goto done;
	}


	// Recursivly verify the identities of all nodes in our snapshot file
	for (mapPort = cl_qmap_head(&localNode->Ports); mapPort != cl_qmap_end(&localNode->Ports); mapPort = cl_qmap_next(mapPort)) {
		portData = PARENT_STRUCT(mapPort, PortData, NodePortsEntry);

		if(portData->neighbor != NULL && portData->neighbor->nodep != NULL && portData->PortNum != 0) {
			FSTATUS status;
			uint8_t path[DR_PATH_SIZE];
			memset(path, 0, sizeof(path));

			// Set the path port number and hop count
			path[0] = 1;
			path[1] = portData->PortNum;

			status = verifyNodeIdentityRecurse(oibPort, portData->neighbor->nodep, path, strictMatching, bailOnError, parsableOutput);
			if(status != FSUCCESS){
				exitStatus = EXIT_STATUS_FAILED_IDENTITY_VERIFICATION;
				goto done;
			}
		}
	}
	status = checkAllNodesVerified(&fabric, parsableOutput);
	if(status != FSUCCESS){
		exitStatus = EXIT_STATUS_FAILED_IDENTITY_VERIFICATION;
		goto done;
	}
	
	// Program the fabric using the SMP information we have
	if(!activateOnly) {
		status = distributeSMPs(oibPort, &fabric, attributesToProgram, directedOnly, bailOnError, parsableOutput);
		if(status != FSUCCESS) {
			if(parsableOutput)
				fprintf(stderr, "error;application;distributeSMPs;Failed to distribute all SMPs\n");
			else
				fprintf(stderr, "ERROR: Failed to distribute all SMPs\n");

			exitStatus = EXIT_STATUS_FAILED_SMP_DISTRIBUTION;
			goto done;
		}
	}

	// Perform fabric activation
	if(!programOnly) {
		status = activateFabric(oibPort, &fabric, directedOnly, bailOnError, parsableOutput);
		if(status != FSUCCESS) {
			if(parsableOutput)
				fprintf(stderr, "error;application;activateFabric;Failed to activate all nodes in fabric\n");
			else
				fprintf(stderr, "ERROR: Failed to activate all nodes in fabric\n");

			exitStatus = EXIT_STATUS_FAILED_ACTIVATION;
			goto done;
		}
	}

done:
	DestroyFabricData(&fabric);

	if (exitStatus == EXIT_STATUS_BADOPTIONS)
		Usage();

	return exitStatus;
}
