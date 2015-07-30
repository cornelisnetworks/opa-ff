/* BEGIN_ICS_COPYRIGHT5 ****************************************

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
#include "oib_utils_sa.h"
#include "dsc_module.h"
#include "dsc_hca.h"
#include "dsc_notifications.h"
#include "opasadb_debug.h"

typedef enum {
	DSC_NOTIFY_NO_ACTION,
	DSC_NOTIFY_REREGISTER,
	DSC_NOTIFY_REINITIALIZE
} notify_t;

typedef struct _context_ {
	struct _context_   *next;
	struct ibv_context *hca_context;
	uint8_t  			port_num;
	uint64_t 			prefix;
	uint64_t 			port_guid;
	pthread_t           event_thread;
	int                 session_index;
} context_t;

static context_t *context_head = NULL;
static pthread_mutex_t m_context = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m_reinitialize = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t c_reinitialize;

static int running = 0;
    
static int report_inform_callback(int status, 
								  context_t *context);

static int report_notice_callback(int status, 
						   		  uint16_t trap_num, 
								  union ibv_gid *gid,
						   		  context_t *context);

typedef struct _oib_session_ {
	struct oib_port       *port;
	uint64_t               port_guid;
} oib_session_t;

static oib_session_t *dsc_oib_session = NULL;
static int dsc_oib_session_count = 0;

struct oib_port * 
dsc_get_oib_session(uint64_t port_guid)
{
	int i;

	if (!dsc_oib_session) {
		return NULL;
	}

	for (i = 0; i < dsc_oib_session_count; i++) {
		if (dsc_oib_session[i].port_guid == port_guid)
			return dsc_oib_session[i].port;
	}

	return NULL;
}

static void * 
dsc_notification_event_thread(void *arg)
{
	context_t *context_arg;

	_DBG_FUNC_ENTRY;

	if (arg == NULL) {
		_DBG_ERROR("Port context container empty.\n");
		pthread_exit (arg);
		_DBG_FUNC_EXIT;
		return arg;
	}
	context_arg = arg;

	if (pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL)) {
		_DBG_ERROR("Failed to set cancel type \n");
		pthread_exit(NULL);
	}
	if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)) {
		_DBG_ERROR("Failed to set cancel state\n");
		pthread_exit(NULL);
	}

	while (running) {
		int action = DSC_NOTIFY_NO_ACTION;
		struct timeval now;
		struct timespec delay;
		struct ibv_sa_event *event;

		pthread_testcancel();
		if (oib_get_sa_event(dsc_oib_session[context_arg->session_index].port, &event) == 0) {
			context_t *context = (context_t*)event->context;;
			struct ibv_sa_net_notice *notice;
			struct ibv_sa_net_notice_data_gid *data_details;

			switch (event->attr_id) {
        		case IBV_SA_ATTR_NOTICE:
					notice = (struct ibv_sa_net_notice*)event->attr;

					data_details = (struct ibv_sa_net_notice_data_gid*)notice->data_details;

					_DBG_DEBUG("NOTICE: source lid = %d, status = %d, attr_count = %d,"
							   " attr_size = %d, attr_offset = %d, attr_id = 0x%d, context = %p\n",
							   ntohs(notice->issuer_lid),
							   event->status,
							   event->attr_count,
							   event->attr_size,
							   event->attr_offset,
							   ntohs(event->attr_id),
							   context);

					{
						union ibv_gid gid;
						memcpy(gid.raw, data_details->gid, sizeof(gid.raw));
						action = report_notice_callback(event->status, 		
														notice->trap_num_device_id, 
														&gid, context);
					}
					break;

        		case IBV_SA_ATTR_INFORM_INFO:
					if (event->status != 0) {
 						_DBG_DEBUG("INFORM_INFO: status = %d, attr_count = %d, attr_size = %d,"
							  	   " attr_offset = %d, attr_id = 0x%x, context = %p\n",
								   event->status,
						   		   event->attr_count,
						   		   event->attr_size,
						   		   event->attr_offset,
						   		   ntohs(event->attr_id),
								   context);
						action = report_inform_callback(event->status, context);
					}
					break;

				default:
					_DBG_WARN("Unhandled notification: status = %d, attr_count = %d, attr_size = %d,"
							  " attr_offset = %d, attr_id = 0x%x, context = %p\n",
							  event->status,
							  event->attr_count,
							  event->attr_size,
							  event->attr_offset,
							  ntohs(event->attr_id),
							  context);
					break;
			}

			oib_ack_sa_event(event);
		}
		
		switch (action) {
			case DSC_NOTIFY_NO_ACTION:
				break;
			case DSC_NOTIFY_REREGISTER:
				break;
			case DSC_NOTIFY_REINITIALIZE:
				if (pthread_mutex_trylock(&m_reinitialize)) {
					// This should be impossible.
					_DBG_INFO("Discarding nested re-initialize event.\n");
					break;
				}

				gettimeofday(&now,NULL);
				delay.tv_sec = now.tv_sec + UnsubscribedScanFrequency;
				delay.tv_nsec = 0;
				
				_DBG_NOTICE("Re-initializing in %d seconds.\n", delay.tv_sec - now.tv_sec);

				pthread_cond_timedwait(&c_reinitialize, &m_reinitialize, &delay);

				dsc_reinitialize_notifications();
				pthread_mutex_unlock(&m_reinitialize);
				break;
		}
	}

	_DBG_FUNC_EXIT;

    pthread_exit (arg);
    return arg;
}

void 
dsc_terminate_notifications(void)
{
	context_t *next = NULL;
	context_t *context;

	_DBG_FUNC_ENTRY;
	running = 0;

	pthread_mutex_lock(&m_context);
	context = context_head;
	context_head = NULL;

	// Free the existing context structures.
	while (context != NULL) {
		next = context->next;

		pthread_cancel(context->event_thread);
		pthread_join(context->event_thread,NULL);

		if (dsc_oib_session[context->session_index].port != NULL) {
			oib_close_port(dsc_oib_session[context->session_index].port);
		}

		free(context);
		context = next;
	}

	pthread_mutex_unlock(&m_context);
	free(dsc_oib_session);

	_DBG_FUNC_EXIT;
}

static int
report_notice_callback(int status, 
						   uint16_t trap_num, 
						   union ibv_gid *gid,
						   context_t *context)
{
    char gid_str[INET6_ADDRSTRLEN];

	_DBG_FUNC_ENTRY;
	
	switch (trap_num)
	{
		case IBV_SA_SM_TRAP_GID_IN_SERVICE:
		{
			_DBG_INFO("Received GID Now In Service = 0x%016"PRIx64"/%s\n", ntoh64(context->port_guid), inet_ntop(AF_INET6, gid->raw, gid_str, sizeof gid_str));
			dsc_port_event(context->port_guid, context->prefix, gid->global.interface_id, dst_port_up);
			break;
		}
		case IBV_SA_SM_TRAP_GID_OUT_OF_SERVICE:
		{
			_DBG_INFO("Received GID Out Of Service = 0x%016"PRIx64"/%s\n", ntoh64(context->port_guid), inet_ntop(AF_INET6, gid->raw, gid_str, sizeof gid_str));
			dsc_port_event(context->port_guid, context->prefix, gid->global.interface_id,dst_port_down);
			break;
		}
		default:
		{
			_DBG_WARN("Received Unknown Notice = %u. Gid = %016"PRIx64"/%s\n", trap_num, context->port_guid, inet_ntop(AF_INET6, gid->raw, gid_str, sizeof gid_str));
			break;
		}
	}
	_DBG_FUNC_EXIT;
	return DSC_NOTIFY_NO_ACTION;
}

static int 
report_inform_callback(int status, context_t *context)
{
	int rval = DSC_NOTIFY_NO_ACTION;
	context_t *valid_context = context_head;
	_DBG_FUNC_ENTRY;

	/*
	 * NOTE: Sometimes the ib_usa module sends us back a valid context. 
	 * Sometimes it doesn't. So, if we have a valid context, we report
	 * the data, if we don't, we print a generic message.
	 */
	while (valid_context && valid_context != context) {
		valid_context = valid_context->next;
	}
	if (context != valid_context) {
		_DBG_INFO("Received an invalid context pointer from ib_usa: 0x%p\n", context);
		context = NULL;
	}
		
	switch (status) {
		case -ECONNABORTED:
			// In this case, we're hoping the async code will tell us when the port is back up.
			if (context) {
				_DBG_WARN("Port %u has lost contact with the SM.\n",
						  context->port_num);
			} else {
				_DBG_WARN("Lost contact with the SM.\n");
			}
			break;
		case -ETIMEDOUT:
			if (context) {
				_DBG_WARN("Port %u has timed out trying to contact with the SM.\n",
						  context->port_num);
			} else {
				_DBG_WARN("Timed out trying contact with the SM.\n");
			}

			rval = DSC_NOTIFY_REINITIALIZE;
			break;
		case -ENETRESET:
			// No action. The Async layer will tell us when the port is back up.
			_DBG_WARN("Lost contact with the SM due to port down.\n");
			break;
		case -EINVAL:
			if (context) {
				_DBG_WARN("Got an \"illegal value\" error on Port %u while trying to contact with the SM.\n",
						  context->port_num);
			} else {
				_DBG_WARN("Got an \"illegal value\" error trying contact with the SM.\n");
			}

			rval = DSC_NOTIFY_REINITIALIZE;
			break;			
		case -EAGAIN:
			_DBG_WARN("Got an EAGAIN error trying to subscribe for notifications.\n");

			rval = DSC_NOTIFY_REINITIALIZE; 
			break;
		default:
			if (context) {
				_DBG_WARN("Got an unknown error (%d) on Port %u while trying to contact with the SM.\n",
						  status, context->port_num);
			} else {
				_DBG_WARN("Got an unknown error (%d) error trying contact with the SM.\n",
						  status);
			}
			// We don't know what the error means, we had better assume the worst.
			rval = DSC_NOTIFY_REINITIALIZE; 
	}
	_DBG_FUNC_EXIT;
	return rval;
}

