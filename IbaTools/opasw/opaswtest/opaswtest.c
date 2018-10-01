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
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>

/* work around conflicting names */

#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include "opamgt_sa_priv.h"
#include <iba/ibt.h>
#include "opaswcommon.h"
#include "opaswmetadata.h"

// globals

int						g_debugMode = 0;
int						g_verbose = 0;
int						g_quiet = 0;
int						g_gotHfi = 0;
int						g_gotPort = 0;
uint8					g_testNum = 0;
int						g_promptForPassword = 0;

#define EEPROMSIZE		(64 * 1024)

#define LIST_FILE_SUPPORTED 0

static void usage(char *app_name)
{
	fprintf(stderr, "usage: %s -t target ", app_name);
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "[-l list_file] ");
#endif
	fprintf(stderr, "[-v|-q] [-h hfi] [-o port] -r testnum");
	fprintf(stderr, " [-K] [-w password]");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -t - target to test\n");
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "   -l - file that holds a list of targets to test\n");
#endif
	fprintf(stderr, "   -v - verbose output\n");
	fprintf(stderr, "   -q - no output\n");
	fprintf(stderr, "   -h - hfi, numbered 1..n, 0= -o port will be a\n");
	fprintf(stderr, "        system wide port num (default is 0)\n");
	fprintf(stderr, "   -o - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "        is 1st active)\n");
	fprintf(stderr, "   -r - test number to run\n");
	fprintf(stderr, "        1  - simple sys table access\n");
	fprintf(stderr, "        2  - fw version\n");
	fprintf(stderr, "        3  - ini descriptors, sys table\n");
	fprintf(stderr, "        4  - address access (prompt)(disabled)\n");
	fprintf(stderr, "        5  - module type\n");
	fprintf(stderr, "        6  - reg 0x6f - which eeprom switch booted from\n");
	fprintf(stderr, "        7  - port table access\n");
	fprintf(stderr, "        8  - port table access - all ports\n");
	fprintf(stderr, "        9  - VPD access\n");
	fprintf(stderr, "        10 - save config\n");
	fprintf(stderr, "        11 - fan status\n");
	fprintf(stderr, "        12 - power supply status\n");
	fprintf(stderr, "        13 - set MTU size to 4K\n");
	fprintf(stderr, "        14 - bus walk\n");
	fprintf(stderr, "        15 - address 0x1040 - v0 or v1\n");
	fprintf(stderr, "        16 - explore EEPROM trailer & config\n");
	fprintf(stderr, "        17 - get board id\n");
	fprintf(stderr, "        18 - dump primary FW EEPROMs\n");
	fprintf(stderr, "        19 - probe mgmt fpga on EDF\n");
	fprintf(stderr, "        20 - asic rev\n");
	fprintf(stderr, "        21 - VMA I2C read using arbitrary Location, length, and offset\n");
	fprintf(stderr, "        22 - VMA I2C write using arbitrary Location, offset, and data bytes\n");
	fprintf(stderr, "   -K - enforce password based security for all operations - prompts for password\n");
	fprintf(stderr, "   -w - enforce password based security for all operations - password on command line\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -o options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -o 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -o 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -o y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -o y  - HFI x, port y\n");

	exit(2);
}

