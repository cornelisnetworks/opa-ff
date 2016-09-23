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
 * The purpose of this application is to do some simple validations of MPI
 * communications. Unlike other benchmark apps is is not intended to run
 * quickly, but to validate the results of each transfer to ensure the
 * correct operation of MPI.
 *
 * The code is deliberately written to be inefficient; in order to (hopefully)
 * expose communication errors and memory corruption issues.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <mpi.h>
#include <time.h>

/* 
 * Defines used by randomize test.
 */
#define SHUFFLERATE 64
#define MINRANDXFER 1024
#define MAXRANDXFER 32768
#define NUMRANDXFERS 65536
#define RANDOMLEN() (MINRANDXFER + (random() % (MAXRANDXFER-MINRANDXFER)))

#define MAX(a,b) ((a>b)?a:b)
#define MIN(a,b) ((a<b)?a:b)

/* 
 * Prints the message if nodes rank is 0.
 */
#define ROOTPRINT(format,args...) { if (myid == 0) fprintf(stderr,format,##args); }

/* 
 * Prints an error, including node rank and line of code.
 */
#define ERRPRINT(format,args...) { fprintf(stderr,"[%d/%d]: ERROR: ",myid,__LINE__); fprintf(stderr,format,##args); }

/* 
 * Prints a message if and only if verbose mode is on. Includes
 * rank and line of code.
 */
#define NODEPRINT(format,args...) if (verboseMode) { fflush(stdout); printf("[%d/%d]: ",myid,__LINE__); printf(format,##args); }

/* 
 * Force a core dump.
 */
#define ERRABORT(retCode) if (retCode != MPI_SUCCESS) { fprintf(stderr,"MPI Failure: %d at line %d\n",retCode,__LINE__); abort(); }

/* 
 * Padding used to look for corruption before or after a read
 */
#define BARRIERSIZE 4096
unsigned int minSize = 16384;	/* Default smallest message size */
unsigned int maxSize = 65536;	/* Default largest message size */
unsigned int maxIters = 1000;	/* Default # of rounds */
int useSendOffset = 0;	/* Default - don't do offest sends. */
unsigned int rawBufferCount = 1;	
unsigned int barrierSize = 4096;	/* Default - 4k protection areas. */

int verboseMode = 0;
int doFast = 0;
int doSlow = 1;
int doRandom = 1;

struct option options[] = {
	{"min", required_argument, NULL, '<'},
	{"max", required_argument, NULL, '>'},
	{"rounds", required_argument, NULL, 'i'},
	{"buffers", required_argument, NULL, 'b'},
	{"barrier", required_argument, NULL, 'B'},
	{"verbose", no_argument, &verboseMode, 1},
	{"fast", no_argument, &doFast, 1},
	{"nofast", no_argument, &doFast, 0},
	{"slow", no_argument, &doSlow, 1},
	{"noslow", no_argument, &doSlow, 0},
	{"random", no_argument, &doRandom, 1},
	{"norandom", no_argument, &doRandom, 0},
	{"sendoffset", no_argument, &useSendOffset, 1},
	{NULL, 0, NULL, 0}
};

void
usage(  )
{
	int i = 0;

	fprintf( stderr, "Usage: mpicheck " );
	for ( i = 0; options[i].name != NULL; i++ ) {
		if ( options[i].has_arg == required_argument ) {
			fprintf( stderr, "[--%s ###] ", options[i].name );
		} else {
			fprintf( stderr, "[--%s] ", options[i].name );
		}
	}
	fprintf( stderr, "\n" );
}

/* 
 * Writes a transfer buffer to disk for post-mortem analysis.
 */
void
dumpbuffer( int myid, char *name, char *buffer, int size )
{
	char fname[1024];
	FILE *f;
	size_t s;

	sprintf( fname, "%s.%03d", name, myid );

	f = fopen( fname, "w" );

	if ( !f ) {
		ERRPRINT( "Failed to open %s\n", fname );
		return;
	}

	s = fwrite( buffer, 1, size, f );
	if ( s != size ) {
		ERRPRINT( "Failed to write %s\n", fname );
	}

	fclose( f );
}

