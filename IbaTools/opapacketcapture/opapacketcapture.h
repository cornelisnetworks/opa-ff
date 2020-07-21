/* BEGIN_ICS_COPYRIGHT5 ****************************************

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

 * ** END_ICS_COPYRIGHT5   ****************************************/

/* [ICS VERSION STRING: unknown] */

#ifndef __H_IBA_PCAP
#define __H_IBA_PCAP

#include "iba/stl_types.h"

#include "iba/public/ipackon.h"

#define BLOCKSIZE			64
#if 1
#define DEFAULT_NUMBLOCKS	(2*1024*1024)
#else
#define DEFAULT_NUMBLOCKS	(500)
#endif

#define STL_WIRESHARK_MAJOR	2
#define STL_WIRESHARK_MINOR	4
/*PCAP Nanosecond format magic*/
#define STL_WIRESHARK_MAGIC	0xa1b23c4d
#define STL_WIRESHARK_ERF	197

#define ERF_TYPE_OPA_SNC        28
#define ERF_TYPE_OPA_9B         29

#define FILTER_COND			0
#define FILTER_DLID			1
#define FILTER_SLID			2
#define FILTER_MCLASS		3
#define FILTER_PKEY			4
#define FILTER_PTYPE		5
#define FILTER_SVCLEV		6
#define FILTER_ATTRID		7
#define FILTER_QP			8
#define FILTER_TRANS_ID_HIGH 9
#define FILTER_TRANS_ID_LOW 10

#define PACKETTYPE_RC		0
#define PACKETTYPE_UC		1
#define PACKETTYPE_RD		2
#define PACKETTYPE_UD		3
#define PACKETTYPE_ERR		9999

#define COND_TYPE_AND		0
#define COND_TYPE_OR		1

#define DEFAULT_TRIGGER_LAG	10

#define PKEY_MASK			0x7fff

// For Future Use if we add a higher complexity to our debugging
#define DBG_FLAGS_BIT0_X_SNC			1
#define DBG_FLAGS_BIT1_X_L2				1<<1
#define DBG_FLAGS_BIT2_X_MAD			1<<2
#define DBG_FLAGS_BIT3_X_PAYLOAD		1<<3
#define DBG_FLAGS_BIT4_P_SNC			1<<4
#define DBG_FLAGS_BIT5_P_SNC			1<<5
#define DBG_FLAGS_BIT6_P_L2				1<<6
#define DBG_FLAGS_BIT7_P_MAD			1<<7

#define DEBUG_TOOL_MODE 1
#define WFR_MODE        2

#define IS_FI_MODE(m) (m == WFR_MODE)

#define STL_WFR_INBOUND 1
#define STL_WFR_OUTBOUND 0
#define STL_WFR_RCV_BYPASS 0x4
#define STL_WFR_RCV_IB 0x2
#define STL_L2_16B 0x2
#define STL_L2_9B 0x3
#define STL_9B_LNH_BTH 0x2
#define STL_9B_LNH_GRH 0x3
#define STL_16B_L4_FM 0x8
#define STL_16B_L4_IB 0x9
#define STL_16B_L4_IB_GLOBAL 0xa

#define STL_16B_L4_FM_SIZE 8

typedef struct _packet {
	uint64				blockNum;
	uint64				size;
	uint64				numBlocks;
	time_t				ts_sec;
	long				ts_nsec;
	struct _packet		*next;
} packet;

typedef struct _pcapHdr_s {
	uint32 				magicNumber;
	uint16				versionMajor;
	uint16				versionMinor;
	int32				tzOffset;
	uint32				sigFigs;
	uint32				snapLen;
	uint32				networkType;
} pcapHdr_t;

typedef struct _pcapRecHdr_s {
	uint32				ts_nsec;
	uint32				ts_sec;
	uint32				packetSize;
	uint32				packetOrigSize;
} pcapRecHdr_t;

