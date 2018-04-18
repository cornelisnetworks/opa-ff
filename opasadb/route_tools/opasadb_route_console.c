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

#include "opamgt_priv.h"
#include "opasadb_route2.h"
#include "statustext.h"


/*******************************************************************************
 *
 * DEFINES
 */

// Configuration parameters
#define NAME_PROG  "iba_smjobmgmt"  // Program name

#define NUM_NAMES_CA_UMAD           16  // Num of HFI names from umad_get
#define NUM_PORT_GUIDS_UMAD         16  // Num of port GUIDs from umad_get
#define NUM_PARAM_PORT_GUID_ALLOC   8   // Num port GUID params to alloc
#define MAX_CHARS_PORT_GUID_ENTER   18  // Max chars job ID entry
#define MAX_DIGITS_PORT_GUID_MATCH  8   // Max digits for partial job ID match
#define NUM_PARAM_JOB_ID_ALLOC      16  // Num job ID params to alloc
#define MAX_CHARS_JOB_ID_ENTER      18  // Max chars job ID entry
#define MAX_DIGITS_JOB_ID_MATCH     8   // Max digits for partial job ID match
#define NUM_PARAM_JOB_NAME_ALLOC    8   // Num job name params to alloc
#define MAX_CHARS_JOB_NAME_ENTER    64  // Max chars job name entry
#define NUM_PARAM_APP_NAME_ALLOC    8   // Num app name params to alloc
#define MAX_CHARS_APP_NAME_ENTER    64  // Max chars app name entry
#define NUM_PARAM_PID_ALLOC         16  // Num PID params to alloc
#define MAX_CHARS_PID_ENTER         18  // Max chars PID entry
#define NUM_PARAM_UID_ALLOC         16  // Num UID params to alloc
#define MAX_CHARS_UID_ENTER         18  // Max chars UID entry
#define NUM_PARAM_TIME_STAMP_ALLOC  8   // Num time stamp params to alloc
#define NUM_PARAM_JOB_LIST_ALLOC    8   // Num job list params to alloc

#define MAX_COST_ENTRIES_LINE   14  // Max cost matrix values per line

