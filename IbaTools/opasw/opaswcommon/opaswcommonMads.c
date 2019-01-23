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

/* work around conflicting names */

#include "iba/stl_types.h"
#include "iba/stl_sm_priv.h"
#include "iba/stl_pm.h"
#include "iba/stl_helper.h"
#include "opamgt_priv.h"
#include <iba/ibt.h>
#include "opaswcommon.h"

#define NEED_TO_RETRY(madStatus)	((madStatus == MAD_STATUS_BUSY) || (madStatus == VM_MAD_STATUS_NACK) || (madStatus == VM_MAD_STATUS_BUS_BUSY) || (madStatus == VM_MAD_STATUS_BUS_HUNG) || (madStatus == VM_MAD_STATUS_LOST_ARB) || (madStatus == VM_MAD_STATUS_TIMEOUT))

// globals

static int				g_transID = 0x0114;

extern int				g_debugMode;
extern int				g_verbose;

FSTATUS sendClassPortInfoMad(struct omgt_port *port, 
							 IB_PATH_RECORD *path, 
							 VENDOR_MAD *mad)
{
	FSTATUS				status = FSUCCESS;
	uint16				madStatus;
    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_GET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_CLASSPORTINFO;
	mad->common.AttributeModifier = 0;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Send mad & recv response

	if (g_debugMode) {
		printf("send buffer for ClassPortInfo:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}

    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (g_debugMode) {
		printf("recv buffer for ClassPortInfo rsp:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}
	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	return(status);
}

FSTATUS sendSessionMgmtGetMad(struct omgt_port *port, 
							  IB_PATH_RECORD *path, 
							  VENDOR_MAD *mad, 
							  uint16 *sessionID)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_session_mgmt_t *sessionMgmtDataP;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_GET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_SESSION_MGMT;
	mad->common.AttributeModifier = 0;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	sessionMgmtDataP = (opasw_session_mgmt_t *)&vendorData->data[0];

	// Send mad & recv response

	if (g_debugMode) {
		printf("send buffer for session mgmt:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}

    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (g_debugMode) {
		printf("recv buffer for session mgmt rsp:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}
	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}

	*sessionID = ntoh16(sessionMgmtDataP->sessionID);
	return(status);
}

FSTATUS sendSessionMgmtReleaseMad(struct omgt_port *port, 
								  IB_PATH_RECORD *path, 
								  VENDOR_MAD *mad, 
								  uint16 sessionID)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_session_mgmt_t *sessionMgmtDataP;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_SET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_SESSION_MGMT;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	sessionMgmtDataP = (opasw_session_mgmt_t *)&vendorData->data[0];
	sessionMgmtDataP->sessionID = sessionID;

	// Send mad & recv response

	if (g_debugMode) {
		printf("send buffer for session release:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}

    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (g_debugMode) {
		printf("recv buffer for session release rsp:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}
	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	return(status);
}

FSTATUS sendMemAccessGetMad(struct omgt_port *port, 
							IB_PATH_RECORD *path, 
							VENDOR_MAD *mad, 
							uint16 sessionID, 
							uint32 memAddr, 
							uint8 dataLen, 
							uint8 *memData)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_s20_mem_access_t *memoryDataP;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_GET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_S20_MEM_ACCESS;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	memoryDataP = (opasw_s20_mem_access_t *)&vendorData->data[0];
	memoryDataP->memoryAddr = ntoh32(memAddr);
	memoryDataP->dataLen = ntoh16(dataLen);

	// Send mad & recv response

	if (g_debugMode) {
		printf("send buffer for mem access:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}

    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (g_debugMode) {
		printf("recv buffer for mem access rsp:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}
	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	// copy the data

	memcpy(memData, memoryDataP->memoryData, dataLen);

	return(status);

}

FSTATUS sendSysTableAccessGetMad(struct omgt_port *port, 
								 IB_PATH_RECORD *path, 
								 VENDOR_MAD *mad, 
								 uint16 sessionID, 
								 uint8 sysTableIndex, 
								 uint8 dataLen, 
								 uint8 *sysTableData)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_ini_sys_table_access_t *sysTableP;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
#if GET
	mad->common.mr.s.Method = MMTHD_GET;
#else
	mad->common.mr.s.Method = MMTHD_SET;
#endif
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_INI_SYS_TABLE_ACC;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	sysTableP = (opasw_ini_sys_table_access_t *)&vendorData->data[0];
	sysTableP->sysTableIndex = sysTableIndex;
	sysTableP->dataLen = ntoh16(dataLen);
	memcpy(sysTableP->sysTableData, sysTableData, dataLen);

	// Send mad & recv response

	if (g_debugMode) {
		printf("send buffer for in sys table access:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}

    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (g_debugMode) {
		printf("recv buffer for in sys table access rsp:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}
	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

#if GET
	// copy the data
	memcpy(sysTableData, sysTableP->sysTableData, dataLen);
#endif

	return(status);

}

FSTATUS sendSysTableAccessSetMad(struct omgt_port *port, 
								 IB_PATH_RECORD *path, 
								 VENDOR_MAD *mad, 
								 uint16 sessionID, 
								 uint8 sysTableIndex, 
								 uint8 dataLen, 
								 uint8 *sysTableData)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_ini_sys_table_access_t *sysTableP;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_SET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_INI_SYS_TABLE_ACC;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	sysTableP = (opasw_ini_sys_table_access_t *)&vendorData->data[0];
	sysTableP->sysTableIndex = sysTableIndex;
	sysTableP->dataLen = ntoh16(dataLen);
	memcpy(sysTableP->sysTableData, sysTableData, dataLen);

	// Send mad & recv response

	// opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));

    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	return(status);

}
FSTATUS sendPortTableAccessSetMad(struct omgt_port *port, 
								  IB_PATH_RECORD *path, 
								  VENDOR_MAD *mad, 
								  uint16 sessionID, 
								  uint8 portTableIndex, 
								  uint16 portNum, 
								  uint8 dataLen, 
								  uint8 *portTableData)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_ini_port_table_access_t *portTableP;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
#if GET
	mad->common.mr.s.Method = MMTHD_GET;
#else
	mad->common.mr.s.Method = MMTHD_SET;
#endif
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_INI_PORT_TABLE_ACC;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	portTableP = (opasw_ini_port_table_access_t *)&vendorData->data[0];
	portTableP->portTableIndex = portTableIndex;
	portTableP->portNum = ntoh16(portNum);
	portTableP->dataLen = ntoh32(dataLen);
	memcpy(portTableP->portTableData, portTableData, dataLen);

	// Send mad & recv response

	if (g_debugMode) {
		printf("send buffer for port table access:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}

    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (g_debugMode) {
		printf("recv buffer for port table access rsp:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}
	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	return(status);

}

FSTATUS sendIniDescriptorGetMad(struct omgt_port *port, 
								IB_PATH_RECORD *path, 
								VENDOR_MAD *mad, 
								uint16 sessionID, 
								opasw_ini_descriptor_get_t *descriptors)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_ini_descriptor_get_t *iniDescriptorData;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_GET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_INI_DESCR_GET;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	iniDescriptorData = (opasw_ini_descriptor_get_t *)&vendorData->data[0];

	// Send mad & recv response

	// opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	// harvest the descriptors - need to byte swap

	descriptors->sysMetaDataAddr =					ntoh32(iniDescriptorData->sysMetaDataAddr);
	descriptors->sysMetaDataLen  =					ntoh32(iniDescriptorData->sysMetaDataLen);
 	descriptors->sysDataAddr =						ntoh32(iniDescriptorData->sysDataAddr);
	descriptors->sysDataLen =						ntoh32(iniDescriptorData->sysDataLen);
	descriptors->portMetaDataAddr =					ntoh32(iniDescriptorData->portMetaDataAddr);
	descriptors->portMetaDataLen =					ntoh32(iniDescriptorData->portMetaDataLen);
	descriptors->portDataAddr =						ntoh32(iniDescriptorData->portDataAddr);
	descriptors->portDataLen =						ntoh32(iniDescriptorData->portDataLen);
#if 0
	descriptors->rxPresetAddr = 					ntoh32(iniDescriptorData->rxPresetAddr);
	descriptors->rxPresetLen = 						ntoh32(iniDescriptorData->rxPresetLen);
	descriptors->txPresetAddr = 					ntoh32(iniDescriptorData->txPresetAddr);
	descriptors->txPresetLen = 						ntoh32(iniDescriptorData->txPresetLen);
	descriptors->qsfpAttenuationAddr = 				ntoh32(iniDescriptorData->qsfpAttenuationAddr);
	descriptors->qsfpAttenuationLen = 				ntoh32(iniDescriptorData->qsfpAttenuationLen);
	descriptors->variableSettingsAddr = 			ntoh32(iniDescriptorData->variableSettingsAddr);
	descriptors->variableSettingsLen = 				ntoh32(iniDescriptorData->variableSettingsLen);
#endif


	return(status);

}

FSTATUS sendGetFwVersionMad(struct omgt_port *port, 
							IB_PATH_RECORD *path, 
							VENDOR_MAD *mad, 
							uint16 sessionID, 
							uint8 *fwVersion)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_fw_version_t *fwVersionP;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_GET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_FW_VERSION;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	fwVersionP = (opasw_fw_version_t *)&vendorData->data[0];

	// Send mad & recv response

	// opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	// copy the fw version from the MAD
	memcpy(fwVersion, fwVersionP->fwVersion, FWVERSION_SIZE);

	return(status);
}

FSTATUS sendRegisterAccessMad(struct omgt_port *port, 
							  IB_PATH_RECORD *path, 
							  VENDOR_MAD *mad, 
							  uint16 sessionID, 
							  uint8 regAddr, 
							  uint32 *regValue, 
							  int get)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_register_access_t *regAccessP;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	if (get)
		mad->common.mr.s.Method = MMTHD_GET;
	else
		mad->common.mr.s.Method = MMTHD_SET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_REG_ACCESS;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	regAccessP = (opasw_register_access_t *)&vendorData->data[0];
	regAccessP->registerAddr = regAddr;
	if (!get)
		regAccessP->registerValue = *regValue;

	// Send mad & recv response

	// opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	if (get)
		*regValue = regAccessP->registerValue;

	return(status);
}

FSTATUS sendRebootMad(struct omgt_port *port, 
					  IB_PATH_RECORD *path, 
					  uint16 sessionID, 
					  VENDOR_MAD *mad, 
					  uint32 delay)
{
	FSTATUS				status;
	opasw_vendor_mad_t	*vendorData;
	opasw_reboot_t		*rebootData;
#ifndef RESET_NORSP
	uint16				madStatus;
#endif

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_SET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_REBOOT;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	rebootData = (opasw_reboot_t *)&vendorData->data[0];
	rebootData->delay = ntoh32(delay);

	// Send mad & recv response

	if (g_debugMode) {
		printf("send buffer for reboot:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}
#ifndef RESET_NORSP
    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);
	if (g_debugMode) {
		printf("recv buffer for reboot rsp:\n");
		opaswDisplayBuffer((char *)mad, sizeof(VENDOR_MAD));
	}
	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}
#else
    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD status is %d\n", status);
		return(status);
	}
#endif

	return(status);
}

FSTATUS sendI2CAccessMad(struct omgt_port *port,
						 IB_PATH_RECORD *path, 
						 uint16 sessionID, 
						 void *mp, 
						 uint8 jumbo, /* set to 1 if mad is a jumbo mad */
						 uint8 method, 
						 int timeout, 
						 uint32 locationDescriptor, 
						 uint16 dataLen, 
						 uint16 dataOffset, 
						 uint8 *data)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_i2c_access_t	*i2cAccessData;
	uint16				madStatus;
	int					retries = 0;;
	int					done = 0;
	VENDOR_MAD			*mad = (VENDOR_MAD *)mp;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	while (!done) {

		// MAD header
		memset(mad, 0, sizeof(VENDOR_MAD));
		mad->common.BaseVersion = STL_BASE_VERSION;
		mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
		mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
		mad->common.mr.AsReg8 = 0;
		mad->common.mr.s.Method = method;
#ifdef IB_STACK_IBACCESS
		mad->common.TransactionID = (++g_transID)<<24;
#else
		mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
		mad->common.AttributeID = MCLASS_ATTRIB_ID_I2C_ACCESS;
		mad->common.AttributeModifier = sessionID;
		/* ??? set bit to indicate jumbo MAD here??? */
#if 0
		if (jumbo)
			mad->common.AttributeModifier ^= (1 << 31);
#endif

		BSWAP_MAD_HEADER((MAD*)mad);

		// Vendor data
		vendorData = (opasw_vendor_mad_t *)mad->VendorData;
		i2cAccessData = (opasw_i2c_access_t *)&vendorData->data[0];
		i2cAccessData->locationDescriptor = ntoh32(locationDescriptor);
		i2cAccessData->dataLen = ntoh16(dataLen);
		i2cAccessData->dataOffset = ntoh16(dataOffset);
		if (method == MMTHD_SET)
			memcpy(i2cAccessData->i2cData, data, dataLen);

		// Send mad & recv response

		if (g_debugMode) {
			printf("send buffer for in i2c access:\n");
			opaswDisplayBuffer((char *)mad, jumbo ? sizeof(VENDOR_MAD_JUMBO) : sizeof(VENDOR_MAD));
		}

        recv_size = sizeof(*mad);
		status = omgt_send_recv_mad_no_alloc(port, 
											(uint8_t *)mad, sizeof(*mad), 
											&addr,
											(uint8_t *)mad, &recv_size,
											timeout, 
											FW_RETRIES);

		if (g_debugMode) {
			printf("recv buffer for in i2c access rcp:\n");
			opaswDisplayBuffer((char *)mad, jumbo ? sizeof(VENDOR_MAD_JUMBO) : sizeof(VENDOR_MAD));
		}
		if (FSUCCESS != status) {
			fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
			return(status);
		}
		if ((madStatus = getMadStatus(mad)) > 0) {
			if (g_verbose)
				printf("non-zero madStatus received: 0x%04x\n", madStatus);
			if (NEED_TO_RETRY(madStatus)) {
				if (++retries > 9) {
					displayStatusMessage(madStatus);
					status = FINVALID_OPERATION;
					done = 1;
				} else {
					usleep(250000);
					continue;
				}
			} else {
				displayStatusMessage(madStatus);
				return(status = FINVALID_OPERATION);
			}
		} else
			done = 1;
	}

	if (method == MMTHD_GET)
		memcpy(data, i2cAccessData->i2cData, dataLen);
	return(status);
}

FSTATUS sendSaveConfigMad(struct omgt_port *port,
						  IB_PATH_RECORD *path, 
						  VENDOR_MAD *mad, 
						  uint16 sessionID)
{
	FSTATUS				status = FSUCCESS;
	opasw_vendor_mad_t	*vendorData;
	opasw_save_config_t *saveConfigP;
	uint16				madStatus;

    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		sl : path->u2.s.SL,
		lid : path->DLID,
		qpn : GSI_QUEUE_PAIR,
		qkey : QP1_WELL_KNOWN_Q_KEY,
		pkey : pkey,
	};
    size_t recv_size;

	// MAD header
	memset(mad, 0, sizeof(VENDOR_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_VENDOR_OPASW;
	mad->common.ClassVersion = STL_VENDOR_SPEC_CLASS_VERSION;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_SET;
#ifdef IB_STACK_IBACCESS
	mad->common.TransactionID = (++g_transID)<<24;
#else
	mad->common.TransactionID = (++g_transID) & 0xffffffff;
#endif
	mad->common.AttributeID = MCLASS_ATTRIB_ID_SAVE_CONFIG;
	mad->common.AttributeModifier = sessionID;

	BSWAP_MAD_HEADER((MAD*)mad);

	// Vendor data
	vendorData = (opasw_vendor_mad_t *)mad->VendorData;
	saveConfigP = (opasw_save_config_t *)&vendorData->data[0];
	saveConfigP->reserved = 0;

	// Send mad & recv response
    recv_size = sizeof(*mad);
	status = omgt_send_recv_mad_no_alloc(port, 
										(uint8_t *)mad, sizeof(*mad), 
										&addr,
										(uint8_t *)mad, &recv_size,
										RESP_WAIT_TIME, 
										DEF_RETRIES);

	if (FSUCCESS != status) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return(status);
	}
	if ((madStatus = getMadStatus(mad)) > 0) {
		displayStatusMessage(madStatus);
		return(status = FINVALID_OPERATION);
	}

	return(status);
}

FSTATUS sendSTLPortStatsPort1Mad(struct omgt_port *port, IB_PATH_RECORD *path, STL_PERF_MAD *mad)
{
	FSTATUS status = FSUCCESS;
	STL_DATA_PORT_COUNTERS_REQ *portCounters;
	size_t recv_size = sizeof(*mad);


    // Determine which pkey to use (full or limited)
    uint16_t pkey = omgt_get_mgmt_pkey(port, get_dlid_from_path(path), 0);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		.lid = path->DLID,
		.qpn = GSI_QUEUE_PAIR,
		.qkey = QP1_WELL_KNOWN_Q_KEY,
		.pkey = pkey,
		.sl = path->u2.s.SL
	};

	memset(mad, 0, sizeof(STL_PERF_MAD));
	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.ClassVersion = STL_PM_CLASS_VERSION;
	mad->common.MgmtClass = MCLASS_PERF;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_GET;
	mad->common.TransactionID = ++g_transID;
	mad->common.AttributeID = STL_PM_ATTRIB_ID_DATA_PORT_COUNTERS;

	mad->common.AttributeModifier = 1<<24;

	portCounters = (STL_DATA_PORT_COUNTERS_REQ *)&(mad->PerfData);
	portCounters->PortSelectMask[3] = 1;
	
	
	BSWAP_MAD_HEADER((MAD*)mad);
	
	status = omgt_send_recv_mad_no_alloc(port, (uint8_t *)mad, sizeof(*mad), &addr,
										(uint8_t *)mad, &recv_size, RESP_WAIT_TIME, 0);
	BSWAP_MAD_HEADER((MAD*)mad);

	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to send MAD or receive response MAD status is %d\n", status);
		return status;
	}
	
	return status;
}