int main(int argc, char *argv[])
{
	char				*cmdName;
	char				*opts="DvqKt:l:h:o:w:r:";
	char				parameter[100];
	char				buf[20];
	char				*pp;
	EUI64				destPortGuid = -1;
	int					c;
	uint32				moduleType;
	uint32				numPorts;
	uint8				hfi = 0;
	uint8				port = 0;
	IB_PATH_RECORD		path;
	uint16				sessionID = 0;
	uint8				sysTableIndex;
	uint8				dataLen;
	uint8				sysTableData[200];
	uint8				memoryData[200];
	uint8				fwVersion[40];
	uint8				*p;
	int					i;
	uint32				location;
#if 0
	uint32				tableIdx;
	table_meta_data_t	*tp;
#endif
	opasw_ini_descriptor_get_t tableDescriptors;
	table_parsed_data_t	*sysParsedDataTable;
	table_parsed_data_t	*portParsedDataTable;
	table_parsed_data_t	**portPtrs;
	uint32				portEntrySize;
	uint32				regValue;
	VENDOR_MAD			mad;
	FSTATUS				status = FSUCCESS;
	uint32				portNumMetaIndex;
	uint32				portSpeedMetaIndex;
	uint32				portMTUMetaIndex;
	uint32				portVLCAPMetaIndex;
	uint32				portLinkWidthMetaIndex;
	uint32				portLinkSpeedMetaIndex;
	uint8				vpdBuffer[300];
	uint8				*v = vpdBuffer;
	uint32				vpdOffset = 0;
	uint32				vpdReadSize;
	uint32				vpdBytesRead = 0;
#if 0
	uint32				vpdAddress;
#endif
	vpd_fruInfo_rec_t	fruInfo;
	uint8				*dp;
	int					len;
	uint8				statusBuffer[64];
	uint32				mtuCap;
	uint32				vlCap;
	uint16				asicVal;
	uint32				asicVal2;
	uint16				dataOffset;
	uint32				dataSize;
	uint16				readLen;
	uint16				bytesRecd;
	uint8				dataBuffer[EEPROMSIZE];
	uint8				bigDataBuffer[4 * EEPROMSIZE];
	uint8				*data;
	uint16				boardID;
	struct              omgt_port *omgt_port_session = NULL;
	FILE				*fp;
	char				dumpFileName[64];

	// determine how we've been invoked
	cmdName = strrchr(argv[0], '/');			// Find last '/' in path
	if (cmdName != NULL) {
		cmdName++;								// Skip over last '/'
	} else {
		cmdName = argv[0];
	}

	// Initialize


	// parse options and parameters
	while (-1 != (c = getopt(argc, argv, opts))) {
		switch (c) {
			case 'D':
				g_debugMode = 1;
				break;

			case 't':
				errno = 0;
				strncpy(parameter, optarg, sizeof(parameter)-1);
				if ((pp = strchr(parameter, ',')) != NULL) {
					*pp = '\0';
				}
				if (FSUCCESS != StringToUint64(&destPortGuid, parameter, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid GUID: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'l':
#if !LIST_FILE_SUPPORTED
				fprintf(stderr, "Error: l option is not supported at this time\n");
				exit(1);
#endif
				break;

			case 'v':
				g_verbose = 1;
				break;

			case 'q':
				g_quiet = 1;
				break;

			case 'h':
				if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid HFI Number: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				g_gotHfi = 1;
				break;

			case 'o':
				if (FSUCCESS != StringToUint8(&port, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid Port Number: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				g_gotPort = 1;
				break;

			case 'r':
				if (FSUCCESS != StringToUint8(&g_testNum, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid Port Number: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'K':
				g_promptForPassword = 1;
				break;

			case 'w':
				fprintf(stderr, "Error: w option is not supported at this time\n");
				exit(1);
				break;

			default:
				usage(cmdName);
				break;

		}
	}

	// user has requested display of help
	if (argc == 1) {
		usage(cmdName);
		exit(0);
	}

	if (-1 == destPortGuid) {
		fprintf(stderr, "%s: Error: Must specify a target GUID\n", cmdName);
		exit(1);
	}

	if (g_testNum == 0) {
		fprintf(stderr, "%s: Error: must enter a test number\n", cmdName);
		exit(1);
	}

	if (g_quiet && (g_debugMode || g_verbose)) {
		fprintf(stderr, "%s: Error: Can not specify both -q and -D|-v\n", cmdName);
		exit(1);
	}

#if 0
#if SECURITY_SUPPORTED
	if (g_promptForPassword) {
		c = vkey_prompt_user(vendorKey, 0);
		if (c) {
			printf("vendor key is : ");
			for (c = 0; c < sizeof(vendorKey); c++)
				printf("%02x ", vendorKey[c]);
			printf("\n");
		}
		else
			fprintf(stderr, "vkey_prompt_user failed\n");
	}
#endif
#endif

#if 0
	if (g_promptForPassword) {
		c = vkey_prompt_user(vendorKey, 0);
		if (c) {
			if (g_verbose) {
				printf("vendor key is : ");
				for (c = 0; c < sizeof(vendorKey); c++)
					printf("%02x ", vendorKey[c]);
				printf("\n");
			}
		}
		else {
			fprintf(stderr, "%s: Error generating vendor key\n", cmdName);
			exit(1);
		}
	} else
		memset(vendorKey, 0, MAX_VENDOR_KEY_LEN);
#endif

	// Get the LID

	struct omgt_params params = {.debug_file = (g_verbose || g_debugMode) ? stderr : NULL};
	status = omgt_open_port_by_num(&omgt_port_session, hfi, port, &params);
	if (status != 0) {
		fprintf(stderr, "%s: Error: Unable to open fabric interface.\n", cmdName);
		exit(1);
	}

	if (getDestPath(omgt_port_session, destPortGuid, cmdName, &path) != FSUCCESS) {
		fprintf(stderr, "%s: Error finding destination path\n", cmdName);
		status = FERROR;
		goto err_exit;
	}

	// Send a ClassPortInfo to see if the switch is responding

	status = sendClassPortInfoMad(omgt_port_session, &path, &mad);
	if (status != FSUCCESS) {
		fprintf(stderr, "%s: Error: Failed to send/rcv ClassPortInfo\n", cmdName);
		goto err_exit;
	}

	// Get a session ID

	sessionID = getSessionID(omgt_port_session, &path);
	if (sessionID == (uint16)-1) {
		fprintf(stderr, "%s: Error: Failed to obtain sessionID\n", cmdName);
		status = FERROR;
		goto err_exit;
	}

	// Send the test mad

	switch (g_testNum) {
		case 1:
#if GET
			sysTableIndex = 10;
#else
			sysTableIndex = 1;
#endif
			dataLen = 2;
			sysTableData[0] = 'A';
			status = sendSysTableAccessGetMad(omgt_port_session, &path, &mad, sessionID, sysTableIndex, dataLen, sysTableData);
			if (status != FSUCCESS) {
				fprintf(stderr, "%s: Error: Failed to access Sys table data - status %d\n", cmdName, status);
				if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
				goto err_exit;
			}
			break;

		case 2:
			status = getFwVersion(omgt_port_session, &path, &mad, sessionID, fwVersion);
#if 0
			printf("fw version is %s\n", fwVersion);
			opaswDisplayBuffer((char *)fwVersion, FWVERSION_SIZE);
#endif
			printf("FW Version is %s\n", fwVersion);
			break;

		case 3:
			status = sendIniDescriptorGetMad(omgt_port_session, &path, &mad, sessionID, &tableDescriptors);
			if (status != FSUCCESS) {
				fprintf(stderr, "%s: Error: Failed to get ini descriptors - status %d\n", cmdName, status);
				if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
				goto err_exit;
			}
			printf("Sys Meta Data               : addr 0x%08x len %d\n", tableDescriptors.sysMetaDataAddr, tableDescriptors.sysMetaDataLen);
			printf("Sys Data                    : addr 0x%08x len %d\n", tableDescriptors.sysDataAddr, tableDescriptors.sysDataLen);
			printf("Port Meta Data              : addr 0x%08x len %d\n", tableDescriptors.portMetaDataAddr, tableDescriptors.portMetaDataLen);
			printf("Port Data                   : addr 0x%08x len %d\n", tableDescriptors.portDataAddr, tableDescriptors.portDataLen);

			printf("parsing sys data:\n");
			sysParsedDataTable = malloc(tableDescriptors.sysDataLen * sizeof(table_parsed_data_t));
#if 0
			status = sendMemAccessGetMad(&path, &mad, sessionID, tableDescriptors.sysDataAddr, (uint8)200, memoryData);
#else
			status = sendMemAccessGetMad(omgt_port_session, &path, &mad, sessionID, tableDescriptors.sysDataAddr, (uint8)(tableDescriptors.sysDataLen * 4) + 8, memoryData);
#endif
			status = parseDataTable(&systemMetaData[0], memoryData, systemMetaDataSize, &sysParsedDataTable[0], 1);
			break;

		case 4:
#if 0
			// prompt for location
			printf("Enter location in hex: ");
			scanf("%x", &location);
			printf("Querying location 0x%x\n", location);
			status = sendMemAccessGetMad(&path, &mad, sessionID, location, (uint8)200, memoryData);

			for (tp = &systemMetaData[0], i = 0; i <= SYSMETATABLESIZE; tp++, i++) {
				int j, len;
				p = &memoryData[(tp->start)/8];
				printf("%s: ", tp->field);
				len = (tp->length)/8 ? (tp->length)/8 : 1;
				for (j = 0; j < len; j++) {
					printf("0x%02x ", *(p + j) & 0xff);
				} 
				printf("\n");
			}
#else
			fprintf(stderr, "Test 4 disabled\n");
#endif
			break;

#if 0
		case 5:
			moduleType = getModuleType(omgt_port_session, &path, sessionID);
			switch(moduleType) {
				case ENC_MOD_TYPE_OPASW:
					printf("module type of dlid %d is OPASW\n", path.DLID);
					break;

				case ENC_MOD_TYPE_VIPER:
					printf("module type of dlid %d is VIPER\n", path.DLID);
					break;

				default:
					printf("module type of dlid %d is unsupported\n", path.DLID);
					break;

			}
			break;
#endif

#define EEPROM_MASK		0x100
		case 6:
			status = sendRegisterAccessMad(omgt_port_session, &path, &mad, sessionID, 
										   (uint8)0x6f, &regValue, 1);
			printf("Reg value is 0x%08x\n", regValue);
			printf("Switch has booted from %s EEPROM\n", (regValue & EEPROM_MASK) ? "secondary" : "primary");
			break;

		case 7:
			status = sendIniDescriptorGetMad(omgt_port_session, &path, &mad, sessionID, &tableDescriptors);
			if (status != FSUCCESS) {
				fprintf(stderr, "%s: Error: Failed to get ini descriptors - status %d\n", cmdName, status);
				if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
				goto err_exit;
			}
			printf("Sys Meta Data               : addr 0x%08x len %d\n", tableDescriptors.sysMetaDataAddr, tableDescriptors.sysMetaDataLen);
			printf("Sys Data                    : addr 0x%08x len %d\n", tableDescriptors.sysDataAddr, tableDescriptors.sysDataLen);
			printf("Port Meta Data              : addr 0x%08x len %d\n", tableDescriptors.portMetaDataAddr, tableDescriptors.portMetaDataLen);
			printf("Port Data                   : addr 0x%08x len %d\n", tableDescriptors.portDataAddr, tableDescriptors.portDataLen);
			numPorts = getNumPorts(omgt_port_session, &path, sessionID);
			printf("numPorts is %d\n", numPorts);

#if 0
			portEntrySize = (tableDescriptors.portDataLen * 4) / numPorts;
#else
			portEntrySize = tableDescriptors.portDataLen / numPorts;
#endif
			printf("portEntrySize is %d\n", (int)portEntrySize);
			portPtrs = malloc(numPorts * sizeof(void *));
			for (i = 0; i < numPorts; i++) {
				status = sendMemAccessGetMad(omgt_port_session, &path, &mad, sessionID, tableDescriptors.portDataAddr + (i * portEntrySize), portEntrySize*4, memoryData);
				if (g_verbose) {
					printf("MemoryData dump:\n");
					opaswDisplayBuffer((char *)memoryData, portEntrySize * 4);
				}
				printf("\n**** parsing port data for port %d:\n", i + 1);
				portParsedDataTable = malloc(tableDescriptors.portDataLen * sizeof(table_parsed_data_t));
				status = parseDataTable(&portMetaData[0], memoryData, portMetaDataSize, &portParsedDataTable[0], 1);
				portPtrs[i] = portParsedDataTable;
			}
			portNumMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "PHYS_PORT_NUMBER");
			portSpeedMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "LINK_SPEED_SUPPORTED");
			portMTUMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "MTU_CAP");
			portVLCAPMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "VL_CAP");
			portLinkWidthMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "LINK_WIDTH_SUPPORTED");
			portLinkSpeedMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "LINK_SPEED_SUPPORTED");
			for (i = 0; i < numPorts; i++) {
				printf("Port %d: \n", i+1);
				printf("         physical port  %d\n", portPtrs[i][portNumMetaIndex].val.intVal);
				printf("         port speed     %d\n", portPtrs[i][portSpeedMetaIndex].val.intVal);
				printf("         port mtu cap   %d\n", portPtrs[i][portMTUMetaIndex].val.intVal);
				printf("         port vl cap    %d\n", portPtrs[i][portVLCAPMetaIndex].val.intVal);
				printf("         port lk wid    %d\n", portPtrs[i][portLinkWidthMetaIndex].val.intVal);
				printf("         port lk spd    %d\n", portPtrs[i][portLinkSpeedMetaIndex].val.intVal);
			}
			break;

		case 8:
			status = sendIniDescriptorGetMad(omgt_port_session, &path, &mad, sessionID, &tableDescriptors);
			if (status != FSUCCESS) {
				fprintf(stderr, "%s: Error: Failed to get ini descriptors - status %d\n", cmdName, status);
				if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
				goto err_exit;
			}
			numPorts = getNumPorts(omgt_port_session, &path, sessionID);
			printf("numPorts is %d\n", numPorts);

			status = sendMemAccessGetMad(omgt_port_session, &path, &mad, sessionID, tableDescriptors.portDataAddr, tableDescriptors.portDataLen * 4, memoryData);
			if (g_verbose) {
				printf("MemoryData dump:\n");
				opaswDisplayBuffer((char *)memoryData, tableDescriptors.portDataLen * 4);
			}
			p = memoryData;
			portPtrs = malloc(numPorts * sizeof(void *));

			for (i = 0; i < numPorts; i++) {
				portParsedDataTable = malloc(tableDescriptors.portDataLen * sizeof(table_parsed_data_t));
				status = parseDataTable(&portMetaData[0], p, portMetaDataSize, &portParsedDataTable[0], 0);
				p += tableDescriptors.portDataLen / numPorts * 4;
				portPtrs[i] = portParsedDataTable;
			}
			portNumMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "PHYS_PORT_NUMBER");
			for (i = 0; i < numPorts; i++) {
				printf("Port %d: physical port %d\n", i+1, portPtrs[i][portNumMetaIndex].val.intVal);
			}
			break;

		case 9:
#if 1
#define VPDREADSIZE 60
#define VPDEEPROMLOCATION 0x8160AA00
			status = FSUCCESS;
			while ((status == FSUCCESS) && (vpdBytesRead < 256)) {
				if ((256 - vpdBytesRead) < VPDREADSIZE) {
					vpdReadSize = 256 - vpdBytesRead;
				} else {
					vpdReadSize = VPDREADSIZE;
				}
				printf("sending read access to 0x%08x for size %d offset %d\n", 
					   VPDEEPROMLOCATION, vpdReadSize, vpdOffset);
				status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
										  NOJUMBOMAD, MMTHD_GET, 10000, VPDEEPROMLOCATION, vpdReadSize, 
										  vpdOffset, v);
				if (status == FSUCCESS) {
					v += vpdReadSize;
					vpdBytesRead += vpdReadSize;
					vpdOffset += vpdReadSize;
				} else {
					fprintf(stderr, "%s: Error sending MAD packet to switch\n", cmdName);
				}
			}
			if (status == FSUCCESS) {
				printf("vpd buffer:\n");
				opaswDisplayBuffer((char *)vpdBuffer, 256);
			}
			memset(&fruInfo, 0, sizeof(fruInfo));