static FSTATUS 
register_port(const char *ca_name, 
			  uint64_t prefix,
			  uint64_t port_guid,
			  unsigned port_num,
			  unsigned state,
			  int session_index)
{
	int rval;

	context_t *context = malloc(sizeof(*context));
	if (!context) {
		_DBG_ERROR("Unable to allocate memory.\n");
		_DBG_FUNC_EXIT;
		return FERROR;
	}

	_DBG_DEBUG("prefix 0x%llx guid 0x%llx port_num %d sess_inx %d\n", 
		   prefix, port_guid, port_num, session_index);
	memset(context, 0, sizeof(context_t));
	context->port_num     = port_num;
	context->port_guid    = port_guid;
	context->prefix       = prefix;
	context->session_index = session_index;

	/* open the oib port object */
	rval = oib_open_port_by_guid(&dsc_oib_session[session_index].port, 
								 htonll(port_guid));
	if (rval) {
		_DBG_ERROR("Cannot open oib port object. (%d)\n", rval);
		goto error;
	}
	dsc_oib_session[session_index].port_guid = port_guid;

	/* spawn the notification thread before registering for the traps */
	running = 1;

	if (pthread_create(&context->event_thread, 
				   NULL, 
				   dsc_notification_event_thread, 
				   context)) {
		_DBG_ERROR("Failed to create the notification thread\n");
		goto error;
	}

	rval = oib_sa_register_trap(dsc_oib_session[session_index].port,
								ntoh16(IBV_SA_SM_TRAP_GID_IN_SERVICE),
								context);

	if (rval) {
		_DBG_ERROR("Cannot Subscribe For Trap SMA_TRAP_GID_IN_SERVICE. (%d)\n", rval);
		goto error1;
	}

	rval = oib_sa_register_trap(dsc_oib_session[session_index].port,
								ntoh16(IBV_SA_SM_TRAP_GID_OUT_OF_SERVICE),
								context);

	if (rval) {
		_DBG_ERROR("Cannot Subscribe For Trap SMA_TRAP_GID_OUT_OF_SERVICE. (%d)\n", rval);
		goto error1;
	}

	pthread_mutex_lock(&m_context);
	context->next = context_head;
	context_head = context;
	pthread_mutex_unlock(&m_context);

	_DBG_FUNC_EXIT;
	return FSUCCESS;

error1:
	pthread_cancel(context->event_thread);
	pthread_join(context->event_thread, NULL);
error:
	if (context) {
		free(context);
	}
	if (dsc_oib_session[session_index].port != NULL) {
		oib_close_port(dsc_oib_session[session_index].port);
		dsc_oib_session[session_index].port = NULL;
	}

	_DBG_FUNC_EXIT;
	return FERROR;
}

