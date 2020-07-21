/* BEGIN_ICS_COPYRIGHT4 ****************************************

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

** END_ICS_COPYRIGHT4   ****************************************/

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

/* work around conflicting names */
#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include "iba/public/ilist.h"
#include "iba/public/ievent.h"
#include "iba/public/ispinlock.h"
#include "iba/public/itimer.h"
#include "opasadb_path_private.h"
#include "opasadb_debug.h"
#include "dsap_topology.h"
#include "dsap_notifications.h"
#include "dsap.h"

static int dsap_scanner_end = 0;

uint32 dsap_scan_frequency = 600;	/* in secconds */
uint32 dsap_unsub_scan_frequency = 10;	/* in secconds */
boolean dsap_publish = 1;
uint32 dsap_default_fabric = DSAP_DEF_FAB_ACT_NORMAL;

static pthread_t dsap_scanner_thread;
static int dsap_scanner_rescan = 0;
static SPIN_LOCK dsap_scanner_lock;
static EVENT dsap_scanner_event;	/* wakes up scanner thread to do
					another scan */

#define SCAN_RING_SIZE 512 	/* Must be power of 2 */
static struct dsap_scan_port {
	uint64                  src_guid;
	uint64                  src_subnet;
	uint64                  dest_guid;
	port_event_type_t       event_type;
} dsap_scan_port_ring[SCAN_RING_SIZE];
static int scan_ring_put = 0;
static int scan_ring_take = 0;

static op_ppath_writer_t shared_memory_writer;

struct sigaction new_action, old_action;

static char *port_event[] = {
	"Source Port Up",
	"Source Port Down",
	"Destination Port Up",
	"Destination Port Down",
	"Port Scan Request",
	"Complete Fabric Scan Request"
};

int dsap_default_fabric_parser(char *str, void *ptr)
{
	int err = 0;

	if (!str || !ptr) {
		acm_log(0, "Bad arguments to default fabric parser.\n");
		err = EINVAL;
		goto exit;
	}

	if (strcmp(str, "none") == 0) {
		dsap_default_fabric = DSAP_DEF_FAB_ACT_NONE;
		goto exit;
	}

	if (strcmp(str, "normal") == 0) {
		dsap_default_fabric = DSAP_DEF_FAB_ACT_NORMAL;
		goto exit;
	}

	if (strcmp(str, "all") == 0) {
		dsap_default_fabric = DSAP_DEF_FAB_ACT_ALL;
		goto exit;
	}

	acm_log(0, "Invalid value (%s) specified for dsap_default_fabric.\n",
		str);
	err = EINVAL;

exit:
	return err;
}

char * dsap_default_fabric_printer(void *ptr)
{
	switch (dsap_default_fabric) {
	case DSAP_DEF_FAB_ACT_NONE:
		return "none";

	case DSAP_DEF_FAB_ACT_NORMAL:
		return "normal";

	case DSAP_DEF_FAB_ACT_ALL:
		return "all";

	default:
		return "UNKNOWN";
	}
}

/*-----------------------------------------------------------
 * Wait for an event - this event is NOT interruptible
 *----------------------------------------------------------- 
*/ 
static FSTATUS 
EventWaitOnSeconds( EVENT *pEvent, int32 wait_seconds)
{
	FSTATUS status = FERROR;
	int retval;

	/* Make sure that the event was started  */
	ASSERT (pEvent->ev_state == Started);

	/*
	 Don't wait if the it has been signaled already!
	*/
	pthread_mutex_lock( &pEvent->ev_mutex );
	if (pEvent->ev_signaled == TRUE) {
		/* autoclear */
		pEvent->ev_signaled = FALSE;

		pthread_mutex_unlock(&pEvent->ev_mutex);
		return FSUCCESS;
	}

	if (wait_seconds == EVENT_NO_TIMEOUT) {
		wait_seconds = 0;
		/* Wait for condition variable ev_condvar to be signaled or
		   broadcast. */
		pthread_cond_wait(&pEvent->ev_condvar,&pEvent->ev_mutex);
	
		pEvent->ev_signaled = FALSE;
		pthread_mutex_unlock(&pEvent->ev_mutex);
		return FSUCCESS;
	} else {
		/* Get the current time */
		if (gettimeofday(&pEvent->ev_curtime, NULL) == 0) {
			pEvent->ev_timeout.tv_sec = pEvent->ev_curtime.tv_sec +
				wait_seconds;
			pEvent->ev_timeout.tv_nsec = pEvent->ev_curtime.tv_usec;
		
			retval = pthread_cond_timedwait(&pEvent->ev_condvar,
							&pEvent->ev_mutex,
							&pEvent->ev_timeout);
			if (retval == ETIMEDOUT) {
				status = FTIMEOUT;
			} else {
				/* We got signalled */
				status = FSUCCESS;
			}
		}
		pEvent->ev_signaled = FALSE;
		pthread_mutex_unlock(&pEvent->ev_mutex);
		return status;
	}
}

