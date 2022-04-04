/* BEGIN_ICS_COPYRIGHT2 ****************************************

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

** END_ICS_COPYRIGHT2   ****************************************/

/* [ICS VERSION STRING: unknown] */

//===========================================================================//
//									     //
// FILE NAME								     //
//    sa_l.h								     //
//									     //
// DESCRIPTION								     //
//    This file contains the localized SA definitions.  These definitions    //
//    do NOT appear in the IBTA specs.  They are implementation dependent.   //
//									     //
// DATA STRUCTURES							     //
//    None								     //
//									     //
// FUNCTIONS								     //
//    None								     //
//									     //
// DEPENDENCIES								     //
//    ib_mad.h								     //
//    ib_status.h							     //
//    ib_const.h							     //
//    sm_pool.h								     //
//    sm_event.h							     //
//    sm_thread.h							     //
//									     //
//									     //
//===========================================================================//

#ifndef	_SA_L_H_
#define	_SA_L_H_

#include "ib_const.h"
#include "ib_types.h"
#include "cs_g.h"
#include "cs_hashtable.h"

// JSY - this is temporary
#define	mai_poll(FD, MAIP)	mai_recv(FD, MAIP, 1)

/* Fake portGuid to use when auto-creating the multicast group */
#define SA_FAKE_MULTICAST_GROUP_MEMBER 0x00066A00FACADE01ull

//
//	This struct is for the queued MAD requests to the SA>
//
#define SA_MADS         256

typedef struct {
	Mai_t		mad;
	uint64_t	timer;
} SAQueue_t;

#define	SA_Queue_Empty()	(sarp == sawp)
#define	SA_Queue_Full()		(((sarp+1) == sawp) || 				\
				((sarp == &sa_queue[SA_MADS-1]) && 		\
				(sawp == &sa_queue[0])))
#define	SA_Queue_Inc(QP)	((QP) != &sa_queue[SA_MADS - 1]) ?		\
				(QP)+1 : &sa_queue[0]


//
//	A frequently used debugging macro.
//
#define	SAPrint(HDR, MAIP)							\
	printf("%s: %02x %04x %08x %016Lx -> %02x\n", (HDR),			\
		(MAIP)->base.method, 						\
		(MAIP)->base.aid, 						\
		(MAIP)->base.amod, 						\
		(MAIP)->base.tid, 						\
		(MAIP)->base.status);

//
//	Comparison mask definitions.
//
//      These arrays contain the number of bytes to compare for the template
//      query case.
//
typedef struct _FieldMask {
	uint16_t	offset;			// offset from 'bit 0'
	uint16_t	length;			// number of bits in field
} FieldMask_t;

//
//      Scratch pad for template queries.
//
extern	uint8_t         template_mask[4096];
extern	uint32_t        template_offset;
extern	uint32_t        template_length;
extern	FieldMask_t     *template_fieldp;

//
// SA Caching
//

#define SA_NUM_CACHES 2         // number of cached query types

#define SA_CACHE_FI_NODES     0 // all NodeRecords where NodeType == FI
#define SA_CACHE_SWITCH_NODES 1 // all NodeRecords where NodeType == Switch

#define SA_CACHE_CLEAN_INTERVAL 30*VTIMER_1S // time between clearing of the
                                       // "previous" list of in-use elements
								   
#define SA_CACHE_NAME_LEN 16 // max length of cache name

// represents a cache containing the response data for a specific SA query
typedef struct SACacheEntry {
	char     name[SA_CACHE_NAME_LEN]; // name of cache for debugging
	uint8_t  valid;     // true if cache contains data
	uint8_t  transient; // marks a temporary cache structure
	uint8_t  *data;     // wire-ready cached data
	uint32_t len;       // length of cached data
	uint32_t refCount;  // # of outstanding references to this cache
	uint32_t records;   // number of records represented by this cache
	struct SACacheEntry *next; // used for "previous" list
} SACacheEntry_t;

// consolidation of cached data sets into a single structure
typedef struct {
	SACacheEntry_t *current[SA_NUM_CACHES]; // array of current caches
	SACacheEntry_t *build[SA_NUM_CACHES]; // array of caches being built in new topology
	SACacheEntry_t *previous; // linked list of all previous caches still in
	                          // use.  should be minimal as all queries using
	                          // the caches should finish between sweeps
	Lock_t         lock; // lock mediating topology and query threads
} SACache_t;

// prototype of function to be called to build each cache
typedef Status_t (*SACacheBuildFunc_t)(SACacheEntry_t *, Topology_t *);

//
//	Authentication structure.
//
typedef	struct {
	STL_LID		lid;		// 16 bit Lid
	Node_t		*nodep;		// node for this Lid
	Port_t		*portp;		// port on the node for this Lid
	PKey_t		pKeys[SM_PKEYS];	// PKeys for comparison
	uint32_t	numKeys;
} Authenticator_t;

