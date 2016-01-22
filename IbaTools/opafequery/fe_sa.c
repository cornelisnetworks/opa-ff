/* BEGIN_ICS_COPYRIGHT7 ****************************************

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

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

#include <stdio.h>
#include <stdlib.h>

#include "fe_sa.h"
#include "fe_connections.h"
#include "iba/stl_sa.h"
#include "iba/stl_sd.h"
#include "iba/ib_mad.h"


#define STL_TRACE_RECORD_COMP_ENCRYPT_MASK       0x55555555
#define PATHRECORD_NUMBPATH 32

/**
 * Fills in the input fields of a query as well as the proper header fields for an STL_NODE_RECORD
 *
 * @param packet Packet containing the header and sa header to fill
 * @param pQuery The query to use when filling the packet's fields
 * @param saHeader Pointer to the sa header in the packet
 * @return FSTATUS return code
 */
static FSTATUS fe_fillInNodeRecord(SA_MAD *saMad, PQUERY pQuery)
{
	SA_HDR* saHeader = &(saMad->SaHdr); 
	STL_NODE_RECORD *pNR = NULL; 	/* Pointer to the record to build */

	if ((saMad == NULL) || (pQuery == NULL)) {
		return (FERROR);
	}

	MAD_SET_ATTRIB_ID(saMad, STL_SA_ATTR_NODE_RECORD);
	MAD_SET_METHOD_TYPE(saMad, SUBN_ADM_GETTABLE);
    MAD_SET_VERSION_INFO(saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);
	pNR = (STL_NODE_RECORD *) saMad->Data;
	memset(pNR, 0, sizeof(STL_NODE_RECORD));

	switch (pQuery->InputType) {
	case InputTypeNoInput:     
		saHeader->ComponentMask = 0;
		break;
	case InputTypeLid:           
		saHeader->ComponentMask = STL_NODE_RECORD_COMP_LID;
		pNR->RID.LID = pQuery->InputValue.Lid;
		break;
	case InputTypePortGuid:
		saHeader->ComponentMask = STL_NODE_RECORD_COMP_PORTGUID;
		pNR->NodeInfo.PortGUID = pQuery->InputValue.Guid;
		break;
	case InputTypeNodeType:
		saHeader->ComponentMask = STL_NODE_RECORD_COMP_NODETYPE;
		pNR->NodeInfo.NodeType = pQuery->InputValue.TypeOfNode;
		break;
	case InputTypeSystemImageGuid:
		saHeader->ComponentMask = STL_NODE_RECORD_COMP_SYSIMAGEGUID;
		pNR->NodeInfo.SystemImageGUID = pQuery->InputValue.Guid;
		break;
	case InputTypeNodeGuid:
		saHeader->ComponentMask = STL_NODE_RECORD_COMP_NODEGUID;
		pNR->NodeInfo.NodeGUID = pQuery->InputValue.Guid;
		break;
	case InputTypeNodeDesc:
		saHeader->ComponentMask = STL_NODE_RECORD_COMP_NODEDESC;
		if (pQuery->InputValue.NodeDesc.NameLength > STL_NODE_DESCRIPTION_ARRAY_SIZE)
			return (FERROR);

		memcpy(pNR->NodeDesc.NodeString,
			   pQuery->InputValue.NodeDesc.Name,
			   pQuery->InputValue.NodeDesc.NameLength);
		break;
	default:
		fprintf(stderr, "Query Not supported within the FE: Input=%s(%d), Output=%s(%d)\n",
				iba_sd_query_input_type_msg(pQuery->InputType),
				pQuery->InputType,
				iba_sd_query_result_type_msg(pQuery->OutputType),
				pQuery->OutputType);
		return(FERROR);
	}

	BSWAP_STL_NODE_RECORD(pNR);
	

	return(FSUCCESS);

}

/**
 * Fills in the input fields of a query as well as the proper header fields for an IB_NODE_RECORD
 *
 * @param packet Packet containing the header and sa header to fill
 * @param pQuery The query to use when filling the packet's fields
 * @param saHeader Pointer to the sa header in the packet
 * @return FSTATUS return code
 */
static FSTATUS fe_fillInIbNodeRecord(SA_MAD *saMad, PQUERY pQuery)
{
	SA_HDR* saHeader = &(saMad->SaHdr); 
	IB_NODE_RECORD *pNR = NULL; 	/* Pointer to the record to build */


	if ((saMad == NULL) || (pQuery == NULL)) {
		return (FERROR);
	}

	MAD_SET_ATTRIB_ID(saMad, SA_ATTRIB_NODE_RECORD);
	MAD_SET_METHOD_TYPE(saMad, SUBN_ADM_GETTABLE);
    MAD_SET_VERSION_INFO(saMad, IB_BASE_VERSION, MCLASS_SUBN_ADM, IB_SUBN_ADM_CLASS_VERSION);
	pNR = (IB_NODE_RECORD *) saMad->Data;
	memset(pNR, 0, sizeof(IB_NODE_RECORD));

	switch (pQuery->InputType) {
	case InputTypeNoInput:     
		saHeader->ComponentMask = 0;
		break;
	case InputTypeLid:           
		saHeader->ComponentMask = IB_NODE_RECORD_COMP_LID;
		pNR->RID.s.LID = pQuery->InputValue.Lid;
		break;
	case InputTypePortGuid:
		saHeader->ComponentMask = IB_NODE_RECORD_COMP_PORTGUID;
        pNR->NodeInfoData.PortGUID = pQuery->InputValue.Guid;
		break;
	case InputTypeNodeType:
		saHeader->ComponentMask = IB_NODE_RECORD_COMP_NODETYPE;
		pNR->NodeInfoData.NodeType = pQuery->InputValue.TypeOfNode;
		break;
	case InputTypeSystemImageGuid:
		saHeader->ComponentMask = IB_NODE_RECORD_COMP_SYSIMAGEGUID;
		pNR->NodeInfoData.SystemImageGUID = pQuery->InputValue.Guid;
		break;
	case InputTypeNodeGuid:
		saHeader->ComponentMask = IB_NODE_RECORD_COMP_NODEGUID;
		pNR->NodeInfoData.NodeGUID = pQuery->InputValue.Guid;
		break;
	case InputTypeNodeDesc:
		saHeader->ComponentMask = IB_NODE_RECORD_COMP_NODEDESC;
		if (pQuery->InputValue.NodeDesc.NameLength > STL_NODE_DESCRIPTION_ARRAY_SIZE)
			return (FERROR);

		memcpy(pNR->NodeDescData.NodeString,
			   pQuery->InputValue.NodeDesc.Name,
			   pQuery->InputValue.NodeDesc.NameLength);
		break;
	default:
		fprintf(stderr, "Query Not supported in FE: Input=%s(%d), Output=%s(%d)\n",
				iba_sd_query_input_type_msg(pQuery->InputType),
				pQuery->InputType,
				iba_sd_query_result_type_msg(pQuery->OutputType),
				pQuery->OutputType);
		return(FERROR);
	}

	BSWAP_IB_NODE_RECORD(pNR);

	return(FSUCCESS);
}


/**
 * Returns a pointer to the ith record in the MadData section of a packet
 *
 * @param packet Packet to retrieve the record offset from
 * @param i Index of the record to retrieve
 * @return Void pointer to the location of that particular record
 */
static __inline
void *fe_packetRecordOffset(SA_MAD *packet, int i)
{
	return (void *)((packet->Data) + (packet->SaHdr.AttributeOffset * sizeof(uint64_t) * i));
}

/**
 * Builds a QUERY_RESULT_VALUES structure so that we may translate the results out of the packet
 * into a format easily printable by available libraries
 *
 * @param ppQueryResults Where to store the location of the response packet we received back from the FE
 * @param pPacket Where the location of the packet containing our FE query results is
 * @param recordSize Size of the particular record type being requested, e.g. STL_NODE_RECORD
 *
 * @return FSTATUS return code
 */
