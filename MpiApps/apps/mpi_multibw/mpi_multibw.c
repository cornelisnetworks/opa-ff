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

/* mpi_multibw - benchmark kernel description and timing methodology.

The benchmark kernel is contained in find_bw and calculates the
aggregate bandwidth achieved across a number of pairs of senders and
receivers. find_bw is parameterized by the message size and the number
of pairs. Additional parameters are passed in as globals - the
window size, the number of skip iterations, the minimum required
number of loops, the maximum possible number of loops and a desired
run-time. The number of loops will be between the minimum and maximum
(inclusive), but the test will terminate before the maximum is reached
if it runs out of desired run-time. Note that each sender/receiver
pair makes this run-time decision independently and thus the number
of loops may vary across the pairs.

Each iteration of the find_bw loop passes a set of messages
from sender to receiver in each pair according to the window size. 
After a receiver gets all of these windowed messages, it responds
with a short message to keep the sender and receiver somewhat
synchonized. There are a number of skip iterations to warm up the
system and get it into a steady state. There is then a barrier to
synchronize all the pairs and the timer is started. After this
synchonization there is no more synchronization across the pairs - they
blast away at their own rate with no attempt in the benchmark to
maintain fairness across the pairs. It was found experimentally that
adding in this synchronization would significantly affect the performance.
Once a pair completes its loops (subject to the desired run-time) 
the end time is noted and the test duration calculated for that pair.

The bandwidth is calculated by aggregating all of the data sent
across all the pairs, and dividing by the *maximum* test duration
across all the pairs. An assumption is that all processes leave
the starting barrier roughly synchronized. As long as the synchronization
skew is small compared to the test run-time (which is a second or more)
then the effect of the skew is negligible. There is no attempt to
sychronize the end times. This forms a time period from the more-or-less
synchronized start to the latest end and the calculation is based 
on the total data sent in this time period. It may be that the pairs 
proceed at different rates due to "unfair" scheduling within the 
MPI implementation. By making the amount of work time-based all pairs
will keep communicating until the end avoiding lost concurrency that
would arise if they all did a fixed amount of work at varying rates.

For large messages the loop granularity is very large and there can be 
considerable periods of time at the end of the time measurement period
where one pair is still working on an iteration while others have 
finished. However, for large messages on most interconnects a single 
pair can max out the bandwidth and there is little need for multi-core 
concurrency to maximize large message bandwidth. For small messages 
the opposite is true. The loop granularity is very small as the 
messages are sent very quickly, while the benefits of multi-core 
concurrency for message rate can be very significant. In this case 
as long as all the processes are kept busy up until close to
the end of the timing period all is well. Hence, the max loop
count is set very large, and it will be the desired run-time that 
terminates the loop.

find_bibw is the bi-directional bandwidth version of find_bw.
*/

#include "mpi.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_REQ_NUM 1000
#define MAX_PROCS 128
#define HOST_NAME_MAX 32

#define MAX_ALIGNMENT 16384
#define MAX_MSG_SIZE (1<<22)
/*N_HALF_ERROR_BOUND is maximum error in % of peak bandwidth for the N/2 
  search. */
#define N_HALF_ERROR_BOUND 0.2
#define MY_BUF_SIZE (MAX_MSG_SIZE + MAX_ALIGNMENT)
#define MANUAL 0
#define AUTOMATIC 1
#define TAG_DATA 100
#define TAG_SKIP 101
#define TAG_LOOP 102
#define TAG_DONE 103
struct pair
{
  int sender;
  int receiver;
}; 

int window_size_small = 64;
int skip_small = 100;
int min_loops_small = 1000;
int max_loops_small = 1000000000;
double seconds_per_message_size_small = 1.0;

int window_size_large = 64;
int skip_large = 2;
int min_loops_large = 20;
int max_loops_large = 1000000000;
double seconds_per_message_size_large = 4.0;

int large_message_size = 8192;
int debug = 0;

char s_buf1[MY_BUF_SIZE];
char r_buf1[MY_BUF_SIZE];
struct pair pair_list[MAX_PROCS];
MPI_Comm mpi_comm_sender;

