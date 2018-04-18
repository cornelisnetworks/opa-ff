/* BEGIN_ICS_COPYRIGHT5 ****************************************

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

** END_ICS_COPYRIGHT5   ****************************************/

/* [ICS VERSION STRING: unknown] */


/*******************************************************************************
 *
 * INCLUDES
 */

#include <stdio.h>

#include "byteswap.h"
#include "opamgt_priv.h"
#include "opasadb_route2.h"
#include "statustext.h"


/*******************************************************************************
 *
 * DEFINES
 */

// Port Handle Table Entry: table of entries, 1 for each created port handle
//  Note that this struct must align with op_route_param_alloc_entry in
//  opasadb_route2.h
struct param_alloc_port_handle_entry
{
	uint64_t reserved;              // Not used
	uint64_t reserved2;             // Not used
	uint64_t reserved3;             // Not used
	OP_ROUTE_PORT_HANDLE port_handle;   // port_handle passed to user
	uint64_t port_guid;             // Port GUID associated w/port_handle
	int port_id;                    // Port ID associated w/port_handle
};


/*******************************************************************************
 *
 * LOCAL VARIABLES
 */

// Port handle table
static struct op_route_param_alloc param_port_handle =
	{ 0, 0, 0, 0, NULL };

// Send/recv timeout mSec (start value 4294 mSec based on RespTimeValue = 19)
static int num_timeout_ms = (2 * 4096 * (1L << 19)) / 1000000;
static int ct_retry = DEFAULT_SD_RETRY_COUNT;   // Send/recv retry count


/*******************************************************************************
 *
 * LOCAL FUNCTIONS
 */

/*******************************************************************************
 *
 * op_route_hton()
 *
 * Description:
 *   Copy host data to network buffer, placing the most significant byte of
 *   interest of host data in the first byte of the network buffer.  The
 *   copy is done a byte at a time so that the endian-ness of the CPU and
 *   word/long-word boundaries in the network buffer do not matter.  The
 *   least significant 'length' bytes of host data are copied.  The pointer
 *   to the network buffer is updated to point to the next available network
 *   data location.
 *
 * Inputs:
 *   pp_bfr_net - Pointer to pointer to network buffer
 *         host - Host data to copy
 *       length - Length of data (in bytes) to copy (max 8)
 *
 * Outputs:
 *   *pp_bfr_net - Next available network data location
 */
static void op_route_hton( uint8_t ** pp_bfr_net,
    uint64_t host,
    int length )
{
	int ix;
	uint8_t * p_bfr_net;

	if ( pp_bfr_net && (p_bfr_net = *pp_bfr_net) && (length > 0) &&
			(length <= 8) )
	{
		for (ix = length, p_bfr_net += (length - 1); ix > 0; host >>= 8, ix--)
			*p_bfr_net-- = host & 0xFF;

		*pp_bfr_net += length;
	}

}  // End of op_route_hton()

/*******************************************************************************
 *
 * op_route_ntoh()
 *
 * Description:
 *   Copy network buffer to host data, placing the first byte of the network
 *   buffer in the most significant byte of interest of host data.  The
 *   copy is done a byte at a time so that the endian-ness of the CPU and
 *   word/long-word boundaries in the network buffer do not matter.  'length'
 *   bytes of the network buffer are copied to the least significant 'length'
 *   bytes of host data, which is returned to the caller.  The pointer to
 *   the network buffer is updated to point to the next available network
 *   data location.
 *
 * Inputs:
 *   pp_bfr_net - Pointer to pointer to network buffer
 *       length - Length of data (in bytes) to copy (max 8)
 *
 * Outputs:
 *   host data - Network data available
 *               *pp_bfr_net = next available network data location
 *           0 - No network data available)
 */
static uint64_t op_route_ntoh( uint8_t ** pp_bfr_net,
    int length )
{
	int ix;
	uint8_t * p_bfr_net;
	uint64_t host = 0;

	if ( pp_bfr_net && (p_bfr_net = *pp_bfr_net) && (length > 0) &&
			(length <= 8) )
	{
		for (ix = length; ix > 0; ix--)
			host = (host << 8) | *p_bfr_net++;

		*pp_bfr_net += length;
	}

	return (host);

}  // End of op_route_ntoh()

/*******************************************************************************
 *
 * op_route_gen_port_handle()
 *
 * Description:
 *   Generate a pseudo-random port handle.  Use multiple calls to rand() if
 *   an int is smaller than OP_ROUTE_PORT_HANDLE or to prevent a port handle
 *   of 0.
 *
 * Inputs:
 *   none
 *
 * Outputs:
 *   OP_ROUTE_PORT_HANDLE
 */
static OP_ROUTE_PORT_HANDLE op_route_gen_port_handle(void)
{
	static unsigned seed_rand = 0;
	OP_ROUTE_PORT_HANDLE port_handle;
	int fb_loop;

	if (!seed_rand)
	{
		seed_rand = (unsigned)time(NULL);
		srand(seed_rand);
	}

	fb_loop = (sizeof(int) < sizeof(OP_ROUTE_PORT_HANDLE));
	for ( port_handle = (OP_ROUTE_PORT_HANDLE)rand(); fb_loop || !port_handle;
			fb_loop = FALSE )
		port_handle = ( (port_handle << (sizeof(int) * 8)) |
			(OP_ROUTE_PORT_HANDLE)rand() );

	return (port_handle);

}  // End of op_route_gen_port_handle()

/*******************************************************************************
 *
 * op_route_get_port_handle_entry()
 *
 * Description:
 *   Get the port table entry associated with the specified port_handle.
 *
 * Inputs:
 *   port_handle - OP_ROUTE_PORT_HANDLE
 *
 * Outputs:
 *   Pointer to port table entry, or NULL if entry not found
 */
static struct param_alloc_port_handle_entry * op_route_get_port_handle_entry(
    OP_ROUTE_PORT_HANDLE port_handle )
{
	uint32_t ix;
	struct param_alloc_port_handle_entry * p_param_entry;

	if (port_handle)
		for ( ix = param_port_handle.num_allocated, p_param_entry =
				(struct param_alloc_port_handle_entry *)param_port_handle.p_params;
				ix > 0; p_param_entry++, ix-- )
			if (p_param_entry->port_handle == port_handle)
				return (p_param_entry);

	return (NULL);

}  // End of op_route_get_port_handle_entry()

/*******************************************************************************
 *
 * op_route_send_recv_query()
 *
 * Description:
 *   Create an OP_ROUTE query, send it and get the response.  The following
 *   queries are supported:
 *     OP_ROUTE_AMOD_CLASS_INFO
 *     OP_ROUTE_AMOD_CREATE_JOB
 *     OP_ROUTE_AMOD_SET_USE_MATRIX
 *     OP_ROUTE_AMOD_POLL_READY
 *     OP_ROUTE_AMOD_COMPLETE_JOB
 *     OP_ROUTE_AMOD_GET_PORTGUID_VEC
 *     OP_ROUTE_AMOD_GET_SWITCH_MAP
 *     OP_ROUTE_AMOD_GET_COST_MATRIX
 *     OP_ROUTE_AMOD_GET_USE_MATRIX
 *     OP_ROUTE_AMOD_GET_JOBS
 *
 * Inputs:
 *         attr_mod - Attribute modifier
 *      port_handle - OP_ROUTE_PORT_HANDLE
 *       optn_query - Query options (AMOD specific)
 *         p_job_id - Pointer to OP_ROUTE_JOB_ID
 *     p_job_status - Pointer to job_status
 *     p_job_params - Pointer to job parameters
 *       p_guid_vec - Pointer to portguid_vec
 *     p_switch_map - Pointer to switch_map
 *   pp_cost_matrix - Pointer to pointer to cost_matrix
 *     p_use_matrix - Pointer to use_matrix
 *  	 p_job_list - Pointer to job_list
 *  	       port - Pointer to opamgt handler
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Query successful
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle invalid
 *                                 - p_job_status NULL
 *                                 - p_guid_vec NULL
 *                                 - p_switch_map NULL
 *                                 - pp_cost_matrix NULL
 *                                 - p_use_matrix NULL
 *           OP_ROUTE_STATUS_ERROR - Registration error
 *                                 - Unable to allocate memory
 */
