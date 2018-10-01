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

#include <opamgt_priv.h>

#include "opasmaquery.h"


uint8 g_detail = CABLEINFO_DETAIL_BRIEF;
unsigned g_verbose = 0;
PrintDest_t g_dest;
uint8 g_drpath[64] = {0};
char *g_cmdname;

/* for pmaquery */
uint32	g_counterSelectMask = 0xffffffff;	// default is all counters, so set all bits
uint64	g_portSelectMask[4] = {0}; // 256 bitmask
uint64	g_vlSelectMask = 0;

/* for pma/sma query*/
uint64_t g_transactID = 0xffffffff12340000; // Upper half overwritten by driver.


optypes_t *g_optypes;

static FSTATUS parsePortMask(const char *str)
{
	unsigned int cnt = 0;
	size_t len = 0;
	char tempStr[65] = {0};

	if (!str)
		return FINVALID_PARAMETER;

	while (isspace(*str)) ++str;

	/* Require leading '0x' */
	if (str[0] != '0' || str[1] != 'x')
		return FINVALID_PARAMETER;

	str = &str[2];

	len = strlen(str);

	snprintf(tempStr, sizeof(tempStr), "%.*s%s",
		(int)(64-len), "0000000000000000000000000000000000000000000000000000000000000000",
		str);

	cnt = sscanf(tempStr, "%16"SCNx64"%16"SCNx64"%16"SCNx64"%16"SCNx64,
		&g_portSelectMask[0], &g_portSelectMask[1], &g_portSelectMask[2],
		&g_portSelectMask[3]);

	if (cnt != 4)
		return FINVALID_PARAMETER;

	return FSUCCESS;
}

void Usage(boolean displayAbridged)
{
	if ( (strcmp(g_cmdname, "opasmaquery") == 0)) {
		sma_Usage(displayAbridged);
	} else if ( (strcmp(g_cmdname, "opapmaquery") == 0)) {
		pma_Usage(displayAbridged);
	}
	exit(1);
}	//Usage

int
string_to_otype(const char *str)
{
	int otype;
	for (otype=0; g_optypes[otype].name; otype++)
	{
		if (strcmp(str, g_optypes[otype].name)==0)
			break;
	}
	return (otype);
}

