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
 * Copyright (C) 2002-2005 the Network-Based Computing Laboratory
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

/* mpi_latencystress

The purpose of this program is to stress links in a large IB fabric. 
While it generates some basic numbers about how the links
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

#define	 stringize(x) #x
#define	 add_quotes(x) stringize(x)

#define	 DEBUG(X,...) if (verbose>2) printf(X, ## __VA_ARGS__ )
#define	 VERBOSE(X,...) if (verbose>1) printf(X, ## __VA_ARGS__ )
#define	 NORMAL(X,...) if (verbose>0) printf(X, ## __VA_ARGS__ )
#define	 RANK0(X,...) if (verbose && my_id == 0) printf(X, ## __VA_ARGS__ )

#define	 MESSAGE_ALIGNMENT 64
#define	 MIN_MSG_SIZE 0
#define	 MAX_MSG_SIZE (1<<22)
#define	 MY_BUF_SIZE (MAX_MSG_SIZE + MESSAGE_ALIGNMENT)
#define  DEFAULT_MINUTES 5

#define  TAG_BASIC 1000
#define  TAG_RSLT1 1001
#define  TAG_RSLT2 1002

/*
 * Command line args.
 */
static	 int minutes = DEFAULT_MINUTES; // how long the test should run.
static	 int verbose = 0; // noisy output
static	 int size = MIN_MSG_SIZE;
static   int csv = 0; // generate CSV file

static	 int num_procs;	 // how many processes in the job?
static	 int my_id;	   // my rank.

#define PATTERN_SIZE 80
static u_int32_t pattern[] = {
    0x63636363,
    0xA3A3A3A3,
    0x54545454,
    0x47474747,
    0x18181818,
    0x63636363,
    0xA3A3A3A3,
    0x54545454,
    0x47474747,
    0x18181818,
    0x63636363,
    0xA3A3A3A3,
    0x54545454,
    0x47474747,
    0x18181818,
    0x63636363,
    0xA3A3A3A3,
    0x54545454,
    0x47474747,
    0x18181818,
};

char	 s_buf1[MY_BUF_SIZE];
char	 r_buf1[MY_BUF_SIZE];

struct	 partner {
	int	 inuse;
	int	 sender;
	int	 receiver;
};

#define	 MAX_HOST_LEN 32
struct	 host { char name[MAX_HOST_LEN]; };

static	 struct partner *pair_list;
static	 struct host *host_list;
static	 short *checked;
static	 double *latency;
static	 unsigned long psize;
static	 unsigned long csize;

static void
dump_checked(int ranks)
{
	int i, j;
	
	printf("    ");
	for (i = 0; i< ranks; i++) {
		printf(":%4d",i);
	}
	printf("\n");
	
	for (j = 0; j < ranks; j++) {
		printf("%4d",j);
		for (i = 0; i < ranks; i++) {
			if (checked[j*ranks+i]) {
				printf(": %02u ", checked[j*ranks+i]);
			} else {
				printf(":    ");
			}
		}
		printf("\n");
	}
}

static int
calculate_pairs(int iteration, int ranks)
{
	int i,j, k, pairs_found, match_found;
	
	memset(pair_list,0,psize);
	
	pairs_found = 0;

	if (iteration > ranks) return 0;

	for (i=0; i<ranks; i++) {
		match_found = 0;
		if (pair_list[i].inuse == 0) {
			for (k=ranks-i-iteration; k>i-ranks; k--) {
				j = (k>=0)?k:(k+ranks);
				if (i == j) {
					// Can't test against yourself.
					break;
				} else if ((pair_list[j].inuse == 0) &&
					(checked[i*ranks+j]==0)) {
					pair_list[i].inuse=1;
					pair_list[i].sender=i;
					pair_list[i].receiver=j;
					pair_list[j]=pair_list[i];
					match_found=1;
					checked[i*ranks+j]=iteration;
					checked[j*ranks+i]=iteration;
					break;
				} else if ((pair_list[j].inuse != 0))
					DEBUG("collision on [%d,%d] (%d)\n",
							i, j, k);

			}
			if (!match_found) {
				// This will happen if
				// ranks is not a power of 2.
				pair_list[i].sender=-1;
				pair_list[i].receiver=-1;
				pair_list[i].inuse=1;
				VERBOSE("%d is idle this iteration.\n", i);
			} else {
				pairs_found++;
			}
		}
	}	
	
	return pairs_found;
}

/*
 * Taken from osu_latency and converted to a function.
 */
int skip = 1000;
int loop = 10000;
int skip_large = 10;
int loop_large = 100;
int large_message_size = 8192;

