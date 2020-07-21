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

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>

/* work around conflicting names */
#include "infiniband/umad.h"
#include "infiniband/verbs.h"

#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include "opamgt_priv.h"
#include "ibprint.h"
#include "stl_print.h"
#include "iba/stl_pm.h"

#define RESP_WAIT_TIME 1000	/* in ms */
#define MAD_ATTEMPTS 1	/* 1 attempt, no retries */

// bit mask
#define OTYPE_INFO 1
#define OTYPE_STATS 2

PrintDest_t g_dest;
int g_printLineByLine = 0;
uint8_t g_detail = 0;
unsigned g_verbose = 0;
uint64_t g_transactID = 0xffffffff1234000;	// Upper half overwritten by umad
uint16_t g_pkey;	// mgmt pkey to use
uint8_t g_cableInfo[STL_CIB_STD_LEN];
uint8_t g_pm_sl = 0xFF;

#if defined(DBGPRINT)
#undef DBGPRINT
#endif
#define VRBSE_PRINT(format, args...) do { if (g_verbose) { fflush(stdout); fprintf(stdout, format, ##args); } } while (0)
#define DBGPRINT(format, args...) do { if (g_verbose>1) { fflush(stdout); fprintf(stderr, format, ##args); } } while (0)

const char *get_port_name(struct omgt_port *portHandle)
{
	static char buf[IBV_SYSFS_NAME_MAX + 6];
	char hfi_name[IBV_SYSFS_NAME_MAX] = {0};
	uint8_t port_num;
	(void)omgt_port_get_hfi_port_num(portHandle, &port_num);
	(void)omgt_port_get_hfi_name(portHandle, hfi_name);
	snprintf(buf, sizeof(buf), "%.*s:%u", IBV_SYSFS_NAME_MAX, hfi_name, port_num);
	return buf;
}

void stl_set_local_route(OUT STL_SMP* smp)
{
	// local directed route
	smp->common.MgmtClass = MCLASS_SM_DIRECTED_ROUTE;
	smp->common.u.DR.s.D = 0;
	smp->common.u.DR.s.Status = 0;
	smp->common.u.DR.HopPointer = 0;
	smp->common.u.DR.HopCount = 0;
	smp->SmpExt.DirectedRoute.DrSLID = STL_LID_PERMISSIVE;
	smp->SmpExt.DirectedRoute.DrDLID = STL_LID_PERMISSIVE;
	return;
}


FSTATUS perform_local_stl_sma_query( IN struct omgt_port *portHandle,
						   IN uint16 attrid,
						   IN uint32 attrmod,
						   OUT STL_SMP* smp )
{
	FSTATUS status;

	smp->common.BaseVersion = STL_BASE_VERSION;
	smp->common.ClassVersion = STL_SM_CLASS_VERSION;
	stl_set_local_route(smp);
	smp->common.mr.AsReg8 = 0;
	smp->common.mr.s.Method = MMTHD_GET;
	smp->common.AttributeID = attrid;
	smp->common.AttributeModifier = attrmod;
	smp->common.TransactionID = (++g_transactID);

	STL_BSWAP_SMP_HEADER(smp);
	{
		struct omgt_mad_addr addr = {
			.lid = 0,
			.qpn = 0,
			.qkey = 0,
			.pkey = g_pkey
		};
        size_t recv_size = sizeof(*smp);
		size_t send_size = STL_MIN_SMP_DR_MAD; // no payload.
		status = omgt_send_recv_mad_no_alloc(portHandle, (uint8_t *)smp, send_size, &addr,
											(uint8_t *)smp, &recv_size, RESP_WAIT_TIME, MAD_ATTEMPTS-1);
		if (FSUCCESS != status)
			fprintf(stderr, "opainfo: MAD failed on hfi:port %s with Status: %s\n",
							get_port_name(portHandle), iba_fstatus_msg(status));
	}
	
	STL_BSWAP_SMP_HEADER(smp);

	if (FSUCCESS == status && smp->common.u.DR.s.Status != MAD_STATUS_SUCCESS) {
		fprintf(stderr, "MAD returned with Bad Status: %s\n",
						iba_mad_status_msg2(smp->common.u.DR.s.Status));
		return FERROR;
	}
	return status;
}

