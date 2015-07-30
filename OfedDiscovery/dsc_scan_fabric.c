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

/* work around conflicting names */
#include <infiniband/mad.h>
#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include "iba/public/ilist.h"
#include "iba/public/ievent.h"
#include "iba/public/ispinlock.h"
#include "iba/public/ithread.h"
#include "iba/public/itimer.h"
#include "opasadb_path_private.h"
#include "opasadb_debug.h"
#include "dsc_hca.h"
#include "dsc_topology.h"
#include "dsc_notifications.h"
#include "dsc_module.h"

int dsc_scanner_end = 0;

uint32 MaxPaths = 16384; 
uint32 ScanFrequency = 600;	// in secconds
uint32 UnsubscribedScanFrequency = 10;	// in secconds
boolean Publish = 1;
uint32 DefaultFabric = normal;

THREAD dsc_scanner_thread;
int dsc_scanner_rescan = 1;
SPIN_LOCK dsc_scanner_lock;
int dsc_scanner_refreshed = 0;	// has initial scan completed
EVENT dsc_scanner_event;	// wakes up scanner thread to do another scan
EVENT dsc_scanner_refresh_event;	// event triggered after initial scan

#define DSC_SCAN_PORT_RING_SZ 512 /* Must be power of 2 */
struct dsc_scan_port {
	uint64 src_guid;
	uint64 src_subnet;
	uint64 dest_guid;
	port_event_type_t port_event_type;
} dsc_scan_port_ring[DSC_SCAN_PORT_RING_SZ];
int dsc_scan_port_ring_put = 0;
int dsc_scan_port_ring_take = 0;

static op_ppath_writer_t shared_memory_writer;

static char *port_event[] = {
	"Source Port Up",
	"Source Port Down",
	"Destination Port Up",
	"Destination Port Down",
	"Port Scan Request",
	"Complete Fabric Scan Request"
};

int dsc_default_fabric_parser(char *str, void *ptr)
{
	int err = 0;

	if (!str || !ptr) {
		_DBG_ERROR("Bad arguments to default fabric parser.\n");
		err = EINVAL;
		goto exit;
	}

	if (strcmp(str, "none") == 0) {
		DefaultFabric = none;
		goto exit;
	}

	if (strcmp(str, "normal") == 0) {
		DefaultFabric = normal;
		goto exit;
	}

	if (strcmp(str, "all") == 0) {
		DefaultFabric = all;
		goto exit;
	}

	_DBG_ERROR("Invalid value (%s) specified for DefaultFabric.\n", str);
	err = EINVAL;

exit:
	return err;
}

char *dsc_default_fabric_printer(void *ptr)
{
	if (DefaultFabric == none) {
		return "none";
	}

	if (DefaultFabric == normal) {
		return "normal";
	}

	if (DefaultFabric == all) {
		return "all";
	}
	
	return "Value for Default Fabric is UNKNOWN.";
}

//-----------------------------------------------------------
// Wait for an event - this event is NOT interruptible
//-----------------------------------------------------------
static FSTATUS 
EventWaitOnSeconds( EVENT *pEvent, int32 wait_seconds)
{
  	// Make sure that the event was started 
  	ASSERT (pEvent->ev_state == Started);

	//
	// Don't wait if the it has been signaled already!
	//
  	pthread_mutex_lock( &pEvent->ev_mutex );
    if (pEvent->ev_signaled == TRUE)
    {
		// autoclear
		pEvent->ev_signaled = FALSE;

		pthread_mutex_unlock( &pEvent->ev_mutex);
        return FSUCCESS;
    }

  	if (wait_seconds == EVENT_NO_TIMEOUT)
	{
		wait_seconds = 0;
		// Wait for condition variable ev_condvar to be signaled or broadcast.
		pthread_cond_wait(&pEvent->ev_condvar,&pEvent->ev_mutex);
	
		pEvent->ev_signaled = FALSE;
  		pthread_mutex_unlock( &pEvent->ev_mutex );
		return FSUCCESS;		
  	}
	else
	{
		FSTATUS status = FERROR;
		int retval;

		// Get the current time
		if (gettimeofday(&pEvent->ev_curtime, NULL) == 0)
		{
			pEvent->ev_timeout.tv_sec = pEvent->ev_curtime.tv_sec + wait_seconds;
			pEvent->ev_timeout.tv_nsec = pEvent->ev_curtime.tv_usec;
		
			retval = pthread_cond_timedwait(&pEvent->ev_condvar, &pEvent->ev_mutex, &pEvent->ev_timeout);
			if (retval == ETIMEDOUT)
			{
				status = FTIMEOUT;
			}
			else
			{
				// We got signalled
				status = FSUCCESS;
			}
		}
		pEvent->ev_signaled = FALSE;
  		pthread_mutex_unlock( &pEvent->ev_mutex );
		return status;		
	}
}

