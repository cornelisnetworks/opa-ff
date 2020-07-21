/* BEGIN_ICS_COPYRIGHT7 ****************************************

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

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#define _GNU_SOURCE
#include <getopt.h>

#include "topology.h"
#ifdef IB_STACK_OPENIB
#include <opamgt_sa_priv.h>
#endif
#include "ib_status.h"
#include "stl_mad_priv.h"
#include "iba/stl_pa_priv.h"
#include "opamgt_priv.h"
#include "opamgt_pa_priv.h"
#include "opamgt_pa.h"
#include "stl_print.h"
#include <infiniband/umad.h>

#define					Q_GETGROUPLIST			1
#define					Q_GETGROUPINFO			2
#define					Q_GETGROUPCONFIG		3
#define					Q_GETPORTCOUNTERS		4
#define					Q_CLRPORTCOUNTERS		5
#define					Q_CLRALLPORTCOUNTERS	6
#define					Q_GETPMCONFIG			7
#define					Q_FREEZEIMAGE			8
#define					Q_RELEASEIMAGE			9
#define					Q_RENEWIMAGE			10
#define					Q_GETFOCUSPORTS			11
#define					Q_GETIMAGECONFIG		12
#define					Q_MOVEFREEZE			13
#define					Q_CLASSPORTINFO			14
#define					Q_GETVFLIST				15
#define					Q_GETVFINFO				16
#define					Q_GETVFCONFIG			17
#define					Q_GETVFPORTCOUNTERS		18
#define					Q_CLRVFPORTCOUNTERS		19
#define					Q_GETVFFOCUSPORTS		20
#define					Q_GETGROUPNODEINFO		21
#define					Q_GETGROUPLINKINFO		22
#define					Q_MAXQUERY				22

#define					MAX_VFABRIC_NAME		64		// from fm_xml.h

int						g_verbose 			= 0;
STL_LID					g_nodeLid			= 0;
uint8					g_portNumber		= 0;
uint64					g_nodeGuid		= 0;
uint32					g_deltaFlag			= 0;
uint32					g_userCntrsFlag		= 0;
uint32					g_selectFlag		= 0;
uint64					g_imageNumber		= 0;
int32					g_imageOffset		= 0;
uint32					g_imageTime			= 0;
uint32					g_beginTime			= 0;
uint32					g_endTime			= 0;
uint64					g_moveImageNumber	= 0;
int32					g_moveImageOffset	= 0;
int32					g_focus				= 0;
int32					g_start				= 0;
int32					g_range				= 0;
STL_FOCUS_PORT_TUPLE			tuple[MAX_NUM_FOCUS_PORT_TUPLES];
uint8					tupleID				= 0;
uint8					g_logicalOperator		= 0;
uint32					g_gotOutput			= 0;
uint32					g_gotGroup			= 0;
uint32					g_gotLid			= 0;
uint32					g_gotPort			= 0;
uint32					g_gotGUID			= 0;
uint32					g_gotSelect			= 0;
uint32					g_gotImgNum			= 0;
uint32					g_gotImgOff			= 0;
uint32					g_gotImgTime		= 0;
uint32					g_gotBegTime		= 0;
uint32					g_gotEndTime		= 0;
uint32					g_gotMoveImgNum		= 0;
uint32					g_gotMoveImgOff		= 0;
uint32					g_gotFocus			= 0;
uint32					g_gotStart			= 0;
uint32					g_gotRange			= 0;
uint32					g_gotLogicalOperator		= 0;
uint32					g_gotTuple			= 0;
uint32					g_gotvfName			= 0;
uint32					g_gotNodeDesc			= 0;
char					g_groupName[STL_PM_GROUPNAMELEN];
char					g_nodeDesc[STL_PM_NODEDESCLEN];
char					g_vfName[MAX_VFABRIC_NAME];
void					*g_SDHandle			= NULL;
PrintDest_t				g_dest;
struct omgt_port 		*g_portHandle		= NULL;
STL_PA_PM_CFG_DATA g_pmConfigData;
boolean g_valid_pm_config_exists = FALSE;


char *g_oob_address = "127.0.0.1";
uint16 g_oob_port = 3245;
char *g_sslParmsFile = "/etc/opa/opamgt_tls.xml";
struct omgt_ssl_params g_ssl_params = {0};
IB_GID g_src_gid = {{0}};
boolean g_isOOB = FALSE;
boolean g_isFEESM = FALSE;
boolean g_isOOBSSL = FALSE;

struct option options[] = {
		// basic controls
		{ "verbose", no_argument, NULL, 'v' },
		{ "hfi", required_argument, NULL, 'h' },
		{ "oob", required_argument, NULL, 'b' },
		{ "port", required_argument, NULL, 'p' },
		{ "source-gid", required_argument, NULL, 'x' },
		{ "feEsm", no_argument, NULL, 'E' },
		{ "ssl-params", required_argument, NULL, 'T' },

		// query data
		{ "output", required_argument, NULL, 'o' },
		{ "groupName", required_argument, NULL, 'g' },
		{ "lid", required_argument, NULL, 'l' },
		{ "portNumber", required_argument, NULL, 'P' },
		{ "nodeGuid", required_argument, NULL, 'G' },
		{ "nodeDesc", required_argument, NULL, 'D' },
		{ "delta", required_argument, NULL, 'd' },
		{ "begin", required_argument, NULL, 'j'},
		{ "end", required_argument, NULL, 'q'},
		{ "userCntrs", no_argument, NULL, 'U' },
		{ "select", required_argument, NULL, 's' },
		{ "imgNum", required_argument, NULL, 'n' },
		{ "imgOff", required_argument, NULL, 'O' },
		{ "imgTime", required_argument, NULL, 'y'},
		{ "moveImgNum", required_argument, NULL, 'm' },
		{ "moveImgOff", required_argument, NULL, 'M' },
		{ "focus", required_argument, NULL, 'f' },
		{ "start", required_argument, NULL, 'S' },
		{ "range", required_argument, NULL, 'r' },
		{ "operator", required_argument, NULL, 'Z'},
		{ "tuple", required_argument, NULL, 'Q'},
		{ "vfName", required_argument, NULL, 'V'},
		{ "timeout", required_argument, NULL, '!'},
		{ "help", no_argument, NULL, '$' },
		{ 0 }
};

int getQueryType(char *outputString)
{
	int q = 0;

	if (strcasecmp(outputString, "groupList") == 0)
		q = Q_GETGROUPLIST;
	else if (strcasecmp(outputString, "groupInfo") == 0)
		q = Q_GETGROUPINFO;
	else if (strcasecmp(outputString, "groupConfig") == 0)
		q = Q_GETGROUPCONFIG;
	else if (strcasecmp(outputString, "portCounters") == 0)
		q = Q_GETPORTCOUNTERS;
	else if (strcasecmp(outputString, "clrPortCounters") == 0)
		q = Q_CLRPORTCOUNTERS;
	else if (strcasecmp(outputString, "clrAllPortCounters") == 0)
		q = Q_CLRALLPORTCOUNTERS;
	else if (strcasecmp(outputString, "pmConfig") == 0)
		q = Q_GETPMCONFIG;
	else if (strcasecmp(outputString, "freezeImage") == 0)
		q = Q_FREEZEIMAGE;
	else if (strcasecmp(outputString, "releaseImage") == 0)
		q = Q_RELEASEIMAGE;
	else if (strcasecmp(outputString, "renewImage") == 0)
		q = Q_RENEWIMAGE;
	else if (strcasecmp(outputString, "moveFreeze") == 0)
		q = Q_MOVEFREEZE;
	else if (strcasecmp(outputString, "focusPorts") == 0)
		q = Q_GETFOCUSPORTS;
	else if (strcasecmp(outputString, "imageInfo") == 0)
		q = Q_GETIMAGECONFIG;
	else if (strcasecmp(outputString, "classPortInfo") == 0)
		q = Q_CLASSPORTINFO;
	else if (strcasecmp(outputString, "vfList") == 0)
		q = Q_GETVFLIST;
	else if (strcasecmp(outputString, "vfInfo") == 0)
		q = Q_GETVFINFO;
	else if (strcasecmp(outputString, "vfConfig") == 0)
		q = Q_GETVFCONFIG;
	else if (strcasecmp(outputString, "vfPortCounters") == 0)
		q = Q_GETVFPORTCOUNTERS;
	else if (strcasecmp(outputString, "clrVfPortCounters") == 0)
		q = Q_CLRVFPORTCOUNTERS;
	else if (strcasecmp(outputString, "vfFocusPorts") == 0)
		q = Q_GETVFFOCUSPORTS;
	else if (strcasecmp(outputString, "groupNodeInfo") == 0)
		q = Q_GETGROUPNODEINFO;
	else if (strcasecmp(outputString, "groupLinkInfo") == 0)
		q = Q_GETGROUPLINKINFO;
	return(q);
}

static STL_CLASS_PORT_INFO * GetClassPortInfo(struct omgt_port *port)
{
	STL_CLASS_PORT_INFO		*response = NULL;

    if (omgt_pa_get_classportinfo(port, &response) != FSUCCESS) {
        fprintf(stderr, "Failed to receive GetClassPortInfo response: %s\n", iba_pa_mad_status_msg(port));
		response = NULL;
    }
	
    return response;
}

static FSTATUS GetPortCounters(struct omgt_port *port, STL_LID nodeLid, uint8_t portNumber, uint32_t deltaFlag, uint32_t userCntrsFlag, uint64 imageNumber, int32 imageOffset, uint32 begin, uint32 end)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
    STL_PORT_COUNTERS_DATA		*response = NULL;

	fprintf(stderr, "Getting Port Counters...\n");
	imageId.imageNumber = end ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = end;

	if (begin){
		STL_PA_IMAGE_ID_DATA beginQuery = {0}, endQuery = {0};

		beginQuery.imageNumber = PACLIENT_IMAGE_TIMED;
		beginQuery.imageTime.absoluteTime = begin;

		endQuery.imageNumber = end ? PACLIENT_IMAGE_TIMED : PACLIENT_IMAGE_CURRENT;
		endQuery.imageTime.absoluteTime = end;

		STL_PORT_COUNTERS_DATA *beginCounters = NULL, *endCounters = NULL;

		if ((beginCounters = iba_pa_single_mad_port_counters_response_query(port, nodeLid, portNumber, deltaFlag, userCntrsFlag, &beginQuery)) != NULL
				&& (endCounters = iba_pa_single_mad_port_counters_response_query(port, nodeLid, portNumber, deltaFlag, userCntrsFlag, &endQuery)) != NULL){

			CounterSelectMask_t clearedCounters = DiffPACounters(endCounters, beginCounters, endCounters);
			if (clearedCounters.AsReg32){
				char counterBuf[128];
				FormatStlCounterSelectMask(counterBuf, clearedCounters);
				fprintf(stderr, "Counters reset, reporting latest count: %s\n", counterBuf);
			}
			status = FSUCCESS;
			PrintStlPAPortCounters(&g_dest, 0, endCounters, (uint32)nodeLid, (uint32)portNumber, 0);
		} else{
        	fprintf(stderr, "Failed to receive GetPortCounters response: %s\n", iba_pa_mad_status_msg(port));
		}

		if (endCounters)
			MemoryDeallocate(endCounters);
		if (beginCounters)
			MemoryDeallocate(beginCounters);
	} else if ((response = iba_pa_single_mad_port_counters_response_query(port, nodeLid, portNumber, deltaFlag, userCntrsFlag, &imageId)) != NULL) {
        status = FSUCCESS;
		PrintStlPAPortCounters(&g_dest, 0, response, nodeLid, (uint32)portNumber, response->flags);
    } else {
        fprintf(stderr, "Failed to receive GetPortCounters response: %s\n", iba_pa_mad_status_msg(port));
    }

	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS ClrPortCounters(struct omgt_port *port, STL_LID nodeLid, uint8_t portNumber, uint32_t selectFlag)
{
	FSTATUS					status= FERROR;
    STL_CLR_PORT_COUNTERS_DATA	*response;

    fprintf(stderr, "Clearing Port Counters...\n");
    if ((response = iba_pa_single_mad_clr_port_counters_response_query(port, nodeLid, portNumber, selectFlag)) != NULL) {
        status = FSUCCESS;
		fprintf(stderr, "Port Counters successfully cleared for node %d port number %d select = 0x%04x\n", nodeLid, portNumber, selectFlag);
    } else {
        fprintf(stderr, "Failed to receive ClrPortCounters response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS ClrAllPortCounters(struct omgt_port *port, uint32_t selectFlag)
{
	FSTATUS					status= FERROR;
    STL_CLR_ALL_PORT_COUNTERS_DATA	*response;

    fprintf(stderr, "Clearing All Port Counters...\n");
    if ((response = iba_pa_single_mad_clr_all_port_counters_response_query(port, selectFlag)) != NULL) {
        status = FSUCCESS;
		fprintf(stderr, "All Port Counters successfully cleared for select = 0x%04x\n", selectFlag);
    } else {
        fprintf(stderr, "Failed to receive ClrAllPortCounters response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS GetPMConfig(struct omgt_port *port, STL_PA_PM_CFG_DATA *pmConfigData)
{
	FSTATUS					status= FERROR;

	fprintf(stderr, "Getting PM Configuration...\n");
	memset(pmConfigData,0, sizeof(STL_PA_PM_CFG_DATA));
	if ((status = omgt_pa_get_pm_config(port, pmConfigData)) != FSUCCESS) {
		fprintf(stderr, "Failed to receive GetPMConfig response: %s\n", iba_pa_mad_status_msg(port));
	}
	return status;
}

static FSTATUS FreezeImage(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			request = {0};
    STL_PA_IMAGE_ID_DATA			*response;

    fprintf(stderr, "Freezing image...\n");
	request.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	request.imageOffset = imageOffset;
	request.imageTime.absoluteTime = imageTime;
    if ((response = iba_pa_single_mad_freeze_image_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive FreezeImage response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS ReleaseImage(struct omgt_port *port, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			request = {0};
    STL_PA_IMAGE_ID_DATA			*response;

    fprintf(stderr, "Releasing image...\n");
	request.imageNumber = imageNumber;
	request.imageOffset = imageOffset;
    if ((response = iba_pa_single_mad_release_image_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive ReleaseImage response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS RenewImage(struct omgt_port *port, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			request = {0};
    STL_PA_IMAGE_ID_DATA			*response;

    fprintf(stderr, "Renewing image...\n");
	request.imageNumber = imageNumber;
	request.imageOffset = imageOffset;
    if ((response = iba_pa_single_mad_renew_image_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive RenewImage response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS MoveFreeze(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint64 moveImageNumber, int32 moveImageOffset)
{
	FSTATUS					status= FERROR;
    STL_MOVE_FREEZE_DATA		request;
    STL_MOVE_FREEZE_DATA		*response;

    fprintf(stderr, "Moving freeze image...\n");
	request.oldFreezeImage.imageNumber = imageNumber;
	request.oldFreezeImage.imageOffset = imageOffset;
	request.newFreezeImage.imageNumber = moveImageNumber;
	request.newFreezeImage.imageOffset = moveImageOffset;
    if ((response = iba_pa_single_mad_move_freeze_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAMoveFreeze(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive MoveFreeze response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS GetGroupList(struct omgt_port *port)
{
	OMGT_QUERY				query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group List...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
				   			iba_sd_query_input_type_msg(query.InputType),
					   		iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_list_response_query(port, &query, &pQueryResults);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA GroupList query Failed: %s (%s) \n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA GroupList query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
			   	pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group List Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_LIST_RESULTS *p = (STL_PA_GROUP_LIST_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
					   					iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
        	fprintf(stderr, "PA Multiple MAD Response for Group Data:\n");
		}

		PrintStlPAGroupList(&g_dest, 1, p->NumGroupListRecords, p->GroupListRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}
static FSTATUS GetGroupList2(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {0};

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group List...\n");

	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;

	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_sd_query_input_type_msg(query.InputType),
			iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_list2_response_query(port, &query, &imageId, &pQueryResults);

	if (!pQueryResults) {
		fprintf(stderr, "%*sPA GroupList query Failed: %s (%s) \n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA GroupList query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status),
			pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group List Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_LIST2_RESULTS *p = (STL_PA_GROUP_LIST2_RESULTS *)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
				iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Group Data:\n");
		}

		PrintStlPAGroupList2(&g_dest, 1, p->NumGroupList2Records, p->GroupList2Records);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}
static FSTATUS GetGroupInfo(struct omgt_port *port, char *groupName, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY				query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA	imageId = {0};

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group Info...\n");

	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;

	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
				   			iba_sd_query_input_type_msg(query.InputType),
					   		iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_stats_response_query(port, &query, groupName, &pQueryResults, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Group Info query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Group Info query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
			   	pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group Info Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_INFO_RESULTS *p = (STL_PA_GROUP_INFO_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
					   					iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
        	fprintf(stderr, "PA Multiple MAD Response for Group Info group name %s:\n", groupName);
		}

		PrintStlPAGroupInfo(&g_dest, 1, p->GroupInfoRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

static FSTATUS GetGroupConfig(struct omgt_port *port, char *groupName, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY				query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group Config...\n");

	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
							iba_sd_query_input_type_msg(query.InputType),
							iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_config_response_query(port, &query, groupName, &pQueryResults, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Group Config query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Group Config query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
				pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group Config Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_CONFIG_RESULTS *p = (STL_PA_GROUP_CONFIG_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Group Config group name %s:\n", groupName);
		}

		PrintStlPAGroupConfig(&g_dest, 1, groupName, p->NumGroupConfigRecords, p->GroupConfigRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

static FSTATUS GetGroupNodeInfo(struct omgt_port *port, char *groupName, STL_LID nodeLid, uint64 nodeGuid, char *nodeDesc, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY				query;
	STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group NodeInfo...\n");

	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n", iba_sd_query_input_type_msg(query.InputType),
			iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_nodeinfo_response_query(port, &query, groupName, nodeLid, nodeGuid, nodeDesc, &pQueryResults, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Group NodeInfo query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Group NodeInfo query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
				pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group NodeInfo Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_NODEINFO_RESULTS *p = (STL_PA_GROUP_NODEINFO_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Group NodeInfo name %s:\n", groupName);
		}

		PrintStlPAGroupNodeInfo(&g_dest, 1, groupName, p->NumGroupNodeInfoRecords, p->GroupNodeInfoRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

static FSTATUS GetGroupLinkInfo(struct omgt_port *port, char *groupName,  STL_LID inputLid, uint8 inputPort, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY				query;
	STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group LinkInfo...\n");

	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n", iba_sd_query_input_type_msg(query.InputType),
			iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_linkinfo_response_query(port, &query, groupName, inputLid, inputPort, &pQueryResults, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Group LinkInfo query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Group LinkInfo query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status),
			pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group LinkInfo Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_LINKINFO_RESULTS *p = (STL_PA_GROUP_LINKINFO_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
				iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Group LinkInfo name %s:\n", groupName);
		}

		PrintStlPAGroupLinkInfo(&g_dest, 1, groupName, p->NumGroupLinkInfoRecords, p->GroupLinkInfoRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

static FSTATUS GetFocusPorts(struct omgt_port *port, char *groupName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset, uint32 imageTime, STL_FOCUS_PORT_TUPLE *tuple, uint8 operator)
{
	OMGT_QUERY				query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Focus Ports...\n");
	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
							iba_sd_query_input_type_msg(query.InputType),
							iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	if (tuple == NULL)
		status = iba_pa_multi_mad_focus_ports_response_query(port, &query, groupName, select, start, range, &pQueryResults, &imageId);
	else
		status = iba_pa_multi_mad_focus_ports_multiselect_response_query(port, &query, groupName, start, range, &pQueryResults, &imageId, tuple, operator);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Focus Ports query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Focus Ports query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
				pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Focus Ports Records Returned\n", 0, "");
	} else {
		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Focus Ports group name %s:\n", groupName);
		}

		if (tuple == NULL) {
			STL_PA_FOCUS_PORTS_RESULTS *p = (STL_PA_FOCUS_PORTS_RESULTS*)pQueryResults->QueryResult;
			PrintStlPAFocusPorts(&g_dest, 1, groupName, p->NumFocusPortsRecords, select, start, range, p->FocusPortsRecords);
		} else {
			STL_PA_FOCUS_PORTS_MULTISELECT_RESULTS *p = (STL_PA_FOCUS_PORTS_MULTISELECT_RESULTS*)pQueryResults->QueryResult;
			PrintStlPAFocusPortsMultiSelect(&g_dest, 1, groupName, p->NumFocusPortsRecords, start, range, p->FocusPortsRecords, operator, tuple);
		}
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

static FSTATUS GetImageInfo(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_INFO_DATA		request = {{0}};
    STL_PA_IMAGE_INFO_DATA		*response;

    fprintf(stderr, "Getting image info...\n");
	request.imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	request.imageId.imageOffset = imageOffset;
	request.imageId.imageTime.absoluteTime = imageTime;
	if ((response = iba_pa_multi_mad_get_image_info_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAImageInfo(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive GetImageInfo response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS GetVFList(struct omgt_port *port)
{
	OMGT_QUERY				query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF List...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
				   			iba_sd_query_input_type_msg(query.InputType),
					   		iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_list_response_query(port, &query, &pQueryResults);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA vfList query Failed: %s (%s) \n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA vfList query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
			   	pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF List Records Returned\n", 0, "");
	} else {
		STL_PA_VF_LIST_RESULTS *p = (STL_PA_VF_LIST_RESULTS *)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
					   					iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
        	fprintf(stderr, "PA Multiple MAD Response for VF list data:\n");
		}

		PrintStlPAVFList(&g_dest, 1, p->NumVFListRecords, p->VFListRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}
static FSTATUS GetVFList2(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA imageId = { 0 };

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF List...\n");
	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_sd_query_input_type_msg(query.InputType),
			iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_list2_response_query(port, &query, &imageId, &pQueryResults);

	if (!pQueryResults) {
		fprintf(stderr, "%*sPA vfList query Failed: %s (%s) \n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA vfList query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status),
			pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF List Records Returned\n", 0, "");
	} else {
		STL_PA_VF_LIST2_RESULTS *p = (STL_PA_VF_LIST2_RESULTS *)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
				iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for VF list data:\n");
		}

		PrintStlPAVFList2(&g_dest, 1, p->NumVFList2Records, p->VFList2Records);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

static FSTATUS GetVFInfo(struct omgt_port *port, char *vfName, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY				query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA	imageId = {0};

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Info...\n");
	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
				   			iba_sd_query_input_type_msg(query.InputType),
					   		iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_info_response_query(port, &query, vfName, &pQueryResults, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA VF Info query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA VF Info query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
			   	pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Info Records Returned\n", 0, "");
	} else {
		STL_PA_VF_INFO_RESULTS *p = (STL_PA_VF_INFO_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
					   					iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
        	fprintf(stderr, "PA Multiple MAD Response for VF Info VF name %s:\n", vfName);
		}

		PrintStlPAVFInfo(&g_dest, 1, p->VFInfoRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

static FSTATUS GetVFConfig(struct omgt_port *port, char *vfName, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY				query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Config...\n");
	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
							iba_sd_query_input_type_msg(query.InputType),
							iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_config_response_query(port, &query, vfName, &pQueryResults, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA VF Config query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA VF Config query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
				pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Config Records Returned\n", 0, "");
	} else {
		STL_PA_VF_CONFIG_RESULTS *p = (STL_PA_VF_CONFIG_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for VF Config vf name %s:\n", vfName);
		}

		PrintStlPAVFConfig(&g_dest, 1, vfName, p->NumVFConfigRecords, p->VFConfigRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

static FSTATUS GetVFPortCounters(struct omgt_port *port, STL_LID nodeLid, uint8_t portNumber, uint32_t deltaFlag, uint32_t userCntrsFlag, char *vfName, uint64 imageNumber, int32 imageOffset, uint32 begin, uint32 end)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
    STL_PA_VF_PORT_COUNTERS_DATA		*response = NULL;

    fprintf(stderr, "Getting Port Counters...\n");
	imageId.imageNumber = end ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = end;

	if (begin){
		STL_PA_IMAGE_ID_DATA beginQuery = {0}, endQuery = {0};

		beginQuery.imageNumber = PACLIENT_IMAGE_TIMED;
		beginQuery.imageTime.absoluteTime = begin;

		endQuery.imageNumber = end ? PACLIENT_IMAGE_TIMED : PACLIENT_IMAGE_CURRENT;
		endQuery.imageTime.absoluteTime = end;

		STL_PA_VF_PORT_COUNTERS_DATA *beginCounters = NULL, *endCounters = NULL;

		if ((beginCounters = iba_pa_single_mad_vf_port_counters_response_query(port, nodeLid, portNumber, 0, 0, vfName, &beginQuery)) != NULL
				&& (endCounters = iba_pa_single_mad_vf_port_counters_response_query(port, nodeLid, portNumber, 0, 0, vfName, &endQuery)) != NULL){

			CounterSelectMask_t clearedCounters = DiffPAVFCounters(endCounters, beginCounters, endCounters);
			if (clearedCounters.AsReg32){
				char counterBuf[128];
				FormatStlCounterSelectMask(counterBuf, clearedCounters);
				fprintf(stderr, "Counters reset, reporting latest count: %s", counterBuf);
			}
			status = FSUCCESS;
			PrintStlPAVFPortCounters(&g_dest, 0, endCounters, (uint32)nodeLid, (uint32)portNumber, 0);
		} else{
        	fprintf(stderr, "Failed to receive GetVFPortCounters response: %s\n", iba_pa_mad_status_msg(port));
		}

		if (endCounters)
		    MemoryDeallocate(endCounters);
	    if (beginCounters)
		    MemoryDeallocate(beginCounters);
     }else if ((response = iba_pa_single_mad_vf_port_counters_response_query(port, nodeLid, portNumber, deltaFlag, userCntrsFlag, vfName, &imageId)) != NULL) {
        status = FSUCCESS;
		PrintStlPAVFPortCounters(&g_dest, 0, response, nodeLid, (uint32)portNumber, response->flags);
    } else {
        fprintf(stderr, "Failed to receive GetPortCounters response: %s\n", iba_pa_mad_status_msg(port));
    }

	if (response)
		MemoryDeallocate(response);
	return status;
}

static FSTATUS ClrVFPortCounters(struct omgt_port *port, uint32_t nodeLid, uint8_t portNumber, uint32_t selectFlag, char *vfName)
{
	FSTATUS					status= FERROR;
    STL_PA_CLEAR_VF_PORT_COUNTERS_DATA	*response;

    fprintf(stderr, "Clearing VF Port Counters...\n");
    if ((response = iba_pa_single_mad_clr_vf_port_counters_response_query(port, nodeLid, portNumber, selectFlag, vfName)) != NULL) {
        status = FSUCCESS;
		fprintf(stderr, "VF Port Counters successfully cleared for vf %s node %d port number %d select = 0x%04x\n", vfName, nodeLid, portNumber, selectFlag);
    } else {
        fprintf(stderr, "Failed to receive ClrVFPortCounters response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

static FSTATUS GetVFFocusPorts(struct omgt_port *port, char *vfName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY				query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Focus Ports...\n");
	imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber;
	imageId.imageOffset = imageOffset;
	imageId.imageTime.absoluteTime = imageTime;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
							iba_sd_query_input_type_msg(query.InputType),
							iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_focus_ports_response_query(port, &query, vfName, select, start, range, &pQueryResults, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA VF Focus Ports query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA VF Focus Ports query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
				pQueryResults->MadStatus, iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Focus Ports Records Returned\n", 0, "");
	} else {
		STL_PA_VF_FOCUS_PORTS_RESULTS *p = (STL_PA_VF_FOCUS_PORTS_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for VF Focus Ports VF name %s:\n", vfName);
		}

		PrintStlPAVFFocusPorts(&g_dest, 1, vfName, p->NumVFFocusPortsRecords, select, start, range, p->FocusPortsRecords);
	}

	status = FSUCCESS;

done:
	// iba_sd_query_port_fabric_info will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		omgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}


void usage(void)
{
	fprintf(stderr, "Usage: opapaquery [-v] [-h hfi|-b addr] [-p port] [-x gid] [-E] [-T ssl_params]\n"
					"                  -o type [-g groupName] [-l nodeLid] [-P portNumber]\n"
					"                  [-G nodeGuid] [-D nodeDesc] [-d delta] \n"
					"                  [-j date_time] [-q date_time] [-U] [-s select]\n"
					"                  [-d delta] [-j date_time] [-q date_time] [-U] [-s select]\n"
					"                  [-f focus] [-S start] [-r range] [-n imgNum] [-O imgOff]\n"
					"                  [-y imgTime] [-m moveImgNum] [-M moveImgOff] [-V vfName]\n");
	fprintf(stderr, "    --help             - display this help text\n");
	fprintf(stderr, "    -v/--verbose       - verbose output\n");
	fprintf(stderr, "    -h/--hfi hfi       - hfi, numbered 1..n, 0= -p port will be a system wide\n");
	fprintf(stderr, "                         port num (default is 0)\n");
	fprintf(stderr, "    -b/--oob address   - Out-of-Band address of node running the FE. Can be\n"
					"                         either hostname, IPv4, or IPv6 address.\n"
					"                         (default is \"%s\"\n", g_oob_address);
	fprintf(stderr, "    -p/--port port     - port, \n"
					"                         In-band: numbered 1..n, 0=1st active (default\n"
					"                         is 1st active)\n"
					"                         Out-of-band: Port FE is listening on (default\n"
					"                         is %u)\n", g_oob_port);
	fprintf(stderr, "    -x/--source-gid src_gid  - Source GID of the local GID [required for most\n");
	fprintf(stderr, "                               Path and Trace Record Queries when Out-of-Band]\n");
	fprintf(stderr, "    -E/--feEsm               - ESM FE\n");
	fprintf(stderr, "    -T/--ssl-params file     - SSL/TLS parameters XML file\n"
					"                               (default is \"%s\")\n", g_sslParmsFile);
	fprintf(stderr, "    -o/--output        - output type, default is groupList\n");
	fprintf(stderr, "    -g/--groupName     - group name for groupInfo query\n");
	fprintf(stderr, "    -l/--lid           - lid of node\n");
	fprintf(stderr, "    -P/--portNumber    - port number within node\n");
	fprintf(stderr, "    --timeout          - timeout(response wait time) in ms, default is 1000ms\n");
	fprintf(stderr, "    -G/--nodeGuid      - node GUID\n");
	fprintf(stderr, "    -D/--nodeDesc      - node Description\n");
	fprintf(stderr, "    -d/--delta         - delta flag for portCounters query - 0 or 1\n");
	fprintf(stderr, "    -j/--begin date_time - obtain portCounters over interval\n");
	fprintf(stderr, "                           beginning at date_time.\n");
	fprintf(stderr, "    -q/--end date_time   - obtain portCounters over interval\n");
	fprintf(stderr, "                           ending at date_time.\n");
	fprintf(stderr, "                           For both -j and -q, date_time may be a time\n");
	fprintf(stderr, "                           entered as HH:MM[:SS] or date as mm/dd/YYYY,\n");
	fprintf(stderr, "                           dd.mm.YYYY, YYYY-mm-dd or date followed by time\n");
	fprintf(stderr, "                           e.g. \"2016-07-04 14:40\". Relative times are\n");
	fprintf(stderr, "                           taken as \"x [second|minute|hour|day](s) ago.\"\n");
	fprintf(stderr, "    -U/--userCntrs     - user controlled counters flag for portCounters query\n");
	fprintf(stderr, "    -s/--select        - 32-bit select flag for clearing port counters\n");
	fprintf(stderr, "         select bits for clrPortCounters (0 is least significant (rightmost))\n");
	fprintf(stderr, "           mask        bit location \n");
	fprintf(stderr, "           0x80000000  31     Xmit Data\n");
	fprintf(stderr, "           0x40000000  30     Rcv Data\n");
	fprintf(stderr, "           0x20000000  29     Xmit Pkts\n");
	fprintf(stderr, "           0x10000000  28     Rcv Pkts\n");
	fprintf(stderr, "           0x08000000  27     Multicast Xmit Pkts\n");
	fprintf(stderr, "           0x04000000  26     Multicast Rcv Pkts\n");
	fprintf(stderr, "           0x02000000  25     Xmit Wait\n");
	fprintf(stderr, "           0x01000000  24     Congestion Discards\n");
	fprintf(stderr, "           0x00800000  23     Rcv FECN\n");
	fprintf(stderr, "           0x00400000  22     Rcv BECN\n");
	fprintf(stderr, "           0x00200000  21     Xmit Time Cong.\n");
	fprintf(stderr, "           0x00100000  20     Xmit Time Wasted BW\n");
	fprintf(stderr, "           0x00080000  19     Xmit Time Wait Data\n");
	fprintf(stderr, "           0x00040000  18     Rcv Bubble\n");
	fprintf(stderr, "           0x00020000  17     Mark FECN\n");
	fprintf(stderr, "           0x00010000  16     Rcv Constraint Errors\n");
	fprintf(stderr, "           0x00008000  15     Rcv Switch Relay\n");
	fprintf(stderr, "           0x00004000  14     Xmit Discards\n");
	fprintf(stderr, "           0x00002000  13     Xmit Constraint Errors\n");
	fprintf(stderr, "           0x00001000  12     Rcv Rmt Phys. Errors\n");
	fprintf(stderr, "           0x00000800  11     Local Link Integrity\n");
	fprintf(stderr, "           0x00000400  10     Rcv Errors\n");
	fprintf(stderr, "           0x00000200   9     Exc. Buffer Overrun\n");
	fprintf(stderr, "           0x00000100   8     FM Config Errors\n");
	fprintf(stderr, "           0x00000080   7     Link Error Recovery\n");
	fprintf(stderr, "           0x00000040   6     Link Error Downed\n");
	fprintf(stderr, "           0x00000020   5     Uncorrectable Errors\n");
	fprintf(stderr, " \n");
 	fprintf(stderr, "         select bits for clrVfPortCounters (0 is least signficant (rightmost))\n");
	fprintf(stderr, "           mask        bit location \n");
	fprintf(stderr, "           0x80000000  31     VLXmitData \n");
	fprintf(stderr, "           0x40000000  30     VLRcvData \n");
	fprintf(stderr, "           0x20000000  29     VLXmitPkts \n");
	fprintf(stderr, "           0x10000000  28     VLRcvPkts \n");
	fprintf(stderr, "           0x08000000  27     VLXmitDiscards \n");
	fprintf(stderr, "           0x04000000  26     VLCongDiscards\n");
	fprintf(stderr, "           0x02000000  25     VLXmitWait \n");
	fprintf(stderr, "           0x01000000  24     VLRcvFECN\n");
	fprintf(stderr, "           0x00800000  23     VLRcvBECN  \n");
	fprintf(stderr, "           0x00400000  22     VLXmitTimeCong\n");
	fprintf(stderr, "           0x00200000  21     VLXmitWastedBW \n");
	fprintf(stderr,	"           0x00100000  20     VLXmitWaitData\n");
	fprintf(stderr, "           0x00080000  19     VLRcvBubble \n");
	fprintf(stderr, "           0x00040000  18     VLMarkFECN\n");
	fprintf(stderr, "           Bits 17-0 reserved\n");
 	fprintf(stderr, "     -f/--focus         - focus select value for getting focus ports\n");
 	fprintf(stderr, "         focus select values:\n");
	fprintf(stderr, "           unexpclrport  - lists unexpectedly cleared ports - LID ordered\n");          // STL_PA_SELECT_UNEXP_CLR_PORT         0x00010101
	fprintf(stderr, "           norespport    - lists no response ports - LID ordered\n");                   // STL_PA_SELECT_NO_RESP_PORT    0x00010102
	fprintf(stderr, "           skippedport   - lists skipped ports - LID ordered\n");                       // STL_PA_SELECT_SKIPPED_PORT          0x00010103
	fprintf(stderr, "           utilhigh      - sorted by utilization - highest first\n");                  // STL_PA_SELECT_UTIL_HIGH         0x00020001
	fprintf(stderr, "           pktrate       - sorted by packet rate - highest first\n");                  // STL_PA_SELECT_UTIL_PKTS_HIGH    0x00020082
	fprintf(stderr, "           utillow       - sorted by utilization - lowest first\n");                   // STL_PA_SELECT_UTIL_LOW          0x00020101
	fprintf(stderr, "           integrity     - sorted by integrity category - highest first\n");             // STL_PA_SELECT_CATEGORY_INTEG         0x00030001
	fprintf(stderr, "           congestion    - sorted by congestion category - highest first\n");            // STL_PA_SELECT_CATEGORY_CONG          0x00030002
	fprintf(stderr, "           smacongestion - sorted by sma congestion category - highest first\n");        // STL_PA_SELECT_CATEGORY_SMA_CONG      0x00030003
	fprintf(stderr, "           bubbles       - sorted by bubble category - highest first\n");                // STL_PA_SELECT_CATEGORY_BUBBLE        0x00030004
	fprintf(stderr, "           security      - sorted by security category - highest first\n");              // STL_PA_SELECT_CATEGORY_SEC           0x00030005
	fprintf(stderr, "           routing       - sorted by routing category - highest first\n");               // STL_PA_SELECT_CATEGORY_ROUT          0x00030006
	fprintf(stderr, "           vfutilhigh    - sorted by utilization - highest first\n");                  // STL_PA_SELECT_VF_UTIL_HIGH         0x00020083
	fprintf(stderr, "           vfpktrate     - sorted by packet rate - highest first\n");                  // STL_PA_SELECT_VF_UTIL_PKTS_HIGH    0x00020084
	fprintf(stderr, "           vfutillow     - sorted by utilization - lowest first\n");                   // STL_PA_SELECT_VF_UTIL_LOW          0x00020102
	fprintf(stderr, "           vfcongestion  - sorted by congestion category - highest first\n");          // STL_PA_SELECT_CATEGORY_VF_CONG          0x00030007
	fprintf(stderr, "           vfbubbles     - sorted by bubble category - highest first\n");              // STL_PA_SELECT_CATEGORY_VF_BUBBLE        0x00030008    
	fprintf(stderr, "    -S/--start           - start of window for focus ports - should always be 0\n");
	fprintf(stderr, "                           for now\n");
	fprintf(stderr, "    -r/--range           - size of window for focus ports list\n");
	fprintf(stderr, "    -Q/--tuple           - a tuple that identifies a focus select value, comparator and argument\n");
	fprintf(stderr, "                              in the form select:comparator:argument.  Up to 8 tuples may be specified as: -Q s1:c1:a1 -Q s2:c2:a2 ... -Q s8:c8:a8\n");
	fprintf(stderr, "                              Valid focus select values are: utilization, pktrate, integrity, congestion, smacongestion, bubbles, security and routing\n");
	fprintf(stderr, "                              Valid comparator values are: GT, GE, LT, LE\n");
	fprintf(stderr, "                              Valid arguments are: any 64-bit value in hexadecimal or decimal.\n");
	fprintf(stderr, "                              When specifying more than one tuple, a logical operator -Z/--operator must also be specified.\n");
	fprintf(stderr, "    -Z/--operator         - a logical operator used to join the results of the specified tuples.\n");
	fprintf(stderr, "           and                - a port is returned if it meets all of the tuple filter criteria\n");
	fprintf(stderr, "           or                 - a port is returned if it meets one or more of the tuple filter criteria\n");
	fprintf(stderr, "    -n/--imgNum          - 64-bit image number \n");
	fprintf(stderr, "    -O/--imgOff          - image offset \n");
	fprintf(stderr, "    -y/--imgTime         - image time - may be used with imageinfo, groupInfo,\n");
	fprintf(stderr, "                           groupInfo, groupConfig, freezeImage, focusPorts,\n");
	fprintf(stderr, "                           vfInfo, vfConfig, and vfFocusPorts. Will return\n");
	fprintf(stderr, "                           closest image within image interval if possible.\n");
	fprintf(stderr, "                           See --begin/--end above for format.\n");
	fprintf(stderr, "    -m/--moveImgNum      - 64-bit image number - used with moveFreeze to move a\n");
	fprintf(stderr, "                           freeze image\n");
	fprintf(stderr, "    -M/--moveImgOff      - image offset - may be used with moveFreeze to move a\n");
	fprintf(stderr, "                           freeze image\n");
	fprintf(stderr, "    -V/--vfName          - VF name for vfInfo query\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Output types:\n");
	fprintf(stderr, "    classPortInfo      - class port info\n");
	fprintf(stderr, "    groupList          - list of PA groups (default)\n");
	fprintf(stderr, "    groupInfo          - summary statistics of a PA group - requires -g option\n");
	fprintf(stderr, "                         for groupName\n");
	fprintf(stderr, "    groupConfig        - configuration of a PA group - requires -g option for\n");
	fprintf(stderr, "                         groupName\n");
	fprintf(stderr, "    groupNodeInfo      - Node Information of a PA group - requires -g option for\n");
	fprintf(stderr, "                         groupName, options -l (lid), -G (nodeGuid) and\n");
	fprintf(stderr, "                         -D (node description) are optional\n");
	fprintf(stderr, "    groupLinkInfo      - Link Information of a PA group - requires -g option for\n");
	fprintf(stderr, "                         groupName, options -l (lid) and -P (port) are optional\n");
	fprintf(stderr, "                         -P 255 is for all ports. Option -P (port) without -l (lid)\n");
	fprintf(stderr, "                         is ignored\n");
	fprintf(stderr, "    portCounters       - port counters of fabric port - requires -l (lid) and\n");
	fprintf(stderr, "                         -P (port) options, -d (delta) is optional\n");
	fprintf(stderr, "    clrPortCounters    - clear port counters of fabric port - requires -l (lid),\n");
	fprintf(stderr, "                         -P (port), and -s (select) options\n");
	fprintf(stderr, "    clrAllPortCounters - clear all port counters in fabric - requires -s\n");
	fprintf(stderr, "                         (select) option\n");
	fprintf(stderr, "    pmConfig           - retrieve PM configuration information\n");
	fprintf(stderr, "    freezeImage        - create freeze frame for image ID - requires -n (imgNum)\n");
	fprintf(stderr, "    releaseImage       - release freeze frame for image ID - requires -n\n");
	fprintf(stderr, "                         (imgNum)\n");
	fprintf(stderr, "    renewImage         - renew lease for freeze frame for image ID - requires -n\n");
	fprintf(stderr, "                         (imgNum)\n");
	fprintf(stderr, "    moveFreeze         - move freeze frame from image ID to new image ID -\n");
	fprintf(stderr, "                         requires -n (imgNum) and -m (moveImgNum)\n");
	fprintf(stderr, "    focusPorts         - get sorted list of ports using utilization or error\n");
	fprintf(stderr, "                         values (from group buckets)\n");
	fprintf(stderr, "    imageInfo          - get information about a PA image (timestamps, etc.) -\n");
	fprintf(stderr, "                         requires -n (imgNum)\n");
	fprintf(stderr, "    vfList             - list of virtual fabrics\n");
	fprintf(stderr, "    vfInfo             - summary statistics of a virtual fabric - requires -V\n");
	fprintf(stderr, "                         option for vfName\n");
	fprintf(stderr, "    vfConfig           - configuration of a virtual fabric - requires -V option\n");
	fprintf(stderr, "                         for vfName\n");
	fprintf(stderr, "    vfPortCounters     - port counters of fabric port - requires -V (vfName), -l\n");
	fprintf(stderr, "                         (lid) and -P (port) options, -d (delta) is optional\n");
	fprintf(stderr, "    vfFocusPorts       - get sorted list of virtual fabric ports using\n");
	fprintf(stderr, "                         utilization or error values (from VF buckets) -\n");
	fprintf(stderr, "                         requires -V (vfname)\n");
	fprintf(stderr, "    clrVfPortCounters  - clear VF port counters of fabric port - requires -l\n");
	fprintf(stderr, "                         (lid), -P (port), -s (select), and -V (vfname) options\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage examples:\n");
	fprintf(stderr, "    opapaquery -o classPortInfo\n");
	fprintf(stderr, "    opapaquery -o groupList\n");
	fprintf(stderr, "    opapaquery -o groupInfo -g All\n");
	fprintf(stderr, "    opapaquery -o groupConfig -g All\n");
	fprintf(stderr, "    opapaquery -o groupNodeInfo -g All -G <nodeGuid>\n");
	fprintf(stderr, "    opapaquery -o groupNodeInfo -g All -D <nodeDesc>\n");
	fprintf(stderr, "    opapaquery -o groupNodeInfo -g All -l 1 \n");
	fprintf(stderr, "    opapaquery -o groupNodeInfo -g All -l 1 -G <nodeGuid>\n");
	fprintf(stderr, "    opapaquery -o groupLinkInfo -g All\n");
	fprintf(stderr, "    opapaquery -o groupLinkInfo -g All -l 1 -P 1\n");
	fprintf(stderr, "    opapaquery -o groupLinkInfo -g All -l 1 -P 255\n");
	fprintf(stderr, "    opapaquery -o portCounters -l 1 -P 1 -d 1\n");
	fprintf(stderr, "    opapaquery -o portCounters -l 1 -P 1 -d 1 -n 0x20000000d02 -O 1\n");
	fprintf(stderr, "    opapaquery -o portCounters -l 1 -P 1 -d 1 -j 13:30 -q 14:20\n");
	fprintf(stderr, "    opapaquery -o clrPortCounters -l 1 -P 1 -s 0xC0000000\n");
	fprintf(stderr, "        (clears XmitData & RcvData)\n");
	fprintf(stderr, "    opapaquery -o clrAllPortCounters -s 0xC0000000\n");
	fprintf(stderr, "        (clears XmitData & RcvData on all ports)\n");
	fprintf(stderr, "    opapaquery -o pmConfig\n");
	fprintf(stderr, "    opapaquery -o freezeImage -n 0x20000000d02\n");
	fprintf(stderr, "    opapaquery -o releaseImage -n 0xd01\n");
	fprintf(stderr, "    opapaquery -o renewImage -n 0xd01\n");
	fprintf(stderr, "    opapaquery -o moveFreeze -n 0xd01 -m 0x20000000d02 -M -2\n");
	fprintf(stderr, "    opapaquery -o focusPorts -g All -f integrity -S 0 -r 20\n");
	fprintf(stderr, "    opapaquery -o focusPorts -g All -S 0 -r 20 -Z AND -Q integrity:ge:20 -Q bubbles:lt:10\n");
	fprintf(stderr, "    opapaquery -o focusPorts -g All -f skippedport -S 0 -r 20\n");
	fprintf(stderr, "    opapaquery -o imageInfo -n 0x20000000d02\n");
	fprintf(stderr, "    opapaquery -o imageInfo -y \"1 hour ago\"\n");
	fprintf(stderr, "    opapaquery -o vfList\n");
	fprintf(stderr, "    opapaquery -o vfInfo -V Default\n");
	fprintf(stderr, "    opapaquery -o vfConfig -V Default\n");
	fprintf(stderr, "    opapaquery -o vfPortCounters -l 1 -P 1 -d 1 -V Default\n");
	fprintf(stderr, "    opapaquery -o clrVfPortCounters -l 1 -P 1 -s 0xC0000000 -V Default\n");
	fprintf(stderr, "        (clears VLXmitData & VLRcvData)\n");
	fprintf(stderr, "    opapaquery -o vfFocusPorts -V Default -f integrity -S 0 -r 20\n");
	exit(2);
}

/* SSL Paramters XML parse code */
static void *sslParmsXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	struct omgt_ssl_params *sslParmsData = IXmlParserGetContext(state);
	memset(sslParmsData, 0, sizeof(struct omgt_ssl_params));
	return sslParmsData;
}
static void sslParmsXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
}
static IXML_FIELD sslSecurityFields[] = {
	{ tag:"SslSecurityEnabled", format:'u', IXML_FIELD_INFO(struct omgt_ssl_params, enable) },
	{ tag:"SslSecurityEnable", format:'u', IXML_FIELD_INFO(struct omgt_ssl_params, enable) },
	{ tag:"SslSecurityDir", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, directory) },
	{ tag:"SslSecurityFFCertificate", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, certificate) },
	{ tag:"SslSecurityFFPrivateKey", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, private_key) },
	{ tag:"SslSecurityFFCaCertificate", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, ca_certificate) },
	{ tag:"SslSecurityFFCertChainDepth", format:'u', IXML_FIELD_INFO(struct omgt_ssl_params, cert_chain_depth) },
	{ tag:"SslSecurityFFDHParameters", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, dh_params) },
	{ tag:"SslSecurityFFCaCRLEnabled", format:'u', IXML_FIELD_INFO(struct omgt_ssl_params, ca_crl_enable) },
	{ tag:"SslSecurityFFCaCRL", format:'s', IXML_FIELD_INFO(struct omgt_ssl_params, ca_crl) },
	{ NULL }
};
static IXML_FIELD sslTopLevelFields[] = {
	{ tag:"SslSecurityParameters", format:'K', subfields:sslSecurityFields, start_func:sslParmsXmlParserStart, end_func:sslParmsXmlParserEnd }, // structure
	{ NULL }
};
FSTATUS ParseSslParmsFile(const char *input_file, int g_verbose, struct omgt_ssl_params *sslParmsData)
{
	unsigned tags_found, fields_found;
	const char *filename=input_file;

	if (strcmp(input_file, "-") == 0) {
		filename="stdin";
		if (FSUCCESS != IXmlParseFile(stdin, "stdin", IXML_PARSER_FLAG_NONE, sslTopLevelFields, NULL, sslParmsData, NULL, NULL, &tags_found, &fields_found)) {
			return FERROR;
		}
	} else {
		if (FSUCCESS != IXmlParseInputFile(input_file, IXML_PARSER_FLAG_NONE, sslTopLevelFields, NULL, sslParmsData, NULL, NULL, &tags_found, &fields_found)) {
			return FERROR;
		}
	}
	if (tags_found != 1 || fields_found != 1) {
		fprintf(stderr, "Warning: potentially inaccurate input '%s': found %u recognized top level tags, expected 1\n", filename, tags_found);
	}

	return FSUCCESS;
}
/* END SSL Paramters XML parse code */

