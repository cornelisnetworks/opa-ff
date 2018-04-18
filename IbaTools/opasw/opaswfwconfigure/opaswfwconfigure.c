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
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* work around conflicting names */

#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include "opamgt_sa_priv.h"
#include <iba/ibt.h>
#include "opaswcommon.h"
#include "opaswmetadata.h"
#include "zlib.h"

/* defines */

#define LIST_FILE_SUPPORTED 0

#define CR						'\r'
#define NL						'\n'
#define NUL						'\0'
#define OB						'['
#define CB						']'
#define EQ						'='
#define QU						'\"'
#define SP						' '

#define TABLE_BLOCKSIZE			25
#define INBUFFERSIZE			4096

#define OPASW_INIBIN			"iniXedge.inibin"
#define VIPER_INIBIN			"iniViper.inibin"
#define PRR_INIBIN				"prrIniEdge.inibin"

#define INI_MAX_SIZE			(16 * 1024)

#define SIGNATURE				0x50724F6d
#define EEPROMSIZE				(64 * 1024)

#define FW_RESP_WAIT_TIME		2000

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

/* typedefs */

/* global variables */

int						g_debugMode = 0;
int						g_verbose = 0;
int						g_quiet = 0;
int						g_gotHfi = 0;
int						g_gotPort = 0;
int						g_gotModule = 0;
int						g_gotSecondary = 0;
int						g_fileParam = 0;
int						g_dirParam = 0;
int						g_respTimeout = FW_RESP_WAIT_TIME;

char					fwFileName[FNAME_SIZE];
char					inibinFileName[FNAME_SIZE];
char					dirName[DNAME_SIZE];

char					*cmdName;

uint8					switchNodeDesc[64];
EUI64					switchNodeGuid;
EUI64					switchSystemImageGuid;
uint32					newNumPorts;
uint32					fmPushButtonState;
uint32					newPortEntrySize;
uint32					oldNumPorts;
uint32					oldPortEntrySize;
uint32					*oldPortDataTable;
uint32					*oldPortMetaTable;
IB_PATH_RECORD			path;
VENDOR_MAD				mad;
uint16					sessionID = 0;
uint32					max_data_table_type;

#define DEBUG_USE_FILE	0

/* for PRR stuff */

// SYSTEM_TABLE definitions
#define SYSTEM_TABLE_FIELD_RESERVED                                  0 
#define SYSTEM_TABLE_FIELD_NODE_STRING                               1 
#define SYSTEM_TABLE_FIELD_SYSTEM_IMAGE_GUID                         2 
#define SYSTEM_TABLE_FIELD_NODE_GUID                                 3 
#define SYSTEM_TABLE_FIELD_NUM_PORTS								 8
#define SYSTEM_TABLE_FIELD_FM_PUSH_BUTTON_STATE                      33

// PORT_TABLE definitions
#define PORT_TABLE_FIELD_LINK_SPEED_SUPPORTED                       7
#define PORT_TABLE_FIELD_LINK_WIDTH_SUPPORTED                       8
#define PORT_TABLE_FIELD_CRC										22
#define PORT_TABLE_FIELD_FM_ENABLED									17
#define PORT_TABLE_FIELD_VCU										28
#define PORT_TABLE_FIELD_EXTERNAL_LOOPBACK_ALLOWED					30

// INI TABLE types
#define INI_TYPE_END_TABLE									0
#define INI_TYPE_SYSTEM_TABLE								1
#define INI_TYPE_PORT_TABLE									2
#define INI_TYPE_DFE_TABLE									3
#define INI_TYPE_FFE_TABLE									4
#define INI_TYPE_FFE_INDIRECT_TABLE							5
#define INI_TYPE_PORT_TYPE_QSFP_TABLE						6
#define INI_TYPE_PORT_TYPE_CUSTOM_TABLE						7
#define INI_TYPE_DATA_TABLE_MAX								(INI_TYPE_PORT_TYPE_CUSTOM_TABLE + 1)
#define INI_TYPE_META_DATA_TABLE_MAX						(INI_TYPE_FFE_INDIRECT_TABLE + 1)

#define S_S20INI_META_TABLE_ENTRY_START_BIT				0
#define L_S20INI_META_TABLE_ENTRY_START_BIT				15
#define S_S20INI_META_TABLE_ENTRY_PROTECTED				15
#define L_S20INI_META_TABLE_ENTRY_PROTECTED				1
#define S_S20INI_META_TABLE_ENTRY_BITS					16
#define L_S20INI_META_TABLE_ENTRY_BITS					16

#define S_S20INI_RECORD_HEADER_RECORD_IDX	0
#define L_S20INI_RECORD_HEADER_RECORD_IDX	6
#define M_S20INI_RECORD_HEADER_RECORD_IDX	(((1 << L_S20INI_RECORD_HEADER_RECORD_IDX) - 1) << S_S20INI_RECORD_HEADER_RECORD_IDX)
#define S_S20INI_RECORD_HEADER_DATA_WORDS	16
#define L_S20INI_RECORD_HEADER_DATA_WORDS	12
#define M_S20INI_RECORD_HEADER_DATA_WORDS	(((1 << L_S20INI_RECORD_HEADER_DATA_WORDS) - 1) << S_S20INI_RECORD_HEADER_DATA_WORDS)
#define S_S20INI_RECORD_HEADER_TABLE_TYPE	28
#define L_S20INI_RECORD_HEADER_TABLE_TYPE	4
#define M_S20INI_RECORD_HEADER_TABLE_TYPE	(((1 << L_S20INI_RECORD_HEADER_TABLE_TYPE) - 1) << S_S20INI_RECORD_HEADER_TABLE_TYPE)

#define FFE_INDIRECT_TABLE_RECORDS_PER_WORD	2

#define SimPrintf printf

typedef	uint8					U8_t;
typedef	uint32					U32_t;
typedef	uint64					U64_t;
typedef int						BOOL;

typedef struct {
	U32_t *ptr;
	U32_t entries;
	U32_t allocated;
	U32_t dataWords;
} S20INI_TABLE_REF;

typedef struct {
	struct S20 *s20;
	U32_t numPorts;
	U32_t tableType;
	U32_t field;
	U32_t *v;
	U32_t vLenInWords;
	U32_t vLenInBits;
	U32_t v32;
	U32_t recordIdx;
	U32_t recordBits;
	const char *argv;
	S20INI_TABLE_REF *metaDataRef;
	S20INI_TABLE_REF *dataRef;
	S20INI_TABLE_REF dataTables[INI_TYPE_DATA_TABLE_MAX];
	S20INI_TABLE_REF metaDataTables[INI_TYPE_META_DATA_TABLE_MAX];
} S20INI;

typedef struct {
	U32_t val;
	const char *text;
} S20INI_ENCODING;

struct s20iniHandler;

