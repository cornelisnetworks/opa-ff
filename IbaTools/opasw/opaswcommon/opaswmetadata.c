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

#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include <iba/ibt.h>
#include "opamgt_sa_priv.h"
#include "opaswcommon.h"
#include "opaswmetadata.h"


table_meta_data_t systemMetaData[] =
{
//	IDX		LEN		START		PROTECTED		DATA_TYPE			FIELD
	/* 0 */		{0,		0,		1,			DT_INTVAL,			"RESERVED" },
	/* 1 */		{512,		0,		0,			DT_ASCII_STRING,		"NODE_STRING"},
	/* 2 */		{64,		512,		1,			DT_NUMERIC_STRING,		"SYSTEM_IMAGE_GUID"},
	/* 3 */		{64,		576,		1,			DT_NUMERIC_STRING,		"NODE_GUID"},
	/* 4 */		{32,		640,		1,			DT_NUMERIC_STRING,		"CAPABILITY_MASK"},
	/* 5 */		{32,		672,		1,			DT_NUMERIC_STRING,		"PORT0_CAPABILITY_MASK"},
	/* 6 */		{32,		704,		1,			DT_NUMERIC_STRING,		"REVISION"},
	/* 7 */		{24,		736,		1,			DT_INTVAL,			"VENDOR_ID"},
	/* 8 */		{8,		760,		1,			DT_INTVAL,			"NUM_PORTS"},
	/* 9 */		{8,		768,		1,			DT_INTVAL,			"INI_META_VERSION"},
	/* 10 */	{8,		776,		1,			DT_INTVAL,			"MODULE_TYPE"},
	/* 11 */	{16,		784,		1,			DT_INTVAL,			"PARTITION_CAP"},
	/* 12 */	{16,		800,		1,			DT_INTVAL,			"DEVICE_ID"},
	/* 13 */	{16,		816,		1,			DT_INTVAL,			"LINEAR_FDB_CAP"},
	/* 14 */	{16,		832,		1,			DT_INTVAL,			"MULTICAST_FDB_CAP"},
	/* 15 */	{16,		848,		1,			DT_INTVAL,			"PARTITION_ENFORCEMENT_CAP"},
	/* 16 */	{0,		0,		1,			DT_INTVAL,			"RESERVED16"},
	/* 17 */	{0,		0,		1,			DT_INTVAL,			"RESERVED17"},
	/* 18 */	{0,		0,		1,			DT_INTVAL,			"RESERVED18"},
	/* 19 */	{5,		872,		1,			DT_INTVAL,			"QSFP_RESET_INIT_ASSERT_TIME_IN_US"},
	/* 20 */	{16,		896,		1,			DT_INTVAL,			"QSFP_POWER_BUDGET_WATTS_IN_TENTHS"},
	/* 21 */	{16,		912,		1,			DT_INTVAL,			"QSFP_MODSEL_SETUP_TIME_IN_US"},
	/* 22 */	{32,		928,		1,			DT_INTVAL,			"QSFP_INITIALIZATION_TIME_IN_US"},
	/* 23 */	{32,		960,		1,			DT_INTVAL,			"QSFP_RESET_TIME_IN_US"},
	/* 24 */	{32,		992,		1,			DT_INTVAL,			"QSFP_POWER_UP_TIME_IN_US"},
	/* 25 */	{8,		1024,		1,			DT_INTVAL,			"QSFP_POWER_PER_QSFP_MAX_IN_TENTHS"},
	/* 26 */	{16,		1040,		1,			DT_INTVAL,			"QSFP_MGR_TASK_DELAY_IN_MS"},
	/* 27 */	{0,		0,		0,			DT_INTVAL,			"RESERVED27"},
	/* 28 */	{8,		1064,		0,			DT_INTVAL,			"QSFP_ATTENUATION_DEFAULT_25G"},
	/* 29 */	{8,		1072,		1,			DT_INTVAL,			"VARIABLE_TABLE_ENTRIES_PER_PORT"},
	/* 30 */	{8,		1080,		1,			DT_INTVAL,			"VARIABLE_TABLE_BASE_PORT"},
	/* 31 */	{16,		1088,		1,			DT_INTVAL,			"VARIABLE_TABLE_SLOT_OFFSET"},
	/* 32 */	{16,		1104,		1,			DT_INTVAL,			"AR_PORTGROUP_FDB_CAP"},
	/* 33 */	{2,		877,		1,			DT_INTVAL,			"FM_PUSH_BUTTON_STATE"},
	/* 34 */	{32,		1120,		1,			DT_INTVAL,			"QSFP_TEMPERATURE_POLL_TIME_IN_US"},
	/* 35 */	{2,		879,		1,			DT_INTVAL,			"PLATFORM_I2C_WRITE_BYTE_COUNT_TEMP"},
	/* 36 */	{32,		1152,		1,			DT_INTVAL,			"PLATFORM_I2C_LOCATION_TEMP"},
	/* 37 */	{16,		1184,		1,			DT_INTVAL,			"PLATFORM_I2C_REGISTER_QSFP_MAX_TEMP"},
	/* 38 */	{16,		1200,		1,			DT_INTVAL,			"PLATFORM_I2C_REGISTER_ASIC_TEMP"},
	/* 39 */	{64,		1216,		1,			DT_INTVAL,			"IPV6_ADDR_127_64"},
	/* 40 */	{64,		1280,		1,			DT_INTVAL,			"IPV6_ADDR_63_0"},
	/* 41 */	{32,		1344,		1,			DT_INTVAL,			"IPV4_ADDR"},
	/* 42 */	{16,		1376,		0,			DT_INTVAL,			"ASIC_TEMP_POLL_DELAY_IN_MS"},
	/* 43 */	{3,		864,		1,			DT_INTVAL,			"QSFP_INHIBIT_RXCDR_PWRSAVINGS_IN_TENTHS"}
};

