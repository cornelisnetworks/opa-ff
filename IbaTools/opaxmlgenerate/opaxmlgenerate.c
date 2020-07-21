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


/*******************************************************************************
 *
 * File: opaxmlgenerate.c
 *
 * Description:
 *	This file implements the opaxmlgenerate program.  opaxmlgenerate takes
 *	CSV data as input and, with user-specified element names, generates
 *	sequences of XML containing the element values within start and end tag
 *	specifications.
 *
 *	opaxmlgenerate uses libXml to manage the XML generation state and for ouptut
 *	functions.  It calls the following functions:
 *	  IXmlInit()
 *	  IXmlOutputStartAttrTag()
 *	  IXmlOutputEndTag()
 *	  IXmlOutputStrLen()
 *
 *	opaxmlgenerate uses 3 types of element names specified by the user.  The
 *	types are distinguished by their command line option.  -g specifies an
 *	an element name that, along with a value from the CSV input file, will
 *	be used to generate XML of the form '<element_name>value</element_name>'.
 *	-h specifies an element name that will be used to generate an "enclosing"
 *	XML header start tag of the form '<element_name>'.  -e specifies an
 *	element name that will be used to generate an enclosing XML header end
 *	tag of the form '</element_name>'.  Enclosing elements do not contain
 *	a value, but serve to separate and organize the elements that do contain
 *	values.
 *
 *	The operation of opaxmlgenerate is further controlled by the following
 *	command line options:
 *	  -d delimiter - specifies the delimiter character that separates input
 *	                 values in the CSV input file; default is semicolon
 *	  -i indent - specifies the number of spaces to indent each level of
 *	              XML output; default is zero
 *	  -X input_file - input CSV data from input_file
 *	  -P param_file - input command line options (parameters) from param_file
 *	  -v - verbose output: output progress reports during generation
 *
 *	opaxmlgenerate generates sequences (fragments) of XML, as opposed to
 *	complete XML specifications.  It is intended that opaxmlgenerate be
 *	invoked from within a script (typically multiple times), using multiple
 *	input files (with possibly different CSV formats) to create the desired
 *	XML output.  The script would output any necessary XML header or trailer
 *	information (XML version or report information) as well as "glue" XML
 *	between fragments.
 *
 *	NOTES:
 *	Output of start and end tags for enclosing elements is controlled by
 *	the placement of command line options -h and -e respectively with
 *	respect to -g options.  No check for matching tags or proper nesting
 *	of tags is performed by opaxmlgenerate.
 */


/*******************************************************************************
 *
 * INCLUDES
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iba/ipublic.h>
#include <iba/public/datatypes.h>
#include <getopt.h>
#include "ixml.h"


/*******************************************************************************
 *
 * DEFINES
 */

#define OK  0
#define ERROR (-1)

#define NAME_PROG  "opaxmlgenerate"		// Program name
#define MAX_PARAMS_FILE  512			// Max number of param file parameters
#define MAX_ELEMENTS  100				// Max number of elements parsable
#define MAX_DELIMIT_CHARS  16			// Max size of delimiter string
#define MAX_INPUT_BUF  8192				// Max size of input buffer
#define MAX_PARAM_BUF  8192				// Max size of parameter file
#define MAX_FILENAME_CHARS  256			// Max size of file name
#define WHITE_SPACE  " \t\r\n"			// White space characters
#define OPTION_LEN 2			// string length of an option(eg. -P)

// Element table entry
typedef struct 
{
	int		lenName;					// Length of element name		
	char	*pName;						// Element name
	int		lenValue;					// Length of element value
	char	*pValue;					// Element value
	int		flags;						// Flags as:  xxxx xEHG
										//   E = 1 - End header
										//   H = 1 - Header
										//   G = 1 - Generate value
} ELEMENT_TABLE_ENTRY;

// Element table flags:
#define ELEM_GENERATE  0x01
#define ELEM_HEADER  0x02
#define ELEM_END  0x04


/*******************************************************************************
 *
 * GLOBAL VARIABLES
 */

int		g_exitstatus  = 0;
int		g_verbose  = 0;
int		g_quiet  = 0;


/*******************************************************************************
 *
 * LOCAL VARIABLES
 */

int  numElementsTable  = 0;				// Number of elements in tbElements[]
uint32  numIndentChars  = 0;		// Num of chars per indent level

FILE  * hFileInput  = NULL;				// Input file handle (default stdin)
										// Input file name (default stdin)
