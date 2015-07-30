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

  \file dsc_hca.c
 
  $Author: mwheinz $
  $Revision: 1.26 $
  $Date: 2015/03/30 19:33:26 $

  \brief Routines for querying HFIs
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>

/* work around conflicting names */
#include <infiniband/mad.h>
#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include "iba/public/ievent.h"
#include "iba/public/ithread.h"

#include "opasadb_debug.h"
#include "iba/ibt.h"
#include "dsc_hca.h"
#include "dsc_notifications.h"
#include "dsc_topology.h"
#include "dsc_module.h"

int hca_count;
struct ibv_device **device_list;
struct ibv_context **hca_context_list;

static int dsc_async_event_scanner_end = 0;
static THREAD dsc_async_event_scanner_thread;
static struct pollfd *pollfd_list;

void dsc_put_local_port(umad_port_t *port)
{
	umad_release_port(port);
	free(port);
	umad_done();
}

umad_port_t* dsc_get_local_port(union ibv_gid *src_gid)
{
	int ca_count;
	char ca_names[UMAD_MAX_DEVICES][UMAD_CA_NAME_LEN];
	umad_port_t *port;
	int i;

	_DBG_FUNC_ENTRY;

	if (umad_init() != 0) {
		_DBG_ERROR("Cannot initialize umad interface.\n");
		_DBG_FUNC_EXIT;
		return NULL;
	}

	ca_count = umad_get_cas_names(ca_names, UMAD_MAX_DEVICES);
	if (ca_count == 0) {
		umad_done();
		_DBG_ERROR("No HFIs found.\n");
		_DBG_FUNC_EXIT;
		return NULL;
	}

	port = malloc(sizeof(*port));
	if (port == NULL) {
		umad_done();
		_DBG_ERROR("Cannot allocate memory for port structure.\n");
		_DBG_FUNC_EXIT;
		return NULL;
	}

	for (i = 0; i < ca_count; i++) {
		umad_ca_t ca;
		int portnum;

		if (umad_get_ca(ca_names[i], &ca) != 0) {
			_DBG_ERROR("Could not get Channel Adapter %s\n", ca_names[i]);
			free(port);
			umad_done();
			_DBG_FUNC_EXIT;
			return NULL;
		}

		for (portnum = 1; portnum <= ca.numports; portnum++) {
			if (umad_get_port(ca_names[i], portnum, port) != 0) {
				_DBG_ERROR("Could not get Port %d on Channel Adapter %s\n", portnum, ca_names[i]);
				umad_release_ca(&ca);
				free(port);
				umad_done();
				_DBG_FUNC_EXIT;
				return NULL;
			}

			if ((src_gid->global.subnet_prefix == port->gid_prefix) &&
				(src_gid->global.interface_id == port->port_guid)) {
				umad_release_ca(&ca);
				_DBG_FUNC_EXIT;
				return port;
			}

			umad_release_port(port);
		}
		
		umad_release_ca(&ca);
	}

	free(port);
	umad_done();

	_DBG_FUNC_EXIT;
	return NULL;
}

void dsc_put_local_ports(umad_port_t *ports, uint32_t port_count)
{
	umad_port_t *port;
	int i;

	for (port = ports, i = 0; i < port_count; port++, i++) {
		umad_release_port(port);
	}

	free(ports);
	umad_done();
}

umad_port_t* dsc_get_local_ports(uint32_t *port_count)
{
	int ca_count;
	char ca_names[UMAD_MAX_DEVICES][UMAD_CA_NAME_LEN];
	umad_port_t *ports;
	int i;

	_DBG_FUNC_ENTRY;

	*port_count = 0;

	if (umad_init() != 0) {
		_DBG_ERROR("Cannot initialize umad interface.\n");
		_DBG_FUNC_EXIT;
		return NULL;
	}

 	ca_count = umad_get_cas_names(ca_names, UMAD_MAX_DEVICES);
	if (ca_count == 0) {
		umad_done();
		_DBG_ERROR("No HFIs found.\n");
		_DBG_FUNC_EXIT;
		return NULL;
	}

	ports = malloc(sizeof(*ports) * (UMAD_CA_MAX_PORTS * UMAD_MAX_DEVICES));
	if (ports == NULL) {
		umad_done();
		_DBG_WARN("Failed to allocate memory for ports!\n");
		_DBG_FUNC_EXIT;
		return NULL;
	}

	memset(ports, 0, sizeof(*ports) * (UMAD_CA_MAX_PORTS * UMAD_MAX_DEVICES));

	for (i = 0; i < ca_count; i++) {
		umad_ca_t ca;
		int portnum;

		if (umad_get_ca(ca_names[i], &ca) != 0) {
			_DBG_ERROR("Could not get Channel Adapter %s\n", ca_names[i]);
			dsc_put_local_ports(ports, *port_count);
			umad_done();
			_DBG_FUNC_EXIT;
			return NULL;
		}

		for (portnum = 1; portnum <= ca.numports; portnum++) {
			if (umad_get_port(ca_names[i], portnum, &ports[*port_count]) != 0) {
				_DBG_ERROR("Could not get Port %d on Channel Adapter %s\n", portnum, ca_names[i]);
				umad_release_ca(&ca);
				dsc_put_local_ports(ports, *port_count);
				umad_done();
				_DBG_FUNC_EXIT;
				return NULL;
			}
			(*port_count)++;
		}
		
		umad_release_ca(&ca);
	}
	
	_DBG_FUNC_EXIT;
	return ports;
}

