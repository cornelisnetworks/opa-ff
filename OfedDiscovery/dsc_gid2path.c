/* BEGIN_ICS_COPYRIGHT4 ****************************************

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

** END_ICS_COPYRIGHT4   ****************************************/
/*!

  \file dsc_gid2path.c
 
  $Revision: 1.55 $
  $Date: 2015/03/30 19:33:26 $

  \brief Various functions for interfacing to IbAccess's SD api.
*/

/* work around conflicting names */
#include <infiniband/mad.h>
#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include "iba/ibt.h"
#include "iba/ib_sd.h"
#include "iba/stl_sd.h"
#include "iba/public/ievent.h"
#include "iba/public/ilist.h"
#include "opasadb_debug.h"
#include "dsc_module.h"
#include "dsc_hca.h"
#include "oib_utils_sa.h"

#include <netinet/in.h>

#define PATHRECORD_NUMBPATH 127 /* max to request */

uint32 SATimeoutSec = 10;
uint32 SARetry = 5;

/* qlogic mod needed to talk to other switches */
uint32 NoSubscribe = 0;


static FSTATUS dsc_check_query_results(PQUERY_RESULT_VALUES pQueryResults)
{
	_DBG_FUNC_ENTRY;

	if (pQueryResults == NULL) {
		_DBG_WARN("NULL query results returned for node record query!\n");
		return FERROR;
	}

	if (pQueryResults->Status != FSUCCESS)
	{
		_DBG_WARN("SA Query Returned Failed status = %s(%d)\n",
				_DBG_PTR(FSTATUS_MSG(pQueryResults->Status)),
				pQueryResults->Status);
		return pQueryResults->Status;
	}

	if (pQueryResults->ResultDataSize == 0) {
		_DBG_INFO("SA Query Returned no data!\n");
		return FNOT_FOUND;
	}

	if (pQueryResults->QueryResult == NULL) {
		_DBG_INFO("SA Query Returned no result data!\n");
		return FNOT_FOUND;
	}

	_DBG_FUNC_EXIT;

	return FSUCCESS;
}

static inline void dump_path_record(IB_PATH_RECORD_NO *path, int byte_order)
{
#if defined(ICSDEBUG)
	char gid_str[INET6_ADDRSTRLEN];
	char gid_str2[INET6_ADDRSTRLEN];

	if (byte_order) {
		_DBG_DEBUG("Path SGID(%s) SLID(0x%x) to DGID(%s) DLID(0x%x) SID(0x%016" PRIx64 ") PKEY(0x%x)\n",
			  inet_ntop(AF_INET6, path->SGID.Raw, gid_str2, sizeof gid_str2),
			  ntohs(path->SLID),
			  inet_ntop(AF_INET6, path->DGID.Raw, gid_str, sizeof gid_str),
			  ntohs(path->DLID),
			  ntohll(path->ServiceID),
			  ntohs(path->P_Key));
	} else {
		_DBG_DEBUG("Query SGID(%Lx:%Lx) SLID(0x%x) to DGID(%Lx:%Lx) DLID(0x%x) SID(0x%016" PRIx64 ") PKEY(0x%x)\n",
			  path->SGID.Type.Global.InterfaceID,
			  path->SGID.Type.Global.SubnetPrefix,
			  path->SLID,
			  path->DGID.Type.Global.InterfaceID,
			  path->DGID.Type.Global.SubnetPrefix,
			  path->DLID,
			  path->ServiceID,
			  path->P_Key);
	}
#endif
}

