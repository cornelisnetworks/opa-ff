/* BEGIN_ICS_COPYRIGHT2 ****************************************

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

** END_ICS_COPYRIGHT2   ****************************************/

/* [ICS VERSION STRING: unknown] */
#include <string.h>
#include <linux/types.h>
#include <iba/ibt.h>
#include <ibprint.h>
#include "dumppath.h"
#include "opasadb_path.h"

/* Returns 0 on error, non-zero on success. */
int parse_gid(char *s, op_gid_t *gid)
{
	if (!strncmp(s,"0x",2)) {
		/* GID format */

		uint64_t prefix=0;
		uint64_t subnet=0;
		char *p, *l, *sep=":";
	
		p = strtok_r(s,sep,&l);
		if (!p) return 0;
	
		prefix = strtoull(p,NULL,0);
		if (!prefix) return 0;
	
		p = strtok_r(NULL, sep, &l);
		if (!p) return 0;
	
		subnet = strtoull(p,NULL,0);
		if (!subnet) return 0;
	
		gid->unicast.interface_id = hton64(subnet);
		gid->unicast.prefix = hton64(prefix);

		return 1;

	} else {
		/* Inet 6 format */

		return inet_pton(AF_INET6, s, gid->raw);
	}
	
}
void print_path_record(char *str, op_path_rec_t *p_path) 
{
	return fprint_path_record(stdout,str,p_path);
}

static void network_to_host_path_record(IB_PATH_RECORD_NO *p_net, 
								 		IB_PATH_RECORD *p_host)
{
	p_host->ServiceID = ntoh64(p_net->ServiceID);
	p_host->DGID.Type.Global.SubnetPrefix = ntoh64(p_net->DGID.Type.Global.SubnetPrefix);
	p_host->DGID.Type.Global.InterfaceID = ntoh64(p_net->DGID.Type.Global.InterfaceID);
	p_host->SGID.Type.Global.SubnetPrefix = ntoh64(p_net->SGID.Type.Global.SubnetPrefix);
	p_host->SGID.Type.Global.InterfaceID = ntoh64(p_net->SGID.Type.Global.InterfaceID);
	p_host->DLID = ntoh16(p_net->DLID);
	p_host->SLID = ntoh16(p_net->SLID);

	p_host->u1.AsReg32 = ntohl(p_net->u1.AsReg32);
	p_host->TClass = p_net->TClass;
	p_host->Reversible = p_net->Reversible;
	p_host->NumbPath = p_net->NumbPath;
	p_host->P_Key = ntohs(p_net->P_Key);
	p_host->u2.AsReg16 = ntohs(p_net->u2.AsReg16);
	p_host->MtuSelector = p_net->MtuSelector;
	p_host->Mtu = p_net->Mtu;
	p_host->RateSelector = p_net->RateSelector;
	p_host->Rate = p_net->Rate;
	p_host->PktLifeTimeSelector = p_net->PktLifeTimeSelector;
	p_host->PktLifeTime = p_net->PktLifeTime;
	p_host->Preference = p_net->Preference;
}

void fprint_path_record(FILE *f, char *str, op_path_rec_t *p_path)
{
	IB_PATH_RECORD path;
	PrintDest_t printdest;

	network_to_host_path_record((IB_PATH_RECORD_NO*)p_path, &path);

	PrintDestInitFile(&printdest,f);

	PrintFunc(&printdest, "%s:\n",str);
	PrintExtendedPathRecord(&printdest,8, &path);
}

