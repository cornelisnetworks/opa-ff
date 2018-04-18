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

#include <stdio.h>
#include <stdlib.h>

/* work around conflicting names */
#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include <arpa/inet.h>
#include "opasadb_debug.h"
#include "dsap.h"
#include "dsap_topology.h"

static QUICK_LIST subnet_list;

/* Command line parsing */
QUICK_LIST sid_range_args;
static int sid_range_args_init = 0;

/*
 * Valid formats are "SID" and "SID-SID" where "SID"
 * is a 64-bit hexadecimal string.
 */
int dsap_service_id_range_parser(char *str, void *ptr)
{
	uint64 lower, upper;
	dsap_service_id_record_t *record;
	int err = 0;
	char *k=0;

	if (!str || !ptr) {
		acm_log(0, "Bad arguments to sid parser.\n");
		err = EINVAL;
		goto exit;
	}

	if (sid_range_args_init == 0) {
		sid_range_args_init = QListInit(&sid_range_args);
		if (!sid_range_args_init) {
			acm_log(0, "Failed to initialize quick list.\n");
			err = ENOMEM;
			goto exit;
		}
	}

	record = malloc(sizeof(dsap_service_id_record_t));
	if (!record) {
		acm_log(0, "Failed to allocate memory.\n");
		err = ENOMEM;
		goto exit;
	}
	memset(record, 0, sizeof(dsap_service_id_record_t));

	/* Disabling ranged parameters for now.
	k = strchr(str,'-');
	if (k) *k=0; */

	lower = strtoull(str, NULL, 0);
	upper = (k)? strtoull(k+1, NULL, 0) : 0;

	record->service_id_range.warned = 0;
	record->service_id_range.lower_service_id = hton64(lower);
	record->service_id_range.upper_service_id = hton64(upper);

	QListSetObj(&record->item, record);
	QListInsertTail(&sid_range_args, &record->item);

exit:
	return err;
}

/*
 * Note that this function isn't re-entrant.
 */
char *dsap_service_id_range_printer(void *ptr)
{
	static char buffer[1024];
	int p = 0;
	LIST_ITEM *item;
	dsap_service_id_record_t *record;
	uint64_t lo, hi;
	int count = 0;

	if (!sid_range_args_init)
		return "";
	else
		p += sprintf(&buffer[p], "\n\t\t");
	for (item = QListHead(&sid_range_args);
		item != NULL && p<960; 
		item = QListNext(&sid_range_args, item)) {
		record = QListObj(item);
		if (record->service_id_range.upper_service_id) {
			lo = record->service_id_range.lower_service_id;
			hi = record->service_id_range.upper_service_id;
			p += sprintf(&buffer[p],
				     "[0x%016"PRIx64"-0x%016"PRIx64"] ",
				     ntoh64(lo), ntoh64(hi));
			count += 2;
		} else {
			lo = record->service_id_range.lower_service_id;
			p +=  sprintf(&buffer[p],"0x%016"PRIx64" ",
				      ntoh64(lo));
			count++;
		}
		if (!(count % 4))
			p += sprintf(&buffer[p], "\n\t\t");
	}

	return buffer;
}

/* Comparison Callback Functions */

static boolean dsap_compare_pkey(LIST_ITEM *item, void *context)
{
	dsap_pkey_t *pkey_item = QListObj(item);
	uint16_t *pkey = context;

	return (pkey_item->pkey == *pkey);
}

static boolean dsap_compare_src_port(LIST_ITEM *item, void *context)
{
	dsap_src_port_t *src_port = QListObj(item);
	union ibv_gid *gid = context;

	return (src_port->gid.global.interface_id == gid->global.interface_id);
}

static boolean dsap_compare_src_port_by_lid(LIST_ITEM *item, void *context)
{
	dsap_src_port_t *src_port = QListObj(item);
	unsigned *lid = context;
	unsigned mask = 0xffff - ((1 << src_port->lmc) - 1);

	return ((src_port->base_lid & mask )== (*lid & mask));
}