typedef struct {
	struct S20 *s20;
	const struct s20iniHandler *infoTable;
	const char *outputFn;
	U32_t line;
	const char *inputFn;
	int argi;
	int argc;
	int tableType; // this needs to be an int for the logic below to work correctly
	U32_t field;
	U32_t recordIdx;
	U32_t portWordsPerPort;
	U32_t wordsPerIdx;
	U32_t startBitIncrement;
	U32_t recordsPerWord;
	U32_t numPorts;
	U32_t *recordData;
	const U32_t *metaDataPtr;
	int result;
	U32_t scanIdx;
	S20INI_TABLE_REF *recordDataRef;
	S20INI_TABLE_REF metaData[INI_TYPE_DATA_TABLE_MAX]; // yes this should be INI_TYPE_DATA_TABLE_MAX
	S20INI_TABLE_REF tableData[INI_TYPE_DATA_TABLE_MAX];
	S20INI_TABLE_REF portCommon;
	BOOL portCommonCopied[256]; // assumes max of 255 ports
	char *argv[256];
	char src[2048];
	char dst[4096];
} S20INI_TOBIN;

typedef BOOL(*procFieldTextIn_t)(S20INI_TOBIN *, int idx, BOOL *insert, U32_t *value);
typedef BOOL(*procFieldTextOut_t)(S20INI *);

typedef struct s20iniHandler {
	procFieldTextIn_t procTextIn;
	procFieldTextOut_t procTextOut;
	const S20INI_ENCODING *encoding;
	const char *name;
} s20iniHandler_t;

#define CRC32_POLYNOMIAL 0x04C11DB7
#define CRC32_PRELOAD 0x46af6449
#define CRC32_REFLECT_POLYNOMIAL 0xEDB88320 // 1110-1101-1011-1000-1000-0011-0010-0000
#define CRC32_REFLECT_PRELOAD 0x9226f562    // 1001-0010-0010-0110-1111-0101-0110-0010

S20INI ini;

static U32_t crc32AddByte(U32_t crc, U8_t u8) {
	U32_t i = 8;
	do {
		crc = ((crc >> 1) | (u8 << 31)) ^ ((crc & 1) ? CRC32_REFLECT_POLYNOMIAL : 0);
		u8 >>= 1;
	} while(--i);
	return crc;
}

U32_t crc32AddBytes(U32_t crc, U32_t u32, U8_t bytes) {
	// note that we are doing this using big-endian byte ordering, since S20 is big-endian
	do {
		crc = crc32AddByte(crc, u32 >> 24);
		u32 <<= 8;
	} while(--bytes);
	return crc;
}

U32_t crc32Finish(U32_t crc) {
	crc = ~crc32AddBytes(crc, 0, 4);
	crc = (crc >> 24) | ((crc >> 8) & 0xff00) | (crc << 24) | ((crc << 8) & 0xff0000);
	return crc;
}

U32_t crc32Init() {
	return CRC32_REFLECT_PRELOAD;
}

U32_t crc32Calculate(U8_t *data, U32_t bytes) {
	U32_t crc = crc32Init();
	U32_t i;
	U32_t val;

	for(i = 0; i < bytes; i += 4) {
		val = (data[i] << 24) + (data[i + 1] << 16) + (data[i + 2] << 8) + data[i + 3];
		crc = crc32AddBytes(crc, val, 4);
	}
	return crc32Finish(crc);
}

void s20iniFieldInsert(const U32_t *metaData, U32_t *dst, U32_t entryIdx, U32_t data) {
	U32_t meta = metaData[entryIdx];
	U32_t start = (meta >> S_S20INI_META_TABLE_ENTRY_START_BIT) & ((1 << L_S20INI_META_TABLE_ENTRY_START_BIT) - 1);
	U32_t startBit = start & 0x1f;
	U32_t startIdx = start >> 5;
	U32_t bits = (meta >> S_S20INI_META_TABLE_ENTRY_BITS) & ((1 << L_S20INI_META_TABLE_ENTRY_BITS) - 1);
	U32_t mask = ((1 << bits) - 1) << startBit;
	dst[startIdx] = ((data << startBit) & mask) | (dst[startIdx] & ~mask);
}

int s20iniFieldIsolate(const U32_t *metaData, U32_t *src, U32_t entryIdx) {
	U32_t meta = metaData[entryIdx];
	U32_t start = (meta >> S_S20INI_META_TABLE_ENTRY_START_BIT) & ((1 << L_S20INI_META_TABLE_ENTRY_START_BIT) - 1);
	U32_t startBit = start & 0x1f;
	U32_t startIdx = start >> 5;
	U32_t bits = (meta >> S_S20INI_META_TABLE_ENTRY_BITS) & ((1 << L_S20INI_META_TABLE_ENTRY_BITS) - 1);
	return (src[startIdx] >> startBit) & ((1 << bits) - 1);
}

int readIniBinFile(char *fileName, U8_t *binBuf)
{
	int				totalRead = -1;
	int				nread;
	U8_t			*p;
	FILE			*fp;

	if ((fp = fopen(fileName, "r")) == NULL) {
		if (errno == ENOENT)
			fprintf(stderr, "Error opening file %s ... old style EMFW not supported\n", fileName);
		else
			fprintf(stderr, "Error opening file %s for input: %s\n", fileName, strerror(errno));
		return(-1);
	}

	p = binBuf;
	while ((nread = fread(p, 1, 1024, fp)) > 0) {
		totalRead += nread;
		p += nread;
	}

	fclose(fp);
	return(totalRead);
}

#if DEBUG_USE_FILE
int writeIniBinFile(char *fileName, U8_t *binBuf, int bufSize)
{
	int				totalWritten = 0;
	int				nwritten;
	int				chunkSize;
	U8_t			*p;
	FILE			*fp;

	if ((fp = fopen(fileName, "w")) == NULL) {
		fprintf(stderr, "Error opening file %s for output: %s\n", fileName, strerror(errno));
		return(-1);
	}

	p = binBuf;
	while (totalWritten < bufSize) {
		if ((bufSize - totalWritten) < 1024)
			chunkSize = bufSize - totalWritten;
		else
			chunkSize = 1024;
		nwritten = fwrite(p, 1, chunkSize, fp);
		totalWritten += nwritten;
		p += nwritten;
	}

	fclose(fp);
	return(totalWritten);
}
#endif

static U32_t S20iniMetaDataEntryLenGet(U32_t entry) {
	return (entry >> S_S20INI_META_TABLE_ENTRY_BITS) & ((1 << L_S20INI_META_TABLE_ENTRY_BITS) - 1);
}

static U32_t S20iniMetaDataEntryStartGet(U32_t entry) {
	return (entry >> S_S20INI_META_TABLE_ENTRY_START_BIT) & ((1 << L_S20INI_META_TABLE_ENTRY_START_BIT) - 1);
}

static U32_t S20iniMetaDataEntryProtectedGet(U32_t entry) {
	return (entry >> S_S20INI_META_TABLE_ENTRY_PROTECTED) & ((1 << L_S20INI_META_TABLE_ENTRY_PROTECTED) - 1);
}

void S20iniMetaDataDump(S20INI *ini) {
	U32_t i;
	U32_t entry;
	S20INI_TABLE_REF *tableRef = &ini->metaDataTables[ini->tableType];

	printf("Dumping table type %d\n", ini->tableType);

	for(i = 0; i < tableRef->entries; ++i) {
		entry = tableRef->ptr[i];
		printf("idx keyword %d len keyword %d start keyword %d protected keyword %d\n",
				i, S20iniMetaDataEntryLenGet(entry),
				   S20iniMetaDataEntryStartGet(entry),
				   S20iniMetaDataEntryProtectedGet(entry));
	}
}

