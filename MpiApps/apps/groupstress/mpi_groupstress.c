/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015-2020, Intel Corporation

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

/*
 * OSU MPI Bandwidth test v2.2
 */
/*
 * Copyright (C) 2002-2006 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University. 
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 */

/*
This program is available under BSD licensing.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

(1) Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

(2) Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

(3) Neither the name of The Ohio State University nor the names of
their contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* mpi_groupstress

The purpose of this program is to stress groups of links in a large
IB fabric. While it generates some basic numbers about how the links
are performing, it is not intended as a benchmark.

*/

#include "mpi.h"
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>

#define  stringize(x) #x
#define  add_quotes(x) stringize(x)

#define  VERBOSE(X,...) if (verbose) fprintf(stderr,X, ## __VA_ARGS__ )
#define  RANK0(X,...) if (verbose && myid == 0) fprintf(stderr,X, ## __VA_ARGS__ )

#define  MIN_MSG_SIZE 16384
#define  MAX_ALIGNMENT 16384
#define  MAX_MSG_SIZE (1<<22)
#define  N_HALF_ERROR_BOUND 0.2
#define  MY_BUF_SIZE (MAX_MSG_SIZE + MAX_ALIGNMENT)

char     s_buf1[MY_BUF_SIZE];
char     r_buf1[MY_BUF_SIZE];

#define  TAG_DATA 100
#define  TAG_SKIP 101
#define  TAG_LOOP 102
#define  TAG_DONE 103

#define  MAX_REQ_NUM 1000

MPI_Request send_request[MAX_REQ_NUM];
MPI_Request recv_request[MAX_REQ_NUM];
MPI_Status  reqstat[MAX_REQ_NUM];

MPI_Comm mpi_comm_sender;

/*
 * Ranks are grouped into "groups". During the test, the ranks in the first 
 * half of each group will pair themselves with matching ranks in the second
 * half of each group. 
 *
 * This permits, correct pairing of ranks in multi-processor nodes.
 * For example, given a fabric made of 4-processor nodes, a group size of 8
 * will allow all the ranks running on node #1 to be paired
 * with ranks running on node #2.
 */
#define  MIN_GROUP_SIZE 2
#define  MAX_GROUP_SIZE 128
#define  DEFAULT_GROUP_SIZE 2

static   int minutes = 60; // how long the test should run.
static   int verbose = 0; // noisy output
static   int group_size = DEFAULT_GROUP_SIZE; // size of the groups
static   int num_groups; // how many groups in the job?
static   int num_procs;  // how many processes in the job?
static   int myid;	   // my rank.
static   int partnerid;  // my partner's rank.
static	 int min_msg_size = MIN_MSG_SIZE;
static   int max_msg_size = MAX_MSG_SIZE;

#if defined(WIDE_PATTERN)
#define PATTERN_SIZE 80
// This pattern is designed to exercise a pattern of different IB symbols
static u_int32_t pattern[] = {
	0x63636363, 		// 00
	0xA3A3A3A3, 		// 01
	0x54545454, 		// 02
	0x47474747, 		// 03
	0x18181818,		// 04
	0x63636363, 		// 05
	0xA3A3A3A3, 		// 06
	0x54545454, 		// 07
	0x47474747, 		// 08
	0x18181818,		// 09
	0x63636363,		// 10 
	0xA3A3A3A3,		// 11 
	0x54545454,		// 12 
	0x47474747,		// 13 
	0x18181818,		// 14
	0x63636363, 		// 15
	0xA3A3A3A3, 		// 16
	0x54545454, 		// 17
	0x47474747, 		// 18
	0x18181818,		// 19
};
#else
#define PATTERN_SIZE 80
// By repeating the same word over and over, we can stress signal integrity
// in a different way...
static u_int32_t pattern[] = {
	0xb5b5b5b5, 		// 00
	0xb5b5b5b5, 		// 01
	0xb5b5b5b5, 		// 02
	0xb5b5b5b5, 		// 03
	0xb5b5b5b5, 		// 04
	0xb5b5b5b5, 		// 05
	0xb5b5b5b5, 		// 06
	0xb5b5b5b5, 		// 07
	0xb5b5b5b5, 		// 08
	0xb5b5b5b5, 		// 09
	0xb5b5b5b5, 		// 10
	0xb5b5b5b5, 		// 11
	0xb5b5b5b5, 		// 12
	0xb5b5b5b5, 		// 13
	0xb5b5b5b5, 		// 14
	0xb5b5b5b5, 		// 15
	0xb5b5b5b5, 		// 16
	0xb5b5b5b5, 		// 17
	0xb5b5b5b5, 		// 18
	0xb5b5b5b5, 		// 19
};
#endif


