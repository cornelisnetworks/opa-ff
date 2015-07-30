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

#include <stdio.h>
#include <stdlib.h>

/* work around conflicting names */
#include <infiniband/mad.h>
#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include <arpa/inet.h>
#include "opasadb_debug.h"
#include "dsc_module.h"
#include "dsc_hca.h"
#include "dsc_topology.h"

static QUICK_LIST subnet_list;

/* Command line parsing */
QUICK_LIST sid_range_args;
static int sid_range_args_init = 0;

/*
 * Valid formats are "SID" and "SID-SID" where "SID"
 * is a 64-bit hexadecimal string.
 */
int dsc_service_id_range_parser(char *str, void *ptr)
{
	uint64 lower, upper;
	dsc_service_id_record_t *record;
	int err = 0;
	char *k=0;
	
	_DBG_FUNC_ENTRY;

	if (!str || !ptr) {
		_DBG_ERROR("Bad arguments to sid parser.\n");
		err = EINVAL;
		goto exit;
	}

	if (sid_range_args_init == 0) {
		sid_range_args_init = QListInit(&sid_range_args);
		if (!sid_range_args_init) {
			_DBG_ERROR("Failed to initialize quick list.\n");
			err = ENOMEM;
			goto exit;
		}
	}

	record = malloc(sizeof(dsc_service_id_record_t));
	if (!record) {
		_DBG_ERROR("Failed to allocate memory.\n");
		err = ENOMEM;
		goto exit;
	}
	
	memset(record,0,sizeof(dsc_service_id_record_t));

	// Disabling ranged parameters for now.
	//k = strchr(str,'-');
	//if (k) *k=0;

	lower = strtoull(str,NULL,0);
	upper = (k)?strtoull(k+1, NULL,0):0;

	record->service_id_range.warned = 0;
	record->service_id_range.lower_service_id = htonll(lower);
	record->service_id_range.upper_service_id = htonll(upper);

	QListSetObj(&record->item, record);
	QListInsertTail(&sid_range_args, &record->item);

exit:
	_DBG_FUNC_EXIT;
	return err;
}

/*
 * Note that this function isn't re-entrant.
 */
char *dsc_service_id_range_printer(void *ptr)
{
	static char buffer[1024];
	int p = 0;
	LIST_ITEM *item;
	
	_DBG_FUNC_ENTRY;

	if (!sid_range_args_init) return "";
	for (item = QListHead(&sid_range_args);
		item != NULL && p<960; 
		item = QListNext(&sid_range_args, item)) {
		dsc_service_id_record_t *record = QListObj(item);
		if (record->service_id_range.upper_service_id) {
			p += sprintf(&buffer[p],"[0x%016"PRIx64"-0x%016"PRIx64"] ",
						ntohll(record->service_id_range.lower_service_id),
						ntohll(record->service_id_range.upper_service_id));
		} else {
			p +=  sprintf(&buffer[p],"0x%016"PRIx64" ",
						 ntohll(record->service_id_range.lower_service_id));
		}
	}

	_DBG_FUNC_EXIT;
	return buffer;
}

/* Comparison Callback Functions */

static boolean dsc_compare_pkey(LIST_ITEM *item, void *context)
{
	dsc_pkey_t *pkey_item = QListObj(item);
	uint16_t   *pkey = context;

	return (pkey_item->pkey == *pkey);
}

static boolean dsc_compare_src_port(LIST_ITEM *item, void *context)
{
	dsc_src_port_t *src_port = QListObj(item);
	union ibv_gid  *gid      = context;

	return (src_port->gid.global.interface_id == gid->global.interface_id);
}

static boolean dsc_compare_src_port_by_lid(LIST_ITEM *item, void *context)
{
	dsc_src_port_t *src_port = QListObj(item);
	unsigned  *lid           = context;
	unsigned  mask = ~(src_port->lmc); 

	return ((src_port->base_lid & mask )== (*lid & mask));
}