typedef struct OutputFocusMap {
	char *string;
	int32 focus;
	} OutputFocusMap_t;

OutputFocusMap_t OutputFocusTable[]= {
	{"unexpclrport",    STL_PA_SELECT_UNEXP_CLR_PORT },   // 0x00010101
	{"norespport",      STL_PA_SELECT_NO_RESP_PORT },      // 0x00010102
	{"skippedport",     STL_PA_SELECT_SKIPPED_PORT },     // 0x00010103
	{"utilhigh",        STL_PA_SELECT_UTIL_HIGH },        // 0x00020001
	{"pktrate",         STL_PA_SELECT_UTIL_PKTS_HIGH },   // 0x00020082
	{"utillow",         STL_PA_SELECT_UTIL_LOW },         // 0x00020101
	{"integrity",       STL_PA_SELECT_CATEGORY_INTEG },        // 0x00030001
	{"congestion",      STL_PA_SELECT_CATEGORY_CONG },         // 0x00030002
	{"smacongestion",   STL_PA_SELECT_CATEGORY_SMA_CONG },     // 0x00030003
	{"bubbles",	        STL_PA_SELECT_CATEGORY_BUBBLE },       // 0x00030004
	{"security" ,       STL_PA_SELECT_CATEGORY_SEC },          // 0x00030005
	{"routing",	        STL_PA_SELECT_CATEGORY_ROUT },         // 0x00030006
	{"vfutilhigh",      STL_PA_SELECT_VF_UTIL_HIGH },        // 0x00020083
	{"vfpktrate",       STL_PA_SELECT_VF_UTIL_PKTS_HIGH },   // 0x00020084
	{"vfutillow",       STL_PA_SELECT_VF_UTIL_LOW },         // 0x00020102
	{"vfcongestion",    STL_PA_SELECT_CATEGORY_VF_CONG },    // 0x00030007
	{"vfbubbles",       STL_PA_SELECT_CATEGORY_VF_BUBBLE },  // 0x00030008
	{ NULL, 0},
};


