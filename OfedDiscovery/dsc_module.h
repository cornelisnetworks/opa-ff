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

  \file dsc_module.h
 
  $Author: mwheinz $
  $Revision: 1.29 $
  $Date: 2015/01/22 18:04:10 $

  Defines the prototypes and constants used by the QLogic Distributed SA
  interface.
*/
#ifndef _DSC_MODULE_H_
#define _DSC_MODULE_H_

#include <iba/ibt.h>
#include <iba/public/ilist.h>

#include "dsc_topology.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define DSC_MAX_HCAS 8

#ifndef MIN
#define MIN(x,y) ({ __typeof (x) _x = (x), _y = (y); (_x<_y)?_x:_y; })
#endif
#ifndef MAX
#define MAX(x,y) ({ __typeof (x) _x = (x), _y = (y); (_x>_y)?_x:_y; })
#endif

#define for_each(list, item) \
	for (item = QListHead(list); item != NULL; item = QListNext(list, item))

typedef enum port_event_type {
	src_port_up,
	src_port_down,
	dst_port_up,
	dst_port_down,
	port_rescan,
	full_rescan
} port_event_type_t;

typedef enum default_fabric_action {
	normal,
	none,
	all
} default_fabric_action_t;

/* Master list of HFIs. */
extern uint32 WaitFirstSweep;	// wait for first sweep in module load
extern uint32 Dbg;
extern uint32 SwitchPathQueryEnabled;
extern uint32 NoSubscribe;
extern uint32 MaxPaths;
extern uint32 UnsubscribedScanFrequency;
extern uint32 ScanFrequency;
extern boolean Publish;
extern uint32 DefaultFabric;

void dsc_cleanup(void);
FSTATUS dsc_init(void);

FSTATUS dsc_sd_driver_start(void);
void dsc_sd_driver_cleanup(void);
FSTATUS dsc_sd_driver_restart(void);

int dsc_default_fabric_parser(char *str, void *ptr);
char *dsc_default_fabric_printer(void *ptr);

FSTATUS dsc_scanner_init(void);
FSTATUS dsc_scanner_start(uint32 wait_sweep);
void dsc_scanner_cleanup(void);

FSTATUS dsc_query_path_records(dsc_src_port_t *src_port, dsc_dst_port_t *dst_port, uint64_t sid, uint16_t pkey);
FSTATUS dsc_query_dst_ports(dsc_subnet_t *subnet);
FSTATUS dsc_query_default_vfinfo_record(dsc_subnet_t *subnet);
FSTATUS dsc_query_vfinfo_records(dsc_subnet_t *subnet, dsc_service_id_range_t *service_id_range);
FSTATUS dsc_query_node_record(union ibv_gid *src_gid, uint64_t *sm_port_guid);
FSTATUS dsc_query_dst_port(union ibv_gid *dst_gid, union ibv_gid *sm_gid, NODE_TYPE *node_type, char * node_desc);

void dsc_port_event(uint64 src_guid, uint64 src_subnet, uint64 dest_guid, port_event_type_t port_event_type);

#if defined(__cplusplus)
}
#endif

#endif
