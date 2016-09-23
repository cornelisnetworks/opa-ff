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
// mpi_pinned.c
//
// To run with, for example, Open MPI:
//  mpirun -x LD_LIBRARY_PATH -np 2 -host host1,host2 ./mpi_pinned
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <sys/time.h>
#include <mpi.h>

#define NREPEAT 10
#define NBYTES  10.e6

void usage(const char * name)
{
    printf("Usage: %s -x LD_LIBRARY_PATH \\ \n"
           "          -np 2 -host computer1,computer2 _out/<arch>/mpi_pinned\n",
           name);
}

int main (int argc, char *argv[])
{
    int rank, size, n, len;
    int result;
    void *a_h, *a_d;
    struct timeval time[2];
    double bandwidth;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    MPI_Status status;

    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    MPI_Get_processor_name(hostname, &len);
    printf("Process %d is on %s\n", rank, hostname);

    if (argc > 1 && *argv[1] == 'h')
    {
        usage(argv[0]);
        exit(0);
    }

#ifdef PINNED
    cudaMallocHost( (void **) &a_h, NBYTES);
#else
    a_h = malloc(NBYTES);
#endif

    result = cudaMalloc( (void **) &a_d, NBYTES);
    if (result)
    {
        printf("ERROR: %s: cudaMalloc failed, error code: %d, which means: %s\n",
               hostname, result, cudaGetErrorString(result));
        exit(1);
    }

    /* Test host -> device bandwidth. */

    gettimeofday(&time[0], NULL);
    for (n=0; n<NREPEAT; n++)
    {
        result = cudaMemcpy(a_d, a_h, NBYTES, cudaMemcpyHostToDevice);
        if (result)
        {
            printf("ERROR: %s: cudaMemcpy failed, error code: %d, which means: %s\n",
                   hostname, result, cudaGetErrorString(result));
            exit(1);
        }

    }
    gettimeofday(&time[1], NULL);

    bandwidth  =        time[1].tv_sec  - time[0].tv_sec;
    bandwidth += 1.e-6*(time[1].tv_usec - time[0].tv_usec);
    bandwidth  = NBYTES*NREPEAT/1.e6/bandwidth;

    printf("Host->device bandwidth for process %d: %f MB/sec\n",rank,bandwidth);

    /* Test MPI send/recv bandwidth. */

    MPI_Barrier(MPI_COMM_WORLD);

    gettimeofday(&time[0], NULL);
    for (n=0; n<NREPEAT; n++)
    {
        if (rank == 0)
            MPI_Send(a_h, NBYTES/sizeof(int), MPI_INT, 1, 0, MPI_COMM_WORLD);
        else
            MPI_Recv(a_h, NBYTES/sizeof(int), MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    }
    gettimeofday(&time[1], NULL);

    bandwidth  =        time[1].tv_sec  - time[0].tv_sec;
    bandwidth += 1.e-6*(time[1].tv_usec - time[0].tv_usec);
    bandwidth  = NBYTES*NREPEAT/1.e6/bandwidth;

    if (rank == 0)
        printf("MPI send/recv bandwidth: %f MB/sec\n", bandwidth);

    cudaFree(a_d);

#ifdef PINNED
    cudaFreeHost(a_h);
#else
    free(a_h);
#endif

    MPI_Finalize();
    return 0;
}
