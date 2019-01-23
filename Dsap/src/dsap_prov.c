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

/* This file incorporates work covered by the following copyright and permission notice */

/* Part of the code is copied and modified from ibacm/prov/acmp/src/acmp.c. */
/*
 * Copyright (c) 2009-2014 Intel Corporation. All rights reserved.
 * Copyright (c) 2013 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under the OpenIB.org BSD license
 * below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AWV
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/*!

  \file dsap_main.c

  \brief Ibacm provider dsap
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define __USE_GNU
#include <search.h> 

#include <opamgt_priv.h>
#include "iba/ibt.h"
#include "opasadb_debug.h"
#include "dsap.h"
#include "dsap_topology.h"
#include "opasadb_path.h"
#include "dsap_notifications.h"

#define src_out     data[0]
#define src_index   data[1]
#define dst_index   data[2]

/* Copied from ibacm/include/acm_mad.h */
#define ACM_ADDRESS_INVALID    0x00
#define ACM_ADDRESS_NAME       0x01
#define ACM_ADDRESS_IP         0x02
#define ACM_ADDRESS_IP6        0x03
#define ACM_ADDRESS_GID        0x04
#define ACM_ADDRESS_LID        0x05
#define ACM_ADDRESS_RESERVED   0x06 

struct dsap_dest {
	uint8_t                address[ACM_MAX_ADDRESS]; 
	char                   name[ACM_MAX_ADDRESS];
	union ibv_gid          gid;
	uint8_t                addr_type;
};

static void * dest_map[ACM_ADDRESS_RESERVED - 1];

QUICK_LIST dsap_dev_list;
SPIN_LOCK dsap_dev_lock;

static char log_data[ACM_MAX_ADDRESS];

static char config_file[128] = "/etc/rdma/dsap.conf";
static char addr_data_file[128] = "/etc/rdma/ibacm_hosts.data";
static int dsap_initialized;

static int dsap_open_dev(const struct acm_device *device, void **dev_context);
static void dsap_close_dev(void *dev_context);
static int dsap_open_port(const struct acm_port *port, void *dev_context,
			  void **port_context);
static void dsap_close_port(void *port_context);
static int dsap_open_endpoint(const struct acm_endpoint *endpoint, 
			      void *port_context, void **ep_context);
static void dsap_close_endpoint(void *ep_context);
static int dsap_add_addr(const struct acm_address *addr, void *ep_context, 
			 void **addr_context);
static void dsap_remove_addr(void *addr_context);
static int dsap_resolve(void *addr_context, struct acm_msg *msg, uint64_t id);
static int dsap_query(void *addr_context, struct acm_msg *msg, uint64_t id);
static int dsap_handle_event(void *port_context, enum ibv_event_type type);
static void dsap_query_perf(void *ep_context, uint64_t *values, uint8_t *cnt);

static struct acm_provider def_prov = {
	.size = sizeof(struct acm_provider),
	.version = 1,
	.name = "dsap",
	.open_device = dsap_open_dev,
	.close_device = dsap_close_dev,
	.open_port = dsap_open_port,
	.close_port = dsap_close_port,
	.open_endpoint = dsap_open_endpoint,
	.close_endpoint = dsap_close_endpoint,
	.add_address = dsap_add_addr,
	.remove_address = dsap_remove_addr,
	.resolve = dsap_resolve,
	.query = dsap_query,
	.handle_event = dsap_handle_event,
	.query_perf = dsap_query_perf,
};

static int dsap_compare_dest(const void *dest1, const void *dest2)
{
	return memcmp(dest1, dest2, ACM_MAX_ADDRESS);
}

static void
dsap_set_dest_addr(struct dsap_dest *dest, uint8_t addr_type,
		   const uint8_t *addr, size_t size)
{
	memcpy(dest->address, addr, size);
	dest->addr_type = addr_type;
	acm_format_name(0, dest->name, sizeof dest->name, addr_type, addr,
			size);
}