static boolean dsap_compare_path(LIST_ITEM *item, void *context)
{
	dsap_path_record_t *dsap_path = QListObj(item);
	IB_PATH_RECORD_NO *path     = context;
	uint16_t key_mask           = htons(0x7fff);

	/* Compare Incoming parameters */
	if (path->ServiceID && (path->ServiceID != dsap_path->path.ServiceID))
		return 0;

	if (path->DGID.Type.Global.InterfaceID && 
	    (path->DGID.Type.Global.InterfaceID != 
	     dsap_path->path.DGID.Type.Global.InterfaceID)) 
		return 0;

	if (path->DGID.Type.Global.SubnetPrefix && 
	    (path->DGID.Type.Global.SubnetPrefix != 
	     dsap_path->path.DGID.Type.Global.SubnetPrefix)) 
		return 0;

	if (path->DLID && (path->DLID != dsap_path->path.DLID)) 
		return 0;

	if ((path->P_Key & key_mask) && 
	    (path->P_Key & key_mask) != (dsap_path->path.P_Key & key_mask))
		return 0;

	/* Found */
	return 1;
}

static boolean dsap_compare_dst_port(LIST_ITEM *item, void *context)
{
	dsap_dst_port_t *dst_port = QListObj(item);
	union ibv_gid  *gid      = context;

	return (dst_port->gid.global.interface_id == gid->global.interface_id);
}

static boolean dsap_compare_dst_port_by_name(LIST_ITEM *item, void *context)
{
	dsap_dst_port_t *dst_port = QListObj(item);
	char  *desc      = (char *)context;

	/* We are not doing the exact match here. As long as the user provided
	   destination is part of the node description, it will be fine for us.
	   */
	return (0 == strncasecmp(dst_port->node_desc, desc, strlen(desc))); 
}

static boolean dsap_compare_service_id_record(LIST_ITEM *item, void *context)
{
	dsap_service_id_record_t *rec = QListObj(item);
	dsap_service_id_range_t  *range  = context;

	return ((rec->service_id_range.lower_service_id == 
		 range->lower_service_id) &&
		(rec->service_id_range.upper_service_id == 
		 range->upper_service_id));
}

static boolean dsap_compare_virtual_fabric(LIST_ITEM *item, void *context)
{
	dsap_virtual_fabric_t *virtual_fabric = QListObj(item);
	uint8_t *vfName = context;

	return (strncmp((const char*)virtual_fabric->vfinfo_record.vfName,
			(const char*)vfName,
			sizeof(virtual_fabric->vfinfo_record.vfName)) == 0);
}

static boolean dsap_compare_subnet(LIST_ITEM *item, void *context)
{
	dsap_subnet_t *subnet = QListObj(item);
	uint64_t * subnet_prefix = context;

	return (subnet->subnet_prefix == *subnet_prefix);
}

/* Search Utility Routines */

dsap_subnet_t* dsap_find_subnet(uint64_t *subnet_prefix)
{
	LIST_ITEM *item = QListFindItem(&subnet_list, dsap_compare_subnet,
					subnet_prefix);

	return item ? QListObj(item) : NULL;
}

dsap_src_port_t* dsap_find_src_port(union ibv_gid *gid)
{
	LIST_ITEM *subnet_item;
	dsap_subnet_t *subnet;
	LIST_ITEM *src_port_item;

	for_each (&subnet_list, subnet_item) {
		subnet = QListObj(subnet_item);
		src_port_item = QListFindItem(&subnet->src_port_list, 
					      dsap_compare_src_port, gid);
		if (src_port_item) 
			return QListObj(src_port_item);
	}

	return NULL;
}

dsap_src_port_t* dsap_find_src_port_by_lid(unsigned *lid)
{
	LIST_ITEM *subnet_item;
	dsap_subnet_t *subnet;
	LIST_ITEM *src_port_item;

	for_each (&subnet_list, subnet_item) {
		subnet = QListObj(subnet_item);
		src_port_item = QListFindItem(&subnet->src_port_list,
					      dsap_compare_src_port_by_lid,
					      lid);
		if (src_port_item)
			return QListObj(src_port_item);
	}

	return NULL;
}

