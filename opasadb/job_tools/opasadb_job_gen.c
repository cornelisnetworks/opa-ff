/* BEGIN_ICS_COPYRIGHT5 ****************************************

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

** END_ICS_COPYRIGHT5   ****************************************/

/* [ICS VERSION STRING: unknown] */


/*******************************************************************************
 *
 * INCLUDES
 */

#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <infiniband/umad.h>

#include "ib_ibt.h"

#include "opamgt_sa_priv.h"
#include "opasadb_route2.h"
#include "statustext.h"


/*******************************************************************************
 *
 * DEFINES
 */

// Configuration parameters
#define NAME_PROG  "iba_smjobgen"   // Program name

#define NUM_PARAM_PORT_GUID_ALLOC   8   // Num port GUID params to alloc
#define MAX_CHARS_PORT_GUID_ENTER   18  // Max chars port GUID entry
#define MAX_CHARS_JOB_NAME_ENTER    64  // Max chars job name entry
#define MAX_CHARS_APP_NAME_ENTER    64  // Max chars app name entry

#define MAX_COST_ENTRIES_LINE   14  // Max cost matrix values per line

// Commands
typedef enum
{
	none = 0,                       // No command
	show,                           // Show HFI port GUIDs
	create                          // Create job

} COMMAND;

// Parameter allocation entry structures
//  Note that these structs must align with op_route_param_alloc_entry in
//  opasadb_route2.h
struct op_route_param_alloc_port_guid_entry
{
	uint64_t size_param;            // Size of param (bf_data) allocated (bytes)
	uint64_t size_data;             // Size of param (bf_data) in use (bytes)
	uint64_t size_enter;            // Size of param entered (chars)
	//  NOTE: previous uint64_t ensures uint64_t alignment of bf_data
	char bf_data[MAX_CHARS_PORT_GUID_ENTER + 1];   // Allocated param buffer
};


/*******************************************************************************
 *
 * LOCAL VARIABLES
 */

static COMMAND command = none;                  // Console command

static uint8_t hfi = 0;
static uint8_t port = 0;
static uint64_t connect_portguid = 0;
static uint8_t optn_create_job = OP_ROUTE_CREATE_JOB_NORMAL;

static struct op_route_param_alloc param_port_guid;

static OP_ROUTE_JOB_ID job_id = 0;          // Active job ID
static struct op_route_job_parameters job_params =  // Active job parameters
	{"", "", 0, 0};
static uint16_t switch_index = 0;               // Active switch index value
static struct op_route_portguid_vec portguid_vec;   // Active portguid_vec
static struct op_route_switch_map switch_map;   // Active switch_map
static uint16_t * p_cost_matrix;                // Pointer to active cost matrix
static struct op_route_use_matrix use_matrix;   // Active use_matrix

static boolean fb_have_connectguid = FALSE; // Parsed connect portGUID value
static boolean fb_have_hfiport = FALSE;     // Parsed HFI or port value
static boolean fb_allguids = FALSE;         // All portGUIDs flag
static boolean fb_have_portguid = FALSE;    // Parsed HFI portGUID value
static boolean fb_showcost = FALSE;         // Show cost_matrix flag
static boolean fb_showguidswitch = FALSE;   // Show GUID/switch flag
static boolean fb_showuse = FALSE;          // Show use_matrix flag
static boolean fb_verbose = FALSE;          // Verbose flag

// Command line option table, each has a short and long flag name
static struct option tb_options[] =
{
	{ "verbose", no_argument, NULL, 'v' },
	{ "portguid", required_argument, NULL, 'g' },
	{ "hfi", required_argument, NULL, 'h' },
	{ "port", required_argument, NULL, 'p' },
	{ "jobname", required_argument, NULL, 'J' },
	{ "appname", required_argument, NULL, 'N' },
	{ "pid", required_argument, NULL, 'i' },
	{ "uid", required_argument, NULL, 'I' },
	{ "allports", no_argument, NULL, 'a' },
	{ "guid", required_argument, NULL, 'G' },
	{ "nocreate", no_argument, NULL, 'n' },
	{ "cost", no_argument, NULL, 'c' },
	{ "switch", no_argument, NULL, 's' },
	{ "index", no_argument, NULL, 'S' },
	{ "use", no_argument, NULL, 'u' },
	{ 0 }
};


/*******************************************************************************
 *
 * FUNCTIONS
 */


