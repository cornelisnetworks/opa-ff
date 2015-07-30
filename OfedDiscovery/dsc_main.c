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

  \file dsc_main.c
 
  $Author: aestrin $
  $Revision: 1.46 $
  $Date: 2015/01/27 23:00:31 $

  \brief Top level Linux module management routines.
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

#include <oib_utils.h>
#include "iba/ibt.h"
#include "opasadb_debug.h"
#include "dsc_module.h"
#include "dsc_topology.h"
#include "dsc_hca.h"

boolean Daemon = FALSE;
boolean dsc_stop_daemon = FALSE;
boolean dsc_child_started = FALSE;

#define LOG_FILE_LENGTH 256
static char LogFile[LOG_FILE_LENGTH] = {0};

#define DAEMON_PID_FILE "/var/run/ib_iba_dsc.pid"

static char RunDir[LOG_FILE_LENGTH] = {'/', 0};

/*
 * Parameter handling - replaces module parameters.
 *
 * name   - variable name of the parameter.
 * desc   - human readable description
 * type   - enumerated type of the parameter (integer, unsigned, boolean, etc.)
 * length - for string parameters, the max length of the string. 
 * ptr	  - pointer to the variable.
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

#if !defined(stringize) 
#define stringize(x) #x
#define add_quotes(x) stringize(x)
#endif

#define PARAMETER(NAME, TYPE, LENGTH, DESC) { .name=add_quotes(NAME), .type=TYPE, .desc=DESC, .length=LENGTH, .ptr=&NAME }

parameter_t	parameters[] = {
	PARAMETER(NoSubscribe,'b', 0, "Do not subscribe to SM for notifications."),
	PARAMETER(ScanFrequency, 'u', 0, "Number of seconds between unconditional sweeps."),
	PARAMETER(UnsubscribedScanFrequency, 'u', 0, "Number of seconds between sweeps when not receiving notifications."),
	PARAMETER(Dbg, 'x', 0, "Debug logging controls. Uses syslog log levels, ranging from 1 (least)\n\tto 7 (trace level)."),
	PARAMETER(WaitFirstSweep, 'u', 0, "Wait for first sweep to complete before return from module init."),
	PARAMETER(Daemon, 'b', 0, "Run as a daemon process."),
	PARAMETER(Publish, 'b', 0, "Publish paths to shared memory. Defaults to true."),
	{	.name="SID", 
		.type='S', 
		.desc="Service ID that identifies a virtual fabric to include in the cache.\n\tCan be specified multiple times.", 
		.parser=dsc_service_id_range_parser, 
		.printer=dsc_service_id_range_printer, 
		.ptr=(void*)&sid_range_args },
	PARAMETER(LogFile, 's', LOG_FILE_LENGTH, "Write log messages to a file instead of the system log\n\tor stderr."),
	{	.name="DefaultFabric",
		.type='S',
		.desc="normal = do not match SIDs to the default fabric unless they don't\n\t"
		      "match any other fabric.\n\tnone = never match SIDs to the default fabric.\n\t"
			  "Defaults to normal.",
		.parser=dsc_default_fabric_parser,
		.printer=dsc_default_fabric_printer,
		.ptr=(void*)&DefaultFabric },
	PARAMETER(RunDir, 's', LOG_FILE_LENGTH, "The directory to operate out of. Useful for debugging."),
	{"", "", 0, 0,NULL} };

static char *typetostr(parameter_t *p) {
	switch (p->type) {
		case 'i': return "Integer";
		case 'u': return "Unsigned";
		case 'x': return "Hex";
		case 'X': return "64bit Hex";
		case 'b': return "Boolean";
		case 's': return "String";
		case 'S': return "Special";
		default : return "Unknown";
	}
}

/* Did you know that stricmp still isn't a POSIX standard? Well, you do now. */
static int strnicmp(const char *a, const char *b, unsigned l) {
	int r=0;
	char al, bl;

	while (r==0 && l>0 && *a != 0 && *b != 0) {
		al = tolower(*(a++));
		bl = tolower(*(b++));
		l--;

		r = (bl-al);
	}

	return r;
}

