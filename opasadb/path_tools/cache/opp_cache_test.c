/* BEGIN_ICS_COPYRIGHT7 ****************************************

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

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <linux/types.h>
#include <endian.h>
#include <getopt.h>
#include <opasadb_path_private.h>
#include <opasadb_debug.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include "dumppath.h"

#define SLEEPTIME 1
#define QUERIESPERPASS 100000
#define DEFAULTPASSES 10

#define MAXPATHS 32768
#define MAXPKEYS 32

#define HFINAME "qib0"
#define PORTNO  1

static IB_PATH_RECORD_NO record[MAXPATHS];
static uint16 pkey[MAXPKEYS];
static int numrecs, numkeys;

static char hfi_name[64] = HFINAME;
static uint16 port_no = PORTNO;

static int done=0;

static uint64_t build_comp_mask(IB_PATH_RECORD_NO path)
{
    uint64_t mask = 0;

    if (path.ServiceID)
        mask |= IB_PATH_RECORD_COMP_SERVICEID;
    if (path.DGID.Type.Global.InterfaceID | path.DGID.Type.Global.SubnetPrefix)
        mask |= IB_PATH_RECORD_COMP_DGID;
    if (path.SGID.Type.Global.InterfaceID | path.SGID.Type.Global.SubnetPrefix)
        mask |= IB_PATH_RECORD_COMP_SGID;
    if (path.DLID)
        mask |= IB_PATH_RECORD_COMP_DLID;
    if (path.SLID)
        mask |= IB_PATH_RECORD_COMP_SLID;
    if (path.P_Key) mask |= IB_PATH_RECORD_COMP_PKEY;

    return mask;
}

static void my_sig(int signo)
{
	done=1;
	return;
}

static void sigalarm(int signo)
{
	return;
}

static int my_sleep(unsigned int secs)
{
	if (signal(SIGALRM, sigalarm) == SIG_ERR) return -1;

	alarm(secs);
	pause();
	return alarm(0);
}

static int readline(FILE *f, char *s, int max)
{
	int i, c;
	memset(s,0,max);

	i=0;
	c=fgetc(f);

	while (c !='\n' && !feof(f) && i < max) {
		s[i++]=c;
		c=fgetc(f);
	}
	return i;
}

#define asciitohex(c) ((((c)>='0') & ((c)<='9'))?((c)-'0'):((c)-'a'+10))

static void getraw(IB_GID_NO *gid, char *p)
{
	char *p2;

	gid->Type.Global.SubnetPrefix = hton64(strtoul(p,&p2,16));
	gid->Type.Global.InterfaceID = hton64(strtoul(p2,NULL,16));
}
		
int readrecord(FILE *f, IB_PATH_RECORD_NO *record)
{
	char *p, *l;

	char buffer[1024];

	memset(record, 0, sizeof(IB_PATH_RECORD_NO));

	if (readline(f,buffer,1024)<=0) {
		return -1;
	}
	p=strtok_r(buffer,":", &l);
	if (!p) 
		return -1;
	getraw(&record->SGID,p);
	p=strtok_r(NULL,":", &l);
	if (!p) 
		return -1;
	getraw(&record->DGID,p);
	p=strtok_r(NULL,":", &l);
	if (!p) 
		return -1;
	record->SLID = htons(strtol(p,NULL,0));
	p=strtok_r(NULL,":", &l);
	if (!p) 
		return -1;
	record->DLID = htons(strtol(p,NULL,0));
	p=strtok_r(NULL,":", &l);
	if (!p) 
		return -1;
	record->P_Key = htons(strtol(p,NULL,0));

	// We dummy up a SID that matches the pkey.
	if (record->P_Key == 0xffff) {
		record->ServiceID=0;
	} else {
		record->ServiceID=record->P_Key;
	}
	return 0;
}

static int load_records(char *fname)
{
	FILE *f;

	f=fopen(fname,"r");
	if (f == NULL) {
		_DBG_ERROR("Failed to open %s\n",fname);
		return -1;
	}

	printf("Loading %s.\n", fname);
	numrecs = 0;

	do {
		if (readrecord(f,&record[numrecs])) break;
		numrecs++;
	} while (numrecs < MAXPATHS);
	fclose(f);

	return numrecs;
}

static int find_pkeys(void)
{
	int i;

	for (numkeys=0, i=0; i<numrecs && numkeys<MAXPKEYS; i++) {
		int k;
		for (k=0; k<numkeys; k++) {
			if (pkey[k] == record[i].P_Key) break;
		}
		if (k>=numkeys) {
			pkey[numkeys]=record[i].P_Key;
			numkeys++;
		}
	}

	return numkeys;
}

/*
 * Creates the shared memory tables. Uses the info from the supplied file,
 * to create one or more virtual fabrics. NOTE: Only supports a single
 * active port.
 */