typedef struct extHeader_s {
	uint64				ts;
	uint8				linkType;		/* Lyink Type 26 = STL; 21 = IB */
	uint8				flags;			/* ERF flags */
	uint16				length;			/* Record Length in Bytes */
	uint16				lossCtr;		/* Number of packets lost between two captured packets */
	uint16				realLength;		/* Wire Length in Bytes */
} extHeader_t;

typedef struct _WFR_SnC_HDR {
	uint8 PortNumber;		// Should be 1 for WFR
	uint8 Direction; 		// 0=Egress (Out), 1=Ingress (In)
	uint8 Reserved[6];
	union {
		uint64 AsReg64;
		STL_FIELDUNION14(PBC, 64,			// if Direction == 0
			reserved_63_48:16,
			pbcstaticratecontrolcnt:16,
			pbcintr:1,
			pbcdcinfo:1,
			pbctestebp:1,
			pbcpacketbypass:1,
			pbcinserthcrc:2,
			pbccreditreturn:1,
			pbcinsertbypassicrc:1,
			pbctestbadicrc:1,
			pbcfecn:1,
			reserved_21_16:6,
			pbcvl:4,
			pbclengthdws:12);
		STL_FIELDUNION17(RHF, 64,			// if Direction == 1
			icrcerr:1,
			reserved_62_62:1,
			eccerr:1,
			lenerr:1,
			tiderr:1,
			rcvtypeerr:3,
			dcerr:1,
			dcuncerr:1,
			khdrlenerr:1,
			hdrqoffset:9,
			egroffset:12,
			rcvseq:4,
			dcinfo:1,
			egrindex:11,
			useegrbfr:1,
			rcvtype:3,
			pktlen:12);
	} u;
} WFR_SnC_HDR;

#define DESTQP_MASK 0x00ffffff

typedef struct _filterFunc_s {
	int			(*filterFunc)(packet *, uint32);
	uint32		filterVal;
	int			ioctl;
	int			notFlag;
} filterFunc_t;

typedef struct _qibPacketFilterCommand_s {
	int			opcode;
	int			length;
	void		*value_ptr;
} qibPacketFilterCommand_t;

/* Driver ioctls */
#define QIB_SNOOP_IOC_MAGIC IB_IOCTL_MAGIC
#define QIB_SNOOP_IOC_BASE_SEQ 0x80
/* This starts our ioctl sequence
 * numbers *way* off from the ones
 * defined in ib_core
 */
#define QIB_SNOOP_IOCGETLINKSTATE \
	_IO(QIB_SNOOP_IOC_MAGIC, QIB_SNOOP_IOC_BASE_SEQ)
#define QIB_SNOOP_IOCSETLINKSTATE \
	_IO(QIB_SNOOP_IOC_MAGIC, QIB_SNOOP_IOC_BASE_SEQ+1)
#define QIB_SNOOP_IOCCLEARQUEUE \
	_IO(QIB_SNOOP_IOC_MAGIC, QIB_SNOOP_IOC_BASE_SEQ+2)
#define QIB_SNOOP_IOCCLEARFILTER \
	_IO(QIB_SNOOP_IOC_MAGIC, QIB_SNOOP_IOC_BASE_SEQ+3)
#define QIB_SNOOP_IOCSETFILTER \
	_IO(QIB_SNOOP_IOC_MAGIC, QIB_SNOOP_IOC_BASE_SEQ+4)


enum qib_packet_filter_opcodes {
	FILTER_BY_LID,
	FILTER_BY_DLID,
	FILTER_BY_MAD_MGMT_CLASS,
	FILTER_BY_QP_NUMBER,
	FILTER_BY_PKT_TYPE,
	FILTER_BY_SERVICE_LEVEL,
	FILTER_BY_PKEY
};

#define IB_PACKET_SIZE		4208

#define STL_MAX_PACKET_SIZE		16*1024 // 16K
#define WIRESHARK_MAX_LENGTH	65535


#define PACKET_OUT_FILE 		"packetDump.pcap"

#include "iba/public/ipackoff.h"

#endif /* __H_IBA_PCAP */