dsap_path_record_t * dsap_find_path_record(dsap_src_port_t *src_port,
					   IB_PATH_RECORD_NO *path)
{
	LIST_ITEM *path_item = QListFindFromHead(&src_port->path_record_list,
						 dsap_compare_path, path);
	if (path_item) 
		return QListObj(path_item);
	return NULL;
}

dsap_dst_port_t* dsap_find_dst_port(union ibv_gid *gid)
{
	LIST_ITEM *subnet_item;
	LIST_ITEM *dst_port_item;

	for_each (&subnet_list, subnet_item) {
		dsap_subnet_t *subnet = QListObj(subnet_item);
		dst_port_item = QListFindItem(&subnet->dst_port_list,
					      dsap_compare_dst_port, gid);
		if (dst_port_item)
			return QListObj(dst_port_item);
	}

	return NULL;
}

dsap_dst_port_t* dsap_find_dst_port_by_name(char *node_desc)
{
	LIST_ITEM *subnet_item;
	LIST_ITEM *dst_port_item;

	for_each (&subnet_list, subnet_item) {
		dsap_subnet_t *subnet = QListObj(subnet_item);
		dst_port_item = QListFindItem(&subnet->dst_port_list,
					      dsap_compare_dst_port_by_name,
					      node_desc);
		if (dst_port_item) {
			return QListObj(dst_port_item);
		}
	}

	return NULL;
}

dsap_virtual_fabric_t* dsap_find_virtual_fabric(uint8_t *vfName, 
						dsap_subnet_t *subnet)
{
	LIST_ITEM *vf_item;
	LIST_ITEM *subnet_item;

	if (subnet) {
		vf_item = QListFindItem(&subnet->virtual_fabric_list, 
					dsap_compare_virtual_fabric, vfName);
		return vf_item ? QListObj(vf_item) : NULL;
	}

	for_each (&subnet_list, subnet_item) {
		subnet = (dsap_subnet_t *)ListObj(subnet_item);
		vf_item = QListFindItem(&subnet->virtual_fabric_list, 
					dsap_compare_virtual_fabric, vfName);
		if (vf_item) 
			return QListObj(vf_item);
	}

	return NULL;
}

size_t dsap_tot_src_port_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item;

	for_each (&subnet_list, item) {
		total_count += dsap_src_port_count(
		   (dsap_subnet_t*)QListObj(item));
	}

	return total_count;
}

size_t dsap_tot_dst_port_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item;

	for_each (&subnet_list, item) {
		total_count += dsap_dst_port_count(
		   (dsap_subnet_t*)QListObj(item));
	}

	return total_count;
}

size_t dsap_tot_vfab_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item;

	for_each (&subnet_list, item) {
		total_count += dsap_virtual_fabric_count(
		   (dsap_subnet_t*)QListObj(item));
	}

	return total_count;
}

size_t dsap_tot_sid_rec_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item1, *item2;
	dsap_subnet_t *subnet;

	for_each (&subnet_list, item1) {
		subnet = QListObj(item1);
		
		for_each (&subnet->virtual_fabric_list, item2) {
			total_count += dsap_service_id_record_count(
			   (dsap_virtual_fabric_t*)QListObj(item2));
		}
	}

	return total_count;
}

size_t dsap_subnet_path_record_count(dsap_subnet_t *subnet)
{
	size_t total_count = 0;
	LIST_ITEM *item;
	for_each (&subnet->src_port_list, item) {
		total_count += dsap_path_record_count(
		   (dsap_src_port_t*)QListObj(item));
	}

	return total_count;
}

size_t dsap_tot_path_rec_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item1;
	dsap_subnet_t *subnet;

	for_each (&subnet_list, item1) {
		subnet = QListObj(item1);
		total_count += dsap_subnet_path_record_count(subnet);
	}

	return total_count;
}

size_t dsap_tot_pkey_count(void)
{
	size_t total_count = 0;
	LIST_ITEM *item1, *item2;

	for_each (&subnet_list, item1) {
		dsap_subnet_t *subnet = QListObj(item1);
		
		for_each (&subnet->src_port_list, item2) {
			total_count += dsap_pkey_count(
			   (dsap_src_port_t*)QListObj(item2));
		}
	}

	return total_count;
}

