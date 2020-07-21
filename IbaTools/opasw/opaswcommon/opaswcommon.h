/* BEGIN_ICS_COPYRIGHT3 ****************************************

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

** END_ICS_COPYRIGHT3   ****************************************/

/* [ICS VERSION STRING: unknown] */

/***********************
 *   MACROS
 ***********************/

#ifndef _OPASW_COMMON_H_
#define _OPASW_COMMON_H_

#include "iba/stl_mad_priv.h"
#include "iba/stl_pm.h"

/***********************
 *   CONSTANTS
 ***********************/

#define QP1_WELL_KNOWN_Q_KEY					0x80010000U
#define GSI_QUEUE_PAIR							1
#define OMGT_GLOBAL_PORTID						-1
#define RESP_WAIT_TIME							(1000)
#define DEF_RETRIES								3
#define FW_RETRIES								5
#define CSS_HEADER_SIZE							644
#define OPASW_BUFFER_PAD						20

#define PS_NOT_PRESENT							0
#define PS_OFFLINE							1
#define PS_ONLINE							2
#define PS_INVALID							3

#define MCLASS_PERF								0x04			/* Mgmt Class for Performance Mgmt (used for ping) */
#define MCLASS_VENDOR_OPASW						0x0A			/* Mgmt Class for OPASW Vendor Mad protocol */
#define STL_VENDOR_SPEC_CLASS_VERSION			0x80

#define MCLASS_ATTRIB_ID_CLASSPORTINFO			0x0001			/* Class Port Info */
#define MCLASS_ATTRIB_ID_SESSION_MGMT			0x0011			/* Session management function */
#define MCLASS_ATTRIB_ID_S20_MEM_ACCESS			0x0012			/* S20 Memory Access function */
#define MCLASS_ATTRIB_ID_INI_SYS_TABLE_ACC		0x0013			/* Ini System Table Access function */
#define MCLASS_ATTRIB_ID_INI_PORT_TABLE_ACC		0x0014			/* Ini Port Table Access function */
#define MCLASS_ATTRIB_ID_INI_DESCR_GET			0x0015			/* Ini Descriptor Get function */
#define MCLASS_ATTRIB_ID_I2C_ACCESS				0x0016			/* I2C Access function */
#define MCLASS_ATTRIB_ID_QSFP_ACCESS			0x0017			/* QSFP Access function */
#define MCLASS_ATTRIB_ID_FW_VERSION				0x0018			/* FW Version function */
#define MCLASS_ATTRIB_ID_REBOOT					0x0019			/* Reboot function */
#define MCLASS_ATTRIB_ID_SAVE_CONFIG			0x001A			/* Save Configuration function */
#define MCLASS_ATTRIB_ID_DUMP					0x001B			/* Dump function */
#define MCLASS_ATTRIB_ID_LED_STATUS				0x001C			/* LED Status function */
#define MCLASS_ATTRIB_ID_PORT_BEACONING			0x001D			/* Port Beaconing function */
#define MCLASS_ATTRIB_ID_REG_ACCESS				0x001E			/* Register Access function */
#define MCLASS_ATTRIB_ID_JUMP_TO_ADDR			0x001F			/* Jump To Address function */
#define MCLASS_ATTRIB_ID_CALL_ROUTINE			0x0020			/* Call Routine function */

#define PM_ATTRIB_ID_PORT_COUNTERS				0x0012			/* PM Port Counters - used for ping */

#define VM_MAD_STATUS_SESSION_ID				0x100			/* Invalid session ID */
#define VM_MAD_STATUS_NACK						0x200			/* NACK - device is busy */
#define VM_MAD_STATUS_BUS_BUSY					0x400			/* Bus busy */
#define VM_MAD_STATUS_BUS_HUNG					0x600			/* Bus hung */
#define VM_MAD_STATUS_LOST_ARB					0x800			/* Lost ARB */
#define VM_MAD_STATUS_TIMEOUT					0xA00			/* Timeout */
#define VM_MAD_STATUS_SAVE_ERROR				0x1000			/* Save Error */