/*
 * Returns the number of published paths.
 */
uintn dsap_publish_paths(void)
{
	int err;
	uintn i;
	uintn count=0;
	uintn subnet_count=0;
	uint32 port_count;
	dsap_subnet_t *subnet;
	LIST_ITEM *vf_item;
	dsap_virtual_fabric_t *vfab;
	LIST_ITEM *sid_item;
	dsap_service_id_record_t *record;
	LIST_ITEM *sport_item;
	int j;
	dsap_src_port_t *sport;
	LIST_ITEM *path_item;
	LIST_ITEM *pkey_item;
	op_ppath_port_record_t op_port;
	dsap_pkey_t *pkey;
	dsap_path_record_t *path;

	acm_log(2, "\n");

	subnet_count = dsap_subnet_count();
	if (subnet_count == 0) {
		acm_log(0, "No subnets to publish.\n");
		goto exit;
	}

	/*
	 * We reserve one extra slot for SID 0 (i.e., no SID)
	 */
	err = op_ppath_initialize_subnets(&shared_memory_writer, subnet_count,
					  dsap_tot_sid_rec_count() + 1);
	if (err) {
		acm_log(0, "Failed to create the subnet table: %s.\n",
			strerror(err));
		goto exit;
	}
		
	port_count = dsap_tot_src_port_count();
	err = op_ppath_initialize_ports(&shared_memory_writer, port_count);
	if (err) {
		acm_log(0, "Failed to create the port table: %s.\n",
			strerror(err));
		goto exit;
	}
		
	err = op_ppath_initialize_vfabrics(&shared_memory_writer, 
					   dsap_tot_vfab_count() + 1);
	if (err) {
		acm_log(0, "Failed to create the virtual fabric table: %s.\n",
			strerror(err));
		goto exit;
	}
		
	err = op_ppath_initialize_paths(&shared_memory_writer,
					dsap_tot_path_rec_count());
	if (err) {
		acm_log(0, "Failed to create the path table: %s.\n",
			strerror(err));
		goto exit;
	}

	for (i=0; i< subnet_count; i++) {
		subnet = dsap_get_subnet_at(i);
		if (!subnet) 
			continue;
		vf_item = QListHead(&subnet->virtual_fabric_list);

		err = op_ppath_add_subnet(&shared_memory_writer,
					  subnet->subnet_prefix);
		if (err) {
			acm_log(0, "Failed to add subnet: %s\n",
				strerror(err));
			goto exit;
		}

		while (vf_item) {
			vfab = QListObj(vf_item);
			sid_item = QListHead(&vfab->service_id_record_list);

			err = op_ppath_add_vfab(
				&shared_memory_writer,
				(char *) vfab->vfinfo_record.vfName,
				subnet->subnet_prefix,
				vfab->vfinfo_record.pKey,
				vfab->vfinfo_record.s1.slBase);
			if (err) {
				acm_log(0, "Failed to add vfab: %s\n",
					strerror(err));
				goto exit;
			}

			while (sid_item) {
				record = QListObj(sid_item);

				err = op_ppath_add_sid(
				    &shared_memory_writer,
				    subnet->subnet_prefix,
				    record->service_id_range.lower_service_id,
				    record->service_id_range.upper_service_id,
				    (char *) vfab->vfinfo_record.vfName);
				if (err) {
					acm_log(0, "Failed to add SID range: "
						   "%s\n",
						strerror(err));
					goto exit;
				}
				sid_item = QListNext(
				   &vfab->service_id_record_list, sid_item);
			}

			vf_item = QListNext(&subnet->virtual_fabric_list,
					    vf_item);
		}

		sport_item = QListHead(&subnet->src_port_list);
		while (sport_item) {
			sport = QListObj(sport_item);
			path_item = QListHead(&sport->path_record_list);
			pkey_item = QListHead(&sport->pkey_list);

			if (sport->state != IBV_PORT_ACTIVE) {
				acm_log(2, "Skip inact port (0x%016"PRIx64")\n",
					ntoh64(sport->gid.global.interface_id));
				sport_item = QListNext(&subnet->src_port_list,
						       sport_item);
				continue;
			}

			memset(&op_port, 0, sizeof(op_port));

			op_port.source_prefix = 
				sport->gid.global.subnet_prefix;
			op_port.source_guid = sport->gid.global.interface_id;
			op_port.base_lid = sport->base_lid;
			op_port.lmc = sport->lmc;
			op_port.port = sport->port_num;
			strcpy(op_port.hfi_name, sport->hfi_name);

			j=0; 
			while (pkey_item && j < PKEY_TABLE_LENGTH) {
				pkey = QListObj(pkey_item);
				op_port.pkey[j] = pkey->pkey;
				j++;
				
				pkey_item = QListNext(&sport->pkey_list,
						      pkey_item);
			}

			err = op_ppath_add_port(&shared_memory_writer,
						op_port);
			if (err) {
				acm_log(0, "Failed to add port: %s\n",
					strerror(err));
				goto exit;
			}

			while (path_item) {
				path = QListObj(path_item);

				err = op_ppath_add_path(&shared_memory_writer,
							&path->path);
				if (err) {
					acm_log(0, "Failed to add path: %s\n",
						strerror(err));
					break;
				}		
				path_item = QListNext(&sport->path_record_list,
						      path_item);
				count++;
			}
			sport_item = QListNext(&subnet->src_port_list, 
					       sport_item);
		}
	}

exit:
	op_ppath_publish(&shared_memory_writer);

	_DBG_FUNC_EXIT;
	return count;
}