static void
dsap_init_dest(struct dsap_dest *dest, uint8_t addr_type,
	       const uint8_t *addr, size_t size)
{
	if (size)
		dsap_set_dest_addr(dest, addr_type, addr, size);
}

static struct dsap_dest *
dsap_alloc_dest(uint8_t addr_type, const uint8_t *addr)
{
	struct dsap_dest *dest;

	dest = calloc(1, sizeof *dest);
	if (!dest) {
		acm_log(0, "ERROR - unable to allocate dest\n");
		return NULL;
	}

	dsap_init_dest(dest, addr_type, addr, ACM_MAX_ADDRESS);
	acm_log(1, "%s\n", dest->name);
	return dest;
}

static struct dsap_dest *
dsap_get_dest(uint8_t addr_type, const uint8_t *addr)
{
	struct dsap_dest *dest, **tdest;

	tdest = tfind(addr, &dest_map[addr_type - 1], dsap_compare_dest);
	if (tdest) {
		dest = *tdest;
		acm_log(2, "%s\n", dest->name);
	} else {
		dest = NULL;
		acm_format_name(2, log_data, sizeof log_data,
				addr_type, addr, ACM_MAX_ADDRESS);
		acm_log(2, "%s not found\n", log_data);
	}
	return dest;
}

static struct dsap_dest *
dsap_acquire_dest(uint8_t addr_type, const uint8_t *addr)
{
	struct dsap_dest *dest;

	acm_format_name(2, log_data, sizeof log_data,
			addr_type, addr, ACM_MAX_ADDRESS);
	acm_log(2, "%s\n", log_data);
	dest = dsap_get_dest(addr_type, addr);
	if (!dest) {
		dest = dsap_alloc_dest(addr_type, addr);
		if (dest) 
			tsearch(dest, &dest_map[addr_type - 1],
				dsap_compare_dest);
	}
	return dest;
}

static void dsap_init_port(struct dsap_port *port, 
			       struct dsap_device *dev, uint8_t port_num)
{
	acm_log(1, "%s %d\n", dev->device->verbs->device->name, port_num);
	port->dev = dev;
	SpinLockInitState(&port->lock);
	SpinLockInit(&port->lock);
	QListInitState(&port->ep_list);
	QListInit(&port->ep_list);
	port->state = IBV_PORT_DOWN;
}

static int dsap_open_dev(const struct acm_device *device, void **dev_context)
{
	struct dsap_device *dev;
	size_t size;
	struct ibv_device_attr attr;
	int i, ret;

	acm_log(1, "dev_guid 0x%"PRIx64" %s\n", ntoh64(device->dev_guid),
		device->verbs->device->name);

	ret = ibv_query_device(device->verbs, &attr);
	if (ret) {
		acm_log(0, "ERROR - ibv_query_device (%s) %d\n", 
			device->verbs->device->name, ret);
		goto err;
	}
	
	size = sizeof(*dev) + sizeof(struct dsap_port) * attr.phys_port_cnt;
	dev = (struct dsap_device *) calloc(1, size);
	if (!dev)
		goto err;

	dev->device = device;
	dev->port_cnt = attr.phys_port_cnt;

	for (i = 0; i < dev->port_cnt; i++) {
		dsap_init_port(&dev->port[i], dev, i + 1);
	}

	ListItemInitState(&dev->item);
	QListSetObj(&dev->item, dev);
	SpinLockAcquire(&dsap_dev_lock);
	QListInsertTail(&dsap_dev_list, &dev->item);
	SpinLockRelease(&dsap_dev_lock);
	*dev_context = dev;

	acm_log(1, "%s opened\n", device->verbs->device->name);
	return 0;
err:
	return -1;
}