static FSTATUS dsc_process_path_records_query_results(dsc_src_port_t *src_port, PPATH_RESULTS pPathResults)
{
	FSTATUS rval = FSUCCESS;
	unsigned i;
	IB_PATH_RECORD_NO *path_record;

	_DBG_FUNC_ENTRY;

	if (pPathResults->NumPathRecords == 0) {
		rval = FNOT_FOUND;
		goto exit;
	}

	for (i = 0, path_record = (IB_PATH_RECORD_NO*)pPathResults->PathRecords;
		 i < pPathResults->NumPathRecords;
		 i++, path_record++) {
		dump_path_record(path_record,1);
		rval = dsc_add_path_record(src_port, path_record);
		if (rval != FSUCCESS) {
			break;
		}
	}

exit:
	_DBG_FUNC_EXIT;
	return rval;
}

FSTATUS dsc_query_path_records(dsc_src_port_t *src_port, dsc_dst_port_t *dst_port, uint64_t sid, uint16_t pkey)
{
	FSTATUS rval = FNOT_FOUND;
	QUERY PathQuery;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	struct oib_port *hdl;

	_DBG_FUNC_ENTRY;

	// While the mask prevents unused fields from being used in the query,
	// this will still help when trying to dump or debug paths.
	memset(&PathQuery,0,sizeof(QUERY));

	// Get ALL Paths between ports using Local and Remote Port GIDs
	PathQuery.InputType = InputTypePathRecord;
	PathQuery.OutputType = OutputTypePathRecordNetworkOrder;
	PathQuery.InputValue.PathRecordValue.ComponentMask = IB_PATH_RECORD_COMP_SERVICEID |
															IB_PATH_RECORD_COMP_DGID |
															IB_PATH_RECORD_COMP_SGID |
															IB_PATH_RECORD_COMP_PKEY |
															IB_PATH_RECORD_COMP_REVERSIBLE |
															IB_PATH_RECORD_COMP_NUMBPATH;
	PathQuery.InputValue.PathRecordValue.PathRecord.DGID.Type.Global.SubnetPrefix = ntohll(dst_port->gid.global.subnet_prefix);
	PathQuery.InputValue.PathRecordValue.PathRecord.DGID.Type.Global.InterfaceID  = ntohll(dst_port->gid.global.interface_id);
	PathQuery.InputValue.PathRecordValue.PathRecord.SGID.Type.Global.SubnetPrefix = ntohll(src_port->gid.global.subnet_prefix);
	PathQuery.InputValue.PathRecordValue.PathRecord.SGID.Type.Global.InterfaceID  = ntohll(src_port->gid.global.interface_id);
	PathQuery.InputValue.PathRecordValue.PathRecord.ServiceID = ntohll(sid);
	PathQuery.InputValue.PathRecordValue.PathRecord.P_Key = ntohs(pkey ) & 0x7fff;
	PathQuery.InputValue.PathRecordValue.PathRecord.Reversible = 1;
	PathQuery.InputValue.PathRecordValue.PathRecord.NumbPath = PATHRECORD_NUMBPATH;

	dump_path_record((IB_PATH_RECORD_NO*)&PathQuery.InputValue.PathRecordValue.PathRecord,0);
	
	hdl = dsc_get_oib_session(src_port->gid.global.interface_id);
	if (!hdl) 
		return FUNAVAILABLE;

	rval = oib_query_sa(hdl, &PathQuery, &pQueryResults);

	if (rval == FSUCCESS) {
		rval = dsc_check_query_results(pQueryResults);
		if (rval == FSUCCESS) {
			if (pQueryResults->MadStatus == MAD_STATUS_BUSY) {
				rval = FBUSY;
			} else {
				rval = dsc_process_path_records_query_results(src_port, (PPATH_RESULTS)pQueryResults->QueryResult);
			}
		}
	}

	if (rval == FNOT_FOUND) {
		// Not found is acceptable here, because we may be querying for a 
		// vfabric the destination isn't a member of.
		rval = FSUCCESS;
	} else if (rval != FSUCCESS) {
		_DBG_ERROR("Path query failed! Error code = %d (%d)\n", 
					rval, (pQueryResults)?pQueryResults->MadStatus:0);
	}

	if ((pQueryResults != NULL) && (pQueryResults->QueryResult != NULL)) {
		oib_free_query_result_buffer(pQueryResults);
	}

	_DBG_FUNC_EXIT;

	return rval;
}

