/* BEGIN_ICS_COPYRIGHT5 ****************************************

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

 * ** END_ICS_COPYRIGHT5   ****************************************/
/* [ICS VERSION STRING: unknown] */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#define _GNU_SOURCE
#include <signal.h>

#ifdef __cplusplus
}
#endif

#include <topology.h>
#include <opamgt_priv.h>
#include "iba/ib_types.h"
#include "iba/ib_sm_priv.h"
#include "iba/ib_helper.h"
#include "iba/ib_ibt.h"
#include "iba/ipublic.h"
#include "ibprint.h"


// macro definitions
#ifndef DBGPRINT
#define DBGPRINT(format, args...) \
	do { if (g_verbose) { fflush(stdout); fprintf(stderr, format, ##args); } } while (0)
#endif
#define APP_NAME					"opachassisip"
#define MYTAG MAKE_MEM_TAG('i','g', 'i', 'p')

#define RESP_WAIT_TIME (1000)	// 1000 milliseconds for receive response

// type definitions
typedef struct ipv4AddrEntry_s {
	cl_map_item_t	AllIpAddrsEntry;	// key is SystemImageGUID
	STL_IPV4_IP_ADDR ipAddr;
} ipv4AddrEntry_t;

typedef struct ipv6AddrEntry_s {
	cl_map_item_t	AllIpAddrsEntry;	// key is SystemImageGUID
	STL_IPV6_IP_ADDR ipAddr;
} ipv6AddrEntry_t;

// global variable
uint8_t g_hfi = 0;
uint8_t g_port = 0;
uint64_t g_bkey = 0;
uint8_t g_verbose = 0;
uint8_t g_omgt_debug = 0;
EUI64 g_portGuid = -1;				// local port to use to access fabric
IB_PORT_ATTRIBUTES	*g_portAttrib;	// attributes for our local port
PrintDest_t g_dest;
cl_qmap_t g_allIpv4Addresses;
cl_qmap_t g_allIpv6Addresses;

// command line options, each has a short and long flag name
struct option options[] = {
	{ "verbose", no_argument, NULL, 'v'},
	{ "hfi", no_argument, NULL, 'h'},
	{ "port", no_argument, NULL, 'p'},
	{ "help", no_argument, NULL, '?'},
	{ 0}
};

static void usage()
{
	fprintf(stderr, "Usage: opachassisip [-v][-h][-p]\n");
	fprintf(stderr, "	 -v/--verbose		   - verbose output\n");
	fprintf(stderr, "	 -h/--hfi hfi		   - hfi to send via, numbered 1..n, 0= -p port will be\n");
	fprintf(stderr, "                            a system wide port num (default is 0)\n");
	fprintf(stderr, "	 -p/--port port		   - port to send via, numbered 1..n, 0=1st active\n");
	fprintf(stderr, "                            (default is 1st active)\n");
	fprintf(stderr, "	 ?/--help			   - produce full help text\n");
	fprintf(stderr, "The -h and -p options permit a variety of selections:\n");
	fprintf(stderr, "    -h 0       - 1st active port in system (this is the default)\n");
	fprintf(stderr, "    -h 0 -p 0  - 1st active port in system\n");
	fprintf(stderr, "    -h x       - 1st active port on HFI x\n");
	fprintf(stderr, "    -h x -p 0  - 1st active port on HFI x\n");
	fprintf(stderr, "    -h 0 -p y  - port y within system (irrespective of which ports are active)\n");
	fprintf(stderr, "    -h x -p y  - HFI x, port y\n");

	exit(2);
}

static void signal_handler(int sig)
{
	DBGPRINT("%s: Abort request received, terminating!\n", APP_NAME);
	exit(-1);
}

static int print_v4v6_ip_info(const char *chassisIP)
{
	struct addrinfo hints, *res, *next;
	int errcode;
	char addrstr[100];
	void *ptr = NULL;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;

	errcode = getaddrinfo (chassisIP, NULL, &hints, &res);
	if (errcode != 0) {
		fprintf(stderr, "Unable to get addressing information on IP address %s\n", chassisIP);
		return -1;
	}

	fprintf(stderr, "Host: %s\n", chassisIP);
	while (res) {
		inet_ntop (res->ai_family, res->ai_addr->sa_data, addrstr, 100);

		switch (res->ai_family) {
		case AF_INET:
			ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
			break;
		case AF_INET6:
			ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
			break;
		}
		inet_ntop (res->ai_family, ptr, addrstr, 100);
		fprintf(stderr, "IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
				addrstr, res->ai_canonname);
		next = res->ai_next;
			 freeaddrinfo(res);
			 res = next;
	}
	freeaddrinfo(res);

	return 0;
}

static FSTATUS ipv4_ip_list(NodeData *nodep, PortData *portp)
{
	int indent = 0;
	cl_map_item_t *mi;
	struct hostent *hp = NULL;
	struct in_addr sin_addr;
	ipv4AddrEntry_t *ipAddrEntryp;
	char ipString[100];

	memset(ipString, 0, sizeof(ipString));
	inet_ntop(AF_INET, &(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV4), ipString, sizeof(ipString));

	if (g_verbose) 
		fprintf(stderr, "Processing IPv4 address: %s\n", ipString);

	memset(&sin_addr, 0, sizeof(sin_addr));
	memcpy(&sin_addr, &(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV4), sizeof(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV4));

	// check whether IP address was already processed
	mi = cl_qmap_get(&g_allIpv4Addresses, nodep->NodeInfo.SystemImageGUID);
	if (mi != cl_qmap_end(&g_allIpv4Addresses)) {
		hp = gethostbyaddr((const char *)&sin_addr, sizeof(sin_addr), AF_INET);
		if (g_verbose) {
			PrintFunc(&g_dest, "%*s 0x%04x,0x%016"PRIx64",%s,%s:Duplicate\n",
					  indent, "",
					  portp->EndPortLID,
					  nodep->NodeInfo.SystemImageGUID,
					  (hp) ? hp->h_name : "",
					  ipString);
		}
		return FSUCCESS;
	}

	// add IP address to the global IPv4 chassis IP list
	ipAddrEntryp = (ipv4AddrEntry_t *)MemoryAllocate2AndClear(sizeof(ipv4AddrEntry_t), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (ipAddrEntryp == NULL) {
		fprintf(stderr, "Error, unable to allocate memory for IP address entry\n");
		return FINSUFFICIENT_MEMORY;
	}

	memcpy(&(ipAddrEntryp->ipAddr), &(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV4), sizeof(ipAddrEntryp->ipAddr));

	mi = cl_qmap_insert(&g_allIpv4Addresses, nodep->NodeInfo.SystemImageGUID, &ipAddrEntryp->AllIpAddrsEntry);
	if (mi != &ipAddrEntryp->AllIpAddrsEntry) {
		MemoryDeallocate(ipAddrEntryp);
		return FERROR;
	}

	// resolve chassis IP address to hostname
	hp = gethostbyaddr((const char *)&sin_addr, sizeof(sin_addr), AF_INET);
	if (g_verbose) {
		PrintFunc(&g_dest, "%*s 0x%04x,0x%016"PRIx64",%s,%s\n",
				  indent, "",
				  portp->EndPortLID,
				  nodep->NodeInfo.SystemImageGUID,
				  (hp) ? hp->h_name : "",
				  ipString);
	} else {
		PrintFunc(&g_dest, "%*s%s\n",
				  indent, "", (hp) ? hp->h_name : ipString);
	}

	return FSUCCESS;
}

static FSTATUS ipv6_ip_list(NodeData *nodep, PortData *portp)
{
	int indent = 0;
	cl_map_item_t *mi;
	struct hostent *hp = NULL;
	struct in6_addr sin_addr;
	ipv6AddrEntry_t *ipAddrEntryp;
	char ipString[100];

	memset(ipString, 0, sizeof(ipString));
	inet_ntop(AF_INET6, &(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV6), ipString, sizeof(ipString));
	
	if (g_verbose)
		fprintf(stderr, "Processing IPv6 address: %s\n", ipString);

	memset(&sin_addr, 0, sizeof(sin_addr));
	memcpy(&sin_addr, &(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV6), sizeof(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV6));

	// check whether IPv6 address was already processed
	mi = cl_qmap_get(&g_allIpv6Addresses, nodep->NodeInfo.SystemImageGUID);
	if (mi != cl_qmap_end(&g_allIpv6Addresses)) {
		hp = gethostbyaddr((const char *)&sin_addr, sizeof(sin_addr), AF_INET6);
		if (g_verbose) {
			PrintFunc(&g_dest, "%*s 0x%04x,0x%016"PRIx64",%s,%s:Duplicate\n",
					  indent, "",
					  portp->EndPortLID,
					  nodep->NodeInfo.SystemImageGUID,
					  (hp) ? hp->h_name : "",
					  ipString);
		}
		return FSUCCESS;
	}

	//
	// add IPv6 address to the global chassis IP list
	ipAddrEntryp = (ipv6AddrEntry_t *)MemoryAllocate2AndClear(sizeof(ipv6AddrEntry_t), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (ipAddrEntryp == NULL) {
		fprintf(stderr, "Error, unable to allocate memory for IP address entry\n");
		return FINSUFFICIENT_MEMORY;
	}

	memcpy(&(ipAddrEntryp->ipAddr), &(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV6), sizeof(ipAddrEntryp->ipAddr));

	mi = cl_qmap_insert(&g_allIpv6Addresses, nodep->NodeInfo.SystemImageGUID, &ipAddrEntryp->AllIpAddrsEntry);
	if (mi != &ipAddrEntryp->AllIpAddrsEntry) {
		MemoryDeallocate(ipAddrEntryp);
		return FERROR;
	}

	// resolve chassis IPv6 address to hostname
	hp = gethostbyaddr((const char *)&sin_addr, sizeof(sin_addr), AF_INET6);
	if (g_verbose) {
		PrintFunc(&g_dest, "%*s 0x%04x,0x%016"PRIx64",%s,%s\n",
				  indent, "",
				  portp->EndPortLID,
				  nodep->NodeInfo.SystemImageGUID,
				  (hp) ? hp->h_name : "",
				  ipString);
	} else {
		PrintFunc(&g_dest, "%*s%s\n",
				  indent, "", (hp) ? hp->h_name : ipString);
	}

	return FSUCCESS;
}

static FSTATUS chassis_ip_sweep(struct omgt_port *port, FabricData_t *fabric)
{
	FSTATUS fstatus = FSUCCESS;
	cl_map_item_t *p;
	int i = 0, pc = 0;
	uint8_t zeroArray[16];
	memset(zeroArray, 0, sizeof(zeroArray));

	// search for switches enabled for enhanced port 0
	for (p=cl_qmap_head(&fabric->AllNodes); p != cl_qmap_end(&fabric->AllNodes); p = cl_qmap_next(p), i++) {
		cl_map_item_t *q;
		NodeData *nodep = PARENT_STRUCT(p, NodeData, AllNodesEntry);

		if (nodep == NULL)
			break;
		if (nodep->NodeInfo.NodeType == STL_NODE_SW) {
			//if (nodep->pSwitchInfo && nodep->pSwitchInfo->SwitchInfoData.u2.s.EnhancedPort0) {
			// get node port related information
			for (pc=0, q=cl_qmap_head(&nodep->Ports); q != cl_qmap_end(&nodep->Ports); q = cl_qmap_next(q), pc++) {
				PortData *portp = PARENT_STRUCT(q, PortData, NodePortsEntry);

				if (portp == NULL)
					break;
				if (portp->PortNum == 0) {
					if(!nodep->pSwitchInfo->SwitchInfoData.u2.s.EnhancedPort0)
						break;

					// If the IPv4 Address is not zero process it
					if(memcmp(&(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV4), zeroArray, sizeof(STL_IPV4_IP_ADDR))) {
						if(g_verbose) {
							char ipString[100];
							memset(ipString, 0, sizeof(ipString));

							inet_ntop(AF_INET, &(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV4), ipString, sizeof(ipString));
							print_v4v6_ip_info(ipString);
						}

						ipv4_ip_list(nodep, portp);
						break;
					// If there was no IPv4 address and the IPv6 address is not zero process it
					} else if(memcmp(&(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV6), zeroArray, sizeof(STL_IPV6_IP_ADDR))) {
						if(g_verbose) {
							char ipString[100];
							memset(ipString, 0, sizeof(ipString));

							inet_ntop(AF_INET6, &(nodep->pSwitchInfo->SwitchInfoData.IPAddrIPV6), ipString, sizeof(ipString));
							print_v4v6_ip_info(ipString);
						}

						ipv6_ip_list(nodep, portp);
						break;
					}
				}
			}
		}
	}
	return fstatus;
}

int main (int argc, char *argv[])
{
	int rc = 0, c, index;
	unsigned long temp;
	char *endptr;
	FabricData_t fabric;
	struct omgt_port *chas_omgt_session;

	Top_setcmdname(APP_NAME);

	//
	// process command line arguments
	while (-1 != (c = getopt_long(argc,argv, "voh:p:", options, &index))) {
		switch (c) {
		case 'v':	// enable debug logging
			g_verbose = 1;
			break;

		case 'o':	// enable opamgt level debug reporting
			g_omgt_debug = 1;
			break;

		case '?':	// help
			usage();
			break;

		case 'h':	// hfi to issue query from
			errno = 0;
			temp = strtoul(optarg, &endptr, 0);
			if (temp > IB_UINT8_MAX || errno || ! endptr || *endptr != '\0') {
				fprintf(stderr, "Error, invalid HFI Number: %s!\n", optarg);
				usage();
			}
			g_hfi = (uint8)temp;
			break;

		case 'p':	// port to issue query from
			errno = 0;
			temp = strtoul(optarg, &endptr, 0);
			if (temp > IB_UINT8_MAX || errno || ! endptr || *endptr != '\0') {
				fprintf(stderr, "Error, invalid Port Number: %s!\n", optarg);
				usage();
			}
			g_port = (uint8)temp;
			break;

		default:
			fprintf(stderr, "Error, invalid option -%c!\n", c);
			usage();
			break;
		}
	} // end while


	if (optind < argc) {
		usage();
	}

	// catch signals and do orderly shutdown
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGPIPE, signal_handler);

	struct omgt_params params = {.debug_file = g_omgt_debug ? stderr : NULL};
	rc = omgt_open_port_by_num(&chas_omgt_session, g_hfi, g_port, &params);
	if (rc) {
		fprintf(stderr, "Error, could not open opamgt session (%d).\n",rc); 
		exit(rc);
	}

	if ((rc = omgt_port_get_port_guid(chas_omgt_session, &g_portGuid)) != 0) {
		fprintf(stderr, "Error, could not determine PortGuid: %u\n", rc);
	}

	InitSweepVerbose(g_verbose?stderr:NULL);

	// sweep fabric for all nodes
	if (FSUCCESS != Sweep(g_portGuid, &fabric, FF_NONE, SWEEP_ALL, !g_verbose))
		fprintf(stderr, "Error, sweep failed!\n"); 
	else {
		// initialize global structures
		cl_qmap_init(&g_allIpv4Addresses, NULL);
		cl_qmap_init(&g_allIpv6Addresses, NULL);
		PrintDestInitFile(&g_dest, stdout);

		// generate chassis IP list 
		chassis_ip_sweep(chas_omgt_session,&fabric);
	}

	// deallocate connections to IB related entities 
	DestroyMad();
	omgt_close_port(chas_omgt_session);

	exit(0);
}
