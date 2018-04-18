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

#include <opamgt_priv.h>

#include "opasmaquery.h"


#define MSG_PMA_SEND_RECV_FAILURE	"failed PMA Send or Receive"

static FSTATUS perform_stl_pma_query(uint8 method, uint16 attrid, uint32 attrmod, argrec *args, STL_PERF_MAD *mad, size_t send_size)
{
	FSTATUS status;
	STL_LID dlid;
	uint8_t sl = args->sl;
	uint8_t port_state;
	if (sl == 0xff) {
		// check if sl has been set, if not use default sl (SM SL)
		(void)omgt_port_get_port_sm_sl(args->omgt_port, &sl);
	}

	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.ClassVersion = STL_PM_CLASS_VERSION;
	mad->common.MgmtClass = MCLASS_PERF;
	mad->common.u.NS.Status.AsReg16 = 0; 
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = method;
	mad->common.AttributeID = attrid;
	mad->common.TransactionID = (++g_transactID);
	mad->common.AttributeModifier = attrmod;

	(void)omgt_port_get_port_state(args->omgt_port, &port_state);
	if (port_state != IB_PORT_ACTIVE) {
		uint8_t port_num;
		char hfi_name[IBV_SYSFS_NAME_MAX] = {0};
		(void)omgt_port_get_hfi_port_num(args->omgt_port, &port_num);
		(void)omgt_port_get_hfi_name(args->omgt_port, hfi_name);
		fprintf(stderr, "WARNING port (%s:%d) is not ACTIVE!\n",
			hfi_name, port_num);
		dlid = args->dlid ? args->dlid : STL_LID_PERMISSIVE; // perm lid for local query
	} else {
		dlid = args->dlid ? args->dlid : args->slid; // use slid for local query
	}

    // Determine which pkey to use (full or limited)
    // Attempt to use full at all times, otherwise, can
    // use the limited for queries of the local port.
    uint16_t pkey = omgt_get_mgmt_pkey(args->omgt_port, args->dlid, args->drpaths);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	if (g_verbose) {
		PrintFunc(&g_dest, "Sending MAD to 0x%08x:\n", dlid);
		PrintMadHeader(&g_dest, 2, &mad->common);
		PrintSeparator(&g_dest);
	}

	BSWAP_MAD_HEADER((MAD*)mad);
	{
		struct omgt_mad_addr addr = {
			lid  : dlid,
			qpn  : 1,
			qkey : QP1_WELL_KNOWN_Q_KEY,
			pkey : pkey,
            sl   : sl
		};
        size_t recv_size = sizeof(*mad);
		status = omgt_send_recv_mad_no_alloc(args->omgt_port, (uint8_t *)mad, send_size+sizeof(MAD_COMMON), &addr,
											(uint8_t *)mad, &recv_size, RESP_WAIT_TIME, 0);
	}
	BSWAP_MAD_HEADER((MAD*)mad);
	if (status == FSUCCESS && g_verbose) {
		PrintFunc(&g_dest, "Received MAD:\n");
		PrintMadHeader(&g_dest, 2, &mad->common);
		PrintSeparator(&g_dest);
	}

	if (FSUCCESS == status && mad->common.u.NS.Status.AsReg16 != MAD_STATUS_SUCCESS) {
		fprintf(stderr, "MAD returned with Bad Status: %s\n",
						iba_mad_status_msg2(mad->common.u.NS.Status.AsReg16));
		return FERROR;
	}
	return status;
} //perform_stl_pma_query

static int getNumPortsFromSelMask(uint64 portSelectMask[])
{
	int i = 0;
	int numPorts = 0;

	while (i < 4) {
		uint64 temp = portSelectMask[i++];

		while (temp) {
			if ( temp & (uint64)1 ) ++numPorts;
			temp>>=1;
		}
	}

	return(numPorts);
}

// get portSelect value to use for port counter query/clear
static uint8 pma_get_portNum(argrec *args)
{
	uint8 port;
	if (args->mflag) {
		// TBD - verify dport is valid:
		// for Switch: 0-maxPort or ALL_PORT_SELECT
		// for CA: must match port number of dlid (perhaps default it to that)
		port = args->dport;
	} else if (! args->dlid) {
		// local query, use same port as local HFI port for query
		port = args->port;
	} else {
		fprintf(stderr, "%s: Error: -m option required for -o %s\n", g_cmdname, args->oname);
		Usage(FALSE);	// exits application
		port = 0;	// keep compiler happy
	}
	VRBSE_PRINT("PortSelect=%u\n", port);
	return port;
}