static boolean dsc_compare_path(LIST_ITEM *item, void *context)
{
	dsc_path_record_t *dsc_path = QListObj(item);
	IB_PATH_RECORD_NO *path     = context;
	uint16_t key_mask           = htons(0x7fff);

	/* Compare Incoming parameters */
	if (path->ServiceID && (path->ServiceID != dsc_path->path.ServiceID )) {
		return 0;
	}
	if (path->DGID.Type.Global.InterfaceID && 
		(path->DGID.Type.Global.InterfaceID != dsc_path->path.DGID.Type.Global.InterfaceID) ) {
		return 0;
	}
	if (path->DGID.Type.Global.SubnetPrefix && 
		(path->DGID.Type.Global.SubnetPrefix != dsc_path->path.DGID.Type.Global.SubnetPrefix) ) {
		return 0;
	}
	if (path->DLID && (path->DLID != dsc_path->path.DLID)) {
		return 0;
	}
	if ((path->P_Key & key_mask) && 
		(path->P_Key & key_mask) != (dsc_path->path.P_Key & key_mask) ) {
		return 0;
	}

	/* Found */
	return 1;
}

static boolean dsc_compare_dst_port(LIST_ITEM *item, void *context)
{
	dsc_dst_port_t *dst_port = QListObj(item);
	union ibv_gid  *gid      = context;

	return (dst_port->gid.global.interface_id == gid->global.interface_id);
}

static boolean dsc_compare_dst_port_by_name(LIST_ITEM *item, void *context)
{
	dsc_dst_port_t *dst_port = QListObj(item);
	char  *desc      = (char *)context;

	/* We are not doing the exact match here. As long as the user provided destination
	   is part of the node description, it will be fine for us. */
	return (0 == strncasecmp(dst_port->node_desc, desc, strlen(desc))); 
}

static boolean dsc_compare_service_id_record(LIST_ITEM *item, void *context)
{
	dsc_service_id_record_t *service_id_record = QListObj(item);
	dsc_service_id_range_t  *service_id_range  = context;

	return ((service_id_record->service_id_range.lower_service_id == service_id_range->lower_service_id) &&
			(service_id_record->service_id_range.upper_service_id == service_id_range->upper_service_id));
}

static boolean dsc_compare_virtual_fabric(LIST_ITEM *item, void *context)
{
	dsc_virtual_fabric_t *virtual_fabric = QListObj(item);
	uint8_t *vfName = context;

	return (strncmp((const char*)virtual_fabric->vfinfo_record.vfName,
					(const char*)vfName,
					sizeof(virtual_fabric->vfinfo_record.vfName)) == 0);
}

static boolean dsc_compare_subnet(LIST_ITEM *item, void *context)
{
	dsc_subnet_t *subnet = QListObj(item);
	union ibv_gid *sm_gid = context;

	return ((subnet->sm_gid.global.subnet_prefix == sm_gid->global.subnet_prefix) &&
			(subnet->sm_gid.global.interface_id == sm_gid->global.interface_id));
}

/* Search Utility Routines */

dsc_subnet_t* dsc_find_subnet(union ibv_gid *sm_gid)
{
	LIST_ITEM *item = QListFindItem(&subnet_list, dsc_compare_subnet, sm_gid);

	return item ? QListObj(item) : NULL;
}

dsc_src_port_t* dsc_find_src_port(union ibv_gid *gid)
{
	LIST_ITEM *subnet_item;

	for_each (&subnet_list, subnet_item) {
		dsc_subnet_t *subnet = QListObj(subnet_item);
		LIST_ITEM *src_port_item = QListFindItem(&subnet->src_port_list, dsc_compare_src_port, gid);
		if (src_port_item) {
			return QListObj(src_port_item);
		}
	}

	return NULL;
}

dsc_src_port_t* dsc_find_src_port_by_lid(unsigned *lid)
{
	LIST_ITEM *subnet_item;

	for_each (&subnet_list, subnet_item) {
		dsc_subnet_t *subnet = QListObj(subnet_item);
		LIST_ITEM *src_port_item = QListFindItem(&subnet->src_port_list, dsc_compare_src_port_by_lid, lid);
		if (src_port_item) {
			return QListObj(src_port_item);
		}
	}

	return NULL;
}