#define MEM_DATA_SIZE							200
#define SYS_TBL_DATA_SIZE						200
#define PORT_TBL_DATA_SIZE						200
#define I2C_DATA_SIZE							1024
#define QSFP_DATA_SIZE							200
#define FWVERSION_SIZE							24
#define NODE_DESC_SIZE							64
#define DNAME_SIZE								256
#define FNAME_SIZE								64

#define STL_EEPROM_COUNT						4
#define STL_MAX_TOTAL_EEPROM_SIZE				(256 * 1024) // This is the *total* size of all of the EEPROMs
#define STL_MAX_EEPROM_SIZE						(64 * 1024) // This is the size of each individual EEPROM

// TODO: Updates for PRR board specifics as needed
// TODO: Chassis EEPROM is 0xA8
// TODO: Module EEPROM is 0xAA
// TODO: PS access through Management FPGA part at 0xE2

// TODO: Update PRR FW addresses: 0xA0, 0xA2, 0xA4, 0xA6
#define STL_PRR_PRI_EEPROM1_ADDR					(uint32)0x8160A000
#define STL_PRR_SEC_EEPROM1_ADDR					(uint32)0x8161A000
#define	STL_PRR_PRI_EEPROM2_ADDR					(uint32)0x8160A200
#define STL_PRR_SEC_EEPROM2_ADDR					(uint32)0x8161A200
#define STL_PRR_PRI_EEPROM3_ADDR					(uint32)0x8160A400
#define STL_PRR_SEC_EEPROM3_ADDR					(uint32)0x8161A400
#define STL_PRR_PRI_EEPROM4_ADDR					(uint32)0x8160A600
#define STL_PRR_SEC_EEPROM4_ADDR					(uint32)0x8161A600

/*
 * PRR VMA packets use 3 bits for Port LTP CRC mode
 */
#define STL_PRR_PORT_LTP_CRC_MODE_VMA_14        0x1       /* 14-bit LTP CRC mode VMA (optional) */
#define STL_PRR_PORT_LTP_CRC_MODE_VMA_48        0x2       /* 48-bit LTP CRC mode VMA (optional) */
#define STL_PRR_PORT_LTP_CRC_MODE_VMA_12_16_PER_LANE    0x4 /* 12/16-bit per lane LTP CRC mode VMA (optional) */

#define STL_PRR_EEPROM_INI_OFFSET				(uint32)((256 * 1024) - 256) // reserved last 256 bytes in EEPROM potentially for module VPD
#define STL_PRR_I2C_LOCATION_ADDR				8

#define STL_PRR_BOARD_ID_MASK					0x1f
#define STL_PRR_BOARD_ID_EDGE48 				0x04
#define STL_PRR_BOARD_ID_EDGE24 				0x05
#define STL_PRR_BOARD_ID_HPE7K					0x09

#define I2C_OPASW_PRI_EEPROM_ADDR				(uint32)0x8000A000
#define I2C_OPASW_SEC_EEPROM_ADDR				(uint32)0x8001A000

#define I2C_OPASW_VPD_EEPROM_ADDR				(uint32)0x8160AA00

#define I2C_OPASW_PSOC_FAN_ADDR					(uint32)0x8000C200
#define I2C_OPASW_LTC2974_TEMP_ADDR				(uint32)0x80009800
#define I2C_OPASW_LTC2974_TEMP_OFFSET				(uint16)0x008D

#define I2C_OPASW_MGMT_FPGA_ADDR				(uint32)0x80407200	// used for power supply access

#define I2C_BOARD_ID_ADDR						I2C_OPASW_MGMT_FPGA_ADDR

#define I2C_OPASW_FAN_SPEED_OFFSET				0x00
#define I2C_OPASW_FAN_CONFIG_OFFSET				0x02
#define I2C_OPASW_FAN_ALARM_OFFSET				0x08
#define I2C_OPASW_FAN_COUNT_OFFSET				0x16