char  nameFileInput[MAX_FILENAME_CHARS + 1]  = "stdin";
FILE  * hFileOutput  = NULL;			// Output file handle (default stdout)
char  nameFileOutput[MAX_FILENAME_CHARS]  = "stdout";  // Output file name (default stdout)

uint32  fvDebugProg  = 0;			// Debug/Trace variable as:
										//  0MMM MMMM NNNN NNNN  SSSS 21RC SIEB SNEB

// Command line option table, each has a short and long flag name
struct option tbOptions[] =
{
	// Basic controls
	{ "verbose", no_argument, NULL, 'v' },
	{ "generate", required_argument, NULL, 'g' },
	{ "header", required_argument, NULL, 'h' },
	{ "end", required_argument, NULL, 'e' },
	{ "infile", required_argument, NULL, 'X' },
	{ "pfile", required_argument, NULL, 'P' },
	{ "delimit", required_argument, NULL, 'd' },
	{ "indent", required_argument, NULL, 'i' },
	{ "debug", required_argument, NULL, 'Z' },
	{ 0 }
};

// Element table; contains information about each element of interest.
//  Elements are contained in the table (0 - numElementsTable-1) in the
//	order they appear on the command line.
ELEMENT_TABLE_ENTRY  tbElements[MAX_ELEMENTS];

// IXml Output State
IXmlOutputState_t state;

// Delimiter string buffer
char  bfDelimit[MAX_DELIMIT_CHARS + 1] = ";";

// Input buffer
char  bfInput[MAX_INPUT_BUF];


/*******************************************************************************
 *
 * LOCAL FUNCTION PROTOTYPES
 */

void dispElementRecord(ELEMENT_TABLE_ENTRY * pElement, const char * pValue);
void errUsage(void);
int findElement(const char *pElement);
void getRecu_opt( int argc, char ** argv, const char *pOptShort,
	struct option tbOptLong[] );


/*******************************************************************************
 *
 * dispElementRecord()
 *
 * Description:
 *	Display (output) an element, either generate (with value) or header (Start
 *	or End).
 *
 * Inputs:
 *	pElement - Pointer to tbElement to output
 *	  pValue - Pointer to value string to output (NULL means no value)
 *
 * Outputs:
 *	none
 */
void dispElementRecord(ELEMENT_TABLE_ENTRY * pElement, const char * pValue)
{
	// Output header element name (as Start tag or End tag)
	if (pElement->flags & ELEM_HEADER)
		IXmlOutputStartAttrTag(&state, pElement->pName, NULL, NULL);

	else if (pElement->flags & ELEM_END)
		IXmlOutputEndTag(&state, pElement->pName);

	// Output generate element name and value
	else if ((pElement->flags & ELEM_GENERATE) && pValue)
		IXmlOutputStrLen(&state, pElement->pName, pValue, strlen(pValue));

}	// End of dispElementRecord()


/*******************************************************************************
 *
 * errUsage()
 *
 * Description:
 *	Output information about program usage and parameters.
 *
 * Inputs:
 *	none
 *
 * Outputs:
 *	none
 */
void errUsage(void)
{
	fprintf(stderr, "Usage: " NAME_PROG " [-v][-d delimiter][-i number][-g element][-h element]\n");
	fprintf(stderr, "                         [-e element][-X input_file][-P param_file]\n");
	fprintf(stderr, "  At least 1 element must be specified\n");
	fprintf(stderr, "  -g/--generate element     - name of XML element to generate\n");
	fprintf(stderr, "                              can be used multiple times\n");
	fprintf(stderr, "                              values assigned to elements in order xxx ... \n");
	fprintf(stderr, "  -h/--header element       - name of XML element in which to enclose generated\n");
	fprintf(stderr, "                              XML elements\n");
	fprintf(stderr, "  -e/--end element          - name of header XML element to end (close)\n");
	fprintf(stderr, "  -d/--delimit delimiter    - delimiter char input between element values\n");
	fprintf(stderr, "                              default is semicolon\n");
	fprintf(stderr, "  -i/--indent number        - number of spaces to indent each level of XML\n");
	fprintf(stderr, "                              output; default is zero\n");
	fprintf(stderr, "  -X/--infile input_file    - generate XML from CSV in input_file\n");
	fprintf(stderr, "  -P/--pfile param_file     - read command parameters from param_file\n");
	fprintf(stderr, "  -v/--verbose              - verbose output: progress reports during generation\n");

	if (hFileInput && (hFileInput != stdin))
		fclose(hFileInput);

	exit(2);
}	// End of errUsage()


