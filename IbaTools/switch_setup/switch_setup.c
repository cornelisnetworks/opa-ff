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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "iba/stl_types.h"

/* Info for all switches */
int LinkWidthSelection;	/* selection for Link Width */
int NodeDescSelection;	/* selection for Node Description */
int FMEnabledSelection; /* selection for FM Enabled */
int LinkCRCModeSelection; /* selection for link CRC mode */

/* Globals for chassis specifics */

void stripCR(char *buf)
{
	char *p;

	if ((p = strchr(buf, '\n')) != NULL)
		*p = '\0';
	if ((p = strchr(buf, '\r')) != NULL)
		*p = '\0';

	return;
}

int getYesNo(char *question, int def)
{
	int done = 0;
	int res = 1;
	char answerBuf[8];

	while (!done)
	{
		printf("%s: ", question);
		if (fgets(answerBuf, 8, stdin) != NULL) {
			stripCR(answerBuf);
			if ((answerBuf[0] == 'n') || (answerBuf[0] == 'N'))
			{
				res = 0;
				done = 1;
			}
			else if ((answerBuf[0] == 'y') || (answerBuf[0] == 'Y'))
			{
				res = 1;
				done = 1;
			}
			else if (strlen(answerBuf) == 0)
			{
				res = def;
				done = 1;
			}
			else
			{
				fprintf(stderr, "<%s> is not a valid entry - please enter y or n ... \n", answerBuf);
			}
		} else {
			fprintf(stderr, "<%s> is not a valid entry - please enter y or n ... \n", answerBuf);
		}
	}
	return(res);
}

int getInt(char *question, int low, int high)
{
	int done = 0;
	int selection;
	int res = 1;
	int i;
	int goodInt;
	char answerBuf[64];

	while (!done)
	{
		printf("%s: ", question);
		if (fgets(answerBuf, 64, stdin) != NULL) {
			stripCR(answerBuf);

			if (strlen(answerBuf) == 0)
				goodInt = 0;
			else {
				for (i = 0, goodInt = 1; (i < strlen(answerBuf)) && goodInt; i++) {
					if (!isdigit(answerBuf[i]))
						goodInt = 0;
				}
			}

			if (goodInt)
				selection = atoi(answerBuf);
			else
				selection = -1;

			if ((selection < low) || (selection > high))
				fprintf(stderr, "<%s> is not a valid entry - please select a choice from %d to %d ...\n", answerBuf, low, high);
			else
			{
				res = selection;
				done = 1;
			}
		} else {
			fprintf(stderr, "<%s> is not a valid entry - please select a choice from %d to %d ...\n", answerBuf, low, high);
		}
	}
	return(res);
}

int getIntRange(char *question, int low, int high, int other)
{
	int done = 0;
	int selection;
	int res = 1;
	int i;
	int goodInt;
	char answerBuf[64];

	while (!done)
	{
		printf("%s: ", question);
		if (fgets(answerBuf, 64, stdin) != NULL) {
		stripCR(answerBuf);

			if (strlen(answerBuf) == 0)
				goodInt = 0;
			else {
				for (i = 0, goodInt = 1; (i < strlen(answerBuf)) && goodInt; i++) {
					if (!isdigit(answerBuf[i]))
						goodInt = 0;
				}
			}

			if (goodInt)
				selection = atoi(answerBuf);
			else
				selection = -1;

			if (((selection < low) || (selection > high)) && (selection != other))
				fprintf(stderr, "<%s> is not a valid entry - please select a choice from %d to %d, or %d ...\n", answerBuf, low, high, other);
			else
			{
				res = selection;
				done = 1;
			}
		} else {
			fprintf(stderr, "<%s> is not a valid entry - please select a choice from %d to %d, or %d ...\n", answerBuf, low, high, other);
		}
	}
	return(res);
}


void removeCR(char *buf)
{
	char *p;

	if ((p = strchr(buf, '\n')) != NULL)
		*p = '\0';
}

int getGeneralInfo()
{
	int setLinkWidthOptions = 0;
	int setNodeDesc = 0;
	int setFMEnabled = 0;
	int setLinkCRCMode = 0;
	int res = 1;

	setLinkWidthOptions = getYesNo("Do you wish to configure the switch Link Width Options? [n]", 0);
	if (setLinkWidthOptions)
	{
		printf("\t1 : 1X\n\t2 : 2X\n\t3 : 1X/2X\n\t4 : 3X\n\t5 : 1X/3X\n\t6 : 2X/3X\n\t7 : 1X/2X/3X\n\t8 : 4X\n\t9 : 1X/4X\n\t10 : 2X/4X\n\t11 : 1X/2X/4X\n\t12 : 3X/4X\n\t13 : 1X/3X/4X\n\t14 : 2X/3X/4X\n\t15 : 1X/2X/3X/4X\n");
		LinkWidthSelection = getInt("Enter selection", 1, 15);
		printf("********************************************************************************************\n");
		printf("*** Note: port 1 on each switch device will always be set to include 4X link width enabled\n");
		printf("*** Note: a reboot of all switch devices is required in order to activate\n*** changes to the switch link width\n");
		printf("********************************************************************************************\n");
	} else
		LinkWidthSelection = 0;

	setNodeDesc = getYesNo("Do you wish to configure the switch Node Description as it is set in the switches file? [n]", 0);
	NodeDescSelection = setNodeDesc;

	setFMEnabled = getYesNo("Do you wish to configure the switch FM Enabled option? [n]", 0);
	if (setFMEnabled) {
		FMEnabledSelection = getInt("Enter 0 for disable, 1 for enable", 0, 1);
		printf("********************************************************************************************\n");
		printf("*** Note: a reboot of all switch devices is required in order to activate\n*** changes to the FM Enabled setting\n");
		printf("********************************************************************************************\n");
	} else 
		FMEnabledSelection = -1;

	setLinkCRCMode = getYesNo("Do you wish to configure the switch Link CRC Mode? [n]", 0);
	if (setLinkCRCMode) {
		printf("\t0 : 16b,\n\t1 : 14b/16b,\n\t2 : 48b/16b,\n\t3 : 48b/14b/16b,\n\t4 : per-lane/16b,\n\t5 : per-lane/14b/16b,\n\t6 : per-lane/48b/16b,\n\t7 : per-lane/48b/14b/16b\n");
		LinkCRCModeSelection = getInt("Enter selection", 0, 7);
		printf("********************************************************************************************\n");
		printf("*** Note: a reboot of all switch devices is required in order to activate\n*** changes to the Link CRC Mode \n");
		printf("********************************************************************************************\n");
	} else
		LinkCRCModeSelection = -1;

	return(res);
}

int
main(int argc, char *argv[])
{
	FILE *fp_out;
	char *dirName;

	dirName = argv[1];
	if (dirName == NULL)
		dirName = ".";

	if (chdir(dirName) < 0) {
		fprintf(stderr, "Error changing directory to %s: %s\n", dirName, strerror(errno));
		exit(1);
	}

	getGeneralInfo();

	if ((fp_out = fopen("./.switchSetup.out","w")) == NULL)
	{
		fprintf(stderr, "Error opening .switchSetup.out for output: %s\n", strerror(errno));
		exit(1);
	}

	/* display general info */
	fprintf(fp_out, "Link Width Selection:%d\n", LinkWidthSelection);
	fprintf(fp_out, "Node Description Selection:%d\n", NodeDescSelection);
	fprintf(fp_out, "FM Enabled Selection:%d\n", FMEnabledSelection);
	fprintf(fp_out, "Link CRC Mode Selection:%d\n", LinkCRCModeSelection);

	fclose(fp_out);
	exit(0);
}