int window_size_small = 64;
long skip_small = 100;
long min_loops_small = 1000;
long max_loops_small = 60000000000;
double seconds_per_message_size_small = 60.0;

int window_size_large = 64;
long skip_large = 2;
long min_loops_large = 20;
long max_loops_large = 60000000000;
double seconds_per_message_size_large = 60.0;

int large_message_size = 8192;

double 
find_bibw(int myid, int target, int size, char *s_buf, char *r_buf)
{
/*This function is the bandwidth test that was previously part of main in 
  osu_bibw, with the additional modification of being able to stream multiple
  stream multiple processors per node.  As of the InfiniPath 1.3 release
  the code can also dynamically determine which processes are on which
  node and set things up appropriately.*/

  double t_start = 0.0, t_end = 0.0, t = 0.0, max_time = 0.0, min_time = 0.0;
  double seconds_per_message_size, sum_loops, dloops;
  int i, j, window_size;
  long skip, loops = 0, min_loops, max_loops;

  if (size < large_message_size)
  {
    skip = skip_small;
    min_loops = min_loops_small;
    max_loops = max_loops_small;
    window_size = window_size_small;
    seconds_per_message_size = seconds_per_message_size_small;
  }
  else
  {
    skip = skip_large;
    min_loops = min_loops_large;
    max_loops = max_loops_large;
    window_size = window_size_large;
    seconds_per_message_size = seconds_per_message_size_large;
  }

  MPI_Barrier (MPI_COMM_WORLD);
  if (target != -1 && myid < target)
  {
    for (i = 0; i < max_loops + skip; i++)
    {
      if (i == skip)
      {
        MPI_Barrier (MPI_COMM_WORLD);
        t_start = MPI_Wtime();
      }
      for (j = 0; j < window_size; j++)
      {
        MPI_Irecv (r_buf, size, MPI_CHAR, target, TAG_DATA,
                  MPI_COMM_WORLD, recv_request + j);
      }
      for (j = 0; j < window_size; j++)
      {
        MPI_Isend (s_buf, size, MPI_CHAR, target, TAG_DATA,
                  MPI_COMM_WORLD, send_request + j);
      }
      MPI_Waitall (window_size, send_request, reqstat);
      MPI_Waitall (window_size, recv_request, reqstat);
      MPI_Recv (r_buf, 4, MPI_CHAR, target, MPI_ANY_TAG, MPI_COMM_WORLD,
               &reqstat[0]);
      if (reqstat[0].MPI_TAG == TAG_DONE)
      {
        t_end = MPI_Wtime();
	i++;
	break;
      }
    }
    if (t_end == 0.0)
    {
      t_end = MPI_Wtime();
    }
    loops = i - skip;
    t = t_end - t_start;
  }
  else if (target != -1 && myid > target) 
  {
    int tag = TAG_SKIP;
    for (i = 0; i < max_loops + skip; i++)
    {
      if (i == skip)
      {
	tag = TAG_LOOP;
        MPI_Barrier (MPI_COMM_WORLD);
        t_start = MPI_Wtime();
      }
      for (j = 0; j < window_size; j++)
      {
        MPI_Isend (s_buf, size, MPI_CHAR, target, TAG_DATA,
                  MPI_COMM_WORLD, send_request + j);
      }
      for (j = 0; j < window_size; j++)
      {
        MPI_Irecv (r_buf, size, MPI_CHAR, target, TAG_DATA,
                  MPI_COMM_WORLD, recv_request + j);
      }
      MPI_Waitall (window_size, send_request, reqstat);
      MPI_Waitall (window_size, recv_request, reqstat);
      if (tag == TAG_LOOP &&
          (i - skip) >= (min_loops - 1) &&
          MPI_Wtime() - t_start >= seconds_per_message_size)
      {
        MPI_Send (s_buf, 4, MPI_CHAR, target, TAG_DONE, MPI_COMM_WORLD);
	i++;
	break;
      }
      else {
        MPI_Send (s_buf, 4, MPI_CHAR, target, tag, MPI_COMM_WORLD);
      }
    }
    loops = i - skip;
  }
  else
  {
    MPI_Barrier(MPI_COMM_WORLD);
  }
  MPI_Reduce (&t, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, mpi_comm_sender);
  MPI_Reduce (&t, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0, mpi_comm_sender);
  MPI_Reduce (&loops, &max_loops, 1, MPI_INTEGER, MPI_MAX, 0, mpi_comm_sender);
  MPI_Reduce (&loops, &min_loops, 1, MPI_INTEGER, MPI_MIN, 0, mpi_comm_sender);
  dloops = (double) loops;
  MPI_Reduce (&dloops, &sum_loops, 1, MPI_DOUBLE, MPI_SUM, 0, mpi_comm_sender);

  if (myid==0)
  {
    double mbytes = ( (size * 2.0) / (1000 * 1000) ) * sum_loops * window_size;
    double bw = (mbytes / num_groups) / max_time;
    VERBOSE("%d bytes, %.2f MB/s, %ld to %ld loops (range %.2f%%), "
             "%.3f to %.3f secs (range %.2f%%)\n",
             size, bw,
             min_loops, max_loops,
             100.0 * ((double) max_loops / (double) min_loops - 1),
             min_time, max_time,
             100.0 * (max_time / min_time - 1));
    return bw;
  }
  return 0;
}

