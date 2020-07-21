 /* BEGIN_ICS_COPYRIGHT4 ****************************************

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

** END_ICS_COPYRIGHT4   ****************************************/
/*!

  \file dsap_module.c
 
  $Revision: 1.5 $
  $Date: 2015/01/27 23:00:24 $

  \brief Provider initialization and cleanup functions.
*/

#include <ctype.h>


/*
  ICS IBT stuff
*/
#include "iba/ibt.h"
#include "iba/public/isemaphore.h"
#include "opasadb_debug.h"

/*
  Module stuff
*/
#include "dsap.h"

// Module parameters


uint32 dsap_dbg_level = _DBG_LVL_NOTICE;

#define LOG_FILE_LENGTH 256
static char dsap_log_file[LOG_FILE_LENGTH] = {0};
static FILE *omgt_log_file = NULL;

/*
 * Parameter handling - replaces module parameters.
 *
 * name   - variable name of the parameter.
 * desc   - human readable description
 * type   - enumerated type of the parameter (integer, unsigned, boolean, etc.)
 * length - for string parameters, the max length of the string. 
 * ptr    - pointer to the variable.
 * parser - points to a function to handle special parameters.
 * printer - points to a function that can print special parameters.
 */
typedef struct parameter {
	char name[32];
	char desc[256];
	char type;
	int  length;
	void *ptr;
	int (*parser)(char *str, void *ptr);
	char *(*printer)(void *ptr);
} parameter_t;

static parameter_t	parameters[] = {
	{	.name="NoSubscribe",
		.type='b',
		.desc="Do not subscribe to SM for notifications.",
		.length = 0,
		.ptr=(void*)&dsap_no_subscribe},
	{	.name="ScanFrequency",
		.type='u',
		.desc="Number of seconds between unconditional sweeps.",
		.length = 0,
		.ptr=(void*)&dsap_scan_frequency},
	{	.name="UnsubscribedScanFrequency",
		.type='u',
		.desc="Number of seconds between sweeps when not receiving "
		      "notifications.",
		.length = 0,
		.ptr=(void*)&dsap_unsub_scan_frequency},
	{	.name="Dbg",
		.type='x',
		.desc="Debug logging controls. Uses syslog log levels, ranging"
		      " from 1 (least)\n\tto 7 (trace level).",
		.length = 0,
		.ptr=(void*)&dsap_dbg_level},
	{	.name="Publish",
		.type='b',
		.desc="Publish paths to shared memory. Defaults to true.",
		.length = 0,
		.ptr=(void*)&dsap_publish},
	{	.name="SID", 
		.type='S', 
		.desc="Service ID that identifies a virtual fabric to include "
		      "in the cache.\n\tCan be specified multiple times.", 
		.parser=dsap_service_id_range_parser, 
		.printer=dsap_service_id_range_printer, 
		.ptr=(void*)&sid_range_args},
	{	.name="LogFile",
		.type='s',
		.desc="Write log messages to a file instead of the system log"
		      "\n\tor stderr.",
		.length = LOG_FILE_LENGTH,
		.ptr=(void*)&dsap_log_file},
	{	.name="dsap_default_fabric",
		.type='S',
		.desc="normal = do not match SIDs to the default fabric unless"
		      " they don't\n\t match any other fabric.\n\tnone = never"
		      " match SIDs to the default fabric.\n\t Defaults to "
		      "normal.",
		.parser=dsap_default_fabric_parser,
		.printer=dsap_default_fabric_printer,
		.ptr=(void*)&dsap_default_fabric },
	{"", "", 0, 0, NULL} 
};

static void dsap_dump_params(parameter_t *params) 
{
	int i=0;

	while (params[i].ptr != NULL) {
		switch (params[i].type) {
		case 'S':
			acm_log(0, "%s = %s\n", params[i].name,
				params[i].printer(params[i].ptr));
			break;
		case 'X':
			acm_log(0, "%s = 0x%016"PRIx64"\n", params[i].name,
				(*(uint64 *) params[i].ptr));
			break;
		case 'x':
			acm_log(0, "%s = 0x%x\n", params[i].name,
				(*(uint32 *) params[i].ptr));
			break;
		case 'u':
			acm_log(0, "%s = %u\n", params[i].name,
				(*(uint32 *) params[i].ptr));
			break;
		case 'i':
			acm_log(0, "%s = %i\n", params[i].name,
				(*(int32 *) params[i].ptr));
			break;
		case 'b':
			acm_log(0, "%s = %s\n", params[i].name,
				(*(uint32 *) params[i].ptr) ? "yes" : "no");
			break;
		case 's':
			acm_log(0, "%s = %s\n", params[i].name,
				(char *)params[i].ptr);
			break;
		default:
			acm_log(0, "%s = Unhandled parameter type.\n",
				params[i].name);
			break;
		} 
		i++;
	} 
}

