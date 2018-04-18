/* BEGIN_ICS_COPYRIGHT5 ****************************************

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

 * ** END_ICS_COPYRIGHT5   ****************************************/
/* [ICS VERSION STRING: unknown] */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <infiniband/umad.h>
#include <infiniband/verbs.h>
#include "ib_notice_net.h"
#include "opamgt_sa_notice.h"
#include "dsap.h"
#include "dsap_notifications.h"

typedef enum {
	DSAP_NOTIFY_NO_ACTION,
	DSAP_NOTIFY_REREGISTER,
	DSAP_NOTIFY_REINITIALIZE
} notify_t;

uint32 dsap_no_subscribe = 0;

static pthread_mutex_t m_context = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m_reinitialize = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t c_reinitialize;

static int report_notice_callback(uint16_t trap_num, IB_GID *gid, struct dsap_port *port);

static int dsap_handle_event(struct dsap_port *port, STL_NOTICE *notice, size_t len,
	void *context, OMGT_STATUS_T status)
{
	struct dsap_port *evt_port = context;
	int action = DSAP_NOTIFY_NO_ACTION;
	STL_TRAP_GID *data_details;
	IB_GID gid;

	if (status == OMGT_STATUS_SUCCESS) {
		if (port != evt_port) {
			/* Verify context matches */
			acm_log(1, "Received an invalid context pointer from ib_usa: 0x%p\n",
				context);
			if (notice) free(notice);
			return action;
		}
		/* NOTICE */
		data_details = (STL_TRAP_GID *)notice->Data;

		acm_log(2, "NOTICE: source lid = %d, status = %d, attr_size = %lu, context = %p\n",
			notice->IssuerLID, status, len, evt_port);
		memcpy(gid.Raw, data_details->Gid.Raw, sizeof(gid.Raw));
		action = report_notice_callback(notice->Attributes.Generic.TrapNumber,
			&gid, evt_port);
		free(notice);
	} else if (status == OMGT_STATUS_DISCONNECT) {
		/* INFORM INFO */
		acm_log(2, "INFORM_INFO: Timeout\n\n");
		acm_log(0, "Port %u has timed out trying to contact with the SM.\n",
			port->port->port_num);

		action = DSAP_NOTIFY_REINITIALIZE;
	} else {
		acm_log(2, "Unhandled notification: status = %u\n", status);
	}
	return action;
}

static void dsap_take_action(int action)
{
	struct timeval now;
	struct timespec delay;

	switch (action) {
	case DSAP_NOTIFY_NO_ACTION:
		break;
	case DSAP_NOTIFY_REREGISTER:
		break;
	case DSAP_NOTIFY_REINITIALIZE:
		if (pthread_mutex_trylock(&m_reinitialize)) {
			/* This should be impossible. */
			acm_log(1, "Discarding nested re-initialize event.\n");
			break;
		}

		gettimeofday(&now,NULL);
		delay.tv_sec = now.tv_sec + dsap_unsub_scan_frequency;
		delay.tv_nsec = 0;
		
		acm_log(1, "Re-initializing in %u seconds.\n", 
			(unsigned int)(delay.tv_sec - now.tv_sec));

		pthread_cond_timedwait(&c_reinitialize, &m_reinitialize, 
				       &delay);

		dsap_reinitialize_notifications();
		pthread_mutex_unlock(&m_reinitialize);
		break;
	}
}

static void * dsap_notification_event_thread(void *arg)
{
	struct dsap_port *port;
	int action;
	STL_NOTICE *notice = NULL;
	size_t notice_len = 0;
	OMGT_STATUS_T status;
	void *context = NULL;

	acm_log(2, "\n");

	if (arg == NULL) {
		acm_log(0, "Port context empty.\n");
		pthread_exit (arg);
		return arg;
	}
	port = arg;

	if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)) {
		acm_log(0, "Failed to set cancel type \n");
		pthread_exit(NULL);
		return NULL;
	}
	if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)) {
		acm_log(0, "Failed to set cancel state\n");
		pthread_exit(NULL);
		return NULL;
	}

	while (!port->terminating) {
		pthread_testcancel();
		action = DSAP_NOTIFY_NO_ACTION;
		status = omgt_sa_get_notice_report(port->omgt_handle, &notice, &notice_len,
			&context, -1);

		action = dsap_handle_event(port, notice, notice_len, context, status);
		
		dsap_take_action(action);
	}

	pthread_exit (arg);
	return arg;
}

void dsap_terminate_notification(struct dsap_port *port)
{
	acm_log(2, "\n");

	if (port->notice_started) {
		port->terminating = 1;
		pthread_cancel(port->notice_thread);
		pthread_join(port->notice_thread, NULL);
		port->notice_started = 0;
	}
	

	if (port->omgt_handle != NULL) {
		omgt_close_port(port->omgt_handle);
		port->omgt_handle = NULL;
	}
}

