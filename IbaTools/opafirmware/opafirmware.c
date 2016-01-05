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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include "icsImageHeader.h"
#include "zlib.h"

#define USCORE		'_'
#define DOT			'.'
#define COLON		':'
#define SPACE		' '
#define TEE			'T'
#define NUL			'\0'

/* inquiry types */
#define SHOWVERSION 1
#define SHOWTYPE    2
#define HELP        3

/*
 * The following set of product codes and bsp codes are to be used
 * in case the firmware does not have the embedded strings for product
 * and bsp name (pre-3.1.0.1). There is no need to update the list with
 * new products and bsps, as all future embedded products will have
 * the embedded strings built in.
 */

/* product codes */
#define	VEX_PRODUCT_CODE				1
#define	VFX_PRODUCT_CODE				2
#define	INFINIFABRIC_PRODUCT_CODE		3
#define	IBX_PRODUCT_CODE				4
#define	INFINIO2000_PRODUCT_CODE		5
#define	INFINIO3000_PRODUCT_CODE		6
#define	INFINIO5000_PRODUCT_CODE		7
#define	INFINIO9000_PRODUCT_CODE		8
#define	X_PRODUCT_CODE					9
#define STL1_PRODUCT_CODE				10

/* bsp codes */
#define	SW_AUX_PRODUCT_CODE				3
#define	PCIX_PRODUCT_CODE				7
#define	MC1125_PRODUCT_CODE				9
#define	T3_PRODUCT_CODE					10
#define	XT3_PRODUCT_CODE				13
#define	N450_PRODUCT_CODE				14
#define	Q7_PRODUCT_CODE					15

#define INBUFFERSIZE					64*1024

#define PROD_SEARCH_STRING				"THIS_IS_THE_ICS_PRODUCT_NAME"
#define BSP_SEARCH_STRING				"THIS_IS_THE_ICS_BSP_NAME"

/* CSS Header definitions */

#define CSS_HEADER_SIZE					644
#define SIGNATURE_LEN					256
#define EXPONENT_LEN					4
#define MODULUS_LEN						256

typedef struct cssHeader_s {
	uint32_t moduleType;
	uint32_t headerLen;
	uint32_t headerVersion;
	uint32_t moduleID;
	uint32_t moduleVendor;
	uint32_t date;
	uint32_t size;
	uint32_t keySize;
	uint32_t modulusSize;
	uint32_t exponentSize;
	uint32_t reserved[22];
	uint8_t  modulus[MODULUS_LEN];
	uint8_t  exponent[EXPONENT_LEN];
	uint8_t  signature[SIGNATURE_LEN];
} cssHeader_t;

#define CSS_MODULE_TYPE					6
#define CSS_HEADER_LEN					0xA1
#define CSS_HEADER_VER					0x10000
#define CSS_DBG_MODULE_ID				0x80000000
#define CSS_PRD_MODULE_ID				0
#define CSS_MODULE_VENDOR				0x8086
#define CSS_KEY_SIZE					0x40
#define CSS_MODULUS_SIZE				0x40
#define CSS_EXPONENT_SIZE				1

char *cmpBuffer = NULL;
char *appBuffer = NULL;
int bufSize;

int cmpBufferSize;
int appBufferSize;

/*
 * the follwing set of functions is borrowed from icsImageUtil
 * in devtools (some of have been modifed slightly)
 */

#define ERR_OK							0
#define ERR_IO_ERROR					-1
#define ERR_INVALID						-2

#define gen_cpu_bswap32(what) ((((uint32_t)what) >> 24) | (((uint32_t)what) << 24) | ((((uint32_t)what) >> 8) & 0xff00) | ((((uint32_t)what) << 8) & 0xff0000))

struct
{
	int hostBigEndian;
	int targetBigEndian;
} g_state;

char g_ioBuffer[INBUFFERSIZE];

int isHostBigEndian (void)
{
	uint32_t	probeValue = 0x12345678;
	char		*probePtr;

	probePtr = (char *)&probeValue;
	if (0x12 == *probePtr)
	{
		return(1);
	} else
		return(0);
}

/*
  initializeState - initializes the global state for the program.  this has to be
                    called before any other routines are called.
*/
void initializeState (void)
{
	g_state.hostBigEndian = isHostBigEndian();
	g_state.targetBigEndian = 1;
}

uint32_t toHostEndian32 (uint32_t value)
{
	if (g_state.hostBigEndian != g_state.targetBigEndian)
		return (gen_cpu_bswap32(value));
	else
		return (value);
}