// Commands
typedef enum
{
	none = 0,                       // No command
	show,                           // Show jobs
	clear                           // Clear jobs
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

struct op_route_param_alloc_job_id_entry
{
	uint64_t size_param;            // Size of param (bf_data) allocated (bytes)
	uint64_t size_data;             // Size of param (bf_data) in use (bytes)
	uint64_t size_enter;            // Size of param entered (chars)
	//  NOTE: previous uint64_t ensures uint64_t alignment of bf_data
	char bf_data[MAX_CHARS_JOB_ID_ENTER + 1];   // Allocated param buffer
};

struct op_route_param_alloc_job_name_entry
{
	uint64_t size_param;            // Size of param (bf_data) allocated (bytes)
	uint64_t size_data;             // Size of param (bf_data) in use (bytes)
	uint64_t size_enter;            // Size of param entered (chars)
	//  NOTE: previous uint64_t ensures uint64_t alignment of bf_data
	char bf_data[MAX_CHARS_JOB_NAME_ENTER + 1];     // Allocated param buffer
};

struct op_route_param_alloc_app_name_entry
{
	uint64_t size_param;            // Size of param (bf_data) allocated (bytes)
	uint64_t size_data;             // Size of param (bf_data) in use (bytes)
	uint64_t size_enter;            // Size of param entered (chars)
	//  NOTE: previous uint64_t ensures uint64_t alignment of bf_data
	char bf_data[MAX_CHARS_APP_NAME_ENTER + 1];     // Allocated param buffer
};

struct op_route_param_alloc_pid_entry
{
	uint64_t size_param;            // Size of param (bf_data) allocated (bytes)
	uint64_t size_data;             // Size of param (bf_data) in use (bytes)
	uint64_t size_enter;            // Size of param entered (chars)
	//  NOTE: previous uint64_t ensures uint64_t alignment of bf_data
	char bf_data[MAX_CHARS_PID_ENTER + 1];  // Allocated param buffer
};

struct op_route_param_alloc_uid_entry
{
	uint64_t size_param;            // Size of param (bf_data) allocated (bytes)
	uint64_t size_data;             // Size of param (bf_data) in use (bytes)
	uint64_t size_enter;            // Size of param entered (chars)
	//  NOTE: previous uint64_t ensures uint64_t alignment of bf_data
	char bf_data[MAX_CHARS_UID_ENTER + 1];  // Allocated param buffer
};

struct op_route_param_alloc_time_stamp_entry
{
	uint64_t size_param;            // Size of param (bf_data) allocated (bytes)
	uint64_t size_data;             // Size of param (bf_data) in use (bytes)
	uint64_t fl_lessmore;           // FALSE - less, TRUE - more
	                                //  (see cmp_param_time())
	//  NOTE: previous uint64_t ensures uint64_t alignment of bf_data
	char bf_data[sizeof(time_t)];   // Allocated param buffer
};

struct op_route_param_alloc_job_list_entry
{
	uint64_t reserved;                  // Not used
	uint64_t reserved2;                 // Not used
	uint64_t reserved3;                 // Not used
	uint64_t port_guid;                 // port_guid of job_list
	OP_ROUTE_PORT_HANDLE port_handle;   // port_handle of job_list
	struct op_route_job_list job_list;  // Allocated job_list
};


/*******************************************************************************
 *
 * LOCAL VARIABLES
 */

static COMMAND command = none;                  // Console command

static int32_t num_name_ca;
static char tb_name_ca_umad[NUM_NAMES_CA_UMAD][UMAD_CA_NAME_LEN];
static int32_t num_port_guid = 0;
static uint64_t tb_port_guid_umad[NUM_PORT_GUIDS_UMAD];
static uint8_t hfi = 0;
static uint8_t port = 0;
static uint64_t connect_portguid = 0;

static struct op_route_param_alloc param_port_guid;
static struct op_route_param_alloc param_job_id;
static struct op_route_param_alloc param_job_name;
static struct op_route_param_alloc param_app_name;
static struct op_route_param_alloc param_pid;
static struct op_route_param_alloc param_uid;
static struct op_route_param_alloc param_time_stamp;
static struct op_route_param_alloc param_job_list;

static uint16_t switch_index = 0;               // Active switch index value
static struct op_route_portguid_vec portguid_vec;   // Active portguid_vec
static struct op_route_switch_map switch_map;   // Active switch_map
static uint16_t * p_cost_matrix;                // Pointer to active cost matrix
static struct op_route_use_matrix use_matrix;   // Active use_matrix

static boolean fb_have_portguid = FALSE;    // Parsed portGUID value
static boolean fb_have_hfiport = FALSE;     // Parsed HFI or port value
static boolean fb_allguids = FALSE;         // All portGUIDs flag
static boolean fb_have_jobid = FALSE;       // Parsed job_id value
static boolean fb_have_jobname = FALSE;     // Parsed job name value
static boolean fb_alljobs = FALSE;          // All jobs flag
static boolean fb_have_appname = FALSE;     // Parsed app name value
static boolean fb_have_pid = FALSE;         // Parsed PID value
static boolean fb_have_uid = FALSE;         // Parsed UID value
static boolean fb_have_timestamp = FALSE;   // Parsed time_stamp value
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
	{ "jobid", required_argument, NULL, 'j' },
	{ "jobname", required_argument, NULL, 'J' },
	{ "appname", required_argument, NULL, 'N' },
	{ "pid", required_argument, NULL, 'i' },
	{ "uid", required_argument, NULL, 'I' },
	{ "timeless", required_argument, NULL, 't' },
	{ "timemore", required_argument, NULL, 'T' },
	{ "alljobs", no_argument, NULL, 'a' },
	{ "allfabrics", no_argument, NULL, 'f' },
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
 * cmp_param_str()
 *
 * Description:
 *   Compare specified parameter value to the specified parameter list as a
 *   string.  Comparison for a partial match anywhere in the string is made
 *   with strstr().
 *
 * Inputs:
 *         p_param - Pointer to parameter string on which to perform comparison
 *   p_param_alloc - Pointer to parameter list to compare
 *
 * Outputs:
 *    TRUE - Parameter string matches at least 1 parameter in parameter list
 *   FALSE - Invalid p_param_alloc
 *           No match 
 */
int cmp_param_str( const char * p_param,
    struct op_route_param_alloc * p_param_alloc )
{
	uint32_t ix;
	struct op_route_param_alloc_entry * p_param_entry;

	if ( !p_param || !p_param_alloc || !p_param_alloc->num_used ||
			!p_param_alloc->p_params )
		return (FALSE);

	for ( ix = p_param_alloc->num_used, p_param_entry = p_param_alloc->p_params;
			ix > 0; ix-- )
	{
		if (strstr(p_param, p_param_entry->bf_data))
			return (TRUE);

		p_param_entry = (struct op_route_param_alloc_entry *)
			((char *)p_param_entry + p_param_alloc->size_param);
	}

	return (FALSE);

}  // End of cmp_param_str()

/*******************************************************************************
 *
 * cmp_param_uint64()
 *
 * Description:
 *   Compare specified parameter value to the specified parameter list as
 *   uint64.  Comparison for a partial match of the most significant n digits
 *   is made based on the number of digits (n = info_param) for each parameter
 *   in the parameter list.
 *
 * Inputs:
 *         n_param - Parameter value on which to perform comparison
 *   p_param_alloc - Pointer to parameter list to compare
 *
 * Outputs:
 *    TRUE - Parameter value matches at least 1 parameter in parameter list
 *   FALSE - Invalid p_param_alloc
 *           No match 
 */
int cmp_param_uint64( uint64_t n_param,
    struct op_route_param_alloc * p_param_alloc )
{
	uint32_t ix;
	struct op_route_param_alloc_entry * p_param_entry;

	if (!p_param_alloc || !p_param_alloc->num_used || !p_param_alloc->p_params)
		return (FALSE);

	for ( ix = p_param_alloc->num_used, p_param_entry = p_param_alloc->p_params;
			ix > 0; ix-- )
	{
		if ( ( n_param & ( 0xFFFFFFFFFFFFFFFF <<
				(((sizeof(uint64_t) * 2) - p_param_entry->info_param) * 4) ) ) ==
				*(uint64_t *)p_param_entry->bf_data )
			return (TRUE);

		p_param_entry = (struct op_route_param_alloc_entry *)
			((char *)p_param_entry + p_param_alloc->size_param);
	}

	return (FALSE);

}  // End of cmp_param_uint64()

/*******************************************************************************
 *
 * cmp_param_time()
 *
 * Description:
 *   Compare specified parameter value to the specified parameter list as
 *   time_t.  Comparison for < or > parameter list value is determined by
 *   fl_lessmore flag (see op_route_param_alloc_time_stamp_entry).
 *
 * Inputs:
 *         n_param - Parameter value on which to perform comparison
 *   p_param_alloc - Pointer to parameter list to compare
 *
 * Outputs:
 *    TRUE - Parameter value matches at least 1 parameter in parameter list
 *   FALSE - Invalid p_param_alloc
 *           No match 
 */
int cmp_param_time( time_t n_param,
    struct op_route_param_alloc * p_param_alloc )
{
	uint32_t ix;
	struct op_route_param_alloc_entry * p_param_entry;

	if (!p_param_alloc || !p_param_alloc->num_used || !p_param_alloc->p_params)
		return (FALSE);

	for ( ix = p_param_alloc->num_used, p_param_entry = p_param_alloc->p_params;
			ix > 0; ix-- )
	{
		if ( ( !p_param_entry->info_param &&
				(n_param < *(uint64_t *)p_param_entry->bf_data) ) ||
				( p_param_entry->info_param &&
				(n_param > *(uint64_t *)p_param_entry->bf_data) ) )
			return (TRUE);

		p_param_entry = (struct op_route_param_alloc_entry *)
			((char *)p_param_entry + p_param_alloc->size_param);
	}

	return (FALSE);

}  // End of cmp_param_time()

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
	fprintf(stderr, "Usage: " NAME_PROG " show|clear [-g portguid][-h hfi][-p port][-j job_id][-J job_name]\n");
	fprintf(stderr, "                                [-N app_name][-i pid][-I uid][-t timeless][-T timemore]\n");
	fprintf(stderr, "                                [-a][-f][-c][-s][-u][-S index][-v]\n");
	fprintf(stderr, "  -g/--portguid portguid    - port GUID to connect via (multiple entries)\n");
	fprintf(stderr, "  -h/--hfi hfi              - hfi to connect via, numbered 1..n, 0= -p port will\n");
	fprintf(stderr, "                              be a system wide port num (default is 0)\n");
	fprintf(stderr, "  -p/--port port            - port to connect via, numbered 1..n, 0=1st active\n");
	fprintf(stderr, "                              (default is 1st active)\n");
	fprintf(stderr, "  -j/--jobid job_id         - show/clear job job_id (multiple)\n");
	fprintf(stderr, "  -J/--jobname job_name     - show/clear job job_name (multiple)\n");
	fprintf(stderr, "  -N/--appname app_name     - show/clear job app_name (multiple)\n");
	fprintf(stderr, "  -i/--pid pid              - show/clear job pid (multiple)\n");
	fprintf(stderr, "  -I/--uid uid              - show/clear job uid (multiple)\n");
	fprintf(stderr, "  -t/--timeless timestamp   - show/clear job < timestamp (multiple)\n");
	fprintf(stderr, "  -t/--timemore timestamp   - show/clear job > timestamp (multiple)\n");
	fprintf(stderr, "  -a/--alljobs              - show/clear all jobs\n");
	fprintf(stderr, "  -f/--allfabrics           - show/clear all fabrics\n");
	fprintf(stderr, "  -c/--cost                 - show cost matrix for job(s)\n");
	fprintf(stderr, "  -s/--switch               - show portGUID vector and switch map for job(s)\n");
	fprintf(stderr, "  -u/--use                  - show use matrix for job(s)\n");
	fprintf(stderr, "  -S/--index index          - switch index for show cost matrix, default is 0\n");
	fprintf(stderr, "  -v/--verbose              - verbose output (show jobs)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");

	exit(2);

}  // End of err_usage()

