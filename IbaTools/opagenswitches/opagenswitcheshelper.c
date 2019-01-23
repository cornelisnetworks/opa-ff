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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stl_helper.h>

unsigned int g_debug_level = 0;
unsigned int g_verbose = 0;

// Local functions

/* cat_bfr() - concatonate buffer2 onto end of buffer1; replace LF in
 * buffer1 with ';' and remove null in buffer1.
 *
 * Inputs:
 *  p_bfr1 - pointer to buffer1
 *  p_bfr2 - pointer to buffer2
 *
 * Outputs:
 *  none
 */
void cat_bfr(char * p_bfr1, char * p_bfr2)
{
	char * p_bfr;

	if (!p_bfr1 || !p_bfr2 || !strlen(p_bfr1) || !strlen(p_bfr2))
		return;
	if ((p_bfr = strchr(p_bfr1, 0x0A)))
	{
		*p_bfr++ = ';';
		strcat(p_bfr, p_bfr2);
	}
	return;
}	// End of cat_bfr()


// Helper functions for opagenswitches.sh

/* proc_links() - read links input file and parse links according
 * to node type.  Write link information to appropriate link file.
 *
 * Inputs:
 *            p_file_links - name of links input file
 *   p_file_links_edge_hfi - name of edge-to-hfi links output file
 *  p_file_links_leaf_edge - name of leaf-to-edge links output file
 *
 * Outputs:
 *  status: 0 - OK
 *          1 - Error
 */
int proc_links( char * p_file_links, char * p_file_links_edge_hfi,
	char * p_file_links_leaf_edge )
{
	int status = 0;
	unsigned int n_size_read = 2048;
	unsigned int ct_line = 0;
	char * p_bfr;
	FILE * h_links = NULL;
	FILE * h_links_edge_hfi = NULL;
	FILE * h_links_leaf_edge = NULL;
	char bf_input[n_size_read * 2 + 1];
	char bf_input2[n_size_read * 2 + 1];

	// Validate inputs
	if (!p_file_links || !p_file_links_edge_hfi || !p_file_links_leaf_edge)
	{
		fprintf( stderr, "opagenswitches:proc_links NULL Parameter (%s %s %s)\n",
			(p_file_links)?p_file_links:"<null>", 
			(p_file_links_edge_hfi)?p_file_links_edge_hfi:"<null>", 
			(p_file_links_leaf_edge)?p_file_links_leaf_edge:"<null>" );
		goto error;
	}
	if ( !(h_links = fopen(p_file_links, "r")) ||
			!(h_links_edge_hfi = fopen(p_file_links_edge_hfi, "w")) ||
			!(h_links_leaf_edge = fopen(p_file_links_leaf_edge, "w")) )
	{
		fprintf( stderr, "opagenswitches:proc_links Unable to open file (%lu %lu %lu)\n",
			(unsigned long)h_links, (unsigned long)h_links_edge_hfi,
			(unsigned long)h_links_leaf_edge );
		goto error;
	}

	// Read links input file
	for (ct_line = 1; ; ct_line++)
	{
		if (fgets(bf_input, n_size_read, h_links))
		{
			ct_line++;
			if (fgets(bf_input2, n_size_read, h_links))
			{
				// Process bf_input[] and bf_input2[]
				if (strstr(bf_input2, "FI"))
				{
					cat_bfr(bf_input, bf_input2);
					fputs(bf_input, h_links_edge_hfi);
				}
				else if (strstr(bf_input, "FI"))
				{
					cat_bfr(bf_input2, bf_input);
					fputs(bf_input2, h_links_edge_hfi);
				}
				else if ( ((p_bfr = strchr(bf_input, ';'))) &&
						((p_bfr = strchr(++p_bfr, ';'))) &&
						((p_bfr = strchr(++p_bfr, ';'))) &&
						((p_bfr = strchr(++p_bfr, 'S'))) &&
						isdigit(*++p_bfr) && isdigit(*++p_bfr) &&
						isdigit(*++p_bfr) )
					continue;
				else if ( ((p_bfr = strchr(bf_input2, ';'))) &&
						((p_bfr = strchr(++p_bfr, ';'))) &&
						((p_bfr = strchr(++p_bfr, ';'))) &&
						((p_bfr = strchr(++p_bfr, 'S'))) &&
						isdigit(*++p_bfr) && isdigit(*++p_bfr) &&
						isdigit(*++p_bfr) )
					continue;
				else
				{
					cat_bfr(bf_input2, bf_input);
					fputs(bf_input2, h_links_leaf_edge);
				}
			}
			else
			{
				fprintf(stderr, "opagenswitches:proc_links Missing Record\n");
				goto error;
			}
		}
		else
			goto exit;

	}	// End of for (ct_line = 1; ; ct_line++)

exit:
	if (h_links)
		fclose(h_links);
	if (h_links_edge_hfi)
		fclose(h_links_edge_hfi);
	if (h_links_leaf_edge)
		fclose(h_links_leaf_edge);
	return (status);

error:
	status = 1;
	goto exit;

}	// End of proc_links()