static FSTATUS dsc_process_dst_ports_query_results(dsc_subnet_t *subnet, PNODE_RECORD_RESULTS pNodeRecordResults)
{
	FSTATUS rval = FSUCCESS;
	unsigned i;

	_DBG_FUNC_ENTRY;

	if (pNodeRecordResults->NumNodeRecords == 0) {
		rval = FNOT_FOUND;
		goto exit;
	}

	for (i = 0; i < pNodeRecordResults->NumNodeRecords; i++) {
		union ibv_gid dst_port_gid;

		dst_port_gid.global.subnet_prefix = subnet->sm_gid.global.subnet_prefix;
		dst_port_gid.global.interface_id = htonll(pNodeRecordResults->NodeRecords[i].NodeInfoData.PortGUID);

		rval = dsc_add_dst_port(&dst_port_gid,
								&subnet->sm_gid,
								pNodeRecordResults->NodeRecords[i].NodeInfoData.NodeType,
								(char *)pNodeRecordResults->NodeRecords[i].NodeDescData.NodeString);
		if (rval != FSUCCESS) {
			_DBG_WARN("Failed to add destination port 0x%Lx\n",
					pNodeRecordResults->NodeRecords[i].NodeInfoData.PortGUID);
			break;
		}
	}

exit:
	_DBG_FUNC_EXIT;
	return rval;
}

FSTATUS dsc_query_dst_ports(dsc_subnet_t *subnet)
{
	FSTATUS rval = FNOT_FOUND;
	LIST_ITEM *item;
	QUERY NodeQuery;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	struct oib_port *hdl;

	_DBG_FUNC_ENTRY;

	NodeQuery.InputType = InputTypeNoInput;
	NodeQuery.OutputType = OutputTypeNodeRecord;


	for_each (&subnet->src_port_list, item) {
		dsc_src_port_t *src_port = QListObj(item);

		hdl = dsc_get_oib_session(src_port->gid.global.interface_id);
		if (!hdl) 
			continue;

		if ((rval = oib_query_sa(hdl, &NodeQuery, &pQueryResults)) == FSUCCESS) {
			break;
		}

		if ((pQueryResults != NULL) && (pQueryResults->QueryResult != NULL)) {
			oib_free_query_result_buffer(pQueryResults);
			pQueryResults = NULL;
		}
	}

	if (rval == FSUCCESS) {
		rval = dsc_check_query_results(pQueryResults);
		if (rval == FSUCCESS) {
			if (pQueryResults->MadStatus == MAD_STATUS_BUSY) {
				rval = FBUSY;
			} else {
				rval = dsc_process_dst_ports_query_results(subnet, (PNODE_RECORD_RESULTS)pQueryResults->QueryResult);
			}
		}

		if ((pQueryResults != NULL) && (pQueryResults->QueryResult != NULL)) {
			oib_free_query_result_buffer(pQueryResults);
		}
	}

	_DBG_FUNC_EXIT;

	return rval;
}

static FSTATUS dsc_add_service_id_range_to_virtual_fabric(dsc_subnet_t *subnet, dsc_service_id_range_t *service_id_range, uint8_t *vfName)
{
	dsc_virtual_fabric_t *virtual_fabric = dsc_find_virtual_fabric(vfName, subnet);

	_DBG_FUNC_ENTRY;

	if (virtual_fabric) {
		FSTATUS rval = dsc_add_service_id_record(virtual_fabric, service_id_range);
		if (rval == FSUCCESS) {
			if (ntohll(service_id_range->upper_service_id) == 0) {
				_DBG_INFO("Added service id 0x%"PRIx64" to virtual fabric %s.\n",
						  ntohll(service_id_range->lower_service_id),
						  vfName);
			} else {
				_DBG_INFO("Added service id range 0x%"PRIx64"..0x%"PRIx64" to virtual fabric %s.\n",
						  ntohll(service_id_range->lower_service_id),
						  ntohll(service_id_range->upper_service_id),
						  vfName);
			}
		} else if (rval == FDUPLICATE) {
			rval = FSUCCESS;
		} else {
			_DBG_ERROR("Unable to add service id record to virtual fabric %s.\n", vfName);
		}

		_DBG_FUNC_EXIT;
		return rval;
	}

	_DBG_FUNC_EXIT;
	return FNOT_FOUND;
}