/* PKEY Handling functions */

size_t dsap_pkey_count(dsap_src_port_t *src_port)
{
	return QListCount(&src_port->pkey_list);
}

/* pkey is in network byte order */
FSTATUS dsap_add_pkey(dsap_src_port_t *src_port, uint16_t pkey)
{
	dsap_pkey_t *new_pkey = malloc(sizeof(*new_pkey));

	if (new_pkey == NULL)
		return FINSUFFICIENT_MEMORY;

	ListItemInitState(&new_pkey->item);
	QListSetObj(&new_pkey->item, new_pkey);
	new_pkey->pkey = pkey;
	QListInsertTail(&src_port->pkey_list, &new_pkey->item);

	return FSUCCESS;
}

FSTATUS dsap_empty_pkey_list(dsap_src_port_t *src_port)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&src_port->pkey_list)) != NULL) {
		free(QListObj(item));
	}

	return FSUCCESS;
}

FSTATUS dsap_remove_pkey(dsap_src_port_t *src_port, uint16_t pkey)
{
	LIST_ITEM *item = 
		QListFindItem(&src_port->pkey_list, dsap_compare_pkey, &pkey);

	if (!item)
		return FNOT_FOUND;

	QListRemoveItem(&src_port->pkey_list, item);
	free(QListObj(item));

	return FSUCCESS;
}

/* Path Record Handling functions */

size_t dsap_path_record_count(dsap_src_port_t *src_port)
{
	return QListCount(&src_port->path_record_list);
}

FSTATUS dsap_add_path_record(dsap_src_port_t *src_port,
			     IB_PATH_RECORD_NO *path_record)
{
	dsap_path_record_t *new_path_record = malloc(sizeof(*new_path_record));

	if (new_path_record == NULL) 
		return FINSUFFICIENT_MEMORY;

	ListItemInitState(&new_path_record->item);
	QListSetObj(&new_path_record->item, new_path_record);
	new_path_record->path = *path_record;
	QListInsertTail(&src_port->path_record_list, &new_path_record->item);

	return FSUCCESS;
}

FSTATUS dsap_empty_path_record_list(dsap_src_port_t *src_port)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&src_port->path_record_list)) != NULL) {
		free(QListObj(item));
	}

	return FSUCCESS;
}

FSTATUS dsap_remove_path_records(dsap_src_port_t *src_port, 
				 union ibv_gid *dst_port_gid)
{
	LIST_ITEM *item, *next_item;
	dsap_path_record_t *path_record;

	for (item = QListHead(&src_port->path_record_list); item != NULL;
	      item = next_item) {
		next_item = QListNext(&src_port->path_record_list, item);
		path_record = QListObj(item);
		if (dst_port_gid->global.interface_id == 
		    path_record->path.DGID.Type.Global.InterfaceID) {
			QListRemoveItem(&src_port->path_record_list, 
					&path_record->item);
			free(path_record);
		}
	}

	return FSUCCESS;
}

/* Src Port Handling functions */

size_t dsap_src_port_count(dsap_subnet_t *subnet)
{
	return QListCount(&subnet->src_port_list);
}

/* The port lock is held */
FSTATUS dsap_update_src_port(dsap_src_port_t *src_port, struct dsap_port *port)
{
	LIST_ITEM * ep_item;
	struct dsap_ep *ep;

	strncpy(src_port->hfi_name, port->dev->device->verbs->device->name,
		sizeof(src_port->hfi_name)-1);
	src_port->hfi_name[sizeof(src_port->hfi_name)-1]=0;
	src_port->port_num = port->port->port_num;
	src_port->base_lid = port->lid;
	src_port->lmc = port->lmc;
	src_port->state = port->state;
	dsap_empty_pkey_list(src_port);
	dsap_empty_path_record_list(src_port);
	
	/* Get the pkey from the ep list */
	for_each(&port->ep_list, ep_item) {
		ep = QListObj(ep_item);
		if (dsap_add_pkey(src_port, htons(ep->endpoint->pkey))
		    != FSUCCESS) {
			dsap_empty_pkey_list(src_port);
			return FINSUFFICIENT_MEMORY;
		}
	}

	acm_log(2, "Updated port %d on HFI %s: base_lid 0x%x, lmc 0x%x\n",
		src_port->port_num, src_port->hfi_name,
		src_port->base_lid, src_port->lmc);

	return FSUCCESS;
}

