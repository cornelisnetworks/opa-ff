/* BEGIN_ICS_COPYRIGHT4 ****************************************

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

** END_ICS_COPYRIGHT4   ****************************************/
/*!

  \file dsap_query.c
 
  \brief Various functions for querying SA.
*/

/* work around conflicting names */
#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include "iba/ibt.h"
#include "iba/stl_sd.h"
#include "iba/public/ievent.h"
#include "iba/public/ilist.h"
#include "opasadb_debug.h"
#include "dsap.h"
#include "opamgt_sa_priv.h"

#include <netinet/in.h>

#define PATHRECORD_NUMBPATH 127 /* max to request */

static FSTATUS dsap_check_query_results(PQUERY_RESULT_VALUES res)
{
	acm_log(2, "\n");

	if (res == NULL) {
		acm_log(1, "NULL query results returned!\n");
		return FERROR;
	}

	if (res->Status != FSUCCESS) {
		acm_log(1, "SA Query Returned Failed status = %s(%d)\n",
			FSTATUS_MSG(res->Status),
			res->Status);
		return res->Status;
	}

	if (res->ResultDataSize == 0) {
		acm_log(1, "SA Query Returned no data!\n");
		return FNOT_FOUND;
	}

	if (res->QueryResult == NULL) {
		acm_log(1, "SA Query Returned no result data!\n");
		return FNOT_FOUND;
	}

	return FSUCCESS;
}