/*******************************************************************************
 *
 * get_opt_time_tm()
 *
 * Description:
 *   Get command line time_stamp specification.  Parses command line time_stamp
 *   option specification of the form 'mm/dd/yyyy hh:mm:ss', and returns the
 *   values in a tm struct.
 *
 * Inputs:
 *   p_str_arg - Pointer to argument string
 *        p_tm - Pointer to tm struct
 * 
 * Outputs:
 *    0 - Get successful
 *        *p_tm = time_stamp specification values
 *   -1 - Get unsuccessful
 */
int get_opt_time_tm(char * p_str_arg, struct tm *p_tm)
{

	p_tm->tm_sec = 0;
	p_tm->tm_min = 0;
	p_tm->tm_hour = 0;
	p_tm->tm_mday = 1;
	p_tm->tm_mon = 0;
	p_tm->tm_year = 0;
	p_tm->tm_wday = 0;
	p_tm->tm_yday = 0;
	p_tm->tm_isdst = 0;

	if ( sscanf( p_str_arg, "%d/%d/%d %d:%d:%d", &p_tm->tm_mon, &p_tm->tm_mday,
			&p_tm->tm_year, &p_tm->tm_hour, &p_tm->tm_min, &p_tm->tm_sec ) != 6 )
		return (-1);

	p_tm->tm_mon -= 1;
	p_tm->tm_year -= 1900;

	return (0);

}	// End of get_opt_time_tm()

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
	struct tm time_stamp;			// Time stamp input area
	char * p_opt_end;               // Pointer to end of parsed parameter
	                                // Pointer to port GUID param
	struct op_route_param_alloc_port_guid_entry * p_param_port_guid;
	                                // Pointer to job ID param
	struct op_route_param_alloc_job_id_entry * p_param_job_id;
	                                // Pointer to job name param
	struct op_route_param_alloc_job_name_entry * p_param_job_name;
	                                // Pointer to app name param
	struct op_route_param_alloc_app_name_entry * p_param_app_name;
	                                // Pointer to PID param
	struct op_route_param_alloc_pid_entry * p_param_pid;
	                                // Pointer to UID param
	struct op_route_param_alloc_uid_entry * p_param_uid;
	                                // Pointer to time stamp param
	struct op_route_param_alloc_time_stamp_entry * p_param_time_stamp;

	// Input command
	if (argc >= 2)
	{
		if (!strcmp(argv[1], "show"))
			command = show;
		else if (!strcmp(argv[1], "clear"))
			command = clear;
	}

	if (command == none)
	{
		fprintf(stderr, NAME_PROG ": Invalid Command:%s\n", argc >= 2 ? argv[1] : "NULL");
		err_usage();
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

		// Port GUID specification
		case 'g':
			if (fb_have_hfiport || fb_allguids)
			{
				fprintf(stderr, NAME_PROG ": Can't use -g with -h/-p/-f\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No port GUID value\n");
				err_usage();
			}

			errno = 0;
			temp = strtoull(optarg, &p_opt_end, 0);
			if (errno || !p_opt_end || (*p_opt_end != '\0'))
			{
				fprintf(stderr, NAME_PROG ": Invalid port GUID value:%s\n", optarg);
				err_usage();
			}

			// Store port GUID parameter
			if (param_port_guid.num_used == param_port_guid.num_allocated)
				if (op_route_alloc_param(&param_port_guid))
				{
					fprintf(stderr, NAME_PROG
						": Unable to allocate port GUID parameter\n");
					err_usage();
				}

			p_param_port_guid =
				&( (struct op_route_param_alloc_port_guid_entry *)
				param_port_guid.p_params )[param_port_guid.num_used++];
			p_param_port_guid->size_param = sizeof(p_param_port_guid->bf_data);
			p_param_port_guid->size_data = sizeof(uint64_t);
			p_param_port_guid->size_enter = sizeof(uint64_t) * 2;
			*(uint64_t *)(p_param_port_guid->bf_data) = temp;
			connect_portguid = temp;

			fb_have_portguid = TRUE;
			break;

		// HFI specification
		case 'h':
			if (fb_have_portguid || fb_allguids)
			{
				fprintf(stderr, NAME_PROG ": Can't use -h with -g/-f\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No HFI value\n");
				err_usage();
			}

			errno = 0;
			temp = (uint64_t)strtoul(optarg, &p_opt_end, 0);
			if ( (temp > IB_UINT8_MAX) || errno || !p_opt_end ||
					(*p_opt_end != '\0') )
			{
				fprintf(stderr, NAME_PROG ": Invalid HFI value:%s\n", optarg);
				err_usage();
			}

			hfi = (uint8_t)temp;
			fb_have_hfiport = TRUE;
			break;

		// Port specification
		case 'p':
			if (fb_have_portguid || fb_allguids)
			{
				fprintf(stderr, NAME_PROG ": Can't use -p with -g/-f\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No port value\n");
				err_usage();
			}

			errno = 0;
			temp = (uint64_t)strtoul(optarg, &p_opt_end, 0);
			if ( (temp > IB_UINT8_MAX) || errno || !p_opt_end ||
					(*p_opt_end != '\0') )
			{
				fprintf(stderr, NAME_PROG ": Invalid port value:%s\n", optarg);
				err_usage();
			}

			port = (uint8_t)temp;
			fb_have_hfiport = TRUE;
			break;

		// Job ID specification
		case 'j':
			if (fb_alljobs)
			{
				fprintf(stderr, NAME_PROG ": Can't use -j with -a\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No job ID value\n");
				err_usage();
			}

			errno = 0;
			temp = strtoull(optarg, &p_opt_end, 16);
			if (errno || !p_opt_end || (*p_opt_end != '\0'))
			{
				fprintf(stderr, NAME_PROG ": Invalid job ID value:%s\n", optarg);
				err_usage();
			}

			// Get length of job ID and process for partial matching
			len_str = strlen(optarg);
			if ((len_str > 2) && ((optarg[1] == 'x') || (optarg[1] == 'X')))
				len_str -= 2;

			if (len_str > MAX_DIGITS_JOB_ID_MATCH)  // No partial ID matching
				len_str = sizeof(OP_ROUTE_JOB_ID) * 2;

			// Store job ID parameter
			if (param_job_id.num_used == param_job_id.num_allocated)
				if (op_route_alloc_param(&param_job_id))
				{
					fprintf(stderr, NAME_PROG ": Unable to allocate parameter\n");
					err_usage();
				}

			p_param_job_id = &( (struct op_route_param_alloc_job_id_entry *)
				param_job_id.p_params )[param_job_id.num_used++];
			p_param_job_id->size_param = sizeof(p_param_job_id->bf_data);
			p_param_job_id->size_data = sizeof(OP_ROUTE_JOB_ID);
			p_param_job_id->size_enter = len_str;
			*(OP_ROUTE_JOB_ID *)(p_param_job_id->bf_data) =
				temp << (((sizeof(OP_ROUTE_JOB_ID) * 2) - len_str) * 4);
			fb_have_jobid = TRUE;

			break;

		// Job name specification
		case 'J':
			if (fb_alljobs)
			{
				fprintf(stderr, NAME_PROG ": Can't use -J with -a\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No job name\n");
				err_usage();
			}

			len_str = strlen(optarg);
			if (len_str > MAX_CHARS_JOB_NAME_ENTER)
			{
				fprintf( stderr, NAME_PROG ": Invalid job name length:%"PRIu64" %s\n",
					len_str, optarg );
				err_usage();
			}

			// Store job name parameter
			if (param_job_name.num_used == param_job_name.num_allocated)
				if (op_route_alloc_param(&param_job_name))
				{
					fprintf(stderr, NAME_PROG ": Unable to allocate parameter\n");
					err_usage();
				}

			p_param_job_name = &( (struct op_route_param_alloc_job_name_entry *)
				param_job_name.p_params )[param_job_name.num_used++];
			p_param_job_name->size_param = sizeof(p_param_job_name->bf_data);
			p_param_job_name->size_data = MAX_CHARS_JOB_NAME_ENTER;
			p_param_job_name->size_enter = len_str;
			strcpy(p_param_job_name->bf_data, optarg);
			fb_have_jobname = TRUE;

			break;

		// App name specification
		case 'N':
			if (fb_alljobs)
			{
				fprintf(stderr, NAME_PROG ": Can't use -N with -a\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No app name\n");
				err_usage();
			}

			len_str = strlen(optarg);
			if (len_str > MAX_CHARS_APP_NAME_ENTER)
			{
				fprintf( stderr, NAME_PROG ": Invalid app name length:%"PRIu64" %s\n",
					len_str, optarg );
				err_usage();
			}

			// Store app name parameter
			if (param_app_name.num_used == param_app_name.num_allocated)
				if (op_route_alloc_param(&param_app_name))
				{
					fprintf(stderr, NAME_PROG ": Unable to allocate parameter\n");
					err_usage();
				}

			p_param_app_name = &( (struct op_route_param_alloc_app_name_entry *)
				param_app_name.p_params )[param_app_name.num_used++];
			p_param_app_name->size_param = sizeof(p_param_app_name->bf_data);
			p_param_app_name->size_data = MAX_CHARS_APP_NAME_ENTER;
			p_param_app_name->size_enter = len_str;
			strcpy(p_param_app_name->bf_data, optarg);
			fb_have_appname = TRUE;

			break;

		// PID specification
		case 'i':
			if (fb_alljobs)
			{
				fprintf(stderr, NAME_PROG ": Can't use -i with -a\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No PID value\n");
				err_usage();
			}

			errno = 0;
			temp = strtoull(optarg, &p_opt_end, 0);
			if (errno || !p_opt_end || (*p_opt_end != '\0'))
			{
				fprintf(stderr, NAME_PROG ": Invalid PID value:%s\n", optarg);
				err_usage();
			}

			// Store PID parameter
			if (param_pid.num_used == param_pid.num_allocated)
				if (op_route_alloc_param(&param_pid))
				{
					fprintf(stderr, NAME_PROG
						": Unable to allocate PID parameter\n");
					err_usage();
				}

			p_param_pid =
				&( (struct op_route_param_alloc_pid_entry *)
				param_pid.p_params )[param_pid.num_used++];
			p_param_pid->size_param = sizeof(p_param_pid->bf_data);
			p_param_pid->size_data = sizeof(uint64_t);
			p_param_pid->size_enter = sizeof(uint64_t) * 2;
			*(uint64_t *)(p_param_pid->bf_data) = temp;

			fb_have_pid = TRUE;
			break;

		// UID specification
		case 'I':
			if (fb_alljobs)
			{
				fprintf(stderr, NAME_PROG ": Can't use -I with -a\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No UID value\n");
				err_usage();
			}

			errno = 0;
			temp = strtoull(optarg, &p_opt_end, 0);
			if (errno || !p_opt_end || (*p_opt_end != '\0'))
			{
				fprintf(stderr, NAME_PROG ": Invalid UID value:%s\n", optarg);
				err_usage();
			}

			// Store UID parameter
			if (param_uid.num_used == param_uid.num_allocated)
				if (op_route_alloc_param(&param_uid))
				{
					fprintf(stderr, NAME_PROG
						": Unable to allocate UID parameter\n");
					err_usage();
				}

			p_param_uid =
				&( (struct op_route_param_alloc_uid_entry *)
				param_uid.p_params )[param_uid.num_used++];
			p_param_uid->size_param = sizeof(p_param_uid->bf_data);
			p_param_uid->size_data = sizeof(uint64_t);
			p_param_uid->size_enter = sizeof(uint64_t) * 2;
			*(uint64_t *)(p_param_uid->bf_data) = temp;

			fb_have_uid = TRUE;
			break;

		// Less/more time_stamp specification
		case 't':
		case 'T':
			if (fb_alljobs)
			{
				fprintf(stderr, NAME_PROG ": Can't use -t/-T with -a\n");
				err_usage();
			}

			if (!optarg)
			{
				fprintf(stderr, NAME_PROG ": No time stamp value\n");
				err_usage();
			}

			if ( get_opt_time_tm(optarg, &time_stamp) ||
				(time_stamp.tm_mday < 1) || (time_stamp.tm_mday > 31) ||
				(time_stamp.tm_mon < 0) || (time_stamp.tm_mon > 11) ||
				(time_stamp.tm_year < 70) ||
				(time_stamp.tm_hour < 0) || (time_stamp.tm_hour > 23) ||
				(time_stamp.tm_min < 0) || (time_stamp.tm_min > 59) ||
				(time_stamp.tm_sec < 0) || (time_stamp.tm_sec > 59) )
			{
				fprintf(stderr, NAME_PROG ": Invalid time stamp value:(%s)\n", optarg);
				err_usage();
			}

			// Store time_stamp parameter
			if (param_time_stamp.num_used == param_time_stamp.num_allocated)
				if (op_route_alloc_param(&param_time_stamp))
				{
					fprintf(stderr, NAME_PROG
						": Unable to allocate time_stamp parameter\n");
					err_usage();
				}

			p_param_time_stamp =
				&( (struct op_route_param_alloc_time_stamp_entry *)
				param_time_stamp.p_params )[param_time_stamp.num_used++];
			p_param_time_stamp->size_param = sizeof(p_param_time_stamp->bf_data);
			p_param_time_stamp->size_data = sizeof(p_param_time_stamp->bf_data);
			p_param_time_stamp->fl_lessmore = (c_opt == 'T');
			*(time_t *)(p_param_time_stamp->bf_data) = mktime(&time_stamp);

			fb_have_timestamp = TRUE;
			break;

		// All jobs specification
		case 'a':
			if (fb_have_jobid)
			{
				fprintf(stderr, NAME_PROG ": Can't use -a with -j\n");
				err_usage();
			}

			fb_alljobs = TRUE;
			break;

		// All portGUIDs specification
		case 'f':
			if (fb_have_portguid || fb_have_hfiport)
			{
				fprintf(stderr, NAME_PROG ": Can't use -f with -g/-h/-p\n");
				err_usage();
			}

			fb_allguids = TRUE;
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
			}

			errno = 0;
			temp = (uint64_t)strtoul(optarg, &p_opt_end, 0);
			if ( (temp > OP_ROUTE_MAX_SWITCHES) || errno || !p_opt_end ||
					(*p_opt_end != '\0') )
			{
				fprintf(stderr, NAME_PROG ": Invalid switch index value:%s\n", optarg);
				err_usage();
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
			break;

        }	// End of switch (c_opt)

    }	// End of while ( ( c_opt = getopt_long( argc, argv,

	// Validate command line arguments
	if (optind < argc)
	{
		fprintf(stderr, "%s: invalid argument %s\n", NAME_PROG, argv[optind]);
		err_usage();
	}

	if (command == show)
	{
		if (!fb_have_portguid && !fb_have_hfiport)
			fb_allguids = TRUE;

		if ( !fb_have_jobid && !fb_have_jobname && !fb_have_appname &&
				!fb_have_pid && !fb_have_uid && !fb_have_timestamp )
			fb_alljobs = TRUE;
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
	int64 ix, ix_2, ix_3, ix_4, ix_5;
	FSTATUS fstatus;
	uint8_t rstatus;
	uint64_t port_guid = 0;
	static OP_ROUTE_PORT_HANDLE port_handle = 0;
	static uint16_t num_job_lists = 0;
	static uint32_t num_jobs = 0;
	struct op_route_job_list job_list;

	struct op_route_param_alloc_port_guid_entry * p_param_port_guid;
	struct op_route_param_alloc_job_list_entry * p_param_job_list;
	struct op_route_job_info * p_job_info;
	struct omgt_port *omgt_port_session = NULL;

	// Initialize allocated parameters
	if ( op_route_init_param( &param_port_guid,
			sizeof(struct op_route_param_alloc_port_guid_entry),
			NUM_PARAM_PORT_GUID_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate port_guid parameter\n");
		err_usage();
	}

	if ( op_route_init_param( &param_job_id,
			sizeof(struct op_route_param_alloc_job_id_entry),
			NUM_PARAM_JOB_ID_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate job_id parameter\n");
		err_usage();
	}

	if ( op_route_init_param( &param_job_name,
			sizeof(struct op_route_param_alloc_job_name_entry),
			NUM_PARAM_JOB_NAME_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate job name parameter\n");
		err_usage();
	}

	if ( op_route_init_param( &param_app_name,
			sizeof(struct op_route_param_alloc_app_name_entry),
			NUM_PARAM_APP_NAME_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate app name parameter\n");
		err_usage();
	}

	if ( op_route_init_param( &param_pid,
			sizeof(struct op_route_param_alloc_pid_entry),
			NUM_PARAM_PID_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate PID parameter\n");
		err_usage();
	}

	if ( op_route_init_param( &param_uid,
			sizeof(struct op_route_param_alloc_uid_entry),
			NUM_PARAM_UID_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate UID parameter\n");
		err_usage();
	}

	if ( op_route_init_param( &param_time_stamp,
			sizeof(struct op_route_param_alloc_time_stamp_entry),
			NUM_PARAM_TIME_STAMP_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate time stamp parameter\n");
		err_usage();
	}

	if ( op_route_init_param( &param_job_list,
			sizeof(struct op_route_param_alloc_job_list_entry),
			NUM_PARAM_JOB_LIST_ALLOC ) )
	{
		fprintf(stderr, NAME_PROG ": Unable to allocate job_list parameter\n");
		err_usage();
	}

	// Get and validate command line arguments
	get_opt(argc, argv, "vg:h:p:j:J:N:i:I:t:T:afcsuS:Z:", tb_options);

	// Initialize
	portguid_vec.p_guids = NULL;
	switch_map.p_switch_indices = NULL;
	p_cost_matrix = NULL;
	use_matrix.p_elements = NULL;

	// Get list of all port GUIDS on all HFIs if all specified
	if (fb_allguids)
	{
		num_name_ca = umad_get_cas_names(tb_name_ca_umad, NUM_NAMES_CA_UMAD);

		for (ix = 0; ix < num_name_ca; ix++)
		{
			num_port_guid = umad_get_ca_portguids( tb_name_ca_umad[ix],
				tb_port_guid_umad, NUM_PORT_GUIDS_UMAD );

			for (ix_2 = 1; ix_2 < num_port_guid; ix_2++)
			{
				if (param_port_guid.num_used == param_port_guid.num_allocated)
					if (op_route_alloc_param(&param_port_guid))
					{
						fprintf( stderr, NAME_PROG
							": Unable to allocate port_guid parameter\n" );
						err_usage();
					}

				p_param_port_guid =
					&( (struct op_route_param_alloc_port_guid_entry *)
					param_port_guid.p_params )[param_port_guid.num_used++];
				p_param_port_guid->size_param = sizeof(p_param_port_guid->bf_data);
				p_param_port_guid->size_data = sizeof(uint64_t);
				p_param_port_guid->size_enter = sizeof(uint64_t) * 2;
				*(uint64_t *)(p_param_port_guid->bf_data) =
					ntoh64(tb_port_guid_umad[ix_2]);

			}  // End of for (ix_2 = 0; ix_2 < num_port_guid; ix_2++)

		}  // End of for (ix = 0; ix < num_name_ca; ix++)

	}  // End of if (fb_allguids)

	// Get default port GUID if none specified
	if (!fb_have_portguid && !fb_allguids)
	{
		fstatus = iba_get_portguid( hfi, port, NULL,
			(uint64_t *)param_port_guid.p_params->bf_data, NULL, NULL,
			NULL, NULL );
		if (FNOT_FOUND == fstatus) {
			fprintf(stderr, NAME_PROG ": %s\n",
					iba_format_get_portguid_error(hfi, port, caCount, portCount));
			//err_usage();
			exit(1);
		} else if (fstatus != FSUCCESS)
		{
			fprintf( stderr, NAME_PROG
				": can't get port GUID from hfi:%u port:%u fstatus:%d (%s)\n",
				hfi, port, (int)fstatus, iba_fstatus_msg(fstatus) );
			//err_usage();
			exit(1);
		}

		p_param_port_guid = (struct op_route_param_alloc_port_guid_entry *)
			param_port_guid.p_params;
		p_param_port_guid->size_param =
			sizeof(p_param_port_guid->bf_data);
		p_param_port_guid->size_data = sizeof(uint64_t);
		p_param_port_guid->size_enter = sizeof(uint64_t) * 2;
		param_port_guid.num_used++;

		connect_portguid = *(uint64_t *)p_param_port_guid->bf_data;
	}

	fstatus = omgt_open_port_by_guid(&omgt_port_session, connect_portguid, NULL);
	if (fstatus != FSUCCESS) {
		fprintf(stderr, "Unable to open fabric interface.\n");
		err_usage();
		return ret_val;
	}

	// Open port connection(s) and get job list(s)
	for ( ix = param_port_guid.num_used, p_param_port_guid =
			(struct op_route_param_alloc_port_guid_entry *)param_port_guid.p_params;
			ix > 0; p_param_port_guid++, ix-- )
	{
		port_guid = *(uint64_t *)p_param_port_guid->bf_data;

		if ( (rstatus = op_route_open(omgt_port_session, port_guid, &port_handle)) !=
				OP_ROUTE_STATUS_OK )
		{
			fprintf( stderr, NAME_PROG ": open Error:%s, GUID:0x%"PRIX64"\n",
				op_route_get_status_text(rstatus), port_guid );
			err_usage();
		}

		if (param_job_list.num_used == param_job_list.num_allocated)
			if (op_route_alloc_param(&param_job_list))
			{
				fprintf(stderr, NAME_PROG ": Unable to allocate job_list parameter\n");
				err_usage();
			}

		if ( (rstatus = op_route_get_job_list(port_handle, omgt_port_session, &job_list)) !=
				OP_ROUTE_STATUS_OK )
		{
			fprintf( stderr, NAME_PROG ": show Error:%s, can't get job list\n",
				op_route_get_status_text(rstatus) );
			err_usage();
		}

		num_job_lists++;
		p_param_job_list = (struct op_route_param_alloc_job_list_entry *)
			param_job_list.p_params + param_job_list.num_used++;
		p_param_job_list->port_guid = port_guid;
		p_param_job_list->port_handle = port_handle;
		p_param_job_list->job_list = job_list;
		num_jobs += job_list.num_jobs;

	}

	// Execute command
	p_param_job_list =
		(struct op_route_param_alloc_job_list_entry *)param_job_list.p_params;
	port_handle = p_param_job_list->port_handle;

	switch (command)
	{

	// Show command
	case show:
	{
		printf( "Show Job(s) (Fabrics:%u Total Jobs:%u):\n", num_job_lists,
			num_jobs );

		for (ix = param_job_list.num_used; ix > 0; p_param_job_list++, ix--)
		{
			if (p_param_job_list->job_list.num_jobs > 0)
				printf( "\nPort GUID:0x%016"PRIX64
					"\nJob ID (hex)  R:Job Routed  U:Use Matrix Rec'd\n",
					p_param_job_list->port_guid );

			for ( ix_2 = p_param_job_list->job_list.num_jobs,
					p_job_info = p_param_job_list->job_list.p_job_info;
					ix_2 > 0; p_job_info++, ix_2-- )
			{
				if ( fb_alljobs ||
						( ( fb_have_jobid || fb_have_jobname || fb_have_appname ||
						fb_have_pid || fb_have_uid || fb_have_timestamp ) &&
						( !fb_have_jobid || 
						cmp_param_uint64(p_job_info->job_id, &param_job_id) ) &&
						( !fb_have_jobname || 
						cmp_param_str(p_job_info->params.name, &param_job_name) ) &&
						( !fb_have_appname || 
						cmp_param_str( p_job_info->params.application_name,
							&param_app_name ) ) &&
						( !fb_have_pid || 
						cmp_param_uint64(p_job_info->params.pid, &param_pid) ) &&
						( !fb_have_uid || 
						cmp_param_uint64(p_job_info->params.uid, &param_uid) ) &&
						( !fb_have_timestamp || 
						cmp_param_time( p_job_info->time_stamp,
						&param_time_stamp ) ) ) )
				{
					// Show job info
					printf( "%016"PRIX64" %c%c %s\n", p_job_info->job_id,
						p_job_info->routed ? 'R' : '.',
						p_job_info->has_use ? 'U' : '.',
						p_job_info->params.name );

					if (fb_verbose)
					{
						printf( "  Timestamp(%"PRIu64"): %s",
                            (uint64_t)p_job_info->time_stamp,
							ctime(&p_job_info->time_stamp) );
						printf( "  Application:%s\n",
							p_job_info->params.application_name );
						printf( "  PID:0x%"PRIX64" UID:0x%"PRIX64"\n",
							p_job_info->params.pid,
							p_job_info->params.uid );
					}

					// Get interrelated tables
					port_handle = p_param_job_list->port_handle;

					if (fb_showguidswitch)
						if ( ( rstatus = op_route_get_portguid_vec( port_handle,
								p_job_info->job_id, omgt_port_session, &portguid_vec ) ) !=
								OP_ROUTE_STATUS_OK )
						{
							fprintf( stderr, NAME_PROG
								": show Error:%s, can't get port GUID vector\n",
								op_route_get_status_text(rstatus) );
							err_usage();
						}

					if (fb_showguidswitch || fb_showcost)
						if ( ( rstatus = op_route_get_switch_map( port_handle,
								p_job_info->job_id, omgt_port_session, &switch_map ) ) !=
								OP_ROUTE_STATUS_OK )
						{
							fprintf( stderr, NAME_PROG
								": show Error:%s, can't get switch map\n",
								op_route_get_status_text(rstatus) );
							err_usage();
						}

					// Show port GUID vector and cost matrix
					if (fb_showguidswitch)
					{
						printf( "  PortGUID Vector: GUIDs:%-5u    Switch Map: Switches:%-u\n",
							portguid_vec.num_guids, switch_map.num_switches );

						for (ix_3 = 0; ix_3 < portguid_vec.num_guids; ix_3++)
							printf( "  %5"PRId64": 0x%016"PRIX64"       %5u\n",
								ix_3, portguid_vec.p_guids[ix_3],
								switch_map.p_switch_indices[ix_3] );
					}

					// Get and show cost matrix
					if (fb_showcost)
					{
						if ( ( rstatus = op_route_get_cost_matrix( port_handle,
								p_job_info->job_id, omgt_port_session, &p_cost_matrix ) ) !=
								OP_ROUTE_STATUS_OK )
						{
							fprintf( stderr, NAME_PROG
								": show Error:%s, can't get cost matrix\n",
								op_route_get_status_text(rstatus) );
							err_usage();
						}
						if (! p_cost_matrix) 
						{
							fprintf(stderr, NAME_PROG ": failed to get cost matrix\n");
							exit(0);
						}

						// Print title and column headers
						printf( "  Cost Matrix (hex): Switches:%u\n",
							switch_map.num_switches );
						if (switch_map.num_switches)
						{
							if (switch_index < switch_map.num_switches)
							{
								ix_3 = switch_index;
								if ( (ix_5 = switch_index + MAX_COST_ENTRIES_LINE) >
										switch_map.num_switches )
									ix_5 = switch_map.num_switches;
							}

							else
							{
								ix_3 = 0;
								if ( (ix_5 = switch_map.num_switches) >
										MAX_COST_ENTRIES_LINE )
									ix_5 = MAX_COST_ENTRIES_LINE;
							}

							printf("        %4"PRId64, ix_3++);
							for ( ; ix_3 < ix_5; ix_3++)
								printf(" %4"PRId64, ix_3);
							printf("\n");

							// Print cost matrix data
							for ( ix_3 = 0; ix_3 < switch_map.num_switches;
									ix_3++ )
							{
								printf("  %4"PRId64":", ix_3);

								ix_4 = ix_3 * switch_map.num_switches;
								if (switch_index < switch_map.num_switches)
								{
									ix_4 += switch_index;
									if ( (ix_5 = switch_map.num_switches - switch_index) >
											MAX_COST_ENTRIES_LINE )
										ix_5 = MAX_COST_ENTRIES_LINE;
								}

								else
								{
									if ( (ix_5 = switch_map.num_switches) >
											MAX_COST_ENTRIES_LINE )
										ix_5 = MAX_COST_ENTRIES_LINE;
								}

								for ( ; ix_5 > 0; ix_5--)
									printf(" %04X", p_cost_matrix[ix_4++]);
								printf("\n");

							}  // End of for ( ix_3 = 0; ix_3 < switch_map.num_switches

						}  // End of if (switch_map.num_switches)

					}  // End of if (fb_showcost)

					// Get and show use matrix
					if (fb_showuse)
					{
						if ( ( rstatus = op_route_get_use_matrix( port_handle,
								p_job_info->job_id, omgt_port_session, &use_matrix ) ) !=
								OP_ROUTE_STATUS_OK )
						{
							fprintf( stderr, NAME_PROG
								": show Error:%s, can't get use matrix\n",
								op_route_get_status_text(rstatus) );
							err_usage();
						}

						printf( "  Use Matrix: Elements:%u Default Use:%u Multiplier:%u\n",
							use_matrix.num_elements, use_matrix.default_use,
							use_matrix.multiplier );

						for (ix_3 = 0; ix_3 < use_matrix.num_elements; ix_3++)
							printf( "  %4"PRId64": Index:%5d DLID:0x%04X Use:%u Bursty:%u\n", ix_3,
								use_matrix.p_elements[ix_3].switch_index,
								use_matrix.p_elements[ix_3].dlid,
								use_matrix.p_elements[ix_3].use,
								use_matrix.p_elements[ix_3].bursty );
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

				}  // End of if ( fb_alljobs || ( fb_have_jobid &&

			}  // End of for ( ix_2 = job_list.num_jobs, p_job_info =

		}  // End of for (ix = param_job_list.num_used; ix > 0;

		break;

	}  // End of case show

	// Clear command
	case clear:
	{

		printf(NAME_PROG ": Clear Command\n");
		printf( "Clear Job(s) (Fabrics:%u Total Jobs:%u):\n", num_job_lists,
			num_jobs );

		for (ix = param_job_list.num_used; ix > 0; p_param_job_list++, ix--)
		{
			for ( ix_2 = p_param_job_list->job_list.num_jobs,
					p_job_info = p_param_job_list->job_list.p_job_info;
					ix_2 > 0; p_job_info++, ix_2-- )
			{
				if ( fb_alljobs ||
						( ( fb_have_jobid || fb_have_jobname || fb_have_appname ||
						fb_have_pid || fb_have_uid || fb_have_timestamp ) &&
						( !fb_have_jobid || 
						cmp_param_uint64(p_job_info->job_id, &param_job_id) ) &&
						( !fb_have_jobname || 
						cmp_param_str(p_job_info->params.name, &param_job_name) ) &&
						( !fb_have_appname || 
						cmp_param_str( p_job_info->params.application_name,
							&param_app_name ) ) &&
						( !fb_have_pid || 
						cmp_param_uint64(p_job_info->params.pid, &param_pid) ) &&
						( !fb_have_uid || 
						cmp_param_uint64(p_job_info->params.uid, &param_uid) ) &&
						( !fb_have_timestamp || 
						cmp_param_time( p_job_info->time_stamp,
						&param_time_stamp ) ) ) )
				{
					// Complete job
					port_handle = p_param_job_list->port_handle;
					printf( "  Completing Job ID:%016"PRIX64" %s\n",
						p_job_info->job_id, p_job_info->params.name );

					if ( ( rstatus = op_route_complete_job( port_handle,
							p_job_info->job_id, omgt_port_session ) ) != OP_ROUTE_STATUS_OK )
						{
							fprintf( stderr, NAME_PROG
								": clear Error:%s, can't complete job ID:%"PRIX64"\n",
								op_route_get_status_text(rstatus),
								p_job_info->job_id );
							err_usage();
						}

				}  // End of if ( fb_alljobs || ( fb_have_jobid &&

			}  // End of for ( ix_2 = p_param_job_list->job_list.num_jobs

		}  // End of for (ix = param_job_list.num_used; ix > 0;

		break;

	}  // End of case clear

	// Invalid command
	default:
		fprintf(stderr, NAME_PROG ": Invalid Command\n");
		err_usage();
		break;

	}  // End of switch (command)

	// Close port connection(s)
	for ( ix = param_job_list.num_used, p_param_job_list =
			(struct op_route_param_alloc_job_list_entry *)param_job_list.p_params;
			ix > 0; p_param_job_list++, ix-- )
	{
		port_handle = p_param_job_list->port_handle;

		rstatus = op_route_close(port_handle);
	}

	// Free allocated parameters
	op_route_free_param(&param_port_guid);
	op_route_free_param(&param_job_list);
	op_route_free_param(&param_job_id);

	if (omgt_port_session != NULL) {
		omgt_close_port(omgt_port_session);
	}

	printf("--------------------------------------------------\n");

	return (ret_val);

}  // End of main()


// End of file