static void dsap_close_dev(void *dev_context)
{
	struct dsap_device *dev = dev_context;

	acm_log(1, "dev_guid 0x%"PRIx64"\n", ntoh64(dev->device->dev_guid));
	SpinLockAcquire(&dsap_dev_lock);
	QListRemoveItem(&dsap_dev_list, &dev->item);
	SpinLockRelease(&dsap_dev_lock);
	free(dev);
}


static void dsap_port_up(struct dsap_port *port)
{
	struct ibv_port_attr attr;
	uint16_t pkey;
	int i, ret;

	acm_log(1, "%s %d\n", port->dev->device->verbs->device->name,
		port->port->port_num);
	ret = ibv_query_port(port->dev->device->verbs, port->port->port_num,
			     &attr);
	if (ret) {
		acm_log(0, "ERROR - unable to get port attribute\n");
		return;
	}

	port->lid = attr.lid;
	port->lmc = attr.lmc;
	port->lid_mask = 0xffff - ((1 << attr.lmc) - 1);
	port->sm_lid = attr.sm_lid;

	for (i = 0; i < attr.pkey_tbl_len; i++) {
		ret = ibv_query_pkey(port->dev->device->verbs,
				     port->port->port_num, i, &pkey);
		if (ret)
			continue;
		pkey = ntohs(pkey);
		if (!(pkey & 0x7fff))
			continue;

		/* Determine pkey index for default partition with preference
		 * for full membership
		 */
		if ((pkey & 0x7fff) == 0x7fff) {
			port->default_pkey_ix = i;
			break;
		}
	}

	/* Start the notification thread */
	if (dsap_notification_register_port(port) != FSUCCESS) {
		dsap_terminate_notification(port);
		acm_log(0, "Failed to register port for notification\n");
		return;
	}

	/*
	 * open the Libopasadb library for this port. If it is not 
	 * available yet, we will try to open it at the first access 
	 * to the database. 
	 * Note: by this time, the first scan may have not been 
	 *       started yet and any attempt to open the libopasadb
	 *       will fail. Hold on until the first request comes in.
	 */
	/*port->lib_handle = op_path_open(port->dev->device->verbs->device, 
					port->port->port_num);
	if (!port->lib_handle) 
		acm_log(1, "Failed to open libopasadb\n"); */

	SpinLockAcquire(&port->lock);
	port->state = IBV_PORT_ACTIVE;
	SpinLockRelease(&port->lock);
	acm_log(1, "%s %d is up\n", port->dev->device->verbs->device->name,
		port->port->port_num);
}

static int dsap_open_port(const struct acm_port *port, void *dev_context,
			  void **port_context)
{
	struct dsap_device *dev = dev_context;
	struct dsap_port *dport;

	if (port->port_num < 1 || port->port_num > dev->port_cnt) {
		acm_log(0, "Error: port_num %d is out of range (max %d)\n",
			port->port_num, dev->port_cnt);
		return -1;
	}

	dport = &dev->port[port->port_num - 1];
	SpinLockAcquire(&dport->lock);
	dport->port = port;
	dport->state = IBV_PORT_DOWN;
	SpinLockRelease(&dport->lock);
	dsap_port_up(dport);
	*port_context = dport;
	/* Theoretically we should send an event to the scanner thread to
	 * announce our presence. However, since the scanner needs the
	 * pkeys, we will delay the announcement until the first 
	 * endpoint is opened. */ 

	return 0;
}

static void dsap_close_port(void *port_context)
{
	struct dsap_port *port = port_context;
	union ibv_gid gid;

	acm_log(1, "%s %d\n", port->dev->device->verbs->device->name,
		port->port->port_num);
	/* Send an event to the scanner */
	if (!acm_get_gid((struct acm_port *) port->port, 0, &gid)) {
		dsap_port_event(gid.global.interface_id,
				gid.global.subnet_prefix,
				gid.global.interface_id,
				DSAP_PT_EVT_SRC_PORT_DOWN);
	}

	if (port->lib_handle) {
		op_path_close(port->lib_handle);
		port->lib_handle = NULL;
	}
	SpinLockAcquire(&port->lock);
	/* Terminate the notification thread */
	dsap_terminate_notification(port);
	port->state = IBV_PORT_DOWN;
	port->port = NULL;
	SpinLockRelease(&port->lock);
}

