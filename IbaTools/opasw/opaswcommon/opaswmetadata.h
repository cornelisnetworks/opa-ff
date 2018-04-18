/* BEGIN_ICS_COPYRIGHT3 ****************************************

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

** END_ICS_COPYRIGHT3   ****************************************/

/* [ICS VERSION STRING: unknown] */

/***********************
 *   MACROS
 ***********************/

#ifndef _XEDGE_METADATA_H_
#define _XEDGE_METADATA_H_


/***********************
 *   CONSTANTS
 ***********************/


#define			ENC_DISABLED				0
#define			ENC_ENABLED					1

#define			ENC_NO						0
#define			ENC_YES						1

#define			ENC_MOD_TYPE_EDGE			0
#define			ENC_MOD_TYPE_SPINE			1
#define			ENC_MOD_TYPE_LEAF			2

#define			ENC_PORT_TYPE_DISC			1
#define			ENC_PORT_TYPE_FIXED			2
#define			ENC_PORT_TYPE_VARIABLE		3
#define			ENC_PORT_TYPE_QSFP			4
#define			ENC_PORT_TYPE_SIPHx16		5

#define			ENC_VLCAP_VL0				1
#define			ENC_VLCAP_VL0_1				2
#define			ENC_VLCAP_VL0_2				3
#define			ENC_VLCAP_VL0_3				4
#define			ENC_VLCAP_VL0_4				5
#define			ENC_VLCAP_VL0_5				6
#define			ENC_VLCAP_VL0_6				7
#define			ENC_VLCAP_VL0_7				8
#define			ENC_VLCAP_VL0_8				9
#define			ENC_VLCAP_VL0_9				10
#define			ENC_VLCAP_VL0_10			11
#define			ENC_VLCAP_VL0_11			12
#define			ENC_VLCAP_VL0_12			13
#define			ENC_VLCAP_VL0_13			14
#define			ENC_VLCAP_VL0_14			15

#define			ENC_MTU_CAP_512				2
#define			ENC_MTU_CAP_1024			3
#define			ENC_MTU_CAP_2048			4
#define			ENC_MTU_CAP_4096			5
#define			ENC_MTU_CAP_8192			8
#define			ENC_MTU_CAP_10240			9

#define			ENC_LINK_SPEED_12G			1
#define			ENC_LINK_SPEED_25G			2
#define			ENC_LINK_SPEED_12G_25G		3

#define			ENC_LINK_WIDTH_1X			1
#define			ENC_LINK_WIDTH_2X			2
#define			ENC_LINK_WIDTH_2X_1X		3
#define			ENC_LINK_WIDTH_3X			4
#define			ENC_LINK_WIDTH_3X_1X		5
#define			ENC_LINK_WIDTH_3X_2X		6
#define			ENC_LINK_WIDTH_3X_2X_1X		7
#define			ENC_LINK_WIDTH_4X			8
#define			ENC_LINK_WIDTH_4X_1X		9
#define			ENC_LINK_WIDTH_4X_2X		10
#define			ENC_LINK_WIDTH_4X_2X_1X		11
#define			ENC_LINK_WIDTH_4X_3X		12
#define			ENC_LINK_WIDTH_4X_3X_1X		13
#define			ENC_LINK_WIDTH_4X_3X_2X		14
#define			ENC_LINK_WIDTH_4X_3X_2X_1X	15

#define			ENC_POLARITY_NORMAL			0
#define			ENC_POLARITY_REVERSE		1

#define			ENC_LREV_4X_MODE_NORMAL		0
#define			ENC_LREV_4X_MODE_REVERSE	4

#define			ENC_LREV_8X_MODE_NORMAL		0
#define			ENC_LREV_8X_MODE_REV_UPPER	1
#define			ENC_LREV_8X_MODE_REV_LOWER	4
#define			ENC_LREV_8X_MODE_REV_ALL	5
#define			ENC_LREV_8X_MODE_SWAP		10
#define			ENC_LREV_8X_MODE_SW_REV_LOWER 11
#define			ENC_LREV_8X_MODE_SW_REV_UPPER 14
#define			ENC_LREV_8X_MODE_SW_REV_ALL 15