static enum op_route_status op_route_send_recv_query( uint32_t attr_mod,
    OP_ROUTE_PORT_HANDLE port_handle,
    uint16_t optn_query,
    OP_ROUTE_JOB_ID * p_job_id,
    enum op_route_job_status * p_job_status,
    struct op_route_job_parameters * p_job_params,
    struct op_route_portguid_vec * p_guid_vec,
    struct op_route_switch_map * p_switch_map,
    uint16_t ** pp_cost_matrix,
    struct op_route_use_matrix * p_use_matrix,
    struct op_route_job_list * p_job_list,
    struct omgt_port * port)
{
	FSTATUS fstatus;
	enum op_route_status rstatus = OP_ROUTE_STATUS_OK;
	int ix, ix_2, ix_3;
	struct param_alloc_port_handle_entry * p_port_handle_entry = NULL;
	SA_MAD * p_mad_send = NULL;
	SA_MAD * p_mad_recv = NULL;
	uint8_t * p_data_wire;
	size_t len_data_alloc = sizeof(MAD_COMMON) + sizeof(RMPP_HEADER) +
		sizeof(SA_HDR);
	int len_recv;
	int qp_sm = 1;
	uint8_t base_version;
	uint8_t mgmt_class;
	uint8_t class_version;
	uint8_t class_method;
	MAD_STATUS mad_status;
	static uint32_t trans_id = 1;
	uint16_t attr_id;
	uint32_t attr_mod_2;

	uint8_t wstatus;
	OP_ROUTE_JOB_ID job_id;
	uint64_t * p_guids = NULL;
	uint64_t * p_guids_2;
	uint16_t * p_switch_indices = NULL;
	uint16_t * p_switch_indices_2;
	uint16_t * p_cost_matrix_2 = NULL;
	uint16_t * p_cost_matrix_l;
	uint16_t * p_cost_matrix_l2;
	uint16_t * p_cost_matrix_r;
	uint16_t * p_cost_matrix_r2;
	struct op_route_use_element * p_use_elements = NULL;
	struct op_route_use_element * p_use_elements_2;
	struct op_route_job_info * p_job_info = NULL;
	struct op_route_job_info * p_job_info_2;
	uint16_t num_guids=-1, num_guids_2=-1;
	uint16_t num_switches=-1, num_switches_2=-1;
	uint16_t num_elements=-1;
	uint16_t use_multiplier;
	uint8_t default_use;
	uint16_t num_jobs;
	uint64_t temp;
	struct omgt_mad_addr addr;
	uint8_t port_state;

	if (p_guid_vec) num_guids = p_guid_vec->num_guids;

	// Common processing
	if (!(p_port_handle_entry = op_route_get_port_handle_entry(port_handle)))
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Ensure the port is active
	(void)omgt_port_get_port_state(port, &port_state);
	if (port_state != PortStateActive)
		return (OP_ROUTE_STATUS_ERROR);
	
	// Construct query based on attribute modifier
	switch (attr_mod)
	{

	case OP_ROUTE_AMOD_CLASS_INFO:
	case OP_ROUTE_AMOD_GET_JOBS:
	{
		if(!(p_mad_send = calloc(1, len_data_alloc)))
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		break;

	}  // End of case OP_ROUTE_AMOD_CLASS_INFO / etc

	case OP_ROUTE_AMOD_CREATE_JOB:
	{
		len_data_alloc += ( sizeof(uint16_t) + OP_ROUTE_MAX_JOB_NAME_LEN +
			OP_ROUTE_MAX_APP_NAME_LEN + sizeof(uint64_t) + sizeof(uint64_t) +
			sizeof(uint16_t) + (num_guids * sizeof(uint64_t)) );

		if(!(p_mad_send = calloc(1, len_data_alloc)))
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		p_data_wire = (uint8_t *)(p_mad_send->Data);
		op_route_hton(&p_data_wire, optn_query, sizeof(uint16_t));
		strcpy((char *)p_data_wire, p_job_params->name);
		p_data_wire += OP_ROUTE_MAX_JOB_NAME_LEN;
		strcpy((char *)p_data_wire, p_job_params->application_name);
		p_data_wire += OP_ROUTE_MAX_APP_NAME_LEN;
		op_route_hton(&p_data_wire, p_job_params->pid, sizeof(p_job_params->pid));
		op_route_hton(&p_data_wire, p_job_params->uid, sizeof(p_job_params->uid));
		op_route_hton(&p_data_wire, num_guids, sizeof(uint16_t));
		for (ix = 0; ix < num_guids; ix++)
			op_route_hton( &p_data_wire, p_guid_vec->p_guids[ix],
				sizeof(uint64_t) );

		break;

	}  // End of case OP_ROUTE_AMOD_CREATE_JOB

	case OP_ROUTE_AMOD_SET_USE_MATRIX:
	{
		len_data_alloc += ( sizeof(OP_ROUTE_JOB_ID) +
			sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t) +
			( p_use_matrix->num_elements *
			(sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint8_t)) ) );

		if(!(p_mad_send = calloc(1, len_data_alloc)))
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		p_data_wire = (uint8_t *)(p_mad_send->Data);
		op_route_hton(&p_data_wire, *p_job_id, sizeof(OP_ROUTE_JOB_ID));
		op_route_hton(&p_data_wire, p_use_matrix->multiplier, sizeof(uint16_t));
		*p_data_wire++ = p_use_matrix->default_use;
		op_route_hton(&p_data_wire, p_use_matrix->num_elements, sizeof(uint16_t));

		for ( ix = p_use_matrix->num_elements,
				p_use_elements = p_use_matrix->p_elements; ix > 0; p_use_elements++, ix-- )
		{
			op_route_hton( &p_data_wire, (p_use_elements->bursty << 15) |
				p_use_elements->switch_index, sizeof(uint16_t) );
			op_route_hton(&p_data_wire, p_use_elements->dlid, sizeof(uint16_t));
			*p_data_wire++ = p_use_elements->use;
		}

