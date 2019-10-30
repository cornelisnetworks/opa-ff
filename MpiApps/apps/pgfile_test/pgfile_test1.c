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
    int n, num_tasks, i, final_n, msgtype;
    float data[100], result[100];
    char sbuff[1000], rbuff[1000];
    int position;
    MPI_Status status;
    
    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    
    /* Get our task id (our rank in the basic group) */
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    /* Get the number of MPI tasks and slaves */
    MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);
     
    if (my_rank != 0) {
        fprintf(stderr,"%s should only be the first program.\n",argv[0]);
        return -1;
    }
    
    fprintf(stderr,"num_tasks = %d\n",num_tasks);
    
    /* Begin User Program */
    
	/* actual argument run from 1 to argc. */
    n = (argc < 100)?argc:100;
    for( i=1 ; i<n ; i++ ){
        data[i-1] = (float)atoi(argv[i]);
    }
	n--; /* drop count by 1 to account for argv[0] */
    
    /* Pack initial data to be sent to slave tasks */
    position = 0;
    MPI_Pack(&n, 1, MPI_INT, sbuff, 1000, &position,
             MPI_COMM_WORLD);
    MPI_Pack(data, 100, MPI_FLOAT, sbuff, 1000, &position,
             MPI_COMM_WORLD);
    
    /* Send initial data to next task */
	fprintf(stderr,"task 0 sending to task 1.\n");
    msgtype = 0;
    MPI_Send(sbuff, position, MPI_PACKED, 1, msgtype,
             MPI_COMM_WORLD);
    
    /* Wait for results from slaves */
	fprintf(stderr,"task 0 waiting for message from task %d.\n",num_tasks-1);
    msgtype = 0;
    MPI_Recv(rbuff, 1000, MPI_PACKED, num_tasks-1,
             msgtype, MPI_COMM_WORLD, &status);
    
    position = 0;
    MPI_Unpack(rbuff, 1000, &position, &final_n, 1, MPI_INT,
               MPI_COMM_WORLD);
    MPI_Unpack(rbuff, 1000, &position, result, final_n,
                MPI_FLOAT, MPI_COMM_WORLD);
    
    fprintf(stderr,"Final Results: final_n = %d\n",final_n);
    if (final_n != n) {
        fprintf(stderr,"Failed sanity test. n != final_n.\n");
        return -1;
    }
    
    for( i=0 ; i<n ; i++ ){
        fprintf(stderr,"%d: %6.2f\n",i, result[i]);
    }

    /* Program Finished. Exit MPI before stopping */
	MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
}