int server(char *fname, int n)
{
	int i, err, pass;
	op_ppath_writer_t w;
	op_ppath_port_record_t port;

	_DBG_FUNC_ENTRY;

	if (load_records(fname) <= 0) {
		return -1;
	}
	
	if (find_pkeys() <= 0) {
		_DBG_ERROR("No pkeys?!?\n");
		return -1;
	}

	_DBG_NOTICE("Creating the shared memory table.\n");
	err = op_ppath_create_writer(&w);
	if (err) {
		_DBG_ERROR("Failed to create shared memory table: %s\n",
				strerror(err));
		return err;
	}

	_DBG_NOTICE("Creating the shared port table.\n");
	err = op_ppath_initialize_ports(&w, 1);
	if (err) {
		_DBG_ERROR( "Failed to create port table: %s\n",
				strerror(err));
		goto error;
	}

	_DBG_NOTICE("Creating the subnet table.\n");
	err = op_ppath_initialize_subnets(&w, 1, numkeys);
	if (err) {
		_DBG_ERROR( "Failed to create subnet table: %s\n",
				strerror(err));
		goto error;
	}

	_DBG_NOTICE("Creating the vfabric table.\n");
	err = op_ppath_initialize_vfabrics(&w, numkeys);
	if (err) {
		_DBG_ERROR( "Failed to create vfab table: %s\n",
				strerror(err));
		goto error;
	}

	_DBG_NOTICE("Adding the subnet.\n");
	err = op_ppath_add_subnet(&w, record[0].SGID.Type.Global.SubnetPrefix);
	if (err) {
		_DBG_ERROR( "Failed to add subnet: %s\n",
				strerror(err));
		goto error;
	}

	_DBG_NOTICE("Adding the vfabrics.\n");
	for (i=0; i<numkeys; i++) {
		uint64 sid;
		char buffer[VFAB_NAME_LENGTH];
		sprintf(buffer,"VF%04x",(unsigned int)pkey[i]);
		err = op_ppath_add_vfab(&w,
								buffer,
								record[0].SGID.Type.Global.SubnetPrefix,
								pkey[i],
								0);
		if (err) {
			_DBG_ERROR( "Failed to add vfab: %s\n",
					strerror(err));
			goto error;
		}

		sid=(pkey[i] == 0xffff)?0:pkey[i];
			
		// NOTE THAT WE ARE SETTING THE SID TO MATCH THE PKEY!
		err = op_ppath_add_sid(&w,
							   record[0].SGID.Type.Global.SubnetPrefix,
							   sid,0,
							   buffer);
		if (err) {
			_DBG_ERROR( "Failed to add sid: %s\n",
					strerror(err));
			goto error;
		}
	}
	
	memset(&port,0, sizeof(op_ppath_port_record_t));

	port.source_prefix =  record[0].SGID.Type.Global.SubnetPrefix;
	port.source_guid =  record[0].SGID.Type.Global.InterfaceID;
	strcpy(port.hfi_name,hfi_name);

	port.port = port_no;

	_DBG_NOTICE("Adding the port.\n");
	err = op_ppath_add_port(&w,port);
	if (err) {
		_DBG_ERROR( "Failed to add port: %s\n",
				strerror(err));
		goto error;
	}
	
	for(pass = 0; pass < n && !done; pass++) {

		if (pass>0) my_sleep(SLEEPTIME);

		_DBG_NOTICE("Pass %d: Inserting records.\n", pass);

		_DBG_NOTICE("Creating the path table.\n");
		err = op_ppath_initialize_paths(&w, MAXPATHS);
		if (err) {
			_DBG_ERROR( "Failed to create path table: %s\n",
					strerror(err));
			goto error;
		}

		for (i=0;i<numrecs;i++) {

			if (op_ppath_add_path(&w, &record[i])) {
				_DBG_ERROR( "Failed to add records.\n");
				err = -1;
				goto error;
			}
		}
		_DBG_NOTICE("Done inserting records.\n");
		op_ppath_publish(&w);

	} 

	printf("Press 'q' to close and delete the shared tables.\n");
	fflush(stdin);
	do {
		err=getchar();
	} while(err != 'q');

error:

	op_ppath_close_writer(&w);

	_DBG_FUNC_EXIT;
	return 0;
}

