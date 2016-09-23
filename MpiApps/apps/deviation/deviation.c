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
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <getopt.h>
#include <errno.h>

#include <mpi.h>

#define HOSTNAME_MAXLEN 64
#define MYBUFSIZE 4*1024*1028
#define MAX_REQ_NUM 100000

/* defaults */
#define LATENCY_TOLERANCE 50
#define LATENCY_DELTA 0.8
#define LATENCY_WARMUP 10
#define LATENCY_SIZE 0
#define LATENCY_LOOP 4000

#define BANDWIDTH_TOLERANCE 20
#define BANDWIDTH_DELTA 150
#define BANDWIDTH_WARMUP 10
#define BANDWIDTH_SIZE (2 * 1024 * 1024)
#define BANDWIDTH_LOOP 30

#define TRIAL_PAIRS 20	/* max pairs to test during trial to select baseline */

#define true	1
#define false	0

char sendBuffer[MYBUFSIZE];
char receiveBuffer[MYBUFSIZE];

MPI_Request request[MAX_REQ_NUM];
MPI_Request request2[MAX_REQ_NUM];
MPI_Status my_stat[MAX_REQ_NUM];

/*#define DEBUG */
/*#define SECOND_PASS */

/* keep track of status per pair for initial test */
typedef struct pairResults
{
	int		sendRank;							/* rank of sending hoat */
	int		receiveRank;						/* rank of receiving host */
	double	latency; 							/* pair latency */
	double	bandwidth;							/* pair bandwidth */
	double	latencyTimeStart;
	double	latencyTimeEnd;
	int		latencyFailed;
	double	bandwidthTimeStart;
	double	bandwidthTimeEnd;
	int		bandwidthFailed;
	char 	sendHostname[HOSTNAME_MAXLEN];		/* sending hostname */
	char 	receiveHostname[HOSTNAME_MAXLEN];	/* receiving hostname */
	int	alone;									/* is this a single */
	int	failed;									/* did this pair fail */
	int     valid;                              /* is this a valid pair */
} pairResults;

/* host list */
typedef struct hostListing
{
	char			hostname[HOSTNAME_MAXLEN];
} hostListing;

/* global variables */
int verbose = false;		/* default to quite mode */
int debug = false;			/* default to no debug statements */
int concurrent = false;		/* default to sequential testing */
int compare_best = false;	/* default to using Avg for comparisons */
int secondPass = false;		/* start with first pass */
int trialPass = false;		/* assume no trial pass */
int trialPairs = TRIAL_PAIRS;/* num pairs run during trail to pick baseline */
int compute_lat_thres = true;	/* default to computing using tol and delta */
int compute_bw_thres = true;	/* default to computing using tol and delta */
int use_bwdelta = false;
int use_bwtol = false;
int use_lattol = false;
int use_latdelta = false;

int latencyTolerance = LATENCY_TOLERANCE;
double latencyDelta = LATENCY_DELTA;
int latencySize = LATENCY_SIZE; /* latency test frame size */
int latencyLoop = LATENCY_LOOP; /* number of latency frame loops */
double latencyUpper = 0;/* failure threshold, latency > Upper fails */

int bandwidthTolerance = BANDWIDTH_TOLERANCE;
double bandwidthDelta = BANDWIDTH_DELTA;
int bandwidthSize = BANDWIDTH_SIZE; /* bandwidth test frame size */
int bandwidthLoop = BANDWIDTH_LOOP; /* number of bandwidth frame loops */
double bandwidthLower = 0;	/* failure threshold, bandwidth < Lower fails */
int bandwidthBiDir = false;	/* do a bidirectional BW test */

char baselineHost[HOSTNAME_MAXLEN];
int baselineRank = 0;
hostListing* hostList = NULL;		/* list of hosts */

double latencyMax = 0;				
double latencyMin = 0;
double latencyAvg = 0;				
double latencyBase = 0;	/* basis for comparison - Avg or Min */

double bandwidthMax = 0;				
double bandwidthMin = 0;
double bandwidthAvg = 0;
double bandwidthBase = 0;	/* basis for comparison - Avg or Max */

/* declarations */
static void initPairs(pairResults *pairData, int pairs);
static void reportTestResults(int pairs, pairResults *pairData);
static int parseInputParameters(int argc, char *argv[]);
static int findBaselineHost(int pairs, int loop, pairResults *pairData);
static void createPairs(int concurrent, pairResults *pairData);
static void runConcurrentLatency(int pairs, int doubles, int size, int loop, 
	pairResults *pairData); 
static void runConcurrentBandwidth(int pairs, int doubles, int size, int loop, 
	pairResults *pairData); 
static void runSequentialLatency(int pairs, int size, int loop, 
	pairResults *pairData); 
static void runSequentialBandwidth(int pairs, int size, int loop, 
	pairResults *pairData); 
static void hostReport(pairResults *pairData, int latency);
static void hostBroadcast(pairResults *pairData);
static void Usage();
static const char *pairedHostname();