static struct dsap_ep *
dsap_alloc_ep(struct dsap_port *port, struct acm_endpoint *endpoint)
{
	struct dsap_ep *ep;

	acm_log(1, "\n");
	ep = calloc(1, sizeof *ep);
	if (!ep)
		return NULL;

	ep->port = port;
	ep->endpoint = endpoint;
	snprintf(ep->id_string, ACM_MAX_ADDRESS, "%s-%d-0x%x", 
		port->dev->device->verbs->device->name,
		port->port->port_num, endpoint->pkey);

	return ep;
}

static int dsap_open_endpoint(const struct acm_endpoint *endpoint, 
			      void *port_context, void **ep_context)
{
	struct dsap_port *port = port_context;
	struct dsap_ep *ep;
	union ibv_gid gid;

	acm_log(2, "creating endpoint for pkey 0x%x\n", endpoint->pkey);
	ep = dsap_alloc_ep(port, (struct acm_endpoint *) endpoint);
	if (!ep)
		return -1;

	snprintf(ep->id_string, ACM_MAX_ADDRESS, "%s-%d-0x%x", 
		port->dev->device->verbs->device->name,
		port->port->port_num, endpoint->pkey);

	ListItemInitState(&ep->item);
	QListSetObj(&ep->item, ep);
	SpinLockAcquire(&port->lock);
	QListInsertTail(&port->ep_list, &ep->item);
	SpinLockRelease(&port->lock);
	*ep_context = (void *) ep;

	/* Now send an event to the scanner: only once per port */
	if (!acm_get_gid((struct acm_port *) port->port, 0, &gid))
		dsap_port_event(gid.global.interface_id,
				gid.global.subnet_prefix,
				gid.global.interface_id,
				DSAP_PT_EVT_SRC_PORT_UP);

	return 0;
}

static void dsap_close_endpoint(void *ep_context)
{
	struct dsap_ep *ep = ep_context;

	acm_log(1, "%s %d pkey 0x%04x\n", 
		ep->port->dev->device->verbs->device->name, 
		ep->port->port->port_num, ep->endpoint->pkey);
	SpinLockAcquire(&ep->port->lock);
	QListRemoveItem(&ep->port->ep_list, &ep->item);
	SpinLockRelease(&ep->port->lock);

	free(ep);
}

static int dsap_add_addr(const struct acm_address *addr, void *ep_context,
 		     void **addr_context)
{
	struct dsap_ep *ep = ep_context;
	int i;

	acm_log(2, "\n");

	for (i = 0; (i < MAX_EP_ADDR) &&
	     (ep->addr_info[i].type != ACM_ADDRESS_INVALID); i++)
		;

	if (i == MAX_EP_ADDR) {
		acm_log(0, "ERROR - no more space for local address\n");
		return -1;
	}
	ep->addr_info[i].type = addr->type;
	memcpy(&ep->addr_info[i].info, &addr->info, sizeof(addr->info));
	ep->addr_info[i].addr = (struct acm_address *) addr;
	ep->addr_info[i].ep = ep;
	*addr_context = &ep->addr_info[i];

	return 0;
}

static void dsap_remove_addr(void *addr_context)
{
	struct dsap_addr *address = addr_context;

	acm_log(2, "\n");
	memset(address, 0, sizeof(*address));
}

static int
dsap_resolve_response(uint64_t id, struct acm_msg *req_msg,
		      struct dsap_ep *ep, uint8_t status)
{
	struct acm_msg msg;

	acm_log(2, "client %"PRId64", status 0x%x\n", id, status);
	memset(&msg, 0, sizeof msg);

