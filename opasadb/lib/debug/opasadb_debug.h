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

/* [ICS VERSION STRING: unknown] */

/*!

  \file srp_debug.h
 
  $Revision: 1.8 $
  $Date: 2015/01/22 18:04:15 $

*/
#ifndef _DBG_DEBUG_H_
#define _DBG_DEBUG_H_

#include <stdarg.h>
#include <syslog.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------------------
		Debug information.
  --------------------------------------------------------------------------------*/

/* ------ Debug Levels definitions ------
 * Following is the list of Debug levels for the user of this debug utility
 * Note: The numbers matches the kernel system log levels. Some of the levels
 *                     as indicated are not used by this debug utility
 */
enum _DBG_LEVELS
{
        _DBG_LVL_EMERGENCY,     /* 0, LOG_EMERG,  System is unusable (Not used, reserved for system usage) */
        _DBG_LVL_ALERT,         /* 1, LOG_ALERT,  Action must be taken immediately (Not used, reserved for system usage) */
        _DBG_LVL_FATAL,         /* 2  LOG_CRIT,   Critical conditions */
        _DBG_LVL_ERROR,         /* 3  LOG_ERR,    Error conditions */                
        _DBG_LVL_WARN,          /* 4  LOG_WARNING,Warning conditions */              
        _DBG_LVL_NOTICE,        /* 5, LOG_NOTICE, Normal but significant condition. */
    	_DBG_LVL_INFO,          /* 6  LOG_INFO,   Informational */
        _DBG_LVL_DEBUG,         /* 7  LOG_DEBUG,  Debug-level messages */
};

/* Default Debug Level */
#define _DBG_LVL_DEFAULT          (_DBG_LVL_NOTICE)

void op_log_syslog(const char *ident, unsigned level, unsigned facility);
void op_log_set_level(unsigned level);
void op_log(const char *fn, unsigned level, const char *format,...);
int  op_log_set_file(const char *fname);
FILE *op_log_get_file(void);

#define stringize(x) #x
#define add_quotes(x) stringize(x)

#define _DBG_INIT(ident, level)					op_log_syslog(ident, level, LOG_LOCAL6)

#if defined(ICSDEBUG)

#define _DBG_ASSERT(_exp_)                      { if(!(_exp_)) op_log(__func__, _DBG_LVL_ERROR, "ASSERT:"#_exp_"\n"); exit(-1); }
#define _DBG_FATAL(varParam...)                 { op_log(__func__, _DBG_LVL_FATAL, "FATAL: "varParam); exit(-1); }
#define _DBG_ERROR(varParam...)                 op_log(__func__, _DBG_LVL_ERROR, "ERROR: "varParam)
#define _DBG_WARN(varParam...)                  op_log(__func__, _DBG_LVL_WARN, "WARN:  "varParam)
#define _DBG_PRINT(varParam...)                 op_log(__func__, _DBG_LVL_NOTICE, varParam)
#define _DBG_NOTICE(varParam...)                op_log(__func__, _DBG_LVL_NOTICE, "NOTICE: "varParam)
#define _DBG_INFO(varParam...)                  op_log(__func__, _DBG_LVL_INFO, "INFO: "varParam)
#define _DBG_DEBUG(varParam...)                 op_log(__func__, _DBG_LVL_DEBUG, "DBG: "varParam)

#define _DBG_FUNC_ENTRY							op_log(__func__, _DBG_LVL_DEBUG, "Enter\n")
#define _DBG_FUNC_EXIT							op_log(__func__, _DBG_LVL_DEBUG, "Exit\n")

#else
// Note that asserts don't terminate in release builds.
#define _DBG_ASSERT(_exp_)                      {	if(!(_exp_)) op_log(NULL, _DBG_LVL_ERROR, "ASSERT:"#_exp_"\n"); }
#define _DBG_FATAL(varParam...)                 { op_log(__func__, _DBG_LVL_FATAL, "FATAL: "varParam); exit(-1); }
#define _DBG_ERROR(varParam...)                 op_log(NULL, _DBG_LVL_ERROR, "ERROR: "varParam)    
#define _DBG_WARN(varParam...)                  op_log(NULL, _DBG_LVL_WARN, "WARN:  "varParam)  
#define _DBG_PRINT(varParam...)                 op_log(NULL, _DBG_LVL_NOTICE, varParam)
#define _DBG_NOTICE(varParam...)                op_log(NULL, _DBG_LVL_NOTICE, "NOTICE: "varParam)
#define _DBG_INFO(varParam...)                  op_log(NULL, _DBG_LVL_INFO, "INFO: "varParam)
#define _DBG_DEBUG(varParam...)                 op_log(NULL, _DBG_LVL_DEBUG, "DBG: "varParam)

#define _DBG_FUNC_ENTRY							
#define _DBG_FUNC_EXIT							

#endif

#define _DBG_PTR(x) 							(x)

#if defined(__cplusplus)
}

#endif
#endif /* _DBG_DEBUG_H_ */