static void dsap_full_rescan(void)
{
	acm_log(2, "\n");
	dsap_scanner_rescan = 1;
	EventTrigger(&dsap_scanner_event);
}


static void dsap_rescan(union ibv_gid *src_gid)
{
	acm_log(2, "\n");
	dsap_port_event(src_gid->global.interface_id,
			src_gid->global.subnet_prefix,
			src_gid->global.interface_id, DSAP_PT_EVT_PORT_RESCAN);
}

static FSTATUS dsap_add_vfinfo_records(dsap_subnet_t *subnet)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *sid_range_item;
	dsap_service_id_record_t *record;

	acm_log(2, "\n");

	for_each(&sid_range_args, sid_range_item) {
		record = QListObj(sid_range_item);
		
		rval = dsap_query_vfinfo_records(subnet, 
						 &record->service_id_range);
		if (rval == FBUSY) 
			break;
		if (rval == FDUPLICATE) 
			rval = FSUCCESS;
		if (rval != FSUCCESS) {
			acm_log(0, "Vfinfo query failed with status %d\n",
				rval);
			rval = FSUCCESS;
		}
	}

	return rval;
}

static boolean dsap_pkey_match_found(dsap_src_port_t *src_port,
				     uint16_t vfab_pKey)
{
	LIST_ITEM *pkey_item;
	dsap_pkey_t *pkey;

	acm_log(2, "\n");
	
	for_each (&src_port->pkey_list, pkey_item) {
		pkey = QListObj(pkey_item);
		/* Mask off the high bit (fields are in network byte order) */
		if ((pkey->pkey & 0xff7f) == (vfab_pKey & 0xff7f))
			return TRUE;
	}

	return FALSE;
}

static FSTATUS dsap_for_each_service_id_record(dsap_src_port_t *src_port,
					       dsap_dst_port_t *dst_port,
					       dsap_virtual_fabric_t *vfab)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *sid_rec_item;
	dsap_service_id_record_t *sid_rec;

	acm_log(2, "\n");
	
	for_each (&vfab->service_id_record_list, sid_rec_item) {
		sid_rec = QListObj(sid_rec_item);

		if (dsap_scanner_end)
			break;
		rval = dsap_query_path_records(
		   src_port, dst_port, 
		   sid_rec->service_id_range.lower_service_id,
		   vfab->vfinfo_record.pKey);
		/* We only need to find one set of path records */
		/* So only scan until a successful set is returned */
		/* or we have exhausted all possible avenues */
		if (rval == FSUCCESS) 
			break;

		/* If the SM is busy retry the scan at a later time */
		/* in order to alleviate congestion.*/
		if (rval == FBUSY) 
			break;
	}

	return rval;
}

