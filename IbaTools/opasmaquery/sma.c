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
#include "stl_mad_priv.h"
#include "stl_helper.h"
#include <opamgt_priv.h>
#include "opasmaquery.h"

#define MSG_SMP_SEND_RECV_FAILURE	"failed SMP Send or Receive"

static void set_route(argrec *args, SMP* smp)
{
	int i;

	DBGPRINT("-> set_route: dlid=0x%08x slid=0x%08x drpaths=%d\n", args->dlid,args->slid,args->drpaths);

	if (args->dlid && !args->drpaths)	//lid routed only
	{
		DBGPRINT("set_route: LID ROUTED\n");
		smp->common.MgmtClass = MCLASS_SM_LID_ROUTED;
		smp->common.u.NS.Status.AsReg16 = 0;
		return;
	}

	//Directed Route

	smp->common.MgmtClass = MCLASS_SM_DIRECTED_ROUTE;
	smp->common.u.DR.s.D = 0;
	smp->common.u.DR.s.Status = 0;
	smp->common.u.DR.HopPointer = 0;
	smp->common.u.DR.HopCount = args->drpaths;

	if (!args->dlid && !args->drpaths)	//local
	{
		DBGPRINT("set_route: LOCAL\n");

		smp->SmpExt.DirectedRoute.DrSLID = LID_PERMISSIVE;
		smp->SmpExt.DirectedRoute.DrDLID = LID_PERMISSIVE;
		return;
	}

	for (i=1;i<=args->drpaths;i++)
	{
		smp->SmpExt.DirectedRoute.InitPath[i]= g_drpath[i];
	}

	if (!args->dlid && args->drpaths) //pure DR
	{
		DBGPRINT("set_route: PURE DR\n");
		smp->SmpExt.DirectedRoute.DrSLID = LID_PERMISSIVE;
		smp->SmpExt.DirectedRoute.DrDLID = LID_PERMISSIVE;
		return;
	}

	if (args->dlid && args->drpaths) // LID then DR
	{
		DBGPRINT("set_route: LID then DR\n");
 		smp->SmpExt.DirectedRoute.DrSLID = args->slid;
		smp->SmpExt.DirectedRoute.DrDLID = LID_PERMISSIVE;
		return;
	}

}	//set_route

static void stl_set_route(argrec *args, STL_SMP* smp)
{
	int i;

	DBGPRINT("-> stl_set_route: dlid=0x%08x slid=0x%08x drpaths=%d\n", args->dlid,args->slid,args->drpaths);

	if (args->dlid && !args->drpaths)	//lid routed only
	{
		DBGPRINT("stl_set_route: LID ROUTED\n");
		smp->common.MgmtClass = MCLASS_SM_LID_ROUTED;
		smp->common.u.NS.Status.AsReg16 = 0;
		return;
	}

	//Directed Route

	smp->common.MgmtClass = MCLASS_SM_DIRECTED_ROUTE;
	smp->common.u.DR.s.D = 0;
	smp->common.u.DR.s.Status = 0;
	smp->common.u.DR.HopPointer = 0;
	smp->common.u.DR.HopCount = args->drpaths;

	if (!args->dlid && !args->drpaths)	//local
	{
		DBGPRINT("stl_set_route: LOCAL\n");

		smp->SmpExt.DirectedRoute.DrSLID = STL_LID_PERMISSIVE;
		smp->SmpExt.DirectedRoute.DrDLID = STL_LID_PERMISSIVE;
		return;
	}

	for (i=1;i<=args->drpaths;i++)
	{
		smp->SmpExt.DirectedRoute.InitPath[i]= g_drpath[i];
	}

	if (!args->dlid && args->drpaths) //pure DR
	{
		DBGPRINT("stl_set_route: PURE DR\n");
		smp->SmpExt.DirectedRoute.DrSLID = STL_LID_PERMISSIVE;
		smp->SmpExt.DirectedRoute.DrDLID = STL_LID_PERMISSIVE;
		return;
	}

	if (args->dlid && args->drpaths) // LID then DR
	{
		DBGPRINT("stl_set_route: LID then DR\n");
 		smp->SmpExt.DirectedRoute.DrSLID = args->slid;
		smp->SmpExt.DirectedRoute.DrDLID = STL_LID_PERMISSIVE;
		return;
	}

}	//stl_set_route


static FSTATUS perform_sma_query(uint16 attrid, uint32 attrmod, argrec *args, SMP *smp)
{
	FSTATUS status;
	MemoryClear(smp, sizeof(*smp));
	smp->common.BaseVersion = IB_BASE_VERSION;
	smp->common.ClassVersion = IB_SM_CLASS_VERSION;
	set_route(args, smp);
	smp->common.mr.AsReg8 = 0;
	smp->common.mr.s.Method = MMTHD_GET;
	smp->common.AttributeID = attrid;
	smp->common.AttributeModifier = attrmod;
	smp->common.TransactionID = (++g_transactID);

    // Determine which pkey to use (full or limited)
    // Attempt to use full at all times, otherwise, can
    // use the limited for queries of the local port.
    uint16_t pkey = omgt_get_mgmt_pkey(args->omgt_port, args->dlid, args->drpaths);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	if (g_verbose) {
		PrintFunc(&g_dest, "Sending SMP:\n");
		PrintSmpHeader(&g_dest, 2, smp);
		PrintSeparator(&g_dest);
	}

	BSWAP_SMP_HEADER(smp);
	{
		struct omgt_mad_addr addr = {
			lid : args->dlid,
			qpn : 0,
			qkey : 0,
			pkey : pkey
		};
        size_t recv_size = sizeof(*smp);
		status = omgt_send_recv_mad_no_alloc(args->omgt_port, (uint8_t *)smp, sizeof(*smp), &addr,
											(uint8_t *)smp, &recv_size, RESP_WAIT_TIME, 0);
	}
	BSWAP_SMP_HEADER(smp);
	if (status == FSUCCESS && g_verbose) {
		PrintFunc(&g_dest, "Received SMP:\n");
		PrintSmpHeader(&g_dest, 2, smp);
		PrintSeparator(&g_dest);
	}

	if (FSUCCESS == status && smp->common.u.DR.s.Status != MAD_STATUS_SUCCESS) {
		fprintf(stderr, "MAD returned with Bad Status: %s\n",
						iba_mad_status_msg2(smp->common.u.DR.s.Status));
		return FERROR;
	}
	return status;
} //perform_sma_query

static FSTATUS perform_stl_aggregate_query(argrec *args, STL_SMP *smp)
{
	FSTATUS status;

    // Determine which pkey to use (full or limited)
    // Attempt to use full at all times, otherwise, can
    // use the limited for queries of the local port.
    uint16_t pkey = omgt_get_mgmt_pkey(args->omgt_port, args->dlid, args->drpaths);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

	struct omgt_mad_addr addr = {
		lid : args->dlid,
		qpn : 0,
		qkey : 0,
        pkey : pkey
	};
    size_t recv_size;

	if (g_verbose) {
		int i;
		STL_AGGREGATE *aggr;

		aggr = STL_AGGREGATE_FIRST(smp);

		PrintFunc(&g_dest, "Sending STL AGGREGATE:\n");
		PrintStlSmpHeader(&g_dest, 2, smp);
		for (i=0; i<smp->common.AttributeModifier; i++) {
			uint16 r = ntoh16(aggr->Result.AsReg16);
			uint16 l = (r&0x7f) * 8;
		    PrintFunc(&g_dest, "%*sAttrID: 0x%04x\n", 4, "",
				ntoh16(aggr->AttributeID));
    		PrintFunc(&g_dest, "%*sResult: 0x%04x (RLen=%d)\n", 6, "",
				ntoh16(aggr->Result.AsReg16), l);
    		PrintFunc(&g_dest, "%*sModifier: 0x%08x\n", 6, "",
				ntoh32(aggr->AttributeModifier));
			PrintSeparator(&g_dest);
			// Can't use STL_AGGREGATE_NEXT because we're in network byte order.
			aggr = (STL_AGGREGATE*)((void*)aggr+l+sizeof(STL_AGGREGATE));
		}
		PrintSeparator(&g_dest);
	}


	STL_BSWAP_SMP_HEADER(smp);
	recv_size = sizeof(*smp);
	status = omgt_send_recv_mad_no_alloc(args->omgt_port, (uint8_t *)smp,
		sizeof(*smp), &addr, (uint8_t *)smp, &recv_size, RESP_WAIT_TIME, 0);

	STL_BSWAP_SMP_HEADER(smp);
	if (status == FSUCCESS && g_verbose) {
		PrintFunc(&g_dest, "Received STL SMP:\n");
		PrintStlSmpHeader(&g_dest, 2, smp);
		PrintSeparator(&g_dest);
	}


	if (FSUCCESS == status && smp->common.u.DR.s.Status != MAD_STATUS_SUCCESS) {
		fprintf(stderr, "Aggregate MAD returned with Bad Status: %s\n",
						iba_mad_status_msg2(smp->common.u.DR.s.Status));
		return FERROR;
	}
	return status;
} //perform_stl_aggregate_query