static void usage(parameter_t *parameters) {
	int i=0;

	fprintf(stderr,"\n\nUsage:\n\n");
	while (parameters[i].ptr != NULL) {
		if (parameters[i].length == 0) {
			fprintf(stderr,"%s (%s):\n\t%s\n",
				parameters[i].name, typetostr(&parameters[i]),
				parameters[i].desc);
		} else if (parameters[i].type == 's') {
			fprintf(stderr,"%s (%s):\n\t%s\n\tThis value can be up to %d characters long.\n",
				parameters[i].name, typetostr(&parameters[i]),
				parameters[i].desc, parameters[i].length);
		} 
		i++;
	}

	fprintf(stderr,
		"\n"
		"Settings can be provided on the command line or in a config file.\n"
		"To specify a config file, the first argument should be \"-f\"\n"
		"and the second argument should be the name of the config file.\n"
		"If a setting is specified in both places, the command line\n"
		"takes precedence. In either case, syntax takes the form \"p=v\"\n"
		"where \"p\" is the parameter and \"v\" is the value.\n\n");
}

static void dump_params(parameter_t *parameter) {
	int i=0;

	_DBG_FUNC_ENTRY;

	while (parameters[i].ptr != NULL) {
		switch (parameters[i].type) {
			case 'S':
				_DBG_INFO("%s = %s\n", parameters[i].name,
						parameters[i].printer(parameters[i].ptr));
				break;
			case 'X':
				_DBG_INFO("%s = 0x%016"PRIx64"\n", parameters[i].name,
							(*(uint64*)parameters[i].ptr));
				break;
			case 'x':
				_DBG_INFO("%s = 0x%x\n", parameters[i].name,
						(*(uint32*)parameters[i].ptr));
				break;
			case 'u':
				_DBG_INFO("%s = %u\n", parameters[i].name,
						(*(uint32*)parameters[i].ptr));
				break;
			case 'i':
				_DBG_INFO("%s = %i\n", parameters[i].name,
						(*(int32*)parameters[i].ptr));
				break;
			case 'b':
				_DBG_INFO("%s = %s\n", parameters[i].name,
						(*(uint32*)parameters[i].ptr)?"yes":"no");
				break;
			case 's':
				_DBG_INFO("%s = %s\n", parameters[i].name,
						parameters[i].ptr);
				break;
			default:
				_DBG_WARN("%s = Unhandled parameter type.\n",
						parameters[i].name);
				break;
		} 
		i++;
	} 
	_DBG_FUNC_EXIT;
}

static char *trim(char *buffer)
{
	char *p;
	int j = strlen(buffer) - 1;

	// Trim trailing whitespace.
	while (j>0 && isspace(buffer[j])) {
		buffer[j] = 0;
		j--;
	}

	p=buffer;
	
	// Trim leading whitespce.
	j=0;
	while (buffer[j]!=0 && isspace(buffer[j])) j++;

	p=&buffer[j];
	if (*p == '#' || *p == '\n' || *p == 0) 
		return NULL;

	return p;
}

static int parse_record(parameter_t *parameters, char *buffer) 
{
	char *k, *v, *l;
	int i=0;

	_DBG_FUNC_ENTRY;

	k=strtok_r(buffer,"=",&l);
	v=strtok_r(NULL,"=",&l);

	if (!k || !v) {
		_DBG_ERROR("Configuration syntax error.\n");
		return -1;
	}

	// strip white space.
	k=trim(k);
	v=trim(v);

	while ((parameters[i].ptr != NULL) && (k != NULL) & (v != NULL)) {

		if (strnicmp(k,parameters[i].name, strlen(k))==0) {
			switch (parameters[i].type) {
				case 'S':
					return parameters[i].parser(v, parameters[i].ptr);
				case 'X':
					(*(uint64*)parameters[i].ptr) = strtoull(v,NULL,0);
					return 0;
				case 'x':
				case 'u':
					(*(uint32*)parameters[i].ptr) = strtoul(v,NULL,0);
					return 0;
				case 'i':
					(*(int32*)parameters[i].ptr) = strtol(v,NULL,0);
					return 0;
				case 's':
					strncpy((char*)parameters[i].ptr,v,parameters[i].length); 
					((char*)parameters[i].ptr)[parameters[i].length-1]=0;
					return 0;
				case 'b':
					if (strcmp(v,"yes")==0 || 
						strcmp(v,"y") == 0 || 
						strcmp(v,"true") == 0 || 
						strcmp(v,"t") == 0) {
						(*(uint32*)parameters[i].ptr) = 1;
					} else {
						(*(uint32*)parameters[i].ptr) = strtol(v,NULL,0);
					}
					return 0;
				default:
					_DBG_ERROR("Unrecognized token in configuration.\n");
			}
		}
		i++;
	} 

	_DBG_FUNC_EXIT;
	return -1;
}