static void 
find_latency(int ranks, int size, char *s_buf, char *r_buf, 
			double *min_lat, double *max_lat, double *avg_lat,
			int *min_rank, int *max_rank)
{
	int i;
	MPI_Status reqstat;
	MPI_Comm mpi_comm_sender;
	MPI_Request request1, request2;
	int partner;
	double lat;
	double summary_f[3];
	int	   summary_i[2];
	int sender_id;

	double t_start = 0.0, t_end = 0.0;

	//VERBOSE("%d @ find_latency(%d, %d, %p, %p)\n",
	//		my_id, ranks, size, s_buf, r_buf);
			
	if (pair_list[my_id].sender == my_id) {
		partner = pair_list[my_id].receiver;
		//VERBOSE("%d -> %d\n", my_id, partner);
	} else if (pair_list[my_id].receiver == my_id) {
		partner = pair_list[my_id].sender;
		//VERBOSE("%d <- %d\n", my_id, partner);
	} else {
		//VERBOSE("%d IDLE\n",my_id);
		partner = -1;
	}
	
	//VERBOSE("%d @ buffers loaded.\n",my_id);
	
	if (size > large_message_size) {
		loop = loop_large;
		skip = skip_large;
	}
	
//	VERBOSE("%d @ barrier.\n", my_id);
	MPI_Barrier(MPI_COMM_WORLD);
	
	if (pair_list[my_id].sender == my_id) {
//		VERBOSE("%d @ sending.\n", my_id);
		MPI_Comm_split(MPI_COMM_WORLD, 1, my_id, &mpi_comm_sender);
		for (i = 0; i < loop + skip; i++) {
			if (i == skip)
				t_start = MPI_Wtime();
			MPI_Send(s_buf, 
					 size, 
					 MPI_CHAR, 
					 partner, 
					 TAG_BASIC, 
					 MPI_COMM_WORLD);
			MPI_Recv(r_buf, 
					 size, 
					 MPI_CHAR, 
					 partner, 
					 TAG_BASIC, 
					 MPI_COMM_WORLD,
					 &reqstat);
		}
		t_end = MPI_Wtime();
	
	} else if (pair_list[my_id].receiver == my_id) {
//		VERBOSE("%d @ receiving.\n", my_id);
		MPI_Comm_split(MPI_COMM_WORLD, 2, my_id, &mpi_comm_sender);
		for (i = 0; i < loop + skip; i++) {
			MPI_Recv(r_buf, 
					 size, 
					 MPI_CHAR, 
					 partner, 
					 TAG_BASIC, 
					 MPI_COMM_WORLD,
					 &reqstat);
			MPI_Send(s_buf, 
					 size, 
					 MPI_CHAR, 
					 partner, 
					 TAG_BASIC, 
					 MPI_COMM_WORLD);
		}
	} else {
		MPI_Comm_split(MPI_COMM_WORLD, 3, my_id, &mpi_comm_sender);
	}

//	VERBOSE("%d @ collectives.\n", my_id);
	
	if (pair_list[my_id].sender == my_id) {
		lat = (t_end - t_start) * 1.0e6 / (2.0 * loop);

		VERBOSE("t_start = %f, t_end = %f, loop = %d, lat = %f\n", t_start, t_end, loop, lat);

		MPI_Reduce(&lat, &summary_f[0], 1, MPI_DOUBLE, MPI_MIN, 0, mpi_comm_sender);
		MPI_Reduce(&lat, &summary_f[1], 1, MPI_DOUBLE, MPI_MAX, 0, mpi_comm_sender);
		MPI_Reduce(&lat, &summary_f[2], 1, MPI_DOUBLE, MPI_SUM, 0, mpi_comm_sender);
		MPI_Gather(&lat, 1, MPI_DOUBLE, latency, 1, MPI_DOUBLE, 0, mpi_comm_sender); 

		MPI_Comm_rank(mpi_comm_sender, &sender_id);
	} else {
		sender_id = -1;
	} 
	  
	MPI_Barrier(MPI_COMM_WORLD);

	// It is possible for the root of the senders to be different from
	// the global rank 0. So, the root of the senders will send 
	// a summary of the results to rank 0, even though this is usually
	// redundant.
	if (sender_id == 0) {
		int j=0;

		for (i=0;i<ranks; i++) {
			// note that ranks in mpi_comm_sender do NOT match
			// ranks in MPI_COMM_WORLD. We need to only show
			// the ranks that actually send data this iteration.
			if (pair_list[i].sender == i) {
				if (csv) {
					printf("%s, %d, %s, %d, %0.2f\n",
						host_list[pair_list[i].sender].name, pair_list[i].sender,
						host_list[pair_list[i].receiver].name, pair_list[i].receiver,
						latency[j]);
					fflush(stdout);
				}	
				// The following bit of jiggery-pokery is so I can identify which rank was
				// slowest and which was fastest.
				if (latency[j] == summary_f[0]) {
					summary_i[0] = i;
				}
				if (latency[j] == summary_f[1]) {
					summary_i[1] = i;
				}

				j++;
			//} else if (pair_list[i].sender == -1) {
			//	NORMAL("%"add_quotes(MAX_HOST_LEN)"s[%d] -> idle\n", host_list[i].name, i);
			}
		}

		summary_f[2] = (j)?(summary_f[2] / j):0.0;

		MPI_Isend(summary_f,
				  3,
				  MPI_DOUBLE,
				  0,
				  TAG_RSLT1,
				  MPI_COMM_WORLD,
				  &request1);
		MPI_Isend(summary_i,
				  2,
				  MPI_INT,
				  0,
				  TAG_RSLT2,
				  MPI_COMM_WORLD,
				  &request2);
	} 

	if (my_id == 0) {
		MPI_Recv(summary_f, 
				 3, 
				 MPI_DOUBLE, 
				 MPI_ANY_SOURCE, 
				 TAG_RSLT1, 
				 MPI_COMM_WORLD,
				 &reqstat);
		MPI_Recv(summary_i, 
				 2, 
				 MPI_INT, 
				 MPI_ANY_SOURCE, 
				 TAG_RSLT2, 
				 MPI_COMM_WORLD,
				 &reqstat);

		*min_lat = summary_f[0];
		*max_lat = summary_f[1];
		*avg_lat = summary_f[2];
		*min_rank = summary_i[0];
		*max_rank = summary_i[1];
	}

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Comm_free(&mpi_comm_sender);

	//VERBOSE("%d @ done.\n", my_id);
}