/*
 * Returns the number of published paths.
 */
uintn dsc_publish_paths(void)
{
	int err;
    uintn i;
    uintn count=0;
    uintn subnet_count=0;
	uint32 port_count;

    _DBG_FUNC_ENTRY;

	subnet_count = dsc_subnet_count();
    if (subnet_count == 0) {
		_DBG_INFO("No subnets to publish.\n");
		goto exit;
	}

	/*
	 * We reserve one extra slot for SID 0 (i.e., no SID)
	 */
	err = op_ppath_initialize_subnets(&shared_memory_writer, 
									  subnet_count,
									  dsc_total_service_id_record_count() + 1);
	if (err) {
		_DBG_ERROR("Failed to create the subnet table: %s.\n",
					strerror(err));
		goto exit;
	}
		
	port_count = dsc_total_src_port_count();
	err = op_ppath_initialize_ports(&shared_memory_writer, port_count);
	if (err) {
		_DBG_ERROR("Failed to create the port table: %s.\n",
					strerror(err));
		goto exit;
	}
		
	err = op_ppath_initialize_vfabrics(&shared_memory_writer, 
									   dsc_total_virtual_fabric_count()+1);
	if (err) {
		_DBG_ERROR("Failed to create the virtual fabric table: %s.\n",
					strerror(err));
		goto exit;
	}
		
	err = op_ppath_initialize_paths(&shared_memory_writer, dsc_total_path_record_count());
	if (err) {
		_DBG_ERROR("Failed to create the path table: %s.\n",
					strerror(err));
		goto exit;
	}

    for (i=0; i< subnet_count; i++) {
		dsc_subnet_t *subnet = dsc_get_subnet_at(i);
		LIST_ITEM *vf_item;

		if (!subnet) {
			_DBG_ERROR("Failed to add subnet: %s\n",
					strerror(err));
			goto exit;
		}

		vf_item = QListHead(&subnet->virtual_fabric_list);
		err = op_ppath_add_subnet(&shared_memory_writer,
								  subnet->sm_gid.global.subnet_prefix);
		if (err) {
			_DBG_ERROR("Failed to add subnet: %s\n",
					strerror(err));
			goto exit;
		}

		while (vf_item) {
			dsc_virtual_fabric_t *vfab = QListObj(vf_item);
			LIST_ITEM *sid_item = QListHead(&vfab->service_id_record_list);

			err = op_ppath_add_vfab(&shared_memory_writer,
									(char*)vfab->vfinfo_record.vfName,
									subnet->sm_gid.global.subnet_prefix,
									vfab->vfinfo_record.pKey,
									vfab->vfinfo_record.s1.sl);
			if (err) {
				_DBG_ERROR("Failed to add virtual fabric: %s\n",
						strerror(err));
				goto exit;
			}

			while (sid_item) {
				dsc_service_id_record_t *record = QListObj(sid_item);

				err = op_ppath_add_sid(&shared_memory_writer,
									   subnet->sm_gid.global.subnet_prefix,
									   record->service_id_range.lower_service_id,
									   record->service_id_range.upper_service_id,
									   (char*)vfab->vfinfo_record.vfName);
				if (err) {
					_DBG_ERROR("Failed to add SID range: %s\n",
							   strerror(err));
					goto exit;
				}
				sid_item = QListNext(&vfab->service_id_record_list, sid_item);
			}

			vf_item = QListNext(&subnet->virtual_fabric_list, vf_item);
		}

		{
			LIST_ITEM *sport_item = QListHead(&subnet->src_port_list);
			while (sport_item) {
				int j;
				dsc_src_port_t *sport = QListObj(sport_item);
				LIST_ITEM *path_item = QListHead(&sport->path_record_list);
				LIST_ITEM *pkey_item = QListHead(&sport->pkey_list);
				op_ppath_port_record_t op_port;

				if (sport->state != IBV_PORT_ACTIVE) {
					_DBG_INFO("Skipping inactive port (0x%016"PRIx64")\n",
							  ntohll(sport->gid.global.interface_id));
					sport_item = QListNext(&subnet->src_port_list, sport_item);	
					continue;
				}

				memset(&op_port,0,sizeof(op_ppath_port_record_t));

				op_port.source_prefix = sport->gid.global.subnet_prefix;
				op_port.source_guid = sport->gid.global.interface_id;
				op_port.base_lid = sport->base_lid;
				op_port.lmc = sport->lmc;
				op_port.port = sport->port_num;
				strcpy(op_port.hca_name, sport->hca_name);

				j=0; 
				while (pkey_item && j < PKEY_TABLE_LENGTH) {
					dsc_pkey_t *pkey = QListObj(pkey_item);
					op_port.pkey[j] = pkey->pkey;
					j++;
					
					pkey_item = QListNext(&sport->pkey_list, pkey_item);
				}

				err = op_ppath_add_port(&shared_memory_writer,
										op_port);
				if (err) {
					_DBG_ERROR("Failed to add port: %s\n",
								strerror(err));
					goto exit;
				}		

				while (path_item) {
					dsc_path_record_t *path = QListObj(path_item);

					err = op_ppath_add_path(&shared_memory_writer,
											&path->path);
					if (err) {
						_DBG_ERROR("Failed to add path: %s\n",
									strerror(err));
						break;
					}		
					path_item = QListNext(&sport->path_record_list, path_item);
					count++;
				}
				sport_item = QListNext(&subnet->src_port_list, sport_item);	
			}
		}
	}

exit:
	op_ppath_publish(&shared_memory_writer);

	_DBG_FUNC_EXIT;
    return count;
}