/* Entry point */
int
main(int argc, char **argv)
{
    int myrank;					/* what is my rank */
	int ranks;					/* how many ranks total */
	int hosts; 					/* how many hosts */
	int pair; 					/* pairing iterator - first pass */
	int pair2; 					/* pairing iterator - second pass */
	int pairs;					/* how many pairings */
	int doubles;				/* how many unique pairs */
	int newPairs;				/* pairs for second round of testing */
	pairResults* pairData;		/* paired data info - first pass */
	pairResults* pairData2;		/* paired data info - second pass */

    MPI_Init(&argc, &argv);

	/* how many hosts are there? */
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);
	if (ranks < 2)
	{
		fprintf(stderr, "Must have at least 2 hosts to run deviation test\n");
		MPI_Finalize();
		return 0;
	}

	/* set globals */
	newPairs = 0;

	/* parse command line arguments */
	if (parseInputParameters(argc, argv) < 0)
	{
		MPI_Finalize();
		exit(2);
	} 

	/* what is my rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* space test data down one line to start out with */
	if (myrank == 0)
		printf("\n");

	/* get memory for host names */
	hostList = malloc(sizeof(hostListing) * ranks);
	assert(hostList);

	/* calculate how many pairs we need for testing */
	if (concurrent == true)
	{
		doubles = pairs = ranks / 2;
		if (ranks % 2 != 0)
			pairs++;
	}
	else
		doubles = pairs = ranks - 1;

	/* get memory for tests */
	pairData = malloc(sizeof(pairResults) * ranks);
	assert(pairData);
	initPairs(&pairData[0], ranks);

	/* get memory for second round of pair testing */
	if (myrank == 0)
	{
		pairData2 = malloc(sizeof(pairResults) * ranks);
		assert(pairData2);
		initPairs(&pairData2[0], ranks);
	}

	if (debug == true)
	{
		printf(
			"Rank %u reports ranks %u pairs %u doubles %u hostList %u pairData %u\n", 
			myrank, ranks, pairs, doubles, (unsigned)sizeof(hostListing) * ranks, 
			(unsigned)sizeof(pairResults) * pairs);
	}

	/* gather the list of hostnames */
    MPI_Allgather((void *)pairedHostname(), HOSTNAME_MAXLEN, MPI_CHAR,
		  hostList, HOSTNAME_MAXLEN, MPI_CHAR, MPI_COMM_WORLD);

	/* get the list of hosts */
	if (myrank == 0 && debug == true)
	{
    	for (hosts = 0; hosts < ranks; hosts++) 
			printf("Rank %u: %s\n", hosts, hostList[hosts].hostname);
	}

	/* if user supplied a baseline host then use that host in sequential tests */
	if (strlen(baselineHost) > 0)
	{
		int found = false;
		for (hosts = 0; hosts < ranks; hosts++)
		{
			if (strcmp(hostList[hosts].hostname, baselineHost) == 0)
			{
				baselineRank = hosts; 
				found = true;
				break;
			}
		}
		if (found == false)
		{
			if (myrank == 0)
				printf("Baseline host not found: %s\n", baselineHost);
			strcpy(baselineHost, hostList[0].hostname);
			baselineRank = 0;
			trialPass = true;
		}
	}
	/* otherwise setup to do a trial run to find the best baseline host */
	else
	{
		strcpy(baselineHost, hostList[0].hostname);
		trialPass = true;
	}

	/* if no baseline host then find the best host */
	if (trialPass == true)
	{
		/* only test the first 20 hosts */
		if (ranks < TRIAL_PAIRS*2)
			trialPairs = ranks / 2;
		else
			trialPairs = TRIAL_PAIRS;

		if (myrank == 0)
		{
			printf("Trial runs of %u hosts are being performed to find\n"
							"the best host since no baseline host was specified.\n",
				trialPairs * 2);
		}

#ifdef DEBUG
		printf("Pairs %u doubles %u trialPairs %u hosts %u\n", 
			pairs, doubles, trialPairs, ranks);
#endif

		MPI_Barrier(MPI_COMM_WORLD);

		if (myrank == 0)
			createPairs(/* concurrent */ true, pairData);

		MPI_Barrier(MPI_COMM_WORLD);

		hostBroadcast(&pairData[0]);

		MPI_Barrier(MPI_COMM_WORLD);

		/* run quick concurrent latency test on hosts with a larger
	       packet size than normal so it is indicitive of both best
           latency and bandwidth.  Bandwidth will be better indicator */
		runConcurrentLatency(trialPairs, trialPairs, BANDWIDTH_SIZE, BANDWIDTH_LOOP, &pairData[0]);

		MPI_Barrier(MPI_COMM_WORLD);

		/* look for the best combo */
		if (myrank == 0)
		{
			baselineRank = findBaselineHost(trialPairs, BANDWIDTH_LOOP, &pairData[0]);
			strcpy(baselineHost, hostList[baselineRank].hostname);
		}
		/* trial pass is done */
		trialPass = false;
	}
	if (myrank == 0)
		printf("\nBaseline host is %s (%d)\n\n", 
			hostList[baselineRank].hostname, baselineRank);

	MPI_Barrier(MPI_COMM_WORLD);

	/* now that we have the list of hosts create the initial pairs */
	initPairs(&pairData[0], ranks);
	if (myrank == 0)
		createPairs(concurrent, pairData);

	MPI_Barrier(MPI_COMM_WORLD);

	/* distribute the pair list to all ranks */
	hostBroadcast(&pairData[0]);

	MPI_Barrier(MPI_COMM_WORLD);

	/* run the latency tests */
	if (concurrent == true)
		runConcurrentLatency(pairs, doubles, latencySize, latencyLoop, &pairData[0]);
	else
		runSequentialLatency(pairs, latencySize, latencyLoop, &pairData[0]);

	/* run the bandwidth tests */
	if (concurrent == true)
		runConcurrentBandwidth(pairs, doubles, bandwidthSize, bandwidthLoop, &pairData[0]);
	else
		runSequentialBandwidth(pairs, bandwidthSize, bandwidthLoop, &pairData[0]);

	MPI_Barrier(MPI_COMM_WORLD);

	/* report latency and bandwidth */
	if (myrank == 0)
	{
		reportTestResults(pairs, &pairData[0]);
	}

	/* if sequential testing then we are done */
	if (concurrent == false)
	{
		MPI_Finalize();
		return 0;
	}

	/* if we are running concurrent tests then find all of the failed ones and 
	   pair them against the baseline rank for a sequential test */
	secondPass = true;

	/* find how many concurrent test failed */
	if (myrank == 0)
	{
		for (pair = 0; pair < pairs; pair++)
		{
			/* keep track of the number of failed pairs */
			if (pairData[pair].failed == true)
			{
				/* if the original send pair was the baselinerank or rank 0 not in the
				   first pair then we only need one pair for the new one */
				if (pairData[pair].sendRank == baselineRank)
					newPairs++;
				else if (pairData[pair].receiveRank == baselineRank)
					newPairs++;
				else if (pair != 0 && pairData[pair].sendRank == 0)
					newPairs++;
				else
					newPairs += 2;
			}
		}
	}

	if (myrank == 0 && newPairs != 0)
	{
		/* create new sequential pairs for round 2 of testing */
		pair2 = 0;
		for (pair = 0; pair < pairs; pair++)
		{
			if (pairData[pair].failed == true)
			{
				/* pair the baseline rank against the failed send rank - only
				   allow rank 0 send rank to occupy the first pair */
				if (pair == 0)
				{
					if (pairData[pair].sendRank != baselineRank)
					{
						pairData2[pair2].receiveRank = pairData[pair].sendRank;
						strcpy(pairData2[pair2].receiveHostname, 
							pairData[pair].sendHostname);
						pair2++;
					}
				}
				else
				{
					if (pairData[pair].sendRank != 0 && 
						pairData[pair].sendRank != baselineRank)
					{
						pairData2[pair2].receiveRank = pairData[pair].sendRank;
						strcpy(pairData2[pair2].receiveHostname, 
							pairData[pair].sendHostname);
						pair2++;
					}
				}
						
				/* pair the baseline rank against the failed receive rank */
				if (pairData[pair].receiveRank != baselineRank)
				{
					pairData2[pair2].receiveRank = pairData[pair].receiveRank;
					strcpy(pairData2[pair2].receiveHostname, 
						pairData[pair].receiveHostname);
					pair2++;
				}
			}
		}

		/* all send hosts for round 2 will use the baseline rank */
		for (pair2 = 0; pair2 < newPairs; pair2++)
		{
			pairData2[pair2].sendRank = baselineRank;
			strcpy(pairData2[pair2].sendHostname, baselineHost);
			pairData2[pair2].valid = true;
			pairData2[pair2].alone = true;
#ifdef DEBUG
			printf("Second Round Pair %u send rank %u receive rank %u\n", 
				pair2, pairData2[pair2].sendRank, pairData2[pair2].receiveRank);
#endif
		}
	}

	/* clear out the first round of test pairs so we can get ready for the 
	   second round and then copy second round to first round pair array */
	initPairs(&pairData[0], ranks);
	if (myrank == 0 && newPairs != 0)
	{
		for (pair = 0; pair < newPairs; pair++)
		{
			pairData[pair] = pairData2[pair];
		}
	}

#ifdef DEBUG
	if (myrank == 0)
	{
		for (pair = 0; pair < ranks; pair++)
		{
			printf(
				"Second Round Pair %u send rank %u receive rank %u valid %u\n",
				pair, pairData[pair].sendRank, pairData[pair].receiveRank, 
				pairData[pair].valid);
		}
	}