static inline void dump_path_record(IB_PATH_RECORD_NO *path, int byte_order)
{
#if defined(ICSDEBUG)
	char gid_str[INET6_ADDRSTRLEN];
	char gid_str2[INET6_ADDRSTRLEN];

	if (byte_order) {
		acm_log(2, "Path SGID(%s) SLID(0x%x) to \n\t\tDGID(%s) "
			   "DLID(0x%x) SID(0x%016" PRIx64 ") PKEY(0x%x)\n",
			  inet_ntop(AF_INET6, path->SGID.Raw, gid_str2,
				    sizeof gid_str2),
			  ntohs(path->SLID),
			  inet_ntop(AF_INET6, path->DGID.Raw, gid_str, 
				    sizeof gid_str),
			  ntohs(path->DLID),
			  ntoh64(path->ServiceID),
			  ntohs(path->P_Key));
	} else {
		acm_log(2, "Query SGID(%"PRIx64":%"PRIx64") SLID(0x%x) to \n\t\tDGID"
			   "(%"PRIx64":%"PRIx64") DLID(0x%x) SID(0x%016" PRIx64 ") "
			   "PKEY(0x%x)\n",
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


static FSTATUS dsap_process_path_records_query_results(
	dsap_src_port_t *src_port, PPATH_RESULTS res)
{
	FSTATUS rval = FSUCCESS;
	unsigned i;
	IB_PATH_RECORD_NO *path_record;

	acm_log(2, "\n");

	if (res->NumPathRecords == 0) {
		rval = FNOT_FOUND;
		goto exit;
	}

	for (i = 0, path_record = (IB_PATH_RECORD_NO*) res->PathRecords;
	     i < res->NumPathRecords; i++, path_record++) {
		dump_path_record(path_record, 1);
		rval = dsap_add_path_record(src_port, path_record);
		if (rval != FSUCCESS) 
			break;
	}

exit:
	return rval;
}


FSTATUS dsap_query_path_records(dsap_src_port_t *src_port,
				dsap_dst_port_t *dst_port,
				uint64_t sid, uint16_t pkey)
{
	FSTATUS rval = FNOT_FOUND;
	OMGT_QUERY query;
	PQUERY_RESULT_VALUES res = NULL;
	struct dsap_port *port;

	acm_log(2, "\n");

	/* While the mask prevents unused fields from being used in the query,
	   this will still help when trying to dump or debug paths. */
	memset(&query,0,sizeof(OMGT_QUERY));

	/* Get ALL Paths between ports using Local and Remote Port GIDs */
	query.InputType = InputTypePathRecord;
	query.OutputType = OutputTypePathRecordNetworkOrder;
	query.InputValue.IbPathRecord.PathRecord.ComponentMask =
		IB_PATH_RECORD_COMP_SERVICEID |
		IB_PATH_RECORD_COMP_DGID |
		IB_PATH_RECORD_COMP_SGID |
		IB_PATH_RECORD_COMP_PKEY |
		IB_PATH_RECORD_COMP_REVERSIBLE |
		IB_PATH_RECORD_COMP_NUMBPATH;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.DGID.Type.Global.SubnetPrefix =
		ntoh64(dst_port->gid.global.subnet_prefix);
	query.InputValue.IbPathRecord.PathRecord.PathRecord.DGID.Type.Global.InterfaceID  =
		ntoh64(dst_port->gid.global.interface_id);
	query.InputValue.IbPathRecord.PathRecord.PathRecord.SGID.Type.Global.SubnetPrefix =
		ntoh64(src_port->gid.global.subnet_prefix);
	query.InputValue.IbPathRecord.PathRecord.PathRecord.SGID.Type.Global.InterfaceID  =
		ntoh64(src_port->gid.global.interface_id);
	query.InputValue.IbPathRecord.PathRecord.PathRecord.ServiceID = ntoh64(sid);
	query.InputValue.IbPathRecord.PathRecord.PathRecord.P_Key = ntohs(pkey ) & 0x7fff;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.Reversible = 1;
	query.InputValue.IbPathRecord.PathRecord.PathRecord.NumbPath = PATHRECORD_NUMBPATH;

	dump_path_record(
	   (IB_PATH_RECORD_NO *) &query.InputValue.IbPathRecord.PathRecord.PathRecord,
	   0);
	
	port = dsap_lock_prov_port(src_port);
	if (!port) 
		return rval;
	if (!port->omgt_handle) {
		goto query_exit;
	}

	rval = omgt_query_sa(port->omgt_handle, &query, &res);
	if (rval == FSUCCESS) {
		rval = dsap_check_query_results(res);
		if (rval == FSUCCESS) {
			if (res->MadStatus == MAD_STATUS_BUSY) {
				rval = FBUSY;
			} else {
				rval = dsap_process_path_records_query_results(
				   src_port, 
				   (PPATH_RESULTS)res->QueryResult);
			}
		}
	}

	if (rval == FNOT_FOUND) {
		/* Not found is acceptable here, because we may be querying
		   for a vfab the destination isn't a member of. */
		rval = FSUCCESS;
	} else if (rval != FSUCCESS) {
		acm_log(0, "Path query failed! Error code = %d (%d)\n", 
			rval, (res) ? res->MadStatus : 0);
	}

	if ((res != NULL) && (res->QueryResult != NULL))
		omgt_free_query_result_buffer(res);

query_exit:
	dsap_release_prov_port(port);

	return rval;
}

static FSTATUS
dsap_process_dst_ports_query_results(dsap_subnet_t *subnet,
				    PNODE_RECORD_RESULTS res)
{
	FSTATUS rval = FSUCCESS;
	unsigned i;
	union ibv_gid dst_port_gid;

	acm_log(2, "\n");

	if (res->NumNodeRecords == 0) {
		rval = FNOT_FOUND;
		goto exit;
	}

	for (i = 0; i < res->NumNodeRecords; i++) {
		dst_port_gid.global.subnet_prefix = subnet->subnet_prefix;
		dst_port_gid.global.interface_id =
			hton64(res->NodeRecords[i].NodeInfoData.PortGUID);

		rval = dsap_add_dst_port(
		   &dst_port_gid, res->NodeRecords[i].NodeInfoData.NodeType,
		   (char *)res->NodeRecords[i].NodeDescData.NodeString);
		if (rval != FSUCCESS) {
			acm_log(0, "Failed to add destination port 0x%"PRIx64"\n",
				res->NodeRecords[i].NodeInfoData.PortGUID);
			break;
		}
	}

exit:
	return rval;
}


FSTATUS dsap_query_dst_ports(dsap_subnet_t *subnet)
{
	FSTATUS rval = FNOT_FOUND;
	LIST_ITEM *item;
	OMGT_QUERY query;
	PQUERY_RESULT_VALUES res = NULL;
	dsap_src_port_t *src_port;
	struct dsap_port *port;

	acm_log(2, "\n");

	query.InputType = InputTypeNoInput;
	query.OutputType = OutputTypeNodeRecord;

	for_each (&subnet->src_port_list, item) {
		src_port = QListObj(item);

		port = dsap_lock_prov_port(src_port);
		if (!port) {
			acm_log(1, "src_port %s/%d not available\n",
				src_port->hfi_name, src_port->port_num);
			continue;
		}
		if (!port->omgt_handle) {
			acm_log(1, "no opamgt port handle for port %s/%d\n",
				port->dev->device->verbs->device->name,
				port->port->port_num);
			goto next_src_port;
		}

		if ((rval = omgt_query_sa(port->omgt_handle, &query, &res)) ==
		    FSUCCESS) {
			dsap_release_prov_port(port);
			break;
		}

		if ((res != NULL) && (res->QueryResult != NULL)) {
			omgt_free_query_result_buffer(res);
			res = NULL;
		}

	next_src_port:
		dsap_release_prov_port(port);
	}

	if (rval != FSUCCESS || res == NULL) {
		acm_log(1, "dst_port query failed\n");
		return rval;
	}

	rval = dsap_check_query_results(res);
	if (rval == FSUCCESS) {
		if (res->MadStatus == MAD_STATUS_BUSY) {
			rval = FBUSY;
		} else {
			rval = dsap_process_dst_ports_query_results(
			   subnet, 
			   (PNODE_RECORD_RESULTS)res->QueryResult);
		}
	}

	if ((res != NULL) && (res->QueryResult != NULL))
		omgt_free_query_result_buffer(res);

	return rval;
}

static FSTATUS 
dsap_add_service_id_range_to_virtual_fabric(
	dsap_subnet_t *subnet, 
	dsap_service_id_range_t *sid_range, 
	uint8_t *vf_name)
{
	FSTATUS rval;
	dsap_virtual_fabric_t *vfab = 
		dsap_find_virtual_fabric(vf_name, subnet);

	acm_log(2, "\n");

	if (vfab) {
		rval = dsap_add_service_id_record(vfab, sid_range);
		if (rval == FSUCCESS) {
			if (sid_range->upper_service_id == 0) {
				acm_log(1, "Added sid 0x%"PRIx64" to vfab "
					   "%s\n",
					ntoh64(sid_range->lower_service_id),
					vf_name);
			} else {
				acm_log(1, "Added sid range 0x%"PRIx64"..0x%"
					   PRIx64" to vfab %s.\n",
					ntoh64(sid_range->lower_service_id),
					ntoh64(sid_range->upper_service_id),
					vf_name);
			}
		} else if (rval == FDUPLICATE) {
			rval = FSUCCESS;
		} else {
			acm_log(0, "Unable to add sid record to vfab %s.\n",
				vf_name);
		}

		return rval;
	}

	return FNOT_FOUND;
}

static FSTATUS 
dsap_process_service_id_range_and_virtual_fabric(
	dsap_subnet_t *subnet,
	dsap_service_id_range_t *sid_range,
	STL_VFINFO_RECORD *rec)
{
	LIST_ITEM *src_port_item;
	FSTATUS rval;
	LIST_ITEM *pkey_item;
	dsap_src_port_t *src_port;
	dsap_pkey_t *pkey;

	acm_log(2, "\n");

	/* Does it already exist? Just add the service id record */
	if (dsap_add_service_id_range_to_virtual_fabric(
		subnet, sid_range, rec->vfName) == FSUCCESS)
		return FSUCCESS;

	/* Find matching PKEY */
	for_each (&subnet->src_port_list, src_port_item) {
		src_port = QListObj(src_port_item);

		for_each (&src_port->pkey_list, pkey_item) {
			pkey = QListObj(pkey_item);
			/* Mask off the high bit (fields are in network byte 
			   order....) */
			if ((pkey->pkey & 0xff7f) != (rec->pKey & 0xff7f))
				continue;

			rval = dsap_add_virtual_fabric(subnet, rec);
			if (rval != FSUCCESS)
				return rval;

			return dsap_add_service_id_range_to_virtual_fabric(
				   subnet, sid_range, rec->vfName);
		}
	}

	if (!sid_range->warned) {
		sid_range->warned=1;
		if (sid_range->upper_service_id == 0) {
			acm_log(1, "Sid 0x%"PRIx64" does not match any vfab "
				   "that this node is a member of.\n",
				hton64(sid_range->lower_service_id));
		} else {
			acm_log(1, "Sid range 0x%"PRIx64"-0x%"PRIx64" does "
				   "not match any vfab that this node is a "
				   "member of.\n",
				hton64(sid_range->lower_service_id),
				hton64(sid_range->upper_service_id));
		}
	}

	return FNOT_FOUND;
}

static FSTATUS dsap_process_vfinfo_record_query_results(
	dsap_subnet_t *subnet, dsap_service_id_range_t *sid_range,
	PSTL_VFINFO_RECORD_RESULTS res)
{
	FSTATUS rval = FNOT_FOUND;
	unsigned i;
	unsigned result_index;
	uint16_t vf_index;

	acm_log(2, "\n");

	if (res->NumVfInfoRecords == 0)
		return rval;

	if (res->NumVfInfoRecords == 1) {
		/* Swap the vfinfo record back to network order */
		BSWAP_STL_VFINFO_RECORD(&res->VfInfoRecords[0]);
		if (dsap_default_fabric != DSAP_DEF_FAB_ACT_NONE)
			goto process_res;

		/* Mask off the high bit (fields in network byte
		   order...) */
		if ((res->VfInfoRecords[0].pKey & 0xff7f) == 0xff7f &&
		    !sid_range->warned) {
			sid_range->warned = 1;
			if (sid_range->upper_service_id == 0) {
				acm_log(1, "Found sid of 0x%016"PRIx64" in "
					   "default/admin vfab only. However, "
					   "it is not configured for use. "
					   "Check your configuration.\n",
					ntoh64(sid_range->lower_service_id));
			} else {
				acm_log(1, "Found sid range of 0x%016"PRIx64
					   "..0x%016"PRIx64" in default/admin"
					   " virtual fabric only. However, it "
					   "is not configured for use. Check "
					   "your configuration.\n",
					ntoh64(sid_range->lower_service_id),
					ntoh64(sid_range->upper_service_id));
			}
		}
		return rval;

	process_res:
		return dsap_process_service_id_range_and_virtual_fabric(
			subnet, sid_range, &res->VfInfoRecords[0]);
	}

	if (sid_range->upper_service_id == 0 && !sid_range->warned) {
		sid_range->warned=1;
		acm_log(1, "Found %d vfab with a sid of 0x%016"PRIx64". "
			   "Check your configuration. Only using first "
			   "non-default vfab found.\n",
			res->NumVfInfoRecords,
			ntoh64(sid_range->lower_service_id));
	} else if (!sid_range->warned) {
		sid_range->warned=1;
		acm_log(1, "Found %d vfab with a sid range of 0x%016"PRIx64".."
			   "0x%016"PRIx64". Check your configuration. Only "
			   "using first non-default vfab found.\n",
			res->NumVfInfoRecords,
			ntoh64(sid_range->lower_service_id),
			ntoh64(sid_range->upper_service_id));
	}

	for (i = 0, result_index = res->NumVfInfoRecords, vf_index = 0xffff;
	     i < res->NumVfInfoRecords; i++) {
		/* Swap the vfinfo record back to network order */
		BSWAP_STL_VFINFO_RECORD(&res->VfInfoRecords[i]);
		/* 
		 * Mask off the high bit (fields are in network byte order...)
		 */
		if ((res->VfInfoRecords[i].pKey & 0xff7f) == 0xff7f) {
			if (sid_range->upper_service_id == 0) {
				acm_log(1, "Ignoring sid 0x%016"PRIx64" on "
					   "default vfab %s.\n",
					ntoh64(sid_range->lower_service_id),
					res->VfInfoRecords[i].vfName);
			} else {
				acm_log(1, "Ignoring sid range 0x%016"PRIx64
					   "..0x%016"PRIx64" on default vfab "
					   "%s.\n",
					ntoh64(sid_range->lower_service_id),
					ntoh64(sid_range->upper_service_id),
					res->VfInfoRecords[i].vfName);
			}
			continue;
		}

		if (vf_index > res->VfInfoRecords[i].vfIndex) {
			vf_index = res->VfInfoRecords[i].vfIndex;
			result_index = i;
		}
	}
	
	if (result_index < res->NumVfInfoRecords) {
		rval = dsap_process_service_id_range_and_virtual_fabric(
		   subnet, sid_range, &res->VfInfoRecords[result_index]);

		return rval;
	}

	if (sid_range->upper_service_id == 0) {
		acm_log(0, "Internal error for sid 0x%016"PRIx64".\n",
			ntoh64(sid_range->lower_service_id));
	} else {
		acm_log(0, "Internal error for sid range 0x%016"PRIx64
			    "..0x%016"PRIx64".\n",
			ntoh64(sid_range->lower_service_id),
			ntoh64(sid_range->upper_service_id));
	}

	return FNOT_FOUND;
}

FSTATUS dsap_query_vfinfo_records(dsap_subnet_t *subnet,
				  dsap_service_id_range_t *sid_range)
{
	FSTATUS rval = FNOT_FOUND;
	FSTATUS status = FNOT_FOUND;
	LIST_ITEM *item;
	OMGT_QUERY query;
	PQUERY_RESULT_VALUES res = NULL;
	dsap_src_port_t *src_port;
	struct dsap_port *port;

	acm_log(2, "\n");

	query.InputType = InputTypeServiceId;
	query.InputValue.VfInfoRecord.ServiceId = ntoh64(sid_range->lower_service_id);
	query.OutputType = OutputTypeStlVfInfoRecord;

	for_each (&subnet->src_port_list, item) {
		src_port = QListObj(item);
		port = dsap_lock_prov_port(src_port);
		if (!port) 
			continue;
		if (!port->omgt_handle) 
			goto next_src_port;

		rval = omgt_query_sa(port->omgt_handle, &query, &res);
		if (rval != FSUCCESS) 
			goto next_src_port;
		
		rval = dsap_check_query_results(res);
		if (rval != FSUCCESS) 
			goto free_result;

		if (res->MadStatus == MAD_STATUS_BUSY) {
			rval = FBUSY;
		} else {
			rval = dsap_process_vfinfo_record_query_results(
			   subnet, sid_range,
			   (PSTL_VFINFO_RECORD_RESULTS)res->QueryResult);
			sid_range->warned = 0;
		}

		if (rval == FSUCCESS) 
			status = FSUCCESS;

	free_result:
		if ((res != NULL) && (res->QueryResult != NULL)) {
			omgt_free_query_result_buffer(res);
			res = NULL;
		}
	next_src_port:
		dsap_release_prov_port(port);
	}

	if (status == FNOT_FOUND && !sid_range->warned) {
		sid_range->warned=1;
		if (sid_range->upper_service_id == 0) {
			acm_log(1, "Could not match SID 0x%016"PRIx64" to any "
				   "vfab visible on subnet 0x%016"PRIx64".\n",
				ntoh64(sid_range->lower_service_id),
				ntoh64(subnet->subnet_prefix));
		} else {
			acm_log(1, "Could not match SID range 0x%016"PRIx64" - 0x%016"PRIx64
				   " to any vfab visible on subnet 0x%016"
				   PRIx64".\n",
				ntoh64(sid_range->lower_service_id),
				ntoh64(sid_range->upper_service_id),
				ntoh64(subnet->subnet_prefix));
		}
	}

	return rval;
}

FSTATUS dsap_query_dst_port(union ibv_gid *dst_gid, NODE_TYPE *node_type,
			    char *node_desc)
{
	FSTATUS rval = FNOT_FOUND;
	LIST_ITEM *item;
	OMGT_QUERY query;
	PQUERY_RESULT_VALUES res = NULL;
	dsap_subnet_t *subnet;
	PNODE_RECORD_RESULTS rec;
	dsap_src_port_t *src_port;
	struct dsap_port *port;

	acm_log(2, "\n");

	query.InputType = InputTypePortGuid;
	query.InputValue.IbNodeRecord.PortGUID = ntoh64(dst_gid->global.interface_id);
	query.OutputType = OutputTypeNodeRecord;
	subnet = dsap_find_subnet((uint64_t *)&dst_gid->global.subnet_prefix);

	if (subnet == NULL) 
		return rval;

	for_each (&subnet->src_port_list, item) {
		src_port = QListObj(item);
	
		port = dsap_lock_prov_port(src_port);
		if (!port) 
			continue;
		if (!port->omgt_handle) 
			goto next_src_port;

		if ((rval = omgt_query_sa(port->omgt_handle, &query,
					 &res)) == FSUCCESS) {
			dsap_release_prov_port(port);
			break;
		}
		if ((res != NULL) && (res->QueryResult != NULL)) {
			omgt_free_query_result_buffer(res);
			res = NULL;
		}
	next_src_port:
		dsap_release_prov_port(port);
	}

	if (rval != FSUCCESS || res == NULL) 
		return rval;

	rval = dsap_check_query_results(res);
	if (rval != FSUCCESS) 
		goto exit;

	if (res->MadStatus == MAD_STATUS_BUSY) {
		rval = FBUSY;
		goto exit;
	}

	rec = (PNODE_RECORD_RESULTS)res->QueryResult;

	if (rec->NumNodeRecords == 0) {
		rval = FNOT_FOUND;
		goto exit;
	}

	if (rec->NumNodeRecords != 1) {
		rval = FERROR;
		goto exit;
	}
	*node_type = rec->NodeRecords[0].NodeInfoData.NodeType;
	memcpy(node_desc, 
	       rec->NodeRecords[0].NodeDescData.NodeString,
	       NODE_DESCRIPTION_ARRAY_SIZE);

exit:
	if ((res != NULL) && (res->QueryResult != NULL))
		omgt_free_query_result_buffer(res);

	return rval;
}