#if 0
			dp = vpdBuffer + FRU_OFFSET;
#else
			dp = vpdBuffer;
			dp += DEVICE_HDR_SIZE;
			// Walk xinfo records until fruInfo record is found
			while (*dp != FRUINFO_TYPE)
			{
				len = *(dp + 2);
				dp += len;
			}
#endif
			// Advance to FRU GUID
			dp += (RECORD_HDR_SIZE + FRU_TYPE_SIZE + FRU_HANDLE_SIZE);
			// advance past length (for appropriate guid type) plus one (for type code itself)
			switch (*dp)
			{
				case 0:
					dp++;
					break;
				case 1:
					dp += (8 + 1);
					break;
				case 2:
					dp += (6 + 1);
					break;
				case 3:
					dp += (16 + 1);
					break;
				case 4:
					dp += (8 + 1);
					break;
				default:
					fprintf(stderr, "invalid GUID type %d\n", *dp);
					break;
			}

			// copy serial number
			len = *dp & LEN_MASK;
			memcpy(&fruInfo.serialNum, dp + 1, len);
			fruInfo.serialNum[len] = '\0';
			dp += len + 1;

			// copy part number
			len = *dp & LEN_MASK;
			memcpy(&fruInfo.partNum, dp + 1, len);
			fruInfo.partNum[len] = '\0';
			dp += len + 1;

			// Copy model
			len = *dp & LEN_MASK;
			memcpy(&fruInfo.model, dp + 1, len);
			fruInfo.model[len] = '\0';
			dp += len + 1;

			// Copy version
			len = *dp & LEN_MASK;
			memcpy(&fruInfo.version, dp + 1, len);
			fruInfo.version[len] = '\0';
			dp += len + 1;

			// Copy mfgName
			len = *dp & LEN_MASK;
			memcpy(&fruInfo.mfgName, dp + 1, len);
			fruInfo.mfgName[len] = '\0';
			dp += len + 1;

			// Copy productName
			len = *dp & LEN_MASK;
			memcpy(&fruInfo.productName, dp + 1, len);
			fruInfo.productName[len] = '\0';
			dp += len + 1;

			// Copy mfgID
			switch (*dp)
			{
				case 0:
					fruInfo.mfgID[0] = '\0';
					dp++;
					break;
				case 1:
				case 2:
					dp++;
#if 0 /* looks like mfgID is not least sig. byte first in the EEPROM, so copy in as is */
					for (i = 0; i < 3; i++)
						fruInfo.mfgID[i] = *(dp + 2 - i);
#else
					memcpy(&fruInfo.mfgID, dp, 3);
#endif
					dp += 3;
					break;
				default:
					fprintf(stderr, "invalid Manufacturer ID code %d\n", *dp);
					break;
			}

			// Parse Mfg Date/Time
			if (*dp & 0x80 ){
				fruInfo.mfgYear  = (*(dp + 3) << 2) + ((*(dp + 2) & 0xc0) >> 6);			/* least sig byte 1st: bits 31-22 */
				fruInfo.mfgMonth = (*(dp + 2) & 0x3c) >> 2;									/* least sig byte 1st: bits 21-18 */
				fruInfo.mfgDay   = ((*(dp + 2) & 0x03) << 3) + ((*(dp + 1) & 0xe0) >> 5);	/* least sig byte 1st: bits 17-13 */
				fruInfo.mfgHours = *(dp + 1) & 0x1f;										/* least sig byte 1st: bits 12-8  */
				fruInfo.mfgMins  = *dp & 0x3f;												/* least sig byte 1st: bits 5-0   */
			} else {
				fruInfo.mfgYear = 0;
				fruInfo.mfgMonth = 0;
				fruInfo.mfgDay = 0;
				fruInfo.mfgHours = 0;
				fruInfo.mfgMins = 0;
			}
			printf("VPD:\n");
			printf("      Serial Number:   %s\n", fruInfo.serialNum);
			printf("      Part Number  :   %s\n", fruInfo.partNum);
			printf("      Model        :   %s\n", fruInfo.model);
			printf("      Version      :   %s\n", fruInfo.version);
			printf("      Mfg Name     :   %s\n", fruInfo.mfgName);
			printf("      Product Name :   %s\n", fruInfo.productName);
			printf("      Mfg ID       :   %02x %02x %02x\n", fruInfo.mfgID[0] & 0xff,
																fruInfo.mfgID[1] & 0xff,
																fruInfo.mfgID[2] & 0xff);
			printf("      Mfg Date     :   %d-%02d-%d\n", fruInfo.mfgMonth, fruInfo.mfgDay, fruInfo.mfgYear + 2000);
			printf("      Mfg Time     :   %02d:%02d\n", fruInfo.mfgHours, fruInfo.mfgMins);