	if (ep) {
		if (status == ACM_STATUS_ENODATA)
			ep->counters[ACM_CNTR_NODATA]++;
		else if (status)
			ep->counters[ACM_CNTR_ERROR]++;
	}
	msg.hdr = req_msg->hdr;
	msg.hdr.status = status;
	msg.hdr.length = ACM_MSG_HDR_LENGTH;
	memset(msg.hdr.data, 0, sizeof(msg.hdr.data));

	if (status == ACM_STATUS_SUCCESS) {
		msg.hdr.length += ACM_MSG_EP_LENGTH;
		msg.resolve_data[0].flags = IBV_PATH_FLAG_GMP |
			IBV_PATH_FLAG_PRIMARY | IBV_PATH_FLAG_BIDIRECTIONAL;
		msg.resolve_data[0].type = ACM_EP_INFO_PATH;
		msg.resolve_data[0].info.path = 
			req_msg->resolve_data[0].info.path;

		if (req_msg->hdr.src_out) {
			msg.hdr.length += ACM_MSG_EP_LENGTH;
			memcpy(&msg.resolve_data[1],
				&req_msg->resolve_data[req_msg->hdr.src_index],
				ACM_MSG_EP_LENGTH);
		}
	}

	return acm_resolve_response(id, &msg);
}

static int
dsap_resolve_dest(struct dsap_ep *ep, struct acm_msg *msg, uint64_t id)
{
	struct dsap_dest *dest;
	struct acm_ep_addr_data *daddr;
	uint8_t status = ACM_STATUS_SUCCESS;
	struct ibv_path_record *path;
	struct ibv_path_record query;

	daddr = &msg->resolve_data[msg->hdr.dst_index];
	acm_format_name(2, log_data, sizeof log_data,
			daddr->type, daddr->info.addr,
			sizeof daddr->info.addr);
	acm_log(2, "dest %s\n", log_data);

	dest = dsap_get_dest(daddr->type, daddr->info.addr);
	if (!dest) {
		acm_log(0, "ERROR - unable to get destination in request\n");
		status = ACM_STATUS_ENODATA;
		goto resp;
	}

	path = &msg->resolve_data[0].info.path;
	memset(&query, 0, sizeof(query));
	query.slid = htons(ep->port->lid);
	query.pkey = htons(ep->endpoint->pkey);
	acm_get_gid((struct acm_port *) ep->port->port, 0, &query.sgid);
	memcpy(&query.dgid, &dest->gid, sizeof(query.dgid));
	acm_log(2, "slid %04x pkey %04x sgid %"PRIx64":%"PRIx64" dgid %"PRIx64":%"PRIx64"\n",
		ntohs(query.slid), ntohs(query.pkey),
		ntoh64(query.sgid.global.subnet_prefix),
		ntoh64(query.sgid.global.interface_id),
		ntoh64(query.dgid.global.subnet_prefix),
		ntoh64(query.dgid.global.interface_id));

	/* If we have not opened libopasadb previously, do it now */
	if (!ep->port->lib_handle) {
		ep->port->lib_handle = 
			op_path_open(ep->port->dev->device->verbs->device,
				     ep->port->port->port_num);
		if (!ep->port->lib_handle) {
			acm_log(0, "Error -- Failed to open libopasadb\n");
			status = ACM_STATUS_ENODATA;
			goto resp;
		}
	}

	if (op_path_get_path_by_rec(ep->port->lib_handle,
				    (op_path_rec_t *) &query,
				    (op_path_rec_t *) path)) {
		acm_log(0, "Error -- Failed to get path record\n");
		status = ACM_STATUS_ENODATA;
	}

resp:
	return dsap_resolve_response(id, msg, ep, status);
}