/*******************************************************************************
 *
 * err_usage()
 *
 * Description:
 *   Output information about program usage and parameters.
 *
 * Inputs:
 *   none
 *
 * Outputs:
 *   none
 */
void err_usage(void)
{
	fprintf(stderr, "Usage: " NAME_PROG " show|create [-g portguid][-h hfi][-p port]\n");
	fprintf(stderr, "                                [-J job_name][-N app_name][-i pid][-I uid]\n");
	fprintf(stderr, "                                [-a][-G portguid][-n][-c][-s][-u][-S index][-v]\n");
	fprintf(stderr, "  -g/--portguid portguid    - port GUID to connect via\n");
	fprintf(stderr, "  -h/--hfi hfi              - hfi to connect via, numbered 1..n, 0= -p port will\n");
	fprintf(stderr, "                              be a system wide port num (default is 0)\n");
	fprintf(stderr, "  -p/--port port            - port to connect via, numbered 1..n, 0=1st active\n");
	fprintf(stderr, "                              (default is 1st active)\n");
	fprintf(stderr, "  -J/--jobname job_name     - create job_name\n");
	fprintf(stderr, "  -N/--appname app_name     - create app_name\n");
	fprintf(stderr, "  -i/--pid pid              - create pid\n");
	fprintf(stderr, "  -I/--uid uid              - create uid\n");
	fprintf(stderr, "  -a/--allports             - show/create all FI ports\n");
	fprintf(stderr, "  -G/--guid portguid        - show/create FI port GUID (multiple entries)\n");
	fprintf(stderr, "  -n/--nocreate             - don't create job\n");
	fprintf(stderr, "  -c/--cost                 - show cost matrix for job\n");
	fprintf(stderr, "  -s/--switch               - show portGUID vector and switch map for job\n");
	fprintf(stderr, "  -u/--use                  - show use matrix for job\n");
	fprintf(stderr, "  -S/--index index          - switch index for show cost matrix, default is 0\n");
	fprintf(stderr, "  -v/--verbose              - verbose output\n");

	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");
}  // End of err_usage()

/*******************************************************************************
 *
 * get_opt()
 *
 * Description:
 *   Get command line options.  Parses command line options using short and
 *   long option (parameter) definitions.  Parameters (file static variables)
 *   are updated appropriately.
 *
 * Inputs:
 *          argc - Number of input parameters
 *          argv - Array of pointers to input parameters
 *   p_opt_short - Pointer to string of short option definitions
 *   tb_opt_long - Array of long option definitions
 * 
 * Outputs:
 *   none
 */
void get_opt( int argc, char ** argv, const char *p_opt_short,
    struct option tb_opt_long[] )
{
	int ix;
	int c_opt;                      // Option parsing char                         
	int ix_opt;                     // Option parsing index
	size_t len_str;                 // String length
	uint64_t temp;                  // Temporary input area
	char * p_opt_end;               // Pointer to end of parsed parameter
	                                // Pointer to port GUID param
	struct op_route_param_alloc_port_guid_entry * p_param_port_guid;

	// Input command
	if (argc >= 2)
	{
		if (!strcmp(argv[1], "show"))
			command = show;
		else if (!strcmp(argv[1], "create"))
			command = create;
	}

	if (command == none)
	{
		fprintf(stderr, NAME_PROG ": Invalid Command:%s\n", argc >= 2 ? argv[1] : "NULL");
		err_usage();
		return;
	}

	// Shift arguments
	for (ix = 1; ix < argc; ix++)
		argv[ix] = argv[ix + 1];
	argc--;

