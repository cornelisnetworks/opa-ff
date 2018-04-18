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
#include <dirent.h>
#include <errno.h>

/* work around conflicting names */

#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include "opamgt_sa_priv.h"
#include <iba/ibt.h>
#include "opaswcommon.h"
#include "zlib.h"

#define FW_RESP_WAIT_TIME		2000

#define NEED_TO_RETRY(madStatus)	((madStatus == VM_MAD_STATUS_NACK) || (madStatus == VM_MAD_STATUS_BUS_BUSY) || (madStatus == VM_MAD_STATUS_BUS_HUNG) || (madStatus == VM_MAD_STATUS_LOST_ARB) || (madStatus == VM_MAD_STATUS_TIMEOUT))

// globals

int						g_debugMode = 0;
int						g_verbose = 0;
int						g_quiet = 0;
int						g_gotHfi = 0;
int						g_gotPort = 0;
int						g_gotSecondary = 0;
int						g_fileParam = 0;
int						g_dirParam = 0;
int32					g_sleepParam = 0;
int32					g_usleepParam = 0;
int32					g_dataLenParam = 0;
int32					g_respTimeout = FW_RESP_WAIT_TIME;
int						g_compareEEPROMs = 0;

typedef uint8_t bool_t;

#define LIST_FILE_SUPPORTED 0

static void usage(char *app_name)
{
	fprintf(stderr, "usage: %s -t target\n", app_name);
#if LIST_FILE_SUPPORTED
	fprintf(stderr, " [-l list_file]");
#endif
	fprintf(stderr, " [-v|-q] [-h hfi] [-o port] [-m module] [-F]");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -t - target for which to verify fw\n");
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "   -l - file that holds a list of targets to update\n");
#endif
	fprintf(stderr, "   -v - verbose output\n");
	fprintf(stderr, "   -q - no output\n");
	fprintf(stderr, "   -h - hfi, numbered 1..n, 0= -o port will be a\n");
	fprintf(stderr, "        system wide port num (default is 0)\n");
	fprintf(stderr, "   -o - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "        is 1st active)\n");
	fprintf(stderr, "   -F - do secondary EEPROM (default is primary)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -o options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -o 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -o 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -o y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -o y  - HFI x, port y\n");

#if 0 /* non-documented options */
	fprintf(stderr, "   -s - delay in seconds between requests\n");
	fprintf(stderr, "   -u - delay in microseconds between requests\n");
	fprintf(stderr, "   -T - response timeout in microseconds\n");
	fprintf(stderr, "   -z - read data length in bytes\n");
	fprintf(stderr, "   -C - compare primary and secondary EEPROM, verify same\n");
#endif

	exit(2);
}

