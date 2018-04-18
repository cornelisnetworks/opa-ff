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

// control experiments/tuning of tool
#define USE_FREEZE 1	// freeze image while gathering data
						// otherwise no freeze, just do queries
						// Freeze may help performance of on-disk queries
						// recommend 1
#define USE_ABS_IMAGENUM 1	// use imageNumber returned in a PA query
						//as opposed to Live=0 with decreasing offset as walk
						// Abs image number avoids definition of live-offset
						// changing due to new PM sweeps occuring mid query
						// recommend 1
#define COMPUTE_DELTA 1	// manually compute counter delta using Get PortCounter
						// absolute.  Works around a PM disk image cache thrash
						// which occurs with Get PortCounter delta

#define					MAX_VFABRIC_NAME		64		// from fm_xml.h

int						g_verbose 			= 0;
STL_LID					g_nodeLid			= 0;
uint8					g_portNumber		= 0;
uint64					g_nodeGUID			= 0;
char					g_nodeDesc[STL_PM_NODEDESCLEN];
uint32					g_deltaFlag			= 1;
//uint32					g_userCntrsFlag		= 0;
//int32					g_focus				= 0;
int32					g_start				= 0;
int32					g_range				= 0;
uint32					g_liveRate			= 0;
uint32					g_gotGroup			= 0;
uint32					g_gotLid			= 0;
uint32					g_gotPort			= 0;
uint32					g_gotGUID			= 0;
uint32					g_gotDesc			= 0;
//uint32					g_gotImgNum			= 0;
//uint32					g_gotImgOff			= 0;
//uint32					g_gotFocus			= 0;
uint32					g_gotStart			= 0;
uint32					g_gotRange			= 0;
uint32					g_gotLive			= 0;
uint32					g_gotXmit			= 0;
//uint32					g_gotvfName			= 0;
char					g_groupName[STL_PM_GROUPNAMELEN];
//char					g_vfName[MAX_VFABRIC_NAME];
PrintDest_t				g_dest;
struct omgt_port 		*g_portHandle		= NULL;

typedef struct ColumnEntry_s {
	uint64 nodeGUID;
	char nodeDesc[64];
	STL_LID nodeLid;
	uint8	portNumber;
#if COMPUTE_DELTA
	uint64	recentValue;
	int		recentValueValid;
#endif
} ColumnEntry_t;

// omits 1st column, which is timestamp
ARRAY g_Columns;	// array of ColumnEntry_t

struct option options[] = {
		// basic controls
		{ "verbose", no_argument, NULL, 'v' },
		{ "hfi", required_argument, NULL, 'h' },
		{ "port", required_argument, NULL, 'p' },

		// query data
		//{ "output", required_argument, NULL, 'o' },
		{ "groupName", required_argument, NULL, 'g' },
		{ "lid", required_argument, NULL, 'l' },
		{ "portNumber", required_argument, NULL, 'P' },
		{ "guid", required_argument, NULL, 'G' },
		{ "desc", required_argument, NULL, 'D' },
		{ "delta", required_argument, NULL, 'd' },
		{ "xmit", required_argument, NULL, 'x' },
		//{ "userCntrs", no_argument, NULL, 'U' },
		//{ "select", required_argument, NULL, 's' },
		//{ "imgNum", required_argument, NULL, 'n' },
		//{ "imgOff", required_argument, NULL, 'O' },
		//{ "moveImgNum", required_argument, NULL, 'm' },
		//{ "moveImgOff", required_argument, NULL, 'M' },
		//{ "focus", required_argument, NULL, 'f' },
		{ "start", required_argument, NULL, 'S' },
		{ "range", required_argument, NULL, 'r' },
		{ "live", required_argument, NULL, 'L' },
		{ "help", no_argument, NULL, '$' },
		{ 0 }
};

static FSTATUS opa_pa_init(uint8 hfi, uint8 port)
{
	FSTATUS fstatus = FERROR;
	int pa_service_state;

	// Open the port
	struct omgt_params params = {
		.debug_file = (g_verbose > 3 ? stderr : NULL),
		.error_file = stderr
	};
	fstatus = omgt_open_port_by_num(&g_portHandle, (int)hfi, port, &params);
	if (fstatus == OMGT_STATUS_SUCCESS) {
		fstatus = omgt_port_get_pa_service_state(g_portHandle, &pa_service_state, OMGT_REFRESH_SERVICE_BAD_STATE);
		if (fstatus == OMGT_STATUS_SUCCESS) {
			if (pa_service_state != OMGT_SERVICE_STATE_OPERATIONAL) {
				fprintf(stderr, "%s: failed to connect, PA Service State is Not Operational: %s (%d)\n",
					__func__, omgt_service_state_totext(pa_service_state), pa_service_state);
				fstatus = FUNAVAILABLE;
			}
		} else {
			fprintf(stderr, "%s: failed to get and refresh pa service state: %u\n", __func__, fstatus);
		}
	} else {
		fprintf(stderr, "%s: failed to open hfi %d, port %d: %u\n", __func__, hfi, port, fstatus);
	}

	return fstatus;
}

#if 0
static FSTATUS GetClassPortInfo(struct omgt_port *port)
{
	FSTATUS					status= FERROR;
	STL_CLASS_PORT_INFO		*response;

    fprintf(stderr, "Getting Class Port Info...\n");
    if ((response = omgt_pa_get_classportinfo(port)) != NULL) {
        status = FSUCCESS;
		PrintStlClassPortInfo(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive GetClassPortInfo response: %s\n", iba_pa_mad_status_msg(port));
    }

	if (response)
		MemoryDeallocate(response);
    return status;
}
#endif

static FSTATUS GetPortCounters(struct omgt_port *port, STL_LID nodeLid, uint8_t portNumber, uint32_t deltaFlag, uint32_t userCntrsFlag, uint64 imageNumber, int32 imageOffset, STL_PORT_COUNTERS_DATA *pCounters)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
    STL_PORT_COUNTERS_DATA		*response;

    if (g_verbose)
		fprintf(stderr, "Getting Port Counters for Lid 0x%8.8x port %u...\n", nodeLid, portNumber);
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
    if ((response = iba_pa_single_mad_port_counters_response_query(port, nodeLid, portNumber, deltaFlag, userCntrsFlag, &imageId)) != NULL) {
        status = FSUCCESS;
    	if (g_verbose > 2)
			PrintStlPAPortCounters(&g_dest, 0, response, nodeLid, (uint32)portNumber, response->flags);
		*pCounters = *response;
    } else {
        fprintf(stderr, "Failed to receive GetPortCounters response: %s\n", iba_pa_mad_status_msg(port));
    }

	if (response)
		MemoryDeallocate(response);
    return status;
}

