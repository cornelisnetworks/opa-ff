/* BEGIN_ICS_COPYRIGHT5 ****************************************

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

 * ** END_ICS_COPYRIGHT5   ****************************************/
/* [ICS VERSION STRING: unknown] */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <sys/mman.h>

void a(int *f)
{
#ifdef USE__STACK
	int z[16384];

	f[0] = 1;
	f[16383] = 2;
#else
	int *z = sbrk(16384);
	if(z == (int *)-1) {
		perror("couldn't allocate more memory while recursing");
		exit(1);
	}

#endif

	printf("a(%p)\n", f);
	sched_yield();
	a(z);
}


int
main(int cnt, char **args)
{
	unsigned long mem, i, pages, pgsz, passes;
	uint64_t tot, totmem=0x1000000000; // 64GB
	// volatile int val;
	volatile char *p;
	int zfirst[16384];
	int verbose=0;

	if(cnt < 2 || (mem = strtoul(args[1], NULL, 0)) <= 0) {
		printf("Usage: %s mem_incr [totmem]\n", args[0]);
		return 1;
	}
	if(cnt == 3 && (totmem = strtoul(args[2], NULL, 0)) <= 0) {
		printf("Usage: %s mem_incr [totmem]\n", args[0]);
		return 1;
	}

	if (mem > totmem) {
		printf("mem_incr is larger than totmem.\n");
		return 1;
	} else if (totmem > 0x100000000000) { // 16 terabytes...
		printf("totmem is too large.\n");
		return 1;
	}
		
	if(0 && mlockall(MCL_CURRENT|MCL_FUTURE))
		perror("mlockall failed");

	pgsz = getpagesize();

	for(tot=passes=0; tot<totmem; passes++, tot+=mem) {
		if(!(p = calloc(mem, 1))) {
			fprintf(stderr,
				"couldn't calloc %lu, pass %lu, %lu allocated: %s\n",
				mem, passes, tot, strerror(errno));
				break;
		}
		else if(verbose)
			printf("calloc'ed (%lx) bytes at %p, tot %lu\n", mem, p, tot);

		// calloc isn't good enough, since with mmap'ed based calloc,
		// it "knows" the pages are zero-filled, so you have to actuall
		// touch each page
		pages = (mem+pgsz-1)/pgsz;
		if((pgsz-1)&(ptrdiff_t)p)
			pages++; // not likely
		if((pgsz-1)&(ptrdiff_t)p)
			pages--; // wasn't page aligned, so one less page
		for(i=0; i<pages; i++) {
			p[i*pgsz] = 1;
			memset((void *)&p[i*pgsz], 0xa5, pgsz); // completely write the page
			sched_yield(); // try to randomize somewhat by letting other stuff run
		}
	}
	printf("alloc'ed %lu (%lx) bytes total\n", tot, tot);

	//printf("Pausing\n"); pause();

	return 0; // OLSON, stack is separate
	printf("recursing to grow stack 16K at a time\n");
	a(zfirst);

	return 0;
}