BOOL S20iniFieldOut(S20INI *ini) {

	if (ini->vLenInBits <= 32) {
		char s[16];
		if (ini->vLenInBits <= 3) {
			strcpy(s, "field %d=%lu");
		} else {
			if (ini->vLenInBits <= 4) {
				strcpy(s, "field %d=0x%x");
			} else {
				snprintf(s, sizeof(s), "field %%d=0x%%0%dx", (int)((ini->vLenInBits + 3) >> 2));
			}
		}
		printf(s, ini->field, *ini->v);
	} else {
		printf("field %d=0x%08x", ini->field, *ini->v);
		while(--ini->vLenInWords) {
			++ini->v;
			printf("%08x", *ini->v);
		}
	}
	printf("\n");
	return FALSE;
}

BOOL S20iniFieldValue(S20INI *ini) {
	U32_t startBit;
	U32_t entry;

	entry = ini->metaDataRef->ptr[ini->field];
	ini->vLenInBits = S20iniMetaDataEntryLenGet(entry);
	if (ini->vLenInBits == 0)
		return TRUE;
	ini->vLenInWords = ((ini->vLenInBits + 31) >> 5);

	startBit = S20iniMetaDataEntryStartGet(entry) + (ini->recordIdx * ini->recordBits);

	if (ini->vLenInBits <= 32) {
		if (ini->recordBits & 31) {
			U32_t recordsPerU32 = 32 / ini->recordBits;
			U32_t modIdx = ini->recordIdx % recordsPerU32;
			startBit -= (modIdx * ini->recordBits);
			startBit += ((recordsPerU32 - modIdx - 1) * ini->recordBits);
		}
		ini->v32 = ini->dataRef->ptr[startBit >> 5];
		if (ini->vLenInBits < 32)
			ini->v32 = (ini->v32 >> (startBit & 0x1f)) & ((1 << ini->vLenInBits) - 1);
		ini->v = &ini->v32;
		ini->vLenInWords = 1;
		if ((ini->tableType == INI_TYPE_SYSTEM_TABLE) && (ini->field == SYSTEM_TABLE_FIELD_NUM_PORTS))
			ini->numPorts = ini->v32;
	} else {
		ini->v = &ini->dataRef->ptr[startBit >> 5];
		ini->vLenInWords = ((ini->vLenInBits + 31) >> 5);
	}
	return FALSE;
}

U32_t S20iniFieldValueOccurrences(S20INI *ini, U32_t value) {
	U32_t count = 0;

	for(ini->recordIdx = 0; ini->recordIdx < ini->numPorts; ++ini->recordIdx) {
		if (S20iniFieldValue(ini))
			break;
		if (ini->vLenInWords == 1)
			if (ini->v32 == value)
				++count;
	}
	return count;
}

static U32_t s20iniTableWords(S20INI_TABLE_REF *tref) {
	U32_t result = 0;
	U32_t i;
	U32_t entry;
	U32_t bits;
	U32_t val;

	for(i = 0; i < tref->entries; ++i) {
		entry = tref->ptr[i];
		bits = S20iniMetaDataEntryLenGet(entry);
		if (bits) {
			val = (S20iniMetaDataEntryStartGet(entry) + bits + 31) >> 5;
			if (val > result)
				result = val;
		}
	}
	return result;
}

void S20iniDataDump(S20INI *ini) {
	struct S20INI_COMMON {
		U32_t count;
		U32_t value;
		U32_t recordIdx;
	} *common;

	ini->recordBits = 0;
	ini->recordIdx = 0;
	ini->dataRef = &ini->dataTables[ini->tableType];

	switch(ini->tableType) {
	case INI_TYPE_SYSTEM_TABLE:
		ini->metaDataRef = &ini->metaDataTables[INI_TYPE_SYSTEM_TABLE];

		printf("dump of system data table type %d\n", ini->tableType);
		for(ini->field = 0; ini->field < ini->metaDataRef->entries; ++ini->field) {
			if (S20iniFieldValue(ini))
				continue;
			if (S20iniFieldOut(ini))
				return;
			printf("\n");
		}
		break;
	case INI_TYPE_PORT_TABLE:
		ini->metaDataRef = &ini->metaDataTables[INI_TYPE_PORT_TABLE];
		printf("dump of port data table type %d\n", ini->tableType);

		if (ini->numPorts)
			ini->recordBits = (ini->dataRef->entries / ini->numPorts) * 32;

		if (ini->metaDataRef->entries) {
			BOOL anyCommon = FALSE;
			common = (struct S20INI_COMMON *)malloc(ini->metaDataRef->entries * sizeof(*common));
			if (common != NULL) {
				memset(common, 0, ini->metaDataRef->entries * sizeof(*common));

				for(ini->field = 1; ini->field < ini->metaDataRef->entries; ++ini->field) {
					U32_t recordIdx;
					for(recordIdx = 0; recordIdx < ini->numPorts; ++recordIdx) {
						ini->recordIdx = recordIdx; // this gets corrupted by S20iniFieldValueOccurrences
						if (S20iniFieldValue(ini))
							continue;
						if (ini->vLenInWords == 1) {
							if ((common[ini->field].count == 0) || (common[ini->field].value != ini->v32)) {
								U32_t value = ini->v32;
								U32_t count = S20iniFieldValueOccurrences(ini, value);
								if ((count >= (ini->numPorts >> 2)) && (count > common[ini->field].count)) {
									common[ini->field].count = count;
									common[ini->field].value = value;
									common[ini->field].recordIdx = recordIdx;
									anyCommon = TRUE;
									if (count >= (ini->numPorts >> 1))
										break;
								}
							}
						}
					}
				}
				if (anyCommon) {
					printf("***port==Common\n");
					for(ini->field = 1; ini->field < ini->metaDataRef->entries; ++ini->field) {
						if (common[ini->field].count) {
							ini->recordIdx = common[ini->field].recordIdx;
							if (S20iniFieldValue(ini))
								continue;
							if (S20iniFieldOut(ini))
								return;
							printf("\n");
						}
					}
				}
			}
		} else
			common = NULL;

		for(ini->recordIdx = 0; ini->recordIdx < ini->numPorts; ++ini->recordIdx) {
			printf("\n");
			printf("***port=%lu\n", (long)(ini->recordIdx + 1));
			for(ini->field = 0; ini->field < ini->metaDataRef->entries; ++ini->field) {
				if (S20iniFieldValue(ini))
					continue;
				if (common && common[ini->field].count && (common[ini->field].value == ini->v32))
					continue;
				if (S20iniFieldOut(ini))
					return;
				printf("\n");
			}
		}
		if (common != NULL)
			free(common);
		break;
	case INI_TYPE_DFE_TABLE:
	case INI_TYPE_FFE_TABLE:
	case INI_TYPE_FFE_INDIRECT_TABLE:
		ini->metaDataRef = &ini->metaDataTables[ini->tableType];
		if (ini->metaDataRef->entries) {
			ini->recordBits = s20iniTableWords(ini->metaDataRef) * 32;
			if (ini->tableType == INI_TYPE_FFE_INDIRECT_TABLE)
				if (ini->recordBits)
					ini->recordBits = 32 / FFE_INDIRECT_TABLE_RECORDS_PER_WORD;

			if (ini->recordBits) {
				U32_t records = (ini->dataRef->entries * 32) / ini->recordBits;

				printf("table type %d\n\n", ini->tableType);
				for(ini->recordIdx = 0; ini->recordIdx < records; ++ini->recordIdx) {
					printf("IDX=%u", ini->recordIdx);
					for(ini->field = 0; ini->field < ini->metaDataRef->entries; ++ini->field) {
						if (S20iniFieldValue(ini))
							continue;
						printf(" ");
						if (S20iniFieldOut(ini))
							return;
					}
					printf("\n");
				}
			}
		}
		break;
	case INI_TYPE_PORT_TYPE_QSFP_TABLE:
	case INI_TYPE_PORT_TYPE_CUSTOM_TABLE:
		ini->metaDataRef = NULL;

		printf("table type %d\n\n", ini->tableType);
		ini->field = 0;

		U32_t i;
		for(i = 0; i < ini->dataRef->entries; ++i) {
			U32_t value = ini->dataRef->ptr[i];
			ini->v = &ini->v32;
			ini->vLenInWords = 1;
			ini->vLenInBits = 8;
			U32_t j;
			for(j = 0; j < 4; ++j) {
				printf("IDX_KEYWORD =%-4lu", (long)((i << 2) + j));
				ini->v32 = (value >> (24 - (j << 3))) & 0xff;
				S20iniFieldOut(ini);
				printf("\n");
			}
		}
		break;
	default:
		break;
	}
}