/*******************************************************************************
 *
 * findElement()
 *
 * Description:
 *	Find specified element in element table.
 *
 * Inputs:
 *	pElement - Pointer to element name
 *
 * Outputs:
 *	Index of element table containing element name, else
 *	-1 - Element name not found
 */
int findElement(const char *pElement)
{
	int		ix;
	int		length;

	length = strlen(pElement);
	for (ix = 0; ix < numElementsTable; ix++)
	{
		if ((tbElements[ix].lenName - 1) == length)
			if (!strcmp(pElement, tbElements[ix].pName))
				return (ix);
	}

	return (ERROR);

}	// End of findElement()


/*******************************************************************************
 *
 * getRecu_opt()
 *
 * Description:
 *	Get command line options (recursively).  Parses command line options
 *	using short and long option (parameter) definitions.  Parameters (global
 *	variables) are updated appropriately.  The parameter '-P param_file' which
 *	takes command line options from a file is also handled.  The -P parameter
 *	can be used within a parameter file; in such cases the function will call
 *	itself recursively.
 *
 * Inputs:
 *	     argc - Number of input parameters
 *	     argv - Array of pointers to input parameters
 *	pOptShort - Pointer to string of short option definitions
 *	tbOptLong - Array of long option definitions
 * 
 * Outputs:
 *	none
 */
void getRecu_opt( int argc, char ** argv, const char *pOptShort,
	struct option tbOptLong[] )
{
	int		ix;
    int		cOpt;						// Option parsing char                         
	int		ixOpt;						// Option parsing index
	int		lenStr;						// String length
	int		argc_recu;					// Recursive argc
	char	*argv_recu[MAX_PARAMS_FILE];  // Recursive argv
	int		optind_recu;				// Recursive optind
	char	*optarg_recu;				// Recursive optarg

	int		ctCharParam;				// Parameter file char count
	FILE	*hFileParam = NULL;			// Parameter file handle
	char	nameFileParam[MAX_FILENAME_CHARS +1];  // Parameter file name
	char	bfParam[MAX_PARAM_BUF + 1];