		break;

	}  // End of case OP_ROUTE_AMOD_SET_USE_MATRIX

	case OP_ROUTE_AMOD_POLL_READY:
	case OP_ROUTE_AMOD_COMPLETE_JOB:
	case OP_ROUTE_AMOD_GET_PORTGUID_VEC:
	case OP_ROUTE_AMOD_GET_SWITCH_MAP:
	case OP_ROUTE_AMOD_GET_COST_MATRIX:
	case OP_ROUTE_AMOD_GET_USE_MATRIX:
	{
		len_data_alloc += sizeof(OP_ROUTE_JOB_ID);
		if(!(p_mad_send = calloc(1, len_data_alloc)))
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		p_data_wire = (uint8_t *)(p_mad_send->Data);
		op_route_hton(&p_data_wire, *p_job_id, sizeof(OP_ROUTE_JOB_ID));

		break;

	}  // End of case OP_ROUTE_AMOD_POLL_READY / etc

	// Invalid attribute modifier
	default:
		printf("op_route_send_recv_query: invalid AMOD:%d\n", attr_mod);
		rstatus = OP_ROUTE_STATUS_ERROR;
		goto cleanup;

	}  // End of switch (attr_mod)

	// Prep query for send; send query and receive response
	MAD_SET_ATTRIB_MOD(p_mad_send, attr_mod);
	MAD_SET_METHOD_TYPE(p_mad_send, SUBN_ADM_GETMULTI);
	MAD_SET_VERSION_INFO( p_mad_send, IB_BASE_VERSION, MCLASS_SUBN_ADM,
		IB_COMM_MGT_CLASS_VERSION );
	MAD_SET_TRANSACTION_ID(p_mad_send, trans_id++);
	MAD_SET_ATTRIB_ID(p_mad_send, SA_ATTRIB_JOB_ROUTE_RECORD);

	if (len_data_alloc > MAD_BLOCK_SIZE)
		p_mad_send->RmppHdr.RmppFlags.s.Active = 1;

	BSWAP_MAD_HEADER((MAD *)p_mad_send);
	BSWAP_SA_HDR(&p_mad_send->SaHdr);

	// Send query

	memset(&addr, 0, sizeof(addr));
	(void)omgt_port_get_port_sm_lid(port, &addr.lid);
	addr.qpn = qp_sm;
	addr.qkey = QP1_WELL_KNOWN_Q_KEY;
	addr.pkey = OMGT_DEFAULT_PKEY;

	fstatus = omgt_send_recv_mad_alloc(port, 
									  (uint8_t *)p_mad_send, len_data_alloc, 
									  &addr,
									  (uint8_t **)&p_mad_recv, (size_t *)&len_recv, 
									  num_timeout_ms, 
									  ct_retry);

	if (fstatus != FSUCCESS || !p_mad_recv)
	{
		if (fstatus == FTIMEOUT || fstatus == FNOT_DONE)
		{
			rstatus = OP_ROUTE_STATUS_TIMEOUT;
			goto cleanup;
		}
		else
		{
			rstatus = OP_ROUTE_STATUS_RECV_ERROR;
			goto cleanup;
		}
	}

	// Fix endian of the received data & check MAD header
	if ((len_recv -= IBA_SUBN_ADM_HDRSIZE) < 0)
	{
		rstatus = OP_ROUTE_STATUS_ERROR;
		goto cleanup;
	}

	BSWAP_MAD_HEADER((MAD *)(p_mad_recv));
	BSWAP_SA_HDR(&p_mad_recv->SaHdr);
	MAD_GET_VERSION_INFO(p_mad_recv, &base_version, &mgmt_class, &class_version);
	MAD_GET_METHOD_TYPE(p_mad_recv, &class_method);
	MAD_GET_STATUS(p_mad_recv, &mad_status);
	MAD_GET_ATTRIB_ID(p_mad_recv, &attr_id);
	MAD_GET_ATTRIB_MOD(p_mad_recv, &attr_mod_2);
	
	if ( (base_version < OP_ROUTE_MIN_BASE_VERSION) ||
			(mgmt_class != MCLASS_SUBN_ADM) ||
			(class_version < OP_ROUTE_MIN_SA_CLASS_VERSION) ||
			(class_method != SUBN_ADM_GETMULTI) || mad_status.AsReg16 ||
			(attr_id != SA_ATTRIB_JOB_ROUTE_RECORD) || (attr_mod_2 != attr_mod) )
	{
		rstatus = OP_ROUTE_STATUS_ERROR;
		goto cleanup;
	}

	// Get wire status in recv data & process common errors
	if ((len_recv -= 1) < 0)
	{
		rstatus = OP_ROUTE_STATUS_ERROR;
		goto cleanup;
	}

	p_data_wire = (uint8_t *)(p_mad_recv->Data);
	wstatus = (enum op_route_wire_status) *p_data_wire++;

	if (wstatus == OP_ROUTE_WIRE_STATUS_INVALID_JOB)
	{
		rstatus = OP_ROUTE_STATUS_INVALID_JOB;
		goto cleanup;
	}

	if (wstatus == OP_ROUTE_WIRE_STATUS_ERROR)
	{
		rstatus = OP_ROUTE_STATUS_ERROR;
		goto cleanup;
	}

	// Process recv data based on attribute modifier
	switch (attr_mod)
	{

	case OP_ROUTE_AMOD_CLASS_INFO:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Check for minimum amount of received data
		if ((len_recv -= 16) < 0)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Check JM class version
		p_data_wire += 1;
		if ( (temp = op_route_ntoh(&p_data_wire, sizeof(uint8_t))) <
				OP_ROUTE_MIN_JM_CLASS_VERSION )
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Get RespTimeValue and calculate num_timeout_ms; instead of doubling
		//  RespTimeValue-based value, could add PortInfo:SubnetTimeout 
		p_data_wire += 5;
		num_timeout_ms = ( 2 * 4096 *
			(1L << (op_route_ntoh(&p_data_wire, sizeof(uint8_t)) & 0x1F)) ) /
			1000000;

		// Verify redirection GID is 0
		if ((temp = op_route_ntoh(&p_data_wire, sizeof(uint64_t))))
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}
		
		// Ignore remaining received data
		len_recv = 0;

		break;

	}  // End of case OP_ROUTE_AMOD_CLASS_INFO

	case OP_ROUTE_AMOD_CREATE_JOB:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
			rstatus = OP_ROUTE_STATUS_OK_PARTIAL;

		// Check for minimum amount of received data
		if ( ( len_recv -= ( sizeof(OP_ROUTE_JOB_ID) +
				((num_guids + 2) * sizeof(uint16_t)) ) ) < 0 )
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Get job ID
		job_id = op_route_ntoh(&p_data_wire, sizeof(OP_ROUTE_JOB_ID));

		// Get switch map
		if ((len_data_alloc = num_guids * sizeof(uint16_t)))
		{
			if (!(p_switch_indices = calloc(1, len_data_alloc)))
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			num_guids_2 = op_route_ntoh(&p_data_wire, sizeof(uint16_t));
			num_switches = op_route_ntoh(&p_data_wire, sizeof(uint16_t));

			if ( (num_guids_2 != num_guids) ||
					(num_switches > OP_ROUTE_MAX_NUM_SWITCHES) ||
					(num_switches > num_guids) )
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			for ( ix = num_guids, p_switch_indices_2 = p_switch_indices;
					ix > 0; p_switch_indices_2++, ix-- )
			{
				if ( ( ( *p_switch_indices_2 =
						op_route_ntoh(&p_data_wire, sizeof(uint16_t)) ) >=
						num_switches ) && (*p_switch_indices_2 != 0xFFFF) )
				{
					rstatus = OP_ROUTE_STATUS_ERROR;
					goto cleanup;
				}
			}
		}

		// Check for minimum amount of received data
		if ((len_recv -= sizeof(uint16_t)) < 0)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Get cost matrix; comes in as 'top right' half
		num_switches_2 = op_route_ntoh(&p_data_wire, sizeof(uint16_t));

		if (num_switches_2 != num_switches)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		if ((len_data_alloc = num_switches * num_switches * sizeof(uint16_t)))
		{
			// Check amount of received data
			if ( ( len_recv -= ( (num_switches * (num_switches - 1)) *
					sizeof(uint16_t) / 2 ) ) < 0 )
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			if (!(p_cost_matrix_2 = calloc(1, len_data_alloc)))
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			for ( ix = num_switches * (num_switches - 1) / 2,
					ix_2 = num_switches - 1,
					p_cost_matrix_r = p_cost_matrix_2 + 1,
					p_cost_matrix_l = p_cost_matrix_2 + num_switches;
					ix > 0;
					p_cost_matrix_r += (num_switches + 1),
					p_cost_matrix_l += (num_switches + 1),
					ix_2-- )

				for ( ix_3 = ix_2,
						p_cost_matrix_r2 = p_cost_matrix_r,
						p_cost_matrix_l2 = p_cost_matrix_l;
						(ix_3 > 0) && (ix > 0);
						p_cost_matrix_r2++, p_cost_matrix_l2 += num_switches,
                        ix_3--, ix-- )
					*p_cost_matrix_r2 = *p_cost_matrix_l2 =
						(uint16_t)op_route_ntoh(&p_data_wire, sizeof(uint16_t));
		}

		// Return data to caller
		if (!(optn_query & OP_ROUTE_CREATE_JOB_NO_CREATE))
			*p_job_id = job_id;
		p_switch_map->num_switches = num_switches;
		p_switch_map->p_switch_indices = p_switch_indices;
		*pp_cost_matrix = p_cost_matrix_2;

		p_switch_indices = NULL;
		p_cost_matrix_2 = NULL;

		break;

	}  // End of case OP_ROUTE_AMOD_CREATE_JOB

	case OP_ROUTE_AMOD_SET_USE_MATRIX:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		break;

	}  // End of case OP_ROUTE_AMOD_SET_USE_MATRIX

	case OP_ROUTE_AMOD_POLL_READY:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Get job_status
		if ((len_recv -= 1) < 0)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		*p_job_status = (enum op_route_job_status) *p_data_wire;

		break;

	}  // End of case OP_ROUTE_AMOD_POLL_READY

	case OP_ROUTE_AMOD_COMPLETE_JOB:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		break;

	}  // End of case OP_ROUTE_AMOD_COMPLETE_JOB

	case OP_ROUTE_AMOD_GET_PORTGUID_VEC:
	{
		if (! p_guid_vec) 
		{
			rstatus = OP_ROUTE_STATUS_INVALID_PARAM;
			goto cleanup;
		}
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Check for minimum amount of received data
		if ((len_recv -= sizeof(uint16_t)) < 0)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Get number of port GUIDs
		num_guids = op_route_ntoh(&p_data_wire, sizeof(uint16_t));

		// Check for minimum amount of received data
		len_data_alloc = num_guids * sizeof(uint64_t);
		if ((len_recv -= len_data_alloc) < 0)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}
		
		if (len_data_alloc)
		{
			if (!(p_guids = calloc(1, len_data_alloc)))
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			// Get port GUIDs
			for (ix = num_guids, p_guids_2 = p_guids; ix > 0; ix--)
				*p_guids_2++ = op_route_ntoh(&p_data_wire, sizeof(uint64_t));
		}

		// Return data to caller
		p_guid_vec->num_guids = num_guids;
		p_guid_vec->p_guids = p_guids;

		break;

	}  // End of case OP_ROUTE_AMOD_GET_PORTGUID_VEC

	case OP_ROUTE_AMOD_GET_SWITCH_MAP:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Check for minimum amount of received data
		if ((len_recv -= (sizeof(uint16_t) + sizeof(uint16_t))) < 0)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Get num_guids and num_switches
		num_guids = op_route_ntoh(&p_data_wire, sizeof(uint16_t));
		num_switches = op_route_ntoh(&p_data_wire, sizeof(uint16_t));

		if ( (num_switches > OP_ROUTE_MAX_NUM_SWITCHES) ||
				(num_switches > num_guids) )
		{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
		}

		// Check for minimum amount of received data
		len_data_alloc = num_guids * sizeof(uint16_t);
		if ((len_recv -= len_data_alloc ) < 0)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		if (len_data_alloc)
		{
			if (!(p_switch_indices = calloc(1, len_data_alloc)))
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			// Get switch map
			for ( ix = num_guids, p_switch_indices_2 = p_switch_indices;
					ix > 0; p_switch_indices_2++, ix-- )
			{
				if ( ( ( *p_switch_indices_2 =
						op_route_ntoh(&p_data_wire, sizeof(uint16_t)) ) >=
						num_switches ) && (*p_switch_indices_2 != 0xFFFF) )
				{
					rstatus = OP_ROUTE_STATUS_ERROR;
					goto cleanup;
				}
			}
		}

		// Return data to caller
		p_switch_map->num_switches = num_switches;
		p_switch_map->p_switch_indices = p_switch_indices;
		p_switch_indices = NULL;

		break;

	}  // End of case OP_ROUTE_AMOD_GET_SWITCH_MAP

	case OP_ROUTE_AMOD_GET_COST_MATRIX:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Check for minimum amount of received data
		if ((len_recv -= sizeof(uint16_t)) < 0 )
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		num_switches = op_route_ntoh(&p_data_wire, sizeof(uint16_t));
		if ( ( len_recv -= ( (num_switches * (num_switches - 1)) *
				sizeof(uint16_t) / 2 ) ) < 0 )
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		len_data_alloc = num_switches * num_switches * sizeof(uint16_t);
		if (len_data_alloc)
		{
			if (!(p_cost_matrix_2 = calloc(1, len_data_alloc)))
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			// Get cost_matrix
			for ( ix = num_switches * (num_switches - 1) / 2,
					ix_2 = num_switches - 1,
					p_cost_matrix_r = p_cost_matrix_2 + 1,
					p_cost_matrix_l = p_cost_matrix_2 + num_switches;
					ix > 0;
					p_cost_matrix_r += (num_switches + 1),
					p_cost_matrix_l += (num_switches + 1),
					ix_2-- )

				for ( ix_3 = ix_2,
						p_cost_matrix_r2 = p_cost_matrix_r,
						p_cost_matrix_l2 = p_cost_matrix_l;
						(ix_3 > 0) && (ix > 0);
						p_cost_matrix_r2++, p_cost_matrix_l2 += num_switches,
                        ix_3--, ix-- )
					*p_cost_matrix_r2 = *p_cost_matrix_l2 =
						(uint16_t)op_route_ntoh(&p_data_wire, sizeof(uint16_t));
		}

		// Return data to caller
		*pp_cost_matrix = p_cost_matrix_2;
		p_cost_matrix_2 = NULL;

		break;

	}  // End of case OP_ROUTE_AMOD_GET_COST_MATRIX

	case OP_ROUTE_AMOD_GET_USE_MATRIX:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Check for minimum amount of received data
		if ( ( len_recv -= ( sizeof(uint16_t) + sizeof(uint8_t) +
				sizeof(uint16_t) ) ) < 0 )
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Get multiplier, default use, and number of use elements
		use_multiplier = op_route_ntoh(&p_data_wire, sizeof(uint16_t));
		default_use = *p_data_wire++;
		num_elements = op_route_ntoh(&p_data_wire, sizeof(uint16_t));

		// Check for minimum amount of received data
		if ( ( len_recv -= ( num_elements * ( sizeof(uint16_t) +
				sizeof(uint16_t) + sizeof(uint8_t) ) ) ) < 0 )
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}
		
		len_data_alloc = num_elements * sizeof(struct op_route_use_element);
		if (len_data_alloc)
		{
			if (!(p_use_elements = calloc(1, len_data_alloc)))
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			// Get use elements
			for ( ix = num_elements, p_use_elements_2 = p_use_elements; ix > 0;
					p_use_elements_2++, ix-- )
			{
				temp = op_route_ntoh(&p_data_wire, sizeof(uint16_t));
				p_use_elements_2->bursty = (temp >> 15) & 0x0001;
				p_use_elements_2->switch_index = temp & 0x7FFF;
				p_use_elements_2->dlid =
					op_route_ntoh(&p_data_wire, sizeof(uint16_t));
				p_use_elements_2->use = *p_data_wire++;
			}
		}

		// Return data to caller
		p_use_matrix->default_use = default_use;
		p_use_matrix->multiplier = use_multiplier;
		p_use_matrix->num_elements = num_elements;
		p_use_matrix->p_elements = p_use_elements;

		break;

	}  // End of case OP_ROUTE_AMOD_GET_USE_MATRIX

	case OP_ROUTE_AMOD_GET_JOBS:
	{
		// Check wire status
		if (wstatus == OP_ROUTE_WIRE_STATUS_OK_PARTIAL)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Check for minimum amount of received data
		if ((len_recv -= sizeof(uint16_t)) < 0)
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}

		// Get number of jobs
		num_jobs = op_route_ntoh(&p_data_wire, sizeof(uint16_t));

		// Check for minimum amount of received data
		if ( ( len_recv -= ( num_jobs * ( sizeof(OP_ROUTE_JOB_ID) +
				sizeof(uint64_t) + sizeof(uint16_t) +
				OP_ROUTE_MAX_JOB_NAME_LEN + OP_ROUTE_MAX_APP_NAME_LEN +
				sizeof(uint64_t) + sizeof(uint64_t) ) ) ) < 0 )
		{
			rstatus = OP_ROUTE_STATUS_ERROR;
			goto cleanup;
		}
		
		if ((len_data_alloc = num_jobs * sizeof(struct op_route_job_info)))
		{
			if (!(p_job_info = calloc(1, len_data_alloc)))
			{
				rstatus = OP_ROUTE_STATUS_ERROR;
				goto cleanup;
			}

			// Get list of job info
			for ( ix = num_jobs, p_job_info_2 = p_job_info; ix > 0;
					p_job_info_2++, ix-- )
			{
				p_job_info_2->job_id =
					op_route_ntoh(&p_data_wire, sizeof(OP_ROUTE_JOB_ID));
				temp = op_route_ntoh(&p_data_wire, sizeof(uint64_t));
				p_job_info_2->time_stamp = temp;
				temp = op_route_ntoh(&p_data_wire, sizeof(uint16_t));
				p_job_info_2->routed = (temp >> 1) & 0x0001;
				p_job_info_2->has_use = temp & 0x0001;
				strncpy( p_job_info_2->params.name, (char *)p_data_wire,
					OP_ROUTE_MAX_JOB_NAME_LEN );
				p_data_wire += OP_ROUTE_MAX_JOB_NAME_LEN;
				strncpy( p_job_info_2->params.application_name, (char *)p_data_wire,
					OP_ROUTE_MAX_APP_NAME_LEN );
				p_data_wire += OP_ROUTE_MAX_APP_NAME_LEN;
				p_job_info_2->params.pid =
					op_route_ntoh(&p_data_wire, sizeof(uint64_t));
				p_job_info_2->params.uid =
					op_route_ntoh(&p_data_wire, sizeof(uint64_t));
			}
		}

		// Return data to caller
		p_job_list->num_jobs = num_jobs;
		p_job_list->p_job_info = p_job_info;

		break;

	}  // End of case OP_ROUTE_AMOD_GET_JOBS

	// Invalid attribute modifier
	default:
		printf("op_route_send_recv_query: invalid AMOD:%d\n", attr_mod);
			rstatus = OP_ROUTE_STATUS_ERROR;

	}  // End of switch (attr_mod)