static int
dsap_resolve_path(struct dsap_ep *ep, struct acm_msg *msg, uint64_t id)
{
	struct ibv_path_record *path;
	uint8_t status = ACM_STATUS_SUCCESS;
	struct ibv_path_record query;

	path = &msg->resolve_data[0].info.path;
	if (!path->slid && ib_any_gid(&path->sgid)) {
		path->slid = htons(ep->port->lid);
		acm_get_gid((struct acm_port *) ep->port->port, 0, 
			    &path->sgid);
	}

	/* If we have not opened libopasadb previously, do it now */
	if (!ep->port->lib_handle) {
		ep->port->lib_handle = 
			op_path_open(ep->port->dev->device->verbs->device,
				     ep->port->port->port_num);
		if (!ep->port->lib_handle) {
			acm_log(0, "Error -- Failed to open libopasadb\n");
			status = ACM_STATUS_ENODATA;
			goto resp;
		}
	}

	memcpy(&query, path, sizeof(query));
	/* The query requires at least one of them be present */
	if (!query.pkey && !query.service_id)
		query.pkey = htons(ep->endpoint->pkey);
	if (op_path_get_path_by_rec(ep->port->lib_handle,
				    (op_path_rec_t *) &query,
				    (op_path_rec_t *) path)) {
		acm_log(0, "Error -- Failed to get path record\n");
		status = ACM_STATUS_ENODATA;
	}

resp:
	return dsap_resolve_response(id, msg, ep, status);
}

static int dsap_resolve(void *addr_context, struct acm_msg *msg, uint64_t id)
{
	struct dsap_addr *address = addr_context;
	struct dsap_ep *ep = address->ep;

	ep->counters[ACM_CNTR_RESOLVE]++;
	if (msg->resolve_data[0].type == ACM_EP_INFO_PATH)
		return dsap_resolve_path(ep, msg, id);
	else
		return dsap_resolve_dest(ep, msg, id);
}

static int dsap_query(void *addr_context, struct acm_msg *msg, uint64_t id)
{
	struct dsap_addr *address = addr_context;
	struct dsap_ep *ep = address->ep;
	uint8_t status = ACM_STATUS_SUCCESS;
	struct ibv_path_record *path;
	struct ibv_path_record query;

	acm_log(2, "\n");

	acm_increment_counter(ACM_CNTR_ROUTE_QUERY);
	ep->counters[ACM_CNTR_ROUTE_QUERY]++;
	/* If we have not opened libopasadb previously, do it now */
	if (!ep->port->lib_handle) {
		ep->port->lib_handle = 
			op_path_open(ep->port->dev->device->verbs->device,
				     ep->port->port->port_num);
		if (!ep->port->lib_handle) {
			acm_log(0, "Error -- Failed to open libopasadb\n");
			status = ACM_STATUS_ENODATA;
			goto resp;
		}
	}

	path = &msg->resolve_data[0].info.path;
	memcpy(&query, path, sizeof(query));
	if (op_path_get_path_by_rec(ep->port->lib_handle, 
				    (op_path_rec_t *) &query,
				    (op_path_rec_t *) path)) {
		acm_log(0, "Error -- Failed to get path record\n");
		status = ACM_STATUS_ENODATA;
	}

resp:
	msg->hdr.opcode |= ACM_OP_ACK;
	msg->hdr.status = status;
	if (status == ACM_STATUS_ENODATA)
		ep->counters[ACM_CNTR_NODATA]++;
	else if (status) 
		ep->counters[ACM_CNTR_ERROR]++;
	return acm_query_response(id, msg);
}