FSTATUS StringToFocus (int32 *value, const char* str)
{
	int i;

	i=0;
	while (OutputFocusTable[i].string!=NULL) {
		if (0 == strcmp(str,OutputFocusTable[i].string) ){
			*value = OutputFocusTable[i].focus;
			return FSUCCESS;
		}
		else i++;
	}

	return FERROR;
}

void opapaquery_exit(FSTATUS status)
{
	if (g_portHandle) {
		omgt_close_port(g_portHandle);
		g_portHandle = NULL;
	}

	fprintf(stderr, "opapaquery completed: %s\n", (status == FSUCCESS) ? "OK" : "FAILED");
	exit((status == FSUCCESS) ? 0 : 1);
}

int main(int argc, char ** argv)
{
    FSTATUS fstatus;
    int c, index;
    uint8 hfi = 0;
    uint16 port = 0;
	char outputType[64];
	STL_LID  tempLid;
	uint8 temp8;
	char tempstr[64];
	int queryType = Q_GETGROUPLIST;
	int ms_timeout = OMGT_DEF_TIMEOUT_MS;
	g_gotOutput = 1;

	PrintDestInitFile(&g_dest, stdout);

	while (-1 != (c = getopt_long(argc,argv, "vh:p:b:x:ET:o:g:G:l:P:d:D:Us:n:O:f:S:r:m:M:V:j:q:y:Z:Q:", options, &index)))
    {
        switch (c)
        {
			case 'v':
				g_verbose++;
				if (g_verbose > 2) umad_debug(g_verbose - 2);
				break;

			case '$':
				usage();
				break;

			case 'h':
				if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid HFI Number: %s\n", optarg);
					usage();
				}
				g_isOOB = FALSE;
				break;
			case 'p':   // port to issue query from
				if (FSUCCESS != StringToUint16(&port, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Port Number: %s\n", optarg);
					usage();
				}
				break;
			case '!':
				if (FSUCCESS != StringToInt32(&ms_timeout, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid timeout value: %s\n", optarg);
					usage();
				}
				break;
			case 'b': // OOB address of FE
				g_oob_address = optarg;
				g_isOOB = TRUE;
				break;
			case 'E': // FE is ESM
				g_isFEESM = TRUE;
				g_isOOB = TRUE;
				break;
			case 'T': // FE Uses SSL
				g_sslParmsFile = optarg;
				g_isOOBSSL = TRUE;
				g_isOOB = TRUE;
				break;
			case 'x': // Source GID
				if (FSUCCESS != StringToGid(&g_src_gid.AsReg64s.H, &g_src_gid.AsReg64s.L, optarg, NULL, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Source Gid: %s\n", optarg);
					usage();
				}
				break;

			case 'o':
				memcpy(outputType, optarg, 64);
				if ((queryType = getQueryType(outputType)) <= 0) {
					fprintf(stderr, "opapaquery: Invalid Output Type: %s\n", outputType);
					usage();
				}
				g_gotOutput = 1;
				break;
			case 'g':
				snprintf(g_groupName, STL_PM_GROUPNAMELEN, "%s", optarg);
				g_gotGroup = 1;
				break;

			case 'l':
				if (FSUCCESS != StringToUint32(&tempLid, optarg, NULL, 0, TRUE) || tempLid == 0) {
					fprintf(stderr, "opapaquery: Invalid Lid Number: %s\n", optarg);
					usage();
				}
				g_nodeLid = tempLid;
				g_gotLid = TRUE;
				break;

			case 'P':
				if (FSUCCESS != StringToUint8(&temp8, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Port Number: %s\n", optarg);
					usage();
				}
				g_portNumber = temp8;
				g_gotPort = TRUE;
				break;

			case 'G':
				if (FSUCCESS != StringToUint64(&g_nodeGuid, optarg, NULL, 0, TRUE) || g_nodeGuid == 0) {
					fprintf(stderr, "opapaquery: Invalid GUID Number: %s\n", optarg);
					usage();
				}
				g_gotGUID = TRUE;
				break;

			case 'D':
				if(optarg[0] == '\0') {
					fprintf(stderr,"Illegal node descriptor parameter: Empty String\n");
					usage();
				}
				snprintf(g_nodeDesc, STL_PM_NODEDESCLEN, "%s", optarg);
				g_gotNodeDesc = TRUE;
				break;

			case 'd':
				if (FSUCCESS != StringToUint32(&g_deltaFlag, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Delta Flag: %s\n", optarg);
					usage();
				}
				break;

			case 'j':
				if (FSUCCESS != StringToDateTime(&g_beginTime, optarg)){
					fprintf(stderr, "opapaquery: Invalid Date/Time: %s\n", optarg);
					usage();
				}
				g_gotBegTime = TRUE;
				break;

			case 'q':
				if (FSUCCESS != StringToDateTime(&g_endTime, optarg)){
					fprintf(stderr, "opapaquery: Invalid Date/Time: %s\n", optarg);
					usage();
				}
				g_gotEndTime = TRUE;
				break;

			case 'U':
				g_userCntrsFlag = 1;
				break;

			case 's':
				if (FSUCCESS != StringToUint32(&g_selectFlag, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Select Flag: %s\n", optarg);
					usage();
				}
				g_gotSelect = TRUE;
				break;

			case 'n':
				if (FSUCCESS != StringToUint64(&g_imageNumber, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Image Number: %s\n", optarg);
					usage();
				}
				g_gotImgNum = TRUE;
				break;

			case 'O':
				if (FSUCCESS != StringToInt32(&g_imageOffset, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Image Offset: %s\n", optarg);
					usage();
				}
				g_gotImgOff = TRUE;
				break;

			case 'y':
				if (FSUCCESS != StringToDateTime(&g_imageTime, optarg)){
					fprintf(stderr, "opapaquery: Invalid Date/Time: %s\n", optarg);
					usage();
				}
				g_gotImgTime = TRUE;
				break;

			case 'm':
				if (FSUCCESS != StringToUint64(&g_moveImageNumber, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Move Image Number: %s\n", optarg);
					usage();
				}
				g_gotMoveImgNum = TRUE;
				break;

			case 'M':
				if (FSUCCESS != StringToInt32(&g_moveImageOffset, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Move Image Offset: %s\n", optarg);
					usage();
				}
				g_gotMoveImgOff = TRUE;
				break;

			case 'f':
				if (FSUCCESS != StringToFocus (&g_focus, optarg)) {
					fprintf(stderr, "opapaquery: Invalid Focus Select Parameter: %s\n", optarg);
					usage();
				}
				g_gotFocus = TRUE;
				break;

			case 'S':
				if (FSUCCESS != StringToInt32(&g_start, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Start Number: %s\n", optarg);
					usage();
				}
				g_gotStart = TRUE;
				break;

			case 'r':
				if (FSUCCESS != StringToInt32(&g_range, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Range Number: %s\n", optarg);
					usage();
				}
				g_gotRange = TRUE;
				break;

			case 'Z':
				if (strcasecmp(optarg, "and") == 0)
					g_logicalOperator = FOCUS_PORTS_LOGICAL_OPERATOR_AND;
				else if (strcasecmp(optarg, "or") == 0)
					g_logicalOperator = FOCUS_PORTS_LOGICAL_OPERATOR_OR;
				else {
					fprintf(stderr, "opapaquery: Invalid Logical Operator value: %s\n", optarg);
					usage();
				}
				g_gotLogicalOperator = TRUE;
				break;

			case 'Q':

				StringCopy(tempstr, optarg, sizeof(tempstr));

				if (tupleID >= MAX_NUM_FOCUS_PORT_TUPLES) {
					fprintf(stderr, "opapaquery: Tuple count exceeded.  Limited to %d tuples\n", MAX_NUM_FOCUS_PORT_TUPLES);
					usage();
				}

				tuple[tupleID].select = 0;
				tuple[tupleID].comparator = FOCUS_PORTS_COMPARATOR_INVALID;

				if (FSUCCESS != StringToTuple(&tuple[tupleID].select, &tuple[tupleID].comparator, &tuple[tupleID].argument, optarg, NULL)) {
					fprintf(stderr, "opapaquery: Invalid Tuple value: %s\n", tempstr);
					usage();
				}
				g_gotTuple = TRUE;
				tupleID++;
				break;


			case 'V':
				snprintf(g_vfName, STL_PM_VFNAMELEN, "%s", optarg);
				g_gotvfName = TRUE;
				break;

            default:
				fprintf(stderr, "opapaquery: Invalid Option %c\n", c);
                usage();
                break;
        }
    } /* end while */

	if (optind < argc)
	{
		fprintf(stderr, "opapaquery: invalid argument %s\n", argv[optind]);
		usage();
	}

	// Check parameter consistency

	if ((queryType == Q_GETGROUPINFO) && (!g_gotGroup)) {
		fprintf(stderr, "opapaquery: Must provide a group name with output type groupInfo\n");
		usage();
	}

	if ((queryType == Q_GETGROUPCONFIG) && (!g_gotGroup)) {
		fprintf(stderr, "opapaquery: Must provide a group name with output type groupConfig\n");
		usage();
	}

	if (queryType == Q_GETFOCUSPORTS) {
		if  ((!g_gotGroup) || (g_gotFocus == g_gotTuple) || (!g_gotRange) || (!g_gotStart)) {
			fprintf(stderr, "opapaquery: Must provide a group name(g), focus(f) or tuple(Q), range(r) and start(S) \n");
			fprintf(stderr, "            with output type focusPorts. Image Number(n) and Image Offset(O) are optional.\n");
			usage();
		}
		else if (g_deltaFlag || g_gotLid || g_gotPort || g_gotSelect || g_gotMoveImgNum || g_gotMoveImgOff ||g_userCntrsFlag || g_gotvfName)
			fprintf(stderr, "opapaquery: for the selected output type only -g, -f or -Q, -r and -S options are required.\n");
			fprintf(stderr, "Ignoring rest..\n");

		if (!g_gotLogicalOperator) {
			if (tupleID > 1) {
				fprintf(stderr, "opapaquery: Must supply a logical operator(Z)\n");
				usage();
			}
		}
	}

	if ((queryType == Q_GETGROUPNODEINFO) && (!g_gotGroup)) {
		fprintf(stderr, "opapaquery: Must provide a group name with output type groupNodeInfo \n");
		usage();
	}

	if ((queryType == Q_GETGROUPLINKINFO) && (!g_gotGroup)) {
		fprintf(stderr, "opapaquery: Must provide a group name with output type groupLinkInfo \n");
		usage();
	}

	if (queryType == Q_GETVFFOCUSPORTS) {
		if ((!g_gotvfName) || (!g_gotFocus) || (!g_gotRange) || (!g_gotStart)) {
				fprintf(stderr, "opapaquery: Must provide a vf_name(V), focus(f), range(r) and start(S) \n");
				fprintf(stderr, "            with output type VFfocusPorts. Image Number and Image Offset are optional.\n");
				usage();
		}
		else if (g_deltaFlag || g_gotLid || g_gotPort || g_gotSelect || g_gotMoveImgNum ||
				g_gotMoveImgOff ||g_userCntrsFlag || g_gotGroup) {
				fprintf(stderr, "opapaquery: for the selected output type only -V, -f, -r and -S options are required.\n");
				fprintf(stderr, "Ignoring rest..\n");
		}
	}

	if ((queryType == Q_GETPORTCOUNTERS) && ((!g_gotLid) || (!g_gotPort))) {
		fprintf(stderr, "opapaquery: Must provide a lid and port with output type portCounters\n");
		usage();
	}

	if ((queryType == Q_CLRPORTCOUNTERS) && ((!g_gotLid) || (!g_gotPort) || (!g_gotSelect))) {
		fprintf(stderr, "opapaquery: Must provide a lid, port, and select flag with output type clrPortCounters\n");
		usage();
	}

	if ((queryType == Q_CLRALLPORTCOUNTERS) && !g_gotSelect) {
		fprintf(stderr, "opapaquery: Must provide a select flag with output type clrAllPortCounters\n");
		usage();
	}

	if ((((queryType == Q_RELEASEIMAGE) || (queryType == Q_RENEWIMAGE) || (queryType == Q_MOVEFREEZE))	&&
		(!g_gotImgNum))) {
		fprintf(stderr, "opapaquery: Must provide an imgNum with releaseImage/renewImage/moveFreeze\n");
		usage();
	}

	if ((queryType == Q_FREEZEIMAGE) && !(g_gotImgNum || g_gotImgTime)){
		fprintf(stderr, "opapaquery: Must provide an imgNum or imgTime with freezeImage\n");
		usage();
	}

	if ((queryType == Q_MOVEFREEZE) && !g_gotMoveImgNum) {
		fprintf(stderr, "opapaquery: Must provide a moveImgNum with output type moveFreeze\n");
		usage();
	}

	if (g_deltaFlag > 1) {
		fprintf(stderr, "opapaquery: delta value must be 0 or 1\n");
		usage();
	}

    if (g_userCntrsFlag && (g_deltaFlag || g_gotImgOff)) {
        fprintf(stderr, "opapaquery: delta value and image offset must be 0 when querying User Counters\n");
        usage();
    }

	if (g_gotBegTime && g_gotEndTime && (g_beginTime >= g_endTime)){
		fprintf(stderr, "opapaquery: begin time must be before end time\n");
		usage();
	}

	if ((queryType == Q_GETVFINFO) && (!g_gotvfName)) {
		fprintf(stderr, "opapaquery: Must provide a VF name with output type vfInfo\n");
		usage();
	}

	if ((queryType == Q_GETVFCONFIG) && (!g_gotvfName)) {
		fprintf(stderr, "opapaquery: Must provide a VF name with output type vfConfig\n");
		usage();
	}

	if ((queryType == Q_GETVFPORTCOUNTERS) && (!g_gotvfName)) {
		fprintf(stderr, "opapaquery: Must provide a VF name with output type vfPortCounters\n");
		usage();
	}

	if ((queryType == Q_GETVFPORTCOUNTERS) && ((!g_gotLid) || (!g_gotPort))) {
		fprintf(stderr, "opapaquery: Must provide a lid and port with output type vfPortCounters\n");
		usage();
	}

	if ((queryType == Q_CLRVFPORTCOUNTERS) && (!g_gotvfName)) {
		fprintf(stderr, "opapaquery: Must provide a VF name with output type clrVfPortCounters\n");
		usage();
	}

	if ((queryType == Q_CLRVFPORTCOUNTERS) && ((!g_gotLid) || (!g_gotPort) || (!g_gotSelect))) {
		fprintf(stderr, "opapaquery: Must provide a lid, port, and select flag with output type clrVfPortCounters\n");
		usage();
	}

	if (queryType == Q_GETGROUPLIST || queryType == Q_GETVFLIST) {
		// only certain options are valid. Rest are ignored.
		if (g_deltaFlag || g_gotGroup || g_gotLid || g_gotPort || g_gotSelect || g_gotGUID ||
			g_gotMoveImgNum || g_gotMoveImgOff || g_gotNodeDesc ||
			g_gotFocus || g_gotStart || g_gotRange || g_userCntrsFlag || g_gotvfName
			|| g_gotBegTime || g_gotEndTime) {
			fprintf(stderr, "opapaquery: for the selected output type only -[hpvnOy] options are valid. Ignoring rest..\n");
		}
	}
	if (queryType == Q_GETPMCONFIG || queryType == Q_CLASSPORTINFO) {
		// only -h, -p and -v options are valid. Rest are ignored.
		if (g_deltaFlag || g_gotGroup || g_gotLid || g_gotPort || g_gotSelect || g_gotGUID ||
			g_gotImgNum || g_gotImgOff || g_gotMoveImgNum || g_gotMoveImgOff || g_gotNodeDesc ||
			g_gotFocus || g_gotStart || g_gotRange || g_userCntrsFlag || g_gotvfName
			|| g_gotImgTime || g_gotBegTime || g_gotEndTime) {
			fprintf(stderr, "opapaquery: for the selected output type only -h, -p and -v options are valid. Ignoring rest..\n");
		}
	}
	else if (queryType == Q_GETGROUPINFO || queryType == Q_GETGROUPCONFIG) {
		// only certain options are valid. Rest are ignored.
		if (g_deltaFlag || g_gotLid || g_gotPort || g_gotSelect || g_gotGUID ||
			g_gotMoveImgNum || g_gotMoveImgOff || g_gotFocus || g_gotStart || g_gotNodeDesc ||
			g_gotRange || g_userCntrsFlag || g_gotvfName ||
			g_gotBegTime || g_gotEndTime) {
			fprintf(stderr, "opapaquery: for the selected output type only -[hpvgnOy] options are valid. Ignoring rest..\n");
		}
	}
	else if (queryType == Q_GETVFINFO || queryType == Q_GETVFCONFIG) {
		// only certain options are valid. Rest are ignored.
		if (g_deltaFlag || g_gotGroup || g_gotLid || g_gotPort || g_gotSelect || g_gotNodeDesc ||
			g_gotMoveImgNum || g_gotMoveImgOff || g_gotFocus || g_gotStart || g_gotGUID ||
			g_gotRange || g_userCntrsFlag || g_gotBegTime ||
			g_gotEndTime) {
			fprintf(stderr, "opapaquery: for the selected output type only -[hpvnOVy] options are valid. Ignoring rest..\n");
		}
	}
	else if (queryType == Q_GETGROUPNODEINFO) {
		// only certain options are valid. Rest are ignored.
		if (g_deltaFlag || g_gotSelect || g_gotMoveImgNum || g_gotMoveImgOff || g_gotPort ||
			g_gotvfName || g_gotFocus || g_gotStart || g_gotRange || g_userCntrsFlag ||
			g_gotBegTime || g_gotEndTime) {
			fprintf(stderr, "opapaquery: for the selected output type only -[hpvlngDGOy] options are valid. Ignoring rest..\n");
		}
	}
	else if (queryType == Q_GETGROUPLINKINFO) {
		// only certain options are valid. Rest are ignored.
		if (g_deltaFlag || g_gotSelect || g_gotMoveImgNum || g_gotMoveImgOff || g_gotGUID ||
			g_gotvfName || g_gotFocus || g_gotStart || g_gotRange || g_userCntrsFlag || g_gotNodeDesc ||
			g_gotBegTime || g_gotEndTime) {
			fprintf(stderr, "opapaquery: for the selected output type only -[hpvlngPOy] options are valid. Ignoring rest..\n");
		}
		if (g_gotPort && !g_gotLid) {
			fprintf(stderr, "opapaquery: -l (lid) also needed with -P (port). Ignoring -P\n");
		}
	}

	/* Open omgt_port */
	struct omgt_params params = {
		.debug_file = (g_verbose > 1 ? stderr : NULL),
		.error_file = stderr
	};
	if (g_isOOB) {
		if (g_sslParmsFile) {
			if ((fstatus = ParseSslParmsFile(g_sslParmsFile, g_verbose, &g_ssl_params)) != FSUCCESS) {
				fprintf(stderr, "opapaquery: Must provide a valid SSL/TLS parameters XML file: %s\n", g_sslParmsFile);
				usage();
			}
		}
		struct omgt_oob_input oob_input = {
			.host = g_oob_address,
			.port = (port ? port : g_oob_port),
			.is_esm_fe = g_isFEESM,
			.ssl_params = g_ssl_params
		};
		fstatus = omgt_oob_connect(&g_portHandle, &oob_input, &params);
		if (fstatus != 0) {
			fprintf(stderr, "opapaquery: Failed to open connection to FE %s:%u: %s\n",
				oob_input.host, oob_input.port, omgt_status_totext(fstatus));
			opapaquery_exit(fstatus);
		}
	} else {
		int pa_service_state;
		fstatus = omgt_open_port_by_num(&g_portHandle, (int)hfi, port, &params);
		if (fstatus == OMGT_STATUS_SUCCESS) {
			fstatus = omgt_port_get_pa_service_state(g_portHandle, &pa_service_state, OMGT_REFRESH_SERVICE_BAD_STATE);
			if (fstatus == OMGT_STATUS_SUCCESS) {
				if (pa_service_state != OMGT_SERVICE_STATE_OPERATIONAL) {
					fprintf(stderr, "opapaquery: failed to connect, PA Service State is Not Operational: %s (%d)\n",
						omgt_service_state_totext(pa_service_state), pa_service_state);
					opapaquery_exit(FUNAVAILABLE);
				}
			} else {
				fprintf(stderr, "opapaquery: failed to get and refresh pa service state: %u\n", fstatus);
				opapaquery_exit(fstatus);
			}
		} else {
			fprintf(stderr, "opapaquery: failed to open hfi %d, port %d: %u\n", hfi, port, fstatus);
			opapaquery_exit(fstatus);
		}
	}

	if (g_verbose)
		set_opapaquery_debug(g_portHandle);

	//set timeout for PA operations
	omgt_set_timeout(g_portHandle, ms_timeout);

	// verify PA has necessary capabilities
	STL_CLASS_PORT_INFO *portInfo;
	if ((portInfo = GetClassPortInfo(g_portHandle)) == NULL){
		fprintf(stderr, "opapaquery: failed to determine PA capabilities\n");
		opapaquery_exit(FERROR);
	}
	STL_PA_CLASS_PORT_INFO_CAPABILITY_MASK paCap;
	memcpy(&paCap, &portInfo->CapMask, sizeof(STL_PA_CLASS_PORT_INFO_CAPABILITY_MASK));
	//if trying to query by time, check if feature available
	if (g_imageTime || g_beginTime || g_endTime){
			if (!(paCap.s.IsAbsTimeQuerySupported)){
				fprintf(stderr, "PA does not support time queries\n");
				MemoryDeallocate(portInfo);
				opapaquery_exit(FERROR);
			}
	}

	//if trying to query extended focus port selects, check if feature is available
	if (g_focus == STL_PA_SELECT_UNEXP_CLR_PORT || g_focus == STL_PA_SELECT_NO_RESP_PORT ||
			g_focus == STL_PA_SELECT_SKIPPED_PORT || g_gotTuple){
			if (!(paCap.s.IsExtFocusTypesSupported)){
				fprintf(stderr, "PA does not support extended focus ports\n");
				MemoryDeallocate(portInfo);
				opapaquery_exit(FERROR);
			}
	}

	//if querying for nodeinfo and linkinfo check if feature is available
	if (queryType == Q_GETGROUPNODEINFO || queryType == Q_GETGROUPLINKINFO){
		if (!(paCap.s.IsTopologyInfoSupported)){
			fprintf(stderr, "PA does not support Topology Information from PM\n");
			MemoryDeallocate(portInfo);
			opapaquery_exit(FERROR);
		}
	}
	if (queryType == Q_GETGROUPLIST || queryType == Q_GETVFLIST) {
		if (!paCap.s.IsPerImageListsSupported && (g_gotImgNum || g_gotImgOff || g_gotImgTime)) {
			fprintf(stderr, "PA does not support Getting historical (Per Image) Group/VF List\n");
			MemoryDeallocate(portInfo);
			opapaquery_exit(FERROR);
		}
	}

	if (IS_VF_FOCUS_SELECT(g_focus)) {
		// verify PM config for VL counters
		if ((fstatus = GetPMConfig(g_portHandle, &g_pmConfigData)) == FERROR){
			fprintf(stderr, "opapaquery: failed to get GetPMConfig \n");
			MemoryDeallocate(portInfo);
			opapaquery_exit(FERROR);
		}
		g_valid_pm_config_exists = TRUE;
		//Check if VF level Focus selects are supported
		if (!(paCap.s.IsVFFocusTypesSupported) ||
			!(g_pmConfigData.pmFlags & STL_PM_PROCESS_VL_COUNTERS)){
			fprintf(stderr, "PA does not support VF Focus selects \n");
			MemoryDeallocate(portInfo);
			opapaquery_exit(FERROR);
		}
	}

	switch (queryType) {
	case Q_GETGROUPLIST:
		if (paCap.s.IsPerImageListsSupported) {
			fstatus = GetGroupList2(g_portHandle, g_imageNumber, g_imageOffset, g_imageTime);
		} else {
			fstatus = GetGroupList(g_portHandle);
		}
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get group list\n");
		}
		break;

	case Q_GETGROUPINFO:
		fstatus = GetGroupInfo(g_portHandle, g_groupName, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get group info\n");
		}
		break;

	case Q_GETGROUPCONFIG:
		fstatus = GetGroupConfig(g_portHandle, g_groupName, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get group configuration\n");
		}
		break;

	case Q_GETGROUPNODEINFO:
		fstatus = GetGroupNodeInfo(g_portHandle, g_groupName, g_nodeLid, g_nodeGuid, g_nodeDesc, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get group node info\n");
		}
		break;

	case Q_GETGROUPLINKINFO:
		fstatus = GetGroupLinkInfo(g_portHandle, g_groupName, g_nodeLid, g_portNumber, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get group link info\n");
		}
		break;


	case Q_GETPORTCOUNTERS:
		fstatus = GetPortCounters(g_portHandle, g_nodeLid, g_portNumber, g_deltaFlag, g_userCntrsFlag, g_imageNumber, g_imageOffset, g_beginTime, g_endTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get port counters\n");
		}
		break;

	case Q_CLRPORTCOUNTERS:
		fstatus = ClrPortCounters(g_portHandle, g_nodeLid, g_portNumber, g_selectFlag);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to clear port counters\n");
		}
		break;

	case Q_CLRALLPORTCOUNTERS:
		fstatus = ClrAllPortCounters(g_portHandle, g_selectFlag);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to clear all port counters\n");
		}
		break;

	case Q_GETPMCONFIG:
		if (!g_valid_pm_config_exists) {
			fstatus = GetPMConfig(g_portHandle, &g_pmConfigData);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "opapaquery: failed to get PM configuration\n");
				break;
			}
		}
		PrintStlPMConfig(&g_dest, 0, &g_pmConfigData);
		fstatus = FSUCCESS;
		break;

	case Q_FREEZEIMAGE:
		fstatus = FreezeImage(g_portHandle, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to freeze image\n");
		}
		break;

	case Q_RELEASEIMAGE:
		fstatus = ReleaseImage(g_portHandle, g_imageNumber, g_imageOffset);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to release image\n");
		}
		break;

	case Q_RENEWIMAGE:
		fstatus = RenewImage(g_portHandle, g_imageNumber, g_imageOffset);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to renew image\n");
		}
		break;

	case Q_MOVEFREEZE:
		fstatus = MoveFreeze(g_portHandle, g_imageNumber, g_imageOffset, g_moveImageNumber, g_moveImageOffset);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to move freeze image\n");
		}
		break;

	case Q_GETFOCUSPORTS:
		fstatus = GetFocusPorts(g_portHandle, g_groupName, g_focus, g_start, g_range, g_imageNumber, g_imageOffset, g_imageTime, g_gotTuple ? tuple:NULL, g_logicalOperator);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get focus ports\n");
		}
		break;

	case Q_GETIMAGECONFIG:
		fstatus = GetImageInfo(g_portHandle, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get image info\n");
		}
		break;

	case Q_CLASSPORTINFO:
		PrintStlClassPortInfo(&g_dest, 0, portInfo, MCLASS_VFI_PM);
		MemoryDeallocate(portInfo);
		fstatus = FSUCCESS;
		break;

	case Q_GETVFLIST:
		if (paCap.s.IsPerImageListsSupported) {
			fstatus = GetVFList2(g_portHandle, g_imageNumber, g_imageOffset, g_imageTime);
		} else {
			fstatus = GetVFList(g_portHandle);
		}
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get vf list\n");
		}
		break;

	case Q_GETVFINFO:
		fstatus = GetVFInfo(g_portHandle, g_vfName, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get VF info\n");
		}
		break;
	case Q_GETVFCONFIG:
		fstatus = GetVFConfig(g_portHandle, g_vfName, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get vf configuration\n");
		}
		break;

	case Q_GETVFPORTCOUNTERS:
		fstatus = GetVFPortCounters(g_portHandle, g_nodeLid, g_portNumber, g_deltaFlag, g_userCntrsFlag, g_vfName, g_imageNumber, g_imageOffset, g_beginTime, g_endTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get vf port counters\n");
		}
		break;

	case Q_CLRVFPORTCOUNTERS:
		fstatus = ClrVFPortCounters(g_portHandle, g_nodeLid, g_portNumber, g_selectFlag, g_vfName);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to clear vf port counters\n");
		}
		break;

	case Q_GETVFFOCUSPORTS:
		fstatus = GetVFFocusPorts(g_portHandle, g_vfName, g_focus, g_start, g_range, g_imageNumber, g_imageOffset, g_imageTime);
		if (fstatus != FSUCCESS) {
			fprintf(stderr, "opapaquery: failed to get VF focus ports\n");
		}
		break;

	default:
		break;
	}
	opapaquery_exit(fstatus);
	//shouldn't reach here
	exit(-1);
}