static FSTATUS perform_stl_sma_query(uint16 attrid, uint32 attrmod, argrec *args,
                            STL_SMP *smp, size_t attr_len, STL_PORTMASK *portMask)
{
	FSTATUS status;
	int i;
	MemoryClear(smp, sizeof(*smp));
	uint8 requestInitPath[64];
	MemoryClear(requestInitPath, sizeof(requestInitPath));

	smp->common.BaseVersion = STL_BASE_VERSION;
	smp->common.ClassVersion = STL_SM_CLASS_VERSION;
	stl_set_route(args, smp);
	smp->common.mr.AsReg8 = 0;
	smp->common.mr.s.Method = MMTHD_GET;
	smp->common.AttributeID = attrid;
	smp->common.AttributeModifier = attrmod;
	smp->common.TransactionID = (++g_transactID);

	if (portMask) {
		STL_PORTMASK *pMask = (STL_PORTMASK *)stl_get_smp_data((STL_SMP *)smp);
		memcpy(pMask, portMask, STL_PORT_SELECTMASK_SIZE);
	}

	if (smp->common.MgmtClass == MCLASS_SM_DIRECTED_ROUTE) {
		memcpy(requestInitPath, smp->SmpExt.DirectedRoute.InitPath, sizeof(requestInitPath));
	}

	if (g_verbose) {
		PrintFunc(&g_dest, "Sending STL SMP:\n");
		PrintStlSmpHeader(&g_dest, 2, smp);
		PrintSeparator(&g_dest);
	}

	uint8_t port_state;
	(void)omgt_port_get_port_state(args->omgt_port, &port_state);
	if (port_state == IB_PORT_DOWN && ((args->dlid && args->dlid != args->slid) || args->drpaths)) {
		uint8_t port_num;
		char hfi_name[IBV_SYSFS_NAME_MAX] = {0};
		(void)omgt_port_get_hfi_port_num(args->omgt_port, &port_num);
		(void)omgt_port_get_hfi_name(args->omgt_port, hfi_name);
		fprintf(stderr, "WARNING port (%s:%d) is not ACTIVE!\n",
			hfi_name, port_num);
	}

    // Determine which pkey to use (full or limited)
    // Attempt to use full at all times, otherwise, can
    // use the limited for queries of the local port.
    uint16_t pkey = omgt_get_mgmt_pkey(args->omgt_port, args->dlid, args->drpaths);
    if (pkey==0) {
        fprintf(stderr, "ERROR: Local port does not have management privileges\n");
        return (FPROTECTION);
    }

    attr_len += stl_get_smp_header_size(smp);
    attr_len = ROUNDUP_TYPE(size_t, attr_len, 8);
	STL_BSWAP_SMP_HEADER(smp);
	{
		struct omgt_mad_addr addr = {
			lid : args->dlid,
			qpn : 0,
			qkey : 0,
            pkey : pkey
		};
        size_t recv_size = sizeof(*smp);
		status = omgt_send_recv_mad_no_alloc(args->omgt_port, (uint8_t *)smp, attr_len, &addr,
											(uint8_t *)smp, &recv_size, RESP_WAIT_TIME, 0);
	}
	STL_BSWAP_SMP_HEADER(smp);
	if (status == FSUCCESS && g_verbose) {
		PrintFunc(&g_dest, "Received STL SMP:\n");
		PrintStlSmpHeader(&g_dest, 2, smp);
		PrintSeparator(&g_dest);
	}

	if (smp->common.MgmtClass == MCLASS_SM_DIRECTED_ROUTE && memcmp(requestInitPath, smp->SmpExt.DirectedRoute.InitPath, sizeof(requestInitPath)) != 0) {
		fprintf(stderr, "Response failed directed route validation, received packet with path: ");
		for(i = 1; i < 64; i++) {
			if(smp->SmpExt.DirectedRoute.InitPath[i] != 0) {
				fprintf(stderr, "%d ", smp->SmpExt.DirectedRoute.InitPath[i]);
			} else {
				break;
			}
		}
		fprintf(stderr, "\n");

		status = FERROR;
	}

	if (FSUCCESS == status && smp->common.u.DR.s.Status != MAD_STATUS_SUCCESS) {
		fprintf(stderr, "MAD returned with Bad Status: %s\n",
						iba_mad_status_msg2(smp->common.u.DR.s.Status));
		return FERROR;
	}
	return status;
} //perform_stl_sma_query

/*
 * Initializes an SMP as an aggregate SMP. Returns offset of the first member.
 * argrecs	standard opasmaquery arguments (for building the route)
 * smp 		pointer to an SMP structure (or a 2k buffer...)
 *
 * NOTE: SMP is in HOST byte order.
 */
static uint32 init_stl_aggregate_query(argrec *args, STL_SMP *smp)
{
	uint32 len = 0;

	MemoryClear(smp, sizeof(STL_SMP));

	smp->common.BaseVersion = STL_BASE_VERSION;
	smp->common.ClassVersion = STL_SM_CLASS_VERSION;
	stl_set_route(args, smp);
	smp->common.mr.AsReg8 = 0;
	smp->common.mr.s.Method = MMTHD_GET;
	smp->common.AttributeID = STL_MCLASS_ATTRIB_ID_AGGREGATE;
	smp->common.AttributeModifier = 0;

	if (g_verbose) {
		PrintFunc(&g_dest, "Preparing STL Aggregate SMP:\n");
		PrintStlSmpHeader(&g_dest, 2, smp);
		PrintSeparator(&g_dest);
	}

	if (smp->common.MgmtClass == MCLASS_SM_LID_ROUTED) {
		len = ((void*)&(smp->SmpExt.LIDRouted.SMPData)-(void*)smp);
	} else {
		len = ((void*)&(smp->SmpExt.DirectedRoute.SMPData)-(void*)smp);
	}

	return len;
}

/*
 * Adds attribute attrid to the aggregate query, along with required padding.
 *
 * Returns new length of aggregate or zero if length would exceed the max
 * size of the MAD.
 *
 * NOTE: AGGREGATE IS ADDED IN NETWORK BYTE ORDER.
 *
 */
static uint32 add_stl_aggregate_member(STL_SMP *smp, uint16 attrid,
	uint32 member_modifier, uint32 member_len)
{
	STL_AGGREGATE 	*member;
	uint32			modifier;
	uint32			offset;
	uint32			i;

	DBGPRINT("-> add_stl_aggregate_member: smp = %p, attrid = 0x%04x, "
			"modifier = %d, len = %d\n", smp, attrid, member_modifier,
			member_len);

	// Offset of first member
	if (smp->common.MgmtClass == MCLASS_SM_LID_ROUTED) {
		member = (STL_AGGREGATE*)&(smp->SmpExt.LIDRouted.SMPData);
	} else {
		member = (STL_AGGREGATE*)&(smp->SmpExt.DirectedRoute.SMPData);
	}

	// Find the end. Slow, but not critical for our purposes.
	modifier = smp->common.AttributeModifier;
	for (i=0; i<modifier; i++) {
		uint16 r = ntoh16(member->Result.AsReg16)&0x7f;
		offset = r*8;
		member = (STL_AGGREGATE*)(((void*)member)+sizeof(STL_AGGREGATE)+offset);
	}

	member_len = ROUNDUP(member_len,8);

	// Make sure we haven't run off the end.
	ASSERT(((void*)member - (void*)smp) < (2048-member_len));

	member->AttributeID = ntoh16(attrid);
	member->Result.AsReg16 = 0;

	DBGPRINT("add_stl_aggregate_member: member_len = %d\n", member_len);

	member->AttributeID = ntoh16(attrid);
	member->AttributeModifier = ntoh32(member_modifier);
	member->Result.AsReg16 = ntoh16(member_len/8);

	// Updates the member count.
	modifier++;
	if (modifier > MAX_AGGREGATE_ATTRIBUTES) return 0;
	smp->common.AttributeModifier = modifier;

	return member_len;
}

static boolean get_node_aggr(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	uint32 len;
	STL_SMP *smp = (STL_SMP *)mad;
	STL_AGGREGATE *aggr;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("-->get_node_aggr\n");

	len = init_stl_aggregate_query(args, smp);
	if (len == 0 ) {
		fprintf(stderr, "get_node_aggr: failed to initialize MAD.\n");
		return FALSE;
	}

	len = add_stl_aggregate_member(smp, STL_MCLASS_ATTRIB_ID_NODE_INFO,
		0, sizeof(STL_NODE_INFO));
	if (len == 0 ) {
		fprintf(stderr, "get_node_aggr: failed to add Node Info member.\n");
		return FALSE;
	}

	len = add_stl_aggregate_member(smp, STL_MCLASS_ATTRIB_ID_NODE_DESCRIPTION,
		0, sizeof(STL_NODE_DESCRIPTION));
	if (len == 0 ) {
		fprintf(stderr, "get_node_aggr: failed to add Node Desc member.\n");
		return FALSE;
	}

	status = perform_stl_aggregate_query (args, smp);
	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	}

	if (smp->common.u.DR.s.Status != MAD_STATUS_SUCCESS) {
		fprintf(stderr, "MAD returned with Bad Status: %s\n",
						iba_mad_status_msg2(smp->common.u.DR.s.Status));
	}

	aggr = STL_AGGREGATE_FIRST(smp);

	BSWAP_STL_AGGREGATE_HEADER(aggr);

	if (aggr->Result.s.Error != 0) {
		fprintf(stderr,"get_node_aggr: Node Info query failed.\n");
	} else if (print || g_verbose) {
		STL_NODE_INFO *pNodeInfo = (STL_NODE_INFO *)(aggr->Data);
		BSWAP_STL_NODE_INFO(pNodeInfo);
		PrintStlNodeInfo(&g_dest, 0 /* indent */, pNodeInfo, args->printLineByLine);
	}

	PrintSeparator(&g_dest);

	aggr = STL_AGGREGATE_NEXT(aggr);

	BSWAP_STL_AGGREGATE_HEADER(aggr);

	if (aggr->Result.s.Error != 0) {
		fprintf(stderr,"get_node_aggr: Node Description query failed.\n");
	} else if (print || g_verbose) {
		STL_NODE_DESCRIPTION *pNodeDesc = (STL_NODE_DESCRIPTION *)(aggr->Data);
		BSWAP_STL_NODE_DESCRIPTION(pNodeDesc);
		PrintStlNodeDesc(&g_dest, 0, pNodeDesc, args->printLineByLine);
	}
	return TRUE;
}	//get_node_aggr

static boolean get_switch_aggr(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	uint32 len;
	STL_SMP *smp = (STL_SMP *)mad;
	STL_AGGREGATE *aggr = NULL;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("-->get_node_aggr\n");

	len = init_stl_aggregate_query(args, smp);
	if (len == 0 ) {
		fprintf(stderr, "get_node_aggr: failed to initialize MAD.\n");
		return FALSE;
	}

	len = add_stl_aggregate_member(smp, STL_MCLASS_ATTRIB_ID_NODE_INFO, 0, sizeof(STL_NODE_INFO));
	if (len == 0 ) {
		fprintf(stderr, "get_node_aggr: failed to add Node Info member.\n");
		return FALSE;
	}

	len = add_stl_aggregate_member(smp, STL_MCLASS_ATTRIB_ID_SWITCH_INFO, 0, sizeof(STL_SWITCH_INFO));
	if (len == 0 ) {
		fprintf(stderr, "get_node_aggr: failed to add Switch Info member.\n");
		return FALSE;
	}

	status = perform_stl_aggregate_query (args, smp);
	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		if (g_verbose) {
			PrintFunc(&g_dest, "Received STL Aggregate SMP:\n");
			PrintStlSmpHeader(&g_dest, 2, smp);
			PrintSeparator(&g_dest);
		}
	}

	if (smp->common.u.DR.s.Status != MAD_STATUS_SUCCESS) {
		fprintf(stderr, "MAD returned with Bad Status: %s\n",
						iba_mad_status_msg2(smp->common.u.DR.s.Status));
	}

	BSWAP_STL_SMP_HEADER(smp);

	aggr = STL_AGGREGATE_FIRST(smp);

	BSWAP_STL_AGGREGATE_HEADER(aggr);

	if (aggr->Result.s.Error != 0) {
		fprintf(stderr,"get_switch_aggr: Node Info query failed.\n");
	} else if (print || g_verbose) {
		STL_NODE_INFO *pNodeInfo = (STL_NODE_INFO *)(aggr->Data);
		BSWAP_STL_NODE_INFO(pNodeInfo);
		PrintStlNodeInfo(&g_dest, 0 /* indent */, pNodeInfo, args->printLineByLine );
	}

	PrintSeparator(&g_dest);

	aggr = STL_AGGREGATE_NEXT(aggr);

	BSWAP_STL_AGGREGATE_HEADER(aggr);

	if (aggr->Result.s.Error != 0) {
		fprintf(stderr,"get_switch_aggr: Switch Info query failed.\n");
	} else if (print || g_verbose) {
		STL_SWITCH_INFO *pSwitchInfo = (STL_SWITCH_INFO *)(aggr->Data);
		BSWAP_STL_SWITCH_INFO(pSwitchInfo);
		PrintStlSwitchInfo(&g_dest, 0/*indent*/, pSwitchInfo, 0/*lid*/, args->printLineByLine );
	}
	return TRUE;
}	//get_switch_aggr