int client(char *fname, int n)
{
	op_ppath_reader_t r;
	int pass, count, err;
	int ret = 0;

	srand((unsigned)time(NULL));

	if (load_records(fname) <= 0) {
		return -1;
	}

	err = op_ppath_create_reader(&r);
	if (err) {
		_DBG_ERROR( "Failed to access shared memory tables: %s\n",
				strerror(err));
		return err;
	}

	_DBG_INFO("Tables are open.\n");

	count = 0;
	pass = 0;

	do {
		IB_PATH_RECORD_NO query;
		int i = rand() % numrecs;
		int j = rand() % 6;

		IB_PATH_RECORD_NO result;

		query = record[i];

		switch (j) {
			case 0:
				/* Query by SID and LIDs. */
				query.P_Key = 0;
				memset(&query.SGID,0,sizeof(IB_GID_NO));
				memset(&query.DGID,0,sizeof(IB_GID_NO));
				break;
			case 1:
				/* Query by PKEY and LIDs. */
				query.ServiceID = 0;
				memset(&query.SGID,0,sizeof(IB_GID_NO));
				memset(&query.DGID,0,sizeof(IB_GID_NO));
				break;
			case 2:
				/* No SID, no pkey, just LIDs. */
				query.ServiceID = 0;
				query.P_Key = 0;
				memset(&query.SGID,0,sizeof(IB_GID_NO));
				memset(&query.DGID,0,sizeof(IB_GID_NO));
				break;
			case 3:
				/* Query by PKEY and GIDs. */
				query.ServiceID = 0;
				query.DLID = 0;
				query.SLID = 0;
				break;
			case 4:
				/* Query by SID and GIDs. */
				query.P_Key = 0;
				query.DLID = 0;
				query.SLID = 0;
				break;
			case 5:
				/* Query by just GIDs. */
				query.P_Key = 0;
				query.ServiceID = 0;
				query.DLID = 0;
				query.SLID = 0;
				break;
		}
		
		err = op_ppath_find_path(&r,
							    hfi_name,
								port_no,
								build_comp_mask(query),
								&query,
								&result);
		if (err) {
			_DBG_ERROR("op_ppath_find_path() failed: %s\n",
						strerror(err));
			fprint_path_record(stderr, "Looking for",
							(op_path_rec_t*)&query);
			ret = -1;
			goto client_exit;
		}

		if (j==0 && ((result.DLID != record[i].DLID) ||
				(result.SLID != record[i].SLID))) {
			_DBG_ERROR( "Returned the wrong record.\n");
			fprint_path_record(stderr,"Looking for",
							  (op_path_rec_t *) & record[i]);
			fprint_path_record(stderr,"Found", (op_path_rec_t *) & result);
			ret = -1;
			goto client_exit;
		} else if (j != 0 && (memcmp((void*)&(result.DGID), (void*)&(record[i].DGID),
            	sizeof(result.DGID)) || 
			memcmp((void*)&(result.SGID), (void*)&(record[i].SGID), 
				sizeof(result.SGID)))) {
			_DBG_ERROR( "Returned the wrong record.\n");
			fprint_path_record(stderr,"Looking for",
							  (op_path_rec_t *) & record[i]);
			fprint_path_record(stderr,"Found", (op_path_rec_t *) & result);
			ret = -1;
			goto client_exit;
		}

		count++;
		if (count >= QUERIESPERPASS) {
			pass++;
			count=0;
			printf("Pass %d complete.\n", pass);
		}
	} while (!done && pass < n);

client_exit:
	op_ppath_close_reader(&r);
	return ret;
}

