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

/*
 * This program is meant to stress the path query system.
 */
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <infiniband/umad.h>
#include "opasadb_path.h"
#include "opasadb_debug.h"
#include "dumppath.h"

#define MAX_SOURCE_PORTS (UMAD_CA_MAX_PORTS * UMAD_MAX_DEVICES)
#define MAX_SIDS 8
#define MAX_PKEYS 8
#define MAX_SECONDS (60 * 60 * 24)
#define MAX_TOGGLES 100
#define SRCTOGGLERATE 0
#define DELAY 10

#define DELIMITER ";"

static 	char remote[512];

//Previously, this was used to stress the API, but since we're now using this 
//to test the SM and the Distributed SA, that isn't the best use of our time.
//#define QUERIESPERPASS 100000

typedef struct _src_record {
	int disable;
	uint64_t prefix;
	uint64_t guid;
	uint16_t base_lid;
	uint16_t num_lids;
	uint16_t port_num;
	char hfi_num;
} src_record;

typedef struct _record {
	int disable;
	int simulated;
	uint64_t guid;
	uint16_t lid;
	char name[64];
	int guid_fail_count;
	int lid_fail_count;
} record;

static void sig_alrm(int signo) { return; }

static int 
my_sleep(unsigned int secs)
{
	if (signal(SIGALRM, sig_alrm) == SIG_ERR) return -1;

	alarm(secs);
	pause();
	return alarm(0);
}

static int 
readline(FILE *f, char *s, int max)
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

static void
portenable(record *r)
{
	char buffer[1024];
	
	if (r->simulated) {
		sprintf(buffer,"ssh root@%s simctl PortUp portguid:0x%016"PRIx64"\n",
		remote, ntoh64(r->guid));
	} else {
		sprintf(buffer,"ssh root@%s /sbin/opaportconfig -l %u enable\n",
				r->name, htons(r->lid));	
	}	
		
	_DBG_DEBUG("%s",buffer);
	system(buffer);
}

static void
portdisable(record *r)
{
	char buffer[1024];
	
	if (r->simulated) {
		sprintf(buffer,"ssh root@%s simctl PortDown portguid:0x%016"PRIx64"\n",
		remote, ntoh64(r->guid));
	} else {
		sprintf(buffer,"ssh root@%s /sbin/opaportconfig -l %u disable\n",
				r->name, htons(r->lid));	
	}	
		
	_DBG_DEBUG("%s",buffer);
	system(buffer);
}

static void
src_portenable(src_record *r)
{
	char buffer[1024];
	
	sprintf(buffer,"/sbin/opaportconfig -l %u enable\n",
			htons(r->base_lid));	
		
	_DBG_DEBUG("%s",buffer);
	system(buffer);
}

static void
src_portdisable(src_record *r)
{
	char buffer[1024];
	
	sprintf(buffer,"/sbin/opaportconfig -l %u disable\n",
			htons(r->base_lid));	
		
	_DBG_DEBUG("%s",buffer);
	system(buffer);
}

static int 
readrecord(FILE *f, record *r)
{
	char *p, *l;
	char buffer[1024];
	memset(r, 0, sizeof(record));

	if (readline(f,buffer,1024)<=0) {
		return -1;
	}
			
	r->disable = 0;

	p=strtok_r(buffer,DELIMITER, &l); if (!p) return -1;
	r->lid = htons(strtol(p,NULL,0));
	p=strtok_r(NULL,DELIMITER, &l); if (!p) return -1;
	r->guid = hton64(strtoull(p,NULL,0));
	
	p=strtok_r(NULL,DELIMITER, &l); if (!p) return -1;
	strcpy(r->name, p);
	
	r->simulated = strstr(p,"Sim") != NULL;
	if (!r->simulated) {
		/* parse name */
		p = strstr(r->name," ");
		if (p) *p=0;
	}
	
	return 0;
}