FSTATUS 
dsc_notifications_register()
{
	FSTATUS fstatus = FERROR;
	int port_index;
	umad_port_t *ports = NULL; 
	uint32 port_count;
	int len;

	_DBG_FUNC_ENTRY;
		
	if (NoSubscribe) {
		_DBG_FUNC_EXIT;
		return FSUCCESS;
	}

	ports = dsc_get_local_ports(&port_count);
	if (ports == NULL) {
		_DBG_ERROR("Cannot get the list of ports.\n");
		fstatus = FERROR;
		goto abort;
	}

	len = sizeof(oib_session_t) * port_count;
	dsc_oib_session = malloc(len);
	if (dsc_oib_session == NULL) {
		_DBG_ERROR("Malloc of oib session memory failed.\n");
		fstatus = FERROR;
		goto abort;
	}
	memset(dsc_oib_session,0,len);
	dsc_oib_session_count = port_count;

	for( port_index = 0; port_index < port_count; port_index++) {
		fstatus = register_port(ports[port_index].ca_name, 
								ports[port_index].gid_prefix,
								ports[port_index].port_guid,
								ports[port_index].portnum,
								ports[port_index].state,
								port_index);
		/**
		 * Do we really want to do this??? This will terminate this 
		 * application!!! 
		 */
		if (fstatus != FSUCCESS)
			goto abort;
	}

	if (ports) dsc_put_local_ports(ports, port_count);
	return fstatus;

abort:
	if (ports) free(ports);
	dsc_terminate_notifications();
	_DBG_FUNC_EXIT;
	return fstatus;
}