#endif

	/* wait for all communications to complete */
	MPI_Barrier(MPI_COMM_WORLD);

	/* broadcast pair data to all hosts */
	hostBroadcast(&pairData[0]);
			
	/* if there are no second pairs to test then we are done in all processes 
	   - we know this since the receive rank of the first pair will be invalid */
	if (pairData[0].valid == false)
	{
		MPI_Finalize();
		return 0;
	}
	
	/* run the second round of latency tests */
	runSequentialLatency(ranks, latencySize, latencyLoop, &pairData[0]);

	/* run the second round of bandwidth tests */
	runSequentialBandwidth(ranks, bandwidthSize, bandwidthLoop, &pairData[0]);

	/* report latency and bandwidth */
	concurrent = false;
	if (myrank == 0 && newPairs != 0)
	{
		reportTestResults(newPairs, &pairData[0]);
	}

    MPI_Finalize();
    return 0;
}

/* duration is in sec, returns latency in usec */
/* each loop is a round trip, so we half it to get 1 way latency */
double
compute_latency(double duration, int loop)
{
	return (duration * 1.0e6) / (2.0 * loop);
}
double
compute_pair_latency(int pair, int loop, pairResults *pairData)
{
	return compute_latency( pairData[pair].latencyTimeEnd
							- pairData[pair].latencyTimeStart, loop);
}

/* size in bytes, duration in seconds, returns MB/s */
/* Note we use the OSU definition of 1.0e6=MB */
double
compute_bandwidth(double duration, int size, int loop)
{
	return (((size/1.0e6) * loop) / duration) * (bandwidthBiDir?2:1);
}

double
compute_pair_bandwidth(int pair, int size, int loop, pairResults *pairData)
{
	return compute_bandwidth(
				pairData[pair].bandwidthTimeEnd - pairData[pair].bandwidthTimeStart, size, loop);
}

/* initialize pairs */
static void
initPairs(pairResults *pairData, int pairs)
{
	int pair;

	for (pair = 0; pair < pairs; pair++)
	{
		pairData[pair].valid = false;
		pairData[pair].sendRank = 0;
		pairData[pair].receiveRank = 0;
		pairData[pair].latency = 0;
		pairData[pair].bandwidth = 0;
		pairData[pair].latencyTimeStart = 0;
		pairData[pair].latencyTimeEnd = 0;
		pairData[pair].latencyFailed = false;
		pairData[pair].bandwidthTimeStart = 0;
		pairData[pair].bandwidthTimeEnd = 0;
		pairData[pair].bandwidthFailed = false;
		pairData[pair].sendHostname[0] = 0;
		pairData[pair].receiveHostname[0] = 0;
		pairData[pair].alone = true;
		pairData[pair].failed = false;
	}
}

/* find the baseline host for testing */
static int
findBaselineHost(int pairs, int loop, pairResults *pairData)
{
	int pair;
	int bestLatencyPair = 0;

	for (pair = 0; pair < pairs; pair++)
	{
#ifdef DEBUG
		printf("pair %u valid=%d\n", pair, pairData[pair].valid);
#endif
		if (pairData[pair].valid == false)
			continue;

		/* calculate latency between pairs */
		pairData[pair].latency = compute_pair_latency(pair, loop, pairData);

		if (debug)
			printf("Trial Latency %.2f usec %s (%u) <-> %s (%u)\n", 
				pairData[pair].latency,
				pairData[pair].sendHostname, pairData[pair].sendRank, 
				pairData[pair].receiveHostname, pairData[pair].receiveRank);

		/* find min latency */
		if (pair == 0)
		{
			latencyMin = pairData[0].latency;
			if (debug)
				printf("Best Candidate: %s (%u)\n",
					pairData[pair].sendHostname, pairData[pair].sendRank);
		}
		else
		{
			if (pairData[pair].latency < latencyMin)
			{
				latencyMin = pairData[pair].latency;
				bestLatencyPair = pair;
				if (debug)
					printf("Best Candidate: %s (%u)\n",
						pairData[pair].sendHostname, pairData[pair].sendRank);
			}
		}
	}
	/* return the sender in the best latency pair */
	return pairData[bestLatencyPair].sendRank;
}
		
