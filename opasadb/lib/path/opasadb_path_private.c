/* BEGIN_ICS_COPYRIGHT2 ****************************************

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

** END_ICS_COPYRIGHT2   ****************************************/

/* [ICS VERSION STRING: unknown] */
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#include <unistd.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <sched.h>
#include <opasadb_debug.h>
#include "opasadb_path_private.h"

// 0 is a "reserved value" for the update counters.
#define BUMP_COUNT(x) { (x)++; if ((x)==0) (x)=1; }

/*
 * Fastest way to process LMC..
 */
uint16_t lmc_mask[] = {
	0xffff, // LMC of zero
	0xfffe, // LMC of one
	0xfffc, // LMC of two
	0xfff8, // LMC of three.
	0xfff0, // LMC of four.
	0xffe0, // LMC of five.
	0xffc0, // LMC of six.
	0xff80, // LMC of seven.
};

void *
op_ppath_allocate_reader(void)
{
	void *p = malloc(sizeof(op_ppath_reader_t));
	return p;
}

void *
op_ppath_allocate_writer(void)
{
	void *p = malloc(sizeof(op_ppath_writer_t));
	return p;
}

static uint32
gid_hash(IB_GID_NO *gid)
{
    uint32 hash=0;
    int i;

    for (i=0; i<16;i++) {
        hash += gid->Raw[i];
    }

    return hash & HASH_TABLE_MASK;
}

#define GID_HASH(g) (gid_hash(g))
#define LID_HASH(i) ((ntohs((uint32)(i)) & HASH_TABLE_MASK))

static void 
close_and_unlink_tables(op_ppath_reader_t *r)
{
	char port_table_name[SHM_NAME_LENGTH];
	char subnet_table_name[SHM_NAME_LENGTH];
	char vfab_table_name[SHM_NAME_LENGTH];
	char path_table_name[SHM_NAME_LENGTH];

	_DBG_FUNC_ENTRY;
	/* The call to op_ppath_close_reader() will unmapped the shared table
	   and therefore we can't refer to it anymore. Copy the table names
	   here for unlinking */
	strncpy(port_table_name, r->shared_table->port_table_name, SHM_NAME_LENGTH -1);
	port_table_name[SHM_NAME_LENGTH - 1] = '\0';
	strncpy(subnet_table_name, r->shared_table->subnet_table_name, SHM_NAME_LENGTH -1);
	subnet_table_name[SHM_NAME_LENGTH - 1] = '\0';
	strncpy(vfab_table_name, r->shared_table->vfab_table_name, SHM_NAME_LENGTH -1);
	vfab_table_name[SHM_NAME_LENGTH - 1] = '\0';
	strncpy(path_table_name, r->shared_table->path_table_name, SHM_NAME_LENGTH -1);
	path_table_name[SHM_NAME_LENGTH - 1] = '\0';

	op_ppath_close_reader(r);

	/* Finally, unlink the tables */
	if (strlen(port_table_name))
		shm_unlink(port_table_name);
	if (strlen(subnet_table_name))
		shm_unlink(subnet_table_name);
	if (strlen(vfab_table_name))
		shm_unlink(vfab_table_name);
	if (strlen(path_table_name))
		shm_unlink(path_table_name);

	_DBG_FUNC_EXIT;
}

void
op_ppath_close_reader(op_ppath_reader_t *r)
{
	_DBG_FUNC_ENTRY;

	if (r->path_table && r->path_table != MAP_FAILED) {
		munmap(r->path_table,r->path_table->size);
		r->path_table=NULL;
	}
	if (r->port_table && r->port_table != MAP_FAILED) {
		munmap(r->port_table,r->port_table->size);
		r->port_table=NULL;
	}
	if (r->vfab_table && r->vfab_table != MAP_FAILED) {
		munmap(r->vfab_table,r->vfab_table->size);
		r->vfab_table=NULL;
	}
	if (r->subnet_table && r->subnet_table != MAP_FAILED) {
		munmap(r->subnet_table,
			   r->subnet_table->subnet_size + 
			   r->subnet_table->sid_size);
		r->subnet_table=NULL;
	}

	if (r->path_fd>0) { close(r->path_fd); r->path_fd=0; }
	if (r->port_fd>0) { close(r->port_fd); r->port_fd=0; }
	if (r->subnet_fd>0) { close(r->subnet_fd); r->subnet_fd=0; }
	if (r->vfab_fd>0) { close(r->vfab_fd); r->vfab_fd=0; }

	/* Use r->shared_fd to differentiate a "unpublished" reader from
	   a "published" reader for a writer */
	if (r->shared_fd > 0 && r->shared_table && 
		r->shared_table != MAP_FAILED) {
		munmap(r->shared_table,sizeof(op_ppath_shared_table_t));
		r->shared_table=NULL;
	}
	if (r->shared_fd) {
		close(r->shared_fd);
		r->shared_fd = 0;
	}

	_DBG_FUNC_EXIT;
}
	
void
op_ppath_close_writer(op_ppath_writer_t *w)
{
	_DBG_FUNC_ENTRY;

	close_and_unlink_tables(&w->published);
	close_and_unlink_tables(&w->unpublished);

	if (w->unpublished.shared_table) {
		free(w->unpublished.shared_table);
	}

	_DBG_FUNC_EXIT;
}

/*
 * Opens and maps the shared table with the name shm_name
 * and permissions as specified by the rw flags.
 */
