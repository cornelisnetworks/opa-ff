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

#define _GNU_SOURCE
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <features.h>

#include "iba/public/ibyteswap.h"
#include "iba/stl_types.h"
#include "iba/ib_mad.h"
#include "iba/stl_pkt.h"
#include "opapacketcapture.h"
#include <umad.h> /* for ioctl commands */
#include <dirent.h>

uint8					*blocks;
packet					*packets;

packet					*freePackets;
packet					*oldestPacket;
packet					*newestPacket;

uint8					*currentBlock;

uint64				 	blocksTableSize;
uint64			 		numpackets;

int				 		numPacketsRead = 0;
int				 		numPacketsTaken = 0;
uint64			 		numPacketsMax = (uint64)-1;
int gotModeArg = 0;
uint8					mode = 0;
int				 		numPacketsStored = 0;
int				 		stopcapture = 0;
int				 		triggerSeen = 0;
int				 		afterTriggerPackets = 0;

uint32					alarmArg = 0;
char					filterFileArg[256];
char					triggerFileArg[256];
int						triggerLag = DEFAULT_TRIGGER_LAG;
int						gotFilterFileArg = 0;
int						gotTriggerFileArg = 0;
int						gotAlarmArg = 0;
int						gotTriggerLagArg = 0;
int						writethrough = 0;
int						retryCount;

filterFunc_t			filters[25];
filterFunc_t			triggers[25];

int						numFilterFunctions = 0;
int						numTriggerFunctions = 0;

int						filterCondition;
int						gotCondition = 0;
int						triggerCondition;
int						gotTriggerCondition = 0;

int						gotdevfile;
int						gotoutfile;
char					devfile[256];
char					out_file[256];
int						fdIn;

int						ioctlConfigured = 0;
qibPacketFilterCommand_t	filterCmd;
uint32					filterValue;

int						verbose = 0;

static void my_handler(int signal)
{
	stopcapture = 1;
	printf("\nopapacketcapture: Triggered\n");
}

int filterSLID(packet *p, uint32 val)
{
	IB_LID lid = STL2IB_LID(val);
	IB_LRH *lrh;
	int res = 0;

	lrh = (IB_LRH *)(&blocks[p->blockNum * BLOCKSIZE]);

	res = (ntoh16(lrh->SrcLID) == lid);

	return(res);
}

int filterDLID(packet *p, uint32 val)
{
	IB_LID lid = STL2IB_LID(val);
	IB_LRH *lrh;
	int res = 0;

	lrh = (IB_LRH *)(&blocks[p->blockNum * BLOCKSIZE]);

	res = (ntoh16(lrh->DestLID) == lid);

	return(res);
}

int filterServiceLevel(packet *p, uint32 val)
{
	uint8 sl = (uint8)val;
	IB_LRH *lrh;
	int res = 0;

	lrh = (IB_LRH *)(&blocks[p->blockNum * BLOCKSIZE]);

	res = (lrh->l.ServiceLevel == sl);

	return(res);
}

int filterMgmtClass(packet *p, uint32 val)
{
	uint8 mc = (uint8)val;
	IB_BTH *b;
	MAD_COMMON *m;
	uint32 destQP;
	int res = 0;

	b = (IB_BTH *)(&blocks[p->blockNum * BLOCKSIZE] + sizeof(IB_LRH));
	destQP = b->Qp.s.DestQPNumber;

	if ((destQP == 0) || (destQP == 1)) {
		m = (MAD_COMMON *)(&blocks[p->blockNum * BLOCKSIZE] + sizeof(IB_LRH) + sizeof(IB_BTH) + sizeof(IB_DETH));
		res = (m->MgmtClass == mc);
	}

	return(res);
}

int filterPKey(packet *p, uint32 val)
{
	uint16 pkey = (uint16)val;
	IB_BTH *b;
	int res = 0;

	b = (IB_BTH *)(&blocks[p->blockNum * BLOCKSIZE] + sizeof(IB_LRH));

	res = ((ntoh16(b->Pkey) & PKEY_MASK) == (pkey & PKEY_MASK));

	return(res);
}

int filterPacketType(packet *p, uint32 val)
{
	uint8 pktType = (uint8)val;
	IB_BTH *b;
	int res = 0;

	b = (IB_BTH *)(&blocks[p->blockNum * BLOCKSIZE] + sizeof(IB_LRH));

	res = ((b->OpCode>>5) == pktType);

	return(res);
}

int filterAttrID(packet *p, uint32 val)
{
	uint16 aid = (uint16)val;
	IB_BTH *b;
	MAD_COMMON *m;
	uint32 destQP;
	int res = 0;

	b = (IB_BTH *)(&blocks[p->blockNum * BLOCKSIZE] + sizeof(IB_LRH));
	destQP = b->Qp.s.DestQPNumber;

	if ((destQP == 0) || (destQP == 1)) {
		m = (MAD_COMMON *)(&blocks[p->blockNum * BLOCKSIZE] + sizeof(IB_LRH) + sizeof(IB_BTH) + sizeof(IB_DETH));
		res = (ntoh16(m->AttributeID) == aid);
	}

	return(res);
}

int filterQueuePair(packet *p, uint32 val)
{
	IB_BTH *b;
	uint32 qp = val;
	uint32 pkt_qp;
	int res = 0;

	b = (IB_BTH *)(&blocks[p->blockNum * BLOCKSIZE] + sizeof(IB_LRH));
	pkt_qp = ntoh32(b->Qp.AsUINT32) & DESTQP_MASK;
	res = (pkt_qp == qp);

	return(res);
}

int applyFilters(packet *p)
{
	int res;
	int i;

	res = (filterCondition == COND_TYPE_AND) ? 1 : 0;

	for (i = 0; (i < numFilterFunctions) && ((filterCondition == COND_TYPE_AND) ? res : !res); i++) {
		if (!filters[i].ioctl) {
			// XOR result with notFlag
			res = filters[i].filterFunc(p, filters[i].filterVal) ^ filters[i].notFlag;
		}
	}

	return(res);
}