#else
			for (c = 0; c < 1; c++)
			{
				for (i = 128; i < 256; i+=128)
				{
					vpdAddress = (c << 24) | (i << 16) | 0xC400;
					status = sendI2CAccessMad(&path, sessionID, &mad, NOJUMBOMAD, MMTHD_GET, 10, vpdAddress, 60, 0, v);
					if (status == FSUCCESS) {
						fprintf(stderr, "%s: Success sending MAD packet to switch - addr 0x%08x\n", cmdName, vpdAddress);
						printf("vpd buffer:\n");
						opaswDisplayBuffer((char *)vpdBuffer, 60);
					}
					else
						fprintf(stderr, "%s: Error sending MAD packet to switch - addr 0x%08x\n", cmdName, vpdAddress);
				}
			}
#endif
			break;

		case 10:
			printf("Are you sure you want to send the save config mad??  ");
			fgets(buf, 20, stdin);
			if ((buf[0] == 'Y') || (buf[0] == 'y')) {
				printf("sending save config mad\n");
				status = sendSaveConfigMad(omgt_port_session, &path, &mad, sessionID);
				if (status != FSUCCESS) {
					fprintf(stderr, "%s: Error: Failed to send/rcv SaveConfig MAD\n", cmdName);
					status = FERROR;
				}
			}
			break;

		case 11:
#if 0
#define FT_PREFIX		0x8000
#else
#define FT_PREFIX		0x8180
#endif

#if 1
#define FT1LOCATION 	(FT_PREFIX << 16) | 0x96dd
#define FT2LOCATION 	(FT_PREFIX << 16) | 0x90de
#else
#define FT1LOCATION 	0x000196dd
#define FT2LOCATION 	0x000196de
#endif
			printf("reading fan status: FT1 0x%08x FT2 0x%08x\n", FT1LOCATION, FT2LOCATION);
			status = FSUCCESS;
			status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
									  NOJUMBOMAD, MMTHD_GET, 2000, FT1LOCATION, 4, 0, 
									  statusBuffer);
#if 0
sendI2CAccessMad(uint16 &path, uint16 sessionID, VENDOR_MAD *mad, uint8 jumbo, uint8 method, int timeout, uint32 locationDescriptor, uint16 dataLen, uint16 dataOffset, uint8 *data,
#endif
			if (status == FSUCCESS) {
				printf("Success reading 0x%08x - buffer:\n", FT1LOCATION);
				opaswDisplayBuffer((char *)statusBuffer, 4);
				printf("FAN Speed of FT1 = %d\n", statusBuffer[0] * 60);
			} else {
				fprintf(stderr, "%s: Error: Failed to send/rcv i2cAccess MAD\n", cmdName);
				status = FERROR;
			}
			if (status == FSUCCESS) {
				status = FSUCCESS;
				status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
										  NOJUMBOMAD, MMTHD_GET, 2000, FT2LOCATION, 4, 0, 
										  statusBuffer);
				if (status == FSUCCESS) {
					printf("Success reading 0x%08x - buffer:\n", FT2LOCATION);
					opaswDisplayBuffer((char *)statusBuffer, 4);
					printf("FAN Speed of FT2 = %d\n", statusBuffer[0] * 60);
				} else {
					fprintf(stderr, "%s: Error: Failed to send/rcv i2cAccess MAD\n", cmdName);
					status = FERROR;
				}
			}
			break;