void S20iniBinToText(struct S20 *s20, U8_t *data, U32_t dataLen, BOOL show, const char *metaFn, const char *dataFn) {
	U32_t i = 0;
	S20INI_TABLE_REF *tableRef;

	memset(&ini, 0, sizeof(ini));

	while((i + 7) < dataLen) {
		U32_t header1;
		U32_t header2;
		U32_t dataWords;
		U32_t crc = 0;
		U32_t crcCheck;
		U32_t tableType;
		U32_t recordIdx;
		U32_t dataIdx;

		header1 = (data[i + 0] << 24) + (data[i + 1] << 16) + (data[i + 2] << 8) + data[i + 3];
		header2 = (data[i + 4] << 24) + (data[i + 5] << 16) + (data[i + 6] << 8) + data[i + 7];
		if (header1 != ~header2) {
			SimPrintf("Header check failed offset=%lu\n", (long)i);
			break;
		}
		i += 8;
		dataIdx = i;
		dataWords = (header1 & M_S20INI_RECORD_HEADER_DATA_WORDS) >> S_S20INI_RECORD_HEADER_DATA_WORDS;
		if (dataWords) {
			crcCheck = crc32Calculate(&data[i], dataWords << 2);
			i += (dataWords << 2);
			crc = (data[i + 0] << 24) + (data[i + 1] << 16) + (data[i + 2] << 8) + data[i + 3];
			i += 4;
			if (crcCheck != crc) {
				SimPrintf("crc check failed at offset %lu\n", (long)(i - 4));
				break;
			}
		}
		tableType = (header1 & M_S20INI_RECORD_HEADER_TABLE_TYPE) >> S_S20INI_RECORD_HEADER_TABLE_TYPE;
		recordIdx = (header1 & M_S20INI_RECORD_HEADER_RECORD_IDX) >> S_S20INI_RECORD_HEADER_RECORD_IDX;
		// SimPrintf("tableType=%lu recordIdx=%lu dataWords=%lu\n", (long)tableType, (long)recordIdx, (long)dataWords);
		if (recordIdx == 0) {
			if (tableType > INI_TYPE_META_DATA_TABLE_MAX) {
				SimPrintf("Unrecognized meta data tableType=%lu\n", (long)tableType);
				break;
			}
			tableRef = &ini.metaDataTables[tableType];
		} else {
			if (recordIdx == 1) {
				if (tableType >= INI_TYPE_DATA_TABLE_MAX) {
					SimPrintf("Unrecognized tableType=%lu\n", (long)tableType);
					break;
				}
				tableRef = &ini.dataTables[tableType];
				max_data_table_type = tableType + 1;
			} else {
				SimPrintf("Unrecognized recordIdx=%lu\n", (long)recordIdx);
				break;
			}
		}

		if (tableRef->ptr != NULL) {
			tableRef->ptr = NULL;
			free(tableRef->ptr);
		}

		tableRef->entries = 0;
		tableRef->ptr = 0;
		if (dataWords) {
			tableRef->dataWords = dataWords;
			if ((tableRef->ptr = (U32_t *)malloc(dataWords * sizeof(U32_t))) != NULL) {
				U32_t j;

				tableRef->entries = dataWords;
				for(j = 0; j < dataWords; ++j) {
					tableRef->ptr[j] = (data[dataIdx] << 24) + (data[dataIdx + 1] << 16) +
							(data[dataIdx + 2] << 8) + data[dataIdx + 3];
					dataIdx += 4;
				}
			} else {
				SimPrintf("Out of memory allocating dataWords=%lu\n", (long)dataWords);
				break;
			}
		}
	}

}

