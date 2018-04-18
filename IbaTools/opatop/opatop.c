/* BEGIN_ICS_COPYRIGHT7 ****************************************

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

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "stl_pm.h"
#include "topology.h"
#include "opamgt_priv.h"
#include "opamgt_pa_priv.h"
#include "opamgt_pa.h"
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>

#undef DBGPRINT
#define DBGPRINT(format, args...) \
	do { if (g_verbose & VERBOSE_STDERR) { fprintf(stderr, format, ##args); } } while (0)

#define PROGRESS_PRINT(newline, format, args...) \
	do { if (! g_quiet) { ProgressPrint(newline, format, ##args); } } while (0)

#define NAME_PROG  "opatop"				// Program name
#define MAX_COLUMNS_PER_LINE  80
#define MAX_LINES_PER_SCREEN  24
#define MAX_LEVELS_MENU  10
#define MAX_SUMMARY_GROUP_LINES_PER_SCREEN 10
#define MAX_VF_SUMMARY_PER_SCREEN 5
#define MAX_GROUPCONFIG_PORTS_PER_SCREEN  (MAX_LINES_PER_SCREEN - 5)
#define MAX_GROUPFOCUS_PORTS_PER_SCREEN  ((MAX_LINES_PER_SCREEN - 5) / 2)
#define MAX_VFCONFIG_PORTS_PER_SCREEN	  MAX_GROUPCONFIG_PORTS_PER_SCREEN
#define MAX_VFFOCUS_PORTS_PER_SCREEN 	  MAX_GROUPFOCUS_PORTS_PER_SCREEN


#define PACLIENT_SEL_ALL                0x00010000  // All ports in group


/* Screen hierarchy map:
 *
 * LEVEL:  SCREEN_NAME:
 *   0     SCREEN_SUMMARY
 *   1         SCREEN_PM_CONFIG
 *   1         SCREEN_IMAGE_INFO
 *   1         SCREEN_GROUP_INFO_SELECT
 *   2             SCREEN_GROUP_BW_STATS
 *   3                 SCREEN_GROUP_FOCUS
 *   4                     SCREEN_PORT_STATS
 *   2             SCREEN_GROUP_CTG_STATS
 *   3                 SCREEN_GROUP_FOCUS
 *   4                     SCREEN_PORT_STATS
 *   2             SCREEN_GROUP_CONFIG
 *   3                 SCREEN_PORT_STATS
 *	 0     SCREEN_VF_SUMMARY
 *	 1		   SCREEN_PM_CONFIG
 *	 1	       SCREEN_IMAGE_INFO
 *	 1         SCREEN_VF_INFO_SELECT
 *	 2             SCREEN_VF_BW_STATS
 *	 3                 SCREEN_VF_FOCUS
 *	 4                     SCREEN_PORT_STATS
 *	 2             SCREEN_VF_CONFIG
 *	 3                 SCREEN_PORT_STATS
 *	 2             SCREEN_VF_CTG_STATS
 *	 3                 SCREEN_VF_FOCUS
 *	 4                     SCREEN_PORT_STATS
 */
#define SCREEN_SUMMARY            0
#define SCREEN_PM_CONFIG          1
#define SCREEN_IMAGE_INFO         2
#define SCREEN_GROUP_INFO_SELECT  3
#define SCREEN_GROUP_BW_STATS     4
#define SCREEN_GROUP_CTG_STATS    5
#define SCREEN_GROUP_CONFIG       6
#define SCREEN_GROUP_FOCUS        7
#define SCREEN_PORT_STATS         8
#define SCREEN_VF_SUMMARY         9
#define SCREEN_VF_INFO_SELECT     10
#define SCREEN_VF_BW_STATS        11
#define SCREEN_VF_CONFIG          12
#define SCREEN_VF_CTG_STATS       13
#define SCREEN_VF_FOCUS           14

#define VERBOSE_NONE		0				// No verbose output
#define VERBOSE_SCREEN		0x0001			// Verbose screen displays
#define VERBOSE_SCREEN_2	0x0002			// Verbose screen more displays
#define VERBOSE_STDERR		0x0004			// Verbose stderr (opatop)
#define VERBOSE_PACLIENT	0x0010			// Verbose stderr PaClient
#define VERBOSE_TOP_MAD		0x0020			// Verbose stderr Topology MADs
#define VERBOSE_TOP_SWEEP	0x0040			// Verbose stderr Topology sweep

#define DEBUG_NONE			0				// No debug functions
#define DEBUG_GROUP			0x0001			// Display group information
#define DEBUG_IMGID			0x0002			// Display single image ID
#define DEBUG_IMGID_2		0x0004			// Display multiple image IDs

#define PATH_HELP		"/usr/share/opa/help/"
// if help files are bigger than this, extra will be silently ignored
#define MAX_HELP_CHARS	10240	// allows for about 10x growth
#define MAX_HELP_LINES	999	// allows for about 10x growth

// ANSI terminal sequences to change color
char bf_color_off[] = {27, '[', '0', 'm', 0};
char bf_color_red[] = {27, '[', '3', '1', 'm', 0};
char bf_color_green[] = {27, '[', '3', '2', 'm', 0};
char bf_color_yellow[] = {27, '[', '3', '3', 'm', 0};
char bf_color_blue[] = {27, '[', '3', '4', 'm', 0};
char bf_color_cyan[] = {27, '[', '3', '6', 'm', 0};
// ANSI terminal sequence to clear screen
char bf_clear_screen[] = {27, '[', 'H', 27, '[', '2', 'J', 0};

struct termios	g_term;					// terminal I/O settings
struct termios	g_term_save;			// saved terminal I/O settings
int				g_exitstatus	= 0;
EUI64			g_portGuid		= -1;	// local port to use to access fabric
uint32			g_interval		= 10;
struct omgt_port *g_portHandle = NULL;

uint32          g_verbose = VERBOSE_NONE;
uint32          g_debug = DEBUG_NONE;
int				g_quiet = 0;			// omit progress output
int				g_expr_funct = 0;		// Experimental functionality (undocumented)

uint8	hfi = 0;
uint8	port = 0;

uint32	ct_interval = 0;
uint8	n_level_menu = 0;
uint8	tb_menu[MAX_LEVELS_MENU];
char	*p_multi_input = "qQ";

typedef struct _opatop_group_cfg {
	char		groupName[STL_PM_GROUPNAMELEN];
	uint32		numPorts;
	STL_PA_PM_GROUP_CFG_RSP *portList;
} OPATOP_GROUP_CFG;

typedef struct _opatop_vf_cfg {
	char		vfName[STL_PM_VFNAMELEN];
	uint32		numPorts;
	STL_PA_VF_CFG_RSP *portList;
} OPATOP_VF_CFG;

typedef struct _opatop_group_focus {
	char		groupName[STL_PM_GROUPNAMELEN];
	uint32		numPorts;
	uint32		select;
	uint32		start;
	uint32		range;
	STL_FOCUS_PORTS_RSP *portList;
} OPATOP_GROUP_FOCUS;

typedef struct _opatop_vf_focus {
	char		vfName[STL_PM_VFNAMELEN];
	uint32		numPorts;
	uint32		select;
	uint32		start;
	uint32		range;
	STL_PA_VF_FOCUS_PORTS_RSP *portList;
} OPATOP_VF_FOCUS;

typedef struct _opatop_group_list {
	uint32		numGroups;
	STL_PA_GROUP_LIST *groupList;
} OPATOP_GROUP_LIST;

typedef struct _opatop_vf_list {
	uint32		numVFs;
	STL_PA_VF_LIST *vfList;
} OPATOP_VF_LIST;

STL_PA_IMAGE_ID_DATA g_imageIdQuery = {PACLIENT_IMAGE_CURRENT, 0};
STL_PA_IMAGE_ID_DATA g_imageIdResp = {PACLIENT_IMAGE_CURRENT, 0};
STL_PA_IMAGE_ID_DATA g_imageIdFreeze = {PACLIENT_IMAGE_CURRENT, 0};
STL_PA_IMAGE_ID_DATA g_imageIdBookmark = {PACLIENT_IMAGE_CURRENT, 0};
STL_PA_IMAGE_ID_DATA g_imageIdTemp = {PACLIENT_IMAGE_CURRENT, 0};
STL_CLASS_PORT_INFO *g_PaClassPortInfo = NULL;
STL_PA_PM_CFG_DATA g_PmConfig;
STL_PA_IMAGE_INFO_DATA g_PmImageInfo;
OPATOP_GROUP_LIST g_PmGroupList = {0};
STL_PA_PM_GROUP_INFO_DATA g_PmGroupInfo;
OPATOP_GROUP_CFG pg_PmGroupConfig = {{0}};
OPATOP_GROUP_FOCUS pg_PmGroupFocus = {{0}};
OPATOP_VF_LIST g_PmVFList;
STL_PA_VF_INFO_DATA g_PmVFInfo;
OPATOP_VF_CFG g_pPmVFConfig = {{0}};

STL_PORT_COUNTERS_DATA g_portCounters = {0};
STL_PA_VF_PORT_COUNTERS_DATA g_hiddenVfPortCounters;
OPATOP_VF_FOCUS g_pPmVFFocus = {{0}};
uint32 g_portCounterFlags;
uint32 g_hiddenVfPortCounterFlags;
int g_offset = 0;
int g_group = -1;
int g_vf	= -1;
int g_ix_port = -1;
STL_LID g_portlid = 0;
uint8 g_portnum = 0;
uint32 g_select = STL_PA_SELECT_UTIL_HIGH;
uint32 g_start = 0;
uint32 g_range = 10;
int g_scroll = 0;
int g_scroll_summary = 0;
int g_scroll_summary_forward = 0;
int g_scroll_summary_backward = 0;
int g_scroll_vf_summary = 0;
int g_scroll_cntrs = 0;
boolean fb_help = FALSE;
boolean fb_valid_pa_client = FALSE;
boolean fb_valid_pa_cpi = FALSE;


boolean fb_valid_group_list = FALSE;
boolean fb_valid_pm_config = FALSE;
boolean fb_valid_image_info = FALSE;
boolean fb_valid_group_info = FALSE;
boolean fb_valid_group_config = FALSE;
boolean fb_valid_group_focus = FALSE;
boolean fb_valid_port_stats = FALSE;
boolean fb_valid_vf_port_stats_hidden = FALSE;
boolean fb_port_neighbor;
boolean fb_port_has_neighbor;
boolean fb_error_displayed;
boolean fb_valid_VF_list = FALSE;
boolean fb_valid_VF_info = FALSE;
boolean fb_valid_VF_config = FALSE;
boolean fb_valid_VF_focus = FALSE;
char bf_error[81];

/*******************************************************************************
*
* get_input - get user input
*
*  Inputs:
*      n_optn - Options as: Fxxx xxxx
*               F = 1 - Flush input
*
*      n_char - Number of input chars (including trailing 0x0A)
*    bf_input - Pointer to caller input buffer
*    bf_multi - Pointer to multi-char commands (case sensitive)
*
*  Return:
*    -1 - Invalid parameter
*     0 - No input available
*    >0 - Number of input characters available
*
*******************************************************************************/
static int get_input( uint32 n_optn, uint32 n_chars, char *bf_input,
	char *bf_multi )
{
	int32 ret_val = 0;
	static uint32 ct_hold = 0;
	static boolean fl_multi = FALSE;
	fd_set fd_set_read;
	struct timeval time_select;
	static char bf_hold[64];

	if (!ct_hold || (n_optn & 0x80))
	{
		memset(bf_hold, 0, sizeof(bf_hold));
		ct_hold = 0;
		fl_multi = FALSE;
	}

	if (!bf_input || !n_chars || (n_chars > sizeof(bf_hold)))
		return (-1);

	FD_ZERO(&fd_set_read);
	FD_SET(0, &fd_set_read);
	time_select.tv_sec = 0;
	time_select.tv_usec = 10000;		// 10 mSec

	if (select(1, &fd_set_read, NULL, NULL, &time_select) > 0)
		bf_hold[ct_hold++] = fgetc(stdin);

	if (ct_hold >= n_chars)
		bf_hold[(ct_hold = n_chars) - 1] = 0x0A;

	if (!fl_multi && bf_multi && strchr(bf_multi, bf_hold[0]))
		fl_multi = TRUE;

	if (ct_hold && (!fl_multi || (bf_hold[ct_hold - 1] == 0x0A)))
	{
		// Return input string and clean-up for next input sequence
		memcpy(bf_input, bf_hold, ct_hold);
		ret_val = ct_hold;
		ct_hold = 0;
	}

	return (ret_val);

}	// End of get_input()

// for the (maxlen+1)'th character in display of a string, return what to show
// ' ' - string is <= maxlen characters
// '*' - string is > maxlen+1 characters
// str[maxlen] - string is exactly maxlen+1 characters
// designed for output which has strict column widths
char strgetlastchar(const char* str, int maxlen)
{
	int len = strlen(str);
	if (len <= maxlen) return ' ';
	else if (len == maxlen+1) return str[maxlen];
	else /* (len > maxlen+1) */ return '*';
}

// similar to strgetlastchar, except if string is <= maxlen returns ""
// design for output where we don't want extra trailing spaces for str
const char* strgetlaststr(const char* str, int maxlen)
{
	static char buf[2];

	int len = strlen(str);
	if (len <= maxlen) return " ";
	else if (len == maxlen+1) { buf[0] = str[maxlen]; buf[1]='\0'; return buf;}
	else /* (len > maxlen+1) */ return "*";
}
	

int DisplayScreen_SMs(void)
{
	if (g_PmImageInfo.numSMs > 0)
	{
		printf( "    Master-SM: LID: 0x%.*X Port: %-3u Priority: %-2u State: %s\n",
			(g_PmImageInfo.SMInfo[0].lid <= IB_MAX_UCAST_LID ? 4:8),
			g_PmImageInfo.SMInfo[0].lid,
			g_PmImageInfo.SMInfo[0].portNumber,
			g_PmImageInfo.SMInfo[0].priority,
			IbSMStateToText(g_PmImageInfo.SMInfo[0].state) );

		// Truncate smNodeDesc to keep line at 80 columns
		printf( "               Name: %-.58s%s\n",
			g_PmImageInfo.SMInfo[0].smNodeDesc,
			strgetlaststr(g_PmImageInfo.SMInfo[0].smNodeDesc, 58));
		printf( "               PortGUID: 0x%016" PRIX64"\n",
			g_PmImageInfo.SMInfo[0].smPortGuid);
	}

	else
		printf( "    Master-SM: none\n\n\n" );

	if (g_PmImageInfo.numSMs > 1)
	{
		printf( " Secondary-SM: LID: 0x%.*X Port: %-3u Priority: %-2u State: %s\n",
			(g_PmImageInfo.SMInfo[1].lid <= IB_MAX_UCAST_LID ? 4:8),
			g_PmImageInfo.SMInfo[1].lid,
			g_PmImageInfo.SMInfo[1].portNumber,
			g_PmImageInfo.SMInfo[1].priority,
			IbSMStateToText(g_PmImageInfo.SMInfo[1].state) );

		// Truncate smNodeDesc to keep line at 80 columns
		printf( "               Name: %-.58s%s\n",
			g_PmImageInfo.SMInfo[1].smNodeDesc,
			strgetlaststr(g_PmImageInfo.SMInfo[1].smNodeDesc, 58));
		printf( "               PortGUID: 0x%016" PRIX64 "\n",
			g_PmImageInfo.SMInfo[1].smPortGuid);
	}

	else
		printf( " Secondary-SM: none\n\n\n" );

	return 6;
}

int DisplayScreen_Err(STL_PM_CATEGORY_STATS * p_PmErrStats, char * p_title)
{
	printf( "%-4s                  Max       0+%%      25+%%      50+%%      75+%%     100+%%\n",
		p_title );
	printf( "    Integrity  %10u %9u %9u %9u %9u %9u\n",
		p_PmErrStats->categoryMaximums.integrityErrors,
		p_PmErrStats->ports[0].integrityErrors,
		p_PmErrStats->ports[1].integrityErrors,
		p_PmErrStats->ports[2].integrityErrors,
		p_PmErrStats->ports[3].integrityErrors,
		p_PmErrStats->ports[4].integrityErrors );
	printf( "    Congestion %10u %9u %9u %9u %9u %9u\n",
		p_PmErrStats->categoryMaximums.congestion,
		p_PmErrStats->ports[0].congestion,
		p_PmErrStats->ports[1].congestion,
		p_PmErrStats->ports[2].congestion,
		p_PmErrStats->ports[3].congestion,
		p_PmErrStats->ports[4].congestion );
	printf( "    SmaCongest %10u %9u %9u %9u %9u %9u\n",
		p_PmErrStats->categoryMaximums.smaCongestion,
		p_PmErrStats->ports[0].smaCongestion,
		p_PmErrStats->ports[1].smaCongestion,
		p_PmErrStats->ports[2].smaCongestion,
		p_PmErrStats->ports[3].smaCongestion,
		p_PmErrStats->ports[4].smaCongestion );
	printf( "    Bubble     %10u %9u %9u %9u %9u %9u\n",
		p_PmErrStats->categoryMaximums.bubble,
		p_PmErrStats->ports[0].bubble,
		p_PmErrStats->ports[1].bubble,
		p_PmErrStats->ports[2].bubble,
		p_PmErrStats->ports[3].bubble,
		p_PmErrStats->ports[4].bubble );
	printf( "    Security   %10u %9u %9u %9u %9u %9u\n",
		p_PmErrStats->categoryMaximums.securityErrors,
		p_PmErrStats->ports[0].securityErrors,
		p_PmErrStats->ports[1].securityErrors,
		p_PmErrStats->ports[2].securityErrors,
		p_PmErrStats->ports[3].securityErrors,
		p_PmErrStats->ports[4].securityErrors );
	printf( "    Routing    %10u %9u %9u %9u %9u %9u\n",
		p_PmErrStats->categoryMaximums.routingErrors,
		p_PmErrStats->ports[0].routingErrors,
		p_PmErrStats->ports[1].routingErrors,
		p_PmErrStats->ports[2].routingErrors,
		p_PmErrStats->ports[3].routingErrors,
		p_PmErrStats->ports[4].routingErrors );
	printf("    Utilization:   %3u.%1u%%  Discards: %3u.%1u%%\n",
		p_PmErrStats->categoryMaximums.utilizationPct10 / 10,
		p_PmErrStats->categoryMaximums.utilizationPct10 % 10,
		p_PmErrStats->categoryMaximums.discardsPct10 / 10,
		p_PmErrStats->categoryMaximums.discardsPct10 % 10);

	return (8);

}	// End of DisplayScreen_Err()