const int systemMetaDataSize = sizeof(systemMetaData) / sizeof(table_meta_data_t);

table_meta_data_t portMetaData[] =
{
//	IDX		LEN		START		PROTECTED		DATA_TYPE			FIELD
	/* 0 */		{0,		0,		1,			DT_INTVAL,			"RESERVED" },
	/* 1 */		{8,		0,		1,			DT_INTVAL,			"PHYS_PORT_NUMBER"},
	/* 2 */		{4,		8,		1,			DT_INTVAL,			"PORT_TYPE"},
	/* 3 */		{5,		12,		1,			DT_INTVAL,			"QSFP_MODSEL_GRP"},
	/* 4 */		{3,		17,		1,			DT_INTVAL,			"QSFP_MODSEL_BIT"},
	/* 5 */		{5,		20,		1,			DT_INTVAL,			"REMOTE_ATTEN_25G"},
	/* 6 */		{5,		25,		1,			DT_INTVAL,			"LOCAL_ATTEN_25G"},
	/* 7 */		{10,		32,		1,			DT_INTVAL,			"LINK_SPEED_SUPPORTED"},
	/* 8 */		{4,		42,		1,			DT_INTVAL,			"LINK_WIDTH_SUPPORTED"},
	/* 9 */		{4,		46,		1,			DT_INTVAL,			"RX_LANE_POLARITY_INVERSION_MASK"},
	/* 10 */	{4,		50,		1,			DT_INTVAL,			"TX_LANE_POLARITY_INVERSION_MASK"},
	/* 11 */	{1,		54,		1,			DT_INTVAL,			"EXT_DEV_CFG"},
	/* 12 */	{4,		55,		0,			DT_INTVAL,			"VL_CAP"},
	/* 13 */	{4,		59,		0,			DT_INTVAL,			"MTU_CAP"},
	/* 14 */	{0,		0,		0,			DT_INTVAL,			"RESERVED14"},
	/* 15 */	{0,		0,		0,			DT_INTVAL,			"RESERVED15"},
	/* 16 */	{1,		63,		0,			DT_INTVAL,			"USER_ENABLED"},
	/* 17 */	{1,		64,		0,			DT_INTVAL,			"FM_ENABLED"},
	/* 18 */	{1,		65,		0,			DT_INTVAL,			"AUTO_LANE_SHEDDING"},
	/* 19 */	{1,		66,		0,			DT_INTVAL,			"MAX_BER_TARGET"},
	/* 20 */	{1,		67,		0,			DT_INTVAL,			"ISL_ENABLED"},
	/* 21 */	{0,		0,		0,			DT_INTVAL,			"RESERVED21"},
	/* 22 */	{3,		70,		0,			DT_INTVAL,			"LTP_CRC_MODE_SUPPORTED"},
	/* 23 */	{8,		96,		0,			DT_INTVAL,			"TARGET_BER_0"},
	/* 24 */	{8,		104,		0,			DT_INTVAL,			"TARGET_BER_1"},
	/* 25 */	{8,		112,		0,			DT_INTVAL,			"LOCAL_MAX_TIMEOUT"},
	/* 26 */	{4,		120,		0,			DT_INTVAL,			"TX_LANE_ENABLE_MASK"},
	/* 27 */	{1,		124,		0,			DT_INTVAL,			"CONTINOUS_REMOTE_UPDATES"},
	/* 28 */	{3,		125,		0,			DT_INTVAL,			"VCU"},
	/* 29 */	{14,		136,		0,			DT_INTVAL,			"VL15_BUFFER_SPACE_IN_BYTES"},
	/* 30 */	{1,		150,		0,			DT_INTVAL,			"EXTERNAL_LOOPBACK_ALLOWED"},
	/* 31 */	{12,		160,		1,			DT_INTVAL,			"TX_PRESET_IDX_FIXED_PORT"},
	/* 32 */	{12,		172,		1,			DT_INTVAL,			"TX_PRESET_IDX_ACTIVE_NO_EQ"},
	/* 33 */	{12,		192,		1,			DT_INTVAL,			"TX_PRESET_IDX_ACTIVE_EQ"},
	/* 34 */	{12,		204,		1,			DT_INTVAL,			"RX_PRESET_IDX"},
	/* 35 */	{1,		216,		1,			DT_INTVAL,			"SIDEBAND_CREDIT_RETURN"},
	/* 36 */	{8,		78,		1,			DT_INTVAL,			"CHANNEL_BASE"},
	/* 37 */	{4,		128,		1,			DT_INTVAL,			"LINK_WIDTH_DOWNGRADE_SUPPORTED"},
	/* 38 */	{0,		0,		1,			DT_INTVAL,			"RESERVED38"},
	/* 39 */	{1,		74,		1,			DT_INTVAL,			"QSFP_PWRCLASS4_INHIBIT_RXCDR"},
	/* 40 */	{1,		75,		1,			DT_INTVAL,			"QSFP_MAX_AMPLITUDE_APPLY"},
	/* 41 */	{2,		76,		1,			DT_INTVAL,			"QSFP_MAX_AMPLITUDE"}
};