int buildNewIniBin(U8_t *binBuffer)
{
	U32_t header1;
	U32_t header2;
	U32_t crc = 0;
	U32_t recordIdx;
	U32_t bufSize = 0;
	U32_t *newSystemMetaTable;
	U32_t *newSystemDataTable;
	U32_t *newPortMetaTable;
	U32_t *newPortDataTable;

	U8_t *p = binBuffer;
	U8_t *q;
	U32_t n, *np;
	int i;
	int j;
	U32_t ports;
	U32_t oldPortBaseIdx;
	U32_t newPortBaseIdx;
	U32_t startBit;
	U8_t segBuffer[2048];
	int segSize;
	U32_t nodeBuf[16];
	U32_t guidBuf[2];

	memset(binBuffer, 0, INI_MAX_SIZE);

	newPortMetaTable = ini.metaDataTables[INI_TYPE_PORT_TABLE].ptr;
	newPortDataTable = ini.dataTables[INI_TYPE_PORT_TABLE].ptr;

	oldPortBaseIdx = 0;
	newPortBaseIdx = 0;
	ports = min(oldNumPorts, newNumPorts);
	for (j = 0; j < ports; j++) {
		s20iniFieldInsert(newPortMetaTable, &newPortDataTable[newPortBaseIdx], PORT_TABLE_FIELD_LINK_WIDTH_SUPPORTED,
				s20iniFieldIsolate(oldPortMetaTable, &oldPortDataTable[oldPortBaseIdx], PORT_TABLE_FIELD_LINK_WIDTH_SUPPORTED));
		s20iniFieldInsert(newPortMetaTable, &newPortDataTable[newPortBaseIdx], PORT_TABLE_FIELD_LINK_SPEED_SUPPORTED,
				s20iniFieldIsolate(oldPortMetaTable, &oldPortDataTable[oldPortBaseIdx], PORT_TABLE_FIELD_LINK_SPEED_SUPPORTED));
		s20iniFieldInsert(newPortMetaTable, &newPortDataTable[newPortBaseIdx], PORT_TABLE_FIELD_FM_ENABLED,
				s20iniFieldIsolate(oldPortMetaTable, &oldPortDataTable[oldPortBaseIdx], PORT_TABLE_FIELD_FM_ENABLED));
		s20iniFieldInsert(newPortMetaTable, &newPortDataTable[newPortBaseIdx], PORT_TABLE_FIELD_CRC,
				s20iniFieldIsolate(oldPortMetaTable, &oldPortDataTable[oldPortBaseIdx], PORT_TABLE_FIELD_CRC));
		s20iniFieldInsert(newPortMetaTable, &newPortDataTable[newPortBaseIdx], PORT_TABLE_FIELD_VCU,
				s20iniFieldIsolate(oldPortMetaTable, &oldPortDataTable[oldPortBaseIdx], PORT_TABLE_FIELD_VCU));
		s20iniFieldInsert(newPortMetaTable, &newPortDataTable[newPortBaseIdx], PORT_TABLE_FIELD_EXTERNAL_LOOPBACK_ALLOWED,
				s20iniFieldIsolate(oldPortMetaTable, &oldPortDataTable[oldPortBaseIdx], PORT_TABLE_FIELD_EXTERNAL_LOOPBACK_ALLOWED));
		oldPortBaseIdx += oldPortEntrySize;
		newPortBaseIdx += newPortEntrySize;
	}

	newSystemMetaTable = ini.metaDataTables[INI_TYPE_SYSTEM_TABLE].ptr;
	newSystemDataTable = ini.dataTables[INI_TYPE_SYSTEM_TABLE].ptr;

	// convert node description to big endian format
	for (i = 0; i < 16; i++)
		nodeBuf[i] = ntoh32(*(U32_t *)&switchNodeDesc[i << 2]);

	// insert node desc
	startBit = S20iniMetaDataEntryStartGet(newSystemMetaTable[SYSTEM_TABLE_FIELD_NODE_STRING]);
	memcpy(&newSystemDataTable[(startBit >> 5)], nodeBuf, 64);

	// convert system image guid to big endian format
	for (i = 0; i < 2; i++) {
		U32_t val;
		memcpy(&val, ((char *)&switchSystemImageGuid) + (i * 4), 4);
		guidBuf[i] = ntoh32(val);
	}

	// insert system image GUID
	startBit = S20iniMetaDataEntryStartGet(newSystemMetaTable[SYSTEM_TABLE_FIELD_SYSTEM_IMAGE_GUID]);
	memcpy(&newSystemDataTable[(startBit >> 5)], guidBuf, 8);

	// convert node guid to big endian format
	for (i = 0; i < 2; i++) {
		U32_t val;
		memcpy(&val, ((char *)&switchNodeGuid) + (i * 4), 4);
		guidBuf[i] = ntoh32(val);
	}

	// insert node GUID
	startBit = S20iniMetaDataEntryStartGet(newSystemMetaTable[SYSTEM_TABLE_FIELD_NODE_GUID]);
	memcpy(&newSystemDataTable[(startBit >> 5)], guidBuf, 8);

	//insert FM Push Button State
	s20iniFieldInsert(newSystemMetaTable, &newSystemDataTable[0], SYSTEM_TABLE_FIELD_FM_PUSH_BUTTON_STATE, fmPushButtonState);

	for(ini.tableType = 1; ini.tableType < INI_TYPE_META_DATA_TABLE_MAX; ++ini.tableType) {
		S20INI_TABLE_REF *newMetaTableRef;

		//S20iniMetaDataDump(&ini);
		memset(segBuffer, 0, 1024);
		segSize = 0;
		q = segBuffer;
		newMetaTableRef = &ini.metaDataTables[ini.tableType];
		recordIdx = 0;
		header1 = 0;
		header1 |= (newMetaTableRef->dataWords << S_S20INI_RECORD_HEADER_DATA_WORDS);
		header1 |= (ini.tableType << S_S20INI_RECORD_HEADER_TABLE_TYPE);
		header1 |= (recordIdx << S_S20INI_RECORD_HEADER_RECORD_IDX);
		header1 = ntoh32(header1);
		header2 = ~header1;
		memcpy(q, &header1, sizeof(header1));
		q += sizeof(header1);
		segSize += sizeof(header1);
		memcpy(q, &header2, sizeof(header1));
		q += sizeof(header2);
		segSize += sizeof(header2);
		memcpy(q, newMetaTableRef->ptr, newMetaTableRef->dataWords * 4);
		for (i = 0; i < newMetaTableRef->dataWords; i++) {
			np = (U32_t *)&q[i * 4];
			n = ntoh32(*np);
			memcpy(np, &n, sizeof(U32_t));
		}
		q += newMetaTableRef->dataWords * 4;
		segSize += newMetaTableRef->dataWords * 4;
		crc = crc32Calculate(&segBuffer[2*sizeof(header1)], newMetaTableRef->dataWords << 2);
		crc = ntoh32(crc);
		memcpy(q, &crc, sizeof(crc));
		q += sizeof(crc);
		segSize += sizeof(crc);
		memcpy(p, segBuffer, segSize);
		p += segSize;
		bufSize += segSize;
	}

	for(ini.tableType = 1; ini.tableType < max_data_table_type; ++ini.tableType) {
		S20INI_TABLE_REF *newDataTableRef;

		//S20iniDataDump(&ini);
		memset(segBuffer, 0, 1024);
		segSize = 0;
		q = segBuffer;
		newDataTableRef = &ini.dataTables[ini.tableType];
		recordIdx = 1;
		header1 = 0;
		header1 |= (newDataTableRef->dataWords << S_S20INI_RECORD_HEADER_DATA_WORDS);
		header1 |= (ini.tableType << S_S20INI_RECORD_HEADER_TABLE_TYPE);
		header1 |= (recordIdx << S_S20INI_RECORD_HEADER_RECORD_IDX);
		header1 = ntoh32(header1);
		header2 = ~header1;
		memcpy(q, &header1, sizeof(header1));
		q += sizeof(header1);
		segSize += sizeof(header1);
		memcpy(q, &header2, sizeof(header1));
		q += sizeof(header2);
		segSize += sizeof(header2);
		memcpy(q, newDataTableRef->ptr, newDataTableRef->dataWords * 4);
		for (i = 0; i < newDataTableRef->dataWords; i++) {
			np = (U32_t *)&q[i * 4];
			n = ntoh32(*np);
			memcpy(np, &n, sizeof(U32_t));
		}
		q += newDataTableRef->dataWords * 4;
		segSize += newDataTableRef->dataWords * 4;
		crc = crc32Calculate(&segBuffer[2*sizeof(header1)], newDataTableRef->dataWords << 2);
		crc = ntoh32(crc);
		memcpy(q, &crc, sizeof(crc));
		q += sizeof(crc);
		segSize += sizeof(crc);
		memcpy(p, segBuffer, segSize);
		p += segSize;
		bufSize += segSize;
	}

	while (bufSize & 3) {
		binBuffer[bufSize] = 0;
		++bufSize;
	}

	return(bufSize);

}

void buildEEPROMTrailer(int dataLen, uint8 *trailerBuf) {
	uint32 i;

	i = hton32(SIGNATURE);
	memcpy(trailerBuf, (char *)&i, 4);
	i = dataLen >> 2;
	i = hton32(i);
	memcpy(&trailerBuf[4], (char *)&i, 4);
	i = 0;
	memcpy(&trailerBuf[8], (char *)&i, 4);
	i = crc32Calculate(trailerBuf, 12);
	i = hton32(i);
	memcpy(&trailerBuf[12], (char *)&i, 4);
}