static int parse_settings(int argc, char **argv, parameter_t *parameters) {
	int i;
	int err = 0;
	char *ptr;

	_DBG_FUNC_ENTRY;

	/* Looking for a config file. */
	if (argc>=3 && strcmp(argv[1],"-f")==0) {
		char record[1024];
		FILE *f = fopen(argv[2],"r");

		if (!f) {
			fprintf(stderr,"Unable to open config file.\n");
			err=-1;
		}

		while(!err && fgets(record,1024,f)) {
			ptr = trim(record);
			if (ptr && strlen(ptr)>0) {
				err = parse_record(parameters, record);
				if (err) {
					fprintf(stderr,"\nInvalid Configuration Option: %s\n", ptr);
				}
			}
		}
		
		i=3;

		if (f) fclose(f);
	} else {
		i=1;
	}

	while (!err && i<argc) {
		err = parse_record(parameters, argv[i]);
		if (err) {
			fprintf(stderr,"\nInvalid Command Line Option: %s\n", argv[i]);
		}
		i++;
	}

	if (err) 
		usage(parameters);

	_DBG_FUNC_EXIT;
	return err;
}

void dsc_sighup_handler(int sig)
{
	_DBG_FUNC_ENTRY;

	dsc_port_event(0, 0, 0, full_rescan);

	_DBG_FUNC_EXIT;
}

void dsc_signal_handler(int sig)
{
	_DBG_FUNC_ENTRY;

	dsc_stop_daemon = TRUE;
	dsc_cleanup();

	_DBG_FUNC_EXIT;
	exit(EXIT_SUCCESS);
}

/*
 * Function : dsc_create_pid_file.
 *
 * Description : Actually creates a pid file at location /var/run.
 */
static void dsc_create_pid_file()
{
	int fd;
	char pid_buf[20];

	_DBG_FUNC_ENTRY;

	fd = open(DAEMON_PID_FILE, O_WRONLY | O_CREAT, 0644);
	if (fd >= 0) {
		sprintf(pid_buf, "%d\n", (int)getpid());
		write(fd, pid_buf, strlen(pid_buf));
		close(fd);
		_DBG_DEBUG("Successfully created a pid file for the Distributed SA daemon.\n");
	} else {
		_DBG_WARN("Failed to create a pid file at location %s\n", DAEMON_PID_FILE);
	}

	_DBG_FUNC_EXIT;
}

/*
 * Function : dsc_create_daemon_pid_file.
 *
 * Description : Creating a pid file in /var/run so that there will not be
 *               multiple instances of daemon running at any time. 
 */
#define PID_BUF_LEN 20
static int dsc_create_daemon_pid_file()
{
	int rval = 0;
	int fd, size;
	char pid_buf[PID_BUF_LEN+1];
	pid_t pid;

	_DBG_FUNC_ENTRY;

	fd = open(DAEMON_PID_FILE, O_RDONLY);
	if (fd >= 0) {
		size = read(fd, pid_buf, PID_BUF_LEN);
		close(fd);
		if (size > 0) {
			pid_buf[size] = '\0';
			pid = atoi(pid_buf);
			/* If the previous daemon is not still running write a
				new pid file immediately.	*/
			if (pid && ((pid == getpid()) || (kill(pid, 0) < 0))) {
				unlink(DAEMON_PID_FILE);
				dsc_create_pid_file();
				goto exit;
			}

			_DBG_WARN("There exists an instance of the Distributed SA daemon already running.\n");
			rval = -1;
			goto exit;
		}
	}

	dsc_create_pid_file();

exit:
	_DBG_FUNC_EXIT;
	return rval;
}

/*
 * Function : dsc_daemon_child_handler.
 *
 * Description : child communicating to parent that it is running fine and parent can
 *               exit now in case of a daemon mode.
 */
static void dsc_daemon_child_handler()
{
	_DBG_FUNC_ENTRY;

	dsc_child_started = TRUE;

	_DBG_FUNC_EXIT;
}