typedef struct {
	unsigned int start;
	unsigned int length;
} RandomRecord;

static void
initrandom(  )
{
	unsigned int seed;
	FILE *f = fopen( "/dev/random", "r" );

	if ( f ) {
		fread( &seed, sizeof( seed ), 1, f );
	}
	fclose( f );
	srandom( seed );
}

/* 
 * Randomize mode pairs up the nodes and performs a "drunkards" walk
 * between them - it performs a large number of sends and receives
 * designed to stress operations when asynchronous messages overlap.
 *
 * Note that randomize does not use the "rounds" setting that the
 * other tests use.
 */
int
randomize( int myid, int numProcs, int minSize, int maxSize, int maxIters )
{
	int i, size, partner, sendMode, numRecords, iters;
	int retCode = MPI_SUCCESS;
	RandomRecord records[NUMRANDXFERS];
	MPI_Request request[NUMRANDXFERS];
	MPI_Status status[NUMRANDXFERS];
	unsigned char *rawBuffer = NULL;	/* Array of buffer+barrier memory
										 * regions. */
	unsigned char *buffer = NULL;	/* Pointer into rawBuffer. */
	unsigned int totalBufferSize = BARRIERSIZE * 2 + maxSize;

	ROOTPRINT( "Beginning Randomize Test Section\n" );
	ROOTPRINT( "(Node sends random length messages to each other.)\n" );

	initrandom(  );

	memset( request, 0, sizeof( MPI_Request ) * NUMRANDXFERS );
	memset( status, 0, sizeof( MPI_Status ) * NUMRANDXFERS );

	sendMode = ( myid % 2 ) == 0;

	if ( sendMode ) {
		partner = myid + 1;
	} else {
		partner = myid - 1;
	}

	rawBuffer = malloc( totalBufferSize );
	if ( !rawBuffer ) {
		ERRPRINT( "Failed to allocate buffers.\n" );
		retCode = ~MPI_SUCCESS;
		goto done;
	}

	buffer = rawBuffer + BARRIERSIZE;
	size = minSize;

	while ( size <= maxSize ) {
		int round = 0;
		iters = maxIters;
		while ( iters > 0 ) {
			/* Initialize the buffer to our rank as a pad value. */
			memset( rawBuffer, myid, totalBufferSize );

			if ( sendMode ) {
				/* Initialize the buffer with some random data. */
				/* Every 4th byte is a check sum of the previous 3. */
				for ( i = 0; i < ( size - 3 ); i += 4 ) {
					buffer[i] = random(  ) % 256;
					buffer[i + 1] = random(  ) % 256;
					buffer[i + 2] = random(  ) % 256;
					buffer[i + 3] =
						( buffer[i] + buffer[i + 1] + buffer[i + 2] ) % 256;
				}

				/* Build a list of randomly sized messages to send. */
				/* Note that we effectively "null terminate" the list. */
				memset( records, 0, sizeof( RandomRecord ) * NUMRANDXFERS );
				i = 0;
				numRecords = 0;
				while ( i < size ) {
					records[numRecords].start = i;
					records[numRecords].length = RANDOMLEN(  );
					if ( records[numRecords].length + i > size ) {
						records[numRecords].length = size - i;
					}
					i += records[numRecords].length;
					numRecords++;
				}

				/* Note that numRecords is now the length of records. */

				/* Swap a few records, for extra evil. */
				i = random(  ) % SHUFFLERATE;
				while ( i < NUMRANDXFERS && records[i].length ) {
					unsigned int j;

					j = i + ( random(  ) % SHUFFLERATE );
					if ( ( j != i ) && ( j < NUMRANDXFERS )
						 && ( records[j].length ) ) {
						RandomRecord r;

						r.start = records[j].start;
						r.length = records[j].length;
						records[j].start = records[i].start;
						records[j].length = records[i].length;
						records[i].start = r.start;
						records[i].length = r.length;
					}
					i = j;
				}

				/* Dump a list of the records. */
				if ( verboseMode ) {
					for ( i = 0; ( i < NUMRANDXFERS ) && ( records[i].length );
						  i++ ) {
						NODEPRINT( "X %d - %d\n", records[i].start,
								   records[i].length );
					}
				}

				/* Send the record list to our partner. */
				retCode = MPI_Send( records,
									NUMRANDXFERS * sizeof( RandomRecord ),
									MPI_BYTE, partner, 0, MPI_COMM_WORLD );
				ERRABORT( retCode );

				for ( i = 0; ( i < NUMRANDXFERS ) && ( records[i].length );
					  i++ ) {
					NODEPRINT( "S %d - %x\n", records[i].start,
							   records[i].length );
					retCode = MPI_Isend( &buffer[records[i].start], 
										 records[i].length,
								   		 MPI_BYTE, partner, i, MPI_COMM_WORLD,
								   		 &request[i] );
					ERRABORT( retCode );
					round++;
					if (!verboseMode && (round % 100) == 0)
						ROOTPRINT(".");
				}
			} else {
				/* Get the record list from our partner. */
				retCode = MPI_Recv( records,
									NUMRANDXFERS * sizeof( RandomRecord ),
									MPI_BYTE,
									partner, 0, MPI_COMM_WORLD, &status[0] );
				ERRABORT( retCode );

				for ( i = 0; ( i < NUMRANDXFERS ) && ( records[i].length );
					  i++ ) {
					NODEPRINT( "R %d - %x\n", records[i].start,
							   records[i].length );
					retCode = MPI_Irecv( &buffer[records[i].start], 
										 records[i].length,
								   		 MPI_BYTE, partner, i, MPI_COMM_WORLD,
								   		 &request[i] );
					ERRABORT( retCode );
				}
				numRecords = i;	/* Save the # of records. */
			}

			retCode = MPI_Waitall( numRecords, request, status );
			ERRABORT( retCode );
			for ( i = 0; i < numRecords; i++ ) {
				ERRABORT( status[i].MPI_ERROR );
			}

			/* Check for corruption. */
			if ( !sendMode ) {
				for ( i = 0; i < ( size - 3 ); i += 4 ) {
					unsigned char c =
						( buffer[i] + buffer[i + 1] + buffer[i + 2] ) % 256;
					if ( c != buffer[i + 3] ) {
						ERRPRINT( "Corruption in the buffer @ %d\n", i );
						dumpbuffer( myid, "rawBuffer", (char*)rawBuffer,
									size + 2 * BARRIERSIZE );
						retCode = ~MPI_SUCCESS;
						goto done;
					}
				}
			}

			iters -= numRecords;

		}

		ROOTPRINT( "\t%8d\n", size );
		size *= 2;
	}

	MPI_Barrier( MPI_COMM_WORLD );
	ROOTPRINT( "Completed Randomize Test Section.\n\n" );

  done:

	if ( rawBuffer )
		free( rawBuffer );

	return retCode;
}