static void dsc_full_rescan(void)
{
	_DBG_FUNC_ENTRY;
	dsc_scanner_rescan = 1;
	EventTrigger(&dsc_scanner_event);
	_DBG_FUNC_EXIT;
}

static void dsc_rescan(union ibv_gid *src_gid)
{
	_DBG_FUNC_ENTRY;
	dsc_port_event(src_gid->global.interface_id,
				   src_gid->global.subnet_prefix,
				   src_gid->global.interface_id, port_rescan);
	_DBG_FUNC_EXIT;
}

static FSTATUS dsc_get_subnet_port_gid(union ibv_gid *src_gid, union ibv_gid *sm_gid)
{
	FSTATUS rval;
	
    _DBG_FUNC_ENTRY;

	sm_gid->global.subnet_prefix = src_gid->global.subnet_prefix;
	rval = dsc_query_node_record(src_gid, &sm_gid->global.interface_id);
	
	_DBG_FUNC_EXIT;

	return rval;
}

static FSTATUS dsc_add_src_ports(void)
{
	FSTATUS rval = FSUCCESS;
	umad_port_t	*src_ports;
	uint32_t port_count;
	
    _DBG_FUNC_ENTRY;

	src_ports = dsc_get_local_ports(&port_count);
	if (src_ports != NULL) {
		uint32_t i;

		for (i = 0; i < port_count; i++) {
			union ibv_gid src_gid;
			union ibv_gid sm_gid;

			src_gid.global.subnet_prefix = src_ports[i].gid_prefix;
			src_gid.global.interface_id = src_ports[i].port_guid;

			if (src_ports[i].state == IBV_PORT_ACTIVE) {
				if (dsc_get_subnet_port_gid(&src_gid, &sm_gid) == FSUCCESS) {
					_DBG_DEBUG("Subnet Manager's GID is 0x%016"PRIx64":0x%016"PRIx64" "
							"for local port 0x%016"PRIx64":0x%016"PRIx64".\n",
							ntohll(sm_gid.global.subnet_prefix),
							ntohll(sm_gid.global.interface_id),
							ntohll(src_gid.global.subnet_prefix),
							ntohll(src_gid.global.interface_id));
				} else {
					_DBG_ERROR("Unable to determine Subnet Manager's port.\n");
					continue;
					
				}
			} else {
				_DBG_DEBUG("Skipping inactive port (0x%016"PRIx64")\n",
							ntohll(src_ports[i].port_guid));
				continue;
			}
			
			rval = dsc_add_src_port(&src_ports[i], &sm_gid);
			if (rval != FSUCCESS) {
				break;
			}
		}

		dsc_put_local_ports(src_ports, port_count);
	} else {
		rval = FERROR;
	}

	_DBG_FUNC_EXIT;

	return rval;
}

static FSTATUS dsc_add_vfinfo_records(dsc_subnet_t *subnet)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *sid_range_item;

	_DBG_FUNC_ENTRY;

	for_each(&sid_range_args, sid_range_item) {
		dsc_service_id_record_t *record = QListObj(sid_range_item);
		
		rval = dsc_query_vfinfo_records(subnet, &record->service_id_range);
		if (rval == FBUSY) break;
		if (rval == FDUPLICATE) rval = FSUCCESS;
		if (rval != FSUCCESS) {
			_DBG_ERROR("dsc_query_info_records failed with status %d\n",rval);
			rval = FSUCCESS;
		}
	}

	_DBG_FUNC_EXIT;

	return rval;
}

static boolean dsc_pkey_match_found(dsc_src_port_t *src_port, uint16_t vfab_pKey)
{
	LIST_ITEM *pkey_item;

	_DBG_FUNC_ENTRY;
	
	for_each (&src_port->pkey_list, pkey_item) {
		dsc_pkey_t *pkey = QListObj(pkey_item);
		// Mask off the high bit (fields are in network byte order)
		if ((pkey->pkey & 0xff7f) == (vfab_pKey & 0xff7f)) {
			return TRUE;
		}
	}

	_DBG_FUNC_EXIT;

	return FALSE;
}