FSTATUS fe_buildSAQueryResults(PQUERY_RESULT_VALUES* ppQueryResults, SA_MAD **ppRsp, uint32_t recordSize, uint32_t response_length)
{
	uint32_t memSize = 0; 									/* Size of memory to allocate for results */
	uint32_t numRecordsReturned = 0; 						/* The number of records returned from our query */
	PQUERY_RESULT_VALUES pQueryResults = *ppQueryResults; 	/* The query results structure to build */

	// if no records IBTA 1.2.1 says AttributeOffset should be 0
	// Assume we never get here for a non-RMPP (eg. non-GetTable) response
	if ((*ppRsp)->SaHdr.AttributeOffset) 
		numRecordsReturned = (int)((response_length-IBA_SUBN_ADM_HDRSIZE) /
					((*ppRsp)->SaHdr.AttributeOffset * sizeof(uint64)));
	else if ((*ppRsp)->common.mr.AsReg8 == SUBN_ADM_GET_RESP)
		numRecordsReturned = 1; /* Different for Get() (i.e. ClassPortInfo) */
	else
		numRecordsReturned = 0;

	/* Build a new data structure to hold the returned data
	 * This structure contains a QUERY_RESULT_VALUES followed by a record results type
	 * In order to be type indifferent, we effectively build a fake record result using 
	 * the same types used in all record results. The memory layout is as such:
	 *
	 * QUERY_RESULTS_VALUES | QUERY_RESULTS_VALUES->QueryResult | Returned Records
	 *   Normal QRV values     NumXRecords from XRecordResults    "              "
	 */
	memSize = sizeof(QUERY_RESULT_VALUES);
	memSize += sizeof(uint32_t);
	memSize += recordSize * numRecordsReturned;
	pQueryResults = malloc(memSize);
	if (!pQueryResults)
		return FERROR;
	memset(pQueryResults, 0, memSize);

	pQueryResults->Status = FSUCCESS;
	pQueryResults->MadStatus = (*ppRsp)->common.u.NS.Status.AsReg16 = MAD_STATUS_SUCCESS;
	pQueryResults->ResultDataSize = response_length;
	/* This doozy of a line indirectly sets the NumXRecords field of the RECORD_RESULTS type */
	*((uint32_t*)(pQueryResults->QueryResult)) = numRecordsReturned;

	*ppQueryResults = pQueryResults;

	return FSUCCESS;
}

/**
 * Take a built packet request and send it, then receive the response a base query response
 *
 * @param connection The connection to the FE to send our request
 * @param msgID Pointer to the msgID for this transaction
 * @param send_mad Packet to send to the FE
 * @param recv_mad Response packet we received back
 * @param ppQueryResults Where to store the location of the QUERY_RESULT_VALUES that is built
 * @param recordSize Size of the particular record type being requested, e.g. STL_NODE_RECORD
 * @return FSTATUS return code
 */
FSTATUS fe_saQueryCommon(struct net_connection *connection, uint32_t msgID, SA_MAD *send_mad, SA_MAD **recv_mad, PQUERY_RESULT_VALUES* ppQueryResults, uint32_t recordSize)
{
	uint32_t responseLength = 0; 							 					/* The length of the SA response */

	BSWAP_SA_HDR(&send_mad->SaHdr);
	BSWAP_MAD_HEADER((MAD *)send_mad);

	/* Send the packet, receive a response, and build the queryResult from the response */
	if (fe_oob_send_packet(connection, (uint8_t *)send_mad,
						   recordSize + sizeof(MAD_COMMON) + sizeof(SA_MAD_HDR))
		!= FSUCCESS)
		return FERROR;
	if (fe_oob_receive_response(connection, (uint8_t **)(recv_mad), &responseLength))
		return FERROR;

	BSWAP_SA_HDR(&((*recv_mad)->SaHdr));
	BSWAP_MAD_HEADER((MAD *)*recv_mad);

	if ((*recv_mad)->common.TransactionID != msgID)
		return FERROR;
	if ((*recv_mad)->common.u.NS.Status.AsReg16) {
		printf("MAD Request failed, MAD status: %d\n", (*recv_mad)->common.u.NS.Status.AsReg16);
		return FERROR;
	}
	if (fe_buildSAQueryResults(ppQueryResults, recv_mad, recordSize, responseLength) != FSUCCESS)
		return FERROR;

	/* If we got here everything went well, so return successful */
	return FSUCCESS;
}

/**
 * Submit the input query to the FE and return the query result values
 *
 * @param pQuery Input PQUERY to submit to the FE
 * @param connection The connection to the FE to send our request
 * @param ppQueryResults Results returned from the FE
 * @return FSTATUS return code
 */