cleanup:
	// Deallocate local buffers if necessary
	if (p_mad_send)
		free(p_mad_send);
	if (p_mad_recv)
		free(p_mad_recv);
	if (p_switch_indices)
		free(p_switch_indices);
	if (p_cost_matrix_2)
		free(p_cost_matrix_2);

	return (rstatus);

}  // End of op_route_send_recv_query()

/*******************************************************************************
 *
 * op_route_dump_job_info()
 *
 * Description: dump job_info structure to console.
 *
 * Inputs:
 *      p_title - Pointer to dump title string
 *     n_indent - Number of spaces to indent
 *   p_job_info - Pointer to job_info
 *
 * Outputs:
 *   job_info data
 */
static void op_route_dump_job_info( char * p_title,
    int n_indent,
    struct op_route_job_info * p_job_info )
{
	if (p_job_info)
	{
		printf( "%*sjob_info(%s): job_id:0x%"PRIX64"\n", n_indent, "", p_title,
			p_job_info->job_id );
		printf( "%*s    time_stamp:%"PRIu64" %s", n_indent, "",
			(uint64_t)p_job_info->time_stamp, ctime(&p_job_info->time_stamp) );
		printf( "%*s    route:%u use:%u\n", n_indent, "", p_job_info->routed,
			p_job_info->has_use );
		printf( "%*s    name:(%s) app:(%s)\n", n_indent, "",
			p_job_info->params.name, p_job_info->params.application_name );
		printf( "%*s    pid:0x%016"PRIX64" uid:0x%016"PRIX64"\n", n_indent, "",
			p_job_info->params.pid, p_job_info->params.uid );
	}

}  // End of op_route_dump_job_info()