/*
  getRecord - retrieves the next record from the given file.  it is assumed
              the the file pointer is positioned at the start of a valid
			  record when this routine is called.  the record is stored in 
			  the supplied record buffer, and the type and size fields are
			  converted to the endian of the host.
*/
int getRecord (FILE *f, IcsImageHeader_Record_t *recordBuffer)
{
	int			result;
	uint32_t	recordType;
	uint32_t	payloadSize;

	result = fread(&recordType, 4, 1, f);
	if (0 == result)
	{
		fprintf(stderr, "Error reading record type\n");
		return(ERR_IO_ERROR);
	}
  
	result = fread(&payloadSize, 4, 1, f);
	if (0 == result)
	{
		fprintf(stderr, "Error reading record size\n");
		return(ERR_IO_ERROR);
	}
  
	result = fread(&(recordBuffer->payload), toHostEndian32(payloadSize), 1, f);
	if (0 == result)
	{
		fprintf(stderr, "Error reading record payload\n");
		return(ERR_IO_ERROR);
	}

	recordBuffer->type = toHostEndian32(recordType);
	recordBuffer->size = toHostEndian32(payloadSize);

	return(ERR_OK);
}

int checkCssHeader (uint8_t *buf)
{
	int							isSpkg = 0;
	cssHeader_t					*cssp;

	cssp = (cssHeader_t *)buf;
	if (cssp->moduleType    == CSS_MODULE_TYPE &&
		cssp->headerLen     == CSS_HEADER_LEN &&
		cssp->headerVersion == CSS_HEADER_VER &&
		cssp->moduleVendor  == CSS_MODULE_VENDOR &&
		(cssp->moduleID     == CSS_DBG_MODULE_ID || cssp->moduleID == CSS_PRD_MODULE_ID) &&
		cssp->keySize       == CSS_KEY_SIZE &&
		cssp->modulusSize   == CSS_MODULUS_SIZE ) {
		isSpkg = 1;
    }

	return(isSpkg);
}

/*
  removeHeader - removes the ICS image header from the given file, if it has one
*/
int removeHeader (char *inputFileName, char *outputBuffer)
{
	uint32_t					imageHeaderSize;
	FILE						*inputFp;
	char						*p;
	IcsImageHeader_Record_t		recordBuffer;
	uint32_t					bytesRead;
	uint32_t					firstRecordType;
	uint8_t						cssBuf[CSS_HEADER_SIZE + 10];
	int							isSpkg = 0;

	cmpBufferSize = 0;
 
	inputFp = fopen(inputFileName, "rb");
	if (NULL == inputFp)
	{
		return(ERR_IO_ERROR);
	}

	if (0 == fread(&cssBuf, CSS_HEADER_SIZE, 1, inputFp))
	{
		fclose(inputFp);
		return(ERR_IO_ERROR);
	}

	isSpkg = checkCssHeader(cssBuf);

	fseek(inputFp, isSpkg ? CSS_HEADER_SIZE : 0, SEEK_SET);

	if (0 == fread(&firstRecordType, 4, 1, inputFp))
	{
		fclose(inputFp);
		return(ERR_IO_ERROR);
	}

	p = outputBuffer;

	fseek(inputFp, isSpkg ? CSS_HEADER_SIZE : 0, SEEK_SET);

	if (ICS_IMAGE_HEADER_RECORD_TYPE_INFO != toHostEndian32(firstRecordType))
	{
		fclose(inputFp);
		return(ERR_INVALID);
	}

	if (ERR_OK != getRecord(inputFp, &recordBuffer))
	{
		fclose(inputFp);
		return(ERR_INVALID);
	}
  
	imageHeaderSize = toHostEndian32(recordBuffer.payload.info.headerSize);
	fseek(inputFp, isSpkg ? CSS_HEADER_SIZE + imageHeaderSize : imageHeaderSize, SEEK_SET);

	while (0 != (bytesRead = fread(g_ioBuffer, 1, INBUFFERSIZE, inputFp)))
	{
		memcpy(p, g_ioBuffer, bytesRead);
		p += bytesRead;
		cmpBufferSize += bytesRead;
	}

	fclose(inputFp);
	return(ERR_OK);
}

/* end of borrowed stuff from icsImageUtil in devtools */