FSTATUS dsc_get_port_gid(uint64_t node_guid, int port_num, union ibv_gid *gid)
{
	FSTATUS rval = FNOT_FOUND;
	char ca_names[UMAD_MAX_DEVICES][UMAD_CA_NAME_LEN];
	int ca_count;
	int i;

	_DBG_FUNC_ENTRY;

	gid->global.subnet_prefix = 0;
	gid->global.interface_id = 0;

	ca_count = umad_get_cas_names(ca_names, UMAD_MAX_DEVICES);
	if (ca_count == 0) {
		_DBG_ERROR("No Channel Adapters found\n");
		goto exit;
	}

	for (i = 0; i < ca_count; i++) {
		umad_ca_t ca;

		if (umad_get_ca(ca_names[i], &ca) != 0) {
			_DBG_ERROR("Could not get Channel Adapter %s\n", ca_names[i]);
			break;
		}

		if (ca.node_guid == node_guid) {
			umad_port_t port;

			if (umad_get_port(ca_names[i], port_num, &port) != 0) {
				_DBG_ERROR("Could not get Port %d on Channel Adapter %s\n", port_num, ca_names[i]);
				umad_release_ca(&ca);
				break;
			}

			gid->global.subnet_prefix = port.gid_prefix;
			gid->global.interface_id = port.port_guid;
			rval = FSUCCESS;
			umad_release_port(&port);
			umad_release_ca(&ca);
			break;
		}

		umad_release_ca(&ca);
	}

exit:
	_DBG_FUNC_EXIT;
	return rval;
}

/*!
  \brief Handle async events.
*/
static void dsc_handle_async_event(struct ibv_context *hca_context, uint64_t node_guid, struct ibv_async_event *async_event)
{
	union ibv_gid gid;

	_DBG_FUNC_ENTRY;

	dsc_get_port_gid(node_guid, async_event->element.port_num, &gid);

	switch (async_event->event_type) {
	case IBV_EVENT_PORT_ERR:
		_DBG_INFO("Detected that port 0x%016"PRIx64":0x%016"PRIx64" is down.\n",
				  ntohll(gid.global.subnet_prefix),
				  ntohll(gid.global.interface_id));
		dsc_port_event(gid.global.interface_id, gid.global.subnet_prefix, gid.global.interface_id, src_port_down);
		break;
	case IBV_EVENT_SM_CHANGE:
		_DBG_INFO("Detected that the SM has programmed port 0x%016"PRIx64":0x%016"PRIx64".\n",
				  ntohll(gid.global.subnet_prefix),
				  ntohll(gid.global.interface_id));
		dsc_port_event(gid.global.interface_id, gid.global.subnet_prefix, gid.global.interface_id, full_rescan);
		break;
	case IBV_EVENT_PKEY_CHANGE:
		_DBG_INFO("Detected that the pkey has changed on port 0x%016"PRIx64":0x%016"PRIx64".\n",
				  ntohll(gid.global.subnet_prefix),
				  ntohll(gid.global.interface_id));
		dsc_port_event(gid.global.interface_id, gid.global.subnet_prefix, gid.global.interface_id, full_rescan);
		break;
	case IBV_EVENT_PORT_ACTIVE:
		_DBG_INFO("Detected that port 0x%016"PRIx64":0x%016"PRIx64" is active.\n",
				  ntohll(gid.global.subnet_prefix),
				  ntohll(gid.global.interface_id));
		dsc_port_event(gid.global.interface_id, gid.global.subnet_prefix, gid.global.interface_id, src_port_up);
		break;
	case IBV_EVENT_LID_CHANGE:
		_DBG_INFO("Detected that a lid has changed for port 0x%016"PRIx64":0x%016"PRIx64".\n",
				  ntohll(gid.global.subnet_prefix),
				  ntohll(gid.global.interface_id));
		dsc_port_event(gid.global.interface_id, gid.global.subnet_prefix, gid.global.interface_id, full_rescan);
		break;
	case IBV_EVENT_CLIENT_REREGISTER:
		_DBG_INFO("Detected that a reregister request has been received for port 0x%016"PRIx64":0x%016"PRIx64".\n",
				  ntohll(gid.global.subnet_prefix),
				  ntohll(gid.global.interface_id));
		dsc_notifications_reregister_port(&gid);
		dsc_port_event(gid.global.interface_id, gid.global.subnet_prefix, gid.global.interface_id, full_rescan);
		break;
	default:
		break;
	}

	_DBG_FUNC_EXIT;
	return;
}