static char *dsap_trim(char *buffer)
{
	char *p;
	int j = strlen(buffer) - 1;

	/* Trim trailing whitespace. */
	while (j>0 && isspace(buffer[j])) {
		buffer[j] = 0;
		j--;
	}

	p=buffer;
	/* Trim leading whitespce.*/
	j=0;
	while (buffer[j]!=0 && isspace(buffer[j])) j++;

	p=&buffer[j];
	if (*p == '#' || *p == '\n' || *p == 0)
		return NULL;

	return p;
}

static int dsap_parse_record(parameter_t *params, char *buffer) 
{
	char *k, *v, *l;
	int i=0;

	k=strtok_r(buffer, "=", &l);
	v=strtok_r(NULL, "=", &l);

	if (!k || !v) {
		acm_log(0, "Config syntax error: %s\n", buffer);
		return -1;
	}

	/* strip white space */
	k=dsap_trim(k);
	v=dsap_trim(v);
	if (!k || !v) {
		acm_log(0, "Config syntax error: %s\n", buffer);
		return -1;
	}

	while (params[i].ptr != NULL && (k != NULL && v != NULL)) {
		if (strncasecmp(k, params[i].name, strlen(k)) == 0) {
			switch (params[i].type) {
			case 'S':
				return params[i].parser(v, params[i].ptr);
			case 'X':
				(*(uint64 *) params[i].ptr) =
					strtoull(v, NULL, 0);
				return 0;
			case 'x':
			case 'u':
				(*(uint32 *) params[i].ptr) =
					strtoul(v, NULL, 0);
				return 0;
			case 'i':
				(*(int32 *) params[i].ptr) = 
					strtol(v, NULL, 0);
				return 0;
			case 's':
				strncpy((char *) params[i].ptr, v,
					params[i].length);
				((char *) params[i].ptr)[params[i].length - 1] = 0;
				return 0;
			case 'b':
				if (strcmp(v,"yes")==0 || 
				    strcmp(v,"y") == 0 || 
				    strcmp(v,"true") == 0 || 
				    strcmp(v,"t") == 0) 
					(*(uint32 *) params[i].ptr) = 1;
				else 
					(*(uint32 *) params[i].ptr) =
						strtol(v, NULL, 0);
				return 0;
			default:
				acm_log(0, "Unrecognized token in config.\n");
			}
		}
		i++;
	} 

	return -1;
}

static int dsap_parse_settings(char *filename, parameter_t *params)
{
	int err = 0;
	char *ptr;
	char record[1024];
	FILE *f;

	acm_log(2, "\n");
	f = fopen(filename,"r");
	if (!f) {
		acm_log(0, "Unable to open config file %s.\n", filename);
		err=-1;
	}

	while(!err && fgets(record, 1024, f)) {
		ptr = dsap_trim(record);
		if (ptr) {
			if ((err = dsap_parse_record(params, record)))
				acm_log(0, "Invalid Config Option: %s\n", ptr);
		}
	}

	if (f) fclose(f);
	return err;
}

/* Get dsap's own configuration parameters */
void dsap_get_config(char *filename)
{
	if (dsap_parse_settings(filename, parameters))
		return;

	if (dsap_dbg_level > 7) dsap_dbg_level = 7;

	if (strlen(dsap_log_file) > 0) {
		op_log_set_file(dsap_log_file);
		op_log_set_level(dsap_dbg_level);
	}

	dsap_dump_params(parameters);
}


/*!
  \brief Cleanup function called when the provider is unloaded.
*/

void dsap_cleanup(void)
{
	acm_log(2, "\n");

	dsap_scanner_cleanup();
	dsap_topology_cleanup();

	if (omgt_log_file) 
		fclose(omgt_log_file);
}

/*!
  \brief Init function called when the provider is loaded.
*/

FSTATUS
dsap_init(void)
{
	FSTATUS fstatus = FERROR;

	acm_log(2, "\n");


	fstatus = dsap_topology_init();
	if (fstatus != FSUCCESS)
		goto exit;

	fstatus = dsap_scanner_init();
	if (FSUCCESS != fstatus)
		goto exit;

	/* Guard against scans without timeout */
	if (dsap_unsub_scan_frequency == 0)
		dsap_unsub_scan_frequency = 10;

	fstatus = dsap_scanner_start();

exit:
	return fstatus;
}
void dsap_omgt_log_init(struct omgt_port *omgt_port)
{
	if (strlen(dsap_log_file) > 0) {
		omgt_log_file = op_log_get_file();
		omgt_set_err(omgt_port, omgt_log_file);
		if (dsap_dbg_level >= 7)
			omgt_set_dbg(omgt_port, omgt_log_file);
	}
}