/* report test results */
static void
reportTestResults(int pairs, pairResults *pairData)
{
	int pair;
	double pairPercentage;
	double latencyRange;
	double latencyWorstPct;
	double bandwidthRange;
	double bandwidthWorstPct;
	int latencyFailed = false;
	int bandwidthFailed = false;

	/* report latency and bandwidth */
	/* we only compute average on 1st pass, too few entries on second pass */
	if (! secondPass) {
		latencyAvg = 0;
		bandwidthAvg = 0;
	}
	for (pair = 0; pair < pairs; pair++)
	{
		/* calculate latency between pairs */
		pairData[pair].latency = compute_pair_latency(pair, latencyLoop, pairData);

		/* find max, min and avg latency */
		if (! secondPass)
			latencyAvg += pairData[pair].latency / pairs;
		if (pair == 0)
		{
			latencyMax = pairData[0].latency;				
			latencyMin = pairData[0].latency;
		} else {
			if (pairData[pair].latency > latencyMax)
				latencyMax = pairData[pair].latency;
			if (pairData[pair].latency < latencyMin)
				latencyMin = pairData[pair].latency;
		}

		/* calculate bandwidth between pairs */
		pairData[pair].bandwidth = compute_pair_bandwidth(pair, bandwidthSize,
					   							bandwidthLoop, pairData);

		/* find max, min and avg bandwidth */
		if (! secondPass)
			bandwidthAvg += pairData[pair].bandwidth / pairs;
		if (pair == 0)
		{
			bandwidthMax = pairData[0].bandwidth;				
			bandwidthMin = pairData[0].bandwidth;
		} else {
			if (pairData[pair].bandwidth > bandwidthMax)
				bandwidthMax = pairData[pair].bandwidth;
			if (pairData[pair].bandwidth < bandwidthMin)
				bandwidthMin = pairData[pair].bandwidth;
		}
	}

	/* we reuse thresholds from 1st pass, too few entries on 2nd pass */
	if (! secondPass) {
		/* calculate upper and lower latency thresholds based on tolerance */
		/* tolerance is expressed as % of Base */
		if (compare_best)
			latencyBase = latencyMin;
		else
			latencyBase = latencyAvg;
		if (compute_lat_thres) {
			/*if user specifies both delta and tolerance, use the less strict*/
			/*If neither are specified, use least strict of defaults */
			if((!use_latdelta && !use_lattol) || (use_latdelta && use_lattol)) {
				latencyUpper = latencyBase * (1 + (latencyTolerance / 100.0));
				if (latencyUpper < latencyBase + latencyDelta)
					latencyUpper = latencyBase + latencyDelta;
			}
			else if(use_lattol) {
				latencyUpper = latencyBase * (1 + (latencyTolerance / 100.0));
			} 
			else if(use_latdelta) { 
				latencyUpper = latencyBase + latencyDelta;
			}
		}

		/* calculate upper and lower bandwidth thresholds based on tolerance */
		/* tolerance is expressed as % of Base */
		if (compare_best)
			bandwidthBase = bandwidthMax;
		else
			bandwidthBase = bandwidthAvg;
		if (compute_bw_thres) {
			/*if user specifies both delta and tolerance, use the less strict*/
			/*If neither are specified, use least strict of defaults */
			if((!use_bwdelta && !use_bwtol) || (use_bwdelta && use_bwtol)) {
				bandwidthLower = bandwidthBase * (1 - (bandwidthTolerance / 100.0));
				if (bandwidthLower > bandwidthBase - bandwidthDelta)
					bandwidthLower = bandwidthBase - bandwidthDelta;
			}
			else if(use_bwtol) {
				bandwidthLower = bandwidthBase * (1 - (bandwidthTolerance / 100.0));
			} 
			else if(use_bwdelta) { 
				bandwidthLower = bandwidthBase - bandwidthDelta;
			}
		
		}
	}

	/* calculate the middle of performance range */
	/*latencyMid = latencyMin + ((latencyMax - latencyMin) / 2); */
	/*bandwidthMid = bandwidthMin + ((bandwidthMax - bandwidthMin) / 2); */

	/* latencyWorstPct could be > 100% when Max is multiple of Min */
	latencyRange = ((latencyMax - latencyMin) / latencyMin) * 100;
	latencyWorstPct = ((latencyMax - latencyBase) / latencyBase) * 100;
	bandwidthRange = ((bandwidthMax - bandwidthMin) / bandwidthMax) * 100;
	bandwidthWorstPct = ((bandwidthMin - bandwidthBase) / bandwidthBase) * 100;

	/* scan through the pairs again and see which ones violate the thresholds */
	for (pair = 0; pair < pairs; pair++)
	{
		pairData[pair].failed = false;
		if (pairData[pair].latency > latencyUpper)
		{
			pairData[pair].failed = true; 
			pairData[pair].latencyFailed = true; 
			latencyFailed = true;
		}
		if (pairData[pair].bandwidth < bandwidthLower)
		{
			pairData[pair].failed = true;
			pairData[pair].bandwidthFailed = true;
			bandwidthFailed = true;
		}
	}

	/* now output the results */
	printf("\n");
	if (secondPass == false)
	{
		if (concurrent == true)
			printf("Concurrent MPI Performance Test Results\n");
		else
			printf("Sequential MPI Performance Test Results\n");
	} else {
		printf("Second Pass - Sequential MPI Performance Test Results\n");
	}

	printf("  Latency Summary:\n");
	printf("    Min: %.2f usec, Max: %.2f usec", latencyMin, latencyMax);
	if (! secondPass)
		printf(", Avg: %.2f usec", latencyAvg);
	if (compare_best) {
		printf("\n    Worst: %+.1f%% of Min\n", latencyWorstPct); 
	} else {
		printf("\n    Range: %+.1f%% of Min, Worst: %+.1f%% of Avg\n",
			 	latencyRange, latencyWorstPct); 
	}
	if (! compute_lat_thres) {
		printf("    Cfg: Threshold: %.2f usec\n", latencyUpper);
	} else if (compare_best) {
		printf("    Cfg: Tolerance: +%u%% of Min, Delta: %.2f usec, Threshold: %.2f usec\n",
					latencyTolerance, latencyDelta, latencyUpper);
	} else if (compute_lat_thres) {
		printf("    Cfg: Tolerance: +%u%% of Avg, Delta: %.2f usec, Threshold: %.2f usec\n",
				latencyTolerance, latencyDelta, latencyUpper);
	}
	printf("         Message Size: %d, Loops: %d\n",
			 latencySize, latencyLoop);

	printf("\n");

	printf("  Bandwidth Summary:\n");
	printf("    Min: %.1f MB/s, Max: %.1f MB/s", bandwidthMin, bandwidthMax); 
	if (! secondPass)
		printf(", Avg: %.1f MB/s", bandwidthAvg); 
	if (compare_best) {
		printf("\n    Worst: %+.1f%% of Max\n", bandwidthWorstPct); 
	} else {
		printf("\n    Range: -%.1f%% of Max, Worst: %+.1f%% of Avg\n",
				bandwidthRange, bandwidthWorstPct); 
	}
	if (! compute_bw_thres) {
		printf("    Cfg: Threshold: %.1f MB/s\n", bandwidthLower);
	} else if (compare_best) {
		printf("    Cfg: Tolerance: -%u%% of Max, Delta: %.1f MB/s, Threshold: %.1f MB/s\n",
			bandwidthTolerance, bandwidthDelta, bandwidthLower);
	} else {
		printf("    Cfg: Tolerance: -%u%% of Avg, Delta: %.1f MB/s, Threshold: %.1f MB/s\n",
				bandwidthTolerance, bandwidthDelta, bandwidthLower);
	}
	printf("         Message Size: %d, Loops: %d BiDir: %s\n",
			 bandwidthSize, bandwidthLoop, bandwidthBiDir?"yes":"no");

	if (verbose || latencyFailed)
	{
		/* report latency stats for each pair */
		printf("\n  Latency Details:\n");
		printf("    Result      Lat     Dev    Host (rank) <-> Host (rank) \n");
		for (pair = 0; pair < pairs; pair++)
		{
			/* skip tests that passed */
			if (! pairData[pair].latencyFailed && ! verbose)
				continue;

			pairPercentage = 
				((pairData[pair].latency - latencyBase) / latencyBase) * 100;
			printf("    %6s  %7.2f %+6.1f%%  %s (%d) <-> %s (%d)\n",
					pairData[pair].latencyFailed ?
						(concurrent? "RETRY " : "FAILED") : "PASSED", 
					pairData[pair].latency, pairPercentage,
					pairData[pair].sendHostname, pairData[pair].sendRank, 
					pairData[pair].receiveHostname, pairData[pair].receiveRank);
		}
	}

	if (verbose || bandwidthFailed)
	{
		/* report bandwidth stats for each pair */
		printf("\n  Bandwidth Details:\n");
		printf("    Result       BW     Dev    Host (rank) --> Host (rank) \n");
		/* report bandwidth stats for each pair */
		for (pair = 0; pair < pairs; pair++)
		{
			/* skip tests that passed */
			if (! pairData[pair].bandwidthFailed && ! verbose)
				continue;

			pairPercentage = 
				((pairData[pair].bandwidth - bandwidthBase) / bandwidthBase) * 100;
			printf("    %6s  %7.1f %+6.1f%%  %s (%d) --> %s (%d)\n",
					pairData[pair].bandwidthFailed ?
						(concurrent? "RETRY " : "FAILED") : "PASSED", 
					pairData[pair].bandwidth, pairPercentage,
					pairData[pair].sendHostname, pairData[pair].sendRank, 
					pairData[pair].receiveHostname, pairData[pair].receiveRank);
		}
	}

	printf("\n");
	if ((latencyFailed || bandwidthFailed) && concurrent) {
		printf("Partial Retry using Sequential with Baseline Host to determine failed host\n");
		printf("\nBaseline host is %s (%d)\n\n", 
			hostList[baselineRank].hostname, baselineRank);
	} else {
		printf("Latency: %s\n", latencyFailed ? "FAILED" : "PASSED");
		printf("Bandwidth: %s\n", bandwidthFailed ? "FAILED" : "PASSED");
	}
}