static void dsc_async_event_scanner(void* dummy)
{

	_DBG_FUNC_ENTRY;

	while (!dsc_async_event_scanner_end) {
		int i;
		int rval;

		for (i = 0; i < hca_count; i++) {
			pollfd_list[i].fd      = hca_context_list[i]->async_fd;
			pollfd_list[i].events  = POLLIN;
			pollfd_list[i].revents = 0;
		}

		rval = poll(pollfd_list, hca_count, 500);
		if (rval > 0) {
			for (i = 0; i < hca_count; i++) {
				if (pollfd_list[i].revents & POLLIN) {
					struct ibv_async_event event;

					ibv_get_async_event(hca_context_list[i], &event);
					dsc_handle_async_event(hca_context_list[i],ibv_get_device_guid(device_list[i]), &event);
					ibv_ack_async_event(&event);
				}
			}
		}
	}

	_DBG_FUNC_EXIT;
}

FSTATUS dsc_initialize_async_event_handler(void)
{
	int i;
	int flags;

	_DBG_FUNC_ENTRY;

	pollfd_list = (struct pollfd*)malloc(sizeof(struct pollfd) * hca_count);
	if (pollfd_list == NULL) {
		_DBG_ERROR("Unable to allocate pollfd_list.\n");
		return FERROR;
	}

	memset(pollfd_list, 0, sizeof(struct pollfd) * hca_count);

	for (i = 0; i < hca_count; i++) {
		flags = fcntl(hca_context_list[i]->async_fd, F_GETFL);
		if (fcntl(hca_context_list[i]->async_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
			_DBG_ERROR("Failed to change file descriptor of async event queue\n");
			free(pollfd_list);
			pollfd_list = NULL;
			return FERROR;
		}
	}

	ThreadInitState(&dsc_async_event_scanner_thread);
	
	if (ThreadCreate(&dsc_async_event_scanner_thread, "ibDscAsyncEvt", dsc_async_event_scanner, NULL) == FALSE) {
		_DBG_ERROR("Unable to create dsc_async_event_scanner_thread.\n");
		free(pollfd_list);
		pollfd_list = NULL;
		return FERROR;
	}

	return FSUCCESS;
}

void dsc_terminate_async_event_handler(void) 
{
	_DBG_FUNC_ENTRY;

	dsc_async_event_scanner_end = 1;
	ThreadDestroy(&dsc_async_event_scanner_thread);
	if (pollfd_list) {
		free(pollfd_list);
		pollfd_list = NULL;
	}

	_DBG_FUNC_EXIT;
}

char* dsc_get_hca_name(uint16_t hca_index)
{
	if (hca_index < hca_count) {
		return device_list[hca_index]->name;
	}

	return NULL;
}

void dsc_hca_cleanup(void)
{
	_DBG_FUNC_ENTRY;

	if (device_list) {
		if (hca_context_list) {
			int i;
			for (i = 0; i < hca_count; i++) {
				if (hca_context_list[i]) {
					ibv_close_device(hca_context_list[i]);
				}
			}

			free(hca_context_list);
			hca_context_list = NULL;
		}

		ibv_free_device_list(device_list);
		device_list = NULL;
	}

	_DBG_FUNC_EXIT;
}

FSTATUS dsc_hca_init(void)
{
	FSTATUS fstatus = FSUCCESS;
	int i;

	_DBG_FUNC_ENTRY;

	device_list = ibv_get_device_list(&hca_count);
	if (device_list == NULL) {
		_DBG_ERROR("No HFIs found.\n");
		fstatus = FERROR;
		goto exit;
	}
	
	hca_context_list = (struct ibv_context**)malloc(sizeof(struct ibv_context*) * hca_count);
	if (hca_context_list == NULL) {
		_DBG_ERROR("Unable to allocate memory for hca context list.\n");
		fstatus = FERROR;
		goto exit;
	}

	memset(hca_context_list, 0, sizeof(struct ibv_context*) * hca_count);

	for (i = 0; i < hca_count; i++) {
		hca_context_list[i] = ibv_open_device(device_list[i]);
		if (hca_context_list[i] == NULL) {
			_DBG_ERROR("Unable to open FI %s.\n", ibv_get_device_name(device_list[i]));
			fstatus = FERROR;
			goto exit;
		}
	}

exit:
	if (fstatus != FSUCCESS) {
		dsc_hca_cleanup();
	}
	_DBG_FUNC_EXIT;
	return fstatus;
}
