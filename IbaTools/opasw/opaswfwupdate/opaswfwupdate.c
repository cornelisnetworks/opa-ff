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
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* work around conflicting names */

#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include "opamgt_sa_priv.h"
#include <iba/ibt.h>
#include "opaswcommon.h"
#include "zlib.h"
#include <openssl/sha.h>

#define FW_FILENAME_BASE		"prrFw"

#define FW_RESP_WAIT_TIME		2000
#define FW_PAUSE				50000

typedef uint8_t bool_t;

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
int						g_resetSwitch = 0;

char					fwFileName[FNAME_SIZE];
char					inibinFileName[FNAME_SIZE];
char					fwDirName[DNAME_SIZE];

uint32 loadFirmwareBuffer(struct omgt_port *port, char **fwBuffer, IB_PATH_RECORD *path, uint16 sessionID)
{
	FILE *fp;
	DIR *fwDir;
	struct dirent *dp;
	int nread;
	uint32 totalRead = 0;
	uint32 bufSize;
	char *p;
	struct stat fileStat;
	uint32 oemHash[8],binaryHash[8];
	VENDOR_MAD                      mad;
	FSTATUS                         status = FSUCCESS;
	int				isValid = 1;
	uint32				acb;
	if (g_dirParam) {
		if (chdir(fwDirName) < 0) {
			fprintf(stderr, "Error: cannot change directory to %s: %s\n", fwDirName, strerror(errno));
			return(0);
		}
		if (stat("emfwMapFile", &fileStat) < 0) {
			if (errno == ENOENT) {
				if ((fwDir = opendir(fwDirName)) == NULL) {
					fprintf(stderr, "Error opening directory %s: %s\n", fwDirName, strerror(errno));
					return(0);
				}
				while ((dp = readdir(fwDir)) != NULL) {
					if (strstr(dp->d_name, FW_FILENAME_BASE) != NULL) {
						snprintf(fwFileName, FNAME_SIZE, "%s/%s", fwDirName, dp->d_name);
						fwFileName[FNAME_SIZE-1]=0;
						break;
					}
				}
				closedir(fwDir);
				if (strlen(fwFileName) == 0) {
					fprintf(stderr, "Error: no firmware file for %s found in directory %s\n", FW_FILENAME_BASE, fwDirName);
					return(0);
				}
			} else {
				fprintf(stderr, "Error: cannot validate emfwMapFile: %s\n", strerror(errno));
				return(0);
			}
		} else {
			getEMFWFileNames(port, path, sessionID, fwFileName, inibinFileName);
		}
	}
	status = getOemHash(port,path,&mad,sessionID,oemHash,&acb);
	if (status == FSUCCESS) {
		if (acb & 0x10000000) {
			if (getBinaryHash(fwFileName,binaryHash) == FSUCCESS) {
				if (memcmp(binaryHash, oemHash, sizeof(binaryHash)))
					isValid = 0;
			}
			else {
				fprintf(stderr,"Unable to get Signature from Binary\n");
				return(0);
			}
		}
	}
	else {
			fprintf(stderr,"Unable to get OEM hash\n");
			return(0);
	}

	if(isValid){
		if (stat(fwFileName, &fileStat) < 0) {
			fprintf(stderr, "Error taking stat of file {%s}: %s\n",
				fwFileName, strerror(errno));
				return(0);
		}

		bufSize = (uint32)fileStat.st_size + 1024; /* pad by 1K to be safe */

		if ((*fwBuffer = malloc(bufSize)) == NULL) {
			fprintf(stderr, "Error allocating memory for firmware buffer\n");
			return(0);
		}

		if ((fp = fopen(fwFileName, "r")) == NULL) {
			fprintf(stderr, "Error opening file %s for input: %s\n", fwFileName, strerror(errno));
			return(0);
		}
		memset(*fwBuffer, 0, bufSize);
		p = *fwBuffer + 4;
		while ((nread = fread(p, 1, 1024, fp)) > 0) {
			totalRead += nread;
			p += nread;
		}

		fclose(fp);

		//	xedgeDisplayBuffer(fwBuffer, totalRead);

		return(totalRead);
	}
	else{
		fprintf(stderr,"Firmware signature mismatch\n");
		return(0);
	}
}

