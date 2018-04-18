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

  \file dsap_module.h
 
  $Revision: 1.3 $
  $Date: 2015/01/22 18:07:38 $

  Defines the prototypes and constants used by the ibacm provider dsap.
*/
#ifndef _DSAP_H_
#define _DSAP_H_

#include <pthread.h>
#include <infiniband/verbs.h>
#include <infiniband/acm.h>
#include <infiniband/acm_prov.h>
#include <infiniband/sa.h> 

#include <iba/ibt.h>
#include <iba/public/ilist.h>
#include "ispinlock_osd.h"
#include "opamgt_sa_priv.h"

#include "dsap_topology.h"

#define MAX_EP_ADDR 4

#define for_each(list, item) \
	for (item = QListHead(list); item != NULL; item = QListNext(list, item))

typedef enum port_event_type {
	DSAP_PT_EVT_SRC_PORT_UP,
	DSAP_PT_EVT_SRC_PORT_DOWN,
	DSAP_PT_EVT_DST_PORT_UP,
	DSAP_PT_EVT_DST_PORT_DOWN,
	DSAP_PT_EVT_PORT_RESCAN,
	DSAP_PT_EVT_FULL_RESCAN
} port_event_type_t;

typedef enum default_fabric_action {
	DSAP_DEF_FAB_ACT_NORMAL,
	DSAP_DEF_FAB_ACT_NONE,
	DSAP_DEF_FAB_ACT_ALL
} default_fabric_action_t;

/* Device/port/endpoint/adddress structures for local provider ports */
struct dsap_device; 

struct dsap_port {
	struct dsap_device      *dev;
	const struct acm_port   *port;
	QUICK_LIST              ep_list;
	SPIN_LOCK               lock;
	enum ibv_port_state     state;
	uint16_t                default_pkey_ix;
	uint16_t                lid;
	uint16_t                lid_mask;
	uint16_t                lmc;
	uint16_t                sm_lid;
	pthread_t               notice_thread;
	int                     notice_started;
	int                     terminating;
	struct omgt_port         *omgt_handle;
	void                    *lib_handle;
};

struct dsap_device {
	const struct acm_device *device;
	LIST_ITEM               item;
	int                     port_cnt;
	struct dsap_port        port[0];
};

struct dsap_ep;

struct dsap_addr {
	uint16_t              type;
	union acm_ep_info     info;
	struct acm_address    *addr;
	struct dsap_ep        *ep;
};

struct dsap_ep {
	struct dsap_port          *port;
	LIST_ITEM                 item;
	char                      id_string[ACM_MAX_ADDRESS];
	const struct acm_endpoint *endpoint;
	SPIN_LOCK                 lock;
	struct dsap_addr          addr_info[MAX_EP_ADDR];
	uint64_t                  counters[ACM_MAX_COUNTER];
};

/* Configuration parameters */
extern uint32 dsap_no_subscribe;
extern uint32 dsap_unsub_scan_frequency;
extern uint32 dsap_scan_frequency;
extern boolean dsap_publish;
extern uint32 dsap_default_fabric;

/* Function prototypes */
void dsap_get_config(char *filename);
void dsap_cleanup(void);
FSTATUS dsap_init(void);
void dsap_omgt_log_init(struct omgt_port *omgt_port);

int dsap_default_fabric_parser(char *str, void *ptr);
char *dsap_default_fabric_printer(void *ptr);

FSTATUS dsap_scanner_init(void);
FSTATUS dsap_scanner_start(void);
void dsap_scanner_cleanup(void);

FSTATUS dsap_query_path_records(dsap_src_port_t *src_port,
				dsap_dst_port_t *dst_port, uint64_t sid, 
				uint16_t pkey);
FSTATUS dsap_query_dst_ports(dsap_subnet_t *subnet);
FSTATUS dsap_query_default_vfinfo_record(dsap_subnet_t *subnet);
FSTATUS dsap_query_vfinfo_records(dsap_subnet_t *subnet,
				  dsap_service_id_range_t *service_id_range);
FSTATUS dsap_query_dst_port(union ibv_gid *dst_gid, NODE_TYPE *node_type,
			    char * node_desc);

void dsap_port_event(uint64 src_guid, uint64 src_subnet, uint64 dest_guid,
		     port_event_type_t port_event_type);

struct dsap_port * dsap_lock_prov_port(dsap_src_port_t *src_port);
void dsap_release_prov_port(struct dsap_port *port);
FSTATUS dsap_add_src_ports(void);

#endif