#define SA_CNTXT_HASH_TABLE_DEPTH	64
extern uint32_t sa_max_cntxt;
extern uint32_t sa_data_length;	// maximum SA response size
extern uint32_t sa_max_ib_path_records;// maximum IB path records in one response
#define SA_CNTXT_MAX_GUIDINFO		256
#define SA_CNTXT_MAX_STANDARD		256
typedef struct sa_cntxt {
	uint64_t	tstamp ;
	uint64_t	tid ;		// Tid for hash table search
	STL_LID		lid ;		// Lid for hash table search
    uint16_t    method;     // initial method requested by initiator
    IBhandle_t	sendFd;     // mai handle to use for sending packets (fd_sa for 1st seg and fd_sa_w threafter)
	uint8_t		hashed ;	// Entry is inserted into the hash table
	uint32_t	ref ;		// Reference count for the structure
    uint32_t    reqDataLen; // length of the getMulti request MAD
	char*		reqData ;	// Data pointer for input getMulti MAD
	char*		data ;		// Data pointer for MAD rmpp response
	uint32_t	len ;		// Length of the MAD response
    uint16_t    attribLen;  // num 8-byte words from start of one attrib to start of next
	Mai_t		mad ;
    /* 1.1 related fields */
    uint32_t    WF;         // Window First: segNum that is first packet in current window
    uint32_t    WL;         // Window Last: segNum that is last packet in current window
    uint32_t    NS;         // Next segNum to be tranmitted by Sender
    uint32_t    ES;         // segNum expected next (Receiver side)
    uint16_t    isDS;       // Double sided getMulti in effect
    uint16_t    reqInProg;  // receipt of request in progress
    uint64_t    RespTimeout;// current response timeout value (13.6.3.1)
    uint64_t    tTime;      // total transaction timeout value (13.6.3.2)
	uint16_t	retries;    // retry count
	uint16_t	last_ack;   // last segment number acked
    uint16_t    segTotal;   // total segments in response
	struct sa_cntxt *next ;	// Link List next pointer
	struct sa_cntxt *prev ;	// Link List prev pointer
    uint8_t     chkSum;     // checksum of rmpp response 
	SACacheEntry_t *cache;  // pointer to cache structure if applicable
	Status_t (*freeDataFunc)(struct sa_cntxt *); // func to call to free data. may
	                        // either free locally allocated data, or defer to
	                        // the cache mechanism to decref the cache
	Status_t (*processFunc)(Mai_t *, struct sa_cntxt *); // function to call
	                        // to process the incoming packet.
} sa_cntxt_t ;

//
//	Macros for SA filter creation.
//
#define	SA_Filter_Init(FILTERP) {					\
	Filter_Init(FILTERP, 0, 0);					\
									\
	(FILTERP)->active |= MAI_ACT_ADDRINFO;				\
	(FILTERP)->active |= MAI_ACT_BASE;				\
	(FILTERP)->active |= MAI_ACT_TYPE;				\
	(FILTERP)->active |= MAI_ACT_DATA;				\
	(FILTERP)->active |= MAI_ACT_DEV;				\
	(FILTERP)->active |= MAI_ACT_PORT;				\
	(FILTERP)->active |= MAI_ACT_QP;				\
	(FILTERP)->active |= MAI_ACT_FMASK;				\
									\
	(FILTERP)->type = MAI_TYPE_EXTERNAL;					\
	(FILTERP)->type = MAI_TYPE_ANY;	/* JSY - temp fix for CAL */	\
									\
	(FILTERP)->dev = sm_config.hca;					\
	(FILTERP)->port = (sm_config.port == 0) ? MAI_TYPE_ANY : sm_config.port;	\
	(FILTERP)->qp = 1;						\
}

typedef enum {
	ContextAllocated = 1,       // new context allocated
	ContextNotAvailable = 2,    // out of context
	ContextExist = 3,           // general duplicate request
    ContextExistGetMulti = 4    // existing getMult request
} SAContextGet_t;

//
// Rate macros - these allow us to compare rates easily
//
#define linkrate_eq(r1, r2) ((r1) == (r2))
#define linkrate_ne(r1, r2) ((r1) != (r2))
#define linkrate_lt(r1, r2) (StlStaticRateToMbps(r1) < StlStaticRateToMbps(r2))
#define linkrate_le(r1, r2) (StlStaticRateToMbps(r1) <= StlStaticRateToMbps(r2))
#define linkrate_gt(r1, r2) (StlStaticRateToMbps(r1) > StlStaticRateToMbps(r2))
#define linkrate_ge(r1, r2) (StlStaticRateToMbps(r1) >= StlStaticRateToMbps(r2))