#define LIST_FILE_SUPPORTED 0

static void usage(char *app_name)
{
	fprintf(stderr, "usage: %s -t target\n", app_name);
#if LIST_FILE_SUPPORTED
	fprintf(stderr, " [-l list_file]");
#endif
	fprintf(stderr, " [-v|-q] [-h hfi] [-o port] [-f fw_file | -d fw_directory] [-S]");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -t - target on which to update fw\n");
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "   -l - file that holds a list of targets to update\n");
#endif
	fprintf(stderr, "   -v - verbose output\n");
	fprintf(stderr, "   -q - no output\n");
	fprintf(stderr, "   -h - hfi, numbered 1..n, 0= -o port will be a\n");
	fprintf(stderr, "        system wide port num (default is 0)\n");
	fprintf(stderr, "   -o - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "        is 1st active)\n");
	fprintf(stderr, "   -f - firmware file (extracted .bin file from emfw) \n");
	fprintf(stderr, "   -d - firmware directory\n");
	fprintf(stderr, "   -S - reset switch after update\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -o options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -o 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -o 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -o y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -o y  - HFI x, port y\n");

#if 0 /* non-documented options */
	fprintf(stderr, "   -F - do secondary EEPROM (default if primary)\n");
	fprintf(stderr, "   -s - delay in seconds between requests\n");
	fprintf(stderr, "   -u - delay in microseconds between requests\n");
	fprintf(stderr, "   -T - response timeout in microseconds\n");
	fprintf(stderr, "   -z - write data length in bytes\n");
#endif

	exit(2);
}