static void 
usage(char **argv)
{
	fprintf(stderr, "Usage: %s [opts] guidlist\n", argv[0]);
	fprintf(stderr, "Stress test SM and Distributed SA query system\n");
	fprintf(stderr, "\toptions include:\n");
	fprintf(stderr, "\t--help\n");
	fprintf(stderr, "\t\tProvide this help text.\n");
	fprintf(stderr, "\t-d <debug level>\n");
	fprintf(stderr, "\t\t set debugging level.\n");
	fprintf(stderr, "\t-s <seconds>\n");
	fprintf(stderr, "\t\tRun for at least <seconds> seconds.\n");
	fprintf(stderr, "\t-r <remote>\n");
	fprintf(stderr, "\t\tThe host running the fabric simulator.\n");
	fprintf(stderr, "\t-x <count>\n");
	fprintf(stderr, "\t\tHow many destinations to toggle up or down after each pass.\n");
	fprintf(stderr, "\t\t(up to " stringize(MAX_TOGGLES)".)\n");
	fprintf(stderr, "\t-X <count>\n");
	fprintf(stderr, "\t\tHow often to toggle a source port up or down (in seconds).\n");
	fprintf(stderr, "\t-D <seconds>\n");
	fprintf(stderr, "\t\tHow long to sleep after each pass.\n");
	fprintf(stderr, "\t\t(To give the SM time to process port events)\n");
	fprintf(stderr, "\t-p <pkey>\n");
	fprintf(stderr, "\t\tInclude <pkey> in the searches.\n");
	fprintf(stderr,"\t\t(Can be specified up to " add_quotes(MAX_PKEYS) " times.)\n");
	fprintf(stderr, "\t-S <sid>\n");
	fprintf(stderr, "\t\tInclude <sid> in the searches.\n");
	fprintf(stderr, "\t-t <error threshold>\n");
	fprintf(stderr, "\t\tAbort the test if the number of path errors to a single destination exceeds threshold.\n");
	fprintf(stderr, "\t\t(the count is reset to zero when a correct result is retrieved.)\n");
	fprintf(stderr,"\t\t(Can be specified up to " add_quotes(MAX_SIDS) " times. Note that\n");
	fprintf(stderr,"\t\tproviding both SIDs and pkeys may cause problems.)\n");
	fprintf(stderr, "\t-e\n");
	fprintf(stderr, "\t\tInstruct simulator to enable all ports before starting.\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
			"'guidlist' is a text file that lists the source and destination\n");
	fprintf(stderr,
			"guids and lids \n");
	fprintf(stderr,
                        "guidlist format is\n");
	fprintf(stderr,
                        "lid_0;guid_0;node_desc_0\n");
	fprintf(stderr,
                        "lid_1;guid_1;node_desc_1\n");
	fprintf(stderr,
                        ".\n");
	fprintf(stderr,
                        ".\n");
	fprintf(stderr,
			"\nExample:\t%s -p 0x9001  guidtable\n", argv[0]);
}

static record *dest_ports;
static src_record src_ports[MAX_SOURCE_PORTS];
int numsources = 0;

int
get_sources(void)
{
	umad_port_t port;
	char ca_names[UMAD_MAX_DEVICES][UMAD_CA_NAME_LEN];
	int ca_count;
	int i, err;

	ca_count = umad_get_cas_names(ca_names, UMAD_MAX_DEVICES);

	for (i = 0; i < ca_count; i++) {
		umad_ca_t ca;
		int j;
		
		err = umad_get_ca(ca_names[i], &ca);
		if (err) {
			_DBG_ERROR("Failed to open %s\n", ca_names[i]);
			return -1;
		}

		for (j = 1; j <= ca.numports; j++) {
			err = umad_get_port(ca_names[i], j, &port);
			if (err) {
				_DBG_ERROR("Failed to get info on port %d of %s\n",
							j, ca_names[i]);
				umad_release_ca(&ca);
				return -1;
			}

			if (port.state == IBV_PORT_ACTIVE) {
				src_ports[numsources].disable = port.state != IBV_PORT_ACTIVE;
				src_ports[numsources].prefix = port.gid_prefix;
				src_ports[numsources].guid = port.port_guid;
				src_ports[numsources].base_lid = htons(port.base_lid);
				src_ports[numsources].num_lids = 1<<(port.lmc);
				src_ports[numsources].port_num = j;
				src_ports[numsources].hfi_num = i + 1;
				numsources++;
			}
		}
	}
	return numsources;
}