FSTATUS fe_processSAQuery(PQUERY pQuery, struct net_connection *connection, PQUERY_RESULT_VALUES* ppQueryResults)
{
	FSTATUS fstatus = FSUCCESS; 							/* Return Status */
	PQUERY_RESULT_VALUES pQueryResults = NULL;              /* The results of our query */
	SA_MAD saMad;                                           /* SA Mad packet to be sent */
	SA_MAD *pRsp = NULL;                                    /* Pointer to Mad response */
	static uint32_t msgIDs = 0;                             /* The msgID or Transaction ID of this message */
	uint32_t msgID = 0; 									/* The msgID or TransactionID of this message */
	int i; 													/* Loop variable */

	/* All fields are supposed to be zeroed out if they are not used. */
	memset(&saMad, 0, sizeof(saMad));
	msgID = msgIDs++;
	MAD_SET_TRANSACTION_ID(&saMad, msgID);
	
	/* Process the query based on SA output type */
	switch((int)(pQuery->OutputType)) {
	case OutputTypeStlClassPortInfo:
		{
			STL_CLASS_PORT_INFO_RESULT 	*pCPIR;
			STL_CLASS_PORT_INFO 		*pCPI;
			
			if (pQuery->InputType != InputTypeNoInput)
				break;

			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GET);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_CLASS_PORT_INFO);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_CLASS_PORT_INFO));
			if (fstatus != FSUCCESS) break;

			pCPIR = (STL_CLASS_PORT_INFO_RESULT*)pQueryResults->QueryResult;
			pCPI = &pCPIR->ClassPortInfo;

			// There should only be one ClassPortInfo result.
			if (pCPIR->NumClassPortInfo > 0) {
				*pCPI = *((STL_CLASS_PORT_INFO*)fe_packetRecordOffset(pRsp, 0));
				BSWAP_STL_CLASS_PORT_INFO(pCPI);
			}

		}
		break;

	case OutputTypeInformInfoRecord:
		{   
			INFORM_INFO_RECORD_RESULTS *pIIRR;
			IB_INFORM_INFO_RECORD      *pIIR = (IB_INFORM_INFO_RECORD *)saMad.Data;

			if(!g_gotSourceGid && (pQuery->InputType == InputTypePortGuid)){
				fprintf(stderr, "SourceGid required for InformInfo query of Input=%s\n", iba_sd_query_input_type_msg(pQuery->InputType));
				break;
			}

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypePortGuid:
				saMad.SaHdr.ComponentMask = IB_INFORMINFO_RECORD_COMP_SUBSCRIBERGID;
				pIIR->RID.SubscriberGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pIIR->RID.SubscriberGID.Type.Global.InterfaceID  = pQuery->InputValue.Guid;
				break;
			case InputTypePortGid:
				saMad.SaHdr.ComponentMask = IB_INFORMINFO_RECORD_COMP_SUBSCRIBERGID;
				pIIR->RID.SubscriberGID = pQuery->InputValue.Gid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_IB_INFORM_INFO_RECORD(pIIR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, SA_ATTRIB_INFORM_INFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, IB_BASE_VERSION, MCLASS_SUBN_ADM,
								 IB_SUBN_ADM_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(IB_INFORM_INFO_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pIIRR = (INFORM_INFO_RECORD_RESULTS*)pQueryResults->QueryResult;
			pIIR  = pIIRR->InformInfoRecords;
			for (i=0; i< pIIRR->NumInformInfoRecords; i++, pIIR++) {
				*pIIR = *((IB_INFORM_INFO_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_IB_INFORM_INFO_RECORD(pIIR);
			}
		}
		break;

	case OutputTypeStlInformInfoRecord:
		{   
			STL_INFORM_INFO_RECORD_RESULTS *pIIRR;
			STL_INFORM_INFO_RECORD         *pIIR = (STL_INFORM_INFO_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
			break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_INFORM_INFO_REC_COMP_SUBSCRIBER_LID;
				pIIR->RID.SubscriberLID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pIIR->Reserved = 0;

			BSWAP_STL_INFORM_INFO_RECORD(pIIR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_INFORM_INFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_INFORM_INFO_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pIIRR = (STL_INFORM_INFO_RECORD_RESULTS*)pQueryResults->QueryResult;
			pIIR  = pIIRR->InformInfoRecords;
			for (i=0; i< pIIRR->NumInformInfoRecords; i++, pIIR++) {
				*pIIR = *((STL_INFORM_INFO_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_INFORM_INFO_RECORD(pIIR);
			}
		}
		break;

	case OutputTypeStlNodeRecord:
		{
			STL_NODE_RECORD_RESULTS *pNRR;
			STL_NODE_RECORD      *pNR;

			/* Take care of input types */
			fstatus = fe_fillInNodeRecord(&saMad, pQuery);
			if (fstatus != FSUCCESS) break;

			/* Query the fe for fabric information */
			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_NODE_RECORD));
			if (fstatus != FSUCCESS) break;

			/* Translate the data */
			pNRR = (STL_NODE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pNR = pNRR->NodeRecords;
			for (i=0; i< pNRR->NumNodeRecords; i++, pNR++) {
				*pNR = *(STL_NODE_RECORD *)fe_packetRecordOffset(pRsp, i);
				BSWAP_STL_NODE_RECORD(pNR);
			}
		}
		break;

	case OutputTypeNodeRecord: 		/* Legacy, IB query */
		{
			NODE_RECORD_RESULTS *pNRR;
			IB_NODE_RECORD      *pNR;

			/* Take care of input types */
			fstatus = fe_fillInIbNodeRecord(&saMad, pQuery);
			if (fstatus != FSUCCESS) break;
	
			/* Query the fe for fabric information */
			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(IB_NODE_RECORD));
			if (fstatus != FSUCCESS) break;
	
			/* Translate the data */
			pNRR = (NODE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pNR = pNRR->NodeRecords;
			for (i=0; i< pNRR->NumNodeRecords; i++, pNR++) {
				*pNR = *((IB_NODE_RECORD *)fe_packetRecordOffset(pRsp, i));
				BSWAP_IB_NODE_RECORD(pNR);
			}
		}
		break;

	case OutputTypeStlSystemImageGuid: 
		{
			GUID_RESULTS        *pGR;
			EUI64               *pGuid;
			STL_NODE_RECORD      *pNR;

			fstatus = fe_fillInNodeRecord(&saMad, pQuery);
			if (fstatus != FSUCCESS) break;

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_NODE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pGR   = (GUID_RESULTS*)pQueryResults->QueryResult;
			pGuid = pGR->Guids;
			for (i=0; i< pGR->NumGuids; i++, pGuid++) {
				pNR = ((STL_NODE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				*pGuid = ntoh64(pNR->NodeInfo.SystemImageGUID);
			}
		}
		break;

	case OutputTypeNodeGuid:        
		{
			GUID_RESULTS        *pGR;
			EUI64               *pGuid;
			STL_NODE_RECORD      *pNR;

			fstatus = fe_fillInIbNodeRecord(&saMad, pQuery);
			if (fstatus != FSUCCESS) break;

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_NODE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pGR   = (GUID_RESULTS*)pQueryResults->QueryResult;
			pGuid = pGR->Guids;
			for (i=0; i< pGR->NumGuids; i++, pGuid++) {
				pNR = ((STL_NODE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				*pGuid = ntoh64(pNR->NodeInfo.NodeGUID);
			}
		}
		break;

	case OutputTypeNodeDesc:
		{
			NODEDESC_RESULTS    *pNDR;
			NODE_DESCRIPTION    *pND;
			IB_NODE_RECORD      *pNR;

			fstatus = fe_fillInIbNodeRecord(&saMad, pQuery);
			if (fstatus != FSUCCESS) break;

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(IB_NODE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pNDR = (NODEDESC_RESULTS*)pQueryResults->QueryResult;
			pND  = pNDR->NodeDescs;
			for (i=0; i< pNDR->NumDescs; i++, pND++) {
				pNR = ((IB_NODE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				memcpy (pND, &pNR->NodeDescData.NodeString[0], NODE_DESCRIPTION_ARRAY_SIZE);
			}
		}
		break;

	case OutputTypeStlNodeDesc:
		{
			STL_NODEDESC_RESULTS    *pNDR;
			STL_NODE_DESCRIPTION    *pND;
			STL_NODE_RECORD      *pNR;

			fstatus = fe_fillInNodeRecord(&saMad, pQuery);
			if (fstatus != FSUCCESS) break;

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_NODE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pNDR = (STL_NODEDESC_RESULTS*)pQueryResults->QueryResult;
			pND  = pNDR->NodeDescs;
			for (i=0; i< pNDR->NumDescs; i++, pND++) {
				pNR = ((STL_NODE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				memcpy (pND, &pNR->NodeDesc.NodeString[0], STL_NODE_DESCRIPTION_ARRAY_SIZE);
			}
		}
		break;

	case OutputTypeStlPortGuid:         
		{
			GUID_RESULTS        *pGR;
			EUI64               *pGuid;
			STL_NODE_RECORD      *pNR;

			fstatus = fe_fillInNodeRecord(&saMad, pQuery);
			if (fstatus != FSUCCESS) break;

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_NODE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pGR   = (GUID_RESULTS*)pQueryResults->QueryResult;
			pGuid = pGR->Guids;
			for (i=0; i< pGR->NumGuids; i++, pGuid++) {
				pNR = ((STL_NODE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				*pGuid = ntoh64(pNR->NodeInfo.PortGUID);
			}
		}
		break;

	case OutputTypeStlLid:              
		{
			STL_LID_RESULTS	*pLR;
			//IB_LID              *pLid;
			uint32		*pLid;
			STL_NODE_RECORD	*pNR;

			fstatus = fe_fillInNodeRecord(&saMad, pQuery);
			if (fstatus != FSUCCESS) break;

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_NODE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pLR   = (STL_LID_RESULTS*)pQueryResults->QueryResult;
			pLid  = pLR->Lids;
			for (i=0; i< pLR->NumLids; i++, pLid++) {
				pNR = ((STL_NODE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_NODE_RECORD(pNR);
				*pLid = pNR->RID.LID;
			}
		}
		break;

	case OutputTypePortInfoRecord:
		{
			PORTINFO_RECORD_RESULTS *pPIR;
			IB_PORTINFO_RECORD      *pPI = (IB_PORTINFO_RECORD *)saMad.Data;
			int						extended_data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:     
				break;
			case InputTypeLid:           
				saMad.SaHdr.ComponentMask = IB_PORTINFO_RECORD_COMP_ENDPORTLID;
				pPI->RID.s.EndPortLID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr,"Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_IB_PORTINFO_RECORD(pPI, TRUE);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, SA_ATTRIB_PORTINFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, IB_BASE_VERSION, MCLASS_SUBN_ADM,
								 IB_SUBN_ADM_CLASS_VERSION);

			/* Query the fe for fabric information */
			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(IB_PORTINFO_RECORD));
			if (fstatus != FSUCCESS) break;

			/* Translate the data. */
			pPIR = (PORTINFO_RECORD_RESULTS*)pQueryResults->QueryResult;
			pPI  = pPIR->PortInfoRecords;
			extended_data = ((saMad.SaHdr.AttributeOffset * 8) >=
							  sizeof(IB_PORTINFO_RECORD) );

			for (i=0; i< pPIR->NumPortInfoRecords; i++, pPI++) {
				*pPI = *((IB_PORTINFO_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_IB_PORTINFO_RECORD(pPI, extended_data);
				/* Clear IB_LINK_SPEED_10G if QDR is overloaded */
				if (extended_data) {
					if ( pPI->PortInfoData.CapabilityMask.s.IsExtendedSpeedsSupported &&
						 !( pPI->RID.s.Options &
							IB_PORTINFO_RECORD_OPTIONS_QDRNOTOVERLOADED ) ) {
						if (pPI->PortInfoData.LinkSpeedExt.Active)
							pPI->PortInfoData.LinkSpeed.Active &= ~IB_LINK_SPEED_10G;
						if (pPI->PortInfoData.LinkSpeedExt.Supported)
							pPI->PortInfoData.Link.SpeedSupported &= ~IB_LINK_SPEED_10G;
						if (pPI->PortInfoData.LinkSpeedExt.Enabled)
							pPI->PortInfoData.LinkSpeed.Enabled &= ~IB_LINK_SPEED_10G;
					}
				}
				else {
					pPI->PortInfoData.LinkSpeedExt.Active = 0;
					pPI->PortInfoData.LinkSpeedExt.Supported = 0;
					pPI->PortInfoData.LinkSpeedExt.Enabled = 0;
				}
			}
		}
		break;

	case OutputTypeStlPortInfoRecord:
		{
			STL_PORTINFO_RECORD_RESULTS	*pPIR;
			STL_PORTINFO_RECORD			*pPI = (STL_PORTINFO_RECORD *)(saMad.Data);

			switch (pQuery->InputType) {
			case InputTypeNoInput:     
				break;
			case InputTypeLid:           
				saMad.SaHdr.ComponentMask = STL_PORTINFO_RECORD_COMP_ENDPORTLID;
				pPI->RID.EndPortLID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr,"Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pPI->RID.Reserved = 0;
			pPI->Reserved = 0;

			BSWAP_STL_PORTINFO_RECORD(pPI);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_PORTINFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			/* Query the fe for fabric information */
			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_PORTINFO_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pPIR = (STL_PORTINFO_RECORD_RESULTS*)pQueryResults->QueryResult;
			pPI  = pPIR->PortInfoRecords;

			for (i=0; i< pPIR->NumPortInfoRecords; i++, pPI++) {
				*pPI = *((STL_PORTINFO_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_PORTINFO_RECORD(pPI);
			}
		}
		break;

	case OutputTypeStlSCSCTableRecord:
		{
			STL_SC_MAPPING_TABLE_RECORD_RESULTS *pSCSCRR;
			STL_SC_MAPPING_TABLE_RECORD         *pSCSCR = (STL_SC_MAPPING_TABLE_RECORD *)saMad.Data;

			pSCSCR->Reserved = 0;
			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_SC2SC_RECORD_COMP_LID;
				pSCSCR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_STL_SC_MAPPING_TABLE_RECORD(pSCSCR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SC_MAPTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SC_MAPPING_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			pSCSCRR = (STL_SC_MAPPING_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSCSCR  = pSCSCRR->SCSCRecords;
			for (i=0; i < pSCSCRR->NumSCSCTableRecords; i++, pSCSCR++) {
				*pSCSCR = *((STL_SC_MAPPING_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SC_MAPPING_TABLE_RECORD(pSCSCR);
			}
		}
		break;

	case OutputTypeStlSwitchInfoRecord:
    	{
			STL_SWITCHINFO_RECORD_RESULTS *pSIR;
			STL_SWITCHINFO_RECORD         *pSI = (STL_SWITCHINFO_RECORD *)saMad.Data;

			pSI->Reserved = 0;
			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:           
				saMad.SaHdr.ComponentMask = STL_SWITCHINFO_RECORD_COMP_LID;
				pSI->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_STL_SWITCHINFO_RECORD(pSI);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SWITCHINFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SWITCHINFO_RECORD));
			if (fstatus != FSUCCESS) break;

			pSIR = (STL_SWITCHINFO_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSI = pSIR->SwitchInfoRecords;
 
			for (i = 0; i < pSIR->NumSwitchInfoRecords; ++i, ++pSI) {
				*pSI = *((STL_SWITCHINFO_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SWITCHINFO_RECORD(pSI);
			}
		}
		break;

	case OutputTypeStlLinearFDBRecord:
		{
			STL_LINEAR_FDB_RECORD_RESULTS *pLFRR;
			STL_LINEAR_FORWARDING_TABLE_RECORD *pLFR = (STL_LINEAR_FORWARDING_TABLE_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_LFT_RECORD_COMP_LID;
				pLFR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pLFR->RID.Reserved = 0;

			BSWAP_STL_LINEAR_FORWARDING_TABLE_RECORD(pLFR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_LINEAR_FWDTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_LINEAR_FORWARDING_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pLFRR = (STL_LINEAR_FDB_RECORD_RESULTS*)pQueryResults->QueryResult;
			pLFR  = pLFRR->LinearFDBRecords;
			for (i=0; i< pLFRR->NumLinearFDBRecords; i++, pLFR++) {
				*pLFR = *((STL_LINEAR_FORWARDING_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_LINEAR_FORWARDING_TABLE_RECORD(pLFR);
			}
		}
		break;

	case OutputTypeRandomFDBRecord:
		{
			RANDOM_FDB_RECORD_RESULTS  *pRFRR;
			IB_RANDOM_FDB_RECORD      *pRFR = (IB_RANDOM_FDB_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = IB_RANDOMFDB_RECORD_COMP_LID;
				pRFR->RID.s.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_IB_RANDOM_FDB_RECORD(pRFR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, SA_ATTRIB_RANDOM_FWDTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, IB_BASE_VERSION, MCLASS_SUBN_ADM, IB_SUBN_ADM_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(IB_RANDOM_FDB_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pRFRR = (RANDOM_FDB_RECORD_RESULTS*)pQueryResults->QueryResult;
			pRFR  = pRFRR->RandomFDBRecords;
			for (i=0; i< pRFRR->NumRandomFDBRecords; i++, pRFR++) {
				*pRFR = *((IB_RANDOM_FDB_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_IB_RANDOM_FDB_RECORD(pRFR);
			}
		}
		break;

	case OutputTypeStlMCastFDBRecord:
		{
			STL_MCAST_FDB_RECORD_RESULTS *pMFRR;
			STL_MULTICAST_FORWARDING_TABLE_RECORD *pMFR = (STL_MULTICAST_FORWARDING_TABLE_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_MFTB_RECORD_COMP_LID;
				pMFR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pMFR->RID.u1.s.Reserved = 0;

			BSWAP_STL_MCFTB_RECORD(pMFR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_MCAST_FWDTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_MULTICAST_FORWARDING_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pMFRR = (STL_MCAST_FDB_RECORD_RESULTS*)pQueryResults->QueryResult;
			pMFR  = pMFRR->MCastFDBRecords;
			for (i=0; i< pMFRR->NumMCastFDBRecords; i++, pMFR++) {
				*pMFR = *((STL_MULTICAST_FORWARDING_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_MCFTB_RECORD(pMFR);
			}
		}
		break;

	case OutputTypeStlSMInfoRecord:     
		{   
			STL_SMINFO_RECORD_RESULTS *pSMIR;
			STL_SMINFO_RECORD         *pSMI = (STL_SMINFO_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:     
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pSMI->Reserved = 0;
                           
			BSWAP_STL_SMINFO_RECORD(pSMI);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SMINFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SMINFO_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pSMIR = (STL_SMINFO_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSMI  = pSMIR->SMInfoRecords;
			for (i=0; i< pSMIR->NumSMInfoRecords; i++, pSMI++) {
				*pSMI = *((STL_SMINFO_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SMINFO_RECORD(pSMI);
			}
		}
		break;

	case OutputTypeStlLinkRecord:       
		{
			STL_LINK_RECORD_RESULTS *pLRR;
			STL_LINK_RECORD         *pLR = (STL_LINK_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:     
				break;
			case InputTypeLid:           
				saMad.SaHdr.ComponentMask = IB_LINK_RECORD_COMP_FROMLID;
				pLR->RID.FromLID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pLR->Reserved = 0;

			BSWAP_STL_LINK_RECORD(pLR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_LINK_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_LINK_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pLRR = (STL_LINK_RECORD_RESULTS*)pQueryResults->QueryResult;
			pLR = pLRR->LinkRecords;
			for (i=0; i< pLRR->NumLinkRecords; i++, pLR++) {
				*pLR = *((STL_LINK_RECORD *)(fe_packetRecordOffset(pRsp, i)));;
				BSWAP_STL_LINK_RECORD(pLR);
			}
		}
		break;
	case OutputTypeServiceRecord:   
		{   
			SERVICE_RECORD_RESULTS *pSRR;
			IB_SERVICE_RECORD      *pSR = (IB_SERVICE_RECORD *)saMad.Data;

			if(!g_gotSourceGid && (pQuery->InputType == InputTypePortGuid)){
				fprintf(stderr, "SourceGid required for ServiceRecord query of Input=%s\n", iba_sd_query_input_type_msg(pQuery->InputType));
				break;
			}

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypePortGuid:
				saMad.SaHdr.ComponentMask = IB_SERVICE_RECORD_COMP_SERVICEGID;
				pSR->RID.ServiceGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pSR->RID.ServiceGID.Type.Global.InterfaceID  = pQuery->InputValue.Guid;
				break;
			case InputTypePortGid:
				saMad.SaHdr.ComponentMask = IB_SERVICE_RECORD_COMP_SERVICEGID;
				pSR->RID.ServiceGID = pQuery->InputValue.Gid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_IB_SERVICE_RECORD(pSR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, SA_ATTRIB_SERVICE_RECORD);
			MAD_SET_VERSION_INFO(&saMad, IB_BASE_VERSION, MCLASS_SUBN_ADM,
								 IB_SUBN_ADM_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(IB_SERVICE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pSRR = (SERVICE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSR  = pSRR->ServiceRecords;
			for (i=0; i< pSRR->NumServiceRecords; i++, pSR++) {
				*pSR = *((IB_SERVICE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_IB_SERVICE_RECORD(pSR);
			}
		}
		break;

#ifndef NO_STL_SERVICE_OUTPUT       // Don't output STL Service if defined
	case OutputTypeStlServiceRecord:   
		{   
			STL_SERVICE_RECORD_RESULTS *pSRR;
			STL_SERVICE_RECORD         *pSR = (STL_SERVICE_RECORD *)saMad.Data;

			if(!g_gotSourceGid && (pQuery->InputType == InputTypePortGuid)){
				fprintf(stderr, "SourceGid required for StlServiceRecord query of Input=%s\n", iba_sd_query_input_type_msg(pQuery->InputType));
				break;
			}

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_SERVICE_RECORD_COMP_SERVICELID;
				pSR->RID.ServiceLID = pQuery->InputValue.Lid;
				break;
			case InputTypePortGuid:
				saMad.SaHdr.ComponentMask = STL_SERVICE_RECORD_COMP_SERVICEGID;
				pSR->RID.ServiceGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pSR->RID.ServiceGID.Type.Global.InterfaceID  = pQuery->InputValue.Guid;
				break;
			case InputTypePortGid:
				saMad.SaHdr.ComponentMask = STL_SERVICE_RECORD_COMP_SERVICEGID;
				pSR->RID.ServiceGID = pQuery->InputValue.Gid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pSR->RID.Reserved = 0;
			pSR->Reserved = 0;

			BSWAP_STL_SERVICE_RECORD(pSR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SERVICE_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SERVICE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pSRR = (STL_SERVICE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSR  = pSRR->ServiceRecords;
			for (i=0; i< pSRR->NumServiceRecords; i++, pSR++) {
				*pSR = *((STL_SERVICE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SERVICE_RECORD(pSR);
			}
		}
		break;
#endif

	case OutputTypeStlPKeyTableRecord:  
		{   
			STL_PKEYTABLE_RECORD_RESULTS  *pPKRR;
			STL_P_KEY_TABLE_RECORD        *pPKR = (STL_P_KEY_TABLE_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_PKEYTABLE_RECORD_COMP_LID;
				pPKR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pPKR->Reserved = 0;

			BSWAP_STL_PARTITION_TABLE_RECORD(pPKR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_P_KEY_TABLE_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_P_KEY_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pPKRR = (STL_PKEYTABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pPKR  = pPKRR->PKeyTableRecords;
			for (i=0; i< pPKRR->NumPKeyTableRecords; i++, pPKR++) {
				*pPKR = *((STL_P_KEY_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_PARTITION_TABLE_RECORD(pPKR);
			}
		}
		break;

	case OutputTypePathRecord:       
	case OutputTypePathRecordNetworkOrder:
		{   
			PATH_RESULTS   *pPRR;
			IB_PATH_RECORD *pPR = (IB_PATH_RECORD *)saMad.Data;

			if(!g_gotSourceGid && (pQuery->InputType != InputTypeGidPair)) {
				fprintf(stderr, "SourceGid (-x) argument required for PathRecord query of input type \"%s\"\n", iba_sd_query_input_type_msg(pQuery->InputType));
				break;
			}

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				/* node record query??? */
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->Reversible                    = 1;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypePKey:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_PKEY |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->Reversible                    = 1;
				pPR->P_Key 		                   = pQuery->InputValue.PKey;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypeSL:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_SL |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->Reversible                    = 1;
				pPR->u2.s.SL 	                   = pQuery->InputValue.SL;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypeServiceId:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_SERVICEID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->Reversible                    = 1;
				pPR->ServiceID	                   = pQuery->InputValue.ServiceId;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypePortGuidPair:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DGID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->DGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID  = pQuery->InputValue.PortGuidPair.SourcePortGuid;
				pPR->DGID.Type.Global.InterfaceID  = pQuery->InputValue.PortGuidPair.DestPortGuid;
				pPR->Reversible                    = 1;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypeGidPair:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DGID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->DGID= pQuery->InputValue.GidPair.DestGid;
				pPR->SGID= pQuery->InputValue.GidPair.SourceGid;
				pPR->Reversible = 1;
				pPR->NumbPath = PATHRECORD_NUMBPATH;
				break;

			case InputTypePortGuid:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DGID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->DGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->DGID.Type.Global.InterfaceID  = pQuery->InputValue.Guid;
				pPR->Reversible                    = 1;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypePortGid:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DGID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->DGID                          = pQuery->InputValue.Gid;
				pPR->Reversible                    = 1;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;

			case InputTypeLid:           
				// This is going to be tricky.
				// I need to specify a source and destination for a path to be valid.
				// In this case - I have a dest lid, but my src lid is unknown to me.
				// BUT - I have my port lid and can create my port GID.
				// So - I'll use a gid for the source and a lid for dest....
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DLID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->DLID                          = pQuery->InputValue.Lid;
				pPR->Reversible                    = 1;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypePathRecord:
			case InputTypePathRecordNetworkOrder:
				saMad.SaHdr.ComponentMask = pQuery->InputValue.PathRecordValue.ComponentMask;
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->DGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				*pPR = pQuery->InputValue.PathRecordValue.PathRecord;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			if (pQuery->InputType != InputTypePathRecordNetworkOrder) {
				BSWAP_IB_PATH_RECORD(pPR);
			}

			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, SA_ATTRIB_PATH_RECORD);
			MAD_SET_VERSION_INFO(&saMad,
								 IB_BASE_VERSION, 
								 MCLASS_SUBN_ADM, 
								 IB_SUBN_ADM_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(IB_PATH_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pPRR = (PATH_RESULTS*)pQueryResults->QueryResult;
			pPR  = pPRR->PathRecords;
			for (i=0; i< pPRR->NumPathRecords; i++, pPR++) {
				*pPR = *((IB_PATH_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				/* We do not want to swap the OutputTypePathRecordNetworkOrder type */
				if (pQuery->OutputType == OutputTypePathRecord) {
					BSWAP_IB_PATH_RECORD(pPR);
				}
			}
		}           
		break;

	case OutputTypeStlVLArbTableRecord: 
		{
			STL_VLARBTABLE_RECORD_RESULTS  *pVLRR;
			STL_VLARBTABLE_RECORD         *pVLR = (STL_VLARBTABLE_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_VLARB_COMPONENTMASK_LID;
				pVLR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pVLR->Reserved = 0;

			BSWAP_STL_VLARBTABLE_RECORD(pVLR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_VLARBTABLE_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_VLARBTABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pVLRR = (STL_VLARBTABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pVLR  = pVLRR->VLArbTableRecords;
			for (i=0; i< pVLRR->NumVLArbTableRecords; i++, pVLR++) {
				*pVLR = *((STL_VLARBTABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_VLARBTABLE_RECORD(pVLR);
			}
		}
		break;

#ifndef NO_STL_MCMEMBER_OUTPUT       // Don't output STL McMember if defined
	case OutputTypeStlMcMemberRecord:  
        {   
			STL_MCMEMBER_RECORD_RESULTS *pMCRR;
			STL_MCMEMBER_RECORD         *pMCR = (STL_MCMEMBER_RECORD *)saMad.Data;

			if(!g_gotSourceGid && (pQuery->InputType == InputTypePortGuid)){
				fprintf(stderr, "SourceGid required for StlMcMemberRecord query of Input=%s\n", iba_sd_query_input_type_msg(pQuery->InputType));
				break;
			}

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypePortGuid:
				saMad.SaHdr.ComponentMask = STL_MCMEMBER_COMPONENTMASK_PORTGID;
				pMCR->RID.PortGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pMCR->RID.PortGID.Type.Global.InterfaceID  = pQuery->InputValue.Guid;
				break;
			case InputTypePortGid:
				saMad.SaHdr.ComponentMask = STL_MCMEMBER_COMPONENTMASK_PORTGID;
				pMCR->RID.PortGID = pQuery->InputValue.Gid;
				break;           
			case InputTypeMcGid:
				saMad.SaHdr.ComponentMask = STL_MCMEMBER_COMPONENTMASK_MGID;
				pMCR->RID.MGID = pQuery->InputValue.Gid;
				break;           
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_MCMEMBER_COMPONENTMASK_MLID;
				pMCR->MLID = pQuery->InputValue.Lid;
				break;
			case InputTypePKey:
				saMad.SaHdr.ComponentMask = STL_MCMEMBER_COMPONENTMASK_PKEY;
				pMCR->P_Key = pQuery->InputValue.PKey;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pMCR->Reserved = 0;
			pMCR->Reserved2 = 0;
			pMCR->Reserved3 = 0;
			pMCR->Reserved4 = 0;
			pMCR->Reserved5 = 0;

			BSWAP_STL_MCMEMBER_RECORD(pMCR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_MCMEMBER_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_MCMEMBER_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pMCRR = (STL_MCMEMBER_RECORD_RESULTS*)pQueryResults->QueryResult;
			pMCR  = pMCRR->McMemberRecords;
			for (i=0; i< pMCRR->NumMcMemberRecords; i++, pMCR++) {
				*pMCR = *((STL_MCMEMBER_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_MCMEMBER_RECORD(pMCR);
			}
		}
		break;
#endif

	case OutputTypeMcMemberRecord:
		{
			MCMEMBER_RECORD_RESULTS *pIbMCRR;
			IB_MCMEMBER_RECORD      *pIbMCR = (IB_MCMEMBER_RECORD *)saMad.Data;

			if(!g_gotSourceGid && (pQuery->InputType == InputTypePortGuid)){
				fprintf(stderr, "SourceGid required for McMemberRecord query of Input=%s\n", iba_sd_query_input_type_msg(pQuery->InputType));
				break;
			}

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypePortGuid:
				saMad.SaHdr.ComponentMask = IB_MCMEMBER_RECORD_COMP_PORTGID;
				pIbMCR->RID.PortGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pIbMCR->RID.PortGID.Type.Global.InterfaceID  = pQuery->InputValue.Guid;
				break;
			case InputTypePortGid:
				saMad.SaHdr.ComponentMask = IB_MCMEMBER_RECORD_COMP_PORTGID;
				pIbMCR->RID.PortGID = pQuery->InputValue.Gid;
				break;           
			case InputTypeMcGid:
				saMad.SaHdr.ComponentMask = IB_MCMEMBER_RECORD_COMP_MGID;
				pIbMCR->RID.MGID = pQuery->InputValue.Gid;
				break;           
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = IB_MCMEMBER_RECORD_COMP_MLID;
				pIbMCR->MLID = pQuery->InputValue.Lid;
				break;
			case InputTypePKey:
				saMad.SaHdr.ComponentMask = IB_MCMEMBER_RECORD_COMP_PKEY;
				pIbMCR->P_Key = pQuery->InputValue.PKey;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_IB_MCMEMBER_RECORD(pIbMCR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, SA_ATTRIB_MCMEMBER_RECORD);

			MAD_SET_VERSION_INFO(&saMad, IB_BASE_VERSION, MCLASS_SUBN_ADM,
								 IB_SUBN_ADM_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(IB_MCMEMBER_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pIbMCRR = (MCMEMBER_RECORD_RESULTS*)pQueryResults->QueryResult;
			pIbMCR  = pIbMCRR->McMemberRecords;
			for (i=0; i< pIbMCRR->NumMcMemberRecords; i++, pIbMCR++) {
				*pIbMCR = *((IB_MCMEMBER_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_IB_MCMEMBER_RECORD(pIbMCR);
			}
		}
		break;

	case OutputTypeStlTraceRecord:   
        {   
			IB_PATH_RECORD           *pPR = (IB_PATH_RECORD *)saMad.Data;
			STL_TRACE_RECORD_RESULTS 	*pTRR;
			STL_TRACE_RECORD			*pTR;

			if(!g_gotSourceGid && (pQuery->InputType == InputTypeGidPair)){
				fprintf(stderr, "SourceGid required for TraceRecord query of Input=%s\n", iba_sd_query_input_type_msg(pQuery->InputType));
				break;
			}

			switch (pQuery->InputType) {
			case InputTypePathRecord:
				saMad.SaHdr.ComponentMask = pQuery->InputValue.PathRecordValue.ComponentMask;
				*pPR = pQuery->InputValue.PathRecordValue.PathRecord;
				break;
			case InputTypePortGuid:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DGID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->DGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->DGID.Type.Global.InterfaceID  = pQuery->InputValue.Guid;
				pPR->Reversible                    = 1;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypePortGid:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DGID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID = g_sourceGid.Type.Global.InterfaceID;
				pPR->DGID                          = pQuery->InputValue.Gid;
				pPR->Reversible                    = 1;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypePortGuidPair:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DGID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->SGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->DGID.Type.Global.SubnetPrefix = g_sourceGid.Type.Global.SubnetPrefix;
				pPR->SGID.Type.Global.InterfaceID  = pQuery->InputValue.PortGuidPair.SourcePortGuid;
				pPR->DGID.Type.Global.InterfaceID  = pQuery->InputValue.PortGuidPair.DestPortGuid;
				pPR->Reversible                    = 1;
				pPR->NumbPath                      = PATHRECORD_NUMBPATH;
				break;
			case InputTypeGidPair:
				saMad.SaHdr.ComponentMask = IB_PATH_RECORD_COMP_DGID |
					IB_PATH_RECORD_COMP_SGID |
					IB_PATH_RECORD_COMP_REVERSIBLE |
					IB_PATH_RECORD_COMP_NUMBPATH; 
				pPR->DGID= pQuery->InputValue.GidPair.DestGid;
				pPR->SGID= pQuery->InputValue.GidPair.SourceGid;
				pPR->Reversible = 1;
				pPR->NumbPath = PATHRECORD_NUMBPATH;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}
                           
			BSWAP_IB_PATH_RECORD(pPR);
			MAD_SET_METHOD_TYPE (&saMad, SUBN_ADM_GETTRACETABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_TRACE_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);
			
			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_TRACE_RECORD));
			if (fstatus != FSUCCESS) break;


			// Translate the data.
			pTRR = (STL_TRACE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pTR  = pTRR->TraceRecords;
			for (i=0; i< pTRR->NumTraceRecords; i++, pTR++) {
				*pTR = *((STL_TRACE_RECORD *)(fe_packetRecordOffset(pRsp, i)));

				pTR->Reserved = 0;
				pTR->Reserved2 = 0;
				BSWAP_STL_TRACE_RECORD(pTR);
				pTR->NodeID ^= STL_TRACE_RECORD_COMP_ENCRYPT_MASK;
				pTR->ChassisID ^= STL_TRACE_RECORD_COMP_ENCRYPT_MASK;
				pTR->EntryPortID ^= STL_TRACE_RECORD_COMP_ENCRYPT_MASK;
				pTR->ExitPortID ^= STL_TRACE_RECORD_COMP_ENCRYPT_MASK;
			}
		}           
		break;

	case OutputTypeStlSLSCTableRecord:
		{
			STL_SL2SC_MAPPING_TABLE_RECORD_RESULTS *pSLSCRR;
			STL_SL2SC_MAPPING_TABLE_RECORD         *pSLSCR = (STL_SL2SC_MAPPING_TABLE_RECORD *)saMad.Data;

			pSLSCR->Reserved2 = 0;
			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_SL2SC_RECORD_COMP_LID;
				pSLSCR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_STL_SL2SC_MAPPING_TABLE_RECORD(pSLSCR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SL2SC_MAPTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SL2SC_MAPPING_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			pSLSCRR = (STL_SL2SC_MAPPING_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSLSCR  = pSLSCRR->SLSCRecords;
			for (i=0; i < pSLSCRR->NumSLSCTableRecords; i++, pSLSCR++) {
				*pSLSCR = *((STL_SL2SC_MAPPING_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SL2SC_MAPPING_TABLE_RECORD(pSLSCR);
			}
		}
		break;

	case OutputTypeStlSCSLTableRecord:
		{
			STL_SC2SL_MAPPING_TABLE_RECORD_RESULTS *pSCSLRR;
			STL_SC2SL_MAPPING_TABLE_RECORD         *pSCSLR = (STL_SC2SL_MAPPING_TABLE_RECORD *)saMad.Data;

			pSCSLR->Reserved2 = 0;
			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_SC2SL_RECORD_COMP_LID;
				pSCSLR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_STL_SC2SL_MAPPING_TABLE_RECORD(pSCSLR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SC2SL_MAPTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SC2SL_MAPPING_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			pSCSLRR = (STL_SC2SL_MAPPING_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSCSLR  = pSCSLRR->SCSLRecords;
			for (i=0; i < pSCSLRR->NumSCSLTableRecords; i++, pSCSLR++) {
				*pSCSLR = *((STL_SC2SL_MAPPING_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SC2SL_MAPPING_TABLE_RECORD(pSCSLR);
			}
		}
		break;

	case OutputTypeStlSCVLntTableRecord:
		{
			STL_SC2PVL_NT_MAPPING_TABLE_RECORD_RESULTS	*pSCVLntRR;
			STL_SC2PVL_NT_MAPPING_TABLE_RECORD			*pSCVLntR = (STL_SC2PVL_NT_MAPPING_TABLE_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_SC2VL_R_RECORD_COMP_LID;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR;
				goto done;
			}

			BSWAP_STL_SC2VL_R_MAPPING_TABLE_RECORD(pSCVLntR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SC2VL_NT_MAPTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);
			
			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SC2PVL_NT_MAPPING_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			pSCVLntRR = (STL_SC2PVL_NT_MAPPING_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSCVLntR = pSCVLntRR->SCVLntRecords;
			for (i=0; i < pSCVLntRR->NumSCVLntTableRecords; i++, pSCVLntR++) {
				*pSCVLntR = *((STL_SC2PVL_NT_MAPPING_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SC2VL_R_MAPPING_TABLE_RECORD(pSCVLntR);
			}
		}
		break;

	case OutputTypeStlSCVLtTableRecord:
		{
			STL_SC2PVL_T_MAPPING_TABLE_RECORD_RESULTS	*pSCVLtRR;
			STL_SC2PVL_T_MAPPING_TABLE_RECORD			*pSCVLtR = (STL_SC2PVL_T_MAPPING_TABLE_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_SC2VL_R_RECORD_COMP_LID;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR;
				goto done;
			}

			BSWAP_STL_SC2VL_R_MAPPING_TABLE_RECORD(pSCVLtR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SC2VL_T_MAPTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);
			
			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SC2PVL_T_MAPPING_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			pSCVLtRR = (STL_SC2PVL_T_MAPPING_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pSCVLtR = pSCVLtRR->SCVLtRecords;
			for (i=0; i < pSCVLtRR->NumSCVLtTableRecords; i++, pSCVLtR++) {
				*pSCVLtR = *((STL_SC2PVL_T_MAPPING_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SC2VL_R_MAPPING_TABLE_RECORD(pSCVLtR);
			}

		}
		break;

	case OutputTypeStlPortGroupFwdRecord:
		{
			STL_PORT_GROUP_FORWARDING_TABLE_RECORD_RESULTS *pPGFDBRR;
			STL_PORT_GROUP_FORWARDING_TABLE_RECORD *pPGFDBR = (STL_PORT_GROUP_FORWARDING_TABLE_RECORD*)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_PGFWDTB_RECORD_COMP_LID;
				pPGFDBR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pPGFDBR->RID.u1.s.Reserved = 0;

			BSWAP_STL_PORT_GROUP_FORWARDING_TABLE_RECORD(pPGFDBR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_PGROUP_FWDTBL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_PORT_GROUP_FORWARDING_TABLE_RECORD));
			if(fstatus != FSUCCESS) break;

			// Translate the data
			pPGFDBRR = (STL_PORT_GROUP_FORWARDING_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pPGFDBR = pPGFDBRR->Records;
            
			for(i = 0; i < pPGFDBRR->NumRecords; i++, pPGFDBR++)
			{
				*pPGFDBR = * ((STL_PORT_GROUP_FORWARDING_TABLE_RECORD*)(fe_packetRecordOffset(pRsp, i)));
                BSWAP_STL_PORT_GROUP_FORWARDING_TABLE_RECORD(pPGFDBR);
			}
		}
		break;

	case OutputTypeStlCableInfoRecord:
		{
			STL_CABLE_INFO_RECORD_RESULTS *pCIRR;
			STL_CABLE_INFO_RECORD *pCIR = (STL_CABLE_INFO_RECORD *)saMad.Data;
		
			switch (pQuery->InputType) {
				case InputTypeNoInput:
					break;
				case InputTypeLid:
					saMad.SaHdr.ComponentMask |= STL_CIR_COMP_LID;
					pCIR->LID = pQuery->InputValue.Lid;
					break;
				default:
					fprintf(stderr, "Query not supported by fequery: Input=%s, Output=%s\n",
							iba_sd_query_input_type_msg(pQuery->InputType),
							iba_sd_query_result_type_msg(pQuery->OutputType));
					fstatus = FERROR; goto done;
					break;
			}

			pCIR->Reserved = 0;

			// Default values.
			saMad.SaHdr.ComponentMask |= STL_CIR_COMP_LEN | STL_CIR_COMP_ADDR;
			pCIR->Length = STL_CABLE_INFO_PAGESZ - 1;
			pCIR->u1.s.Address = STL_CIB_STD_START_ADDR;

			BSWAP_STL_CABLE_INFO_RECORD(pCIR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_CABLE_INFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);
			
			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_CABLE_INFO_RECORD));
			if (fstatus != FSUCCESS) break;

			pCIRR = (STL_CABLE_INFO_RECORD_RESULTS *)pQueryResults->QueryResult;
			pCIR = pCIRR->CableInfoRecords;
			for (i=0; i < pCIRR->NumCableInfoRecords; ++i, pCIR++) {
				*pCIR = *((STL_CABLE_INFO_RECORD*)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_CABLE_INFO_RECORD(pCIR);
			}
		}
		break;

	case OutputTypeStlVfInfoRecord:  
		{   
			STL_VFINFO_RECORD_RESULTS *pVFRR;
			STL_VFINFO_RECORD         *pVFR = (STL_VFINFO_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypePKey:
				saMad.SaHdr.ComponentMask = STL_VFINFO_REC_COMP_PKEY;
				pVFR->pKey = pQuery->InputValue.PKey;
				break;
			case InputTypeSL:
				saMad.SaHdr.ComponentMask = STL_VFINFO_REC_COMP_SL;
				pVFR->s1.sl = pQuery->InputValue.SL;
				break;
			case InputTypeServiceId:
				saMad.SaHdr.ComponentMask = STL_VFINFO_REC_COMP_SERVICEID;
				pVFR->ServiceID = pQuery->InputValue.ServiceId;
				break;
			case InputTypeMcGid:
				saMad.SaHdr.ComponentMask = STL_VFINFO_REC_COMP_MGID;
				pVFR->MGID = pQuery->InputValue.Gid;
				break;           
			case InputTypeIndex:
				saMad.SaHdr.ComponentMask = STL_VFINFO_REC_COMP_INDEX;
				pVFR->vfIndex = pQuery->InputValue.vfIndex;
				break;
			case InputTypeNodeDesc:
				saMad.SaHdr.ComponentMask = STL_VFINFO_REC_COMP_NAME;
				if (pQuery->InputValue.NodeDesc.NameLength > 63)
					return (FERROR);

				memcpy(pVFR->vfName,
					   pQuery->InputValue.NodeDesc.Name,
					   pQuery->InputValue.NodeDesc.NameLength);
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pVFR->rsvd1 = 0;
			pVFR->s1.rsvd2 = 0;
			pVFR->s1.rsvd3 = 0;
			pVFR->s1.rsvd4 = 0;
			pVFR->s1.rsvd5 = 0;
			pVFR->rsvd6 = 0;
			memset(pVFR->rsvd7, 0, sizeof(pVFR->rsvd7));

			BSWAP_STL_VFINFO_RECORD(pVFR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_VF_INFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_VFINFO_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pVFRR = (STL_VFINFO_RECORD_RESULTS*)pQueryResults->QueryResult;
			pVFR  = pVFRR->VfInfoRecords;
			for (i=0; i< pVFRR->NumVfInfoRecords; i++, pVFR++) {
				*pVFR = *((STL_VFINFO_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_VFINFO_RECORD(pVFR);
			}
		}
		break;

	case OutputTypeStlPortGroupRecord:
		{
			STL_PORT_GROUP_TABLE_RECORD_RESULTS *pPGTRR;
			STL_PORT_GROUP_TABLE_RECORD *pPGTR = (STL_PORT_GROUP_TABLE_RECORD*)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = STL_PGTB_RECORD_COMP_LID;
				pPGTR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pPGTR->RID.Reserved = 0;
			pPGTR->Reserved2 = 0;

			BSWAP_STL_PORT_GROUP_TABLE_RECORD(pPGTR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_PORTGROUP_TABLE_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_PORT_GROUP_TABLE_RECORD));
			if(fstatus != FSUCCESS) break;

			// Translate the data
			pPGTRR = (STL_PORT_GROUP_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pPGTR = pPGTRR->Records;
            
			for(i = 0; i < pPGTRR->NumRecords; i++, pPGTR++)
			{
				*pPGTR = * ((STL_PORT_GROUP_TABLE_RECORD*)(fe_packetRecordOffset(pRsp, i)));
                BSWAP_STL_PORT_GROUP_TABLE_RECORD(pPGTR);
			}
		}
		break;

	case OutputTypeStlBufCtrlTabRecord:
        {
			STL_BUFFER_CONTROL_TABLE_RECORD_RESULTS *pBCTRR;
			STL_BUFFER_CONTROL_TABLE_RECORD         *pBCTR = (STL_BUFFER_CONTROL_TABLE_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = BFCTRL_COMPONENTMASK_COMP_LID;
				pBCTR->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			memset(pBCTR->Reserved, 0, sizeof(pBCTR->Reserved));

			BSWAP_STL_BUFFER_CONTROL_TABLE_RECORD(pBCTR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_BUFF_CTRL_TAB_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_BUFFER_CONTROL_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data.
			pBCTRR = (STL_BUFFER_CONTROL_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pBCTR = pBCTRR->BufferControlRecords;
			for (i=0; i< pBCTRR->NumBufferControlRecords; i++, pBCTR++) {
				*pBCTR = *((STL_BUFFER_CONTROL_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_BUFFER_CONTROL_TABLE_RECORD(pBCTR);
			}
			break;
        }
		break;

	case OutputTypeStlFabricInfoRecord:
		{
			STL_FABRICINFO_RECORD_RESULT 	*pFIR;
			STL_FABRICINFO_RECORD 		*pFI;
			
			if (pQuery->InputType != InputTypeNoInput)
				break;

			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GET);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_FABRICINFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_FABRICINFO_RECORD));
			if (fstatus != FSUCCESS) break;

			pFIR = (STL_FABRICINFO_RECORD_RESULT*)pQueryResults->QueryResult;
			pFI = &pFIR->FabricInfoRecord;

			// There should only be one FabricInfo result.
			if (pFIR->NumFabricInfoRecords > 0) {
				*pFI = *((STL_FABRICINFO_RECORD*)fe_packetRecordOffset(pRsp, 0));
				BSWAP_STL_FABRICINFO_RECORD(pFI);
			}

		}
		break;

	case OutputTypeStlQuarantinedNodeRecord:
		{
			STL_QUARANTINED_NODE_RECORD_RESULTS *pQNRR;
			STL_QUARANTINED_NODE_RECORD         *pQNR = (STL_QUARANTINED_NODE_RECORD *)saMad.Data;

			if(pQuery->InputType != InputTypeNoInput)
			{
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			BSWAP_STL_QUARANTINED_NODE_RECORD(pQNR);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_QUARANTINED_NODE_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_QUARANTINED_NODE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data
			pQNRR = (STL_QUARANTINED_NODE_RECORD_RESULTS*) pQueryResults->QueryResult;
			pQNR = pQNRR->QuarantinedNodeRecords;
			for(i = 0; i < pQNRR->NumQuarantinedNodeRecords; i++, pQNR++)
			{
				*pQNR = *((STL_QUARANTINED_NODE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_QUARANTINED_NODE_RECORD(pQNR);
			}
			break;
		}

	case OutputTypeStlCongInfoRecord:
		{
			STL_CONGESTION_INFO_RECORD_RESULTS *pRecResults;
			STL_CONGESTION_INFO_RECORD         *pRec = (STL_CONGESTION_INFO_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = CIR_COMPONENTMASK_COMP_LID;
				pRec->LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pRec->reserved = 0;

			BSWAP_STL_CONGESTION_INFO_RECORD(pRec);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_CONGESTION_INFO_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_CONGESTION_INFO_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data
			pRecResults = (STL_CONGESTION_INFO_RECORD_RESULTS*)pQueryResults->QueryResult;
			pRec = pRecResults->Records;
			for(i = 0; i < pRecResults->NumRecords; i++, pRec++)
			{
				*pRec = *((STL_CONGESTION_INFO_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_CONGESTION_INFO_RECORD(pRec);
			}
			break;
		}

	case OutputTypeStlSwitchCongRecord:
		{
			STL_SWITCH_CONGESTION_SETTING_RECORD_RESULTS *pRecResults;
			STL_SWITCH_CONGESTION_SETTING_RECORD         *pRec = (STL_SWITCH_CONGESTION_SETTING_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = SWCSR_COMPONENTMASK_COMP_LID;
				pRec->LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pRec->reserved = 0;

			BSWAP_STL_SWITCH_CONGESTION_SETTING_RECORD(pRec);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SWITCH_CONG_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SWITCH_CONGESTION_SETTING_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data
			pRecResults = (STL_SWITCH_CONGESTION_SETTING_RECORD_RESULTS*)pQueryResults->QueryResult;
			pRec = pRecResults->Records;
			for(i = 0; i < pRecResults->NumRecords; i++, pRec++)
			{
				*pRec = *((STL_SWITCH_CONGESTION_SETTING_RECORD *)(fe_packetRecordOffset(pRsp, i)));
                BSWAP_STL_SWITCH_CONGESTION_SETTING_RECORD(pRec);
			}
			break;
		}

	case OutputTypeStlSwitchPortCongRecord:
		{
			STL_SWITCH_PORT_CONGESTION_SETTING_RECORD_RESULTS *pRecResults;
			STL_SWITCH_PORT_CONGESTION_SETTING_RECORD         *pRec = (STL_SWITCH_PORT_CONGESTION_SETTING_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = SWPCSR_COMPONENTMASK_COMP_LID;
				pRec->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			memset(pRec->Reserved, 0, sizeof(pRec->Reserved));

			BSWAP_STL_SWITCH_PORT_CONGESTION_SETTING_RECORD(pRec);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_SWITCH_PORT_CONG_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_SWITCH_PORT_CONGESTION_SETTING_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data
			pRecResults = (STL_SWITCH_PORT_CONGESTION_SETTING_RECORD_RESULTS*)pQueryResults->QueryResult;
			pRec = pRecResults->Records;
			for(i = 0; i < pRecResults->NumRecords; i++, pRec++)
			{
				*pRec = *((STL_SWITCH_PORT_CONGESTION_SETTING_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_SWITCH_PORT_CONGESTION_SETTING_RECORD(pRec);
			}
			break;
		}

	case OutputTypeStlHFICongRecord:
		{
			STL_HFI_CONGESTION_SETTING_RECORD_RESULTS *pRecResults;
			STL_HFI_CONGESTION_SETTING_RECORD         *pRec = (STL_HFI_CONGESTION_SETTING_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = HCSR_COMPONENTMASK_COMP_LID;
				pRec->LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pRec->reserved = 0;

			BSWAP_STL_HFI_CONGESTION_SETTING_RECORD(pRec);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_HFI_CONG_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_HFI_CONGESTION_SETTING_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data
			pRecResults = (STL_HFI_CONGESTION_SETTING_RECORD_RESULTS*)pQueryResults->QueryResult;
			pRec = pRecResults->Records;
			for(i = 0; i < pRecResults->NumRecords; i++, pRec++)
			{
				*pRec = *((STL_HFI_CONGESTION_SETTING_RECORD *)(fe_packetRecordOffset(pRsp, i)));
				BSWAP_STL_HFI_CONGESTION_SETTING_RECORD(pRec);
			}
			break;
		}

	case OutputTypeStlHFICongCtrlRecord:
		{
			STL_HFI_CONGESTION_CONTROL_TABLE_RECORD_RESULTS *pRecResults;
			STL_HFI_CONGESTION_CONTROL_TABLE_RECORD         *pRec = (STL_HFI_CONGESTION_CONTROL_TABLE_RECORD *)saMad.Data;

			switch (pQuery->InputType) {
			case InputTypeNoInput:
				break;
			case InputTypeLid:
				saMad.SaHdr.ComponentMask = HCCTR_COMPONENTMASK_COMP_LID;
				pRec->RID.LID = pQuery->InputValue.Lid;
				break;
			default:
				fprintf(stderr, "Query Not supported by fequery: Input=%s, Output=%s\n",
						iba_sd_query_input_type_msg(pQuery->InputType),
						iba_sd_query_result_type_msg(pQuery->OutputType));
				fstatus = FERROR; goto done;
			}

			pRec->reserved = 0;

			BSWAP_STL_HFI_CONGESTION_CONTROL_TABLE_RECORD(pRec);
			MAD_SET_METHOD_TYPE(&saMad, SUBN_ADM_GETTABLE);
			MAD_SET_ATTRIB_ID(&saMad, STL_SA_ATTR_HFI_CONG_CTRL_RECORD);
			MAD_SET_VERSION_INFO(&saMad, STL_BASE_VERSION, MCLASS_SUBN_ADM, STL_SA_CLASS_VERSION);

			fstatus = fe_saQueryCommon(connection, msgID, &saMad, &pRsp, &pQueryResults, sizeof(STL_HFI_CONGESTION_CONTROL_TABLE_RECORD));
			if (fstatus != FSUCCESS) break;

			// Translate the data
			pRecResults = (STL_HFI_CONGESTION_CONTROL_TABLE_RECORD_RESULTS*)pQueryResults->QueryResult;
			pRec = pRecResults->Records;
			for(i = 0; i < pRecResults->NumRecords; i++, pRec++)
			{
				*pRec = *((STL_HFI_CONGESTION_CONTROL_TABLE_RECORD *)(fe_packetRecordOffset(pRsp, i)));
                BSWAP_STL_HFI_CONGESTION_CONTROL_TABLE_RECORD(pRec);
			}
			break;
		}

 

	default:
		break;
	}

	done:
	/* Set the results and return our status */
	*ppQueryResults = pQueryResults;
	return fstatus;
}