int main(int argc, char *argv[])
{
	char				*cmdName;
	char				*opts="CDFvqt:l:h:o:m:s:u:T:z:";
	char				*fwBuffer = NULL;
	char				*imageDesc = "primary";
	char				parameter[100];
	char				*p;
	uint8				initialBuffer[32];
	uint8				*data = NULL;
	int					fwSize = 0;;
	int					bufferSize;
	EUI64				destPortGuid = -1;
	int					c;
	uint8				hfi = 0;
	uint8				port = 0;
	IB_PATH_RECORD		path;
	uint16				sessionID=(uint16)-1;
	uint32				locationDescriptor;
	uint32				dataLen = 0;
	uint32				dataOffset;
	uint32				*u;
	uint32				crcEEPROM;
	uint32				crc;
	VENDOR_MAD			mad;
	FSTATUS				status = FSUCCESS;
	struct              omgt_port *omgt_port_session = NULL;

	// determine how we've been invoked
	cmdName = strrchr(argv[0], '/');			// Find last '/' in path
	if (cmdName != NULL) {
		cmdName++;								// Skip over last '/'
	} else {
		cmdName = argv[0];
	}

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

			case 's':
				if (FSUCCESS != StringToInt32(&g_sleepParam, optarg, NULL, 0, TRUE)
					|| g_sleepParam < 0) {
					fprintf(stderr, "%s: Error: Invalid delay value: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'u':
				if (FSUCCESS != StringToInt32(&g_usleepParam, optarg, NULL, 0, TRUE)
					|| g_usleepParam < 0) {
					fprintf(stderr, "%s: Error: Invalid delay value: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'z':
				if (FSUCCESS != StringToInt32(&g_dataLenParam, optarg, NULL, 0, TRUE)
					|| g_dataLenParam < 0) {
					fprintf(stderr, "%s: Error: Invalid data length value: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'T':
				if (FSUCCESS != StringToInt32(&g_respTimeout, optarg, NULL, 0, TRUE)
					|| g_respTimeout < 0) {
					fprintf(stderr, "%s: Error: Invalid delay value: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'F':
				g_gotSecondary = 1;
				imageDesc = "secondary";
				break;

			case 'C':
				g_compareEEPROMs = 1;
				break;

			default:
				usage(cmdName);
				break;

		}
	}

	// user has requested display of help
	if (argc == 1) {
		usage(cmdName);
	}

	if (-1 == destPortGuid) {
		fprintf(stderr, "%s: Error: Must specify a target GUID\n", cmdName);
		exit(1);
	}

	if (g_quiet && (g_debugMode || g_verbose)) {
		fprintf(stderr, "%s: Error: Can not specify both -q and -D|-v\n", cmdName);
		exit(1);
	}

	if (g_sleepParam && g_usleepParam) {
		fprintf(stderr, "%s: Error: Can not specify both -s and -u\n", cmdName);
		exit(1);
	}


	// read the file into the buffer

	// Get the path

	struct omgt_params params = {.debug_file = g_debugMode ? stderr : NULL};
	status = omgt_open_port_by_num(&omgt_port_session, hfi, port, &params);
	if (status != 0) {
		fprintf(stderr, "%s: Error: Unable to open fabric interface.\n", cmdName);
		exit(1);
	}

	if (getDestPath(omgt_port_session, destPortGuid, cmdName, &path) != FSUCCESS) {
		fprintf(stderr, "%s: Error: Failed to get destination path\n", cmdName);
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

	locationDescriptor = g_gotSecondary ? STL_PRR_SEC_EEPROM1_ADDR :
						 STL_PRR_PRI_EEPROM1_ADDR;
	dataOffset = 4;

	if (g_compareEEPROMs) {
		uint8 primaryBuf[STL_MAX_EEPROM_SIZE];
		uint8 secondaryBuf[STL_MAX_EEPROM_SIZE];
		uint32 crc1, crc2;
		int currentEEPROM;
		
		for (currentEEPROM = 0; currentEEPROM < STL_EEPROM_COUNT; currentEEPROM++) {

			status = opaswEepromRW(omgt_port_session, &path, sessionID, (void *)&mad, g_respTimeout, 
								   STL_MAX_EEPROM_SIZE, currentEEPROM * STL_MAX_EEPROM_SIZE, 
								   (uint8 *)primaryBuf, FALSE, FALSE);

			status = opaswEepromRW(omgt_port_session, &path, sessionID, (void *)&mad, g_respTimeout, 
								   STL_MAX_EEPROM_SIZE, currentEEPROM * STL_MAX_EEPROM_SIZE, 
								   (uint8 *)secondaryBuf, FALSE, TRUE);

			crc1 = crc32(0, (uint8 *)primaryBuf, STL_MAX_EEPROM_SIZE);
			crc2 = crc32(0, (uint8 *)secondaryBuf, STL_MAX_EEPROM_SIZE);
			if (crc1 != crc2) {
				printf("EEPROM #%u CRC mismatch\n", currentEEPROM+1);
			} else {
				printf("EEPROM #%u CRC match\n", currentEEPROM+1);
			}
		
		}

		/* yes, we are exiting with success via the error path */
		goto err_exit;
	}

	// first, read 16 bytes to access the firmware size

	status = sendI2CAccessMad(omgt_port_session, &path, sessionID, &mad, 
							  NOJUMBOMAD, MMTHD_GET, g_respTimeout, locationDescriptor, 16, 
							  dataOffset + CSS_HEADER_SIZE + 4, initialBuffer);
	if (status == FSUCCESS) {
		fwSize = ntoh32(*(uint32 *)&initialBuffer[0]) * 4;
		if (fwSize < 0) {
			fprintf(stderr, "%s: Error: Invalid firmware length: %d\n", cmdName, fwSize);
			status = FERROR;
			goto err_exit;
		} else {
			bufferSize = fwSize + CSS_HEADER_SIZE + 1024 + OPASW_BUFFER_PAD;
			fwBuffer = malloc(bufferSize);
			if (fwBuffer) {
				memset(fwBuffer, 0, sizeof(bufferSize));
				data = (uint8 *)fwBuffer;
			} else {
				fprintf(stderr, "%s: Error: Failed to allocate memory of length: %d\n", cmdName, fwSize);
				status = FERROR;
				goto err_exit;
			}
		}
	} else goto err_exit;

	dataLen = fwSize + CSS_HEADER_SIZE;
	data = (uint8 *)fwBuffer;
	dataOffset = 0;

	status = opaswEepromRW(omgt_port_session, &path, sessionID, &mad, g_respTimeout, 
						   dataLen + 4, dataOffset, data, FALSE, g_gotSecondary);
	if (!g_quiet && (status == FSUCCESS)) {
		printf("\n");
		fflush(stdout);
	}
	if (status == FSUCCESS) {
		u = (uint32 *)&fwBuffer[fwSize + CSS_HEADER_SIZE];
		crcEEPROM = *u;
		crc = crc32(0, (uint8 *)&fwBuffer[CSS_HEADER_SIZE + 4], fwSize - 4);
		if (crcEEPROM != crc) {
			printf("%s: invalid %s image found\n", cmdName, imageDesc);
			fprintf(stderr, "CRC mismatch: 0x%08x : 0x%08x\n", ntoh32(crc), ntoh32(crcEEPROM));
			status = FERROR;
		} else {
			if (g_verbose)
				fprintf(stderr, "CRC match: 0x%08x : 0x%08x\n", ntoh32(crc), ntoh32(crcEEPROM));
			printf("%s: valid %s image found\n", cmdName, imageDesc);
		}
	}

err_exit:
	if (sessionID != (uint16)-1)
		releaseSession(omgt_port_session, &path, sessionID);

	omgt_close_port(omgt_port_session);

	if (status == FSUCCESS) {
		printf("opaswfwverify completed\n");
		exit(0);
	} else
		exit(1);

}