#if 0
		case 12:
			printf("reading ps status: PS 0x%08x\n", PSLOCATION);
			status = FSUCCESS;
			status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
									  NOJUMBOMAD, MMTHD_GET, 2000, I2C_OPASW_PS_ADDR, 4, PS_MGMT_FPGA_OFFSET, 
									  statusBuffer);
			if (status == FSUCCESS) {
				uint16 val;
				char *c;
				printf("Success reading 0x%08x - buffer:\n", PSLOCATION);
				opaswDisplayBuffer((char *)statusBuffer, 4);
				val = *(uint16 *)statusBuffer;
#if 1
				val = ntoh16(val);
#endif
				printf("val is 0x%04x\n", val);
				c = (char *)&val;
				printf("val bytes 0x%02x 0x%02x\n", *c & 0xff, *(c + 1) & 0xff);

				printf("PS1_PRESENT         = %d\n", (val >> PSU1_MGMT_FPGA_BIT_PRESENT) & 0x1);
				printf("PS1_PWR_OK          = %d\n", (val >> PSU1_MGMT_FPGA_BIT_PWR_OK) & 0x1);
				printf("PS2_PRESENT         = %d\n", (val >> PSU2_MGMT_FPGA_BIT_PRESENT) & 0x1);
				printf("PS2_PWR_OK          = %d\n", (val >> PSU2_MGMT_FPGA_BIT_PWR_OK) & 0x1);
			} else {
				fprintf(stderr, "%s: Error: Failed to send/rcv i2cAccess MAD\n", cmdName);
				status = FERROR;
			}
			break;
#endif

		case 13:
			status = sendIniDescriptorGetMad(omgt_port_session, &path, &mad, sessionID, &tableDescriptors);
			if (status != FSUCCESS) {
				fprintf(stderr, "%s: Error: Failed to get ini descriptors - status %d\n", cmdName, status);
				if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
				goto err_exit;
			}
			printf("Sys Meta Data               : addr 0x%08x len %d\n", tableDescriptors.sysMetaDataAddr, tableDescriptors.sysMetaDataLen);
			printf("Sys Data                    : addr 0x%08x len %d\n", tableDescriptors.sysDataAddr, tableDescriptors.sysDataLen);
			printf("Port Meta Data              : addr 0x%08x len %d\n", tableDescriptors.portMetaDataAddr, tableDescriptors.portMetaDataLen);
			printf("Port Data                   : addr 0x%08x len %d\n", tableDescriptors.portDataAddr, tableDescriptors.portDataLen);
			numPorts = getNumPorts(omgt_port_session, &path, sessionID);
			printf("numPorts is %d\n", numPorts);
			portMTUMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "MTU_CAP");
			printf("portMTUMetaIndex is %d\n", portMTUMetaIndex);
			if (portMTUMetaIndex < 0) {
				fprintf(stderr, "%s: Error: can not find MTU in metaData table\n", cmdName);
				if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
				goto err_exit;
			}
			portVLCAPMetaIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portDataLen, "VL_CAP");
			printf("portVLCAPMetaIndex is %d\n", portVLCAPMetaIndex);
			if (portVLCAPMetaIndex < 0) {
				fprintf(stderr, "%s: Error: can not find VLCAP in metaData table\n", cmdName);
				if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
				goto err_exit;
			}
