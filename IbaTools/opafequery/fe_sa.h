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

#ifndef FE_SA_H
#define FE_SA_H

#include "fe_connections.h"
#include "iba/stl_sd.h"
#include "iba/ib_generalServices.h"

extern IB_GID g_sourceGid;
extern uint32_t g_gotSourceGid;

FSTATUS fe_buildSAQueryResults(PQUERY_RESULT_VALUES* ppQueryResults, SA_MAD **ppRsp, uint32_t record_size, uint32_t response_length);
FSTATUS fe_saQueryCommon(struct net_connection *connection, uint32_t msgID, SA_MAD *send_mad, SA_MAD **recv_mad, PQUERY_RESULT_VALUES* ppQueryResults, uint32_t record_size);
FSTATUS fe_processSAQuery(PQUERY pQuery, struct net_connection *connection, PQUERY_RESULT_VALUES* ppQueryResults);

/* Calculate 8-byte multiple padding for multi record SA responses */
#define Calculate_Padding(RECSIZE)  ( (((RECSIZE)%8) == 0 ) ? 0 : (8 - ((RECSIZE)%8))  )

#endif