int DisplayScreen_Group(STL_PA_PM_GROUP_INFO_DATA * p_PmGroupInfo, int n_group)
{
	int ct_lines = 0;
	STL_PA_PM_UTIL_STATS * p_PmUtilStats;
	char *p_state_integ, *p_color_integ;
	char *p_state_congst, *p_color_congst;
	char *p_state_smacong, *p_color_smacong;
	char *p_state_bubble, *p_color_bubble;
	char *p_state_secure, *p_color_secure;
	char *p_state_routing, *p_color_routing;

#define EVAL_ERR_STAT2(statistic, state, color) \
	if (p_PmGroupInfo->internalCategoryStats.ports[4].statistic > 0 \
		|| p_PmGroupInfo->externalCategoryStats.ports[4].statistic > 0) \
	{	state = "OVER"; color = bf_color_red; } \
	else if (p_PmGroupInfo->internalCategoryStats.ports[3].statistic > 0 \
			|| p_PmGroupInfo->externalCategoryStats.ports[3].statistic > 0) \
	{	state = "Warn"; color = bf_color_yellow; } \
	else if (p_PmGroupInfo->internalCategoryStats.ports[2].statistic > 0 \
			|| p_PmGroupInfo->externalCategoryStats.ports[2].statistic > 0) \
	{	state = "Mod"; color = bf_color_cyan; } \
	else if (p_PmGroupInfo->internalCategoryStats.ports[1].statistic > 0 \
			|| p_PmGroupInfo->externalCategoryStats.ports[1].statistic > 0) \
	{	state = "Low"; color = bf_color_blue; } \
	else \
	{	state = "min"; color = bf_color_green; }

	// Summarize internal utilization
	if (p_PmGroupInfo->numInternalPorts)
	{
		p_PmUtilStats = &p_PmGroupInfo->internalUtilStats;
		printf( "%d %-10.10s%c Int %9u %9u %9u %9u %9u %9u\n",
			n_group - g_scroll_summary,
			p_PmGroupInfo->groupName, 
			strgetlastchar(p_PmGroupInfo->groupName, 10),
			p_PmUtilStats->avgMBps,
			p_PmUtilStats->minMBps != UINT_MAX ? p_PmUtilStats->minMBps : 0,
			p_PmUtilStats->maxMBps, p_PmUtilStats->avgKPps,
			p_PmUtilStats->minKPps != UINT_MAX ? p_PmUtilStats->minKPps : 0,
			p_PmUtilStats->maxKPps );

		ct_lines += 1;
	}

	// Summarize send and recv utilization
	if (p_PmGroupInfo->numExternalPorts)
	{
		p_PmUtilStats = &p_PmGroupInfo->sendUtilStats;

		if (p_PmGroupInfo->numInternalPorts)
			printf("             ");
		else
			printf("%d %-10.10s%c", n_group - g_scroll_summary,
						p_PmGroupInfo->groupName,
						strgetlastchar(p_PmGroupInfo->groupName, 10));

		printf( " Snd %9u %9u %9u %9u %9u %9u\n", p_PmUtilStats->avgMBps,
			p_PmUtilStats->minMBps != UINT_MAX ? p_PmUtilStats->minMBps : 0,
			p_PmUtilStats->maxMBps, p_PmUtilStats->avgKPps,
			p_PmUtilStats->minKPps != UINT_MAX ? p_PmUtilStats->minKPps : 0,
			p_PmUtilStats->maxKPps );
		p_PmUtilStats = &p_PmGroupInfo->recvUtilStats;
		printf( "              Rcv %9u %9u %9u %9u %9u %9u\n",
			p_PmUtilStats->avgMBps,
			p_PmUtilStats->minMBps != UINT_MAX ? p_PmUtilStats->minMBps : 0,
			p_PmUtilStats->maxMBps, p_PmUtilStats->avgKPps,
			p_PmUtilStats->minKPps != UINT_MAX ? p_PmUtilStats->minKPps : 0,
			p_PmUtilStats->maxKPps );

		ct_lines += 2;
	}
	if (p_PmGroupInfo->numInternalPorts == 0
		&& p_PmGroupInfo->numExternalPorts == 0)
	{
		printf( "%d %-10.10s%c No ports in group\n", n_group - g_scroll_summary,
						p_PmGroupInfo->groupName,
						strgetlastchar(p_PmGroupInfo->groupName, 10));
		ct_lines += 1;
	} else {
		// Summarize errors
		EVAL_ERR_STAT2(integrityErrors, p_state_integ, p_color_integ);
		EVAL_ERR_STAT2(congestion, p_state_congst, p_color_congst);
		EVAL_ERR_STAT2(smaCongestion, p_state_smacong, p_color_smacong);
		EVAL_ERR_STAT2(bubble, p_state_bubble, p_color_bubble);
		EVAL_ERR_STAT2(securityErrors, p_state_secure, p_color_secure);
		EVAL_ERR_STAT2(routingErrors, p_state_routing, p_color_routing);

		printf( "      %sInteg:%-4s%s %sCongst:%-4s%s %sSmaCong:%-4s%s %sBubble:%-4s%s %sSecure:%-4s%s %sRouting:%-4s%s\n",
			p_color_integ, p_state_integ, bf_color_off,
			p_color_congst, p_state_congst, bf_color_off,
			p_color_smacong, p_state_smacong, bf_color_off,
			p_color_bubble, p_state_bubble, bf_color_off,
			p_color_secure, p_state_secure, bf_color_off,
			p_color_routing, p_state_routing, bf_color_off );

		ct_lines += 1;
	}

	return (ct_lines);

}	// End of DisplayScreen_Group()

int ScreenLines_Group(STL_PA_PM_GROUP_INFO_DATA * p_PmGroupInfo, int n_group)
{
	int ct_lines = 0;
	if (p_PmGroupInfo->numInternalPorts)
		ct_lines += 1;	// Int
	if (p_PmGroupInfo->numExternalPorts)
		ct_lines += 2;	// Send and Rcv
	ct_lines += 1;	// error summary
	return (ct_lines);

}	// End of ScreenLines_Group()

int DisplayScreen_Util(STL_PA_PM_UTIL_STATS * p_PmUtilStats, char * p_title)
{
	printf( "%-4s  TotMBps  AvgMBps  MinMBps  MaxMBps      TotKPps  AvgKPps  MinKPps  MaxKPps\n",
		p_title );
	printf( "   %10" PRIu64 " %8u %8u %8u %12" PRIu64 " %8u %8u %8u\n",
		p_PmUtilStats->totalMBps, p_PmUtilStats->avgMBps,
		p_PmUtilStats->minMBps != UINT_MAX ? p_PmUtilStats->minMBps : 0,
		p_PmUtilStats->maxMBps, p_PmUtilStats->totalKPps, p_PmUtilStats->avgKPps,
		p_PmUtilStats->minKPps != UINT_MAX ? p_PmUtilStats->minKPps : 0,
		p_PmUtilStats->maxKPps );
	printf("     Buckt 0+%%   10+%%   20+%%   30+%%   40+%%   50+%%   60+%%   70+%%   80+%%   90+%%\n");
	printf("        %6u %6u %6u %6u %6u %6u %6u %6u %6u %6u\n",
		p_PmUtilStats->BWBuckets[0], p_PmUtilStats->BWBuckets[1],
		p_PmUtilStats->BWBuckets[2], p_PmUtilStats->BWBuckets[3],
		p_PmUtilStats->BWBuckets[4], p_PmUtilStats->BWBuckets[5],
		p_PmUtilStats->BWBuckets[6], p_PmUtilStats->BWBuckets[7],
		p_PmUtilStats->BWBuckets[8], p_PmUtilStats->BWBuckets[9] );
	printf("     NoResp %-.3s Ports: PMA: %6u  Topo: %6u\n", p_title,
		p_PmUtilStats->pmaNoRespPorts, p_PmUtilStats->topoIncompPorts);

	return (5);

}	// End of DisplayScreen_Util()

int DisplayScreen_VFGroup(STL_PA_VF_INFO_DATA* p_PmVfGrpInfo, int n_group)
{
	STL_PA_PM_UTIL_STATS *p_PmUtilStats = &p_PmVfGrpInfo->internalUtilStats;
	char *p_state_integ, *p_color_integ;
	char * p_state_congst, * p_color_congst;
	char * p_state_smacong, * p_color_smacong;
	char * p_state_bubble, * p_color_bubble;
	char * p_state_secure, * p_color_secure;
	char * p_state_routing, * p_color_routing;

	if (p_PmVfGrpInfo->numPorts == 0)
	{
		printf( "%d %-10.10s%c No ports in vf\n", n_group - g_scroll_summary,
				p_PmVfGrpInfo->vfName,
				strgetlastchar(p_PmVfGrpInfo->vfName, 10));
		return 1;
	}

#define VFEVAL_ERR_STAT(statistic, state, color) \
	if (p_PmVfGrpInfo->internalCategoryStats.ports[4].statistic > 0 ) \
	{	state = "OVER"; color = bf_color_red; } \
	else if (p_PmVfGrpInfo->internalCategoryStats.ports[3].statistic > 0 )\
	{	state = "Warn"; color = bf_color_yellow; } \
	else if (p_PmVfGrpInfo->internalCategoryStats.ports[2].statistic > 0 )\
	{	state = "Mod"; color = bf_color_cyan; } \
	else if (p_PmVfGrpInfo->internalCategoryStats.ports[1].statistic > 0 )\
	{	state = "Low"; color = bf_color_blue; } \
	else \
	{	state = "min"; color = bf_color_green; }

	printf("%d %-10.10s%c Int %9u %9u %9u %9u %9u %9u\n",
		n_group - g_scroll_vf_summary,
		p_PmVfGrpInfo->vfName,
		strgetlastchar(p_PmVfGrpInfo->vfName, 10),
		p_PmUtilStats->avgMBps,
		p_PmUtilStats->minMBps != UINT_MAX ? p_PmUtilStats->minMBps : 0,
		p_PmUtilStats->maxMBps, p_PmUtilStats->avgKPps,
		p_PmUtilStats->minKPps != UINT_MAX ? p_PmUtilStats->minKPps : 0,
		p_PmUtilStats->maxKPps );

	VFEVAL_ERR_STAT(integrityErrors, p_state_integ, p_color_integ);
	VFEVAL_ERR_STAT(congestion, p_state_congst, p_color_congst);
	VFEVAL_ERR_STAT(smaCongestion, p_state_smacong, p_color_smacong);
	VFEVAL_ERR_STAT(bubble, p_state_bubble, p_color_bubble);
	VFEVAL_ERR_STAT(securityErrors, p_state_secure, p_color_secure);
	VFEVAL_ERR_STAT(routingErrors, p_state_routing, p_color_routing);

	printf( "     %sInteg:%-4s%s %sCongst:%-4s%s %sSmaCong:%-4s%s %sBubble:%-4s%s %sSecure:%-4s%s %sRouting:%-4s%s\n",
		p_color_integ, p_state_integ, bf_color_off,
		p_color_congst, p_state_congst, bf_color_off,
		p_color_smacong, p_state_smacong, bf_color_off,
		p_color_bubble, p_state_bubble, bf_color_off,
		p_color_secure, p_state_secure, bf_color_off,
		p_color_routing, p_state_routing, bf_color_off );

	return 2;
}

// display the given help file and allow the user to scroll through it
// this is purposely kept simple since all our help files are less than
// 1KB and less than 100 lines (most are only 1-2 screens long).
// For security reasons, we don't want to callout to an external tool
// we expect the help text files to have newlines to limit lines to 80 columns
void DisplayScreen_Help(const char* helpfile)
{
	char help_text[MAX_HELP_CHARS];
	char *help_line[MAX_HELP_LINES];	// pointers into help_text
	int fd;
	int chars;
	int lines;
	int i;
	char *p;
	int cur_line;

	fd = open(helpfile, O_RDONLY);
	if (fd < 0)
	{
		printf("Unable to open help file: %s: %s\n", helpfile, strerror(errno));
		printf("Press return to continue:\n");
        fgetc(stdin);
		return;
	}
	// if file is too big silently ignore extra
	chars = read(fd, help_text, MAX_HELP_CHARS-1);
	if (chars < 0)
	{
		printf("Unable to read help file: %s: %s\n", helpfile, strerror(errno));
		printf("Press return continue:\n");
        fgetc(stdin);
		close(fd);
		return;
	}
	if (! chars)
	{
		printf("Help File empty: %s\n", helpfile);
		printf("Press return to continue:\n");
        fgetc(stdin);
		close(fd);
		return;
	}
	help_text[chars] = '\0';	// null terminate
	if (close(fd) < 0)
	{
		printf("Unable to close help file: %s: %s\n", helpfile, strerror(errno));
		printf("Press return to continue:\n");
        fgetc(stdin);
		return;
	}
	// figure out where each line starts
	// if we reach MAX_HELP_LINES, ignore the extra
	for (lines=0, p=help_text; *p && lines< MAX_HELP_LINES;)
	{
		help_line[lines++] = p;
		// find start of next line
		p = strchr(p, '\n');
		if (! p)	// file ended last line without a \n
			break;
		// replace \n with \0 so we can printf single lines
		*p++ = '\0';
	}

	cur_line = 1;
	while (TRUE)
	{
		// first line is a title of help page
		printf("%s" NAME_PROG ": %s: Lines %3d-%3d of %3d\n", bf_clear_screen,
			help_line[0], cur_line, MIN(cur_line+20, lines-1), lines-1);
		for (i=0; i<21; i++)
		{
			if (cur_line+i < lines)
				printf("%s\n", help_line[cur_line+i]);
			else
				printf("\n");
		}
		// this command line is a little more verbose since it may be the
		// first time user is reading help and learning how commands work
		// scroll commands are nop on single screen help files,so omit in prompt
		if (lines > 22)
			printf("Press Command Key: u=up (done help) s=scroll fwd S=scroll back <sp>=next line:\n");
		else
			printf("Press Command Key: u=up (done help):\n");
		switch (fgetc(stdin))
		{
			case (int)(unsigned char)'u':
			case (int)(unsigned char)'U':
			case EOF:
				return;
			case (int)(unsigned char)'s':	// scroll forward to next screen
				if (cur_line+21 < lines)
					cur_line+=21;
				break;
			case (int)(unsigned char)'S':	// scross back to prev screen
				if (cur_line <= 22)
					cur_line = 1;
				else
					cur_line -= 21;
				break;
			case (int)(unsigned char)' ':	// forward 1 line
				if (cur_line+21 < lines)	// no use scrolling on last screen
					cur_line++;
				break;
			default:
				// ignore input, redisplay at cur_line
				break;
		}
	}	// End of while (TRUE)
}