static FSTATUS dsc_for_each_service_id_record(dsc_src_port_t *src_port, dsc_dst_port_t *dst_port, dsc_virtual_fabric_t *virtual_fabric)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *service_id_record_item;

	_DBG_FUNC_ENTRY;
	
	for_each (&virtual_fabric->service_id_record_list, service_id_record_item) {
		dsc_service_id_record_t *service_id_record = QListObj(service_id_record_item);

		if (dsc_scanner_end) {
			break;
		}

		rval = dsc_query_path_records(src_port,
									  dst_port,
									  service_id_record->service_id_range.lower_service_id,
									  virtual_fabric->vfinfo_record.pKey);
		/* We only need to find one set of path records */
		/* So only scan until a successful set is returned */
		/* or we have exhausted all possible avenues */
		if (rval == FSUCCESS) {
			break;
		}

		/* If the SM is busy retry the scan at a later time */
		/* in order to alleviate congestion.*/
		if (rval == FBUSY) {
			break;
		}
	}

	_DBG_FUNC_EXIT;

	return rval;
}

static FSTATUS dsc_for_each_virtual_fabric(dsc_subnet_t *subnet, dsc_src_port_t *src_port, dsc_dst_port_t *dst_port)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *virtual_fabric_item;

	_DBG_FUNC_ENTRY;
	
	for_each (&subnet->virtual_fabric_list, virtual_fabric_item) {
		dsc_virtual_fabric_t *virtual_fabric = QListObj(virtual_fabric_item);

		if (dsc_scanner_end) {
			break;
		}

		if (dsc_pkey_match_found(src_port, virtual_fabric->vfinfo_record.pKey)) {
			rval = dsc_for_each_service_id_record(src_port, dst_port, virtual_fabric);
			if (rval != FSUCCESS) {
				break;
			}
		}
	}

	_DBG_FUNC_EXIT;

	return rval;
}

static FSTATUS dsc_for_each_dst_port(dsc_subnet_t *subnet, dsc_src_port_t *src_port)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *dst_port_item;

	_DBG_FUNC_ENTRY;
	
	for_each (&subnet->dst_port_list, dst_port_item) {
		dsc_dst_port_t *dst_port = QListObj(dst_port_item);

		if (dsc_scanner_end) {
			break;
		}

		if (dst_port->node_type != STL_NODE_FI) {
#ifdef PRINT_PORTS_FOUND
			_DBG_DEBUG("Found Switch Port 0x%016"PRIx64":0x%016"PRIx64".\n",
					   ntohll(dst_port->gid.global.subnet_prefix),
					   ntohll(dst_port->gid.global.interface_id));
#endif
			continue;
		}

#ifdef PRINT_PORTS_FOUND
		_DBG_DEBUG("Found Channel Adapter Port 0x%016"PRIx64":0x%016"PRIx64".\n",
				   ntohll(dst_port->gid.global.subnet_prefix),
				   ntohll(dst_port->gid.global.interface_id));
#endif
		rval = dsc_for_each_virtual_fabric(subnet, src_port, dst_port);
		if (rval != FSUCCESS) {
			break;
		}
	}

	_DBG_FUNC_EXIT;

	return rval;
}

static FSTATUS dsc_add_path_records(dsc_subnet_t *subnet)
{
	FSTATUS rval = FSUCCESS;
	LIST_ITEM *src_port_item;

	_DBG_FUNC_ENTRY;
	
	for_each (&subnet->src_port_list, src_port_item) {
		dsc_src_port_t *src_port = QListObj(src_port_item);

		if (dsc_scanner_end) {
			break;
		}

		if (src_port->state == IBV_PORT_ACTIVE) {
			rval = dsc_for_each_dst_port(subnet, src_port);
			if (rval != FSUCCESS) {
				break;
			}
		
			_DBG_DEBUG("Found %d path records on port 0x%016"PRIx64".\n",
					   dsc_path_record_count(src_port),
					   ntohll(src_port->gid.global.interface_id));
		}
	}

	_DBG_FUNC_EXIT;

	return rval;
}