static void getStlPortSelectMask(uint64 *dest, argrec *args)
{
	if (g_portSelectMask[3] || g_portSelectMask[2] ||
		g_portSelectMask[1] || g_portSelectMask[0]) {
		// ok as is
		memcpy(dest, g_portSelectMask, sizeof(g_portSelectMask));
	} else if (! args->dlid) {
		// local query, use same port as local HFI port for query
		memset(dest, 0, sizeof(uint64) * 4);
		dest[3] = (1 << args->port);
	} else {
		fprintf(stderr, "Error: portSelectMask must be non-zero - must select at least one port\n");
		Usage(FALSE);	// exits application
	}
	VRBSE_PRINT("PortSelectMask=0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64" 0x%016"PRIx64"\n",
				dest[0], dest[1], dest[2], dest[3]);
}

#if 0
static boolean checkStlVLSelectMask(uint64 VLSelectMask)
{
	int i;

	/* check that only bits 0-7 and 15 can be set */
	for (i = 8; i < 15; i++) {
		if ((VLSelectMask >> i) & 1) {
			fprintf(stderr, "Error: VLSelectMask cannot select VL %d - invalid\n", i);
			return(FALSE);
		}
	}
	for (i = 16; i < 63; i++) {
		if ((VLSelectMask >> i) & 1) {
			fprintf(stderr, "Error: VLSelectMask cannot select VL %d - invalid\n", i);
			return(FALSE);
		}
	}
	return(TRUE);
}
#endif

static boolean stl_pma_get_class_port_info(argrec *args, uint8_t *pm, size_t pm_len, boolean print)
{
	STL_PERF_MAD *mad = (STL_PERF_MAD*)pm;
	FSTATUS status = FSUCCESS;

	STL_CLASS_PORT_INFO *pClassPortInfo = (STL_CLASS_PORT_INFO *)&(mad->PerfData);
	MemoryClear(mad, sizeof(*mad));

	if ( FSUCCESS != (status = perform_stl_pma_query (MMTHD_GET, STL_PM_ATTRIB_ID_CLASS_PORTINFO, 0, args, mad, 0))) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_PMA_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		BSWAP_STL_CLASS_PORT_INFO(pClassPortInfo );
		if (print)
			PrintStlClassPortInfo(&g_dest, 0 /* indent */, pClassPortInfo, MCLASS_PERF);
		return TRUE;
	}
} //stl_pma_get_class_port_info

static boolean stl_pma_get_PortStatus(argrec *args, uint8_t *pm, size_t pm_len, boolean print)
{
	STL_PERF_MAD *mad = (STL_PERF_MAD*)pm;
	FSTATUS status = FSUCCESS;

	STL_PORT_STATUS_REQ *pStlPortStatus = (STL_PORT_STATUS_REQ *)&(mad->PerfData);
	STL_PORT_STATUS_RSP *pStlPortStatusRsp;
	MemoryClear(mad, sizeof(*mad));

	pStlPortStatus->PortNumber = pma_get_portNum(args);
	pStlPortStatus->VLSelectMask = g_vlSelectMask;
	memset(pStlPortStatus->Reserved, 0, sizeof(pStlPortStatus->Reserved));
	BSWAP_STL_PORT_STATUS_REQ(pStlPortStatus );

	if ( FSUCCESS != (status = perform_stl_pma_query (MMTHD_GET, STL_PM_ATTRIB_ID_PORT_STATUS, 0x01000000, args, mad, sizeof(STL_PORT_STATUS_REQ)))) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_PMA_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		pStlPortStatusRsp = (STL_PORT_STATUS_RSP *)pStlPortStatus;
		BSWAP_STL_PORT_STATUS_RSP(pStlPortStatusRsp);
		if (print)
			PrintStlPortStatusRsp(&g_dest, 0 /* indent */, pStlPortStatusRsp);
		return TRUE;
	}
}