int dump(char *fname)
{
	op_ppath_reader_t r;
	int err;
	unsigned i;
	FILE *f = fopen(fname,"w");

	if (!f) {
		_DBG_ERROR("Can't open %s for writing.\n",fname);
		return -1;
	}

	err = op_ppath_create_reader(&r);
	if (err) {
		_DBG_ERROR( "Failed to access shared memory tables: %s\n",
				strerror(err));
		fclose(f);
		return err;
	}

	fprintf(f,"Shared Table:\n");
	fprintf(f,"\tABI Version: %u\n", r.shared_table->abi_version);
	fprintf(f,"\tSubnet Name: %s\n", r.shared_table->subnet_table_name);
	fprintf(f,"\tSubnet Update Count: %u\n", 
			r.shared_table->subnet_update_count);
	fprintf(f,"\tPort Name: %s\n", r.shared_table->port_table_name);
	fprintf(f,"\tPort Update Count: %u\n", r.shared_table->port_update_count);
	fprintf(f,"\tVFab Name: %s\n", r.shared_table->vfab_table_name);
	fprintf(f,"\tVFab Update Count: %u\n", r.shared_table->vfab_update_count);
	fprintf(f,"\tPath Name: %s\n", r.shared_table->path_table_name);
	fprintf(f,"Path Update Count: %u\n", r.shared_table->path_update_count);

	fprintf(f,"\n\nSubnet Table:\n");
	fprintf(f,"\tSubnet Size: %u\n", r.subnet_table->subnet_size);
	fprintf(f,"\tSubnet Count: %u\n", r.subnet_table->subnet_count);
	fprintf(f,"\tSID Size: %u\n", r.subnet_table->sid_size);
	fprintf(f,"\tSID Count: %u\n", r.subnet_table->sid_count);
	for (i = 1; i <= r.subnet_table->subnet_count ; i++) {
		fprintf(f,"\tSubnet[%u]:\n",i);
		fprintf(f,"\t\tPrefix: 0x%016lx\n",
				hton64(r.subnet_table->subnet[i].source_prefix));
		fprintf(f,"\t\tFirst SID: %u\n",
				r.subnet_table->subnet[i].first_sid);
	}
	for (i = 1; i <= r.subnet_table->sid_count ; i++) {
		fprintf(f,"\tSID[%u]:\n",i);
		fprintf(f,"\t\tVFab ID: %u\n",r.sid_table[i].vfab_id);
		fprintf(f,"\t\tLower: 0x%016lx\n",
				hton64(r.sid_table[i].lower_sid));
		fprintf(f,"\t\tUpper: 0x%016lx\n",
				hton64(r.sid_table[i].upper_sid));
		fprintf(f,"\t\tNext: %u\n",
				r.sid_table[i].next);
	}

	fprintf(f,"\n\nPort Table:\n");
	fprintf(f,"\tSize: %u\n", r.port_table->size);
	fprintf(f,"\tCount: %u\n", r.port_table->count);
	for (i = 1; i <= r.port_table->count ; i++) {
		int j;
		fprintf(f,"\tPort[%u]: %s/%u\n", i, 
				r.port_table->port[i].hfi_name,
				r.port_table->port[i].port);
		fprintf(f,"\t\tSubnet ID: %u\n",
				r.port_table->port[i].subnet_id);
		fprintf(f,"\t\tPrefix: 0x%016lx\n",
				hton64(r.port_table->port[i].source_prefix));
		fprintf(f,"\t\tGuid: 0x%016lx\n",
				hton64(r.port_table->port[i].source_guid));
		fprintf(f,"\t\tBase Lid: 0x%04x\n",
				r.port_table->port[i].base_lid);
		fprintf(f,"\t\tLMC: 0x%04x\n",
				r.port_table->port[i].lmc);
		fprintf(f,"\t\tPkeys:\n");
		for (j=0; j<PKEY_TABLE_LENGTH && r.port_table->port[i].pkey[j]!=0; j++) 
			fprintf(f,"\t\t\t0x%04x\n", ntohs(r.port_table->port[i].pkey[j]));
	}

	fprintf(f,"\n\nVFab Table:\n");
	fprintf(f,"\tSize: %u\n", r.vfab_table->size);
	fprintf(f,"\tCount: %u\n", r.vfab_table->count);
	for (i = 1; i <= r.vfab_table->count ; i++) {
		int j;
		fprintf(f,"\tVFab[%u]: %s\n", i, 
				r.vfab_table->vfab[i].vfab_name);
		fprintf(f,"\t\tPrefix: 0x%16lx\n",
				hton64(r.vfab_table->vfab[i].source_prefix));
		fprintf(f,"\t\tPKey: 0x%04x\n",
				hton16(r.vfab_table->vfab[i].pkey));
		fprintf(f,"\t\tService Level: 0x%04x\n",
				hton16(r.vfab_table->vfab[i].sl));

		fprintf(f,"\n\t\tDLID Hash Table:\n");
		for (j=0;j<HASH_TABLE_SIZE; j++) {
			fprintf(f,"\t\t\tRecord[%u] = %u\n",
					j,r.vfab_table->vfab[i].first_dlid[j]);
		}

		fprintf(f,"\n\t\tDGUID Hash Table:\n");
		for (j=0;j<HASH_TABLE_SIZE; j++) {
			fprintf(f,"\t\t\tRecord[%u] = %u\n",
					j,r.vfab_table->vfab[i].first_dguid[j]);
		}
		
	}

	fprintf(f,"\n\nPath Table\n");
	fprintf(f,"\tSize: %u\n", r.path_table->size);
	fprintf(f,"\tCount: %u\n", r.path_table->count);
	for (i=1; i<= r.path_table->count; i++) {
		char s[128];
		sprintf(s,"Record[%u] (%s) Lid->%u, Guid->%u",i, 
				r.path_table->table[i].flags?"Used":"Unused",
				r.path_table->table[i].next_lid,  
				r.path_table->table[i].next_guid);
		fprint_path_record(f,s,(op_path_rec_t*)&(r.path_table->table[i].path));
	}

	op_ppath_close_reader(&r);
	fclose(f);
	return 0;
}