static char *short_options = "g:vl:u:t:h";
static struct option long_options[] = {
	{ .name = "verbose", .has_arg = 0, .val = 'v' },
	{ .name = "group", .has_arg = 1, .val = 'g' },
	{ .name = "min", .has_arg = 1, .val = 'l' },
	{ .name = "max", .has_arg = 1, .val = 'u' },
    { .name = "time", .has_arg = 1, .val = 't' },
	{ .name = "help", .has_arg = 0, .val = 'h' },
	{ 0 }
};

static char *usage_text[] = {
	"Verbose.",
	"Group size. Should be an even number between " add_quotes(MIN_GROUP_SIZE) " and " add_quotes(MAX_GROUP_SIZE),
	"Minimum Message Size. Should be between " add_quotes(MIN_MSG_SIZE) " and " add_quotes(MAX_MSG_SIZE),
	"Maximum Message Size. Should be between " add_quotes(MIN_MSG_SIZE) " and " add_quotes(MAX_MSG_SIZE),
    "The duration of the test, in minutes. Defaults to 60 minutes. -1 to run forever.",
	"Provides this help text.",
	0
};

static void 
usage() 
{
	int i=0;
	
	if (myid == 0) {
		fprintf(stderr,"\nError processing command line arguments.\n\n");
		fprintf(stderr,"USAGE:\n");
		while (long_options[i].name != NULL) {
			fprintf(stderr, "  -%c/--%-8s %s     %s\n",
				long_options[i].val,
				long_options[i].name,
				(long_options[i].has_arg)?"<arg>":"     ",
				usage_text[i]);
			i++;
		}
		fprintf(stderr,"\n\n");
	}
}

