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
 * The purpose of these programs (pgfile_test1 and pgfile_test2)
 * is to demonstrate how to use the pgfile capability in mpirun_rsh.
 *
 * pgfile_test1 should be the first program in the pgfile. It accepts
 * numeric arguments from the pgfile and passes them to the next program in
 * the pgfile, which should be pgfile_test2.
 *
 * pgfile_test2 accepts a message containing a set of numbers from the previous
 * program in the pgfile, adds them to its own arguments and passes the
 * result to the following program. The last instance then passes its result
 * back to pgfile_test1, which prints the final result.
 */
#include <stdio.h>
#include <mpi.h>
main(int argc, char *argv[])
{
    int my_rank;
    int n, n2, i, min, dest, num_tasks, msgtype;
    float args[100], data[100], result[100];
    char rbuff[1000], sbuff[1000];
    int position;
    MPI_Status status;
    
    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    
    /* Get our task id (our rank in the basic group) */
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    /* Get the number of MPI tasks */
    MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);
    
    if (my_rank == 0) {
        fprintf(stderr,"%s cannot be the first program.\n",argv[0]);
        return -1;
    }
    
	/* Just to make the print outs easier to read. */
	sleep(my_rank);

	/* process arguments, skipping argv[0] */
    n = (argc < 100)?argc:100;
    for( i=1 ; i<n ; i++ ){
        args[i-1] = (float)atoi(argv[i]);
    }
	n--; /* account for argv[0] */
    
    
    /* Receive data from previous task. */
	fprintf(stderr,"Task %d waiting for message from task %d\n",
			my_rank, my_rank - 1);
    msgtype = 0;
    MPI_Recv(rbuff, 1000, MPI_PACKED, my_rank - 1,
             msgtype, MPI_COMM_WORLD, &status);
    
    /* Unpack data. */
    position = 0;
    MPI_Unpack(rbuff, 1000, &position, &n2, 1, MPI_INT,
               MPI_COMM_WORLD);
    MPI_Unpack(rbuff, 1000, &position, data, n2, MPI_FLOAT,
               MPI_COMM_WORLD);
    
    /* Do simple calculations with data */
    min = (n<n2)?n:n2;
    for (i=0;i<min;i++) {
        result[i] = args[i] + data[i];
    }
    
    if (n<n2) {
        for (i=min;i<n2;i++) {
            result[i]=data[i];
        }
    } else if (n2<n) {
        for (i=min;i<n;i++) {
            result[i]=args[i];
        }
    }
    
    /* Pack result */
    position = 0;
    MPI_Pack(&n2, 1, MPI_INT, sbuff, 1000, &position,
             MPI_COMM_WORLD);
    MPI_Pack(result, 100, MPI_FLOAT, sbuff, 1000, &position,
             MPI_COMM_WORLD);
    
    /* Send result to next task in the ring. */
    dest = my_rank + 1; 
    if (dest >= num_tasks) {
        dest = 0;
    }
    
	fprintf(stderr,"Task %d sending to task %d\n",my_rank, dest);
    msgtype = 0;
    MPI_Send(sbuff, position, MPI_PACKED, dest, msgtype,
             MPI_COMM_WORLD);
    
    /* Program finished. Exit from MPI */
	MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}