static int
report_notice_callback(uint16_t trap_num, IB_GID *gid, struct dsap_port *port)
{
	char gid_str[INET6_ADDRSTRLEN];
	union ibv_gid src_gid;

	acm_log(2, "\n");
	if (acm_get_gid((struct acm_port *) port->port, 0, &src_gid)) {
		acm_log(0, "Failed to get src gid\n");
		goto cb_exit;
	}

	switch (trap_num)
	{
	case IBV_SA_SM_TRAP_GID_IN_SERVICE:
		acm_log(1, "Received GID Now In Service = 0x%016"PRIx64"/%s\n",
			ntoh64(src_gid.global.interface_id),
			inet_ntop(AF_INET6, gid->Raw, gid_str, sizeof gid_str));
		dsap_port_event(src_gid.global.interface_id,
				src_gid.global.subnet_prefix,
				gid->Type.Global.InterfaceID,
				DSAP_PT_EVT_DST_PORT_UP);
		break;
	case IBV_SA_SM_TRAP_GID_OUT_OF_SERVICE:
		acm_log(1, "Received GID Out Of Service = 0x%016"PRIx64"/%s\n", 
			ntoh64(src_gid.global.interface_id), 
			inet_ntop(AF_INET6, gid->Raw, gid_str, sizeof gid_str));
		dsap_port_event(src_gid.global.interface_id,
				src_gid.global.subnet_prefix,
				gid->Type.Global.InterfaceID,
				DSAP_PT_EVT_DST_PORT_DOWN);
		break;
	default:
		acm_log(0, "Received Unknown Notice = %u. Gid = %016"PRIx64"/%s\n", 
			trap_num, src_gid.global.interface_id, 
			inet_ntop(AF_INET6, gid->Raw, gid_str, sizeof gid_str));
		break;
	}

cb_exit:
	return DSAP_NOTIFY_NO_ACTION;
}

FSTATUS dsap_notification_register_port(struct dsap_port *port)
{
	int rval;
	union ibv_gid gid;

	acm_log(2, "port %s/%d\n", port->dev->device->verbs->device->name,
		port->port->port_num);

	/* open the opamgt port object */
	if (acm_get_gid((struct acm_port *) port->port, 0, &gid)) {
		acm_log(0, "Failed to get gid\n");
		goto error;
	}
	if ((rval = omgt_open_port_by_guid(&port->omgt_handle,
					   ntoh64(gid.global.interface_id), NULL))) {
		acm_log(0, "Cannot open opamgt port object. (%d)\n", rval);
		goto error;
	}

	//set logging
	dsap_omgt_log_init(port->omgt_handle);

	if (dsap_no_subscribe)
		return FSUCCESS;

	/* spawn the notification thread before registering for the traps */
	port->terminating = 0;
	if (pthread_create(&port->notice_thread, NULL,
			   dsap_notification_event_thread, port)) {
		acm_log(0, "Failed to create the notification thread\n");
		goto error;
	}

	rval = omgt_sa_register_trap(port->omgt_handle,
				    ntoh16(IBV_SA_SM_TRAP_GID_IN_SERVICE),
				    port);
	if (rval) {
		acm_log(0, "Cannot subscribe for Trap GID_IN_SERVICE. (%d)\n",
			rval);
		goto error1;
	}

	rval = omgt_sa_register_trap(port->omgt_handle,
				    ntoh16(IBV_SA_SM_TRAP_GID_OUT_OF_SERVICE),
				    port);
	if (rval) {
		acm_log(0, "Cannot subscrb for Trap GID_OUT_OF_SERVICE.(%d)\n",
			rval);
		goto error1;
	}
	port->notice_started = 1;

	return FSUCCESS;

error1:
	port->terminating = 1;
	pthread_cancel(port->notice_thread);
	pthread_join(port->notice_thread, NULL);
error:
	port->notice_started = 0;

	return FERROR;
}

FSTATUS dsap_notification_reregister_port(struct dsap_port *port)
{
	FSTATUS fstatus = FSUCCESS;
	int rval;

	acm_log(2, "\n");

	if (dsap_no_subscribe)
		return FSUCCESS;

	pthread_mutex_lock(&m_context);
	if (!port || !port->omgt_handle) {
		fstatus = FERROR;
		acm_log(0, "Invalid parameters.\n");
		goto error;
	}

	rval = omgt_sa_register_trap(port->omgt_handle, 
				    ntoh16(IBV_SA_SM_TRAP_GID_IN_SERVICE), 
				    port);
	if (rval) {
		acm_log(0, "Cannot subscribe for Trap GID_IN_SERVICE. (%d)\n",
			rval);
		fstatus = FERROR;
		goto error;
	}

	rval = omgt_sa_register_trap(port->omgt_handle, 
				    ntoh16(IBV_SA_SM_TRAP_GID_OUT_OF_SERVICE),
				    port);
	if (rval) {
		acm_log(0, "Cannot subscrb for Trap GID_OUT_OF_SERVICE.(%d)\n",
			rval);
		fstatus = FERROR;
	}

error:
	pthread_mutex_unlock(&m_context);

	return fstatus;
}

/*
 * There seems to be a bug in ib_usa that causes it to crash if you 
 * just try to re-subscribe after a time out error. So, we trash
 * the whole channel and reconnect.
 */
FSTATUS dsap_reinitialize_notifications(void)
{
	acm_log(2, "Initiating re-scan.\n");

	dsap_port_event(0, 0, 0, DSAP_PT_EVT_FULL_RESCAN);

	return FSUCCESS;
}