/*******************************************************************************
 *
 * op_route_dump_job_list()
 *
 * Description: dump job_list to console.
 *
 * Inputs:
 *      p_title - Pointer to dump title string
 *     n_indent - Number of spaces to indent
 *   p_job_list - Pointer to job_list
 *
 * Outputs:
 *   job_list data
 */
static void op_route_dump_job_list( char * p_title,
    int n_indent,
    struct op_route_job_list * p_job_list )
{
	int ix;
	struct op_route_job_info * p_job_info;
	char bf_title2[81];

	if (p_job_list)
	{
		printf( "%*sjob list(%s): num_jobs: %u\n", n_indent, "", p_title,
			p_job_list->num_jobs );

		for ( ix = 0, p_job_info = p_job_list->p_job_info;
				(ix < p_job_list->num_jobs) && p_job_info;
				p_job_info++, ix++ )
		{
			sprintf(bf_title2, "%d", ix);
			op_route_dump_job_info(bf_title2, n_indent+2, p_job_info);
		}
	}

}  // End of op_route_dump_job_list()


/*******************************************************************************
 *
 * GLOBAL FUNCTIONS
 */

/*******************************************************************************
 *
 * op_route_dump()
 *
 * Description: top-level dump function for op_route data structures to console.
 *   This function calls specific dump functions needed based on calling
 *   parameters.
 *
 * Inputs:
 *           p_title - Pointer to dump title string
 *   fb_port_handles - Flag to dump port handles table
 *     p_port_handle - Pointer to OP_ROUTE_PORT_HANDLE
 *          p_job_id - Pointer to OP_ROUTE_JOB_ID
 *      p_job_params - Pointer to job parameters
 *        p_guid_vec - Pointer to portguid_vec
 *      p_switch_map - Pointer to switch_map
 *     p_cost_matrix - Pointer to cost_matrix
 *      p_use_matrix - Pointer to use_matrix
 *        p_job_info - Pointer to job_info
 *        p_job_list - Pointer to job_list
 *
 * Outputs:
 *   job information
 */
void op_route_dump( char * p_title,
    int fb_port_handles,
    OP_ROUTE_PORT_HANDLE * p_port_handle,
    OP_ROUTE_JOB_ID * p_job_id,
    struct op_route_job_parameters * p_job_params,
    struct op_route_portguid_vec * p_guid_vec,
    struct op_route_switch_map * p_switch_map,
    uint16_t * p_cost_matrix,
    struct op_route_use_matrix * p_use_matrix,
    struct op_route_job_info * p_job_info,
	struct op_route_job_list * p_job_list )
{
	int ix, ix_2, ix_3;
	int n_indent = 2;
	uint16_t num_switches;
	struct param_alloc_port_handle_entry * p_param_entry;

	printf("op_route_dump (%s):\n", p_title);

	if (fb_port_handles)
	{
		printf( "%*sport handles: alloc:%u inuse: %u\n", n_indent, "",
			param_port_handle.num_allocated, param_port_handle.num_used );
		for ( ix = 0, p_param_entry = (struct param_alloc_port_handle_entry *)
				param_port_handle.p_params;
				ix < param_port_handle.num_allocated; p_param_entry++, ix++ )
			printf( "%*s%d: port_h:0x%"PRIX64" GUID:0x%"PRIX64
				" port_id:%d\n", n_indent+2, "", ix, p_param_entry->port_handle,
				p_param_entry->port_guid, p_param_entry->port_id );
	}

	printf("%*sp_porthandle:0x%"PRIX64, n_indent, "", (uint64_t)p_port_handle);
	if (p_port_handle)
		printf("  porthandle:0x%"PRIX64, (uint64_t)*p_port_handle);
	printf("\n");

	if (p_job_id)
		printf("%*sjob_id:0x%"PRIX64"\n", n_indent, "", (uint64_t)*p_job_id);

	if (p_job_params)
	{
			printf( "%*sjob_parameters: name:(%s) app:(%s)\n", n_indent, "",
				p_job_params->name, p_job_params->application_name );
			printf( "%*spid:0x%016"PRIX64" uid:0x%016"PRIX64"\n",
				n_indent+2, "", p_job_params->pid, p_job_params->uid );
	}

	if (p_guid_vec)
	{
		printf("%*sguid_vec: num_guids:%u\n", n_indent, "", p_guid_vec->num_guids);
		for (ix = 0; (ix < p_guid_vec->num_guids) && p_guid_vec->p_guids; ix++)
			printf( "%*s%5d: 0x%016"PRIX64"\n", n_indent+2, "", ix,
				p_guid_vec->p_guids[ix] );
	}

	if (p_switch_map && p_guid_vec)
	{
		printf( "%*sswitch_map: num_switches:%u (num_guids:%u)\n", n_indent, "",
			p_switch_map->num_switches, p_guid_vec->num_guids );
		for ( ix = 0; (ix < p_guid_vec->num_guids) &&
				p_switch_map->p_switch_indices; ix++ )
			printf( "%*s%5d: %5u\n", n_indent+2, "", ix,
				p_switch_map->p_switch_indices[ix] );
	}

	if ( p_cost_matrix && p_switch_map &&
			((num_switches = p_switch_map->num_switches)) )
	{
		printf( "%*scost_matrix: (num_switches:%u)\n", n_indent, "",
			num_switches );
		for (ix = 0; ix < num_switches; ix++)
			printf("%*s%4d", ix ? 1 : n_indent+6, "", ix);
		printf("\n");

		for (ix = 0, ix_2 = 0; ix < num_switches; ix++)
		{
			printf("%*s%4d:", n_indent, "", ix);
			for (ix_3 = 0; ix_3 < num_switches; ix_3++)
				printf(" %04X", p_cost_matrix[ix_2++]);
			printf("\n");
		}
	}