int portMetaDataSize = sizeof(portMetaData) / sizeof(table_meta_data_t);

table_meta_data_t rxPresetsMetaData[] =
{
//	IDX		LEN		START		PROTECTED		DATA_TYPE			FIELD
  /* 0 */	{0,		0,				1,			DT_INTVAL,			"RESERVED"},
  /* 1 */	{1,		0,				1,			DT_INTVAL,			"QSFP_RX_OUTPUT_CDR_APPLY"},
  /* 2 */	{1,		1,				1,			DT_INTVAL,			"QSFP_RX_OUTPUT_EMP_APPLY"},
  /* 3 */	{1,		2,				1,			DT_INTVAL,			"QSFP_RX_OUTPUT_AMP_APPLY"},
  /* 4 */	{1,		3,				1,			DT_INTVAL,			"QSFP_RX_OUTPUT_CDR"},
  /* 5 */	{4,		4,				1,			DT_INTVAL,			"QSFP_RX_OUTPUT_EMP"},
  /* 6 */	{2,		8,				1,			DT_INTVAL,			"QSFP_RX_OUTPUT_AMP"}
};

int rxPresetsMetaDataSize = sizeof(rxPresetsMetaData) / sizeof(table_meta_data_t);

table_meta_data_t txPresetsMetaData[] =
{
//	IDX		LEN		START		PROTECTED		DATA_TYPE			FIELD
  /* 0 */	{0,		0,				1,			DT_INTVAL,			"RESERVED"},
  /* 1 */	{4,		0,				1,			DT_INTVAL,			"PRECUR"},
  /* 2 */	{5,		4,				1,			DT_INTVAL,			"ATTN"},
  /* 3 */	{5,		9,				1,			DT_INTVAL,			"POSTCUR"},
  /* 4 */	{1,		14,				1,			DT_INTVAL,			"QSFP_TX_INPUT_CDR_APPLY"},
  /* 5 */	{1,		15,				1,			DT_INTVAL,			"QSFP_TX_INPUT_EQ_APPLY"},
  /* 6 */	{1,		16,				1,			DT_INTVAL,			"QSFP_TX_INPUT_CDR"},
  /* 7 */	{4,		17,				1,			DT_INTVAL,			"QSFP_TX_INPUT_EQ"}
};

int txPresetsMetaDataSize = sizeof(txPresetsMetaData) / sizeof(table_meta_data_t);

