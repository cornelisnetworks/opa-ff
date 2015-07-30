/* BEGIN_ICS_COPYRIGHT4 ****************************************

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

** END_ICS_COPYRIGHT4   ****************************************/
/*!

  \file dsc_hca.h
 
  $Author: mwheinz $
  $Revision: 1.12 $
  $Date: 2015/01/22 18:04:10 $

  \brief Routines for querying HFIs
*/
#if !defined(_DSC_HCA_H_)
#define _DSC_HCA_H_

#include <infiniband/umad.h>
#include <infiniband/verbs.h>

#include "iba/ibt.h"

#if defined(__cplusplus)
extern "C" {
#endif

extern int hca_count;
extern struct ibv_device **device_list;
extern struct ibv_context **hca_context_list;

void dsc_put_local_port(umad_port_t *port);
umad_port_t* dsc_get_local_port(union ibv_gid *src_gid);

void dsc_put_local_ports(umad_port_t *ports, uint32_t port_count);
umad_port_t* dsc_get_local_ports(uint32_t *port_count);

FSTATUS dsc_initialize_async_event_handler(void);
void dsc_terminate_async_event_handler(void);

char* dsc_get_hca_name(uint16_t hca_index);

void dsc_hca_cleanup(void);
FSTATUS dsc_hca_init(void);

struct oib_port * dsc_get_oib_session(uint64_t port_guid);

#if defined(__cplusplus)
}
#endif

#endif