static FSTATUS dsap_for_each_virtual_fabric(dsap_subnet_t *subnet,
					    dsap_src_port_t *src_port,
					    dsap_dst_port_t *dst_port)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *vfab_item;
	dsap_virtual_fabric_t *vfab;

	acm_log(2, "\n");
	
	for_each (&subnet->virtual_fabric_list, vfab_item) {
		vfab = QListObj(vfab_item);

		if (dsap_scanner_end) 
			break;

		if (dsap_pkey_match_found(src_port, 
					  vfab->vfinfo_record.pKey)) {
			rval = dsap_for_each_service_id_record(
			   src_port, dst_port, vfab);
			if (rval != FSUCCESS)
				break;
		}
	}

	return rval;
}

static FSTATUS dsap_for_each_dst_port(dsap_subnet_t *subnet, 
				      dsap_src_port_t *src_port)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *dst_port_item;
	dsap_dst_port_t *dst_port;

	acm_log(2, "\n");
	
	for_each (&subnet->dst_port_list, dst_port_item) {
		dst_port = QListObj(dst_port_item);

		if (dsap_scanner_end) {
			break;
		}

		if (dst_port->node_type != STL_NODE_FI) {
#ifdef PRINT_PORTS_FOUND
			acm_log(2,  "Found Switch Port 0x%016"PRIx64":0x%016"
				    PRIx64".\n",
				ntoh64(dst_port->gid.global.subnet_prefix),
				ntoh64(dst_port->gid.global.interface_id));
#endif
			continue;
		}

#ifdef PRINT_PORTS_FOUND
		acm_log(2, "Found HFI Port 0x%016"PRIx64":0x%016"PRIx64".\n",
			ntoh64(dst_port->gid.global.subnet_prefix),
		ntoh64(dst_port->gid.global.interface_id));
#endif
		rval = dsap_for_each_virtual_fabric(subnet, src_port,
						    dst_port);
		if (rval != FSUCCESS)
			break;
	}

	return rval;
}

static FSTATUS dsap_add_path_records(dsap_subnet_t *subnet)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *src_port_item;
	dsap_src_port_t *src_port;

	acm_log(2, "\n");
	
	for_each (&subnet->src_port_list, src_port_item) {
		src_port = QListObj(src_port_item);

		if (dsap_scanner_end) {
			break;
		}

		rval = dsap_for_each_dst_port(subnet, src_port);
		if (rval != FSUCCESS) 
			break;
		
		acm_log(2, "Found %u path records on port 0x%016"PRIx64".\n",
			(unsigned int)dsap_path_record_count(src_port),
			ntoh64(src_port->gid.global.interface_id));
	}

	return rval;
}

FSTATUS dsap_scan_subnets(void)
{
	FSTATUS  rval = FERROR;
	uint32_t subnet_index;
	int      retry_needed = 0;
	int      error_occurred = 0;
	uint64_t prefix;

	acm_log(2, "\n");
	for (subnet_index = 0; subnet_index < dsap_subnet_count();
	     subnet_index++) {
		dsap_subnet_t *subnet = dsap_get_subnet_at(subnet_index);
		if (!subnet) 
			continue;

		if (dsap_scanner_end)
			break;

		if (!subnet->current) {
			if (FBUSY == (rval = dsap_query_dst_ports(subnet))) {
				retry_needed = 1; 
				continue;
			} else if (rval != FSUCCESS) {
				error_occurred = 1;
				continue;
			} else {
				prefix = subnet->subnet_prefix;
				acm_log(0, "Found %u dest ports for subnet "
					   "0x%016"PRIx64"\n",
					(unsigned int)dsap_dst_port_count(subnet),
					ntoh64(prefix));
			}

			rval = dsap_add_vfinfo_records(subnet);
			if (rval == FBUSY) {
				retry_needed = 1; 
				continue;
			} else if (rval != FSUCCESS) {
				error_occurred = 1;
				continue;
			} else {
				prefix = subnet->subnet_prefix;
				acm_log(0, "Found %u vfabs for subnet 0x%016"
					   PRIx64"\n",
					(unsigned int)dsap_virtual_fabric_count(subnet),
					ntoh64(prefix));
			}

			rval = dsap_add_path_records(subnet);
			if (rval == FBUSY) {
				retry_needed = 1; 
				continue;
			} else if (rval != FSUCCESS) {
				error_occurred = 1;
				continue;
			} else {
				prefix = subnet->subnet_prefix;
				acm_log(0, "Found %u paths for subnet 0x%016"
					   PRIx64"\n",
					(unsigned int)dsap_subnet_path_record_count(subnet),
					ntoh64(prefix));
			}
			subnet->current = 1;
		}
	}

	if (error_occurred) 
		rval = FERROR;
	else if (retry_needed)
		rval = FBUSY;
	else 
		rval = FSUCCESS;

	return rval;
}