#define I2C_OPASW_FAN_COUNT_VALUE				0x01
#define I2C_OPASW_FAN_ALARM_VALUE				0x07
#define I2C_OPASW_FAN_CONFIG_VALUE				0x23
#define I2C_OPASW_FAN_SPEED_VALUE				0x2e

#define I2C_OPASW_MGMT_FPGA_REG_RD				0x0B
#define I2C_OPASW_MGMT_FPGA_REG_WR				0x02

#define I2C_OPASW_BD_MGMT_FPGA_OFFSET 			0x0000
#define I2C_OPASW_PS_MGMT_FPGA_OFFSET 			0x0021
#define I2C_OPASW_PRR_ASIC_TEMP_MGMT_FPGA_OFFSET 	0x00e8
#define I2C_OPASW_MAX_QSFP_TEMP_MGMT_FPGA_OFFSET 	0x00e6
#define I2C_OPASW_TEMP_SENSOR_COUNT                     3

#define PSU1_MGMT_FPGA_BIT_PRESENT               0
#define PSU1_MGMT_FPGA_BIT_PWR_OK                1
#define PSU2_MGMT_FPGA_BIT_PRESENT               2
#define PSU2_MGMT_FPGA_BIT_PWR_OK                3

#define ASIC_VERSION_MEM_ADDR					0x000012D6
#define QSFP_MGR_TEMPERATURE_MAX_DETECTED_MEM_ADDR		0x0002000E

#define ASIC_ARCH_VERSION_MASK					0x00FF0000
#define ASIC_ARCH_VERSION_SHFT					16
#define ASIC_CHIP_NAME_MASK						0x0000FF00
#define ASIC_CHIP_NAME_SHFT						8
#define ASIC_CHIP_STEP_MASK						0x000000F0
#define ASIC_CHIP_STEP_SHFT						4
#define ASIC_CHIP_REV_MASK						0x0000000F
#define ASIC_CHIP_REV_SHFT						0

#define ASIC_CHIP_STEP_A						0
#define ASIC_CHIP_STEP_B						1
#define ASIC_CHIP_STEP_C						2

#define OPASW_MODULE							1

#define DEVICE_HDR_SIZE							7 // xinfo device header size
#define RECORD_HDR_SIZE							4 // xinfo record header size
#define FRU_TYPE_SIZE							1 // size for fru type field in fruInfo record
#define FRU_HANDLE_SIZE							1 // size for fru handle field in fruInfo record
#define FRUINFO_TYPE							2 // type code for fruInfo record
#define LEN_MASK								0x3f // mask for encoded length field
#define VPD_READ_SIZE							60 // size of VPD read block

#define MAX_FAN_SPEED							25300		// 15% above the max operating speed of 22000 RPM
#define MIN_FAN_SPEED							5525		// 15% below the min operating speed of 6500 RPM
#define TOP_FAN_SPEED							(0xff * 60)	// absolute max if register remains at 0xff
#define OPASW_PSOC_FAN_CTRL_TACHS				6
#define OPASW_PSOC_FAN1_ACTUAL_SPEED_OFFSET		4

#define MAX_VENDOR_KEY_LEN						16

#define NOJUMBOMAD								0
#define JUMBOMAD								1

#define TEMP_STR_LENGTH 64
#define OEM_HASH_SIGNER_START_ADDRESS					0x12c4
#define OEM_HASH_SIGNER_END_ADDRESS					0x12cb
#define AUTHENTICATION_CONTROL_BIT_ADDRESS				0x12c3

/***********************
 *   DATA TYPES
 ***********************/

#include "iba/public/ipackon.h"

/* Vendor Specific Management MAD formats */
typedef struct _VENDOR_MAD_JUMBO {
	MAD_COMMON	common;				/* Generic MAD Header */
	
	uint8_t		VendorData[STL_IBA_GS_DATASIZE];	/* Data Payload */
} PACK_SUFFIX VENDOR_MAD_JUMBO, *PVENDOR_MAD_JUMBO;