static void usage(char *app_name)
{
	fprintf(stderr, "usage: %s -t target ", app_name);
#if LIST_FILE_SUPPORTED
	fprintf(stderr, " [-l list_file]");
#endif
	fprintf(stderr, " [-v|-q] [-h hfi] [-o port] [-f ini_file | -d ini_directory ]");
	fprintf(stderr, "\n");
	fprintf(stderr, "   -t - target on which to update fw\n");
#if LIST_FILE_SUPPORTED
	fprintf(stderr, "   -l - file that holds a list of targets to update\n");
#endif
	fprintf(stderr, "   -v - verbose output\n");
	fprintf(stderr, "   -q - no output\n");
	fprintf(stderr, "   -h - hfi, numbered 1..n, 0= -o port will be a\n");
	fprintf(stderr, "        system wide port num (default is 0)\n");
	fprintf(stderr, "   -o - port, numbered 1..n, 0=1st active (default\n");
	fprintf(stderr, "        is 1st active)\n");
	fprintf(stderr, "   -f - ini file\n");
	fprintf(stderr, "   -d - directory for ini file\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "The -h and -o options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -o 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -o 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -o y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -o y  - HFI x, port y\n");
#if 0 /* non-documented options */
	fprintf(stderr, "   -F - do secondary EEPROM (default if primary)\n");
#endif

	exit(2);
}

int s20ReadMem(struct omgt_port *port, U32_t addr, U32_t words, uint32 *data) {
	U32_t remainingWords = words;
	int status = FSUCCESS;
	U32_t xferWords;
	uint32 *p = data;

	while(remainingWords && (status == FSUCCESS)) {
		xferWords = 16;
		if (remainingWords < xferWords)
			xferWords = remainingWords;

		status = sendMemAccessGetMad(port, &path, &mad, sessionID, addr, xferWords * 4, (U8_t *)p);
		if (status == FSUCCESS) {
			U32_t i;
			for(i = 0; i < xferWords; ++i)
				p[i] = hton32(p[i]);
			p += xferWords;
			remainingWords -= xferWords;
			addr += xferWords;
		}
	}
	return status;
}

/* offsets need to be adjusted for multi EEPROM setup */
int adjustEepromOffset(int totalOffset, uint16 *newOffset, uint32 *newLocation) {
	*newOffset = totalOffset % STL_MAX_EEPROM_SIZE;
	int numEeprom = totalOffset / STL_MAX_EEPROM_SIZE;
	switch (numEeprom) {
	case 0:
		*newLocation = g_gotSecondary ? STL_PRR_SEC_EEPROM1_ADDR : STL_PRR_PRI_EEPROM1_ADDR;
		break;
	case 1:
		*newLocation = g_gotSecondary ? STL_PRR_SEC_EEPROM2_ADDR : STL_PRR_PRI_EEPROM2_ADDR;
		break;
	case 2:
		*newLocation = g_gotSecondary ? STL_PRR_SEC_EEPROM3_ADDR : STL_PRR_PRI_EEPROM3_ADDR;
		break;
	case 3:
		*newLocation = g_gotSecondary ? STL_PRR_SEC_EEPROM4_ADDR : STL_PRR_PRI_EEPROM4_ADDR;
		break;
	default:
		*newLocation = g_gotSecondary ? STL_PRR_SEC_EEPROM1_ADDR : STL_PRR_PRI_EEPROM1_ADDR;
		numEeprom = 0;
		break;
	}
	return numEeprom;
}

uint32 getLocationOfEeprom(int eeprom) {
	switch (eeprom) {
	case 0:
		return g_gotSecondary ? STL_PRR_SEC_EEPROM1_ADDR : STL_PRR_PRI_EEPROM1_ADDR;
	case 1:
		return g_gotSecondary ? STL_PRR_SEC_EEPROM2_ADDR : STL_PRR_PRI_EEPROM2_ADDR;
	case 2:
		return g_gotSecondary ? STL_PRR_SEC_EEPROM3_ADDR : STL_PRR_PRI_EEPROM3_ADDR;
	case 3:
		return g_gotSecondary ? STL_PRR_SEC_EEPROM4_ADDR : STL_PRR_PRI_EEPROM4_ADDR;
	default:
		return g_gotSecondary ? STL_PRR_SEC_EEPROM1_ADDR : STL_PRR_PRI_EEPROM1_ADDR;
	}
}

FSTATUS EepromRW(struct omgt_port *port, IB_PATH_RECORD *path, uint16 sessionID,
	void *mad, uint8 jumbo, uint8 method, int timeout, uint32 locationDescriptor,
	uint16 dataLen, uint16 dataOffset, uint8 *data)
{
	FSTATUS				status;
	uint32				maxXfer = 128;
	uint32				remainingLen;
	uint32				xferLen;
	uint32				secondary;

	remainingLen = dataLen;
	secondary = locationDescriptor & 0x00010000;

	if (secondary && (method == MMTHD_SET))
		locationDescriptor &= 0x7fffffff;

	while (remainingLen) {
		xferLen = MIN(remainingLen, maxXfer);

		status = sendI2CAccessMad(port, path, sessionID, mad, jumbo, method, timeout, locationDescriptor, xferLen, dataOffset, data);
		if (status != FSUCCESS) {
			return(status);
		}
		dataOffset += xferLen;
		data += xferLen;
		remainingLen -= xferLen;
	}

	return FSUCCESS;
}

int main(int argc, char *argv[])
{
	const char			*opts="DFvqKBbt:l:h:o:d:m:f:T:";
	char				parameter[100];
	char				*p;
	EUI64				destPortGuid = -1;
	int					c;
	uint8				hfi = 0;
	uint8				port = 0;
	FSTATUS				status;

	int					iniBinSize;
	int					iniBinSizeOut;
	U8_t				iniBinBuf[INI_MAX_SIZE];
	U8_t				iniBinBufOut[INI_MAX_SIZE];
	opasw_ini_descriptor_get_t tableDescriptors;
	uint8				trailer[16];
	uint8				eepromBuffer[INI_MAX_SIZE];
	uint8				eepromBuffer2[INI_MAX_SIZE];
	uint32				eepromReadSize;
	uint32				crcSize;
	uint16				eepromReadOffset;
	uint32				crcOut, crcIn;
	uint8				*ep;
	uint32				eepromWriteSize;
	uint16				eepromOffset;
	uint32				locationDescriptor;
	uint32				byteCount;
	int					primary = 1;
	int					overwriteFactoryDefaults = 0;
	struct stat			fileStat;
	struct              omgt_port *omgt_port_session = NULL;
	int					currentEeprom;


	// determine how we've been invoked
	cmdName = strrchr(argv[0], '/');			// Find last '/' in path
	if (cmdName != NULL) {
		cmdName++;								// Skip over last '/'
	} else {
		cmdName = argv[0];
	}

	// parse options and parameters
	while (-1 != (c = getopt(argc, argv, opts))) {
		switch (c) {
			case 'D':
				g_debugMode = 1;
				break;

			case 't':
				errno = 0;
				strncpy(parameter, optarg, sizeof(parameter)-1);
				parameter[sizeof(parameter)-1] = 0;
				if ((p = strchr(parameter, ',')) != NULL) {
					*p = '\0';
				}
				if (FSUCCESS != StringToUint64(&destPortGuid, parameter, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid GUID: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			case 'l':
#if !LIST_FILE_SUPPORTED
				fprintf(stderr, "Error: l option is not supported at this time\n");
				exit(1);
#endif
				break;

			case 'v':
				g_verbose = 1;
				break;

			case 'q':
				g_quiet = 1;
				break;

			case 'h':
				if (FSUCCESS != StringToUint8(&hfi, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid HFI Number: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				g_gotHfi = 1;
				break;

			case 'o':
				if (FSUCCESS != StringToUint8(&port, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "%s: Error: Invalid Port Number: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				g_gotPort = 1;
				break;

			case 'f':
				g_fileParam = 1;
				strncpy(inibinFileName, optarg, FNAME_SIZE-1);
				inibinFileName[FNAME_SIZE-1] = 0;
				break;

			case 'd':
				g_dirParam = 1;
				strncpy(dirName, optarg, DNAME_SIZE-1);
				dirName[DNAME_SIZE-1]=0;
				break;

			case 'b':
				primary = 1 - primary;
				break;

			case 'B':
				overwriteFactoryDefaults = 1 - overwriteFactoryDefaults;
				break;

			case 'F':
				g_gotSecondary = 1;
				break;

			case 'T':
				if (FSUCCESS != StringToInt32(&g_respTimeout, optarg, NULL, 0, TRUE)
					|| g_respTimeout < 0) {
					fprintf(stderr, "%s: Error: Invalid delay value: %s\n", cmdName, optarg);
					usage(cmdName);
				}
				break;

			default:
				usage(cmdName);
				break;

		}
	}

	// user has requested display of help
	if (argc == 1) {
		usage(cmdName);
	}

	if (-1 == destPortGuid) {
		fprintf(stderr, "%s: Error: Must specify a target GUID\n", cmdName);
		exit(1);
	}

	if ((!g_fileParam) && (!g_dirParam)) {
		fprintf(stderr, "%s: Error: Must specify either a directory or file for the ini files\n", cmdName);
		exit(1);
	} else if ((g_fileParam) && (g_dirParam)) {
		fprintf(stderr, "%s: Error: Cannot specify both a directory and file for the ini files\n", cmdName);
		exit(1);
	}

	if (g_quiet && (g_debugMode || g_verbose)) {
		fprintf(stderr, "%s: Error: Can not specify both -q and -D|-v\n", cmdName);
		exit(1);
	}

	if (g_gotSecondary)
		primary = 0;

	locationDescriptor = primary ? STL_PRR_PRI_EEPROM1_ADDR : STL_PRR_SEC_EEPROM1_ADDR;

	// Get the path

	struct omgt_params params = {.debug_file = g_debugMode ? stderr : NULL};
	status = omgt_open_port_by_num(&omgt_port_session, hfi, port, &params);
	if (status != 0) {
		fprintf(stderr, "%s: Error: Unable to open fabric interface.\n", cmdName);
		exit(1);
	}

	if (getDestPath(omgt_port_session, destPortGuid, cmdName, &path) != FSUCCESS) {
		fprintf(stderr, "%s: Error: Failed to get destination path\n", cmdName);
		status = FERROR;
		goto err_exit;
	}

	// Send a ClassPortInfo to see if the switch is responding

	status = sendClassPortInfoMad(omgt_port_session, &path, &mad);
	if (status != FSUCCESS) {
		fprintf(stderr, "%s: Error: Failed to send/rcv ClassPortInfo\n", cmdName);
		goto err_exit;
	}

	// Get a session ID

	sessionID = getSessionID(omgt_port_session, &path);
	if (sessionID == (uint16)-1) {
		fprintf(stderr, "%s: Error: Failed to obtain sessionID\n", cmdName);
		status = FERROR;
		goto err_exit;
	}

	// Read in the inibin file and parse it

	if (g_dirParam) {
		if (chdir(dirName) < 0) {
			fprintf(stderr, "Error: cannot change directory to %s: %s\n", dirName, strerror(errno));
			if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
			goto err_exit;
		}
		strcpy(inibinFileName, PRR_INIBIN);
		if (stat("emfwMapFile", &fileStat) < 0) {
			if (errno == ENOENT) {
				strcpy(inibinFileName, OPASW_INIBIN);
			} else {
				fprintf(stderr, "Error: cannot validate emfwMapFile: %s\n", strerror(errno));
				if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
				status = FERROR;
				goto err_exit;
			}
		} else {
			getEMFWFileNames(omgt_port_session, &path, sessionID, fwFileName, inibinFileName);
		}
	}

	if (strstr(inibinFileName, ".inibin") == NULL) {
		fprintf(stderr, "Error: old style EMFW not valid with this release\n");
		if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
		status = FERROR;
		goto err_exit;
	}

	iniBinSize = readIniBinFile(inibinFileName, iniBinBuf);
	if (iniBinSize < 0) {
		if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
		goto err_exit;
	}

	S20iniBinToText(NULL, iniBinBuf, iniBinSize, 0, "metaFn", "dataFn");


	status = getNodeDescription(omgt_port_session, &path, sessionID, switchNodeDesc);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to acquire node description - status %d\n", status);
	}

	status = getFmPushButtonState(omgt_port_session, &path, sessionID, &fmPushButtonState);
        if (status != FSUCCESS) {
                fprintf(stderr, "Error: Failed to acquire FM Push Button Status - status %d\n", status);
        }

	status = getGuid(omgt_port_session, &path, sessionID, &switchSystemImageGuid, SYSTEM_IMAGE_GUID);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to acquire system image guid - status %d\n", status);
	}

	status = getGuid(omgt_port_session, &path, sessionID, &switchNodeGuid, NODE_GUID);
	if (status != FSUCCESS) {
		fprintf(stderr, "Error: Failed to acquire node guid - status %d\n", status);
	}

	status = sendIniDescriptorGetMad(omgt_port_session, &path, &mad, sessionID, &tableDescriptors);
	if (status != FSUCCESS) {
		fprintf(stderr, "%s: Error: Failed to get ini descriptors - status %d\n", cmdName, status);
		if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
		goto err_exit;
	}
	oldNumPorts = getNumPorts(omgt_port_session, &path, sessionID);
	oldPortEntrySize = tableDescriptors.portDataLen / oldNumPorts;
	newNumPorts = s20iniFieldIsolate(ini.metaDataTables[INI_TYPE_SYSTEM_TABLE].ptr,
			ini.dataTables[INI_TYPE_SYSTEM_TABLE].ptr, SYSTEM_TABLE_FIELD_NUM_PORTS),
	newPortEntrySize = ini.dataTables[INI_TYPE_PORT_TABLE].dataWords / newNumPorts;

	// fetch the port meta data
	oldPortMetaTable = (uint32 *)malloc(tableDescriptors.portMetaDataLen * sizeof(uint32));
	status = s20ReadMem(omgt_port_session, tableDescriptors.portMetaDataAddr, tableDescriptors.portMetaDataLen, oldPortMetaTable);

	// fetch the port table data
	oldPortDataTable = (uint32 *)malloc(tableDescriptors.portDataLen * sizeof(uint32));
	status = s20ReadMem(omgt_port_session, tableDescriptors.portDataAddr, tableDescriptors.portDataLen, oldPortDataTable);

	if (g_verbose) {
		printf("Port Data dump:\n");
		opaswDisplayBuffer((char *)oldPortDataTable, tableDescriptors.portDataLen * 4);
	}

	int totalEepromOffset = STL_MAX_TOTAL_EEPROM_SIZE - 256 - sizeof(trailer);
	currentEeprom = adjustEepromOffset(totalEepromOffset, &eepromOffset, &locationDescriptor);

	if (overwriteFactoryDefaults == 0) {
		status = EepromRW(omgt_port_session, &path, sessionID, (void *)&mad, NOJUMBOMAD, MMTHD_GET, 
								  g_respTimeout, locationDescriptor, sizeof(trailer),
								  eepromOffset, trailer);
		if (status != FSUCCESS) {
			fprintf(stderr, "%s: Error: Failed to get eeprom trailer - status %d\n", cmdName, status);
			if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);
			goto err_exit;
		}
		U32_t signature = ntoh32(*(U32_t *)&trailer[0]);
		U32_t len = ntoh32(*(U32_t *)&trailer[4]);
		U32_t crc = ntoh32(*(U32_t *)&trailer[12]);

		if (signature == SIGNATURE) {
			if (crc32Calculate(trailer, 12) == crc) {
				// move backwards past the factory default INI setting to the user trailer
				eepromOffset -= ((len << 2) + sizeof(trailer));
			} else {
				overwriteFactoryDefaults = 1;
				printf("INI trailer crc mismatch expected=0x%x actual=0x%x\n",
						crc32Calculate(trailer, 12), crc);
			}
		} else {
			overwriteFactoryDefaults = 1;
			printf("INI trailer signature mismatch expected=0x%x actual=0x%x\n",
						SIGNATURE, signature);
		}
	}

	// format new inibin now that we have user-settable values

	iniBinSizeOut = buildNewIniBin(iniBinBufOut);
	totalEepromOffset -= iniBinSizeOut;
	currentEeprom = adjustEepromOffset(totalEepromOffset, &eepromOffset, &locationDescriptor);
	int totalEepromReadOffset = totalEepromOffset;

	// construct the trailer

	buildEEPROMTrailer(iniBinSizeOut, trailer);

	ep = eepromBuffer;
	eepromWriteSize = 0;
	if (overwriteFactoryDefaults) {
		// invalidate the previous trailer
		memset (ep, 0xff, sizeof(trailer));
		ep += sizeof(trailer);
		eepromWriteSize = sizeof(trailer);
		totalEepromOffset -= sizeof(trailer); // will also be updating the previous trailer
		currentEeprom = adjustEepromOffset(totalEepromOffset, &eepromOffset, &locationDescriptor);
		totalEepromReadOffset = totalEepromOffset;
	}
	
	memcpy(ep, iniBinBufOut, iniBinSizeOut);
	memcpy(ep + iniBinSizeOut, trailer, sizeof(trailer));
	eepromWriteSize += iniBinSizeOut + sizeof(trailer);
	eepromReadSize = eepromWriteSize;

	if (overwriteFactoryDefaults) {
		printf("overwriting factory defaults in %s eeprom\n", primary ? "primary" : "secondary");
	} else {
		if (!primary)
			printf("overwriting configuration in secondary eeprom\n");
	}

	// calculate checkout on outgoing buffer
	crcOut = crc32Calculate(eepromBuffer, eepromWriteSize);

	// write to EEPROM in loop

#if DEBUG_USE_FILE == 0
	ep = eepromBuffer;
	status = FSUCCESS;
	byteCount = 0;
	while ((status == FSUCCESS) && eepromWriteSize) {
		U32_t xferLen = I2C_DATA_SIZE;

		if (xferLen > eepromWriteSize)
			xferLen = eepromWriteSize;

		if (byteCount < STL_MAX_EEPROM_SIZE && (xferLen + eepromOffset) > STL_MAX_EEPROM_SIZE)
			xferLen = STL_MAX_EEPROM_SIZE - eepromOffset;

		if (g_verbose)
			printf("Sending offset %d (0x%04x)\n", eepromOffset, eepromOffset);
		status = EepromRW(omgt_port_session, &path, sessionID, (void *)&mad, NOJUMBOMAD, MMTHD_SET, 
								  g_respTimeout, locationDescriptor,
								  xferLen, eepromOffset, ep);
		if (status == FSUCCESS) {
			ep += xferLen;
			eepromWriteSize -= xferLen;
			if (byteCount < STL_MAX_EEPROM_SIZE && (xferLen + eepromOffset) > STL_MAX_EEPROM_SIZE)
				locationDescriptor = getLocationOfEeprom(--currentEeprom);
			eepromOffset += xferLen;
			byteCount += xferLen;
		} else {
			/* retry ??? */
			fprintf(stderr, "%s: Error sending MAD packet to switch\n", cmdName);
		}
	}
#else
	printf("eepromOffset=0x%x eepromWriteSize=0x%x\n", eepromOffset, eepromWriteSize);
	writeIniBinFile("eeprom.bin", eepromBuffer, eepromWriteSize);
#endif

	// Read back and calculate checksum
	usleep(250000);
	ep = eepromBuffer2;
	status = FSUCCESS;
	byteCount = 0;
	crcSize = eepromReadSize;

	currentEeprom = adjustEepromOffset(totalEepromReadOffset, &eepromReadOffset, &locationDescriptor);

	while ((status == FSUCCESS) && eepromReadSize) {
		U32_t xferLen = I2C_DATA_SIZE;
		if (xferLen > eepromReadSize)
			xferLen = eepromReadSize;

		if (byteCount < STL_MAX_EEPROM_SIZE && (xferLen + eepromReadOffset) > STL_MAX_EEPROM_SIZE)
			xferLen = STL_MAX_EEPROM_SIZE - eepromReadOffset;

		if (g_verbose)
			printf("Reading offset %d (0x%04x)\n", eepromReadOffset, eepromReadOffset);

		status = EepromRW(omgt_port_session, &path, sessionID, (void *)&mad, NOJUMBOMAD, MMTHD_GET, 
								  g_respTimeout, locationDescriptor,
								  xferLen, eepromReadOffset, ep);
		if (status == FSUCCESS) {
			ep += xferLen;
			eepromReadSize -= xferLen;
			if (byteCount < STL_MAX_EEPROM_SIZE && (xferLen + eepromReadOffset) > STL_MAX_EEPROM_SIZE)
				locationDescriptor = getLocationOfEeprom(--currentEeprom);
			eepromReadOffset += xferLen;
			byteCount += xferLen;
		} else {
			/* retry ??? */
			fprintf(stderr, "%s: Error reading MAD packet to switch\n", cmdName);
		}
	}
	crcIn = crc32Calculate(eepromBuffer2, crcSize);
	printf("%s: Config block in EEPROM is %s\n", cmdName, (crcOut == crcIn) ? "valid" : "invalid");

	if (sessionID>0) releaseSession(omgt_port_session, &path, sessionID);

	printf("opaswfwconfigure completed\n");

err_exit:
	omgt_close_port(omgt_port_session);

	if (status == FSUCCESS)
		exit(0);
	else
		exit(1);

}