#define			ENC_PKT_FOR_MODE_CUT_THRU	0
#define			ENC_PKT_FOR_MODE_ST_FW		1

#define			ENC_DISTRIBUTION_BAL		0
#define			ENC_DISTRIBUTION_VL0_LDED	1
#define			ENC_DISTRIBUTION_VLH_LDED	2
#define			ENC_DISTRIBUTION_UP_ELVTR	3
#define			ENC_DISTRIBUTION_DWN_ELVTR	4
#define			ENC_DISTRIBUTION_EX_BAL		5

#define			ENC_DMSEL_DFE5				0
#define			ENC_DMSEL_DFE2				1
#define			ENC_DMSEL_NRZ				2
#define			ENC_DMSEL_NRZLP				3

#define			ENC_SLEW_32PS				0
#define			ENC_SLEW_50PS				3
#define			ENC_SLEW_54PS				5
#define			ENC_SLEW_88PS				7

#define			ENC_FM_BUTTON_STATE_ALL		0
#define			ENC_FM_BUTTON_STATE_NONE	1
#define			ENC_FM_BUTTON_STATE_PORT1	2
#define			ENC_FM_BUTTON_STATE_FM_ENABLED 3

#define			ENC_CRC_16b					0
#define			ENC_CRC_14b_16b				1
#define			ENC_CRC_48b_16b				2
#define			ENC_CRC_48b_14b_16b			3
#define			ENC_CRC_PERLANE_16b			4
#define			ENC_CRC_PERLANE_14b_16b		5
#define			ENC_CRC_PERLANE_48b_16b		6
#define			ENC_CRC_PERLANE_48b_14b_16b	7

#define			SYSMETATABLESIZE			32
#define			PORTMETATABLESIZE			43

#define			DT_INTVAL					1
#define			DT_ASCII_STRING				2
#define			DT_NUMERIC_STRING			3

#define			NODE_GUID					1
#define			SYSTEM_IMAGE_GUID			2

/***********************
 *   DATA TYPES
 ***********************/

typedef struct table_meta_data_s {
	// index is the actual index in the table
	uint16		length;
	uint16		start;
	uint16		protected;
	uint16		dataType;
	char		field[256];
	
} table_meta_data_t;

typedef struct table_parsed_data_s {
	// index is the actual index in the table
	uint32		lengthInBytes;
	uint16		dataType;
	union {
		uint8		*arrayPtr;
		uint32		intVal;
	} val;
} table_parsed_data_t;


/***********************
 *   EXTERNS
 ***********************/

extern table_meta_data_t systemMetaData[];
extern table_meta_data_t portMetaData[];
extern table_meta_data_t rxPresetsMetaData[];
extern table_meta_data_t txPresetsMetaData[];
extern table_meta_data_t qsfpAttenuationMetaData[];
extern table_meta_data_t variableSettingsMetaData[];
extern const int systemMetaDataSize;
extern int portMetaDataSize;
extern int rxPresetsMetaDataSize;
extern int txPresetsMetaDataSize;
extern int qsfpAttenuationMetaDataSize;
extern int variableSettingsMetaDataSize;



/***********************
 *   FUNCTION PROTOTYPES
 ***********************/

int32 getMetaDataIndexByField (table_meta_data_t *table, int tableSize, const char *field);
FSTATUS parseDataTable(table_meta_data_t *metaData, uint8 *dataBuffer, uint16 tableLen, table_parsed_data_t *parsedData, int showme);
uint32 getModuleType(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID);
uint32 getNumPorts(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID);
FSTATUS getNodeDescription(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, uint8 *nodeDesc);
FSTATUS getFmPushButtonState(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, uint32 *state);
FSTATUS getGuid(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, EUI64 *guid, int which);

#endif /* _XEDGE_METADATA_H_ */