/* 
 * Calculate the average time to complete MPI_Init.
 */
#define HOSTNAMELEN 32
#define HOSTOFFSET  8
int
calc_init_time( int myid, int numProcs, time_t t_start, time_t t_end,
				time_t * t_avg )
{
	time_t t_delta, t_min, t_max, t_temp;
	int i, retCode = MPI_SUCCESS;

	if ( myid != 0 ) {
		/* Send our time to init back to the 0 node. */
		char mydata[HOSTNAMELEN + HOSTOFFSET];

		/* Build a buffer containing our host name and the elapsed time. */
		t_delta = t_end - t_start;
		*( ( time_t * ) mydata ) = t_delta;
		gethostname( &mydata[HOSTOFFSET], HOSTNAMELEN );

		mydata[HOSTOFFSET + HOSTNAMELEN - 1] = 0;

		retCode = MPI_Send( mydata,
							HOSTNAMELEN + HOSTOFFSET, MPI_CHAR, 0, 0,
							MPI_COMM_WORLD );
		ERRABORT( retCode );
	} else {
		/* Collect time to init data. */
		MPI_Status status;
		char curdata[HOSTNAMELEN + HOSTOFFSET];
		char min_host[HOSTNAMELEN];
		char max_host[HOSTNAMELEN];
		double avg;

		/* Initially the root is both best and worst. */
		gethostname( min_host, HOSTNAMELEN );
		gethostname( max_host, HOSTNAMELEN );

		t_delta = t_end - t_start;
		t_min = t_delta;
		t_max = t_delta;

		/* 
		 * We don't care what order the replies come in, 
		 * as long as we get them all.
		 */
		for ( i = 0; i < numProcs - 1; i++ ) {
			memset( &status, 0, sizeof( status ) );
			retCode = MPI_Recv( curdata,
								HOSTNAMELEN + HOSTOFFSET, MPI_CHAR,
								MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status );
			ERRABORT( retCode );
			t_temp = *( ( time_t * ) curdata );
			t_delta += t_temp;

			if ( t_temp < t_min ) {
				t_min = t_temp;
				strcpy( min_host, &curdata[HOSTOFFSET] );
			} else if ( t_temp > t_max ) {
				t_max = t_temp;
				strcpy( max_host, &curdata[HOSTOFFSET] );
			}
		}

		avg = ( double ) t_delta / ( double ) numProcs;
		ROOTPRINT( "Maximum time to init was: %8ld seconds (%s).\n",
				   t_max, max_host );
		ROOTPRINT( "Minimum time to init was: %8ld seconds (%s).\n",
				   t_min, min_host );
		ROOTPRINT( "Average time to init was: %8.2lf seconds.\n", avg );
	}

	return retCode;
}