table_meta_data_t qsfpAttenuationMetaData[] =
{
// IDX		LEN		START		PROTECTED		DATA_TYPE			FIELD
  /* 0 */	{0,		0,				1,			DT_INTVAL,			"RESERVED"},
  /* 1 */	{12,	0,				1,			DT_INTVAL,			"TX_PRESET_IDX"},
  /* 2 */	{12,	12,				1,			DT_INTVAL,			"RX_PRESET_IDX"}
};

int qsfpAttenuationMetaDataSize = sizeof(qsfpAttenuationMetaData) / sizeof(table_meta_data_t);

table_meta_data_t variableSettingsMetaData[] =
{
// IDX		LEN		START		PROTECTED		DATA_TYPE			FIELD
  /* 0 */	{0,		0,				1,			DT_INTVAL,			"RESERVED"},
  /* 1 */	{12,	0,				1,			DT_INTVAL,			"TX_PRESET_IDX"},
  /* 2 */	{12,	12,				1,			DT_INTVAL,			"RX_PRESET_IDX"}
};

int variableSettingsMetaDataSize = sizeof(variableSettingsMetaData) / sizeof(table_meta_data_t);

int32 getMetaDataIndexByField(table_meta_data_t *table, int tableSize, const char *field)
{
	int32				returnIdx = -1;
	int					tableIdx;
	table_meta_data_t	*tp = table;

	for (tableIdx = 0, tp = table; (tableIdx <= tableSize); tableIdx++, tp++) {
		if (strcmp(field, tp->field) == 0) {
			returnIdx = tableIdx;
			break;
		}
	}

	return(returnIdx);
}

#define MASK32(numBits)				((1<<numBits)-1)

uint32 getBitMask32(int startBit, int numBits)
{
	return((MASK32(numBits))<<(32 - startBit - numBits));
}

#define DISPLAY 1

FSTATUS parseDataTable(table_meta_data_t *metaData, uint8 *dataBuffer, uint16 tableLen, table_parsed_data_t *parsedData, int showme)
{
	FSTATUS			status = FSUCCESS;
	int				i;
	int				lengthInBytes;
	int				wordOffset;
	int				byteOffset;
	uint8			*p;
	uint32			fieldValue = 0;
	uint32			tempWord;

	for (i = 0; i < tableLen; i++) {
		wordOffset = (metaData[i].start >> 5);
		byteOffset = (wordOffset << 2);
		tempWord = (dataBuffer[byteOffset] << 24) + (dataBuffer[byteOffset + 1] << 16) + (dataBuffer[byteOffset + 2] << 8) + dataBuffer[byteOffset + 3];
		lengthInBytes = (metaData[i].length / 8) + (metaData[i].length % 8);
		if (metaData[i].length <= 32)
			fieldValue = (tempWord >> (metaData[i].start & 0x1f)) & ((1 << metaData[i].length) - 1);
		else
			fieldValue = tempWord;
		if (metaData[i].dataType != DT_INTVAL) {
			p = (uint8 *)&dataBuffer[byteOffset];
			parsedData[i].val.arrayPtr = (uint8 *)malloc(lengthInBytes * sizeof(uint8));
			memcpy(parsedData[i].val.arrayPtr, (char *)p, lengthInBytes);
			parsedData[i].lengthInBytes = lengthInBytes;
		} else {
			p = (uint8 *)&fieldValue;
			parsedData[i].val.intVal = fieldValue;
			parsedData[i].lengthInBytes = 4;
		}
	}

	if (showme) {
		for (i = 0; i < tableLen; i++) {
			int j;
			printf("Field %s:\t", metaData[i].field);
			if (metaData[i].dataType != DT_INTVAL) {
				printf("Array len %d: ", parsedData[i].lengthInBytes);
				for (j = 0; j < parsedData[i].lengthInBytes; j++)
					printf("0x%02x ", parsedData[i].val.arrayPtr[j]);
				printf("\n");
			} else {
				printf("Numerical value 0x%08x\n", parsedData[i].val.intVal);
			}
		}
	}

	return(status);
}