void DisplayScreen(void)
{
	int ix;
	int ct_lines;
	time_t time_now = time(NULL);
	char *p_select;
	char bf_screen[81];

	// Display help screens if requested
	if (fb_help)
	{
		if (tb_menu[n_level_menu] == SCREEN_SUMMARY || 
			tb_menu[n_level_menu] == SCREEN_VF_SUMMARY)
			DisplayScreen_Help(PATH_HELP "opatop_summary.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_PM_CONFIG)
			DisplayScreen_Help(PATH_HELP "opatop_pm_config.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_IMAGE_INFO)
			DisplayScreen_Help(PATH_HELP "opatop_img_config.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_GROUP_INFO_SELECT)
			DisplayScreen_Help(PATH_HELP "opatop_group_info_sel.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_GROUP_BW_STATS)
			DisplayScreen_Help(PATH_HELP "opatop_group_bw.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_GROUP_CTG_STATS ||
				 tb_menu[n_level_menu] == SCREEN_VF_CTG_STATS)
			DisplayScreen_Help(PATH_HELP "opatop_group_ctg.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_GROUP_CONFIG)
			DisplayScreen_Help(PATH_HELP "opatop_group_config.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_GROUP_FOCUS ||
				 tb_menu[n_level_menu] == SCREEN_VF_FOCUS)
			DisplayScreen_Help(PATH_HELP "opatop_group_focus.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_PORT_STATS)
			DisplayScreen_Help(PATH_HELP "opatop_port_stats.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_VF_INFO_SELECT)
			DisplayScreen_Help(PATH_HELP "opatop_vf_info_sel.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_VF_BW_STATS)
			DisplayScreen_Help(PATH_HELP "opatop_vf_bw.hlp");
		else if (tb_menu[n_level_menu] == SCREEN_VF_CONFIG)
			DisplayScreen_Help(PATH_HELP "opatop_vf_config.hlp");

		printf("Waiting for screen refresh...\n");
		fb_help = FALSE;
		return;
	}

	// Query for congestion data based on screen being displayed;
	// Note that queries are done only for the data needed for a screen;
	// Also note that when SCREEN_PORT_STATS has SCREEN_GROUP_FOCUS as its
	//   parent screen, GroupFocus data must not change
	g_imageIdResp.imageNumber = PACLIENT_IMAGE_CURRENT;
	g_imageIdResp.imageOffset = 0;

	if (!fb_valid_pa_client)
	{
		int pa_service_state = OMGT_SERVICE_STATE_UNKNOWN;
		struct omgt_params params = { .debug_file = (g_verbose & VERBOSE_PACLIENT ? stderr : NULL) };
		if (!omgt_open_port_by_num(&g_portHandle, hfi, port, &params))
		{
			if (!omgt_port_get_pa_service_state(g_portHandle, &pa_service_state, OMGT_REFRESH_SERVICE_BAD_STATE))
			{
				fb_valid_pa_client = (pa_service_state == OMGT_SERVICE_STATE_OPERATIONAL);
			}
		}
	}

	if (!fb_valid_pa_cpi) {
		if (omgt_pa_get_classportinfo(g_portHandle, &g_PaClassPortInfo) == FSUCCESS) {
			fb_valid_pa_cpi = TRUE;
			MemoryDeallocate(g_PaClassPortInfo);
		}
	}

	if (omgt_pa_get_image_info(g_portHandle, g_imageIdQuery, &g_PmImageInfo) == FSUCCESS)
		fb_valid_image_info = TRUE;
	else
	{
		fb_valid_pa_client = FALSE;
		fb_valid_image_info = FALSE;
	}

	if ( (tb_menu[n_level_menu] == SCREEN_PM_CONFIG) ||
			!fb_valid_pm_config )
	{
		if (omgt_pa_get_pm_config(g_portHandle, &g_PmConfig) == FSUCCESS)
			fb_valid_pm_config = TRUE;
		else
			fb_valid_pm_config = FALSE;
	}

	if ( (tb_menu[n_level_menu] == SCREEN_SUMMARY) ||
			(tb_menu[n_level_menu] == SCREEN_PM_CONFIG) ||
			!fb_valid_group_list )
	{
		omgt_pa_release_group_list(&g_PmGroupList.groupList);
		if (omgt_pa_get_group_list(g_portHandle, &g_PmGroupList.numGroups,
									     &g_PmGroupList.groupList) == FSUCCESS)
			fb_valid_group_list = TRUE;
		else
			fb_valid_group_list = FALSE;
	}

	if (tb_menu[n_level_menu] == SCREEN_VF_SUMMARY ||
		!fb_valid_group_list ) {

		omgt_pa_release_vf_list(&g_PmVFList.vfList);
		if (omgt_pa_get_vf_list(g_portHandle, &g_PmVFList.numVFs,
									     &g_PmVFList.vfList) == FSUCCESS)
			fb_valid_VF_list = TRUE;
		else
			fb_valid_VF_list = FALSE;	}

	if ( (tb_menu[n_level_menu] == SCREEN_GROUP_INFO_SELECT) ||
			(tb_menu[n_level_menu] == SCREEN_GROUP_BW_STATS) ||
			(tb_menu[n_level_menu] == SCREEN_GROUP_CTG_STATS) ||
			(tb_menu[n_level_menu] == SCREEN_GROUP_FOCUS) )
	{
		if ( fb_valid_group_list && ( omgt_pa_get_group_info( g_portHandle, g_imageIdQuery,
				g_PmGroupList.groupList[g_group].groupName, &g_imageIdResp,
				&g_PmGroupInfo) == FSUCCESS ) )
			fb_valid_group_info = TRUE;
		else
			fb_valid_group_info = FALSE;
	}

	if (tb_menu[n_level_menu] == SCREEN_VF_INFO_SELECT || 
		tb_menu[n_level_menu] == SCREEN_VF_BW_STATS ||
		tb_menu[n_level_menu] == SCREEN_VF_CTG_STATS ||
		tb_menu[n_level_menu] == SCREEN_VF_FOCUS)  {
		if (fb_valid_VF_list) {
			OMGT_QUERY query = {0};
			PQUERY_RESULT_VALUES pQueryResults = NULL;
			
			query.InputType = InputTypeNoInput;
			query.OutputType = OutputTypePaTableRecord;

			if (iba_pa_multi_mad_vf_info_response_query(g_portHandle, &query, g_PmVFList.vfList[g_vf].vfName,
				&pQueryResults, &g_imageIdResp) == FSUCCESS) {

				g_PmVFInfo = ((STL_PA_VF_INFO_RESULTS*)pQueryResults->QueryResult)->VFInfoRecords[0];
				fb_valid_VF_info = TRUE;
			} else fb_valid_VF_info = FALSE;	
		} else fb_valid_VF_info = FALSE;
	}

	// Do default query to get g_imageIdResp
	if ((g_group < 0) && fb_valid_group_list)
		omgt_pa_get_group_info(g_portHandle, g_imageIdQuery, g_PmGroupList.groupList[0].groupName,
			&g_imageIdResp, &g_PmGroupInfo);

	if (tb_menu[n_level_menu] == SCREEN_GROUP_CONFIG)
	{
		omgt_pa_release_group_config(&pg_PmGroupConfig.portList);
		if ( fb_valid_group_list && ( omgt_pa_get_group_config(g_portHandle, g_imageIdQuery,
				g_PmGroupList.groupList[g_group].groupName, &g_imageIdResp, &pg_PmGroupConfig.numPorts,
				&pg_PmGroupConfig.portList ) == FSUCCESS ) )
		{
			fb_valid_group_config = TRUE;
			strcpy(pg_PmGroupConfig.groupName, g_PmGroupList.groupList[g_group].groupName);
		}
		else
			fb_valid_group_config = FALSE;
	}

	if (tb_menu[n_level_menu] == SCREEN_VF_CONFIG) {
		omgt_pa_release_vf_config(&g_pPmVFConfig.portList);
		if (fb_valid_VF_list && (omgt_pa_get_vf_config(g_portHandle, g_imageIdQuery,
				g_PmVFList.vfList[g_vf].vfName, &g_imageIdResp, &g_pPmVFConfig.numPorts, &g_pPmVFConfig.portList) == FSUCCESS))
		{
			fb_valid_VF_config = TRUE;
			strcpy(g_pPmVFConfig.vfName, g_PmVFList.vfList[g_vf].vfName);
		}
		else fb_valid_VF_config = FALSE;
	}

	if (tb_menu[n_level_menu] == SCREEN_GROUP_FOCUS)
	{
		omgt_pa_release_group_focus(&pg_PmGroupFocus.portList);
		if ( fb_valid_group_list && ( omgt_pa_get_group_focus( g_portHandle, g_imageIdQuery,
                        g_PmGroupList.groupList[g_group].groupName, g_select, g_start, g_range,
                        &g_imageIdResp, &pg_PmGroupFocus.numPorts, &pg_PmGroupFocus.portList ) == FSUCCESS ) )
		{
			fb_valid_group_focus = TRUE;
			strcpy(pg_PmGroupFocus.groupName, g_PmGroupList.groupList[g_group].groupName);
		}
		else
			fb_valid_group_focus = FALSE;

		pg_PmGroupFocus.select = g_select;
		pg_PmGroupFocus.start = g_start;
		pg_PmGroupFocus.range = g_range;
	}

	if (tb_menu[n_level_menu] == SCREEN_VF_FOCUS) {

		omgt_pa_release_vf_focus(&g_pPmVFFocus.portList);
		if (fb_valid_VF_list && (omgt_pa_get_vf_focus(g_portHandle, g_imageIdQuery,
                        g_PmVFList.vfList[g_vf].vfName, g_select, g_start, g_range,
                        &g_imageIdResp, &g_pPmVFFocus.numPorts, &g_pPmVFFocus.portList) == FSUCCESS) )
		{
			fb_valid_VF_focus = TRUE;
			strcpy(g_pPmVFFocus.vfName, g_PmVFList.vfList[g_vf].vfName);
		}
		else
			fb_valid_VF_focus = FALSE;

		g_pPmVFFocus.select = g_select;
		g_pPmVFFocus.start = g_start;
		g_pPmVFFocus.range = g_range;
	}

	if (tb_menu[n_level_menu] == SCREEN_PORT_STATS)
	{
		fb_valid_port_stats = FALSE;

		{
			if ( omgt_pa_get_port_stats2(g_portHandle, g_imageIdQuery, g_portlid, g_portnum,
					&g_imageIdResp, &g_portCounters, &g_portCounterFlags, 1, 0) == FSUCCESS) {
				fb_valid_port_stats = TRUE;
			}
		}
		if (fb_valid_port_stats && (g_PmConfig.pmFlags & STL_PM_PROCESS_VL_COUNTERS) &&
			omgt_pa_get_vf_port_stats2(g_portHandle, g_imageIdQuery, "HIDDEN_VL15_VF",
				g_portlid, g_portnum, &g_imageIdResp, &g_hiddenVfPortCounters,
				&g_hiddenVfPortCounterFlags, 1, 0 ) == FSUCCESS ) {
			fb_valid_vf_port_stats_hidden = TRUE;
		} else {
			fb_valid_vf_port_stats_hidden = FALSE;
		}
	}

	// Display header lines
	if (fb_valid_image_info)
	{
		if (g_PmImageInfo.imageInterval) {
			// Display ImageInterval if present
			snprintf(bf_screen, sizeof(bf_screen), " %us @ %s", g_PmImageInfo.imageInterval,
				ctime((time_t *)&g_PmImageInfo.sweepStart));
		} else {
			snprintf(bf_screen, sizeof(bf_screen), " %s", ctime((time_t *)&g_PmImageInfo.sweepStart));
		}
		// replace '\n' character with '\0'
		bf_screen[strlen(bf_screen) - 1] = 0;
	
		if (g_imageIdQuery.imageNumber == PACLIENT_IMAGE_CURRENT)
		{
			sprintf(&bf_screen[strlen(bf_screen)], ", Live");
	
			if (g_debug & DEBUG_IMGID)
				sprintf( &bf_screen[strlen(bf_screen)], " (0x%016" PRIX64 ",%d)",
					g_imageIdQuery.imageNumber, g_imageIdQuery.imageOffset );
	
			strcat(bf_screen, "\n");
		}
	
		else if (g_imageIdFreeze.imageNumber != PACLIENT_IMAGE_CURRENT)
		{
			if (g_verbose & VERBOSE_SCREEN)
				sprintf(&bf_screen[strlen(bf_screen)], ", %d", g_offset);
			else
				sprintf(&bf_screen[strlen(bf_screen)], ", Hist");
	
			if (g_debug & DEBUG_IMGID)
				sprintf( &bf_screen[strlen(bf_screen)], " (0x%016" PRIX64 ",%d)",
					g_imageIdFreeze.imageNumber, g_imageIdFreeze.imageOffset );
	
			sprintf(&bf_screen[strlen(bf_screen)], " Now:%s", ctime(&time_now));
		}
	
		else if (g_imageIdBookmark.imageNumber != PACLIENT_IMAGE_CURRENT)
		{
			sprintf(&bf_screen[strlen(bf_screen)], ", Bkmk");
	
			if (g_debug & DEBUG_IMGID)
				sprintf( &bf_screen[strlen(bf_screen)], " (0x%016" PRIX64 ",%d)",
					g_imageIdBookmark.imageNumber, g_imageIdBookmark.imageOffset );
	
			sprintf(&bf_screen[strlen(bf_screen)], " Now:%s", ctime(&time_now));
		}
	
		printf("%s" NAME_PROG ": Img:%s", bf_clear_screen, bf_screen);
	}

	else
		printf( "%s" NAME_PROG ": IMAGE SUMMARY NOT AVAILABLE\n",
			bf_clear_screen );

	ct_lines = MAX_LINES_PER_SCREEN - 1 - 2;	// -1 for head, 2 for menu
	if (g_debug & DEBUG_GROUP)
		ct_lines += 1;
	if (g_debug & DEBUG_IMGID_2)
		ct_lines += 4;

	if (g_debug & DEBUG_IMGID_2)
	{    
		printf( "         ImgRsp:0x%" PRIX64 ",%d\n", g_imageIdResp.imageNumber,
			g_imageIdResp.imageOffset );
		printf( "         ImgFrz:0x%" PRIX64 ",%d\n",
			g_imageIdFreeze.imageNumber, g_imageIdFreeze.imageOffset );
		printf( "         ImgBkm:0x%" PRIX64 ",%d\n",
			g_imageIdBookmark.imageNumber, g_imageIdBookmark.imageOffset );
		printf( "         ImgTmp:0x%" PRIX64 ",%d\n", g_imageIdTemp.imageNumber,
			g_imageIdTemp.imageOffset );
		ct_lines -= 4;
	}

	if (g_debug & DEBUG_GROUP)
	{
		printf( "Interval:%u  Grp:%d  PortIx:%d  Offset:%d\n", ct_interval,
			g_group, g_ix_port, g_offset );
		ct_lines -= 1;
	}

	// Display selected screen
	switch (tb_menu[n_level_menu])
	{
	case SCREEN_SUMMARY:
		if (fb_valid_image_info)
		{
			char* status_color = bf_color_off;
			char* status_message = "";

			printf( "Summary:  SW: %5u Ports: SW: %5u  HFI: %5u       Link: %5u\n",
				g_PmImageInfo.numSwitchNodes, g_PmImageInfo.numSwitchPorts,
				g_PmImageInfo.numHFIPorts, g_PmImageInfo.numLinks);
			printf( "          SM: %5u Node NRsp: %5u Skip: %5u Port NRsp: %5u Skip: %5u\n",
				g_PmImageInfo.numSMs, g_PmImageInfo.numNoRespNodes,
				g_PmImageInfo.numSkippedNodes, g_PmImageInfo.numNoRespPorts,
				g_PmImageInfo.numSkippedPorts );
			if (g_PmImageInfo.numUnexpectedClearPorts) {
				status_color = bf_color_red;
				status_message = "Unexpected Clear";
			}
			printf("%s%-17s%s   AvgMBps   MinMBps   MaxMBps   AvgKPps   MinKPps   MaxKPps\n",
				status_color, status_message, bf_color_off);
			ct_lines -= 3;

			if ( fb_valid_group_list && g_PmGroupList.numGroups )
			{
				// if PM groups we were showing are removed, go to last
				if (g_scroll_summary >= g_PmGroupList.numGroups)
					g_scroll_summary = g_PmGroupList.numGroups-1;
				// compute how far we can scroll back
				int ct_group_lines = MAX_SUMMARY_GROUP_LINES_PER_SCREEN;
				g_scroll_summary_backward = 0;	// default if at start
				for (ix = g_scroll_summary-1; ix >= 0; ix--)
				{
					if ( omgt_pa_get_group_info(g_portHandle, g_imageIdQuery,
							g_PmGroupList.groupList[ix].groupName, &g_imageIdResp,
							&g_PmGroupInfo) == FSUCCESS )
						ct_group_lines -= ScreenLines_Group(&g_PmGroupInfo, ix);
						if (ct_group_lines >= 0)
							g_scroll_summary_backward = ix;	// can go here
						else
							break;
				}
				// display what fits and save scroll forward point
				ct_group_lines = MAX_SUMMARY_GROUP_LINES_PER_SCREEN;
				g_scroll_summary_forward = g_scroll_summary;	// initialize
				for (ix = g_scroll_summary; ix < g_PmGroupList.numGroups; ix++)
				{
					if ( omgt_pa_get_group_info(g_portHandle, g_imageIdQuery,
							g_PmGroupList.groupList[ix].groupName, &g_imageIdResp,
							&g_PmGroupInfo) == FSUCCESS )
					{
						// only output if it will fit
						ct_group_lines -= ScreenLines_Group(&g_PmGroupInfo, ix);
						g_scroll_summary_forward = ix;	// go here if scroll fwd
						if (ct_group_lines < 0)
							break;
						ct_lines -= DisplayScreen_Group(&g_PmGroupInfo, ix);
					}
				}
			}

			else
			{
				printf("Summary: GROUP LIST NOT AVAILABLE\n");
				ct_lines -= 1;
			}

			// put SM at bottom of screen (6 lines), leave 1 line for bf_error
			// and 1 for extra menu line
			for ( ; ct_lines > 8; ct_lines--)
				printf("\n");

			ct_lines -= DisplayScreen_SMs();
		}

		else
		{
			printf("Summary: IMAGE INFO NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	case SCREEN_VF_SUMMARY:
		if (fb_valid_VF_list) {
			char* status_color = bf_color_off;
			char* status_message = "";

			printf( "Summary:  SW: %5u Ports: SW: %5u  HFI: %5u       Link: %5u\n",
				g_PmImageInfo.numSwitchNodes, g_PmImageInfo.numSwitchPorts,
				g_PmImageInfo.numHFIPorts, g_PmImageInfo.numLinks);
			printf( "          SM: %5u Node NRsp: %5u Skip: %5u Port NRsp: %5u Skip: %5u\n",
				g_PmImageInfo.numSMs, g_PmImageInfo.numNoRespNodes,
				g_PmImageInfo.numSkippedNodes, g_PmImageInfo.numNoRespPorts,
				g_PmImageInfo.numSkippedPorts );
			if (g_PmImageInfo.numUnexpectedClearPorts) {
				status_color = bf_color_red;
				status_message = "Unexpected Clear";
			}
			printf("%s%-17s%s   AvgMBps   MinMBps   MaxMBps   AvgKPps   MinKPps   MaxKPps\n",
				status_color, status_message, bf_color_off);
			ct_lines -= 3;
			
			if (g_PmVFList.numVFs < 1) {
				printf("Summary: VF LIST NOT AVAILABLE\n");
				ct_lines -= 1;
			} else {
				// if VFs we were showing are removed, go to last
				if (g_scroll_vf_summary >= g_PmVFList.numVFs)
					g_scroll_vf_summary = g_PmVFList.numVFs-1;
				for (ix = g_scroll_vf_summary; ix < MIN(g_scroll_vf_summary + MAX_VF_SUMMARY_PER_SCREEN, g_PmVFList.numVFs) ; ++ix) {
					OMGT_QUERY query = {0};
					PQUERY_RESULT_VALUES pQueryResults = NULL;
				
					query.InputType = InputTypeNoInput;
					query.OutputType = OutputTypePaTableRecord;

					if (iba_pa_multi_mad_vf_info_response_query(g_portHandle, &query, g_PmVFList.vfList[ix].vfName,
							&pQueryResults, &g_imageIdResp) == FSUCCESS) {
						STL_PA_VF_INFO_DATA *p = &((STL_PA_VF_INFO_RESULTS*)pQueryResults->QueryResult)->VFInfoRecords[0];
						ct_lines -= DisplayScreen_VFGroup(p, ix);
					}
				}
			}

			// put SM at bottom of screen (6 lines), leave 1 line for bf_error
			// and 1 for extra menu line
			for ( ; ct_lines > 8; ct_lines--)
				printf("\n");

			ct_lines -= DisplayScreen_SMs();
		}

		break;

	case SCREEN_PM_CONFIG:
		if (fb_valid_pm_config)
		{
			char buf[80];

			printf( "PM Config:\n" );
			printf( " Sweep Interval: %u sec  PM Flags(0x%X):\n",
				g_PmConfig.sweepInterval, g_PmConfig.pmFlags);
			StlFormatPmFlags(buf, g_PmConfig.pmFlags);
			printf( "   %s\n", buf);
			StlFormatPmFlags2(buf, g_PmConfig.pmFlags);
			printf( "   %s\n", buf);
			printf( " Max Clients:  %7u\n", g_PmConfig.maxClients);
			printf( " Total Images: %7u   Freeze Images: %-7u   Freeze Lease: %u seconds\n",
				g_PmConfig.sizeHistory, g_PmConfig.sizeFreeze, g_PmConfig.lease );
			printf( " Ctg Thresholds: Integrity:      %4u  Congestion:    %4u\n",
				g_PmConfig.categoryThresholds.integrityErrors,
				g_PmConfig.categoryThresholds.congestion );
			printf( "                 SmaCongest:     %4u  Bubble:        %4u\n",
				g_PmConfig.categoryThresholds.smaCongestion,
				g_PmConfig.categoryThresholds.bubble );
			printf( "                 Security:       %4u  Routing:       %4u\n",
				g_PmConfig.categoryThresholds.securityErrors,
				g_PmConfig.categoryThresholds.routingErrors );
			printf( " Integrity Wts:  Link Qual:      %4u  Uncorrectable: %4u\n",
				g_PmConfig.integrityWeights.LinkQualityIndicator,
				g_PmConfig.integrityWeights.UncorrectableErrors );
			printf( "                 Link Downed:    %4u  Rcv Errors:    %4u\n",
				g_PmConfig.integrityWeights.LinkDowned,
				g_PmConfig.integrityWeights.PortRcvErrors );
			printf( "                 Excs Bfr Ovrn:  %4u  FM Config Err: %4u\n",
				g_PmConfig.integrityWeights.ExcessiveBufferOverruns,
				g_PmConfig.integrityWeights.FMConfigErrors );
			printf( "                 Link Err Reco:  %4u  Loc Link Integ:%4u\n",
				g_PmConfig.integrityWeights.PortRcvErrors,
				g_PmConfig.integrityWeights.LocalLinkIntegrityErrors);
			printf( "                 Lnk Wdth Dngd:  %4u   \n",
				g_PmConfig.integrityWeights.LinkWidthDowngrade );
			printf( " Congest Wts:    Cong Discards:  %4u  Rcv FECN:      %4u\n",
				g_PmConfig.congestionWeights.SwPortCongestion,
				g_PmConfig.congestionWeights.PortRcvFECN);
			printf( "                 Rcv BECN:       %4u  Mark FECN:     %4u\n",
				g_PmConfig.congestionWeights.PortRcvBECN,
				g_PmConfig.congestionWeights.PortMarkFECN);
			printf( "                 Xmit Time Cong: %4u  Xmit Wait:     %4u  \n",
				g_PmConfig.congestionWeights.PortXmitTimeCong,
				g_PmConfig.congestionWeights.PortXmitWait);
			printf( " PM Memory Size: %"PRIu64" MB (%" PRIu64 " bytes)\n",
				g_PmConfig.memoryFootprint/(1000*1000),
				g_PmConfig.memoryFootprint );
			printf( " PMA MADs: MaxAttempts:   %3u MinRespTimeout: %3u RespTimeout: %4u\n",
				g_PmConfig.maxAttempts, g_PmConfig.minRespTimeout, g_PmConfig.respTimeout );
			printf( " Sweep: MaxParallelNodes: %3u PmaBatchSize:   %3u ErrorClear:  %4u\n",
				g_PmConfig.maxParallelNodes, g_PmConfig.pmaBatchSize,
				g_PmConfig.errorClear);
			ct_lines -= 20;
		}
		else
		{
		   	printf("PM Config: PM CONFIG NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		if (g_verbose & VERBOSE_SCREEN_2)
		{
			printf("\n" NAME_PROG " Config:\n");
			printf(" Interval: %7u seconds   PortGUID: 0x%016" PRIX64 "\n",
				g_interval, g_portGuid );
			printf(" Verbose: 0x%04X", g_verbose);

			if (g_debug)
				printf("              Debug:0x%04X\n", g_debug);
			else
				printf("\n");

			printf(" NumGroups: %5u\n", g_PmGroupList.numGroups);
			printf(" Quiet: %5s\n", g_quiet ? "TRUE" : "FALSE");
			ct_lines -= 6;
		}

		break;

	case SCREEN_IMAGE_INFO:
		if (fb_valid_image_info)
		{
			printf("Image Info:\n");
			ct_lines -= 1;

			if (g_debug & DEBUG_IMGID)
			{
				printf( " Img Number: 0x%" PRIX64 ", Offset: %d\n",
					g_PmImageInfo.imageId.imageNumber,
					g_PmImageInfo.imageId.imageOffset );
				ct_lines -= 1;
			}

			printf(" Sweep Start: %s", ctime((time_t *)&g_PmImageInfo.sweepStart));

			if (g_verbose & VERBOSE_SCREEN)
				printf( " Sweep Duration: %8.6f Seconds\n",
					(float)g_PmImageInfo.sweepDuration / 1000000. );
			else
				printf( " Sweep Duration: %5.3f Seconds\n",
					(float)g_PmImageInfo.sweepDuration / 1000000. );

			if (g_PmImageInfo.imageInterval) {
				printf(" Image Interval: %u Seconds\n\n", g_PmImageInfo.imageInterval);
			} else {
				printf("\n\n");
			}

			printf( " Num SW-Ports: %7u  HFI-Ports: %7u\n",
				g_PmImageInfo.numSwitchPorts, g_PmImageInfo.numHFIPorts);
			printf( " Num SWs:      %7u  Num Links: %7u  Num SMs:   %7u\n\n",
				g_PmImageInfo.numSwitchNodes, g_PmImageInfo.numLinks,
				g_PmImageInfo.numSMs );
			printf( " Num NRsp Nodes: %7u  Ports: %7u  Unexpected Clear Ports: %u\n",
				g_PmImageInfo.numNoRespNodes, g_PmImageInfo.numNoRespPorts,
				g_PmImageInfo.numUnexpectedClearPorts);
			printf( " Num Skip Nodes: %7u  Ports: %7u\n\n",
				g_PmImageInfo.numSkippedNodes, g_PmImageInfo.numSkippedPorts );
			ct_lines -= 10;

			ct_lines -= DisplayScreen_SMs();
		}

		else
		{
		   	printf("Image Info: IMAGE INFO NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	case SCREEN_GROUP_INFO_SELECT:
		if (fb_valid_group_info)
		{
			printf( "Group Info Sel: %s\n", g_PmGroupInfo.groupName);
			if (g_PmGroupInfo.minInternalRate != IB_STATIC_RATE_DONTCARE
				|| g_PmGroupInfo.maxInternalRate != IB_STATIC_RATE_DONTCARE)
				printf( "Int NumPorts: %u  Rate Min: %4s  Max: %4s\n",
			   		g_PmGroupInfo.numInternalPorts,
					StlStaticRateToText(g_PmGroupInfo.minInternalRate),
					StlStaticRateToText(g_PmGroupInfo.maxInternalRate));
			else
				printf( "Int NumPorts: %u\n", g_PmGroupInfo.numInternalPorts);

			if (g_PmGroupInfo.minExternalRate != IB_STATIC_RATE_DONTCARE
				|| g_PmGroupInfo.maxExternalRate != IB_STATIC_RATE_DONTCARE)
				printf( "Ext NumPorts: %u  Rate Min: %4s  Max: %4s\n",
					g_PmGroupInfo.numExternalPorts,
					StlStaticRateToText(g_PmGroupInfo.minExternalRate),
					StlStaticRateToText(g_PmGroupInfo.maxExternalRate));
			else
				printf( "Ext NumPorts: %u\n", g_PmGroupInfo.numExternalPorts);
			printf("  Group Performance (P)\n");
			printf("  Group Statistics (S)\n");
			printf("  Group Config (C)\n");
			ct_lines -= 6;
		}

		else
		{
		   	printf("Group Info Select: GROUP INFO NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;
	case SCREEN_VF_INFO_SELECT:
		if (fb_valid_VF_info) {
			printf("VF Info Select: %s\n", g_PmVFInfo.vfName);
			if (g_PmVFInfo.minInternalRate != IB_STATIC_RATE_DONTCARE
				|| g_PmVFInfo.maxInternalRate != IB_STATIC_RATE_DONTCARE)
				printf( "NumPorts: %u  Rate Min: %4s  Max: %4s\n",
			   		g_PmVFInfo.numPorts,
					StlStaticRateToText(g_PmVFInfo.minInternalRate),
					StlStaticRateToText(g_PmVFInfo.maxInternalRate));
			else
				printf( "NumPorts: %u\n", g_PmVFInfo.numPorts);

			printf("  Group Performance (P)\n");
			printf("  Group Statistics (S)\n");
			printf("  Group Config (C)\n");
			ct_lines -= 5;
		} else {
			printf("VF Info Select: VF INFO NOT AVAILABLE\n");
			ct_lines -= 1;
		}
		break;

	case SCREEN_GROUP_BW_STATS:
		if (fb_valid_group_info)
		{
			if (g_select == STL_PA_SELECT_UTIL_PKTS_HIGH)
				p_select = "UtlPkt-Hi";
			else if (g_select == STL_PA_SELECT_UTIL_LOW)
				p_select = "Util-Low";
// Future enhancement
#if 0
			else if (g_select == PACLIENT_SEL_ALL)
				p_select = "ALL";
#endif
			else
			{
				g_select = STL_PA_SELECT_UTIL_HIGH;
				p_select = "Util-High";
			}

			printf( "Group BW Util: %-.44s%s  Criteria: %s",
				g_PmGroupInfo.groupName,
				strgetlaststr(g_PmGroupInfo.groupName, 44),
 				p_select );
			if (g_expr_funct)
				printf("  Start: %d", g_start);
			printf("  Number: %d\n", g_range);
			ct_lines -= 1;

			if (g_PmGroupInfo.numInternalPorts)
				ct_lines -= DisplayScreen_Util(&g_PmGroupInfo.internalUtilStats, "Int:");

			if (g_PmGroupInfo.numExternalPorts)
			{
				ct_lines -= DisplayScreen_Util(&g_PmGroupInfo.sendUtilStats, "Snd:");
				ct_lines -= 1;
				ct_lines -= DisplayScreen_Util(&g_PmGroupInfo.recvUtilStats, "Rcv:");
			}


			if (g_PmGroupInfo.numInternalPorts == 0
				&& g_PmGroupInfo.numExternalPorts == 0)
			{
				printf("\n");
				printf( "No ports in group\n");
				ct_lines -= 2;
			} else {
				printf("\n");
			    printf("                      Max       0+%%      25+%%      50+%%      75+%%     100+%%\n");
			    ct_lines-=2;

			    if (g_PmGroupInfo.numInternalPorts) {
			      printf( "Int Congestion %10u %9u %9u %9u %9u %9u\n",
				    g_PmGroupInfo.internalCategoryStats.categoryMaximums.congestion,
				    g_PmGroupInfo.internalCategoryStats.ports[0].congestion,
				    g_PmGroupInfo.internalCategoryStats.ports[1].congestion,
				    g_PmGroupInfo.internalCategoryStats.ports[2].congestion,
				    g_PmGroupInfo.internalCategoryStats.ports[3].congestion,
				    g_PmGroupInfo.internalCategoryStats.ports[4].congestion );
			      ct_lines-=1;
			    }

			    if (g_PmGroupInfo.numExternalPorts) {
				  printf( "Ext Congestion %10u %9u %9u %9u %9u %9u\n",
				    g_PmGroupInfo.externalCategoryStats.categoryMaximums.congestion,
				    g_PmGroupInfo.externalCategoryStats.ports[0].congestion,
				    g_PmGroupInfo.externalCategoryStats.ports[1].congestion,
				    g_PmGroupInfo.externalCategoryStats.ports[2].congestion,
				    g_PmGroupInfo.externalCategoryStats.ports[3].congestion,
				    g_PmGroupInfo.externalCategoryStats.ports[4].congestion );
				  ct_lines -=1;
			    }
			}
		}

		else
		{
			printf("Group BW Util: GROUP INFO NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;
	case SCREEN_VF_BW_STATS:
		if (fb_valid_VF_info) {
			if (g_select == STL_PA_SELECT_UTIL_PKTS_HIGH)
				p_select = "UtlPkt-Hi";
			else if (g_select == STL_PA_SELECT_UTIL_LOW)
				p_select = "Util-Low";
// Future enhancement
#if 0
			else if (g_select == PACLIENT_SEL_ALL)
				p_select = "ALL";
#endif
			else
			{
				g_select = STL_PA_SELECT_UTIL_HIGH;
				p_select = "Util-High";
			}

			printf("VF BW Util: %-.45s%s Criteria: %s",
				g_PmVFInfo.vfName,
				strgetlaststr(g_PmVFInfo.vfName, 45),
 				p_select);
			if (g_expr_funct)
				printf("  Start: %d", g_start);
			printf("  Number: %d\n", g_range);
			ct_lines -= 1;

			if (g_PmVFInfo.numPorts == 0)
			{
				printf("\n");
				printf( "No ports in vf\n");
				ct_lines -= 2;
			}
			else {
				ct_lines -= DisplayScreen_Util(&g_PmVFInfo.internalUtilStats, "Int:");
				printf("\n");
			    printf("                      Max       0+%%      25+%%      50+%%      75+%%     100+%%\n");
			    ct_lines -=2;
			    printf( "Int Congestion %10u %9u %9u %9u %9u %9u\n",
			    		  g_PmVFInfo.internalCategoryStats.categoryMaximums.congestion,
					  g_PmVFInfo.internalCategoryStats.ports[0].congestion,
					  g_PmVFInfo.internalCategoryStats.ports[1].congestion,
					  g_PmVFInfo.internalCategoryStats.ports[2].congestion,
					  g_PmVFInfo.internalCategoryStats.ports[3].congestion,
					  g_PmVFInfo.internalCategoryStats.ports[4].congestion );
			    ct_lines-=1;
			}
		} else {
			printf("VF BW Util: VF INFO NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	case SCREEN_GROUP_CTG_STATS:
		if (fb_valid_group_info)
		{
			if (g_select == STL_PA_SELECT_CATEGORY_CONG)
				p_select = "Congst";
			else if (g_select == STL_PA_SELECT_CATEGORY_SMA_CONG)
				p_select = "SmaCong";
			else if (g_select == STL_PA_SELECT_CATEGORY_BUBBLE)
				p_select = "Bubble";
			else if (g_select == STL_PA_SELECT_CATEGORY_SEC)
				p_select = "Secure";
			else if (g_select == STL_PA_SELECT_CATEGORY_ROUT)
				p_select = "Routing";
// Future enhancement
#if 0
			else if (g_select == PACLIENT_SEL_ALL)
				p_select = "ALL";
#endif
			else
			{
				g_select = STL_PA_SELECT_CATEGORY_INTEG;
				p_select = "Integ";
			}
	
			printf( "Group Ctg Stats: %-.43s%s Criteria: %s",
				g_PmGroupInfo.groupName,
				strgetlaststr(g_PmGroupInfo.groupName, 43),
 				p_select );
			if (g_expr_funct)
				printf("  Start: %d", g_start);
			printf("  Number: %d\n", g_range);
			ct_lines -= 1;
	
			if (g_PmGroupInfo.numInternalPorts)
				ct_lines -= DisplayScreen_Err(&g_PmGroupInfo.internalCategoryStats, "Int");
	
			if (g_PmGroupInfo.numExternalPorts)
			{
				printf("\n");
				ct_lines -= 1;
				ct_lines -= DisplayScreen_Err(&g_PmGroupInfo.externalCategoryStats, "Ext");
			}

			if (g_PmGroupInfo.numInternalPorts == 0
				&& g_PmGroupInfo.numExternalPorts == 0)
			{
				printf("\n");
				printf( "No ports in group\n");
				ct_lines -= 2;
			}
		}

		else
		{
		   	printf("Group Ctg Stats: GROUP INFO NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	case SCREEN_VF_CTG_STATS:
		if (fb_valid_VF_info) {
			if (g_select == STL_PA_SELECT_CATEGORY_CONG)
				p_select = "Congst";
			else if (g_select == STL_PA_SELECT_CATEGORY_SMA_CONG)
				p_select = "SmaCong";
			else if (g_select == STL_PA_SELECT_CATEGORY_BUBBLE)
				p_select = "Bubble";
			else if (g_select == STL_PA_SELECT_CATEGORY_SEC)
				p_select = "Secure";
			else if (g_select == STL_PA_SELECT_CATEGORY_ROUT)
				p_select = "Routing";
// Future enhancement
#if 0
			else if (g_select == PACLIENT_SEL_ALL)
				p_select = "ALL";
#endif
			else
			{
				g_select = STL_PA_SELECT_CATEGORY_INTEG;
				p_select = "Integ";
			}

			printf("VF Ctg Stats: %-.46s%s Criteria: %s",
				g_PmVFInfo.vfName,
				strgetlaststr(g_PmVFInfo.vfName, 46),
				p_select);
			if (g_expr_funct)
				printf("  Start: %d", g_start);
			printf("  Number: %d\n", g_range);
			ct_lines -= 1;

			if (g_PmVFInfo.numPorts == 0)
			{
				printf("\n");
				printf( "No ports in vf\n");
				ct_lines -= 2;
			}
			else {
				ct_lines -= DisplayScreen_Err(&g_PmVFInfo.internalCategoryStats, "Int");
			}
		} else {
			printf("VF Ctg Stats: VF INFO NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	case SCREEN_GROUP_CONFIG:
		if (fb_valid_group_config)
		{
			printf( "Group Config: %-.45s%s NumPorts: %u\n",
				pg_PmGroupConfig.groupName,
				strgetlaststr(pg_PmGroupConfig.groupName, 45),
 				pg_PmGroupConfig.numPorts );
			printf("  Ix    LIDx   Port   Node GUID 0x   NodeDesc\n");
			ct_lines -= 2;
	
			// if ports we were showing are removed, go to last port
			if (g_scroll >= pg_PmGroupConfig.numPorts)
				g_scroll = pg_PmGroupConfig.numPorts-1;
			for ( ix = g_scroll; (ix < pg_PmGroupConfig.numPorts) &&
					((ix - g_scroll) < MAX_GROUPCONFIG_PORTS_PER_SCREEN); ix++ )
			{
				// Truncate nodeDesc to keep line at 80 columns
				printf( "%5d %.*X %3u  %016" PRIX64 " %-.42s%s\n", ix,
					(pg_PmGroupConfig.portList[ix].nodeLid <= IB_MAX_UCAST_LID ? 4:8),
					pg_PmGroupConfig.portList[ix].nodeLid,
					pg_PmGroupConfig.portList[ix].portNumber,
					pg_PmGroupConfig.portList[ix].nodeGUID,
					pg_PmGroupConfig.portList[ix].nodeDesc,
					strgetlaststr(pg_PmGroupConfig.portList[ix].nodeDesc, 42) );
				ct_lines -= 1;
			}
	
		}
		else if (fb_valid_group_info &&
					g_PmGroupInfo.numInternalPorts == 0 &&
					g_PmGroupInfo.numExternalPorts == 0)
		{
			printf("\n");
			printf( "No ports in group\n");
			ct_lines -= 2;
		}
		else
		{
			printf("Group Config: GROUP CONFIG NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	case SCREEN_VF_CONFIG:
		if (fb_valid_VF_config) {
			printf("VF Config: %-.45s%s NumPorts: %u\n",
				g_pPmVFConfig.vfName,
				strgetlaststr(g_pPmVFConfig.vfName, 45),
				g_pPmVFConfig.numPorts);
			printf("  Ix    LIDx   Port   Node GUID 0x   NodeDesc\n");
			ct_lines -= 2;

			// if ports we were showing are removed, go to last port
			if (g_scroll >= g_pPmVFConfig.numPorts)
				g_scroll = g_pPmVFConfig.numPorts-1;
			for ( ix = g_scroll; (ix < g_pPmVFConfig.numPorts) &&
					((ix - g_scroll) < MAX_GROUPCONFIG_PORTS_PER_SCREEN); ix++ )
			{
				// Truncate nodeDesc to keep line at 80 columns
				printf( "%5d %.*X %3u  %016" PRIX64 " %-.42s%s\n", ix,
					(g_pPmVFConfig.portList[ix].nodeLid <= IB_MAX_UCAST_LID ? 4:8),
					g_pPmVFConfig.portList[ix].nodeLid,
					g_pPmVFConfig.portList[ix].portNumber,
					g_pPmVFConfig.portList[ix].nodeGUID,
					g_pPmVFConfig.portList[ix].nodeDesc,
					strgetlaststr(g_pPmVFConfig.portList[ix].nodeDesc, 42) );
				ct_lines -= 1;
			}

		} else if (fb_valid_VF_info && g_PmVFInfo.numPorts == 0) {
			printf("\n");
			printf( "No ports in vf\n");
			ct_lines -= 2;
		} else {
			printf("VF Config: VF CONFIG NOT AVAILABLE\n");
			ct_lines -= 1;
		}
		break;

	case SCREEN_GROUP_FOCUS:
		if (fb_valid_group_focus)
		{
			char* status_color_local = bf_color_off;
			char* status_symbol_local = " ";
			char* status_color_neighbor = bf_color_off;
			char* status_symbol_neighbor = " ";

			if (pg_PmGroupFocus.select == STL_PA_SELECT_UTIL_PKTS_HIGH)
				p_select = "UtlPkt-Hi";
			else if (pg_PmGroupFocus.select == STL_PA_SELECT_UTIL_LOW)
				p_select = " Util-Low";
			else if (pg_PmGroupFocus.select == STL_PA_SELECT_CATEGORY_INTEG)
				p_select = "Integrity";
			else if (pg_PmGroupFocus.select == STL_PA_SELECT_CATEGORY_CONG)
				p_select = " Congestn";
			else if (pg_PmGroupFocus.select == STL_PA_SELECT_CATEGORY_SMA_CONG)
				p_select = "SmaCongst";
			else if (pg_PmGroupFocus.select == STL_PA_SELECT_CATEGORY_BUBBLE)
				p_select = "  Bubble";
			else if (pg_PmGroupFocus.select == STL_PA_SELECT_CATEGORY_SEC)
				p_select = " Security";
			else if (pg_PmGroupFocus.select == STL_PA_SELECT_CATEGORY_ROUT)
				p_select = "  Routing";
// Future enhancement
#if 0
			else if (pg_PmGroupFocus.select == PACLIENT_SEL_ALL)
				p_select = "      ALL";
#endif
			else
			{
				pg_PmGroupFocus.select = STL_PA_SELECT_UTIL_HIGH;
				p_select = "Util-High";
			}
	
			printf( "Group Focus: %-.29s%s  GrpNumPorts: %u  NumPorts: %u",
				pg_PmGroupFocus.groupName,
				strgetlaststr(pg_PmGroupFocus.groupName, 29),
				g_PmGroupInfo.numInternalPorts + g_PmGroupInfo.numExternalPorts,
				pg_PmGroupFocus.numPorts );
			if (g_expr_funct)
				printf("  StartIx: %u", pg_PmGroupFocus.start);
			printf("  Number: %u\n", pg_PmGroupFocus.range);
			printf("  Ix  %9s   LIDx   Port   Node GUID 0x   NodeDesc\n", p_select);
			ct_lines -= 2;
	
			// if ports we were showing are removed, go to last port
			if (g_scroll >= pg_PmGroupFocus.numPorts)
				g_scroll = pg_PmGroupFocus.numPorts-1;
			for ( ix = g_scroll; (ix < pg_PmGroupFocus.numPorts) &&
					((ix - g_scroll) < MAX_GROUPFOCUS_PORTS_PER_SCREEN); ix++ )
			{
				switch (pg_PmGroupFocus.portList[ix].localStatus) {
				case STL_PA_FOCUS_STATUS_PMA_IGNORE:
					status_color_local = bf_color_blue;
					status_symbol_local = "~";
					break;
				case STL_PA_FOCUS_STATUS_PMA_FAILURE:
					status_color_local = bf_color_yellow;
					status_symbol_local = "!";
					break;
				case STL_PA_FOCUS_STATUS_TOPO_FAILURE:
					status_color_local = bf_color_red;
					status_symbol_local = "?";
					break;
				case STL_PA_FOCUS_STATUS_OK:
				default:
					status_color_local = bf_color_off;
					status_symbol_local = " ";
				}
				switch (pg_PmGroupFocus.portList[ix].neighborStatus) {
				case STL_PA_FOCUS_STATUS_PMA_IGNORE:
					status_color_neighbor = bf_color_blue;
					status_symbol_neighbor = "~";
					break;
				case STL_PA_FOCUS_STATUS_PMA_FAILURE:
					status_color_neighbor = bf_color_yellow;
					status_symbol_neighbor = "!";
					break;
				case STL_PA_FOCUS_STATUS_TOPO_FAILURE:
					status_color_neighbor = bf_color_red;
					status_symbol_neighbor = "?";
					break;
				case STL_PA_FOCUS_STATUS_OK:
				default:
					status_color_neighbor = bf_color_off;
					status_symbol_neighbor = " ";
				}
				printf("%s%s%4u ", status_color_local, status_symbol_local, ix);

				if ( ( (pg_PmGroupFocus.select >= STL_PA_SELECT_CATEGORY_INTEG) &&
						(pg_PmGroupFocus.select <= STL_PA_SELECT_CATEGORY_ROUT) ) ||
						(pg_PmGroupFocus.select == STL_PA_SELECT_UTIL_PKTS_HIGH) )
					printf("%9" PRIu64 "%s",
						pg_PmGroupFocus.portList[ix].value,
						bf_color_off);
				else
					printf( "%9.1f%s",
						(float)pg_PmGroupFocus.portList[ix].value / 10.0,
						bf_color_off);

				printf( " %.*X %3u  %016" PRIX64 " %-.32s%s\n",
					(pg_PmGroupFocus.portList[ix].nodeLid <= IB_MAX_UCAST_LID ? 4:8),
					pg_PmGroupFocus.portList[ix].nodeLid,
					pg_PmGroupFocus.portList[ix].portNumber,
					pg_PmGroupFocus.portList[ix].nodeGUID,
					pg_PmGroupFocus.portList[ix].nodeDesc,
					strgetlaststr(pg_PmGroupFocus.portList[ix].nodeDesc, 32) );

				if (pg_PmGroupFocus.portList[ix].neighborLid)
				{
					printf("%s%s <-> ", status_color_neighbor,
						status_symbol_neighbor);
	
					if ( ( (pg_PmGroupFocus.select >= STL_PA_SELECT_CATEGORY_INTEG) &&
							(pg_PmGroupFocus.select <= STL_PA_SELECT_CATEGORY_ROUT) ) ||
							(pg_PmGroupFocus.select == STL_PA_SELECT_UTIL_PKTS_HIGH) )
						printf( "%9" PRIu64 "%s",
							pg_PmGroupFocus.portList[ix].neighborValue,
							bf_color_off);
					else
						printf( "%9.1f%s",
							(float)pg_PmGroupFocus.portList[ix].neighborValue / 10.0,
							bf_color_off);
	
					// Truncate neighborNodeDesc to keep line at 80 columns
					printf( " %.*X %3u  %016" PRIX64 " %-.34s%s\n",
						(pg_PmGroupFocus.portList[ix].neighborLid <= IB_MAX_UCAST_LID ? 4:8),
						pg_PmGroupFocus.portList[ix].neighborLid,
						pg_PmGroupFocus.portList[ix].neighborPortNumber,
						pg_PmGroupFocus.portList[ix].neighborGuid,
						pg_PmGroupFocus.portList[ix].neighborNodeDesc,
						strgetlaststr(pg_PmGroupFocus.portList[ix].neighborNodeDesc, 34) );
				}
				else
					printf("%s%s <-> none%s\n", status_color_neighbor,
						status_symbol_neighbor, bf_color_off);

				ct_lines -= 2;
			}
		} else if (fb_valid_group_info &&
				   g_PmGroupInfo.numInternalPorts == 0 &&
				   g_PmGroupInfo.numExternalPorts == 0) {
			printf("\n");
			printf( "No ports in group\n");
			ct_lines -= 2;
		} else {
		   	printf("Group Focus: GROUP FOCUS NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	case SCREEN_VF_FOCUS:
		if (fb_valid_VF_focus)
		{
			char* status_color_local = bf_color_off;
			char* status_symbol_local = " ";
			char* status_color_neighbor = bf_color_off;
			char* status_symbol_neighbor = " ";

			if (g_pPmVFFocus.select == STL_PA_SELECT_UTIL_PKTS_HIGH)
				p_select = "UtlPkt-Hi";
			else if (g_pPmVFFocus.select == STL_PA_SELECT_UTIL_LOW)
				p_select = " Util-Low";
			else if (g_pPmVFFocus.select == STL_PA_SELECT_CATEGORY_INTEG)
				p_select = "Integrity";
			else if (g_pPmVFFocus.select == STL_PA_SELECT_CATEGORY_CONG)
				p_select = " Congestn";
			else if (g_pPmVFFocus.select == STL_PA_SELECT_CATEGORY_SMA_CONG)
				p_select = "SmaCongst";
			else if (g_pPmVFFocus.select == STL_PA_SELECT_CATEGORY_BUBBLE)
				p_select = "   Bubble";
			else if (g_pPmVFFocus.select == STL_PA_SELECT_CATEGORY_SEC)
				p_select = " Security";
			else if (g_pPmVFFocus.select == STL_PA_SELECT_CATEGORY_ROUT)
				p_select = "  Routing";
// Future enhancement
#if 0
			else if (g_pPmVFFocus.select == PACLIENT_SEL_ALL)
				p_select = "      ALL";
#endif
			else
			{
				g_pPmVFFocus.select = STL_PA_SELECT_UTIL_HIGH;
				p_select = "Util-High";
			}
	
			printf( "VF Focus: %-.30s%s  GrpNumPorts: %u  NumPorts: %u",
				g_pPmVFFocus.vfName,
				strgetlaststr(g_pPmVFFocus.vfName, 30),
				g_PmVFInfo.numPorts,
				g_pPmVFFocus.numPorts );
			if (g_expr_funct)
				printf("  StartIx: %u", g_pPmVFFocus.start);
			printf("  Number: %u\n", g_pPmVFFocus.range);
			printf("  Ix  %9s   LIDx   Port   Node GUID 0x   NodeDesc\n", p_select);
			ct_lines -= 2;
	
			// if ports we were showing are removed, go to last port
			if (g_scroll >= g_pPmVFFocus.numPorts)
				g_scroll = g_pPmVFFocus.numPorts-1;
			for ( ix = g_scroll; (ix < g_pPmVFFocus.numPorts) &&
					((ix - g_scroll) < MAX_VFFOCUS_PORTS_PER_SCREEN); ix++ )
			{
				switch (g_pPmVFFocus.portList[ix].localStatus) {
				case STL_PA_FOCUS_STATUS_PMA_IGNORE:
					status_color_local = bf_color_blue;
					status_symbol_local = "~";
					break;
				case STL_PA_FOCUS_STATUS_PMA_FAILURE:
					status_color_local = bf_color_yellow;
					status_symbol_local = "!";
					break;
				case STL_PA_FOCUS_STATUS_TOPO_FAILURE:
					status_color_local = bf_color_red;
					status_symbol_local = "?";
					break;
				case STL_PA_FOCUS_STATUS_OK:
				default:
					status_color_local = bf_color_off;
					status_symbol_local = " ";
				}
				switch (g_pPmVFFocus.portList[ix].neighborStatus) {
				case STL_PA_FOCUS_STATUS_PMA_IGNORE:
					status_color_neighbor = bf_color_blue;
					status_symbol_neighbor = "~";
					break;
				case STL_PA_FOCUS_STATUS_PMA_FAILURE:
					status_color_neighbor = bf_color_yellow;
					status_symbol_neighbor = "!";
					break;
				case STL_PA_FOCUS_STATUS_TOPO_FAILURE:
					status_color_neighbor = bf_color_red;
					status_symbol_neighbor = "?";
					break;
				case STL_PA_FOCUS_STATUS_OK:
				default:
					status_color_neighbor = bf_color_off;
					status_symbol_neighbor = " ";
				}
				printf("%s%s%4u ", status_color_local, status_symbol_local, ix);

				if ( ( (g_pPmVFFocus.select >= STL_PA_SELECT_CATEGORY_INTEG) &&
						(g_pPmVFFocus.select <= STL_PA_SELECT_CATEGORY_ROUT) ) ||
						(g_pPmVFFocus.select == STL_PA_SELECT_UTIL_PKTS_HIGH) )
					printf("%9" PRIu64 "%s",
						 g_pPmVFFocus.portList[ix].value, bf_color_off);
				else
					printf( "%9.1f%s",
						(float)g_pPmVFFocus.portList[ix].value / 10.0,
						bf_color_off);

				// Truncate nodeDesc to keep line at 80 columns
				printf( " %.*X %3u  %016" PRIX64 " %-.32s%s\n",
					(g_pPmVFFocus.portList[ix].nodeLid <= IB_MAX_UCAST_LID ? 4:8),
					g_pPmVFFocus.portList[ix].nodeLid,
					g_pPmVFFocus.portList[ix].portNumber,
					g_pPmVFFocus.portList[ix].nodeGUID,
					g_pPmVFFocus.portList[ix].nodeDesc,
					strgetlaststr(g_pPmVFFocus.portList[ix].nodeDesc, 32) );

				if (g_pPmVFFocus.portList[ix].neighborLid)
				{
					printf("%s%s <-> ", status_color_neighbor,
						status_symbol_neighbor);
	
					if ( ( (g_pPmVFFocus.select >= STL_PA_SELECT_CATEGORY_INTEG) &&
							(g_pPmVFFocus.select <= STL_PA_SELECT_CATEGORY_ROUT) ) ||
							(g_pPmVFFocus.select == STL_PA_SELECT_UTIL_PKTS_HIGH) )
						printf( "%9" PRIu64 "%s",
							g_pPmVFFocus.portList[ix].neighborValue,
							bf_color_off);
					else
						printf( "%9.1f%s",
							(float)g_pPmVFFocus.portList[ix].neighborValue / 10.0,
							bf_color_off);
	
					// Truncate neighborNodeDesc to keep line at 80 columns
					printf( " %.*X %3u  %016" PRIX64 " %-.34s%s\n",
						(g_pPmVFFocus.portList[ix].neighborLid <= IB_MAX_UCAST_LID ? 4:8),
						g_pPmVFFocus.portList[ix].neighborLid,
						g_pPmVFFocus.portList[ix].neighborPortNumber,
						g_pPmVFFocus.portList[ix].neighborGuid,
						g_pPmVFFocus.portList[ix].neighborNodeDesc,
						strgetlaststr(g_pPmVFFocus.portList[ix].neighborNodeDesc, 34) );
				}
				else
					printf("%s%s <-> none%s\n", status_color_neighbor,
						status_symbol_neighbor, bf_color_off);

				ct_lines -= 2;
			}
		}
		else if (fb_valid_VF_info &&
				   g_PmVFInfo.numPorts == 0)
		{
			printf("\n");
			printf( "No ports in vf\n");
			ct_lines -= 2;
		}
		else
		{
		   	printf("VF Focus: VF FOCUS NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	case SCREEN_PORT_STATS:
		if (fb_valid_port_stats)
		{
			if (tb_menu[n_level_menu - 1] == SCREEN_GROUP_CONFIG)
			{
				if (fb_valid_group_config)
				{
					printf( "Port Stats: %-.44s%s LID: 0x%X PortNum: %u\n",
						pg_PmGroupConfig.groupName,
						strgetlaststr(pg_PmGroupConfig.groupName, 44),
						pg_PmGroupConfig.portList[g_ix_port - g_start].nodeLid,
						pg_PmGroupConfig.portList[g_ix_port - g_start].portNumber );
					printf( "NodeDesc: %-.41s%s NodeGUID: 0x%016"PRIX64"\n\n",
						pg_PmGroupConfig.portList[g_ix_port - g_start].nodeDesc,
						strgetlaststr(pg_PmGroupConfig.portList[g_ix_port - g_start].nodeDesc, 41),
						pg_PmGroupConfig.portList[g_ix_port - g_start].nodeGUID );
					ct_lines -= 3;
				}

				else
				{
					printf("Port Stats: GROUP CONFIG NOT AVAILABLE\n");
					ct_lines -= 1;
				}
			} else if (tb_menu[n_level_menu - 1] == SCREEN_VF_CONFIG) {
				if (fb_valid_VF_config) {
					printf( "Port Stats: %-.44s%s LID: 0x%X PortNum: %u\n",
						g_pPmVFConfig.vfName,
						strgetlaststr(g_pPmVFConfig.vfName, 44),
						g_pPmVFConfig.portList[g_ix_port - g_start].nodeLid,
						g_pPmVFConfig.portList[g_ix_port - g_start].portNumber );
					printf( "NodeDesc: %-.41s%c NodeGUID: 0x%016"PRIX64"\n\n",
						g_pPmVFConfig.portList[g_ix_port - g_start].nodeDesc,
						strgetlastchar(g_pPmVFConfig.portList[g_ix_port - g_start].nodeDesc, 41),
						g_pPmVFConfig.portList[g_ix_port - g_start].nodeGUID );
					ct_lines -= 3;
				} else {
					printf("Port Stats: VF CONFIG NOT AVAILABLE\n");
					ct_lines -= 1;
				}
			}

			else
			{
				char *status_color = bf_color_off;
				char *status_message = "";
				if (g_portCounterFlags & STL_PA_PC_FLAG_UNEXPECTED_CLEAR) {
					status_color = bf_color_red;
					status_message = "  Unexp Clear";
				}

				if (tb_menu[n_level_menu - 1] == SCREEN_GROUP_FOCUS) {
					if (fb_valid_group_focus)
					{
						if (fb_port_neighbor)
						{
							printf( "Port Stats: %-.10s%s LID: 0x%X PortNum: %u Rate: %4s MTU:%5s%s%s%s\n",
								pg_PmGroupFocus.groupName,
								strgetlaststr(pg_PmGroupFocus.groupName, 10),
								pg_PmGroupFocus.portList[g_ix_port - g_start].neighborLid,
								pg_PmGroupFocus.portList[g_ix_port - g_start].neighborPortNumber,
								StlStaticRateToText(pg_PmGroupFocus.portList[g_ix_port - g_start].rate),
								IbMTUToText(pg_PmGroupFocus.portList[g_ix_port - g_start].maxVlMtu),
								status_color, status_message, bf_color_off );
							printf( "NodeDesc: %-.41s%s NodeGUID: 0x%016"PRIX64"\n",
								pg_PmGroupFocus.portList[g_ix_port - g_start].neighborNodeDesc,
								strgetlaststr(pg_PmGroupFocus.portList[g_ix_port - g_start].neighborNodeDesc, 41),
								pg_PmGroupFocus.portList[g_ix_port - g_start].neighborGuid );
							printf( "Neighbor: %-.41s%s LID: 0x%X PortNum: %u\n",
								pg_PmGroupFocus.portList[g_ix_port - g_start].nodeDesc,
								strgetlaststr(pg_PmGroupFocus.portList[g_ix_port - g_start].nodeDesc, 41),
								pg_PmGroupFocus.portList[g_ix_port - g_start].nodeLid,
								pg_PmGroupFocus.portList[g_ix_port - g_start].portNumber );
						}

						else
						{
							printf( "Port Stats: %-.10s%s LID: 0x%X PortNum: %u Rate: %4s MTU:%5s%s%s%s\n",
								pg_PmGroupFocus.groupName,
								strgetlaststr(pg_PmGroupFocus.groupName, 10),
								pg_PmGroupFocus.portList[g_ix_port - g_start].nodeLid,
								pg_PmGroupFocus.portList[g_ix_port - g_start].portNumber,
								StlStaticRateToText(pg_PmGroupFocus.portList[g_ix_port - g_start].rate),
								IbMTUToText(pg_PmGroupFocus.portList[g_ix_port - g_start].maxVlMtu),
								status_color, status_message, bf_color_off );
							printf( "NodeDesc: %-.41s%s NodeGUID: 0x%016"PRIX64"\n",
								pg_PmGroupFocus.portList[g_ix_port - g_start].nodeDesc,
								strgetlaststr(pg_PmGroupFocus.portList[g_ix_port - g_start].nodeDesc, 41),
								pg_PmGroupFocus.portList[g_ix_port - g_start].nodeGUID );

							if (pg_PmGroupFocus.portList[g_ix_port - g_start].neighborLid)
								printf( "Neighbor: %-.41s%s LID: 0x%X PortNum: %u\n",
									pg_PmGroupFocus.portList[g_ix_port - g_start].neighborNodeDesc,
									strgetlaststr(pg_PmGroupFocus.portList[g_ix_port - g_start].neighborNodeDesc, 41),
									pg_PmGroupFocus.portList[g_ix_port - g_start].neighborLid,
									pg_PmGroupFocus.portList[g_ix_port - g_start].neighborPortNumber );
							else
								printf("Neighbor:none\n");
						}
	
						ct_lines -= 3;
					}

					else
					{
						printf("Port Stats: GROUP FOCUS NOT AVAILABLE\n");
						ct_lines -= 1;
					}
				} else if (tb_menu[n_level_menu - 1] == SCREEN_VF_FOCUS) {
					if (fb_valid_VF_focus)
					{
						if (fb_port_neighbor)
						{
							printf( "Port Stats: %-.10s%s LID: 0x%X PortNum: %u Rate: %4s MTU:%5s%s%s%s\n",
								g_pPmVFFocus.vfName,
								strgetlaststr(g_pPmVFFocus.vfName, 10),
								g_pPmVFFocus.portList[g_ix_port - g_start].neighborLid,
								g_pPmVFFocus.portList[g_ix_port - g_start].neighborPortNumber,
								StlStaticRateToText(g_pPmVFFocus.portList[g_ix_port - g_start].rate),
								IbMTUToText(g_pPmVFFocus.portList[g_ix_port - g_start].maxVlMtu),
								status_color, status_message, bf_color_off );
							printf( "NodeDesc: %-.41s%s NodeGUID: 0x%016"PRIX64"\n",
								g_pPmVFFocus.portList[g_ix_port - g_start].neighborNodeDesc,
								strgetlaststr(g_pPmVFFocus.portList[g_ix_port - g_start].neighborNodeDesc, 41),
								g_pPmVFFocus.portList[g_ix_port - g_start].neighborGuid );
							printf( "Neighbor: %-.41s%s LID: 0x%X PortNum: %u\n",
								g_pPmVFFocus.portList[g_ix_port - g_start].nodeDesc,
								strgetlaststr(g_pPmVFFocus.portList[g_ix_port - g_start].nodeDesc, 41),
								g_pPmVFFocus.portList[g_ix_port - g_start].nodeLid,
								g_pPmVFFocus.portList[g_ix_port - g_start].portNumber );
						}

						else
						{
							printf( "Port Stats: %-.10s%s LID: 0x%X PortNum: %u Rate: %4s MTU:%5s%s%s%s\n",
								g_pPmVFFocus.vfName,
								strgetlaststr(g_pPmVFFocus.vfName, 10),
								g_pPmVFFocus.portList[g_ix_port - g_start].nodeLid,
								g_pPmVFFocus.portList[g_ix_port - g_start].portNumber,
								StlStaticRateToText(g_pPmVFFocus.portList[g_ix_port - g_start].rate),
								IbMTUToText(g_pPmVFFocus.portList[g_ix_port - g_start].maxVlMtu),
								status_color, status_message, bf_color_off );
							printf( "NodeDesc: %-.41s%s NodeGUID: 0x%016"PRIX64"\n",
								g_pPmVFFocus.portList[g_ix_port - g_start].nodeDesc,
								strgetlaststr(g_pPmVFFocus.portList[g_ix_port - g_start].nodeDesc, 41),
								g_pPmVFFocus.portList[g_ix_port - g_start].nodeGUID );

							if (g_pPmVFFocus.portList[g_ix_port - g_start].neighborLid)
								printf( "Neighbor: %-.41s%s LID: 0x%X PortNum: %u\n",
									g_pPmVFFocus.portList[g_ix_port - g_start].neighborNodeDesc,
									strgetlaststr(g_pPmVFFocus.portList[g_ix_port - g_start].neighborNodeDesc, 41),
									g_pPmVFFocus.portList[g_ix_port - g_start].neighborLid,
									g_pPmVFFocus.portList[g_ix_port - g_start].neighborPortNumber );
							else
								printf("Neighbor: none\n");
						}
	
						ct_lines -= 3;
					}

					else
					{
						printf("Port Stats: VF FOCUS NOT AVAILABLE\n");
						ct_lines -= 1;
					}
				}

			}

			/* First half of counters (Data, Cong, Integ) */
			if (g_scroll_cntrs == 0) {
				printf(" Xmit: Data: %10"PRIu64" MB (%10"PRIu64" Flits) Pkts: %10"PRIu64"\n",
					g_portCounters.portXmitData / FLITS_PER_MB,
					g_portCounters.portXmitData, g_portCounters.portXmitPkts);
				printf( " Recv: Data: %10"PRIu64" MB (%10"PRIu64" Flits) Pkts: %10"PRIu64"\n",
					g_portCounters.portRcvData / FLITS_PER_MB,
					g_portCounters.portRcvData, g_portCounters.portRcvPkts );
				printf( " Multicast: Xmit Pkts: %-10"PRIu64"  Recv Pkts: %-10"PRIu64"\n",
					g_portCounters.portMulticastXmitPkts, g_portCounters.portMulticastRcvPkts );

				{
					printf("\n\n");
				}
				//ct_lines -= 5;
				printf( " Integrity:\n");
				printf( "  Link Quality:    %10u | Lanes Down:       %10u\n",
					g_portCounters.lq.s.linkQualityIndicator,
					g_portCounters.lq.s.numLanesDown);
				printf( "  Uncorrectable:   %10llu | Link Downed:      %10llu\n",
					(unsigned long long)g_portCounters.uncorrectableErrors,
					(unsigned long long)g_portCounters.linkDowned);
				printf( "  Loc Lnk Integ:   %10llu | Lnk Err Recov:    %10llu\n",
					(unsigned long long)g_portCounters.localLinkIntegrityErrors,
					(unsigned long long)g_portCounters.linkErrorRecovery);
				printf( "  Rcv Errors:      %10llu | Excs Bfr Ovrn*:   %10llu\n",
					(unsigned long long)g_portCounters.portRcvErrors,
					(unsigned long long)g_portCounters.excessiveBufferOverruns);
				printf( "  FM Conf Err:     %10llu |\n",
					(unsigned long long)g_portCounters.fmConfigErrors);
				//ct_lines -= 6; //11
				printf( " Congestion:\n");
				printf( "  Cong Discards:   %10llu | Rcv FECN*:        %10llu\n",
					(unsigned long long)g_portCounters.swPortCongestion,
					(unsigned long long)g_portCounters.portRcvFECN);
				printf( "  Mark FECN:       %10llu | Rcv BECN:         %10llu\n",
					(unsigned long long)g_portCounters.portMarkFECN,
					(unsigned long long)g_portCounters.portRcvBECN);
				printf( "  Xmit Wait:       %10llu | Xmit Time Cong:   %10llu\n",
					(unsigned long long)g_portCounters.portXmitWait,
					(unsigned long long)g_portCounters.portXmitTimeCong);
				//ct_lines -= 4; //15
				ct_lines -= 15;
			} else if (g_scroll_cntrs == 1) {
				printf( " Bubble:\n" );
				printf( "  Xmit Wasted BW:  %10llu | Rcv Bubble*:      %10llu\n",
					(unsigned long long)g_portCounters.portXmitWastedBW,
					(unsigned long long)g_portCounters.portRcvBubble);
				printf( "  Xmit Wait Data:  %10llu |\n",
					(unsigned long long)g_portCounters.portXmitWaitData);
				printf( " Security:\n");
				printf( "  Xmit Constrain:  %10llu | Rcv Constrain*:   %10llu\n",
					(unsigned long long)g_portCounters.portXmitConstraintErrors,
					(unsigned long long)g_portCounters.portRcvConstraintErrors);
				printf( " Routing and Others:\n");
				printf( "  Rcv Sw Relay:    %10llu | Xmit Discards:    %10llu\n",
					(unsigned long long)g_portCounters.portRcvSwitchRelayErrors,
					(unsigned long long)g_portCounters.portXmitDiscards);
				ct_lines -= 7;
				if(fb_valid_vf_port_stats_hidden) {
					printf( " SmaCongestion (VL15):\n");
					printf( "  Cong Discards:   %10llu | Xmit Wait:        %10llu\n",
						(unsigned long long)g_hiddenVfPortCounters.swPortVFCongestion,
						(unsigned long long)g_hiddenVfPortCounters.portVFXmitWait);
					ct_lines -= 2;
				}
			}
		}
		else
		{
		   	printf("Port Stats: PORT COUNTERS NOT AVAILABLE\n");
			ct_lines -= 1;
		}

		break;

	default:
		printf("INVALID MENU VALUE\n");
		ct_lines -= 1;
		break;

	}	// End of switch (tb_menu[n_level_menu])

	// Display error line
	if (bf_error[0])
	{
		printf("ERROR: %s\n", bf_error);
		fb_error_displayed = TRUE;
		ct_lines -= 1;
	}

	// Display input prompt
	if (tb_menu[n_level_menu] == SCREEN_VF_SUMMARY || 
		tb_menu[n_level_menu] == SCREEN_SUMMARY)
		ct_lines -= 1;	// extra line for menu in these screens
	for ( ; ct_lines > 0; ct_lines--)
		printf("\n");

	printf("Quit up Live/rRev/fFwd/bookmrked Bookmrk Unbookmrk ?help |%c", 
			(tb_menu[n_level_menu] == SCREEN_VF_SUMMARY || 
			 tb_menu[n_level_menu] == SCREEN_SUMMARY) ? '\n' : ' ');
	p_multi_input = "qQ";

	switch (tb_menu[n_level_menu])
	{
	case SCREEN_VF_SUMMARY:
	case SCREEN_SUMMARY:
		printf("sS Pmcfg Imginfo View 0-n:\n");
		break;
	
	case SCREEN_VF_INFO_SELECT:
	case SCREEN_GROUP_INFO_SELECT:
		printf("P S C:\n");
		break;

	case SCREEN_VF_CTG_STATS:
	case SCREEN_VF_BW_STATS:
	case SCREEN_GROUP_BW_STATS:
	case SCREEN_GROUP_CTG_STATS:
		if (g_expr_funct)
		{
			printf("cC I0-n N0-n Detail:\n");
			p_multi_input = "iInNqQ";
		}
		else
		{
			printf("cC N0-n Detail:\n");
			p_multi_input = "nNqQ";
		}
		break;

	case SCREEN_VF_CONFIG:
	case SCREEN_GROUP_CONFIG:
		printf("sS P0-n:\n");
		p_multi_input = "pPqQ";
		break;

	case SCREEN_VF_FOCUS:
	case SCREEN_GROUP_FOCUS:
		if (g_expr_funct)
		{
			printf("sS cC I0-n N0-n P0-n:\n");
			p_multi_input = "iInNpPqQ";
		}
		else
		{
			printf("sS cC N0-n P0-n:\n");
			p_multi_input = "nNpPqQ";
		}
		break;

	case SCREEN_PORT_STATS:
		if (tb_menu[n_level_menu - 1] == SCREEN_GROUP_FOCUS ||
			tb_menu[n_level_menu - 1] == SCREEN_VF_FOCUS )
		{
			if (fb_port_has_neighbor)
				printf("Neighbor ");
		}
		printf("sS:\n");
		fflush(stdout);
		break;
	default:
		printf("\n ");
		break;

	}	// End of switch (tb_menu[n_level_menu])

}	// End of DisplayScreen()

// command line options, each has a short and long flag name
struct option options[] = {
		// basic controls
		{ "verbose", no_argument, NULL, 'v' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "hfi", required_argument, NULL, 'h' },
		{ "port", required_argument, NULL, 'p' },
		{ "interval", no_argument, NULL, 'i' },
		{ "help", no_argument, NULL, '$' },	// use an invalid option character
		{ 0 }
};

void Usage_full(void)
{
	fprintf(stderr, "Usage: " NAME_PROG " [-v][-q] [-h hfi] [-p port]\n                  [-i seconds]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       " NAME_PROG " --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose level        - verbose output level (additive):\n");
	fprintf(stderr, "                                1  - Screen\n");
	fprintf(stderr, "                                4  - STDERR (" NAME_PROG ")\n");
	fprintf(stderr, "                                16 - STDERR PaClient\n");
	fprintf(stderr, "    -q/--quiet                - disable progress reports\n");
	fprintf(stderr, "    -h/--hfi hfi              - hfi, numbered 1..n, 0= -p port will be a\n");
	fprintf(stderr, "                                system wide port num (default is 0)\n");
	fprintf(stderr, "    -p/--port port            - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "                                is 1st active)\n");
	fprintf(stderr, "    -i/--interval seconds     - interval at which PA queries will be performed\n");
	fprintf(stderr, "                                to refresh to the latest PA image (default=10s) \n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");

	exit(2);
}

void Usage(void)
{
	fprintf(stderr, "Usage: " NAME_PROG " [-v][-q] [-i seconds]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       " NAME_PROG " --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose level        - verbose output level\n");
	fprintf(stderr, "    -q/--quiet                - disable progress reports\n");
	fprintf(stderr, "    -i/--interval seconds     - interval at which PA queries will be performed\n");
	fprintf(stderr, "                                to refresh to the latest PA image (default=10s)\n");
	exit(2);
}

int main(int argc, char ** argv)
{
    FSTATUS             fstatus;
    int                 c;
	int					index;
	unsigned long		temp;
	long int			temp2		= 0;
	char *endptr;
	int n_cmd;
	char tb_cmd[64];
	time_t time_start;
	int pa_service_state = OMGT_SERVICE_STATE_UNKNOWN;

	Top_setcmdname(NAME_PROG);
	g_quiet = ! isatty(2);	// disable progress if stderr is not tty

	// process command line arguments
	while (-1 != (c = getopt_long(argc,argv, "v:qh:p:i:D:X", options, &index)))
    {
        switch (c)
        {
			case '$':
				Usage_full();
				break;
            case 'v':
				errno = 0;
				temp = strtoul(optarg, &endptr, 0);
				if (temp > INT_MAX || errno || ! endptr || *endptr != '\0') {
					fprintf(stderr, "pm: Invalid Verbose Value: %s\n", optarg);
					Usage();
				}
                g_verbose = temp;
#ifdef IB_STACK_OPENIB
				//_openib_debug_ = 1;
#endif
                break;
            case 'q':
				g_quiet = 1;
				break;
            case 'h':	// hfi to issue query from
				errno = 0;
				temp = strtoul(optarg, &endptr, 0);
				if (temp > IB_UINT8_MAX || errno || ! endptr || *endptr != '\0') {
					fprintf(stderr, "pm: Invalid HFI Number: %s\n", optarg);
					Usage();
				}
				hfi = (uint8)temp;
                break;
            case 'p':	// port to issue query from
				errno = 0;
				temp = strtoul(optarg, &endptr, 0);
				if (temp > IB_UINT8_MAX || errno || ! endptr || *endptr != '\0') {
					fprintf(stderr, "pm: Invalid Port Number: %s\n", optarg);
					Usage();
				}
				port = (uint8)temp;
                break;
            case 'i':	// get performance stats over interval
				errno = 0;
				temp = strtoul(optarg, &endptr, 0);
				if (temp > INT_MAX || errno || ! endptr || *endptr != '\0') {
					fprintf(stderr, "pm: Invalid Interval: %s\n", optarg);
					Usage();
				}
                g_interval = temp;
                break;
            case 'D':	// NOTE: this option is not documented in usage
				errno = 0;
				temp = strtoul(optarg, &endptr, 0);
				if (temp > INT_MAX || errno || ! endptr || *endptr != '\0') {
					fprintf(stderr, "pm: Invalid Debug Value: %s\n", optarg);
					Usage();
				}
                g_debug = temp;
				break;
            case 'X':	// NOTE: this option is not documented in usage
				g_expr_funct = 1;
				break;
            default:
                fprintf(stderr, "pm: Invalid option -%c\n", c);
                Usage();
                break;
        }
    } /* end while */

	if (optind < argc)
	{
		Usage();
	}

	printf(NAME_PROG " Initializing...(%d seconds)\n", g_interval);
	tb_menu[0] = SCREEN_SUMMARY;
	bf_error[0] = 0;
	fb_error_displayed = FALSE;

	struct omgt_params params = {.debug_file = (g_verbose & VERBOSE_PACLIENT ? stderr : NULL)};
	if (!omgt_open_port_by_num(&g_portHandle, hfi, port, &params))
	{
		/* Port is open, check PA Service */
		fstatus = omgt_port_get_pa_service_state(g_portHandle, &pa_service_state, OMGT_REFRESH_SERVICE_BAD_STATE);
		if (fstatus != FSUCCESS || pa_service_state != OMGT_SERVICE_STATE_OPERATIONAL)
		{
			printf(NAME_PROG " PA Service Not Operational: %s (%d)\n",
				omgt_service_state_totext(pa_service_state), pa_service_state );
			g_exitstatus = 1;
			goto done;
		}
		fb_valid_pa_client = TRUE;
	}
	// Configure terminal for single character at a time input
	if (tcgetattr(fileno(stdin), &g_term_save) < 0)
	{
		printf(NAME_PROG " Unable to Get Terminal Configuration\n");
		g_exitstatus = 1;
		goto done;
	}

	g_term = g_term_save;
	g_term.c_lflag &= ~(ICANON | ECHOCTL);
	if (tcsetattr(fileno(stdin), TCSAFLUSH, &g_term) < 0)
	{
		printf(NAME_PROG " Unable to Configure Terminal\n");
		g_exitstatus = 1;
		goto quit;
	}

	while (TRUE)
	{
		time_start = time(NULL);
		ct_interval += 1;
		DisplayScreen();

		while ((time(NULL) - time_start) < g_interval)
		{
			if (get_input(0, 7, tb_cmd, p_multi_input) > 0)
			{
				// Clear displayed error
				if (fb_error_displayed)
				{
					bf_error[0] = 0;
					fb_error_displayed = FALSE;
				}

				// Process command
				if ((n_cmd = tb_cmd[0]) == 0x0A)
					;

				else if (n_cmd == '?')
				   	fb_help = TRUE;
	
				else if ((n_cmd == 'u') && (n_level_menu > 0))
				{
				   	if (tb_menu[--n_level_menu] == SCREEN_SUMMARY || tb_menu[n_level_menu] == SCREEN_VF_SUMMARY)
						g_group = -1;

				   	if ( (tb_menu[n_level_menu] == SCREEN_GROUP_CONFIG) ||
						 (tb_menu[n_level_menu] == SCREEN_GROUP_FOCUS) ||
						 (tb_menu[n_level_menu] == SCREEN_VF_CONFIG) || 
						 (tb_menu[n_level_menu] == SCREEN_VF_FOCUS) )
						g_ix_port = -1;
				}
	
				else if (toupper(n_cmd) == 'L')
				{
					if (g_imageIdFreeze.imageNumber != PACLIENT_IMAGE_CURRENT)
					{
						if ( (fstatus = omgt_pa_release_image(g_portHandle, g_imageIdFreeze)) !=
								FSUCCESS )
							snprintf( &bf_error[strlen(bf_error)], sizeof(bf_error)-strlen(bf_error),
								"UNABLE TO UNFREEZE HISTORY IMAGE fstatus:%u %s",
								fstatus, iba_fstatus_msg(fstatus) );

						g_imageIdFreeze.imageNumber = PACLIENT_IMAGE_CURRENT;
					}

					g_imageIdQuery.imageNumber = PACLIENT_IMAGE_CURRENT;
					g_offset = g_imageIdQuery.imageOffset = 0;
				}
	
				else if ((toupper(n_cmd) == 'F') || (toupper(n_cmd) == 'R'))
				{
					if (n_cmd > 'R')
						temp2 = 1;
					else
						temp2 = 5;

					if (toupper(n_cmd) == 'R')
						temp2 = -temp2;

					if (g_imageIdQuery.imageNumber == PACLIENT_IMAGE_CURRENT)
					{
						g_imageIdTemp.imageNumber = g_imageIdResp.imageNumber;
						g_imageIdTemp.imageOffset = temp2;

						if ( ( fstatus = omgt_pa_freeze_image(g_portHandle, g_imageIdTemp,
								&g_imageIdResp) ) == FSUCCESS )
						{
							g_imageIdQuery = g_imageIdFreeze = g_imageIdResp;
							g_offset = temp2;
						}

						else
							snprintf( &bf_error[strlen(bf_error)], sizeof(bf_error)-strlen(bf_error),
								"%s IMAGE NOT AVAILABLE FROM LIVE fstatus:%u %s",
								temp2 < 0 ? "REVERSE" : "FORWARD",
								fstatus, iba_fstatus_msg(fstatus) );
					}

					else if ( g_imageIdFreeze.imageNumber !=
							PACLIENT_IMAGE_CURRENT )
					{
						g_imageIdTemp.imageNumber =
							g_imageIdFreeze.imageNumber;
						g_imageIdTemp.imageOffset =
							g_imageIdFreeze.imageOffset + temp2;

						if ( ( fstatus = omgt_pa_move_image_freeze(g_portHandle, g_imageIdFreeze,
							&g_imageIdTemp) ) == FSUCCESS )
						{
							g_imageIdQuery = g_imageIdFreeze = g_imageIdTemp;
							g_offset += temp2;
						}

						else
							snprintf( &bf_error[strlen(bf_error)], sizeof(bf_error)-strlen(bf_error),
								"%s IMAGE NOT AVAILABLE FROM HIST fstatus:%u %s",
								temp2 < 0 ? "REVERSE" : "FORWARD",
								fstatus, iba_fstatus_msg(fstatus) );
					}

					else if ( g_imageIdBookmark.imageNumber !=
							PACLIENT_IMAGE_CURRENT )
					{
						g_imageIdTemp.imageNumber =
							g_imageIdBookmark.imageNumber;
						g_imageIdTemp.imageOffset = temp2;

						if ( ( fstatus = omgt_pa_freeze_image( g_portHandle, g_imageIdTemp,
								&g_imageIdResp) ) == FSUCCESS )
						{
							g_imageIdQuery = g_imageIdFreeze = g_imageIdResp;
							g_offset = temp2;
						}

						else
							snprintf( &bf_error[strlen(bf_error)], sizeof(bf_error)-strlen(bf_error),
								"%s IMAGE NOT AVAILABLE FROM BKMK fstatus:%u %s",
								temp2 < 0 ? "REVERSE" : "FORWARD",
								fstatus, iba_fstatus_msg(fstatus) );
					}

					else
						snprintf( &bf_error[strlen(bf_error)], sizeof(bf_error)-strlen(bf_error),
							"NO IMAGE FROM WHICH TO SELECT" );
				}

				else if ( (n_cmd == 'b') &&
						( g_imageIdBookmark.imageNumber !=
						PACLIENT_IMAGE_CURRENT ) )
				{
					if ( g_imageIdFreeze.imageNumber !=
							PACLIENT_IMAGE_CURRENT )
					{
						omgt_pa_release_image(g_portHandle, g_imageIdFreeze);
						g_imageIdFreeze.imageNumber = PACLIENT_IMAGE_CURRENT;
					}

					g_imageIdQuery.imageNumber = g_imageIdBookmark.imageNumber;
					g_offset = g_imageIdQuery.imageOffset = 0;
				}
	
				else if ( (n_cmd == 'B') &&
                           ( g_imageIdBookmark.imageNumber ==
						PACLIENT_IMAGE_CURRENT ) )
				{
					if (g_imageIdQuery.imageNumber == PACLIENT_IMAGE_CURRENT)
					{
						if ( ( fstatus = omgt_pa_freeze_image(g_portHandle, g_imageIdResp,
								&g_imageIdBookmark) ) == FSUCCESS )
							g_imageIdQuery.imageNumber =
								g_imageIdBookmark.imageNumber;

						else
							snprintf( &bf_error[strlen(bf_error)], sizeof(bf_error)-strlen(bf_error),
								"UNABLE TO BOOKMARK LIVE IMAGE fstatus:%u %s",
								fstatus, iba_fstatus_msg(fstatus) );
					}

					else if ( g_imageIdFreeze.imageNumber !=
							PACLIENT_IMAGE_CURRENT )
					{
						g_imageIdQuery = g_imageIdBookmark = g_imageIdFreeze;
						g_imageIdFreeze.imageNumber = PACLIENT_IMAGE_CURRENT;
					}

					else
						snprintf( &bf_error[strlen(bf_error)], sizeof(bf_error)-strlen(bf_error),
							"NO IMAGE FROM WHICH TO BOOKMARK" );

					g_offset = 0;
				}
	
				else if ( (n_cmd == 'U') &&
						( g_imageIdBookmark.imageNumber !=
						PACLIENT_IMAGE_CURRENT ) )
				{
					if (omgt_pa_release_image(g_portHandle, g_imageIdBookmark) != FSUCCESS)
						snprintf( &bf_error[strlen(bf_error)], sizeof(bf_error)-strlen(bf_error),
							"UNABLE TO UNBOOKMARK IMAGE");

					if ( ( g_imageIdQuery.imageNumber ==
							g_imageIdBookmark.imageNumber ) &&
							(g_imageIdQuery.imageOffset == 0) )
						g_imageIdQuery.imageNumber = PACLIENT_IMAGE_CURRENT;

					g_imageIdBookmark.imageNumber = PACLIENT_IMAGE_CURRENT;
				}
	
				else if (toupper(n_cmd) == 'Q')
					goto quit;
	
				if (n_level_menu == 0 && toupper(n_cmd) == 'V') {
					tb_menu[n_level_menu] = 
						tb_menu[n_level_menu] == SCREEN_VF_SUMMARY ? 
							SCREEN_SUMMARY : SCREEN_VF_SUMMARY;
					g_group = -1;
					//continue;
				}

				if (tb_menu[n_level_menu] == SCREEN_SUMMARY ||
					tb_menu[n_level_menu] == SCREEN_VF_SUMMARY)
				{
					if (toupper(n_cmd) == 'P')
						tb_menu[++n_level_menu] = SCREEN_PM_CONFIG;
	
					else if (toupper(n_cmd) == 'I')
						tb_menu[++n_level_menu] = SCREEN_IMAGE_INFO;
					else if (n_cmd == 's' && tb_menu[n_level_menu] == SCREEN_SUMMARY)
						g_scroll_summary = g_scroll_summary_forward;
					else if (n_cmd == 'S' && tb_menu[n_level_menu] == SCREEN_SUMMARY)
						g_scroll_summary = g_scroll_summary_backward;
					else if (n_cmd == 's' && tb_menu[n_level_menu] == SCREEN_VF_SUMMARY)
						g_scroll_vf_summary = MIN(g_scroll_vf_summary + MAX_VF_SUMMARY_PER_SCREEN, g_PmVFList.numVFs - 1);
					else if (n_cmd == 'S' && tb_menu[n_level_menu] == SCREEN_VF_SUMMARY)
						g_scroll_vf_summary = MAX(g_scroll_vf_summary - MAX_VF_SUMMARY_PER_SCREEN, 0);
					else if (n_cmd >= '0') 
					{
						n_cmd -= '0';

						if (tb_menu[n_level_menu] == SCREEN_SUMMARY &&
							fb_valid_group_list && 
							(g_scroll_summary + n_cmd) < g_PmGroupList.numGroups) {

							g_group = n_cmd + g_scroll_summary;
							tb_menu[++n_level_menu] = SCREEN_GROUP_INFO_SELECT;
						}
						else if (tb_menu[n_level_menu] == SCREEN_VF_SUMMARY &&
							fb_valid_VF_list &&
							(g_scroll_vf_summary + n_cmd) < g_PmVFList.numVFs) {
							
							g_vf = n_cmd + g_scroll_vf_summary;
							tb_menu[++n_level_menu] = SCREEN_VF_INFO_SELECT;
						}
					}
				}

				if (tb_menu[n_level_menu] == SCREEN_GROUP_INFO_SELECT)
				{
					if (toupper(n_cmd) == 'P')
					{
						tb_menu[++n_level_menu] = SCREEN_GROUP_BW_STATS;
						g_select = STL_PA_SELECT_UTIL_HIGH;
					}

					else if (toupper(n_cmd) == 'S')
					{
						tb_menu[++n_level_menu] = SCREEN_GROUP_CTG_STATS;
						g_select = STL_PA_SELECT_CATEGORY_INTEG;	
					}
	
					else if (toupper(n_cmd) == 'C')
					{
						g_scroll = 0;
						tb_menu[++n_level_menu] = SCREEN_GROUP_CONFIG;
					}
				}

				if (tb_menu[n_level_menu] == SCREEN_VF_INFO_SELECT) {
					if (toupper(n_cmd) == 'P') {
						tb_menu[++n_level_menu] = SCREEN_VF_BW_STATS;
						g_select = STL_PA_SELECT_UTIL_HIGH;
					} else if (toupper(n_cmd) == 'S') {
						tb_menu[++n_level_menu] = SCREEN_VF_CTG_STATS;
						g_select = STL_PA_SELECT_UTIL_HIGH;
					} else if (toupper(n_cmd) == 'C') {
						g_scroll = 0;
						tb_menu[++n_level_menu] = SCREEN_VF_CONFIG;
					}
				}
	
				if ( (tb_menu[n_level_menu] == SCREEN_GROUP_BW_STATS) ||
						(tb_menu[n_level_menu] == SCREEN_GROUP_CTG_STATS) ||
						(tb_menu[n_level_menu] == SCREEN_GROUP_FOCUS) )
				{
					if ((toupper(n_cmd) == 'I') && g_expr_funct)
						if ( (sscanf(tb_cmd + 1, "%lu", &temp) == 1) &&
								fb_valid_group_config &&
								(temp < pg_PmGroupConfig.numPorts) )
							g_start = temp;

					if (toupper(n_cmd) == 'N')
						if (sscanf(tb_cmd + 1, "%lu", &temp) == 1)
							g_range = temp;
				}

				if (tb_menu[n_level_menu] == SCREEN_VF_BW_STATS ||
					tb_menu[n_level_menu] == SCREEN_VF_CTG_STATS ||
					tb_menu[n_level_menu] == SCREEN_VF_FOCUS) {
					/*
					if ((toupper(n_cmd) == 'I') && g_expr_funct)
						if ( (sscanf(tb_cmd + 1, "%lu", &temp) == 1) &&
								fb_valid_VF_config &&
								(temp < pg_PmGroupConfig->numPorts) )
							g_start = temp;
					*/
					if (toupper(n_cmd) == 'N')
						if (sscanf(tb_cmd + 1, "%lu", &temp) == 1)
							g_range = temp;
				}
	
				if (tb_menu[n_level_menu] == SCREEN_GROUP_BW_STATS ||
					tb_menu[n_level_menu] == SCREEN_VF_BW_STATS) {
					if (n_cmd == 'c')
					{
						if (g_select == STL_PA_SELECT_UTIL_HIGH)
							g_select = STL_PA_SELECT_UTIL_PKTS_HIGH;
						else if (g_select == STL_PA_SELECT_UTIL_PKTS_HIGH)
							g_select = STL_PA_SELECT_UTIL_LOW;
// Future enhancement
#if 0
						else if (g_select == STL_PA_SELECT_UTIL_LOW)
							g_select = PACLIENT_SEL_ALL;
#endif
						else
							g_select = STL_PA_SELECT_UTIL_HIGH;
					}

					else if (n_cmd == 'C')
					{
						if (g_select == STL_PA_SELECT_UTIL_HIGH)
							g_select = STL_PA_SELECT_UTIL_LOW;
// Future enhancement
#if 0
						if (g_select == STL_PA_SELECT_UTIL_HIGH)
							g_select = PACLIENT_SEL_ALL;
						else if (g_select == PACLIENT_SEL_ALL)
							g_select = STL_PA_SELECT_UTIL_LOW;
#endif
						else if (g_select == STL_PA_SELECT_UTIL_LOW)
							g_select = STL_PA_SELECT_UTIL_PKTS_HIGH;
						else
							g_select = STL_PA_SELECT_UTIL_HIGH;
					}
				}
	
				if (tb_menu[n_level_menu] == SCREEN_GROUP_CTG_STATS ||
					tb_menu[n_level_menu] == SCREEN_VF_CTG_STATS )
				{
					if (n_cmd == 'c')
					{
						if (g_select == STL_PA_SELECT_CATEGORY_INTEG)
							g_select = STL_PA_SELECT_CATEGORY_CONG;
						else if (g_select == STL_PA_SELECT_CATEGORY_CONG)
							g_select = STL_PA_SELECT_CATEGORY_SMA_CONG;
						else if (g_select == STL_PA_SELECT_CATEGORY_SMA_CONG)
							g_select = STL_PA_SELECT_CATEGORY_BUBBLE;
						else if (g_select == STL_PA_SELECT_CATEGORY_BUBBLE)
							g_select = STL_PA_SELECT_CATEGORY_SEC;
						else if (g_select == STL_PA_SELECT_CATEGORY_SEC)
							g_select = STL_PA_SELECT_CATEGORY_ROUT;
// Future enhancement
#if 0
						else if (g_select == STL_PA_SELECT_CATEGORY_ROUT)
							g_select = PACLIENT_SEL_ALL;
#endif
						else
							g_select = STL_PA_SELECT_CATEGORY_INTEG;
					}

					else if (n_cmd == 'C')
					{
						if (g_select == STL_PA_SELECT_CATEGORY_ROUT)
							g_select = STL_PA_SELECT_CATEGORY_SEC;
						else if (g_select == STL_PA_SELECT_CATEGORY_SEC)
							g_select = STL_PA_SELECT_CATEGORY_BUBBLE;
						else if (g_select == STL_PA_SELECT_CATEGORY_BUBBLE)
							g_select = STL_PA_SELECT_CATEGORY_SMA_CONG;
						else if (g_select == STL_PA_SELECT_CATEGORY_SMA_CONG)
							g_select = STL_PA_SELECT_CATEGORY_CONG;
						else if (g_select == STL_PA_SELECT_CATEGORY_INTEG)
							g_select = STL_PA_SELECT_CATEGORY_ROUT;
// Future enhancement
#if 0
						else if (g_select == STL_PA_SELECT_CATEGORY_INTEG)
							g_select = PACLIENT_SEL_ALL;
						else if (g_select == PACLIENT_SEL_ALL)
							g_select = STL_PA_SELECT_CATEGORY_ROUT;
#endif
						else
							g_select = STL_PA_SELECT_CATEGORY_INTEG;
					}
				}

				if (tb_menu[n_level_menu] == SCREEN_GROUP_FOCUS ||
					tb_menu[n_level_menu] == SCREEN_VF_FOCUS )
				{
					if (toupper(n_cmd) == 'P')
					{
						if ( (sscanf(tb_cmd + 1, "%lu", &temp) == 1) &&
								(temp >= g_start) )
						{
							if (tb_menu[n_level_menu] == SCREEN_GROUP_FOCUS &&
								fb_valid_group_focus && 
							   (temp < g_start + pg_PmGroupFocus.numPorts) ) {
								g_ix_port = temp;
								g_portlid =
									pg_PmGroupFocus.portList[g_ix_port - g_start].nodeLid;
								g_portnum =
									pg_PmGroupFocus.portList[g_ix_port - g_start].portNumber;
								fb_port_has_neighbor = (0 != pg_PmGroupFocus.portList[g_ix_port - g_start].neighborLid);
								fb_port_neighbor = FALSE;
								tb_menu[++n_level_menu] = SCREEN_PORT_STATS;
								g_scroll_cntrs = 0;
							} else if (tb_menu[n_level_menu] == SCREEN_VF_FOCUS &&
										fb_valid_VF_focus && 
										(temp < g_start + g_pPmVFFocus.numPorts)) {
								g_ix_port = temp;
								g_portlid = 
									g_pPmVFFocus.portList[g_ix_port - g_start].nodeLid;
								g_portnum =
									g_pPmVFFocus.portList[g_ix_port - g_start].portNumber;
								fb_port_has_neighbor = (0 != g_pPmVFFocus.portList[g_ix_port - g_start].neighborLid);
								fb_port_neighbor = FALSE;
								tb_menu[++n_level_menu] = SCREEN_PORT_STATS;
								g_scroll_cntrs = 0;
							}
						}
					}

					else if (n_cmd == 'c')
					{
						if (g_select == STL_PA_SELECT_UTIL_HIGH)
							g_select = STL_PA_SELECT_UTIL_PKTS_HIGH;
						else if (g_select == STL_PA_SELECT_UTIL_PKTS_HIGH)
							g_select = STL_PA_SELECT_UTIL_LOW;
						else if (g_select == STL_PA_SELECT_UTIL_LOW)
							g_select = STL_PA_SELECT_CATEGORY_INTEG;
						else if (g_select == STL_PA_SELECT_CATEGORY_INTEG)
							g_select = STL_PA_SELECT_CATEGORY_CONG;
						else if (g_select == STL_PA_SELECT_CATEGORY_CONG)
							g_select = STL_PA_SELECT_CATEGORY_SMA_CONG;
						else if (g_select == STL_PA_SELECT_CATEGORY_SMA_CONG)
							g_select = STL_PA_SELECT_CATEGORY_BUBBLE;
						else if (g_select == STL_PA_SELECT_CATEGORY_BUBBLE)
							g_select = STL_PA_SELECT_CATEGORY_SEC;
						else if (g_select == STL_PA_SELECT_CATEGORY_SEC)
							g_select = STL_PA_SELECT_CATEGORY_ROUT;
// Future enhancement
#if 0
						else if (g_select == STL_PA_SELECT_CATEGORY_ROUT)
							g_select = PACLIENT_SEL_ALL;
#endif
						else
							g_select = STL_PA_SELECT_UTIL_HIGH;
					}

					else if (n_cmd == 'C')
					{
						if (g_select == STL_PA_SELECT_UTIL_HIGH)
							g_select = STL_PA_SELECT_CATEGORY_ROUT;
						else if (g_select == STL_PA_SELECT_CATEGORY_ROUT)
							g_select = STL_PA_SELECT_CATEGORY_SEC;
						else if (g_select == STL_PA_SELECT_CATEGORY_SEC)
							g_select = STL_PA_SELECT_CATEGORY_BUBBLE;
						else if (g_select == STL_PA_SELECT_CATEGORY_BUBBLE)
							g_select = STL_PA_SELECT_CATEGORY_SMA_CONG;
						else if (g_select == STL_PA_SELECT_CATEGORY_SMA_CONG)
							g_select = STL_PA_SELECT_CATEGORY_CONG;
						else if (g_select == STL_PA_SELECT_CATEGORY_CONG)
							g_select = STL_PA_SELECT_CATEGORY_INTEG;
						else if (g_select == STL_PA_SELECT_CATEGORY_INTEG)
							g_select = STL_PA_SELECT_UTIL_LOW;
						else if (g_select == STL_PA_SELECT_UTIL_LOW)
							g_select = STL_PA_SELECT_UTIL_PKTS_HIGH;
// Future enhancement
#if 0
						else if (g_select == STL_PA_SELECT_UTIL_HIGH)
							g_select = PACLIENT_SEL_ALL;
						else if (g_select == PACLIENT_SEL_ALL)
							g_select = STL_PA_SELECT_CATEGORY_ROUT;
#endif
						else
							g_select = STL_PA_SELECT_UTIL_HIGH;
					}
				}

				if ( (tb_menu[n_level_menu] == SCREEN_GROUP_BW_STATS) ||
						(tb_menu[n_level_menu] == SCREEN_GROUP_CTG_STATS) )
				{
					if (toupper(n_cmd) == 'D')
					{
						g_scroll = 0;
						tb_menu[++n_level_menu] = SCREEN_GROUP_FOCUS;
					}
				}
				
				if ( (tb_menu[n_level_menu] == SCREEN_VF_BW_STATS) ||
					 (tb_menu[n_level_menu] == SCREEN_VF_CTG_STATS) ) {
				
					if (toupper(n_cmd) == 'D') {
						g_scroll = 0;
						tb_menu[++n_level_menu] = SCREEN_VF_FOCUS;
					}
				}

				if (tb_menu[n_level_menu] == SCREEN_GROUP_CONFIG)
				{
					if (toupper(n_cmd) == 'P')
						if ( (sscanf(tb_cmd + 1, "%lu", &temp) == 1) &&
								fb_valid_group_config &&
								(temp < pg_PmGroupConfig.numPorts) )
						{
							g_ix_port = temp;
							g_portlid =
								pg_PmGroupConfig.portList[g_ix_port].nodeLid;
							g_portnum =
								pg_PmGroupConfig.portList[g_ix_port].portNumber;
							fb_port_has_neighbor = FALSE;
							fb_port_neighbor = FALSE;
							tb_menu[++n_level_menu] = SCREEN_PORT_STATS;
							g_scroll_cntrs = 0;
						}
				}

				if (tb_menu[n_level_menu] == SCREEN_GROUP_CONFIG)
				{
					if (n_cmd == 's' && fb_valid_group_config)
						g_scroll = MIN(g_scroll + MAX_GROUPCONFIG_PORTS_PER_SCREEN, pg_PmGroupConfig.numPorts - 1);
					else if (n_cmd == 'S')
						g_scroll = MAX(g_scroll - MAX_GROUPCONFIG_PORTS_PER_SCREEN, 0);
				}

				if (tb_menu[n_level_menu] == SCREEN_VF_CONFIG)
				{
					if (n_cmd == 's' && fb_valid_VF_config)
						g_scroll = MIN(g_scroll + MAX_VFCONFIG_PORTS_PER_SCREEN, g_pPmVFConfig.numPorts - 1);
					else if (n_cmd == 'S')
						g_scroll = MAX(g_scroll - MAX_VFCONFIG_PORTS_PER_SCREEN, 0);
				}

				if (tb_menu[n_level_menu] == SCREEN_VF_CONFIG)
				{
					if (toupper(n_cmd) == 'P')
						if ( (sscanf(tb_cmd + 1, "%lu", &temp) == 1) &&
								fb_valid_VF_config &&
								(temp < g_pPmVFConfig.numPorts) )
						{
							g_ix_port = temp;
							g_portlid =
								g_pPmVFConfig.portList[g_ix_port].nodeLid;
							g_portnum =
								g_pPmVFConfig.portList[g_ix_port].portNumber;
							fb_port_has_neighbor = FALSE;
							fb_port_neighbor = FALSE;
							tb_menu[++n_level_menu] = SCREEN_PORT_STATS;
						}
				}

				if (tb_menu[n_level_menu] == SCREEN_GROUP_FOCUS)
				{
					if (n_cmd == 's' && fb_valid_group_focus)
						g_scroll = MIN(g_scroll + MAX_GROUPFOCUS_PORTS_PER_SCREEN, pg_PmGroupFocus.numPorts - 1);
					else if (n_cmd == 'S')
						g_scroll = MAX(g_scroll - MAX_GROUPFOCUS_PORTS_PER_SCREEN, 0);
				}

				if (tb_menu[n_level_menu] == SCREEN_VF_FOCUS) {

					if (n_cmd == 's' && fb_valid_VF_focus)
						g_scroll = MIN(g_scroll + MAX_VFFOCUS_PORTS_PER_SCREEN, g_pPmVFFocus.numPorts - 1);
					else if (n_cmd == 'S')
						g_scroll = MAX(g_scroll - MAX_VFFOCUS_PORTS_PER_SCREEN, 0);
				}

				if ( (tb_menu[n_level_menu] == SCREEN_PORT_STATS) &&
						(tb_menu[n_level_menu - 1] == SCREEN_GROUP_FOCUS) )
				{
					if ((toupper(n_cmd) == 'N') && fb_valid_group_focus
						&& fb_port_has_neighbor)
					{
						if ((fb_port_neighbor = !fb_port_neighbor))
						{
							g_portlid =
								pg_PmGroupFocus.portList[g_ix_port - g_start].neighborLid;
							g_portnum =
								pg_PmGroupFocus.portList[g_ix_port - g_start].neighborPortNumber;
						}

						else
						{
							g_portlid =
								pg_PmGroupFocus.portList[g_ix_port - g_start].nodeLid;
							g_portnum =
								pg_PmGroupFocus.portList[g_ix_port - g_start].portNumber;
						}
					}
				}

				if ( (tb_menu[n_level_menu] == SCREEN_PORT_STATS) &&
						(tb_menu[n_level_menu - 1] == SCREEN_VF_FOCUS) )
				{
					if ((toupper(n_cmd) == 'N') && fb_valid_VF_focus
						&& fb_port_has_neighbor)
					{
						if ((fb_port_neighbor = !fb_port_neighbor))
						{
							g_portlid =
								g_pPmVFFocus.portList[g_ix_port - g_start].neighborLid;
							g_portnum =
								g_pPmVFFocus.portList[g_ix_port - g_start].neighborPortNumber;
						}

						else
						{
							g_portlid =
								g_pPmVFFocus.portList[g_ix_port - g_start].nodeLid;
							g_portnum =
								g_pPmVFFocus.portList[g_ix_port - g_start].portNumber;
						}
					}
				}

				if (tb_menu[n_level_menu] == SCREEN_PORT_STATS) {
					if (n_cmd == 's') {
						g_scroll_cntrs = 1;
					} else if (n_cmd == 'S') {
						g_scroll_cntrs = 0;
					}
				}

				DisplayScreen();

			}	// End of if ((temp2 = select(1, &fd_set_read, NULL, NULL
	
		}	// End of while ((time(NULL) - time_start) < g_interval)

		// Renew frozen image if present
		if (g_imageIdFreeze.imageNumber != PACLIENT_IMAGE_CURRENT)
			omgt_pa_renew_image(g_portHandle, g_imageIdFreeze);

		// Renew bookmark image if present
		if (g_imageIdBookmark.imageNumber != PACLIENT_IMAGE_CURRENT)
			omgt_pa_renew_image(g_portHandle, g_imageIdBookmark);
	
	}	// End of while (TRUE)

quit:
	// release any frozen image
	if (g_imageIdFreeze.imageNumber != PACLIENT_IMAGE_CURRENT)
		omgt_pa_release_image(g_portHandle, g_imageIdFreeze);

	// release any bookmark image
	if (g_imageIdBookmark.imageNumber != PACLIENT_IMAGE_CURRENT)
		omgt_pa_release_image(g_portHandle, g_imageIdBookmark);

	// restore terminal settings
	tcsetattr(fileno(stdin), TCSAFLUSH, &g_term_save);

done:
	if (g_portHandle) omgt_close_port(g_portHandle);
	g_portHandle = NULL;

	if (g_exitstatus == 2)
		Usage();

	return g_exitstatus;
}