/* 
 * Simple whisper down the lane test. Rank 0 sends a message to rank 1,
 * who passes it to 2, etc., till rank N returns it to rank 0. At that
 * point, it is compared against what was sent.
 *
 * This code is made more complicated by the desire to mangle memory 
 * usage in order to uncover possible corruption issues. If useSendOffset
 * is specified, each successive round moves the buffer one byte higher
 * in memory. In addition, if rawBufferCount is > 1, the code will use
 * multiple buffers in a ring. This is primarily to cause an increase in
 * total memory consumption to induce paging operations, which can affect
 * memory transfers to/from the HCA.
 *
 */
int
slowRing( int myid, int numProcs, int minSize, int maxSize, int maxIters )
{
	unsigned int i, size, iters, round;
	int retCode = MPI_SUCCESS;

	MPI_Request *request = NULL;
	MPI_Status *status = NULL;

	char **rawSendBuffer = NULL;
	char **rawRecvBuffer = NULL;
	char *sendBuffer, *recvBuffer;

	request = malloc( sizeof( MPI_Request ) );
	status = malloc( sizeof( MPI_Status ) );
	if ( !request || !status ) {
		ERRPRINT( "Failed to allocate requests and status.\n" );
		retCode = ~MPI_SUCCESS;
		goto done;
	}

	memset( request, 0, sizeof( MPI_Request ) );
	memset( status, 0, sizeof( MPI_Status ) );

	rawSendBuffer = malloc( sizeof( char * ) * rawBufferCount );
	rawRecvBuffer = malloc( sizeof( char * ) * rawBufferCount );
	memset( rawSendBuffer, 0, sizeof( char * ) * rawBufferCount );
	memset( rawRecvBuffer, 0, sizeof( char * ) * rawBufferCount );

	ROOTPRINT( "Beginning Slow Ring Test Section\n" );
	ROOTPRINT( "(This test passes messages from node to node in a ring\n"
			   "and validates that the end result matches the original\n"
			   "message.)\n" );

	size = minSize;
	iters = maxIters;

	while ( size <= maxSize ) {

		for ( round = 0; round < iters; round++ ) {
			size_t rawBufferSize =
				( barrierSize * 2 ) + ( size + ( useSendOffset ? round : 0 ) );
			char *currentRawSendBuffer;
			char *currentRawRecvBuffer;

			/* 
			 * If we already had a buffer here, we free it then
			 * reallocate it. In a sane world, we will normally
			 * get the same memory back again, but this could also
			 * trigger various "optimizations" in malloc's handling
			 * of the heap and the operating system's handling of 
			 * the page table - and it stresses any malloc management
			 * magic in MPI itself...
			 */
			if ( rawSendBuffer[round % rawBufferCount] != NULL ) {
				free( rawSendBuffer[round % rawBufferCount] );
			}
			rawSendBuffer[round % rawBufferCount] = malloc( rawBufferSize );
			if ( rawRecvBuffer[round % rawBufferCount] != NULL ) {
				free( rawRecvBuffer[round % rawBufferCount] );
			}
			rawRecvBuffer[round % rawBufferCount] = malloc( rawBufferSize );
			if ( !rawRecvBuffer[round % rawBufferCount]
				 || !rawSendBuffer[round % rawBufferCount] ) {
				ERRPRINT( "Failed to allocate buffers.\n" );
				retCode = ~MPI_SUCCESS;
				goto done;
			}

			currentRawSendBuffer = rawSendBuffer[round % rawBufferCount];
			currentRawRecvBuffer = rawRecvBuffer[round % rawBufferCount];

			sendBuffer =
				currentRawSendBuffer + barrierSize +
				( useSendOffset ? round : 0 );
			recvBuffer =
				currentRawRecvBuffer + barrierSize +
				( useSendOffset ? round : 0 );

			/* 
			 * Setting up guard areas to check for corruption.
			 */
			memset( currentRawSendBuffer, myid, rawBufferSize );
			memset( currentRawRecvBuffer, myid, rawBufferSize );

			/* Create some data to send and validate against. */
			for ( i = 0; i < size; i++ )
				sendBuffer[i] = ( unsigned char ) ( i % 256 );

			if ( myid == 0 ) {
				/* Root initiates the loop and vets the results. */
				NODEPRINT( "S node 1\n" );

				retCode =
					MPI_Send( sendBuffer, size, MPI_CHAR, 1, round,
							  MPI_COMM_WORLD );
				ERRABORT( retCode );

				NODEPRINT( "W node %d\n", numProcs - 1 );

				retCode = MPI_Recv( recvBuffer,
									size,
									MPI_CHAR,
									numProcs - 1,
									round, MPI_COMM_WORLD, status );
				ERRABORT( retCode );

				if ( memcmp( currentRawSendBuffer,
							 currentRawRecvBuffer, rawBufferSize ) ) {
					ERRPRINT( "%d byte messages do not match!\n", size );
					dumpbuffer( myid, "rawSendBuffer", currentRawSendBuffer,
								rawBufferSize );
					dumpbuffer( myid, "rawRecvBuffer", currentRawRecvBuffer,
								rawBufferSize );
					retCode = ~MPI_SUCCESS;
					goto done;
				}
			} else {
				/* Everyone else waits for a message and then repeats it to
				 * their neighbor. */
				NODEPRINT( "W node %d\n", myid - 1 );
				retCode = MPI_Recv( recvBuffer,
									size,
									MPI_CHAR,
									myid - 1, round, MPI_COMM_WORLD, status );
				ERRABORT( retCode );
				NODEPRINT( "S node %d\n", myid + 1 );
				retCode = MPI_Send( recvBuffer,
									size,
									MPI_CHAR,
									( myid + 1 ) % numProcs,
									round, MPI_COMM_WORLD );
				ERRABORT( retCode );

				/* 
				 * Note that we never sent SendBuffer but it should still
				 * be a correct model of what we actually received then 
				 * sent.
				 */
				if ( memcmp( currentRawSendBuffer,
							 currentRawRecvBuffer, rawBufferSize ) ) {
					ERRPRINT( "%d byte messages do not match!\n", size );
					dumpbuffer( myid, "rawSendBuffer", currentRawSendBuffer,
								rawBufferSize );
					dumpbuffer( myid, "rawRecvBuffer", currentRawRecvBuffer,
								rawBufferSize );
					retCode = ~MPI_SUCCESS;
					goto done;
				}
			}

			if ( !verboseMode && ( round % 100 ) == 0 )
				ROOTPRINT( "." );
		}
		ROOTPRINT( "\t%8d\n", size );

		size *= 2;
		iters = ( iters > 1 ) ? iters / 2 : 1;
	}

	ROOTPRINT( "Completed Slow Ring Test Section.\n\n" );

  done:
	if ( rawRecvBuffer ) {
		for ( i = 0; i < rawBufferCount; i++ ) {
			if ( rawRecvBuffer[i] ) {
				free( rawRecvBuffer[i] );
			}
		}

		free( rawRecvBuffer );
	}

	if ( rawSendBuffer ) {
		for ( i = 0; i < rawBufferCount; i++ ) {
			if ( rawSendBuffer[i] ) {
				free( rawSendBuffer[i] );
			}
		}

		free( rawSendBuffer );
	}

	if ( request )
		free( request );
	if ( status )
		free( status );
	return retCode;
}