	if (p_use_matrix)
	{
		printf( "%*suse_matrix: num_elements:%u default_use:%u multiplier:%u\n",
			n_indent, "", p_use_matrix->num_elements, p_use_matrix->default_use,
			p_use_matrix->multiplier );
		for ( ix = 0; (ix < p_use_matrix->num_elements) &&
				p_use_matrix->p_elements; ix++ )
			printf( "%*s%d: sw_index:%5d dlid:0x%04X use:%u bursty:%u\n",
				n_indent+2, "", ix, p_use_matrix->p_elements[ix].switch_index,
				p_use_matrix->p_elements[ix].dlid,
				p_use_matrix->p_elements[ix].use,
				p_use_matrix->p_elements[ix].bursty );
	}

	op_route_dump_job_info(p_title, n_indent, p_job_info);
	op_route_dump_job_list(p_title, n_indent, p_job_list);


}  // End of op_route_dump()

/*******************************************************************************
 *
 * op_route_init_param()
 *
 * Description:
 *	 Initialize the specified parameter structure.
 *
 * Inputs:
 *      p_param - Pointer to parameter structure
 *   size_param - Size of parameter entry
 *    num_alloc - Num of params in allocation unit
 *
 * Outputs:
 *    0 - Initialization successful
 *   -1 - Initialization unsuccessful
 */
int op_route_init_param( struct op_route_param_alloc * p_param,
    uint32_t size_param, uint32_t num_alloc )
{
	void * p_alloc;

	if (!p_param || !size_param || !num_alloc)
		return (-1);

	if ((p_alloc = calloc(num_alloc, size_param)))
	{
		p_param->size_param = size_param;
		p_param->num_allocated = num_alloc;
		p_param->num_used = 0;
		p_param->num_alloc = num_alloc;
		p_param->p_params = p_alloc;
	}

	else
		return (-1);

	return (0);

}  // End of op_route_init_param()

/*******************************************************************************
 *
 * op_route_free_param()
 *
 * Description:
 *	 Free parameters for the specified parameter structure.
 *
 * Inputs:
 *      p_param - Pointer to parameter structure
 *
 * Outputs:
 *    none
 */
void op_route_free_param(struct op_route_param_alloc * p_param)
{
	if (p_param && p_param->p_params)
	{
		free(p_param->p_params);
		p_param->num_allocated = 0;
		p_param->num_used = 0;
		p_param->p_params = NULL;
	}

}  // End of op_route_free_param()

/*******************************************************************************
 *
 * op_route_alloc_param()
 *
 * Description:
 *	 Allocate a block of parameters according to specified parameter pointer.
 *
 * Inputs:
 *   p_param - Pointer to parameter structure
 *
 * Outputs:
 *    0 - Allocation successful
 *   -1 - Allocation unsuccessful
 */
int op_route_alloc_param(struct op_route_param_alloc * p_param)
{
	char * p_alloc;

	if (!p_param)
		return (-1);

	if ( ( p_alloc = realloc( p_param->p_params, p_param->size_param *
			(p_param->num_allocated + p_param->num_alloc) ) ) )
	{
		memset( p_alloc + (p_param->size_param * p_param->num_allocated), 0,
			p_param->size_param * p_param->num_alloc );
		p_param->p_params = (struct op_route_param_alloc_entry *)p_alloc;
		p_param->num_allocated += p_param->num_alloc;
	}

	else
		return (-1);

	return (0);

}  // End of op_route_alloc_param()

/*******************************************************************************
 *
 * op_route_dump_param()
 *
 * Description: dump information, as supplied by calling parameters, to console.
 *
 * Inputs:
 *    p_title - Pointer to dump title string
 *    p_param - Pointer to parameter structure
 *
 * Outputs:
 *    Allocated parameter information
 */
void op_route_dump_param( char * p_title,
    struct op_route_param_alloc * p_param )
{
	int ix;
	struct op_route_param_alloc_entry * p_param_entry;

	printf("op_route_dump_param (%s):\n", p_title);

	if (p_param)
	{
		printf( "  sz_param:%"PRIu64" allocd:%u used:%u alloc:%u p_params:0x%"PRIX64"\n",
            p_param->size_param, p_param->num_allocated, p_param->num_used,
			p_param->num_alloc, (uint64_t)p_param->p_params );

		for (ix = 0; ix < p_param->num_allocated; ix++)
		{
			p_param_entry = (struct op_route_param_alloc_entry *)
				((char *)p_param->p_params + (p_param->size_param * ix));
			printf( "  %4d: sz_param:%"PRIu64" sz_data:%"PRIu64" info_param:%"
				PRIu64" data64:0x%016"PRIX64"\n",
				ix, p_param_entry->size_param, p_param_entry->size_data,
				p_param_entry->info_param,
				*(uint64_t *)p_param_entry->bf_data );
		}
	}

}  // End of op_route_dump_param()

/*******************************************************************************
 *
 * op_route_get_status_text()
 *
 * Description:
 *   Get a text string corresponding to the specified op_route_status value.
 *
 * Inputs:
 *   status - Status value
 *
 * Outputs:
 *   status string - Valid status value
 *            NULL - Invalid status value
 */
extern char * op_route_get_status_text(enum op_route_status status)
{
	switch(status)
	{
	case OP_ROUTE_STATUS_OK:
		return("No Error");

	case OP_ROUTE_STATUS_OK_PARTIAL:
		return("Partial Success");

	case OP_ROUTE_STATUS_ERROR:
		return("General Error");

	case OP_ROUTE_STATUS_SEND_ERROR:
		return("Send Error");

	case OP_ROUTE_STATUS_RECV_ERROR:
		return("Receive Error");

	case OP_ROUTE_STATUS_TIMEOUT:
		return("Response Timeout");

	case OP_ROUTE_STATUS_INVALID_JOB:
		return("Invalid Job ID");

	case OP_ROUTE_STATUS_INVALID_PARAM:
		return("Invalid Parameter");

	default:
		return(NULL);
	}  // End of switch(status)

}  // End of op_route_get_status_text()

/*******************************************************************************
 *
 * op_route_open()
 *
 * Description:
 *   Open a routing connection by registering with SD.  Each successful open
 *   creates an OP_ROUTE_PORT_HANDLE, which is entered in the port table,
 *   and passed back to the caller.
 *
 * Inputs:
 *            port - Pointer to opamgt handler
 *       port_guid - GUID of port on which to register
 *   p_port_handle - Pointer to OP_ROUTE_PORT_HANDLE
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Open successful
 *                                   *p_port_handle = OP_ROUTE_PORT_HANDLE
 *   OP_ROUTE_STATUS_INVALID_PARAM - p_port_handle NULL
 *           OP_ROUTE_STATUS_ERROR - port_guid already registered
 *                                 - Unable to allocate memory
 *                                 - Register error
 */
enum op_route_status op_route_open( struct omgt_port * port,
									uint64_t port_guid,
									OP_ROUTE_PORT_HANDLE * p_port_handle )
{
	uint32_t ix, ix_2;
	int fb_break;
	struct param_alloc_port_handle_entry * p_param_entry, * p_param_entry_2;

	if (!p_port_handle)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Check for port_guid already in port table
	if (!param_port_handle.num_allocated)
		if ( op_route_init_param( &param_port_handle,
				sizeof(struct param_alloc_port_handle_entry),
				NUM_PARAM_PORT_HANDLE_ALLOC ) )
			return (OP_ROUTE_STATUS_ERROR);

	for ( ix = param_port_handle.num_allocated, p_param_entry =
			(struct param_alloc_port_handle_entry *)param_port_handle.p_params;
			ix > 0; p_param_entry++, ix-- )
		if (p_param_entry->port_guid == port_guid)
			return (OP_ROUTE_STATUS_ERROR);

	// Find first available port table entry and register
	if (param_port_handle.num_used == param_port_handle.num_allocated)
		if (op_route_alloc_param(&param_port_handle))
			return (OP_ROUTE_STATUS_ERROR);

	for ( ix = param_port_handle.num_allocated, p_param_entry =
			(struct param_alloc_port_handle_entry *)param_port_handle.p_params;
			ix > 0; p_param_entry++, ix-- )
	{
		if (!p_param_entry->port_handle)
		{
			// Get port_handle and check for duplicate
			for (fb_break = FALSE; !fb_break; )
			{
				p_param_entry->port_handle = op_route_gen_port_handle();

				for ( ix_2 = param_port_handle.num_allocated, p_param_entry_2 =
				(struct param_alloc_port_handle_entry *)param_port_handle.p_params;
							; p_param_entry_2++ )
				{
					if ( (ix_2 != ix) && ( p_param_entry->port_handle ==
										   p_param_entry_2->port_handle ) )
						break;	// Get another port_handle

					else if (--ix_2 == 0)
					{
						fb_break = TRUE;
						break;
					}
				}
			}

			p_param_entry->port_guid = port_guid;
			*p_port_handle = p_param_entry->port_handle;

			// Get ClassPortInfo
			if ( op_route_send_recv_query( OP_ROUTE_AMOD_CLASS_INFO,
						*p_port_handle, 0, NULL, NULL, NULL, NULL, NULL, NULL,
						NULL, NULL, port ) == OP_ROUTE_STATUS_OK )
			{
				param_port_handle.num_used++;
				return (OP_ROUTE_STATUS_OK);
			}

			p_param_entry->port_handle = 0;
			p_param_entry->port_guid = 0;
			p_param_entry->port_id = 0;

			break;

		}  // End of if (!!p_param_entry->port_handle)

	}  // End of for ( ix = param_port_handle.num_allocated; ix > 0;

	return (OP_ROUTE_STATUS_ERROR);

}  // End of op_route_open()