static boolean get_node_desc(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;


	DBGPRINT("-->get_node_desc");
	status = perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_NODE_DESCRIPTION, 0, args,
                            smp, sizeof(STL_NODE_DESCRIPTION), NULL);


	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		if (print || g_verbose) {
			STL_NODE_DESCRIPTION *pNodeDesc = (STL_NODE_DESCRIPTION *)stl_get_smp_data(smp);
			BSWAP_STL_NODE_DESCRIPTION(pNodeDesc);
			PrintStlNodeDesc(&g_dest, 0, pNodeDesc, args->printLineByLine);
		}
		return TRUE;
	}

} //get_node_desc

static boolean get_ib_node_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	NODE_INFO *pNodeInfo;
	SMP *smp = (SMP *)mad;

	if (mad_len < sizeof(SMP))
		return FALSE;

	pNodeInfo = (NODE_INFO *)&(smp->SmpExt.DirectedRoute.SMPData);

	DBGPRINT("-->get_ib_node_info\n");
	status = perform_sma_query (MCLASS_ATTRIB_ID_NODE_INFO, 0, args, smp);
	BSWAP_NODE_INFO(pNodeInfo);

	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		if (print || g_verbose)
			PrintNodeInfo(&g_dest, 0 /* indent */, pNodeInfo);
		return TRUE;
	}
}	//get_ib_node_info

static boolean get_node_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("-->get_node_info\n");
	status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_NODE_INFO, 0,
                    args, smp, sizeof(STL_NODE_INFO), NULL);

	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	}

	STL_NODE_INFO *pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
	BSWAP_STL_NODE_INFO(pNodeInfo);

	if (print || g_verbose) {
		PrintStlNodeInfo(&g_dest, 0 /* indent */, pNodeInfo, args->printLineByLine );
	}
	return TRUE;
}	//get_node_info

static boolean get_port_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	STL_SMP *smp = (STL_SMP *)mad;
    STL_PORT_INFO *pPortInfo;
	STL_NODE_INFO *pNodeInfo;
	uint32_t amod = 0x01000000;
	int i;
	uint16_t firstPort = 0;

	if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
		fprintf(stderr, "%s failed: unable to get_node_info\n", __func__);
		return FALSE;
	}
	pNodeInfo = (STL_NODE_INFO*)stl_get_smp_data(smp);

	if (args->mflag) {
		if (pNodeInfo->NodeType == STL_NODE_SW) {
			if ((args->dport + args->mcount - 1) > pNodeInfo->NumPorts) {
				fprintf(stderr, "%s failed: specified port [%u-%u] invalid must be %u - %u for this SW\n",
					__func__, args->dport, (args->dport + args->mcount - 1), 1, pNodeInfo->NumPorts);
				return FALSE;
			}
			amod = (uint32_t)(args->mcount << 24);
			amod |= (uint32_t)args->dport;
			firstPort = args->dport;
		} else if (strcmp(args->oname, "portinfo") == 0)
			fprintf(stderr, "%s: -m ignored for port_info against an HFI\n", g_cmdname);
	}
	if (pNodeInfo->NodeType == STL_NODE_FI)
		firstPort = pNodeInfo->u1.s.LocalPortNum;


	DBGPRINT("---->get_port_info\n");
	DBGPRINT("->dlid=0x%08x slid=0x%08x dport=%d,%d drpaths=%d\n",
				args->dlid,args->slid,args->dport,args->mcount,args->drpaths);

	// Get PortInfo using attrmod
	status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_PORT_INFO, amod, args,
                                    smp, sizeof(*pPortInfo), NULL);

	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	}

    pPortInfo = (STL_PORT_INFO *)stl_get_smp_data(smp);
	for (i = 0; i < ((amod>>24)&0xFF); i++) {
		BSWAP_STL_PORT_INFO(pPortInfo);
		if (print) {
			PrintFunc(&g_dest, "%*sPort %u Info\n", 0, "", firstPort + i);
			PrintStlPortInfo(&g_dest, 3, pPortInfo, 0 /*portGuid*/, args->printLineByLine);
		}
		pPortInfo = (STL_PORT_INFO *)((size_t)(pPortInfo) + ROUNDUPP2(sizeof(STL_PORT_INFO), 8));
	}

	return TRUE;
} //get_port_info

static boolean get_led_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	STL_SMP *smp = (STL_SMP *)mad;
	STL_LED_INFO *pLedInfo;
	STL_NODE_INFO *pNodeInfo;
	uint32_t amod = 0x01000000;

	if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
		fprintf(stderr, "%s failed: unable to get_node_info\n", __func__);
		return FALSE;
	}
	pNodeInfo = (STL_NODE_INFO*)stl_get_smp_data(smp);

	if (args->mflag) {
		if (pNodeInfo->NodeType == STL_NODE_FI)
			fprintf(stderr, "%s: -m ignored for led_info against an HFI\n", g_cmdname);
		else
			amod |= (uint32_t)args->dport;
	}

	DBGPRINT("---->get_led_info\n");
	DBGPRINT("->dlid=0x%08x slid=0x%08x dport=%d drpaths=%d\n",
			args->dlid,args->slid,args->dport,args->drpaths);

	status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_LED_INFO, amod, args,
                                    smp, sizeof(*pLedInfo), NULL);

	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	}

    pLedInfo = (STL_LED_INFO *)stl_get_smp_data(smp);
    BSWAP_STL_LED_INFO(pLedInfo);

	if (print)
		PrintStlLedInfo(&g_dest, 0/*indent*/, pLedInfo, 0/*portGuid*/, args->printLineByLine);

	return TRUE;
} //get_led_info


static boolean get_pstate_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	uint8 startPort = args->inport;
	uint8 endPort = args->outport;
	STL_SMP *smp = (STL_SMP *)mad;
	uint32_t amod;
	uint8_t count;
	uint16_t firstPort;

	if (mad_len < sizeof(SMP))
		return FALSE;

	if (args->mflag && !args->mflag2)
		startPort = endPort = args->dport;

	DBGPRINT("---->get_pstate_info\n");
	{
		uint8 maxPort;
		uint8 nodetype;
		STL_NODE_INFO *pNodeInfo;

		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_pstate_info failed: unable to get_node_info\n");
			return FALSE;
		}
		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);

		nodetype = pNodeInfo->NodeType;
		maxPort = pNodeInfo->NumPorts;

		if (nodetype == STL_NODE_FI) {
			if (args->mflag)
				fprintf(stderr, "%s: -m ignored for pstateinfo against an HFI\n", g_cmdname);
			startPort = endPort = 0;
			firstPort = pNodeInfo->u1.s.LocalPortNum;
		} else {
			if (startPort > endPort) {
				fprintf(stderr, "%s failed: start port > end port!\n", __func__);
				return FALSE;
			}
			if (endPort > maxPort) {
				fprintf(stderr, "%s failed: request exceeds "
								"port count: %d max ports\n", __func__, maxPort);
				return FALSE;
			}

			if (startPort == 0  && endPort == 0 && !args->mflag2 && !args->mflag) {
				startPort = 0;
				endPort = maxPort;
			}
			firstPort = startPort;
		}
	}

	count = endPort - startPort + 1;
	amod = (count<<24) | startPort;
	STL_PORT_STATE_INFO *psip;
	status = perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_PORT_STATE_INFO, amod, args,
                            smp, sizeof(STL_PORT_STATE_INFO)*count, NULL);
	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	}

	psip = (STL_PORT_STATE_INFO *)stl_get_smp_data(smp);
	BSWAP_STL_PORT_STATE_INFO(psip, count);
	if (print)
		PrintStlPortStateInfo(&g_dest, 0, psip, count, firstPort, args->printLineByLine);

	return TRUE;
} //get_pstate_info


static boolean get_sm_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	STL_SMP *smp = (STL_SMP *)mad;
	STL_SM_INFO *pSmInfo;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("-->get_sm_info\n");

	status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_SM_INFO, 0, args,
                            smp, sizeof(*pSmInfo), NULL);

	pSmInfo = (STL_SM_INFO *)stl_get_smp_data(smp);
	BSWAP_STL_SM_INFO(pSmInfo);

	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
		if (print)
			PrintStlSMInfo(&g_dest, 0/*indent*/, pSmInfo, 0/*lid*/, args->printLineByLine);
		return TRUE;
	}
}//get_sm_info

static boolean get_sw_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("-->get_sw_info\n");
	{
		STL_NODE_INFO *pNodeInfo;

		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_sw_info failed: unable to get_node_info\n");
			return FALSE;
		}
		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		if (pNodeInfo->NodeType != STL_NODE_SW) {
			fprintf(stderr, "get_sw_info failed: node is not a switch\n");
			return FALSE;
		}
	}

	status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_SWITCH_INFO, 0, args,
                                smp, sizeof(STL_SWITCH_INFO), NULL);
	if (status != FSUCCESS) {
		fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
		return FALSE;
	} else {
        STL_SWITCH_INFO *pSwitchInfo = (STL_SWITCH_INFO *)stl_get_smp_data(smp);
        BSWAP_STL_SWITCH_INFO(pSwitchInfo);

		if (print)
            PrintStlSwitchInfo(&g_dest, 0/*indent*/, pSwitchInfo, 0/*lid*/, args->printLineByLine );
		return TRUE;
	}
} //get_sw_info

static boolean get_slsc_map(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status = FSUCCESS;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("---->get_slsc_map\n");

	STL_SLSCMAP *pSLSCMap;
	status = perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_SL_SC_MAPPING_TABLE, 0, args,
                                smp, sizeof(*pSLSCMap), NULL);
	pSLSCMap = (STL_SLSCMAP *)stl_get_smp_data(smp);
	BSWAP_STL_SLSCMAP(pSLSCMap);
	if (status != FSUCCESS)
		goto done;
	if (print || g_verbose)
		PrintStlSLSCMapSmp(&g_dest, 0 /* indent */, smp, args->printLineByLine);

done:
	if (status != FSUCCESS) {
		fprintf(stderr, "get_slsc_map failed: error obtaining scsl\n");
		return FALSE;
	} else  {
		return TRUE;
	}
} //get_slsc_map

