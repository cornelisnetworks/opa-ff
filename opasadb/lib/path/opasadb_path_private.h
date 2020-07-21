/* BEGIN_ICS_COPYRIGHT2 ****************************************

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

** END_ICS_COPYRIGHT2   ****************************************/

/* [ICS VERSION STRING: unknown] */

#if !defined(_OPA_SA_DB_PATH_PRIVATE_H)

#define _OPA_SA_DB_PATH_PRIVATE_H
#define OPA_SA_DB_PATH_TABLE_VERSION 3

#include <linux/types.h>
#include <iba/ibt.h>


#include <opasadb_path.h>

/*
 * NOTE: THIS CODE IS NOT FOR USE EXCEPT BY DISCOVERY AND OPA_SA_DB.
 * OTHER USERS SHOULD STICK TO USING OPA_SA_DB_PATH.H.
 *
 * opasadb exposes the path table as a set of shared-memory
 * regions using a no-lock mechanism. See op_ppath_shared_table_t 
 * for a description of how access is managed.
 *
 * Unless noted, all functions return 0 on success, non-zero on error.
 * The non-zero error will be one of the standard (errno.h) error codes.
 *
 * Note: all data should fall on 4-byte boundaries.
 *
 * Note: All table types except shared_table_t begin with 4 32-bit
 * unsigned values. The first two indicate the maximum length of the 
 * table in bytes. (Most of the tables only use one length. In that 
 * case, the second field is reserved.) The second two indicate the 
 * number of records that are actually populated.
 */

// Note that all name lengths fall on 4-byte boundaries.

// The maximum length of one of names of the port, vfab and path tables.
#define SHM_NAME_LENGTH 32

// Cribbed from /usr/include/infiniband/verbs.h
#define HFI_NAME_LENGTH 64

// Pulled right out of thin air. TBD: What's the correct length?
#define VFAB_NAME_LENGTH 32

// There's some confusion as to the number of pkeys a port can have.
// I've always believed it was 32, but I can't find a spec indicating
// that value or any other.
#define PKEY_TABLE_LENGTH 32

// The shared table uses a "well known" name, all others have
// unique suffixes that change with each update.
#define SHM_TABLE_NAME "INTEL_SA_DSC"
#define SUBNET_TABLE_NAME "_SUB_"
#define VFAB_TABLE_NAME "_VFAB_"
#define PORT_TABLE_NAME "_PORT_"
#define PATH_TABLE_NAME "_PATH_"

// Must be a power of 2 and (HASH_TABLE_MASK-1), respectively.
//#define HASH_TABLE_SIZE 16384
//#define HASH_TABLE_MASK 16383
// These are for developer testing only.
#define HASH_TABLE_SIZE 4096
#define HASH_TABLE_MASK 4095

// How many times to retry a path query before giving up.
#define MAX_RETRIES 5

/*
 * op_ppath_shared_table_t
 *
 * abi_version		- used to detect collisions between different versions
 * XX_update_count	- allows clients to detect when the published data has
 *					  changed.
 * XX_table_name	- the name of the shared memory object containing
 *					  the named table.
 *
 * The shared memory model is designed to be lockless. This is done via
 * a "publish" model. The tables named in this structure are "published"
 * and will never change. They can, however, be replaced by new versions.
 * When that happens, the update_count is incremented. Clients cache
 * a copy of the update_count in local memory. When they notice that the
 * published number no longer matches their cached value, they close and
 * re-open the shared memory tables, allowing them to access the new
 * versions.
 *
 * In this way, even if the update count changes in the middle of a query,
 * the client can detect it an repeat the query.
 * 
 * Note that an update_count of 0 has a special meaning - it means no data
 * has been published yet.
 */
typedef struct {
	uint32 	abi_version;

	uint32 	port_update_count;
	uint32 	subnet_update_count;
	uint32 	vfab_update_count;
	uint32 	path_update_count;

	uint64 	reserved;

	char	port_table_name[SHM_NAME_LENGTH];
	char	subnet_table_name[SHM_NAME_LENGTH];
	char	vfab_table_name[SHM_NAME_LENGTH];
	char	path_table_name[SHM_NAME_LENGTH];
} op_ppath_shared_table_t;

/* 
 * All the data tables begin with 4 32-bit fields.  This lets us get some 
 * info about the table without knowing which type it is.
 * 
 * All *_table_t types should begin with the same structure.
 */