static int dsap_handle_event(void *port_context, enum ibv_event_type type)
{
	struct dsap_port *port = port_context;
	union ibv_gid gid;

	acm_log(2, "Event %d\n", type);

	/* Only some events are passed to us */
	switch (type) 
	{
	case IBV_EVENT_CLIENT_REREGISTER:
		if (dsap_notification_reregister_port(port) != FSUCCESS) {
			acm_log(0, "Failed to re-register port  %s/%d for "
				   "notification\n",
				port->dev->device->verbs->device->name,
				port->port->port_num);
			return -1;
		}
		acm_get_gid((struct acm_port *) port->port, 0, &gid);
		dsap_port_event(gid.global.interface_id,
				gid.global.subnet_prefix,
				gid.global.interface_id,
				DSAP_PT_EVT_FULL_RESCAN);
		break;
	case IBV_EVENT_SM_CHANGE:
		acm_get_gid((struct acm_port *) port->port, 0, &gid);
		dsap_port_event(gid.global.interface_id,
				gid.global.subnet_prefix,
				gid.global.interface_id,
				DSAP_PT_EVT_FULL_RESCAN);
		break;
	default:
		/* Ignore */
		break;
	}
	return 0;
}

static void dsap_query_perf(void *ep_context, uint64_t *values, uint8_t *cnt)
{
	struct dsap_ep *ep = ep_context;
	int i;

	for (i = 0; i < ACM_MAX_COUNTER; i++)
		values[i] = hton64(ep->counters[i]);
	*cnt = ACM_MAX_COUNTER;
}

static void dsap_parse_hosts_file(void)
{
	FILE *f;
	char s[120];
	char addr[INET6_ADDRSTRLEN], gid[INET6_ADDRSTRLEN];
	uint8_t name[ACM_MAX_ADDRESS];
	struct in6_addr ip_addr, ib_addr;
	struct dsap_dest *dest;
	uint8_t addr_type;

	if (!(f = fopen(addr_data_file, "r"))) {
		acm_log(0, "ERROR - couldn't open %s\n", addr_data_file);
		return;
	}

	while (fgets(s, sizeof s, f)) {
		if (s[0] == '#')
			continue;

		if (sscanf(s, "%46s%46s", addr, gid) != 2)
			continue;

		acm_log(2, "%s", s);
		if (inet_pton(AF_INET6, gid, &ib_addr) <= 0) {
			acm_log(0, "ERROR - %s is not IB GID\n", gid);
			continue;
		}
		memset(name, 0, ACM_MAX_ADDRESS);
		if (inet_pton(AF_INET, addr, &ip_addr) > 0) {
			addr_type = ACM_ADDRESS_IP;
			memcpy(name, &ip_addr, 4);
		} else if (inet_pton(AF_INET6, addr, &ip_addr) > 0) {
			addr_type = ACM_ADDRESS_IP6;
			memcpy(name, &ip_addr, sizeof(ip_addr));
		} else {
			addr_type = ACM_ADDRESS_NAME;
			strncpy((char *)name, addr, ACM_MAX_ADDRESS);
		}

		dest = dsap_acquire_dest(addr_type, name);
		if (!dest) {
			acm_log(0, "ERROR - unable to create dest %s\n", addr);
			continue;
		}
		memcpy(&dest->gid, &ib_addr, sizeof(ib_addr));

		acm_log(1, "added host %s address type %d IB GID %s\n",
			addr, addr_type, gid);
	}

	fclose(f);
}

void dsap_free_map_node(void *nodep)
{
    struct dsap_dest *dest = nodep;

    free(dest);
}

static void dsap_remove_mapping(void)
{
	int i;

	for (i = 0; i < (ACM_ADDRESS_RESERVED - 1); i++) {
		if (dest_map[i]) 
			tdestroy(dest_map[i], dsap_free_map_node);
	}
}

/* Get ibacm configuration options related to dist_sap */
static void dsap_set_options(void)
{
	FILE *f;
	char s[120];
	char opt[32], value[256];
	const char *opts_file = acm_get_opts_file();

	if (!(f = fopen(opts_file, "r")))
		return;

	while (fgets(s, sizeof s, f)) {
		if (s[0] == '#')
			continue;

		memset(opt, 0, 32);
		memset(value, 0, 256);
		if (sscanf(s, "%32s%256s", opt, value) != 2)
			continue;

		if (!strcasecmp("dsap_conf_file", opt)) {
			strncpy(config_file, value, 127);
			config_file[127] =  '\0';
		}
		else if (!strcasecmp("addr_data_file", opt)) {
			strncpy(addr_data_file, value, 127);
			addr_data_file[127] = '\0';
		}
	}

	fclose(f);
}