static boolean get_scsc_map(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status = FSUCCESS;
	STL_SMP *smp = (STL_SMP *)mad;
	NODE_TYPE nodetype;
	int i, extended = 0;
	uint8 maxPort;
	uint8 inport, startInPort, endInPort;
	uint8 outport, startOutPort, endOutPort;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;


	DBGPRINT("---->get_scsc_map\n");

	{
		STL_NODE_INFO *pNodeInfo;
		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
			return FALSE;

		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		nodetype = pNodeInfo->NodeType;
		maxPort = pNodeInfo->NumPorts;
	}
	if (nodetype != STL_NODE_SW) {
		fprintf(stderr, "get_scsc_map failed: node is not a switch\n");
		return FALSE;
	}


	uint8 firstPort = 1;
	if (args->mflag && !args->mflag2) {
		// Get the set of tables for [inport, *]
		if (args->dport > maxPort || args->dport < firstPort) {
			fprintf(stderr, "get_scsc_map failed: specified port %u invalid must be %u - %u for this SW\n",
				args->dport, firstPort, maxPort);
			return FALSE;
		}
		startInPort = endInPort = args->dport;
		startOutPort = firstPort;
		endOutPort = maxPort;
	} else if (args->mflag2) {
		// Get the set of tables for the single pair [inport, outport]
		if (args->inport > maxPort || args->inport < firstPort) {
			fprintf(stderr, "get_scsc_map failed: specified port %u invalid must be %u - %u for this SW\n",
				args->inport, firstPort, maxPort);
			return FALSE;
		}
		if (args->outport > maxPort || args->outport < firstPort) {
			fprintf(stderr, "get_scsc_map failed: specified port %u invalid must be %u - %u for this SW\n",
				args->outport, firstPort, maxPort);
			return FALSE;
		}
		startInPort = endInPort = args->inport;
		startOutPort = endOutPort = args->outport;
	} else {
		// Get the tables for all possible pairs of ports.
		startInPort=startOutPort = firstPort;
		endInPort = endOutPort = maxPort;
	}

	for (i=0; i<=extended; i++) {
		for (inport = startInPort; inport <= endInPort; inport++) {
			/**
			* Technically we could fit 4 more blocks (ports) in a LID
			* routed SMP.  But it is not worth it in this tool.
			*/
			for (outport = startOutPort; outport <= endOutPort; outport += STL_NUM_SCSC_BLOCKS_PER_DRSMP) {
				STL_SCSCMAP *pSCSCMap;
				uint16_t numBlocks = 1;
				uint32_t attrmod = 0;

				numBlocks = MIN(STL_NUM_SCSC_BLOCKS_PER_DRSMP, (endOutPort - outport)+1);
				attrmod = (numBlocks << 24) | (i << 20) | (inport << 8) | outport;

				status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_SC_SC_MAPPING_TABLE, attrmod, args,
								smp, sizeof(*pSCSCMap)*numBlocks, NULL);
				pSCSCMap = (STL_SCSCMAP *)stl_get_smp_data(smp);
				BSWAP_STL_SCSCMAP(pSCSCMap);
				if (status != FSUCCESS)
					goto done;
				if (print || g_verbose)
					PrintStlSCSCMapSmp(&g_dest, 0 /* indent */, smp,
						(inport==startInPort && outport==startOutPort), args->printLineByLine);
			}
		}
	}
done:
	if (status != FSUCCESS) {
		fprintf(stderr, "get_scsc_map failed: error obtaining scsc\n");
		return FALSE;
	} else  {
		return TRUE;
	}
} //get_scsc_map

static boolean get_scsl_map(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status = FSUCCESS;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("---->get_scsl_map\n");

	STL_SCSLMAP *pSCSLMap;
	status = perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_SC_SL_MAPPING_TABLE, 0, args,
                                smp, sizeof(*pSCSLMap), NULL);
	pSCSLMap = (STL_SCSLMAP *)stl_get_smp_data(smp);
	BSWAP_STL_SCSLMAP(pSCSLMap);
	if (status != FSUCCESS)
		goto done;
	if (print || g_verbose)
		PrintStlSCSLMapSmp(&g_dest, 0 /* indent */, smp, args->printLineByLine);

done:
	if (status != FSUCCESS) {
		fprintf(stderr, "get_scsl_map failed: error obtaining scsl\n");
		return FALSE;
	} else  {
		return TRUE;
	}
} //get_scsl_map

/** =========================================================================
 * Generic scvlx call
 * attribute defines which table is queried.
 */
static boolean get_scvlx_map(argrec *args, uint8_t *mad, size_t mad_len,
				boolean print, uint16_t attribute)
{
	FSTATUS status = FSUCCESS;
	STL_SMP *smp = (STL_SMP *)mad;
	NODE_TYPE nodetype;
	STL_NODE_INFO *pNodeInfo;
	uint8 maxPort;
	uint16 port = 0;
	uint8 startPort = 0;
	uint8 endPort = 0;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;


	DBGPRINT("---->get_scvlx_map\n");

	{
		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
			return FALSE;

		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		nodetype = pNodeInfo->NodeType;
		maxPort = pNodeInfo->NumPorts;
	}
	if (nodetype == STL_NODE_SW) {
		uint8 firstPort = (attribute == STL_MCLASS_ATTRIB_ID_SC_VLNT_MAPPING_TABLE)? 1 : 0;
		if (args->mflag2) {
			if (args->inport > maxPort || args->inport < firstPort) {
				fprintf(stderr, "get_scvlx_map failed: specified port %u invalid must be %u - %u for this SW\n",
					args->inport, firstPort, maxPort);
				return FALSE;
			}
			if (args->outport > maxPort || args->outport < firstPort) {
				fprintf(stderr, "get_scvlx_map failed: specified port %u invalid must be %u - %u for this SW\n",
					args->outport, firstPort, maxPort);
				return FALSE;
			}
			startPort = args->inport;
			endPort = args->outport;
		} else if (args->mflag) {
			if (args->dport > maxPort || args->dport < firstPort) {
				fprintf(stderr, "get_scvlx_map failed: specified port %u invalid must be %u - %u for this SW\n",
					args->dport, firstPort, maxPort);
				return FALSE;
			}
			startPort = args->dport;
			endPort = args->dport;
		} else {
			startPort = firstPort;
			endPort = maxPort;
		}
	} else {
		startPort = endPort = 0;
		if (args->mflag)
			fprintf(stderr, "%s: -m ignored for SCVLx against an HFI\n", g_cmdname);
	}

	/**
	 * Technically we could fit 4 more blocks (ports) in a LID routed SMP.
	 * But it is not worth it in this tool.
	 */
	for (port = startPort; port <= endPort; port += STL_NUM_SCVL_BLOCKS_PER_DRSMP) {
		STL_SCVLMAP *pSCVLMap;
		uint16_t numBlocks = MIN(STL_NUM_SCVL_BLOCKS_PER_DRSMP, (endPort - port)+1);
		uint32_t attrmod = (numBlocks << 24) | port;

		status = perform_stl_sma_query (attribute, attrmod, args,
                                    smp, sizeof(*pSCVLMap)*numBlocks, NULL);
		pSCVLMap = (STL_SCVLMAP *)stl_get_smp_data(smp);
		BSWAP_STL_SCVLMAP(pSCVLMap);
		if (status != FSUCCESS)
			goto done;
		if (print || g_verbose)
			PrintStlSCVLxMapSmp(&g_dest, 0 /* indent */, smp, nodetype, (port==startPort), args->printLineByLine);
	}
done:
	if (status != FSUCCESS) {
		fprintf(stderr, "get_scvlx_map failed: error obtaining scvlx\n");
		return FALSE;
	} else  {
		return TRUE;
	}
} //get_scvlx_map

static boolean get_scvlt_map(argrec *args, uint8_t *mad, size_t mad_len, boolean print) {
	boolean rc = get_scvlx_map(args, mad, mad_len, print, STL_MCLASS_ATTRIB_ID_SC_VLT_MAPPING_TABLE);
	if (!rc) {
		fprintf(stderr, "get_scvlt_map failed\n");
	}
	return (rc);
} //get_scvlt_map

static boolean get_scvlnt_map(argrec *args, uint8_t *mad, size_t mad_len, boolean print) {
	boolean rc = get_scvlx_map(args, mad, mad_len, print, STL_MCLASS_ATTRIB_ID_SC_VLNT_MAPPING_TABLE);
	if (!rc) {
		fprintf(stderr, "get_scvlnt_map failed\n");
	}
	return (rc);
} //get_scvlnt_map

static boolean get_scvlr_map(argrec *args, uint8_t *mad, size_t mad_len, boolean print) {
	boolean rc = get_scvlx_map(args, mad, mad_len, print, STL_MCLASS_ATTRIB_ID_SC_VLR_MAPPING_TABLE);
	if (!rc) {
		fprintf(stderr, "get_scvlr_map failed\n");
	}
	return (rc);
} //get_scvlr_map

static boolean print_vlarb_element(argrec *args, STL_SMP *smp, uint8_t startPort,
					uint8_t endPort, uint8_t section,
					uint8_t nodeType)
{
	int indent = 0;
	uint16_t port;
	FSTATUS status;

	/**
	 * Technically we could fit more blocks (ports) in a LID routed SMP.
	 * But it is not worth it in this tool.
	 */
	for (port=startPort; port <= endPort; port += STL_NUM_VLARB_PORTS_PER_DRSMP) {
		uint8_t numPorts = MIN(STL_NUM_VLARB_PORTS_PER_DRSMP, endPort - port + 1);
		uint32_t attrmod = (numPorts << 24) | (section << 16) | port;
		status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_VL_ARBITRATION,
						attrmod, args, smp, sizeof(STL_VLARB_TABLE)*numPorts, NULL);
		if (status != FSUCCESS) {
			fprintf(stderr, "%s: %s: %s\n", "get_vlarb", MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
			return status;
		}

		PrintStlVLArbTableSmp(&g_dest, indent, smp, nodeType, args->printLineByLine);
	}
	return FSUCCESS;
}