void copyAndReplace (char *src, char *dest, char from, char to)
{
	char *pSrc;
	char *pDest;

	for (pSrc = src, pDest = dest; *pSrc != NUL; pSrc++, pDest++)
	{
		if (*pSrc == from)
			*pDest = to;
		else
			*pDest = *pSrc;
	}
	*pDest = NUL;
	return;
}

void displayVersion(char *firmwareVersion)
{
	/* display the version - pass to copyAndReplace to make a dot-separated string */

	char outputVersion[ICS_IMAGE_HEADER_VERSION_SIZE];

	copyAndReplace(firmwareVersion, outputVersion, USCORE, DOT);

	printf("%s\n", outputVersion);
}

void displayType(uint32_t firmwareType, uint32_t bspType)
{
	/* display the type, based on type code */

	switch(firmwareType)
	{
		case VEX_PRODUCT_CODE:
			printf("VEx.");
			break;
		case VFX_PRODUCT_CODE:
			printf("VFx.");
			break;
		case INFINIFABRIC_PRODUCT_CODE:
			printf("InfiniFabric.");
			break;
		case IBX_PRODUCT_CODE:
			printf("IBx.");
			break;
		case INFINIO2000_PRODUCT_CODE:
			printf("InfinIO2000.");
			break;
		case INFINIO3000_PRODUCT_CODE:
			printf("InfinIO3000.");
			break;
		case INFINIO5000_PRODUCT_CODE:
			printf("InfinIO5000.");
			break;
		case INFINIO9000_PRODUCT_CODE:
			printf("InfinIO9000.");
			break;
		case X_PRODUCT_CODE:
			printf("X.");
			break;
		case STL1_PRODUCT_CODE:
			printf("Intel_Omni_Path_Fabric_Switches_STL1_Series.");
			break;
		default:
			fprintf(stderr, "Unknown product code.");
			break;
	}

	switch(bspType)
	{
		case SW_AUX_PRODUCT_CODE:
			printf("sw_aux\n");
			break;
		case PCIX_PRODUCT_CODE:
			printf("pcix\n");
			break;
		case MC1125_PRODUCT_CODE:
			printf("mc1125\n");
			break;
		case T3_PRODUCT_CODE:
			printf("t3\n");
			break;
		case XT3_PRODUCT_CODE:
			printf("xt3\n");
			break;
		case N450_PRODUCT_CODE:
			printf("n450\n");
			break;
		case Q7_PRODUCT_CODE:
			printf("q7\n");
			break;
		default:
			fprintf(stderr, "Unknown bsp code\n");
			break;
	}

	return;
}

#define CHECK_ERR(err, msg) \
{ \
	if (err != Z_OK) \
	{ \
		fprintf(stderr, "%s error: %d\n", msg, err); \
		if (inBuffer) free(inBuffer); \
		if (outBuffer) free(outBuffer); \
		return(ERR_INVALID); \
	} \
}

int doInflate(char *smallBuffer, char *bigBuffer)
{
	unsigned char			*inBuffer = NULL;
	unsigned char			*outBuffer = NULL;
	char					*smallBufferP;
	char					*bigBufferP;
	int						inBufferSize;
	int						outBufferSize;
	int						bytesToGo = cmpBufferSize;
	int						bytesCopied = 0;
	int						bytesToProcess;
	int						err;
	int						done = 0;
	z_stream				decompStream;

	inBufferSize = INBUFFERSIZE;
	outBufferSize = INBUFFERSIZE;

	if ((inBuffer = (unsigned char *)malloc(inBufferSize)) == NULL) {
		fprintf(stderr, "Error allocating memory for input buffer\n");
		goto memerr;
	}
	if ((outBuffer = (unsigned char *)malloc(outBufferSize)) == NULL) {
		fprintf(stderr, "Error allocating memory for output buffer\n");
		goto memerr;
	}

	smallBufferP = smallBuffer + 1; /* move past magic number */
	bigBufferP = bigBuffer;

	decompStream.zalloc = (alloc_func)0;
	decompStream.zfree = (free_func)0;
	decompStream.opaque = (voidpf)0;

	err = inflateInit(&decompStream);
	CHECK_ERR(err, "inflateInit");

	bytesToProcess = (bytesToGo > inBufferSize) ? inBufferSize : bytesToGo;
	memcpy(inBuffer, smallBufferP, bytesToProcess);
	bytesToGo -= bytesToProcess;
	smallBufferP += bytesToProcess;

	decompStream.next_in  = inBuffer;
	decompStream.next_out  = outBuffer;

	while (!done)
	{
		decompStream.avail_in = bytesToProcess;
		decompStream.avail_out = outBufferSize;

		while(decompStream.avail_in)
		{
			err = inflate(&decompStream, Z_NO_FLUSH);
			if (err == Z_STREAM_END)
				break;
			CHECK_ERR(err, "inflate");

			memcpy(bigBufferP, outBuffer, decompStream.next_out - outBuffer);
			bigBufferP += decompStream.next_out - outBuffer;
			bytesCopied += decompStream.next_out - outBuffer;

			decompStream.next_out = outBuffer;
			decompStream.avail_out = outBufferSize;
		}

		bytesToProcess = (bytesToGo > inBufferSize) ? inBufferSize : bytesToGo;
		if (bytesToProcess == 0)
			done = 1;
		else
		{
			memcpy(inBuffer, smallBufferP, bytesToProcess);
			bytesToGo -= bytesToProcess;
			smallBufferP += bytesToProcess;

			decompStream.next_in  = inBuffer;
			decompStream.next_out = outBuffer;
		}
	}

	err = inflateEnd(&decompStream);
	CHECK_ERR(err, "inflateEnd");

	free(inBuffer);
	free(outBuffer);

	appBufferSize = bytesCopied;

	return(ERR_OK);

memerr:
	if (inBuffer != NULL)
		free(inBuffer);
	if (outBuffer != NULL)
		free(outBuffer);
	return(Z_MEM_ERROR);
}