FSTATUS dsc_scan_subnets(void)
{
	FSTATUS  rval = FERROR;
	uint32_t subnet_index;
	int      retry_needed = 0;
	int      error_occurred = 0;

	_DBG_FUNC_ENTRY;

	for (subnet_index = 0; subnet_index < dsc_subnet_count(); subnet_index++) {
		dsc_subnet_t *subnet = dsc_get_subnet_at(subnet_index);

		if (dsc_scanner_end) {
			break;
		}

		if (subnet && !subnet->current) {
			if (FBUSY == (rval = dsc_query_dst_ports(subnet))) {
				retry_needed = 1; 
				continue;
			} else if (rval != FSUCCESS) {
				error_occurred = 1;
				continue;
			} else {
				_DBG_DEBUG("Found %d destination ports for subnet 0x%016"
						  PRIx64"\n",
						  dsc_dst_port_count(subnet),
						  ntohll(subnet->sm_gid.global.subnet_prefix));
			}

			if (FBUSY == (rval = dsc_add_vfinfo_records(subnet))) {
				retry_needed = 1; 
				continue;
			} else if (rval != FSUCCESS) {
				error_occurred = 1;
				continue;
			} else {
				_DBG_DEBUG("Found %d virtual fabrics for subnet 0x%016"
						  PRIx64"\n",
						  dsc_virtual_fabric_count(subnet),
						  ntohll(subnet->sm_gid.global.subnet_prefix));
			}
			if (FBUSY == (rval = dsc_add_path_records(subnet))) {
				retry_needed = 1; 
				continue;
			} else if (rval != FSUCCESS) {
				error_occurred = 1;
				continue;
			} else {
				_DBG_DEBUG("Found %d paths for subnet 0x%016"
						  PRIx64"\n",
						  dsc_subnet_path_record_count(subnet),
						  ntohll(subnet->sm_gid.global.subnet_prefix));
			}
			subnet->current = 1;
		}
	}

    _DBG_FUNC_EXIT;

	if (error_occurred) rval = FERROR;
	else if (retry_needed) rval = FBUSY;
	else rval = FSUCCESS;

	return rval;
}

static boolean dsc_dst_port_up(union ibv_gid *dst_gid, union ibv_gid *src_gid, union ibv_gid *sm_gid)
{
	FSTATUS rval;
	NODE_TYPE node_type;
	char node_desc[NODE_DESCRIPTION_ARRAY_SIZE];

	_DBG_FUNC_ENTRY;
	
	rval = dsc_query_dst_port(dst_gid, sm_gid, &node_type, node_desc);
	if ((rval != FSUCCESS) || (node_type != STL_NODE_FI)) {
		dsc_full_rescan();
		_DBG_FUNC_EXIT;
		return FALSE;
	}

	rval = dsc_add_dst_port(dst_gid, sm_gid, node_type, node_desc);
	if ((rval == FSUCCESS) || (rval == FDUPLICATE)) {
		dsc_subnet_t *subnet = dsc_find_subnet(sm_gid);
		dsc_src_port_t *src_port = dsc_find_src_port(src_gid);
		dsc_dst_port_t *dst_port = dsc_find_dst_port(dst_gid);

		dsc_remove_path_records(src_port, dst_gid);

		rval = dsc_for_each_virtual_fabric(subnet, src_port, dst_port);
		if (rval != FSUCCESS) {
			dsc_rescan(src_gid);
			_DBG_FUNC_EXIT;
			return FALSE;
		}

		_DBG_FUNC_EXIT;
		return TRUE;

	}

	_DBG_ERROR("Failure Adding Destination Port 0x%016"PRIx64":0x%016"PRIx64"\n",
			   ntohll(dst_gid->global.subnet_prefix),
			   ntohll(dst_gid->global.interface_id));

	_DBG_FUNC_EXIT;
	return FALSE;
}

static boolean dsc_dst_port_down(union ibv_gid *dst_gid)
{
	dsc_dst_port_t *dst_port = dsc_find_dst_port(dst_gid);

	_DBG_FUNC_ENTRY;

	if (dst_port != NULL) {
		if (dst_port->node_type != STL_NODE_FI) {
			dsc_full_rescan();
			_DBG_FUNC_EXIT;
			return TRUE;
		}

		if (dsc_remove_dst_port(dst_gid) == FSUCCESS) {
			_DBG_FUNC_EXIT;
			return TRUE;
		}
	}

	_DBG_INFO("Unable To Remove Destination Port 0x%016"PRIx64":0x%016"PRIx64"\n",
			  ntohll(dst_gid->global.subnet_prefix),
			  ntohll(dst_gid->global.interface_id));

	_DBG_FUNC_EXIT;
	return FALSE;
}

static boolean dsc_src_port_down(union ibv_gid *src_gid)
{
	umad_port_t *src_port;
	
	_DBG_FUNC_ENTRY;

	src_port = dsc_get_local_port(src_gid);
	if (src_port != NULL) {
		boolean rval = (dsc_update_src_port(src_port, NULL) == FSUCCESS);
		dsc_put_local_port(src_port);
		if (rval) {
			_DBG_FUNC_EXIT;
			return TRUE;
		}
	}

	_DBG_INFO("Unable To Find Source Port 0x%016"PRIx64":0x%016"PRIx64"\n",
			  ntohll(src_gid->global.subnet_prefix),
			  ntohll(src_gid->global.interface_id));

	_DBG_FUNC_EXIT;
	return FALSE;
}