static boolean dsap_dst_port_up(union ibv_gid *dst_gid, union ibv_gid *src_gid)
{
	FSTATUS rval;
	NODE_TYPE node_type;
	char node_desc[NODE_DESCRIPTION_ARRAY_SIZE];
	dsap_subnet_t *subnet;
	dsap_src_port_t *src_port;
	dsap_dst_port_t *dst_port;

	acm_log(2, "\n");
	
	rval = dsap_query_dst_port(dst_gid, &node_type, node_desc);
	if ((rval != FSUCCESS) || (node_type != STL_NODE_FI)) {
		dsap_full_rescan();
		return FALSE;
	}

	rval = dsap_add_dst_port(dst_gid, node_type, node_desc);
	if ((rval == FSUCCESS) || (rval == FDUPLICATE)) {
		subnet = dsap_find_subnet((uint64_t *)&dst_gid->global.subnet_prefix);
		src_port = dsap_find_src_port(src_gid);
		dst_port = dsap_find_dst_port(dst_gid);

		dsap_remove_path_records(src_port, dst_gid);

		rval = dsap_for_each_virtual_fabric(subnet, src_port,
						    dst_port);
		if (rval != FSUCCESS) {
			dsap_rescan(src_gid);
			return FALSE;
		}

		return TRUE;
	}

	acm_log(0, "Failure Adding Dest Port 0x%016"PRIx64":0x%016"PRIx64"\n",
		ntoh64(dst_gid->global.subnet_prefix),
		ntoh64(dst_gid->global.interface_id));

	return FALSE;
}

static boolean dsap_dst_port_down(union ibv_gid *dst_gid)
{
	dsap_dst_port_t *dst_port = dsap_find_dst_port(dst_gid);

	acm_log(2, "\n");

	if (dst_port != NULL) {
		if (dst_port->node_type != STL_NODE_FI) {
			dsap_full_rescan();
			return TRUE;
		}

		if (dsap_remove_dst_port(dst_gid) == FSUCCESS)
			return TRUE;
	}

	acm_log(1, "Unable To Remove Dst Port 0x%016"PRIx64":0x%016"PRIx64"\n",
		ntoh64(dst_gid->global.subnet_prefix),
		ntoh64(dst_gid->global.interface_id));

	return FALSE;
}

static boolean dsap_src_port_down(union ibv_gid *src_gid)
{
	acm_log(2, "\n");

	if (dsap_remove_src_port(src_gid) == FSUCCESS)
		return TRUE;

	acm_log(0, "Unable To Find Src Port 0x%016"PRIx64":0x%016"PRIx64"\n",
		ntoh64(src_gid->global.subnet_prefix),
		ntoh64(src_gid->global.interface_id));

	return FALSE;
}

static boolean dsap_src_port_up(union ibv_gid *src_gid, char *src_desc)
{
	FSTATUS rval;
	dsap_src_port_t *src_port = dsap_find_src_port(src_gid);
	struct dsap_port *port;

	acm_log(2, "\n");

	if (!src_port) 
		return FALSE;
	rval = dsap_add_dst_port(src_gid, STL_NODE_FI,
				 src_desc);
	if ((rval == FSUCCESS) || (rval == FDUPLICATE)) {
		port = dsap_lock_prov_port(src_port);
		if (port != NULL) {
			rval = dsap_update_src_port(src_port, port);
			dsap_release_prov_port(port);
			if (rval == FSUCCESS)
				return TRUE;
		}
	}

	acm_log(0, "Unable To Locate Src Port 0x%016"PRIx64":0x%016"PRIx64"\n",
		ntoh64(src_gid->global.subnet_prefix),
		ntoh64(src_gid->global.interface_id));

	dsap_full_rescan();

	return FALSE;
}