/* 
 * Fast whisper down the lane test. Rank 0 sends many messages to rank 1,
 * who passes them to 2, etc., till rank N returns them to rank 0. At that
 * point, the messages are compared against what was sent.
 */
int
fastRing( int myid, int numProcs, int minSize, int maxSize, int maxIters )
{
	unsigned int i, j, size, iters, round;
	int retCode = MPI_SUCCESS;

	MPI_Request *request = NULL;
	MPI_Status *status = NULL;

	char **rawSendBuffer = NULL;
	char **rawRecvBuffer = NULL;
	char *sendBuffer, *recvBuffer;
	size_t rawBufferSize =
		( barrierSize * 2 ) + ( maxSize + ( useSendOffset ? maxIters : 0 ) );

	request = malloc( (rawBufferCount+maxIters) * sizeof( MPI_Request ) );
	status = malloc( (rawBufferCount+maxIters) * sizeof( MPI_Status ) );
	if ( !request || !status ) {
		ERRPRINT( "Failed to allocate requests and status.\n" );
		retCode = ~MPI_SUCCESS;
		goto done;
	}

	memset( request, 0, (rawBufferCount+maxIters) * sizeof( MPI_Request ) );
	memset( status, 0, (rawBufferCount+maxIters) * sizeof( MPI_Status ) );

	rawSendBuffer = malloc( sizeof( char * ) * rawBufferCount );
	rawRecvBuffer = malloc( sizeof( char * ) * rawBufferCount );
	memset( rawSendBuffer, 0, sizeof( char * ) * rawBufferCount );
	memset( rawRecvBuffer, 0, sizeof( char * ) * rawBufferCount );

	for ( i = 0; i < rawBufferCount; i++ ) {
		rawSendBuffer[i] = malloc( rawBufferSize );
		rawRecvBuffer[i] = malloc( rawBufferSize );

		if ( !rawRecvBuffer[i] || !rawSendBuffer[i] ) {
			ERRPRINT( "Failed to allocate buffers.\n" );
			retCode = ~MPI_SUCCESS;
			goto done;
		}
	}

	ROOTPRINT( "Beginning Fast Ring Test Section\n" );
	ROOTPRINT( "(This test passes multiple messages from node to node in a\n"
			   "ring and validates that the end result matches the original\n"
			   "message.)\n" );

	size = minSize;
	iters = maxIters;

	while ( size <= maxSize ) {

		if ( myid == 0 ) {
			for ( j = 0; j < rawBufferCount; j++) {
				char *currentRawSendBuffer = rawSendBuffer[j];
				char *currentRawRecvBuffer = rawRecvBuffer[j];

				/* 
				 * Setting up guard areas to check for corruption.
				 */
				memset( currentRawSendBuffer, myid, rawBufferSize );
				memset( currentRawRecvBuffer, myid, rawBufferSize );

				sendBuffer = currentRawSendBuffer + barrierSize;

				/* Create some data to send and validate against. */
				for ( i = 0; i < size; i++ )
					sendBuffer[i] = ( unsigned char ) ( i % 256 );
			}

			for ( round = 0; round < iters; round+= rawBufferCount ) {

				/* Fire in the hole! */
				for (j = 0; j<rawBufferCount; j++) {
					char *currentRawSendBuffer = rawSendBuffer[j];

					sendBuffer = currentRawSendBuffer + barrierSize;

					NODEPRINT( "S node 1\n" );

					retCode = MPI_Isend( sendBuffer, size, MPI_CHAR, 1, 
										 round + j,
							   			 MPI_COMM_WORLD, 
										 &request[round+j] );
					ERRABORT( retCode );
				}


				/* Process the messages as they come home. */
				for ( j = 0; j < rawBufferCount; j++ ) {
					char *currentRawRecvBuffer = rawRecvBuffer[j];
					char *currentRawSendBuffer = rawSendBuffer[j];

					recvBuffer = currentRawRecvBuffer + barrierSize +
								 ( useSendOffset ? (round + j) : 0 );

					NODEPRINT( "W node %d / %p \n", numProcs - 1, recvBuffer );

					retCode = MPI_Recv( recvBuffer,
										size,
										MPI_CHAR,
										numProcs - 1,
										round+j, 
										MPI_COMM_WORLD, 
										&(status[round+j]) );
					ERRABORT( retCode );

					if ( memcmp( currentRawSendBuffer, currentRawRecvBuffer,
							 rawBufferSize ) ) {
						ERRPRINT( "%d byte messages do not match!\n", size );
						dumpbuffer( myid, "rawSendBuffer", currentRawSendBuffer,
									rawBufferSize );
						dumpbuffer( myid, "rawRecvBuffer", currentRawRecvBuffer,
									rawBufferSize );
						retCode = ~MPI_SUCCESS;
						goto done;
					}

					if ( !verboseMode && ( (round+j) % 100 ) == 0 )
							ROOTPRINT( "." );

				}
			}

		} else {
			unsigned iters2 = iters;
			if ((iters % rawBufferCount) != 0) 
				iters2 += (rawBufferCount - (iters % rawBufferCount));

			/* Everyone else waits for a message and then repeats it to their
			 * neighbor. */
			for ( round = 0; round < iters2; round++ ) {
				char *currentRawSendBuffer =
					rawSendBuffer[round % rawBufferCount];
				char *currentRawRecvBuffer =
					rawRecvBuffer[round % rawBufferCount];

				sendBuffer = currentRawSendBuffer + barrierSize;
				recvBuffer = currentRawRecvBuffer + barrierSize;

				/* 
				 * Setting up guard areas to check for corruption.
				 */
				memset( currentRawSendBuffer, myid, rawBufferSize );
				memset( currentRawRecvBuffer, myid, rawBufferSize );

				/* Create some data to send and validate against. */
				for ( i = 0; i < size; i++ )
					sendBuffer[i] = ( unsigned char ) ( i % 256 );

				NODEPRINT( "W node %d\n", myid - 1 );
				retCode = MPI_Recv( recvBuffer,
									size,
									MPI_CHAR,
									myid - 1, round, MPI_COMM_WORLD, status );
				ERRABORT( retCode );
				NODEPRINT( "S node %d\n", myid + 1 );
				retCode = MPI_Send( recvBuffer,
									size,
									MPI_CHAR,
									( myid + 1 ) % numProcs,
									round, MPI_COMM_WORLD );
				ERRABORT( retCode );

				/* 
				 * Note that we never sent SendBuffer but it should still
				 * be a correct model of what we actually received then 
				 * sent.
				 */
				if ( memcmp( currentRawSendBuffer,
							 currentRawRecvBuffer, rawBufferSize ) ) {
					ERRPRINT( "%d byte messages do not match!\n", size );
					dumpbuffer( myid, "rawSendBuffer", currentRawSendBuffer,
								rawBufferSize );
					dumpbuffer( myid, "rawRecvBuffer", currentRawRecvBuffer,
								rawBufferSize );
					retCode = ~MPI_SUCCESS;
					goto done;
				}
			}
		}
		ROOTPRINT( "\t%8d\n", size );

		size *= 2;
		iters = ( iters > 1 ) ? iters / 2 : 1;
	}

	ROOTPRINT( "Completed Fast Ring Test Section.\n\n" );

  done:
	if ( rawRecvBuffer ) {
		for ( i = 0; i < rawBufferCount; i++ ) {
			if ( rawRecvBuffer[i] ) {
				free( rawRecvBuffer[i] );
			}
		}

		free( rawRecvBuffer );
	}

	if ( rawSendBuffer ) {
		for ( i = 0; i < rawBufferCount; i++ ) {
			if ( rawSendBuffer[i] ) {
				free( rawSendBuffer[i] );
			}
		}

		free( rawSendBuffer );
	}

	if ( request )
		free( request );
	if ( status )
		free( status );
	return retCode;
}