// iterators to help construct Path Record responses
typedef struct lid_iterator_s {
	STL_LID	src_start;
	STL_LID	src_endp1;
	STL_LID	src_lmc_mask;
	STL_LID	dst_start;
	STL_LID	dst_endp1;
	STL_LID	dst_lmc_mask;
	STL_LID	slid;
	STL_LID	dlid;
	uint8	phase;
	uint8	end_phase;
} lid_iterator_t;

// these are defined in fm_xml.h
//#define PATH_MODE_MINIMAL	  0	// no more than 1 path per lid
//#define PATH_MODE_PAIRWISE  1	// cover every lid on "bigger side" exactly once
//#define PATH_MODE_ORDERALL  2	// PAIRWISE, then all src, all dst (skip dups)
//#define PATH_MODE_SRCDSTALL 3	// all src, all dst

void lid_iterator_init(lid_iterator_t *iter,
				Port_t *src_portp, STL_LID src_start_lid, STL_LID src_lid_len,
				Port_t *dst_portp, STL_LID dst_start_lid, STL_LID dst_lid_len,
				int path_mode,
			   	STL_LID *slid, STL_LID *dlid);
int lid_iterator_done(lid_iterator_t *iter);
void lid_iterator_next(lid_iterator_t *iter, STL_LID *slid, STL_LID *dlid);
// This handles 1 fixed lid, 1 GID or wildcard style queries.
// src is the fixed side, dst is the iterated side.
// doesn't really matter if src is source or dest and visa versa
void lid_iterator_init1(lid_iterator_t *iter,
				Port_t *src_portp, STL_LID slid, STL_LID src_lid_start, STL_LID src_lid_len,
				Port_t *dst_portp, STL_LID dst_lid_start, STL_LID dst_lid_len,
			   	int path_mode,
			   	STL_LID *dlid);
int lid_iterator_done1(lid_iterator_t *iter);
void lid_iterator_next1(lid_iterator_t *iter, STL_LID *dlid);

//
//	External declarations.
//
extern SubscriberTable_t   saSubscribers;
extern ServiceRecTable_t   saServiceRecords;
extern  uint32_t    saDebugRmpp;    // controls output of SA INFO RMPP+ messages
extern uint32_t     saRmppCheckSum; // rmppp response checksum control
extern	uint8_t		*sa_data;
extern SACache_t	saCache;
extern SACacheBuildFunc_t	saCacheBuildFunctions[];	

//
// Prototypes
//
Status_t    sa_SubscriberInit(void);
void        sa_SubscriberDelete(void);
void        sa_SubscriberClear(void);
Status_t    sa_ServiceRecInit(void);
void        sa_ServiceRecDelete(void);
void        sa_ServiceRecClear(void);
Status_t    sa_McGroupInit(void);
void        sa_McGroupDelete(void);
Status_t	sa_Authenticate_Path(STL_LID, STL_LID);
Status_t	sa_Authenticate_Access(uint32_t, STL_LID, STL_LID, STL_LID);
Status_t	sa_Compare_Node_Port_PKeys(Node_t*, Port_t*);
Status_t	sa_Compare_Port_PKeys(Port_t*, Port_t*);
extern uint32_t    saDebugPerf;  // control SA performance messages; default is off

Status_t	sa_data_offset(uint16_t class, uint16_t type);
Status_t	sa_create_template_mask(uint16_t, uint64_t);
Status_t	sa_template_test_mask(uint64_t, uint8_t *, uint8_t **, uint32_t, uint32_t, uint32_t *);
Status_t	sa_template_test(uint8_t *, uint8_t **, uint32_t, uint32_t, uint32_t *);
Status_t    sa_template_test_noinc(uint8_t *, uint8_t *, uint32_t);
void        sa_increment_and_pad(uint8_t **, uint32_t, uint32_t, uint32_t *);
Status_t    sa_check_len(uint8_t *, uint32_t, uint32_t);

Status_t    sa_getMulti_resend_ack(sa_cntxt_t *);
Status_t	sa_send_reply(Mai_t *, sa_cntxt_t*);
Status_t	sa_send_multi(Mai_t *, sa_cntxt_t*);
Status_t	sa_send_single(Mai_t *, sa_cntxt_t*);

Status_t	sa_ClassPortInfo(Mai_t *, sa_cntxt_t* );
Status_t	sa_InformInfo(Mai_t *, sa_cntxt_t* );
Status_t	sa_InformRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_LFTableRecord(Mai_t *, sa_cntxt_t* );


Status_t	sa_LinkRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_McMemberRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_MFTableRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_NodeRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_PartitionRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_PathRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_PortInfoRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_SAResponse(Mai_t *, sa_cntxt_t* );
Status_t	sa_SCSCTableRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_SLSCTableRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_SCSLTableRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_SCVLTableRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_SMInfoRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_ServiceRecord(Mai_t *, sa_cntxt_t* );
Status_t    sa_ServiceRecord_Age(uint32_t *);
Status_t	sa_SwitchInfoRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_Trap(Mai_t *);
Status_t	sa_VLArbitrationRecord(Mai_t *, sa_cntxt_t* );