static int
open_shared_table(op_ppath_reader_t *r, char *name, int rw)
{
	int err= -1;
	int prot;

	_DBG_FUNC_ENTRY;

	if (rw & O_RDWR) {
		prot = PROT_READ | PROT_WRITE;
	} else {
		prot = PROT_READ;
	}

	r->shared_fd = shm_open(name, rw, 0644);
	if (r->shared_fd < 0) {
		_DBG_DEBUG("Failed to open %s\n",name);
		goto error;
	}
	
	if (rw & O_CREAT) {
		/* Create the file at the specified size. */
		if (ftruncate(r->shared_fd, sizeof(op_ppath_shared_table_t))) {
			_DBG_ERROR("Unable to size %s\n",name);
			goto error;
		}
	}

	r->shared_table = mmap(0, 
						   sizeof(op_ppath_shared_table_t),
						   prot,
						   MAP_SHARED,
						   r->shared_fd,
						   0);
	if (r->shared_table == MAP_FAILED) {
		_DBG_DEBUG("Failed to map %s to memory.\n",name);
		goto error;
	}

	_DBG_FUNC_EXIT;
	return 0;
error:
	err = errno;
	if (r->shared_fd > 0) {
		close(r->shared_fd);
		if (rw & O_CREAT) 
			shm_unlink(name);
	}
	r->shared_fd = 0;
	r->shared_table = NULL;
	_DBG_FUNC_EXIT;
	return err;
}

enum {
	PORT_TABLE, 
	PATH_TABLE,
	SUBNET_TABLE,
	VFAB_TABLE,
	SID_TABLE
};

/*
 * The following function opens and maps one of the other shared tables.
 *
 * table 	- the type of table being opened/created.
 * rw 		- file access flags.
 * c		- # of items to allocate if creating.
 * c2		- # of secondary items to allocate if creating SUBNET_TABLE, else 0.
 */
static int
open_ppath_table(op_ppath_reader_t *r, int table, int rw, unsigned c, unsigned c2)
{
	int err=0;
	int prot;
	char *name = NULL;
	op_ppath_header_t *h;
	off_t size1, size2;
	int fd = -1;

	_DBG_FUNC_ENTRY;

	switch (table) {
		case PORT_TABLE:
			name = r->shared_table->port_table_name; 
			size1 = PORT_TABLE_SIZE(c); 
			size2 = 0;
			if (rw & O_CREAT) {
				BUMP_COUNT(r->shared_table->port_update_count);
				sprintf(name, 
					SHM_TABLE_NAME PORT_TABLE_NAME "%06u", 
					(unsigned int)r->shared_table->port_update_count);
			}
			break;
		case PATH_TABLE:
			name = r->shared_table->path_table_name; 
			size1 = PATH_TABLE_SIZE(c);
			size2 = 0;
			if (rw & O_CREAT) {
				BUMP_COUNT(r->shared_table->path_update_count);
				sprintf(name, 
					SHM_TABLE_NAME PATH_TABLE_NAME "%06u", 
					(unsigned int)r->shared_table->path_update_count);
			}
			break;
		case VFAB_TABLE:
			name = r->shared_table->vfab_table_name; 
			size1 = VFAB_TABLE_SIZE(c);
			size2 = 0;
			if (rw & O_CREAT) {
				BUMP_COUNT(r->shared_table->vfab_update_count);
				sprintf(name, 
					SHM_TABLE_NAME VFAB_TABLE_NAME "%06u", 
					(unsigned int)r->shared_table->vfab_update_count);
			}
			break;
		case SUBNET_TABLE:
			name = r->shared_table->subnet_table_name; 
			size1 = SUBNET_TABLE_SIZE(c);
			size2 = SID_TABLE_SIZE(c2);
			if (rw & O_CREAT) {
				BUMP_COUNT(r->shared_table->subnet_update_count);
				sprintf(name, 
					SHM_TABLE_NAME SUBNET_TABLE_NAME "%06u", 
					(unsigned int)r->shared_table->subnet_update_count);
			}
			break;
		default:
			size1 = size2 = 0;
			_DBG_ERROR("Bad table specified. (%d)\n", table);
			goto error;
	}

	if (rw & O_CREAT) {
		prot = PROT_READ | PROT_WRITE;
	} else {
		// Note that this overrides the sizes set above.
		// We will get the real sizes from the header.
		prot = PROT_READ;
		size1 = sizeof(op_ppath_header_t);
		size2 = 0;
	}

	fd = shm_open(name, rw, 0644);
	if (fd < 0) {
		_DBG_ERROR("Failed to open %s\n",name);
		goto error;
	}
	
	if (rw & O_CREAT) {
		/* Create the file at the specified size. */
		if (ftruncate(fd, size1 + size2)) {
			_DBG_ERROR("Unable to size %s\n",name);
			goto error;
		}
	}

	h = (op_ppath_header_t*)mmap(0, size1 + size2, prot, MAP_SHARED, fd, 0);
	if (h == MAP_FAILED) {
		_DBG_ERROR("Unable to map %s\n",name);
		goto error;
	}

	if (rw & O_CREAT) {
		/*
		 * clear the table & set the maximum lengths.
		 */
		memset((char*)h,0,size1+size2);
		h->s1 = size1; 
		h->s2 = size2;
	} else {
		/* 
		 * In this case, read the size out of the existing file
		 * and re-map to the correct size.
		 */
		off_t size3 = h->s1 + h->s2;
		munmap(h,size1+size2);
		h = (op_ppath_header_t*)mmap(0, size3, prot, MAP_SHARED, fd, 0);

		if ((void *)h == MAP_FAILED) goto error;
	}

	// There are definitely times when OOP would be a big win.
	switch (table) {
		case PORT_TABLE:
			r->old_port_update_count = r->shared_table->port_update_count;
			r->port_table = (op_ppath_port_table_t*)h;
			r->port_fd = fd;
			break;
		case PATH_TABLE:
			r->old_path_update_count = r->shared_table->path_update_count;
			r->path_table = (op_ppath_table_t*)h;
			r->path_fd = fd;
			break;
		case VFAB_TABLE:
			r->old_vfab_update_count = r->shared_table->vfab_update_count;
			r->vfab_table = (op_ppath_vfab_table_t*)h;
			r->vfab_fd = fd;
			break;
		case SUBNET_TABLE:
			// The special case that makes much of this necessary.
			// The sid table is appended to the end of the subnet table.
			r->subnet_table = (op_ppath_subnet_table_t*)h;
			r->sid_table = (op_ppath_sid_record_t*)(((char*)h)+(h->s1));
			r->subnet_fd = fd;
			r->old_subnet_update_count = r->shared_table->subnet_update_count;
			break;
	}
	_DBG_FUNC_EXIT;
	return 0;

error:
	if (fd >= 0) {
		close(fd);
		if (rw & O_CREAT)
			shm_unlink(name);
	}
	err = errno;
	_DBG_FUNC_EXIT;
	return err;
}