int displayEmbeddedProdAndBsp(void)
{
	int							result;
	int							returnVal = 0;
	char						*p;
	char						*p1, *p2;
	char						productName[64];
	char						bspName[64];
	int							prodStringFound = 0;
	int							bspStringFound = 0;

/*
 * First, perform the inflate from the compressed buffer to the
 * application buffer
 */

	if ((result = doInflate(cmpBuffer, appBuffer)) != ERR_OK)
		return(returnVal);

/* 
 * In the application buffer, look for both search patterns so
 * as to locate the Product and BSP names. Use memchr to find the
 * first character ('T'), then compare rest of string. If not found,
 * resume search for next occurrence of 'T'.
 */

	p = memchr(appBuffer, TEE, appBufferSize);
	if (p == NULL) {
		returnVal = 0;
		goto fail;
	}
	while (!prodStringFound && (p != NULL))
	{
		if (memcmp(p, PROD_SEARCH_STRING, strlen(PROD_SEARCH_STRING)) == 0)
		{
			prodStringFound = 1;
			returnVal = 1;
			p1 = strchr(p, COLON) + 1;
			if (p1 == NULL) {
				returnVal = 0;
				goto fail;
			}
			for (p2 = productName; (*p1 != SPACE) && (*p1 != NUL); p1++, p2++)
				*p2 = *p1;
			*p2 = NUL;
		}
		else
		{
			p = memchr(p + 1, TEE, appBufferSize - (p - appBuffer));
		}
	}
	if (returnVal)
	{
		p = memchr(appBuffer, TEE, appBufferSize);

		while (p && !bspStringFound)
		{
			if (memcmp(p, BSP_SEARCH_STRING, strlen(BSP_SEARCH_STRING)) == 0)
			{
				bspStringFound = 1;
				p1 = strchr(p, COLON) + 1;
				if (p1 == NULL) {
					returnVal = 0;
					goto fail;
				}
				for (p2 = bspName; (*p1 != SPACE) && (*p1 != NUL); p1++, p2++)
					*p2 = *p1;
				*p2 = NUL;
			}
			else
			{
				p = memchr(p + 1, TEE, appBufferSize - (p - appBuffer));
			}
		}
		if (!bspStringFound)
			returnVal = 0;
		else
			printf("%s.%s\n", productName, bspName);
	}

fail:
	return(returnVal);
}

void Usage(void)
{
	fprintf(stderr, "Usage: opafirmware [--help] [--showVersion|--showType firmwareFile]\n");
	fprintf(stderr, "    --help - full help text\n");

	exit(1);
}