	// Input command line arguments
	while ( ( c_opt = getopt_long( argc, argv, p_opt_short, tb_opt_long,
		&ix_opt ) ) != -1 )
    {
		len_str = 0;
        switch (c_opt)
        {

		// Verbose
		case 'v':
			fb_verbose = TRUE;
			break;

		// Connect port GUID specification
		case 'g':
			if (fb_have_hfiport)
			{
				fprintf(stderr, NAME_PROG ": Can't use -g with -h/-p\n");
				err_usage();
				return;
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No connect port GUID value\n");
				err_usage();
				return;
			}

			errno = 0;
			temp = strtoull(optarg, &p_opt_end, 0);
			if (errno || !p_opt_end || (*p_opt_end != '\0'))
			{
				fprintf(stderr, NAME_PROG ": Invalid connect port GUID value:%s\n", optarg);
				err_usage();
				return;
			}

			connect_portguid = temp;
			fb_have_connectguid = TRUE;
			break;

		// HFI specification
		case 'h':
			if (fb_have_connectguid)
			{
				fprintf(stderr, NAME_PROG ": Can't use -h with -g\n");
				err_usage();
				return;
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No HFI value\n");
				err_usage();
				return;
			}

			errno = 0;
			temp = (uint64_t)strtoul(optarg, &p_opt_end, 0);
			if ( (temp > IB_UINT8_MAX) || errno || !p_opt_end ||
					(*p_opt_end != '\0') )
			{
				fprintf(stderr, NAME_PROG ": Invalid HFI value:%s\n", optarg);
				err_usage();
				return;
			}

			hfi = (uint8_t)temp;
			fb_have_hfiport = TRUE;
			break;

		// Port specification
		case 'p':
			if (fb_have_connectguid)
			{
				fprintf(stderr, NAME_PROG ": Can't use -p with -g\n");
				err_usage();
				return;
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No port value\n");
				err_usage();
				return;
			}

			errno = 0;
			temp = (uint64_t)strtoul(optarg, &p_opt_end, 0);
			if ( (temp > IB_UINT8_MAX) || errno || !p_opt_end ||
					(*p_opt_end != '\0') )
			{
				fprintf(stderr, NAME_PROG ": Invalid port value:%s\n", optarg);
				err_usage();
				return;
			}

			port = (uint8_t)temp;
			fb_have_hfiport = TRUE;
			break;

		// Job name specification
		case 'J':
			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No job name\n");
				err_usage();
				return;
			}

			len_str = strlen(optarg);
			if (len_str > MAX_CHARS_JOB_NAME_ENTER)
			{
				fprintf( stderr, NAME_PROG ": Invalid job name length:%"PRIu64" %s\n",
					len_str, optarg );
				err_usage();
				return;
			}

			strcpy(job_params.name, optarg);
			break;

		// App name specification
		case 'N':
			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No app name\n");
				err_usage();
				return;
			}

			len_str = strlen(optarg);
			if (len_str > MAX_CHARS_APP_NAME_ENTER)
			{
				fprintf( stderr, NAME_PROG ": Invalid app name length:%"PRIu64" %s\n",
					len_str, optarg );
				err_usage();
				return;
			}

			strcpy(job_params.application_name, optarg);
			break;

		// PID specification
		case 'i':
			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No PID value\n");
				err_usage();
				return;
			}

			errno = 0;
			temp = strtoull(optarg, &p_opt_end, 0);
			if (errno || !p_opt_end || (*p_opt_end != '\0'))
			{
				fprintf(stderr, NAME_PROG ": Invalid PID value:%s\n", optarg);
				err_usage();
				return;
			}

			job_params.pid = temp;
			break;

		// UID specification
		case 'I':
			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No UID value\n");
				err_usage();
				return;
			}

			errno = 0;
			temp = strtoull(optarg, &p_opt_end, 0);
			if (errno || !p_opt_end || (*p_opt_end != '\0'))
			{
				fprintf(stderr, NAME_PROG ": Invalid UID value:%s\n", optarg);
				err_usage();
				return;
			}

			job_params.uid = temp;
			break;

		// All FI port GUIDs specification
		case 'a':
			if (fb_have_portguid)
			{
				fprintf(stderr, NAME_PROG ": Can't use -a with -G\n");
				err_usage();
				return;
			}

			fb_allguids = TRUE;
			break;

		// FI port GUID specification
		case 'G':
			if (fb_allguids)
			{
				fprintf(stderr, NAME_PROG ": Can't use -G with -a\n");
				err_usage();
				return;
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No FI port GUID value\n");
				err_usage();
				return;
			}

			errno = 0;
			temp = strtoull(optarg, &p_opt_end, 0);
			if (errno || !p_opt_end || (*p_opt_end != '\0'))
			{
				fprintf(stderr, NAME_PROG ": Invalid FI port GUID value:%s\n", optarg);
				err_usage();
				return;
			}

			// Store FI port GUID parameter
			if (param_port_guid.num_used == param_port_guid.num_allocated)
				if (op_route_alloc_param(&param_port_guid))
				{
					fprintf(stderr, NAME_PROG
						": Unable to allocate port GUID parameter\n");
					err_usage();
					return;
				}

			p_param_port_guid =
				&( (struct op_route_param_alloc_port_guid_entry *)
				param_port_guid.p_params )[param_port_guid.num_used++];
			p_param_port_guid->size_param = sizeof(p_param_port_guid->bf_data);
			p_param_port_guid->size_data = sizeof(uint64_t);
			p_param_port_guid->size_enter = sizeof(uint64_t) * 2;
			*(uint64_t *)(p_param_port_guid->bf_data) = temp;

			fb_have_portguid = TRUE;
			break;

		// Don't create job specification
		case 'n':
			optn_create_job = OP_ROUTE_CREATE_JOB_NO_CREATE;
			break;

		// Show cost matrix specification
		case 'c':
			fb_showcost = TRUE;
			break;

		// Show port GUID vector and switch map specifications
		case 's':
			fb_showguidswitch = TRUE;
			break;

		// Switch index value
		case 'S':
			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No switch index value\n");
				err_usage();
				return;
			}

			errno = 0;
			temp = (uint64_t)strtoul(optarg, &p_opt_end, 0);
			if ( (temp > OP_ROUTE_MAX_SWITCHES) || errno || !p_opt_end ||
					(*p_opt_end != '\0') )
			{
				fprintf(stderr, NAME_PROG ": Invalid switch index value:%s\n", optarg);
				err_usage();
				return;
			}

			switch_index = (uint16_t)temp;
			break;

		// Show use matrix specification
		case 'u':
			fb_showuse = TRUE;
			break;

		default:
			fprintf(stderr, NAME_PROG ": Invalid Option -<%c>\n", c_opt);
			err_usage();
			return;
			break;

        }	// End of switch (c_opt)

    }	// End of while ( ( c_opt = getopt_long( argc, argv,

	// Validate command line arguments
	if (optind < argc)
	{
		fprintf(stderr, "%s: invalid argument %s\n", NAME_PROG, argv[optind]);
		err_usage();
		return;
	}

	if (command == show)
	{
		if (!fb_have_portguid)
			fb_allguids = TRUE;
	}

}	// End of get_opt()