uint32 getModuleType(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID)
{
	FSTATUS						status = FSUCCESS;
	uint8						*p;
	uint8						memoryData[200];
	opasw_ini_descriptor_get_t	tableDescriptors;
	table_parsed_data_t			*parsedDataTable;
	VENDOR_MAD					mad;
	int32						metaIndex;
	uint32						retValue;

	status = sendIniDescriptorGetMad(port, path, &mad, sessionID, &tableDescriptors);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to get ini descriptors - status %d\n", status);
		return((uint32)-1);
	}

	parsedDataTable = malloc(tableDescriptors.sysDataLen * sizeof(table_parsed_data_t));
	if (parsedDataTable == NULL) {
		fprintf(stderr, "Error: Failed to allocate required memory.\n");
		return((uint32)-1);
	}

	status = sendMemAccessGetMad(port, path, &mad, sessionID, tableDescriptors.sysDataAddr, (uint8)200, memoryData);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to access system table memory - status %d\n", status);
		free(parsedDataTable);
		return((uint32)-1);
	}
	p = memoryData;
	status = parseDataTable(systemMetaData, p, MIN(tableDescriptors.sysMetaDataLen,systemMetaDataSize), &parsedDataTable[0], 0); 
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to parse system data table - status %d\n", status);
		free(parsedDataTable);
		return((uint32)-1);
	}

	metaIndex = getMetaDataIndexByField(systemMetaData, MIN(tableDescriptors.sysMetaDataLen,systemMetaDataSize), "MODULE_TYPE");
	retValue = parsedDataTable[metaIndex].val.intVal;
	free(parsedDataTable);
	return retValue;
}

uint32 getNumPorts(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID)
{
	FSTATUS						status = FSUCCESS;
	uint8						*p;
	uint8						memoryData[200];
	opasw_ini_descriptor_get_t	tableDescriptors;
	table_parsed_data_t			*parsedDataTable;
	VENDOR_MAD					mad;
	int32						metaIndex;
	uint32						retValue;

	status = sendIniDescriptorGetMad(port, path, &mad, sessionID, &tableDescriptors);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to get ini descriptors - status %d\n", status);
		return((uint32)-1);
	}

	parsedDataTable = malloc(tableDescriptors.sysDataLen * sizeof(table_parsed_data_t));
	if (parsedDataTable == NULL) {
		fprintf(stderr, "Error: Failed to allocate required memory.\n");
		return((uint32)-1);
	}

	status = sendMemAccessGetMad(port, path, &mad, sessionID, tableDescriptors.sysDataAddr, (uint8)200, memoryData);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to access system table memory - status %d\n", status);
		free(parsedDataTable);
		return((uint32)-1);
	}
	p = memoryData;
	status = parseDataTable(systemMetaData, p, MIN(tableDescriptors.sysMetaDataLen,systemMetaDataSize), &parsedDataTable[0], 0);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to parse system data table - status %d\n", status);
		free(parsedDataTable);
		return((uint32)-1);
	}

	metaIndex = getMetaDataIndexByField(systemMetaData, MIN(tableDescriptors.sysMetaDataLen,systemMetaDataSize), "NUM_PORTS");
	retValue = parsedDataTable[metaIndex].val.intVal;
	free(parsedDataTable);
	return retValue;
}

FSTATUS getNodeDescription(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, uint8 *nodeDesc)
{
	FSTATUS						status = FSUCCESS;
	uint8						*p;
	uint8						memoryData[200];
	opasw_ini_descriptor_get_t	tableDescriptors;
	table_parsed_data_t			*parsedDataTable;
	VENDOR_MAD					mad;
	int32						metaIndex;

	status = sendIniDescriptorGetMad(port, path, &mad, sessionID, &tableDescriptors);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to get ini descriptors - status %d\n", status);
		return((uint32)-1);
	}

	parsedDataTable = malloc(tableDescriptors.sysDataLen * sizeof(table_parsed_data_t));
	if (parsedDataTable == NULL) {
		fprintf(stderr, "Error: Failed to allocate required memory.\n");
		return((uint32)-1);
	}

	status = sendMemAccessGetMad(port, path, &mad, sessionID, tableDescriptors.sysDataAddr, (uint8)200, memoryData);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to access system table memory - status %d\n", status);
		free(parsedDataTable);
		return((uint32)-1);
	}
	p = memoryData;
	status = parseDataTable(&systemMetaData[0], p, MIN(tableDescriptors.sysMetaDataLen, systemMetaDataSize), &parsedDataTable[0], 0);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to parse system data table - status %d\n", status);
		free(parsedDataTable);
		return((uint32)-1);
	}

	metaIndex = getMetaDataIndexByField(&systemMetaData[0], MIN(tableDescriptors.sysMetaDataLen, systemMetaDataSize), "NODE_STRING");
	memcpy(nodeDesc, parsedDataTable[metaIndex].val.arrayPtr, parsedDataTable[metaIndex].lengthInBytes);
	nodeDesc[parsedDataTable[metaIndex].lengthInBytes] = '\0';
	free(parsedDataTable);
	return(status);
}

