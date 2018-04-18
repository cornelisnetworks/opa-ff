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
#include "iba/stl_helper.h"
#include "iba/stl_types.h"
#include "opamgt_sa_priv.h"
#include <iba/ibt.h>
#include "opaswcommon.h"
#include "opaswmetadata.h"

#define EEPROM_MASK		0x100
// defining valid values for fan tray and power supply
// TODO: update when actual information becomres available
#define MAX_FT	4
#define MIN_FT	1
#define MAX_PS	2
#define MIN_PS	1

//defining values for fan tray status
#define FAN_STATUS_0	0X01
#define FAN_STATUS_1	0X02
#define FAN_STATUS_2	0X04
#define FAN_STATUS_3	0X08
#define FAN_STATUS_4	0X10
#define FAN_STATUS_5	0X20
#define FAN_STATUS_TRAY1 (FAN_STATUS_0 | FAN_STATUS_1 | FAN_STATUS_2)
#define FAN_STATUS_TRAY2 (FAN_STATUS_3 | FAN_STATUS_4 | FAN_STATUS_5)

// globals

int						g_debugMode = 0;
int						g_verbose = 0;
int						g_quiet = 0;
int						g_gotHfi = 0;
int						g_gotPort = 0;
uint8					g_queryNum = 0;
int						g_gotModule = 0;
int8					g_intParam = 0;

#define LIST_FILE_SUPPORTED 0

