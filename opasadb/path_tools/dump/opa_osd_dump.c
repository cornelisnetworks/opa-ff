/* BEGIN_ICS_COPYRIGHT7 ****************************************

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

#define HFINAME "qib0"
#define PORTNO  1

int 
dump(FILE *f)
{
	op_ppath_reader_t r;
	int err;
	unsigned i;

	err = op_ppath_create_reader(&r);
	if (err) {
		_DBG_ERROR( "Failed to access shared memory tables: %s\n",
				strerror(err));
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
	fprintf(f,"\tPath Update Count: %u\n", r.shared_table->path_update_count);

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
		op_ppath_vfab_record_t *vf = &r.vfab_table->vfab[i];
		fprintf(f,"\tVFab[%u]: %s\n", i, 
				vf->vfab_name);
		fprintf(f,"\t\tPrefix: 0x%16lx\n",
				hton64(vf->source_prefix));
		fprintf(f,"\t\tPKey: 0x%04x\n",
				hton16(vf->pkey));
		fprintf(f,"\t\tService Level: 0x%04x\n",
				hton16(vf->sl));

		fprintf(f,"\n\t\tDLID Hash Table:\n");
		for (j=0;j<HASH_TABLE_SIZE; j++) {
			fprintf(f,"\t\t\tRecord[%u] = %u\n",
					j,vf->first_dlid[j]);
		}
		fprintf(f,"\n\t\tDGUID Hash Table:\n");
		for (j=0;j<HASH_TABLE_SIZE; j++) {
			fprintf(f,"\t\t\tRecord[%u] = %u\n",
					j,vf->first_dguid[j]);
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
	return 0;
}

void Usage(int status)
{
	fprintf(stderr,  "Usage: opa_osd_dump (options)\n");
	fprintf(stderr,  "Print the current contents of the distributed SA shared memory database.\n");
	fprintf(stderr,  "Options are:\n");
	fprintf(stderr, "\t-v/--verbose\t<arg>\tLog Level. Corresponds to a Kernel log level,\n\t\t\t\ta number from 1 to 7\n");
	fprintf(stderr, "\t--help\t\t\tPrint this help text\n");
	fprintf(stderr, "\nExample:\topa_osd_dump > opasadb_contents\n");

	exit(status);
}


int main(int argc, char *argv[])
{
	int debug = _DBG_LVL_NOTICE;

	do {
		int c;

		static char *short_options = "v:";
		static struct option long_options[] = {
			{.name = "verbose",.has_arg = 1,.val = 'v'},
			{.name = "help", .has_arg = 0, .val = '$'},
			{0}
		};

		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'v':
			debug = strtol(optarg,NULL,0);
			break;
		case '$':
			Usage(0);//exits
			break;
		default: 
			Usage(2);//exits
			break;
		}
	} while(1);

	op_log_set_level(debug);

	dump(stdout);
	return 0;
}