void usage();
double find_bw(int size, int num_pairs, char *s_buf, char *r_buf);
double find_bibw(int size, int num_pairs, char *s_buf, char *r_buf);
void find_n_half(double maxbw, int num_pairs, char *s_buf,
                 char *r_buf, int myid);
int set_pair_list(int num_pairs);

double (*find_fn)(int size, int num_pairs, char *s_buf, char *r_buf) = find_bw;

MPI_Request send_request[MAX_REQ_NUM];
MPI_Request recv_request[MAX_REQ_NUM];
MPI_Status  reqstat[MAX_REQ_NUM];

int main(int argc, char *argv[])
{

  int myid, num_procs, num_pairs=0, i;
  int size, align_size, org_method = AUTOMATIC;
  char *s_buf, *r_buf;
  double maxbw=0.0, bw=0.0;
  
  MPI_Init(&argc, &argv);
  for (i = 0; i < MAX_PROCS; i++)
  {
    /*initialize to -1 because it is outside the legal range for ranks*/
    pair_list[i].sender = -1;
    pair_list[i].receiver = -1;
  }
  MPI_Comm_size (MPI_COMM_WORLD, &num_procs);
  MPI_Comm_rank (MPI_COMM_WORLD, &myid);

  while ( (i = getopt(argc, argv, "-bhm:")) != -1)
  {
    switch(i)
    {
      case 'b':
	find_fn = find_bibw;
	break;
      case 'h':
        if (myid == 0)
        {
          usage();
          MPI_Abort(MPI_COMM_WORLD, 0);
        }
        break;
      case 'm':
        org_method = MANUAL;
        num_pairs = (int)strtoul(optarg, NULL, 0);
        if ((num_pairs < 1))
        {
          if (myid == 0) {
            fprintf(stderr, "procs/node must be >= 1\n");
            usage();
          }
          MPI_Abort(MPI_COMM_WORLD, 1);
        } 
        break;
      default:
        if ((!optopt) && (!num_pairs) && (strtoul(optarg,NULL,0)))
        {
          num_pairs = (int) strtoul(optarg, NULL, 0);
        }         
        else
        { 
          if (myid == 0)
          {
            usage();
            MPI_Abort(MPI_COMM_WORLD, 1);
          }
        }
     }
  }    

  if(org_method == AUTOMATIC)
  {
    num_pairs = set_pair_list(num_pairs);
  }
  else
  {
    /*do the simple way */
    for (i = 0; i < num_pairs; i++)
    {
      pair_list[i].sender = i;
      pair_list[i + num_pairs].sender = i;
      pair_list[i].receiver = i + num_pairs; 
      pair_list[i + num_pairs].receiver = i + num_pairs;
    } 
    for (i = num_pairs*2; i < num_procs; i++)
    {
      pair_list[i].sender = -1;
      pair_list[i].receiver = -1;
    }
  }
  align_size = getpagesize();
  s_buf =
      (char *) (((unsigned long) s_buf1 + (align_size - 1)) /
                align_size * align_size);
  r_buf =
      (char *) (((unsigned long) r_buf1 + (align_size - 1)) /
                align_size * align_size);
  if (myid == 0) 
  {
    fprintf(stdout, "# PathScale Modified OSU MPI Bandwidth Test\n");
    fprintf(stdout, "(OSU Version 2.2, PathScale $Revision: 1.4 $)\n");
    fprintf(stdout, "# Running on %d procs per node"
            " (%s traffic for each process pair)\n",
            num_pairs,
	    (find_fn == find_bw) ? "uni-directional" : "bi-directional");
    fprintf(stdout, "# Size\t\tAggregate Bandwidth (MB/s)\tMessages/s \n");
  }
  MPI_Barrier (MPI_COMM_WORLD);
  MPI_Comm_split(MPI_COMM_WORLD, pair_list[myid].sender == myid,
                 myid, &mpi_comm_sender);
    /* Loop over bandwidth test */
  for (size = 1; size <= MAX_MSG_SIZE; size *= 2)
  {
    /*Pulled out message test code into function find_bw/find_bibw*/
    bw=find_fn(size, num_pairs, s_buf, r_buf);
    if (myid == 0)
    {
      double rate;
      rate = 1000*1000*bw/size;
      fprintf(stdout, "%d\t\t%f\t\t\t%f\n", size, bw, rate);
      fflush(stdout);
      if(bw>maxbw) /*keep track of maximum bandwidth for N/2*/
      {
        maxbw=bw;
      }
    }
  }
  MPI_Bcast(&maxbw,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
  find_n_half(maxbw, num_pairs, s_buf, r_buf, myid);
  MPI_Finalize();
  return 0;
}

void usage()
{
  extern char *__progname;
  char *cmdname = strrchr(__progname, '/');
  cmdname = strrchr(__progname, '/');
  if(cmdname)
    cmdname++;
  else
    cmdname = __progname;
  printf("Usage: mpirun -np <procs> %s <num_pairs>\n", cmdname);
  printf("Unless the -m switch is used, %s requires you to run on only\n",
         cmdname);
  printf("two separate nodes.  It will pass messages between the two \n");
  printf("nodes with <num_pairs> processes on the first sending to\n");
  printf("the same number on the second.\n\n");
  printf("To run anything other than 1 node to 1 node, use the -m \n");
  printf("switch:\n");
  printf("mpirun -np <procs> %s -m <num_pairs>\n", cmdname);
  printf("This tells %s that you are (m)anually arranging the processes\n",
         cmdname);
  printf("in the hostfile or using -ppn, and will let you do more \n");
  printf("elaborate tests like 1 8-core node sending to 2 4-core nodes.\n\n");
  printf("In this case the first <num_pairs> processes by rank will send\n");
  printf("to the next <num_pairs> processes by rank.\n");
  printf("Use -b for bi-directional bandwidth instead of uni-directional.\n");
  return;
}

double find_bw(int size, int num_pairs, char *s_buf, char *r_buf)
{
/*This function is the bandwidth test that was previously part of main in 
  osu_bw, with the additional modification of being able to stream multiple
  stream multiple processors per node.  As of the InfiniPath 1.3 release
  the code can also dynamically determine which processes are on which
  node and set things up appropriately.*/
  double t_start = 0.0, t_end = 0.0, t = 0.0, max_time = 0.0, min_time = 0.0;
  double seconds_per_message_size, sum_loops, dloops;
  int i, j, myid, target, skip, loops, min_loops, max_loops, window_size;
  for (i = 0; i < size; i++)
  {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }

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
  MPI_Comm_rank (MPI_COMM_WORLD, &myid);
  MPI_Barrier (MPI_COMM_WORLD);
  if (pair_list[myid].sender == myid)
  {
    target = pair_list[myid].receiver;
    for (i = 0; i < max_loops + skip; i++)
    {
      if (i == skip)
      {
        MPI_Barrier (MPI_COMM_WORLD);
        t_start = MPI_Wtime();
      }
      for (j = 0; j < window_size; j++)
      {
        MPI_Isend (s_buf, size, MPI_CHAR, target, TAG_DATA,
                  MPI_COMM_WORLD, send_request + j);
      }
      MPI_Waitall (window_size, send_request, reqstat);
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
  else if (pair_list[myid].receiver == myid)
  {
    int tag = TAG_SKIP;
    target = pair_list[myid].sender;
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
        MPI_Irecv (r_buf, size, MPI_CHAR, target, TAG_DATA,
                  MPI_COMM_WORLD, recv_request + j);
      }
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
    double mbytes = ( (size * 1.0) / (1000 * 1000) ) * sum_loops * window_size;
    double bw = mbytes / max_time;
    if (debug)
    {
      printf("%d bytes, %.2f MB/s, %d to %d loops (range %.2f%%), "
             "%.3f to %.3f secs (range %.2f%%)\n",
             size, bw,
             min_loops, max_loops,
             100.0 * ((double) max_loops / (double) min_loops - 1),
             min_time, max_time,
             100.0 * (max_time / min_time - 1));
    }
    return bw;
  }
  return 0;
}

double find_bibw(int size, int num_pairs, char *s_buf, char *r_buf)
{
/*This function is the bandwidth test that was previously part of main in 
  osu_bibw, with the additional modification of being able to stream multiple
  stream multiple processors per node.  As of the InfiniPath 1.3 release
  the code can also dynamically determine which processes are on which
  node and set things up appropriately.*/
  double t_start = 0.0, t_end = 0.0, t = 0.0, max_time = 0.0, min_time = 0.0;
  double seconds_per_message_size, sum_loops, dloops;
  int i, j, myid, target, skip, loops, min_loops, max_loops, window_size;
  for (i = 0; i < size; i++)
  {
    s_buf[i] = 'a';
    r_buf[i] = 'b';
  }

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
  MPI_Comm_rank (MPI_COMM_WORLD, &myid);
  MPI_Barrier (MPI_COMM_WORLD);
  if (pair_list[myid].sender == myid)
  {
    target = pair_list[myid].receiver;
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
  else if (pair_list[myid].receiver == myid)
  {
    int tag = TAG_SKIP;
    target = pair_list[myid].sender;
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
    double bw = mbytes / max_time;
    if (debug)
    {
      printf("%d bytes, %.2f MB/s, %d to %d loops (range %.2f%%), "
             "%.3f to %.3f secs (range %.2f%%)\n",
             size, bw,
             min_loops, max_loops,
             100.0 * ((double) max_loops / (double) min_loops - 1),
             min_time, max_time,
             100.0 * (max_time / min_time - 1));
    }
    return bw;
  }
  return 0;
}

void find_n_half(double maxbw, int num_pairs, char *s_buf, 
                 char *r_buf, int myid)
{
  double bw = 0.0, error;
  int size = 1, lastsize = 0, top=0, bottom=0, tmp;
  error = (maxbw / 100) * N_HALF_ERROR_BOUND;
  if (myid == 0)
  {
    fprintf(stdout, "Searching for N/2 bandwidth.  Maximum ");
    fprintf(stdout, "Bandwidth of %f MB/s", maxbw);
    fprintf(stdout, "...\n");
    fflush(stdout);
  }
  while ( (size != lastsize) && (size <= MAX_MSG_SIZE) &&
        (bw < (maxbw / 2 - error) || bw > (maxbw / 2 + error)))
  {
    bw = find_fn (size, num_pairs, s_buf, r_buf); 
    if (myid == 0)
    {
      if (bw < (maxbw / 2 - error))
      {
        tmp = size;
        if (lastsize > size || top) /*We have a top! */
        {
          if (lastsize > size) 
          {
            top = lastsize;
          }
          size = size + (top - size) / 2;
        }
        else /*Still searching for a top */
        {
          size = size << 1 ;
        }
        lastsize = tmp;
      }
      if( bw > (maxbw / 2 + error))
      {
        tmp = size;
        if (lastsize < size)
        {
          bottom = lastsize;
        }
        size = size - (size - bottom) / 2;
        lastsize = tmp;
      }
    }
    MPI_Bcast (&size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast (&lastsize, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast (&bw, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  }
  if (myid == 0)
  {
    if (size > MAX_MSG_SIZE) {
        fprintf (stdout, "Unable to find N/2 bandwidth in sizes");
        fprintf (stdout, " from 0 to %d bytes\n", MAX_MSG_SIZE);
    }
    else {
        fprintf (stdout, "Found N/2 bandwidth of %f MB/s at size",bw);
        fprintf (stdout, " %i bytes\n", size); 
    }
  }
  MPI_Barrier (MPI_COMM_WORLD);
  return;
}

int set_pair_list (int num_pairs)
/* pair_list is global.  Also note every rank is doing this, so most errors *
 * will be seen by all ranks.  Thus for many of these it should be safe to  *
 *  have only the first call usage and abort. */
{
  char host_name[HOST_NAME_MAX], host_name_buff[HOST_NAME_MAX*MAX_PROCS];
  char host[2][HOST_NAME_MAX];
  size_t len = HOST_NAME_MAX;
  int num_procs, myid, i, j;
  int proc_counter[2] = {0,0};
  
  MPI_Comm_size (MPI_COMM_WORLD, &num_procs);
  MPI_Comm_rank (MPI_COMM_WORLD, &myid);
  memset (host, 0, HOST_NAME_MAX*2);
  memset (host_name, 0, HOST_NAME_MAX);
  memset (host_name_buff, 0, HOST_NAME_MAX*MAX_PROCS);
  if (gethostname (host_name, len))
  {
    fprintf (stderr, "Error with rank %d getting hostname!\n", myid);
    MPI_Abort (MPI_COMM_WORLD, 1);
  }
  else
  { 
    MPI_Allgather(host_name, HOST_NAME_MAX, MPI_CHAR, host_name_buff, 
                  HOST_NAME_MAX, MPI_CHAR, MPI_COMM_WORLD);
    for (i = 0; i < num_procs; i++)
      {
      /*If the first host is not null and they are not identical */
      if (*host[0] && (strncmp (host[0], &host_name_buff[i*HOST_NAME_MAX],
                                         HOST_NAME_MAX) != 0))
      {
        /*And the second is not null and they are not identical */
        if (*host[1] && (strncmp (host[1], &host_name_buff[i*HOST_NAME_MAX],
                                          HOST_NAME_MAX) != 0))
        {
          if (myid == 0)
          {
            usage();
            fprintf (stderr, "\nMore than 2 hosts identified! Aborting!\n");
            MPI_Abort (MPI_COMM_WORLD,1);
          }
        }
        else 
        {
          /*second host is either null or identical, so can assign */
          memcpy (host[1], &host_name_buff[i*HOST_NAME_MAX], HOST_NAME_MAX);
          /*on second host, so assign to be receiver*/
          pair_list[i].receiver = i;
          /*If num_pairs not specified, don't check this */
          if ( (++proc_counter[1] > num_pairs) && num_pairs)
          {
            if (myid == 0)
            {
              fprintf (stderr, "\nMore than specified %d ", num_pairs);
              fprintf (stderr, "processes found on node 1!  Aborting!\n");
              MPI_Abort (MPI_COMM_WORLD, 1); 
            }
          } 
        }
      }
      else /*first host is either null or identical, so can assign */
      {
        memcpy (host[0], &host_name_buff[i*HOST_NAME_MAX], HOST_NAME_MAX);
        /*on first host, so assign to be sender*/
        pair_list[i].sender = i;
        /*If procs per node not specified, don't check this */
        if ( (++proc_counter[0] > num_pairs) && num_pairs)
        {
          if (myid == 0)
          {
            fprintf (stderr, "\nMore than specified %d ", num_pairs);
            fprintf (stderr, "processes found on node 0!  Aborting!\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
          }
        }
      }
    } /*end for (i = 0; i < num_procs; i++) */
    if ( strncmp (host[1], "", 1) == 0 )  //if we have only 1 node
    {
      if (myid == 0)
      {
        fprintf (stderr, "Only one node detected! Aborting\n");
        MPI_Abort (MPI_COMM_WORLD, 1);
      }
    }
    if (proc_counter[0] != proc_counter[1]) 
   {
      if (myid == 0)
      {
        fprintf (stderr, "Number of processes different on node 0 than\n");
        fprintf (stderr, "on node one!  Aborting!\n");
        MPI_Abort (MPI_COMM_WORLD, 1);
      }
    }
    if (num_pairs == 0) 
    {
      num_pairs = proc_counter[0];
    }
    if (num_pairs > proc_counter[0] || 
        num_pairs > proc_counter[1])
    {
      if (myid == 0)
      {
        fprintf (stderr, "There aren't enough processes on the nodes to \n");
        fprintf (stderr, "run with your specified num_pairs: %i.  Aborting!\n", 
                 num_pairs);
        MPI_Abort (MPI_COMM_WORLD, 1);
      }
    }
        
    /*If we got here we know we have 2 nodes, and every element of pair_list
     *up to num_procs - 1 has either a sender or a receiver.  We want to 
     *walk through and match them all up. */
    for (i = 0; i < num_procs; i++)
    {
      if (pair_list[i].receiver < 0)
      /*If we don't know a receiver*/
      { 
        for (j = 0; j < num_procs; j++)
        /*Loop over the whole list so that we don't have to separately do
        * receiver case.  */
        {
          if (pair_list[j].sender < 0)
          /*Find someone who doesn't know a receiver*/ 
          {
            pair_list[j].sender = i;
            pair_list[i].receiver = j;
            break;
          }
        }
      }
    } /*Should now be all nice and paired up.*/
  }  /* end else of if gethostbyname */
  /* We return num_pairs, in case we figured it out for main */
  return num_pairs;
} 