static FSTATUS dsc_process_service_id_range_and_virtual_fabric(dsc_subnet_t *subnet,
															   dsc_service_id_range_t *service_id_range,
															   STL_VFINFO_RECORD *vf_info_record)
{
	LIST_ITEM *src_port_item;

	_DBG_FUNC_ENTRY;

	/* Does it already exist? Just add the service id record */
	if (dsc_add_service_id_range_to_virtual_fabric(subnet,
												   service_id_range,
												   vf_info_record->vfName) == FSUCCESS) {
		_DBG_FUNC_EXIT;
		return FSUCCESS;
	}

	/* Find matching PKEY */
	for_each (&subnet->src_port_list, src_port_item) {
		FSTATUS rval = FNOT_FOUND;
		LIST_ITEM *pkey_item;
		dsc_src_port_t *src_port = QListObj(src_port_item);

		for_each (&src_port->pkey_list, pkey_item) {
			dsc_pkey_t *pkey = QListObj(pkey_item);
			// Mask off the high bit (fields are in network byte order....)
			if ((pkey->pkey & 0xff7f) == (vf_info_record->pKey & 0xff7f)) {
				rval = dsc_add_virtual_fabric(subnet, vf_info_record);
				if (rval == FSUCCESS) {
					rval = dsc_add_service_id_range_to_virtual_fabric(subnet,
																	  service_id_range,
																	  vf_info_record->vfName);
				}
				_DBG_FUNC_EXIT;
				return rval;
			}
		}
	}

	if (! service_id_range->warned) {
		service_id_range->warned=1;
		if (ntohll(service_id_range->upper_service_id) == 0) {
			_DBG_WARN("Sid 0x%"PRIx64" does not match any virtual fabric that this node is a member of.\n",
					htonll(service_id_range->lower_service_id));
		} else {
			_DBG_WARN("Sid range 0x%"PRIx64"-0x%"PRIx64" does not match any virtual fabric that this node is a member of.\n",
				  htonll(service_id_range->lower_service_id),
					htonll(service_id_range->upper_service_id));
		}
	}

	_DBG_FUNC_EXIT;
	return FNOT_FOUND;
}