static boolean stl_pma_clear_PortStatus(argrec *args, uint8_t *pm, size_t pm_len, boolean print)
{
	STL_PERF_MAD *mad = (STL_PERF_MAD*)pm;
	uint32	attrmod;
	FSTATUS status = FSUCCESS;

	STL_CLEAR_PORT_STATUS *pStlClearPortStatus = (STL_CLEAR_PORT_STATUS *)&(mad->PerfData);
	MemoryClear(mad, sizeof(*mad));

	getStlPortSelectMask(pStlClearPortStatus->PortSelectMask, args);
	attrmod = 1 << 24;
	pStlClearPortStatus->CounterSelectMask.AsReg32 = g_counterSelectMask;
	BSWAP_STL_CLEAR_PORT_STATUS_REQ(pStlClearPortStatus);

	if ( FSUCCESS != (status = perform_stl_pma_query (MMTHD_SET, STL_PM_ATTRIB_ID_CLEAR_PORT_STATUS, attrmod, args, mad, sizeof(STL_CLEAR_PORT_STATUS)))) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_PMA_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		BSWAP_STL_CLEAR_PORT_STATUS_REQ(pStlClearPortStatus);
		if (print)
			PrintStlClearPortStatus(&g_dest, 0 /* indent */, pStlClearPortStatus);
		return TRUE;
	}
}

static boolean stl_pma_get_DataCounters(argrec *args, uint8_t *pm, size_t pm_len, boolean print)
{
	STL_PERF_MAD *mad = (STL_PERF_MAD*)pm;
	uint32		 attrmod;
	FSTATUS status = FSUCCESS;

	STL_DATA_PORT_COUNTERS_REQ *pStlDataPortCountersReq = (STL_DATA_PORT_COUNTERS_REQ *)&(mad->PerfData);
	STL_DATA_PORT_COUNTERS_RSP *pStlDataPortCountersRsp;
	MemoryClear(mad, sizeof(*mad));

	getStlPortSelectMask(pStlDataPortCountersReq->PortSelectMask, args);
	attrmod = getNumPortsFromSelMask(pStlDataPortCountersReq->PortSelectMask) << 24;
	pStlDataPortCountersReq->VLSelectMask = g_vlSelectMask;
	BSWAP_STL_DATA_PORT_COUNTERS_REQ(pStlDataPortCountersReq);

	if ( FSUCCESS != (status = perform_stl_pma_query (MMTHD_GET, STL_PM_ATTRIB_ID_DATA_PORT_COUNTERS, attrmod, args, mad, sizeof(STL_DATA_PORT_COUNTERS_REQ)))) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_PMA_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		pStlDataPortCountersRsp = (STL_DATA_PORT_COUNTERS_RSP *)pStlDataPortCountersReq;
		BSWAP_STL_DATA_PORT_COUNTERS_RSP(pStlDataPortCountersRsp);
		if (print)
			PrintStlDataPortCountersRsp(&g_dest, 0 /* indent */, pStlDataPortCountersRsp);
		return TRUE;
	}
}

static boolean stl_pma_get_ErrorCounters(argrec *args, uint8_t *pm, size_t pm_len, boolean print)
{
	STL_PERF_MAD *mad = (STL_PERF_MAD*)pm;
	uint32		 attrmod;
	FSTATUS status = FSUCCESS;

	STL_ERROR_PORT_COUNTERS_REQ *pStlErrorPortCountersReq = (STL_ERROR_PORT_COUNTERS_REQ *)&(mad->PerfData);
	STL_ERROR_PORT_COUNTERS_RSP *pStlErrorPortCountersRsp;
	MemoryClear(mad, sizeof(*mad));

	getStlPortSelectMask(pStlErrorPortCountersReq->PortSelectMask, args);
	attrmod = getNumPortsFromSelMask(pStlErrorPortCountersReq->PortSelectMask) << 24;
	pStlErrorPortCountersReq->VLSelectMask = g_vlSelectMask;

	BSWAP_STL_ERROR_PORT_COUNTERS_REQ(pStlErrorPortCountersReq);

	if ( FSUCCESS != (status = perform_stl_pma_query (MMTHD_GET, STL_PM_ATTRIB_ID_ERROR_PORT_COUNTERS, attrmod, args, mad, sizeof(STL_ERROR_PORT_COUNTERS_REQ)))) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_PMA_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		pStlErrorPortCountersRsp = (STL_ERROR_PORT_COUNTERS_RSP *)pStlErrorPortCountersReq;
		BSWAP_STL_ERROR_PORT_COUNTERS_RSP(pStlErrorPortCountersRsp);
		if (print)
			PrintStlErrorPortCountersRsp(&g_dest, 0 /* indent */, pStlErrorPortCountersRsp);
		return TRUE;
	}
}