int applyTriggers(packet *p)
{
	int res;
	int i;

	res = (triggerCondition == COND_TYPE_AND) ? 1 : 0;

	for (i = 0; (i < numTriggerFunctions) && ((triggerCondition == COND_TYPE_AND) ? res : !res); i++) {
		res = triggers[i].filterFunc(p, triggers[i].filterVal) ^ triggers[i].notFlag;
	}

	return(res);
}

int getFilterType(char *filter)
{
	int res;
	if (!strcmp(filter, "COND"))
		res = FILTER_COND;
	else if (!strcmp(filter, "DLID"))
		res = FILTER_DLID;
	else if (!strcmp(filter, "SLID"))
		res = FILTER_SLID;
	else if (!strcmp(filter, "MCLASS"))
		res = FILTER_MCLASS;
	else if (!strcmp(filter, "PKEY"))
		res = FILTER_PKEY;
	else if (!strcmp(filter, "PTYPE"))
		res = FILTER_PTYPE;
	else if (!strcmp(filter, "SVCLEV"))
		res = FILTER_SVCLEV;
	else if (!strcmp(filter, "ATTRID"))
		res = FILTER_ATTRID;
	else if (!strcmp(filter, "QP"))
		res = FILTER_QP;
	else
		res = -1;

	return(res);
}

int getPacketType(char *filter)
{
	int res;
	if (!strcmp(filter, "RC"))
		res = PACKETTYPE_RC;
	else if (!strcmp(filter, "UC"))
		res = PACKETTYPE_UC;
	else if (!strcmp(filter, "RD"))
		res = PACKETTYPE_RD;
	else if (!strcmp(filter, "UD"))
		res = PACKETTYPE_UD;
	else
		res = PACKETTYPE_ERR;

	return(res);
}

int getCondType(char *condition)
{
	int res;
	if (!strcmp(condition, "AND"))
		res = COND_TYPE_AND;
	else if (!strcmp(condition, "OR"))
		res = COND_TYPE_OR;
	else
		res = -1;

	return(res);
}

void setupIoctl(int filter, uint32 *valPtr)
{
	filterCmd.value_ptr = (void *)valPtr;
	switch (filter) {
		case FILTER_DLID:
			filterCmd.opcode = FILTER_BY_DLID;
			filterCmd.length = sizeof(uint16);
			break;
		case FILTER_SLID:
			filterCmd.opcode = FILTER_BY_LID;
			filterCmd.length = sizeof(uint16);
			break;
		case FILTER_MCLASS:
			filterCmd.opcode = FILTER_BY_MAD_MGMT_CLASS;
			filterCmd.length = sizeof(uint8);
			break;
		case FILTER_PKEY:
			filterCmd.opcode = FILTER_BY_PKEY;
			filterCmd.length = sizeof(uint16);
			break;
		case FILTER_PTYPE:
			filterCmd.opcode = FILTER_BY_PKT_TYPE;
			filterCmd.length = sizeof(uint8);
			break;
		case FILTER_SVCLEV:
			filterCmd.opcode = FILTER_BY_SERVICE_LEVEL;
			filterCmd.length = sizeof(uint8);
			break;
		case FILTER_ATTRID:
			/* not supported in driver */
			fprintf(stderr, "Error: filter on attribute ID not supported in qib driver\n");
			exit(1);
			break;
		case FILTER_QP:
			filterCmd.opcode = FILTER_BY_QP_NUMBER;
			filterCmd.length = sizeof(uint32);
			break;
	}
	return;
}