/*
 * Closes the specified table.
 */
static void
close_ppath_table(op_ppath_reader_t *r, int table)
{
	_DBG_FUNC_ENTRY;

	switch(table) {
		case PATH_TABLE:
			if (r->path_table && r->path_table != MAP_FAILED) {
				munmap(r->path_table,r->path_table->size);
				r->path_table=NULL;
			}
			if (r->path_fd>0) { close(r->path_fd); r->path_fd=0; }
			break;
		case PORT_TABLE:
			if (r->port_table && r->port_table != MAP_FAILED) {
				munmap(r->port_table,r->port_table->size);
				r->port_table=NULL;
			}
			if (r->port_fd>0) { close(r->port_fd); r->port_fd=0; }
			break;
		case VFAB_TABLE:
			if (r->vfab_table && r->vfab_table != MAP_FAILED) {
				munmap(r->vfab_table,r->vfab_table->size);
				r->vfab_table=NULL;
			}
			if (r->vfab_fd>0) { close(r->vfab_fd); r->vfab_fd=0; }
			break;
		case SUBNET_TABLE:
			if (r->subnet_table && r->subnet_table != MAP_FAILED) {
				munmap(r->subnet_table,
			   r->subnet_table->subnet_size + 
			   r->subnet_table->sid_size);
				r->subnet_table=NULL;
			}
			if (r->subnet_fd>0) { close(r->subnet_fd); r->subnet_fd=0; }
			break;
	}

	_DBG_FUNC_EXIT;
}

static int
reopen_ppath_table(op_ppath_reader_t *r, int table, int rw, unsigned c, unsigned c2)
{
	close_ppath_table(r,table);
	return open_ppath_table(r,table,rw,c,c2);
}

/*
 * Tries to connect to an existing shared_table and,
 * if that fails, creates a new one.
 */
static int
connect_shared_table_rw(op_ppath_reader_t *r)
{
	int err;

	_DBG_FUNC_ENTRY;
	//err = open_shared_table(r, SHM_TABLE_NAME, O_RDWR);
	err = open_shared_table(r, SHM_TABLE_NAME, O_RDWR | O_CREAT);
	if (err) {
		_DBG_ERROR("Unable to create shared memory table.\n");
	} else {
		memset(r->shared_table,0,sizeof(op_ppath_shared_table_t));
		r->shared_table->abi_version = OPA_SA_DB_PATH_TABLE_VERSION;
	}
	_DBG_FUNC_EXIT;
	return err;
}

/*
 * Tries to connect to an existing shared_table.
 * 
 * Returns non-zero on error.
 */
static int
connect_shared_table(op_ppath_reader_t *r)
{
	int err;

	_DBG_FUNC_ENTRY;

	err = open_shared_table(r, SHM_TABLE_NAME, O_RDONLY);
	if (err) {
		_DBG_ERROR("Unable to open shared memory table.\n");
	} else if (r->shared_table->abi_version != OPA_SA_DB_PATH_TABLE_VERSION) {
		_DBG_ERROR("Incorrect ABI version.\n");
	}
	_DBG_FUNC_EXIT;
	return err;
}

/*
 * Tries to disconnect from an existing shared_table.
 * This function is only called when it fails to 
 * create the writer after connecting to the shared table 
 * successfully. It covers only the last part of 
 * op_ppath_create_writer(). 
 */
static void
disconnect_shared_table(op_ppath_reader_t *r)
{
	_DBG_FUNC_ENTRY;

	if (r->shared_table && r->shared_table != MAP_FAILED) {
		munmap(r->shared_table, sizeof(op_ppath_shared_table_t));
		r->shared_table=NULL;
	}

	if (r->shared_fd > 0) {
		close(r->shared_fd);
		shm_unlink(SHM_TABLE_NAME);
	}
	_DBG_FUNC_EXIT;
}

/*
 * Initializes the structures needed to write the shared memory tables.
 * read/write access. Returns zero on success or non-zero on error.
 * If the tables already exist this function will restore contact with them.
 */