/* parse bandwidth and latency input parameters */
static int
parseInputParameters(int argc,char *argv[])
{
	int myrank;
	int opt;
	char *p;

	/* what is my rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* parse args */
	for (opt = 1; opt < argc; opt++)
	{
		if (strcasecmp(argv[opt], "-bwtol") == 0) {
			use_bwtol = true;
			errno = 0;
			opt++;
			if (argv[opt])
				bandwidthTolerance = (long)strtoul(argv[opt], &p, 10);
			/* since bw tolerance is % of Avg or Max, low BW must be 0-100% */
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| bandwidthTolerance < 0 || bandwidthTolerance > 100)
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid bandwidth tolerance: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					fprintf(stderr, 
						"Bandwidth tolerance must be a percentage from 0 to 100\n");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-bwdelta") == 0) {
			use_bwdelta = true;
			errno = 0;
			opt++;
			if (argv[opt])
				bandwidthDelta = strtod(argv[opt], &p);
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| bandwidthDelta < 0)
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid bandwidth delta: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					fprintf(stderr, "Bandwidth Delta must be number of MB/s\n");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-bwthres") == 0) {
			compute_bw_thres = false;
			errno = 0;
			opt++;
			if (argv[opt])
				bandwidthLower = strtod(argv[opt], &p);
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| bandwidthLower < 0)
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid bandwidth threshold: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					fprintf(stderr, "Bandwidth Threshold must be number of MB/s\n");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-bwloop") == 0) {
			errno = 0;
			opt++;
			if (argv[opt])
				bandwidthLoop = (long)strtoul(argv[opt], &p, 10);
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| bandwidthLoop <= 0 )
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid bandwidth loop count: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-bwsize") == 0) {
			errno = 0;
			opt++;
			if (argv[opt])
				bandwidthSize = (long)strtoul(argv[opt], &p, 10);
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| bandwidthSize < 0 )
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid bandwidth message size: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-bwbidir") == 0) {
			bandwidthBiDir = true;
		} else if (strcasecmp(argv[opt], "-bwunidir") == 0) {
			bandwidthBiDir = false;
		} else if (strcasecmp(argv[opt], "-lattol") == 0) {
			use_lattol = true;
			errno = 0;
			opt++;
			if (argv[opt])
				latencyTolerance = (long)strtoul(argv[opt], &p, 10);
			/* Max latency > 2x of Avg or Min is possible, so allow 0-500% */
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| latencyTolerance < 0 || latencyTolerance > 500)
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid latency tolerance: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					fprintf(stderr, 
						"Latency tolerance must be a percentage from 0 to 500\n");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-latdelta") == 0) {
			use_latdelta = true;
			errno = 0;
			opt++;
			if (argv[opt])
				latencyDelta = strtod(argv[opt], &p);
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| latencyDelta < 0)
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid latency delta: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					fprintf(stderr, "Latency Delta must be number of usec\n");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-latthres") == 0) {
			compute_lat_thres = false;
			errno = 0;
			opt++;
			if (argv[opt])
				latencyUpper = strtod(argv[opt], &p);
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| latencyUpper < 0)
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid latency threshold: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					fprintf(stderr, "Latency Threshold must be number of usec\n");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-latloop") == 0) {
			errno = 0;
			opt++;
			if (argv[opt])
				latencyLoop = (long)strtoul(argv[opt], &p, 10);
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| latencyLoop <= 0 )
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid latency loop count: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-latsize") == 0) {
			errno = 0;
			opt++;
			if (argv[opt])
				latencySize = (long)strtoul(argv[opt], &p, 10);
			if (argv[opt] == NULL || argv[opt] == p || errno || *p
				|| latencySize < 0 )
			{
				if (myrank == 0) {
					fprintf(stderr, "Invalid latency message size: %s\n", argv[opt]?argv[opt]:"required parameter missing");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-h") == 0) {
			if (argv[++opt] != NULL) {
				strcpy(baselineHost, argv[opt]);
			} else {
				if (myrank == 0) {
					fprintf(stderr, "-h argument requires hostname\n");
					Usage();
				}
				return -1;
			}
		} else if (strcasecmp(argv[opt], "-v") == 0)
			verbose = true;
		else if (strcasecmp(argv[opt], "-vv") == 0)
			verbose = debug = true;
		else if (strcasecmp(argv[opt], "-c") == 0)
			concurrent = true;
		else if (strcasecmp(argv[opt], "-b") == 0)
			compare_best = true;
		else {
			if (myrank == 0) {
				fprintf(stderr, "Invalid Argument: %s\n", argv[opt]); 
				Usage();
			}
			return -1;
		}
	}

	if (myrank == 0 && debug == true)
		printf("Settings - verbose %u concurrent %u debug %u\n", 
			verbose, concurrent, debug);

	return 0;
}

/* create pairings */
static void
createPairs(int parallel, pairResults* pairData)
{
	int pair = 0;
	int host = 0;
	int hosts;
	int ranks;
	int myrank;

	/* how many hosts are there? */
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);

	/* what is my rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	hosts = ranks;

	while (hosts > 0)
	{
		/* if creating trial pairs then only create up to trialPairs of them */
		if (trialPass == true && pair > trialPairs - 1)
			return;

		/* if running concurrent tests then pair up each host with one of the
		   other hosts in the list */
		if (parallel == true)
		{
			/* if we still have at least 2 hosts then set them both init a pair */
			if (hosts >= 2)
			{
				pairData[pair].sendRank = host;
				strcpy(pairData[pair].sendHostname, hostList[host++].hostname);  
				pairData[pair].receiveRank = host;
				strcpy(pairData[pair].receiveHostname, hostList[host++].hostname);  
				pairData[pair].alone = false;
				hosts -= 2;
			}
			/* otherwise pair the last one with the first one - but it will
		   	   run later */
			else
			{
				pairData[pair].sendRank = 0;
				strcpy(pairData[pair].sendHostname, hostList[0].hostname);
				pairData[pair].receiveRank = host;
				strcpy(pairData[pair].receiveHostname, hostList[host++].hostname);
				pairData[pair].alone = true;
				hosts -= 1;
			}
		}
		/* must be running sequential tests from the baseline rank */
		else
		{
			if (host == baselineRank)
			{
				host++;
				hosts--;
				continue;
			}
			pairData[pair].sendRank = baselineRank;
			strcpy(pairData[pair].sendHostname, baselineHost);
			pairData[pair].receiveRank = host;
			strcpy(pairData[pair].receiveHostname, hostList[host++].hostname);
			pairData[pair].alone = true;
			hosts -= 1;
		}
		pairData[pair].valid = true;
		if (myrank == 0 && debug == true)
		{
			printf("createPairs() Pair %u send rank %u receive rank %u\n", 
				pair, pairData[pair].sendRank, pairData[pair].receiveRank);
		}
		pair++;
	}
}

/* this is the heart of the latency test */
/* because the test is a ping-pong type latency test, their is no need for a
 * barrier.  The first packet (which is part of warmup) will implicitly
 * synchronize the sender/receiver pair
 */
void do_latency(int pair, int myrank, int size, int loop, pairResults* pairData)
{
	int iterations;
	MPI_Status stat;

	if (pairData[pair].sendRank == myrank)
	{
		for (iterations = 0; iterations < loop + LATENCY_WARMUP; iterations++)
		{
			if (iterations == LATENCY_WARMUP)
			{
				/* snapshot of start time */
				pairData[pair].latencyTimeStart = MPI_Wtime();
			}

			MPI_Send(sendBuffer, size, MPI_CHAR, pairData[pair].receiveRank, 
				iterations, MPI_COMM_WORLD);
			MPI_Recv(receiveBuffer, size, MPI_CHAR, pairData[pair].receiveRank, 
				iterations + 1000, MPI_COMM_WORLD, &stat);
		}
		/* snapshot of end time */
		pairData[pair].latencyTimeEnd = MPI_Wtime();

		if (debug == true)
		{
			printf(
				"Concurrent Latency %.2f usec %s (%u) <-> %s (%u)\n", 
				compute_pair_latency(pair, loop, pairData),
				pairData[pair].sendHostname, pairData[pair].sendRank, 
				pairData[pair].receiveHostname, pairData[pair].receiveRank);
		}
	} else if (pairData[pair].receiveRank == myrank) {
		for (iterations = 0; iterations < loop + LATENCY_WARMUP; iterations++)
		{
			MPI_Recv(receiveBuffer, size, MPI_CHAR, pairData[pair].sendRank, 
				iterations, MPI_COMM_WORLD, &stat);
			MPI_Send(sendBuffer, size, MPI_CHAR, pairData[pair].sendRank, 
				iterations + 1000, MPI_COMM_WORLD);
		}
	}
}