dsc_path_record_t * dsc_find_path_record(dsc_src_port_t *src_port, IB_PATH_RECORD_NO *path) 
{
	LIST_ITEM *path_item = QListFindFromHead(&src_port->path_record_list, dsc_compare_path, path);
	if (path_item) {
		return QListObj(path_item);
	}
	return NULL;
}

dsc_dst_port_t* dsc_find_dst_port(union ibv_gid *gid)
{
	LIST_ITEM *subnet_item;

	for_each (&subnet_list, subnet_item) {
		dsc_subnet_t *subnet = QListObj(subnet_item);
		LIST_ITEM *dst_port_item = QListFindItem(&subnet->dst_port_list, dsc_compare_dst_port, gid);
		if (dst_port_item) {
			return QListObj(dst_port_item);
		}
	}

	return NULL;
}

dsc_dst_port_t* dsc_find_dst_port_by_name(char *node_desc)
{
	LIST_ITEM *subnet_item;

	for_each (&subnet_list, subnet_item) {
		dsc_subnet_t *subnet = QListObj(subnet_item);
		LIST_ITEM *dst_port_item = QListFindItem(&subnet->dst_port_list, dsc_compare_dst_port_by_name, node_desc);
		if (dst_port_item) {
			return QListObj(dst_port_item);
		}
	}

	return NULL;
}

dsc_virtual_fabric_t* dsc_find_virtual_fabric(uint8_t *vfName, dsc_subnet_t *subnet)
{
	LIST_ITEM *vf_item;
	LIST_ITEM *subnet_item;

	if (subnet) {
		vf_item = QListFindItem(&subnet->virtual_fabric_list, dsc_compare_virtual_fabric, vfName);
		return vf_item ? QListObj(vf_item) : NULL;
	}

	for_each (&subnet_list, subnet_item) {
		subnet = (dsc_subnet_t *)ListObj(subnet_item);
		vf_item = QListFindItem(&subnet->virtual_fabric_list, dsc_compare_virtual_fabric, vfName);
		if (vf_item) {
			return QListObj(vf_item);
		}
	}

	return NULL;
}

size_t dsc_total_src_port_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item;

	for_each (&subnet_list, item) {
		total_count += dsc_src_port_count((dsc_subnet_t*)QListObj(item));
	}

	return total_count;
}

size_t dsc_total_dst_port_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item;

	for_each (&subnet_list, item) {
		total_count += dsc_dst_port_count((dsc_subnet_t*)QListObj(item));
	}

	return total_count;
}

size_t dsc_total_virtual_fabric_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item;

	for_each (&subnet_list, item) {
		total_count += dsc_virtual_fabric_count((dsc_subnet_t*)QListObj(item));
	}

	return total_count;
}

size_t dsc_total_service_id_record_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item1, *item2;

	for_each (&subnet_list, item1) {
		dsc_subnet_t *subnet = QListObj(item1);
		
		for_each (&subnet->virtual_fabric_list, item2) {
			total_count += dsc_service_id_record_count((dsc_virtual_fabric_t*)QListObj(item2));
		}
	}

	return total_count;
}

size_t dsc_subnet_path_record_count(dsc_subnet_t *subnet)
{
	size_t total_count = 0;
	LIST_ITEM *item;
	for_each (&subnet->src_port_list, item) {
		total_count += dsc_path_record_count((dsc_src_port_t*)QListObj(item));
	}

	return total_count;
}

size_t dsc_total_path_record_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item1;

	for_each (&subnet_list, item1) {
		dsc_subnet_t *subnet = QListObj(item1);
		
		total_count += dsc_subnet_path_record_count(subnet);
	}

	return total_count;
}

size_t dsc_total_pkey_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item1, *item2;

	for_each (&subnet_list, item1) {
		dsc_subnet_t *subnet = QListObj(item1);
		
		for_each (&subnet->src_port_list, item2) {
			total_count += dsc_pkey_count((dsc_src_port_t*)QListObj(item2));
		}
	}

	return total_count;
}

/* PKEY Handling functions */

