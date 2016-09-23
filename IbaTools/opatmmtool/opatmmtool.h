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

#ifndef _ADMIN_H
#define _ADMIN_H

typedef enum {
	E_ADMIN_CMD_GET_VERSION,
	E_ADMIN_CMD_FIRMWARE_UPDATE,
	E_ADMIN_CMD_REBOOT,
	E_ADMIN_CMD_SYS_STATUS,
	E_ADMIN_CMD_READ_MEM,
	E_ADMIN_CMD_WRITE_OTP,
	E_ADMIN_CMD_LOCK_OTP
} adminCmd_t;

typedef enum {
	OP_TMM_REBOOT,
	OP_TMM_FWVER,
	OP_TMM_FILEVER,
	OP_TMM_UPDATE,
	OP_TMM_DUMP,
	OP_TMM_WRITE,
	OP_TMM_LOCK,
	OP_TMM_STATUS
} tmmOps_t;

#define SLV_ADDR 0x96
#define VERSION_LEN 20
#define CHUNK_SIZE 252
#define I2C_FILE_1 "/sys/kernel/debug/hfi1/hfi1_"
#define I2C_FILE_2 "/i2c1"
#define I2C_FILE_SIZE 40
#define RETRIES 3
#define OTP_SIZE 528
#define BYTES_OFFSET 1
#define	I2C_RETRIES 5

// TODO: find out the actual values
#define SYS_STATUS_NONE -1
#define SYS_STATUS_OK 0

#endif 