int
main( int argc, char *argv[] )
{
	int retCode, myid, numProcs, i;
	time_t t1, t2, t3;

	t1 = time( NULL );
	MPI_Init( &argc, &argv );
	MPI_Comm_size( MPI_COMM_WORLD, &numProcs );
	MPI_Comm_rank( MPI_COMM_WORLD, &myid );
	t2 = time( NULL );

	while ( -1 !=
			( retCode = getopt_long_only( argc, argv, "", options, &i ) ) ) {
		switch ( retCode ) {
		case '<':
			minSize = strtoul( optarg, NULL, 0 );
			break;
		case '>':
			maxSize = strtoul( optarg, NULL, 0 );
			break;
		case 'i':
			maxIters = strtoul( optarg, NULL, 0 );
			break;
		case 'b':
			rawBufferCount = strtoul( optarg, NULL, 0 );
			break;
		case 'B':
			barrierSize = strtoul( optarg, NULL, 0 );
			break;
		case '?':
			usage(  );
			goto done;
		default:
			break;
		}
	}

	ROOTPRINT( "-----------------------------\n" );
	ROOTPRINT( "-----------------------------\n" );
	ROOTPRINT( "Initialization complete.\n" );
	MPI_Barrier( MPI_COMM_WORLD );
	retCode = calc_init_time( myid, numProcs, t1, t2, &t3 );
	ERRABORT( retCode );

	ROOTPRINT( "--------------------------\n" );
	ROOTPRINT( "--------------------------\n" );
	ROOTPRINT( "Number of nodes      = %d\n", numProcs );
	ROOTPRINT( "Minimum message size = %d\n", minSize );
	ROOTPRINT( "Maximum message size = %d\n", maxSize );
	ROOTPRINT( "Maximum # of rounds  = %d\n\n", maxIters );
	ROOTPRINT( "Use Message Send Offset = %s\n", useSendOffset ? "ON" : "OFF" );
	ROOTPRINT( "Message Buffer Count    = %d\n", rawBufferCount );
	ROOTPRINT( "Message Barrier Size    = %d\n\n", barrierSize );

	if ( doSlow ) {
		MPI_Barrier( MPI_COMM_WORLD );
		retCode = slowRing( myid, numProcs, minSize, maxSize, maxIters );
		ERRABORT( retCode );
	}
	if ( doFast ) {
		MPI_Barrier( MPI_COMM_WORLD );
		retCode = fastRing( myid, numProcs, minSize, maxSize, maxIters );
		ERRABORT( retCode );
	}
	if ( doRandom ) {
		MPI_Barrier( MPI_COMM_WORLD );
		retCode = randomize( myid, numProcs, minSize, maxSize, maxIters );
		ERRABORT( retCode );
	}

	ROOTPRINT( "--------------------------\n" );
	ROOTPRINT( "--------------------------\n" );

  done:
	MPI_Finalize(  );
	return retCode;
}