FSTATUS get_local_stl_port_info( IN struct omgt_port *portHandle,
						   OUT STL_SMP* smp )
{
	FSTATUS status;

	if (g_verbose) {
		VRBSE_PRINT("Sending Get(PortInfo) to hfi:port %s\n",
						get_port_name(portHandle));
	}

	MemoryClear(smp, sizeof(*smp));
	
	status = perform_local_stl_sma_query(portHandle,
							STL_MCLASS_ATTRIB_ID_PORT_INFO, 1<<24, smp);
	BSWAP_STL_PORT_INFO((STL_PORT_INFO*)stl_get_smp_data(smp));
	return status;
}

FSTATUS get_local_stl_cable_info ( IN struct omgt_port *portHandle,
							 IN unsigned cabaddr,
							 OUT STL_SMP* smp )
{
	FSTATUS status;

	if (g_verbose) {
		VRBSE_PRINT("Sending Get(CableInfo) to hfi:port %s\n",
						get_port_name(portHandle));
	}
	MemoryClear(smp, sizeof(*smp));

	status = perform_local_stl_sma_query(portHandle,
							STL_MCLASS_ATTRIB_ID_CABLE_INFO,
							(cabaddr & 0x7ff)<<19 | (STL_CABLE_INFO_MAXLEN)<<13,
							smp);
	BSWAP_STL_CABLE_INFO((STL_CABLE_INFO*)stl_get_smp_data(smp));

	return status;
}

FSTATUS perform_local_stl_pma_query( IN struct omgt_port *portHandle,
						   IN uint16 attrid,
						   IN uint32 attrmod,
						   OUT STL_PERF_MAD* mad )
{
	FSTATUS status;
	STL_LID dlid;
	uint8_t port_state;

	mad->common.BaseVersion = STL_BASE_VERSION;
	mad->common.MgmtClass = MCLASS_PERF;
	mad->common.ClassVersion = STL_PM_CLASS_VERSION;
	mad->common.u.NS.Status.AsReg16 = 0;
	mad->common.mr.AsReg8 = 0;
	mad->common.mr.s.Method = MMTHD_GET;
	mad->common.AttributeID = attrid;
	mad->common.AttributeModifier = attrmod;
	mad->common.TransactionID = (++g_transactID);
	// rest of fields should be ignored for a Get, zero'ed above

	(void)omgt_port_get_port_state(portHandle, &port_state);
	if (port_state != IB_PORT_ACTIVE) {
		dlid = STL_LID_PERMISSIVE; // special case for local query
	} else {
		(void)omgt_port_get_port_lid(portHandle, &dlid);
	}
		
	BSWAP_MAD_HEADER((MAD*)mad);
	{
		struct omgt_mad_addr addr = {
			.lid = dlid, 
			.qpn = 1,
			.qkey = QP1_WELL_KNOWN_Q_KEY,
			.pkey = g_pkey,
			.sl = g_pm_sl,
		};
		size_t recv_size = sizeof(*mad);
		size_t send_size = STL_GS_HDRSIZE + sizeof(STL_PORT_STATUS_REQ);
		status = omgt_send_recv_mad_no_alloc(portHandle, (uint8_t *)mad, send_size, &addr,
										(uint8_t *)mad, &recv_size, RESP_WAIT_TIME, MAD_ATTEMPTS-1);
		if (FSUCCESS != status)
			fprintf(stderr, "opainfo: MAD failed on hfi:port %s with Status: %s\n",
							get_port_name(portHandle), iba_fstatus_msg(status));

	}
	BSWAP_MAD_HEADER((MAD*)mad);
	
	if (FSUCCESS == status && mad->common.u.NS.Status.AsReg16 != MAD_STATUS_SUCCESS) {
		fprintf(stderr, "MAD returned with Bad Status: %s\n",
						iba_mad_status_msg2(mad->common.u.NS.Status.AsReg16));
		return FERROR;
	}
	return status;
}