typedef struct {
    // Maximum size of the tables, in bytes.
    uint32 s1;
    uint32 s2;
    // Current size of the tables, in records.
    uint32 c1;
    uint32 c2;
} op_ppath_header_t;

typedef struct {
	char	hfi_name[HFI_NAME_LENGTH];
	uint16	port;
	uint16	subnet_id;
	uint16	base_lid;
	uint16	lmc;
	uint64	source_prefix;
	uint64	source_guid;
	uint16	pkey[PKEY_TABLE_LENGTH]; // table ends with a zero value.
} op_ppath_port_record_t;

typedef struct {
	uint32 	size;
	uint32 	reserved;
	uint32 	count;
	uint32 	reserved2;
	op_ppath_port_record_t port[];
} op_ppath_port_table_t;

#define PORT_TABLE_SIZE(ps) (sizeof(op_ppath_port_table_t) + sizeof(op_ppath_port_record_t)*(ps+1))

typedef struct {
	uint64	source_prefix;
	uint32	first_sid;
	uint32	reserved;
} op_ppath_subnet_record_t;

typedef struct {
	uint64 	lower_sid;
	uint64 	upper_sid;
	uint32 	vfab_id;
	uint32	next;
} op_ppath_sid_record_t;

typedef struct {
	uint32	subnet_size;
	uint32	sid_size;
	uint32	subnet_count;
	uint32	sid_count;
	op_ppath_subnet_record_t subnet[];
	// op_ppath_sid_record_t sid[];
} op_ppath_subnet_table_t;

#define SID_TABLE_SIZE(ss) (sizeof(op_ppath_sid_record_t)*(ss+1))
#define SUBNET_TABLE_SIZE(fs) (sizeof(op_ppath_subnet_table_t)+sizeof(op_ppath_subnet_record_t)*(fs+1))

typedef struct {
	char 	vfab_name[VFAB_NAME_LENGTH];
	uint64	source_prefix;
	uint16	pkey;
	uint16	sl;
	uint32	first_dlid[HASH_TABLE_SIZE];
	uint32	first_dguid[HASH_TABLE_SIZE];
} op_ppath_vfab_record_t;
	
typedef struct {
	uint32 size;
	uint32 reserved;
	uint32 count;
	uint32 reserved2;
	op_ppath_vfab_record_t vfab[];
} op_ppath_vfab_table_t;

#define VFAB_TABLE_SIZE(vfs) (sizeof(op_ppath_vfab_table_t)+sizeof(op_ppath_vfab_record_t)*(vfs+1))

typedef struct {
	IB_PATH_RECORD_NO 	path;
	uint32				flags; // if 0, this record is unused. 
	uint32				reserved;
	uint32 				next_guid; 	
	uint32				next_lid;  	
} __attribute__((packed)) op_ppath_record_t;


typedef struct {
	uint32				size; 
	uint32				reserved;
	uint32				count; 
	uint32				reserved2;
	op_ppath_record_t	table[];
} __attribute__((packed)) op_ppath_table_t;

#define PATH_TABLE_SIZE(ps) (sizeof(op_ppath_table_t)+sizeof(op_ppath_record_t)*(ps+1))


/*
 *******************
 * API FOR BOTH WRITER & READERS
 *******************
 */

typedef struct {
	op_ppath_shared_table_t *shared_table; 
	op_ppath_port_table_t	*port_table;   
	op_ppath_subnet_table_t	*subnet_table;
	op_ppath_vfab_table_t	*vfab_table;     
	op_ppath_sid_record_t	*sid_table; // Actually points into port_table mem.
	op_ppath_table_t		*path_table;   

	// file descriptors for mmapped data.
	int						shared_fd; 	 
	int						port_fd;
	int						subnet_fd;
	int						vfab_fd;
	int						path_fd;

	// Compared with the versions in shared_table to detect updates.
	uint32 					old_port_update_count;
	uint32 					old_subnet_update_count;
	uint32 					old_vfab_update_count;
	uint32 					old_path_update_count;
} op_ppath_reader_t;

/*
 * Convenience function. Allocates a reader (or writer) object.
 * Used to help isolate IBAccess and OpenIB from each other.
 */
void *op_ppath_allocate_reader(void);
void *op_ppath_allocate_writer(void);