static boolean get_vlarb(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	uint8 vlCap;
	uint8 startPort = 1, endPort = 1;
	NODE_TYPE nodeType;
	STL_SMP *smp = (STL_SMP *)mad;
	STL_NODE_INFO *pNodeInfo;
	STL_PORT_INFO *pPortInfo;
	STL_SWITCH_INFO *pSwitchInfo;
	uint8_t numPorts;
	int vlarb = 1;

	if (mad_len < sizeof(SMP))
		return FALSE;

	DBGPRINT("-->get_vlarb\n");

	if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
		return FALSE;

	pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
	nodeType = pNodeInfo->NodeType;
	numPorts = pNodeInfo->NumPorts;

	if (nodeType == STL_NODE_SW) {
		argrec argsCpy = *args;
		argsCpy.mflag = 1;
		argsCpy.dport = 0;
		argsCpy.mcount = 1;

		if (!get_port_info(&argsCpy, (uint8_t *)smp, sizeof(*smp), g_verbose))
			return FALSE;

		pPortInfo = (STL_PORT_INFO *)stl_get_smp_data(smp);
		vlarb = pPortInfo->CapabilityMask3.s.VLSchedulingConfig == STL_VL_SCHED_MODE_VLARB;
	}

	if (!get_port_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
		return FALSE;

	pPortInfo = (STL_PORT_INFO *)stl_get_smp_data(smp);
	DBGPRINT("get_vlarb: dlid 0x%08x dport %d has VLCap=%d\n",
			pPortInfo->LID, args->dport, pPortInfo->VL.s2.Cap);

	vlCap = pPortInfo->VL.s2.Cap;

	if (args->mflag && vlCap == 1) {
			fprintf(stderr, "get_vlarb failed: no vlarbtable as this port only has 1 vl; VL0\n");
			return FALSE;
	}

	if (nodeType != STL_NODE_SW)
		vlarb = pPortInfo->CapabilityMask3.s.VLSchedulingConfig == STL_VL_SCHED_MODE_VLARB;

	if (!vlarb) {
		fprintf(stderr, "get_vlarb failed: vlarbtable not supported\n");
		return FALSE;
	}

	if (nodeType == STL_NODE_SW) {
		if (!get_sw_info(args,(uint8_t *)smp, sizeof(*smp), g_verbose))
			return FALSE;

		pSwitchInfo = (STL_SWITCH_INFO *)stl_get_smp_data(smp);
		if ((pSwitchInfo->u2.s.EnhancedPort0 == 1) && (vlCap > 1))
			startPort = 0;

        endPort = numPorts;
	}

	if (args->mflag) {
		if (nodeType == STL_NODE_SW) {
			if (args->dport > endPort) {
				fprintf(stderr, "get_vlarb failed: port number is higher than switch's number of ports\n");
				return FALSE;
			}
			startPort = endPort = args->dport;
		} else
			fprintf(stderr, "%s: -m ignored for vlarb against an HFI\n", g_cmdname);
	}

	status = print_vlarb_element(args, smp, startPort, endPort, STL_VLARB_LOW_ELEMENTS, nodeType);
	if (status != FSUCCESS)
		return status;

	status = print_vlarb_element(args, smp, startPort, endPort, STL_VLARB_HIGH_ELEMENTS, nodeType);
	if (status != FSUCCESS)
		return status;

	status = print_vlarb_element(args, smp, startPort, endPort, STL_VLARB_PREEMPT_ELEMENTS, nodeType);
	if (status != FSUCCESS)
		return status;

	status = print_vlarb_element(args, smp, startPort, endPort, STL_VLARB_PREEMPT_MATRIX, nodeType);
	if (status != FSUCCESS)
		return status;

	return TRUE;
} //get_vlarb


static boolean get_pkey(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status = FSUCCESS;
	STL_SMP *smp = (STL_SMP *)mad;
	uint8 nodetype;
	uint16 cap;
	uint16 block, startBlock, endBlock;
	uint8 port;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("---->get_pkey\n");

	{
		STL_NODE_INFO *pNodeInfo;

		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
			return FALSE;

		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		nodetype = pNodeInfo->NodeType;
		cap = pNodeInfo->PartitionCap;
	}
	if (nodetype == STL_NODE_SW) {
		STL_SWITCH_INFO *pSwInfo;
		if (args->dport) {
			if (!get_sw_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
				return FALSE;
			pSwInfo = (STL_SWITCH_INFO *)stl_get_smp_data(smp);
			cap = pSwInfo->PartitionEnforcementCap;
		}
		port = args->dport;
	} else {
		if (args->mflag)
			fprintf(stderr, "%s: -m ignored for pkey against an HFI\n", g_cmdname);
		port = 0;
	}

	if (args->bflag) {
		if (args->block > MAX_PKEY_BLOCK_NUM) {
			fprintf(stderr, "get_pkey failed: block specified: %u must be 0 - %u\n", args->block,
				MAX_PKEY_BLOCK_NUM);
			return FALSE;
		}
		if (args->block > (cap-1)/ NUM_PKEY_ELEMENTS_BLOCK) {
			fprintf(stderr, "get_pkey failed: specified block %u exceeds cap: cap=%u\n",
					args->block, cap);
			return FALSE;
		}
		startBlock = args->block;
		endBlock = startBlock + args->bcount - 1;
	} else {
		startBlock=0;
		endBlock = ((cap-1) / NUM_PKEY_ELEMENTS_BLOCK);
	}

	for (block = startBlock; block <= endBlock; block += STL_NUM_PKEY_BLOCKS_PER_DRSMP) {
		uint16_t sub;
		uint16_t numBlocks = MIN(STL_NUM_PKEY_BLOCKS_PER_DRSMP, (endBlock - block)+1);
		uint32_t attrmod = 0;

		attrmod = port << 16;
		attrmod |= PKEY_BLOCK_NUM_MASK & block;
		attrmod |= (0xff & numBlocks) << 24;
		status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_PART_TABLE, attrmod, args,
                                        smp,
                                        sizeof(STL_PARTITION_TABLE)*numBlocks, NULL);
		if (status != FSUCCESS)
			break;

		for (sub = 0; sub < numBlocks; sub++) {
			STL_PARTITION_TABLE *pPartTab = (STL_PARTITION_TABLE *)
							(stl_get_smp_data(smp)
								+ (sub *
								sizeof(STL_PARTITION_TABLE)));
			BSWAP_STL_PARTITION_TABLE(pPartTab);
		}
		if (print)
			PrintStlPKeyTableSmp(&g_dest, 0/*indent*/, smp, nodetype,
					(block==startBlock), (startBlock==endBlock), args->printLineByLine);
	}

	if (status != FSUCCESS) {
		fprintf(stderr, "get_pkey failed: error obtaining pkeys\n");
		return FALSE;
	} else
		return TRUE;
} //get_pkey


static boolean get_linfdb(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status = FSUCCESS;
	STL_LID maxLid;
	uint16 block, startBlock, endBlock;
	uint32 attribute = STL_MCLASS_ATTRIB_ID_LINEAR_FWD_TABLE;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT ("-->get_linfdb\n");

	{
		STL_NODE_INFO *pNodeInfo = NULL;
		STL_SWITCH_INFO *pSwInfo = NULL;
		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_linfdb failed: unable to get_node_info\n");
			return FALSE;
		}
		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		if (pNodeInfo->NodeType != STL_NODE_SW) {
			fprintf(stderr, "get_linfdb failed: node is not a switch\n");
			return FALSE;
		}

		if (!get_sw_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_linfdb failed: unable to get_sw_info\n");
			return FALSE;
		}

		pSwInfo = (STL_SWITCH_INFO *)stl_get_smp_data(smp);
		if (pSwInfo->LinearFDBCap == 0) {
			fprintf(stderr, "get_linfdb failed: Linear Forwarding Table not supported\n");
			return FALSE;
		}
		maxLid = MIN(pSwInfo->LinearFDBTop, pSwInfo->LinearFDBCap-1);
		if(maxLid == 0){
			printf("Linear Forwarding Table is empty.\n");
			return TRUE;
		}
	}

	if (args->flid) {
		if (args->flid > maxLid) {
			fprintf(stderr, "get_linfdb failed: FLID 0x%x is greater than maxLid 0x%x\n", args->flid, maxLid);
			return FALSE;
		}
		startBlock = endBlock = args->flid / MAX_LFT_ELEMENTS_BLOCK;
	} else if (args->bflag) {
		startBlock = args->block;
		endBlock = startBlock + args->bcount - 1;
	} else {
		startBlock = 0;
		endBlock = maxLid / MAX_LFT_ELEMENTS_BLOCK;
	}

	for (block = startBlock; block <= endBlock; block += STL_NUM_LFT_BLOCKS_PER_DRSMP) {
		uint16_t sub;
		uint16_t numBlocks = MIN(STL_NUM_LFT_BLOCKS_PER_DRSMP, (endBlock - block)+1);
		uint32_t attrmod = 0;
		STL_LINEAR_FORWARDING_TABLE *pForwardBlock;

		attrmod = block;
		attrmod |= (0xff & numBlocks) << 24;

		status = perform_stl_sma_query (attribute, attrmod, args,
                                        smp, sizeof(*pForwardBlock)*numBlocks, NULL);
		if (status != FSUCCESS)
			break;

		if (print) for (sub = 0, pForwardBlock=(STL_LINEAR_FORWARDING_TABLE*)stl_get_smp_data(smp);
						sub < numBlocks; sub++, pForwardBlock ++) {
			BSWAP_STL_LINEAR_FORWARDING_TABLE(pForwardBlock);
			PrintStlLinearFDB(&g_dest, 0/*indent*/, pForwardBlock, block+sub, args->printLineByLine);
		}
	}
	if (status != FSUCCESS) {
		fprintf(stderr, "get_linfdb failed: unable to obtain LFT\n");
		return FALSE;
	} else
		return TRUE;
} //get_linfdb


static boolean get_mcfdb(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status = FSUCCESS;
	STL_LID maxLid;
	uint32 block, startBlock, endBlock;
	uint8 maxPosition;
	uint8 maxPort;
	uint16 position, startPosition, endPosition;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT ("-->get_mcfdb\n");
	{
		STL_NODE_INFO *pNodeInfo = NULL;
		STL_SWITCH_INFO *pSwInfo = NULL;

		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
			return FALSE;
		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		if (pNodeInfo->NodeType != STL_NODE_SW) {
			fprintf(stderr, "get_mcfdb failed: node is not a switch\n");
			return FALSE;
		}
		maxPort = pNodeInfo->NumPorts;
		maxPosition = (pNodeInfo->NumPorts+1)/STL_PORT_MASK_WIDTH;

		if (!get_sw_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_mcfdb failed: unable to get_sw_info\n");
			return FALSE;
		}

		pSwInfo = (STL_SWITCH_INFO *)stl_get_smp_data(smp);
		if (pSwInfo->MulticastFDBCap == 0) {
			fprintf(stderr, "get_mcfdb failed: Multicast Forwarding Table not supported\n");
			return FALSE;
		}
		maxLid = pSwInfo->MulticastFDBTop;
	}

	if (maxLid == 0 || (maxLid < STL_LID_MULTICAST_BEGIN)) {
		fprintf(stderr, "get_mcfdb failed: Multicast FDB Top is zero.\n");
		return FALSE;
	}
	if (args->flid) {
		if ((args->flid < STL_LID_MULTICAST_BEGIN) || (args->flid > STL_LID_MULTICAST_END)) {
			fprintf(stderr, "get_mcfdb failed: Multicast LID specified: 0x%x must be 0x%x - 0x%x\n", args->flid, STL_LID_MULTICAST_BEGIN, STL_LID_MULTICAST_END);
			return FALSE;
		}
		if (args->flid > maxLid) {
			fprintf(stderr, "get_mcfdb failed: Multicast LID specified: 0x%x in exceeds Switch Max Multicast Lid 0x%x\n",
				args->flid, maxLid);
			return FALSE;
		}

		startBlock = endBlock = (args->flid - STL_LID_MULTICAST_BEGIN) / STL_NUM_MFT_ELEMENTS_BLOCK;

	} else if (args->bflag) {
		startBlock = args->block;
		endBlock = startBlock + args->bcount - 1;

	} else {
		startBlock = 0;
		endBlock = (maxLid - STL_LID_MULTICAST_BEGIN) / STL_NUM_MFT_ELEMENTS_BLOCK;
	}

	if (args->mflag) {
		if (args->dport > maxPort) {
			fprintf(stderr, "get_mcfdb failed: specified port %u exceeds available ports: %u\n",
				args->dport, maxPort);
			return FALSE;
		}
		startPosition = endPosition = args->dport / STL_PORT_MASK_WIDTH;
	} else {
		startPosition = 0;
		endPosition = maxPosition;
	}

	for (block = startBlock; block <= endBlock; block += STL_NUM_MFT_BLOCKS_PER_DRSMP) {
        uint16_t numBlocks = MIN(STL_NUM_MFT_BLOCKS_PER_DRSMP, (endBlock - block)+1);

		for (position = startPosition; position <= endPosition; position++) {
        	uint32_t attrmod = ((0xff & numBlocks) << 24) | (position << 22) |
								(0x0fffff & block);
			status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_MCAST_FWD_TABLE,
											attrmod, args, smp,
                                            sizeof(STL_MULTICAST_FORWARDING_TABLE)*numBlocks, NULL);
			if (status != FSUCCESS)
				break;

			if (print)
				PrintStlMCastSmp(&g_dest, 0, smp, position, block, numBlocks, args->printLineByLine);
		}
		if (status != FSUCCESS)
			break;
	}
	if (status != FSUCCESS) {
		fprintf(stderr, "get_mcfdb failed: unable to obtain MFT\n");
		return FALSE;
	} else
		return TRUE;
} //get_mcfdb