	// Input command line arguments
	while ((cOpt = getopt_long(argc, argv, pOptShort, tbOptLong, &ixOpt)) != -1)
    {
		lenStr = 0;
        switch (cOpt)
        {
		// Verbose
		case 'v':
			g_verbose = 1;
			break;

		// Generate element name specification
		case 'g':
		// Header element name specification
		case 'h':
		// End (header) element name specification
		case 'e':
			if (numElementsTable == MAX_ELEMENTS)
			{
				fprintf(stderr, NAME_PROG ": Too many Element Names\n");
				errUsage();
			}

			else if (!optarg || ((lenStr = strlen(optarg) + 1) == 1))
			{
				fprintf(stderr, NAME_PROG ": Invalid Element Name: %s\n", optarg ? optarg:"");
				errUsage();
			}

			else
			{
				if ( (tbElements[numElementsTable].pName = malloc(lenStr)) )
				{
					strcpy(tbElements[numElementsTable].pName, optarg);
					tbElements[numElementsTable].lenName = lenStr;
				}

				else
				{
					fprintf(stderr, NAME_PROG ": Unable to Allocate Name Memory\n");
					errUsage();
				}

				if (cOpt == 'g')
					tbElements[numElementsTable++].flags |= ELEM_GENERATE;

				else if (cOpt == 'h')
					tbElements[numElementsTable++].flags |= ELEM_HEADER;

				else if (cOpt == 'e')
					tbElements[numElementsTable++].flags |= ELEM_END;
			}

			break;

		// Delimiter string specification
		case 'd':
			if ( !optarg || ((lenStr = strlen(optarg)) == 0) ||
					(lenStr > MAX_DELIMIT_CHARS) )
			{
				fprintf(stderr, NAME_PROG ": Invalid delimiter size: %d\n", lenStr);
				errUsage();
			}

			strncpy(bfDelimit, optarg, MAX_DELIMIT_CHARS);
			bfDelimit[MAX_DELIMIT_CHARS]=0; // Ensure null terminated
			break;

		// Indent chars specification
		case 'i':
			if (FSUCCESS != StringToUint32(&numIndentChars, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, NAME_PROG ": Invalid indent parameter size: %d\n", lenStr);
				errUsage();
			}
			break;

		// Input file specification
		case 'X':
			if ( !optarg || ((lenStr = strlen(optarg)) == 0) ||
					(lenStr > MAX_FILENAME_CHARS) )
			{
				fprintf(stderr, NAME_PROG ": Invalid Input File Name: %s\n", optarg ? optarg:"");
				errUsage();
			}

			else if (hFileInput != stdin)
			{
				fprintf( stderr, NAME_PROG ": Multiple Input Files: %s and %s\n",
					nameFileInput, optarg );
				errUsage();
			}

			if (strcmp(optarg, "-"))
			{
				strncpy(nameFileInput, optarg, MAX_FILENAME_CHARS);
				nameFileInput[MAX_FILENAME_CHARS]=0; // Ensure null terminated
				hFileInput = fopen(nameFileInput, "r");
	
				if (!hFileInput)
				{
					fprintf( stderr, NAME_PROG ": Unable to Open Input File: %s\n",
						nameFileInput );
					perror("");
					errUsage();
				}
			}

			break;

		// Parameter file specification
		case 'P':
			if (!optarg || ((lenStr = strlen(optarg)) == 0) ||
					(lenStr > MAX_FILENAME_CHARS) )
			{
				fprintf(stderr, NAME_PROG ": Invalid Parameter File Name: %s\n", optarg ? optarg:"");
				errUsage();
			}

			// Open parameter file
			strncpy(nameFileParam, optarg,MAX_FILENAME_CHARS);
			nameFileParam[MAX_FILENAME_CHARS]=0; // Ensure null terminated.
			hFileParam = fopen(nameFileParam, "r");

			if (!hFileParam)
			{
				fprintf( stderr, NAME_PROG ": Unable to Open Parameter File: %s\n",
					nameFileParam );
				perror("");
				errUsage();
			}

			// Read parameter file
			if (g_verbose)
				fprintf(stderr, NAME_PROG ": Reading Param File: %s\n", nameFileParam);

			ctCharParam = (int)fread(bfParam, 1, MAX_PARAM_BUF, hFileParam);
			bfParam[MAX_PARAM_BUF-1]=0;

			if (ferror(hFileParam))
			{
				fclose(hFileParam);
				fprintf(stderr, NAME_PROG ": Read Error: %s\n", nameFileParam);
				errUsage();
			}

			if ((ctCharParam == MAX_PARAM_BUF) && !feof(hFileParam))
			{
				fclose(hFileParam);
				fprintf( stderr, NAME_PROG ": Parameter file (%s) too large (> %d bytes)\n",
					nameFileParam, MAX_PARAM_BUF );
				errUsage();
			}

			fclose(hFileParam);

			if (ctCharParam > 0)
			{
				// Parse parameter file into tokens
				argc_recu = 2;
				argv_recu[0] = nameFileParam;
			
				for (ix = 1; ; ix++, argc_recu++)
				{
					if (ix == MAX_PARAMS_FILE)
					{
						fprintf( stderr, NAME_PROG ": Parameter file (%s) has too many parameters (> %d)\n",
										nameFileParam, MAX_PARAMS_FILE );
						errUsage();
					}
					if ( (argv_recu[ix] = strtok((ix == 1) ? bfParam : NULL, WHITE_SPACE))
							== NULL )
					{
						argc_recu--;
						break;
					}

					if (!strncmp(argv_recu[ix], "-P", OPTION_LEN))
					{
						fprintf( stderr, NAME_PROG ": Parameter file (%s) cannot contain -P option\n", nameFileParam);
						errUsage();
					}

					if ( (argv_recu[ix] - bfParam + strlen(argv_recu[ix]) + 1) ==
							ctCharParam )
						break;

				}

				// Process parameter file parameters
				if (argc_recu > 1)
				{
					optind_recu = optind;	// Save optind & optarg
					optarg_recu = optarg;
					optind = 1;   			// Init optind & optarg
					optarg = NULL;
					getRecu_opt(argc_recu, argv_recu, pOptShort, tbOptLong);
					optind = optind_recu;	// Restore optind & optarg
					optarg = optarg_recu;
				}

			}	// End of if (ctCharParam > 0)

			break;

		// Debug trace specification
		case 'Z':
			if (FSUCCESS != StringToUint32(&fvDebugProg, optarg, NULL, 0, TRUE)) {
				fprintf(stderr, NAME_PROG ": Invalid Debug Setting: %s\n", optarg);
				errUsage();
			}
			break;

		default:
			fprintf(stderr, NAME_PROG ": Invalid Option -<%c>\n", cOpt);
			errUsage();
			break;

        }	// End of switch (cOpt)

    }	// End of while ((cOpt = getopt_long(argc, argv,