void setupFilters(char *filterFile)
{
	FILE *fp;
	char inbuf[128];
	char *p;
	char *p1;
	char filterType[10];
	int filter;
	uint32 val;
	char strVal[10];
	int not;

	if ((fp = fopen(filterFile, "r")) == NULL) {
		fprintf(stderr, "Error opening file <%s> for input: %s\n", filterFile, strerror(errno));
		exit(1);
	}

	while (fgets(inbuf, 128, fp) != NULL) {
		p = inbuf;
		if (*p == '#')
			continue;
		if (*p == '\n')
			continue;
		if (*p == ' ')
			continue;
		if ((*p == '!') || (*p == '~')) {
			not = 1;
			p++;
		} else
			not = 0;
		if ((p1 = strchr(p, '#')) != NULL)
			*p1 = '\0';
		sscanf(p, "%9s", filterType);
		if ((filter = getFilterType(filterType)) < 0) {
			fprintf(stderr, "Invalid filter type %s\n", filterType);
			exit(1);
		}
		switch (filter) {
			case FILTER_DLID:
				sscanf(p, "%9s %u", filterType, &val);
				filters[numFilterFunctions].filterFunc = &filterDLID;
				filters[numFilterFunctions].filterVal = val;
				filters[numFilterFunctions].ioctl = 0;
				filters[numFilterFunctions].notFlag = not;
				if (!ioctlConfigured && !not) {
					ioctlConfigured = 1;
					filterValue = val;
					setupIoctl(filter, &filterValue);
					filters[numFilterFunctions].ioctl = 1;
				}
				numFilterFunctions++;
				break;
			case FILTER_SLID:
				sscanf(p, "%9s %u", filterType, &val);
				filters[numFilterFunctions].filterFunc = &filterSLID;
				filters[numFilterFunctions].filterVal = val;
				filters[numFilterFunctions].ioctl = 0;
				filters[numFilterFunctions].notFlag = not;
				if (!ioctlConfigured && !not) {
					ioctlConfigured = 1;
					filterValue = val;
					setupIoctl(filter, &filterValue);
					filters[numFilterFunctions].ioctl = 1;
				}
				numFilterFunctions++;
				break;
			case FILTER_MCLASS:
				sscanf(p, "%9s %u", filterType, &val);
				filters[numFilterFunctions].filterFunc = &filterMgmtClass;
				filters[numFilterFunctions].filterVal = val;
				filters[numFilterFunctions].ioctl = 0;
				filters[numFilterFunctions].notFlag = not;
				if (!ioctlConfigured && !not) {
					ioctlConfigured = 1;
					filterValue = val;
					setupIoctl(filter, &filterValue);
					filters[numFilterFunctions].ioctl = 1;
				}
				numFilterFunctions++;
				break;
			case FILTER_PKEY:
				sscanf(p, "%9s 0x%x", filterType, &val);
				filters[numFilterFunctions].filterFunc = &filterPKey;
				filters[numFilterFunctions].filterVal = val;
				filters[numFilterFunctions].ioctl = 0;
				filters[numFilterFunctions].notFlag = not;
				if (!ioctlConfigured && !not) {
					ioctlConfigured = 1;
					filterValue = (val & PKEY_MASK);
					setupIoctl(filter, &filterValue);
					filters[numFilterFunctions].ioctl = 1;
				}
				numFilterFunctions++;
				break;
			case FILTER_PTYPE:
				sscanf(p, "%9s %9s", filterType, strVal);
				if ((val = getPacketType(strVal)) == PACKETTYPE_ERR) {
					fprintf(stderr, "Invalid packet type %s\n", strVal);
					exit(1);
				}
				filters[numFilterFunctions].filterFunc = &filterPacketType;
				filters[numFilterFunctions].filterVal = val;
				filters[numFilterFunctions].ioctl = 0;
				filters[numFilterFunctions].notFlag = not;
				if (!ioctlConfigured && !not) {
					ioctlConfigured = 1;
					filterValue = val;
					setupIoctl(filter, &filterValue);
					filters[numFilterFunctions].ioctl = 1;
				}
				numFilterFunctions++;
				break;
			case FILTER_SVCLEV:
				sscanf(p, "%9s %u", filterType, &val);
				filters[numFilterFunctions].filterFunc = &filterServiceLevel;
				filters[numFilterFunctions].filterVal = val;
				filters[numFilterFunctions].ioctl = 0;
				filters[numFilterFunctions].notFlag = not;
				if (!ioctlConfigured && !not) {
					ioctlConfigured = 1;
					filterValue = val;
					setupIoctl(filter, &filterValue);
					filters[numFilterFunctions].ioctl = 1;
				}
				numFilterFunctions++;
				break;
			case FILTER_ATTRID:
				sscanf(p, "%9s %9s", filterType, strVal);
				if (strstr(strVal, "0x") != NULL) {
					sscanf(p, "%9s 0x%x", strVal, &val);
				} else {
					sscanf(p, "%9s %u", strVal, &val);
				}
				filters[numFilterFunctions].filterFunc = &filterAttrID;
				filters[numFilterFunctions].filterVal = val;
				filters[numFilterFunctions].ioctl = 0;
				filters[numFilterFunctions].notFlag = not;
#if 0 /* filter on attribute id not supported in driver */
				if (!ioctlConfigured && !not) {
					ioctlConfigured = 1;
					filterValue = val;
					setupIoctl(filter, &filterValue);
					filters[numFilterFunctions].ioctl = 1;
				}
#endif
				numFilterFunctions++;
				break;
			case FILTER_QP:
				sscanf(p, "%9s %u", filterType, &val);
				filters[numFilterFunctions].filterFunc = &filterQueuePair;
				filters[numFilterFunctions].filterVal = val;
				filters[numFilterFunctions].ioctl = 0;
				filters[numFilterFunctions].notFlag = not;
				if (!ioctlConfigured && !not) {
					ioctlConfigured = 1;
					filterValue = val;
					setupIoctl(filter, &filterValue);
					filters[numFilterFunctions].ioctl = 1;
				}
				numFilterFunctions++;
				break;
			case FILTER_COND:
				if (not) {
					fprintf(stderr, "Not sign not allowed on condition\n");
					exit(1);
				}
				if (gotCondition) {
					fprintf(stderr, "Only one condition statement allowed\n");
					exit(1);
				}
				gotCondition = 1;
				sscanf(p, "%9s %9s", filterType, strVal);
				if ((filterCondition = getCondType(strVal)) < 0) {
					fprintf(stderr, "Invalid condition type %s\n", strVal);
					exit(1);
				}
				break;
		}
	}

	fclose(fp);

	/* if condition is OR, turn off ioctl */
	if (filterCondition == COND_TYPE_OR) {
		int i;
		ioctlConfigured = 0;
		for (i = 0; i <= numFilterFunctions; i++)
			filters[i].ioctl = 0;
	}
	return;
}