static boolean dsap_src_port_rescan(union ibv_gid *src_gid)
{
	dsap_subnet_t *subnet = dsap_find_subnet(
				(uint64_t *)&src_gid->global.subnet_prefix);
	dsap_src_port_t *src_port = dsap_find_src_port(src_gid);
	FSTATUS rval;

	acm_log(2, "\n");

	if (!subnet || !src_port) 
		return FALSE;

	if (dsap_src_port_up(src_gid, src_port->hfi_name) == FALSE)
		return FALSE;

	rval = dsap_for_each_dst_port(subnet, src_port);
	if (rval != FSUCCESS) {
		dsap_rescan(src_gid);
		return FALSE;
	}

	return TRUE;
}

static boolean dsap_process_port_events(void)
{
	boolean publish = FALSE;
	uint64 src_guid, src_subnet, dest_guid;
	port_event_type_t event_type;
	union ibv_gid src_gid;
	union ibv_gid dst_gid;
	dsap_src_port_t *src_port;

	acm_log(2, "\n");

	SpinLockAcquire(&dsap_scanner_lock);

	while(scan_ring_put != scan_ring_take) {
		scan_ring_take++;
		scan_ring_take &= (SCAN_RING_SIZE - 1);
		src_guid = dsap_scan_port_ring[scan_ring_take].src_guid;
		src_subnet = dsap_scan_port_ring[scan_ring_take].src_subnet;
		dest_guid = dsap_scan_port_ring[scan_ring_take].dest_guid;
		event_type = dsap_scan_port_ring[scan_ring_take].event_type;
		SpinLockRelease(&dsap_scanner_lock);

		src_gid.global.subnet_prefix = src_subnet;
		src_gid.global.interface_id = src_guid;

		dst_gid.global.subnet_prefix = src_subnet;
		dst_gid.global.interface_id = dest_guid;

		switch (event_type) {
		case DSAP_PT_EVT_DST_PORT_UP:
			acm_log(1, "PROCESSING GID(0x%016"PRIx64") IN SERVICE"
				   "ON PORT 0x%016"PRIx64".\n",
				ntoh64(dest_guid), ntoh64(src_guid));

			src_port = dsap_find_src_port(&src_gid);
			if (src_port == NULL) {
				acm_log(0, "Unable to find src port.\n");
				dsap_full_rescan();
				break;
			}

			if (dsap_dst_port_up(&dst_gid, &src_gid) == TRUE)
				publish = TRUE;
			break;

		case DSAP_PT_EVT_DST_PORT_DOWN:
			acm_log(1, "PROCESSING GID(0x%016"PRIx64") OUT OF"
				   "SERVICE ON PORT 0x%016"PRIx64".\n",
				ntoh64(dest_guid),
				ntoh64(src_guid));
			if (dsap_dst_port_down(&dst_gid) == TRUE)
				publish = TRUE;
			break;

		case DSAP_PT_EVT_SRC_PORT_UP:
			acm_log(1, "PROCESSING LOCAL PORT(0x%016"PRIx64") "
				   "ACTIVE.\n", ntoh64(src_guid));
			publish = TRUE;
			dsap_scanner_rescan = 1;
			break;

		case DSAP_PT_EVT_SRC_PORT_DOWN:
			acm_log(1, "PROCESSING LOCAL PORT(0x%016"PRIx64") "
				   "DOWN.\n", ntoh64(src_guid));
			if (dsap_src_port_down(&src_gid) == TRUE)
				publish = TRUE;
			break;

		case DSAP_PT_EVT_PORT_RESCAN:
			acm_log(1, "PROCESSING RESCAN REQUEST FOR PORT(0x%016"
				   PRIx64").\n", ntoh64(src_guid));

			if (dsap_src_port_rescan(&src_gid) == TRUE)
				publish = TRUE;
			break;

		case DSAP_PT_EVT_FULL_RESCAN:
			acm_log(1, "PROCESSING FULL FABRIC RESCAN REQUESTED BY"
				   " PORT(0x%016"PRIx64").\n",
				ntoh64(src_guid));
			dsap_full_rescan();
			goto exit;
		}
		SpinLockAcquire(&dsap_scanner_lock);
	}
	SpinLockRelease(&dsap_scanner_lock);

exit:
	return publish;
}