/*
 * Function : dsc_initialize_daemon_and_run.
 *
 * Description : If daemon mode selected, initializes the daemon and runs it.
 */
static int dsc_initialize_daemon_and_run()
{
	pid_t pid;
	int status;

	_DBG_FUNC_ENTRY;

	if (Daemon) {
		signal(SIGUSR1, dsc_daemon_child_handler);
		pid = fork();
		switch(pid) {
			case -1 : 
				_DBG_FATAL("Fork Failed : %s\n", strerror(errno));
				_DBG_FUNC_EXIT;
				return 1;

			case 0  :
				if (setsid() < 0) {
					_DBG_FUNC_EXIT;
					exit(1);
				}

				_DBG_PRINT("Running from %s.\n",RunDir);
				if (chdir(RunDir) < 0) { /* Daemon requirements. */
					_DBG_FATAL("Failed to change directory to %s\n", RunDir);
					_DBG_FUNC_EXIT;
					exit(1);
				}

				umask(0);
				_DBG_PRINT("Distributed SA starting as daemon.\n");
				break; /* Child process. */

			default :
				_DBG_PRINT("PID of the Distributed SA daemon : %ld.\n", pid);
				sleep(5);
				if (dsc_child_started == FALSE ||
						0 != waitpid(pid, &status, WNOHANG)) {
					_DBG_PRINT("Child failed to stay running.\n");
					_DBG_FUNC_EXIT;
					return 1;
				} 

				_DBG_FUNC_EXIT;
				return 0;	/* Child started, parent exits. */
		}

		/* Making sure that we do not accept any input from stdin and no output to the
		   terminal or error. */
		(void) freopen("/dev/null", "r", stdin);
		(void) freopen("/dev/null", "a", stdout);
		(void) freopen("/dev/null", "a", stderr);

	} else {
		_DBG_PRINT("Distributed SA starting.\n");
		_DBG_PRINT("Running from %s.\n",RunDir);
		if (chdir(RunDir) < 0) { /* Daemon requirements. */
			_DBG_FATAL("Failed to change directory to %s\n", RunDir);
			_DBG_FUNC_EXIT;
			exit(1);
		}
	}

	/* Handle some important signals. */
	if (signal(SIGTERM, dsc_signal_handler) == SIG_ERR) {
		goto daemon_exit;
	}

	if (signal(SIGHUP,  dsc_sighup_handler) == SIG_ERR) {
		goto daemon_exit;
	}

	if (signal(SIGINT,  dsc_signal_handler) == SIG_ERR) {
		goto daemon_exit;
	}

	if (Daemon) { /* Telling parent that we got through. */
		kill(getppid(), SIGUSR1);

		if (dsc_create_daemon_pid_file() < 0) {
			_DBG_FUNC_EXIT;
			return 1;
		}

		if (dsc_init() != FSUCCESS) {
			_DBG_FUNC_EXIT;
			return -1;
		}

		while (!dsc_stop_daemon) {
			sleep(10);
		}
	} else {
		if (dsc_init() != FSUCCESS) {
			_DBG_FUNC_EXIT;
			return -1;
		}

		while (getc(stdin) != 'q') {
			sleep(10);
		}
	}

	_DBG_PRINT("Distributed SA terminating.\n");
	_DBG_FUNC_EXIT;
	return 0;

daemon_exit:
	_DBG_ERROR("Failed to replace the default signal handler with the "
			   "signal handler of the daemon.\n");
	_DBG_FUNC_EXIT;
	return 1;
}

int 
main(int argc, char **argv)
{
	FILE *oib_log_file = NULL;
	if (parse_settings(argc, argv, parameters)) {
		return -1;
	}

	if (Dbg > 7) {
		Dbg = 7;
	}

	if (strlen(LogFile)>0) {
		op_log_set_file(LogFile);
		op_log_set_level(Dbg);
		oib_log_file = op_log_get_file();
	} else if (Daemon) { 
		op_log_syslog(basename(argv[0]), Dbg, LOG_LOCAL6); 
		oib_log_file = OIB_DBG_FILE_SYSLOG;
	} else {
		op_log_set_level(Dbg);
		oib_log_file = stderr;
	}

	oib_set_err(oib_log_file);
	if (Dbg >= 7)
		oib_set_dbg(oib_log_file);

	dump_params(parameters);

	return dsc_initialize_daemon_and_run();
}
