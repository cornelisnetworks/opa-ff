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
#include "stl_pa.h"
#include "stl_pm.h"
#include "ib_pm.h"
#include "ib_mad.h"
#include "stl_print.h"
#include "fe_connections.h"
#include "fe_pa.h"

#define min(a,b) (((a)<(b))?(a):(b))

#define STL_MAX_PORT_CONFIG_DATA_SIZE (sizeof(STL_PA_PM_GROUP_CFG_RSP) * 2048)
#define STL_MAX_GROUP_LIST_SIZE (sizeof(STL_PA_GROUP_LIST) * (STL_PM_MAX_GROUPS+1))
#define PA_REQ_HDR_SIZE (sizeof(MAD_COMMON) + sizeof(SA_MAD_HDR))

extern PrintDest_t g_dest;

/** 
 *  Send a PA query and get the result.
 * 
 * @param method            PA method identifier.
 * @param attr_id           PA attribute identifier
 * @param attr_mod          PA attribute modifier
 * @param snd_data          Outbound request data.
 * @param snd_data_len      Outbound request data length.
 * @param rcv_buf_len       Max rcv buffer length 
 * @param rsp_mad           Response MAD packet. The caller should free it after use.
 * @param rec_size          Query return result record size
 * @param query_result      Query return result (pointer to pointer to the query result). Allocated
 *                          here if successful and the caller must free it.
 *
 * @return 
 *   FSUCCESS - Query successful
 *     FERROR - Error
 */
static FSTATUS 
fe_pa_query_common (
	struct net_connection   *connection,
    uint8_t                 method,
    uint32_t                attr_id,
    uint32_t                attr_mod,
    void                    *snd_data,
    uint32_t                snd_data_len,
    uint32_t                *rcv_buf_len,
    uint8_t                 **rsp_mad,
    uint32_t                rec_sz,
    PQUERY_RESULT_VALUES    *query_result 
    )
{
    FSTATUS    fstatus = FSUCCESS;
    uint32_t   rec_cnt = 0;
    uint32_t   mem_size;
	int        resp_expected;
	SA_MAD     saMad;
	uint32 msgID = 0;
	static uint32_t msgIDs = 0;

    // default the parameters to failed state.
    *query_result = NULL;
    *rsp_mad= NULL;
	memset(&saMad, 0, sizeof(SA_MAD));

	// Set all the mad attributes
	saMad.common.mr.s.Method = method;
	saMad.common.AttributeID = attr_id;
	saMad.common.AttributeModifier = attr_mod;
	saMad.common.BaseVersion = STL_BASE_VERSION;
	saMad.common.ClassVersion = STL_PM_CLASS_VERSION;
	saMad.common.MgmtClass = MCLASS_VFI_PM;
	memcpy(&(saMad.Data), snd_data, snd_data_len);

	msgID = msgIDs++;
	MAD_SET_TRANSACTION_ID((SA_MAD *)&saMad, msgID);
	BSWAP_SA_HDR((SA_HDR *)(&(saMad.SaHdr)));
	BSWAP_MAD_HEADER((MAD *)&saMad);

	// Send the packet, receive a response
	if (fe_oob_send_packet(connection, (uint8_t *)&saMad, snd_data_len) != FSUCCESS)
		return FERROR;
	if (fe_oob_receive_response(connection, (uint8_t **)(rsp_mad), rcv_buf_len))
		return FERROR;
	if (((SA_MAD *)(*rsp_mad))->common.u.NS.Status.AsReg16) {
		printf("MAD Request failed, MAD status: %d\n", ((SA_MAD *)(*rsp_mad))->common.u.NS.Status.AsReg16);
		return FERROR;
	}
	BSWAP_SA_HDR(&(((SA_MAD *)(*rsp_mad))->SaHdr));
	BSWAP_MAD_HEADER((MAD *)*rsp_mad);

	// If we received a response, check the number of records returned
	resp_expected = (*rcv_buf_len) ? 1 : 0;
	if (resp_expected) {
		uint32 attr_offset = ((SA_MAD *)(*rsp_mad))->SaHdr.AttributeOffset;
		if (attr_offset)
			rec_cnt = (*rcv_buf_len - IBA_SUBN_ADM_HDRSIZE) / (attr_offset * sizeof(uint64));
		else
			rec_cnt = 0;
	} else {
		return FERROR;
	}

    // query result is the size of one of the IBACCESS expected data types.
    // Multiply that size by the count of records we got back.
    // Add to that size, the size of the size of the query response struct.
    mem_size  = rec_sz * rec_cnt;
    mem_size += sizeof (uint32_t); 
    mem_size += sizeof (QUERY_RESULT_VALUES);

    // resultDataSize should be 0 when status is not successful and no data is returned
	*query_result = malloc(mem_size);
    if (*query_result == NULL) 
    {
        fprintf(stderr, "fe_pa_query_common: error allocating query result buffer\n");
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
    memset(*query_result, 0, mem_size);
    (*query_result)->Status = fstatus;
    (*query_result)->MadStatus = ((SA_MAD *)(*rsp_mad))->common.u.NS.Status.AsReg16;
    (*query_result)->ResultDataSize = rec_sz * rec_cnt;   
    *((uint32_t*)((*query_result)->QueryResult)) = rec_cnt;

done:
    return (fstatus);
}

/**
 *  Get multi-record response for pa group data (group list).
 *
 * @param connection                net_connection to send through
 * @param query                     Pointer to the query 
 * @param pquery_result             Pointer to query result to be filled. The caller 
 *                                  has to free the buffer. 
 * @param query_control_parameters  Optional query control parameters (retry, timeout). 
 *
 * @return 
 *   FSUCCESS       - Get successfully
 *     FERROR       - Error
 */
FSTATUS 
fe_pa_multi_mad_group_response_query(
    IN struct net_connection        *connection,
    IN PQUERY                       query,
    OUT PQUERY_RESULT_VALUES        *pquery_result,
    IN COMMAND_CONTROL_PARAMETERS   *query_control_parameters OPTIONAL
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE] = {0};
    STL_PA_GROUP_LIST           *pa_data;
    STL_PA_GROUP_LIST_RESULTS   *pa_result;

    // optional query parameters are not supported in Open IB.
    if (query_control_parameters != NULL) 
    {
        printf("Optional Query Control paramerers are not supported in OPENIB\n");
        return (FERROR);
    }

    // process the command.
    switch (query->OutputType) 
    {
        case OutputTypePaTableRecord:       
        {
			if(g_verbose)
	            fprintf(stderr, "Sending Get Multi Record request\n");

            // submit request
            fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_GRP_LIST, 0,
										 request_data, sizeof(request_data),
										 &rcv_buf_len, &rsp_mad,
										 STL_PA_GROUP_LIST_NSIZE + STL_MAX_GROUP_LIST_SIZE,
										 &query_result);
            if (fstatus == FSUCCESS) 
            {
                if (g_verbose) 
                {
                    fprintf(stderr, "Completed request: OK\n");
                }

                // translate the data.
				data = ((SA_MAD*) rsp_mad)->Data;
				pa_result = (STL_PA_GROUP_LIST_RESULTS*)query_result->QueryResult;
				pa_data = pa_result->GroupListRecords;
				if (pa_result->NumGroupListRecords) {
					memcpy(pa_data, data, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE);
				}
            } 
            else 
            {
                if (g_verbose)
                    fprintf(stderr, "Completed request: FAILED\n");
            }
        }
        break;

    default:
        fprintf(stderr, "Query Not supported in OPEN IB\n");
        fstatus = FERROR; 
        goto done;
        break;
    }

done:
    *pquery_result = query_result;

    return fstatus;
}