void dsap_port_event(uint64 src_guid, uint64 src_subnet, uint64 dest_guid,
		     port_event_type_t event_type)
{
	acm_log(2, "Port Event Src %"PRIx64":%"PRIx64", Dst %"PRIx64", %s.\n",
		ntoh64(src_subnet), ntoh64(src_guid), ntoh64(dest_guid),
		port_event[event_type]);
	SpinLockAcquire(&dsap_scanner_lock);
	scan_ring_put++;
	scan_ring_put &= (SCAN_RING_SIZE - 1);
	if(scan_ring_put == scan_ring_take) {
		/* full ring, just do a full scan, leave ring empty as side
		   effect */
		dsap_scanner_rescan = 1;
	} else {
		dsap_scan_port_ring[scan_ring_put].src_guid = src_guid;
		dsap_scan_port_ring[scan_ring_put].src_subnet = src_subnet;
		dsap_scan_port_ring[scan_ring_put].dest_guid = dest_guid;
		dsap_scan_port_ring[scan_ring_put].event_type = event_type;
	}
	SpinLockRelease(&dsap_scanner_lock);
	EventTrigger(&dsap_scanner_event);
}

static void kill_proc_handler(int signo){
	if (signo == SIGTERM) {
		dsap_scanner_cleanup();
		if(sigaction(SIGTERM, &old_action, NULL) < 0)
                	acm_log(2, "Signal handler Restore failed \n");
		raise(SIGTERM);
	}
}

#define SCAN_DELAY 5

