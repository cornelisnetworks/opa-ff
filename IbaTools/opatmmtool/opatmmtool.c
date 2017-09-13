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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <getopt.h>

#include "iba/stl_types.h"
#include "iba/ib_helper.h"
#include "opatmmtool.h"

tmmOps_t op = OP_TMM_FWVER;
char *file_name;
int gotFile = 0;
uint8_t hfi = 1;
int i2cFd = 0;
int fileFd = 0;
int verbose = 0;


FSTATUS readFromFile(int fileFd, int offset, unsigned char *buffer, int len) {
	if (lseek(fileFd, offset, SEEK_SET) < 0) return FERROR;

	if (read(fileFd, buffer, len) < len) return FERROR;
	return FSUCCESS;
}

FSTATUS writeToFile(int fileFd, int offset, unsigned char *buffer, int len) {
	if (lseek(fileFd, offset, SEEK_SET) < 0) return FERROR;

	if (write(fileFd, buffer, len) < len) return FERROR;
	return FSUCCESS;
}


FSTATUS doReboot()
{
	uint32_t offset = (BYTES_OFFSET << 24) | (SLV_ADDR << 16) | (E_ADMIN_CMD_REBOOT & 0xFF);
	uint8_t buffer[1] = {0xFF};
	
	FSTATUS status = writeToFile(i2cFd, offset, (unsigned char*)&buffer[0], 1);
	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Encountered an error sending reboot command: %s\n", strerror(errno));
		return status;
	}
	if (verbose) printf("opatmmtool: Successfully sent reboot command\n");
	return status;
}

// Search the binary find to find 'VERSION=x.x.x.x.x'
FSTATUS getVersionFile(char *version)
{
	unsigned char buffer[8];
	const unsigned char key[8] = {"VERSION="};

	while(read(fileFd, &buffer[0], sizeof(buffer)) == sizeof(buffer))
	{
		// check to see if we have found the version string
		if (memcmp(&buffer[0], &key[0], sizeof(buffer)) == 0) {
			// found the version string
			if (read(fileFd, &version[0], VERSION_LEN) < 0) return FERROR;

			// the buffer now contains the version - but possibly some other junk on the end as well
			int i;
			for (i = 0; i < (VERSION_LEN-1) && (isdigit(version[i]) || version[i] == '.'); i++) ;
			version[i] = '\0';

			return FSUCCESS;
		}
		// keep looking, seek back -7
		if (lseek(fileFd, (off_t)-1*(sizeof(buffer)-1), SEEK_CUR) < 0) return FERROR;
	}

	// never found the version
	return FERROR;
}


FSTATUS getVersionFW(boolean silent, char *version)
{
	uint8_t buffer[VERSION_LEN] = {0};
	uint32_t *major, *minor, *maint, *patch, *build;
	uint32_t *version_ptr = (uint32_t*) &buffer[0];

	uint32_t offset = (BYTES_OFFSET << 24) | (SLV_ADDR << 16) | (E_ADMIN_CMD_GET_VERSION & 0xFF);
	FSTATUS status = readFromFile(i2cFd, offset, (unsigned char*)&buffer[0], VERSION_LEN);

	if (silent) return status;

	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Encountered an error while reading current firmware version: %s\n", strerror(errno));
		return status;
	}

	// Major, Minor, Maintenance, Patch, Build
	major = version_ptr++;
	minor = version_ptr++;
	maint = version_ptr++;
	patch = version_ptr++;
	build = version_ptr;

	printf("Current Firmware Version=%d.%d.%d.%d.%d\n", *major, *minor, *maint, *patch, *build);
	if(version)
		snprintf(version, VERSION_LEN,"%d.%d.%d.%d.%d", *major, *minor, *maint, *patch, *build);

	return status;
}

