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

  \file dsap_topology.h
 
  $Revision: 1.4 $
  $Date: 2015/01/27 23:00:24 $

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

typedef struct dsap_pkey {
	LIST_ITEM item;
	uint16_t  pkey; /* Stored in network order */
} dsap_pkey_t;

typedef struct dsap_path_record {
	LIST_ITEM         item;
	IB_PATH_RECORD_NO path; /* Stored in network order */
} dsap_path_record_t;

typedef struct dsap_src_port {
	LIST_ITEM     item;
	union ibv_gid gid;    /* Stored in network order */
	char hfi_name[IBV_SYSFS_NAME_MAX];
	uint8_t port_num;
	unsigned base_lid;
	unsigned lmc;
	unsigned state;
	QUICK_LIST pkey_list;
	QUICK_LIST path_record_list;
} dsap_src_port_t;

typedef struct dsap_dst_port {
	LIST_ITEM     item;
	union ibv_gid gid;    /* Stored in network order */
	NODE_TYPE     node_type;
	char          node_desc[NODE_DESCRIPTION_ARRAY_SIZE]; 
} dsap_dst_port_t;

typedef struct dsap_service_id_range {
	int warned; /* set if an error has been emitted for this SID range. */
	uint64_t lower_service_id; /* Stored in network order */
	uint64_t upper_service_id; /* Stored in network order */
} dsap_service_id_range_t;

typedef struct dsap_service_id_record {
	LIST_ITEM              item;
	dsap_service_id_range_t service_id_range;
} dsap_service_id_record_t;

typedef struct dsap_virtual_fabric {
	LIST_ITEM          item;
	STL_VFINFO_RECORD vfinfo_record; /* Stored in network order */
	QUICK_LIST         service_id_record_list;
} dsap_virtual_fabric_t;

typedef struct dsap_subnet {
	LIST_ITEM     item;
	uint64_t      subnet_prefix;  /* Stored in network order */
	QUICK_LIST    src_port_list;
	QUICK_LIST    dst_port_list;
	QUICK_LIST    virtual_fabric_list;
	int           current; /* 0 = needs to be updated. */
} dsap_subnet_t;

struct dsap_port;

/* PKEY Handling functions */
size_t dsap_pkey_count(dsap_src_port_t *src_port);
FSTATUS dsap_add_pkey(dsap_src_port_t *src_port, uint16_t pkey);
FSTATUS dsap_empty_pkey_list(dsap_src_port_t *src_port);
FSTATUS dsap_remove_pkey(dsap_src_port_t *src_port, uint16_t pkey);

/* Path Record Handling functions */
size_t dsap_path_record_count(dsap_src_port_t *src_port);
size_t dsap_subnet_path_record_count(dsap_subnet_t *subnet);
FSTATUS dsap_add_path_record(dsap_src_port_t *src_port,
			     IB_PATH_RECORD_NO *path_record);
FSTATUS dsap_empty_path_record_list(dsap_src_port_t *src_port);
FSTATUS dsap_remove_path_records(dsap_src_port_t *src_port,
				 union ibv_gid *dst_port_gid);

/* Src Port Handling functions */
size_t dsap_src_port_count(dsap_subnet_t *subnet);
FSTATUS dsap_update_src_port(dsap_src_port_t *src_port,
			     struct dsap_port *port);
FSTATUS dsap_add_src_port(struct dsap_port *port);
FSTATUS dsap_empty_src_port_list(dsap_subnet_t *subnet);
FSTATUS dsap_remove_src_port(union ibv_gid *src_gid);

/* Dst Port Handling functions */
size_t dsap_dst_port_count(dsap_subnet_t *subnet);
FSTATUS dsap_add_dst_port(union ibv_gid *dst_port_gid, NODE_TYPE node_type,
			  char *node_desc);
FSTATUS dsap_empty_dst_port_list(dsap_subnet_t *subnet);
FSTATUS dsap_remove_dst_port(union ibv_gid *dst_port_gid);

/* ServiceID Record Handling functions */
size_t dsap_service_id_record_count(dsap_virtual_fabric_t *virtual_fabric);
FSTATUS dsap_add_service_id_record(dsap_virtual_fabric_t *virtual_fabric,
				   dsap_service_id_range_t *service_id_range);
FSTATUS dsap_empty_service_id_record_list(
   dsap_virtual_fabric_t *virtual_fabric);
FSTATUS dsap_remove_service_id_record(
   dsap_virtual_fabric_t *virtual_fabric,
   dsap_service_id_range_t *service_id_range);

/* Virtual Fabric Record Handling functions */
size_t dsap_virtual_fabric_count(dsap_subnet_t *subnet);
FSTATUS dsap_add_virtual_fabric(dsap_subnet_t *subnet,
				STL_VFINFO_RECORD *vfinfo_record);
FSTATUS dsap_empty_virtual_fabric_list(dsap_subnet_t *subnet);
FSTATUS dsap_remove_virtual_fabric(dsap_subnet_t *subnet, uint8_t *vfName);

/* Subnet Handling functions */
size_t dsap_subnet_count(void);
dsap_subnet_t* dsap_get_subnet_at(uint32_t index);
FSTATUS dsap_add_subnet(uint64_t subnet_prefix);
FSTATUS dsap_empty_subnet_list(void);

/* Search Utility Routines */
dsap_subnet_t* dsap_find_subnet(uint64_t *subnet_prefix);
dsap_src_port_t* dsap_find_src_port(union ibv_gid *gid);
dsap_src_port_t* dsap_find_src_port_by_lid(unsigned *lid);
dsap_path_record_t * dsap_find_path_record(dsap_src_port_t *src_port,
					   IB_PATH_RECORD_NO *path);
dsap_dst_port_t* dsap_find_dst_port(union ibv_gid *gid);
dsap_dst_port_t* dsap_find_dst_port_by_name(char *node_desc);
dsap_virtual_fabric_t* dsap_find_virtual_fabric(uint8_t *vfName,
						dsap_subnet_t *subnet);

size_t dsap_tot_src_port_count(void);
size_t dsap_tot_dst_port_count(void);
size_t dsap_tot_vfab_count(void);
size_t dsap_tot_sid_rec_count(void);
size_t dsap_tot_path_rec_count(void);
size_t dsap_tot_pkey_count(void);

void dsap_topology_cleanup(void);
FSTATUS dsap_topology_init(void);

/*
 * Used for parsing & printing command line arguments.
 */
int dsap_service_id_range_parser(char *str, void *ptr);
char *dsap_service_id_range_printer(void *ptr);
extern QUICK_LIST sid_range_args;

#endif