static boolean get_portgroup(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status = FSUCCESS;
	int maxGrps;
	uint16 block, startBlock, endBlock;
	uint32 attribute = STL_MCLASS_ATTRIB_ID_PORT_GROUP_TABLE;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT ("-->get_portgroup\n");

	{
		STL_NODE_INFO *pNodeInfo = NULL;
		STL_SWITCH_INFO *pSwInfo = NULL;
		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_portgroup failed: unable to get_node_info\n");
			return FALSE;
		}
		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		DBGPRINT("STL_NODE_INFO\n");
		if (pNodeInfo->NodeType != STL_NODE_SW) {
			fprintf(stderr, "get_portgroup failed: node is not a switch\n");
			return FALSE;
		}

		if (!get_sw_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_portgroup failed: unable to get_sw_info\n");
			return FALSE;
		}

		pSwInfo = (STL_SWITCH_INFO *)stl_get_smp_data(smp);
		if (pSwInfo->CapabilityMask.s.IsAdaptiveRoutingSupported == 0) {
			fprintf(stderr, "get_portgroup failed: Adaptive Routing not supported\n");
			return FALSE;
		}

		// Use cap for now..
		uint8_t pgtop = pSwInfo->PortGroupTop;
		if (pgtop==0) {
			fprintf(stderr, "get_portgroup failed: No port groups defined\n");
			return FALSE;
		}
		maxGrps = (pgtop-1)/NUM_PGT_ELEMENTS_BLOCK;
	}

	if (args->bflag) {
		startBlock = args->block;
		if (startBlock > maxGrps) {
			fprintf(stderr, "get_portgroup failed: Start block out exceeds max blocks\n");
			return FALSE;
		}
		endBlock = (args->bcount>0) ? startBlock+args->bcount-1 : startBlock;
		if (endBlock > maxGrps) {
			// If requested block count exceeds max, just limit to max
			endBlock = maxGrps;
		}
	} else {
		startBlock = 0;
		endBlock = maxGrps;
	}

	for (block = startBlock; block <= endBlock; block += STL_NUM_PGTABLE_BLOCKS_PER_DRSMP) {
		uint16_t sub;
		uint16_t numBlocks = MIN(STL_NUM_PGTABLE_BLOCKS_PER_DRSMP, (endBlock - block)+1);
		uint32_t attrmod = 0;
		STL_PORT_GROUP_TABLE *pPortGroupBlock;

		attrmod = block;
		attrmod |= (0xff & numBlocks) << 24;

		status = perform_stl_sma_query (attribute, attrmod, args, smp,
                                        sizeof(*pPortGroupBlock) * numBlocks, NULL);
		if (status != FSUCCESS)
			break;

		if (print) for (sub = 0, pPortGroupBlock=(STL_PORT_GROUP_TABLE*)stl_get_smp_data(smp);
						sub < numBlocks; sub++, pPortGroupBlock ++) {
			BSWAP_STL_PORT_GROUP_TABLE(pPortGroupBlock);
			PrintStlPortGroupTable(&g_dest, 0/*indent*/, (uint64_t *)(pPortGroupBlock->PgtBlock), block+sub, 0, args->printLineByLine);
		}
	}
	if (status != FSUCCESS) {
		fprintf(stderr, "get_portgroup failed: unable to obtain PGT\n");
		return FALSE;
	} else
		return TRUE;
}

static boolean get_portgroupfdb(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status = FSUCCESS;
	STL_LID maxLid;
	uint16 block, startBlock, endBlock;
	uint32 attribute = STL_MCLASS_ATTRIB_ID_PORT_GROUP_FWD_TABLE;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT ("-->get_portgroupfdb\n");

	{
		STL_NODE_INFO *pNodeInfo = NULL;
		STL_SWITCH_INFO *pSwInfo = NULL;
		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_portgroupfdb failed: unable to get_node_info\n");
			return FALSE;
		}
		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		if (pNodeInfo->NodeType != STL_NODE_SW) {
			fprintf(stderr, "get_portgroupfdb failed: node is not a switch\n");
			return FALSE;
		}

		if (!get_sw_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_portgroupfdb failed: unable to get_sw_info\n");
			return FALSE;
		}

		pSwInfo = (STL_SWITCH_INFO *)stl_get_smp_data(smp);
		if (pSwInfo->CapabilityMask.s.IsAdaptiveRoutingSupported == 0) {
			fprintf(stderr, "get_portgroupfdb failed: Adaptive Routing not supported\n");
			return FALSE;
		}
		maxLid = MIN(pSwInfo->LinearFDBTop,
					 pSwInfo->PortGroupFDBCap ? pSwInfo->PortGroupFDBCap - 1 : DEFAULT_MAX_PGFT_LID);
	}

	if (args->flid) {
		if (args->flid > maxLid) {
			fprintf(stderr, "get_portgroupfdb failed: FLID 0x%x is greater than maxLid 0x%x\n", args->flid, maxLid);
			return FALSE;
		}
		startBlock = endBlock = args->flid / NUM_PGFT_ELEMENTS_BLOCK;
	} else if (args->bflag) {
		startBlock = args->block;
		if (startBlock > maxLid / NUM_PGFT_ELEMENTS_BLOCK) {
			fprintf(stderr, "get_portgroupfdb failed: Start block out exceeds max blocks\n");
			return FALSE;
		}
		endBlock = startBlock + args->bcount - 1;
		if (endBlock > maxLid / NUM_PGFT_ELEMENTS_BLOCK) {
			endBlock = maxLid / NUM_PGFT_ELEMENTS_BLOCK;
			fprintf(stderr, "portgroupfdb failed: Requested blocks(%d) exceeds max blocks(%d)\n",
							args->bcount, endBlock+1);
			return FALSE;
		}
	} else {
		startBlock = 0;
		endBlock = maxLid / NUM_PGFT_ELEMENTS_BLOCK;
	}

	for (block = startBlock; block <= endBlock; block += STL_NUM_PGFT_BLOCKS_PER_DRSMP) {
		uint16_t sub;
		uint16_t numBlocks = MIN(STL_NUM_PGFT_BLOCKS_PER_DRSMP, (endBlock - block)+1);
		uint32_t attrmod = 0;
		STL_PORT_GROUP_FORWARDING_TABLE *pPortGroupBlock;

		attrmod = block;
		attrmod |= (0xff & numBlocks) << 24;

		status = perform_stl_sma_query (attribute, attrmod, args, smp,
                                    sizeof(*pPortGroupBlock) * numBlocks, NULL);
		if (status != FSUCCESS)
			break;

		if (print) for (sub = 0, pPortGroupBlock=(STL_PORT_GROUP_FORWARDING_TABLE*)stl_get_smp_data(smp);
						sub < numBlocks; sub++, pPortGroupBlock ++) {
			BSWAP_STL_PORT_GROUP_FORWARDING_TABLE(pPortGroupBlock);
			PrintStlPortGroupFDB(&g_dest, 0/*indent*/, pPortGroupBlock, block+sub, args->printLineByLine);
		}
	}
	if (status != FSUCCESS) {
		fprintf(stderr, "get_portgroupfdb failed: unable to obtain LFT\n");
		return FALSE;
	} else
		return TRUE;
}



static boolean get_cong_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	STL_CONGESTION_INFO *pCongestionInfo;
	STL_SMP *smp = (STL_SMP*)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("--->get_cong_info\n");

	if (perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_CONGESTION_INFO, 0, args,
                                smp, sizeof(*pCongestionInfo), NULL) != FSUCCESS) {
		fprintf(stderr, "get_cong_info failed: unable to obtain congestion info\n");
		return FALSE;
	}

	pCongestionInfo = (STL_CONGESTION_INFO *)stl_get_smp_data(smp);
	BSWAP_STL_CONGESTION_INFO(pCongestionInfo);
	if (print)
		PrintStlCongestionInfo(&g_dest, 0, pCongestionInfo, args->printLineByLine);

	return TRUE;

}

static boolean get_sw_cong_log(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	STL_SWITCH_CONGESTION_LOG *pSwitchCongestionLog;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;


	DBGPRINT("--->get_sw_cong_log\n");
	{
		STL_NODE_INFO *pNodeInfo = NULL;
		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
			return FALSE;

		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		if (pNodeInfo->NodeType != STL_NODE_SW) {
			fprintf(stderr, "get_sw_cong_log failed: node is not a switch\n");
			return FALSE;
		}
	}


	if (perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_SWITCH_CONGESTION_LOG, 0, args,
                                smp, sizeof(*pSwitchCongestionLog), NULL) != FSUCCESS) {
		fprintf(stderr, "get_sw_cong_log failed: unable to obtain switch congestion log\n");
		return FALSE;
	}

	pSwitchCongestionLog = (STL_SWITCH_CONGESTION_LOG *) stl_get_smp_data(smp);
	BSWAP_STL_SWITCH_CONGESTION_LOG(pSwitchCongestionLog);
	if (print)
		PrintStlSwitchCongestionLog(&g_dest, 0, pSwitchCongestionLog, args->printLineByLine);

	return TRUE;
}