int
op_ppath_create_writer(op_ppath_writer_t *w)
{
	int err = 0;

	_DBG_FUNC_ENTRY;

	memset(w, 0, sizeof(op_ppath_writer_t));

	err = connect_shared_table_rw(&(w->published));
	if (err) goto error;


	/*
	 * The "unpublished" table isn't shared, so we allocate local memory. 
	 * In this case, the shared_fd is 0, which can be used to differentiate 
	 * "unpublished" from "published" table. 
	 */
	w->unpublished.shared_table = malloc(sizeof(op_ppath_shared_table_t));
	if (!(w->unpublished.shared_table)) {
		err = ENOMEM;
		disconnect_shared_table(&(w->published));
		goto error;
	}

	memset(w->unpublished.shared_table,0,sizeof(op_ppath_shared_table_t));

	/* We do this to ensure that when we open writeable tables,
	 * they will have an update count greater than the currently
	 * published tables. (If we did not find currently published
	 * tables, then we're just copying zeros.)
	 */
	w->unpublished.shared_table->port_update_count = w->published.shared_table->port_update_count;
	w->unpublished.shared_table->subnet_update_count = w->published.shared_table->subnet_update_count;
	w->unpublished.shared_table->vfab_update_count = w->published.shared_table->vfab_update_count;
	w->unpublished.shared_table->path_update_count = w->published.shared_table->path_update_count;
	
error:
	_DBG_FUNC_EXIT;
	return err;
}

/*
 * Opens the shared memory segments for read-only access.
 *
 * Note that if any of the opens fails, it closes everything
 * and tries again, up to MAX_RETRIES times, on the assumption
 * that the failure indicates a collision with discovery.
 */ 
int 
op_ppath_create_reader(op_ppath_reader_t *r)
{
	int err = 0;
	
	_DBG_FUNC_ENTRY;

	memset(r, 0, sizeof(op_ppath_reader_t));

	do {
		err = connect_shared_table(r);
		if (err) continue;
	
		err = open_ppath_table(r, PORT_TABLE, O_RDONLY, 0, 0);
		if (err) { op_ppath_close_reader(r); continue; }

		err = open_ppath_table(r, PATH_TABLE, O_RDONLY, 0, 0);
		if (err) { op_ppath_close_reader(r); continue; }
	
		err = open_ppath_table(r, SUBNET_TABLE, O_RDONLY, 0, 0);
		if (err) { op_ppath_close_reader(r); continue; }

		err = open_ppath_table(r, VFAB_TABLE, O_RDONLY, 0, 0);
		if (err) { op_ppath_close_reader(r); continue; }
		
	} while (0);

	_DBG_FUNC_EXIT;
	return err;
}


/*
 * Each of the following functions create a new, unpublished, table.
 * If there is an existing table, it is closed an unmapped
 * before the new one is created.
 */
int 
op_ppath_initialize_ports(op_ppath_writer_t *w, unsigned max_ports)
{
	int err = 0;
	op_ppath_reader_t *r = &(w->unpublished);
	
	_DBG_FUNC_ENTRY;

	close_ppath_table(r,PORT_TABLE);
	
	w->max_ports = max_ports;

	err = open_ppath_table(r, PORT_TABLE, O_RDWR | O_CREAT, max_ports, 0);


	_DBG_FUNC_EXIT;
	return err;
}

int 
op_ppath_initialize_vfabrics(op_ppath_writer_t *w, unsigned max_vfabs)
{
	int err = 0;
	op_ppath_reader_t *r = &(w->unpublished);
	
	_DBG_FUNC_ENTRY;

	close_ppath_table(r,VFAB_TABLE);

	w->max_vfabs = max_vfabs;

	err = open_ppath_table(r, VFAB_TABLE, O_RDWR | O_CREAT, max_vfabs, 0);

	_DBG_FUNC_EXIT;
	return err;
}

int 
op_ppath_initialize_paths(op_ppath_writer_t *w, unsigned max_paths)
{
	int err = 0;
	op_ppath_reader_t *r = &(w->unpublished);
	
	_DBG_FUNC_ENTRY;

	close_ppath_table(r,PATH_TABLE);

	w->max_paths = max_paths;

	err = open_ppath_table(r, PATH_TABLE, O_RDWR | O_CREAT, max_paths, 0);

	_DBG_FUNC_EXIT;
	return err;
}

int 
op_ppath_initialize_subnets(op_ppath_writer_t *w, unsigned max_subnets, unsigned max_sids)
{
	int err = 0;
	op_ppath_reader_t *r = &(w->unpublished);
	
	_DBG_FUNC_ENTRY;

	if (!w) {
		errno = EINVAL;
		goto error;
	}

	close_ppath_table(r,SUBNET_TABLE);

	w->max_subnets = max_subnets; w->max_sids = max_sids;

	err = open_ppath_table(r, SUBNET_TABLE, O_RDWR | O_CREAT, max_subnets, max_sids);

error:
	_DBG_FUNC_EXIT;
	return err;
}

/*
 * The next several functions each add an item to an unpublished table.
 * Returns 0 on success, ENOMEM if the table is full.
 */
int 
op_ppath_add_subnet(op_ppath_writer_t *w, uint64 prefix)
{
	int err = 0;
	op_ppath_reader_t *r = &(w->unpublished);
	_DBG_FUNC_ENTRY;

	if (!w) {
		errno = EINVAL;
		goto error;
	}

	if (r->subnet_table->subnet_count >= w->max_subnets) {
		errno = ENOMEM;
		goto error;
	}

	r->subnet_table->subnet_count++;
	r->subnet_table->subnet[r->subnet_table->subnet_count].source_prefix = prefix;
	r->subnet_table->subnet[r->subnet_table->subnet_count].first_sid = 0;
	r->subnet_table->subnet[r->subnet_table->subnet_count].reserved = 0;
	
	_DBG_FUNC_EXIT;
	return 0;

error:
	_DBG_FUNC_EXIT;
	err = errno;
	return err;
}