int
main(int argc, char *argv[])
{
	int done = 0;
	int err = 0;
	int c;
	int align_size = getpagesize();
	int size;
	time_t done_time;
	
	char *s_buf = (char*)(((unsigned long)s_buf1 + (align_size - 1)) /
					align_size * align_size);

	char *r_buf = (char*)(((unsigned long)r_buf1 + (align_size - 1)) /
					align_size * align_size);

	double bw=0.0;

	memset(r_buf1,'a',MY_BUF_SIZE);

	for(c=0;c<(MAX_MSG_SIZE-PATTERN_SIZE);c+=PATTERN_SIZE)
		memcpy(s_buf+c,pattern,PATTERN_SIZE);

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	while ( -1 != (c = getopt_long(argc, argv, short_options, long_options, NULL))) {
		switch (c) {
			case 'v': 
				verbose=1; 
				break;

			case 'g': 
				group_size = strtol(optarg, NULL, 0);
				if (group_size < MIN_GROUP_SIZE || group_size > MAX_GROUP_SIZE || (group_size % 2) != 0) {
					usage(); 
					err = -1; 
					goto exit;
				}
				break;
			
			case 'l':
				min_msg_size = strtoul(optarg, NULL, 0);
				if (min_msg_size < MIN_MSG_SIZE || min_msg_size > MAX_MSG_SIZE) {
					usage();
					err = -1; 
					goto exit;
				}
				break;

			case 'u':
				max_msg_size = strtoul(optarg, NULL, 0);
				if (max_msg_size < MIN_MSG_SIZE || min_msg_size > MAX_MSG_SIZE) {
					usage();
					err = -1; 
					goto exit;
				}
				break;

			case 't':
				minutes = strtol(optarg, NULL, 0);
				if (minutes == 0 && strcmp(optarg,"0")) {
					usage();
					err = -1; 
					goto exit;
				}
				break;

			case 'h':
			default:
				usage();
				err = -1; 
				goto exit;
		}
	}

	num_groups = num_procs / group_size;

	MPI_Barrier(MPI_COMM_WORLD);

	/* Calculate my partner's rank. */
	{
		int g; // group #
		int q; // rank within group.

		g = myid / group_size; 
		q = (myid % group_size) + (group_size / 2); 
		if (q > (group_size-1)) q -= group_size;

		partnerid = g * group_size + q;
		if (partnerid >= num_procs) partnerid = -1;
	}

	if (partnerid >= 0) 
	{
		VERBOSE("%d is partnered with %d.\n", myid, partnerid);
	}
	else
	{
		VERBOSE("%d has no partner.\n", myid);
	}
	
	MPI_Comm_split(MPI_COMM_WORLD, (myid<partnerid), myid, &mpi_comm_sender);
	
	if (myid == 0) {
		printf("\n\nMPI_GroupStress BIBW Cable Stress Test\n");
		if (minutes > 0)  {
			printf("%d groups of %d, running for %d minutes.\n", 
					num_groups, group_size, minutes);
			done_time = time(NULL) + minutes*60;
		} else {
			printf("%d groups of %d, running till interrupted.\n", 
					num_groups, group_size);
			done_time=(time_t)-1;
		}

		printf("\n# Size\t\tGroup Agg Bandwidth (MB/s)\tMessages/s\tTime Left\n");
	}

	do {
		for (size = min_msg_size; size <= max_msg_size; size *= 2) 
		{
			bw = find_bibw(myid, partnerid, size, s_buf, r_buf);
			if (myid == 0) 
			{
				double rate = 1000*1000*bw/size;
				int h, m, s;
			
				if (minutes > 0) {
					time_t rt = done_time - time(NULL);

					if (rt<0) rt=0;

					h = rt/3600;
					m = rt/60-(h*60);
					s = (h>0)?0:(rt-(m*60));

					printf("%d\t\t%8.2f\t\t\t%8.2f\t%02d:%02d:%02d\n", 
							size, bw, rate, h, m, s);
					fflush(stdout);
				} else {
					printf("%d\t\t%8.2f\t\t\t%8.2f\t--:--:--\n", 
							size, bw, rate);
					fflush(stdout);
				}
			}
		}
		if (myid == 0) {
			if (min_msg_size != max_msg_size) {
				printf("\n\nMPI_GroupStress BIBW Cable Stress Test\n");
				printf("\n# Size\t\tGroup Agg Bandwidth (MB/s)\tMessages/s\tTime Left\n");
			}
			done = (minutes > 0) && (done_time < time(NULL));
		}
		MPI_Bcast(&done,1,MPI_INT,0,MPI_COMM_WORLD);
	} while (!done);

	VERBOSE("%d at final barrier.\n",myid);
	MPI_Barrier(MPI_COMM_WORLD);

exit:
	VERBOSE("%d at finalize.\n",myid);
	MPI_Finalize();
	return err;
}