static void usage(char *app_name)
{
	fprintf(stderr, "usage: %s -t target ", app_name);
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "[-l list_file] ");
#endif
	fprintf(stderr, "[-v|-q] [-h hfi] [-o port] -Q query-number [-i int]");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -t - target to query\n");
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "   -l - file that holds a list of targets to query\n");
#endif
	fprintf(stderr, "   -v - verbose output\n");
	fprintf(stderr, "   -q - no output\n");
	fprintf(stderr, "   -h - hfi, numbered 1..n, 0= -o port will be a\n");
	fprintf(stderr, "        system wide port num (default is 0)\n");
	fprintf(stderr, "   -o - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "        is 1st active)\n");
	fprintf(stderr, "   -Q - query number to run:\n");
	fprintf(stderr, "        2 - boot from primary or secondary EEPROM\n");
	fprintf(stderr, "        3 - display firwmare version\n");
	fprintf(stderr, "        4 - display VPD Info - comma-separated list\n");
	fprintf(stderr, "        5 - get node description\n");
	fprintf(stderr, "        6 - temperature readings\n");
	fprintf(stderr, "        7 - fan speed\n");
	fprintf(stderr, "        8 - power supply status - use with -i [%d-%d]\n", MIN_PS, MAX_PS);
	fprintf(stderr, "        9 - display asic version\n");
	fprintf(stderr, "       10 - display session ID\n");
	fprintf(stderr, "       11 - display basic configuration\n");
	fprintf(stderr, "       12 - board ID\n");
	fprintf(stderr, "   -i - integer parameter - use with queries 7,8\n");
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
	const char			*opts="Dvqt:l:h:o:m:Q:i:";
	char				parameter[100];
	char				*p;
	EUI64				destPortGuid = -1;
	int					c;
	int					i;
	uint8				hfi = 0;
	uint8				port = 0;
	IB_PATH_RECORD		path;
	uint16				sessionID;
	uint32				regValue;
	uint32				fanSpeed[OPASW_PSOC_FAN_CTRL_TACHS];
	char				tempStrs[I2C_OPASW_TEMP_SENSOR_COUNT][TEMP_STR_LENGTH];
	uint32				psStatus;
	uint8				fwVersion[40];
	vpd_fruInfo_rec_t	vpdInfo;
	char				mfgID[10];
	char				mfgDate[20];
	char				mfgTime[10];
	uint8				nodeDesc[80];
	opasw_ini_descriptor_get_t tableDescriptors;
	table_parsed_data_t	*portParsedDataTable=NULL;
	uint32				numPorts;
	uint32				portEntrySize;
	uint8				memoryData[200];
	uint8				boardID;
	VENDOR_MAD			mad;
	FSTATUS				status = FSUCCESS;
	uint32				asicVersion;
	uint8				chipStep;
	uint8				chipRev;



	table_parsed_data_t	*portPtrs=NULL;
	uint32				portLinkWidthSupportedIndex;
	uint32				portLinkSpeedSupportedIndex;
	uint32				portFMEnabledIndex;
	uint32				portLinkCRCModeIndex;
	uint32				portvCUIndex;
	uint32				portExternalLoopbackAllowedIndex;
	char				portLinkCRCModeValue[35];
	char				portLinkWidthSupportedText[20];
	char				portLinkSpeedSupportedText[20];
	struct              omgt_port *omgt_port_session = NULL;

	uint8				fanStatus;

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
				parameter[sizeof(parameter)-1] = 0;
				if ((p = strchr(parameter, ',')) != NULL) {
					*p = '\0';
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

			case 'Q':
				if (FSUCCESS != StringToUint8(&g_queryNum, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid Query Number: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'i':
				if (FSUCCESS != StringToInt8(&g_intParam, optarg, NULL, 0, TRUE)
					|| g_intParam < 0) {
					fprintf(stderr, "%s: Error: Invalid integer parameter: %s\n", cmdName, optarg);
					usage(cmdName);
				}
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

	if (g_queryNum == 0) {
		fprintf(stderr, "%s: Error: must enter a query number\n", cmdName);
		exit(1);
	}

	if ((g_queryNum == 8) && ((g_intParam > MAX_PS) || (g_intParam < MIN_PS))) {
		fprintf(stderr, "%s: Error: Query number 8 - must use -i for valid Power Supply number %d - %d\n", cmdName, MIN_PS, MAX_PS);
		usage(cmdName);
	}

	if (g_quiet && (g_debugMode || g_verbose)) {
		fprintf(stderr, "%s: Error: Can not specify both -q and -D|-v\n", cmdName);
		exit(1);
	}

	// Get the path

	struct omgt_params params = {.debug_file = (g_verbose || g_debugMode) ? stderr : NULL};
	status = omgt_open_port_by_num(&omgt_port_session, hfi, port, &params);
	if (status != 0) {
		fprintf(stderr, "%s: Error: Unable to open fabric interface.\n", cmdName);
		exit(1);
	}

	if (getDestPath(omgt_port_session, destPortGuid, cmdName, &path) != FSUCCESS) {
		fprintf(stderr, "%s: Error: Failed to get destination path\n", cmdName);
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

	switch (g_queryNum) {
		case 1:
			fprintf(stderr, "Error: Module type query no longer supported.\n");
			status = FERROR;
			break;

		case 2:
			status = sendRegisterAccessMad(omgt_port_session, &path, &mad, sessionID, 
										   (uint8)0x6f, &regValue, 1);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error: Failed to access register - status %d\n", status);
				break;
			}
			printf("Switch has booted from %s firmware image\n", (regValue & EEPROM_MASK) ? "failsafe" : "primary");
			break;

		case 3:
			status = getFwVersion(omgt_port_session, &path, &mad, sessionID, fwVersion);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error: Failed to acquire fw version - status %d\n", status);
				break;
			}
			printf("FW Version is %s\n", fwVersion);
			break;

		case 4:
			status = getBoardID(omgt_port_session, &path, &mad, sessionID, &boardID);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error, Failed to get board id - status %d\n",status);
				break;
			}
			status = getVPDInfo(omgt_port_session, &path, &mad, sessionID, OPASW_MODULE, boardID, &vpdInfo);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error: Failed to access vpd info - status %d\n", status);
				break;
			}
			snprintf(mfgID, sizeof(mfgID), "%02x%02x%02x", vpdInfo.mfgID[0] & 0xff, vpdInfo.mfgID[1] & 0xff, vpdInfo.mfgID[2] & 0xff);
			snprintf(mfgDate, sizeof(mfgDate), "%d-%02d-%d", vpdInfo.mfgMonth, vpdInfo.mfgDay, vpdInfo.mfgYear + 2000);
			snprintf(mfgTime, sizeof(mfgTime), "%02d:%02d", vpdInfo.mfgHours, vpdInfo.mfgMins);
			printf("VPD \"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
				vpdInfo.serialNum,
				vpdInfo.partNum,
				vpdInfo.model,
				vpdInfo.version,
				vpdInfo.mfgName,
				vpdInfo.productName,
				mfgID,
				mfgDate,
				mfgTime);
			break;

		case 5:
			status = getNodeDescription(omgt_port_session, &path, sessionID, nodeDesc);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error: Failed to acquire node description - status %d\n", status);
				break;
			}
			printf("Node description is %s\n", nodeDesc);
			break;
		case 6:
			status = getBoardID(omgt_port_session, &path, &mad, sessionID, &boardID);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error, Failed to get board id - status %d\n",status);
				break;
			}
			status = getTempReadings(omgt_port_session, &path, &mad, sessionID, tempStrs, boardID);

			for (i=0; i<I2C_OPASW_TEMP_SENSOR_COUNT; i++) {
				printf("SENSOR %d: %s ", i, tempStrs[i]);
			}
			printf("\n");
			if (status != FSUCCESS) {
				fprintf(stderr, "Error: Failed to get one or more temperature readings - status %s\n",
					iba_fstatus_msg(status & 0xFF));
			}
			break;
		case 7:
			status = getBoardID(omgt_port_session, &path, &mad, sessionID, &boardID);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error, Failed to get board id - status %d\n",status);
				break;
			}
			fanStatus =0;
			if (boardID == STL_PRR_BOARD_ID_HPE7K) {
				printf("FAN 0:NOTAVAIL ");
				printf("FAN 1:NOTAVAIL ");
				printf("FAN 2:NOTAVAIL ");
				printf("FAN 3:NOTAVAIL ");
				printf("FAN 4:NOTAVAIL ");
				printf("FAN 5:NOTAVAIL ");
				printf("\n");
			}
			else {
				for (i = 0; i < OPASW_PSOC_FAN_CTRL_TACHS; i++) {
					status = getFanSpeed(omgt_port_session, &path, &mad, sessionID,
									 (uint32)i, &fanSpeed[i]);
					if (status != FSUCCESS) {
						fprintf(stderr, "Error: Failed to get fan speed for fan %d - status %d\n", i, status);
						break;
					}
					if (g_verbose) {
						printf("Fan speed is %d\n", fanSpeed[i]);
					}
				// TODO: stl1baseboard.c only reports the speed itself, not FAST/SLOW/NORMAL, so I can't confirm that this matches
					if (fanSpeed[i] == 0 ) {
						fanStatus |= 1 << i;
					}
				}
				// print results
				if ((fanStatus & FAN_STATUS_TRAY1) == FAN_STATUS_TRAY1) {
					printf("FAN 0:NOTPRESENT ");
					printf("FAN 1:NOTPRESENT ");
					printf("FAN 2:NOTPRESENT ");
				}
				else {
					for (i=0; i<3; i++) {
						if (fanSpeed[i] > MAX_FAN_SPEED)
							printf("FAN %d:FAST ", i);
						else if (fanSpeed[i] < MIN_FAN_SPEED)
							printf("FAN %d:SLOW ", i);
						else
							printf("FAN %d:NORMAL ", i);
					}
				}// end else TRAY1
				if ((fanStatus & FAN_STATUS_TRAY2) == FAN_STATUS_TRAY2) {
					printf("FAN 3:NOTPRESENT ");
					printf("FAN 4:NOTPRESENT ");
					printf("FAN 5:NOTPRESENT ");
				}
				else {
					for (i=3; i<6; i++) {
						if (fanSpeed[i] > MAX_FAN_SPEED)
							printf("FAN %d:FAST ", i);
						else if (fanSpeed[i] < MIN_FAN_SPEED)
							printf("FAN %d:SLOW ", i);
						else
							printf("FAN %d:NORMAL ", i);
					}
				}// end els TRAY2

			printf("\n");
			}
			break;

		case 8:
			status = getPowerSupplyStatus(omgt_port_session, &path, &mad, sessionID, g_intParam, 
										  &psStatus);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error: Failed to get power supply status for ps %d - status %d\n", g_intParam, status);
				break;
			}
			switch (psStatus) {
				case PS_ONLINE:
					printf("PS %d: ONLINE\n", g_intParam);
					break;
				case PS_OFFLINE:
					printf("PS %d: OFFLINE\n", g_intParam);
					break;
				case PS_NOT_PRESENT:
					printf("PS %d: NOT PRESENT\n", g_intParam);
					break;
				case PS_INVALID:
					printf("PS %d: INVALID\n", g_intParam);
					break;
				default:  
					fprintf(stderr, "Error: Failed to get power supply status for ps %d\n", g_intParam); 
					break;
			}
			break;

		case 9:
			status = getAsicVersion(omgt_port_session, &path, &mad, sessionID, &asicVersion);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error: Failed to get ASIC version - status %d\n", status);
				break;
			}

			chipStep = (asicVersion & ASIC_CHIP_STEP_MASK) >> ASIC_CHIP_STEP_SHFT;
			chipRev = (asicVersion & ASIC_CHIP_REV_MASK) >> ASIC_CHIP_REV_SHFT;
			printf("ASIC Version: V");
			if (chipRev == 0) {
				switch (chipStep) {
					case ASIC_CHIP_STEP_A:
						printf("1\n");
						break;
					case ASIC_CHIP_STEP_B:
						printf("2\n");
						break;
					case ASIC_CHIP_STEP_C:
						printf("3\n");
						break;
					default:
						printf("0\n");
						break;
				}
			} else {
				printf("0\n");
			}
			break;

		case 10:
			printf("Session ID: %d\n", sessionID);
			break;
		case 11:
			{
				/* query port 2 */
				int dest_port_query=1;
				printf("Switch configuration values\n");
				status = sendIniDescriptorGetMad(omgt_port_session, &path, &mad, sessionID, &tableDescriptors);
				if (status != FSUCCESS) {
					fprintf(stderr, "%s: Error: Failed to get ini descriptors - status %d\n", cmdName, status);
					goto retErr;
				}
				numPorts = getNumPorts(omgt_port_session, &path, sessionID);

				if( numPorts <= 0){
					fprintf(stderr,"error in fetching port records\n");
					goto retErr;
				}
				portEntrySize = tableDescriptors.portDataLen / numPorts;
				status = sendMemAccessGetMad(omgt_port_session, &path, &mad, sessionID, tableDescriptors.portDataAddr + (dest_port_query * portEntrySize), portEntrySize*4, memoryData);
				if (status != FSUCCESS) {
					printf("Mem Access MAD Failed \n");
					goto retErr;
				}

				if (g_verbose) {
					printf("MemoryData dump:\n");
					opaswDisplayBuffer((char *)memoryData, portEntrySize * 4);
				}
				portParsedDataTable = malloc(tableDescriptors.portMetaDataLen * sizeof(table_parsed_data_t));
				if(portParsedDataTable == NULL)
				{
					fprintf(stderr,"Not enough memory \n");
					goto retErr;
				}
				status = parseDataTable(&portMetaData[0], memoryData, portMetaDataSize, portParsedDataTable, 0);
				if(status != FSUCCESS) {
					fprintf(stderr," failed: parseDataTable \n");
					goto retErr;
				}
				portPtrs = portParsedDataTable;
				portLinkWidthSupportedIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portMetaDataLen, "LINK_WIDTH_SUPPORTED");
				portLinkSpeedSupportedIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portMetaDataLen, "LINK_SPEED_SUPPORTED");
				portFMEnabledIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portMetaDataLen, "FM_ENABLED");
				portLinkCRCModeIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portMetaDataLen, "LTP_CRC_MODE_SUPPORTED");
				portvCUIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portMetaDataLen, "VCU");
				portExternalLoopbackAllowedIndex = getMetaDataIndexByField(&portMetaData[0], tableDescriptors.portMetaDataLen, "EXTERNAL_LOOPBACK_ALLOWED");

				status = getNodeDescription(omgt_port_session, &path, sessionID, nodeDesc);
				if (status != FSUCCESS) {
					fprintf(stderr, "Error: Failed to acquire node description - status %d\n", status);
					goto retErr;
				}