int 
main(int argc, char **argv)
{
	FILE *f = NULL;
	int c, err, pf;
	int debug = _DBG_LVL_ERROR;
	uint16_t port;
	unsigned i, numdests, numtoggle, srctogglerate;
	uint64_t sid[MAX_SIDS];
	unsigned pkey[MAX_PKEYS];
	unsigned numpkeys, numsids, enable_ports;
	unsigned falsepos, falseneg, passes, seconds, queries, delay;
	struct timeval start_time, end_time, elapsed_time, next_src_toggle, now_time;
	void *context;
	struct ibv_context *hfi;
	struct ibv_device *device;
	int error_threshold = 65536; // Just because...
	int ret = 0;

	setlocale(LC_ALL, "");

	/* Default values. */
	strcpy(remote,"strife");
	port = 1;
	seconds = 0;
	numdests = 1024;
	numsources = 0;
	numtoggle = 10;
	numpkeys = 0;
	numsids = 0;
	enable_ports = 0;
	srctogglerate = SRCTOGGLERATE;
	delay = DELAY;

	if (argc > 1){
		if (!strcmp(argv[1], "--help")){
			usage(argv);
			exit(0);
		}
	}

	while ((c = getopt(argc, argv, "ed:s:r:p:S:x:X:t:D:")) != EOF) {
		switch (c) {
		case 'e':
			enable_ports = 1;
			break;
		case 'p':
			if (numpkeys < MAX_PKEYS) {
				pkey[numpkeys++] = htons(strtoul(optarg,NULL,0));
			} else {
				_DBG_ERROR("Too many pkeys.\n");
				return -1;
			}
			break;
		case 'S':
			if (numsids < MAX_SIDS) {
				sid[numsids++] = hton64(strtoull(optarg,NULL,0));
			} else {
				_DBG_ERROR("Too many sids.\n");
				return -1;
			}
			break;
		case 'd':
			debug = strtoul(optarg,NULL,0);
			op_log_set_level(strtoul(optarg,NULL,0));
			break;
		case 'r':
			strcpy(remote,optarg);
			break;
		case 'x':
			numtoggle = strtoul(optarg, NULL, 0);
			if (numtoggle == ULONG_MAX) 
				numtoggle = 0;
			break;
		case 'X':
			srctogglerate = strtoul(optarg, NULL, 0);
			if (srctogglerate > MAX_SECONDS) srctogglerate = MAX_SECONDS;
			break;
		case 's':
			seconds = strtoul(optarg, NULL, 0);
			if (seconds > MAX_SECONDS) seconds = MAX_SECONDS;
			break;
		case 't':
			error_threshold = strtoul(optarg, NULL, 0);
			break;
		case 'D':
			delay = strtoul(optarg, NULL, 0);
			break;
		default:
			usage(argv);
			return -1;
		}
	}

	if (optind >= argc) {
		usage(argv);
		return -1;
	}

	f=fopen(argv[optind],"r");
	if (f == NULL) {
		fprintf(stderr,"Failed to open guid file (%s)\n",argv[optind+1]);
		return -1; 
	}

	numsources = get_sources();
	if (numsources < 0) {
		fprintf(stderr,"Could not read source port data.\n");
		ret = -1;
		goto failopenguid;
	}

	hfi = op_path_find_hfi("",&device);
	if (!hfi || !device) {
		fprintf(stderr,"Could not open HFI.\n");
		ret = -1;
		goto failopenguid;
	}

	context = op_path_open(device, port);
	if (!context) {
		fprintf(stderr, "Could not open path interface.\n");
		goto failopen;
	}

	dest_ports = malloc(sizeof(record)*numdests);
	if (!dest_ports) {
		fprintf(stderr, "Could not allocate memory for destinations.\n");
		goto failmalloc;
	}

	i=0;
	while (!readrecord(f, &dest_ports[i])) {
		i++;
		if (i >= numdests) {
			numdests = numdests * 2;
			dest_ports = realloc(dest_ports, numdests * sizeof(record));
			if (!dest_ports) {
				fprintf(stderr, "Could not allocate memory for destinations.\n");
				goto failmalloc;
			}
		}
	}
		
	fclose(f);
	f = NULL;
	numdests=i;
	_DBG_NOTICE("Read %u destinations and %u sources.\n",
			numdests,numsources);
	if (numsources == 0 || numdests == 0) goto fail;

	falsepos=falseneg=queries=passes=0;
	
	gettimeofday(&start_time, NULL);
	next_src_toggle = start_time;
	next_src_toggle.tv_sec += srctogglerate;
	
	srand48(start_time.tv_sec);

	if (enable_ports) {
		_DBG_PRINT("Instructing the simulator to enable all ports.\n");
		for (i=0; i<numsources; i++) {
			src_portenable(&src_ports[i]);
		}

		for (i=0; i<numdests; i++) {
			portenable(&dest_ports[i]);
		}
		_DBG_PRINT("Sleeping %d seconds before scanning.\n", delay);
		my_sleep(delay);
	}

	do {
		unsigned j;
		char datestring[256];
		time_t now = time(NULL);
		struct tm *ptm;

		if (now && (ptm = localtime(&now)) != NULL) {
			strftime (datestring, 
					  sizeof(datestring), 
					  nl_langinfo (D_T_FMT), 
					  ptm);
		} else {
			strncpy(datestring,"(unknown)", sizeof(datestring));
		}

		_DBG_PRINT("Pass: %u, %s\n", passes, datestring);

		//for (j=0; j<QUERIESPERPASS; j++) {
		for (j=0; j<numdests; j++) {
			op_path_rec_t query, response;
			int d = j; //lrand48() % numdests;
			int s = lrand48() % numsources;
			int lid_query = 0;

			memset(&query,0,sizeof(query));

			lid_query = lrand48() % 2;
			if (lid_query) {
				unsigned lmc_offset = lrand48()%src_ports[s].num_lids;
				query.slid = htons(ntohs(src_ports[s].base_lid) + lmc_offset);
				query.dlid = htons(ntohs(dest_ports[d].lid) + lmc_offset);
			} else {
				query.sgid.unicast.prefix = src_ports[s].prefix;
				query.sgid.unicast.interface_id = src_ports[s].guid;
				query.dgid.unicast.prefix = src_ports[s].prefix;
				query.dgid.unicast.interface_id = dest_ports[d].guid;
			}
			if (numpkeys) query.pkey = pkey[lrand48() % numpkeys];
			if (numsids) query.service_id = sid[lrand48() % numsids];

			queries++;
			err = op_path_get_path_by_rec(context,
										  &query,
										  &response);
			pf = 0;
			if ((err != 0) && (dest_ports[d].disable == 0) &&
				(src_ports[s].disable == 0)) {
					pf=1;
					falseneg++;
					if (lid_query) 
						dest_ports[d].lid_fail_count++;
					else
						dest_ports[d].guid_fail_count++;

			} else if ((err == 0) && ((dest_ports[d].disable != 0) ||
                (src_ports[s].disable != 0))) {
					pf = 1;
					falsepos++;
					if (lid_query) 
						dest_ports[d].lid_fail_count++;
					else
						dest_ports[d].guid_fail_count++;
			} else if (lid_query) {
				dest_ports[d].lid_fail_count = 0;
			} else {
				dest_ports[d].guid_fail_count = 0;
			}

			if (pf) {
				_DBG_ERROR("Get Path returned %d for query on %s port 0x%016"PRIx64".\n(%d lid errors, %d gid errors on this port.)\n",
						err, (dest_ports[d].disable || src_ports[s].disable)?"disabled":"enabled",
						ntoh64(dest_ports[d].guid),
						dest_ports[d].lid_fail_count, dest_ports[d].guid_fail_count);
				if (debug >= _DBG_LVL_INFO || dest_ports[d].lid_fail_count > error_threshold ||
					dest_ports[d].guid_fail_count > error_threshold) {
					fprint_path_record(stderr,"Query",&query);
					if (err == 0) 
						print_path_record("Response",&response);
					if (dest_ports[d].lid_fail_count > error_threshold ||
						dest_ports[d].guid_fail_count > error_threshold) {
						seconds = 0;
						_DBG_ERROR("Aborting test.\n");
					}
				}
			}
		}

		_DBG_NOTICE("Completed %u queries for pass %d, %u false negatives, %u false positives.\n", 
					j, passes, falseneg, falsepos);
		
		gettimeofday(&now_time, NULL);
		if ((srctogglerate != 0) && (numsources > 1) &&
			(now_time.tv_sec > next_src_toggle.tv_sec)) {
			static int disabled=0;
			unsigned k;
			
			if (!disabled) {
				k = (unsigned)lrand48() % numsources;
				src_ports[k].disable = 1;
				src_portdisable(&src_ports[k]);
				disabled=1;
			} else {
				for (k=0; k<numsources;k++) {
					if (src_ports[k].disable) {
						src_ports[k].disable = 0;
						src_portenable(&src_ports[k]);
						break;
					}
				}
				disabled=0;
			}
			
			next_src_toggle.tv_sec = now_time.tv_sec + srctogglerate;
		}

		for (j=0; j<numtoggle; j++) {
			unsigned k = (unsigned)lrand48() % numdests;

			dest_ports[k].disable = ~dest_ports[k].disable;
				
			if (dest_ports[k].disable) {
				portdisable(&dest_ports[k]);
			} else {
				portenable(&dest_ports[k]);
			}
			
		} 

		if (numtoggle > 0) {
			_DBG_PRINT("Sleeping %u seconds before next pass.\n", delay);
			my_sleep(delay);
		} else {
			my_sleep(1);
		}
		
		gettimeofday(&end_time, NULL);
		timersub(&end_time, &start_time, &elapsed_time);
		passes++;
	} while (elapsed_time.tv_sec < seconds);

	if (srctogglerate) for (i=0; i<numsources; i++) {
		src_portenable(&src_ports[i]);
	}

	if (numtoggle) for (i=0; i<numdests; i++) {
		portenable(&dest_ports[i]);
	}

	_DBG_PRINT("%u queries, %u false negatives, %u false positives\n%u passes, %ld seconds elapsed time.\n",
		   queries, falseneg, falsepos, passes, elapsed_time.tv_sec);

fail:
	free(dest_ports);
failmalloc:
	op_path_close(context);
failopen:
	ibv_close_device(hfi);
failopenguid:
	if (f) 
		fclose(f);
	return ret;
}
