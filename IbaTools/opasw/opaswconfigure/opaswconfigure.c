/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015-2020, Intel Corporation

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

#define EEPROM_MASK		0x100

// globals


int						g_debugMode = 0;
int						g_verbose = 0;
int						g_quiet = 0;
int						g_gotHfi = 0;
int						g_gotPort = 0;
uint8					g_configNum = 0;
int						g_gotIntParam = 0;
int						g_gotStrParam = 0;
int						g_gotSave = 0;


#define LIST_FILE_SUPPORTED 0

static void usage(char *app_name)
{
	fprintf(stderr, "usage: %s -t target ", app_name);
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "[-l list_file] ");
#endif
	fprintf(stderr, "[-v|-q] [-h hfi] [-o port] -C config-option-number -S [-i integer-val] [-s string-val]");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -t - target to configure\n");
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "   -l - file that holds a list of targets to reset\n");
#endif
	fprintf(stderr, "   -v - verbose output\n");
	fprintf(stderr, "   -q - no output\n");
	fprintf(stderr, "   -h - hfi, numbered 1..n, 0= -o port will be a\n");
	fprintf(stderr, "        system wide port num (default is 0)\n");
	fprintf(stderr, "   -o - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "        is 1st active)\n");
	fprintf(stderr, "   -C - query number to run:\n");
	fprintf(stderr, "        1 - node description - -s for string value\n");
	fprintf(stderr, "        5 - link width supported - -i for integer value (1-1X, 2-2X, 3-2X_1X, 4-3X, 5-3_1X, 6-3X_2X, 7-3X_2X_1X, 8-4X, 9-4X_1X, 10-4X_2X, 11-4X_2X_1X, 12-4X_3X, 13-4X_3X_1X, 14-4X_3X_2X, 15-4X_3X_2X_1X)\n");
	fprintf(stderr, "        7 - link speed supported - -i for integer value (1-12G, 2-25G, 3-12G_25G)\n");
	fprintf(stderr, "        8 - FM Enabled - -i for integer value (0-disabled, 1-enabled)\n");
	fprintf(stderr, "        9 - Link CRC Mode - i for integer value (0-16b, 1-14b/16b, 2-48b/16b, 3-48b/14b/16b, 4-per lane/16b, 5-per lane/14b/16b, 6-per lane/48b/16b, 7-per lane/48b/14b/16b)\n");
	fprintf(stderr, "        10 - vCU - i for integer value\n");
#if 0 /* non-documented options */
	fprintf(stderr, "        11 - External loopback allowed\n");