static FSTATUS dsc_process_vfinfo_record_query_results(dsc_subnet_t *subnet,
													   dsc_service_id_range_t *service_id_range,
													   PSTL_VFINFO_RECORD_RESULTS pVFInfoRecordResults)
{
	FSTATUS rval = FNOT_FOUND;
	unsigned i;
	unsigned result_index;
	uint16_t vf_index;

	_DBG_FUNC_ENTRY;

	if (pVFInfoRecordResults->NumVfInfoRecords == 0) {
		_DBG_FUNC_EXIT;
		return rval;
	}

	if (pVFInfoRecordResults->NumVfInfoRecords == 1) {
		/* Swap the vfinfo record back to network order */
		BSWAP_STL_VFINFO_RECORD(&pVFInfoRecordResults->VfInfoRecords[0]);
		if (DefaultFabric == none) {
			/* Mask off the high bit (fields in network byte order...) */
			if ((pVFInfoRecordResults->VfInfoRecords[0].pKey & 0xff7f) == 0xff7f && !service_id_range->warned) {
				service_id_range->warned=1;
				if (ntoh64(service_id_range->upper_service_id) == 0) {
					_DBG_WARN("Found service id of 0x%016"PRIx64" in default/admin virtual fabric only. "
							  "However it is not configured for use. "
							  "Check your configuration. ",
							  ntoh64(service_id_range->lower_service_id));
				} else {
					_DBG_WARN("Found service id range of 0x%016"PRIx64"..0x%016"PRIx64" in default/admin virtual fabric only. "
							  "However it is not configured for use. "
							  "Check your configuration. ",
							  ntoh64(service_id_range->lower_service_id),
							  ntoh64(service_id_range->upper_service_id));
				}
			}
			_DBG_FUNC_EXIT;
			return rval;
		}

		rval = dsc_process_service_id_range_and_virtual_fabric(subnet,
															   service_id_range,
															   &pVFInfoRecordResults->VfInfoRecords[0]);
		_DBG_FUNC_EXIT;
		return rval;
	}

	if (ntoh64(service_id_range->upper_service_id) == 0 && !service_id_range->warned) {
		service_id_range->warned=1;
		_DBG_WARN("Found %d virtual fabrics with a service id of 0x%016"PRIx64". "
				  "Check your configuration. "
				  "Only using first non-default virtual fabric found.\n",
				  pVFInfoRecordResults->NumVfInfoRecords,
				  ntoh64(service_id_range->lower_service_id));
	} else if (!service_id_range->warned) {
		service_id_range->warned=1;
		_DBG_WARN("Found %d virtual fabrics with a service id range of 0x%016"PRIx64"..0x%016"PRIx64". "
				  "Check your configuration. "
				  "Only using first non-default virtual fabric found.\n",
				  pVFInfoRecordResults->NumVfInfoRecords,
				  ntoh64(service_id_range->lower_service_id),
				  ntoh64(service_id_range->upper_service_id));
	}

	for (i = 0, result_index = pVFInfoRecordResults->NumVfInfoRecords, vf_index = 0xffff;
		 i < pVFInfoRecordResults->NumVfInfoRecords;
		 i++) {
		/* Swap the vfinfo record back to network order */
		BSWAP_STL_VFINFO_RECORD(&pVFInfoRecordResults->VfInfoRecords[i]);
		/* Mask off the high bit (fields are in network byte order....) */
		if ((pVFInfoRecordResults->VfInfoRecords[i].pKey & 0xff7f) == 0xff7f) {
			if (ntoh64(service_id_range->upper_service_id) == 0) {
				_DBG_INFO("Ignoring sid 0x%016"PRIx64" on default virtual fabric %s.\n",
						  ntoh64(service_id_range->lower_service_id),
						  pVFInfoRecordResults->VfInfoRecords[i].vfName);
			} else {
				_DBG_INFO("Ignoring sid range 0x%016"PRIx64"..0x%016"PRIx64" on default virtual fabric %s.\n",
						  ntoh64(service_id_range->lower_service_id),
						  ntoh64(service_id_range->upper_service_id),
						  pVFInfoRecordResults->VfInfoRecords[i].vfName);
			}
			continue;
		}

		if (vf_index > pVFInfoRecordResults->VfInfoRecords[i].vfIndex) {
			vf_index = pVFInfoRecordResults->VfInfoRecords[i].vfIndex;
			result_index = i;
		}
	}
	
	if (result_index < pVFInfoRecordResults->NumVfInfoRecords) {
		rval = dsc_process_service_id_range_and_virtual_fabric(subnet,
															   service_id_range,
															   &pVFInfoRecordResults->VfInfoRecords[result_index]);
		_DBG_FUNC_EXIT;
		return rval;
	}

	if (ntoh64(service_id_range->upper_service_id) == 0) {
		_DBG_ERROR("Internal error for sid 0x%016"PRIx64".\n",
				   ntoh64(service_id_range->lower_service_id));
	} else {
		_DBG_ERROR("Internal error for sid range 0x%016"PRIx64"..0x%016"PRIx64".\n",
				   ntoh64(service_id_range->lower_service_id),
				   ntoh64(service_id_range->upper_service_id));
	}

	_DBG_FUNC_EXIT;
	return FNOT_FOUND;
}