FSTATUS dsap_remove_src_port(union ibv_gid *src_gid)
{
	dsap_src_port_t *src_port;
	dsap_subnet_t *subnet;

	acm_log(2, "\n");

	src_port = dsap_find_src_port(src_gid);
	if (!src_port) 
		return FNOT_FOUND;

	/* It should be in the dst port list */
	if (dsap_remove_dst_port(src_gid)) {
		acm_log(0, "Failure Removing Dst Port 0x%016"PRIx64":0x%016"
			   PRIx64"\n",
			ntoh64(src_gid->global.subnet_prefix),
			ntoh64(src_gid->global.interface_id));
	}

	dsap_empty_pkey_list(src_port);
	dsap_empty_path_record_list(src_port);

	subnet = dsap_find_subnet(&src_port->gid.global.subnet_prefix);
	if (subnet) 
		QListRemoveItem(&subnet->src_port_list, &src_port->item);

	free(src_port);

	return FSUCCESS;
}

/* The port lock is held */
FSTATUS dsap_add_src_port(struct dsap_port *port)
{
	FSTATUS rval;
	dsap_subnet_t *subnet;
	union ibv_gid src_port_gid;
	dsap_src_port_t *src_port;

	if (acm_get_gid((struct acm_port *) port->port, 0, &src_port_gid))
		return FNOT_FOUND;

	subnet = dsap_find_subnet(&src_port_gid.global.subnet_prefix);
	if (!subnet) {
		if (dsap_add_subnet(src_port_gid.global.subnet_prefix) != 
		    FSUCCESS)
			return FINSUFFICIENT_MEMORY;

		subnet = dsap_find_subnet(&src_port_gid.global.subnet_prefix);
		if (!subnet) 
			return FNOT_FOUND; /* This better not happen */
	}

	src_port = dsap_find_src_port(&src_port_gid);
	if (src_port) 
		return FDUPLICATE;

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

	rval = dsap_update_src_port(src_port, port);
	if (rval != FSUCCESS) {
		free(src_port);
		return rval;
	}

	acm_log(2, "Added port %d on HFI %s\n", src_port->port_num, 
		src_port->hfi_name);

	return FSUCCESS;
}

FSTATUS dsap_empty_src_port_list(dsap_subnet_t *subnet)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&subnet->src_port_list)) != NULL) {
		dsap_src_port_t *src_port = QListObj(item);
		dsap_empty_path_record_list(src_port);
		dsap_empty_pkey_list(src_port);
		QListDestroy(&src_port->path_record_list);
		free(src_port);
	}

	return FSUCCESS;
}

/* Dst Port Handling functions */

size_t dsap_dst_port_count(dsap_subnet_t *subnet)
{
	return QListCount(&subnet->dst_port_list);
}