/*******************************************************************************
 *
 * op_route_close()
 *
 * Description:
 *   Close a routing connection by deregistering with SD.  Remove the
 *   specified OP_ROUTE_PORT_HANDLE from the port table.
 *
 * Inputs:
 *   port_handle - OP_ROUTE_PORT_HANDLE
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Close successful
 *   OP_ROUTE_STATUS_INVALID_PARAM - Invalid OP_ROUTE_PORT_HANDLE
 */
enum op_route_status op_route_close(OP_ROUTE_PORT_HANDLE port_handle)
{
	struct param_alloc_port_handle_entry * p_param_entry;

	if ((p_param_entry = op_route_get_port_handle_entry(port_handle)))
	{
		p_param_entry->port_handle = 0;
		p_param_entry->port_guid = 0;
		p_param_entry->port_id = 0;
		param_port_handle.num_used--;

		return (OP_ROUTE_STATUS_OK);
	}

	return (OP_ROUTE_STATUS_INVALID_PARAM);

}  // End of op_route_close()

/*******************************************************************************
 *
 * op_route_create_job()
 *
 * Description:
 *   Create a job on the specified port_handle, using the specified name and
 *   port GUID vector.  Upon job creation, supply a job ID, switch map and
 *   cost matrix.
 *
 * Inputs:
 *      port_handle - OP_ROUTE_PORT_HANDLE
 *      optn_create - Options for create job (see OP_ROUTE_CREATE_JOB_xx)
 *     p_job_params - Pointer to job parameters
 *  	 p_guid_vec - Pointer to portguid_vec
 *             port - Pointer to opamgt handle 
 *         p_job_id - Pointer to OP_ROUTE_JOB_ID
 *     p_switch_map - Pointer to switch_map
 *   pp_cost_matrix - Pointer to pointer to cost_matrix
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Create successful
 *                                   *p_switch_map->p_switch_indices =
 *                                     created switch_map
 *                                   **pp_cost_matrix = created cost_matrix
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle NULL
 *                                 - p_job_params NULL
 *                                 - p_guid_vec NULL
 *                                 - p_job_id NULL
 *                                 - p_switch_map NULL
 *                                 - pp_cost_matrix NULL
 *   TBD restore next if OP_ROUTE_MIN_NUM_GUIDS raised above 0
 *                              (  - guid_vec too small  )
 *           OP_ROUTE_STATUS_ERROR - Error during create
 */
enum op_route_status op_route_create_job( OP_ROUTE_PORT_HANDLE port_handle,
    uint16_t optn_create,
    struct op_route_job_parameters * p_job_params,
    struct op_route_portguid_vec * p_guid_vec,
	struct omgt_port * port,
    OP_ROUTE_JOB_ID * p_job_id,                 // output
    struct op_route_switch_map * p_switch_map,  // output
    uint16_t ** pp_cost_matrix )                // output
{
	enum op_route_status rstatus;

	if ( !port_handle || !p_job_params || !p_guid_vec || ( !p_job_id &&
			!(optn_create & OP_ROUTE_CREATE_JOB_NO_CREATE) ) ||
			!p_switch_map || !pp_cost_matrix
			/* TBD restore if #define changed || (p_guid_vec->num_guids < OP_ROUTE_MIN_NUM_GUIDS) */ )
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Query CreateJob record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_CREATE_JOB, port_handle,
		optn_create, p_job_id, NULL, p_job_params, p_guid_vec, p_switch_map,
		pp_cost_matrix, NULL, NULL, port );

	return (rstatus);

}  // End of op_route_create_job()

/*******************************************************************************
 *
 * op_route_complete_job()
 *
 * Description:
 *   Complete the specified job on the specified port_handle.
 *
 * Inputs:
 *   port_handle - OP_ROUTE_PORT_HANDLE
 *  	p_job_id - Pointer to OP_ROUTE_JOB_ID
 *  	    port - Pointer to opamgt port handler
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Completion successful
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle NULL
 *           OP_ROUTE_STATUS_ERROR - Error during completion
 */
enum op_route_status op_route_complete_job( OP_ROUTE_PORT_HANDLE port_handle,
    OP_ROUTE_JOB_ID job_id,
	struct omgt_port * port  )
{
	enum op_route_status rstatus;

	if (!port_handle)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Query CompleteJob record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_COMPLETE_JOB, port_handle,
		0, &job_id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, port );

	return (rstatus);

}  // End of op_route_complete_job()

/*******************************************************************************
 *
 * op_route_get_portguid_vec()
 *
 * Description:
 *   Get port GUID vector for the specified job on the specified port_handle.
 *
 * Inputs:
 *      port_handle - OP_ROUTE_PORT_HANDLE
 *           job_id - OP_ROUTE_JOB_ID
 *             port - Pointer to opamgt handle
 *   p_portguid_vec - Pointer to portguid_vec
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Get successful
 *                                   *p_portguid_vec = portguid_vec
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle NULL
 *                                 - p_guid_vec NULL
 *           OP_ROUTE_STATUS_ERROR - Error during get
 */
enum op_route_status op_route_get_portguid_vec( OP_ROUTE_PORT_HANDLE port_handle,
    OP_ROUTE_JOB_ID job_id,
	struct omgt_port * port,
    struct op_route_portguid_vec * p_guid_vec )     // output
{
	enum op_route_status rstatus;

	if (!port_handle || !p_guid_vec)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Query PortGUID vector record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_GET_PORTGUID_VEC,
		port_handle, 0, &job_id, NULL, NULL, p_guid_vec, NULL, NULL, NULL,
		NULL, port );

	return (rstatus);

}  // End of op_route_get_portguid_vec()

/*******************************************************************************
 *
 * op_route_release_portguid_vec()
 *
 * Description:
 *   Release the specified portguid_vec.
 *
 * Inputs:
 *   p_guid_vec - Pointer to portguid_vec
 *
 * Outputs:
 *   none
 */
void op_route_release_portguid_vec(struct op_route_portguid_vec * p_guid_vec)
{
	if (p_guid_vec && p_guid_vec->p_guids)
	{
		free(p_guid_vec->p_guids);
		p_guid_vec->p_guids = NULL;
	}

}  // End of op_route_release_portguid_vec()

/*******************************************************************************
 *
 * op_route_get_switch_map()
 *
 * Description:
 *   Get switch_map for the specified job on the specified port_handle.
 *
 * Inputs:
 *    port_handle - OP_ROUTE_PORT_HANDLE
 *  	   job_id - OP_ROUTE_JOB_ID
 *  	     port - Pointer to opamgt handle
 *   p_switch_map - Pointer to switch_map
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Get successful
 *                                   *p_switch_map = switch_map
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle NULL
 *                                 - p_switch_map NULL
 *           OP_ROUTE_STATUS_ERROR - Error during get
 */
enum op_route_status op_route_get_switch_map( OP_ROUTE_PORT_HANDLE port_handle,
    OP_ROUTE_JOB_ID job_id,
	struct omgt_port * port,
    struct op_route_switch_map * p_switch_map )  // output
{
	enum op_route_status rstatus;