size_t dsc_pkey_count(dsc_src_port_t *src_port)
{
	return QListCount(&src_port->pkey_list);
}

FSTATUS dsc_add_pkey(dsc_src_port_t *src_port, uint16_t pkey)
{
	dsc_pkey_t *new_pkey = malloc(sizeof(*new_pkey));

	if (new_pkey == NULL) {
		return FINSUFFICIENT_MEMORY;
	}

	ListItemInitState(&new_pkey->item);
	QListSetObj(&new_pkey->item, new_pkey);
	new_pkey->pkey = pkey;
	QListInsertTail(&src_port->pkey_list, &new_pkey->item);

	return FSUCCESS;
}

FSTATUS dsc_empty_pkey_list(dsc_src_port_t *src_port)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&src_port->pkey_list)) != NULL) {
		free(QListObj(item));
	}

	return FSUCCESS;
}

FSTATUS dsc_remove_pkey(dsc_src_port_t *src_port, uint16_t pkey)
{
	LIST_ITEM *item = QListFindItem(&src_port->pkey_list, dsc_compare_pkey, &pkey);

	if (!item) {
		return FNOT_FOUND;
	}

	QListRemoveItem(&src_port->pkey_list, item);
	free(QListObj(item));

	return FSUCCESS;
}

/* Path Record Handling functions */

size_t dsc_path_record_count(dsc_src_port_t *src_port)
{
	return QListCount(&src_port->path_record_list);
}

FSTATUS dsc_add_path_record(dsc_src_port_t *src_port,
							IB_PATH_RECORD_NO *path_record)
{
	dsc_path_record_t *new_path_record = malloc(sizeof(*new_path_record));

	if (new_path_record == NULL) {
		return FINSUFFICIENT_MEMORY;
	}

	ListItemInitState(&new_path_record->item);
	QListSetObj(&new_path_record->item, new_path_record);
	new_path_record->path = *path_record;
	QListInsertTail(&src_port->path_record_list, &new_path_record->item);

	return FSUCCESS;
}

FSTATUS dsc_empty_path_record_list(dsc_src_port_t *src_port)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&src_port->path_record_list)) != NULL) {
		free(QListObj(item));
	}

	return FSUCCESS;
}

FSTATUS dsc_remove_path_records(dsc_src_port_t *src_port, union ibv_gid *dst_port_gid)
{
	LIST_ITEM *item, *next_item;

	for (item = QListHead(&src_port->path_record_list); item != NULL; item = next_item) {
		next_item = QListNext(&src_port->path_record_list, item);
		dsc_path_record_t *path_record = QListObj(item);
		if (dst_port_gid->global.interface_id == path_record->path.DGID.Type.Global.InterfaceID) {
			QListRemoveItem(&src_port->path_record_list, &path_record->item);
			free(path_record);
		}
	}

	return FSUCCESS;
}

/* Src Port Handling functions */

size_t dsc_src_port_count(dsc_subnet_t *subnet)
{
	return QListCount(&subnet->src_port_list);
}

FSTATUS dsc_update_src_port(umad_port_t *port, union ibv_gid *sm_gid)
{
	union ibv_gid src_port_gid;
	dsc_src_port_t *src_port;
	int pkey_index;

	src_port_gid.global.subnet_prefix = port->gid_prefix;
	src_port_gid.global.interface_id = port->port_guid;

	src_port = dsc_find_src_port(&src_port_gid);
	if (src_port == NULL) {
		return FNOT_FOUND;
	}

	src_port->gid = src_port_gid;
	if (sm_gid) {
		src_port->sm_gid = *sm_gid;
	}
	strncpy(src_port->hca_name, port->ca_name, sizeof(src_port->hca_name));
	src_port->port_num = port->portnum;
	src_port->base_lid = port->base_lid;
	src_port->lmc = port->lmc;
	src_port->state = port->state;
	dsc_empty_pkey_list(src_port);
	dsc_empty_path_record_list(src_port);
	
	if (port->state == IBV_PORT_ACTIVE) {
		for (pkey_index = 0; (pkey_index < port->pkeys_size) && (port->pkeys[pkey_index] != 0); pkey_index++) {
			if (dsc_add_pkey(src_port, htons(port->pkeys[pkey_index])) != FSUCCESS) {
				dsc_empty_pkey_list(src_port);
				return FINSUFFICIENT_MEMORY;
			}
		}
	} else {
		/* It should be in the dst port list */
		if (dsc_remove_dst_port(&src_port_gid)) {
			_DBG_INFO("Failure Removing Destination Port 0x%016"PRIx64":0x%016"PRIx64"\n",
					  ntohll(src_port_gid.global.subnet_prefix),
					  ntohll(src_port_gid.global.interface_id));
		}
	}

	_DBG_INFO("Updated port %d on HFI %s: base_lid 0x%x, lmc 0x%x\n", 
			  src_port->port_num, src_port->hca_name,
			  src_port->base_lid, src_port->lmc);

	return FSUCCESS;
}