void Usage_full(void)
{
	fprintf(stderr, "Usage: opafirmware [--help] [--showVersion|--showType firmwareFile]\n");
	fprintf(stderr, "    --help - full help text\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "  --showVersion STL1.q7.10.0.0.0.spkg\n");
	fprintf(stderr, "      10.0.0.0\n");
	fprintf(stderr, "  --showType STL1.q7.10.0.0.0.spkg\n");
	fprintf(stderr, "      Omni_Path_Switch_Products.q7\n");

	exit(0);
}

int main(int argc, char *argv[])
{
	char						*firmwareFileName;
	FILE						*fpFirmwareFile;
	int							inquiryType;
	int							firstRecordType;
	int							result;
	int							fileSize;
	int							isSpkg = 0;
	struct stat					statBuf;
	IcsImageHeader_Record_t		recordBuffer;
	uint8_t						cssBuf[CSS_HEADER_SIZE + 10];

	struct option longopts[] = 
	{
		{ "showVersion", no_argument, &inquiryType, SHOWVERSION },
		{ "showType",    no_argument, &inquiryType, SHOWTYPE    },
		{ "help",        no_argument, &inquiryType, HELP        },
		{ 0,             0,           0           , 0           }
	};

	/* parse options for inquiry type */
	if ((result = getopt_long(argc, argv, "", longopts, NULL)))	// assignment, not ==
		Usage();

	if (inquiryType == HELP)
		Usage_full();

	if (argc != 3)
		Usage();

	/* set up endian-ness */

	initializeState();

	/* stat file to get size and allocate buffers */

	firmwareFileName = argv[2];

	if (stat(firmwareFileName, &statBuf) < 0) {
		fprintf(stderr, "Error taking stat of file {%s}: %s\n",
			firmwareFileName, strerror(errno));
		exit(1);
	}

	fileSize = (int)statBuf.st_size;
	bufSize = fileSize + 1024; /* pad by 1K to be safe */
	if ((cmpBuffer = malloc(bufSize)) == NULL) {
		fprintf(stderr, "Error allocating memory for firmware buffer\n");
		goto fail;
	}
	if ((appBuffer = malloc(bufSize * 3)) == NULL) {
		fprintf(stderr, "Error allocating memory for inflated firmware buffer\n");
		goto fail;
	}

	memset(cmpBuffer, 0, bufSize);
	memset(appBuffer, 0, bufSize * 3);

	/* open file */
 
	if ((fpFirmwareFile = fopen(firmwareFileName, "rb")) == NULL)
	{
		fprintf(stderr, "Error opening file {%s} for input: %s\n",
			firmwareFileName, strerror(errno));
		goto fail;
	}

	if (0 == fread(&cssBuf, CSS_HEADER_SIZE, 1, fpFirmwareFile))
	{
		fprintf(stderr, "Error reading file {%s}: %s\n",
			firmwareFileName, strerror(errno));
		goto fail;
	}

	isSpkg = checkCssHeader(cssBuf);

	/* get first record, check if ICS image file */

	fseek(fpFirmwareFile, isSpkg ? CSS_HEADER_SIZE : 0, SEEK_SET);

	if (0 == fread(&firstRecordType, 4, 1, fpFirmwareFile))
	{
		fprintf(stderr, "Error reading file {%s}: %s\n",
			firmwareFileName, strerror(errno));
		goto fail;
	}

	fseek(fpFirmwareFile, isSpkg ? CSS_HEADER_SIZE : 0, SEEK_SET);

	if (ICS_IMAGE_HEADER_RECORD_TYPE_INFO != toHostEndian32(firstRecordType))
	{
		fprintf(stderr, "This is not a valid firmware image file\n");
		goto fail;
	}

	/* read in header, display what is asked for */

	if (ERR_OK != getRecord(fpFirmwareFile, &recordBuffer))
		goto fail;

	switch (inquiryType)
	{
		case SHOWVERSION:
			displayVersion(recordBuffer.payload.info.version);
			break;
		case SHOWTYPE:
			/*
			 * For the type, we really want to look for the embedded strings first.
			 * Call removeHeader, which will remove the ICS header from the package
			 * file and place it in the compressed buffer. Then call
			 * displayEmbeddedProdAndBsp which inflates and then searches. If the search
			 * fails, it is a pre-3.1.0.1 release, so feed the codes from the ICS header
			 * to displayType.
			 */
			result = removeHeader(argv[2], cmpBuffer);
			if (!displayEmbeddedProdAndBsp()) /* if this fails, try old way */
				displayType(toHostEndian32(recordBuffer.payload.info.productCode),
			    	        toHostEndian32(recordBuffer.payload.info.bspCode));
			break;
		default:
			fprintf(stderr, "Unknown inquiry type\n");
			goto fail;
	}

	fclose(fpFirmwareFile);
fail:
	if (cmpBuffer != NULL)
		free(cmpBuffer);
	if (appBuffer != NULL)
		free(appBuffer);
	exit(0);
}