static boolean get_hfi_cong_log(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	STL_HFI_CONGESTION_LOG *pHfiCongestionLog;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("--->get_hfi_cong_log\n");
	{
		STL_NODE_INFO *pNodeInfo = NULL;

		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose))
			return FALSE;

		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);

		if (pNodeInfo->NodeType == STL_NODE_SW) {
			STL_SWITCH_INFO *pSwitchInfo = NULL;

			if (perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_SWITCH_INFO, 0, args,
                                    smp, sizeof(*pSwitchInfo), NULL) != FSUCCESS) {
				fprintf(stderr, "get_hfi_cong_log failed: Couldn't get SwitchInfo for switch\n");
				return FALSE;
			}

			pSwitchInfo = (STL_SWITCH_INFO *)stl_get_smp_data(smp);
			BSWAP_STL_SWITCH_INFO(pSwitchInfo);

			if (!pSwitchInfo->u2.s.EnhancedPort0) {
				fprintf(stderr, "get_hfi_cong_log failed: Switch Port 0 not enhanced.\n");
				return FALSE;
			}
		}
	}

	if (perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_HFI_CONGESTION_LOG, 0, args,
                                smp, sizeof(*pHfiCongestionLog), NULL) != FSUCCESS) {
		fprintf(stderr, "get_hfi_cong_sett failed: unable to get HFI congestion log\n");
		return FALSE;
	}

	pHfiCongestionLog = (STL_HFI_CONGESTION_LOG *)stl_get_smp_data(smp);
	BSWAP_STL_HFI_CONGESTION_LOG(pHfiCongestionLog);

	if (print)
		PrintStlHfiCongestionLog(&g_dest, 0, pHfiCongestionLog, args->printLineByLine);

	return TRUE;
}

static boolean get_hfi_cong_sett(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	STL_HFI_CONGESTION_SETTING *pHfiCongestionSetting;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("--->get_hfi_cong_sett\n");

	if (perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_HFI_CONGESTION_SETTING, 0, args,
                                smp, sizeof(*pHfiCongestionSetting), NULL) != FSUCCESS) {
		fprintf(stderr, "get_hfi_cong_sett failed: unable to obtain hfi congestion setting\n");
		return FALSE;
	}

	pHfiCongestionSetting = (STL_HFI_CONGESTION_SETTING *) stl_get_smp_data(smp);
	BSWAP_STL_HFI_CONGESTION_SETTING(pHfiCongestionSetting);
	if (print)
		PrintStlHfiCongestionSetting(&g_dest, 0, pHfiCongestionSetting, args->printLineByLine);

	return TRUE;
}

static boolean get_sw_cong_sett(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	STL_SWITCH_CONGESTION_SETTING *pSwitchCongestionSetting;
	STL_SMP *smp = (STL_SMP *)mad;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	DBGPRINT("--->get_sw_cong_sett\n");

	if (perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_SWITCH_CONGESTION_SETTING, 0, args,
                                smp, sizeof(*pSwitchCongestionSetting), NULL) != FSUCCESS) {
		fprintf(stderr, "get_sw_cong_sett failed: unable to obtain switch congestion setting\n");
		return FALSE;
	}

	pSwitchCongestionSetting = (STL_SWITCH_CONGESTION_SETTING *) stl_get_smp_data(smp);
	BSWAP_STL_SWITCH_CONGESTION_SETTING(pSwitchCongestionSetting);
	if (print)
		PrintStlSwitchCongestionSetting(&g_dest, 0, pSwitchCongestionSetting, args->printLineByLine);

	return TRUE;
}

static boolean get_swport_cong_sett(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	STL_SWITCH_PORT_CONGESTION_SETTING *pSwitchPortCongestionSetting;
	STL_SMP *smp = (STL_SMP *)mad;
	uint32_t amod = 1<<24;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	if (args->bflag) amod = (args->bcount<<24) | (args->block & 0xff);


	DBGPRINT("--->get_swport_cong_sett\n");

	if (perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_SWITCH_PORT_CONGESTION_SETTING, amod, args,
                                smp, sizeof(*pSwitchPortCongestionSetting), NULL) != FSUCCESS) {
		fprintf(stderr, "get_swport_cong_sett failed: unable to obtain switch congestion setting\n");
		return FALSE;
	}

	pSwitchPortCongestionSetting = (STL_SWITCH_PORT_CONGESTION_SETTING *) stl_get_smp_data(smp);
	BSWAP_STL_SWITCH_PORT_CONGESTION_SETTING(pSwitchPortCongestionSetting, (amod >> 24));

	if (print)
		PrintStlSwitchPortCongestionSettingSmp(&g_dest, 0, smp, args->printLineByLine);

	return TRUE;

}

static boolean get_hfi_congcont_table(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	STL_HFI_CONGESTION_CONTROL_TABLE *pHfiCongestionControl;
	STL_SMP *smp = (STL_SMP *)mad;
	uint32_t amod = 1<<24;

	if (mad_len < sizeof(STL_SMP))
		return FALSE;

	if (args->bcount > 14) { // 14 blocks is approximately the maximum number of blocks per MAD
		fprintf(stderr, "get_hfi_congcont_table failed: Requested number of blocks for this query is too large. Maximum of 14 blocks.\n");
		return FALSE;
	}

	if (args->bflag) amod = (args->bcount<<24 | (args->block & 0xff));

	DBGPRINT("--->get_hfi_congcont_table\n");

	if (perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_HFI_CONGESTION_CONTROL_TABLE, amod, args,
                            smp, sizeof(*pHfiCongestionControl), NULL) != FSUCCESS) {
		fprintf(stderr, "get_hfi_congcont_table failed: unable to obtain HFI congestion control table\n");
		return FALSE;
	}

	pHfiCongestionControl = (STL_HFI_CONGESTION_CONTROL_TABLE *) stl_get_smp_data(smp);
	BSWAP_STL_HFI_CONGESTION_CONTROL_TABLE(pHfiCongestionControl, amod>>24 & 0xff);

	if (print)
		PrintStlHfiCongestionControlTab(&g_dest, 0, pHfiCongestionControl, amod>>24, amod & 0xff, args->printLineByLine);

	return TRUE;
}

static boolean get_bfr_ctrl_table(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	uint8 maxPort;
    uint8 startPort = args->inport;
    uint8 endPort = args->outport;
    uint8 nodetype;
	STL_SMP *smp = (STL_SMP *)mad;
    uint8 maxcount = STL_NUM_BFRCTLTAB_BLOCKS_PER_DRSMP;
	uint8_t block;

	if (mad_len < sizeof(SMP))
		return FALSE;

	if (args->mflag && !args->mflag2)
		startPort = endPort = args->dport;

	DBGPRINT("-->get_bfr_ctrl_table\n");
	{
		STL_NODE_INFO *pNodeInfo;

		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_bfr_ctrl_table failed: unable to get_node_info\n");
			return FALSE;
		}
		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);

        nodetype = pNodeInfo->NodeType;
		maxPort = pNodeInfo->NumPorts;

		if (nodetype == STL_NODE_FI) {
			if (args->mflag)
				fprintf(stderr, "%s: -m ignored for bfrctrl against an HFI\n", g_cmdname);

			startPort = 0;
			endPort = 0;
		} else if (startPort > endPort || endPort > maxPort) {
			fprintf(stderr, "get_bfr_ctrl_table failed: request exceeds "
							"port count: %d max ports\n", maxPort);
			return FALSE;
		}

		if (nodetype == STL_NODE_SW && startPort == 0  && endPort == 0 &&
			!args->mflag2 && !args->mflag) {
			startPort = 1;
			endPort = maxPort;
		}
	}

	for (block = startPort; block <= endPort; block += maxcount) {
		uint8_t n = MIN(maxcount, (endPort - block)+1);
		uint32_t amod = (n << 24) | block;
		status = perform_stl_sma_query (STL_MCLASS_ATTRIB_ID_BUFFER_CONTROL_TABLE, amod, args,
                        smp, sizeof(STL_BUFFER_CONTROL_TABLE)*n, NULL);

		if (status != FSUCCESS) {
			fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
			return FALSE;
		}

		// PrintStlBfrCtlTableSmp will do BSWAP
		if (print)
			PrintStlBfrCtlTableSmp(&g_dest, 0, smp, nodetype, args->printLineByLine);
	}

	return TRUE;
} //get_bfr_ctrl_table

static boolean get_cable_info(argrec *args, uint8_t *mad, size_t mad_len, boolean print)
{
	FSTATUS status;
	STL_SMP *smp = (STL_SMP *)mad;
	uint16_t startAddr = STL_CIB_STD_LOW_PAGE_ADDR;
	uint16_t len = 2*STL_CIB_STD_LEN;
	STL_NODE_INFO *pNodeInfo;
	NODE_TYPE nodetype;
	uint8_t maxPort;
	uint8_t cableInfoData[STL_CABLE_INFO_PAGESZ];
	uint8_t get_len = 0;
	uint8_t bufoffset = 0;	// where to add next data to cableInfoData
	uint16_t bufstart = 0;	// startAddr for cableInfoData[0]
	uint8_t buflen = 0;	// amount of data in cableInfoData
	boolean hexOnly = FALSE;	//indicates if only hexdump of cableInfoData should be produced

	if (args->bflag) {
		startAddr = args->block;
		if (args->bcount==0) {
			fprintf(stderr, "get_cable_info failed: Invalid length specified\n");
			return FALSE;
		}
		len = args->bcount;
		hexOnly = TRUE;
	}
	uint8_t portType;

	if (mad_len < sizeof(SMP))
		return FALSE;

	{
		if (!get_node_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_cable_info failed: unable to get_node_info\n");
			return FALSE;
		}

		pNodeInfo = (STL_NODE_INFO *)stl_get_smp_data(smp);
		nodetype = pNodeInfo->NodeType;
		maxPort = pNodeInfo->NumPorts;
	}

	if (nodetype == STL_NODE_SW) {
		if (args->mflag) {
			if (args->dport > maxPort || args->dport < 1) {
				fprintf(stderr, "get_cable_info failed: specified port %u invalid must be %u - %u for this SW\n",
					args->dport, 1, maxPort);
				return FALSE;
			}
		} else {
			fprintf(stderr, "get_cable_info failed: Must specify destination port on SW\n");
			return FALSE;
		}
	} else {
		args->dport = pNodeInfo->u1.s.LocalPortNum;
		if (args->mflag) {
			fprintf(stderr, "%s: -m ignored for cable_info against an HFI\n", g_cmdname);
		}
	}

	if (startAddr + len-1 > STL_CABLE_INFO_MAXADDR) {
		fprintf(stderr, "get_cable_info failed: Invalid address range\n");
		return FALSE;
	}

	DBGPRINT("-->get_cable_info\n");
	{
		STL_PORT_INFO *pPortInfo;

		if (!get_port_info(args, (uint8_t *)smp, sizeof(*smp), g_verbose)) {
			fprintf(stderr, "get_cable_info_failed: unable to get_port_info\n");
			return FALSE;
		}

		pPortInfo = (STL_PORT_INFO *)stl_get_smp_data(smp);

		portType = pPortInfo->PortPhysConfig.s.PortType;
	}

	for (; len > 0; startAddr += get_len, len -= get_len) {
		if (bufoffset == 0) {
			bufstart = startAddr;
			buflen = 0;
		}
		// can't cross page in single query
		get_len = MIN(len, STL_CABLE_INFO_DATA_SIZE);
		get_len = MIN(get_len, STL_CABLE_INFO_PAGESZ - (startAddr % STL_CABLE_INFO_PAGESZ));

		DEBUG_ASSERT(startAddr/STL_CABLE_INFO_PAGESZ == (startAddr + get_len-1)/STL_CABLE_INFO_PAGESZ);

		uint32_t amod = (startAddr & 0x0fff)<<19 | ((get_len-1) & 0x3f)<<13 | (args->dport & 0xff);

		status = perform_stl_sma_query(STL_MCLASS_ATTRIB_ID_CABLE_INFO, amod, args,
                                smp, sizeof(STL_CABLE_INFO), NULL);
		if (status != FSUCCESS) {
			fprintf(stderr, "%s: %s: %s\n", __func__, MSG_SMP_SEND_RECV_FAILURE, iba_fstatus_msg(status));
			return FALSE;
		}

		if (print) {
			uint8_t detail = g_detail;
			STL_CABLE_INFO *pCableInfo = (STL_CABLE_INFO*)stl_get_smp_data(smp);

			if (detail > CABLEINFO_DETAIL_ALL)
				detail = CABLEINFO_DETAIL_ALL;
			BSWAP_STL_CABLE_INFO(pCableInfo);
			memcpy(&cableInfoData[bufoffset], pCableInfo->Data, get_len);
			buflen += get_len;
			if (len <= get_len /* final SMP */
				|| ((startAddr + get_len) % STL_CABLE_INFO_PAGESZ) == 0 /* page end */
				) {
				PrintStlCableInfo(&g_dest, 0, cableInfoData, bufstart, buflen, portType, detail, args->printLineByLine,hexOnly);
				bufoffset = 0;
			} else {
				bufoffset += get_len;
			}
		}
	}

	return TRUE;
} //get_cable_info