FSTATUS get_local_stl_port_status( IN struct omgt_port *portHandle,
						   OUT STL_PERF_MAD* mad )
{
	FSTATUS status;
	STL_PORT_STATUS_REQ *pPortCounterReq = (STL_PORT_STATUS_REQ *)&(mad->PerfData);

	MemoryClear(mad, sizeof(*mad));

	if (g_verbose) {
		VRBSE_PRINT("Sending Get(PortStatus) to hfi:port %s\n",
						get_port_name(portHandle));
	}
	(void)omgt_port_get_hfi_port_num(portHandle, &pPortCounterReq->PortNumber);
	//pPortCounterReq->VLSelectMask = 0x8001; // Mask for VL15 & VL0 by default (same as pmaquery).

	BSWAP_STL_PORT_STATUS_REQ(pPortCounterReq);

	status = perform_local_stl_pma_query(portHandle,
						STL_PM_ATTRIB_ID_PORT_STATUS, 1<<24, mad);
	BSWAP_STL_PORT_STATUS_RSP((STL_PORT_STATUS_RSP*)&(mad->PerfData));
	return status;
}


void show_info( struct omgt_port *portHandle,
					 IN int outputType,
					 IN STL_PORT_INFO* pPortInfo,
					 IN uint8_t* cableInfo,
					 IN STL_PORT_STATUS_RSP *pPortStatusRsp)
{
	uint64_t portGUID = 0;
	(void)omgt_port_get_port_guid(portHandle, &portGUID);

	if (! outputType) {
		uint8_t detail = g_detail;

		if (detail > CABLEINFO_DETAIL_ALL)
			detail = CABLEINFO_DETAIL_ALL;

		PrintStlPortSummary(&g_dest, 0, get_port_name(portHandle),
				pPortInfo, portGUID, g_pkey,
				cableInfo, STL_CIB_STD_HIGH_PAGE_ADDR, STL_CIB_STD_LEN, 
 				pPortStatusRsp, detail, g_printLineByLine);
	}

	if (outputType & (OTYPE_INFO|OTYPE_STATS))
		PrintFunc(&g_dest, "%s\n", get_port_name(portHandle));
	if ((outputType & OTYPE_INFO) && pPortInfo)
		PrintStlPortInfo(&g_dest, 3, pPortInfo, portGUID, g_printLineByLine);
	if ((outputType & OTYPE_STATS) && pPortStatusRsp)
		PrintStlPortStatusRsp(&g_dest, 3, pPortStatusRsp);
	fflush(stdout);
}

void Usage(void)
{

	fprintf(stderr, "Usage: opainfo [-h hfi] [-p port] [-o type] [-g] [-d detail] [-s pm_sl] [-v [-v]...]\n");
	fprintf(stderr, "    -h hfi     hfi, numbered 1..n, 0=system wide port num\n");
	fprintf(stderr, "               (default is 0)\n");
	fprintf(stderr, "    -p port    port, numbered 1..n, 0=1st active\n");
	fprintf(stderr, "               (default is all ports on selected hfi)\n");
	fprintf(stderr, "    -o type    output type specified (can appear more than once)\n");
	fprintf(stderr, "               info - output detailed portinfo\n");
	fprintf(stderr, "               stats - output detailed port counters\n");
	fprintf(stderr, "               (behavior without -o gives a brief summary of portinfo,\n");
	fprintf(stderr, "               counters and cableinfo)\n");
	fprintf(stderr, "    -g         Display in line-by-line format (default is summary format)\n");
	fprintf(stderr, "    -d detail  output detail level 0-2 for CableInfo only (default 0):\n");
	fprintf(stderr, "               (-d option ignored when used with -o type)\n");
	fprintf(stderr, "                   0 - minimal crucial info (e.g. cable length, vendor)\n");
	fprintf(stderr, "                   1 - brief summary\n");
	fprintf(stderr, "                   2 - extended brief summary\n");
	fprintf(stderr, "    -s pm_sl   Specify different Service Level for PMA traffic\n");
	fprintf(stderr, "    -v         verbose output. Additional invocations will turn on debugging,\n");
	fprintf(stderr, "               openib debugging and libibumad debugging.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       all ports on all HFIs (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  1st active port in system\n");
	fprintf(stderr, "    -h x       all ports on HFI x\n");
	fprintf(stderr, "    -h x -p 0  1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  HFI x, port y\n");
	exit(0);
}


