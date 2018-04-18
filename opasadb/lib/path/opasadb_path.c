/* BEGIN_ICS_COPYRIGHT2 ****************************************

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

** END_ICS_COPYRIGHT2   ****************************************/

/* [ICS VERSION STRING: unknown] */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <linux/types.h>
#include <verbs.h>
#include "opasadb_debug.h"
#include "opasadb_path.h"

/*
 * IbAccess headers don't play well with OFED headers, and OFED doesn't
 * export the path mask. I have to duplicate these defines here.
 */
#define IB_PATH_RECORD_COMP_SERVICEID                   0x00000003
#define IB_PATH_RECORD_COMP_DGID                        0x00000004
#define IB_PATH_RECORD_COMP_SGID                        0x00000008
#define IB_PATH_RECORD_COMP_DLID                        0x00000010
#define IB_PATH_RECORD_COMP_SLID                        0x00000020
#define IB_PATH_RECORD_COMP_RAWTRAFFIC                  0x00000040
    /* reserved field                                   0x00000080 */
#define IB_PATH_RECORD_COMP_FLOWLABEL                   0x00000100
#define IB_PATH_RECORD_COMP_HOPLIMIT                    0x00000200
#define IB_PATH_RECORD_COMP_TCLASS                      0x00000400
#define IB_PATH_RECORD_COMP_REVERSIBLE                  0x00000800
#define IB_PATH_RECORD_COMP_NUMBPATH                    0x00001000
#define IB_PATH_RECORD_COMP_PKEY                        0x00002000
#define IB_PATH_RECORD_COMP_QOS_CLASS                   0x00004000
#define IB_PATH_RECORD_COMP_SL                          0x00008000
#define IB_PATH_RECORD_COMP_MTUSELECTOR                 0x00010000
#define IB_PATH_RECORD_COMP_MTU                         0x00020000
#define IB_PATH_RECORD_COMP_RATESELECTOR                0x00040000
#define IB_PATH_RECORD_COMP_RATE                        0x00080000
#define IB_PATH_RECORD_COMP_PKTLIFESELECTOR             0x00100000
#define IB_PATH_RECORD_COMP_PKTLIFE                     0x00200000
#define IB_PATH_RECORD_COMP_PREFERENCE                  0x00400000
    /* reserved field                                   0x00800000 */
#define IB_PATH_RECORD_COMP_ALL                         0x007fff7f

/*
 * IbAccess headers don't play well with OFED headers. Because of this,
 * I have to put modified prototypes here.
 */
int op_ppath_find_path(void *reader,
					   const char *hfi_name,
					   __u16 port,
					   __u64 mask,
					   op_path_rec_t *query,
					   op_path_rec_t *result);

void op_ppath_close_reader(void *r);

void *op_ppath_allocate_reader(void);

int op_ppath_create_reader(void *reader);

struct op_path_context {
	void  	*reader;

	struct 	ibv_context *ibv_context;		
	struct 	ibv_device_attr device_attr;	
    struct 	ibv_port_attr port_attr; 		

	__u16	port_num;
	__u16	*pkey_table;
};

static uint64_t build_comp_mask(op_path_rec_t path)
{
	uint64_t mask = 0;

	if (path.service_id) 
		mask |= IB_PATH_RECORD_COMP_SERVICEID;

	if (path.dgid.unicast.interface_id | path.dgid.unicast.prefix) 
		mask |= IB_PATH_RECORD_COMP_DGID;
	if (path.sgid.unicast.interface_id | path.sgid.unicast.prefix) 
		mask |= IB_PATH_RECORD_COMP_SGID;
	if (path.dlid) 
		mask |= IB_PATH_RECORD_COMP_DLID;
	if (path.slid) 
		mask |= IB_PATH_RECORD_COMP_SLID;
	if (ntohl(path.hop_flow_raw) & 0x80000000) 
		mask |= IB_PATH_RECORD_COMP_RAWTRAFFIC;
	if (ntohl(path.hop_flow_raw) & 0x0FFFFF00)
		mask |= IB_PATH_RECORD_COMP_FLOWLABEL;
	if (ntohl(path.hop_flow_raw) & 0x000000FF)
		mask |= IB_PATH_RECORD_COMP_HOPLIMIT;
	if (path.tclass) 
		mask |= IB_PATH_RECORD_COMP_TCLASS;
	if (path.num_path & 0xFF) mask |= IB_PATH_RECORD_COMP_REVERSIBLE;
	if (path.num_path & 0x7F) mask |= IB_PATH_RECORD_COMP_NUMBPATH;
	if (path.pkey) mask |= IB_PATH_RECORD_COMP_PKEY;
	if (ntohs(path.qos_class_sl) & 0x0FFF) 
		mask |= IB_PATH_RECORD_COMP_QOS_CLASS;
	if (ntohs(path.qos_class_sl) & 0xF000) 
		mask |= IB_PATH_RECORD_COMP_SL;
	if (path.mtu & 0xC0) 
		mask |= IB_PATH_RECORD_COMP_MTUSELECTOR;
	if (path.mtu & 0x3F) 
		mask |= IB_PATH_RECORD_COMP_MTU;
	if (path.rate & 0xC0) 
		mask |= IB_PATH_RECORD_COMP_RATESELECTOR;
	if (path.rate & 0x3F) 
		mask |= IB_PATH_RECORD_COMP_RATE;
	if (path.pkt_life & 0xC0) 
		mask |= IB_PATH_RECORD_COMP_PKTLIFESELECTOR;
	if (path.pkt_life & 0x3F) 
		mask |= IB_PATH_RECORD_COMP_PKTLIFE;
	if (path.preference) 
		mask |= IB_PATH_RECORD_COMP_PREFERENCE;

	return mask;
};