#endif
/* remember to update opaswfwconfigure.c when adding new configuration items */
	fprintf(stderr, "   -S - save configuration\n");
	fprintf(stderr, "   -i - integer value parameter\n");
	fprintf(stderr, "   -s - string value parameter\n");
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
	const char			*opts="DvqSt:l:h:o:m:C:i:s:";
	char				parameter[100];
	char				strParameter[100];
	int32				integerParameter = 0;
	char				*p;
	EUI64				destPortGuid = -1;
	int					c;
	int					i;
	uint8				hfi = 0;
	uint8				port = 0;
	IB_PATH_RECORD		path;
	uint16				sessionID = 0;
	int32				metaIndex = -1;
	uint32				numPorts;
	uint32				linkWidth;
	uint32				linkSpeed;
	uint32				port1LinkWidth;
	uint32				fmEnabled;
	uint32				linkCRCMode;
	uint32				vCU;
	uint32				extLoopbackAllowed;
	VENDOR_MAD			mad;
	FSTATUS				status = FSUCCESS;
	uint8				nodeDescription[NODE_DESC_SIZE];
	struct              omgt_port *omgt_port_session = NULL;

	// determine how we've been invoked
	cmdName = strrchr(argv[0], '/');			// Find last '/' in path
	if (cmdName != NULL) {
		cmdName++;								// Skip over last '/'
	} else {
		cmdName = argv[0];
	}

	// Initialize

	strParameter[0] = '\0';


	// parse options and parameters
	while (-1 != (c = getopt(argc, argv, opts))) {
		switch (c) {
			case 'D':
				g_debugMode = 1;
				break;

			case 't':
				errno = 0;
				StringCopy(parameter, optarg, sizeof(parameter));
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

			case 'C':
				if (!g_configNum) {
					if (FSUCCESS != StringToUint8(&g_configNum, optarg, NULL, 0, TRUE)) {
						fprintf(stderr, "%s: Error: Invalid Config-option Number: %s\n", cmdName, optarg);
						usage(cmdName);
					}
				} else {
					fprintf(stderr, "%s: Error: Only one instance of -C allowed\n\n", cmdName);
					usage(cmdName);
				}
				break;

			case 's':
				g_gotStrParam = 1;
				StringCopy(strParameter, optarg, sizeof(strParameter));
				break;

			case 'i':
				g_gotIntParam = 1;
				if (FSUCCESS != StringToInt32(&integerParameter, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid integer parameter: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'S':
				g_gotSave = 1;
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

	if (g_configNum == 0) {
		fprintf(stderr, "%s: Error: must enter a configuration option number\n", cmdName);
		exit(1);
	}

	if (g_quiet && (g_debugMode || g_verbose)) {
		fprintf(stderr, "%s: Error: Can not specify both -q and -D|-v\n", cmdName);
		exit(1);
	}

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

	// Perform the config option

	switch (g_configNum) {
		case 1:
			if (!g_gotStrParam) {
				fprintf(stderr, "%s: Error: must enter a string parameter (-s) with configuration option 1\n", cmdName);
				status = FERROR;
			} else if (strlen(strParameter) > 63) {
				fprintf(stderr, "%s: Error: Invalid node description: %s\n", cmdName, strParameter);
				status = FERROR;
			} else {
				metaIndex = getMetaDataIndexByField(systemMetaData, systemMetaDataSize, "NODE_STRING");
			}
			if (metaIndex < 0) {
				fprintf(stderr, "%s: Error: can not find NODE_STRING in metaData table\n", cmdName);
				status = FERROR;
			}
			if (status == FSUCCESS) {
				strncpy((char*)nodeDescription, strParameter, NODE_DESC_SIZE);
				nodeDescription[NODE_DESC_SIZE-1]=0;
				status = sendSysTableAccessSetMad(omgt_port_session, &path, &mad, sessionID, (uint8)metaIndex, 
												  (uint8)NODE_DESC_SIZE, nodeDescription);
				if (status != FSUCCESS) {
					fprintf(stderr, "%s: Error: Failed to send/rcv SysTableAccessSet MAD\n", cmdName);
					status = FERROR;
				}
			}
			break;

		case 2:
			fprintf(stderr, "%s: Error: vendor key no longer supported\n", cmdName);
			status = FERROR;
			break;

		case 3:
			fprintf(stderr, "%s: Error: MTU Capability no longer supported\n", cmdName);
			status = FERROR;
			break;

		case 4:
			fprintf(stderr, "%s: Error: VL Capability no longer supported\n", cmdName);
			status = FERROR;
			break;

		case 5:
			if (!g_gotIntParam) {
				fprintf(stderr, "%s: Error: must enter a integer parameter (-i) with configuration option 5\n", cmdName);
				status = FERROR;
			} else if ((integerParameter < 1) || (integerParameter > 15)) {
				fprintf(stderr, "%s: Error: bad integer value %d; must be 1-1X, 2-2X, 3-2X_1X, 4-3X, 5-3_1X, 6-3X_2X, 7-3X_2X_1X, 8-4X, 9-4X_1X, 10-4X_2X, 11-4X_2X_1X, 12-4X_3X, 13-4X_3X_1X, 14-4X_3X_2X, 15-4X_3X_2X_1X\n", cmdName, integerParameter);
				status = FERROR;
			} else {
				metaIndex = getMetaDataIndexByField(&portMetaData[0], portMetaDataSize, "LINK_WIDTH_SUPPORTED");
				if (metaIndex < 0) {
					fprintf(stderr, "%s: Error: can not find LINK WIDTH SUPPORTED in metaData table\n", cmdName);
					status = FERROR;
				}
			}
			if (status == FSUCCESS) {
				numPorts = getNumPorts(omgt_port_session, &path, sessionID);
				linkWidth = integerParameter;
				// make sure that port1 always has 4x enabled
				port1LinkWidth = linkWidth | 0x8;

				linkWidth = ntoh32(linkWidth);
				port1LinkWidth = ntoh32(port1LinkWidth);

				// first set port 1
				status = sendPortTableAccessSetMad(omgt_port_session, &path, &mad, sessionID, 
												   (uint8)metaIndex, 1, 4, (uint8 *)&port1LinkWidth);
				if (status != FSUCCESS) {
					fprintf(stderr, "%s: Error: Failed to send/rcv PortTableAccessSet MAD for port %d\n", cmdName, 1);
					status = FERROR;
				}

				// now set ports 2 through numPorts
				for (i = 2; i <= numPorts; i++) {
					status = sendPortTableAccessSetMad(omgt_port_session, &path, &mad, sessionID, 
													   (uint8)metaIndex, i, 4, (uint8 *)&linkWidth);
					if (status != FSUCCESS) {
						fprintf(stderr, "%s: Error: Failed to send/rcv PortTableAccessSet MAD for port %d\n", cmdName, i);
						status = FERROR;
					}
				}
			}
			break;

		case 7:
			if (!g_gotIntParam) {
				fprintf(stderr, "%s: Error: must enter a integer parameter (-i) with configuration option 7\n", cmdName);
				status = FERROR;
			} else if ((integerParameter < 1) || (integerParameter > 3)) {
				fprintf(stderr, "%s: Error: bad integer value %d; must be 1 for 12G, 2 for 25G, 3 for 12G/25G\n", cmdName, integerParameter);
				status = FERROR;
			} else {
				metaIndex = getMetaDataIndexByField(&portMetaData[0], portMetaDataSize, "LINK_SPEED_SUPPORTED");
				if (metaIndex < 0) {
					fprintf(stderr, "%s: Error: can not find LINK SPEED SUPPORTED in metaData table\n", cmdName);
					status = FERROR;
				}
			}
			if (status == FSUCCESS) {
				numPorts = getNumPorts(omgt_port_session, &path, sessionID);
				linkSpeed = integerParameter;

				linkSpeed = ntoh32(linkSpeed);
				// set ports 1 through numPorts
				for (i = 1; i <= numPorts; i++) {
					status = sendPortTableAccessSetMad(omgt_port_session, &path, &mad, sessionID, 
													   (uint8)metaIndex, i, 4, (uint8 *)&linkSpeed);
					if (status != FSUCCESS) {
						fprintf(stderr, "%s: Error: Failed to send/rcv PortTableAccessSet MAD for port %d\n", cmdName, i);
						status = FERROR;
					}
				}
			}
			break;
		case 8:
			if (!g_gotIntParam) {
				fprintf(stderr, "%s: Error: must enter an integer parameter (-i) with configuration option 8\n", cmdName);
				status = FERROR;
			} else if ((integerParameter < 0) || (integerParameter > 1)) {
				fprintf(stderr, "%s: Error: bad integer value %d; must be 0 for disable or 1 for enable\n", cmdName, integerParameter);
				status = FERROR;
			} else {
				metaIndex = getMetaDataIndexByField(&portMetaData[0], portMetaDataSize, "FM_ENABLED");
				if (metaIndex < 0) {
					fprintf(stderr, "%s: Error: can not find FM ENABLED in metaData table\n", cmdName);
					status = FERROR;
				}
			}
			if (status == FSUCCESS) {
				numPorts = getNumPorts(omgt_port_session, &path, sessionID);
				fmEnabled = integerParameter;

				fmEnabled = ntoh32(fmEnabled);
				// set ports 1 through numPorts
				for (i = 1; i <= numPorts; i++) {
					status = sendPortTableAccessSetMad(omgt_port_session, &path, &mad, sessionID,
													   (uint8)metaIndex, i, 4, (uint8 *)&fmEnabled);
					if (status != FSUCCESS) {
						fprintf(stderr, "%s: Error: Failed to send/rcv PortTableAccessSet MAD for port %d\n", cmdName, i);
						status = FERROR;
					}
				}
			}
			break;
		case 9:	
			if (!g_gotIntParam) {
				fprintf(stderr, "%s: Error: must enter an integer parameter (-i) with configuration option 9\n", cmdName);
				status = FERROR;
			} else if ((integerParameter < 0) || (integerParameter > 7)) {
				fprintf(stderr, "%s: Error: bad integer value %d; must be 0-16b, 1-14b/16b, 2-48b/16b, 3-48b/14b/16b, 4-per lane/16b, 5-per lane/14b/16b, 6-per lane/48b/16b, 7-per lane/48b/14b/16b\n", cmdName, integerParameter);
				status = FERROR;
			} else {
				metaIndex = getMetaDataIndexByField(&portMetaData[0], portMetaDataSize, "LTP_CRC_MODE_SUPPORTED");
				if (metaIndex < 0) {
					fprintf(stderr, "%s: Error: can not find CRC in metaData table\n", cmdName);
					status = FERROR;
				}
			}
			if (status == FSUCCESS) {
				numPorts = getNumPorts(omgt_port_session, &path, sessionID);
				linkCRCMode = integerParameter;

				linkCRCMode = ntoh32(linkCRCMode);
				// set ports 1 through numPorts
				for (i = 1; i <= numPorts; i++) {
					status = sendPortTableAccessSetMad(omgt_port_session, &path, &mad, sessionID,
													   (uint8)metaIndex, i, 4, (uint8 *)&linkCRCMode);
					if (status != FSUCCESS) {
						fprintf(stderr, "%s: Error: Failed to send/rcv PortTableAccessSet MAD for port %d\n", cmdName, i);
						status = FERROR;
					}
				}
			}
			break;
		case 10:
			if (!g_gotIntParam) {
				fprintf(stderr, "%s: Error: must enter an integer parameter (-i) with configuration option 10\n", cmdName);
				status = FERROR;
			} else if (integerParameter < 0 || integerParameter > 7) { 
				fprintf(stderr, "%s: Error: bad integer value %d; Must be 0-7", cmdName, integerParameter);
				status = FERROR;
			} else {
				metaIndex = getMetaDataIndexByField(&portMetaData[0], portMetaDataSize, "VCU");
				if (metaIndex < 0) {
					fprintf(stderr, "%s: Error: can not find vCU in metaData table\n", cmdName);
					status = FERROR;
				}
			}
			if (status == FSUCCESS) {
				numPorts = getNumPorts(omgt_port_session, &path, sessionID);
				vCU = integerParameter;

				vCU = ntoh32(vCU);
				// set ports 1 through numPorts
				for (i = 1; i <= numPorts; i++) {
					status = sendPortTableAccessSetMad(omgt_port_session, &path, &mad, sessionID,
													   (uint8)metaIndex, i, 4, (uint8 *)&vCU);
					if (status != FSUCCESS) {
						fprintf(stderr, "%s: Error: Failed to send/rcv PortTableAccessSet MAD for port %d\n", cmdName, i);
						status = FERROR;
					}
				}
			}
			break;
		case 11:
			if (!g_gotIntParam) {
				fprintf(stderr, "%s: Error: must enter an integer parameter (-i) with configuration option 11\n", cmdName);
				status = FERROR;
			} else if (integerParameter < 0 || integerParameter > 1) {
				fprintf(stderr, "%s: Error: bad integer value %d; must be 0 for disable or 1 for enable\n", cmdName, integerParameter);
				status = FERROR;
			} else {
				metaIndex = getMetaDataIndexByField(&portMetaData[0], portMetaDataSize, "EXTERNAL_LOOPBACK_ALLOWED");
				if (metaIndex < 0) {
					fprintf(stderr, "%s: Error: can not find EXTERNAL LOOPBACK ALLOWED in metaData table\n", cmdName);
					status = FERROR;
				}
			}
			if (status == FSUCCESS) {
				numPorts = getNumPorts(omgt_port_session, &path, sessionID);
				extLoopbackAllowed = integerParameter;

				extLoopbackAllowed = ntoh32(extLoopbackAllowed);
				// set ports 1 through numPorts
				for (i = 1; i <= numPorts; i++) {
					status = sendPortTableAccessSetMad(omgt_port_session, &path, &mad, sessionID,
													   (uint8)metaIndex, i, 4, (uint8 *)&extLoopbackAllowed);
					if (status != FSUCCESS) {
						fprintf(stderr, "%s: Error: Failed to send/rcv PortTableAccessSet MAD for port %d\n", cmdName, i);
						status = FERROR;
					}
				}
			}
			break;
		default:
			if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
			fprintf(stderr, "Error: Invalid configuration option number %d\n", g_configNum);
			usage(cmdName);
			break;
	}

	if (g_gotSave) {
		// save configuration to make change permanent
		status = sendSaveConfigMad(omgt_port_session, &path, &mad, sessionID);
		if (status != FSUCCESS) {
			fprintf(stderr, "%s: Error: Failed to send/rcv SaveConfig MAD\n", cmdName);
			status = FERROR;
		}
	}

	if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);

	printf("opaswconfigure completed\n");

err_exit:
	omgt_close_port(omgt_port_session);

	if (status == FSUCCESS)
		exit(0);
	else
		exit(1);

}