int main(int argc, char *argv[])
{
	char				*cmdName;
	const char			*opts="FDSvqt:l:h:o:d:f:m:s:u:T:z:";
	uint8				*fwBuffer = NULL;
	uint8				*fwBuffer2 = NULL;
	char				parameter[100];
	uint8				*p;
#if FWDELAY_WORKAROUND
	char				*sleepVal;
#endif
	uint32				fwSize;
	EUI64				destPortGuid = -1;
	int					c;
	uint8				hfi = 0;
	uint8				port = 0;
	IB_PATH_RECORD		path;
	uint16				sessionID = 0;
	uint32				*u;
	uint32				crcEEPROM;
	uint32				crc;
	VENDOR_MAD_JUMBO	mad;
	FSTATUS				status = FSUCCESS;
	struct              omgt_port *omgt_port_session = NULL;

	// determine how we've been invoked
	cmdName = strrchr(argv[0], '/');			// Find last '/' in path
	if (cmdName != NULL) {
		cmdName++;								// Skip over last '/'
	} else {
		cmdName = argv[0];
	}

	// initialize
	fwFileName[0] = '\0';
	

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
				if ((p = (uint8 *)strchr(parameter, ',')) != NULL) {
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

			case 'f':
				g_fileParam = 1;
				memcpy(fwFileName, optarg, FNAME_SIZE);
				// Validate Filename
				if (!fwFileName[0]) {
					fprintf(stderr, "%s: Error: null input filename\n", cmdName);
					exit(2);
				}
				break;

			case 'd':
				g_dirParam = 1;
				memcpy(fwDirName, optarg, DNAME_SIZE);
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

			case 'S':
				g_resetSwitch = 1;
				break;

			case 'F':
				g_gotSecondary = 1;
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

	if ((!g_fileParam) && (!g_dirParam)) {
		fprintf(stderr, "%s: Error: Must specify either a directory or file for the firmware\n", cmdName);
		exit(1);
	} else if ((g_fileParam) && (g_dirParam)) {
		fprintf(stderr, "%s: Error: Cannot specify both a directory and file for the firmware\n", cmdName);
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

#if FWDELAY_WORKAROUND
	if ((sleepVal = getenv("FWDELAY")) != NULL) {
		g_usleepParam = atoi(sleepVal);
		printf("Overriding delay with value %d\n", g_usleepParam);
	}
#endif

	// Get the path

	struct omgt_params params = {.debug_file = g_debugMode ? stderr : NULL};
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

	status = sendClassPortInfoMad(omgt_port_session, &path, (VENDOR_MAD*)&mad);
	if (status != FSUCCESS) {
		fprintf(stderr, "%s: Error: Failed to send/rcv ClassPortInfo\n", cmdName);
		status = FERROR;
		goto err_exit;
	}

	// Get a session ID

	sessionID = getSessionID(omgt_port_session, &path);
	if (sessionID == (uint16)-1) {
		fprintf(stderr, "%s: Error: Failed to obtain sessionID\n", cmdName);
		status = FERROR;
		goto err_exit;
	}

	// read the file into the buffer

	fwSize = loadFirmwareBuffer(omgt_port_session, (char **)&fwBuffer, &path, sessionID);
	if (fwSize == 0) {
		fprintf(stderr, "%s: Error: Failed to load firmware file\n", cmdName);
		if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
		status = FERROR;
		goto err_exit;
	}
	u = (uint32 *)&fwBuffer[fwSize - 4];

	// Loop sending i2cAccess sets to update firmware

	if (!g_quiet) {
		printf("Updating firmware... ");
		fflush(stdout);
	}

	if (status != FSUCCESS) {
		// error trying to erase the old firmware
		fprintf(stderr, "%s: Error erasing old firmware rc: %d\n", cmdName, status);
	} else {
		status = opaswEepromRW(omgt_port_session, &path, sessionID, (void *)&mad, g_respTimeout,
							   fwSize + 4, 0, (uint8 *)fwBuffer, TRUE, g_gotSecondary);
		if (status != FSUCCESS) {
			// error trying to update the firmware
			fprintf(stderr, "%s: Error updating firmware rc: %d\n", cmdName, status);
		}
	}

	// now read firmware back in and verify checksum

	if (!g_quiet) {
		printf("\nUpdate complete ... verifying ... ");
		fflush(stdout);
	}
	if ((fwBuffer2 = malloc(fwSize + 1024)) == NULL) {
		fprintf(stderr, "Error allocating memory for firmware read back buffer\n");
		if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
		goto err_exit;
    }

	if (status == FSUCCESS) {
		memset(fwBuffer2, 0, fwSize + 1024);

		status = opaswEepromRW(omgt_port_session, &path, sessionID, (void *)&mad, g_respTimeout,
							   fwSize + 4, 0, (uint8 *)fwBuffer2, FALSE, g_gotSecondary);

		if (!g_quiet) {
			printf("\n");
			fflush(stdout);
		}
		if (status == FSUCCESS) {
			u = (uint32 *)&fwBuffer2[fwSize];
			crcEEPROM = *u;
			crc = crc32(0, (uint8 *)&fwBuffer2[CSS_HEADER_SIZE + 4], fwSize - 4 - CSS_HEADER_SIZE);
			if (crcEEPROM != crc) {
				printf("Verification failed\n");
				fprintf(stderr, "Error: CRC mismatch: 0x%08x : 0x%08x\n", ntoh32(crc), ntoh32(crcEEPROM));
				status = FERROR;
			} else {
				if (g_verbose)
					fprintf(stderr, "CRC match: 0x%08x : 0x%08x\n", ntoh32(crc), ntoh32(crcEEPROM));
				printf("Verification succeeded\n");
			}
		}
		// reset switch if necessary
		if ((status == FSUCCESS) && g_resetSwitch) {
			printf("Resetting switch...\n");
			status = sendRebootMad(omgt_port_session, &path, sessionID, (VENDOR_MAD*)&mad, 2000);
			if (status != FSUCCESS) {
				fprintf(stderr, "%s: Error sending MAD packet to switch\n", cmdName);
			}
		}
	}

	if (!g_resetSwitch)
		if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);

	printf("opaswfwupdate completed\n");

err_exit:
	omgt_close_port(omgt_port_session);

	if (fwBuffer != NULL)
		free(fwBuffer);

	if (fwBuffer2 != NULL)
		free(fwBuffer2);

	if (status == FSUCCESS)
		exit(0);
	else
		exit(1);

}
