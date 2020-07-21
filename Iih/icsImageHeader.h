/* BEGIN_ICS_COPYRIGHT7 ****************************************

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

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

#ifndef ICS_IMAGE_HEADER_H_INCLUDED
#define ICS_IMAGE_HEADER_H_INCLUDED

/*
  icsImageHeader.h

  This include file defines the Ics Image Header file format, which is
  designed to be prepended to a loadable image, e.g. an ELF file.

  The header is composed of records, each of which has a type field, a
  size field, and a payload.  The size field indicates the size of the
  payload in bytes.  Many record types may be defined, and may occur
  in the actual header in any order, except that the first record in
  the header must be the Info record, and the last record in the
  header must be the Terminator record.

  The endianness of the image header is the endianness of the target
  system - i.e. the endianness of the system that the attached image
  is to be loaded on.

*/


#include <stdio.h>

#if defined(VXWORKS)
#include "vxWorks.h"
#else
#include <stdint.h>
#define UINT8 uint8_t
#define UINT32 uint32_t
#endif

#define ICS_IMAGE_HEADER_RECORD_TYPE_INFO 0x49435301  /* file magic number - 'I' 'C' 'S' 0x01 */
#define ICS_IMAGE_HEADER_RECORD_TYPE_INFO_WRONG_ENDIAN 0x01534349  /* what you would see in the wrong endian */
#define ICS_IMAGE_HEADER_RECORD_TYPE_MD5  1
#define ICS_IMAGE_HEADER_RECORD_TYPE_TERMINATOR  2
#define ICS_IMAGE_HEADER_RECORD_TYPE_VXWORKSIMAGETYPE 3
#define ICS_IMAGE_HEADER_RECORD_TYPE_CBC_FILE_INFO 4
#define ICS_IMAGE_HEADER_RECORD_TYPE_MBC_FILE_INFO 5
#define ICS_IMAGE_HEADER_RECORD_TYPE_ACM_FILE_INFO 6
#define ICS_IMAGE_HEADER_RECORD_TYPE_XIO_FPGA_CFG  7
#define ICS_IMAGE_HEADER_RECORD_TYPE_Q7IMAGETYPE   8

#define ICS_IMAGE_HEADER_VERSION_SIZE 20
#define ICS_IMAGE_HEADER_FILE_VERSION_SIZE 40

#define ICS_IMAGE_HEADER_FILE_INFO_FLAG_NONE       0
#define ICS_IMAGE_HEADER_FILE_INFO_FLAG_GZIPPED    1
#define ICS_IMAGE_HEADER_FILE_INFO_FLAG_VXDEFLATED 2

#define ICS_IMAGE_HEADER_PAYLOAD_SIZE(record_name) (sizeof(IcsImageHeader_##record_name##Payload_t))
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_INFO ICS_IMAGE_HEADER_PAYLOAD_SIZE(Info)
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_MD5 ICS_IMAGE_HEADER_PAYLOAD_SIZE(Md5)
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_TERMINATOR ICS_IMAGE_HEADER_PAYLOAD_SIZE(Terminator)
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_VXWORKSIMAGETYPE ICS_IMAGE_HEADER_PAYLOAD_SIZE(VxWorksImageType)
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_CBC_FILE_INFO ICS_IMAGE_HEADER_PAYLOAD_SIZE(FileInfo)
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_MBC_FILE_INFO ICS_IMAGE_HEADER_PAYLOAD_SIZE(FileInfo)
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_ACM_FILE_INFO ICS_IMAGE_HEADER_PAYLOAD_SIZE(FileInfo)
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_XIO_FPGA_CFG ICS_IMAGE_HEADER_PAYLOAD_SIZE(XioFpgaCfgType)
#define ICS_IMAGE_HEADER_PAYLOAD_SIZE_Q7IMAGETYPE ICS_IMAGE_HEADER_PAYLOAD_SIZE(q7ImageType)

#define ICS_IMAGE_HEADER_ENUM_VXWORKSIMAGETYPE_BOOTROM     1
#define ICS_IMAGE_HEADER_ENUM_VXWORKSIMAGETYPE_LOADABLE    2
#define ICS_IMAGE_HEADER_ENUM_Q7IMAGETYPE_BIOS             3
#define ICS_IMAGE_HEADER_ENUM_Q7IMAGETYPE_BOARD_CONTROLLER 4
#define ICS_IMAGE_HEADER_ENUM_XIO_FPGA_CFG_SUBTYPE         1

typedef struct IcsImageHeaderInfoPayloadStruct {
  UINT32 productCode;
  UINT32 bspCode;
  char   version[ICS_IMAGE_HEADER_VERSION_SIZE];
  UINT32 headerSize;
  UINT32 recordCount;
  UINT32 imageSize;
} __attribute__ ((packed)) IcsImageHeader_InfoPayload_t ;


typedef UINT8 IcsImageHeader_Md5Payload_t[16];


typedef struct IcsImageHeaderTerminatorPayloadStruct {
  UINT32 recordCount;
} __attribute__ ((packed)) IcsImageHeader_TerminatorPayload_t ;


typedef UINT32 IcsImageHeader_VxWorksImageTypePayload_t;

typedef UINT32 IcsImageHeader_XioFpgaCfgTypePayload_t;

typedef UINT32 IcsImageHeader_q7ImageTypePayload_t;

typedef struct FileInfoPayloadStruct {
  UINT32 offset;
  UINT32 size;
  UINT32 flags;
  char version[ICS_IMAGE_HEADER_FILE_VERSION_SIZE];
} __attribute__ ((packed)) IcsImageHeader_FileInfoPayload_t ;


typedef struct IcsImageHeaderRecordStruct {
  UINT32 type;
  UINT32 size;
  union IcsImageHeaderRecordPayloadUnion {
    IcsImageHeader_InfoPayload_t info;
    IcsImageHeader_Md5Payload_t md5;
    IcsImageHeader_TerminatorPayload_t terminator;
    IcsImageHeader_VxWorksImageTypePayload_t vxWorksImageType;
    IcsImageHeader_FileInfoPayload_t fileInfo;
	IcsImageHeader_XioFpgaCfgTypePayload_t xioFpgaCfgType;
	IcsImageHeader_q7ImageTypePayload_t q7ImageType;
  } payload;
} __attribute__ ((packed)) IcsImageHeader_Record_t ;


#endif /* ICS_IMAGE_HEADER_H_INCLUDED */