int 
op_ppath_add_port(op_ppath_writer_t *w, op_ppath_port_record_t port)
{
	int err = 0;
	int i;
	op_ppath_reader_t *r = &(w->unpublished);

	_DBG_FUNC_ENTRY;

	if (!w) {
		err = EINVAL;
		goto error;
	}

	if (r->port_table->count >= w->max_ports) {
		err = ENOMEM;
		goto error;
	}

	r->port_table->count++;

	for (i=1; i <= r->subnet_table->subnet_count; i++) {
		if (r->subnet_table->subnet[i].source_prefix == port.source_prefix)
			break;
	}

	if (i >  r->subnet_table->subnet_count) {
		_DBG_WARN("Trying to add a port without a matching subnet.\n");
		err = EINVAL;
		goto error;
	}

	port.subnet_id = i;

	r->port_table->port[r->port_table->count] = port;

	_DBG_FUNC_EXIT;
	return 0;

error:
	_DBG_FUNC_EXIT;
	return err;
}

int 
op_ppath_add_sid(op_ppath_writer_t *w, 
				 uint64 prefix, 
				 uint64 lower_sid, 
				 uint64 upper_sid, 
				 char *vfab_name)
{
	int err = 0;
	int i, n;
	uint16 subnet_id;
	op_ppath_reader_t *r = &(w->unpublished);

	_DBG_FUNC_ENTRY;

	if (!w) {
		errno = EINVAL;
		goto error;
	}

	if (r->subnet_table->sid_count >= w->max_sids) {
		errno = ENOMEM;
		goto error;
	}

	// reserve a slot for our new record.
	// note the pre-increment!
	n = ++r->subnet_table->sid_count;

	r->sid_table[n].lower_sid = lower_sid;
	r->sid_table[n].upper_sid = upper_sid;

	// Note that record 0 is reserved.
	for (i=1; i <= r->subnet_table->subnet_count; i++) {
		if (r->subnet_table->subnet[i].source_prefix == prefix)
			break;
	}

	if (i > r->subnet_table->subnet_count) {
		_DBG_WARN("Trying to add a sid without a matching subnet.\n");
		errno = EINVAL;
		goto error;
	}

	subnet_id = i;

	// Note that record 0 is reserved.
	for (i=1; i <= r->vfab_table->count; i++) {
		if (!(strcmp(r->vfab_table->vfab[i].vfab_name, vfab_name)))
			break;
	}

	if (i > r->vfab_table->count) {
		_DBG_WARN("Trying to add a sid without a matching virtual fabric.\n");
		errno = EINVAL;
		goto error;
	}

	r->sid_table[n].vfab_id = i;
	r->sid_table[n].next = r->subnet_table->subnet[subnet_id].first_sid;
	r->subnet_table->subnet[subnet_id].first_sid = n;

	_DBG_FUNC_EXIT;
	return 0;

error:
	_DBG_FUNC_EXIT;
	err = errno;
	return err;
}

/*
 * Adds a virtual fabric to the unpublished vfab table.
 *
 * Returns 0 on success, or ENOMEM if the table is full.
 */
int op_ppath_add_vfab(op_ppath_writer_t *w, 
					  char *name,
					  uint64 prefix,
					  uint16 pkey,
					  uint16 sl)
{
	int err = 0;
	int i;
	op_ppath_reader_t *r = &(w->unpublished);

	_DBG_FUNC_ENTRY;

	if (!w) {
		errno = EINVAL;
		goto error;
	}

	if (r->vfab_table->count >= w->max_vfabs) {
		errno = ENOMEM;
		goto error;
	}

	// Note the pre-increment.
	i = ++r->vfab_table->count;
	snprintf(r->vfab_table->vfab[i].vfab_name, VFAB_NAME_LENGTH, "%s", name);
	r->vfab_table->vfab[i].source_prefix = prefix;
	r->vfab_table->vfab[i].pkey = pkey;
	r->vfab_table->vfab[i].sl = sl;

	_DBG_FUNC_EXIT;
	return 0;

error:
	_DBG_FUNC_EXIT;
	err = errno;
	return err;
}

/*
 * Adds a path to the unpublished path table.
 * Sorts the path into the appropriate subnet and 
 * virtual fabric based on the SID and SGID.
 *
 * Returns 0 on success, or ENOMEM if the table is full.
 */