typedef struct _VENDOR_MAD {
	MAD_COMMON	common;				/* Generic MAD Header */
	
	uint8_t		VendorData[256 - 24];	/* Data Payload */
} PACK_SUFFIX VENDOR_MAD, *PVENDOR_MAD;

#include "iba/public/ipackoff.h"

typedef struct opasw_vendor_mad_s {
	uint32_t data[56];
} opasw_vendor_mad_t;

typedef struct opasw_vendor_jmad_s {
	uint32_t data[STL_IBA_GS_DATASIZE/4];
} opasw_vendor_jmad_t;

/* MAD data payload types */

typedef struct opasw_session_mgmt_s {
	uint16		sessionID;
} opasw_session_mgmt_t;

typedef struct opasw_s20_mem_access_s {
	uint32		memoryAddr;
	uint16		dataLen;
	uint16		reserved;
	uint8		memoryData[MEM_DATA_SIZE];
} opasw_s20_mem_access_t;

typedef struct opasw_ini_sys_table_access_s {
	uint8		sysTableIndex;
	uint8		reserved;
	uint16		dataLen;
	uint32		reserved2;
	uint8		sysTableData[SYS_TBL_DATA_SIZE];
} opasw_ini_sys_table_access_t;

typedef struct opasw_ini_port_table_access_s {
	uint8		portTableIndex;
	uint8		reserved;
	uint16		portNum;
	uint32		dataLen;
	uint8		portTableData[PORT_TBL_DATA_SIZE];
} opasw_ini_port_table_access_t;

typedef struct opasw_ini_descriptor_get_s {
	uint32		sysMetaDataAddr;
	uint32		sysMetaDataLen;
	uint32		sysDataAddr;
	uint32		sysDataLen;
	uint32		portMetaDataAddr;
	uint32		portMetaDataLen;
	uint32		portDataAddr;
	uint32		portDataLen;
} opasw_ini_descriptor_get_t;

typedef struct opasw_i2c_access_s {
	uint32		locationDescriptor;
	uint16		dataLen;
	uint16		dataOffset;
	uint8		i2cData[I2C_DATA_SIZE];
	uint8		reserved2[16];
} opasw_i2c_access_t;

typedef struct opasw_qsfp_access_s {
	uint32		qsfpNumber;
	uint16		dataLen;
	uint16		dataOffset;
	uint8		qsfpData[QSFP_DATA_SIZE];
	uint8		reserved2[16];
} opasw_qsfp_access_t;

typedef struct opasw_fw_version_s {
	uint8		fwVersion[FWVERSION_SIZE];
} opasw_fw_version_t;

typedef struct opasw_reboot_s {
	uint32		delay;
} opasw_reboot_t;

typedef struct opasw_save_config_s {
	uint32		reserved;
} opasw_save_config_t;

typedef struct opasw_dump_s {
	uint32		reserved;
} opasw_dump_t;

typedef struct opasw_led_status_s {
	uint32		reserved;
} opasw_led_status_t;

typedef struct opasw_port_beaconing_s {
	uint32		reserved;
} opasw_port_beaconing_t;

typedef struct opasw_register_access_s {
	uint8		registerAddr;
	uint8		shadowIndicator;
	uint16		reserved;
	uint32		registerValue;
} opasw_register_access_t;

typedef struct opasw_jump_to_addr_s {
	uint32		jumpAddr;
	uint32		delay;
} opasw_jump_to_addr_t;

typedef struct opasw_call_routine_s {
	uint32		procedureAddr;
	uint32		param1;
	uint32		param2;
	uint32		param3;
	uint32		param4;
	uint32		returnValue;
} opasw_call_routine_t;

typedef struct vpd_fruInfo_rec_s {
	char serialNum[256];
   	char partNum[256];
   	char model[256];
   	char version[256];
   	char mfgName[256];
   	char productName[256];
   	char mfgID[3];
   	int mfgYear;
   	int mfgMonth;
   	int mfgDay;
   	int mfgHours;
   	int mfgMins;
} vpd_fruInfo_rec_t;