	if (!port_handle || !p_switch_map)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Query switch_map record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_GET_SWITCH_MAP,
		port_handle, 0, &job_id, NULL, NULL, NULL, p_switch_map, NULL, NULL,
		NULL, port );

	return (rstatus);

}  // End of op_route_get_switch_map()

/*******************************************************************************
 *
 * op_route_release_switch_map()
 *
 * Description:
 *   Release the specified switch_map.
 *
 * Inputs:
 *   p_op_route_switch_map - Pointer to switch_map
 *
 * Outputs:
 *   none
 */
void op_route_release_switch_map(struct op_route_switch_map * p_switch_map)
{
	if (p_switch_map && p_switch_map->p_switch_indices)
	{
		free(p_switch_map->p_switch_indices);
		p_switch_map->p_switch_indices = NULL;
	}

}  // End of op_route_release_switch_map()

/*******************************************************************************
 *
 * op_route_get_cost_matrix()
 *
 * Description:
 *   Get cost_matrix for the specified job on the specified port_handle.
 *
 * Inputs:
 *      port_handle - OP_ROUTE_PORT_HANDLE
 *  		 job_id - OP_ROUTE_JOB_ID
 *  		   port - Pointer to opamgt handle
 *   pp_cost_matrix - Pointer to pointer to cost_matrix
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Get successful
 *                                   **pp_cost_matrix = cost_matrix
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle NULL
 *                                 - pp_cost_matrix NULL
 *           OP_ROUTE_STATUS_ERROR - Error during get
 */
enum op_route_status op_route_get_cost_matrix( OP_ROUTE_PORT_HANDLE port_handle,
    OP_ROUTE_JOB_ID job_id,
	struct omgt_port * port,
    uint16_t ** pp_cost_matrix )    // output
{
	enum op_route_status rstatus;

	if (!port_handle || !pp_cost_matrix)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Query cost_matrix record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_GET_COST_MATRIX,
		port_handle, 0, &job_id, NULL, NULL, NULL, NULL, pp_cost_matrix, NULL,
		NULL, port );

	return (rstatus);

}  // End of op_route_get_cost_matrix()

/*******************************************************************************
 *
 * op_route_release_cost_matrix()
 *
 * Description:
 *   Release the specified cost_matrix.
 *
 * Inputs:
 *   p_cost_matrix - Pointer to cost_matrix
 *
 * Outputs:
 *   none
 */
void op_route_release_cost_matrix(uint16_t * p_cost_matrix)
{
	if (p_cost_matrix)
		free(p_cost_matrix);

}  // End of op_route_release_cost_matrix()

/*******************************************************************************
 *
 * op_route_get_use_matrix()
 *
 * Description:
 *   Get use_matrix for the specified job on the specified port_handle.
 *
 * Inputs:
 *    port_handle - OP_ROUTE_PORT_HANDLE
 *  	   job_id - OP_ROUTE_JOB_ID
 *  	     port - Pointer to opamgt handle
 *   p_use_matrix - Pointer to use_matrix
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Get successful
 *                                   *p_use_matrix = use_matrix
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle NULL
 *                                 - p_use_matrix NULL
 *           OP_ROUTE_STATUS_ERROR - Error during get
 */
enum op_route_status op_route_get_use_matrix( OP_ROUTE_PORT_HANDLE port_handle,
    OP_ROUTE_JOB_ID job_id,
	struct omgt_port * port,
    struct op_route_use_matrix * p_use_matrix )
{
	enum op_route_status rstatus;

	if (!port_handle || !p_use_matrix)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Query use_matrix record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_GET_USE_MATRIX,
		port_handle, 0, &job_id, NULL, NULL, NULL, NULL, NULL, p_use_matrix,
		NULL, port );

	return (rstatus);

}  // End of op_route_get_use_matrix()

/*******************************************************************************
 *
 * op_route_set_use_matrix()
 *
 * Description:
 *   Set use_matrix for the specified job on the specified port_handle.
 *
 * Inputs:
 *    port_handle - OP_ROUTE_PORT_HANDLE
 *  	   job_id - OP_ROUTE_JOB_ID
 *  	     port - Pointer to opamgt handle
 *   p_use_matrix - Pointer to use_matrix
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Set successful
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle NULL
 *                                 - p_use_matrix NULL
 *           OP_ROUTE_STATUS_ERROR - Error during set
 */
enum op_route_status op_route_set_use_matrix( OP_ROUTE_PORT_HANDLE port_handle,
    OP_ROUTE_JOB_ID job_id,
	struct omgt_port * port,
    struct op_route_use_matrix * p_use_matrix )
{
	enum op_route_status rstatus;

	if (!port_handle || !p_use_matrix)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Send set use_matrix record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_SET_USE_MATRIX,
		port_handle, 0, &job_id, NULL, NULL, NULL, NULL, NULL, p_use_matrix,
		NULL, port );

	return (rstatus);

}  // End of op_route_set_use_matrix()

/*******************************************************************************
 *
 * op_route_release_use_matrix()
 *
 * Description:
 *   Release the specified use_matrix.
 *
 * Inputs:
 *   p_use_matrix - Pointer to use_matrix
 *
 * Outputs:
 *   none
 */
void op_route_release_use_matrix(struct op_route_use_matrix * p_use_matrix)
{
	if (p_use_matrix && p_use_matrix->p_elements)
	{
		free(p_use_matrix->p_elements);
		p_use_matrix->p_elements = NULL;
	}

}  // End of op_route_release_use_matrix()

/*******************************************************************************
 *
 * op_route_get_job_list()
 *
 * Description:
 *   Get a list of created jobs on the specified port_handle.
 *
 * Inputs:
 *   port_handle - OP_ROUTE_PORT_HANDLE
 *          port - Pointer to opamgt handle
 *    p_job_list - Pointer to job list (array of job_info)
 *
 * Outputs:
 *      OP_ROUTE_STATUS_OK - Get successful
 *                           *p_job_list = job_list
 *   OP_ROUTE_STATUS_ERROR - Error during get
 */
enum op_route_status op_route_get_job_list( OP_ROUTE_PORT_HANDLE port_handle,
	struct omgt_port * port,
    struct op_route_job_list * p_job_list)   // output
{
	enum op_route_status rstatus;

	if (!port_handle || !p_job_list)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Query JobList record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_GET_JOBS, port_handle,
		0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, p_job_list, port );

	return (rstatus);

}  // End of op_route_get_job_list()

/*******************************************************************************
 *
 * op_route_release_job_list()
 *
 * Description:
 *   Release the specified job_list.
 *
 * Inputs:
 *   p_job_list - Pointer to job_list
 *
 * Outputs:
 *   none
 */
void op_route_release_job_list(struct op_route_job_list * p_job_list)
{
	if (p_job_list && p_job_list->p_job_info)
	{
		free(p_job_list->p_job_info);
		p_job_list->p_job_info = NULL;
	}

}  // End of op_route_release_job_list()

/*******************************************************************************
 *
 * op_route_poll_ready()
 *
 * Description:
 *   Poll for the status of the specified job on the specified port_handle.
 *
 * Inputs:
 *   port_handle - OP_ROUTE_PORT_HANDLE
 *  	  job_id - OP_ROUTE_JOB_ID
 *  	    port - Pointer to opamgt handle
 *  p_job_status - Pointer to job status
 *
 * Outputs:
 *              OP_ROUTE_STATUS_OK - Poll successful
 *                                   *p_job_status = job status
 *   OP_ROUTE_STATUS_INVALID_PARAM - port_handle invalid
 *                                 - p_job_status NULL
 *           OP_ROUTE_STATUS_ERROR - Poll unsuccessful
 */
enum op_route_status op_route_poll_ready( OP_ROUTE_PORT_HANDLE port_handle,
    OP_ROUTE_JOB_ID job_id, 
	struct omgt_port * port,
	enum op_route_job_status * p_job_status )
{
	enum op_route_status rstatus;

	if (!port_handle || !p_job_status)
		return (OP_ROUTE_STATUS_INVALID_PARAM);

	// Query PollReady record
	rstatus = op_route_send_recv_query( OP_ROUTE_AMOD_POLL_READY, port_handle,
		0, &job_id, p_job_status, NULL, NULL, NULL, NULL, NULL, NULL, port );

	return (rstatus);

}  // End of op_route_poll_ready()


// End of file