FSTATUS dsc_query_vfinfo_records(dsc_subnet_t *subnet, dsc_service_id_range_t *service_id_range)
{
	FSTATUS rval = FNOT_FOUND;
	FSTATUS status = FNOT_FOUND;
	LIST_ITEM *item;
	QUERY VFInfoQuery;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	struct oib_port *hdl;

	_DBG_FUNC_ENTRY;

	VFInfoQuery.InputType = InputTypeServiceId;
	VFInfoQuery.InputValue.ServiceId = ntoh64(service_id_range->lower_service_id);
	VFInfoQuery.OutputType = OutputTypeStlVfInfoRecord;

	for_each (&subnet->src_port_list, item) {
		dsc_src_port_t *src_port = QListObj(item);

		if (src_port->state != IBV_PORT_ACTIVE) {
			_DBG_INFO("Skipping inactive port (0x%016"PRIx64")\n",
					  ntoh64(src_port->gid.global.interface_id));
			continue;
		} else {
			_DBG_INFO("Checking VFabs on port (0x%016"PRIx64")\n",
					  ntoh64(src_port->gid.global.interface_id));
		}
		
		hdl = dsc_get_oib_session(src_port->gid.global.interface_id);
		if (!hdl) 
			continue;

		if ((rval = oib_query_sa(hdl, &VFInfoQuery, &pQueryResults)) == FSUCCESS) {

			rval = dsc_check_query_results(pQueryResults);
			if (rval == FSUCCESS) {
				if (pQueryResults->MadStatus == MAD_STATUS_BUSY) {
					rval = FBUSY;
				} else {
					rval = dsc_process_vfinfo_record_query_results(subnet,
																service_id_range,
																(PSTL_VFINFO_RECORD_RESULTS)pQueryResults->QueryResult);
					service_id_range->warned = 0;
				}
			}
			if (rval == FSUCCESS) status = FSUCCESS;
		}

		if ((pQueryResults != NULL) && (pQueryResults->QueryResult != NULL)) {
			oib_free_query_result_buffer(pQueryResults);
			pQueryResults = NULL;
		}
	}

	if (status == FNOT_FOUND && !service_id_range->warned) {
		service_id_range->warned=1;
		if (ntohll(service_id_range->upper_service_id) == 0) {
		_DBG_WARN("Could not match SID 0x%016"PRIx64" to any virtual fabric visible on subnet 0x%016"PRIx64".\n",
				ntoh64(service_id_range->lower_service_id),
				ntoh64(subnet->sm_gid.global.subnet_prefix));
		} else {
			_DBG_WARN("Could not match SID range 0x%016"PRIx64" to any virtual fabric visible on subnet 0x%016"PRIx64".\n",
				ntoh64(service_id_range->lower_service_id),
				ntoh64(service_id_range->upper_service_id),
				ntoh64(subnet->sm_gid.global.subnet_prefix));
		}				
	}

	_DBG_FUNC_EXIT;

	return rval;
}