int op_ppath_add_path(op_ppath_writer_t *w, IB_PATH_RECORD_NO *record)
{
	int err = 0;
	int i;
	
	uint64 ho_sid;

	uint32 lid_hash;
	uint32 guid_hash;

	op_ppath_reader_t *r = &(w->unpublished);
	op_ppath_subnet_record_t *subnet;
	op_ppath_vfab_record_t *vfab;
	op_ppath_sid_record_t *sid;
	op_ppath_record_t *path;

	_DBG_FUNC_ENTRY;

	if (!w) {
		errno = EINVAL;
		goto error;
	}

	for (i=1; i <= r->subnet_table->subnet_count; i++) {
		if (r->subnet_table->subnet[i].source_prefix == record->SGID.Type.Global.SubnetPrefix)
			break;
	}
	if (i > r->subnet_table->subnet_count) {
		_DBG_WARN("Trying to add a path without a matching subnet.\n");
		errno = EINVAL;
		goto error;
	}

	subnet = &(r->subnet_table->subnet[i]);

	ho_sid = ntoh64(record->ServiceID);
	i = subnet->first_sid;
	while (i) {
		uint64 ho_lower_sid;
		uint64 ho_upper_sid;

		sid = &(r->sid_table[i]);
		ho_lower_sid = ntoh64(sid->lower_sid);
		ho_upper_sid = ntoh64(sid->upper_sid);

		if (ho_lower_sid == 0 && ho_sid == 0) {
			// Adding with a blank sid. Add to the default.
			break;
		} else if (ho_upper_sid == 0 && ho_sid == ho_lower_sid) {
			// Exact match.
			break;
		} else if (ho_lower_sid != 0 && 
				   ho_sid >= ho_lower_sid && 
				   ho_sid <= ho_upper_sid) {
			// Matches range.
			break;
		}

		i = sid->next;
	}

	if (!i) {
		_DBG_WARN("Trying to add a path without a matching virtual fabric.\n");
		errno = EINVAL;
		goto error;
	}

	vfab = &(r->vfab_table->vfab[sid->vfab_id]);

	if (r->path_table->count >= w->max_paths) {
		errno = ENOMEM;
		goto error;
	}

	// Note the pre-increment.
	i = ++r->path_table->count;
	r->path_table->table[i].path=(*record);
	r->path_table->table[i].flags=1;
	path = &(r->path_table->table[i]);
	
	guid_hash = GID_HASH(&(record->DGID));
	lid_hash = LID_HASH(record->DLID);

	path->next_lid = vfab->first_dlid[lid_hash];
	path->next_guid = vfab->first_dguid[guid_hash];

	vfab->first_dlid[lid_hash] = i;
	vfab->first_dguid[guid_hash] = i;
	
	_DBG_FUNC_EXIT;
	return 0;

error:
	_DBG_FUNC_EXIT;
	err = errno;
	return err;
}
void
op_ppath_publish(op_ppath_writer_t *w)
{
	_DBG_FUNC_ENTRY;
	op_ppath_shared_table_t *published = w->published.shared_table;
	op_ppath_shared_table_t *unpublished = w->unpublished.shared_table;

	if (published->port_update_count != unpublished->port_update_count) {
		_DBG_INFO("Publishing updated port table.\n");
		close_ppath_table(&w->published, PORT_TABLE);
		shm_unlink(published->port_table_name);
		strcpy(published->port_table_name,
				unpublished->port_table_name);
	}
	if (published->subnet_update_count != unpublished->subnet_update_count) {
		_DBG_INFO("Publishing updated subnet table.\n");
		close_ppath_table(&w->published, SUBNET_TABLE);
		shm_unlink(published->subnet_table_name);
		strcpy(published->subnet_table_name,
				unpublished->subnet_table_name);
	}
	if (published->vfab_update_count != unpublished->vfab_update_count) {
		_DBG_INFO("Publishing updated vfab table.\n");
		close_ppath_table(&w->published, VFAB_TABLE);
		shm_unlink(published->vfab_table_name);
		strcpy(published->vfab_table_name,
				unpublished->vfab_table_name);
	}
	if (published->path_update_count != unpublished->path_update_count) {
		_DBG_INFO("Publishing updated path table.\n");
		close_ppath_table(&w->published, PATH_TABLE);
		shm_unlink(published->path_table_name);
		strcpy(published->path_table_name,
				unpublished->path_table_name);
	}

	/* 
	 * We do these last to try to minimize the risk that a client
 	 * will see the update before it has finished. Updating the names
	 * without updating the counters is safe, because active clients
	 * only look at the counts for changes.
	 */
	published->port_update_count = unpublished->port_update_count;
	published->subnet_update_count = unpublished->subnet_update_count;
	published->vfab_update_count = unpublished->vfab_update_count;
	published->path_update_count = unpublished->path_update_count;

	_DBG_FUNC_EXIT;
}

#if 0
static int 
cmp_rec(uint64 mask, IB_PATH_RECORD_NO *a, IB_PATH_RECORD_NO *b)
{
	if ((mask & IB_PATH_RECORD_COMP_SERVICEID) &&
		(a->ServiceID != b->ServiceID)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_DGID) &&
		memcmp((void*)&(a->DGID), (void*)&(b->DGID), 
			sizeof(a->DGID))) return -1;
	if ((mask & IB_PATH_RECORD_COMP_SGID) &&
		memcmp((void*)&(a->SGID), (void*)&(b->SGID), 
			sizeof(a->SGID))) return -1;
	if ((mask & IB_PATH_RECORD_COMP_DLID) &&
		(a->DLID != b->DLID)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_SLID) &&
		(a->SLID != b->SLID)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_RAWTRAFFIC) &&
		(RAW_TRAFFIC(a->u1) != RAW_TRAFFIC(b->u1))) return -1;
	if ((mask & IB_PATH_RECORD_COMP_FLOWLABEL) &&
		(FLOW_LABEL(a->u1) != FLOW_LABEL(b->u1))) return -1;
	if ((mask & IB_PATH_RECORD_COMP_HOPLIMIT) &&
		(HOP_LIMIT(a->u1) != HOP_LIMIT(b->u1))) return -1;
	if ((mask & IB_PATH_RECORD_COMP_TCLASS) &&
		(a->TClass != b->TClass)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_REVERSIBLE) &&
		(a->Reversible != b->Reversible)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_PKEY) &&
		(a->P_Key != b->P_Key)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_SL) &&
		(SL(a->u2) != SL(b->u2))) return -1;
	if ((mask & IB_PATH_RECORD_COMP_MTUSELECTOR) &&
		(a->MtuSelector != b->MtuSelector)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_MTU) &&
		(a->Mtu != b->Mtu)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_RATESELECTOR) &&
		(a->RateSelector != b->RateSelector)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_RATE) &&
		(a->Rate != b->Rate)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_PKTLIFESELECTOR) &&
		(a->PktLifeTimeSelector != b->PktLifeTimeSelector)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_PKTLIFE) &&
		(a->PktLifeTime != b->PktLifeTime)) return -1;
	if ((mask & IB_PATH_RECORD_COMP_PREFERENCE) &&
		(a->Preference != b->Preference)) return -1;

	return 0;
}
#endif