int main(int argc, char** argv)
{
    int rc;
	STL_SMP smp;
	int c;
	uint8 hfi = 0;
	uint8 port = 0;	// default to port 0, this way pma can will choose first active.
	uint32 slid = 0;
	const char *options;
	int otype = 0;
	uint8 i;
	char *mports = NULL;
	query_func_t func;
	argrec args = {0};
	int status = 0;
    args.sl = 0xff; // set to invalid number to detect if set.

	g_cmdname = strrchr(argv[0], '/');
	if (g_cmdname != NULL) {
		g_cmdname++;
	} else {
		g_cmdname = argv[0];
	}

	if ( (strcmp(g_cmdname, "opasmaquery") == 0)) {
		options = "vd:nl:m:h:p:o:b:f:gt:i?";
		g_optypes = sma_query;
		otype = string_to_otype("nodeinfo");
	} else if ( (strcmp(g_cmdname, "opapmaquery") == 0)) {
		options = "vn:s:l:m:h:p:o:e:w:?";
		g_optypes = stl_pma_query;
		otype = string_to_otype("getportstatus");
	} else {
		fprintf(stderr, "Invalid command name (must be opasmaquery or opapmaquery)\n");
		exit(1);
	}

	VRBSE_PRINT("Debug: cmdname=%s; Options=%s\n", g_cmdname, options);

	if (argc>1) {
		if (!strcmp(argv[1],"--help")) {
			Usage(FALSE);
			exit(2);
		}
	}

	while (-1 != (c = getopt(argc, argv, options))) {
		switch (c) {
		case 'g':
			args.printLineByLine++;
			break;
		case 'd':
			if (FSUCCESS != StringToUint8(&g_detail, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid Detail Level: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			break;
		case 'i':
			args.iflag = TRUE;
			break;
		case 'v':
			g_verbose++;
			if (g_verbose>3) umad_debug(g_verbose-3);
			break;
		case 's':
			if (FSUCCESS != StringToUint8(&args.sl, optarg, NULL, 0, TRUE)
				|| args.sl > 15 ) {
				fprintf(stderr, "%s: Invalid Service Level: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			break;
		case 'l':
			if (FSUCCESS != StringToUint32(&args.dlid, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid DLID: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			break;
		case 't':
			if (FSUCCESS != StringToUint8(&args.table, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid table: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			args.tflag = TRUE;
			break;
		case 'b':
			{
				char *sTemp;

				if (*(sTemp = optarg) == ',') {
					args.block = 0;
					sTemp++;

					if (FSUCCESS != StringToUint8(&args.bcount, sTemp, NULL, 0, TRUE)) {
						fprintf(stderr, "%s: Invalid block count: %s\n", g_cmdname, optarg);
						Usage(FALSE);
					}

					args.bflag = TRUE;
					break;
				}

				if (FSUCCESS != StringToUint16(&args.block, optarg, &sTemp, 0, TRUE)) {
					fprintf(stderr, "%s: Invalid block: %s\n", g_cmdname, optarg);
					Usage(FALSE);
				}

				if (*sTemp == ',') {
					sTemp++;
					if (FSUCCESS != StringToUint8(&args.bcount, sTemp, NULL, 0, TRUE)) {
						fprintf(stderr, "%s: Invalid block count: %s\n", g_cmdname, optarg);
						Usage(FALSE);
					}
				} else {
					args.bcount=1;
				}

				args.bflag = TRUE;
			}
			break;
		case 'f':
			if (FSUCCESS != StringToUint32(&args.flid, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid FLID: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			break;
		case 'm':
			args.mflag = TRUE;
			mports = optarg;	// Can be a single port or a pair.
			break;
		case 'h':
			if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid HFI Number: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			break;
		case 'p':
			if (FSUCCESS != StringToUint8(&port, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid Port Number: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			break;
		case 'o':
			otype = string_to_otype(optarg);
			if (! g_optypes[otype].name)
				{
					fprintf(stderr, "%s: invalid arg for -o %s\n", g_cmdname, optarg);
					Usage(FALSE);
				}
			break;
		case 'e':
			if (FSUCCESS != StringToUint32(&g_counterSelectMask, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid error select mask: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			args.eflag = TRUE;
			break;
		case 'n':
			if (FSUCCESS != parsePortMask(optarg)) {
				fprintf(stderr, "%s: Invalid port select mask: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			break;
		case 'w':
			if (FSUCCESS != StringToUint64(&g_vlSelectMask, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid VL select mask: %s\n", g_cmdname, optarg);
				Usage(FALSE);
			}
			break;

		default:
			Usage(TRUE);
			break;
		}
	}

	func = g_optypes[otype].pf;
	args.oname = g_optypes[otype].name;
	DBGPRINT("argc=%d optind=%d argv[optind] in string= %s\n", argc, optind, argv[optind]);
	if ( (strcmp(g_cmdname, "opasmaquery") == 0)) {
		// for SMA only: allow DR path as additional arguments
		if ((argc - optind) > MAX_DR_PATH)
			Usage(FALSE);

		for (i=1;optind<argc;optind++,i++) {
			if (FSUCCESS != StringToUint8(&g_drpath[i], argv[optind], NULL, 0, TRUE)) {
				fprintf(stderr, "Invalid Directed Route %s\n", argv[optind]);
				Usage(FALSE);
			}
			args.drpaths++;
		}
	} else {
		// all others: no additional arguments
		if (argc > optind)
			Usage(FALSE);
	}



	if (args.flid && ! g_optypes[otype].fflag) {
		fprintf(stderr, "%s: -f ignored for -o %s\n", g_cmdname, g_optypes[otype].name);
	}

	if (args.bflag && ! g_optypes[otype].bflag) {
		fprintf(stderr, "%s: -b ignored for -o %s\n", g_cmdname, g_optypes[otype].name);
	}

	if (args.eflag && ! g_optypes[otype].eflag) {
		fprintf(stderr, "%s: -e ignored for -o %s\n", g_cmdname, g_optypes[otype].name);
	}

	if (args.mflag) {
		if (! g_optypes[otype].mflag && ! g_optypes[otype].mflag2) {
			fprintf(stderr, "%s: -m ignored for -o %s\n", g_cmdname, g_optypes[otype].name);
		} else if (g_optypes[otype].mflag && !strchr(mports,',')) {
			// simple port number
			if (FSUCCESS != StringToUint8(&args.dport, mports, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid port: %s\n", g_cmdname, mports);
				Usage(FALSE);
			}
			args.mcount=1;
		} else if (g_optypes[otype].mflag2 && strchr(mports,',')) {
			// inport,outport
			char *sTemp;
			if (FSUCCESS != StringToUint8(&args.inport, mports, &sTemp, 0, TRUE)
				|| ! sTemp || *sTemp != ',') {
				fprintf(stderr, "%s: Invalid ports: %s, ports must be the form <inport,outport>\n", g_cmdname, mports);
				Usage(FALSE);
			}
			sTemp++;
			if (FSUCCESS != StringToUint8(&args.outport, sTemp, NULL, 0, TRUE)) {
				fprintf(stderr, "%s: Invalid outport: %s\n", g_cmdname, mports);
				Usage(FALSE);
			}
			args.mflag2=1;
		} else if (otype == string_to_otype("portinfo") && strchr(mports,',')) {
			// port,count
			char *sTemp;
			if (FSUCCESS != StringToUint8(&args.dport, mports, &sTemp, 0, TRUE)
				|| ! sTemp || *sTemp != ',') {
				fprintf(stderr, "%s: Invalid ports: %s, ports must be the form <port,count>\n", g_cmdname, mports);
				Usage(FALSE);
			}
			sTemp++;
			if (FSUCCESS != StringToUint8(&args.mcount, sTemp, NULL, 0, TRUE) || args.mcount == 0) {
				fprintf(stderr, "%s: Invalid Port Count: %s\n", g_cmdname, mports);
				Usage(FALSE);
			}
		} else {
			fprintf(stderr, "%s: illegal value for -m: \"%s\"\n",
				g_cmdname, mports);
			Usage(FALSE);
		}
	}

	struct omgt_params params = {.debug_file = g_verbose > 2 ? stdout : NULL};
	status = omgt_open_port_by_num (&args.omgt_port, hfi, port, &params);
	if (status == OMGT_STATUS_NOT_DONE) {
		// Wildcard search for either/or port & hfi yielded no ACTIVE ports.
		if (!port) {
			char *sErr_msg="No Active port found";
			if(!hfi) {
				// System wildcard search for Active port failed.
				// Fallback and query default hfi:1, port:1
				fprintf(stderr, "%s in System. Trying default hfi:1 port:1\n", sErr_msg);
				hfi = 1;
				port = 1;
				status = omgt_open_port_by_num(&args.omgt_port, hfi, port, &params);
			} else {
				fprintf(stderr, "%s on hfi:%d\n", sErr_msg, hfi);
				exit (1);
			}
		}
	}
	if (status != 0) {
		fprintf(stderr, "failed to open port hfi %d:%d: %s\n", hfi, port, strerror(status));
		exit(1);
	}

	(void)omgt_port_get_port_lid(args.omgt_port, &slid);
	(void)omgt_port_get_hfi_port_num(args.omgt_port, &args.port);
	args.slid = (STL_LID)slid;

	PrintDestInitFile(&g_dest, stdout);

	if (! func(&args, (uint8_t *)&smp, sizeof(smp), TRUE)) {
		rc = 1;
	} else {
        rc = 0;
    }

    omgt_close_port(args.omgt_port);

	return rc;
} //main