/* proc_linksum() - read topology input file and parse links according
 * to node type.  Write link information to appropriate linksum file.
 *
 * Inputs:
 *        p_file_topology - name of topology input file
 *   p_file_linksum_edge_hfi - name of edge-to-hfi linksum output file
 *  p_file_linksum_leaf_edge - name of leaf-to-edge linksum output file
 *
 * Outputs:
 *  status: 0 - OK
 *          1 - Error
 */
int proc_linksum( char * p_file_topology, char * p_file_linksum_edge_hfi,
	char * p_file_linksum_leaf_edge )
{
	int status = 0;
	unsigned int n_size_read = 2048;
	unsigned int ct_line = 0;
	char * p_bfr;
	FILE * h_topology = NULL;
	FILE * h_linksum_edge_hfi = NULL;
	FILE * h_linksum_leaf_edge = NULL;
	char bf_input[n_size_read * 2 + 1];
	char bf_input2[n_size_read * 2 + 1];

	// Validate inputs
	if (!p_file_topology || !p_file_linksum_edge_hfi || !p_file_linksum_leaf_edge)
	{
		fprintf( stderr, "opagenswitches:proc_linksum NULL Parameter (%s %s %s)\n",
			(p_file_topology)?p_file_topology:"<null", 
			(p_file_linksum_edge_hfi)?p_file_linksum_edge_hfi:"<null", 
			(p_file_linksum_leaf_edge)?p_file_linksum_leaf_edge:"<null" );
		goto error;
	}
	if ( !(h_topology = fopen(p_file_topology, "r")) ||
			!(h_linksum_edge_hfi = fopen(p_file_linksum_edge_hfi, "w")) ||
			!(h_linksum_leaf_edge = fopen(p_file_linksum_leaf_edge, "w")) )
	{
		fprintf( stderr, "opagenswitches:proc_linksum Unable to open file (%lu %lu %lu)\n",
			(unsigned long)h_topology, (unsigned long)h_linksum_edge_hfi,
			(unsigned long)h_linksum_leaf_edge );
		goto error;
	}

	// Read topology input file
	for (ct_line = 1; ; ct_line++)
	{
		if (fgets(bf_input, n_size_read, h_topology))
		{
			ct_line++;
			if (fgets(bf_input2, n_size_read, h_topology))
			{
				// Process bf_input[] and bf_input2[]
				if (strstr(bf_input, "FI"))
				{
					cat_bfr(bf_input2, bf_input);
					fputs(bf_input2, h_linksum_edge_hfi);
				}
				else if (strstr(bf_input2, "FI"))
				{
					cat_bfr(bf_input, bf_input2);
					fputs(bf_input, h_linksum_edge_hfi);
				}
				else if ( ((p_bfr = strchr(bf_input2, ';'))) &&
						((p_bfr = strchr(++p_bfr, ';'))) &&
						((p_bfr = strchr(++p_bfr, 'S'))) &&
						isdigit(*++p_bfr) && isdigit(*++p_bfr) &&
						isdigit(*++p_bfr) )
					continue;
				else if ( ((p_bfr = strchr(bf_input, ';'))) &&
						((p_bfr = strchr(++p_bfr, ';'))) &&
						((p_bfr = strchr(++p_bfr, 'S'))) &&
						isdigit(*++p_bfr) && isdigit(*++p_bfr) &&
						isdigit(*++p_bfr) )
					continue;
				else
				{
					cat_bfr(bf_input2, bf_input);
					fputs(bf_input2, h_linksum_leaf_edge);
				}
			}
			else
			{
				fprintf(stderr, "opagenswitches:proc_linksum Missing Record\n");
				goto error;
			}
		}
		else
			goto exit;

	}	// End of for (ct_line = 1; ; ct_line++)

exit:
	if (h_topology)
		fclose(h_topology);
	if (h_linksum_edge_hfi)
		fclose(h_linksum_edge_hfi);
	if (h_linksum_leaf_edge)
		fclose(h_linksum_leaf_edge);
	return (status);

error:
	status = 1;
	goto exit;

}	// End of proc_linksum()


// Main function
int main(int argc, char** argv)
{
	int status = 1;
	char *p_cmdname = NULL;
	char *p_arg1 = NULL;
	char *p_arg2 = NULL;
	char *p_arg3 = NULL;

	if (argc >= 1)
	{
		p_cmdname = argv[1];
		if (argc >= 2)
		{
			p_arg1 = argv[2];
			if (!p_arg1) {
				fprintf(stderr, "opagenswitches: Error: null input filename\n");
				exit(status);
			}
			if (argc >= 3)
			{
				p_arg2 = argv[3];
				if (!p_arg2) {
					fprintf(stderr, "opagenswitches: Error: null input filename\n");
					exit(status);
				}
				if (argc >= 4)
				{
					p_arg3 = argv[4];
					if (!p_arg3) {
						fprintf(stderr, "opagenswitches: Error: null input filename\n");
						exit(status);
					}
				}
			}
		}
	}

	else
		exit(status);

	if (!strcmp(p_cmdname, "proc_linksum"))
	{
		status = proc_linksum(p_arg1, p_arg2, p_arg3);
	}

	else if (!strcmp(p_cmdname, "proc_links"))
	{
		status = proc_links(p_arg1, p_arg2, p_arg3);
	}

	else
	{
		exit(status);
	}

	exit(status);

}	// End of main