/**
 *  Retrieve group list and print resulting records
 *
 * @param connection                net_connection to send through
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetGroupList(struct net_connection *connection)
{
	QUERY					query;
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
	status = fe_pa_multi_mad_group_response_query(connection, &query, &pQueryResults, NULL);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA GroupList query Failed: %s\n", 0, "", iba_fstatus_msg(status));
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

		PrintStlPAGroupList(&g_dest, 1, p->NumGroupListRecords, p->GroupListRecords);
	}

	status = FSUCCESS;

done:
    if(pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Get multi-record response for pa group info (stats).
 *
 * @param connection                net_connection to send through
 * @param query                     Pointer to the query 
 * @param group_name                Group name 
 * @param pquery_result             Pointer to query result to be filled. The caller has to 
 *                                  to free the buffer. 
 * @param query_control_parameters  Optional query control parameters (retry, timeout). 
 * @param image_id                  Pointer to the image ID. 
 *
 * @return 
 *   FSUCCESS       - Get successfully
 *     FERROR       - Error
 */
FSTATUS 
fe_pa_multi_mad_group_stats_response_query(
    IN struct net_connection 		*connection,
    IN PQUERY                       query,
    IN char                         *group_name,
    OUT PQUERY_RESULT_VALUES        *pquery_result,
    IN COMMAND_CONTROL_PARAMETERS   *query_control_parameters OPTIONAL,
    IN STL_PA_IMAGE_ID_DATA                *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                         rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + 80] = {0};  // 80 = GroupName + ImageID (request)
    STL_PA_PM_GROUP_INFO_DATA             *pa_data;
    STL_PA_PM_GROUP_INFO_DATA             *p;
    STL_PA_GROUP_INFO_RESULTS       *pa_result;

    // optional query parameters are not supported in Open IB.
    if (query_control_parameters != NULL) 
    {
        printf("Optional Query Control paramerers are not supported in OPENIB\n");
        return (FERROR);
    }

    p = (STL_PA_PM_GROUP_INFO_DATA *)request_data;
    strcpy(p->groupName, group_name);
    p->imageId.imageNumber = image_id->imageNumber;
    p->imageId.imageOffset = image_id->imageOffset;
	BSWAP_STL_PA_PM_GROUP_INFO(p, 1);
    
    // process the command.
    switch (query->OutputType) 
    {
        case OutputTypePaTableRecord:       
        {
			if(g_verbose)
            	fprintf(stderr, "Sending Get Multi Record request\n");

            // submit request
            fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_GRP_INFO, 0,
                                      request_data, sizeof(request_data),
                                      &rcv_buf_len, 
                                      &rsp_mad, STL_PA_GROUP_INFO_NSIZE, &query_result); 
            if (fstatus == FSUCCESS) 
            {
                if (g_verbose) 
                {
                    fprintf(stderr, "Completed request: OK\n");
                }

                // translate the data.
				data = ((SA_MAD*) rsp_mad)->Data;
                pa_result = (STL_PA_GROUP_INFO_RESULTS*)query_result->QueryResult;
                pa_data = pa_result->GroupInfoRecords;
                memcpy(pa_data, data, min(STL_PA_GROUP_INFO_NSIZE * pa_result->NumGroupInfoRecords, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));
                BSWAP_STL_PA_PM_GROUP_INFO(pa_data, 0);
            } 
            else 
            {
                if (g_verbose)
                    fprintf(stderr, "Completed request: FAILED\n");
            }
        }
        break;

    default:
        fprintf(stderr, "Query Not supported in OPEN IB\n");
        fstatus = FERROR; 
        goto done;
        break;
    }

done:
    *pquery_result = query_result;

    return fstatus;
}

/**
 *  Retrieve group info and print resulting records
 *
 * @param connection                net_connection to send through
 * @param group_name                Group name 
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetGroupInfo(struct net_connection *connection, char *groupName, uint64 imageNumber, int32 imageOffset)
{
	QUERY					query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;
    STL_PA_IMAGE_ID_DATA			imageId = {0};

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
	status = fe_pa_multi_mad_group_stats_response_query(connection, &query, groupName, &pQueryResults, NULL, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Group Info query Failed: %s \n", 0, "", iba_fstatus_msg(status));
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
    if (pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Get multi-record response for pa group config.
 *
 * @param connection                net_connection to send through
 * @param query                     Pointer to the query 
 * @param group_name                Group name 
 * @param pquery_result             Pointer to query result to be filled. The caller has 
 *                                  to free the buffer. 
 * @param query_control_parameters  Optional query control parameters (retry, timeout). 
 * @param image_id                  Pointer to the image ID. 
 *
 * @return 
 *   FSUCCESS       - Get successfully
 *     FERROR       - Error
 */
FSTATUS 
fe_pa_multi_mad_group_config_response_query(
    IN struct net_connection 		*connection,
    IN PQUERY                       query,
    IN char                         *group_name,
    OUT PQUERY_RESULT_VALUES        *pquery_result,
    IN COMMAND_CONTROL_PARAMETERS   *query_control_parameters OPTIONAL,
    IN STL_PA_IMAGE_ID_DATA                *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_PM_GROUP_CFG_REQ)] = {0};
	int							i;
    STL_PA_PM_GROUP_CFG_RSP          *pa_data;
    STL_PA_PM_GROUP_CFG_REQ          *p;
    STL_PA_GROUP_CONFIG_RESULTS     *pa_result;

    // optional query parameters are not supported in Open IB.
    if (query_control_parameters != NULL) 
    {
        printf("Optional Query Control paramerers are not supported in OPENIB\n");
        return (FERROR);
    }
    
    p = (STL_PA_PM_GROUP_CFG_REQ *)request_data;
    strcpy(p->groupName, group_name);

    p->imageId.imageNumber = hton64(image_id->imageNumber);
    p->imageId.imageOffset = hton32(image_id->imageOffset);
    
    // process the command.
    switch (query->OutputType) 
    {
        case OutputTypePaTableRecord:       
        {
			if(g_verbose)
            	fprintf(stderr, "Sending Get Multi Record request\n");

            // submit request
            fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_GRP_CFG, 0,
                                      request_data, sizeof(request_data),
                                      &rcv_buf_len, &rsp_mad, 
									  STL_PA_GROUP_CONFIG_NSIZE + STL_MAX_PORT_CONFIG_DATA_SIZE, 
									  &query_result); 
            if (fstatus == FSUCCESS) 
            {
                if (g_verbose) 
                    fprintf(stderr, "Completed request: OK\n");

                // translate the data.
				data = ((SA_MAD*) rsp_mad)->Data;
                pa_result = (STL_PA_GROUP_CONFIG_RESULTS*)query_result->QueryResult;
                pa_data = pa_result->GroupConfigRecords;
                if (pa_result->NumGroupConfigRecords) 
                {
					memcpy(pa_data, data, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE);
					for (i = 0; i < pa_result->NumGroupConfigRecords; i++)
                    	BSWAP_STL_PA_GROUP_CONFIG_RSP(&pa_data[i]);
                }
            } 
            else 
            {
                if (g_verbose)
                    fprintf(stderr, "Completed request: FAILED\n");
            }
        }
        break;

    default:
        fprintf(stderr, "Query Not supported in OPEN IB\n");
        fstatus = FERROR; 
        goto done;
        break;
    }

done:
    *pquery_result = query_result;

    return fstatus;
}

/**
 *  Retrieve group config for a select group(s) and print the resulting records
 *
 * @param connection                net_connection to send through
 * @param group_name                Group name 
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetGroupConfig(struct net_connection *connection, char *groupName, uint64 imageNumber, int32 imageOffset)
{
	QUERY					query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Group Config...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
							iba_sd_query_input_type_msg(query.InputType),
							iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = fe_pa_multi_mad_group_config_response_query(connection, &query, groupName, &pQueryResults, NULL, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Group Config query Failed: %s\n", 0, "", iba_fstatus_msg(status));
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

		PrintStlPAGroupConfig(&g_dest, 1, groupName, p->NumGroupConfigRecords, p->GroupConfigRecords);
	}

	status = FSUCCESS;

done:
    if(pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Get port statistics (counters)
 *
 * @param connection            net_connection to send through
 * @param node_lid              Remote node LID.
 * @param port_num              Remote port number. 
 * @param port_counters         Pointer to port counters to fill. 
 * @param delta_flag            1 for delta counters, 0 for raw counters.
 * @param user_cntrs_flag       1 for running counters, 0 for PM Image counters.
 * @param image_id              Pointer to image ID of port counters to get. 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the counters data.
 *     NULL         - Error
 */