Status_t	sa_TraceRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_VFabricRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_JobManagement(Mai_t *, sa_cntxt_t* );
Status_t	sa_FabricInfoRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_QuarantinedNodeRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_CongInfoRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_CableInfoRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_SwitchCongRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_SwitchPortCongRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_HFICongRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_HFICongCtrlRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_BufferControlTableRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_PortGroupRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_PortGroupFwdRecord(Mai_t *, sa_cntxt_t* );
Status_t	sa_DeviceGroupMemberRecord(Mai_t *, sa_cntxt_t * );
Status_t	sa_DeviceGroupNameRecord(Mai_t *, sa_cntxt_t * );
Status_t	sa_DeviceTreeMemberRecord(Mai_t *, sa_cntxt_t * );
Status_t	sa_NodeRecord_BuildCACache(SACacheEntry_t *, Topology_t *);
Status_t	sa_NodeRecord_BuildSwitchCache(SACacheEntry_t *, Topology_t *);

Status_t	sa_SwitchCostRecord(Mai_t *, sa_cntxt_t* );

Status_t	pathrecord_userexit(uint8_t *, uint32_t *);
Status_t	multipathrecord_userexit(uint8_t *, uint32_t *);
Status_t	servicerecord_userexit(Mai_t *);
Status_t	tracerecord_userexit(uint8_t *, uint32_t *);

Status_t	sa_process_getmulti(Mai_t *, sa_cntxt_t*);
Status_t	sa_process_mad(Mai_t *, sa_cntxt_t*);
Status_t    sa_process_inflight_rmpp_request(Mai_t *, sa_cntxt_t*);
void		sa_main_reader(uint32_t, uint8_t **);
void		sa_main_writer(uint32_t argc, uint8_t ** argv);
void        sa_cntxt_age(void);
sa_cntxt_t* sa_cntxt_find( Mai_t* );
SAContextGet_t	sa_cntxt_get( Mai_t*, void**);
Status_t   	sa_cntxt_release( sa_cntxt_t* );
Status_t	sa_cntxt_reserve( sa_cntxt_t* );
Status_t	sa_cntxt_data( sa_cntxt_t*, void*, uint32_t );
Status_t	sa_cntxt_data_cached(sa_cntxt_t *, void *, uint32_t, SACacheEntry_t *);
void        sa_cntxt_clear(void);
Status_t	sa_SetDefBcGrp( void );

Status_t	sa_cache_init(void);
Status_t	sa_cache_alloc_transient(SACacheEntry_t *[], int, SACacheEntry_t **);
Status_t	sa_cache_get(int, SACacheEntry_t **);
void		sa_cache_clean(void);
Status_t	sa_cache_release(SACacheEntry_t *);
Status_t    sa_cache_cntxt_free(sa_cntxt_t *);

char *      sa_getMethodText(int method);
char *      sa_getAidName(uint16_t aid);

void		dumpGid(IB_GID gid);
void		dumpGuid(Guid_t guid);
void		dumpBytes(uint8_t * bytes, size_t num_bytes);
void		dumpHWords(uint16_t * hwords, size_t num_hwords);
void		dumpWords(uint32_t * words, size_t num_words);
void		dumpGuids(Guid_t * guids, size_t num_guids);  

STL_SERVICE_RECORD* getNextService(uint64_t *serviceId, IB_GID *serviceGid, uint16_t *servicep_key, STL_SERVICE_RECORD *pSrp);
STL_SERVICE_RECORD* getService(uint64_t *serviceId, IB_GID *serviceGid, uint16_t *servicep_key, STL_SERVICE_RECORD *pSrp);

McGroup_t*	getBroadCastGroup(IB_GID * pGid, McGroup_t *pGroup);
McGroup_t*	getNextBroadCastGroup(IB_GID * pGid, McGroup_t *pGroup);
McMember_t*	getBroadCastGroupMember(IB_GID * pGid, uint32_t index, McMember_t *pMember);
McMember_t*	getNextBroadCastGroupMember(IB_GID * pGid, uint32_t *index, McMember_t *pMember);
Status_t     clearBroadcastGroups(int);  // TRUE=recreate broadcast group

Status_t sa_McMemberRecord_Set(Topology_t *topop, Mai_t *maip, uint32_t *records);
Status_t sa_McMemberRecord_Delete(Topology_t *topop, Mai_t *maip, uint32_t *records);

Status_t sm_sa_forward_trap(STL_NOTICE *noticep);

#endif	// _SA_L_H_