/* run concurrent latency test between pairs */
static void
runConcurrentLatency(int pairs, int doubles, int size, int loop, pairResults* pairData) 
{
	int bytes;
	int pair;
	int validPairs = 0;
	int myrank;

	/* what is my rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* how many pairs are valid */
	for (pair = 0; pair < pairs; pair++)
	{
		if (pairData[pair].valid == true)
			validPairs++;
	}
	if (myrank == 0 && secondPass == false && debug == false && trialPass == false)
		printf("Running Concurrent MPI Latency Tests - Pairs %u   Testing ",
		validPairs);

	if (myrank == 0 && debug == true)
		printf("\n");

	/* initialize the buffer data */
	for (bytes = 0; bytes < size; bytes++)
	{
		sendBuffer[bytes]= 'a';
		receiveBuffer[bytes]= 'b';
	}

	/* now run the latency tests concurrently for all of the complete pairs */
	/* we sync up once at the start, then let each test run independently
	 * so they all run concurrently
	 */
	MPI_Barrier(MPI_COMM_WORLD);
	for (pair = 0; pair < doubles; pair++)
	{
		/* skip if not valid */
		if (pairData[pair].valid == false)
			continue;

		if (myrank == 0 && debug == false && trialPass == false)
		{
			printf("%5u", pair + 1);
			fflush(stdout);
		}
		do_latency(pair, myrank, size, loop, pairData);
		if (myrank == 0 && debug == false && trialPass == false)
		{
			printf("\b\b\b\b\b");
			fflush(stdout);
		}
	}

	MPI_Barrier(MPI_COMM_WORLD); /* just to be safe */
	/* now do the odd pair against rank 0 */
	for (pair = doubles; pair < pairs; pair++)
	{
		/* skip if not valid */
		if (pairData[pair].valid == false)
			continue;

		if (myrank == 0 && debug == false && trialPass == false)
		{
			printf("%5u", pair + 1);
			fflush(stdout);
		}
		do_latency(pair, myrank, size, loop, pairData);
		if (myrank == 0 && debug == false && trialPass == false)
		{
			printf("\b\b\b\b\b");
			fflush(stdout);
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	if (myrank == 0 && trialPass == false)
		printf("\n");

	/* report test pair data */
	hostReport(&pairData[0], /* latency */ true);

	/* wait for all communications to complete */
	MPI_Barrier(MPI_COMM_WORLD);
}

/* run sequential latency test between pairs */
static void
runSequentialLatency(int pairs, int size, int loop, pairResults* pairData) 
{
	int bytes;
	int pair;
	int validPairs = 0;
	int myrank;

	/* what is my rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* how many valid pairs */
	for (pair = 0; pair < pairs; pair++)
	{
		if (pairData[pair].valid == true)
			validPairs++;
	}
	if (myrank == 0 && secondPass == false && debug == false)
		printf("Running Sequential MPI Latency Tests - Pairs %u   Testing ",
		validPairs);

	if (myrank == 0 && secondPass == true && debug == false)
		printf("Second Pass - Running Sequential MPI Latency Tests - Pairs %u   Testing ",
		validPairs);

	if (myrank == 0 && debug == true)
		printf("\n");

	/* initialize the buffer data */
	for (bytes = 0; bytes < size; bytes++)
	{
		sendBuffer[bytes]= 'a';
		receiveBuffer[bytes]= 'b';
	}

	for (pair = 0; pair < pairs; pair++)
	{
		/* skip if not valid */
		if (pairData[pair].valid == false)
		{
			MPI_Barrier(MPI_COMM_WORLD);
			continue;
		}

		if (myrank == 0 && debug == false)
		{
			printf("%5u", pair + 1);
			fflush(stdout);
		}

		MPI_Barrier(MPI_COMM_WORLD);
		do_latency(pair, myrank, size, loop, pairData);
		if (myrank == 0 && debug == false)
		{
			printf("\b\b\b\b\b");
			fflush(stdout);
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	if (myrank == 0)
		printf("\n");

	/* report test pair data */
	hostReport(&pairData[0], /* latency */ true);

	/* wait for all communications to complete */
	MPI_Barrier(MPI_COMM_WORLD);
}

/* a simple way to synchronize two nodes without using a barrier nor
 * collective is to send a round trip packet
 */
void pair_barrier(int pair, int myrank, pairResults* pairData)
{
	MPI_Status stat;

	if (pairData[pair].sendRank == myrank)
	{
		MPI_Send(sendBuffer, 0, MPI_CHAR, pairData[pair].receiveRank, 
				0, MPI_COMM_WORLD);
		MPI_Recv(receiveBuffer, 0, MPI_CHAR, pairData[pair].receiveRank, 
				1000, MPI_COMM_WORLD, &stat);
	} else if (pairData[pair].receiveRank == myrank) {
		MPI_Recv(receiveBuffer, 0, MPI_CHAR, pairData[pair].sendRank, 
			0, MPI_COMM_WORLD, &stat);
		MPI_Send(sendBuffer, 0, MPI_CHAR, pairData[pair].sendRank, 
			1000, MPI_COMM_WORLD);
	}
}

/* this is the heart of the bandwidth test */
void do_bandwidth(int pair, int myrank, int size, int loop,
			 char *s_buf, char *r_buf, pairResults* pairData)
{
	int iterations;

	/* warmup */
	if (pairData[pair].sendRank == myrank)
	{
		if (bandwidthBiDir) {
			for (iterations = 0; iterations < BANDWIDTH_WARMUP; iterations++) 
			{
				MPI_Irecv(r_buf, size, MPI_CHAR, pairData[pair].receiveRank, 
					100, MPI_COMM_WORLD, request2 + iterations);
			}
		}
		for (iterations = 0; iterations < BANDWIDTH_WARMUP; iterations++) 
		{
			MPI_Isend(s_buf, size, MPI_CHAR, pairData[pair].receiveRank, 
				100, MPI_COMM_WORLD, request + iterations);
		}
		MPI_Waitall(BANDWIDTH_WARMUP, request, my_stat);
		if (bandwidthBiDir)
			MPI_Waitall(BANDWIDTH_WARMUP, request2, my_stat);
		MPI_Recv(r_buf, 4, MPI_CHAR, pairData[pair].receiveRank, 
				101, MPI_COMM_WORLD, &my_stat[0]);
	}
	else if (pairData[pair].receiveRank == myrank)
	{
		for (iterations = 0; iterations < BANDWIDTH_WARMUP; iterations++) 
		{
			MPI_Irecv(r_buf, size, MPI_CHAR, pairData[pair].sendRank, 
				100, MPI_COMM_WORLD, request + iterations);
		}
		if (bandwidthBiDir) {
			for (iterations = 0; iterations < BANDWIDTH_WARMUP; iterations++) 
			{
				MPI_Isend(s_buf, size, MPI_CHAR, pairData[pair].sendRank, 
					100, MPI_COMM_WORLD, request2 + iterations);
			}
		}
		MPI_Waitall(BANDWIDTH_WARMUP, request, my_stat);
		if (bandwidthBiDir)
			MPI_Waitall(BANDWIDTH_WARMUP, request2, my_stat);
		MPI_Send(s_buf, 4, MPI_CHAR, pairData[pair].sendRank, 101, MPI_COMM_WORLD);
	}

	pair_barrier(pair, myrank, pairData);

	if (pairData[pair].sendRank == myrank)
	{
		/* snapshot of start time */
		pairData[pair].bandwidthTimeStart = MPI_Wtime();

		if (bandwidthBiDir) {
			for (iterations = 0; iterations < loop; iterations++) 
			{
				MPI_Irecv(r_buf, size, MPI_CHAR, pairData[pair].receiveRank, 
					100, MPI_COMM_WORLD, request2 + iterations);
			}
		}

		for (iterations = 0; iterations < loop; iterations++) 
		{
			MPI_Isend(s_buf, size, MPI_CHAR, pairData[pair].receiveRank, 
				100, MPI_COMM_WORLD, request + iterations);
		}
		MPI_Waitall(loop, request, my_stat);
		if (bandwidthBiDir)
			MPI_Waitall(loop, request2, my_stat);
		MPI_Recv(r_buf, 4, MPI_CHAR, pairData[pair].receiveRank, 
			101, MPI_COMM_WORLD, &my_stat[0]);

		/* snapshot of end time */
		pairData[pair].bandwidthTimeEnd = MPI_Wtime();

		if (debug == true)
		{
			printf(
				"Concurrent Bandwidth %.1f MB/s %s (%u) --> %s (%u)\n",
				compute_pair_bandwidth(pair, bandwidthSize,
					   							bandwidthLoop, pairData),
				pairData[pair].sendHostname, pairData[pair].sendRank, 
				pairData[pair].receiveHostname, pairData[pair].receiveRank);
		}
	}
	else if (pairData[pair].receiveRank == myrank)
	{
		for (iterations = 0; iterations < loop; iterations++) 
		{
			MPI_Irecv(r_buf, size, MPI_CHAR, pairData[pair].sendRank, 
				100, MPI_COMM_WORLD, request + iterations);
		}
		if (bandwidthBiDir) {
			for (iterations = 0; iterations < loop; iterations++) 
			{
				MPI_Isend(s_buf, size, MPI_CHAR, pairData[pair].sendRank, 
					100, MPI_COMM_WORLD, request2 + iterations);
			}
		}
		MPI_Waitall(loop, request, my_stat);
		if (bandwidthBiDir)
			MPI_Waitall(loop, request2, my_stat);
		MPI_Send(s_buf, 4, MPI_CHAR, pairData[pair].sendRank, 101, MPI_COMM_WORLD);
	}
}

/* run concurrent bandwidth test between pairs */
static void
runConcurrentBandwidth(int pairs, int doubles, int size, int loop, pairResults* 
	pairData) 
{
	int bytes;
	int pair;
	int validPairs = 0;
	int myrank;
	int pageSize;
	char *s_buf, *r_buf;

	/* what is my rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* how many are valid */
	for (pair = 0; pair < pairs; pair++)
	{
		if (pairData[pair].valid == true)
			validPairs++;
	}
	if (myrank == 0 && secondPass == false && debug == false && trialPass == false)
		printf("Running Concurrent MPI Bandwidth Tests - Pairs %u   Testing ",
		validPairs);

	if (myrank == 0 && debug == true)
		printf("\n");

	/* get memory page size for this host */
	pageSize = getpagesize();

	/* set send and receive buffer pointers */
	s_buf = (char*)(((unsigned long)sendBuffer + (pageSize -1))/pageSize * pageSize);
	r_buf = (char*)(((unsigned long)receiveBuffer + (pageSize -1))/pageSize * pageSize);
	assert((s_buf != NULL) && (r_buf != NULL));

	/* initialize the buffer data */
	for (bytes = 0; bytes < size; bytes++)
	{
		s_buf[bytes]= 'a';
		r_buf[bytes]= 'b';
	}

	MPI_Barrier(MPI_COMM_WORLD);

	for (pair = 0; pair < doubles; pair++)
	{
		/* skip if not valid */
		if (pairData[pair].valid == false)
			continue;

		if (myrank == 0 && debug == false && trialPass == false)
		{
			printf("%5u", pair + 1);
			fflush(stdout);
		}
		pair_barrier(pair, myrank, pairData);
		do_bandwidth(pair, myrank, size, loop, s_buf, r_buf, pairData);
		if (myrank == 0 && debug == false && trialPass == false)
		{
			printf("\b\b\b\b\b");
			fflush(stdout);
		}
	}

	MPI_Barrier(MPI_COMM_WORLD); /* just to be safe */
	/* now do the odd pair agains rank 0 */
	for (pair = doubles; pair < pairs; pair++)
	{
		/* skip if not valid */
		if (pairData[pair].valid == false)
			continue;

		if (myrank == 0 && debug == false && trialPass == false)
		{
			printf("%5u", pair + 1);
			fflush(stdout);
		}
		pair_barrier(pair, myrank, pairData);
		do_bandwidth(pair, myrank, size, loop, s_buf, r_buf, pairData);
		if (myrank == 0 && debug == false && trialPass == false)
		{
			printf("\b\b\b\b\b");
			fflush(stdout);
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	if (myrank == 0 && trialPass == false)
		printf("\n");

	/* report test pair data */
	hostReport(&pairData[0], /* bandwidth */ false);

	/* wait for all communications to complete */
	MPI_Barrier(MPI_COMM_WORLD);
}

/* run sequential bandwidth test between pairs */
static void
runSequentialBandwidth(int pairs, int size, int loop, pairResults* pairData) 
{
	int bytes;
	int pair;
	int validPairs = 0;
	int myrank;
	int pageSize;
	char *s_buf, *r_buf;

	/* what is my rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* how many pairs are valid */
	for (pair = 0; pair < pairs; pair++)
	{
		if (pairData[pair].valid == true)
			validPairs++;
	}
	if (myrank == 0 && secondPass == false && debug == false)
		printf("Running Sequential MPI Bandwidth Tests - Pairs %u   Testing ",
		validPairs);

	if (myrank == 0 && secondPass == true && debug == false)
		printf("Second Pass - Running Sequential MPI Bandwidth Tests - Pairs %u   Testing ",
		validPairs);

	if (myrank == 0 && debug == true)
		printf("\n");

	/* get memory page size for this host */
	pageSize = getpagesize();

	/* set send and receive buffer pointers */
	s_buf = (char*)(((unsigned long)sendBuffer + (pageSize -1))/pageSize * pageSize);
	r_buf = (char*)(((unsigned long)receiveBuffer + (pageSize -1))/pageSize * pageSize);
	assert((s_buf != NULL) && (r_buf != NULL));

	/* initialize the buffer data */
	for (bytes = 0; bytes < size; bytes++)
	{
		s_buf[bytes]= 'a';
		r_buf[bytes]= 'b';
	}

	MPI_Barrier(MPI_COMM_WORLD);

	for (pair = 0; pair < pairs; pair++)
	{
		/* skip if not valid */
		if (pairData[pair].valid == false)
		{
			MPI_Barrier(MPI_COMM_WORLD);
			continue;
		}

		if (myrank == 0 && debug == false)
		{
			printf("%5u", pair + 1);
			fflush(stdout);
		}

		MPI_Barrier(MPI_COMM_WORLD);
		do_bandwidth(pair, myrank, size, loop, s_buf, r_buf, pairData);
		if (myrank == 0 && debug == false)
		{
			printf("\b\b\b\b\b");
			fflush(stdout);
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	if (myrank == 0)
		printf("\n");

	/* report test pair data */
	hostReport(&pairData[0], /* bandwidth */ false);

	/* wait for all communications to complete */
	MPI_Barrier(MPI_COMM_WORLD);
}

/* report results from other ranks to rank 0 */
static void
hostReport(pairResults* pairData, int latency)
{
	int myrank;
	int ranks;
	int hosts;
	MPI_Status stat;
	pairResults receiveResults;

	/* what is my rank */
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* how many hosts are there? */
	MPI_Comm_size(MPI_COMM_WORLD, &ranks);

	/* send all reports to rank 0 */
	for (hosts = 0; hosts < ranks; hosts++)
	{
		/* skip invalid pairs */
		if (pairData[hosts].valid == false)
			continue;

		if (myrank != 0 && pairData[hosts].sendRank == myrank)
		{
			MPI_Send(&pairData[hosts], sizeof(pairResults), MPI_CHAR, 0, 1, MPI_COMM_WORLD);
#ifdef DEBUG
			printf("Sending latency report from rank %u\n", myrank);
#endif
		}

		if (myrank == 0 && pairData[hosts].sendRank != 0)
		{
			MPI_Recv(&receiveResults, sizeof(pairResults), MPI_CHAR,
				pairData[hosts].sendRank, 1, MPI_COMM_WORLD, &stat);
			if (latency == true)
			{
				pairData[hosts].latencyTimeStart = receiveResults.latencyTimeStart;
				pairData[hosts].latencyTimeEnd = receiveResults.latencyTimeEnd;
#ifdef DEBUG
				printf("Receiving latency report from rank %u\n",
					pairData[hosts].sendRank);
#endif
			}
			else
			{
				pairData[hosts].bandwidthTimeStart = receiveResults.bandwidthTimeStart;
				pairData[hosts].bandwidthTimeEnd = receiveResults.bandwidthTimeEnd;
#ifdef DEBUG
				printf("Receiving bandwidth report from rank %u\n",
					pairData[hosts].sendRank);
#endif
			}
		}
	}

	/* wait for all communications to complete */
	MPI_Barrier(MPI_COMM_WORLD);
}

/* broadcast test pairs to all hosts from rank 0 */
static void
hostBroadcast(pairResults *pairData)
{
	int myrank;
	int ranks;
	int hosts;
	MPI_Status stat;

	/* what is my rank */
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	/* how many hosts are there? */
	MPI_Comm_size(MPI_COMM_WORLD, &ranks);

	/* broadcast pair list to all ranks */
	for (hosts = 0; hosts < ranks; hosts++)
	{
		if (myrank == 0 && hosts != 0)
		{
			MPI_Send(&pairData[0], sizeof(pairResults) * ranks, MPI_CHAR, 
				hosts, 1, MPI_COMM_WORLD);
#ifdef DEBUG
			printf("Broadcasting pair list from rank %u to rank %u\n",
				myrank, hosts);
#endif
		}
	}

	/* receive pair list from rank 0 */
	if (myrank != 0)
	{
		MPI_Recv(&pairData[0], sizeof(pairResults) * ranks, MPI_CHAR,
			0, 1, MPI_COMM_WORLD, &stat);
#ifdef DEBUG
		printf("Rank %u receiving broadcast pair list\n", myrank);
#endif
	}

	/* wait for all communications to complete */
	MPI_Barrier(MPI_COMM_WORLD);
}

/* display usage statement */
static void
Usage()
{
	fprintf(stderr, "Usage: deviation [-bwtol %%] [-bwdelta MBs] [-bwthres MBs]\n"
					"                 [-bwloop count] [-bwsize size] [-bwbidir|-bwunidir]\n"
					"                 [-lattol %%] [-latdelta usec] [-latthres usec]\n"
					"                 [-latloop count] [-latsize size]\n"
				    "                 [-c] [-b] [-v] [-vv] [-h reference_host]\n");

	fprintf(stderr, " -bwtol     Percent of bandwidth degradation allowed below Avg value\n");
	fprintf(stderr, " -bwdelta   Limit in MB/s of bandwidth degradation allowed below Avg value\n");
	fprintf(stderr, " -bwthres   Lower Limit in MB/s of bandwidth allowed\n");
	fprintf(stderr, " -bwloop    Number of loops to execute each bandwidth test\n");
	fprintf(stderr, " -bwsize    Size of message to use for bandwidth test\n");
	fprintf(stderr, " -bwbidir   Perform a bidirectional bandwidth test\n");
	fprintf(stderr, " -bwunidir  Perform a unidirectional bandwidth test (default)\n");
	fprintf(stderr, " -lattol    Percent of latency degradation allowed above Avg value\n");
	fprintf(stderr, " -latdelta  Limit in usec of latency degradation allowed above Avg value\n");
	fprintf(stderr, " -latthres  Upper Limit in usec of latency allowed\n");
	fprintf(stderr, " -latloop   Number of loops to execute each latency test\n");
	fprintf(stderr, " -latsize   Size of message to use for latency test\n");
	fprintf(stderr, " -c         Run test pairs concurrently instead of the default of sequential\n");
	fprintf(stderr, " -b         When comparing results against tolerance and delta use best\n"     "            instead of Avg\n");
	fprintf(stderr, " -v         verbose output\n");
	fprintf(stderr, " -vv        Very verbose output\n");
	fprintf(stderr, " -h         Baseline host to use for sequential pairing\n");
	fprintf(stderr, "Both bwtol and bwdelta must be exceeded to fail bandwidth test when both are supplied\n");
	fprintf(stderr, "When bwthres is supplied, bwtol and bwdelta are ignored\n");
	fprintf(stderr, "Both lattol and latdelta must be exceeded to fail latency test when both are supplied\n");
	fprintf(stderr, "When lathres is supplied, lattol and latdelta are ignored\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "For consistency with OSU benchmarks MB/s is defined as 1000000 bytes/s\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Example of running a test with bandwidth tolerance set to 20%%\n"
		" and latency tolerance set to 30%%, running concurrently, with a known good\n"
		" host compute0001, and running in verbose mode\n");
	fprintf(stderr, "      deviation -bwtol 20 -lattol 30 -c -h compute0001 -v\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "      deviation -bwthres 1200.5 -latthres 3.5 -c -h compute0001 -v\n");
	fprintf(stderr, "      deviation -bwdelta 200 -latdelta 0.5 -bwtol 0 -lattol 0\n");
}

/* Get the name of my host */
static const char* 
pairedHostname()
{
    static char *host = NULL;
    static char buf[256];

    if (!host) 
	{
		gethostname(buf, sizeof buf-1);
		buf[sizeof buf-1] = '\0';
		host = buf;
    }
    return host;
}

/* vi:set ts=4: */