FSTATUS doFwUpdate()
{
	uint32_t offset;
	int ret;
	uint8_t buffer[256] = {0};
	uint32_t fwLength;
	struct stat fileInfo;
	uint32_t counter = 0;
	int readLen = 0;
	FSTATUS status = FSUCCESS;
	

	ret = fstat(fileFd, &fileInfo);
	if (ret != 0) {
		fprintf(stderr, "opatmmtool: Unable to stat firmware file\n");
		return FERROR;
	}

	// 8 bytes, truncating to 4
	fwLength = (unsigned int) fileInfo.st_size;

	if (verbose) printf("opatmmtool: Firmware length=%d\n", fwLength);

	memcpy(&buffer[0], (void*)&fwLength, 4);

	// Send firmware length
	offset = (BYTES_OFFSET << 24) | (SLV_ADDR << 16) | (E_ADMIN_CMD_FIRMWARE_UPDATE & 0xFF);
	status = writeToFile(i2cFd, offset, (unsigned char*)&buffer[0], 4);
	if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Failed to send firmware length: %s\n", strerror(errno));
			return status;
	}

	// Sleep while device erases Flash
	if (verbose) printf("opatmmtool: Waiting for device to erase flash...\n");
	sleep(2);
	if (verbose) printf("opatmmtool: Transmitting firmware\n");

	//file may have already been read to extract version, reset pointer to start of file. 
	ret = lseek(fileFd, 0, SEEK_SET);
	if (ret != 0) {
		fprintf(stderr, "opatmmtool: Unable to seek firmware file\n");
		return FERROR;
	}

	// Transmit firmware
	while (counter < fwLength) {
		readLen = read(fileFd, &buffer[0], CHUNK_SIZE);
		if (readLen < 0) {
			fprintf(stderr, "opatmmtool: Failed to read firmware file: %s\n", strerror(errno));
			return FERROR;
		}

		offset = (SLV_ADDR << 16);
		status = writeToFile(i2cFd, offset, (unsigned char*)&buffer[0], readLen);
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Failed to send data to the device: %s\n", strerror(errno));
			return status;
		}
		counter += readLen;
	}
	if (verbose) printf("opatmmtool: Successfully transmitted firmware to device\n");
	return status;
}

FSTATUS dumpOTP()
{
	uint32_t offset;
	uint8_t buffer[OTP_SIZE] = {0};
	FSTATUS status;

	offset = (BYTES_OFFSET << 24) | (SLV_ADDR << 16) | (E_ADMIN_CMD_READ_MEM & 0xFF);
	status = readFromFile(i2cFd, offset, (unsigned char*)&buffer[0], OTP_SIZE);
	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Unable to read one-time programmable region: %s\n", strerror(errno));
		return status;
	}

	status = writeToFile(fileFd, 0, (unsigned char*)&buffer[0], OTP_SIZE);
	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Unable to output data from one-time programmable region to file %s: %s\n", file_name, strerror(errno));
		return status;
	}
	if (verbose) printf("opatmmtool: Successfully dumped one-time programmable region to file\n");
	return status;
}

FSTATUS writeOTP()
{
	uint32_t offset;
	uint8_t buffer[OTP_SIZE] = {0};
	FSTATUS status = FSUCCESS;

	// read the file into the buffer
	status = readFromFile(fileFd, 0, (unsigned char*)&buffer[0], OTP_SIZE);
	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Unable to read datafile %s: %s\n", file_name, strerror(errno));
		return status;
	}

	offset = (BYTES_OFFSET << 24) | (SLV_ADDR << 16) | (E_ADMIN_CMD_WRITE_OTP & 0xFF);

	status = writeToFile(i2cFd, offset, (unsigned char*)&buffer[0], OTP_SIZE);
	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Encountered an error writing to one-time programmable region: %s\n", strerror(errno));
		return status;
	}
	if (verbose) printf("opatmmtool: Successfully wrote to one-time programmable region\n");
	return status;
}

FSTATUS doLock()
{
	uint32_t offset;
	uint8_t buffer[1] = {0xFF};
	FSTATUS status = FSUCCESS;

	offset = (BYTES_OFFSET << 24) | (SLV_ADDR << 16) | (E_ADMIN_CMD_LOCK_OTP & 0xFF);

	status = writeToFile(i2cFd, offset, (unsigned char*)&buffer[0], 1);
	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Encountered an error sending lock command to device: %s\n", strerror(errno));
		return status;
	}
	if (verbose) printf("opatmmtool: Successfully locked one-time programmable region\n");
	return status;
}