typedef struct pm_port_counters_s {
	uint8 reserved;
	uint8 portSelect;
} pm_port_counters_t;

static inline STL_LID get_dlid_from_path(IB_PATH_RECORD *path)
{
	if((ntoh64(path->DGID.Type.Global.InterfaceID) >> 40) ==
		OMGT_STL_OUI)
		return ntoh64(path->DGID.Type.Global.InterfaceID)
			& 0xFFFFFFFF;
	return path->DLID;
}

/***********************
 *   FUNCTION PROTOTYPES
 ***********************/

FSTATUS getDestPath(struct omgt_port *port, EUI64 destPortGuid, char *cmd, IB_PATH_RECORD *pathp);
uint16 getSessionID(struct omgt_port *port, IB_PATH_RECORD *path);
void releaseSession(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID);
FSTATUS getFwVersion(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 *fwVersion);
FSTATUS getVPDInfo(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 module, uint8 BoardID, vpd_fruInfo_rec_t *vpdInfo);
FSTATUS getFanSpeed(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 fanNum, uint32 *fanSpeed);
FSTATUS getTempReadings(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, char tempStrs[I2C_OPASW_TEMP_SENSOR_COUNT][TEMP_STR_LENGTH], uint8 BoardID);
FSTATUS getPowerSupplyStatus(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 psNum, uint32 *psStatus);
FSTATUS getAsicVersion(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 *asicVersion);
FSTATUS getMaxQsfpTemperatureMaxDetected(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, boolean *maxDetected);
FSTATUS getBoardID(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 *boardID);
FSTATUS doPingSwitch(struct omgt_port *port, IB_PATH_RECORD *path, STL_PERF_MAD *mad);
FSTATUS getOemHash(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 *oemhash, uint32 *acb);
FSTATUS getEMFWFileNames(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, char *fwFileName, char *inibinFileName);
FSTATUS	getBinaryHash(char *fwFileName, uint32 *binaryHash);

void opaswDisplayBuffer(char *buffer, int dataLen);

FSTATUS sendClassPortInfoMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad);
FSTATUS sendSessionMgmtGetMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 *sessionID);
FSTATUS sendSessionMgmtReleaseMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID);
FSTATUS sendMemAccessGetMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 memAddr, uint8 dataLen, uint8 *memData);
FSTATUS sendIniDescriptorGetMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, opasw_ini_descriptor_get_t *descriptors);
FSTATUS sendSysTableAccessGetMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 sysTableIndex, uint8 dataLen, uint8 *sysTableData);
FSTATUS sendSysTableAccessSetMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 sysTableIndex, uint8 dataLen, uint8 *sysTableData);
FSTATUS sendPortTableAccessSetMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 portTableIndex, uint16 portNum, uint8 dataLen, uint8 *portTableData);
FSTATUS sendGetFwVersionMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 *fwVersion);
FSTATUS sendRegisterAccessMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 regAddr, uint32 *regValue, int get);
FSTATUS sendRebootMad(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, VENDOR_MAD *mad, uint32 delay);
FSTATUS sendI2CAccessMad(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, void *mad, uint8 jumbo, uint8 method, int timeout, uint32 locationDescriptor, uint16 dataLen, 
						 uint16 dataOffset, uint8 *data);
FSTATUS sendSTLPortStatsPort1Mad(struct omgt_port *port, IB_PATH_RECORD *path, STL_PERF_MAD *mad);
FSTATUS sendSaveConfigMad(struct omgt_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID);
uint16 getMadStatus(VENDOR_MAD *mad);
void displayStatusMessage(uint16 madStatus);

FSTATUS opaswEepromRW(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID, void *mad, int timeout, uint32 len, uint32 offset, uint8 *data, boolean writeData, boolean secondary);

const char* StlPortLtpCrcModeVMAToText(uint16_t mode, char *str, size_t len);

#endif /* _OPASW_COMMON_H_ */