FSTATUS dsap_add_dst_port(union ibv_gid *dst_port_gid, NODE_TYPE node_type,
			  char * node_desc)
{
	dsap_subnet_t *subnet = dsap_find_subnet(
				&dst_port_gid->global.subnet_prefix);
	dsap_dst_port_t *dst_port;

	if (!subnet) 
		return FNOT_DONE;

	dst_port = dsap_find_dst_port(dst_port_gid);
	if (dst_port) 
		return FDUPLICATE;

	dst_port = malloc(sizeof(*dst_port));
	if (dst_port == NULL) 
		return FINSUFFICIENT_MEMORY;

	ListItemInitState(&dst_port->item);
	QListSetObj(&dst_port->item, dst_port);
	dst_port->gid       = *dst_port_gid;
	dst_port->node_type = node_type;
	memcpy(dst_port->node_desc, node_desc, NODE_DESCRIPTION_ARRAY_SIZE);
	QListInsertTail(&subnet->dst_port_list, &dst_port->item);

#ifdef PRINT_PORTS_FOUND
	if (dst_port->node_type == STL_NODE_FI) {
		acm_log(2, "Added HFI Port 0x%016"PRIx64":0x%016"PRIx64"\n",
			ntoh64(dst_port->gid.global.subnet_prefix),
			ntoh64(dst_port->gid.global.interface_id));
	} else {
		acm_log(2, "Added Switch Port 0x%016"PRIx64":0x%016"PRIx64"\n",
			ntoh64(dst_port->gid.global.subnet_prefix),
			ntoh64(dst_port->gid.global.interface_id));
	}
#endif

	return FSUCCESS;
}

FSTATUS dsap_empty_dst_port_list(dsap_subnet_t *subnet)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&subnet->dst_port_list)) != NULL) {
		free(QListObj(item));
	}

	return FSUCCESS;
}

FSTATUS dsap_remove_dst_port(union ibv_gid *dst_port_gid)
{
	dsap_subnet_t *subnet;
	dsap_dst_port_t *dst_port;
	LIST_ITEM *item;

	dst_port = dsap_find_dst_port(dst_port_gid);
	if (!dst_port) {
		return FNOT_FOUND;
	}

	subnet = dsap_find_subnet(&dst_port_gid->global.subnet_prefix);
	if (!subnet) {
		free(dst_port);
		return FNOT_FOUND;
	}

	if (dst_port->node_type == STL_NODE_FI) {
		for_each (&subnet->src_port_list, item) {
			dsap_remove_path_records(
			   (dsap_src_port_t *)ListObj(item), &dst_port->gid);
		}
	}

	QListRemoveItem(&subnet->dst_port_list, &dst_port->item);
	free(dst_port);

	return FSUCCESS;
}

/* Service ID Record Handling functions */

size_t dsap_service_id_record_count(dsap_virtual_fabric_t *virtual_fabric)
{
	return QListCount(&virtual_fabric->service_id_record_list);
}

FSTATUS dsap_add_service_id_record(dsap_virtual_fabric_t *virtual_fabric,
				   dsap_service_id_range_t *service_id_range)
{
	LIST_ITEM *item = QListFindItem(&virtual_fabric->service_id_record_list,
					dsap_compare_service_id_record, 
					service_id_range);
	dsap_service_id_record_t *new_service_id_record;

	if (item) 
		return FDUPLICATE;

	new_service_id_record = malloc(sizeof(*new_service_id_record));
	if (new_service_id_record == NULL) {
		return FINSUFFICIENT_MEMORY;
	}

	ListItemInitState(&new_service_id_record->item);
	QListSetObj(&new_service_id_record->item, new_service_id_record);
	new_service_id_record->service_id_range = *service_id_range;
	QListInsertTail(&virtual_fabric->service_id_record_list, 
			&new_service_id_record->item);

	return FSUCCESS;
}

FSTATUS dsap_empty_service_id_record_list(dsap_virtual_fabric_t *vfab)
{
	LIST_ITEM *item;

	while ((item = QListRemoveHead(&vfab->service_id_record_list))
	       != NULL) {
		free(QListObj(item));
	}

	return FSUCCESS;
}

FSTATUS dsap_remove_service_id_record(dsap_virtual_fabric_t *vfab,
				      dsap_service_id_range_t *sid_range)
{
	LIST_ITEM *item = QListFindItem(&vfab->service_id_record_list,
					dsap_compare_service_id_record,
					sid_range);

	if (!item) 
		return FNOT_FOUND;

	QListRemoveItem(&vfab->service_id_record_list, item);
	free(QListObj(item));

	return FSUCCESS;
}

/* Virtual Fabric Record Handling functions */

size_t dsap_virtual_fabric_count(dsap_subnet_t *subnet)
{
	return QListCount(&subnet->virtual_fabric_list);
}

