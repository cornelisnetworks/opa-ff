/* BEGIN_ICS_COPYRIGHT5 ****************************************

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

 * ** END_ICS_COPYRIGHT5   ****************************************/
/* [ICS VERSION STRING: unknown] */
#include <stdio.h>
#include "stl_pa_priv.h"
#include "stl_pm.h"
#include "ib_mad.h"
#include "stl_print.h"
#include "opamgt_pa_priv.h"
#include "fe_pa.h"

#define min(a,b) (((a)<(b))?(a):(b))

#define STL_MAX_PORT_CONFIG_DATA_SIZE (sizeof(STL_PA_PM_GROUP_CFG_RSP) * 2048)
#define STL_MAX_GROUP_LIST_SIZE (sizeof(STL_PA_GROUP_LIST) * (STL_PM_MAX_GROUPS+1))
#define PA_REQ_HDR_SIZE (sizeof(MAD_COMMON) + sizeof(SA_MAD_HDR))

extern PrintDest_t g_dest;
/**
 *  Retrieve group list and print resulting records
 *
 * @param port                      omgt_port to send through
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetGroupList(struct omgt_port *port)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType  = InputTypeNoInput;
	query.OutputType = OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group List...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_pa_query_input_type_msg(query.InputType),
			iba_pa_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_list_response_query(port, &query, &pQueryResults);
	if (!pQueryResults) {
		fprintf(stderr, "%*sPA GroupList query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA GroupList query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status), pQueryResults->MadStatus,
			iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group List Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_LIST_RESULTS *p = (STL_PA_GROUP_LIST_RESULTS *)pQueryResults->QueryResult;

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
	if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Retrieve group info and print resulting records
 *
 * @param port                      omgt_port to send through
 * @param group_name                Group name 
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetGroupInfo(struct omgt_port *port, char *groupName, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {
		.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = imageTime
	};

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType  = InputTypeNoInput;
	query.OutputType = OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group Info...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_pa_query_input_type_msg(query.InputType),
			iba_pa_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_stats_response_query(port, &query, groupName, &pQueryResults, &imageId);
	if (!pQueryResults) {
		fprintf(stderr, "%*sPA Group Info query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Group Info query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status), pQueryResults->MadStatus,
			iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group Info Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_INFO_RESULTS *p = (STL_PA_GROUP_INFO_RESULTS *)pQueryResults->QueryResult;

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
	if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Retrieve group config for a select group(s) and print the resulting records
 *
 * @param port                      omgt_port to send through
 * @param group_name                Group name 
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetGroupConfig(struct omgt_port *port, char *groupName, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {
		.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = imageTime
	};

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType  = InputTypeNoInput;
	query.OutputType = OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group Config...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_pa_query_input_type_msg(query.InputType),
			iba_pa_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_group_config_response_query(port, &query, groupName, &pQueryResults, &imageId);
	if (!pQueryResults) {
		fprintf(stderr, "%*sPA Group Config query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Group Config query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status), pQueryResults->MadStatus,
			iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Group Config Records Returned\n", 0, "");
	} else {
		STL_PA_GROUP_CONFIG_RESULTS *p = (STL_PA_GROUP_CONFIG_RESULTS *)pQueryResults->QueryResult;

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
	if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Retrieve port counters for a specific port and print resulting record
 *
 * @param port                      omgt_port to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param port                      omgt_port to send through
 * @param delta 					Whether to get the delta since previous image
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetPortCounters(struct omgt_port *port, uint32_t nodeLid, uint8_t portNumber, uint32_t deltaFlag,
	uint32_t userCtrsFlag, uint64 imageNumber, int32 imageOffset, uint32_t begin, uint32_t end)
{
	FSTATUS status = FERROR;
	STL_PORT_COUNTERS_DATA *response = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {
		.imageNumber = (end ? PACLIENT_IMAGE_TIMED : imageNumber),
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = end
	};

	fprintf(stderr, "Getting Port Counters...\n");

	if (begin) {
		STL_PA_IMAGE_ID_DATA beginQuery = {0}, endQuery = {0};

		beginQuery.imageNumber = PACLIENT_IMAGE_TIMED;
		beginQuery.imageTime.absoluteTime = begin;

		endQuery.imageNumber = end ? PACLIENT_IMAGE_TIMED : PACLIENT_IMAGE_CURRENT;
		endQuery.imageTime.absoluteTime = end;

		STL_PORT_COUNTERS_DATA *beginCounters = NULL, *endCounters = NULL;

		if ((beginCounters = iba_pa_single_mad_port_counters_response_query(port, nodeLid, portNumber, 0, 0, &beginQuery)) != NULL
			&& (endCounters = iba_pa_single_mad_port_counters_response_query(port, nodeLid, portNumber, 0, 0, &endQuery)) != NULL) {

			CounterSelectMask_t clearedCounters = DiffPACounters(endCounters, beginCounters, endCounters);
			if (clearedCounters.AsReg32) {
				char counterBuf[128];
				FormatStlCounterSelectMask(counterBuf, clearedCounters);
				fprintf(stderr, "Counters reset, reporting latest count: %s\n", counterBuf);
			}
			status = FSUCCESS;
			PrintStlPAPortCounters(&g_dest, 0, endCounters, (uint32)nodeLid, (uint32)portNumber, 0);
		} else {
			fprintf(stderr, "Failed to receive GetPortCounters response\n");
		}

		if (endCounters)
			MemoryDeallocate(endCounters);
		if (beginCounters)
			MemoryDeallocate(beginCounters);
	} else if ((response = iba_pa_single_mad_port_counters_response_query(port, nodeLid, portNumber, deltaFlag, userCtrsFlag, &imageId)) != NULL) {
		status = FSUCCESS;
		PrintStlPAPortCounters(&g_dest, 0, response, (uint32)nodeLid, (uint32)portNumber, response->flags);
	} else {
		fprintf(stderr, "Failed to receive GetPortCounters response\n");
	}

	if (response)
		MemoryDeallocate(response);
	return status;
}

/**
 *  Clear port counters for a specific port 
 *
 * @param port                      omgt_port to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param port                      omgt_port to send through
 * @param selectFlag 				Counter select mask
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_ClrPortCounters(struct omgt_port *port, uint32_t nodeLid, uint8_t portNumber, uint32_t selectFlag)
{
	FSTATUS status = FERROR;
	STL_CLR_PORT_COUNTERS_DATA *response = NULL;

	fprintf(stderr, "Clearing Port Counters...\n");
	if ((response = iba_pa_single_mad_clr_port_counters_response_query(port, nodeLid, portNumber, selectFlag)) != NULL) {
		status = FSUCCESS;
		fprintf(stderr, "Port Counters successfully cleared for node %d port number %d select = 0x%04x\n", nodeLid, portNumber, selectFlag);
	} else {
		fprintf(stderr, "Failed to receive ClrPortCounters response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Clear port counters for all ports 
 *
 * @param port                      omgt_port to send through
 * @param selectFlag 				Counter select mask
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_ClrAllPortCounters(struct omgt_port *port, uint32_t selectFlag)
{
	FSTATUS status = FERROR;
	STL_CLR_ALL_PORT_COUNTERS_DATA *response = NULL;

	fprintf(stderr, "Clearing All Port Counters...\n");
	if ((response = iba_pa_single_mad_clr_all_port_counters_response_query(port, selectFlag)) != NULL) {
		status = FSUCCESS;
		fprintf(stderr, "All Port Counters successfully cleared for select = 0x%04x\n", selectFlag);
	} else {
		fprintf(stderr, "Failed to receive ClrAllPortCounters response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Get the current PM configuration settings
 *
 * @param port                      omgt_port to send through
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetPMConfig(struct omgt_port *port)
{
	FSTATUS status = FERROR;
	STL_PA_PM_CFG_DATA *response = NULL;

	fprintf(stderr, "Getting PM Configuration...\n");
	if ((response = iba_pa_single_mad_get_pm_config_response_query(port)) != NULL) {
		status = FSUCCESS;
		PrintStlPMConfig(&g_dest, 0, response);
	} else {
		fprintf(stderr, "Failed to receive GetPMConfig response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Freeze an image within the PM
 *
 * @param port                      omgt_port to send through
 * @param imageNumber 				Image number to freeze
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_FreezeImage(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	FSTATUS status = FERROR;
	STL_PA_IMAGE_ID_DATA *response = NULL;
	STL_PA_IMAGE_ID_DATA request = {
		.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = imageTime
	};

	fprintf(stderr, "Freezing image...\n");
	if ((response = iba_pa_single_mad_freeze_image_response_query(port, &request)) != NULL) {
		status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
	} else {
		fprintf(stderr, "Failed to receive FreezeImage response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Release (unfreeze) an image within the PM
 *
 * @param port                      omgt_port to send through
 * @param imageNumber 				Image number to freeze
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_ReleaseImage(struct omgt_port *port, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS status = FERROR;
	STL_PA_IMAGE_ID_DATA *response = NULL;
	STL_PA_IMAGE_ID_DATA request = {
		.imageNumber = imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = 0
	};

	fprintf(stderr, "Releasing image...\n");
	if ((response = iba_pa_single_mad_release_image_response_query(port, &request)) != NULL) {
		status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
	} else {
		fprintf(stderr, "Failed to receive ReleaseImage response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Renew an image within the PM
 *
 * @param port                      omgt_port to send through
 * @param imageNumber 				Image number to freeze
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_RenewImage(struct omgt_port *port, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS status = FERROR;
	STL_PA_IMAGE_ID_DATA *response = NULL;
	STL_PA_IMAGE_ID_DATA request = {
		.imageNumber = imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = 0
	};

	fprintf(stderr, "Renewing image...\n");
	if ((response = iba_pa_single_mad_renew_image_response_query(port, &request)) != NULL) {
		status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
	} else {
		fprintf(stderr, "Failed to receive RenewImage response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Renew an image within the PM
 *
 * @param port                      omgt_port to send through
 * @param imageNumber 				Frozen image number
 * @param imageOffset 				Image offset
 * @param moveImageNumber 			New image number to move to
 * @param moveImageOffset 			New image offset to move to
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_MoveFreeze(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint64 moveImageNumber, int32 moveImageOffset)
{
	FSTATUS status = FERROR;
	STL_MOVE_FREEZE_DATA *response = NULL;
	STL_MOVE_FREEZE_DATA request = {
		.oldFreezeImage.imageNumber = imageNumber,
		.oldFreezeImage.imageOffset = imageOffset,
		.newFreezeImage.imageNumber = moveImageNumber,
		.newFreezeImage.imageOffset = moveImageOffset
	};

	fprintf(stderr, "Moving freeze image...\n");
	if ((response = iba_pa_single_mad_move_freeze_response_query(port, &request)) != NULL) {
		status = FSUCCESS;
		PrintStlPAMoveFreeze(&g_dest, 0, response);
	} else {
		fprintf(stderr, "Failed to receive MoveFreeze response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Retrieve the focus port list for given parameters
 *
 * @param port                      omgt_port to send through
 * @param group_name                Group name 
 * @param select                	Select field for focus portlist
 * @param start                     Start index value of portlist 
 * @param range                     Index range of portlist. 
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetFocusPorts(struct omgt_port *port, char *groupName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {
		.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = imageTime
	};

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType  = InputTypeNoInput;
	query.OutputType = OutputTypePaTableRecord;

	fprintf(stderr, "Getting Focus Ports...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_pa_query_input_type_msg(query.InputType),
			iba_pa_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_focus_ports_response_query(port, &query, groupName, select, start, range, &pQueryResults, &imageId);
	if (!pQueryResults) {
		fprintf(stderr, "%*sPA Focus Ports query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA Focus Ports query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status), pQueryResults->MadStatus,
			iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Focus Ports Records Returned\n", 0, "");
	} else {
		STL_PA_FOCUS_PORTS_RESULTS *p = (STL_PA_FOCUS_PORTS_RESULTS *)pQueryResults->QueryResult;

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
				iba_pa_mad_status_msg(port));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Focus Ports group name %s:\n", groupName);
		}

		PrintStlPAFocusPorts(&g_dest, 1, groupName, p->NumFocusPortsRecords, select, start, range, p->FocusPortsRecords);
	}

	status = FSUCCESS;

done:
	if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Retrieve information about an image
 *
 * @param port                      omgt_port to send through
 * @param imageNumber 				Frozen image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetImageInfo(struct omgt_port *port, uint64 imageNumber, int32 imageOffset, uint32_t imageTime)
{
	FSTATUS status = FERROR;
	STL_PA_IMAGE_INFO_DATA *response = NULL;
	STL_PA_IMAGE_INFO_DATA request = {
		.imageId.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber,
		.imageId.imageOffset = imageOffset,
		.imageId.imageTime.absoluteTime = imageTime
	};

	fprintf(stderr, "Getting image info...\n");
	if ((response = iba_pa_multi_mad_get_image_info_response_query(port, &request)) != NULL) {
		status = FSUCCESS;
		PrintStlPAImageInfo(&g_dest, 0, response);
	} else {
		fprintf(stderr, "Failed to receive GetImageInfo response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Retrieve the list of virtual fabrics
 *
 * @param port                      omgt_port to send through
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetVFList(struct omgt_port *port)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType  = InputTypeNoInput;
	query.OutputType = OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF List...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_pa_query_input_type_msg(query.InputType),
			iba_pa_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_list_response_query(port, &query, &pQueryResults);

	if (!pQueryResults) {
		fprintf(stderr, "%*sPA vfList query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA vfList query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status), pQueryResults->MadStatus,
			iba_pa_mad_status_msg(port));
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
	if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Retrieve info about a particular virtual fabric
 *
 * @param port                      omgt_port to send through
 * @param vfName 					Virtual fabric name
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetVFInfo(struct omgt_port *port, char *vfName, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {
		.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = imageTime
	};

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType  = InputTypeNoInput;
	query.OutputType = OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Info...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_pa_query_input_type_msg(query.InputType),
			iba_pa_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_info_response_query(port, &query, vfName, &pQueryResults, &imageId);

	if (!pQueryResults) {
		fprintf(stderr, "%*sPA VF Info query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA VF Info query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status), pQueryResults->MadStatus,
			iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Info Records Returned\n", 0, "");
	} else {
		STL_PA_VF_INFO_RESULTS *p = (STL_PA_VF_INFO_RESULTS *)pQueryResults->QueryResult;

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
	if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Retrieve the configuration of a particular virtual fabric
 *
 * @param port                      omgt_port to send through
 * @param vfName 					Virtual fabric name
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetVFConfig(struct omgt_port *port, char *vfName, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {
		.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = imageTime
	};

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType  = InputTypeNoInput;
	query.OutputType = OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Config...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_pa_query_input_type_msg(query.InputType),
			iba_pa_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_config_response_query(port, &query, vfName, &pQueryResults, &imageId);

	if (!pQueryResults) {
		fprintf(stderr, "%*sPA VF Config query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA VF Config query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status), pQueryResults->MadStatus,
			iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Config Records Returned\n", 0, "");
	} else {
		STL_PA_VF_CONFIG_RESULTS *p = (STL_PA_VF_CONFIG_RESULTS *)pQueryResults->QueryResult;

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
	if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Get the port counters for a specific virtual fabric(+port)
 *
 * @param port                      omgt_port to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param port                      omgt_port to send through
 * @param deltaFlag                 1 for delta counters, 0 for raw counters.
 * @param userCntrsFlag             1 for running counters, 0 for PM Image counters.
 * @param vfName 					Virtual fabric name
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetVFPortCounters(struct omgt_port *port, uint32_t nodeLid, uint8_t portNumber, uint32_t deltaFlag,
	uint32_t userCntrsFlag, char *vfName, uint64 imageNumber, int32 imageOffset, uint32_t begin, uint32_t end)
{
	FSTATUS status = FERROR;
	STL_PA_VF_PORT_COUNTERS_DATA *response = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {
		.imageNumber = end ? PACLIENT_IMAGE_TIMED : end,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = end
	};

	fprintf(stderr, "Getting Port Counters...\n");

	if (begin) {
		STL_PA_IMAGE_ID_DATA beginQuery = {0}, endQuery = {0};

		beginQuery.imageNumber = PACLIENT_IMAGE_TIMED;
		beginQuery.imageTime.absoluteTime = begin;

		endQuery.imageNumber = end ? PACLIENT_IMAGE_TIMED : PACLIENT_IMAGE_CURRENT;
		endQuery.imageTime.absoluteTime = end;

		STL_PA_VF_PORT_COUNTERS_DATA *beginCounters = NULL, *endCounters = NULL;

		if ((beginCounters = iba_pa_single_mad_vf_port_counters_response_query(port, nodeLid, portNumber, 0, 0, vfName, &beginQuery)) != NULL
			&& (endCounters = iba_pa_single_mad_vf_port_counters_response_query(port, nodeLid, portNumber, 0, 0, vfName, &endQuery)) != NULL) {

			CounterSelectMask_t clearedCounters = DiffPAVFCounters(endCounters, beginCounters, endCounters);
			if (clearedCounters.AsReg32) {
				char counterBuf[128];
				FormatStlCounterSelectMask(counterBuf, clearedCounters);
				fprintf(stderr, "Counters reset, reporting latest count: %s", counterBuf);
			}
			status = FSUCCESS;
			PrintStlPAVFPortCounters(&g_dest, 0, endCounters, (uint32)nodeLid, (uint32)portNumber, 0);
		} else {
			fprintf(stderr, "Failed to receive GetVFPortCounters response\n");
		}

		if (endCounters)
			MemoryDeallocate(endCounters);
		if (beginCounters)
			MemoryDeallocate(beginCounters);
	} else if ((response = iba_pa_single_mad_vf_port_counters_response_query(port, nodeLid, portNumber, deltaFlag, userCntrsFlag, vfName, &imageId)) != NULL) {
		status = FSUCCESS;
		PrintStlPAVFPortCounters(&g_dest, 0, response, (uint32)nodeLid, (uint32)portNumber, response->flags);
	} else {
		fprintf(stderr, "Failed to receive GetVFPortCounters response\n");
	}

	if (response) free(response);
	return status;
}

/**
 *  Clear the port counters for a specific virtual fabric(+port)
 *
 * @param port                      omgt_port to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param port                      omgt_port to send through
 * @param selectFlag 				Counter select mask
 * @param vfName 					Virtual fabric name
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_ClrVFPortCounters(struct omgt_port *port, uint32_t nodeLid, uint8_t portNumber, uint32_t selectFlag, char *vfName)
{
	FSTATUS status = FERROR;
	STL_PA_CLEAR_VF_PORT_COUNTERS_DATA *response = NULL;

	fprintf(stderr, "Clearing VF Port Counters...\n");
	if ((response = iba_pa_single_mad_clr_vf_port_counters_response_query(port, nodeLid, portNumber, selectFlag, vfName)) != NULL) {
		status = FSUCCESS;
		fprintf(stderr, "VF Port Counters successfully cleared for vf %s node %d port number %d select = 0x%04x\n", vfName, nodeLid, portNumber, selectFlag);
	} else {
		fprintf(stderr, "Failed to receive ClrVFPortCounters response\n");
	}
	if (response) free(response);
	return status;
}

/**
 *  Retrieve the focus port list for the given virtual fabric
 *
 * @param port                      omgt_port to send through
 * @param vfName 					Virtual fabric name
 * @param select                	Select field for focus portlist
 * @param start                     Start index value of portlist 
 * @param range                     Index range of portlist. 
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
*/
FSTATUS fe_GetVFFocusPorts(struct omgt_port *port, char *vfName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset, uint32 imageTime)
{
	OMGT_QUERY query;
	FSTATUS status;
	PQUERY_RESULT_VALUES pQueryResults = NULL;
	STL_PA_IMAGE_ID_DATA imageId = {
		.imageNumber = imageTime ? PACLIENT_IMAGE_TIMED : imageNumber,
		.imageOffset = imageOffset,
		.imageTime.absoluteTime = imageTime
	};

	memset(&query, 0, sizeof(query));   // initialize reserved fields
	query.InputType  = InputTypeNoInput;
	query.OutputType = OutputTypePaTableRecord;

	fprintf(stderr, "Getting VF Focus Ports...\n");
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
			iba_pa_query_input_type_msg(query.InputType),
			iba_pa_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = iba_pa_multi_mad_vf_focus_ports_response_query(port, &query, vfName, select, start, range, &pQueryResults, &imageId);
	if (!pQueryResults) {
		fprintf(stderr, "%*sPA VF Focus Ports query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sPA VF Focus Ports query Failed: %s MadStatus 0x%x: %s\n", 0, "",
			iba_fstatus_msg(pQueryResults->Status), pQueryResults->MadStatus,
			iba_pa_mad_status_msg(port));
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo VF Focus Ports Records Returned\n", 0, "");
	} else {
		STL_PA_VF_FOCUS_PORTS_RESULTS *p = (STL_PA_VF_FOCUS_PORTS_RESULTS *)pQueryResults->QueryResult;

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
	if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}