/*******************************************************************************
 *
 * main()
 *
 * Description:
 *   Console function; provides general maintenance and debugging facilities
 *   for library functions.
 *
 * Inputs:
 *   argc - Number of input parameters
 *   argv - Array of pointers to input parameters
 *
 * Outputs:
 *   Exit status 0 - no errors
 *   Exit status 2 - error
 */
int main(int argc, char ** argv)
{
	int ret_val = 0;
	int64 ix, ix_2, ix_3;
	FSTATUS fstatus;
	uint8_t rstatus;
	OP_ROUTE_PORT_HANDLE port_handle = 0;
	OMGT_QUERY SDQuery;
	QUERY_RESULT_VALUES *pSDQueryResults = NULL;
	GUID_RESULTS *pSDGuidResults = NULL;
	static struct omgt_port *omgt_port_session = NULL;

	struct op_route_param_alloc_port_guid_entry * p_param_port_guid;

	// Initialize allocated parameters
	if ( op_route_init_param( &param_port_guid,
			sizeof(struct op_route_param_alloc_port_guid_entry),
			NUM_PARAM_PORT_GUID_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate port_guid parameter\n");
		err_usage();
		return 2;
	}

	// Get and validate command line arguments
	get_opt(argc, argv, "vg:h:p:J:N:i:I:aG:ncsuS:", tb_options);

	// Initialize
	memset(&portguid_vec, 0, sizeof(portguid_vec));
	memset(&switch_map, 0, sizeof(switch_map));
	memset(&use_matrix, 0, sizeof(use_matrix));

	// Get default port GUID if none specified
	if (!fb_have_connectguid)
	{
		fstatus = iba_get_portguid( hfi, port, NULL, &connect_portguid, NULL,
			NULL, NULL, NULL );

		if (FNOT_FOUND == fstatus) {
			fprintf(stderr, NAME_PROG ": %s\n",
					iba_format_get_portguid_error(hfi, port, caCount, portCount));
			//err_usage();
			exit(1);
		} else if (fstatus != FSUCCESS)
		{
			fprintf( stderr, NAME_PROG
				": can't get port GUID from hfi:%u port:%u fstatus:%u (%s)\n",
				hfi, port, (unsigned int)fstatus, iba_fstatus_msg(fstatus) );
			exit(1);
		}
	}

	fstatus = omgt_open_port_by_guid(&omgt_port_session, connect_portguid, NULL);
	if (fstatus != FSUCCESS) {
		fprintf(stderr, "Unable to open fabric interface.\n");
		ret_val = 2;
		goto free_param;
	}

	// Get list of all FI port GUIDS if all specified
	if (fb_allguids)
	{
		memset(&SDQuery, 0, sizeof(SDQuery));
		SDQuery.InputType = InputTypeNodeType;
		SDQuery.InputValue.IbNodeRecord.NodeType = STL_NODE_FI;
		SDQuery.OutputType = OutputTypePortGuid;

		fstatus = omgt_query_sa(omgt_port_session, 
							   &SDQuery, 
							   &pSDQueryResults);

		if (fstatus != FSUCCESS || !pSDQueryResults)
		{
			fprintf( stderr, NAME_PROG ": SD query for FI port GUIDs failed (status=0x%X): %s\n",
				(int)fstatus, iba_fstatus_msg(fstatus) );
			err_usage();
			ret_val = 2;
			goto close_port;
		}

		if ( !(pSDGuidResults = (GUID_RESULTS *)pSDQueryResults->QueryResult) ||
				!pSDGuidResults->NumGuids )
		{
			fprintf(stderr, NAME_PROG ": No SD query FI port GUIDs\n");
			omgt_free_query_result_buffer(pSDQueryResults);
			err_usage();
			ret_val = 2;
			goto close_port;
		}

		for (ix = 0; ix < pSDGuidResults->NumGuids; ix++)
		{
			if (param_port_guid.num_used == param_port_guid.num_allocated)
				if (op_route_alloc_param(&param_port_guid))
				{
					fprintf( stderr, NAME_PROG
						": Unable to allocate port_guid parameter\n" );
					omgt_free_query_result_buffer(pSDQueryResults);
					err_usage();
					ret_val = 2;
					goto close_port;
				}

			p_param_port_guid =
				&( (struct op_route_param_alloc_port_guid_entry *)
				param_port_guid.p_params )[param_port_guid.num_used++];
			p_param_port_guid->size_param = sizeof(p_param_port_guid->bf_data);
			p_param_port_guid->size_data = sizeof(uint64_t);
			p_param_port_guid->size_enter = sizeof(uint64_t) * 2;
			*(uint64_t *)(p_param_port_guid->bf_data) =
				pSDGuidResults->Guids[ix];

		}  // End of for (ix = 0; ix < pSDGuidResults->NumGuids; ix++)

		omgt_free_query_result_buffer(pSDQueryResults);

	}  // End of if (fb_allguids)

	// Open port connection
	if ( (rstatus = op_route_open(omgt_port_session, connect_portguid, &port_handle)) !=
			OP_ROUTE_STATUS_OK )
	{
		fprintf( stderr, NAME_PROG ": open Error rstatus(0x%X):%s, GUID:0x%"
			PRIX64"\n", rstatus, op_route_get_status_text(rstatus),
			connect_portguid );
		err_usage();
		ret_val = 2;
		goto close_port;
	}

	// Copy param_port_guid to portguid_vec
	if ((portguid_vec.num_guids = param_port_guid.num_used))
		if ( !( portguid_vec.p_guids =
				calloc(portguid_vec.num_guids, sizeof(uint64_t)) ) )
		{
			fprintf(stderr, NAME_PROG ": Unable to alloc port GUID vector\n");
			err_usage();
			ret_val = 2;
			goto close_route;
		}

	for ( ix = 0, p_param_port_guid =
			(struct op_route_param_alloc_port_guid_entry *)param_port_guid.p_params;
			ix < portguid_vec.num_guids; p_param_port_guid++, ix++ )
		portguid_vec.p_guids[ix] = *(uint64_t *)p_param_port_guid->bf_data;

	// Execute command
	switch (command)
	{

	// Show command
	case show:
	{
		printf(NAME_PROG ": Show Ports\n");

		printf("CA-Ports(%u):\n", param_port_guid.num_used);
		for (ix = 0; ix < portguid_vec.num_guids; ix++)
			printf("  %-3"PRId64" 0x%016"PRIX64"\n", ix, portguid_vec.p_guids[ix]);

		break;

	}  // End of case show

	// Create job command
	case create:
	{
		rstatus = op_route_create_job( port_handle, optn_create_job, &job_params,
			&portguid_vec, omgt_port_session, &job_id, &switch_map, &p_cost_matrix );
		if (rstatus != OP_ROUTE_STATUS_OK)
		{
			fprintf(stderr, NAME_PROG ": Create Job Error rstatus(0x%X):%s\n",
				rstatus, op_route_get_status_text(rstatus));
			goto close_route;
		}

		printf(NAME_PROG ": Create Job: ID:0x%016"PRIX64"\n", job_id);

		// Show job parameters
		if (fb_verbose)
		{
			printf("\nJob Parameters: Name:%s AppName:%s\n", job_params.name,
				job_params.application_name );
			printf( "  PID:0x%"PRIX64" UID:0x%"PRIX64"\n",
				job_params.pid, job_params.uid );
		}

		// Show port GUID vector and switch map
		if (fb_showguidswitch)
		{
			printf( "\nPortGUID Vector: GUIDs:%-5u    Switch Map: Switches:%-u\n",
				portguid_vec.num_guids, switch_map.num_switches );

			for (ix = 0; ix < portguid_vec.num_guids; ix++)
				printf( "%5"PRId64": 0x%016"PRIX64"       %5u\n",
					ix, portguid_vec.p_guids[ix],
					switch_map.p_switch_indices[ix] );
		}

		// Get and show cost matrix
		if (fb_showcost)
		{
			// Print title and column headers
			printf( "\nCost Matrix (hex): Switches:%u\n",
				switch_map.num_switches );
			if (switch_map.num_switches)
			{
				if (switch_index < switch_map.num_switches)
				{
					ix = switch_index;
					if ( (ix_3 = switch_index + MAX_COST_ENTRIES_LINE) >
							switch_map.num_switches )
						ix_3 = switch_map.num_switches;
				}

				else
				{
					ix = 0;
					if ( (ix_3 = switch_map.num_switches) >
							MAX_COST_ENTRIES_LINE )
						ix_3 = MAX_COST_ENTRIES_LINE;
				}

				printf("        %4"PRId64, ix++);
				for ( ; ix < ix_3; ix++)
					printf(" %4"PRId64, ix);
				printf("\n");

				// Print cost matrix data
				for ( ix = 0; ix < switch_map.num_switches;
						ix++ )
				{
					printf("  %4"PRId64":", ix);

					ix_2 = ix * switch_map.num_switches;
					if (switch_index < switch_map.num_switches)
					{
						ix_2 += switch_index;
						if ( (ix_3 = switch_map.num_switches - switch_index) >
								MAX_COST_ENTRIES_LINE )
							ix_3 = MAX_COST_ENTRIES_LINE;
					}

					else
					{
						if ( (ix_3 = switch_map.num_switches) >
								MAX_COST_ENTRIES_LINE )
							ix_3 = MAX_COST_ENTRIES_LINE;
					}

					for ( ; ix_3 > 0; ix_3--)
						printf(" %04X", p_cost_matrix[ix_2++]);
					printf("\n");

				}  // End of for ( ix = 0; ix < switch_map.num_switches

			}  // End of if (switch_map.num_switches)

		}  // End of if (fb_showcost)

		// Get and show use matrix
		if (fb_showuse)
		{
			printf( "\nUse Matrix: Elements:%u Default Use:%u Multiplier:%u\n",
				use_matrix.num_elements, use_matrix.default_use,
				use_matrix.multiplier );

			for (ix = 0; ix < use_matrix.num_elements; ix++)
				printf( "  %4"PRId64": Index:%5d DLID:0x%04X Use:%u Bursty:%u\n", ix,
					use_matrix.p_elements[ix].switch_index,
					use_matrix.p_elements[ix].dlid,
					use_matrix.p_elements[ix].use,
					use_matrix.p_elements[ix].bursty );
		}

		if (fb_showguidswitch || fb_showcost || fb_showuse)
			printf("\n");

		// Release and clear tables
		op_route_release_portguid_vec(&portguid_vec);
		portguid_vec.num_guids = 0;
		op_route_release_switch_map(&switch_map);
		switch_map.num_switches = 0;
		op_route_release_cost_matrix(p_cost_matrix);
		p_cost_matrix = NULL;
		op_route_release_use_matrix(&use_matrix);
		use_matrix.num_elements = 0;
		use_matrix.default_use = 0;
		use_matrix.multiplier = 0;

		break;

	}  // End of case create

	// Invalid command
	default:
		fprintf(stderr, NAME_PROG ": Invalid Command\n");
		err_usage();
		ret_val = 2;
		break;

	}  // End of switch (command)

	// Close port connection
close_route:
	op_route_close(port_handle);

close_port:
	omgt_close_port(omgt_port_session);

free_param:
	op_route_free_param(&param_port_guid);

	printf("--------------------------------------------------\n");

	return (ret_val);

}  // End of main()


// End of file