static boolean dsc_src_port_up(union ibv_gid *src_gid, union ibv_gid *sm_gid, char *src_desc)
{
	FSTATUS rval;
	
	_DBG_FUNC_ENTRY;

	rval = dsc_add_dst_port(src_gid, sm_gid, STL_NODE_FI, src_desc);
	if ((rval == FSUCCESS) || (rval == FDUPLICATE)) {
		umad_port_t *src_port;

		src_port = dsc_get_local_port(src_gid);
		if (src_port != NULL) {
			rval = dsc_update_src_port(src_port, sm_gid);
			dsc_put_local_port(src_port);
			if (rval == FSUCCESS) {
				_DBG_FUNC_EXIT;
				return TRUE;
			}
		}
	}

	_DBG_ERROR("Unable To Locate Source Port 0x%016"PRIx64":0x%016"PRIx64"\n",
			   ntohll(src_gid->global.subnet_prefix),
			   ntohll(src_gid->global.interface_id));

	dsc_full_rescan();

	_DBG_FUNC_EXIT;
	return FALSE;
}

static boolean dsc_src_port_rescan(union ibv_gid *src_gid, union ibv_gid *sm_gid)
{
	dsc_subnet_t *subnet = dsc_find_subnet(sm_gid);
	dsc_src_port_t *src_port = dsc_find_src_port(src_gid);
	FSTATUS rval;

	_DBG_FUNC_ENTRY;

	if (dsc_src_port_up(src_gid, sm_gid, src_port->hca_name) == FALSE) {
		_DBG_FUNC_EXIT;
		return FALSE;
	}

	rval = dsc_for_each_dst_port(subnet, src_port);
	if (rval != FSUCCESS) {
		dsc_rescan(src_gid);
		_DBG_FUNC_EXIT;
		return FALSE;
	}
		
	_DBG_FUNC_EXIT;
	return TRUE;
}

static boolean dsc_process_port_events(void)
{
	boolean publish = FALSE;

	_DBG_FUNC_ENTRY;

	SpinLockAcquire(&dsc_scanner_lock);

	while(dsc_scan_port_ring_put != dsc_scan_port_ring_take) {
		uint64 src_guid, src_subnet, dest_guid;
		port_event_type_t port_event_type;
		union ibv_gid src_gid;
		union ibv_gid dst_gid;
		union ibv_gid sm_gid;
		dsc_src_port_t *src_port;

		dsc_scan_port_ring_take++;
		dsc_scan_port_ring_take &= (DSC_SCAN_PORT_RING_SZ - 1);
		src_guid = dsc_scan_port_ring[dsc_scan_port_ring_take].src_guid;
		src_subnet = dsc_scan_port_ring[dsc_scan_port_ring_take].src_subnet;
		dest_guid = dsc_scan_port_ring[dsc_scan_port_ring_take].dest_guid;
		port_event_type = dsc_scan_port_ring[dsc_scan_port_ring_take].port_event_type;
		SpinLockRelease(&dsc_scanner_lock);

		src_gid.global.subnet_prefix = src_subnet;
		src_gid.global.interface_id = src_guid;

		dst_gid.global.subnet_prefix = src_subnet;
		dst_gid.global.interface_id = dest_guid;

		switch (port_event_type) {
			case dst_port_up:
				_DBG_NOTICE("PROCESSING GID(0x%016"PRIx64") IN SERVICE ON PORT 0x%016"PRIx64".\n",
							ntohll(dest_guid),
							ntohll(src_guid));

				src_port = dsc_find_src_port(&src_gid);
				if (src_port == NULL) {
					_DBG_ERROR("Unable to find src port.\n");
					dsc_full_rescan();
					break;
				}

				if (dsc_dst_port_up(&dst_gid, &src_gid, &src_port->sm_gid) == TRUE) {
					publish = TRUE;
				}
				break;

			case dst_port_down:
				_DBG_NOTICE("PROCESSING GID(0x%016"PRIx64") OUT OF SERVICE ON PORT 0x%016"PRIx64".\n",
							ntohll(dest_guid),
							ntohll(src_guid));
				if (dsc_dst_port_down(&dst_gid) == TRUE) {
					publish = TRUE;
				}
				break;

			case src_port_up:
				_DBG_NOTICE("PROCESSING LOCAL PORT(0x%016"PRIx64") ACTIVE.\n", ntohll(src_guid));
				break;

			case src_port_down:
				_DBG_NOTICE("PROCESSING LOCAL PORT(0x%016"PRIx64") DOWN.\n", ntohll(src_guid));
				if (dsc_src_port_down(&src_gid) == TRUE) {
					publish = TRUE;
				}
				break;

			case port_rescan:
				_DBG_NOTICE("PROCESSING RESCAN REQUEST FOR PORT(0x%016"PRIx64").\n", ntohll(src_guid));
	
				if (dsc_get_subnet_port_gid(&src_gid, &sm_gid) != FSUCCESS) {
					_DBG_ERROR("Unable to determine Subnet Manager's port.\n");
					dsc_full_rescan();
					break;
				}

				if (dsc_src_port_rescan(&src_gid, &sm_gid) == TRUE) {
					publish = TRUE;
				}
				break;

			case full_rescan:
				_DBG_NOTICE("PROCESSING FULL FABRIC RESCAN REQUESTED BY PORT(0x%016"PRIx64").\n", ntohll(src_guid));
				dsc_full_rescan();
				goto exit;
		}
		SpinLockAcquire(&dsc_scanner_lock);
	}
	SpinLockRelease(&dsc_scanner_lock);

exit:
	_DBG_FUNC_EXIT;

	return publish;
}