FSTATUS dsc_query_node_record(union ibv_gid *src_gid, uint64 *sm_port_guid)
{
	FSTATUS rval;
	QUERY NodeQuery;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	PSTL_NODE_RECORD_RESULTS pNodeRecRes;
	umad_port_t *local_port;
	struct oib_port *hdl;

	local_port = dsc_get_local_port(src_gid);
	if (!local_port) {
		_DBG_ERROR("Local port not found\n");
		return FERROR;
	}

	NodeQuery.InputType = InputTypeLid;              
	NodeQuery.InputValue.Lid = local_port->sm_lid;
	NodeQuery.OutputType = OutputTypeStlNodeRecord;     

	hdl = dsc_get_oib_session(local_port->port_guid);
	if (!hdl) 
		rval = FUNAVAILABLE;
	 else 
		rval = oib_query_sa(hdl, &NodeQuery, &pQueryResults);

	if (rval == FSUCCESS)                                                                                                          
	{                                                                                                                              
		rval = dsc_check_query_results(pQueryResults);                                                                         
		if (rval == FSUCCESS)                                                                                                  
		{                                                                                                                      
			pNodeRecRes = (PSTL_NODE_RECORD_RESULTS)pQueryResults->QueryResult;
			if (pQueryResults->MadStatus == MAD_STATUS_BUSY) {                                                             
				rval = FBUSY;                                                                                          
			} else {                                                                                                       
				//rval = dsc_process_dst_ports_query_result                                                            
				if (pNodeRecRes->NumNodeRecords == 0) {                                                              
					rval = FNOT_FOUND;                                                                             
				} else {                                                                                               
					*sm_port_guid = hton64(pNodeRecRes->NodeRecords[0].NodeInfo.PortGUID);
				}                                                                                                      
																       
			}                                                                                                              
																       
			if (pQueryResults->QueryResult != NULL) {                                         
				oib_free_query_result_buffer(pQueryResults);                                                        
			}                                                                                                              
		}                                                                                                                      
	} 
	dsc_put_local_port(local_port);                                                                                                                             
	return rval;
}

FSTATUS dsc_query_dst_port(union ibv_gid *dst_gid, union ibv_gid *sm_gid, NODE_TYPE *node_type, char *node_desc)
{
	FSTATUS rval = FNOT_FOUND;
	LIST_ITEM *item;
	QUERY NodeQuery;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	dsc_subnet_t *subnet;
	PNODE_RECORD_RESULTS pNodeRecordResults;
	struct oib_port *hdl;

	_DBG_FUNC_ENTRY;

	NodeQuery.InputType = InputTypePortGuid;
	NodeQuery.InputValue.Guid = ntohll(dst_gid->global.interface_id);
	NodeQuery.OutputType = OutputTypeNodeRecord;

	subnet = dsc_find_subnet(sm_gid);

	if (subnet != NULL) {
		for_each (&subnet->src_port_list, item) {
			dsc_src_port_t *src_port = QListObj(item);
	
			hdl = dsc_get_oib_session(src_port->gid.global.interface_id);
			if (!hdl) 
				continue;

			if ((rval = oib_query_sa(hdl, &NodeQuery, &pQueryResults)) == FSUCCESS) {
				break;
			}
			if ((pQueryResults != NULL) && (pQueryResults->QueryResult != NULL)) {
				oib_free_query_result_buffer(pQueryResults);
				pQueryResults = NULL;
			}
		}

		if (rval == FSUCCESS) {
			rval = dsc_check_query_results(pQueryResults);
			if (rval == FSUCCESS) {
				if (pQueryResults->MadStatus == MAD_STATUS_BUSY) {
					rval = FBUSY;
					goto exit;
				}

				pNodeRecordResults = (PNODE_RECORD_RESULTS)pQueryResults->QueryResult;

				if (pNodeRecordResults->NumNodeRecords == 0) {
					rval = FNOT_FOUND;
					goto exit;
				}

				if (pNodeRecordResults->NumNodeRecords != 1) {
					rval = FERROR;
					goto exit;
				}

				*node_type = pNodeRecordResults->NodeRecords[0].NodeInfoData.NodeType;
				memcpy(node_desc, pNodeRecordResults->NodeRecords[0].NodeDescData.NodeString,
					   NODE_DESCRIPTION_ARRAY_SIZE);
			}
		}
	}

exit:
	if ((pQueryResults != NULL) && (pQueryResults->QueryResult != NULL)) {
		oib_free_query_result_buffer(pQueryResults);
	}

	_DBG_FUNC_EXIT;

	return rval;
}
