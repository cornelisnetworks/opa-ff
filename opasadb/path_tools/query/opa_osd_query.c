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

/*
 * This program demonstrates how to use the op_path_path functions. 
 *
 * It allows the user to create an arbitrary path query and to view
 * the response. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "dumppath.h"
#include "opasadb_path.h"
#include "opasadb_debug.h"

#define DEFAULT_SID 0x1000117500000000

void Usage()
{
	fprintf(stderr,"Usage: opa_osd_query (options)\n");
	fprintf(stderr,"Query the opasadb for path records\n\n");
	fprintf(stderr,"Options are:\n");
	fprintf(stderr, "\t-v/--verbose\t<arg>\tDebug Level. Should be a number between 1 and 7\n");
	fprintf(stderr, "\t-s/--slid\t<arg>\tSource LID\n");
	fprintf(stderr, "\t-d/--dlid\t<arg>\tDestination LID\n");
	fprintf(stderr, "\t-S/--sgid\t<arg>\tSource GID (in GID or inet6 format)\n");
	fprintf(stderr, "\t-D/--dgid\t<arg>\tDestination GID (in GID or inet6 format)\n");
	fprintf(stderr, "\t-k/--pkey\t<arg>\tPartition Key\n");
	fprintf(stderr, "\t-i/--sid\t<arg>\tService ID\n");
	fprintf(stderr, "\t-h/--hfi\t<arg>\tThe HFI to use (Defaults to the first hfi)\n");
	fprintf(stderr, "\t-p/--port\t<arg>\tThe port to use (Defaults to the first port)\n");
	fprintf(stderr, "\t--help\t\t\tProvide full help text\n");
	
	exit(2);
}


void Usage_full()
{
	fprintf(stderr,"Usage: opa_osd_query (options)\n");
	fprintf(stderr,"Query the opasadb for path records\n\n");
	fprintf(stderr,"Options are:\n");
	fprintf(stderr, "\t-v/--verbose\t<arg>\tDebug Level. Should be a number between 1 and 7\n");
	fprintf(stderr, "\t-s/--slid\t<arg>\tSource LID\n");
	fprintf(stderr, "\t-d/--dlid\t<arg>\tDestination LID\n");
	fprintf(stderr, "\t-S/--sgid\t<arg>\tSource GID (in GID or inet6 format)\n");
	fprintf(stderr, "\t-D/--dgid\t<arg>\tDestination GID (in GID or inet6 format)\n");
	fprintf(stderr, "\t-k/--pkey\t<arg>\tPartition Key\n");
	fprintf(stderr, "\t-i/--sid\t<arg>\tService ID\n");
	fprintf(stderr, "\t-h/--hfi\t<arg>\tThe HFI to use (Defaults to the first hfi)\n");
	fprintf(stderr, "\t-p/--port\t<arg>\tThe port to use (Defaults to the first port)\n");
	fprintf(stderr, "\t--help\t\t\tProvide full help text\n");
	fprintf(stderr,"\n"
				"This tool allows you to create an arbitrary path\n"
				"query and view the result. \n\n"
				"All arguments are optional, but ill-formed\n"
				"queries can be expected to fail. You must provide\n"
				"at least a pair of lids, or a pair of gids.\n\n"
				"Mixing of lids and gid in query is not permitted.\n\n"
				"SID or PKey can also be provided but not both.\n\n"
				"If you have multiple HFIs, the same lid can appear on\n"
				"more than one HFI, so you must specify which HFI to use\n"
				"when searching by lids and you have multiple HFIs.\n\n"
				"Numbers can be in decimal, hex or octal.\n\n"
				"Gids can be specified in GID format (\"0x00000000:0x00000000\")\n"
				"or in Inet6 format (\"x:x:x:x:x:x:x:x\").\n\n"
				"The HFI can be identified by name (\"hfi1_0\") or by\n"
				"number (1, 2, 3, et cetera)\n\n"
				"Example:\topa_osd_query -s2 -d4\n");
	exit(0);
}

int main(int argc, char **argv)
{
	int err, debug;
	char *hfi_name = NULL;
	int  port = 1;

	op_path_rec_t query;
	op_path_rec_t response;

	struct ibv_context *context;
	struct ibv_device *device;
	void *hfi;

	memset(&query,0,sizeof(query));
	memset(&response,0,sizeof(response));

	do {
		int c;

		static char *short_options = "v:h:p:s:d:S:D:k:i:";
		static struct option long_options[] = {
			{ .name = "verbose", .has_arg = 1, .val = 'v' },
			{ .name = "slid", .has_arg = 1, .val = 's' },
			{ .name = "dlid", .has_arg = 1, .val = 'd' },
			{ .name = "sgid", .has_arg = 1, .val = 'S' },
			{ .name = "dgid", .has_arg = 1, .val = 'D' },
			{ .name = "pkey", .has_arg = 1, .val = 'k' },
			{ .name = "sid",  .has_arg = 1, .val = 'i' },
			{ .name = "hfi",  .has_arg = 1, .val = 'h' },
			{ .name = "port", .has_arg = 1, .val = 'p' },
			{ .name = "help", .has_arg = 0, .val = '$' },
			{0}
		};
		
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1) 
			break;

		switch (c) {
		case 'v': debug = strtol(optarg,NULL,0); op_log_set_level(debug); break;
		case 'h': hfi_name = (char*)strdupa(optarg); break;
		case 'p': port = strtol(optarg,NULL,0); break;
		case 's': query.slid = htons(strtol(optarg,NULL,0)); break;
		case 'd': query.dlid = htons(strtol(optarg,NULL,0)); break;
		case 'k': query.pkey = htons(strtol(optarg,NULL,0)); break;
		case 'i': query.service_id = hton64(strtoll(optarg, NULL, 0)); break;
		case 'S': 
			if (!parse_gid(optarg, &query.sgid)) {
				fprintf(stderr, "Badly formatted SGID.\n"); 
				return -1;
			}
			break;
		case 'D': 
			if (!parse_gid(optarg, &query.dgid)) {
				fprintf(stderr, "Badly formatted DGID.\n");
				return -1;
			}
			break;
		case '$':
			Usage_full(); //exits
			break;
		default:
			Usage(); //exits
			break;
		}			
	} while (1);

	if ((query.pkey == 0) && (query.service_id == 0)) {
		query.service_id = hton64(DEFAULT_SID);
	}
	print_path_record("Query Parameters", &query);
	if ((query.pkey != 0) && (query.service_id != 0)) {
                fprintf(stderr, "Query using both Service ID and PKey not supported\n");
                return -1;
	}
	/*
 	 * Finds and opens the HFI.
	 */
	hfi = op_path_find_hfi(hfi_name,&device);
	if (!device || !hfi) {
		fprintf(stderr, "Could not open device %s, error code %d\n", (hfi_name?hfi_name:"<null>"), errno);
		return -1;
	} else {
		fprintf(stderr,"Using device %s\n",
				device->name);
	}

	/*
	 * op_path_find_hfi maps the device name to an ibv_device
 	 * structure. op_path_open opens a channel to the SM 
	 * using the specified device and the specified port.
	 */
	if ((context=op_path_open(device,port)) == NULL) {
		fprintf(stderr, "Could not access the Distributed SA. This may mean that\n"
			"the ibacm/dsap has not been started, or that it has not been able\n"
			"to contact the SA.\n");
		return -1;
	}

	err = op_path_get_path_by_rec(context,
								 &query,
								 &response);

	if (err == EINVAL || err == ENOENT) {
		printf("No path found that matches this query.\n");
	} else if (err) {
		printf("******\n"
				"Error: Get Path returned %d for query: %s\n"
				"******\n", 
				err, strerror(err));
	} else {
		print_path_record("Result",&response);
	}

	ibv_close_device(hfi);
	op_path_close(context);

	return 0;
}