// GPIO pin status, 5 uint8 values
FSTATUS getStatus()
{
	uint32_t offset;
	uint8_t buffer[5] = {0};
	FSTATUS status = FSUCCESS;

	offset = (BYTES_OFFSET << 24) | (SLV_ADDR << 16) | (E_ADMIN_CMD_SYS_STATUS & 0xFF);
	status = readFromFile(i2cFd, offset, (unsigned char*)&buffer[0], 5);
	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Unable to read system status: %s\n", strerror(errno));
		return -1;
	}

	printf("GPIO Pin Status: 0x%x  0x%x  0x%x  0x%x  0x%x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);

	return status;
}

boolean checkWritable() // 1 if the otp is fully writable (unlocked), 0 otherwise
{
	uint32_t offset;
	uint8_t buffer[OTP_SIZE] = {0};
	uint8_t *locks = &buffer[OTP_SIZE - 16];
	boolean write_status, partial;
	FSTATUS status = FSUCCESS;
	// lock statuses are 16 additional bytes at the end of the otp region
	// each lock byte corresponds to a 32 byte sector of the otp region

	offset = (BYTES_OFFSET << 24) | (SLV_ADDR << 16) | (E_ADMIN_CMD_READ_MEM & 0xFF);
	status = readFromFile(i2cFd, offset, (unsigned char*)&buffer[0], OTP_SIZE);
	if (status != FSUCCESS) {
		fprintf(stderr, "opatmmtool: Unable to read system lock state: %s\n", strerror(errno));
		return -1;
	}
	// if all sectors are locked, report 0
	// if all sectors are unlocked, report 1
	// if a mixture of locks/unlocks, report 0 with a warning

	write_status = (boolean)*locks;
	partial = (boolean)*locks;
	int i;
	for (i = 1; i < 16; i++) {
		write_status &= (boolean)*(locks + i);
		partial |= (boolean)*(locks + i);
	}

	if (!write_status && partial) { // write_status = 0 means at least one sector is locked, partial = 1 means at least one sector is unlocked
		fprintf(stderr, "opatmmtool: Warning: One-Time Programmable region is partially unlocked\n");
		for (i = 0; i < 16; i++) {
			fprintf(stderr, "\tsector[%d] = %s\n", i, *(locks + i)?"unlocked":"locked");
		}
	}
	return write_status;
}

static void Usage(int exitcode)
{
	fprintf(stderr, "Usage: opatmmtool [-v] [-h hfi] [-f file] <operation>\n");
	fprintf(stderr, "          or\n");
	fprintf(stderr, "       opatmmtool --help\n");
	fprintf(stderr, "  --help - produce full help text\n\n");
	fprintf(stderr, "  -v - verbose output\n");
	fprintf(stderr, "  -h hfi - hfi, numbered 1..n, default is 1st hfi\n");
	fprintf(stderr, "  -f filename - firmware file or output file\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  <operation> -- one of the following\n");
	fprintf(stderr, "    reboot - reboot the TMM\n");
	fprintf(stderr, "    fwversion - report the current firmware version\n");
	fprintf(stderr, "    fileversion - report the file version, requires -f fw_file\n");
	fprintf(stderr, "    update - perform a firmware update, requires -f fw_file\n");
	fprintf(stderr, "    dumpotp - dump the one-time programmable region, requires -f output_file\n");
//	fprintf(stderr, "    writeotp - write to the one-time programmable region, requires -f input_file\n");
	fprintf(stderr, "    lockotp - lock the one-time programmable region\n");
	fprintf(stderr, "    status - display the current GPIO pin status\n");
	if (gotFile) {
		free(file_name);
	}
	exit(exitcode);
}