#if 0
static FSTATUS GetPMConfig(struct omgt_port *port)
{
	FSTATUS					status= FERROR;
    STL_PA_PM_CFG_DATA			*response;

    fprintf(stderr, "Getting PM Configuration...\n");
    if ((response = iba_pa_single_mad_get_pm_config_response_query(port)) != NULL) {
        status = FSUCCESS;
		PrintStlPMConfig(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive GetPMConfig response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}
#endif

#if USE_FREEZE
static FSTATUS FreezeImage(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, STL_PA_IMAGE_ID_DATA *pImageId)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			request = {0};
    STL_PA_IMAGE_ID_DATA			*response;

    if (g_verbose)
		fprintf(stderr, "Freezing image 0x%lx %d...\n", imageNumber, imageOffset);
	request.imageNumber = imageNumber;
	request.imageOffset = imageOffset;
    if ((response = iba_pa_single_mad_freeze_image_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
    	if (g_verbose > 1)
			PrintStlPAImageId(&g_dest, 0, response);
		if (pImageId)
			*pImageId = *response;
    } else {
		//if (port->mad_status == STL_MAD_STATUS_STL_PA_NO_IMAGE)
		//	status = FNOT_FOUND;
        fprintf(stderr, "Failed to receive FreezeImage response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}
#endif

#if USE_FREEZE
static FSTATUS ReleaseImage(struct omgt_port *port, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			request = {0};
    STL_PA_IMAGE_ID_DATA			*response;

    if (g_verbose)
		fprintf(stderr, "Releaseing image 0x%lx %d...\n", imageNumber, imageOffset);
	request.imageNumber = imageNumber;
	request.imageOffset = imageOffset;
    if ((response = iba_pa_single_mad_release_image_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
		if (g_verbose > 1)
			PrintStlPAImageId(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive ReleaseImage response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}
#endif

#if 0
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
#endif

#if USE_FREEZE
static FSTATUS MoveFreeze(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint64 moveImageNumber, int32 moveImageOffset, STL_PA_IMAGE_ID_DATA *pImageId)
{
	FSTATUS					status= FERROR;
    STL_MOVE_FREEZE_DATA		request;
    STL_MOVE_FREEZE_DATA		*response;

    if (g_verbose)
		fprintf(stderr, "Moving freeze image 0x%lx %d to 0x%lx %d...\n", imageNumber, imageOffset, moveImageNumber, moveImageOffset);
	request.oldFreezeImage.imageNumber = imageNumber;
	request.oldFreezeImage.imageOffset = imageOffset;
	request.newFreezeImage.imageNumber = moveImageNumber;
	request.newFreezeImage.imageOffset = moveImageOffset;
    if ((response = iba_pa_single_mad_move_freeze_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
    	if (g_verbose > 1)
			PrintStlPAMoveFreeze(&g_dest, 0, response);
		if (pImageId)
			*pImageId = response->newFreezeImage;
    } else {
		if (omgt_get_pa_mad_status(port) == STL_MAD_STATUS_STL_PA_NO_IMAGE)
			status = FNOT_FOUND;
		else
        	fprintf(stderr, "Failed to receive MoveFreeze response: %s\n", iba_pa_mad_status_msg(port));
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}
#endif

static FSTATUS GetGroupList(struct omgt_port *port, char groupName[STL_PM_GROUPNAMELEN])
{
	OMGT_QUERY				query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	if (g_verbose)
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
			   	pQueryResults->MadStatus, iba_sd_mad_status_msg(pQueryResults->MadStatus));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group List Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_LIST_RESULTS *p = (STL_PA_GROUP_LIST_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
					   					iba_sd_mad_status_msg(pQueryResults->MadStatus));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
        	fprintf(stderr, "PA Multiple MAD Response for Group Data:\n");
		}

		if (g_verbose > 1)
			PrintStlPAGroupList(&g_dest, 1, p->NumGroupListRecords, p->GroupListRecords);
		// return 1st group name in list
		memcpy(groupName, p->GroupListRecords[0].groupName, STL_PM_GROUPNAMELEN);
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

#if 0
static FSTATUS GetGroupInfo(struct omgt_port *port, char *groupName, uint64 imageNumber, int32 imageOffset)
{
	OMGT_QUERY				query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA	imageId = {0};

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group Info...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
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
			   	pQueryResults->MadStatus, iba_sd_mad_status_msg(pQueryResults->MadStatus));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group Info Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_INFO_RESULTS *p = (STL_PA_GROUP_INFO_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
					   					iba_sd_mad_status_msg(pQueryResults->MadStatus));
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
#endif

static void OutputHeading()
{
	int i;

	printf("time");
	for (i=0; i < ArrayGetSize(&g_Columns); i++) {
		ColumnEntry_t *q = (ColumnEntry_t*)ArrayGetPtr(&g_Columns, i);
		printf(",%.*s:%u", 
				(int)(unsigned)sizeof(q->nodeDesc), q->nodeDesc,
				q->portNumber);
	}
	printf("\n");
}

boolean ColumnCompare(uint32 index, void* const elem, void *const context)
{
	ColumnEntry_t * const p = (ColumnEntry_t* const)elem;
	ColumnEntry_t * const q = (ColumnEntry_t* const)context;
	return (p->nodeGUID == q->nodeGUID && p->portNumber == q->portNumber);
}

static FSTATUS GetAndPrintGroupConfig(struct omgt_port *port, char *groupName,
 uint64 imageNumber, int32 imageOffset,
 STL_PA_IMAGE_INFO_DATA *pImageInfo)
{
	OMGT_QUERY				query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;
	// do we need to output heading line
	int heading = (ArrayGetSize(&g_Columns) == 0);
	char timestr[81];
	int i;
	STL_PA_PM_GROUP_CFG_RSP 	*q;
	uint32 index = 0;
	int bad = 0;
#if COMPUTE_DELTA
	int printValues = (! heading || ! g_deltaFlag);
#else
	int printValues = 1;
#endif

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	if (g_verbose)
		fprintf(stderr, "Getting Group Config...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
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
				pQueryResults->MadStatus, iba_sd_mad_status_msg(pQueryResults->MadStatus));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group Config Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_CONFIG_RESULTS *p = (STL_PA_GROUP_CONFIG_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_sd_mad_status_msg(pQueryResults->MadStatus));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Group Config group name %s:\n", groupName);
		}

		if (g_verbose > 2)
			PrintStlPAGroupConfig(&g_dest, 1, groupName, p->NumGroupConfigRecords, p->GroupConfigRecords);
		if (g_verbose)
#if COMPUTE_DELTA
			fprintf(stderr, "Processing Records at %s", ctime((time_t *)&pImageInfo->sweepStart));
#else
			fprintf(stderr, "Processing Records for %u sec at %s", pImageInfo->imageInterval, ctime((time_t *)&pImageInfo->sweepStart));
#endif

		// GroupConfig is unsorted in discovery order, must search and insert
		for (i=0, q = p->GroupConfigRecords; i< p->NumGroupConfigRecords; i++,q++) {
			ColumnEntry_t entry;
			if (g_gotLid && q->nodeLid != g_nodeLid)
				continue;
			if (g_gotGUID && q->nodeGUID != g_nodeGUID)
				continue;
			if (g_gotDesc && 0 != strncmp(g_nodeDesc,q->nodeDesc, sizeof(g_nodeDesc)) )
				continue;
			if (g_gotPort && q->portNumber != g_portNumber)
				continue;
			// ColumnCompare only uses GUID and portNumber
			entry.nodeGUID = q->nodeGUID;
			entry.portNumber = q->portNumber;
			// the ConfigRecords will tend to be in the same order from image
			// to image so to speed things up start close to where we
			// expect to see it
			index = ArrayFindFromIndex(&g_Columns, ColumnCompare, &entry, index);
			if (index < ArrayGetSize(&g_Columns)) {
				// found, refresh nodeLid in case it changed
				((ColumnEntry_t*)ArrayGetPtr(&g_Columns, index))->nodeLid = q->nodeLid;
			} else {
				// not found, will add at end.
				memcpy(entry.nodeDesc, q->nodeDesc, sizeof(entry.nodeDesc));
				entry.nodeLid = q->nodeLid;
#if COMPUTE_DELTA
				// save to keep code simple, but unused if ! g_deltaFlag
				entry.recentValueValid = FALSE;
				entry.recentValue = 0;
#endif
				ArraySet(&g_Columns, index, &entry, IBA_MEM_FLAG_NONE, NULL);
			}
		}
		if (heading)
			OutputHeading();
		if (printValues) {
			snprintf(timestr, sizeof(timestr), "%s", ctime((time_t *)&pImageInfo->sweepStart));
			// replace '\n' character with '\0'
			timestr[strlen(timestr) - 1] = 0;
#if COMPUTE_DELTA
			printf("%s", timestr);
#else
			printf("%s for %u sec", timestr, pImageInfo->imageInterval);
#endif
		}

		for (i=0; i < ArrayGetSize(&g_Columns); i++) {
			ColumnEntry_t *e = (ColumnEntry_t*)ArrayGetPtr(&g_Columns, i);
			STL_PORT_COUNTERS_DATA Counters;

			if (FSUCCESS != GetPortCounters(port, e->nodeLid, e->portNumber,
#if COMPUTE_DELTA
					0,
#else
					g_deltaFlag,
#endif
					0, imageNumber, imageOffset, &Counters)) {
				// assume port is down during this image
				if (printValues)
					printf(",");
				//status = FERROR;
				//goto done;
#if COMPUTE_DELTA
				// save to keep code simple, but unused if ! g_deltaFlag
				e->recentValueValid = FALSE;
#endif
			} else {
				uint64 value;
				if (g_gotXmit)
	   				value = Counters.portXmitPkts;
				else
	   				value = Counters.localLinkIntegrityErrors;
#if COMPUTE_DELTA
				if (g_deltaFlag) {
					if (e->recentValueValid) {
						if (g_gotLive) {
							if (g_gotXmit) {
								if (printValues)
	   								printf(",%lu", value - e->recentValue);
								if (value < e->recentValue)
									bad=1;
							} else {
								// LocalLinkIntegrity is cleared on port bounce
								// absolute value can go backwards
								if (printValues) {
									if (value < e->recentValue) {
	   									printf(",%lu", value);
									} else {
	   									printf(",%lu", value - e->recentValue);
									}
								}
							}
						} else {
							if (g_gotXmit) {
								if (printValues)
	   								printf(",%lu", e->recentValue - value);
								if (e->recentValue < value)
									bad=1;
							} else {
								// LocalLinkIntegrity is cleared on port bounce
								// absolute value can go backwards
								if (printValues) {
									if (e->recentValue < value) {
	   									printf(",%lu", value);
									} else {
	   									printf(",%lu", e->recentValue - value);
									}
								}
							}
						}
					} else {
						if (printValues)
							printf(",");
					}
				} else {
					if (printValues)
	   					printf(",%lu", value);
				}
				// save to keep code simple, but unused if ! g_deltaFlag
				e->recentValueValid = TRUE;
				e->recentValue = value;
#else
				if (printValues)
	   				printf(",%lu", value);
				if (g_deltaFlag && value > 18000000000000000000ULL)
					bad=1;
#endif
			}
		}
		if (printValues)
			printf("\n");
		if (bad) {
			fprintf(stderr, "Unexpected negative counter: imageNumber=%lu, offset=%d\n", imageNumber, imageOffset);
			//exit(1);
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

#if 0
static void PrintFocusPorts(uint32 select,
			STL_PA_IMAGE_INFO_DATA *pImageInfo,
			STL_FOCUS_PORTS_RSP 	*pFocusPort,
			STL_PORT_COUNTERS_DATA *pCounters,
			STL_PORT_COUNTERS_DATA *pNeighborCounters)
{
	char timestr[81];
	int reverse = 0;
	STL_PORT_COUNTERS_DATA *temp;

	snprintf(timestr, sizeof(timestr), "%s", ctime((time_t *)&pImageInfo->sweepStart));
	// replace '\n' character with '\0'
	timestr[strlen(timestr) - 1] = 0;
	// TBD - pCounters vs pNeighborCounters will be in order of larger value, we want in a predictable order so can sort or filter on a single column to see all info for a given device
	if (pNeighborCounters && 
				(pFocusPort->nodeGUID > pFocusPort->neighborGuid
					|| (pFocusPort->nodeGUID == pFocusPort->neighborGuid
						&& pFocusPort->portNumber > pFocusPort->neighborPortNumber)))
		reverse = 1;

	// HEADING: timestamp;rate;nodeDesc;port
	// TBD - include GUIDs?
	if (reverse) {
		printf("%s for %u sec;%4s;%.*s;%u;", timestr, pImageInfo->imageInterval,
				StlStaticRateToText(pFocusPort->rate),
				(int)(unsigned)sizeof(pFocusPort->neighborNodeDesc), pFocusPort->neighborNodeDesc,
				pFocusPort->neighborPortNumber);
		temp = pCounters; pCounters=pNeighborCounters;pNeighborCounters=temp;
	}else {
		printf("%s for %u sec;%4s;%.*s;%u;", timestr, pImageInfo->imageInterval,
				StlStaticRateToText(pFocusPort->rate),
				(int)(unsigned)sizeof(pFocusPort->nodeDesc), pFocusPort->nodeDesc,
				pFocusPort->portNumber);
	}


	switch (select) {
	case STL_PA_SELECT_CATEGORY_INTEG:
	// HEADING: LinkQualityIndicator;UncorectableErrors;LinkDowned;NumLanesDown;RcvErrors;ExcessiveBufferOverruns;FMConfigErrors;LinkErrorRecovery;LocalLinkIntegrity
			printf("%u;%u;%u;%u;%lu;%lu;%lu;%u;%lu",
		   		pCounters->lq.s.linkQualityIndicator,
		   		pCounters->uncorrectableErrors,
		   		pCounters->linkDowned,
				pCounters->lq.s.numLanesDown,
	   			pCounters->portRcvErrors,
	   			pCounters->excessiveBufferOverruns,
	   			pCounters->fmConfigErrors,
		   		pCounters->linkErrorRecovery,
	   			pCounters->localLinkIntegrityErrors);

		if (pNeighborCounters) {
			// HEADING: nodeDesc;port
			if (reverse) {
				printf(";%.*s;%u;",
					(int)(unsigned)sizeof(pFocusPort->nodeDesc), pFocusPort->nodeDesc,
					pFocusPort->portNumber);
			} else {
				printf(";%.*s;%u;",
					(int)(unsigned)sizeof(pFocusPort->neighborNodeDesc), pFocusPort->neighborNodeDesc,
			// HEADING: LinkQualityIndicator;UncorectableErrors;LinkDowned;NumLanesDown;RcvErrors;ExcessiveBufferOverruns;FMConfigErrors;LinkErrorRecovery;LocalLinkIntegrity
					pFocusPort->neighborPortNumber);
			}
			printf("%u;%u;%u;%u;%lu;%lu;%lu;%u;%lu",
		   		pNeighborCounters->lq.s.linkQualityIndicator,
		   		pNeighborCounters->uncorrectableErrors,
		   		pNeighborCounters->linkDowned,
				pNeighborCounters->lq.s.numLanesDown,
	   			pNeighborCounters->portRcvErrors,
	   			pNeighborCounters->excessiveBufferOverruns,
	   			pNeighborCounters->fmConfigErrors,
		   		pNeighborCounters->linkErrorRecovery,
	   			pNeighborCounters->localLinkIntegrityErrors);
		}
		printf("\n");
		break;
	default:
		printf("\n");
		break;
	}
}
#endif
#if 0
		PrintFunc(dest, "%*s%u:LID:0x%08x  Port:%u  Rate: %4s MTU: %5s nbrLID:0x%08x  nbrPort:%u\n",
				indent, "", i+1, pFocusPorts[i].nodeLid, pFocusPorts[i].portNumber,
				StlStaticRateToText(pFocusPorts[i].rate), IbMTUToText(pFocusPorts[i].mtu),
				pFocusPorts[i].neighborLid, pFocusPorts[i].neighborPortNumber);
		PrintFunc(dest, "%*s   Value:  %16"PRIu64"   nbrValue:  %16"PRIu64"\n",
				indent, "", pFocusPorts[i].value, pFocusPorts[i].neighborValue);
		PrintFunc(dest, "%*s   GUID: 0x%016"PRIx64"   nbrGuid: 0x%016"PRIx64"\n",
				indent, "", pFocusPorts[i].nodeGUID, pFocusPorts[i].neighborGuid);
		PrintFunc(dest, "%*s   Status: %s Name: %.*s\n", indent, "",
				StlFocusFlagToText(pFocusPorts[i].localFlags),
				sizeof(pFocusPorts[i].nodeDesc), pFocusPorts[i].nodeDesc);
		PrintFunc(dest, "%*s   Status: %s Neighbor Name: %.*s\n", indent, "",
				StlFocusFlagToText(pFocusPorts[i].neighborFlags),
				sizeof(pFocusPorts[i].neighborNodeDesc), pFocusPorts[i].neighborNodeDesc);
	PrintStlPAImageId(dest, indent, &pFocusPorts[0].imageId);


	PrintFunc(dest, "%*s%s controlled Port Counters (%s) for LID 0x%08x, port number %u%s:\n",
				indent, "", (flags & STL_PA_PC_FLAG_USER_COUNTERS) ? "User" : "PM",
				(flags & STL_PA_PC_FLAG_DELTA) ? "delta" : "total",
			   	nodeLid, portNumber,
				(flags&STL_PA_PC_FLAG_UNEXPECTED_CLEAR)?" (Unexpected Clear)":"");
	PrintFunc(dest, "%*sPerformance: Transmit\n", indent, "");
	PrintFunc(dest, "%*s    Xmit Data             %20"PRIu64" MB (%"PRIu64" Flits)\n",
			indent, "",
 			pPortCounters->portXmitData/FLITS_PER_MB,
			pPortCounters->portXmitData);
	PrintFunc(dest, "%*s    Xmit Pkts             %20"PRIu64"\n",
			indent, "",
			pPortCounters->portXmitPkts);
	PrintFunc(dest, "%*s    MC Xmit Pkts          %20"PRIu64"\n",
			indent, "",
			pPortCounters->portMulticastXmitPkts);
	PrintFunc(dest, "%*sPerformance: Receive\n",
			indent, "");
	PrintFunc(dest, "%*s    Rcv Data              %20"PRIu64" MB (%"PRIu64" Flits)\n",
			indent, "",
 			pPortCounters->portRcvData/FLITS_PER_MB,
			pPortCounters->portRcvData);
	PrintFunc(dest, "%*s    Rcv Pkts              %20"PRIu64"\n",
			indent, "",
			pPortCounters->portRcvPkts);
	PrintFunc(dest, "%*s    MC Rcv Pkts           %20"PRIu64"\n",
			indent, "",
			pPortCounters->portMulticastRcvPkts);
	PrintFunc(dest, "%*sSignal Integrity Errors:             \n",
			indent, "");
	PrintFunc(dest, "%*s    Link Quality Ind      %10u\n",
			indent, "",
		   	pPortCounters->lq.s.linkQualityIndicator);
	PrintFunc(dest, "%*s    Uncorrectable Err     %10u\n",
			indent, "",
		   	pPortCounters->uncorrectableErrors);
	PrintFunc(dest, "%*s    Link Downed           %10u\n",
			indent, "",
		   	pPortCounters->linkDowned);
	PrintFunc(dest, "%*s    Num Lanes Down        %10u\n",
			indent, "",
			pPortCounters->lq.s.numLanesDown);
	PrintFunc(dest, "%*s    Rcv Errors            %10u\n",
			indent, "",
	   		pPortCounters->portRcvErrors);
	PrintFunc(dest, "%*s    Exc. Buffer Overrun   %10u\n",
			indent, "",
	   		pPortCounters->excessiveBufferOverruns);
	PrintFunc(dest, "%*s    FM Config             %10u\n",
			indent, "",
	   		pPortCounters->fmConfigErrors);
	PrintFunc(dest, "%*s    Link Error Recovery   %10u\n",
			indent, "",
		   	pPortCounters->linkErrorRecovery);
	PrintFunc(dest, "%*s    Local Link Integrity  %10u\n",
			indent, "",
	   		pPortCounters->localLinkIntegrityErrors);
	PrintFunc(dest, "%*s    Rcv Rmt Phys Err      %10u\n",
			indent, "",
	   		pPortCounters->portRcvRemotePhysicalErrors);
	PrintFunc(dest, "%*sSecurity Errors:              \n",
			indent, "");
	PrintFunc(dest, "%*s    Xmit Constraint       %10u\n",
			indent, "",
	   		pPortCounters->portXmitConstraintErrors);
	PrintFunc(dest, "%*s    Rcv Constraint        %10u\n",
			indent, "",
	   		pPortCounters->portRcvConstraintErrors);
	PrintFunc(dest, "%*sRouting and Other Errors:     \n",
			indent, "");
	PrintFunc(dest, "%*s    Rcv Sw Relay Err      %10u\n",
			indent, "",
	   		pPortCounters->portRcvSwitchRelayErrors);
	PrintFunc(dest, "%*s    Xmit Discards         %10u\n",
			indent, "",
	   		pPortCounters->portXmitDiscards);
	PrintFunc(dest, "%*sCongestion:             \n",
			indent, "");
	PrintFunc(dest, "%*s    Cong Discards         %10u\n",
			indent, "",
	   		pPortCounters->swPortCongestion);
	PrintFunc(dest, "%*s    Rcv FECN              %10u\n",
			indent, "",
		   	pPortCounters->portRcvFECN);
	PrintFunc(dest, "%*s    Rcv BECN              %10u\n",
			indent, "",
		   	pPortCounters->portRcvBECN);
	PrintFunc(dest, "%*s    Mark FECN             %10u\n",
			indent, "",
		   	pPortCounters->portMarkFECN);
	PrintFunc(dest, "%*s    Xmit Time Cong        %10u\n",
			indent, "",
		   	pPortCounters->portXmitTimeCong);
	PrintFunc(dest, "%*s    Xmit Wait             %10u\n",
			indent, "",
		   	pPortCounters->portXmitWait);
	PrintFunc(dest, "%*sBubbles:	             \n",
			indent, "");
	PrintFunc(dest, "%*s    Xmit Wasted BW        %10u\n",
			indent, "",
		   	pPortCounters->portXmitWastedBW);
	PrintFunc(dest, "%*s    Xmit Wait Data        %10u\n",
			indent, "",
		   	pPortCounters->portXmitWaitData);
	PrintFunc(dest, "%*s    Rcv Bubble            %10u\n",
			indent, "",
		   	pPortCounters->portRcvBubble);
	PrintStlPAImageId(dest, indent+2, &pPortCounters->imageId);
#endif

#if 0
static FSTATUS GetAndPrintFocusPorts(struct omgt_port *port, char *groupName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset, STL_PA_IMAGE_INFO_DATA *pImageInfo)
{
	OMGT_QUERY				query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	if (g_verbose)
		fprintf(stderr, "Getting Focus Ports...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
	if (g_verbose)
		fprintf(stderr, "Query: Input=%s, Output=%s\n",
							iba_sd_query_input_type_msg(query.InputType),
							iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	// TBD - fails with insufficient resources in PA if range is maxint.  What is the limit?  Do we need to do a groupconfig query instead?  Then how do we get the neighbor information?
	status = iba_pa_multi_mad_focus_ports_response_query(port, &query, groupName, select, start, MIN(range, pImageInfo->numSwitchPorts+pImageInfo->numHFIPorts), &pQueryResults, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Focus Ports query Failed: %s (%s)\n", 0, "", iba_fstatus_msg(status), iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Focus Ports query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
				pQueryResults->MadStatus, iba_sd_mad_status_msg(pQueryResults->MadStatus));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Focus Ports Records Returned\n", 0, "");
	} else {
		STL_PA_FOCUS_PORTS_RESULTS *p = (STL_PA_FOCUS_PORTS_RESULTS*)pQueryResults->QueryResult;
		int						i;
		STL_FOCUS_PORTS_RSP 	*q;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_sd_mad_status_msg(pQueryResults->MadStatus));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Focus Ports group name %s:\n", groupName);
		}

		if (g_verbose > 2)
			PrintStlPAFocusPorts(&g_dest, 1, groupName, p->NumFocusPortsRecords, select, start, range, p->FocusPortsRecords);
		if (g_verbose)
#if COMPUTE_DELTA
			fprintf(stderr, "%s", ctime((time_t *)&pImageInfo->sweepStart));
#else
			fprintf(stderr, "%s for %u sec", ctime((time_t *)&pImageInfo->sweepStart), pImageInfo->imageInterval);
#endif
		for (i=0, q = p->FocusPortsRecords; i< p->NumFocusPortsRecords; i++,q++) {
			STL_PORT_COUNTERS_DATA Counters;
			STL_PORT_COUNTERS_DATA neighborCounters;

			// TBDif (q->value == 0 && q->neighborValue == 0)
			//	continue;	// TBD or break
			if (FSUCCESS != GetPortCounters(port, q->nodeLid, q->portNumber, 1, 0, imageNumber, imageOffset, &Counters)) {
				status = FERROR;
				goto done;
			}
			if (q->neighborLid) { // has a neighbor
				if (FSUCCESS != GetPortCounters(port, q->neighborLid, q->neighborPortNumber, 1, 0, imageNumber, imageOffset, &neighborCounters)) {
					status = FERROR;
					goto done;
				}
			}
			PrintFocusPorts(select, pImageInfo, q, &Counters,
							q->neighborLid?&neighborCounters:NULL);
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
#endif

static FSTATUS GetImageInfo(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, STL_PA_IMAGE_INFO_DATA* pImageInfo)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_INFO_DATA		request = {{0}};
    STL_PA_IMAGE_INFO_DATA		*response;;

	if (g_verbose)
    	fprintf(stderr, "Getting image info... 0x%lx %d\n", imageNumber, imageOffset);
	request.imageId.imageNumber = imageNumber;
	request.imageId.imageOffset = imageOffset;
    if ((response = iba_pa_multi_mad_get_image_info_response_query(port, &request)) != NULL) {
        status = FSUCCESS;
		*pImageInfo = *response;
		if (g_verbose > 1)
			PrintStlPAImageInfo(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive GetImageInfo response: %s\n", iba_pa_mad_status_msg(port));
		if (omgt_get_pa_mad_status(port) == STL_MAD_STATUS_STL_PA_NO_IMAGE)
			status = FNOT_FOUND;
    }
	if (response)
		MemoryDeallocate(response);
    return status;
}

#if 0
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
			   	pQueryResults->MadStatus, iba_sd_mad_status_msg(pQueryResults->MadStatus));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF List Records Returned\n", 0, "");
	} else {
		STL_PA_VF_LIST_RESULTS *p = (STL_PA_VF_LIST_RESULTS *)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
					   					iba_sd_mad_status_msg(pQueryResults->MadStatus));
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
#endif

#if 0
static FSTATUS GetVFInfo(struct omgt_port *port, char *vfName, uint64 imageNumber, int32 imageOffset)
{
	OMGT_QUERY				query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA	imageId = {0};

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Info...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
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
			   	pQueryResults->MadStatus, iba_sd_mad_status_msg(pQueryResults->MadStatus));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Info Records Returned\n", 0, "");
	} else {
		STL_PA_VF_INFO_RESULTS *p = (STL_PA_VF_INFO_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
					   					iba_sd_mad_status_msg(pQueryResults->MadStatus));
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
#endif

#if 0
static FSTATUS GetVFConfig(struct omgt_port *port, char *vfName, uint64 imageNumber, int32 imageOffset)
{
	OMGT_QUERY				query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Config...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
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
				pQueryResults->MadStatus, iba_sd_mad_status_msg(pQueryResults->MadStatus));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Config Records Returned\n", 0, "");
	} else {
		STL_PA_VF_CONFIG_RESULTS *p = (STL_PA_VF_CONFIG_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_sd_mad_status_msg(pQueryResults->MadStatus));
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
#endif

#if 0
static FSTATUS GetVFPortCounters(struct omgt_port *port, STL_LID nodeLid, uint8_t portNumber, uint32_t deltaFlag, uint32_t userCntrsFlag, char *vfName, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
    STL_PA_VF_PORT_COUNTERS_DATA		*response;

    fprintf(stderr, "Getting Port Counters...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
    if ((response = iba_pa_single_mad_vf_port_counters_response_query(port, nodeLid, portNumber, deltaFlag, userCntrsFlag, vfName, &imageId)) != NULL) {
        status = FSUCCESS;
		PrintStlPAVFPortCounters(&g_dest, 0, response, nodeLid, (uint32)portNumber, response->flags);
    } else {
        fprintf(stderr, "Failed to receive GetVFPortCounters response: %s\n", iba_pa_mad_status_msg(port));
    }

	if (response)
		MemoryDeallocate(response);
    return status;
}
#endif

#if 0
static FSTATUS GetVFFocusPorts(struct omgt_port *port, char *vfName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset)
{
	OMGT_QUERY				query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Focus Ports...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
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
				pQueryResults->MadStatus, iba_sd_mad_status_msg(pQueryResults->MadStatus));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Focus Ports Records Returned\n", 0, "");
	} else {
		STL_PA_VF_FOCUS_PORTS_RESULTS *p = (STL_PA_VF_FOCUS_PORTS_RESULTS*)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_sd_mad_status_msg(pQueryResults->MadStatus));
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
#endif

void usage(void)
{
	fprintf(stderr, "Usage: opapaextract [-v] [-h hfi] [-p port] [-g groupName]\n");
	fprintf(stderr, "                   [-l nodeLid] [-G nodeGUID] [-D desc] [-P portNumber]\n");
	fprintf(stderr, "                   [-S start] [-r range] [-L rate] [-d delta] [-x]\n");
	//fprintf(stderr, "Usage: opapaquery [-v] [-h hfi] [-p port] -o type [-g groupName] [-l nodeLid]\n");
	//fprintf(stderr, "                   [-P portNumber] [-d delta] [-U] [-s select] [-f focus]\n");
	//fprintf(stderr, "                   [-S start] [-r range] [-n imgNum] [-O imgOff] [-m moveImgNum]\n");
	//fprintf(stderr, "                   [-M moveImgOff] [-V vfName]\n");
	fprintf(stderr, "    --help             - display this help text\n");
	fprintf(stderr, "    -v/--verbose       - verbose output\n");
	fprintf(stderr, "    -h/--hfi hfi       - hfi, numbered 1..n, 0= -p port will be a system wide\n");
	fprintf(stderr, "                         port num (default is 0)\n");
	fprintf(stderr, "    -p/--port port     - port, numbered 1..n, 0=1st active (default is 1st\n");
	fprintf(stderr, "                         active)\n");
	//fprintf(stderr, "    -o/--output        - output type, default is groupList\n");
	fprintf(stderr, "    -g/--groupName     - PM group to query, default is 1st defined group (All)\n");
	fprintf(stderr, "    -l/--lid           - lid of node for portCounters query\n");
	fprintf(stderr, "    -G/--guid          - nodeGUID of node for portCounters query\n");
	fprintf(stderr, "    -D/--desc          - node desc of node for portCounters query\n");
	fprintf(stderr, "    -P/--portNumber    - port number for portCounters query\n");
	fprintf(stderr, "    -d/--delta         - delta flag for portCounters query - 0 or 1 (default 1)\n");
	fprintf(stderr, "    -x/--xmit          - get xmit counters (default is linkintegrity)\n");
	//fprintf(stderr, "    -U/--userCntrs     - user controlled counters flag for portCounters query\n");
	//fprintf(stderr, "     -f/--focus         - focus select value for getting focus ports\n");
	//fprintf(stderr, "           focus select values:\n");
	//fprintf(stderr, "           utilhigh      - sorted by utilization - highest first\n");                  // STL_PA_SELECT_UTIL_HIGH         0x00020001
	//fprintf(stderr, "           pktrate       - sorted by packet rate - highest first\n");                  // STL_PA_SELECT_UTIL_PKTS_HIGH    0x00020082
	//fprintf(stderr, "           utillow       - sorted by utilization - lowest first\n");                   // STL_PA_SELECT_UTIL_LOW          0x00020101
	//fprintf(stderr, "           integrity     - sorted by integrity errors - highest first\n");             // STL_PA_SELECT_CATEGORY_INTEG         0x00030001
	//fprintf(stderr, "           congestion    - sorted by congestion errors - highest first\n");            // STL_PA_SELECT_CATEGORY_CONG          0x00030002
	//fprintf(stderr, "           smacongestion - sorted by sma congestion errors - highest first\n");        // STL_PA_SELECT_CATEGORY_SMA_CONG      0x00030003
	//fprintf(stderr, "           bubbles       - sorted by bubble errors - highest first\n");                // STL_PA_SELECT_CATEGORY_BUBBLE        0x00030004
	//fprintf(stderr, "           security      - sorted by security errors - highest first\n");              // STL_PA_SELECT_CATEGORY_SEC           0x00030005
	//fprintf(stderr, "           routing       - sorted by routing errors - highest first\n");               // STL_PA_SELECT_CATEGORY_ROUT          0x00030006
	//fprintf(stderr, "    -S/--start           - start of window for focus ports - should always be 0\n");
	//fprintf(stderr, "                           for now\n");
	//fprintf(stderr, "    -r/--range           - size of window for focus ports list\n");
	fprintf(stderr, "    -S/--start         - start image, default is 0 (current time)\n");
	fprintf(stderr, "    -r/--range         - number of images to output, default is all\n");
	fprintf(stderr, "    -L/--live          - get live data at given rate in seconds\n");
	//fprintf(stderr, "    -n/--imgNum          - 64-bit image number - may be used with groupInfo,\n");
	//fprintf(stderr, "                           groupConfig, portCounters (delta)\n");
	//fprintf(stderr, "    -O/--imgOff          - image offset - may be used with groupInfo, groupConfig,\n");
	//fprintf(stderr, "                           portCounters (delta)\n");
	//fprintf(stderr, "    -m/--moveImgNum      - 64-bit image number - used with moveFreeze to move a\n");
	//fprintf(stderr, "                           freeze image\n");
	//fprintf(stderr, "    -M/--moveImgOff      - image offset - may be used with moveFreeze to move a\n");
	//fprintf(stderr, "                           freeze image\n");
	//fprintf(stderr, "    -V/--vfName          - VF name for vfInfo query\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");
	fprintf(stderr, "\n");
	//fprintf(stderr, "Usage examples:\n");
	//fprintf(stderr, "    opapaquery -o classPortInfo\n");
	//fprintf(stderr, "    opapaquery -o groupList\n");
	//fprintf(stderr, "    opapaquery -o groupInfo -g All\n");
	//fprintf(stderr, "    opapaquery -o groupConfig -g All\n");
	//fprintf(stderr, "    opapaquery -o portCounters -l 1 -P 1 -d 1\n");
	//fprintf(stderr, "    opapaquery -o portCounters -l 1 -P 1 -d 1 -n 0x20000000d02 -O 1\n");
	//fprintf(stderr, "    opapaquery -o clrPortCounters -l 1 -P 1 -s 0xC0000000\n");
	//fprintf(stderr, "        (clears XmitData & RcvData)\n");
	//fprintf(stderr, "    opapaquery -o clrAllPortCounters -s 0xC0000000\n");
	//fprintf(stderr, "        (clears XmitData & RcvData on all ports)\n");
	//fprintf(stderr, "    opapaquery -o pmConfig\n");
	//fprintf(stderr, "    opapaquery -o freezeImage -n 0x20000000d02\n");
	//fprintf(stderr, "    opapaquery -o releaseImage -n 0xd01\n");
	//fprintf(stderr, "    opapaquery -o renewImage -n 0xd01\n");
	//fprintf(stderr, "    opapaquery -o moveFreeze -n 0xd01 -m 0x20000000d02 -M -2\n");
	//fprintf(stderr, "    opapaquery -o focusPorts -g All -f integrity -S 0 -r 20\n");
	//fprintf(stderr, "    opapaquery -o imageInfo -n 0x20000000d02\n");
	//fprintf(stderr, "    opapaquery -o vfList\n");
	//fprintf(stderr, "    opapaquery -o vfInfo -V Default\n");
	//fprintf(stderr, "    opapaquery -o vfConfig -V Default\n");
	//fprintf(stderr, "    opapaquery -o vfPortCounters -l 1 -P 1 -d 1 -V Default\n");
	//fprintf(stderr, "    opapaquery -o clrVfPortCounters -l 1 -P 1 -s 0xC0000000\n");
	//fprintf(stderr, "        (clears VLXmitData & VLRcvData)\n");
	//fprintf(stderr, "    opapaquery -o vfFocusPorts -V Default -f integrity -S 0 -r 20\n");

	exit(2);
}

#if 0
typedef struct OutputFocusMap {
	char *string;
	int32 focus;
	} OutputFocusMap_t;

OutputFocusMap_t OutputFocusTable[]= {
	{"utilhigh",        STL_PA_SELECT_UTIL_HIGH },        // 0x00020001
	{"pktrate",         STL_PA_SELECT_UTIL_PKTS_HIGH },   // 0x00020082
	{"utillow",         STL_PA_SELECT_UTIL_LOW },         // 0x00020101
	{"integrity",       STL_PA_SELECT_CATEGORY_INTEG },        // 0x00030001
	{"congestion",      STL_PA_SELECT_CATEGORY_CONG },         // 0x00030002
	{"smacongestion",   STL_PA_SELECT_CATEGORY_SMA_CONG },     // 0x00030003
	{"bubbles",	        STL_PA_SELECT_CATEGORY_BUBBLE },       // 0x00030004
	{"security" ,       STL_PA_SELECT_CATEGORY_SEC },          // 0x00030005
	{"routing",	        STL_PA_SELECT_CATEGORY_ROUT },         // 0x00030006
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
#endif

int main(int argc, char ** argv)
{
    FSTATUS fstatus;
    int c, index;
    uint8 hfi = 0;
    uint8 port = 0;
	uint32 temp32;
	uint8 temp8;
	STL_PA_IMAGE_ID_DATA imageId;
	STL_PA_IMAGE_INFO_DATA ImageInfo;

	// start at current image
	imageId.imageNumber		= PACLIENT_IMAGE_CURRENT;
	imageId.imageOffset		= 0;

	PrintDestInitFile(&g_dest, stderr);

	Top_setcmdname("opapaquery");

	while (-1 != (c = getopt_long(argc,argv, "vh:p:o:g:l:P:G:D:d:xUs:n:O:f:S:r:L:m:M:V:", options, &index)))
    {
        switch (c)
        {
            case 'v':
                g_verbose++;
				if (g_verbose>4) umad_debug(g_verbose-4);
                break;

            case '$':
                usage();
                break;

	    	case 'h':
				if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid HFI Number: %s\n", optarg);
					usage();
				}
				break;

			case 'p':
				if (FSUCCESS != StringToUint8(&port, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Port Number: %s\n", optarg);
					usage();
				}
				break;

	    	case 'g':
				snprintf(g_groupName, STL_PM_GROUPNAMELEN, "%s", optarg);
				g_gotGroup = 1;
				break;

			case 'l':
				if (FSUCCESS != StringToUint32(&temp32, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Lid Number: %s\n", optarg);
					usage();
				}
				g_nodeLid = (STL_LID)temp32;
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
				if (FSUCCESS != StringToUint64(&g_nodeGUID, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid GUID Number: %s\n", optarg);
					usage();
				}
				g_gotGUID = TRUE;
				break;

			case 'D':
				snprintf(g_nodeDesc, sizeof(g_nodeDesc), "%s", optarg);
				g_gotDesc = TRUE;
				break;

			case 'd':
				if (FSUCCESS != StringToUint32(&g_deltaFlag, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Delta Flag: %s\n", optarg);
					usage();
				}
				break;

			case 'x':
				g_gotXmit = TRUE;
				break;

			//case 'U':
			//	g_userCntrsFlag = 1;
			//	break;

			//case 'n':
			//	if (FSUCCESS != StringToUint64(&imageId.imageNumber, optarg, NULL, 0, TRUE)) {
			//		fprintf(stderr, "opapaquery: Invalid Image Number: %s\n", optarg);
			//		usage();
			//	}
			//	g_gotImgNum = TRUE;
			//	break;

			//case 'O':
			//	if (FSUCCESS != StringToInt32(&imageId.imageOffset, optarg, NULL, 0, TRUE)) {
			//		fprintf(stderr, "opapaquery: Invalid Image Offset: %s\n", optarg);
			//		usage();
			//	}
			//	g_gotImgOff = TRUE;
			//	break;

			//case 'f':
			//	if (FSUCCESS != StringToFocus (&g_focus, optarg)) {
			//		fprintf(stderr, "opapaquery: Invalid Focus Number: %s\n", optarg);
			//		usage();
			//	}
			//	g_gotFocus = TRUE;
			//	break;

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

			case 'L':
				//fprintf(stderr, "opapaquery: -L option not working yet\n");
				//usage();
				if (FSUCCESS != StringToUint32(&g_liveRate, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "opapaquery: Invalid Range Number: %s\n", optarg);
					usage();
				}
				g_gotLive = TRUE;
				break;

			//case 'V':
			//	snprintf(g_vfName, STL_PM_VFNAMELEN, "%s", optarg);
			//	g_gotvfName = TRUE;
			//	break;

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

	// Check parameter consistency - TBD
	if (g_gotStart && g_gotLive)
	{
		fprintf(stderr, "opapaquery: -L option cannot be used with -S\n");
		usage();
	}

	if (g_gotLive && g_liveRate == 0)
	{
		fprintf(stderr, "opapaquery: -L option must be non-zero\n");
		usage();
	}


    // initialize connections to OPA related entities 
    if ((fstatus = opa_pa_init(hfi, port)) != FSUCCESS) {
		fprintf(stderr, "opapaquery: failed to initialize OPA PA layer - status = %d\n", fstatus);
        exit(-1);
	}

	if (g_verbose > 3)
		set_opapaquery_debug(g_portHandle);

	ArrayInitState(&g_Columns);
	ArrayInit(&g_Columns, 0, 50, sizeof(ColumnEntry_t), IBA_MEM_FLAG_NONE);

	if (! g_gotGroup) {
		// default to 1st group name which should be "All"
		if (FSUCCESS != GetGroupList(g_portHandle, g_groupName))
			goto exit;
		fprintf(stderr, "Using Group: %s\n", g_groupName);
	}
	if (g_gotStart) {
		imageId.imageOffset = -g_start;
	}
		
	//if (! g_gotFocus) {
	//	g_focus = STL_PA_SELECT_CATEGORY_INTEG;
	//	fprintf(stderr, "Using Focus: integrity\n");
	//}
	if (! g_gotRange) {
		if (g_gotLive)
			g_range = 100;	// pick a reasonable fixed limit
		else
			g_range = IB_INT32_MAX;
		fprintf(stderr, "Using range: %d\n", g_range);
	}

	// we get image so we can use canonical imageId for freeze and
	// subsequent access inside loop, this way our initial query using
	// an offset relative to current won't unexpectedly move on us if a 
	// PM sweep occurs
	if (FSUCCESS != GetImageInfo(g_portHandle, imageId.imageNumber, imageId.imageOffset, &ImageInfo))
		goto exit;
#if USE_ABS_IMAGENUM
	imageId.imageNumber = ImageInfo.imageId.imageNumber;
	imageId.imageOffset = ImageInfo.imageId.imageOffset;
#endif

#if USE_FREEZE
	if (FSUCCESS != FreezeImage(g_portHandle, imageId.imageNumber, imageId.imageOffset, &imageId))
		goto exit;
	//fprintf(stderr, "Froze %lu, %d\n", imageId.imageNumber, imageId.imageOffset);
#endif

	do {
		if (g_range % 10 == 0)
#if COMPUTE_DELTA
			fprintf(stderr, "Processing Records for %s", ctime((time_t *)&ImageInfo.sweepStart));
#else
			fprintf(stderr, "Processing Records for %u sec at %s", ImageInfo.imageInterval, ctime((time_t *)&ImageInfo.sweepStart));
#endif

		if (FSUCCESS != GetAndPrintGroupConfig(g_portHandle, g_groupName, imageId.imageNumber, imageId.imageOffset, &ImageInfo))
			goto exit;
#if 0
		if (FSUCCESS != GetAndPrintFocusPorts(g_portHandle, g_groupName, g_focus, g_start, g_range, imageId.imageNumber, imageId.imageOffset, &ImageInfo))
			goto exit;
#endif
#if USE_FREEZE
		//fprintf(stderr, "Release %lu, %d\n", imageId.imageNumber, imageId.imageOffset);
		if (g_gotLive) {
			(void)ReleaseImage(g_portHandle, imageId.imageNumber, imageId.imageOffset);
			sleep(g_liveRate);
		} else {
			fstatus = MoveFreeze(g_portHandle, imageId.imageNumber, imageId.imageOffset, imageId.imageNumber, imageId.imageOffset-1, &imageId);
			// TBD - should we use unfreeze and freeze in case our lease timed out?
 			if (FNOT_FOUND == fstatus)
				break;
			else if (FSUCCESS != fstatus)
				goto release;
		}
		//fprintf(stderr, "Froze %lu, %d\n", imageId.imageNumber, imageId.imageOffset);
#else
		if (g_gotLive) {
			sleep(g_liveRate);
		} else {
			imageId.imageOffset -= 1;
		}
#endif
		if (g_gotLive) {
			imageId.imageNumber		= PACLIENT_IMAGE_CURRENT;
			imageId.imageOffset		= 0;
		}
		fstatus = GetImageInfo(g_portHandle, imageId.imageNumber, imageId.imageOffset, &ImageInfo);
		if (FSUCCESS != fstatus && FNOT_FOUND != fstatus)
			goto release;
#if USE_ABS_IMAGENUM
		if (FSUCCESS == fstatus) {
			imageId.imageNumber = ImageInfo.imageId.imageNumber;
			imageId.imageOffset = ImageInfo.imageId.imageOffset;
		}
#endif
#if USE_FREEZE
		if (g_gotLive) {
			fstatus = FreezeImage(g_portHandle, imageId.imageNumber, imageId.imageOffset, &imageId);
			if (FSUCCESS != fstatus)
				break;
		}
#endif
	} while (FSUCCESS == fstatus && --g_range);
#if USE_FREEZE
	//fprintf(stderr, "Release %lu, %d\n", imageId.imageNumber, imageId.imageOffset);
	(void)ReleaseImage(g_portHandle, imageId.imageNumber, imageId.imageOffset);
#endif
	OutputHeading();
	ArrayDestroy(&g_Columns);
	omgt_close_port(g_portHandle);
	g_portHandle = NULL;
	exit(0);

release:
#if USE_FREEZE
	//fprintf(stderr, "Release %lu, %d\n", imageId.imageNumber, imageId.imageOffset);
	(void)ReleaseImage(g_portHandle, imageId.imageNumber, imageId.imageOffset);
#endif
exit:
	OutputHeading();
	ArrayDestroy(&g_Columns);
	omgt_close_port(g_portHandle);
	g_portHandle = NULL;
	exit(1);
}

// TBD - ideas for output
// - allow a -o option to cause output to be similar to:
// opaextracterror - 1 line per port, error counters only
// opaextractperf - 1 line per port, all counters
// opaextractsat - 1 line per linke, LQI and cable/link info (no)
// opaextrastat2 - 1 line per link, link info and all counters per port
//
// focus could limit to ports with non-zero for that focus
// allow focus based on existing categories as well as some individual counters or custom categories
// per VF (mainly useful for data movement or congestion, skip for now)
//
// how to organize output
// one spreadsheet for all info, use can filter and sort
// one csv per device ( tricky since list of devices is different per image)
//
