/* BEGIN_ICS_COPYRIGHT3 ****************************************

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

** END_ICS_COPYRIGHT3   ****************************************/

/* [ICS VERSION STRING: unknown] */

/***********************
 *   MACROS
 ***********************/

#ifndef _OPASW_COMMON_H_
#define _OPASW_COMMON_H_

#if !defined(VXWORKS) || (defined(STL_GEN) && (STL_GEN >= 1))
#include "iba/stl_mad.h"
#else
#include "iba/ib_mad.h"
#endif

/***********************
 *   CONSTANTS
 ***********************/

#define QP1_WELL_KNOWN_Q_KEY					0x80010000U
#define GSI_QUEUE_PAIR							1
#define OIB_GLOBAL_PORTID						-1
#define RESP_WAIT_TIME							(1000)
#define DEF_RETRIES								3
#define FW_RETRIES								5
#define CSS_HEADER_SIZE							644
#define OPASW_BUFFER_PAD						20

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


#define STL_PRR_EEPROM_INI_OFFSET				(uint32)((256 * 1024) - 256) // reserved last 256 bytes in EEPROM potentially for module VPD
#define STL_PRR_I2C_LOCATION_ADDR				8

#define I2C_OPASW_PRI_EEPROM_ADDR				(uint32)0x8000A000
#define I2C_OPASW_SEC_EEPROM_ADDR				(uint32)0x8001A000

#define I2C_OPASW_VPD_EEPROM_ADDR				(uint32)0x8160AA00

#define I2C_OPASW_PSOC_FAN_ADDR					(uint32)0x8000C200

// TODO: MGMT FPGA has a non standard I2C specification, may need to change 0x20- to 0x04-
#define I2C_OPASW_PS_ADDR 						(uint32)0x20007200

#define I2C_BOARD_ID_ADDR						(uint32)0x8000B200

#define I2C_OPASW_FAN_SPEED_OFFSET				0x00
#define I2C_OPASW_FAN_CONFIG_OFFSET				0x02
#define I2C_OPASW_FAN_ALARM_OFFSET				0x08
#define I2C_OPASW_FAN_COUNT_OFFSET				0x16

#define I2C_OPASW_FAN_COUNT_VALUE				0x01
#define I2C_OPASW_FAN_ALARM_VALUE				0x07
#define I2C_OPASW_FAN_CONFIG_VALUE				0x23
#define I2C_OPASW_FAN_SPEED_VALUE				0x2e

// TODO: MGMT FPGA has a non standard I2C specification, may need to change 0x00- to 0x0b-
#define I2C_OPASW_PS_MGMT_FPGA_OFFSET 			0x0021

#define PSU1_MGMT_FPGA_BIT_PRESENT               0
#define PSU1_MGMT_FPGA_BIT_PWR_OK                1
#define PSU2_MGMT_FPGA_BIT_PRESENT               2
#define PSU2_MGMT_FPGA_BIT_PWR_OK                3

#define ASIC_VERSION_MEM_ADDR					0x00001040

#define OPASW_MODULE							1

#define DEVICE_HDR_SIZE							7 // xinfo device header size
#define RECORD_HDR_SIZE							4 // xinfo record header size
#define FRU_TYPE_SIZE							1 // size for fru type field in fruInfo record
#define FRU_HANDLE_SIZE							1 // size for fru handle field in fruInfo record
#define FRUINFO_TYPE							2 // type code for fruInfo record
#define LEN_MASK								0x3f // mask for encoded length field
#define VPD_SIZE								256 // size of VPD block
#define VPD_READ_SIZE							60 // size of VPD read block

#define MAX_FAN_SPEED							25300		// 15% above the max operating speed of 22000 RPM
#define MIN_FAN_SPEED							5525		// 15% below the min operating speed of 6500 RPM
#define TOP_FAN_SPEED							(0xff * 60)	// absolute max if register remains at 0xff
#define OPASW_PSOC_FAN_CTRL_TACHS				6
#define OPASW_PSOC_FAN1_ACTUAL_SPEED_OFFSET		4

#define MAX_VENDOR_KEY_LEN						16