static struct ibv_device	**dev_list = NULL;
static int num_devices = 0;

/*
 * Convenience function. Given the name of an HFI,
 * returns the ibv_device structure associated with it.
 * Returns NULL if the HFI could not be found.
 *
 * HFI can be identified by name ("mthfi0") or by number
 * "1", "2", et cetera.
 *
 * OPENS THE HFI! Use ibv_close_device() to release it.
 */ 
struct ibv_context *
op_path_find_hfi(const char *name, 
			    struct ibv_device **device)
{
	struct ibv_device	*ibv_dev = NULL;
	struct ibv_context	*context = NULL;
	int i;

	if (!dev_list) {
		dev_list = ibv_get_device_list(&num_devices);
	}
	if (!dev_list) {
		errno = EFAULT;
		return NULL;
	}
	if (name == NULL || name[0]=='\0') {
		i=0;
	} else if (isdigit(name[0])) {
		i = strtoul(name,NULL,0) - 1;
		if (i < 0 || i >= num_devices){
			errno = EFAULT;
                        return NULL;
		}
	} else {
		for (i=0; i < num_devices; i++) {
			if (!strcmp(ibv_get_device_name(dev_list[i]), name))
				break;
		}
		if (i >= num_devices) {
			errno = EFAULT;
			return NULL;
		}
	}
	ibv_dev = dev_list[i];

	/*
	 * Opens the verbs interface to the HFI.
	 * Note that this will increment the usage counter for that
	 * HFI. This needs to be done before we release the device list.
	 */
	if(ibv_dev) {
		context = ibv_open_device(ibv_dev);
		if (!context) {
			errno = EFAULT;
			*device = NULL;
		} else {
			*device = ibv_dev;
		}
	} else {
		*device = NULL;
		errno = ENODEV;
	}

	return context;
}

/*
 * Opens the interface to the opp module.
 * Must be called before any use of the path functions.
 *
 * device		The verbs context for the HFI.
 *				Can be acquired via op_path_find_hfi.
 *
 * port_num		The port to use for sending queries.
 *
 * This information is used for querying pkeys and 
 * calculating timeouts.
 *
 * Returns a pointer to the op_path context on success, or returns NULL 
 * and sets errno if the device could not be opened.
 */
void *
op_path_open(struct ibv_device *device, int p)
{
	int         i, err;
	struct op_path_context *context;

	if (!device) {
		errno=ENXIO;
		return NULL;
	}

	context = malloc(sizeof(struct op_path_context));
	if (!context) {
		errno=ENOMEM;
		return NULL;
	}
	memset(context,0,sizeof(struct op_path_context));

	context->ibv_context = ibv_open_device(device);
	if (!context->ibv_context) {
		errno=ENODEV;
		goto open_device_failed;
	}

	context->port_num = p;

	context->reader = op_ppath_allocate_reader();
	if (!context->reader) {
		errno=ENOMEM;
		goto alloc_reader_failed;
	}

	err = op_ppath_create_reader(context->reader);
	if (err) {
		errno=err;
		goto create_reader_failed;
	}

	if ((err=ibv_query_device(context->ibv_context,
                         &(context->device_attr)))) {
		errno=EFAULT;
		goto query_attr_failed;
	}

	if ((err=ibv_query_port(context->ibv_context,
                       context->port_num,
                       &(context->port_attr)))) {
		errno=EFAULT;
		goto query_attr_failed;
	}

	context->pkey_table = malloc(context->device_attr.max_pkeys* sizeof(int));     
	if (!context->pkey_table) {
		errno= ENOMEM;
		goto query_attr_failed;
	}
	memset(context->pkey_table,0,context->device_attr.max_pkeys* sizeof(int));

	for (i = 0, err = 0; !err && i<context->device_attr.max_pkeys; i++) {
		err = ibv_query_pkey(context->ibv_context, context->port_num, i,
                             &(context->pkey_table[i]));
		if (err) {
			errno=EFAULT;
			goto query_pkey_failed;
		}
	}

	return context;

query_pkey_failed:
	free(context->pkey_table);
query_attr_failed:
	op_ppath_close_reader(context->reader);
create_reader_failed:
	free(context->reader);
alloc_reader_failed:
	ibv_close_device(context->ibv_context);
open_device_failed:
	free(context);
	return NULL;
}