FSTATUS
dsc_notifications_reregister_port(union ibv_gid *gid)
{
	FSTATUS fstatus = FSUCCESS;
	context_t *prev = NULL;
	context_t *context;
	int rval;

	_DBG_FUNC_ENTRY;

	pthread_mutex_lock(&m_context);

	context = context_head;

	while (context != NULL) {
		if (context->prefix == gid->global.subnet_prefix && 
			context->port_guid == gid->global.interface_id) {
			break;
		} 
		prev = context; context = context->next;
	}

	if (!context) {
		fstatus = FERROR;
		_DBG_INFO("Failed to get port gid for re-registration.\n");
		goto error;
	}

	rval = oib_sa_register_trap(dsc_oib_session[context->session_index].port,
								ntoh16(IBV_SA_SM_TRAP_GID_IN_SERVICE), 
								context);

	if (rval) {
		_DBG_ERROR("Cannot Subscribe For Trap SMA_TRAP_GID_IN_SERVICE. (%d)\n", rval);
		fstatus = FERROR;
		goto error;
	}

	rval = oib_sa_register_trap(dsc_oib_session[context->session_index].port, 
								ntoh16(IBV_SA_SM_TRAP_GID_OUT_OF_SERVICE), 
								context);

	if (rval) {
		_DBG_ERROR("Cannot Subscribe For Trap SMA_TRAP_GID_OUT_OF_SERVICE. (%d)\n", rval);
		fstatus = FERROR;
		goto error;
	}

error:

	pthread_mutex_unlock(&m_context);

	_DBG_FUNC_EXIT;
	return fstatus;
}

/*
 * There seems to be a bug in ib_usa that causes it to crash if you 
 * just try to re-subscribe after a time out error. So, we trash
 * the whole channel and reconnect.
 */
FSTATUS
dsc_reinitialize_notifications()
{
	_DBG_FUNC_ENTRY;

	_DBG_INFO("Initiating re-scan.\n");

	dsc_port_event(0,0,0,full_rescan);

	_DBG_FUNC_EXIT;
	return FSUCCESS;
}