static boolean stl_pma_get_ErrorInfo(argrec *args, uint8_t *pm, size_t pm_len, boolean print)
{
	STL_PERF_MAD *mad = (STL_PERF_MAD*)pm;
	uint32		 attrmod;
	uint32 portCount = 0;
	FSTATUS status = FSUCCESS;

	STL_ERROR_INFO_REQ *pStlErrorInfoReq = (STL_ERROR_INFO_REQ *)&(mad->PerfData);
	MemoryClear(mad, sizeof(*mad));

	getStlPortSelectMask(pStlErrorInfoReq->PortSelectMask, args);
	pStlErrorInfoReq->ErrorInfoSelectMask.AsReg32 = g_counterSelectMask;
	portCount = getNumPortsFromSelMask(pStlErrorInfoReq->PortSelectMask);
	attrmod = portCount << 24;

	if (portCount == 0)
		return FALSE;

	BSWAP_STL_ERROR_INFO_REQ(pStlErrorInfoReq);

	if ( FSUCCESS != (status = perform_stl_pma_query (MMTHD_GET, STL_PM_ATTRIB_ID_ERROR_INFO, attrmod, args, mad, sizeof(STL_ERROR_INFO_REQ)))) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_PMA_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		BSWAP_STL_ERROR_INFO_RSP((STL_ERROR_INFO_RSP*)pStlErrorInfoReq);
		if (print)
			PrintStlErrorInfoRsp(&g_dest, 0 /* indent */,
			  (STL_ERROR_INFO_RSP*)pStlErrorInfoReq, 0);
		return TRUE;
	}
}

static boolean stl_pma_clear_ErrorInfo(argrec *args, uint8_t *pm, size_t pm_len, boolean print)
{
	STL_PERF_MAD *mad = (STL_PERF_MAD*)pm;
	uint32		 attrmod;
	uint32 portCount = 0;
	FSTATUS status = FSUCCESS;

	STL_ERROR_INFO_REQ *pStlErrorInfoReq = (STL_ERROR_INFO_REQ *)&(mad->PerfData);
	MemoryClear(mad, sizeof(*mad));

	getStlPortSelectMask(pStlErrorInfoReq->PortSelectMask, args);
	pStlErrorInfoReq->ErrorInfoSelectMask.AsReg32 = g_counterSelectMask;
	portCount = getNumPortsFromSelMask(pStlErrorInfoReq->PortSelectMask);
	attrmod = portCount << 24;

	if (portCount == 0)
		return FALSE;

	BSWAP_STL_ERROR_INFO_REQ(pStlErrorInfoReq);

	if ( FSUCCESS != (status = perform_stl_pma_query (MMTHD_SET, STL_PM_ATTRIB_ID_ERROR_INFO, attrmod, args, mad, sizeof(STL_ERROR_INFO_REQ)))) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_PMA_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		// Do not try to swap the ports
		BSWAP_STL_ERROR_INFO_REQ(pStlErrorInfoReq);
		if (print)
			PrintStlErrorInfoRsp(&g_dest, 0 /* indent */,
			  (STL_ERROR_INFO_RSP*)pStlErrorInfoReq, 1);
		return TRUE;
	}
}


optypes_t stl_pma_query [] = {
//name					func							displayAbridged, description, Flags				m,     m2,    f,     b,     i,     t,     g,     n,     l,     e,    w
{ "classportinfo",		stl_pma_get_class_port_info,	TRUE,	"class of port info", 					FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE},
{ "getportstatus",		stl_pma_get_PortStatus,			TRUE,   "list of port counters",			 	TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE},
{ "clearportstatus",	stl_pma_clear_PortStatus,		FALSE,	"clears the port counters",				FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  TRUE,  TRUE, TRUE},
{ "getdatacounters",	stl_pma_get_DataCounters,		TRUE,	"list of data counters", 				FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  TRUE, FALSE, TRUE},
{ "geterrorcounters",	stl_pma_get_ErrorCounters,		TRUE,	"list of error counters", 				FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  TRUE, FALSE, TRUE},
{ "geterrorinfo",		stl_pma_get_ErrorInfo,			TRUE,	"list of error info", 					FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  TRUE, FALSE, FALSE},
{ "clearerrorinfo",		stl_pma_clear_ErrorInfo,		FALSE,	"clears the error info", 				FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  TRUE, TRUE,  FALSE},


{ NULL }
};