int main(int argc, char *argv[])
{
	const char	*opts="vh:f:";
	static struct option longopts[] = {{"help", 0, 0, '$'}, {0, 0, 0, 0}};
	int opt_idx;
	int c;
	int ret;
	FSTATUS status = FSUCCESS;

	// prase options and paramemters
	while (-1 != (c = getopt_long(argc, argv, opts, &longopts[0], &opt_idx))) {
		switch (c) {
		case '$':
			Usage(0);
		case 'v':
			verbose = 1;
			break;
		case 'h':
			if (StringToUint8(&hfi, optarg, NULL, 0, TRUE) != FSUCCESS || hfi <= 0) {
				fprintf(stderr, "opatmmtool: Invalid HFI Number: %s\n", optarg);
				Usage(2);
			}
			break;
		case 'f':
			file_name = strdup(optarg);
			if(file_name == 0) {
				fprintf(stderr, "opatmmtool: Process Out of Memory\n");
				status = FINSUFFICIENT_MEMORY;
				goto err_exit;
			}
			gotFile = 1;
			break;
		default:
			Usage(2);
		}
	}

	// process the operation
	if (argc != optind) {
		char *operation = argv[optind];

		if (!strcmp(operation, "reboot")) op = OP_TMM_REBOOT;
		else if (!strcmp(operation, "fwversion")) op = OP_TMM_FWVER;
		else if (!strcmp(operation, "fileversion")) op = OP_TMM_FILEVER;
		else if (!strcmp(operation, "update")) op = OP_TMM_UPDATE;
		else if (!strcmp(operation, "dumpotp")) op = OP_TMM_DUMP;
		else if (!strcmp(operation, "writeotp")) op = OP_TMM_WRITE;
		else if (!strcmp(operation, "lockotp")) op = OP_TMM_LOCK;
		else if (!strcmp(operation, "status")) op = OP_TMM_STATUS;
		else {
			fprintf(stderr, "Invalid operation: %s\n", operation);
			Usage(2);
		}
	}

	// check that all required arguments have been provided
	if ((op == OP_TMM_FILEVER ||
		 op == OP_TMM_UPDATE ||
		 op == OP_TMM_DUMP ||
		 op == OP_TMM_WRITE) 
		&& !gotFile) {
		fprintf(stderr, "opatmmtool: Operation requires -f fwFile\n");
		Usage(2);
	}

	// open the i2c driver interface
	if (op != OP_TMM_FILEVER) { // only command that doesn't require the driver interface
		char *i2cFile = (char *)malloc(I2C_FILE_SIZE);
		if (!i2cFile) {
			fprintf(stderr, "opatmmtool: No available memory\n");
			status = FINSUFFICIENT_MEMORY;
			goto err_exit;
		}
		snprintf(i2cFile, I2C_FILE_SIZE, "%s%d%s", I2C_FILE_1, --hfi, I2C_FILE_2); // the actual number is offset by one, so 1st hfi -> 0, 2nd hfi -> 1
		i2cFd = open(i2cFile, O_RDWR);
		int i = 0;
		while (i2cFd < 0 && errno == EBUSY && i++ < I2C_RETRIES) {
			sleep(1);
			i2cFd = open(i2cFile, O_RDWR);
		}
		if (i2cFd < 0) {
			fprintf(stderr, "opatmmtool: Unable to open driver interface: %s\n", strerror(errno));
			free(i2cFile);
			status = -1;
			goto err_exit;
		}

		if (verbose) printf("opatmmtool: Opened the driver interface\n");
		free(i2cFile);
	}

	// open the file
	if (op == OP_TMM_FILEVER || 
		op == OP_TMM_UPDATE ||
		op == OP_TMM_WRITE) {
		fileFd = open(file_name, O_RDONLY);
		if (fileFd < 0) {
			fprintf(stderr, "opatmmtool: Unable to open file %s\n", file_name);
			status = FERROR;
			goto err_exit;
		}
	} else if (op == OP_TMM_DUMP) {
		fileFd = open(file_name, O_RDWR|O_CREAT|O_TRUNC, 00644);
		if (fileFd < 0) {
			fprintf(stderr, "opatmmtool: Unable to open file %s\n", file_name);
			status = FERROR;
			goto err_exit;
		}
	}

	if (op != OP_TMM_FILEVER) {
		// use get FW version to see if board has tmm
		status = getVersionFW(1, NULL);
		if (status != FSUCCESS) {
			if (errno == ENXIO) {
				fprintf(stderr, "opatmmtool: Thermal Management Microchip sensor not installed\n");
			} else {
				fprintf(stderr, "opatmmtool: Thermal Management Microchip sensor not responding\n");
			}
			goto err_exit;
		}
	}

	// do the operation
	switch (op) {
	case OP_TMM_REBOOT:
	{
		if (verbose) printf("opatmmtool: Rebooting...\n");

		status = doReboot();
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to send reboot command to device\n");
			goto err_exit;
		}
		break;
	}

	case OP_TMM_FWVER:
	{
		status = getVersionFW(0, NULL);
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to read current firmware version\n");
			goto err_exit;
		}
		break;
	}

	case OP_TMM_FILEVER:
	{
		char version[VERSION_LEN];
		status = getVersionFile(version);
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to read version from firmware file '%s'\n", file_name);
			goto err_exit;
		} else {
			printf("File Firmware Version=%s\n", version);
		}
		break;
	}

	case OP_TMM_UPDATE:
	{
		char file_version[VERSION_LEN];
		char fw_version[VERSION_LEN];
		status = getVersionFile(file_version);
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to read version from firmware file '%s'\n", file_name);
			goto err_exit;
		} else {
			printf("File Firmware Version=%s\n", file_version);
		}

		status = doFwUpdate();
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to update the firmware\n");
			goto err_exit;
		}

		if (verbose) printf("opatmmtool: Firmware transmitted, wait for device ready\n");

		sleep(5);
		// check that update was successful by trying to get the new firmware version
		status = getVersionFW(0, fw_version);
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to validate firmware update success\n");
			goto err_exit;
		}

		if(strcmp(file_version, fw_version))
			printf("Firmware Update Failed\n");
		else 
			printf("Firmware Update Completed\n");
	



		break;
	}

	case OP_TMM_DUMP:
	{
		if (verbose) { // verbose option reports the lock status before dumping
			if(checkWritable()) printf("One-time programmable region is currently not locked\n");
			else printf("One-time programmable region has been locked\n");
		}

		status = dumpOTP();
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to dump one-time programmable region\n");
			goto err_exit;
		}

		break;
	}

	case OP_TMM_WRITE:
	{
		// check lock first
		if (!checkWritable()) {
			fprintf(stderr, "opatmmtool: Warning: One-Time Programmable region is already locked\n");
		}
		// validate that file is = 512 bytes
		struct stat fileInfo;
		ret = fstat(fileFd, &fileInfo);
		if (ret != 0) {
			fprintf(stderr, "opatmmtool: Unable to stat firmware file\n");
			status = FERROR;
			goto err_exit;
		}

		if (fileInfo.st_size != OTP_SIZE) {
			fprintf(stderr, "opatmmtool: Invalid input file for OTP write, must be 512 bytes\n");
			status = FERROR;
			goto err_exit;
		}

		status = writeOTP();
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to write to one-time programmable region\n");
			goto err_exit;
		}
		break;
	}

	case OP_TMM_LOCK:
	{
		status = doLock();
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to lock one-time programmable region\n");
			goto err_exit;
		}
		break;
	}

	case OP_TMM_STATUS:
	{
		if (verbose) {
			if (checkWritable()) 
				printf("One-Time Programmable Region is unlocked\n");
			else
				printf("One-Time Programmable Region is locked\n");
		}

		status = getStatus();
		if (status != FSUCCESS) {
			fprintf(stderr, "opatmmtool: Unable to check status\n");
			goto err_exit;
		}
		break;
	}
	}

err_exit:
	if (i2cFd > 0) close(i2cFd);
	if (fileFd > 0) close(fileFd);

	if (gotFile) {
		free(file_name);
	}

	if (status == FSUCCESS)
		exit(0);
	else
		exit(1);

}