#define PRINT_REC(str,fmt,arg...)  printf("        %-*s : "fmt,35,str,arg);

				StlLinkWidthToText(portPtrs[portLinkWidthSupportedIndex].val.intVal, portLinkWidthSupportedText, 20);
				StlLinkSpeedToText(portPtrs[portLinkSpeedSupportedIndex].val.intVal, portLinkSpeedSupportedText, 20);

				PRINT_REC("Link Width"," %s\n", portLinkWidthSupportedText);
				PRINT_REC("Link Speed"," %s\n", portLinkSpeedSupportedText);
				PRINT_REC("FM Enabled"," %s\n", portPtrs[portFMEnabledIndex].val.intVal ? "Yes" : "No");
				PRINT_REC("Link CRC Mode"," %s\n", StlPortLtpCrcModeVMAToText(portPtrs[portLinkCRCModeIndex].val.intVal,portLinkCRCModeValue,sizeof(portLinkCRCModeValue)));
				PRINT_REC("vCU"," %d\n", portPtrs[portvCUIndex].val.intVal);
				PRINT_REC("External Loopback Allowed"," %s\n", portPtrs[portExternalLoopbackAllowedIndex].val.intVal ? "Yes" : "No");
				PRINT_REC("Node Description"," %s\n", strlen((const char *)nodeDesc)==0?(const char *)"no description":(char *)nodeDesc);
retErr:
				if(portParsedDataTable)
					free(portParsedDataTable);
				break;
			}
		case 12:
			status = getBoardID(omgt_port_session, &path, &mad, sessionID, &boardID);
			if (status != FSUCCESS) {
				fprintf(stderr, "Error: Failed to get board id - status %d\n", status);
				break;
			}
			printf("BoardID: 0x%02x\n", boardID);
			break;

		default:
			fprintf(stderr, "Error: Invalid query number %d\n", g_queryNum);
			releaseSession(omgt_port_session, &path, sessionID);
			usage(cmdName);
			break;
	}

	releaseSession(omgt_port_session, &path, sessionID);

	printf("opaswquery completed\n");

err_exit:
	omgt_close_port(omgt_port_session);

	if (status == FSUCCESS)
		exit(0);
	else
		exit(1);

}