static void dsap_log_options(void)
{
	acm_log(0, "dsap config file %s\n", config_file);
	acm_log(0, "address data file %s\n", addr_data_file);
}

static void __attribute__((constructor)) dsap_prov_init(void)
{
	int i;

	dsap_set_options();
	dsap_log_options();
	dsap_get_config(config_file);

	QListInitState(&dsap_dev_list);
	QListInit(&dsap_dev_list);
	SpinLockInitState(&dsap_dev_lock);
	SpinLockInit(&dsap_dev_lock);
	for (i = 0; i < (ACM_ADDRESS_RESERVED - 1); i++) 
		dest_map[i] = NULL;

	if (dsap_init())
		return;
	dsap_parse_hosts_file();
	dsap_initialized = 1;
}

static void __attribute__((destructor)) dsap_prov_exit(void)
{
	acm_log(1, "Unloading...\n");

	dsap_remove_mapping();
	dsap_cleanup();

	dsap_initialized = 0;
}

int provider_query(struct acm_provider **provider, uint32_t *version)
{
	acm_log(1, "\n");

	if (!dsap_initialized)
		return -1;

	if (provider)
		*provider = &def_prov;
	if (version)
		*version = 1;

	return 0;
}

/* Get the provider port and hold the port lock. The caller
   must call dsap_release_prov_port() to release the port
   lock if this call succeeds */
struct dsap_port * dsap_lock_prov_port(dsap_src_port_t *src_port)
{
	LIST_ITEM * dev_item;
	struct dsap_device *dev;
	struct dsap_port *port = NULL;
	int i;
	union ibv_gid gid;
	
	acm_log(2, "port %s/%d\n", src_port->hfi_name, src_port->port_num);

	/* Protect against device close */
	SpinLockAcquire(&dsap_dev_lock);
	for_each(&dsap_dev_list, dev_item) {
		dev = QListObj(dev_item);

		for (i = 0; i < dev->port_cnt; i++) {
			port = &dev->port[i];
			SpinLockAcquire(&port->lock);
			if (port->state != IBV_PORT_ACTIVE)
				goto next_port;
			if (acm_get_gid((struct acm_port *) port->port, 0,
					&gid))
				goto next_port;
			if (!memcmp(&gid, &src_port->gid, sizeof(gid)))
				goto get_exit;
		next_port:
			SpinLockRelease(&port->lock);
			port = NULL;
		}
	}
get_exit:
	SpinLockRelease(&dsap_dev_lock);

	return port;
}

void dsap_release_prov_port(struct dsap_port *port)
{
	acm_log(2, "port %s/%d\n", port->dev->device->verbs->device->name,
		port->port->port_num);

	SpinLockRelease(&port->lock);
}

FSTATUS dsap_add_src_ports(void)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM * dev_item;
	struct dsap_device *dev;
	struct dsap_port *port;
	int i;
	
	acm_log(2, "\n");

	/* Protect against device close */
	SpinLockAcquire(&dsap_dev_lock);
	for_each(&dsap_dev_list, dev_item) {
		dev = QListObj(dev_item);

		for (i = 0; i < dev->port_cnt; i++) {
			port = &dev->port[i];
			SpinLockAcquire(&port->lock);
			if (dev->port[i].state != IBV_PORT_ACTIVE)
				goto next_port;

			rval = dsap_add_src_port(port);
			if (rval != FSUCCESS) {
				SpinLockRelease(&port->lock);
				goto src_exit;
			}
		next_port:
			SpinLockRelease(&port->lock);
		}
	}
src_exit:
	SpinLockRelease(&dsap_dev_lock);

	return rval;
}