/*
 * Opens shared memory tables for reading only.
 * Returns 0 on success (and fills in the reader structure)
 * Non-zero on error. (see errno.h)
 */
int op_ppath_create_reader(op_ppath_reader_t *r);

/*
 * Search by query & mask. Because of the structure of 
 * the shared data, you must specify EITHER:
 * 
 * hfi_name, port, source lid and destination lid
 * 
 * OR:
 * 
 * source gid, destination gid.
 *
 * The only other parameters that can be used at
 * this point are the SID and the PKEY. All other
 * fields are ignored.
 */
int op_ppath_find_path(op_ppath_reader_t *r,
                       char *hfi_name,
                       uint16 port,
                       uint64 mask,
                       IB_PATH_RECORD_NO *query,
                       IB_PATH_RECORD_NO *result);

/*
 * MHEINZ: TBD: add functions to examine the virtual fabric data?
 */

void op_ppath_close_tables(op_ppath_reader_t *reader);


/*
 *******************
 * API FOR WRITER ONLY (discovery)
 *******************
 */

typedef struct {
	/*
 	 * The "published" version of the data.
	 */
	op_ppath_reader_t	published;

	/*
	 * The "unpublished" version.
	 */	
	op_ppath_reader_t	unpublished;

	/*
	 * The size of each table, in records.
	 */
	uint32				max_ports;
	uint32				max_sids;
	uint32				max_subnets;
	uint32				max_vfabs;
	uint32 				max_paths;  
} op_ppath_writer_t;


/*
 * Initializes the structures needed to write the shared memory tables.
 * read/write access. Returns zero on success or non-zero on error.
 * If the tables already exist this function will restore contact with them. 
 */
int op_ppath_create_writer(op_ppath_writer_t *w);

/*
 * Each of the next 4 functions creates a new, unpublished, shared memory 
 * region for the named data. If an unpublished region already exists,
 * it is unlinked.
 */
int op_ppath_initialize_ports(op_ppath_writer_t *w, 
							  unsigned max_ports);

int op_ppath_initialize_subnets(op_ppath_writer_t *w, 
								unsigned max_subnets,
								unsigned max_sids);

int op_ppath_initialize_vfabrics(op_ppath_writer_t *w, 
								 unsigned max_vfabs);

int op_ppath_initialize_paths(op_ppath_writer_t *w, 
							  unsigned max_paths);

/*
 * Adds a subnet to the unpublished subnet table.
 *
 * Returns 0 on success, or -ENOMEM if the table is full.
 */
int op_ppath_add_subnet(op_ppath_writer_t *w, uint64 prefix);

/*
 * Adds a virtual fabric to the unpublished vfab table.
 *
 * Returns 0 on success, or -ENOMEM if the table is full.
 */
int op_ppath_add_vfab(op_ppath_writer_t *w, 
					  char *name,
					  uint64 prefix,
					  uint16 pkey,
					  uint16 sl);

/*
 * Adds a port to the port table.
 *
 * Returns 0 on success, or -ENOMEM if the table is full.
 */
int op_ppath_add_port(op_ppath_writer_t *w, op_ppath_port_record_t port);

/*
 * Adds a SID range to the port & vfab tables. If the upper sid is zero,
 * it is assumed that this is a single SID, rather than a range.
 *
 * Returns 0 on success, or -ENOMEM if the table is full.
 */
int op_ppath_add_sid(op_ppath_writer_t *w, 
					 uint64 prefix,
					 uint64 lower_sid,
					 uint64 upper_sid,
					 char *vfab_name);

/*
 * Adds a path record to the path table and links it to the appropriate
 * virtual fabric (based on the source guid and SID).
 *
 * Returns 0 on success, or -ENOMEM if the table is full.
 */
int op_ppath_add_path(op_ppath_writer_t *w, IB_PATH_RECORD_NO *record);

/*
 * Replaces existing tables with new "published" versions. 
 * Unaltered tables are not changed.
 */
void op_ppath_publish(op_ppath_writer_t *w);

/*
 * Closes the writer and unlinks the shared memory blocks.
 */
void op_ppath_close_writer(op_ppath_writer_t *w);

/*
 * Closes the reader does not unlink the shared memory blocks.
 */
 void op_ppath_close_reader(op_ppath_reader_t *r);

 /*
  * Returns the version # of the private API.
  */
unsigned op_ppath_version(void);

#endif