static void * dsap_scanner(void* dummy)
{
	int32                  timeout_sec;
	uint64                 last_scan = 0;
	int32                  scan_delay = SCAN_DELAY;
	uintn                  pub_count = 0;
	uintn                  publish = 0;
	FSTATUS                rval;
	int                    err;
	uint64                 time_stamp;
	uint64                 since_scan;

	acm_log(2, "\n");

	/* Setting up signal handler to cleanup database*/
	new_action.sa_handler = kill_proc_handler;
	sigemptyset (&new_action.sa_mask);
	new_action.sa_flags = 0;
	if(sigaction(SIGTERM, &new_action, &old_action) < 0)
		acm_log(2, "Signal handler Init failed \n");

	timeout_sec = 0;

	if (dsap_publish) {
		if (op_ppath_version() != 3) {
			acm_log(0, "opasadb mis-match %u. Cannot Publish.\n",
				OPA_SA_DB_PATH_TABLE_VERSION);
			return NULL;
		}
		err = op_ppath_create_writer(&shared_memory_writer);
		if (err != 0) {
			acm_log(0, "Failed to create shared memory tables.\n");
			return NULL;
		}
	}

	while (!dsap_scanner_end) {
		if (dsap_process_port_events()) 
			publish = 1;

		if (dsap_scanner_rescan) {
			acm_log(2, "Attempting Fabric Rescan.\n");
			SpinLockAcquire(&dsap_scanner_lock);
			scan_ring_take = scan_ring_put;
			SpinLockRelease(&dsap_scanner_lock);

			/* Don't do fabric scans more than once every
			 * scan_delay second(s).
			 */
			if ((GetTimeStamp() / 1000000) < 
			     (last_scan + scan_delay)) {
				timeout_sec = scan_delay;
				acm_log(2, "Delaying Fabric Sweep By %u Sec\n",
					scan_delay);
				goto delay_scan;
			}

			/* Set the last scan time to the last time a full scan
			   was started, not when it was finished. */
			last_scan = GetTimeStamp() / 1000000;

			acm_log(2, "Performing Full Fabric Sweep.\n");
			dsap_empty_subnet_list();

			rval = dsap_add_src_ports();
			if (rval != FSUCCESS) {
				/* Wait a bit before trying to add the ports
				   again.*/
				timeout_sec = dsap_unsub_scan_frequency;
				scan_delay = dsap_unsub_scan_frequency;
				goto delay_scan;
			}

			acm_log(2, "After adding src ports there are %lu "
				   "subnets %lu total src ports\n\t\t%lu "
				   "total dst ports %lu total virtual "
				   "fabrics %lu pkeys %lu path_records.\n",
				dsap_subnet_count(),
				dsap_tot_src_port_count(),
				dsap_tot_dst_port_count(),
				dsap_tot_vfab_count(),
				dsap_tot_pkey_count(),
				dsap_tot_path_rec_count());

			while (((rval = dsap_scan_subnets()) == FBUSY) && 
			       !dsap_scanner_end) {
				scan_delay = lrand48() % 
					dsap_unsub_scan_frequency;
				timeout_sec = scan_delay;
				acm_log(2, "An SM Reported Busy. Delaying "
					   "Fabric Scan %u Seconds.\n",
					(unsigned)(scan_delay));
				EventWaitOnSeconds(&dsap_scanner_event, 
						   timeout_sec);
			}

			dsap_scanner_rescan = 0;
			publish = 1;
	
			if (dsap_no_subscribe) 
				timeout_sec = dsap_unsub_scan_frequency;
			else
				timeout_sec = dsap_scan_frequency;

			acm_log(2, "After fabric scan there are %lu local ports"
				   " on %lu subnets,\n\t\t%lu destination ports,"
				   " %lu total virtual fabrics, %lu pkeys and %lu"
				   " path_records.\n",
				dsap_tot_src_port_count(),
				dsap_subnet_count(),
				dsap_tot_dst_port_count(),
				dsap_tot_vfab_count(),
				dsap_tot_pkey_count(),
				dsap_tot_path_rec_count());
		}

		if (publish && dsap_publish) {
			pub_count = dsap_publish_paths();
			acm_log(2, "Published %lu paths.\n", pub_count);
			publish=0;
		}

delay_scan:
		if (timeout_sec != 0) {
			time_stamp = GetTimeStamp() / 1000000;

			if (time_stamp >= last_scan) {
				since_scan = (time_stamp - last_scan);
			} else {
				/* Something or someone set the system clock
				   back in time */
				acm_log(2, "current time_stamp < last_scan."
					   "Forcing full fabric sweep\n");
				since_scan = timeout_sec;
				last_scan = time_stamp;
			}

			if (since_scan >= timeout_sec) {
				acm_log(2, "since_scan >= timeout_sec. "
					   "Performing full fabric sweep.\n");
				dsap_scanner_rescan = 1;
			} else {
				acm_log(2, "Waiting on scanner event for %d "
					   "sec.\n",
					(int32)(timeout_sec - since_scan));
				if (EventWaitOnSeconds(
				      &dsap_scanner_event,
				      timeout_sec - since_scan) != FSUCCESS)
					dsap_scanner_rescan = 1;
			}
		} else {
			EventWaitOnSeconds(&dsap_scanner_event, 
					   EVENT_NO_TIMEOUT);
		}
	}

	return NULL;
}

FSTATUS dsap_scanner_init(void)
{
	srand48(GetTimeStamp());

	SpinLockInitState(&dsap_scanner_lock);
	if (! SpinLockInit(&dsap_scanner_lock))
		goto faillock;

	EventInitState(&dsap_scanner_event);
	if (! EventInit(&dsap_scanner_event))
		goto failsevent;

	return FSUCCESS;

failsevent:
	SpinLockDestroy(&dsap_scanner_lock);
faillock:
	return FINSUFFICIENT_RESOURCES;
}

FSTATUS dsap_scanner_start(void)
{
	int rval;

	rval = pthread_create(&dsap_scanner_thread, NULL, dsap_scanner, NULL);

	return rval ? FINSUFFICIENT_RESOURCES: FSUCCESS;
}

void dsap_scanner_cleanup(void)
{
	acm_log(2, "\n");

	dsap_scanner_end = 1;
	EventTrigger(&dsap_scanner_event);
	pthread_join(dsap_scanner_thread, NULL);
	EventDestroy(&dsap_scanner_event);
	SpinLockDestroy(&dsap_scanner_lock);

	acm_log(1, "Closing shared memory.\n");
	op_ppath_close_writer(&shared_memory_writer);


	return;
}
