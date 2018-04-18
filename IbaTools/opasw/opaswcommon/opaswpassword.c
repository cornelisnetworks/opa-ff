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
#include <unistd.h>
#include <getopt.h>
#include <limits.h>

/* work around conflicting names */

#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include "opamgt_priv.h"
#include <iba/ibt.h>
#include <openssl/md5.h>
#include "opaswcommon.h"

int vkey_prompt_user(uint8 *vendorKey, int vkeyChange)
{
	int					status = 1;
	MD5_CTX				state;
	char				*password;
	char				password1[65];
	char				password2[65];
	const char			*psw1Prompt = (vkeyChange) ? "Enter new password:" : "Enter password:";
	const char			*psw2Prompt = (vkeyChange) ? "Re-enter new password:" : "Re-enter password:";

	memset(password1, 0, sizeof(password1));
	memset(password2, 0, sizeof(password2));

	if ( !vkeyChange && ((password = getenv("opaswpassword")) != NULL)) {
		strncpy(password1, password, sizeof(password1) - 1);
	} else if ( vkeyChange && ((password = getenv("xedge_save_password")) != NULL)) {
		strncpy(password1, password, sizeof(password1) - 1);
	} else {
		if ((password = getpass(psw1Prompt)) == NULL) {
			fprintf(stderr, "Unable to get password\n");
			status = 0;
		}

		if (status) {
			strncpy(password1, password, sizeof(password1) - 1);
			if ((password = getpass(psw2Prompt)) == NULL) {
				fprintf(stderr, "Unable to get re-entered password\n");
				status = 0;
			}
		}
	}

	if (status) {
		strncpy(password2, password, sizeof(password2) - 1);
		if (strcmp(password1, password2) == 0) {
			if (strlen(password1) == 0 && strlen(password2) == 0) {
				memset(vendorKey, 0, MAX_VENDOR_KEY_LEN);
			} else {
				MD5_Init(&state);
				MD5_Update(&state, (uint8 *)password1, strlen(password1));
				MD5_Final(vendorKey, &state);
			}
		} else {
			fprintf(stderr, "Password entries do not match\n");
			status = 0;
		}
	}

	return(status);
}