void setupTriggers(char *triggerFile)
{
	FILE *fp;
	char inbuf[128];
	char *p;
	char *p1;
	char triggerType[10];
	int trigger;
	uint32 val;
	char strVal[10];
	int not;

	if ((fp = fopen(triggerFile, "r")) == NULL) {
		fprintf(stderr, "Error opening file <%s> for input: %s\n", triggerFile, strerror(errno));
		exit(1);
	}

	while (fgets(inbuf, 128, fp) != NULL) {
		p = inbuf;
		if (*p == '#')
			continue;
		if (*p == '\n')
			continue;
		if (*p == ' ')
			continue;
		if ((*p == '!') || (*p == '~')) {
			not = 1;
			p++;
		} else
			not = 0;
		if ((p1 = strchr(p, '#')) != NULL)
			*p1 = '\0';
		sscanf(p, "%9s", triggerType);
		if ((trigger = getFilterType(triggerType)) < 0) {
			fprintf(stderr, "Invalid trigger type %s\n", triggerType);
			exit(1);
		}
		switch (trigger) {
			case FILTER_DLID:
				sscanf(p, "%9s %u", triggerType, &val);
				triggers[numTriggerFunctions].filterFunc = &filterDLID;
				triggers[numTriggerFunctions].filterVal = val;
				triggers[numTriggerFunctions].ioctl = 0;
				triggers[numTriggerFunctions].notFlag = not;
				numTriggerFunctions++;
				break;
			case FILTER_SLID:
				sscanf(p, "%9s %u", triggerType, &val);
				triggers[numTriggerFunctions].filterFunc = &filterSLID;
				triggers[numTriggerFunctions].filterVal = val;
				triggers[numTriggerFunctions].ioctl = 0;
				triggers[numTriggerFunctions].notFlag = not;
				numTriggerFunctions++;
				break;
			case FILTER_MCLASS:
				sscanf(p, "%9s %u", triggerType, &val);
				triggers[numTriggerFunctions].filterFunc = &filterMgmtClass;
				triggers[numTriggerFunctions].filterVal = val;
				triggers[numTriggerFunctions].ioctl = 0;
				triggers[numTriggerFunctions].notFlag = not;
				numTriggerFunctions++;
				break;
			case FILTER_PKEY:
				sscanf(p, "%9s 0x%x", triggerType, &val);
				triggers[numTriggerFunctions].filterFunc = &filterPKey;
				triggers[numTriggerFunctions].filterVal = val;
				triggers[numTriggerFunctions].ioctl = 0;
				triggers[numTriggerFunctions].notFlag = not;
				numTriggerFunctions++;
				break;
			case FILTER_PTYPE:
				sscanf(p, "%9s %9s", triggerType, strVal);
				if ((val = getPacketType(strVal)) == PACKETTYPE_ERR) {
					fprintf(stderr, "Invalid packet type %s\n", strVal);
					exit(1);
				}
				triggers[numTriggerFunctions].filterFunc = &filterPacketType;
				triggers[numTriggerFunctions].filterVal = val;
				triggers[numTriggerFunctions].ioctl = 0;
				triggers[numTriggerFunctions].notFlag = not;
				numTriggerFunctions++;
				break;
			case FILTER_SVCLEV:
				sscanf(p, "%9s %u", triggerType, &val);
				triggers[numTriggerFunctions].filterFunc = &filterServiceLevel;
				triggers[numTriggerFunctions].filterVal = val;
				triggers[numTriggerFunctions].ioctl = 0;
				triggers[numTriggerFunctions].notFlag = not;
				numTriggerFunctions++;
				break;
			case FILTER_ATTRID:
				sscanf(p, "%9s %9s", triggerType, strVal);
				if (strstr(strVal, "0x") != NULL) {
					sscanf(p, "%9s 0x%x", strVal, &val);
				} else {
					sscanf(p, "%9s %u", strVal, &val);
				}
				triggers[numTriggerFunctions].filterFunc = &filterAttrID;
				triggers[numTriggerFunctions].filterVal = val;
				triggers[numTriggerFunctions].ioctl = 0;
				triggers[numTriggerFunctions].notFlag = not;
				numTriggerFunctions++;
				break;
			case FILTER_QP:
				sscanf(p, "%9s %u", triggerType, &val);
				triggers[numTriggerFunctions].filterFunc = &filterQueuePair;
				triggers[numTriggerFunctions].filterVal = val;
				triggers[numTriggerFunctions].ioctl = 0;
				triggers[numTriggerFunctions].notFlag = not;
				numTriggerFunctions++;
				break;
			case FILTER_COND:
				if (not) {
					fprintf(stderr, "Not sign not allowed on condition\n");
					exit(1);
				}
				if (gotCondition) {
					fprintf(stderr, "Only one condition statement allowed\n");
					exit(1);
				}
				gotTriggerCondition = 1;
				sscanf(p, "%9s %9s", triggerType, strVal);
				if ((triggerCondition = getCondType(strVal)) < 0) {
					fprintf(stderr, "Invalid condition type %s\n", strVal);
					exit(1);
				}
				break;
		}
	}

	fclose(fp);

	return;
}

void growPacketTable()
{
	packet		*newPackets;
	int			i;
	uint32		newNumPackets;
	uint32		index;

	// grow by a quarter
	newNumPackets = numpackets * 1.25;
	newPackets = (packet *)malloc(newNumPackets * sizeof(packet));
	if (newPackets == NULL) {
		fprintf(stderr, "opapacketcapture: Error allocating new packets array: %s\n", strerror(errno));
		exit(1);
	}

	// copy in current table
	memcpy(newPackets, packets, numpackets * sizeof(packet));

	// adjust next pointers
	for (i = 0; i < (numpackets-1); i++) {
		index = (uint32)(packets[i].next - packets);
		newPackets[i].next = newPackets + index;
	}

	// initialize rest of table
	for (i = (numpackets - 1); i < (newNumPackets - 1); i++)
		newPackets[i].next = &newPackets[i+1];
	newPackets[newNumPackets-1].next = NULL;

	// reassign free/oldest/newest pointers
	index = (uint32)(freePackets - packets);
	freePackets = newPackets + index;
	index = (uint32)(oldestPacket - packets);
	oldestPacket = newPackets + index;
	index = (uint32)(newestPacket - packets);
	newestPacket = newPackets + index;

	// free old table and reset pointer 
	free(packets);
	packets = newPackets;
	numpackets = newNumPackets;

	return;
}

packet *getPacket()
{
	packet *ret;

	if (freePackets->next == NULL)
		growPacketTable();
	ret = freePackets;
	freePackets = freePackets->next;
	numPacketsTaken++;

	return(ret);
}

void returnPacket(packet *p)
{
	p->next = freePackets;
	freePackets = p;
	numPacketsTaken--;
	return;
}