#if 1
			mtuCap = ntoh32(ENC_MTU_CAP_4096);
#else
			mtuCap = ntoh32(1);
#endif
			vlCap = ntoh32(ENC_VLCAP_VL0_3);
#if 0
			status = sendPortTableAccessSetMad(&path, &mad, sessionID, (uint8)portVLCAPMetaIndex, 1, 4, (uint8 *)&vlCap);
#endif
			for (i = 1; i <= numPorts; i++) {
				status = sendPortTableAccessSetMad(omgt_port_session, &path, &mad, sessionID, 
												   (uint8)portMTUMetaIndex, i, 4, (uint8 *)&mtuCap);
				if (status != FSUCCESS) {
					fprintf(stderr, "%s: Error: Failed to set port table - status %d\n", cmdName, status);
					if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
					goto err_exit;
				}
			}
			break;

		case 14:
#define EEPROMLOCATION 0x00000000
			for (i = EEPROMLOCATION; i <= EEPROMLOCATION + 0xffffffff; i++) {
				status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
										  NOJUMBOMAD, MMTHD_GET, 2000, i, 4, 0, statusBuffer);
				if (status == FSUCCESS) {
					printf("Success reading 0x%08x - buffer:\n", i);
					opaswDisplayBuffer((char *)statusBuffer, 4);
				} else {
					fprintf(stderr, "%s: Error: Failed to send/rcv i2cAccess MAD for loc 0x%08x\n", cmdName, i);
					status = FERROR;
				}
			}
			break;

		case 15:
			location = 0x1040;
			status = sendMemAccessGetMad(omgt_port_session, &path, &mad, sessionID, location, (uint8)200, memoryData);
			if (g_verbose) {
				printf("MemoryData dump:\n");
				opaswDisplayBuffer((char *)memoryData, 10);
			}
			p = memoryData;
			asicVal = *(uint16 *)p;
			printf("Data at 0x1040: 0x%02x 0x%02x\n", *p & 0xff, *(p + 1) & 0xff);
			printf("Asic val is %d\n", asicVal >> 16);
			break;

		case 16:
