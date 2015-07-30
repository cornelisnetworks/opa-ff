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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

void Usage()
{
	fprintf(stderr, "Usage: opagetipaddrtype addr\n");
	exit(2);
}

int main(int argc, char **argv)
{
	struct addrinfo *addrinfo = NULL;
	int err;

	if (argc != 2)
		Usage();

	// resolve name to an IP address
	if (0 != (err = getaddrinfo(argv[1], NULL, NULL, &addrinfo)) 
							|| ! addrinfo) {
		fprintf(stderr,"Error: Unable to resolve name %s, %s\n",
						argv[1], gai_strerror(err));
		exit(1);
	}
	if (addrinfo[0].ai_family == AF_INET) {
		fprintf(stdout, "ipv4\n");
		exit(0);
	} else if (addrinfo[0].ai_family == AF_INET6) {
		fprintf(stdout, "ipv6\n");
		exit(0);
	} else {
		fprintf(stderr, "Error: Unrecognized address type for hostname %s\n",argv[1]);
		exit(1);
	}
}