FSTATUS getFmPushButtonState(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, uint32 *state)
{
        FSTATUS                                         status = FSUCCESS;
        uint8                                           *p;
        uint8                                           memoryData[200];
        opasw_ini_descriptor_get_t      tableDescriptors;
        table_parsed_data_t                     *parsedDataTable;
        VENDOR_MAD                                      mad;
        int32                                           metaIndex;

        status = sendIniDescriptorGetMad(port, path, &mad, sessionID, &tableDescriptors);
        if (status != FSUCCESS) {
                fprintf(stderr, "Error: Failed to get ini descriptors - status %d\n", status);
                return((uint32)-1);
        }

        parsedDataTable = malloc(tableDescriptors.sysDataLen * sizeof(table_parsed_data_t));
        if (parsedDataTable == NULL) {
                fprintf(stderr, "Error: Failed to allocate required memory.\n");
                return((uint32)-1);
        }

        status = sendMemAccessGetMad(port, path, &mad, sessionID, tableDescriptors.sysDataAddr, (uint8)200, memoryData);
        if (status != FSUCCESS) {
                fprintf(stderr, "Error: Failed to access system table memory - status %d\n", status);
                free(parsedDataTable);
                return((uint32)-1);
        }
        p = memoryData;
        status = parseDataTable(&systemMetaData[0], p, MIN(tableDescriptors.sysMetaDataLen, systemMetaDataSize), &parsedDataTable[0], 0);
        if (status != FSUCCESS) {
                fprintf(stderr, "Error: Failed to parse system data table - status %d\n", status);
                free(parsedDataTable);
                return((uint32)-1);
        }

        metaIndex = getMetaDataIndexByField(&systemMetaData[0], MIN(tableDescriptors.sysMetaDataLen, systemMetaDataSize), "FM_PUSH_BUTTON_STATE");
        memcpy(state, (char *)&parsedDataTable[metaIndex].val.intVal,parsedDataTable[metaIndex].lengthInBytes);
        free(parsedDataTable);
        return(status);
}


FSTATUS getGuid(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, EUI64 *guid, int which)
{
	FSTATUS						status = FSUCCESS;
	uint8						*p;
	uint8						memoryData[200];
	opasw_ini_descriptor_get_t	tableDescriptors;
	table_parsed_data_t			*parsedDataTable;
	VENDOR_MAD					mad;
	int32						metaIndex;

	status = sendIniDescriptorGetMad(port, path, &mad, sessionID, &tableDescriptors);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to get ini descriptors - status %d\n", status);
		return((uint32)-1);
	}

	parsedDataTable = malloc(tableDescriptors.sysDataLen * sizeof(table_parsed_data_t));
	if (parsedDataTable == NULL) {
		fprintf(stderr, "Error: Failed to allocate required memory.\n");
		return((uint32)-1);
	}

	status = sendMemAccessGetMad(port, path, &mad, sessionID, tableDescriptors.sysDataAddr, (uint8)200, memoryData);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to access system table memory - status %d\n", status);
		free(parsedDataTable);
		return((uint32)-1);
	}
	p = memoryData;
	status = parseDataTable(systemMetaData, p, MIN(tableDescriptors.sysMetaDataLen, systemMetaDataSize), &parsedDataTable[0], 0);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to parse system data table - status %d\n", status);
		free(parsedDataTable);
		return((uint32)-1);
	}

	if (which == SYSTEM_IMAGE_GUID) {
		metaIndex = getMetaDataIndexByField(systemMetaData, MIN(tableDescriptors.sysDataLen, systemMetaDataSize), "SYSTEM_IMAGE_GUID");
		memcpy(guid, parsedDataTable[metaIndex].val.arrayPtr, parsedDataTable[metaIndex].lengthInBytes);
	} else if (which == NODE_GUID) {
		metaIndex = getMetaDataIndexByField(systemMetaData, MIN(tableDescriptors.sysDataLen, systemMetaDataSize), "NODE_GUID");
		memcpy(guid, parsedDataTable[metaIndex].val.arrayPtr, parsedDataTable[metaIndex].lengthInBytes);
	} else {
		fprintf(stderr, "Error: Invalid guid type\n");
		status = FERROR;
	}
	free(parsedDataTable);
	return(status);
}