/*
 * These functions check to see if the update count of the named table has been changed.
 *
 * Returns true if a re-open is needed.
 */
static inline int validate_port_table(op_ppath_reader_t *r) { return (r->old_port_update_count != r->shared_table->port_update_count); }
static inline int validate_path_table(op_ppath_reader_t *r) { return (r->old_path_update_count != r->shared_table->path_update_count); }
static inline int validate_vfab_table(op_ppath_reader_t *r) { return (r->old_vfab_update_count != r->shared_table->vfab_update_count); }
static inline int validate_subnet_table(op_ppath_reader_t *r) { return (r->old_subnet_update_count != r->shared_table->subnet_update_count); }

int op_ppath_find_path(op_ppath_reader_t *r, 
					   char *hfi_name,
					   uint16 port,
					   uint64 mask,
					   IB_PATH_RECORD_NO *query,
					   IB_PATH_RECORD_NO *result)
{
	int err=0;
	int retries;
	int gidquery = 0;
	int valid = 0;
	uint32 p;
	op_ppath_port_record_t *port_record;
	op_ppath_sid_record_t *sid_record;
	op_ppath_vfab_record_t *vfab;
	op_ppath_record_t *path_record = NULL;
	uint64 ho_sid;


	_DBG_FUNC_ENTRY;

	if (!query || !result) {
		_DBG_INFO("Null query or result.\n");
		err = EINVAL;
		goto error;
	}

	if (mask & (IB_PATH_RECORD_COMP_SGID | IB_PATH_RECORD_COMP_DGID)) {
		_DBG_DEBUG("GID based query.\n");
		gidquery = 1;
	} else if (mask & (IB_PATH_RECORD_COMP_SLID | IB_PATH_RECORD_COMP_DLID)) {
		_DBG_DEBUG("LID based query.\n");
		gidquery = 0;
	} else {
		_DBG_INFO("Must specify either the source & dest lids or the source & dest gids.\n");
		err = EINVAL;
		goto error;
	}


	if (mask & IB_PATH_RECORD_COMP_SERVICEID) {
		// We need host byte order for doing ranged compares.
		_DBG_DEBUG("SID query.\n");
		ho_sid = ntoh64(query->ServiceID);
	} else {
		// Use the pkey, or just use the default vfabric 
		_DBG_DEBUG("non-SID query.\n");
		ho_sid = 0;
	}

	memset(result,0,sizeof(IB_PATH_RECORD_NO));
	retries = 0;
	do {
		// Yield the processor so that, if an update is underway,
		// it will have a chance to finish before we try to read the tables.
		sched_yield(); 
	
		retries++;
		
		err=0;

		// Find the correct port. If the port table changed,
		// begin the query over again.
		if (validate_port_table(r)) {
			_DBG_DEBUG("Reloading the port table.\n");
			err = reopen_ppath_table(r, PORT_TABLE, O_RDONLY, 0, 0);
			if (err) goto error;
			continue; // NOTE THE CONTINUE!
		}

		if (mask & IB_PATH_RECORD_COMP_SGID) {
			_DBG_DEBUG("Identifying port by source gid.\n");
			for (p=1;p<=r->port_table->count;p++) {
				if (r->port_table->port[p].source_guid == query->SGID.Type.Global.InterfaceID)
					break;
			}
		} else if (hfi_name && hfi_name[0] !=0) {
			uint16_t lid = ntohs(query->SLID);
			_DBG_DEBUG("Identifying port by hfi and source lid.\n");
			for (p=1;p<=r->port_table->count;p++) {
				uint16_t base_lid = lid & lmc_mask[r->port_table->port[p].lmc];
				if (r->port_table->port[p].base_lid == base_lid && !strcmp(r->port_table->port[p].hfi_name,hfi_name))
					break;
			}
		} else {
			_DBG_INFO("Attempt to query a path without specifying HFI + Port or source gid.\n");
			err = EINVAL;
			goto error;
		}

		if (p>r->port_table->count) {
			_DBG_INFO("Could not find a matching port.\n");
			err = EINVAL;
			goto error;
		}

		port_record = &(r->port_table->port[p]);

		if (mask & IB_PATH_RECORD_COMP_PKEY) {
			// verify that the pkey is valid for this port.
			_DBG_DEBUG("Validating the pkey against the port.\n");
			for (p=0; p<PKEY_TABLE_LENGTH && port_record->pkey[p] != 0; p++) {
				if ((query->P_Key&0x0080) && 
					(port_record->pkey[p] == query->P_Key)) {
					/* Looking for a full-membership path. */
					break;
				} else if ((port_record->pkey[p]&0xff7f) == query->P_Key) { 
					/* Looking for a limited membership path. */
					break;
				}
			}
			if (p >= PKEY_TABLE_LENGTH || port_record->pkey[p] == 0) {
				_DBG_INFO("Port is not a member of the specified partition.\n");
				err = EINVAL;
				goto error;
			}
		} 

		// Validate the subnet table. If the table is invalid, re-open it, 
		// then retry the whole query.
		p=port_record->subnet_id;
		if (validate_subnet_table(r)) {
			_DBG_DEBUG("Reloading the subnet table.\n");
			err = reopen_ppath_table(r, SUBNET_TABLE, O_RDONLY, 0, 0);
			if (err) goto error;
			continue; // NOTE THE CONTINUE!
		}

		// Validate the vfabric. 
		if (validate_vfab_table(r)) {
			_DBG_DEBUG("Reloading the fabric table.\n");
			err = reopen_ppath_table(r, VFAB_TABLE, O_RDONLY, 0, 0);
			if (err) goto error;
			continue; // NOTE THE CONTINUE!
		}

		if (mask & IB_PATH_RECORD_COMP_PKEY) {
			// Match the pkey to a virtual fabric.
			_DBG_DEBUG("Using the pkey to identify the virtual fabric.\n");
			for (p=1; p<=r->vfab_table->count; p++) {
				// The VFab may or may not have the membership bit set.
				// This is not a bug.
				if ((r->vfab_table->vfab[p].source_prefix == 
						port_record->source_prefix) &&
					(r->vfab_table->vfab[p].pkey & 0xff7f) ==
						(query->P_Key & 0xff7f) )
					break;
			}
			if (p > r->vfab_table->count) {
				_DBG_INFO("Could not find a matching pkey.\n");
				err = EINVAL;
				goto error;
			}
		} else {
			uint32 def_sid = 0;
			// Find the sid. As before, if we can't find it
			// re-try the whole query.
			p = r->subnet_table->subnet[p].first_sid;
			while (p) {
				uint64 ho_lower_sid;
				uint64 ho_upper_sid;
				sid_record = &(r->sid_table[p]);
				ho_lower_sid = ntoh64(sid_record->lower_sid);
				ho_upper_sid = ntoh64(sid_record->upper_sid);

				// Flag the default vfab when we find it.
				if (ho_lower_sid == 0) def_sid = p;

				if (ho_lower_sid == 0 && ho_sid == 0) {
					// Queried with a blank sid. Return the default.
					break;
				} else if (ho_upper_sid == 0 && ho_sid == ho_lower_sid) {
					// Exact match.
					break;
				} else if (ho_lower_sid != 0 && 
						   ho_sid >= ho_lower_sid && 
						   ho_sid <= ho_upper_sid) {
					break;
				}
				p = sid_record->next;
			}
			if (!p) {
				// Didn't find a good match, use the default.
				p = def_sid;
				sid_record = &(r->sid_table[p]);
			}
			if (!p) {
				_DBG_INFO("Failed to match query to any sid.\n");
				err = EINVAL;
				goto error;
			}

			p=sid_record->vfab_id;
		}

		vfab = &(r->vfab_table->vfab[p]);

		if (validate_path_table(r)) {
			_DBG_DEBUG("Reloading the path table.\n");
			err = reopen_ppath_table(r, PATH_TABLE, O_RDONLY, 0, 0);
			continue; // NOTE THE CONTINUE!
		}

		// what's the first path to look at?
		if (gidquery) {
			p = vfab->first_dguid[GID_HASH(&(query->DGID))];
		} else {
			p = vfab->first_dlid[LID_HASH(query->DLID)];
		}

		// At this point, we should be looking at a short list of paths.
		// These paths will be on the correct subnet and vfabric,
		// so we only check the lids. If we get out of this while loop
		// with a hit, break out of the outer while loop, too.
		while (p) {
			path_record = &(r->path_table->table[p]);

			/*
			 * These if statements are hairy, so here's the logic:
			 *
			 * If we are doing a gidquery and the gids match and 
			 * EITHER the pkey matches OR we are not doing pkey queries
			 * then we have a match.
			 * 
			 * else, if we are NOT doing a gidquery and the LIDs match and
			 * EITHER the pkey matches OR we are not doing pkey queries
			 * then we have a match.
			 */
			if (gidquery && 
				!memcmp((void*)&(query->SGID), 
					   (void*)&(path_record->path.SGID), 
					   sizeof(IB_GID_NO)) &&
				!memcmp((void*)&(query->DGID), 
					   (void*)&(path_record->path.DGID), 
					   sizeof(IB_GID_NO)) &&
        		((!(mask & IB_PATH_RECORD_COMP_PKEY)) || 
					((path_record->path.P_Key&0xff7f) == (query->P_Key&0xff7f)))) {
				_DBG_DEBUG("gid match found.\n");
				valid = 1;
				break;
			} else if (!gidquery && 
				query->SLID == path_record->path.SLID && 
				query->DLID == path_record->path.DLID &&
        		((!(mask & IB_PATH_RECORD_COMP_PKEY)) || 
					((path_record->path.P_Key&0xff7f) == (query->P_Key&0xff7f)))) {
				_DBG_DEBUG("lid match found.\n");
				valid = 1;
				break;
			}
			if (gidquery) {
				p = path_record->next_guid;
			} else {
				p = path_record->next_lid;
			}
		}
		
		// One last sanity check.
		if (valid && path_record->flags ==0) valid = 0;

	} while (!valid && (retries <= MAX_RETRIES));

	if (valid) {
		*result = path_record->path;
		result->ServiceID = query->ServiceID;
		err = 0;
	} else if (err == 0) {
		// If we got here, we never had an error, but we didn't find 
		// the path, either.
		err = ENOENT;
	}

error:
	_DBG_FUNC_EXIT;
	return err;
}


unsigned op_ppath_version(void)
{
        return OPA_SA_DB_PATH_TABLE_VERSION;
}
