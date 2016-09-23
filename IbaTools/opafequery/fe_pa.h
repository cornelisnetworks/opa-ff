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

#ifndef FE_PA_INCLUDE
#define FE_PA_INCLUDE

#include "ib_types.h"
#include "ib_pa.h"

#include "fe_connections.h"
#include "iba/ib_generalServices.h"

extern int g_verbose;

STL_CLASS_PORT_INFO *
fe_pa_classportinfo_response_query(struct net_connection *connection);
FSTATUS fe_GetGroupList(struct net_connection *connection);
FSTATUS fe_GetGroupInfo(struct net_connection *connection, char *groupName, uint64 imageNumber, int32 imageOffset, uint32 imageTime);
FSTATUS fe_GetGroupConfig(struct net_connection *connection, char *groupName, uint64 imageNumber, int32 imageOffset, uint32 imageTime);
FSTATUS fe_GetPortCounters(struct net_connection *connection, uint32_t nodeLid, uint8_t portNumber, uint32_t deltaFlag, uint32_t userCtrsFlag, uint64 imageNumber, int32 imageOffset, uint32_t begin, uint32_t end);
FSTATUS fe_ClrPortCounters(struct net_connection *connection, uint32_t nodeLid, uint8_t portNumber, uint32_t selectFlag);
FSTATUS fe_ClrAllPortCounters(struct net_connection *connection, uint32_t selectFlag);
FSTATUS fe_GetPMConfig(struct net_connection *connection);
FSTATUS fe_FreezeImage(struct net_connection *connection, uint64 imageNumber, int32 imageOffset, uint32 imageTime);
FSTATUS fe_ReleaseImage(struct net_connection *connection, uint64 imageNumber, int32 imageOffset);
FSTATUS fe_RenewImage(struct net_connection *connection, uint64 imageNumber, int32 imageOffset);
FSTATUS fe_MoveFreeze(struct net_connection *connection, uint64 imageNumber, int32 imageOffset, uint64 moveImageNumber, int32 moveImageOffset);
FSTATUS fe_GetFocusPorts(struct net_connection *connection, char *groupName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset, uint32 imageTime);
FSTATUS fe_GetImageInfo(struct net_connection *connection, uint64 imageNumber, int32 imageOffset, uint32_t imageTime);
STL_CLASS_PORT_INFO * fe_GetClassPortInfo(struct net_connection *connection);
FSTATUS fe_GetVFList(struct net_connection *connection);
FSTATUS fe_GetVFInfo(struct net_connection *connection, char *vfName, uint64 imageNumber, int32 imageOffset, uint32 imageTime);
FSTATUS fe_GetVFConfig(struct net_connection *connection, char *vfName, uint64 imageNumber, int32 imageOffset, uint32 imageTime);
FSTATUS fe_GetVFPortCounters(struct net_connection *connection, uint32_t nodeLid, uint8_t portNumber, uint32_t deltaFlag, uint32_t userCntrsFlag, char *vfName, uint64 imageNumber, int32 imageOffset, uint32_t begin, uint32_t end);
FSTATUS fe_ClrVFPortCounters(struct net_connection *connection, uint32_t nodeLid, uint8_t portNumber, uint32_t selectFlag, char *vfName);
FSTATUS fe_GetVFFocusPorts(struct net_connection *connection, char *vfName, uint32 select, uint32 start, uint32 range, uint64 imageNumber, int32 imageOffset, uint32 imageTime);

#endif /* FE_PA_INCLUDE */