static char *short_options = "s:vt:ch";
static struct option long_options[] = {
	{ .name = "verbose", .has_arg = 0, .val = 'v' },
	{ .name = "size", .has_arg = 0, .val = 's' },
	{ .name = "time", .has_arg = 1, .val = 't' },
	{ .name = "csv", . has_arg = 0, .val = 'c' },
	{ .name = "help", .has_arg = 0, .val = 'h' },
	{ 0 }
};

static char *usage_text[] = {
	"Verbose. Outputs some debugging information. Use multiple times for more detailed information.",
	"Message Size. Should be between " add_quotes(MIN_MSG_SIZE) " and " add_quotes(MAX_MSG_SIZE),
	"The duration of the test, in minutes. Defaults to "
		add_quotes(DEFAULT_MINUTES) " minutes or use -1 to run forever.",
	"Outputs raw data in a CSV file format, suitable for use in Excel."
	"Provides this help text.",
	0
};

static void 
usage() 
{
	int i=0;
	
	if (my_id == 0) {
		fprintf(stderr,"\nError processing command line arguments.\n\n");
		fprintf(stderr,"USAGE:\n");
		while (long_options[i].name != NULL) {
			fprintf(stderr, "  -%c/--%-8s %s	 %s\n",
				long_options[i].val,
				long_options[i].name,
				(long_options[i].has_arg)?"<arg>":"		",
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
	int c, i;
	int align_size = MESSAGE_ALIGNMENT;
	time_t done_time;

	//int DebugWait = 1; // used to attach gdb.

	char *s_buf = (char*)(((unsigned long)s_buf1 + (align_size - 1)) /
					align_size * align_size);

	char *r_buf = (char*)(((unsigned long)s_buf1 + (align_size - 1)) /
					align_size * align_size);

	memset(r_buf1,'a',MY_BUF_SIZE);

	for(c=0;c<(MAX_MSG_SIZE-PATTERN_SIZE);c+=PATTERN_SIZE)
		memcpy(s_buf+c,pattern,PATTERN_SIZE);

	int min_rank, max_rank, num_pairs;
	struct partner round_fastest, round_slowest;
	int found_fastest = 0;
	int found_slowest = 0;
	double min_lat, max_lat;
	double avg_lat = 0;
	double final_min = 99999999.0, final_max = 0.0;
	double round_min, round_max;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_id);

	//if (my_id == 0) while (DebugWait); // used to attach gdb.

	while ( -1 != (c = getopt_long(argc, argv, short_options, long_options, NULL))) {
		switch (c) {
			case 'v': 
				verbose += 1; 
				break;

			case 's':
				size = strtoul(optarg, NULL, 0);
				if (size < MIN_MSG_SIZE || size > MAX_MSG_SIZE) {
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

			case 'c': 
				csv = 1;
				break;
			case 'h':
			default:
				usage();
				err = -1; 
				goto exit;
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);

	RANK0("Allocating buffers.\n");
	psize = sizeof (struct partner) * num_procs;
	pair_list = malloc(psize);
	csize = sizeof(short) * num_procs * num_procs;
	checked = malloc(csize);
	latency = malloc(sizeof(double)*num_procs);
	host_list = malloc(sizeof(struct host)*num_procs);
	
	if (!pair_list || !checked || !latency || !host_list) {
		fprintf(stderr,"malloc failed.\n");
		err = -1;
		goto exit;
	}
	
	// Broadcast the hostnames.
	{
		struct host myname;
		gethostname(myname.name, MAX_HOST_LEN-1);
		myname.name[MAX_HOST_LEN-1]='\0';
		//VERBOSE("%d hostname: %s\n",my_id,myname.name);
		memset(host_list, 0, sizeof(struct host)*num_procs);
		MPI_Allgather(&myname, sizeof(myname), MPI_CHAR, host_list, sizeof(myname), MPI_CHAR, MPI_COMM_WORLD); 
	}
	
	if (my_id == 0) {
		if (minutes > 0) {
			done_time = time(NULL) + minutes*60;
		} else {
			done_time = (time_t)-1;
		}
	}

	do {
		memset(checked,0,csize);
		if ((my_id == 0) && !csv) {
			printf("\n\nMPI HCA Latency Stress Test\n");
			printf("Msg Size:\t%d\n",size);
			if (minutes > 0) {
				long rt = done_time-time(NULL);
				if (rt > 3600) {
					printf("Time Left:\t%ld hours and %ld minutes\n",
						rt/3600,
						rt/60 - (rt/3600)*60);
				} else if (rt > 60) {
					printf("Time Left:\t%ld minutes and %ld seconds\n",rt/60,
						rt - (rt/60)*60);
				} else {
					printf("Time Left:\t%ld seconds\n",(rt > 0)?rt:0);
				}
			} else {
				printf("Time Left:\ttil interrupted.\n");
			}
		}

		round_max = 0.0;
		round_min = 99999999.0;
		num_pairs = 1;

		for (i=1; num_pairs > 0; i++) {
			if (my_id == 0) {
				// Rank 0 calculates the pairs and distributes the
				// info to other nodes.
				int j;

				for (j=0;j<num_procs;j++)
					latency[j]=-1.0;

				NORMAL("Iteration #%d\n",i);
				num_pairs = calculate_pairs(i, num_procs);
			
				// If we found no pairs to test, time to stop.
				if (num_pairs == 0) {
					NORMAL("No pairs left to test.\n");
					pair_list[0].inuse = -2;
					if (verbose>1) dump_checked(num_procs);
				} else if (verbose>2) {
					dump_checked(num_procs);
				}
			}

			MPI_Bcast(pair_list, 
					  psize, 
					  MPI_UNSIGNED_CHAR, 
					  0, 
					  MPI_COMM_WORLD);

			// If rank 0 says there are no pairs to test, 
			// then it's time to stop.
			if (pair_list[0].inuse == -2) {
				break;
			}

			find_latency(num_procs, 
						 size, 
						 s_buf, 
						 r_buf, 
						 &min_lat, 
						 &max_lat, 
						 &avg_lat, 
						 &min_rank, 
						 &max_rank);	
			
			
			if (my_id == 0 && min_lat < round_min) {
				round_min = min_lat;
				round_fastest = pair_list[min_rank];
				found_fastest=1;
				if (round_min < final_min) final_min = round_min;
			}
			if (my_id == 0 && max_lat > round_max) {
				round_max = max_lat;
				round_slowest = pair_list[max_rank]; 
				found_slowest = 1;
				if (round_max > final_max) final_max = round_max;
			}	
		}
	
		MPI_Barrier(MPI_COMM_WORLD);

		if (my_id == 0) {
			if (!csv) {
				printf("Avg Latency:\t%0.2f\n",avg_lat);
				if(found_fastest) {
					printf("Fastest Pair:\n%"add_quotes(MAX_HOST_LEN)"s -> %"add_quotes(MAX_HOST_LEN)"s\t%0.2f\n",
						host_list[round_fastest.sender].name,
						host_list[round_fastest.receiver].name,
						round_min);
				}
				if(found_slowest) {
					printf("Slowest Pair:\n%"add_quotes(MAX_HOST_LEN)"s -> %"add_quotes(MAX_HOST_LEN)"s\t%0.2f\n",
						host_list[round_slowest.sender].name,
						host_list[round_slowest.receiver].name,
						round_max);
				}
			} 
			done = (minutes > 0) && (done_time < time(NULL));
		}
		MPI_Bcast(&done,1,MPI_INT,0,MPI_COMM_WORLD);
	} while (!done);

	if (my_id == 0) {
		fprintf(stderr,"\n\nMPI HCA Latency Stress Test\n");
		fprintf(stderr,"Msg Size:\t%d\n",size);
		fprintf(stderr,"Final Min:\t%0.2f\n",final_min);
		fprintf(stderr,"Final Max:\t%0.2f\n",final_max);
	}

exit:
	//VERBOSE("%d at finalize.\n",my_id);
	MPI_Finalize();
	return err;
}