optypes_t sma_query [] = {
//name			func			        displayAbridged, description,    mflag mflag2 fflag bflag iflag tflag
{ "bfrctrl",	 get_bfr_ctrl_table,	TRUE,  "Buffer control tables", TRUE, TRUE, },
{ "cableinfo",	 get_cable_info,		TRUE,  "Cable info", TRUE, FALSE, FALSE, TRUE},
{ "conginfo",	 get_cong_info, 	    TRUE,  "Congestion info", FALSE },
{ "desc",        get_node_desc,         TRUE,  "Node descriptions/names", FALSE },
{ "hficongcon",  get_hfi_congcont_table,FALSE, "HFI congesting control settings", FALSE, FALSE, TRUE, TRUE },
{ "hficonglog",  get_hfi_cong_log,	    FALSE, "HFI congestion logs", FALSE, FALSE, FALSE, TRUE },
{ "hficongset",  get_hfi_cong_sett,     FALSE, "HFI congestion settings", FALSE },
{ "linfdb",      get_linfdb,            FALSE, "Switch linear FDB tables", FALSE,  FALSE, TRUE,  TRUE },
{ "mcfdb",       get_mcfdb,             FALSE, "Switch multicast FDB tables", TRUE,  FALSE, TRUE, TRUE },
{ "portgroup",   get_portgroup,         FALSE, "Adaptive Routing port groups", FALSE, FALSE, FALSE, TRUE},
{ "portgroupfdb",get_portgroupfdb,      FALSE, "Adaptive Routing port group FWD tables", FALSE,  FALSE, TRUE, TRUE },
{ "nodeaggr",    get_node_aggr,         TRUE,  "Node info and node descriptions", FALSE },
{ "nodedesc",    get_node_desc,         FALSE, "Node descriptions/names", FALSE },
{ "nodeinfo",	 get_node_info,	        FALSE, "Node info", TRUE },
{ "node",	     get_node_info,	        FALSE, "Node info", TRUE },
{ "portinfo",	 get_port_info,	        TRUE,  "Port info", TRUE, FALSE, FALSE, FALSE, TRUE },
{ "pstateinfo",  get_pstate_info,       FALSE, "Switch port state info", TRUE, TRUE },
{ "pkey",		 get_pkey,		        FALSE, "P-Key tables ", TRUE,  FALSE, FALSE, TRUE },
{ "slsc",	     get_slsc_map,	        FALSE, "SL to SC mapping tables", FALSE },
{ "scsl",	     get_scsl_map,	        FALSE, "SC to SL mapping tables", FALSE },
{ "scsc",	     get_scsc_map,	        FALSE, "SC to SC mapping tables", TRUE, TRUE },
{ "scvlt",	     get_scvlt_map,	        FALSE, "SC to VLt tables", TRUE, TRUE },
{ "scvlnt",	     get_scvlnt_map,        FALSE, "SC to VLnt tables", 	TRUE, TRUE },
{ "scvlr",	     get_scvlr_map,         FALSE, "SC to VLr tables", 	TRUE, TRUE },
{ "sminfo",	     get_sm_info,	        FALSE, "SM info", FALSE },
{ "swaggr",      get_switch_aggr,       FALSE, "Node info and switch info", FALSE },
{ "swconglog",	 get_sw_cong_log,	    FALSE, "Switch congestion logs", FALSE, FALSE, FALSE, TRUE },
{ "swcongset",	 get_sw_cong_sett,	    FALSE, "Switch congestion settings", FALSE },
{ "swinfo",	     get_sw_info,	        TRUE,  "Switch info", FALSE },
{ "swportcong",  get_swport_cong_sett,  FALSE, "Switch congestion settings", FALSE, FALSE, FALSE, TRUE },
{ "vlarb",	     get_vlarb,	            FALSE, "VL arbitration tables", TRUE },
{ "ibnodeinfo",	 get_ib_node_info,		FALSE, "IB node info", FALSE },
{ "ledinfo",	 get_led_info,		    FALSE, "Led Info", TRUE },
{ NULL }
};

void sma_Usage(boolean displayAbridged)
{
	DBGPRINT("sma_Usage\n");
	int i = 0;

	fprintf(stderr, "Usage: %s -o otype [standard options] [otype options] [hop hop hop...]\n", g_cmdname);
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       opasmaquery --help\n");
	fprintf(stderr, "    --help - produce full help text\n\n");

	fprintf(stderr, "Standard options: [-v] [-d detail] [-g] [-l lid] [-h hfi] [-p port]\n");
	fprintf(stderr, "    -o otype Output type. See below for list.\n");
	fprintf(stderr, "    -v       Verbose output. Can be specified more than once for\n");
	fprintf(stderr, "             additional openib debugging and libibumad debugging.\n");
	fprintf(stderr, "    -d detail  Output detail level 0-n CableInfo only (default is 2)\n");
	fprintf(stderr, "    -g       Display in line-by-line format (default is summary format)\n");
	fprintf(stderr, "    -l lid   Destination lid, default is local port\n");
	fprintf(stderr, "    -h hfi   hfi, numbered 1..n, 0= -p port will be a\n");
	fprintf(stderr, "             system wide port num (default is 0)\n");
	fprintf(stderr, "    -p port  port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "             is 1st active)\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");
	fprintf(stderr, "Note: will fallback to h:1 p:1 if no active port is found in system\n\n");
	fprintf(stderr, "otype options vary by report: [-m port|port1,port2]\n");
	fprintf(stderr, "    [-f lid] [-b block[,count]]\n");
	fprintf(stderr, "    -m port  port in destination device to query\n");
	fprintf(stderr, "    -m port1,port2  for some reports, a range of ports between\n");
	fprintf(stderr, "             port1 and port2. For others, this is describes\n");
	fprintf(stderr, "             an inport/outport pair.\n");
	fprintf(stderr, "    -f lid   lid to lookup in forwarding table to select an LFT or MFT block\n");
	fprintf(stderr, "             Default is to show entire table\n");
	fprintf(stderr, "    -b b     display block or byte \"b\" of a larger table\n");
	fprintf(stderr, "    -b b,c   display \"c\" blocks or bytes of data starting with block/byte \"b\"\n");
	fprintf(stderr, "    -b ,c    display \"c\" blocks or bytes of data starting with block/byte 0\n");

	fprintf(stderr, "Possible output types (default is nodeinfo):\n");
	i=0;
	fprintf(stderr, "         %-12s: %-55s\n","Output Type","Description");
	fprintf(stderr, "                     : %-55s\n","Supported Options");
	fprintf(stderr, "         %-12s: %-55s\n","-----------","---------------------------------------------------");
	while (sma_query[i].name != NULL) {
		if ( (displayAbridged && sma_query[i].displayWithAbridged) || (!displayAbridged) ) {
			fprintf(stderr, "         %-12s: %-55s\n",sma_query[i].name,sma_query[i].description);

			if ((sma_query[i].gflag)||(sma_query[i].mflag)||(sma_query[i].mflag2)||(sma_query[i].bflag)||(sma_query[i].fflag)||
				(sma_query[i].tflag)||(sma_query[i].iflag)) {
				fprintf(stderr, "         %-12s:", " ");
				if (sma_query[i].mflag) fprintf(stderr, " [-m dest_port]");
				if (sma_query[i].mflag2) fprintf(stderr, " [-m port1,port2]");
				if (sma_query[i].bflag) fprintf(stderr, " [-b %s]",
					strcmp(sma_query[i].name, "cableinfo") == 0 ? "address[,length]" : "block[,count]");
				if (sma_query[i].fflag) fprintf(stderr, " [-f flid]");
//				if (sma_query[i].iflag) fprintf(stderr, " [-i]");
				fprintf(stderr,"\n");
			}
			fprintf(stderr,"\n");
		}
		i++;
	}

	if (!displayAbridged) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Examples:\n");
		fprintf(stderr, "---------\n");
		fprintf(stderr, "opasmaquery -o desc -l 6                  # get nodedesc via lid routed\n");
		fprintf(stderr, "opasmaquery -o nodedesc 1 3               # get nodedesc via directed route\n");
		fprintf(stderr, "                                           # (2 dr hops)\n");
		fprintf(stderr, "opasmaquery -o nodeinfo -l 2 3            # get nodeinfo via a combination of\n");
		fprintf(stderr, "                                           # lid routed and directed route\n");
		fprintf(stderr, "                                           # (1 dr hop)\n");
		fprintf(stderr, "opasmaquery -o portinfo                   # get local port info\n");
		fprintf(stderr, "opasmaquery -o portinfo -l 6 -m 1         # get port info of port 1 of lid 6\n");
		fprintf(stderr, "opasmaquery -o pkey -l 2 3                # get pkey table entries starting\n");
		fprintf(stderr, "                                           # (lid routed to lid 2,\n");
		fprintf(stderr, "                                           # then 1 dr hop to port 3)\n");
		fprintf(stderr, "opasmaquery -o vlarb -l 6                 # get vlarb table entries from lid 6\n");
		fprintf(stderr, "opasmaquery -o swinfo -l 2                # get switch info\n");
		fprintf(stderr, "opasmaquery -o sminfo -l 1                # get SM info\n");
		fprintf(stderr, "opasmaquery -o slsc -l 3                  # get sl2sc table entries from lid 3\n");
		fprintf(stderr, "opasmaquery -o scsl -l 3                  # get sc2sl table entries from lid 3\n");
	}
	exit(2);
}	//Usage