#define NOJUMBOMAD								0
#define JUMBOMAD								1

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
	uint32		ffeMetaDataAddr;
	uint32		ffeMetaDataLen;
	uint32		ffeDataAddr;
	uint32		ffeDataLen;
	uint32		ffeIndirectMetaDataAddr;
	uint32		ffeIndirectMetaDataLen;
	uint32		ffeIndirectDataAddr;
	uint32		ffeIndirectDataLen;
	uint32		qsfpMetaDataAddr;
	uint32		qsfpMetaDataLen;
	uint32		qsfpDataAddr;
	uint32		qsfpDataLen;
	uint32		customMetaDataAddr;
	uint32		customMetaDataLen;
	uint32		customDataAddr;
	uint32		customDataLen;
	uint32		dfeMetaDataAddr;
	uint32		dfeMetaDataLen;
	uint32		dfeDataAddr;
	uint32		dfeDataLen;
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



/***********************
 *   FUNCTION PROTOTYPES
 ***********************/

FSTATUS getDestPath(struct oib_port *port, EUI64 destPortGuid, char *cmd, IB_PATH_RECORD *pathp);
uint16 getSessionID(struct oib_port *port, IB_PATH_RECORD *path);
void releaseSession(struct oib_port *port, IB_PATH_RECORD *path, uint16 sessionID);
FSTATUS getFwVersion(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 *fwVersion);
FSTATUS getVPDInfo(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 module, vpd_fruInfo_rec_t *vpdInfo);
FSTATUS getFanSpeed(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 fanNum, uint32 *fanSpeed);
FSTATUS getPowerSupplyStatus(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 psNum, uint32 *psStatus);
FSTATUS getAsicVersion(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint16 *asicVersion);
FSTATUS getBoardID(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 *boardID);
FSTATUS doPingSwitch(struct oib_port *port, IB_PATH_RECORD *path, STL_PERF_MAD *mad);
FSTATUS getEMFWFileNames(struct oib_port *port, IB_PATH_RECORD *path, uint16 sessionID, char *fwFileName, char *inibinFileName);

void opaswDisplayBuffer(char *buffer, int dataLen);

FSTATUS sendClassPortInfoMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad);
FSTATUS sendSessionMgmtGetMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 *sessionID);
FSTATUS sendSessionMgmtReleaseMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID);
FSTATUS sendMemAccessGetMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint32 memAddr, uint8 dataLen, uint8 *memData);
FSTATUS sendIniDescriptorGetMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, opasw_ini_descriptor_get_t *descriptors);
FSTATUS sendSysTableAccessGetMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 sysTableIndex, uint8 dataLen, uint8 *sysTableData);
FSTATUS sendSysTableAccessSetMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 sysTableIndex, uint8 dataLen, uint8 *sysTableData);
FSTATUS sendPortTableAccessSetMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 portTableIndex, uint16 portNum, uint8 dataLen, uint8 *portTableData);
FSTATUS sendGetFwVersionMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 *fwVersion);
FSTATUS sendRegisterAccessMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID, uint8 regAddr, uint32 *regValue, int get);
FSTATUS sendRebootMad(struct oib_port *port, IB_PATH_RECORD *path, uint16 sessionID, VENDOR_MAD *mad, uint32 delay);
FSTATUS sendI2CAccessMad(struct oib_port *port, IB_PATH_RECORD *path, uint16 sessionID, void *mad, uint8 jumbo, uint8 method, int timeout, uint32 locationDescriptor, uint16 dataLen, 
						 uint16 dataOffset, uint8 *data);
FSTATUS sendPortStatsPort1Mad(struct oib_port *port, IB_PATH_RECORD *path, PERF_MAD *mad);
FSTATUS sendSTLPortStatsPort1Mad(struct oib_port *port, IB_PATH_RECORD *path, STL_PERF_MAD *mad);
FSTATUS sendSaveConfigMad(struct oib_port *port, IB_PATH_RECORD *path, VENDOR_MAD *mad, uint16 sessionID);
uint16 getMadStatus(VENDOR_MAD *mad);
void displayStatusMessage(uint16 madStatus);

FSTATUS opaswEepromRW(struct oib_port *port, IB_PATH_RECORD *path, uint16 sessionID, void *mad, int timeout, uint32 len, uint32 offset, uint8 *data, boolean writeData, boolean secondary);

#endif /* _OPASW_COMMON_H_ */