FSTATUS dsc_add_src_port(umad_port_t *port, union ibv_gid *sm_gid)
{
	FSTATUS rval;
	dsc_subnet_t *subnet;
	union ibv_gid src_port_gid;
	dsc_src_port_t *src_port;

	subnet = dsc_find_subnet(sm_gid);
	if (!subnet) {
		if (dsc_add_subnet(sm_gid) != FSUCCESS) {
			return FINSUFFICIENT_MEMORY;
		}

		subnet = dsc_find_subnet(sm_gid);
		if (!subnet) {
			return FNOT_FOUND; /* This better not happen */
		}
	}

	src_port_gid.global.subnet_prefix = port->gid_prefix;
	src_port_gid.global.interface_id = port->port_guid;

	src_port = dsc_find_src_port(&src_port_gid);
	if (src_port) {
		return FDUPLICATE;
	}

	src_port = malloc(sizeof(*src_port));
	if (src_port == NULL) {
		return FINSUFFICIENT_MEMORY;
	}

	ListItemInitState(&src_port->item);
	QListSetObj(&src_port->item, src_port);
	src_port->gid = src_port_gid;
	QListInitState(&src_port->pkey_list);
	QListInit(&src_port->pkey_list);
	QListInitState(&src_port->path_record_list);
	QListInit(&src_port->path_record_list);
	QListInsertTail(&subnet->src_port_list, &src_port->item);

	rval = dsc_update_src_port(port, sm_gid);
	if (rval != FSUCCESS) {
		free(src_port);
		return rval;
	}

	_DBG_DEBUG("Added port %d on HFI %s\n", src_port->port_num, src_port->hca_name);

	return FSUCCESS;
}

FSTATUS dsc_empty_src_port_list(dsc_subnet_t *subnet)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&subnet->src_port_list)) != NULL) {
		dsc_src_port_t *src_port = QListObj(item);
		dsc_empty_path_record_list(src_port);
		dsc_empty_pkey_list(src_port);
		QListDestroy(&src_port->path_record_list);
		free(src_port);
	}

	return FSUCCESS;
}

/* Dst Port Handling functions */

size_t dsc_dst_port_count(dsc_subnet_t *subnet)
{
	return QListCount(&subnet->dst_port_list);
}

FSTATUS dsc_add_dst_port(union ibv_gid *dst_port_gid, union ibv_gid *sm_gid, NODE_TYPE node_type, char * node_desc)
{
	dsc_subnet_t *subnet = dsc_find_subnet(sm_gid);
	dsc_dst_port_t *dst_port;

	if (!subnet) {
		return FNOT_DONE;
	}

	dst_port = dsc_find_dst_port(dst_port_gid);
	if (dst_port) {
		return FDUPLICATE;
	}

	dst_port = malloc(sizeof(*dst_port));
	if (dst_port == NULL) {
		return FINSUFFICIENT_MEMORY;
	}

	ListItemInitState(&dst_port->item);
	QListSetObj(&dst_port->item, dst_port);
	dst_port->gid       = *dst_port_gid;
	dst_port->sm_gid    = *sm_gid;
	dst_port->node_type = node_type;
	memcpy(dst_port->node_desc, node_desc, NODE_DESCRIPTION_ARRAY_SIZE);
	QListInsertTail(&subnet->dst_port_list, &dst_port->item);

#ifdef PRINT_PORTS_FOUND
	if (dst_port->node_type == STL_NODE_FI) {
		_DBG_DEBUG("Added Channel Adapter Port 0x%016"PRIx64":0x%016"PRIx64".\n",
					ntohll(dst_port->gid.global.subnet_prefix),
					ntohll(dst_port->gid.global.interface_id));
	} else {
		_DBG_DEBUG("Added Switch Port 0x%016"PRIx64":0x%016"PRIx64".\n",
					ntohll(dst_port->gid.global.subnet_prefix),
					ntohll(dst_port->gid.global.interface_id));
	}
#endif

	return FSUCCESS;
}