void dsc_port_event(uint64 src_guid, uint64 src_subnet, uint64 dest_guid, port_event_type_t port_event_type)
{
    _DBG_FUNC_ENTRY;

	_DBG_INFO("Dsc Port Event Src = %"PRIx64":%"PRIx64", Dst = %"PRIx64", = %s.\n",
			  ntohll(src_subnet),
			  ntohll(src_guid),
			  ntohll(dest_guid),
			  port_event[port_event_type]);
    SpinLockAcquire(&dsc_scanner_lock);
	dsc_scan_port_ring_put++;
	dsc_scan_port_ring_put &= (DSC_SCAN_PORT_RING_SZ - 1);
	if(dsc_scan_port_ring_put == dsc_scan_port_ring_take) {
		/* full ring, just do a full scan, leave ring empty as side effect */
		dsc_scanner_rescan = 1;
	} else {
		dsc_scan_port_ring[dsc_scan_port_ring_put].src_guid = src_guid;
		dsc_scan_port_ring[dsc_scan_port_ring_put].src_subnet = src_subnet;
		dsc_scan_port_ring[dsc_scan_port_ring_put].dest_guid = dest_guid;
		dsc_scan_port_ring[dsc_scan_port_ring_put].port_event_type = port_event_type;
	}
    SpinLockRelease(&dsc_scanner_lock);
	EventTrigger(&dsc_scanner_event);
    _DBG_FUNC_EXIT;
}

#define SCAN_DELAY 5