FSTATUS dsap_add_virtual_fabric(dsap_subnet_t *subnet,
				STL_VFINFO_RECORD *vfinfo_record)
{
	dsap_virtual_fabric_t *vfab = 
		dsap_find_virtual_fabric(vfinfo_record->vfName, subnet);

	if (vfab) 
		return FDUPLICATE;

	vfab = malloc(sizeof(*vfab));
	if (vfab == NULL) 
		return FINSUFFICIENT_MEMORY;

	ListItemInitState(&vfab->item);
	QListSetObj(&vfab->item, vfab);
	vfab->vfinfo_record = *vfinfo_record;
	QListInitState(&vfab->service_id_record_list);
	QListInit(&vfab->service_id_record_list);
	QListInsertTail(&subnet->virtual_fabric_list, &vfab->item);

	return FSUCCESS;
}

FSTATUS dsap_empty_virtual_fabric_list(dsap_subnet_t *subnet)
{
	LIST_ITEM *item;
	dsap_virtual_fabric_t *vfab;

	while ((item = QListRemoveHead(&subnet->virtual_fabric_list))
	       != NULL) {
		vfab = QListObj(item);
		dsap_empty_service_id_record_list(vfab);
		QListDestroy(&vfab->service_id_record_list);
		free(vfab);
	}

	return FSUCCESS;
}

FSTATUS dsap_remove_virtual_fabric(dsap_subnet_t *subnet, uint8_t *vfName)
{
	dsap_virtual_fabric_t *vfab = dsap_find_virtual_fabric(vfName, subnet);

	if (!vfab) 
		return FNOT_FOUND;

	QListRemoveItem(&subnet->virtual_fabric_list, &vfab->item);
	dsap_empty_service_id_record_list(vfab);
	QListDestroy(&vfab->service_id_record_list);
	free(vfab);

	return FSUCCESS;
}

/* Subnet Handling functions */

size_t dsap_subnet_count(void)
{
	return QListCount(&subnet_list);
}

dsap_subnet_t* dsap_get_subnet_at(uint32_t index)
{
	LIST_ITEM *item = QListGetItemAt(&subnet_list, index);

	if (item) 
		return (dsap_subnet_t*)QListObj(item);

	return NULL;
}

FSTATUS dsap_add_subnet(uint64_t subnet_prefix)
{
	dsap_subnet_t *subnet = dsap_find_subnet(&subnet_prefix);

	if (subnet) 
		return FDUPLICATE;

	subnet = malloc(sizeof(*subnet));
	if (subnet == NULL)
		return FINSUFFICIENT_MEMORY;
	memset((void*)subnet,0,sizeof(*subnet));

	ListItemInitState(&subnet->item);
	QListSetObj(&subnet->item, subnet);
	subnet->subnet_prefix = subnet_prefix;
	QListInitState(&subnet->src_port_list);
	QListInit(&subnet->src_port_list);
	QListInitState(&subnet->dst_port_list);
	QListInit(&subnet->dst_port_list);
	QListInitState(&subnet->virtual_fabric_list);
	QListInit(&subnet->virtual_fabric_list);
	QListInsertTail(&subnet_list, &subnet->item);

	return FSUCCESS;
}

FSTATUS dsap_empty_subnet_list(void)
{
	LIST_ITEM *item;
	dsap_subnet_t *subnet;

	acm_log(2, "Emptying subnet list.\n");

	while ((item = QListRemoveHead(&subnet_list)) != NULL) {
		subnet = QListObj(item);
		dsap_empty_src_port_list(subnet);
		dsap_empty_dst_port_list(subnet);
		dsap_empty_virtual_fabric_list(subnet);
		QListDestroy(&subnet->src_port_list);
		QListDestroy(&subnet->dst_port_list);
		QListDestroy(&subnet->virtual_fabric_list);
		free(subnet);
	}

	return FSUCCESS;
}

void dsap_topology_cleanup(void)
{
	dsap_empty_subnet_list();
}

FSTATUS dsap_topology_init(void)
{
	QListInitState(&subnet_list);
	QListInit(&subnet_list);

	return FSUCCESS;
}
