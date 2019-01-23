/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015-2018, Intel Corporation

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

#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

// work around conflicting names
#include "infiniband/umad.h"
#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include "iba/ib_ibt.h"
#include "iba/ipublic.h"
#include "ibprint.h"

#include "iba/stl_mad_priv.h"
#include "iba/stl_sm_priv.h"
#include "iba/stl_pm.h"
#include "stl_print.h"

extern uint8 g_detail;
extern unsigned g_verbose;
extern PrintDest_t g_dest;
extern uint8 g_drpath[64];
extern char *g_cmdname;

extern uint64_t g_transactID;
#define RESP_WAIT_TIME (1000)	// 1000 milliseconds for receive response

#if defined(DBGPRINT)
#undef DBGPRINT
#endif
#define VRBSE_PRINT(format, args...) do { if (g_verbose) { fflush(stdout); fprintf(stdout, format, ##args); } } while (0)
#define DBGPRINT(format, args...) do { if (g_verbose>1) { fflush(stdout); fprintf(stderr, format, ##args); } } while (0)

#define MAX_DR_PATH	63

typedef struct _argrec {
	uint8			port;	// local port (-p)
	STL_LID			slid;	//source lid
	STL_LID			dlid;	//dest lid (-l)
	STL_LID			flid;	// -f lid to lookup in fwd table to select block
	boolean				bflag;	// was -b option supplied
	boolean				eflag;  // was -e option used
	uint16				block;	// block specified via -b
	uint8				bcount;	// optional count specified via -b
	uint8				drpaths;// additional SMA command line arguments
	boolean				mflag;	// was -m option supplied
	boolean				mflag2;	// were two ports supplied
	boolean				iflag;	// switch internal
	boolean				tflag;	// was -t option supplied
	uint8				table;	// optional table specification
	uint8				dport;	// typical -m option
	uint8				mcount;	// optional count specified via -m
	uint8				inport; // used for pstate, BCT, SCVL and SCSC maps.
	uint8				outport;// used for pstate, BCT, SCVL and SCSC maps.
	const char*			oname;	// -o name
	struct omgt_port    *omgt_port;
	boolean             printLineByLine; // modify output to be line by line
    uint8               sl;
	boolean				cflag;
} argrec;

typedef	boolean (*query_func_t)(argrec *args, uint8_t *mad, size_t mad_len, boolean print);
typedef struct optypes_s {
	const char *name;
	query_func_t pf;
	boolean displayWithAbridged;
	const char* description;
	boolean mflag;	// is -m port option applicable
	boolean mflag2;	// is -m inport,outport option applicable
	boolean fflag;	// is -f option applicable
	boolean bflag;	// is -b block option applicable
	boolean iflag;	// is -i option applicable
	boolean tflag;	// is -t option applicable
	boolean cflag;	// is -c option applicable
	boolean nflag;  // is -n option applicable
	boolean lflag;  // is -l option applicable
	boolean eflag;  // is -e option applicable
	boolean wflag;  // is -w option applicable
} optypes_t;

extern void Usage(boolean displayAbridged);

// SMA Query
extern optypes_t sma_query [];
extern void sma_Usage(boolean displayAbridged);

// PMA Query
extern optypes_t stl_pma_query [];
extern void pma_Usage(boolean displayAbridged);
extern uint32   g_counterSelectMask;
extern uint64   g_portSelectMask[4];
extern uint64   g_vlSelectMask;