void dsc_scanner(void* dummy)
{
	int32                  timeout_sec;
	uint64                 last_scan = 0;
	int32                  scan_delay = SCAN_DELAY;
	uintn                  pub_count = 0;
	uintn				   publish = 0;
	FSTATUS                rval;
	int                    err;

    _DBG_FUNC_ENTRY;

	timeout_sec = 0;

	if (Publish) {
		if (op_ppath_version() != OPA_SA_DB_PATH_TABLE_VERSION) {
			_DBG_ERROR("The version of the opasadb Library does match %u. Cannot Publish.\n",
						OPA_SA_DB_PATH_TABLE_VERSION);
			return;
		}
		err = op_ppath_create_writer(&shared_memory_writer);
		if (err != 0) {
			_DBG_ERROR("Failed to create shared memory tables.\n");
			return;
		}
	}

	while (!dsc_scanner_end) {
		if (dsc_process_port_events()) {
			publish = 1;
		} 
		
		if (dsc_scanner_rescan) {

			_DBG_INFO("Attempting Fabric Rescan.\n");

			SpinLockAcquire(&dsc_scanner_lock);
			dsc_scan_port_ring_take = dsc_scan_port_ring_put;
    		SpinLockRelease(&dsc_scanner_lock);

			/* Don't do fabric scans more than once every
			 * scan_delay second(s).
			 */
			if (GetTimeStamp()/1000000 < last_scan+scan_delay) {
				timeout_sec = scan_delay;
				_DBG_INFO("Delaying Fabric Sweep By %u Seconds.\n", scan_delay);
				goto delay_scan;
			}

			// Set the last scan time to the last time a full scan was
			// started, not when it was finished. 
			last_scan = GetTimeStamp() / 1000000;

			_DBG_INFO("Performing Full Fabric Sweep.\n");

			dsc_empty_subnet_list();

			rval = dsc_add_src_ports();
			if (rval != FSUCCESS) {
				/* Wait a bit before trying to add the ports again. */
				timeout_sec = UnsubscribedScanFrequency;
				scan_delay = UnsubscribedScanFrequency;
				goto delay_scan;
			}

			_DBG_DEBUG("After dsc_add_src_ports there are %d subnets %d "
					   "total src ports %d total dst ports %d total virtual "
					   "fabrics %d pkeys %d path_records.\n",
					   dsc_subnet_count(),
					   dsc_total_src_port_count(),
					   dsc_total_dst_port_count(),
					   dsc_total_virtual_fabric_count(),
					   dsc_total_pkey_count(),
					   dsc_total_path_record_count());

			while (((rval = dsc_scan_subnets()) == FBUSY) && !dsc_scanner_end) {
				scan_delay = lrand48() % UnsubscribedScanFrequency;
				timeout_sec = scan_delay;
				_DBG_INFO("An SM Reported Busy. "
						  "Delaying Fabric Scan %u Seconds.\n",
						  (unsigned)(scan_delay));
				EventWaitOnSeconds(&dsc_scanner_event,
								   timeout_sec);
			}

			dsc_scanner_rescan = 0;
			publish = 1;
	
			if (NoSubscribe) {
				timeout_sec = UnsubscribedScanFrequency;
			} else {
				timeout_sec = ScanFrequency;
			}

			_DBG_INFO("After fabric scan there are %d local ports on %d subnets,"
					  " %d destination ports, %d total virtual fabrics, %d pkeys"
					  " and %d path_records.\n",
					  dsc_total_src_port_count(),
					  dsc_subnet_count(),
					  dsc_total_dst_port_count(),
					  dsc_total_virtual_fabric_count(),
					  dsc_total_pkey_count(),
					  dsc_total_path_record_count());
		}

		if (publish && Publish) {
			pub_count = dsc_publish_paths();
			_DBG_INFO("Published %lu paths.\n", pub_count);
			publish=0;
		}

delay_scan:
		if (timeout_sec != 0) {
			uint64 time_stamp = GetTimeStamp() / 1000000;
			uint64 since_scan;

			if (time_stamp >= last_scan) {
				since_scan = (time_stamp - last_scan);
			} else {
				/* Something or someone set the system clock back in time */
				_DBG_WARN("current time_stamp < last_scan. Forcing full fabric sweep\n");
				since_scan = timeout_sec;
				last_scan = time_stamp;
			}

			if (since_scan >= timeout_sec) {
				_DBG_INFO("since_scan >= timeout_sec. Performing full fabric sweep.\n");
				dsc_scanner_rescan = 1;	// full fabric sweep
			} else {
				_DBG_DEBUG("Waiting on scanner event for %d sec.\n", (int32)(timeout_sec - since_scan));
				if (FSUCCESS != EventWaitOnSeconds(&dsc_scanner_event,
												 timeout_sec - since_scan))
				{
					dsc_scanner_rescan = 1;	// full fabric sweep
				}
			}
		} else {
			EventWaitOnSeconds(&dsc_scanner_event, EVENT_NO_TIMEOUT);
		}
	}

    _DBG_FUNC_EXIT;
}

FSTATUS dsc_scanner_init(void)
{
	srand48(GetTimeStamp());

	SpinLockInitState(&dsc_scanner_lock);
	if (! SpinLockInit(&dsc_scanner_lock))
		goto faillock;

	EventInitState(&dsc_scanner_event);
	if (! EventInit(&dsc_scanner_event))
		goto failsevent;

	EventInitState(&dsc_scanner_refresh_event);
	if (! EventInit(&dsc_scanner_refresh_event))
		goto failrevent;

	ThreadInitState(&dsc_scanner_thread);
	return FSUCCESS;

failrevent:
	EventDestroy(&dsc_scanner_event);
failsevent:
	SpinLockDestroy(&dsc_scanner_lock);
faillock:
	return FINSUFFICIENT_RESOURCES;
}

FSTATUS dsc_scanner_start(uint32 wait_sweep)
{
	boolean rval;
	rval = ThreadCreate(&dsc_scanner_thread, "ibDsc", dsc_scanner, NULL);
	if (rval && wait_sweep) {
		// one time uninterruptible wait
		EventWaitOn(&dsc_scanner_refresh_event, EVENT_NO_TIMEOUT);
	}
	return rval?FSUCCESS:FINSUFFICIENT_RESOURCES;
}

void dsc_scanner_cleanup(void)
{
    _DBG_FUNC_ENTRY;

	dsc_scanner_end = 1;
	EventTrigger(&dsc_scanner_event);
	ThreadDestroy(&dsc_scanner_thread);
	EventDestroy(&dsc_scanner_event);
	EventDestroy(&dsc_scanner_refresh_event);
	SpinLockDestroy(&dsc_scanner_lock);

	_DBG_INFO("Closing shared memory.\n");
	op_ppath_close_writer(&shared_memory_writer);

    _DBG_FUNC_EXIT;
	return;
}