/*
 * Closes the interface to the opp module.
 * No path functions will work after calling this function.
 */
void 
op_path_close(void *uc)
{
	struct op_path_context *context = (struct op_path_context *)uc;
	
	op_ppath_close_reader(context->reader);
	
	/*
	 * Close the HFI and release the memory being used.
	 */
	ibv_close_device(context->ibv_context);

	/*
     * Releases the memory used by the list - and decrements the usage
     * counters on the HFIs. Note that this is important; all the HFIs
     * in the device list are flagged as being used from the time
     * the list is acquired until it is freed.
     */
    if (dev_list) {
		ibv_free_device_list(dev_list);
		dev_list = NULL;
		num_devices = 0;
	}
	
	free(context);
}

/*
 * Retrieves an IB Path Record given a particular query.
 *
 * context		Pointer to the HFI and port to query on.
 *
 * query		The path record to use for asking the query.
 *				All fields should be zero'ed out except for
 *				those being used to query! For example,
 *				a simple query might specify the source lid,
 *				the destination lid and a pkey. All fields should
 *				be in network byte order.
 * 
 * response		The path record where the completed path will
 *				be written. All fields are in network byte order.
 *
 * RETURN VAL:	This function will return a 0 on success,
 *				or non-zero on error.
 */
int 
op_path_get_path_by_rec(void *uc,
					   op_path_rec_t *query, 
					   op_path_rec_t *response)
{
	uint64_t mask = build_comp_mask(*query);
	struct op_path_context *context = (struct op_path_context *)uc;

	return op_ppath_find_path(context->reader,
							  ibv_get_device_name(context->ibv_context->device),
							  context->port_num,
							  mask,
					  		  query,
							  response);
}

/*
 * Retrieves an IB Path Record to a given destination.
 *
 * op_path_context  A handle to the interface to query over.
 *
 * hfi_context  A handle to the HFI that will be used for
 *              this path. 
 *
 * port         The local port number that will be used 
 *              for this path. (not the lid.)
 *
 * dgid         The destination GID.
 *
 * pkey         The desired pkey. This should be zero to
 *              accept the default.
 *
 * response     The path record where the completed path will
 *              be written. All fields are in network byte order.
 *
 * RETURN VAL:  This function will return a value of 0 on success,
 *              or non-zero on error.
 */
int 
op_path_get_path_to_dgid(void *uc,
						uint16_t pkey,
						union ibv_gid dgid,
						op_path_rec_t *response)
{

	op_path_rec_t query;
	struct op_path_context *context = (struct op_path_context *)uc;
		
	memset(&query,0,sizeof(query));

	query.dgid.unicast.prefix = dgid.global.subnet_prefix;
	query.dgid.unicast.interface_id = dgid.global.interface_id;
	query.pkey = pkey;
	query.slid = ntohs(context->port_attr.lid);

	return op_path_get_path_by_rec(uc,&query,response);
}


/*
 * Converts a pkey to a pkey index, which is needed by the queue pairs.
 * Returns 0 on succes, non zero on error.
 */
int op_path_find_pkey(void *uc, uint16_t pkey, uint16_t *pkey_index)
{
	int i;
	struct op_path_context *context = (struct op_path_context *)uc;

	for (i = 0; i< context->device_attr.max_pkeys; i++) {
		if (pkey == context->pkey_table[i]) {
			*pkey_index = (uint16_t) i;
			return 0;
		}
	}

	return EINVAL;
}

/*
 * Given an HFI and a path record's packet lifetime attribute, 
 * compute the packet lifetime for a queue pair.
 */
uint8_t op_path_compute_timeout(struct ibv_context *hfi_context, 
								uint8_t pkt_life)
{
	uint8_t result;
	struct ibv_device_attr device_attr;

	pkt_life &= 0x1f;

	if (ibv_query_device(hfi_context, &device_attr))
		return 0;

	/* Adding 1 has the effect of doubling the timeout. */
	/* We do this because we're looking for a round-trip figure. */
	result = pkt_life + 1; 

	/* If the HFI's local delay is larger than the time out so far, */
	/* replace the result with the local delay, doubled. */
	if (pkt_life < device_attr.local_ca_ack_delay) {
		result = device_attr.local_ca_ack_delay + 1;
	}  else {
		/* The local delay is smaller than the packet life time, */
		/* so we'll simply double the timeout value again. */
		result += 1;
	}

	if (result > 0x1f) result = 0x1f;

	return result;
}

