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

  \file dsc_topology.h
 
  $Author: aestrin $
  $Revision: 1.13 $
  $Date: 2015/01/27 23:00:31 $

  \brief Routines for populating tables to define fabric topology from the SM.
*/

#if !defined(_DSC_TOPOLOGY_H_)
#define _DSC_TOPOLOGY_H_

#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include "iba/ibt.h"
#include <opasadb_path.h>

#include "ilist.h"
#include "iba/stl_sd.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct dsc_pkey {
	LIST_ITEM item;
	uint16_t  pkey; /* Stored in network order */
} dsc_pkey_t;

typedef struct dsc_path_record {
	LIST_ITEM         item;
	IB_PATH_RECORD_NO path; /* Stored in network order */
} dsc_path_record_t;

typedef struct dsc_src_port {
	LIST_ITEM     item;
	union ibv_gid gid;    /* Stored in network order */
	union ibv_gid sm_gid; /* Stored in network order */
	char hca_name[IBV_SYSFS_NAME_MAX];
	uint8_t port_num;
	unsigned base_lid;
	unsigned lmc;
	unsigned state;
	QUICK_LIST pkey_list;
	QUICK_LIST path_record_list;
} dsc_src_port_t;

typedef struct dsc_dst_port {
	LIST_ITEM     item;
	union ibv_gid gid;    /* Stored in network order */
	union ibv_gid sm_gid; /* Stored in network order */
	NODE_TYPE     node_type;
	char          node_desc[NODE_DESCRIPTION_ARRAY_SIZE]; 
	/* TODO: For IP address resolution, we need to add stl IPAddrPrimary and IPAddrSecondary here.
	   For IPv4 address from the the caller, we can always convert it into IPv6 format*/
} dsc_dst_port_t;

typedef struct dsc_service_id_range {
	int warned; /* set if an error has been emitted for this SID range. */
	uint64_t lower_service_id; /* Stored in network order */
	uint64_t upper_service_id; /* Stored in network order */
} dsc_service_id_range_t;

typedef struct dsc_service_id_record {
	LIST_ITEM              item;
	dsc_service_id_range_t service_id_range;
} dsc_service_id_record_t;

typedef struct dsc_virtual_fabric {
	LIST_ITEM          item;
	STL_VFINFO_RECORD vfinfo_record; /* Stored in network order */
	QUICK_LIST         service_id_record_list;
} dsc_virtual_fabric_t;

typedef struct dsc_subnet {
	LIST_ITEM     item;
	union ibv_gid sm_gid;  /* Stored in network order */
	QUICK_LIST    src_port_list;
	QUICK_LIST    dst_port_list;
	QUICK_LIST    virtual_fabric_list;
	int	          current; /* 0 = needs to be updated. */
} dsc_subnet_t;


/* PKEY Handling functions */
size_t dsc_pkey_count(dsc_src_port_t *src_port);
FSTATUS dsc_add_pkey(dsc_src_port_t *src_port, uint16_t pkey);
FSTATUS dsc_empty_pkey_list(dsc_src_port_t *src_port);
FSTATUS dsc_remove_pkey(dsc_src_port_t *src_port, uint16_t pkey);

/* Path Record Handling functions */
size_t dsc_path_record_count(dsc_src_port_t *src_port);
size_t dsc_subnet_path_record_count(dsc_subnet_t *subnet);
FSTATUS dsc_add_path_record(dsc_src_port_t *src_port, IB_PATH_RECORD_NO *path_record);
FSTATUS dsc_empty_path_record_list(dsc_src_port_t *src_port);
FSTATUS dsc_remove_path_records(dsc_src_port_t *src_port, union ibv_gid *dst_port_gid);

/* Src Port Handling functions */
size_t dsc_src_port_count(dsc_subnet_t *subnet);
FSTATUS dsc_update_src_port(umad_port_t *port, union ibv_gid *sm_gid);
FSTATUS dsc_add_src_port(umad_port_t *port, union ibv_gid *sm_gid);
FSTATUS dsc_empty_src_port_list(dsc_subnet_t *subnet);

/* Dst Port Handling functions */
size_t dsc_dst_port_count(dsc_subnet_t *subnet);
FSTATUS dsc_add_dst_port(union ibv_gid *dst_port_gid, union ibv_gid *sm_gid, NODE_TYPE node_type, char *node_desc);
FSTATUS dsc_empty_dst_port_list(dsc_subnet_t *subnet);
FSTATUS dsc_remove_dst_port(union ibv_gid *dst_port_gid);

/* ServiceID Record Handling functions */
size_t dsc_service_id_record_count(dsc_virtual_fabric_t *virtual_fabric);
FSTATUS dsc_add_service_id_record(dsc_virtual_fabric_t *virtual_fabric, dsc_service_id_range_t *service_id_range);
FSTATUS dsc_empty_service_id_record_list(dsc_virtual_fabric_t *virtual_fabric);
FSTATUS dsc_remove_service_id_record(dsc_virtual_fabric_t *virtual_fabric, dsc_service_id_range_t *service_id_range);

/* Virtual Fabric Record Handling functions */
size_t dsc_virtual_fabric_count(dsc_subnet_t *subnet);
FSTATUS dsc_add_virtual_fabric(dsc_subnet_t *subnet, STL_VFINFO_RECORD *vfinfo_record);
FSTATUS dsc_empty_virtual_fabric_list(dsc_subnet_t *subnet);
FSTATUS dsc_remove_virtual_fabric(dsc_subnet_t *subnet, uint8_t *vfName);

/* Subnet Handling functions */
size_t dsc_subnet_count(void);
dsc_subnet_t* dsc_get_subnet_at(uint32_t index);
FSTATUS dsc_add_subnet(union ibv_gid *sm_gid);
FSTATUS dsc_empty_subnet_list(void);

/* Search Utility Routines */
dsc_subnet_t* dsc_find_subnet(union ibv_gid *sm_gid);
dsc_src_port_t* dsc_find_src_port(union ibv_gid *gid);
dsc_src_port_t* dsc_find_src_port_by_lid(unsigned *lid);
dsc_path_record_t * dsc_find_path_record(dsc_src_port_t *src_port, IB_PATH_RECORD_NO *path);
dsc_dst_port_t* dsc_find_dst_port(union ibv_gid *gid);
dsc_dst_port_t* dsc_find_dst_port_by_name(char *node_desc);
dsc_virtual_fabric_t* dsc_find_virtual_fabric(uint8_t *vfName, dsc_subnet_t *subnet);

size_t dsc_total_src_port_count(void);
size_t dsc_total_dst_port_count(void);
size_t dsc_total_virtual_fabric_count(void);
size_t dsc_total_service_id_record_count(void);
size_t dsc_total_path_record_count(void);
size_t dsc_total_pkey_count(void);

void dsc_topology_cleanup(void);
FSTATUS dsc_topology_init(void);

/*
 * Used for parsing & printing command line arguments.
 */
#define DSC_MAX_SIDS
int dsc_service_id_range_parser(char *str, void *ptr);
char *dsc_service_id_range_printer(void *ptr);
extern QUICK_LIST sid_range_args;

#if defined(__cplusplus)
}
#endif

#endif