void pma_Usage(boolean displayAbridged)
{
	int i = 0;

	fprintf(stderr, "Usage: opapmaquery -o otype [standard options] [otype options]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opapmaquery --help\n");
	fprintf(stderr, "       --help - produce full help text\n\n");

	fprintf(stderr, "Standard Options: [-v] [-s sl] [-l lid] [-h hfi] [-p port]\n");
	fprintf(stderr, "    -o otype Output type. See below for list.\n");
	fprintf(stderr, "    -v       Verbose output. Can be specified more than once for\n");
	fprintf(stderr, "             additional openib debugging and libibumad debugging.\n");
	fprintf(stderr, "    -s       Specify different Service Level (default is SM SL)\n");
	fprintf(stderr, "    -l lid   Destination lid, default is local port\n");
	fprintf(stderr, "    -h hfi   hfi, numbered 1..n, 0= -p port will be a\n");
	fprintf(stderr, "             system wide port num (default is 0)\n");
	fprintf(stderr, "    -p port  port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "             is 1st active)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "otype options vary by report: [-m port] [-n mask] [-e mask] [-w mask]\n");
	fprintf(stderr, "    -m port  Port in destination device to query.\n");
	fprintf(stderr, "    -n mask  Port Mask, in hex, bits represent ports 63-0\n");
	fprintf(stderr, "             (e.g. 0x2 for port 1, 0x6 for ports 1,2)\n");
	if(!displayAbridged) {
		fprintf(stderr, "    -e mask  Counter/error Select Mask, select bit positions as shown below\n");
		fprintf(stderr, "             0 is least significant (rightmost)\n");
		fprintf(stderr, "             default is all bits set (e.g. 0xffffffe0)\n");
		fprintf(stderr, "                         (for Counters):           (for Error Info):\n");
		fprintf(stderr, "             mask	  bit location  \n");
		fprintf(stderr, "             0x80000000  31    Xmit Data           Rcv Error Info\n");
		fprintf(stderr, "             0x40000000  30    Rcv Data            Excessive Buffer Overrun\n");
		fprintf(stderr, "             0x20000000  29    Xmit Pkts           Xmit Const Error Info\n");
		fprintf(stderr, "             0x10000000  28    Rcv Pkts            Rcv Const Error Info\n");
		fprintf(stderr, "             0x08000000  27    Multicast Xmit Pkts Rcv Switch Relay Error Info\n");
		fprintf(stderr, "             0x04000000  26    Multicast Rcv Pkts  Uncorrectable Error Info\n");
		fprintf(stderr, "             0x02000000  25    Xmit Wait           FM Config Error Info\n");
		fprintf(stderr, "             0x01000000  24    Congestion Discards\n");
		fprintf(stderr, "             0x00800000  23    Rcv FECN\n");
		fprintf(stderr, "             0x00400000  22    Rcv BECN\n");
		fprintf(stderr, "             0x00200000  21    Xmit Time Cong.\n");
		fprintf(stderr, "             0x00100000  20    Xmit Time Wasted BW\n");
		fprintf(stderr, "             0x00080000  19    Xmit Time Wait Data\n");
		fprintf(stderr, "             0x00040000  18    Rcv Bubble\n");
		fprintf(stderr, "             0x00020000  17    Mark FECN\n");
		fprintf(stderr, "             0x00010000  16    Rcv Constraint Errors\n");
		fprintf(stderr, "             0x00008000  15    Rcv Switch Relay\n");
		fprintf(stderr, "             0x00004000  14    Xmit Discards\n");
		fprintf(stderr, "             0x00002000  13    Xmit Constraint Errors\n");
		fprintf(stderr, "             0x00001000  12    Rcv Rmt Phys. Errors\n");
		fprintf(stderr, "             0x00000800  11    Local Link Integrity\n");
		fprintf(stderr, "             0x00000400  10    Rcv Errors\n");
		fprintf(stderr, "             0x00000200   9    Exc. Buffer Overrun\n");
		fprintf(stderr, "             0x00000100   8    FM Config Errors\n");
		fprintf(stderr, "             0x00000080   7    Link Error Recovery\n");
		fprintf(stderr, "             0x00000040   6    Link Error Downed\n");
		fprintf(stderr, "             0x00000020   5    Uncorrectable Errors\n");
	}
	fprintf(stderr, "    -w mask  Virtual Lane Select Mask, in hex, bits represent VL number 31-0\n");
	fprintf(stderr, "             (e.g. 0x1 for VL 0, 0x3 for VL 0,1) default is none\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Possible output types (default is getportstatus):\n");
	fprintf(stderr, "         %-17s: %-45s\n","Output Type","Description");
	fprintf(stderr, "                          : %-45s\n","Supported Options");
	fprintf(stderr, "         %-17s: %-45s\n","----------------","-----------------------------------------");
	i = 0;
	while (stl_pma_query[i].name != NULL) {
		if ( (displayAbridged && stl_pma_query[i].displayWithAbridged) || (!displayAbridged) ) {
			fprintf(stderr, "         %-17s: %-45s\n",stl_pma_query[i].name,stl_pma_query[i].description);

			if ((stl_pma_query[i].mflag)||(stl_pma_query[i].mflag2)||(stl_pma_query[i].bflag)||(stl_pma_query[i].fflag)
				||(stl_pma_query[i].nflag)||(stl_pma_query[i].eflag)||(stl_pma_query[i].wflag)){
				fprintf(stderr, "         %-17s:", " ");
				if (stl_pma_query[i].mflag) fprintf(stderr, " [-m port]");
				if (stl_pma_query[i].mflag2) fprintf(stderr, " [-m port1,port2]");
				if (stl_pma_query[i].bflag) fprintf(stderr, " [-b block[,count]]");
				if (stl_pma_query[i].fflag) fprintf(stderr, " [-f flid]");
				if (stl_pma_query[i].nflag) fprintf(stderr, " [-n port mask]");
				if (stl_pma_query[i].eflag) fprintf(stderr, " [-e counter mask]");
				if (stl_pma_query[i].wflag) fprintf(stderr, " [-w vl mask]");
				fprintf(stderr, "\n");
			}
			fprintf(stderr, "\n");
		}
		i++;
	}
	fprintf(stderr, "\n");
	if (!displayAbridged) {
		fprintf(stderr, "Basic Examples:\n");
		fprintf(stderr, "---------------\n");
		fprintf(stderr, "opapmaquery -o classportinfo\n");
		fprintf(stderr, "opapmaquery -o getportstatus           # get data and error counts, local port\n");
		fprintf(stderr, "opapmaquery -o getdatacounters -n 0x2  # get data counts, local port 1\n");
		fprintf(stderr, "opapmaquery -o geterrorcounters -n 0x2 # get error counts, local port 1\n");
		fprintf(stderr, "opapmaquery -o clearportstatus -n 0x2  # clear all counters local port 1\n");
		fprintf(stderr, "opapmaquery -o geterrorinfo -n 0x2     # get error info for local port 1\n");
		fprintf(stderr, "opapmaquery -o clearerrorinfo -n 0x2   # clear all error info, local port 1\n");
		fprintf(stderr, "Additional examples:\n");
		fprintf(stderr, "--------------------\n");
		fprintf(stderr, "For device at lid 6, get data counters on ports 1-6, inclusive of VL 0 data\n");
		fprintf(stderr, "   opapmaquery -o getdatacounters -l 6 -n 0x7e -w 0x1\n");
		fprintf(stderr, "For device at lid 6, on port 1, clear only error counters:\n");
		fprintf(stderr, "   opapmaquery -o clearportstatus -l 6 -n 0x2 -e 0x1ffff\n");
		fprintf(stderr, "For device at lid 6, on ports 1, clear Uncorrectable Error Info\n");
		fprintf(stderr, "   opapmaquery -o clearerrorinfo -l 6 -n 0x2 -e 0x04000000\n");
	}
	exit(2);
}	//Usage