void addNewPacket(packet *p)
{
	int done;
	packet *p1;
	packet *formerNewestPacket;
	int overlap;
	int myEndBlockNum = p->blockNum + p->numBlocks - 1;
	int oldestEndBlockNum;

	if (oldestPacket != NULL) {
		newestPacket->next = p;
		formerNewestPacket = newestPacket;
		newestPacket = p;
		done = 0;
		while (!done) {
			/* did this packet overwrite oldest? */
			overlap = 0;
			oldestEndBlockNum = oldestPacket->blockNum + oldestPacket->numBlocks - 1;
			if ((myEndBlockNum >= oldestPacket->blockNum) && (myEndBlockNum <= oldestEndBlockNum))	/* overwrote part of oldest */
				overlap = 2;
			else if ((p->blockNum <= oldestPacket->blockNum) && (myEndBlockNum >= oldestEndBlockNum))	/* new packet engulfs oldest */
				overlap = 3;
			if (overlap) {
				p1 = oldestPacket;
				oldestPacket = oldestPacket->next;
				returnPacket(p1);
			} else
				done = 1;
		}
		if (!applyFilters(p)) {
			p->numBlocks = 0;
			newestPacket = formerNewestPacket;
			newestPacket->next = NULL;
			returnPacket(p);
		} else
			numPacketsStored++;
		if (numTriggerFunctions && applyTriggers(p)) {
			if (!triggerSeen) {
				triggerSeen = 1;
				printf("\nopapacketcapture: Triggered by trigger file conditions\n");
			}
		}
	} else {
		oldestPacket = p;
		newestPacket = p;
		p->next = NULL;
		if (!applyFilters(p)) {
			oldestPacket = NULL;
			newestPacket = NULL;
			p->numBlocks = 0;
		} else
			numPacketsStored++;
		if (numTriggerFunctions && applyTriggers(p)) {
			if (!triggerSeen) {
				triggerSeen = 1;
				printf("\nopapacketcapture: Triggered by trigger file conditions\n");
			}
		}
	}


	return;
}

void advanceCurrentBlock(packet *p)
{

	currentBlock += (p->numBlocks * BLOCKSIZE);

	/* wrap if big packet will be too big */
	if ((blocksTableSize - (currentBlock - blocks)) < STL_MAX_PACKET_SIZE) {
		currentBlock = blocks;
	}
	return;
}

int initPcapHeader(int fd)
{
	pcapHdr_t			fileHdr;
	memset(&fileHdr, 0, sizeof(fileHdr));

	fileHdr.magicNumber =		STL_WIRESHARK_MAGIC;
	fileHdr.versionMajor =		STL_WIRESHARK_MAJOR;
	fileHdr.versionMinor =		STL_WIRESHARK_MINOR;
#if 0
	fileHdr.snapLen =			IB_PACKET_SIZE;
#else
	fileHdr.snapLen =			65535;
#endif
	fileHdr.networkType =		STL_WIRESHARK_ERF;

	return ((write(fd, &fileHdr, sizeof(fileHdr)) > 0) ? 0 : -1);
}

void writePCAP(int fd, uint64 pktLen, time_t sec, long nsec, uint8 *pkt)
{
	pcapRecHdr_t		pcapRec;
	extHeader_t			ext;
	WFR_SnC_HDR			*snc = (WFR_SnC_HDR *)pkt;
	WFR_SnC_HDR			wfrLiteSnc = {0};
	int i;
	int erfRecordLen;

	/* Adjust to keep 'nsec' less than 1 second */
	while (nsec >= 1E9L) {
		sec++;
		nsec -= 1E9L;
	}

	pcapRec.ts_sec =            sec;
	pcapRec.ts_nsec =           nsec;
	ext.flags =                 4; /* set variable length bit */
	ext.lossCtr =               0;
	ext.linkType =              ERF_TYPE_OPA_SNC;

	/* The high 32 bits of the timestamp contain the integer number of seconds
	 * while the lower 32 bits contain the binary fraction of the second.
	 * Unlike the rest of the ERF header this is little endian
	 */
	StoreLeU64((uint8*)&ext.ts, ((uint64) sec << 32) + (((uint64) nsec << 32) / 1000 / 1000 / 1000));

	if (IS_FI_MODE(mode)) {
		if (verbose > 1) {
			fprintf(stderr, "Direction:  %u\n", snc->Direction);
			fprintf(stderr, "PortNumber: %u\n", snc->PortNumber);
			fprintf(stderr, "PBC/RHF:    0x%"PRIx64"\n", snc->u.AsReg64);
			fprintf(stderr, "pktLen:     %"PRIu64"\n", pktLen);
		}
		ext.flags |= (snc->PortNumber & 0x1);
	} else {
		ext.flags |= 1;
		wfrLiteSnc.Direction = 2;
		wfrLiteSnc.PortNumber = 1;
		pktLen += sizeof(WFR_SnC_HDR);
	}

	erfRecordLen = pktLen + sizeof(extHeader_t);
	/* PCAP record length, including pseudoheaders(e.g.ERF) but not PCAP header */
	pcapRec.packetSize =                    erfRecordLen;
	/* Technically should be wire length + erf header length but no
	 * difference for OPA. Usually both set to ERF record length
	 * because packetSize < packetOrigSize should be true
	 */
	pcapRec.packetOrigSize =                erfRecordLen;
	/* Total ERF record length including header and any padding */
	ext.length =                            hton16(erfRecordLen);
	/* Packet wire length */
	ext.realLength =                        hton16(pktLen);

	if (write(fd, &pcapRec, sizeof(pcapRec)) < 0) {
		fprintf(stderr, "Failed to write PCAP packet header\n");
	}
	if (write(fd, &ext, sizeof(ext)) < 0) {
		fprintf(stderr, "Failed to write Ext packet header\n");
	}
	if (wfrLiteSnc.Direction == 2) {
		if (write(fd, &wfrLiteSnc, sizeof(WFR_SnC_HDR)) < 0) {
			fprintf(stderr, "Failed to write SnC packet header\n");
		}
		pktLen -= sizeof(WFR_SnC_HDR);
	}
	if (write(fd, pkt, pktLen) < 0) {
		fprintf(stderr, "Failed to write packet.\n");
	}

	if (verbose > 2) {
		fprintf(stderr, "TO PCAP: ");
		i=0;
		if (wfrLiteSnc.Direction == 2) {
			for (; i < sizeof(WFR_SnC_HDR); i++ ){
				if (i % 8 == 0) fprintf(stderr, "\n0x%04x ", i);
				fprintf(stderr, "%02x ", ((uint8 *)&wfrLiteSnc)[i] );
				if (i % 8 == 3) fprintf(stderr, " ");
			}
		}
		for (; i < pktLen; i++ ) {
			if (i % 8 == 0) fprintf(stderr, "\n0x%04x ", i);
			fprintf(stderr,"%02x ", pkt[i]);
			if (i % 8 == 3) fprintf(stderr, " ");
		}
		fprintf(stderr, "\n");
	}

	return;
}