	// Validate command line arguments
	if (optind < argc)
	{
		fprintf(stderr, "%s: invalid argument %s\n", NAME_PROG, argv[optind]);
		errUsage();
	}

}	// End of getRecu_opt()


/*******************************************************************************
 *
 * main()
 *
 * Description:
 *	main function for program.
 *
 * Inputs:
 *	argc - Number of input parameters
 *	argv - Array of pointers to input parameters
 *
 * Outputs:
 *	Exit status 0 - no errors
 *	Exit status 2 - error (input parameter, resources, file I/O, XML parsing)
 */
int main(int argc, char ** argv)
{
	int		ix;							// Loop index
	int		ctCharInput = 0;			// Input file char count
	char	*pValue = NULL;				// Pointer to current input value
	char	*pValueNext = bfInput + MAX_INPUT_BUF;  // Ptr to next input value

	// Initialize for generation
	hFileInput = stdin;
	hFileOutput = stdout;

	for (ix = 0; ix < MAX_ELEMENTS; ix++)
	{
		tbElements[ix].lenName = 0;
		tbElements[ix].pName = NULL;
		tbElements[ix].lenValue = 0;
		tbElements[ix].pValue = NULL;
		tbElements[ix].flags = 0;
	}

	// Get and validate command line arguments
	getRecu_opt(argc, argv, "vg:h:e:X:P:d:i:Z:", tbOptions);
	if (numElementsTable <= 0)
	{
		fprintf(stderr, NAME_PROG ": No Elements Specified\n");
		errUsage();
	}

	IXmlInit(&state, hFileOutput, numIndentChars, IXML_OUTPUT_FLAG_NONE, NULL);

	// Output Report
	strcat(bfDelimit, "\n");			// Append New Line to bfDelimit

	for (ix = 0; ; )
	{
		// Generate element
		if (tbElements[ix].flags & ELEM_GENERATE)
		{
			// Check for end of bfInput
			if ( ((pValueNext - bfInput) > (MAX_INPUT_BUF >> 1)) &&
					!feof(hFileInput) )
			{
				// Shift bfInput[] and read input file
				memmove(bfInput, pValueNext, MAX_INPUT_BUF + bfInput - pValueNext);

				if (g_verbose)
					fprintf( stderr, NAME_PROG ": Reading Input File: %s\n",
						nameFileInput );

				ctCharInput = MAX_INPUT_BUF + bfInput - pValueNext +
                    (int)fread( bfInput + MAX_INPUT_BUF - pValueNext + bfInput,
                    1, pValueNext - bfInput, hFileInput );
				pValueNext = bfInput;

				if (ferror(hFileInput))
				{
					fprintf(stderr, NAME_PROG ": Read Error: %s\n", nameFileInput);
					errUsage();
				}
			}

			// Check for end of input file
			if ((pValueNext - bfInput) >= ctCharInput)
			{
				fprintf( stderr,
			NAME_PROG ": WARNING End of Input file (%s) before End of Element List (%d items)\n",
						nameFileInput, numElementsTable );
				g_exitstatus = 2;
				break;
			}

			/* strtok() skips over multiple consecutive delimiters as a group;
			   the following causes a NULL element value to be processed if 2
			   consecutive delimiters are encountered.
			*/
			if (!strchr(bfDelimit, *pValueNext))
			{
				pValue = strtok(pValueNext, bfDelimit);
				if (!pValue) 
				{
					fprintf( stderr,
						NAME_PROG ": WARNING Malformed Input file (%s) strtok() error.\n",
						nameFileInput);
					break;
				}
				dispElementRecord(&tbElements[ix++], pValue);
				pValueNext = pValue + strlen(pValue) + 1;
			}

			else
			{
				dispElementRecord(&tbElements[ix++], NULL);
				pValueNext += 1;
			}
		}

		// Header or Header End element
		else
			dispElementRecord(&tbElements[ix++], NULL);

		// Check for end of tbElements[]
		if (ix >= numElementsTable)
		{
			if ((pValueNext - bfInput) < ctCharInput)
				ix = 0;					// Wrap to beg of tbElements[]

			else						// End of report
				break;
		}

	}	// End of for (ix = 0; ; )

	if (hFileInput && (hFileInput != stdin))
		fclose(hFileInput);

	return (g_exitstatus);

}	// End of main()


// End of file