/*
 * Fills out a qp_attr structure and its mask based on the contents
 * of a path record and an optional alternate path.
 *
 * alt_path	can be null. In that case, no alternate path will be 
 * set.
 *
 * Returns a mask listing the attributes that have been set. Returns 0 
 * on error and sets errno.
 */
unsigned op_path_qp_attr(struct ibv_qp_attr *qp_attr,
						void *uc,
						op_path_rec_t *primary_path,
						void *alt_uc,
						op_path_rec_t *alt_path)

{
	struct op_path_context *context = uc;
	uint16_t pkey_index = 0;

	struct op_path_context *alt_context = alt_uc;
	uint16_t alt_pkey_index = 0;

	unsigned mask = 0;
	struct ibv_ah_attr *ah_attr = &qp_attr->ah_attr;

	if (op_path_find_pkey(context,primary_path->pkey, &pkey_index)) {
		errno = EINVAL;
		return 0;
	}

	qp_attr->path_mtu				= OP_PATH_REC_MTU(primary_path);
	mask							|= IBV_QP_PATH_MTU;
	qp_attr->pkey_index				= pkey_index;
	mask							|= IBV_QP_PKEY_INDEX;

	ah_attr->dlid					= ntohs(primary_path->dlid);
	ah_attr->sl						= OP_PATH_REC_SL(primary_path);
	ah_attr->src_path_bits			= ntohs(primary_path->slid) & ((1<<context->port_attr.lmc) - 1); 
	ah_attr->static_rate			= primary_path->rate & 0x3f;
	ah_attr->port_num				= context->port_num;
	ah_attr->is_global				= (OP_PATH_REC_HOP_LIMIT(primary_path)>0)?1:0;
	if (ah_attr->is_global) {
		ah_attr->grh.dgid.global.subnet_prefix = primary_path->dgid.unicast.prefix;
		ah_attr->grh.dgid.global.interface_id = primary_path->dgid.unicast.interface_id;
		ah_attr->grh.flow_label		= OP_PATH_REC_FLOW_LBL(primary_path);
		ah_attr->grh.sgid_index		= 0; // BUGBUG - One day this will be wrong.
		ah_attr->grh.hop_limit		= OP_PATH_REC_HOP_LIMIT(primary_path);
		ah_attr->grh.traffic_class	= primary_path->tclass;
	}
	mask                            |= IBV_QP_AV;

	if (alt_path && alt_context) {
		struct ibv_ah_attr *alt_ah_attr = &qp_attr->alt_ah_attr;

		if (op_path_find_pkey(alt_context,alt_path->pkey, &alt_pkey_index))
			return 0;
		
		qp_attr->path_mig_state 	= IBV_MIG_ARMED;
		mask 						|= IBV_QP_PATH_MIG_STATE;
		qp_attr->alt_pkey_index		= alt_pkey_index;
		mask 						|= IBV_QP_ALT_PATH;

		alt_ah_attr->dlid			= ntohs(alt_path->dlid);
		alt_ah_attr->sl				= OP_PATH_REC_SL(alt_path);
		alt_ah_attr->src_path_bits	= ntohs(alt_path->slid) & ((1<<alt_context->port_attr.lmc) - 1);
		alt_ah_attr->static_rate	= alt_path->rate & 0x3f;
		alt_ah_attr->port_num		= alt_context->port_num;
		alt_ah_attr->is_global		= (OP_PATH_REC_HOP_LIMIT(alt_path)>0)?1:0;
		if (alt_ah_attr->is_global) {
			alt_ah_attr->grh.dgid.global.subnet_prefix = alt_path->dgid.unicast.prefix;
			alt_ah_attr->grh.dgid.global.interface_id = alt_path->dgid.unicast.interface_id;
			alt_ah_attr->grh.flow_label	= OP_PATH_REC_FLOW_LBL(alt_path);
			alt_ah_attr->grh.sgid_index	= 0; // BUGBUG - One day this will be wrong.
			alt_ah_attr->grh.hop_limit	= OP_PATH_REC_HOP_LIMIT(alt_path);
			alt_ah_attr->grh.traffic_class	= alt_path->tclass;
		}
	}

	return mask;
}

unsigned op_path_check_version(void)
{
	return OPA_SA_DB_PATH_API_VERSION;
}