void showPackets(packet *in) {
	packet *p;
	IB_LRH *pkt;
	IB_BTH *b;
	MAD_COMMON *m;
	char packetType[5];

	if (in) {
		p = in;
	} else {
		p = oldestPacket;
	}
	while (p != NULL) {
		pkt = (IB_LRH *)(&blocks[p->blockNum * BLOCKSIZE]);
		b = (IB_BTH *)((uint8 *)pkt + sizeof(IB_LRH));
		m = (MAD_COMMON *)((uint8 *)pkt + sizeof(IB_LRH) + sizeof(IB_BTH) + sizeof(IB_DETH));
		switch (b->OpCode>>5) {
			case 0:
				strcpy(packetType, "RC");
				break;
			case 1:
				strcpy(packetType, "UC");
				break;
			case 2:
				strcpy(packetType, "RD");
				break;
			case 3:
				strcpy(packetType, "UD");
				break;
			default:
				strcpy(packetType, "??");
				break;
		}
		printf("showpacket:dest lid %d\tsrc lid %d\tsvc lev %d\tmgmt class %d\tpkey is 0x%04x\ttype is %s\n", ntoh16(pkt->DestLID), ntoh16(pkt->SrcLID), pkt->l.ServiceLevel, m->MgmtClass, ntoh16(b->Pkey), packetType);
		if (in) {
			break;
		} else {
			p = p->next;
		}
	}

	return;
}

void writePacketData()
{
	packet	*p;
	int		fd;

	if (gotoutfile) {
		fd = open(out_file, O_RDWR|O_CREAT|O_TRUNC, 00644);
	} else {
		fd = open(PACKET_OUT_FILE, O_RDWR|O_CREAT|O_TRUNC, 00644);
	}
	if (fd < 0) {
		fprintf(stderr, "Error opening output file %s\n", strerror(errno));
		exit(1);
	}
	if (initPcapHeader(fd) < 0) {
		fprintf(stderr, "Error writing pcap header - %s\n", strerror(errno));
		exit(1);
	}

	p = oldestPacket;

	while (p != NULL) {
		writePCAP(fd, (unsigned short) p->size, p->ts_sec, p->ts_nsec, &blocks[p->blockNum * BLOCKSIZE]);
		p = p->next;
	}

	close(fd);

	return;
}
int debugtool_capture_device_filter(const struct dirent *d) {
	int hfi = -1;
	int port = -1;

	if (2 == sscanf(d->d_name,  "ipath_capture_%02d_%02d", &hfi, &port)) {
		return (hfi != -1 && port != -1 ? 1 : 0);
	}
	return 0;
}
int wfr_capture_device_filter(const struct dirent *d) {
	int gen = -1;
	int hfi = -1;

	if (2 == sscanf(d->d_name, "hfi%d_diagpkt%d", &gen, &hfi)) {
		return (gen == 1 && hfi != -1 ? 1 : 0);
	}
	return 0;
}
int all_capture_device_filter(const struct dirent *d) {
	int gen = -1;
	int hfi = -1;
	int port = -1;

	if (2 == sscanf(d->d_name, "hfi%d_diagpkt%d", &gen, &hfi)) {
		return ((gen == 1) && hfi != -1 ? 1 : 0);

	}
	if (2 == sscanf(d->d_name,  "ipath_capture_%02d_%02d", &hfi, &port)) {
		return (hfi != -1 && port != -1 ? 1 : 0);
	}
	return 0;
}

static char *modeToText(uint8 mode){
	switch (mode) {
	case 0: return "All";
	case DEBUG_TOOL_MODE: return "DebugTool";
	case WFR_MODE: return "WFR";
	default: return "Unknown";
	}
}

static void Usage(int exitcode)
{
	fprintf(stderr, "Usage: opapacketcapture [-o outfile] [-d devfile] [-f filterfile] [-t triggerfile] [-l triggerlag]\n");
	fprintf(stderr, "                          [-a alarm] [-p packets] [-s maxblocks] [-v [-v]]\n");
	fprintf(stderr, "            or\n");
	fprintf(stderr, "       opapacketcapture --help\n");
	fprintf(stderr, "   --help - produce full help text\n");
	fprintf(stderr, "   -o - output file for captured packets - default is "PACKET_OUT_FILE"\n");
	fprintf(stderr, "   -d - device file for capturing packets\n");
	fprintf(stderr, "   -f - filter file used for filtering - if absent, no filtering\n");
	fprintf(stderr, "   -t - trigger file used for triggering a stop capture - if absent, normal triggering \n");
	fprintf(stderr, "   -l - trigger lag: number of packets to collect after trigger condition met before dump and exit (default is 10)\n");
	fprintf(stderr, "   -a - number of seconds for alarm trigger to dump capture and exit\n");
	fprintf(stderr, "   -p - number of packets for alarm trigger to dump capture and exit\n");
	fprintf(stderr, "   -s - number of blocks to allocate for ring buffer (in Millions) [block = 64 Bytes] - default is 2 (128 MiB)\n");
//	fprintf(stderr, "   -m - protocol mode: 0=All, 1=DebugTool, 2=WFR; default is All\n");
	fprintf(stderr, "   -v - verbose output (Use verbose Level 1+ to show levels)\n");
	if (verbose) {
		fprintf(stderr, "        Level 1: Live Packet Count\n");
		fprintf(stderr, "        Level 2: Basic Packet read info \n");
		fprintf(stderr, "        Level 3: HEX Dump of packet going into output file\n");
		fprintf(stderr, "        Level 4: HEX Dump of data coming over snoop device\n");
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "To stop capture and trigger dump, kill with SIGINT or SIGUSR1.\n");
	fprintf(stderr, "Program will dump packets to file and exit\n");

	exit(exitcode);
}