FSTATUS dsc_empty_dst_port_list(dsc_subnet_t *subnet)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&subnet->dst_port_list)) != NULL) {
		free(QListObj(item));
	}

	return FSUCCESS;
}

FSTATUS dsc_remove_dst_port(union ibv_gid *dst_port_gid)
{
	dsc_subnet_t *subnet;
	dsc_dst_port_t *dst_port;
	LIST_ITEM *item;

	dst_port = dsc_find_dst_port(dst_port_gid);
	if (!dst_port) {
		return FNOT_FOUND;
	}

	subnet = dsc_find_subnet(&dst_port->sm_gid);
	if (!subnet) {
		free(dst_port);
		return FNOT_FOUND;
	}

	if (dst_port->node_type == STL_NODE_FI) {
		for_each (&subnet->src_port_list, item) {
			dsc_remove_path_records((dsc_src_port_t *)ListObj(item), &dst_port->gid);
		}
	}

	QListRemoveItem(&subnet->dst_port_list, &dst_port->item);
	free(dst_port);

	return FSUCCESS;
}

/* Service ID Record Handling functions */

size_t dsc_service_id_record_count(dsc_virtual_fabric_t *virtual_fabric)
{
	return QListCount(&virtual_fabric->service_id_record_list);
}

FSTATUS dsc_add_service_id_record(dsc_virtual_fabric_t *virtual_fabric,
								  dsc_service_id_range_t *service_id_range)
{
	LIST_ITEM *item = QListFindItem(&virtual_fabric->service_id_record_list, dsc_compare_service_id_record, service_id_range);
	dsc_service_id_record_t *new_service_id_record;

	if (item) {
		return FDUPLICATE;
	}

	new_service_id_record = malloc(sizeof(*new_service_id_record));
	if (new_service_id_record == NULL) {
		return FINSUFFICIENT_MEMORY;
	}

	ListItemInitState(&new_service_id_record->item);
	QListSetObj(&new_service_id_record->item, new_service_id_record);
	new_service_id_record->service_id_range = *service_id_range;
	QListInsertTail(&virtual_fabric->service_id_record_list, &new_service_id_record->item);

	return FSUCCESS;
}

FSTATUS dsc_empty_service_id_record_list(dsc_virtual_fabric_t *virtual_fabric)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&virtual_fabric->service_id_record_list)) != NULL) {
		free(QListObj(item));
	}

	return FSUCCESS;
}

FSTATUS dsc_remove_service_id_record(dsc_virtual_fabric_t *virtual_fabric,
									 dsc_service_id_range_t *service_id_range)
{
	LIST_ITEM *item = QListFindItem(&virtual_fabric->service_id_record_list, dsc_compare_service_id_record, service_id_range);

	if (!item) {
		return FNOT_FOUND;
	}

	QListRemoveItem(&virtual_fabric->service_id_record_list, item);
	free(QListObj(item));

	return FSUCCESS;
}

/* Virtual Fabric Record Handling functions */

size_t dsc_virtual_fabric_count(dsc_subnet_t *subnet)
{
	return QListCount(&subnet->virtual_fabric_list);
}