/**
 * find the output type requested
 * 
 * @param name
 * 
 * @return int
 */
int checkOutputType(const char *name)
{
	if ( 0 == strcmp(name, "info") )
		return OTYPE_INFO;
	else if ( 0 == strcmp(name, "stats") )
		return OTYPE_STATS;
	else
		return -1;
}  // int checkOutputType(const char *name)

// command line options
struct option options[] = {
		{ "help", no_argument, NULL, '$' },	// use an invalid option character
		{ 0 }
};

int main(int argc, char *argv[])
{
	uint8 hfi = 0;	// all HFIs
	uint8 port = 0;
	int allPorts = 1;
	int outputType = 0;
	uint8 pm_sl = 0xFF;

	int c;
	int index;
	int ret;
	FSTATUS             fstatus;
	uint32_t portCount;
	STL_SMP smpPortInfo;
	STL_SMP smpCableInfo;
	STL_PERF_MAD madPortStatusRsp;
	STL_PORT_INFO* pPortInfo;
	int have_cableinfo;
	STL_PORT_STATUS_RSP *pPortStatusRsp = 0;
    struct omgt_port     *portHandle = NULL;

	while (-1 != (c = getopt_long(argc,argv, "h:p:o:d:gvs:", options, &index)))
	{
		switch (c) {
		case '$':
			Usage();
			break;
		case 'h':
			if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opainfo: Invalid HFI Number: %s\n", optarg);
				Usage();
			}
			break;
		case 'p':
			if (FSUCCESS != StringToUint8(&port, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opainfo: Invalid Port Number: %s\n", optarg);
				Usage();
			}
			allPorts = 0;
			break;
		case 'o':
			outputType |= checkOutputType(optarg);
			if (outputType < 0)
				Usage();
			break;
		case 'g':
			g_printLineByLine = 1;
			break;
		case 'd':
			if (FSUCCESS != StringToUint8(&g_detail, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opainfo: Invalid Detail: %s\n", optarg);
				Usage();
			}
			break;
		case 'v':
			g_verbose++;
			if (g_verbose > 3) umad_debug(g_verbose - 3);
			break;
		case 's':
			if (FSUCCESS != StringToUint8(&pm_sl, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "opainfo: Invalid PM SL: %s\n", optarg);
				Usage();
			}
			break;
		default:
			fprintf(stderr, "opainfo: Invalid option -%c\n", c);
			Usage();
			break;
		}   // switch
	}  // while

	if (optind < argc)
	{
		Usage();
	}

	PrintDestInitFile(&g_dest, stdout);

	if (allPorts) {
		// determine port count for selected hfi
		// if hfi==0, its system wide port count
		// if port!=0, we get a specific port (fstatus parsing easier below)
		fstatus = omgt_get_portguid( hfi, 1, NULL, NULL, NULL, NULL,
									NULL, NULL, NULL, &portCount,
									NULL, NULL, NULL );

		if (hfi && FSUCCESS != fstatus) {
			fprintf(stderr, "opainfo: Failed to find hfi %d\n", hfi);
			exit(1);
		} else if (FSUCCESS != fstatus && FNOT_FOUND != fstatus) {
			fprintf(stderr, "opainfo: Failed to determine number of HFIs\n");
			exit(1);
		} else if (portCount == 0) {
			fprintf(stderr, "opainfo: No HFIs found\n");
			exit(1);
		}
		port = 1;	// start at 1st port
	} else {
		portCount = 1;
	}

	for (; portCount > 0; port++, portCount--)
	{
		have_cableinfo = FALSE;
		struct omgt_params params = {.debug_file = g_verbose > 2 ? stderr : NULL};
		ret = omgt_open_port_by_num(&portHandle, hfi, port, &params);
		if (port == 0 && ret == OMGT_STATUS_NOT_DONE) {
			// asked for 1st active, but none active, use 1st port
			port = 1;
			ret = omgt_open_port_by_num(&portHandle, hfi, port, &params);
		}
		if (ret != 0) {
			printf("opainfo: Unable to open hfi:port %u:%u\n", hfi, port);
			continue;
		}

    	// Determine which pkey to use (full or limited)
    	// Attempt to use full at all times, otherwise, can
    	// use the limited for queries of the local port.
    	g_pkey = omgt_get_mgmt_pkey(portHandle, 0, 0);
    	if (g_pkey==0) {
        	fprintf(stderr, "opainfo: Unable to find mgmt pkey on hfi:port %s\n",
					get_port_name(portHandle));
			goto next;
    	}

		fstatus = get_local_stl_port_info( portHandle, &smpPortInfo);
		if (FSUCCESS != fstatus)
		{
			fprintf(stderr, "opainfo: Failed to get Portinfo for hfi:port %s\n", 
					get_port_name(portHandle));
			goto next;	// skip to next port, if any
		}
		pPortInfo = (STL_PORT_INFO *)stl_get_smp_data(&smpPortInfo);

		if (IsCableInfoAvailable(pPortInfo)) {
			uint16_t addr;
			uint8_t *data;
			have_cableinfo = TRUE;	// assume success
			for (addr = STL_CIB_STD_HIGH_PAGE_ADDR, data=g_cableInfo;
				 addr + STL_CABLE_INFO_MAXLEN <= STL_CIB_STD_END_ADDR; addr += STL_CABLE_INFO_DATA_SIZE, data += STL_CABLE_INFO_DATA_SIZE)
			{
				fstatus = get_local_stl_cable_info(portHandle, addr, &smpCableInfo);
				if (FSUCCESS != fstatus) {
					fprintf(stderr, "opainfo: Failed to get Cableinfo for hfi:port %s\n", 
							get_port_name(portHandle));
					have_cableinfo = FALSE;
				} else  {
					memcpy(data, ((STL_CABLE_INFO *)stl_get_smp_data(&smpCableInfo))->Data, STL_CABLE_INFO_DATA_SIZE);
				}
			}
		} else {
			have_cableinfo = FALSE;
		}

		// If PM SL not set, use SM's SL
		if (pm_sl == 0xFF) {
			(void)omgt_port_get_port_sm_sl(portHandle, &pm_sl);
		}
		g_pm_sl = pm_sl;
		fstatus = get_local_stl_port_status(portHandle, &madPortStatusRsp); 
		if (FSUCCESS != fstatus) {
			fprintf(stderr, "opainfo: Failed to get PMA Port Status for hfi:port %s\n", 
					get_port_name(portHandle));
			pPortStatusRsp = NULL;
		} else  {
			pPortStatusRsp = (STL_PORT_STATUS_RSP *)&(madPortStatusRsp.PerfData);
		}

		show_info(portHandle, outputType, pPortInfo, have_cableinfo?g_cableInfo:NULL, pPortStatusRsp);
next:
		omgt_close_port(portHandle);
		portHandle = NULL;
	}

	return 0 ;

}  // main