int main (int argc, char *argv[])
{
	int	    	numRead;
	int	    	numDevs = 0;
	int		    i;
	int		    c;
	packet	    *newPacket;
	char	    strArg[64] = {0};
	const char  *opts="o:d:f:t:l:a:p:s:m:v";
	const struct option longopts[] = {{"help", 0, 0, '$'},
						{0, 0, 0, 0}};
	FILE	    *fp = NULL;
	int		    fd = 0;
	struct timespec ts = {0};
	uint64      numblocks = DEFAULT_NUMBLOCKS;
	unsigned    lasttime=0;
	struct dirent **d;
	char deviceCmpStr[80] = {0};

	while (-1 != (c = getopt_long(argc, argv, opts, longopts, NULL))) {
		switch (c) {
		case 'a':
			strncpy(strArg, optarg, sizeof(strArg)-1);
			strArg[sizeof(strArg)-1]=0;
			gotAlarmArg = 1;
			break;
		case 'p':
			if (FSUCCESS != StringToUint64(&numPacketsMax, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opapacketcapture: Invalid size: %s\n", optarg);
				Usage(2);
			}
			break;
		case 'm':
			if (FSUCCESS != StringToUint8(&mode, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opapacketcapture: Invalid mode: %s\n", optarg);
				Usage(2);
			}
			gotModeArg = 1;
			break;
		case 'f':
			strncpy(filterFileArg, optarg, sizeof(filterFileArg)-1);
			filterFileArg[sizeof(filterFileArg)-1]=0;
			gotFilterFileArg = 1;
			break;
		case 't':
			strncpy(triggerFileArg, optarg, sizeof(triggerFileArg)-1);
			triggerFileArg[sizeof(triggerFileArg)-1]=0;
			gotTriggerFileArg = 1;
			break;
		case 'd':
			strncpy(devfile, optarg, sizeof(devfile)-1);
			devfile[sizeof(devfile)-1]=0;
			gotdevfile = 1;
			break;
		case 'o':
			strncpy(out_file, optarg, sizeof(out_file)-1);
			out_file[sizeof(out_file)-1]=0;
			gotoutfile = 1;
			break;
		case 'l':
			sscanf(optarg, "%d", &triggerLag);
			gotTriggerLagArg = 1;
			break;
		case 's':
			if (FSUCCESS != StringToUint64(&numblocks, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opapacketcapture: Invalid size: %s\n", optarg);
				Usage(2);
			}
			numblocks *= (1024 * 1024);
			break;
		case 'v':
			verbose++;
			break;
		case '$':
			Usage(0);
		default:
			fprintf(stderr, "opapacketcapture: Invalid option -%c\n", c);
			Usage(2);
		}
	}

	if (gotAlarmArg) {
		char *p;
		int alarmMult = 1;
		if ((p = strchr(strArg, 'm')) != NULL) {
			alarmMult = 60;
		} else if ((p = strchr(strArg, 'h')) != NULL) {
			alarmMult = 60*60;
		} else if ((p = strchr(strArg, 'd')) != NULL) {
			alarmMult = 60*60*24;
		}
		sscanf(strArg, "%u", &alarmArg);
		alarmArg *= alarmMult;
	}

	/* Scan "/dev/" directory for capture devices */
	if (gotModeArg) {
		switch (mode) {
		case DEBUG_TOOL_MODE:
			numDevs = scandir("/dev/", &d, debugtool_capture_device_filter, alphasort);
			break;
		case WFR_MODE:
			numDevs = scandir("/dev/", &d, wfr_capture_device_filter, alphasort);
			break;
		default:
			numDevs = scandir("/dev/", &d, all_capture_device_filter, alphasort);
		}
	} else {
		numDevs = scandir("/dev/", &d, all_capture_device_filter, alphasort);
	}
	if (numDevs == 0) {
		fprintf(stderr, "opapacketcapture: No capture devices found on system: Mode %s\n", modeToText(mode));
		exit(1);
	}

	/* Check if supplied devfile matches one of the possible found devices */
	if (gotdevfile) {
		boolean isFound = FALSE;
		for (i = 0; i < numDevs; i++) {
			snprintf(deviceCmpStr, sizeof(deviceCmpStr), "/dev/%s", d[i]->d_name);
			if (strncmp(devfile, deviceCmpStr, sizeof(deviceCmpStr)) == 0) {
				isFound = TRUE;
				break;
			}
		}
		if (!isFound) {
			fprintf(stderr, "opapacketcapture: Error %s does not match 1 of %u devices found on system: mode %s\n", devfile, numDevs, modeToText(mode));
			for (i = 0; verbose && i < numDevs; i++) {
				fprintf(stderr, "  /dev/%s\n", d[i]->d_name);
			}
			exit(1);
		}
	} else if (numDevs > 1) {
		fprintf(stderr, "opapacketcapture: Error %u devices found on system, please choose one: mode %s\n", numDevs, modeToText(mode));
		for (i = 0; verbose && i < numDevs; i++) {
			fprintf(stderr, "  /dev/%s\n", d[i]->d_name);
		}
		exit(1);
	} else {
		snprintf(devfile, sizeof(devfile), "/dev/%s", d[0]->d_name);
	}

	/* now that devfile is known try to determine operating mode */
	if (!gotModeArg) {
		int gen = -1;
		int hfi = -1;
		int port = -1;
		if (2 == sscanf(devfile, "/dev/hfi%d_diagpkt%d", &gen, &hfi)) {
			switch (gen) {
			case 1:
				mode = WFR_MODE;
				break;
			default:
				fprintf(stderr, "opapacketcapture: Error could not determine operating mode from devfile: %s\n", devfile);
				exit(1);
			}
		} else if (2 == sscanf(devfile, "/dev/ipath_capture_%02d_%02d", &hfi, &port) && (hfi != -1 && port != -1)) {
			mode = DEBUG_TOOL_MODE;
		} else {
			fprintf(stderr, "opapacketcapture: Error could not determine operating mode from devfile: %s\n", devfile);
			exit(1);
		}
	}

	printf("opapacketcapture: Capturing from %s using %s mode\n", devfile, modeToText(mode));

	blocksTableSize = BLOCKSIZE * numblocks;
	blocks = (uint8 *)malloc(blocksTableSize);
	if (blocks == NULL) {
		fprintf(stderr, "opapacketcapture: Error allocating blocks array: %s\n", strerror(errno));
		exit(1);
	}

	numpackets = (numblocks/3);
	packets = (packet *)malloc(numpackets * sizeof(packet));
	if (packets == NULL) {
		fprintf(stderr, "opapacketcapture: Error allocating packets array: %s\n", strerror(errno));
		exit(1);
	}
	for (i = 0; i < (numpackets-1); i++)
		packets[i].next = &packets[i+1];
	packets[numpackets-1].next = NULL;

	currentBlock = blocks;
	freePackets = packets;
	oldestPacket = NULL;
	newestPacket = NULL;
	stopcapture = 0;
	retryCount = 0;

	filterCondition = COND_TYPE_AND;

	signal(SIGINT, my_handler);
	signal(SIGUSR1, my_handler);
	signal(SIGALRM, my_handler);

	if (gotFilterFileArg)
		setupFilters(filterFileArg);

	if (gotTriggerFileArg)
		setupTriggers(triggerFileArg);

	/* open file */
	fdIn = open(devfile, O_RDONLY);
	
	if (fdIn < 0) {
		fprintf(stderr, "opapacketcapture: Unable to open: %s: mode %u: %s\n",
			devfile, mode, strerror(errno));
		free(packets);
		return -1;
	}

	if (alarmArg)
		alarm(alarmArg);

	if (writethrough) {
		unlink(out_file);
		fp = fopen(out_file, "a+");
		if (fp == NULL) {
			exit(1);
		}
		fd = fileno(fp);
		if (initPcapHeader(fd) < 0) {
			fprintf(stderr, "opapacketcapture: Error writing pcap header: %s\n", strerror(errno));
			exit(1);
		}

	}

	if (ioctlConfigured) {
		if (ioctl(fdIn, QIB_SNOOP_IOCSETFILTER, &filterCmd) < 0) {
			fprintf(stderr, "opapacketcapture: Error issuing ioctl to QIB driver to set filter: %s\n", strerror(errno));
			exit(1);
		}
	}

	if (!clock_getres(CLOCK_REALTIME, &ts)) {
		if (verbose)
			printf("opapacketcapture: Clock precision: %ldns\n", ts.tv_nsec);
		if (ts.tv_nsec != 1)
			fprintf(stderr, "opapacketcapture: Error clock precision not 1ns: %ldns\n", ts.tv_nsec );
	} else {
		fprintf(stderr, "opapacketcapture: Error getting clock precision: %s\n", strerror(errno));
		exit(1);
	}

	printf("opapacketcapture: Capturing packets using %llu MiB buffer\n", (long long unsigned int)blocksTableSize/(1024*1024));

	while (!stopcapture) {
		numRead = read(fdIn, (char *)currentBlock , STL_MAX_PACKET_SIZE);

		if (numRead == 0) {
			stopcapture = 1;
			continue;
		}

		if (numRead < 0) {
			if (errno == EAGAIN) {
				if (++retryCount <= 25)
					continue;
			}
			if (errno != EINTR)
				fprintf(stderr, "opapacketcapture: Error reading packet: %s\n", strerror(errno));
			stopcapture = 1;
			continue;
		}

		retryCount = 0;

		clock_gettime(CLOCK_REALTIME, &ts);
		numPacketsRead++;
		if (verbose > 1) {
			fprintf(stderr, "Packet %u", numPacketsRead);
		}
		if (verbose > 3) {
			fprintf(stderr," numRead: %u 0x%x", numRead, numRead);
			for ( i = 0; i < numRead; i++) {
				if (i % 8 == 0) fprintf(stderr, "\n0x%04x ", i);
				fprintf(stderr,"%02x ", currentBlock[i]);
				if (i % 8 == 3) fprintf(stderr, " ");
			}
		}
		if (verbose > 1) fprintf(stderr, "\n");

		/* get next packet and fill it */
		newPacket = getPacket();
		newPacket->blockNum = (currentBlock - blocks) / BLOCKSIZE;
		newPacket->size = numRead;
		newPacket->numBlocks = (numRead / BLOCKSIZE) + ((numRead % BLOCKSIZE) ? 1 : 0);
		newPacket->ts_sec = ts.tv_sec;
		newPacket->ts_nsec = ts.tv_nsec;
		newPacket->next = NULL;

		addNewPacket(newPacket);

		if (triggerSeen) {
			if (afterTriggerPackets++ == triggerLag)
				stopcapture = 1;
		}

		advanceCurrentBlock(newPacket);

		if (verbose == 1 && ts.tv_sec > lasttime+5) {
			lasttime = ts.tv_sec;
			printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b%10u packets", numPacketsRead);
			fflush(stdout);
		}

		if (numPacketsRead >= numPacketsMax) {
			stopcapture = 1;
		}
	}
	if (verbose)
		printf("\n");

	/* clear filter if nec. and close */
	if (ioctlConfigured) {
		if (ioctl(fdIn, QIB_SNOOP_IOCCLEARFILTER, NULL) < 0) {
			fprintf(stderr, "opapacketcapture: Error issuing ioctl to QIB driver to clear filter: %s\n", strerror(errno));
			exit(1);
		}
	}
	close(fdIn);

	/*showPackets(NULL);*/
	printf("Number of packets stored is %d\n", numPacketsStored);

	writePacketData();

	exit(0);
}