#if 1
#define DATALEN  (EEPROMSIZE / 2)
#else
#define DATALEN  (256 + 16)
#endif
printf("TEST 16!!!\n");
			memset(dataBuffer, 0, EEPROMSIZE);
			location = I2C_OPASW_PRI_EEPROM_ADDR;
			dataOffset = EEPROMSIZE - DATALEN;
			dataSize = DATALEN;
			data = dataBuffer;
			bytesRecd = 0;
			while (bytesRecd < dataSize) {
				if ((dataSize - bytesRecd) < 192)
					readLen = dataSize - bytesRecd;
				else
					readLen = 192;
				printf("reading loc 0x%08x offset %d len %d\n", location, dataOffset, readLen);
				status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
										  NOJUMBOMAD, MMTHD_GET, 2000, location, readLen, dataOffset, data);
				if (status == FSUCCESS) {
					data += readLen;
					bytesRecd += readLen;
					dataOffset += readLen;
				} else {
					fprintf(stderr, "%s: Error: Failed to send/rcv i2cAccess MAD for loc 0x%08x offset %d\n", cmdName, location, dataOffset);
					status = FERROR;
				}
				usleep(200);
			}
			opaswDisplayBuffer((char *)dataBuffer, dataSize);
			break;

		case 17:
			location = 0x80407200;
			data = dataBuffer;
			memset(dataBuffer, 0, EEPROMSIZE);
			status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
									  NOJUMBOMAD, MMTHD_GET, 2000, location, 4, 0x0b00, data);
			boardID = (uint16)data[0];
			printf("board ID is 0x%04x\n", boardID);
			printf("data back is 0x%02x 0x%02x 0x%02x 0x%02x\n", data[0], data[1], data[2], data[3]);
			break;

		case 18:
			data = bigDataBuffer;
			status = opaswEepromRW(omgt_port_session, &path, sessionID, &mad, 2000, 4 * EEPROMSIZE, 0, data, FALSE, FALSE);
			data = bigDataBuffer;
			for (i = 0; i < 4; i++) {
				sprintf(dumpFileName, "dumpEEPROM-%d", i);
				fp = fopen(dumpFileName, "w");
				fwrite(data, 1, EEPROMSIZE, fp);
				fclose(fp);
				data += EEPROMSIZE;
			}
			break;

		case 19:
			location = I2C_OPASW_MGMT_FPGA_ADDR;
			for (i = 0; i <= 0x22; i++) {
				if ((i == 0x10) ||
				    (i == 0x17) ||
				    (i == 0x02) ||
				    (i == 0x05) ||
				    (i == 0x0c) ||
				    (i == 0x0d) ||
				    (i == 0x0e) ||
				    (i == 0x0f) ||
				    (i == 0x11) ||
				    (i == 0x15) ||
				    (i == 0x1c) ||
				    (i == 0x1e) ||
				    (i == 0x1f) ||
				    (i == 0x20) ||
				    (i == 0x22) ||
				    (i == 0x18))
					continue;
				memset(dataBuffer, 0, EEPROMSIZE);
				memset(&mad, 0, sizeof(VENDOR_MAD));
				data = dataBuffer;
				printf("probing register 0x%x\n", i);
				dataOffset = (0xb << 8) | i;
				status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
									  NOJUMBOMAD, MMTHD_GET, 2000, location, 1, dataOffset, data);
				printf("returned 0x%x\n", dataBuffer[0] & 0xff);
			}
			break;

		case 20:
			status = sendMemAccessGetMad(omgt_port_session, &path, &mad, sessionID, 0x12d6, (uint8)4, memoryData);
			p = memoryData;
			asicVal2 = ntoh32(*(uint32 *)p);
			printf("Value for asic rev is 0x%08x\n", asicVal2);
			printf("Version is 0x%02x Chip name is %s step&rev %c%d \n", (asicVal2 & 0x00ff0000) >> 16, ((asicVal2 & 0x0000ff00) >> 8) == 2 ? "PRR" : "Unknown", ((asicVal2 & 0x000000f0) >> 4) + 'A', asicVal2 & 0x0000000f);
			break;

		case 21:
		{
			int rc;
			uint32 offset;
			char *in;
			// prompt for location and, optionally, offset, & length
			printf("Enter location in hex (speed:2,reserved:6,flags:4,bus:4,addr:8,slot:8), [offset & length]: ");
			rc = scanf("%m[^\n]", &in);
			if (rc > 0) {
				rc = sscanf(in, "%x %x %x", &location, &offset, &dataSize);
				if (rc >= 1) {
					if (rc < 3) {
						// defaults if user didn't specify
						dataSize = 1;
						offset = 0;
					}
					dataOffset = (uint16) offset;
					printf("Querying location 0x%08x, offset 0x%08x for %d bytes\n", location, dataOffset, dataSize);
					status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad,
											  NOJUMBOMAD, MMTHD_GET, 2000, location, dataSize, dataOffset, dataBuffer);
					if (status == FSUCCESS) {
						opaswDisplayBuffer((char *)dataBuffer, dataSize);
					} else {
						fprintf(stderr, "%s: Error: Failed to send/rcv i2cAccess MAD for loc 0x%08x offset %d\n", cmdName, location, dataOffset);
					}
				}
				free(in);
			}
			break;
		}

		case 22:
		{
			int rc;
			int i = 0;
			char *in;
			char *tok;
			char *ctxt;
			char *delim = " \t\n";
			uint32 tmp;
			dataOffset = 0;	// keep compiler happy
			printf("Enter location, offset, and data bytes (in hex): ");
			rc = scanf("%m[^\n]", &in);
			if (rc > 0) {
				tok = strtok_r(in, delim, &ctxt);
				while (NULL != tok) {
					rc = sscanf(tok, "%x", &tmp);
					if (rc > 0) {
						if (0 == i)
							location = tmp;
						else if (1 == i)
							dataOffset = (uint16)tmp;
						else {
							dataBuffer[i-2] = (uint8)tmp;
						}
						i++;
					} else {
						fprintf(stderr, "%s: Error: \"%s\" is not a hex number\n",cmdName, tok);
						status = FERROR;
						break;
					}
					tok = strtok_r(NULL, delim, &ctxt);
				}
				if (status == FSUCCESS) {
					if (i > 2) {
						// parsing was successful and we got location, offset, and at least 1 data byte
						dataSize = i-2;
						printf("Writing location 0x%08x, offset 0x%08x with %d bytes\n", location, dataOffset, dataSize);
						status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad,
												  NOJUMBOMAD, MMTHD_SET, 2000, location, dataSize, dataOffset, dataBuffer);
						if (status != FSUCCESS) {
							fprintf(stderr, "%s: Error: Failed to send/rcv i2cAccess MAD for loc 0x%08x offset %d\n", cmdName, location, dataOffset);
						}
					} else {
						fprintf(stderr, "%s: Error: Expected at least 3 tokens, only got %d\n", cmdName, i);
						status = FERROR;
					}
				}
				free(in);
			}
			break;
		}
		default:
			fprintf(stderr, "Invalid test number\n");
			usage(cmdName);
			break;
	}

#if GET
	opaswDisplayBuffer((char *)sysTableData, dataLen);
#endif

	if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);

	printf("opaswtest completed\n");

err_exit:
	if (omgt_port_session != NULL) {
		omgt_close_port(omgt_port_session);
	}

	if (status == FSUCCESS)
		exit(0);
	else
		exit(1);

}
