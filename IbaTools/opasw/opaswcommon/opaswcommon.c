/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015-2018, Intel Corporation

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
#include "openssl/sha.h"


#ifndef stringize
#define stringize(x) #x
#endif

#ifndef add_quotes
#define add_quotes(x) stringize(x)
#endif

#ifdef DBGPRINT
#undef DBGPRINT
#endif
#define DBGPRINT(format, args...) \
	do { if (g_verbose) { fflush(stdout); fprintf(stderr, format, ##args); } } while (0)

#define MODULUS_LEN 256
#define MODULUS_OFFSET 128
#define MAX_PATH_RECS 1

extern int g_verbose;


FSTATUS getDestPath(struct omgt_port *port, EUI64 destPortGuid, char *cmd, IB_PATH_RECORD *pathp)
{
	FSTATUS					fstatus;
	EUI64					portGuid		= -1;
	uint64_t				portPrefix		= -1;
	OMGT_QUERY				query;
	PQUERY_RESULT_VALUES	pq				= NULL;
	uint8_t					sm_sl			= -1;

	(void)omgt_port_get_port_guid(port, &portGuid);
	(void)omgt_port_get_port_prefix(port, &portPrefix);
	(void)omgt_port_get_port_sm_sl(port, &sm_sl);

	memset(&query, 0, sizeof(query));		// initialize reserved fields
	query.InputType = InputTypePathRecord;
	query.InputValue.IbPathRecord.PathRecord.ComponentMask =  IB_PATH_RECORD_COMP_SGID |
		IB_PATH_RECORD_COMP_DGID |
		IB_PATH_RECORD_COMP_PKEY |
		IB_PATH_RECORD_COMP_NUMBPATH |
		IB_PATH_RECORD_COMP_REVERSIBLE |
		IB_PATH_RECORD_COMP_SL;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.SGID.Type.Global.SubnetPrefix = portPrefix;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.SGID.Type.Global.InterfaceID = portGuid;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.DGID.Type.Global.SubnetPrefix = portPrefix;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.DGID.Type.Global.InterfaceID = destPortGuid;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.P_Key = 0x7fff;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.NumbPath = MAX_PATH_RECS;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.Reversible = 1;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.u2.s.SL = sm_sl;
	query.OutputType = OutputTypePathRecord;

	DBGPRINT("Query: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(query.InputType),
						iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	fstatus = omgt_query_sa(port, &query, &pq);

	if (! pq)
	{
		fprintf(stderr, "%*sSA PathRecord query Failed: %s\n", 0, "", FSTATUS_MSG(fstatus));
		goto fail;
	} else if (pq->Status != FSUCCESS)
	{
		fprintf(stderr, "%*sSA PathRecord query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				FSTATUS_MSG(pq->Status),
				pq->MadStatus, iba_sd_mad_status_msg(pq->MadStatus));
		free(pq);
		goto fail;
	} else if (pq->ResultDataSize == 0)
	{
		fprintf(stderr, "%*sNo Path Records Returned\n", 0, "");
		fstatus = FNOT_FOUND;
		free(pq);
		goto fail;
	} else
	{
		PATH_RESULTS *p = (PATH_RESULTS*)pq->QueryResult;

		DBGPRINT("MadStatus 0x%x: %s\n", pq->MadStatus,
									iba_sd_mad_status_msg(pq->MadStatus));
		DBGPRINT("%d Bytes Returned\n", pq->ResultDataSize);
		if (p->NumPathRecords == 0)
		{
			fprintf(stderr, "%*sNo Path Records Returned\n", 0, "");
			fstatus = FNOT_FOUND;
			goto fail;
		}
		*pathp = p->PathRecords[0];
		return(FSUCCESS);

	}

fail:
	return(FERROR);
}

void opaswDisplayBuffer(char *buffer, int dataLen)
{

	char *p = buffer;
	int i;

	for (i = 0; i < dataLen; i+=2) {
		if (!(i % 16))
			printf("\n%05x: ", i);
		printf("%02x%02x ", *p & 0xff, *(p+1) & 0xff);
		p += 2;
	}

	printf("\n");

	return;
}

uint16 getSessionID(struct omgt_port *port, IB_PATH_RECORD *path)
{

	VENDOR_MAD		mad;
	FSTATUS			status;
	uint16			sessionID;

	status = sendSessionMgmtGetMad(port, path, &mad, &sessionID); 
	if ((status == FSUCCESS) && (sessionID != 0))
		return(sessionID);
	else
		return((uint16)-1);
}

void releaseSession(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID)
{
	VENDOR_MAD		mad;
	FSTATUS			status;

	status = sendSessionMgmtReleaseMad(port, path, &mad, sessionID);
	if (status != FSUCCESS)
		fprintf(stderr, "Error: failed to release session\n");

	return;
}

uint16 getMadStatus(VENDOR_MAD *mad)
{
	uint16			status;
	return(status = ntoh16(mad->common.u.NS.Status.AsReg16));
}

void displayStatusMessage(uint16 madStatus)
{
	switch (madStatus) {

		case MAD_STATUS_SUCCESS:
			printf("Valid Field\n");
			break;

		case MAD_STATUS_BUSY:
			printf("Invalid MAD: Busy\n");
			break;

		case MAD_STATUS_UNSUPPORTED_CLASS_VER:
			printf("Invalid MAD: Unsupported Class or Version\n");
			break;

		case MAD_STATUS_UNSUPPORTED_METHOD:
			printf("Invalid MAD: Unsupported Method\n");
			break;

		case MAD_STATUS_UNSUPPORTED_METHOD_ATTRIB:
			printf("Invalid MAD: Unsupported Method/Attribute combination\n");
			break;

		case MAD_STATUS_INVALID_ATTRIB:
			printf("Invalid MAD: Invalid Attribute or Attribute modifier\n");
			break;

		case VM_MAD_STATUS_SESSION_ID:
			printf("Invalid MAD: Invalid Session ID\n");
			break;

		case VM_MAD_STATUS_NACK:
			printf("Invalid MAD: Nack\n");
			break;

		case VM_MAD_STATUS_BUS_BUSY:
			printf("Invalid MAD: Bus busy\n");
			break;

		case VM_MAD_STATUS_BUS_HUNG:
			printf("Invalid MAD: Bus hung\n");
			break;

		case VM_MAD_STATUS_LOST_ARB:
			printf("Invalid MAD: Lost ARB\n");
			break;

		case VM_MAD_STATUS_TIMEOUT:
			printf("Invalid MAD: Operation timeout\n");
			break;

		case VM_MAD_STATUS_SAVE_ERROR:
			printf("Invalid MAD: Save error\n");
			break;

		default:
			printf("Invalid MAD: Invalid value in status field\n");
			break;
	}
}

int compareFwVersion(uint8 *version1, uint8 *version2)
{
	int versionParts1[5];
	int versionParts2[5];
	int i;

	// parse versions

	sscanf((char *)version1, "%d.%d.%d.%d.%d",
		&versionParts1[0],
		&versionParts1[1],
		&versionParts1[2],
		&versionParts1[3],
		&versionParts1[4]);
	sscanf((char *)version2, "%d.%d.%d.%d.%d",
		&versionParts2[0],
		&versionParts2[1],
		&versionParts2[2],
		&versionParts2[3],
		&versionParts2[4]);
	for (i = 0; i < 5; i++) {
		if (versionParts1[0] != versionParts2[0])
			return((versionParts1[0] > versionParts2[0]) ? 1 : -1);
		if (versionParts1[1] != versionParts2[1])
			return((versionParts1[1] > versionParts2[1]) ? 1 : -1);
		if (versionParts1[2] != versionParts2[2])
			return((versionParts1[2] > versionParts2[2]) ? 1 : -1);
		if (versionParts1[3] != versionParts2[3])
			return((versionParts1[3] > versionParts2[3]) ? 1 : -1);
		if (versionParts1[4] != versionParts2[4])
			return((versionParts1[4] > versionParts2[4]) ? 1 : -1);
	}
	return(0);
}

FSTATUS getFwVersion(struct omgt_port *port, 
					 IB_PATH_RECORD *path, 
					 VENDOR_MAD *mad, 
					 uint16 sessionID, 
					 uint8 *fwVersion)
{

	FSTATUS			status;
	uint8			fwVerBuf[FWVERSION_SIZE];
	uint32			fwUpper;
	uint32			fwLower;

	status = sendGetFwVersionMad(port, path, mad, sessionID, fwVerBuf);
	if (status == FSUCCESS) {
		fwUpper = ntoh32(*(uint32 *)&fwVerBuf[0]);
		fwLower = ntoh32(*(uint32 *)&fwVerBuf[4]);
		sprintf((char *)fwVersion, "%d.%d.%d.%d.%d",
				(fwUpper >> 24) & 0xff,
				(fwUpper >> 16) & 0xff,
				(fwUpper >> 8) & 0xff,
				fwUpper & 0xff,
				fwLower);
	}
	return(status);
}

#define ADDRESS_CHANGE_VERSION "5.0.3.0.4"
#define DEVELOPMENT_VERSION "0.0.0.0.0"

FSTATUS getVPDInfo(struct omgt_port *port, 
				   IB_PATH_RECORD *path, 
				   VENDOR_MAD *mad, 
				   uint16 sessionID, 
				   uint32 module, 
				   uint8 BoardID,
				   vpd_fruInfo_rec_t *vpdInfo)
{

	FSTATUS			status = FSUCCESS;
	uint8			*vpdBuffer = NULL;
	uint8			devHdrBuf[DEVICE_HDR_SIZE];
	uint8			*v;
	uint8			*dp;
	uint32			vpdOffset = 0;
	uint32			vpdBufSize;
	uint32			vpdReadSize;
	uint32			vpdBytesRead = 0;
	uint32			vpdAddress = 0;
	int				oemInfoPresent = (BoardID == STL_PRR_BOARD_ID_HPE7K);
	int				len;
	int				numOEMFields;

	switch (module) {
		case OPASW_MODULE:
			vpdAddress = I2C_OPASW_VPD_EEPROM_ADDR;
			break;
		default:
			fprintf(stderr, "getVPDInfo: Invalid module\n");
			status = FERROR;
			break;
	}

	status = sendI2CAccessMad(port, path, sessionID, (void *)mad, NOJUMBOMAD, MMTHD_GET, RESP_WAIT_TIME, vpdAddress, DEVICE_HDR_SIZE, 0, devHdrBuf);
	if (status == FSUCCESS) {
		vpdBufSize = (devHdrBuf[5]<<8) + devHdrBuf[4];
		if ((vpdBuffer = malloc(vpdBufSize)) == NULL) {
			fprintf(stderr, "getVpdInfo: Error allocating vpd buffer\n");
			status = FERROR;
			goto bail;
		}
		v = vpdBuffer;
	} else {
		fprintf(stderr, "getVpdInfo: Error sending MAD packet to switch\n");
		goto bail;
	}

	while ((status == FSUCCESS) && (vpdBytesRead < vpdBufSize)) {
		if ((vpdBufSize - vpdBytesRead) < VPD_READ_SIZE)
			vpdReadSize = vpdBufSize - vpdBytesRead;
		else
			vpdReadSize = VPD_READ_SIZE;
		status = sendI2CAccessMad(port, path, sessionID, (void *)mad, NOJUMBOMAD, MMTHD_GET, RESP_WAIT_TIME, vpdAddress, vpdReadSize, vpdOffset, v);
		if (status == FSUCCESS) {
			v += vpdReadSize;
			vpdBytesRead += vpdReadSize;
			vpdOffset += vpdReadSize;
		} else {
			fprintf(stderr, "getVpdInfo: Error sending MAD packet to switch\n");
			goto bail;
		}
	}

#define LOOPLIMIT 15		// cap loop to search for FRUINFO
	if (status == FSUCCESS) {

		int loopCount;
		memset(vpdInfo, 0, sizeof(vpd_fruInfo_rec_t));
		dp = vpdBuffer;
		dp += DEVICE_HDR_SIZE;
		// Walk xinfo records until fruInfo record is found
		for (loopCount = 0; (loopCount < LOOPLIMIT) && (*dp != FRUINFO_TYPE); loopCount++)
		{
			len = *(dp + 2);
			dp += len;
		}
		if (loopCount == LOOPLIMIT)
		{
			fprintf(stderr, "getVpdInfo: Error parsing VPD info\n");
			status = FERROR;
			goto bail;
		}
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
		memcpy(vpdInfo->serialNum, dp + 1, len);
		vpdInfo->serialNum[len] = '\0';
		dp += len + 1;

		// copy part number
		len = *dp & LEN_MASK;
		memcpy(vpdInfo->partNum, dp + 1, len);
		vpdInfo->partNum[len] = '\0';
		dp += len + 1;

		// Copy model
		len = *dp & LEN_MASK;
		memcpy(vpdInfo->model, dp + 1, len);
		vpdInfo->model[len] = '\0';
		dp += len + 1;

		// Copy version
		len = *dp & LEN_MASK;
		memcpy(vpdInfo->version, dp + 1, len);
		vpdInfo->version[len] = '\0';
		dp += len + 1;

		// Copy mfgName
		len = *dp & LEN_MASK;
		memcpy(vpdInfo->mfgName, dp + 1, len);
		vpdInfo->mfgName[len] = '\0';
		dp += len + 1;

		// Copy productName
		len = *dp & LEN_MASK;
		memcpy(vpdInfo->productName, dp + 1, len);
		vpdInfo->productName[len] = '\0';
		dp += len + 1;

		// Copy mfgID
		switch (*dp)
		{
			case 0:
				vpdInfo->mfgID[0] = '\0';
				dp++;
				break;
			case 1:
			case 2:
				dp++;
		/* looks like mfgID is not least sig. byte first in the EEPROM, so copy in as is (do not byteswap) */
				memcpy(vpdInfo->mfgID, dp, 3);
				dp += 3;
				break;
			default:
				fprintf(stderr, "invalid Manufacturer ID code %d\n", *dp);
				break;
		}

		// Parse Mfg Date/Time
		if (*dp & 0x80 ){
			vpdInfo->mfgYear  = (*(dp + 3) << 2) + ((*(dp + 2) & 0xc0) >> 6);			/* least sig byte 1st: bits 31-22 */
			vpdInfo->mfgMonth = (*(dp + 2) & 0x3c) >> 2;									/* least sig byte 1st: bits 21-18 */
			vpdInfo->mfgDay   = ((*(dp + 2) & 0x03) << 3) + ((*(dp + 1) & 0xe0) >> 5);	/* least sig byte 1st: bits 17-13 */
			vpdInfo->mfgHours = *(dp + 1) & 0x1f;										/* least sig byte 1st: bits 12-8  */
			vpdInfo->mfgMins  = *dp & 0x3f;												/* least sig byte 1st: bits 5-0   */
		} else {
			vpdInfo->mfgYear = 0;
			vpdInfo->mfgMonth = 0;
			vpdInfo->mfgDay = 0;
			vpdInfo->mfgHours = 0;
			vpdInfo->mfgMins = 0;
		}

		dp += 4;
		numOEMFields = *dp & LEN_MASK;
		if (oemInfoPresent && (numOEMFields == 3)) {
			// only process if exactly 3 OEM fields

			dp++;

			// copy serial number
			len = *dp & LEN_MASK;
			memcpy(vpdInfo->serialNum, dp + 1, len);
			vpdInfo->serialNum[len] = '\0';
			dp += len + 1;

			// copy part number
			len = *dp & LEN_MASK;
			memcpy(vpdInfo->partNum, dp + 1, len);
			vpdInfo->partNum[len] = '\0';

			// oem rev is the last part of part number, after second hyphen
			if ((dp = (uint8 *)strchr((char *)vpdInfo->partNum, '-')) != NULL) {
				if ((dp = (uint8 *)strchr((char *)dp + 1, '-')) != NULL) {
					dp++;
					len = strlen((char *)dp);
					memcpy(vpdInfo->version, dp, len);
					vpdInfo->version[len] = '\0';
				} else {
					vpdInfo->version[0] = '\0';
				}
			} else {
				vpdInfo->version[0] = '\0';
			}
		}
	}

bail:
	if (vpdBuffer) free(vpdBuffer);
	return(status);
}

int ltc2974_L11_to_Celsius(uint16 half16)
{
        int exponent;
        int mantissa;
        int celsius;

        /* extract components from LinearFloat5_11 (L11) format */
        exponent = half16 >> 11;
        mantissa = half16 & 0x7ff;

        /* if mantissa is negative, sign extend */
        if (mantissa > 0x03FF)
                mantissa |= 0xFFFFF800;

        /* determine sign of exponent */
        if (exponent > 0x0F) {
                /* exponent is negative, sign extend */
                exponent |= 0xFFFFFFE0;
                /* make exponent positive */
                exponent = (~exponent)+1;
                /* divide instead of multiply */
                celsius = mantissa / (1 << exponent);
        }
        else /* exponent is positive */
                celsius = mantissa * (1 << exponent);

        return celsius;
}

FSTATUS getTempReadings(struct omgt_port *port, IB_PATH_RECORD *path,
		VENDOR_MAD *mad, uint16 sessionID, char tempStrs[I2C_OPASW_TEMP_SENSOR_COUNT][TEMP_STR_LENGTH], uint8 BoardID)
{
	FSTATUS status = FSUCCESS;
	uint8 ErrorFlags = 0;
	uint16 mgmtFpgaOffset = 0;
	boolean maxDetected;
	union {
		uint8 u8[2];
		uint16 u16;
	} value;

//for GMF modules, LTC2974 may not be there

	if (BoardID == STL_PRR_BOARD_ID_HPE7K) {
		snprintf(tempStrs[0], TEMP_STR_LENGTH, "LTC2974: N/A");
	}
	else {
	 // LTC2974
		// It is possible the LTC2974 temp sensor may need to be initilized
		status = sendI2CAccessMad(port, path, sessionID, (void *)mad, NOJUMBOMAD, MMTHD_GET, RESP_WAIT_TIME,
			I2C_OPASW_LTC2974_TEMP_ADDR, 2, I2C_OPASW_LTC2974_TEMP_OFFSET, &value.u8[0]);

		if (status != FSUCCESS) {
			//fprintf(stderr, "getTempReadings: Error sending MAD packet to switch to read LTC2974 temp\n");
			snprintf(tempStrs[0], TEMP_STR_LENGTH, "LTC2974: N/A");
			ErrorFlags |= (1<<0);
		} else {
			snprintf(tempStrs[0], TEMP_STR_LENGTH, "LTC2974: %dC", ltc2974_L11_to_Celsius(value.u16));
		}
	}

	{ // PRR ASIC
		mgmtFpgaOffset = (I2C_OPASW_MGMT_FPGA_REG_RD << 8) | I2C_OPASW_PRR_ASIC_TEMP_MGMT_FPGA_OFFSET;
		status = sendI2CAccessMad(port, path, sessionID, (void *)mad, NOJUMBOMAD, MMTHD_GET, RESP_WAIT_TIME,
			I2C_OPASW_MGMT_FPGA_ADDR, 1, mgmtFpgaOffset, &value.u8[0]);

		if (status != FSUCCESS) {
			//fprintf(stderr, "getTempReadings: Error sending MAD packet to switch to read PRR ASIC temp\n");
			snprintf(tempStrs[1], TEMP_STR_LENGTH, "PRR ASIC: N/A");
			ErrorFlags |= (1<<2);
		} else {
			snprintf(tempStrs[1], TEMP_STR_LENGTH, "PRR ASIC: %dC", value.u8[0]);
		}
	}
	{ // CHECK QSFPTemperatureMaxDetected
		status = getMaxQsfpTemperatureMaxDetected(port, path, mad, sessionID, &maxDetected);
		if (status != FSUCCESS) {
			fprintf(stderr,"Error: getMaxQsfpTemperatureMaxDetected failed\n");
			iba_fstatus_msg(status);
		}
	}

	{ // MAX QSFP
		if (maxDetected) {
			mgmtFpgaOffset = (I2C_OPASW_MGMT_FPGA_REG_RD << 8) | I2C_OPASW_MAX_QSFP_TEMP_MGMT_FPGA_OFFSET;
			status = sendI2CAccessMad(port, path, sessionID, (void *)mad, NOJUMBOMAD, MMTHD_GET, RESP_WAIT_TIME,
				I2C_OPASW_MGMT_FPGA_ADDR, 1, mgmtFpgaOffset, &value.u8[0]);

			if (status != FSUCCESS) {
				//fprintf(stderr, "getTempReadings: Error sending MAD packet to switch to read MAX QSFP temp\n");
				snprintf(tempStrs[2], TEMP_STR_LENGTH, "MAX QSFP: N/A");
				ErrorFlags |= (1<<1);
			} else {
				snprintf(tempStrs[2], TEMP_STR_LENGTH, "MAX QSFP: %dC", value.u8[0]);
			}
		} else {
				snprintf(tempStrs[2], TEMP_STR_LENGTH, "MAX QSFP: N/A");
		}
	}

	return(status | (ErrorFlags << 8));

}

FSTATUS getFanSpeed(struct omgt_port *port, 
					IB_PATH_RECORD *path, 
					VENDOR_MAD *mad, 
					uint16 sessionID, 
					uint32 fanNum, 
					uint32 *fanSpeed)
{
	FSTATUS			status = FSUCCESS;
	uint32			readOffset = 0;
	union {
		uint8 u8[2];
		uint16 u16;
	} value;

	// validate the fan number
	if (fanNum >= OPASW_PSOC_FAN_CTRL_TACHS) {
		fprintf(stderr, "getFanSpeed: Invalid fan index: %d\n", fanNum);
		return FERROR;
	}

	// readOffset calculated as seen in BSP/bpscommon/ibml/psocFanCtrl.c - psocFanCtrl_ReadTach
	readOffset = OPASW_PSOC_FAN1_ACTUAL_SPEED_OFFSET + (fanNum * 2);

	status = sendI2CAccessMad(port, path, sessionID, (void *)mad, NOJUMBOMAD, MMTHD_GET, RESP_WAIT_TIME,
							  I2C_OPASW_PSOC_FAN_ADDR, 2, readOffset, &value.u8[0]);

	if (status != FSUCCESS) {
		fprintf(stderr, "getFanSpeed: Error sending MAD packet to switch to read\n");
	} else {
		// tachReading obtained as seen in BSP/bspcommon/i2c.c - i2cReadWordReg (sort of)
		int tachReading = value.u16;
		// fanSpeed calculated as seen in BSP/bpscommon/ibml/psocFanCtrl.c - psocFanCtrl_ReadTach
		int temp = (tachReading & 0xFF) << 8;
		*fanSpeed = (((tachReading >> 8) & 0xFF) | temp);
	}

	return(status);

	// TODO: do we still need to check if the fan needs to be initialized? Its not
	// clear to me from the bsp code
}

int getNumPS() {
	return 2;
	//This function will always return 2 for now, but in the future this number may change based upon manufacturer or design.
}

FSTATUS getPowerSupplyStatus(struct omgt_port *port, 
							 IB_PATH_RECORD *path, 
							 VENDOR_MAD *mad, 
							 uint16 sessionID, 
							 uint32 psNum, 
							 uint32 *psStatus)
{
	FSTATUS			status = FSUCCESS;
	uint32			mgmtFpgaAddress = 0;
	uint16 			mgmtFpgaOffset = 0;
	uint32			powerSupplyStatus = 0;
	uint8			dataBuffer[10];
	uint8			*psPtr;
	uint8			value;
        int 			numPS;

	numPS = getNumPS();
	mgmtFpgaAddress = I2C_OPASW_MGMT_FPGA_ADDR;
	mgmtFpgaOffset = (I2C_OPASW_MGMT_FPGA_REG_RD << 8) | I2C_OPASW_PS_MGMT_FPGA_OFFSET;

	if ((psNum < 1) || (psNum > 2)) {
		status = FERROR;
		fprintf(stderr, "getPowerSupplyStatus: bad power supply number %d\n", psNum);
	}

	if (status == FSUCCESS) {
		status = sendI2CAccessMad(port, path, sessionID, (void *)mad, NOJUMBOMAD, MMTHD_GET, RESP_WAIT_TIME, 
								  mgmtFpgaAddress, 1, mgmtFpgaOffset, dataBuffer);

		if (status != FSUCCESS) {
			fprintf(stderr, "getPowerSupplyStatus: Error sending MAD packet to switch\n");
		} else {
			psPtr = dataBuffer;
 			value = *psPtr;
			if (psNum == 1) {
				powerSupplyStatus = ((value >> PSU1_MGMT_FPGA_BIT_PRESENT) & 0x01);
				if (powerSupplyStatus) {
					if ((value >> PSU1_MGMT_FPGA_BIT_PWR_OK) & 0x01)
						*psStatus = PS_ONLINE;
					else
						*psStatus = PS_OFFLINE;
				} else {
					*psStatus = PS_NOT_PRESENT;
				}
			}
			else if (numPS<2) {
				*psStatus = PS_INVALID;
			}
			else {
				powerSupplyStatus = ((value >> PSU2_MGMT_FPGA_BIT_PRESENT) & 0x01);
				if (powerSupplyStatus) {
					if ((value >> PSU2_MGMT_FPGA_BIT_PWR_OK) & 0x01)
						*psStatus = PS_ONLINE;
					else
						*psStatus = PS_OFFLINE;
				} else {
					*psStatus = PS_NOT_PRESENT;
				}
			}
		}
	}

	return(status);
}

FSTATUS getMaxQsfpTemperatureMaxDetected(struct omgt_port *port, 
					   IB_PATH_RECORD *path, 
					   VENDOR_MAD *mad, 
					   uint16 sessionID, 
					   boolean *maxDetected)
{

	FSTATUS			status;
	uint32			location = QSFP_MGR_TEMPERATURE_MAX_DETECTED_MEM_ADDR;
	uint32			nextLocation;
	uint32			result;
	uint8			memoryData[4]; //32 bits

	*maxDetected = FALSE;
	status = sendMemAccessGetMad(port, path, mad, sessionID, location, (uint8)4, memoryData);
	if (status == FSUCCESS) {
		nextLocation = ntoh32(*(uint32 *)memoryData);
	} else {
		fprintf(stderr, "sendMemAccessGetMad error\n");
		return(status);
	}
	status = sendMemAccessGetMad(port, path, mad, sessionID, nextLocation, (uint8)4, memoryData);
	if (status == FSUCCESS) {
		result = ntoh32(*(uint32 *)memoryData);
		*maxDetected = !((0x80000000 & result) >> 31);
		//If the 31st bit is 1 the value is NOT valid
		//If the 31st bit is 0 the value is valid
	} else {
		fprintf(stderr, "sendMemAccessGetMad error\n");
	}
	
	return(status);
}


FSTATUS getAsicVersion(struct omgt_port *port, 
					   IB_PATH_RECORD *path, 
					   VENDOR_MAD *mad, 
					   uint16 sessionID, 
					   uint32 *asicVersion)
{

	FSTATUS			status;
	uint32			location = ASIC_VERSION_MEM_ADDR;
	uint8			memoryData[10];

	status = sendMemAccessGetMad(port, path, mad, sessionID, location, (uint8)10, memoryData);
	if (status == FSUCCESS) {
		*asicVersion = ntoh32(*(uint32 *)memoryData);
	}
	return(status);
}

FSTATUS getOemHash(struct omgt_port *port,
                                           IB_PATH_RECORD *path,
                                           VENDOR_MAD *mad,
                                           uint16 sessionID,
                                           uint32 *oemHash,uint32 *acb)
{

        FSTATUS                 status;
        uint32                  location = OEM_HASH_SIGNER_START_ADDRESS;
        uint8                   memoryData[4];
	int			i = 0;
	status = sendMemAccessGetMad(port, path, mad, sessionID, AUTHENTICATION_CONTROL_BIT_ADDRESS, (uint8)4, memoryData);
	if(status != FSUCCESS)
		return status;
	*acb = ntoh32 (*(uint32 *)memoryData);
	for(location = OEM_HASH_SIGNER_START_ADDRESS; location <= OEM_HASH_SIGNER_END_ADDRESS; location++){
		status = sendMemAccessGetMad(port, path, mad, sessionID, location, (uint8)4, memoryData);
		if (status == FSUCCESS) {
			oemHash[i] = ntoh32 (*(uint32 *)memoryData);
			i++;
		}
		else
			return(status);
	}
        return(status);
}


FSTATUS getBoardID(struct omgt_port *port,
				   IB_PATH_RECORD *path, 
				   VENDOR_MAD *mad, 
				   uint16 sessionID, 
				   uint8 *boardID)
{

	FSTATUS			status;
	uint32			boardIdAddress = I2C_BOARD_ID_ADDR;
	uint16			offset;
	uint8			dataBuffer;

	offset = (I2C_OPASW_MGMT_FPGA_REG_RD << 8) | I2C_OPASW_BD_MGMT_FPGA_OFFSET;

	status = sendI2CAccessMad(port, path, sessionID, (void *)mad, NOJUMBOMAD, MMTHD_GET, RESP_WAIT_TIME, 
							  boardIdAddress, 1, offset, &dataBuffer);
	if (status == FSUCCESS) {
		dataBuffer &= STL_PRR_BOARD_ID_MASK;
		*boardID = dataBuffer;
	}
	return(status);
}

FSTATUS doPingSwitch(struct omgt_port *port, IB_PATH_RECORD *path, STL_PERF_MAD *mad)
{

	FSTATUS			status;

	status = sendSTLPortStatsPort1Mad(port, path, mad);

	return(status);
}


FSTATUS getEMFWFileNames(struct omgt_port *port, 
						 IB_PATH_RECORD *path, 
						 uint16 sessionID, 
						 char *fwFileName, 
						 char *inibinFileName)
{
	FSTATUS				status = FSUCCESS;
	VENDOR_MAD			mad;
	uint8				boardID;
	FILE				*fpInibinMap = NULL;
	char				readBuf[80];
	char				fwName[FNAME_SIZE];
	char				inibinName[FNAME_SIZE];
	unsigned int		id;

	fwFileName[0] = 0;
	inibinFileName[0] = 0;

	status = getBoardID(port, path, &mad, sessionID, &boardID);

	if (status == FSUCCESS) {
		fpInibinMap = fopen("emfwMapFile", "r");
		if (fpInibinMap == NULL) {
			status = FINVALID_PARAMETER;
		} else {
			while (fgets(readBuf, 80, fpInibinMap) != NULL) {
				if (readBuf[0] != '0')
					continue;
				sscanf(readBuf, "%x %"add_quotes(FNAME_SIZE)"s %"add_quotes(FNAME_SIZE)"s", &id, fwName, inibinName);
				if (id == boardID) {
					strcpy(fwFileName, fwName);
					strcpy(inibinFileName, inibinName);
					break;
				}
			}
		}
		if (fpInibinMap)
			fclose(fpInibinMap);
	}

	return(status);
}

FSTATUS  getBinaryHash(char *fwFileName,uint32 *binaryHash)
{
	FILE *fp;
	int nread;
	char buf[MODULUS_LEN];
	unsigned char hash[SHA256_DIGEST_LENGTH];
	if (fwFileName == NULL || strlen(fwFileName) == 0) {
		fprintf(stderr, "Error: Firmware file name is invalid\n");
		return FERROR;
	}
	else if ((fp = fopen(fwFileName,"rb")) == NULL) {
		return FERROR;
	}
	else if (fseek(fp, MODULUS_OFFSET, 0)) {
		fclose(fp);
		return FERROR;
	}
	else if ((nread = fread(buf,1,MODULUS_LEN,fp)) == MODULUS_LEN) {
		fclose(fp);
		int i = 0;
		(void *)SHA256((unsigned char*)buf,sizeof(buf),hash);
		for(i = 0; i < (SHA256_DIGEST_LENGTH/sizeof(uint32)); i++)
			binaryHash[i] = ntoh32(*(uint32 *)(hash +(i * sizeof(uint32))));

		return FSUCCESS;
	}

	fclose(fp);
	return FERROR;
}
/* opaswEepromRW: Reads from or Writes to the switch EEPROM
   based on prrEepromRW in prrFwUpdate.c */
FSTATUS opaswEepromRW(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, void *mad, int timeout,
					  uint32 len, uint32 offset, uint8 *data, boolean writeData, boolean secondary) {
	uint32 maxXfer = 128;
	uint32 xferLen;
	int result;
	uint32 remainingLen = len;
	uint32 eepromAddrIncrement;
	uint32 location = secondary ? STL_PRR_SEC_EEPROM1_ADDR : STL_PRR_PRI_EEPROM1_ADDR;


	while (remainingLen) {
		xferLen = MIN(remainingLen, maxXfer);

		eepromAddrIncrement = ((offset >> 16) & 0x03) << 1;

		// update the location to account for the 4 EEPROMs
		location = (secondary ? STL_PRR_SEC_EEPROM1_ADDR : STL_PRR_PRI_EEPROM1_ADDR) + (eepromAddrIncrement << STL_PRR_I2C_LOCATION_ADDR);

		// for now, access the secondary bus at 100KHz for writing
		if (secondary && writeData)
			location &= 0x7fffffff;

		if (writeData) {
			result = sendI2CAccessMad(port, path, sessionID, mad, NOJUMBOMAD, MMTHD_SET, timeout, location, xferLen, offset & 0xffff, data);
		} else {
			result = sendI2CAccessMad(port, path, sessionID, mad, NOJUMBOMAD, MMTHD_GET, timeout, location, xferLen, offset & 0xffff, data);
		}

		if (result != FSUCCESS) {
			return result;
		}
		offset += xferLen;
		data += xferLen;
		remainingLen -= xferLen;
	}
	return FSUCCESS;
}

const char*
StlPortLtpCrcModeVMAToText(uint16_t mode, char *str, size_t len)
{
        size_t n = 0;
        size_t i = 0;

        str[0]='\0';

        PRINT_OR_OUT(str, len, "16-bit,");
        if (mode & STL_PRR_PORT_LTP_CRC_MODE_VMA_14)
                PRINT_OR_OUT(str, len, "14-bit,");
        if (mode & STL_PRR_PORT_LTP_CRC_MODE_VMA_48)
                PRINT_OR_OUT(str, len, "48-bit,");
        if (mode & STL_PRR_PORT_LTP_CRC_MODE_VMA_12_16_PER_LANE)
                PRINT_OR_OUT(str, len, "12-16/lane,");

        str[n-1] = 0; // Eliminate trailing comma

out:
        return str;
}