int main(int argc, char *argv[])
{
	int mode = 's';
	int i;
	int n=DEFAULTPASSES;
	char filename[512];
	int debug = _DBG_LVL_NOTICE;

	memset(filename, 0, 512);

	do {
		int c;

		static char *short_options = "sdcf:n:v:";
		static struct option long_options[] = {
			{.name = "server",.has_arg = 0,.val = 's'},
			{.name = "client",.has_arg = 0,.val = 'c'},
			{.name = "dump",.has_arg = 0,.val = 'd'},
			{.name = "file",.has_arg = 1,.val = 'f'},
			{.name = "num",.has_arg = 1,.val = 'n'},
			{.name = "hfi",.has_arg = 1,.val = 'H'},
			{.name = "port",.has_arg = 1,.val = 'p'},
			{.name = "verbose",.has_arg = 1,.val = 'v'},
			{0}
		};

		static char *usage[] = {
			"Server mode: loads the specified file and emulates dsap",
			"Client mode: loads the specified file and emulates a client",
			"Dumps the current contents of the discovery tables to the file.",
			"the file containing the node table (required)",
			"the number of passes to make",
			"the default hfi. (Defaults to "HFINAME".)",
			"the default port. (Defaults to 1.)",
			"the level of logging to do.",
			NULL
		};

		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'H':
			strcpy(hfi_name, optarg);
			break;
		case 'p':
			port_no = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			n = strtoul(optarg,NULL,0);
			break;
		case 'c':
		case 'd':
		case 's':
			mode = c;
			break;
		case 'v':
			debug = strtol(optarg,NULL,0);
			break;
		case 'f':
			strncpy(filename, optarg, 511);
			break;
		default: 
			i=0;
			printf( "Usage: %s (options)\n", argv[0]);
			printf( "Options are:\n");
			while (long_options[i].name != NULL) {
				printf( "  [--%-8s %s | -%c %s]    %s\n",
						long_options[i].name,
						(long_options[i].has_arg) ? "<arg>" : "",
						long_options[i].val,
						(long_options[i].has_arg) ? "<arg>" : "",
						usage[i]);
				i++;
			}
			return -1;
		}
	} while(1);

	op_log_set_level(debug);

	if (filename[0]==0) {
		_DBG_ERROR("You must provide a filename.\n");
		return -1;
	}

	signal(SIGINT, my_sig);

	switch (mode) {
		case 'c':	
			client(filename,n);
			break;
		case 's':	
			server(filename,n);
			break;
		case 'd':	
			dump(filename);
			break;
	}
	return 0;
}
