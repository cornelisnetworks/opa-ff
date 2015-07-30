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

  \file dsc_module.c
 
  $Author: aestrin $
  $Revision: 1.27 $
  $Date: 2015/01/27 23:00:31 $

  \brief Top level Linux module management routines.
*/
/*
  ICS IBT stuff
*/
#include "iba/ibt.h"
#include "iba/public/isemaphore.h"

/*
  Module stuff
*/
#include "dsc_module.h"
#include "dsc_topology.h"
#include "dsc_hca.h"
#include "opasadb_debug.h"
#include "dsc_notifications.h"
#include <infiniband/umad.h>
#include <infiniband/verbs.h>

// Module parameters

uint64_t DestPortGuid = 0;
uint32 WaitFirstSweep = 0;	// wait for first sweep in module load

uint32 Dbg = _DBG_LVL_NOTICE;

/*
  IB data
*/
char 	dsc_ca_names[DSC_MAX_HCAS][UMAD_CA_NAME_LEN];
int 	dsc_ca_count;


/*!
  \brief Release all globally allocated resources. Called by the
  system when rmmod is called or the system is shut down.
*/

void dsc_cleanup(void)
{
	_DBG_FUNC_ENTRY;
	_DBG_PRINT("Unloading Distributed SA.\n");

	dsc_terminate_async_event_handler();
	dsc_terminate_notifications();
	dsc_scanner_cleanup();
	dsc_hca_cleanup();
	dsc_topology_cleanup();

	_DBG_FUNC_EXIT;
}

/*!
  \brief Loads the module into kernel space and does global initializations. 
  Called by insmod or modprobe.
*/

FSTATUS
dsc_init(void)
{
	FSTATUS fstatus = FERROR;
	int i;

	_DBG_FUNC_ENTRY;

	_DBG_PRINT("Initializing Distributed SA.\n");

	dsc_ca_count = umad_get_cas_names(dsc_ca_names,DSC_MAX_HCAS);
	if (dsc_ca_count == 0) {
		_DBG_ERROR("Cannot get hcas.\n");
		fstatus = FERROR;
		goto exit;
	}

	for (i=0;i<dsc_ca_count;i++) {
		_DBG_INFO("HFI %d: %s\n", i, dsc_ca_names[i]);
	}

	fstatus = dsc_topology_init();
	if (fstatus != FSUCCESS)
		goto exit;

	fstatus = dsc_hca_init();
	if (fstatus != FSUCCESS)
		goto exit;

	fstatus = dsc_initialize_async_event_handler();
	if (fstatus != FSUCCESS)
		goto exit;

	fstatus = dsc_scanner_init();
	if (FSUCCESS != fstatus)
		goto exit;

	/* Guard against scans without timeout */
	if (UnsubscribedScanFrequency == 0) {
		UnsubscribedScanFrequency = 10;
	}

	if (!NoSubscribe && (dsc_notifications_register() != FSUCCESS)) {
		_DBG_ERROR("Failed to register for notifications!\n");
		fstatus = FERROR;
		goto exit;
	}

	fstatus = dsc_scanner_start(WaitFirstSweep);

exit:
	_DBG_FUNC_EXIT;
	return fstatus;
}
