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

  \file opasadb_debug.c
 
  $Revision: 1.10 $
  $Date: 2015/01/27 23:00:35 $

*/
#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <time.h>
#include "opasadb_debug.h"

static unsigned verbosity_level = _DBG_LVL_DEFAULT;
static unsigned use_syslog = 0;
static FILE *log_file = NULL;

int  op_log_set_file(const char *fname) {
	FILE *f = fopen(fname,"a");
	if (!f) return -1;

	log_file = f;
	use_syslog = 0;
	return 0;
}

FILE *op_log_get_file(void) {
	return log_file;
}

void op_log_syslog(const char *ident, unsigned level, unsigned facility) {
	verbosity_level = level;
	openlog(ident,LOG_NOWAIT,facility);
	use_syslog = 1;
}

void op_log_set_level(unsigned level) {
	verbosity_level = level;
}

void op_log(const char *fn, unsigned level, const char *format,...)
{
	va_list ap;
	char buffer[1024];

	// We have to do this here, because stderr isn't a constant.
	if (!log_file && !use_syslog) log_file = stderr;

	if (verbosity_level >= level) {
		if (use_syslog) {
			sprintf(buffer,"%s%c%s",
					(fn)?fn:"",
					(fn)?'/':' ',
					format);
		} else {
			char tbuf[1024];
			time_t t = time(NULL);
			struct tm tm;
		   	
			localtime_r(&t,&tm);
			strftime(tbuf,64,"%Y-%m-%d %H:%M:%S",&tm);
			snprintf(buffer, sizeof(buffer),
					 "%s|%s%c%s",
					 tbuf,
					 (fn)?fn:"",
					 (fn)?'/':' ',
					 format);
		}

		va_start(ap, format);
		if (use_syslog) vsyslog(LOG_CRIT, buffer, ap);
		else vfprintf(log_file, buffer, ap);
		va_end(ap);
	}

	if (!use_syslog) fflush(log_file);
}