STL_PORT_COUNTERS_DATA *
fe_pa_single_mad_port_counters_response_query(
    IN struct net_connection *connection,
    IN uint32_t               node_lid,
    IN uint8_t                port_number,
    IN uint32_t               delta_flag,
    IN uint32_t               user_cntrs_flag,
    IN STL_PA_IMAGE_ID_DATA  *image_id
    )
{
    FSTATUS                 fstatus = FSUCCESS;
    QUERY_RESULT_VALUES     *query_result = NULL;
    STL_PORT_COUNTERS_DATA       *response = NULL;
    STL_PORT_COUNTERS_DATA      *p;
    uint8_t                 *rsp_mad, *data;
    uint32_t                rcv_buf_len; 
    char                    request_data[PA_REQ_HDR_SIZE + sizeof(STL_PORT_COUNTERS_DATA)] = {0};

    p = (STL_PORT_COUNTERS_DATA *)request_data;
    p->nodeLid = node_lid;
    p->portNumber = port_number;
    p->flags = (delta_flag ? STL_PA_PC_FLAG_DELTA : 0) |
			   (user_cntrs_flag ? STL_PA_PC_FLAG_USER_COUNTERS : 0);
    p->imageId.imageNumber = image_id->imageNumber;
    p->imageId.imageOffset = image_id->imageOffset;
	memset(p->reserved, 0, sizeof(p->reserved));
	memset(p->reserved2, 0, sizeof(p->reserved2));
	p->lq.s.reserved = 0;
    BSWAP_STL_PA_PORT_COUNTERS(p);

	if(g_verbose)
    	fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_GET, STL_PA_ATTRID_GET_PORT_CTRS, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len, 
                              &rsp_mad, STL_PA_PORT_COUNTERS_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
		goto done;
    } 
    else 
    {
        if (g_verbose) 
        {
            fprintf(stderr, "Completed request: OK\n");
        }
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_PORT_COUNTERS_NSIZE);
	if(!response)
	{
		if (g_verbose)
			fprintf(stderr, "Error allocating query response buffer\n");
		goto done;
	}
	memset(response, 0, STL_PA_PORT_COUNTERS_NSIZE);

    memcpy((uint8 *)response, data, min(STL_PA_PORT_COUNTERS_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_PORT_COUNTERS(response);

done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Retrieve port counters for a specific port and print resulting record
 *
 * @param connection                net_connection to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param portNumber 				Specific port to get the counters from
 * @param delta 					Whether to get the delta since previous image
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetPortCounters(struct net_connection *connection, uint32_t nodeLid, uint8_t portNumber, uint32_t deltaFlag, uint32_t userCtrsFlag, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
    STL_PORT_COUNTERS_DATA		*response;

    fprintf(stderr, "Getting Port Counters...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
    if ((response = fe_pa_single_mad_port_counters_response_query(connection, nodeLid, portNumber, deltaFlag, userCtrsFlag, &imageId)) != NULL) {
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
 *  Clear port statistics (counters)
 *
 * @param connection            net_connection to send through
 * @param node_lid              Remote node LID.
 * @param port_num              Remote port number. 
 * @param select                Port's counters to clear. 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the clear port counters data (containing the
 *                    info as which counters have been cleared).
 *     NULL         - Error
 */
STL_CLR_PORT_COUNTERS_DATA *
fe_pa_single_mad_clr_port_counters_response_query(
    IN struct net_connection *connection,
    IN uint32_t               node_lid,
    IN uint8_t                port_number,
    IN uint32_t               select
    )
{
    FSTATUS                 fstatus = FSUCCESS;
    QUERY_RESULT_VALUES     *query_result = NULL;
    STL_CLR_PORT_COUNTERS_DATA  *response = NULL;
    STL_CLR_PORT_COUNTERS_DATA  *p;
    uint8_t                 *rsp_mad, *data;
    uint32_t                rcv_buf_len; 
    char                    request_data[PA_REQ_HDR_SIZE + sizeof(STL_CLR_PORT_COUNTERS_DATA)] = {0};

    p = (STL_CLR_PORT_COUNTERS_DATA *)request_data;
    p->NodeLid = node_lid;
    p->PortNumber = port_number;
    p->CounterSelectMask.AsReg32 = select;
	p->CounterSelectMask.s.Reserved = 0;
	memset(p->Reserved, 0, sizeof(p->Reserved));
    BSWAP_STL_PA_CLR_PORT_COUNTERS(p);

	if(g_verbose)
	    fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_SET, STL_PA_ATTRID_CLR_PORT_CTRS, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len, 
                              &rsp_mad, STL_PA_CLR_PORT_COUNTERS_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_CLR_PORT_COUNTERS_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_CLR_PORT_COUNTERS_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_CLR_PORT_COUNTERS_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_CLR_PORT_COUNTERS(response);
done:
    if(query_result) free(query_result);
    return response;
}


/**
 *  Clear port counters for a specific port 
 *
 * @param connection                net_connection to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param portNumber 				Specific port to get the counters from
 * @param selectFlag 				Counter select mask
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_ClrPortCounters(struct net_connection *connection, uint32_t nodeLid, uint8_t portNumber, uint32_t selectFlag)
{
	FSTATUS					status= FERROR;
    STL_CLR_PORT_COUNTERS_DATA	*response;

    fprintf(stderr, "Clearing Port Counters...\n");
    if ((response = fe_pa_single_mad_clr_port_counters_response_query(connection, nodeLid, portNumber, selectFlag)) != NULL) {
        status = FSUCCESS;
		fprintf(stderr, "Port Counters successfully cleared for node %d port number %d select = 0x%04x\n", nodeLid, portNumber, selectFlag);
    } else {
        fprintf(stderr, "Failed to receive ClrPortCounters response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Clear all ports, statistics (counters)
 *
 * @param connection            net_connection to send through
 * @param select                Port's counters to clear. 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the clear all port counters data (containing the
 *                    info as which counters have been cleared). 
 *     NULL         - Error
 */
STL_CLR_ALL_PORT_COUNTERS_DATA *
fe_pa_single_mad_clr_all_port_counters_response_query(
    IN struct net_connection *connection,
    IN uint32_t               select
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    STL_CLR_ALL_PORT_COUNTERS_DATA  *response = NULL;
    STL_CLR_ALL_PORT_COUNTERS_DATA  *p;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_CLR_ALL_PORT_COUNTERS_DATA)] = {0};

    p = (STL_CLR_ALL_PORT_COUNTERS_DATA *)request_data;
    p->CounterSelectMask.AsReg32 = select;
	p->CounterSelectMask.s.Reserved = 0;
    BSWAP_STL_PA_CLR_ALL_PORT_COUNTERS(p);

	if(g_verbose)
	    fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_SET, STL_PA_ATTRID_CLR_ALL_PORT_CTRS, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len, 
                              &rsp_mad, STL_PA_CLR_ALL_PORT_COUNTERS_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_CLR_ALL_PORT_COUNTERS_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_CLR_ALL_PORT_COUNTERS_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_CLR_ALL_PORT_COUNTERS_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_CLR_ALL_PORT_COUNTERS(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Clear port counters for all ports 
 *
 * @param connection                net_connection to send through
 * @param selectFlag 				Counter select mask
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_ClrAllPortCounters(struct net_connection *connection, uint32_t selectFlag)
{
	FSTATUS					status= FERROR;
    STL_CLR_ALL_PORT_COUNTERS_DATA	*response;

    fprintf(stderr, "Clearing All Port Counters...\n");
    if ((response = fe_pa_single_mad_clr_all_port_counters_response_query(connection, selectFlag)) != NULL) {
        status = FSUCCESS;
		fprintf(stderr, "All Port Counters successfully cleared for select = 0x%04x\n", selectFlag);
    } else {
        fprintf(stderr, "Failed to receive ClrAllPortCounters response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Get the PM config data.
 *
 * @param connection                net_connection to send through
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for pm config data.
 *     NULL         - Error
 */
STL_PA_PM_CFG_DATA *
fe_pa_single_mad_get_pm_config_response_query(
    IN struct net_connection *connection
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    STL_PA_PM_CFG_DATA       *response = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_PM_CFG_DATA)] = {0};


	if(g_verbose)
    	fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_GET, STL_PA_ATTRID_GET_PM_CONFIG, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len,
                              &rsp_mad, STL_PA_PM_CONFIG_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_PM_CONFIG_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_PM_CONFIG_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_PM_CONFIG_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_PM_CFG(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Get the current PM configuration settings
 *
 * @param connection                net_connection to send through
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetPMConfig(struct net_connection *connection)
{
	FSTATUS					status= FERROR;
    STL_PA_PM_CFG_DATA			*response;

    fprintf(stderr, "Getting PM Configuration...\n");
    if ((response = fe_pa_single_mad_get_pm_config_response_query(connection)) != NULL) {
        status = FSUCCESS;
		PrintStlPMConfig(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive GetPMConfig response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Freeze a sweep image.
 *
 * @param connection            net_connection to send through
 * @param image_id              ID of image to freeze. 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the frozen image id data.
 *     NULL         - Error
 */
STL_PA_IMAGE_ID_DATA *
fe_pa_single_mad_freeze_image_response_query(
    IN struct net_connection *connection,
    IN STL_PA_IMAGE_ID_DATA    *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    STL_PA_IMAGE_ID_DATA               *response = NULL;
    STL_PA_IMAGE_ID_DATA               *p;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_IMAGE_ID_DATA)] = {0};

    p = (STL_PA_IMAGE_ID_DATA *)request_data;
    p->imageNumber = hton64(image_id->imageNumber);
    p->imageOffset = hton32(image_id->imageOffset);

	if(g_verbose)
    	fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_SET, STL_PA_ATTRID_FREEZE_IMAGE, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len,
                              &rsp_mad, STL_PA_IMAGE_ID_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
        {
            fprintf(stderr, "Completed request: OK\n");
        }
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_IMAGE_ID_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_IMAGE_ID_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_IMAGE_ID_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_IMAGE_ID(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Freeze an image within the PM
 *
 * @param connection                net_connection to send through
 * @param imageNumber 				Image number to freeze
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_FreezeImage(struct net_connection *connection, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			request = {0};
    STL_PA_IMAGE_ID_DATA			*response;

    fprintf(stderr, "Freezing image...\n");
	request.imageNumber = imageNumber;
	request.imageOffset = imageOffset;
    if ((response = fe_pa_single_mad_freeze_image_response_query(connection, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive FreezeImage response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Release a sweep image.
 *
 * @param connection            net_connection to send through
 * @param image_id              Ponter to id of image to release. 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the released image id data.
 *     NULL         - Error
 */
STL_PA_IMAGE_ID_DATA *
fe_pa_single_mad_release_image_response_query(
    IN struct net_connection *connection,
    IN STL_PA_IMAGE_ID_DATA    *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    STL_PA_IMAGE_ID_DATA               *response = NULL;
    STL_PA_IMAGE_ID_DATA               *p;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_IMAGE_ID_DATA)] = {0};

    p = (STL_PA_IMAGE_ID_DATA *)request_data;
    p->imageNumber = hton64(image_id->imageNumber);
    p->imageOffset = hton32(image_id->imageOffset);

	if(g_verbose)
	    fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_SET, STL_PA_ATTRID_RELEASE_IMAGE, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len,
                              &rsp_mad, STL_PA_IMAGE_ID_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_IMAGE_ID_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_IMAGE_ID_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_IMAGE_ID_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_IMAGE_ID(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Release (unfreeze) an image within the PM
 *
 * @param connection                net_connection to send through
 * @param imageNumber 				Image number to freeze
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_ReleaseImage(struct net_connection *connection, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			request = {0};
    STL_PA_IMAGE_ID_DATA			*response;

    fprintf(stderr, "Releasing image...\n");
	request.imageNumber = imageNumber;
	request.imageOffset = imageOffset;
    if ((response = fe_pa_single_mad_release_image_response_query(connection, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive ReleaseImage response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Renew a sweep image.
 *
 * @param connection            net_connection to send through
 * @param image_id              Ponter to id of image to renew. 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the renewed image id data.
 *     NULL         - Error
 */
STL_PA_IMAGE_ID_DATA *
fe_pa_single_mad_renew_image_response_query(
    IN struct net_connection *connection,
    IN STL_PA_IMAGE_ID_DATA    *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    STL_PA_IMAGE_ID_DATA               *response = NULL;
    STL_PA_IMAGE_ID_DATA               *p;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_IMAGE_ID_DATA)] = {0};

    p = (STL_PA_IMAGE_ID_DATA *)request_data;
    p->imageNumber = hton64(image_id->imageNumber);
    p->imageOffset = hton32(image_id->imageOffset);

	if (g_verbose)
    	fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_SET, STL_PA_ATTRID_RENEW_IMAGE, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len, 
                              &rsp_mad, STL_PA_IMAGE_ID_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_IMAGE_ID_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_IMAGE_ID_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_IMAGE_ID_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_IMAGE_ID(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Renew an image within the PM
 *
 * @param connection                net_connection to send through
 * @param imageNumber 				Image number to freeze
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_RenewImage(struct net_connection *connection, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			request;
    STL_PA_IMAGE_ID_DATA			*response;

    fprintf(stderr, "Renewing image...\n");
	request.imageNumber = imageNumber;
	request.imageOffset = imageOffset;
    if ((response = fe_pa_single_mad_renew_image_response_query(connection, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAImageId(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive RenewImage response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Move a frozen image 1 to image 2.
 *
 * @param port                  Local port to operate on. 
 * @param move_info             Ponter to move info (src and dest image ID). 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the image  move info.
 *     NULL         - Error
 */
STL_MOVE_FREEZE_DATA *
fe_pa_single_mad_move_freeze_response_query(
    IN struct net_connection *connection,
    IN STL_MOVE_FREEZE_DATA *move_info
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    STL_MOVE_FREEZE_DATA            *response = NULL;
    STL_MOVE_FREEZE_DATA            *p;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_MOVE_FREEZE_DATA)] = {0};

    p = (STL_MOVE_FREEZE_DATA *)request_data;
    p->oldFreezeImage.imageNumber = hton64(move_info->oldFreezeImage.imageNumber);
    p->oldFreezeImage.imageOffset = hton32(move_info->oldFreezeImage.imageOffset);
    p->newFreezeImage.imageNumber = hton64(move_info->newFreezeImage.imageNumber);
    p->newFreezeImage.imageOffset = hton32(move_info->newFreezeImage.imageOffset);

	if (g_verbose)
    	fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_SET, STL_PA_ATTRID_MOVE_FREEZE_FRAME, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len,
                              &rsp_mad, STL_PA_MOVE_FREEZE_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_MOVE_FREEZE_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_MOVE_FREEZE_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_MOVE_FREEZE_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_MOVE_FREEZE(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Renew an image within the PM
 *
 * @param connection                net_connection to send through
 * @param imageNumber 				Frozen image number
 * @param imageOffset 				Image offset
 * @param moveImageNumber 			New image number to move to
 * @param moveImageOffset 			New image offset to move to
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_MoveFreeze(struct net_connection *connection, uint64 imageNumber, int32 imageOffset, uint64 moveImageNumber, int32 moveImageOffset)
{
	FSTATUS					status= FERROR;
    STL_MOVE_FREEZE_DATA		request;
    STL_MOVE_FREEZE_DATA		*response;

    fprintf(stderr, "Moving freeze image...\n");
	request.oldFreezeImage.imageNumber = imageNumber;
	request.oldFreezeImage.imageOffset = imageOffset;
	request.newFreezeImage.imageNumber = moveImageNumber;
	request.newFreezeImage.imageOffset = moveImageOffset;
    if ((response = fe_pa_single_mad_move_freeze_response_query(connection, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAMoveFreeze(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive MoveFreeze response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Get multi-record response for pa group focus portlist.
 *
 * @param connection                net_connection to send through
 * @param query                     Pointer to the query 
 * @param group_name                Group name 
 * @param select                    Select value for focus portlist. 
 * @param start                     Start index value of portlist 
 * @param range                     Index range of portlist. 
 * @param pquery_result             Pointer to query result to be filled. The caller has 
 *                                  to fill the buffer. 
 * @param query_control_parameters  Optional query control parameters (retry, timeout). 
 * @param imageID                   Pointer to the image ID. 
 *
 * @return 
 *   FSUCCESS       - Get successfully
 *     FERROR       - Error
 */
FSTATUS 
fe_pa_multi_mad_focus_ports_response_query (
    IN struct net_connection 		*connection,
    IN PQUERY                       query,
    IN char                         *group_name,
    IN uint32                       select,
    IN uint32                       start,
    IN uint32                       range,
    OUT PQUERY_RESULT_VALUES        *pquery_result,
    IN COMMAND_CONTROL_PARAMETERS   *query_control_parameters OPTIONAL,
    IN STL_PA_IMAGE_ID_DATA                *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_FOCUS_PORTS_REQ)] = {0};
	int							i;
    STL_FOCUS_PORTS_RSP           *pa_data;
    STL_FOCUS_PORTS_REQ           *p;
    STL_PA_FOCUS_PORTS_RESULTS      *pa_result;

    // optional query parameters are not supported in Open IB.
    if (query_control_parameters != NULL) 
    {
        printf("Optional Query Control paramerers are not supported in OPENIB\n");
        return (FERROR);
    }

    p = (STL_FOCUS_PORTS_REQ *)request_data;
    strcpy(p->groupName, group_name);
    p->select = hton32(select);
    p->start = hton32(start);
    p->range = hton32(range);

    p->imageId.imageNumber = hton64(image_id->imageNumber);
    p->imageId.imageOffset = hton32(image_id->imageOffset);
    
    // process the command.
    switch (query->OutputType) 
    {
        case OutputTypePaTableRecord:       
        {
			if (g_verbose)
            	fprintf(stderr, "Sending Get Multi Record request\n");

            // submit request
            fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_FOCUS_PORTS, 0,
                                      request_data, sizeof(request_data),
                                      &rcv_buf_len,
                                      &rsp_mad, STL_MAX_PORT_CONFIG_DATA_SIZE, &query_result); 
            if (fstatus == FSUCCESS) 
            {
                if (g_verbose) 
                    fprintf(stderr, "Completed request: OK\n");

				data = ((SA_MAD*) rsp_mad)->Data;
                // translate the data.
                pa_result = (STL_PA_FOCUS_PORTS_RESULTS *)query_result->QueryResult;
                pa_data = pa_result->FocusPortsRecords;
                if (pa_result->NumFocusPortsRecords) 
                {
					memcpy(pa_data, data, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE);
					for (i = 0; i < pa_result->NumFocusPortsRecords; i++)
                    	BSWAP_STL_PA_FOCUS_PORTS_RSP(&pa_data[i]);
                }
            } 
            else 
            {
                if (g_verbose)
                    fprintf(stderr, "Completed request: FAILED\n");
            }
        }
        break;

    default:
        fprintf(stderr, "Query Not supported in OPEN IB\n");
        fstatus = FERROR; 
        goto done;
        break;
    }

done:
    *pquery_result = query_result;

    return fstatus;
}

/**
 *  Retrieve the focus port list for given parameters
 *
 * @param connection 				net_connection to send through
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
FSTATUS fe_GetFocusPorts(struct net_connection *connection, char *groupName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset)
{
	QUERY					query;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeNoInput;
	query.OutputType 	= OutputTypePaTableRecord;

	fprintf(stderr, "Getting Focus Ports...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
	if (g_verbose)
		printf("Query: Input=%s, Output=%s\n",
							iba_sd_query_input_type_msg(query.InputType),
							iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = fe_pa_multi_mad_focus_ports_response_query(connection, &query, groupName, select, start, range, &pQueryResults, NULL, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA Focus Ports query Failed: %s\n", 0, "", iba_fstatus_msg(status));
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

		if (g_verbose) {
			fprintf(stderr, "MadStatus 0x%x: %s\n", pQueryResults->MadStatus,
										iba_sd_mad_status_msg(pQueryResults->MadStatus));
			fprintf(stderr, "%d Bytes Returned\n", pQueryResults->ResultDataSize);
			fprintf(stderr, "PA Multiple MAD Response for Focus Ports group name %s:\n", groupName);
		}

		PrintStlPAFocusPorts(&g_dest, 1, groupName, p->NumFocusPortsRecords, select, start, range, p->FocusPortsRecords);
	}

	status = FSUCCESS;

done:
    if(pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Get image info
 *
 * @param connection 			net_connection to send through
 * @param image_info            Ponter to image info (containing valid image ID). 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the image info data.
 *     NULL         - Error
 */
STL_PA_IMAGE_INFO_DATA *
fe_pa_multi_mad_get_image_info_response_query (
    IN struct net_connection  *connection,
    IN STL_PA_IMAGE_INFO_DATA  *image_info
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    STL_PA_IMAGE_INFO_DATA             *response = NULL;
    STL_PA_IMAGE_INFO_DATA             *p;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_IMAGE_INFO_DATA)] = {0};

    p = (STL_PA_IMAGE_INFO_DATA *)request_data;
    p->imageId.imageNumber = hton64(image_info->imageId.imageNumber);
    p->imageId.imageOffset = hton32(image_info->imageId.imageOffset);
	p->reserved = 0;

    fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_IMAGE_INFO, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len,
                              &rsp_mad, STL_PA_IMAGE_INFO_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_IMAGE_INFO_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_IMAGE_INFO_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_IMAGE_INFO_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_IMAGE_INFO(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Retrieve information about an image
 *
 * @param connection 				net_connection to send through
 * @param imageNumber 				Frozen image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetImageInfo(struct net_connection *connection, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_INFO_DATA		request = {{0}};
    STL_PA_IMAGE_INFO_DATA		*response;;

    fprintf(stderr, "Getting image info...\n");
	request.imageId.imageNumber = imageNumber;
	request.imageId.imageOffset = imageOffset;
    if ((response = fe_pa_multi_mad_get_image_info_response_query(connection, &request)) != NULL) {
        status = FSUCCESS;
		PrintStlPAImageInfo(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive GetImageInfo response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Get the classportinfo from the given port.
 *
 * @param connection    net_connection to send through
 *
 * @return 
 *   Valid pointer  -   A pointer to the ClassPortInfo.
 *   NULL           -   Error.
 */
STL_CLASS_PORT_INFO *
fe_pa_classportinfo_response_query(struct net_connection *connection)
{
    FSTATUS                 fstatus = FSUCCESS;
    QUERY_RESULT_VALUES     *query_result = NULL;
    STL_CLASS_PORT_INFO     *response = NULL;
    uint32_t                record_size = sizeof(STL_CLASS_PORT_INFO);
    uint8_t                 *rsp_mad, *data;
    uint32_t                rcv_buf_len; 
    char                    request_data[PA_REQ_HDR_SIZE] = {0};

	if(g_verbose)
		fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_GET, STL_PA_ATTRID_GET_CLASSPORTINFO, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len, 
                              &rsp_mad, sizeof(STL_CLASS_PORT_INFO), &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
        {
            fprintf(stderr, "Completed request: OK\n");
        }
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(record_size);
	if(!response)
	{
		if (g_verbose)
			fprintf(stderr, "Error allocating query response buffer\n");
		goto done;
	}
	memset(response, 0, record_size);

    memcpy((uint8 *)response, data, min(record_size, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_CLASS_PORT_INFO(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Retrieve PA ClassPortInfo and print resulting record
 *
 * @param connection 				net_connection to send through
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetClassPortInfo(struct net_connection *connection)
{
	FSTATUS					status= FERROR;
	STL_CLASS_PORT_INFO		*response;

    fprintf(stderr, "Getting Class Port Info...\n");
    if ((response = fe_pa_classportinfo_response_query(connection)) != NULL) {
        status = FSUCCESS;
		PrintStlClassPortInfo(&g_dest, 0, response);
    } else {
        fprintf(stderr, "Failed to receive GetClassPortInfo response\n");
    }

	if (response)
		free(response);
    return status;
}

/**
 *  Get multi-record response for pa vf data (vf list).
 *
 * @param connection 				net_connection to send through
 * @param query                     Pointer to the query 
 * @param pquery_result             Pointer to query result to be filled. The caller 
 *                                  has to free the buffer. 
 * @param query_control_parameters  Optional query control parameters (retry, timeout). 
 *
 * @return 
 *   FSUCCESS       - Get successfully
 *     FERROR       - Error
 */
FSTATUS 
fe_pa_multi_mad_vf_list_response_query(
    IN struct net_connection		*connection,
    IN PQUERY                       query,
    OUT PQUERY_RESULT_VALUES        *pquery_result,
    IN COMMAND_CONTROL_PARAMETERS   *query_control_parameters OPTIONAL
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE] = {0};
    STL_PA_VF_LIST	        	*pa_data;
    STL_PA_VF_LIST_RESULTS     	*pa_result;

    // optional query parameters are not supported in Open IB.
    if (query_control_parameters != NULL) 
    {
        printf("Optional Query Control paramerers are not supported in OPENIB\n");
        return (FERROR);
    }

    // process the command.
    switch (query->OutputType) 
    {
        case OutputTypePaTableRecord:       
        {
			if (g_verbose)
            	fprintf(stderr, "Sending Get Multi Record request\n");

            // submit request
            fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_VF_LIST, 0, 
                                      request_data, sizeof(request_data),
                                      &rcv_buf_len, 
                                      &rsp_mad, STL_PA_VF_LIST_NSIZE + STL_MAX_GROUP_LIST_SIZE,
									  &query_result);
            if (fstatus == FSUCCESS) 
            {
                if (g_verbose) 
                {
                    fprintf(stderr, "Completed request: OK\n");
                }

                // translate the data.
				data = ((SA_MAD*) rsp_mad)->Data;
				pa_result = (STL_PA_VF_LIST_RESULTS *)query_result->QueryResult;
				pa_data = pa_result->VFListRecords;
				if (pa_result->NumVFListRecords) {
					memcpy(pa_data, data, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE);
				}
            } 
            else 
            {
                if (g_verbose)
                    fprintf(stderr, "Completed request: FAILED\n");
            }
        }
        break;

    default:
        fprintf(stderr, "Query Not supported in OPEN IB\n");
        fstatus = FERROR; 
        goto done;
        break;
    }

done:
    *pquery_result = query_result;

    return fstatus;
}

/**
 *  Retrieve the list of virtual fabrics
 *
 * @param connection 				net_connection to send through
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetVFList(struct net_connection *connection)
{
	QUERY					query;
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
	status = fe_pa_multi_mad_vf_list_response_query(connection, &query, &pQueryResults, NULL);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA vfList query Failed: %s\n", 0, "", iba_fstatus_msg(status));
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
    if(pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Get multi-record response for pa vf info (stats).
 *
 * @param connection 				net_connection to send through
 * @param query                     Pointer to the query 
 * @param vf_name                	VF name 
 * @param pquery_result             Pointer to query result to be filled. The caller has to 
 *                                  to free the buffer. 
 * @param query_control_parameters  Optional query control parameters (retry, timeout). 
 * @param image_id                  Pointer to the image ID. 
 *
 * @return 
 *   FSUCCESS       - Get successfully
 *     FERROR       - Error
 */
FSTATUS 
fe_pa_multi_mad_vf_info_response_query(
    IN struct net_connection 		*connection,
    IN PQUERY                       query,
    IN char                         *vf_name,
    OUT PQUERY_RESULT_VALUES        *pquery_result,
    IN COMMAND_CONTROL_PARAMETERS   *query_control_parameters OPTIONAL,
    IN STL_PA_IMAGE_ID_DATA                *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + 88] = {0};  // 88 = VFName + Reserved + ImageID
    STL_PA_VF_INFO_DATA             *pa_data;
    STL_PA_VF_INFO_DATA             *p;
    STL_PA_VF_INFO_RESULTS       *pa_result;

    // optional query parameters are not supported in Open IB.
    if (query_control_parameters != NULL) 
    {
        printf("Optional Query Control paramerers are not supported in OPENIB\n");
        return (FERROR);
    }
    
    p = (STL_PA_VF_INFO_DATA *)request_data;
    strcpy(p->vfName, vf_name);
    p->imageId.imageNumber = image_id->imageNumber;
    p->imageId.imageOffset = image_id->imageOffset;
	BSWAP_STL_PA_VF_INFO(p, 1);
    
    // process the command.
    switch (query->OutputType) 
    {
        case OutputTypePaTableRecord:       
        {
			if (g_verbose)
	            fprintf(stderr, "Sending Get Multi Record request\n");

            // submit request
            fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_VF_INFO, 0,
                                      request_data, sizeof(request_data),
                                      &rcv_buf_len,
                                      &rsp_mad, STL_PA_VF_INFO_NSIZE, &query_result); 
            if (fstatus == FSUCCESS) 
            {
                if (g_verbose) 
                {
                    fprintf(stderr, "Completed request: OK\n");
                }

                // translate the data.
				data = ((SA_MAD*) rsp_mad)->Data;
                pa_result = (STL_PA_VF_INFO_RESULTS*)query_result->QueryResult;
                pa_data = pa_result->VFInfoRecords;
                memcpy(pa_data, data, min(STL_PA_VF_INFO_NSIZE * pa_result->NumVFInfoRecords, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));
                BSWAP_STL_PA_VF_INFO(pa_data, 0);
            } 
            else 
            {
                if (g_verbose)
                    fprintf(stderr, "Completed request: FAILED\n");
            }
        }
        break;

    default:
        fprintf(stderr, "Query Not supported in OPEN IB\n");
        fstatus = FERROR; 
        goto done;
        break;
    }

done:
    *pquery_result = query_result;

    return fstatus;
}

/**
 *  Retrieve info about a particular virtual fabric
 *
 * @param connection 				net_connection to send through
 * @param vfName 					Virtual fabric name
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetVFInfo(struct net_connection *connection, char *vfName, uint64 imageNumber, int32 imageOffset)
{
	QUERY					query;
	FSTATUS					status;
	PQUERY_RESULT_VALUES	pQueryResults = NULL;
    STL_PA_IMAGE_ID_DATA			imageId = {0};

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
	status = fe_pa_multi_mad_vf_info_response_query(connection, &query, vfName, &pQueryResults, NULL, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA VF Info query Failed: %s\n", 0, "", iba_fstatus_msg(status));
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
    if(pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Get multi-record response for pa vf config.
 *
 * @param connection 				net_connection to send through
 * @param query                     Pointer to the query 
 * @param vf_name                	VF name 
 * @param pquery_result             Pointer to query result to be filled. The caller has to 
 *                                  to free the buffer. 
 * @param query_control_parameters  Optional query control parameters (retry, timeout). 
 * @param image_id                  Pointer to the image ID. 
 *
 * @return 
 *   FSUCCESS       - Get successfully
 *     FERROR       - Error
 */
FSTATUS 
fe_pa_multi_mad_vf_config_response_query(
    IN struct net_connection        *connection,
    IN PQUERY                       query,
    IN char                         *vf_name,
    OUT PQUERY_RESULT_VALUES        *pquery_result,
    IN COMMAND_CONTROL_PARAMETERS   *query_control_parameters OPTIONAL,
    IN STL_PA_IMAGE_ID_DATA                *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_VF_CFG_REQ)] = {0};
	int							i;
    STL_PA_VF_CFG_RSP           *pa_data;
    STL_PA_VF_CFG_REQ           *p;
    STL_PA_VF_CONFIG_RESULTS     *pa_result;

    // optional query parameters are not supported in Open IB.
    if (query_control_parameters != NULL) 
    {
        printf("Optional Query Control paramerers are not supported in OPENIB\n");
        return (FERROR);
    }
    
    p = (STL_PA_VF_CFG_REQ *)request_data;
    strcpy(p->vfName, vf_name);

    p->imageId.imageNumber = hton64(image_id->imageNumber);
    p->imageId.imageOffset = hton32(image_id->imageOffset);
    
    // process the command.
    switch (query->OutputType) 
    {
        case OutputTypePaTableRecord:       
        {
			if (g_verbose)
	            fprintf(stderr, "Sending Get Multi Record request\n");

            // submit request
            fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_VF_CONFIG, 0,
                                      request_data, sizeof(request_data),
                                      &rcv_buf_len,
                                      &rsp_mad, STL_MAX_PORT_CONFIG_DATA_SIZE, &query_result); 
            if (fstatus == FSUCCESS) 
            {
                if (g_verbose) 
                    fprintf(stderr, "Completed request: OK\n");

                // translate the data.
				data = ((SA_MAD*) rsp_mad)->Data;
                pa_result = (STL_PA_VF_CONFIG_RESULTS*)query_result->QueryResult;
                pa_data = pa_result->VFConfigRecords;
                if (pa_result->NumVFConfigRecords) 
                {
					memcpy(pa_data, data, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE);
					for (i = 0; i < pa_result->NumVFConfigRecords; i++)
                    	BSWAP_STL_PA_VF_CFG_RSP((STL_PA_VF_CFG_RSP *)pa_data);
                }
            } 
            else 
            {
                if (g_verbose)
                    fprintf(stderr, "Completed request: FAILED\n");
            }
        }
        break;

    default:
        fprintf(stderr, "Query Not supported in OPEN IB\n");
        fstatus = FERROR; 
        goto done;
        break;
    }

done:
    *pquery_result = query_result;

    return fstatus;
}

/**
 *  Retrieve the configuration of a particular virtual fabric
 *
 * @param connection 				net_connection to send through
 * @param vfName 					Virtual fabric name
 * @param imageNumber 				Image number
 * @param imageOffset 				Image offset
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_GetVFConfig(struct net_connection *connection, char *vfName, uint64 imageNumber, int32 imageOffset)
{
	QUERY					query;
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
	status = fe_pa_multi_mad_vf_config_response_query(connection, &query, vfName, &pQueryResults, NULL, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA VF Config query Failed: %s\n", 0, "", iba_fstatus_msg(status));
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
    if(pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

/**
 *  Get multi-record response for pa vf config.
 *
 * @param connection 				net_connection to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param portNumber 				Specific port to get the counters from
 * @param delta_flag                1 for delta counters, 0 for raw counters.
 * @param user_cntrs_flag           1 for running counters, 0 for PM Image counters.
 * @param vfName 					Virtual fabric name
 * @param image_id                  Pointer to the image ID. 
 *
 * @return 
 *   Valid pointer  -   A pointer to the VF Port Counters Data.
 *   NULL           -   Error.
 */
STL_PA_VF_PORT_COUNTERS_DATA *
fe_pa_single_mad_vf_port_counters_response_query(
    IN struct net_connection *connection,
    IN uint32_t               node_lid,
    IN uint8_t                port_number,
    IN uint32_t               delta_flag,
    IN uint32_t               user_cntrs_flag,
    IN char                  *vfName,
    IN STL_PA_IMAGE_ID_DATA  *image_id
    )
{
    FSTATUS                 fstatus = FSUCCESS;
    QUERY_RESULT_VALUES     *query_result = NULL;
    STL_PA_VF_PORT_COUNTERS_DATA       *response = NULL;
    STL_PA_VF_PORT_COUNTERS_DATA      *p;
    uint8_t                 *rsp_mad, *data;
    uint32_t                rcv_buf_len; 
    char                    request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_VF_PORT_COUNTERS_DATA)] = {0};

    p = (STL_PA_VF_PORT_COUNTERS_DATA *)request_data;
    p->nodeLid = node_lid;
    p->portNumber = port_number;
    p->flags =  (delta_flag ? STL_PA_PC_FLAG_DELTA : 0) |
                (user_cntrs_flag ? STL_PA_PC_FLAG_USER_COUNTERS : 0);
	strcpy(p->vfName, vfName);

    p->imageId.imageNumber = image_id->imageNumber;
    p->imageId.imageOffset = image_id->imageOffset;
	memset(p->reserved, 0, sizeof(p->reserved));
	p->reserved1 = 0;
    BSWAP_STL_PA_VF_PORT_COUNTERS(p);

	if (g_verbose)
    	fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_GET, STL_PA_ATTRID_GET_VF_PORT_CTRS, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len,
                              &rsp_mad, STL_PA_VF_PORT_COUNTERS_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }


	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_VF_PORT_COUNTERS_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_VF_PORT_COUNTERS_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_VF_PORT_COUNTERS_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_VF_PORT_COUNTERS(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Get the port counters for a specific virtual fabric(+port)
 *
 * @param connection 				net_connection to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param portNumber 				Specific port to get the counters from
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
FSTATUS fe_GetVFPortCounters(struct net_connection *connection, uint32_t nodeLid, uint8_t portNumber, uint32_t deltaFlag, uint32_t userCntrsFlag, char *vfName, uint64 imageNumber, int32 imageOffset)
{
	FSTATUS					status= FERROR;
    STL_PA_IMAGE_ID_DATA			imageId = {0};
    STL_PA_VF_PORT_COUNTERS_DATA		*response;

    fprintf(stderr, "Getting Port Counters...\n");
	imageId.imageNumber = imageNumber;
	imageId.imageOffset = imageOffset;
    if ((response = fe_pa_single_mad_vf_port_counters_response_query(connection, nodeLid, portNumber, deltaFlag, userCntrsFlag, vfName, &imageId)) != NULL) {
        status = FSUCCESS;
		PrintStlPAVFPortCounters(&g_dest, 0, response, (uint32)nodeLid, (uint32)portNumber, response->flags);
    } else {
        fprintf(stderr, "Failed to receive GetVFPortCounters response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Get multi-record response for pa vf config.
 *
 * @param connection 				net_connection to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param portNumber 				Specific port to get the counters from
 * @param delta_flag 	            1 for delta counters, 0 for running counters. 
 * @param vfName 					Virtual fabric name
 * @param image_id                  Pointer to the image ID. 
 *
 * @return 
 *   Valid Pointer  - A pointer to the memory for the clear vf port counters data (containing the
 *                    info as which counters have been cleared).
 *   NULL           -   Error.
 */
STL_PA_CLEAR_VF_PORT_COUNTERS_DATA *
fe_pa_single_mad_clr_vf_port_counters_response_query(
    IN struct net_connection *connection,
    IN uint32_t         node_lid,
    IN uint8_t         port_number,
    IN uint32_t         select,
	IN char				*vfName
    )
{
    FSTATUS                 fstatus = FSUCCESS;
    QUERY_RESULT_VALUES     *query_result = NULL;
    STL_PA_CLEAR_VF_PORT_COUNTERS_DATA  *response = NULL;
    STL_PA_CLEAR_VF_PORT_COUNTERS_DATA  *p;
    uint8_t                 *rsp_mad, *data;
    uint32_t                rcv_buf_len; 
    char                    request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_CLEAR_VF_PORT_COUNTERS_DATA)] = {0};

    p = (STL_PA_CLEAR_VF_PORT_COUNTERS_DATA *)request_data;
    p->nodeLid = hton32(node_lid);
    p->portNumber = port_number;
    p->vfCounterSelectMask.AsReg32 = hton32(select);
	strcpy(p->vfName, vfName);
	memset(p->reserved, 0, sizeof(p->reserved));
	p->reserved2 = 0;

	if (g_verbose)
    	fprintf(stderr, "Sending Get Single Record request\n");

    // submit request
    fstatus = fe_pa_query_common(connection, STL_PA_CMD_SET, STL_PA_ATTRID_CLR_VF_PORT_CTRS, 0,
                              request_data, sizeof(request_data),
                              &rcv_buf_len,
                              &rsp_mad, STL_PA_CLR_VF_PORT_COUNTERS_NSIZE, &query_result);
    if (fstatus != FSUCCESS) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, requeset failed: status=%d\n", fstatus);
        goto done;
    } 
    else if (query_result->ResultDataSize) 
    {
        if (g_verbose)
            fprintf(stderr, "Error, unexpected multiple MAD response\n");
        goto done;
    } 
    else 
    {
        if (g_verbose) 
            fprintf(stderr, "Completed request: OK\n");
    }

	data = ((SA_MAD*) rsp_mad)->Data;
	response = malloc(STL_PA_CLR_VF_PORT_COUNTERS_NSIZE);
    if (response == NULL) 
    {
        fprintf(stderr, "%s: error allocating response buffer\n", __func__);
        fstatus = FINSUFFICIENT_MEMORY;
        goto done;
    }
	memset(response, 0, STL_PA_CLR_VF_PORT_COUNTERS_NSIZE);
    memcpy((uint8 *)response, data, min(STL_PA_CLR_VF_PORT_COUNTERS_NSIZE, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE));

    // translate the data.
    BSWAP_STL_PA_CLEAR_VF_PORT_COUNTERS(response);
done:
    if(query_result) free(query_result);
    return response;
}

/**
 *  Clear the port counters for a specific virtual fabric(+port)
 *
 * @param connection 				net_connection to send through
 * @param nodeLid 					Lid of the node to get port counters from
 * @param portNumber 				Specific port to get the counters from
 * @param selectFlag 				Counter select mask
 * @param vfName 					Virtual fabric name
 *
 * @return
 *   FSUCCESS 		- Get successfully
 *     FERROR 		- Error
 */
FSTATUS fe_ClrVFPortCounters(struct net_connection *connection, uint32_t nodeLid, uint8_t portNumber, uint32_t selectFlag, char *vfName)
{
	FSTATUS					status= FERROR;
    STL_PA_CLEAR_VF_PORT_COUNTERS_DATA	*response;

    fprintf(stderr, "Clearing VF Port Counters...\n");
    if ((response = fe_pa_single_mad_clr_vf_port_counters_response_query(connection, nodeLid, portNumber, selectFlag, vfName)) != NULL) {
        status = FSUCCESS;
		fprintf(stderr, "VF Port Counters successfully cleared for vf %s node %d port number %d select = 0x%04x\n", vfName, nodeLid, portNumber, selectFlag);
    } else {
        fprintf(stderr, "Failed to receive ClrVFPortCounters response\n");
    }
    if(response) free(response);
    return status;
}

/**
 *  Get multi-record response for pa vf focus portlist.
 *
 * @param connection 				net_connection to send through
 * @param query                     Pointer to the query 
 * @param vf_name                Group name 
 * @param select                    Select value for focus portlist. 
 * @param start                     Start index value of portlist 
 * @param range                     Index range of portlist. 
 * @param pquery_result             Pointer to query result to be filled. The caller has 
 *                                  to fill the buffer. 
 * @param query_control_parameters  Optional query control parameters (retry, timeout). 
 * @param imageID                   Pointer to the image ID. 
 *
 * @return 
 *   FSUCCESS       - Get successfully
 *     FERROR       - Error
 */
FSTATUS 
fe_pa_multi_mad_vf_focus_ports_response_query (
    IN struct net_connection 		*connection,
    IN PQUERY                       query,
    IN char                         *vf_name,
    IN uint32                       select,
    IN uint32                       start,
    IN uint32                       range,
    OUT PQUERY_RESULT_VALUES        *pquery_result,
    IN COMMAND_CONTROL_PARAMETERS   *query_control_parameters OPTIONAL,
    IN STL_PA_IMAGE_ID_DATA                *image_id
    )
{
    FSTATUS                     fstatus = FSUCCESS;
    QUERY_RESULT_VALUES         *query_result = NULL;
    uint8_t                     *rsp_mad, *data;
    uint32_t                    rcv_buf_len; 
    char                        request_data[PA_REQ_HDR_SIZE + sizeof(STL_PA_VF_FOCUS_PORTS_REQ)] = {0};
	int							i;
    STL_PA_VF_FOCUS_PORTS_RSP     *pa_data;
    STL_PA_VF_FOCUS_PORTS_REQ     *p;
    STL_PA_VF_FOCUS_PORTS_RESULTS   *pa_result;

    // optional query parameters are not supported in Open IB.
    if (query_control_parameters != NULL) 
    {
        printf("Optional Query Control paramerers are not supported in OPENIB\n");
        return (FERROR);
    }

    p = (STL_PA_VF_FOCUS_PORTS_REQ *)request_data;
    strcpy(p->vfName, vf_name);
    p->select = hton32(select);
    p->start = hton32(start);
    p->range = hton32(range);

    p->imageId.imageNumber = hton64(image_id->imageNumber);
    p->imageId.imageOffset = hton32(image_id->imageOffset);
    
    // process the command.
    switch (query->OutputType) 
    {
        case OutputTypePaTableRecord:       
        {
			if (g_verbose)
            	fprintf(stderr, "Sending Get Multi Record request\n");

            // submit request
            fstatus = fe_pa_query_common(connection, STL_PA_CMD_GETTABLE, STL_PA_ATTRID_GET_VF_FOCUS_PORTS, 0,
                                      request_data, sizeof(request_data),
                                      &rcv_buf_len,
                                      &rsp_mad, STL_MAX_PORT_CONFIG_DATA_SIZE, &query_result); 
            if (fstatus == FSUCCESS) 
            {
                if (g_verbose) 
                    fprintf(stderr, "Completed request: OK\n");

                // translate the data.
				data = ((SA_MAD*) rsp_mad)->Data;
                pa_result = (STL_PA_VF_FOCUS_PORTS_RESULTS *)query_result->QueryResult;
                pa_data = pa_result->FocusPortsRecords;
                if (pa_result->NumVFFocusPortsRecords) 
                {
					memcpy(pa_data, data, rcv_buf_len - IBA_SUBN_ADM_HDRSIZE);
					for (i = 0; i < pa_result->NumVFFocusPortsRecords; i++)
                    	BSWAP_STL_PA_VF_FOCUS_PORTS_RSP(&pa_data[i]);
                }
            } 
            else 
            {
                if (g_verbose)
                    fprintf(stderr, "Completed request: FAILED\n");
            }
        }
        break;

    default:
        fprintf(stderr, "Query Not supported in OPEN IB\n");
        fstatus = FERROR; 
        goto done;
        break;
    }

done:
    *pquery_result = query_result;

    return fstatus;
}

/**
 *  Retrieve the focus port list for the given virtual fabric
 *
 * @param connection 				net_connection to send through
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
FSTATUS fe_GetVFFocusPorts(struct net_connection *connection, char *vfName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset)
{
	QUERY					query;
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
	status = fe_pa_multi_mad_vf_focus_ports_response_query(connection, &query, vfName, select, start, range, &pQueryResults, NULL, &imageId);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sPA VF Focus Ports query Failed: %s\n", 0, "", iba_fstatus_msg(status));
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
    if(pQueryResults) free(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}