FSTATUS dsc_add_virtual_fabric(dsc_subnet_t *subnet,
							   STL_VFINFO_RECORD *vfinfo_record)
{
	dsc_virtual_fabric_t *virtual_fabric = dsc_find_virtual_fabric(vfinfo_record->vfName, subnet);

	if (virtual_fabric) {
		return FDUPLICATE;
	}

	virtual_fabric = malloc(sizeof(*virtual_fabric));
	if (virtual_fabric == NULL) {
		return FINSUFFICIENT_MEMORY;
	}

	ListItemInitState(&virtual_fabric->item);
	QListSetObj(&virtual_fabric->item, virtual_fabric);
	virtual_fabric->vfinfo_record = *vfinfo_record;
	QListInitState(&virtual_fabric->service_id_record_list);
	QListInit(&virtual_fabric->service_id_record_list);
	QListInsertTail(&subnet->virtual_fabric_list, &virtual_fabric->item);

	return FSUCCESS;
}

FSTATUS dsc_empty_virtual_fabric_list(dsc_subnet_t *subnet)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&subnet->virtual_fabric_list)) != NULL) {
		dsc_virtual_fabric_t *virtual_fabric = QListObj(item);
		dsc_empty_service_id_record_list(virtual_fabric);
		QListDestroy(&virtual_fabric->service_id_record_list);
		free(virtual_fabric);
	}

	return FSUCCESS;
}

FSTATUS dsc_remove_virtual_fabric(dsc_subnet_t *subnet,
								  uint8_t *vfName)
{
	dsc_virtual_fabric_t *virtual_fabric = dsc_find_virtual_fabric(vfName, subnet);

	if (!virtual_fabric) {
		return FNOT_FOUND;
	}

	QListRemoveItem(&subnet->virtual_fabric_list, &virtual_fabric->item);
	dsc_empty_service_id_record_list(virtual_fabric);
	QListDestroy(&virtual_fabric->service_id_record_list);
	free(virtual_fabric);

	return FSUCCESS;
}

/* Subnet Handling functions */

size_t dsc_subnet_count(void)
{
	return QListCount(&subnet_list);
}

dsc_subnet_t* dsc_get_subnet_at(uint32_t index)
{
	LIST_ITEM *item = QListGetItemAt(&subnet_list, index);

	if (item) {
		return (dsc_subnet_t*)QListObj(item);
	}

	return NULL;
}

FSTATUS dsc_add_subnet(union ibv_gid *sm_gid)
{
	dsc_subnet_t *subnet = dsc_find_subnet(sm_gid);

	if (subnet) {
		return FDUPLICATE;
	}

	subnet = malloc(sizeof(*subnet));
	if (subnet == NULL) {
		return FINSUFFICIENT_MEMORY;
	}
	memset((void*)subnet,0,sizeof(*subnet));

	ListItemInitState(&subnet->item);
	QListSetObj(&subnet->item, subnet);
	subnet->sm_gid = *sm_gid;
	QListInitState(&subnet->src_port_list);
	QListInit(&subnet->src_port_list);
	QListInitState(&subnet->dst_port_list);
	QListInit(&subnet->dst_port_list);
	QListInitState(&subnet->virtual_fabric_list);
	QListInit(&subnet->virtual_fabric_list);
	QListInsertTail(&subnet_list, &subnet->item);

	return FSUCCESS;
}

FSTATUS dsc_empty_subnet_list(void)
{
	LIST_ITEM *item;

	_DBG_DEBUG("Emptying subnet list.\n");

	while ((item = QListRemoveHead(&subnet_list)) != NULL) {
		dsc_subnet_t *subnet = QListObj(item);
		dsc_empty_src_port_list(subnet);
		dsc_empty_dst_port_list(subnet);
		dsc_empty_virtual_fabric_list(subnet);
		QListDestroy(&subnet->src_port_list);
		QListDestroy(&subnet->dst_port_list);
		QListDestroy(&subnet->virtual_fabric_list);
		free(subnet);
	}

	return FSUCCESS;
}

void dsc_topology_cleanup(void)
{
	dsc_empty_subnet_list();
}

FSTATUS dsc_topology_init(void)
{
	QListInitState(&subnet_list);
	QListInit(&subnet_list);

	return FSUCCESS;
}
