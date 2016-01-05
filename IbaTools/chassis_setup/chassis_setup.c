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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "chassis_setup.h"
#include "iba/stl_types.h"

#define NO_TZ_DST_SETUP		100
#define SYSLOG_PORT_DEFAULT 	-1
#define SYSLOG_FACILITY_DEFAULT -1
/* Info for all chassis */
int tzOffset;					/* Offset in hours (from UTC) */
dstInfo_t dstInfo;				/* DST start and end info */
char syslogIpAddr[20];  			/* IP addr of syslog server */
int syslogPort = SYSLOG_PORT_DEFAULT;		/* Port for syslog */
int syslogFacility = SYSLOG_FACILITY_DEFAULT; 	/* Facility level for syslog */
char ntpIpAddr[20];  				/* IP addr of NTP server */
int linkWidthSelection;				/* selection for link width */
int setname;					/* should OPA Node Desc be set to match ethernet name */
int linkCRCModeSelection; 			/* selection for Link CRC mode */

/* Globals for chassis specifics */

int numChassis;
chassisInfo_t *chassisInfoTable;
int chassisInfoTableSize;

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
		if (fgets(answerBuf, 8, stdin)) {
			stripCR(answerBuf);
			if (strncmp(answerBuf, "none", 4) == 0)
			{
				res = 0;
				done = 1;
			}
			else if ((answerBuf[0] == 'n') || (answerBuf[0] == 'N'))
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
		}
	}
	return(res);
}

int getInt(char *question, int low, int high, boolean allowAbort, int abortValue)
{
	int done = 0;
	int selection;
	int res = 1;
	int i;
	int goodInt;
	char answerBuf[64];

	while (!done)
	{
		if (allowAbort)
			printf("%s (or none): ", question);
		else
			printf("%s: ", question);
		if (fgets(answerBuf, 64, stdin)) {
			stripCR(answerBuf);

			if (allowAbort && (strncmp(answerBuf, "none", 4) == 0))
				return abortValue;

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
		}
	}
	return(res);
}

void setupTZInfo(tzCode_t timeZone)
{
	switch (timeZone)
	{
		case TZ_NA_ATLA:
			/* NAmer - Atlantic */
			if (daylight)
				dstInfo = NA_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = -4;
			break;
		case TZ_NA_EAST:
			/* NAmer - Eastern */
			if (daylight)
				dstInfo = NA_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = -5;
			break;
		case TZ_NA_CENT:
			/* NAmer - Central */
			if (daylight)
				dstInfo = NA_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = -6;
			break;
		case TZ_NA_MOUN:
			/* NAmer - Mountain */
			if (daylight)
				dstInfo = NA_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = -7;
			break;
		case TZ_NA_PACI:
			/* NAmer - Pacific */
			if (daylight)
				dstInfo = NA_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = -8;
			break;
		case TZ_NA_ASKA:
			/* NAmer - Alaska */
			if (daylight)
				dstInfo = NA_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = -9;
			break;
		case TZ_NA_HAWI:
			/* NAmer - Hawaii */
			dstInfo = None_DSTInfo;
			tzOffset = -10;
			break;
		case TZ_EU_UTC:
			/* Europe - UTC */
			if (daylight)
				dstInfo = EUR_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = 0;
			break;
		case TZ_EU_CENT:
			/* Europe - Central */
			if (daylight)
				dstInfo = EUR_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = 1;
			break;
		case TZ_EU_EAST:
			/* Europe - Eastern */
			if (daylight)
				dstInfo = EUR_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = 2;
			break;
		case TZ_EU_MOSC:
			/* Europe - Russia/Moscow */
			if (daylight)
				dstInfo = EUR_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = 3;
			break;
		case TZ_AS_CHIN:
			/* China */
			dstInfo = None_DSTInfo;
			tzOffset = 8;
			break;
		case TZ_AS_JAPN:
			/* Japan */
			dstInfo = None_DSTInfo;
			tzOffset = 9;
			break;
		case TZ_AU_SYDN:
			/* Australia - Sydney */
			if (daylight)
				dstInfo = AUS_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = 10;
			break;
		case TZ_NZ_AUCK:
			/* New Zealand - Auckland */
			if (daylight)
				dstInfo = NZ_DSTInfo;
			else
				dstInfo = None_DSTInfo;
			tzOffset = 12;
			break;
	}
	return;
}

void getTimeZoneInfo(int configureTZDST)
{
	int useServerTZInfo = 1;
	int tzSelection;
	int configureManually = 0;
	int done = 0;
	tzCode_t timeZone = TZ_NA_EAST;
	char questionText[100];

	if (configureTZDST)
	{
		useServerTZInfo = getYesNo("Do you want to use the local timezone information from the local server? [y]", 1);

		if (useServerTZInfo)
		{
			tzset();
			switch (TIMEZONEOFFSET)
			{
				case 0:
					/* UTC */
					timeZone = TZ_EU_UTC;
					break;
				case 4:
					/* NAmer - Atlantic */
					timeZone = TZ_NA_ATLA;
					break;
				case 5:
					/* NAmer - Eastern */
					timeZone = TZ_NA_EAST;
					break;
				case 6:
					/* NAmer - Central */
					timeZone = TZ_NA_CENT;
					break;
				case 7:
					/* NAmer - Mountain */
					timeZone = TZ_NA_MOUN;
					break;
				case 8:
					/* NAmer - Pacific */
					timeZone = TZ_NA_PACI;
					break;
				case 9:
					/* NAmer - Alaska */
					timeZone = TZ_NA_ASKA;
					break;
				case 10:
					/* NAmer - Hawaii */
					timeZone = TZ_NA_HAWI;
					break;
				case -1:
					/* Cent Europe */
					timeZone = TZ_EU_CENT;
					break;
				case -2:
					/* East Europe */
					timeZone = TZ_EU_EAST;
					break;
				case -3:
					/* Russia - Moscow */
					timeZone = TZ_EU_MOSC;
					break;
				case -8:
					/* China */
					timeZone = TZ_AS_CHIN;
					break;
				case -9:
					/* Japan */
					timeZone = TZ_AS_JAPN;
					break;
				case -10:
					/* Australia - Sydney */
					timeZone = TZ_AU_SYDN;
					break;
				case -12:
					/* New Zealand - Auckland */
					timeZone = TZ_NZ_AUCK;
					break;
			}
			setupTZInfo(timeZone);
		} /* using server TZ info */
		else
		{ /* not using server TZ info */

			printf("Select a locale that matches your location: \n");
			printf("\t1  - North America - Atlantic\n");
			printf("\t2  - North America - Eastern\n");
			printf("\t3  - North America - Central\n");
			printf("\t4  - North America - Mountain\n");
			printf("\t5  - North America - Arizona\n");
			printf("\t6  - North America - Pacific\n");
			printf("\t7  - North America - Alaska\n");
			printf("\t8  - North America - Hawaii\n");
			printf("\t9  - UK\n");
			printf("\t10 - Central Europe\n");
			printf("\t11 - Eastern Europe\n");
			printf("\t12 - Russia - Moscow\n");
			printf("\t13 - China\n");
			printf("\t14 - Japan\n");
			printf("\t15 - Australia - Sydney\n");
			printf("\t16 - New Zealand - Auckland\n");
			printf("\t17 - Configure manually\n");
			tzSelection = getInt("Enter selection", 1, 17, FALSE, 0);
			daylight = 1;
			switch (tzSelection)
			{
				case 1:
					timeZone = TZ_NA_ATLA;
					break;
				case 2:
					timeZone = TZ_NA_EAST;
					break;
				case 3:
					timeZone = TZ_NA_CENT;
					break;
				case 4:
					timeZone = TZ_NA_MOUN;
					break;
				case 5:
					timeZone = TZ_NA_MOUN;
					daylight = 0;
					break;
				case 6:
					timeZone = TZ_NA_PACI;
					break;
				case 7:
					timeZone = TZ_NA_ASKA;
					break;
				case 8:
					timeZone = TZ_NA_HAWI;
					daylight = 0;
					break;
				case 9:
					timeZone = TZ_EU_UTC;
					break;
				case 10:
					timeZone = TZ_EU_CENT;
					break;
				case 11:
					timeZone = TZ_EU_EAST;
					break;
				case 12:
					timeZone = TZ_EU_MOSC;
					break;
				case 13:
					timeZone = TZ_AS_CHIN;
					daylight = 0;
					break;
				case 14:
					timeZone = TZ_AS_JAPN;
					daylight = 0;
					break;
				case 15:
					timeZone = TZ_AU_SYDN;
					break;
				case 16:
					timeZone = TZ_NZ_AUCK;
					break;
				case 17:
					configureManually = 1;
					break;
			}
			if (!configureManually)
				setupTZInfo(timeZone);
			else
			{
				tzOffset = getInt("Enter timezone offset in hours - positive is east, negative is west of UTC", -12, 12, FALSE, 0); 
				daylight = getYesNo("Does your timezone observe Daylight Saving or Summer Time? [y]", 1);
				if (daylight)
				{
					while (!done)
					{
						printf("You will be prompted for adjustment information - which day of the week in which month (e.g. 2nd Sunday in March),\n");
						printf("for both start and end of the observance.\n\n");
						printf("For month, valid replies are:           3-11: 3-Mar, 4-Apr, 5-May, 6-Jun, 7-Jul, 8-Aug, 9-Sep, 10-Oct, 11-Nov\n");
						printf("For day of week, valid replies are:     1-7:  1-Sun, 2-Mon, 3-Tue, 4-Wed, 5-Thu, 6-Fri, 7-Sat\n");
						printf("For which day, valid replies are:       1-5:  1-1st, 2-2nd, 3-3rd, 4-4th, 5-last\n");
						dstInfo.startMonth = getInt("Enter the month in which your timezone begins observing DST/Summer Time", 3, 11, FALSE, 0);
						dstInfo.startDay = getInt("Enter the day of week on which your timezone begins observing DST/Summer Time", 1, 7, FALSE, 0);
						snprintf(questionText, sizeof(questionText), "Enter which %s in %s your timezone begins observing DST/Summer Time", DaysOfWeek[dstInfo.startDay], MonthsOfYear[dstInfo.startMonth]);
						dstInfo.startWhich = getInt(questionText, 1, 5, FALSE, 0);
						dstInfo.endMonth = getInt("Enter the month in which your timezone ends observing DST/Summer Time", 3, 11, FALSE, 0);
						dstInfo.endDay = getInt("Enter the day of week on which your timezone ends observing DST/Summer Time", 1, 7, FALSE, 0);
						snprintf(questionText, sizeof(questionText), "Enter which %s in %s your timezone ends observing DST/Summer Time", DaysOfWeek[dstInfo.endDay], MonthsOfYear[dstInfo.endMonth]);
						dstInfo.endWhich = getInt(questionText, 1, 5, FALSE, 0);
						printf("You have selected the following start/end parameters:\n");
						printf("  Start DST/Summer Time on the %s %s in %s\n", Ordinals[dstInfo.startWhich], DaysOfWeek[dstInfo.startDay], MonthsOfYear[dstInfo.startMonth]);
						printf("  End DST/Summer Time on the %s %s in %s\n", Ordinals[dstInfo.endWhich], DaysOfWeek[dstInfo.endDay], MonthsOfYear[dstInfo.endMonth]);
						done = getYesNo("Does this reflect your timezone's start and end DST/Summer Time adjustments? [y]", 1);
					}
				}
			}
	
		} /* not using server TZ info */
	} else {
		tzOffset = NO_TZ_DST_SETUP;
		memset((void *)&dstInfo, 0, sizeof(dstInfo));
	}

	return;
}

int validateIpAddrSection(char *p)
{
	int res = 0;
	int digitCount = 0;

	while(isdigit(*p++))
	{
		digitCount++;
	}
	if (digitCount <= 3)
		res = digitCount;

	return(res);
}

int getIpAddress(char *ipAddress, char *promptText)
{
	char inbuf[64];
	int ipAddrParts[4];
	int res = 1;
	int done = 0;
	int i = 0;
	char *p;
	int goodSyntax = 1;

	while (!done)
	{
		p = inbuf;
		printf("Enter IP address for %s (or none): ", promptText);
		if (fgets(inbuf, 64, stdin) == NULL) {
			goodSyntax = 0;
			printf("\n");
		} else {
			if (strncmp(inbuf, "none", 4) == 0)
			{
				return 0;
			}
			goodSyntax = 1;
			for (i = 0; i < 4; i++)
			{
				if ((res = validateIpAddrSection(p)) > 0)
					p += res;
				else
					goodSyntax = 0;

				if (i < 3)
				{
					if (*p == '.')
						p++;
					else
						goodSyntax = 0;
				}
			}
		}

		if (goodSyntax)
		{
			sscanf(inbuf, "%d.%d.%d.%d", &ipAddrParts[0], &ipAddrParts[1], &ipAddrParts[2], &ipAddrParts[3]);
			goodSyntax = 1;
			i = 0;
			while (goodSyntax && (i < 4))
			{
				if ((ipAddrParts[i] < 0) || (ipAddrParts[i] > 255))
					goodSyntax = 0;
				i++;
			}
		}

		if (!goodSyntax)
			fprintf(stderr, "Bad IP Address syntax: %s", inbuf);
		else
			done = 1;
	}

	if (goodSyntax)
		snprintf(ipAddress, 16, "%d.%d.%d.%d", ipAddrParts[0], ipAddrParts[1], ipAddrParts[2], ipAddrParts[3]);
	else
		res = 0;

	return(res);
}

int validLicenseKey(char *key)
{
	char *p;
	int goodSyntax = 1;
	int i;
	int j;

	/* valid syntax is 5 groups of six alphanumeric chars, each followed by a hyphen, and lastly
	   a single alphanumeric */
	p = key;
	for (i = 0; (i < 5) && goodSyntax; i++)
	{
		for (j = 0; (j < 6) && goodSyntax; j++)
		{
			if (!isalnum(*p++))
				goodSyntax = 0;
			if (j == 5)
			{
				if (*p++ != '-')
					goodSyntax = 0;
			}
		}
	}
	if (!isalnum(*p))
		goodSyntax = 0;

	return(goodSyntax);
}

void removeCR(char *buf)
{
	char *p;

	if ((p = strchr(buf, '\n')) != NULL)
		*p = '\0';
}

int getLicenseKey(char *smKey, char *promptText)
{
	int done = 0;
	int res = 1;
	char keyBuf[80];

	while (!done)
	{
		printf("Enter license key for %s: ", promptText);
		if (fgets(keyBuf, 80, stdin)) {
			removeCR(keyBuf);

			if (validLicenseKey(keyBuf))
				done = 1;
			else
				fprintf(stderr, "Bad license key syntax\n");
		}
	}

	strcpy(smKey, keyBuf);

	return(res);
}

int storeChassisInfo(char *ipAddr, char *smKey)
{
	chassisInfo_t *oldTable;
	int i;
	int res = 1;

	if (numChassis == chassisInfoTableSize)
	{
		oldTable = chassisInfoTable;
		if ((chassisInfoTable = (chassisInfo_t *)malloc(sizeof(chassisInfo_t) * (chassisInfoTableSize + CHASSISINFOBLOCK))) == NULL)
		{
			fprintf(stderr, "Error allocating table for chassis info\n");
			return(0);
		}
		if (oldTable != NULL)
		{
			for (i = 0; i < chassisInfoTableSize; i++)
				memcpy(&chassisInfoTable[i], &oldTable[i], sizeof(chassisInfo_t));
			free(oldTable);
		}
		chassisInfoTableSize += CHASSISINFOBLOCK;
	}

	memcpy(&chassisInfoTable[numChassis].chassisIpAddr, ipAddr, strlen(ipAddr)+1);
	memcpy(&chassisInfoTable[numChassis].chassisSMKey, smKey, strlen(smKey)+1);
	numChassis++;

	return(res);
}

int getChassisSpecifics(int argc, char *argv[])
{
	int res = 1;
	int i;
#if 0
	int ipAddr;
	int licenseKey;
	char promptText[20];
#endif
	char chassisIpAddr[20];
	char chassisSMKey[50];

	for (i = 2; i < argc; i++)
	{
#if 0
		sprintf(promptText, "chassis %s", argv[i]);
		printf("%s\n", promptText);
		ipAddr = getYesNo("Do you wish to set the chassis IP address for this chassis? [n]", 0);
		if (ipAddr)
		{
			if (!(res = getIpAddress(chassisIpAddr, promptText)))
				NOIPADDR(chassisIpAddr);
		}
		else
			NOIPADDR(chassisIpAddr);
#else
		NOIPADDR(chassisIpAddr);
#endif

#if 0
		licenseKey = getYesNo("Do you wish to add an SM license key for this chassis? [y]", 1);
		if (licenseKey)
		{
			if (!(res = getLicenseKey(chassisSMKey, promptText)))
				strcpy(chassisSMKey, "NO-KEY");
		}
		else
			strcpy(chassisSMKey, "NO-KEY");
		res = storeChassisInfo(chassisIpAddr, chassisSMKey);
#else
		strcpy(chassisSMKey, "NO-KEY");
		res = storeChassisInfo(chassisIpAddr, chassisSMKey);
#endif
	}

	return(res);
}

int getGeneralInfo()
{
	int cfgSyslog = 0;
	int useSyslogServer = 0;
	int useSyslogPort = 0;
	int useSyslogFacility = 0;
	int useNTPServer = 0;
	int setTzInfo = 0;
	int setLinkWidth = 0;
	int setLinkCRCMode = 0;
	int res = 1;

	cfgSyslog = getYesNo("Do you wish to adjust syslog configuration settings? [y]", 1);
	if (cfgSyslog)
	{
		useSyslogServer = getYesNo("Do you wish to configure a syslog server? [y]", 1);
		if (useSyslogServer)
		{
			if (!(res = getIpAddress(syslogIpAddr, "syslog server")))
				NOIPADDR(syslogIpAddr);
		}
		else
		{
			NOIPADDR(syslogIpAddr);
		}
		useSyslogPort = getYesNo("Do you wish to configure the syslog TCP/UDP port number? [n]", 0);
		if (useSyslogPort)
		{
			syslogPort = getInt("Enter TCP/UDP port number for syslog", 1, 1<<16, TRUE, SYSLOG_PORT_DEFAULT);
		}
		useSyslogFacility = getYesNo("Do you wish to configure the syslog facility? [n]", 0);
		if (useSyslogFacility)
		{
			syslogFacility = getInt("Enter facility level for syslog (0-23)", 0, 23, TRUE, SYSLOG_FACILITY_DEFAULT);
		}
	}
	else
	{
		NOIPADDR(syslogIpAddr);
	}

	useNTPServer = getYesNo("Do you wish to configure an NTP server? [y]", 1);
	if (useNTPServer)
	{
		if (!(res = getIpAddress(ntpIpAddr, "NTP server")))
			NOIPADDR(ntpIpAddr);
	}
	else
		NOIPADDR(ntpIpAddr);

	setTzInfo = getYesNo("Do you wish to configure timezone and DST information? [y]", 1);
	getTimeZoneInfo(setTzInfo);

	setLinkWidth = getYesNo("Do you wish to configure the chassis link width? [n]", 0);
	if (setLinkWidth)
	{
		printf("\t1 : 1X\n\t2 : 2X\n\t3 : 1X/2X\n\t4 : 3X\n\t5 : 1X/3X\n\t6 : 2X/3X\n\t7 : 1X/2X/3X\n\t8 : 4X\n\t9 : 1X/4X\n\t10 : 2X/4X\n\t11 : 1X/2X/4X\n\t12 : 3X/4X\n\t13 : 1X/3X/4X\n\t14 : 2X/3X/4X\n\t15 : 1X/2X/3X/4X\n");
		linkWidthSelection = getInt("Enter selection", 1, 15, TRUE, 0);
		if (linkWidthSelection > 0)
		{
			printf("*******************************************************************************\n");
			printf("*** Note: a reboot of all chassis devices is required in order to activate\n*** changes to the chassis link width\n");
			printf("*******************************************************************************\n");
		}
	} else
		linkWidthSelection = 0;

	setname = getYesNo("Do you wish to configure OPA Node Desc to match ethernet chassis name? [y]", 1);
	if (setname) {
		printf("*******************************************************************************\n");
		printf("*** Note: a reboot of all chassis devices is required in order to activate\n*** changes to the chassis OPA Node Desc\n");
		printf("*******************************************************************************\n");
	}

	setLinkCRCMode = getYesNo("Do you wish to configure the Link CRC Mode? [n]", 0);
	if (setLinkCRCMode) {
		printf("\t0 : 16b,\n\t1 : 14b/16b,\n\t2 : 48b/16b,\n\t3 : 48b/14b/16b,\n\t4 : per-lane/16b,\n\t5 : per-lane/14b/16b,\n\t6 : per-lane/48b/16b,\n\t7 : per-lane/48b/14b/16b\n");
		linkCRCModeSelection = getInt("Enter selection", 0, 7, TRUE, -1);
		if (linkCRCModeSelection > -1)
		{
			printf("*******************************************************************************\n");
			printf("*** Note: a reboot of all chassis devices is required in order to activate\n*** changes to the chassis link CRC mode\n");
			printf("*******************************************************************************\n");
		}
	} else
		linkCRCModeSelection = -1;

	return(res);
}

int
main(int argc, char *argv[])
{
	int i;
	FILE *fp_out;
	char *dirName;

	chassisInfoTable = NULL;
	chassisInfoTableSize = 0;
	numChassis = 0;

	dirName = argv[1];
	if (dirName == NULL)
		dirName = ".";

	if (chdir(dirName) < 0) {
		fprintf(stderr, "Error changing directory to %s: %s\n", dirName, strerror(errno));
		exit(1);
	}

	getChassisSpecifics(argc, argv);
	getGeneralInfo();

	if ((fp_out = fopen("./.chassisSetup.out","w")) == NULL)
	{
		fprintf(stderr, "Error opening .chassisSetup.out for output: %s\n", strerror(errno));
		exit(1);
	}

	/* display chassis info */
	fprintf(fp_out, "Number of chassis: %d\n", numChassis);
	for (i = 0; i < numChassis; i++)
	{
		fprintf(fp_out, "%d Chassis %s IP_Addr: %s\n", i+2, argv[i+2], chassisInfoTable[i].chassisIpAddr);
		fprintf(fp_out, "%d Chassis %s SM_Key: %s\n", i+2, argv[i+2], (chassisInfoTable[i].chassisSMKey != NULL) ? chassisInfoTable[i].chassisSMKey : "NULL");
	}

	/* display general info */
	fprintf(fp_out, "Syslog Server IP_Address:%s\n", syslogIpAddr);
	fprintf(fp_out, "Syslog Port:%d\n", syslogPort);
	fprintf(fp_out, "Syslog Facility:%d\n", syslogFacility);
	fprintf(fp_out, "NTP Server IP_Address:%s\n", ntpIpAddr);
	fprintf(fp_out, "Timezone offset:%d\n", tzOffset);
	fprintf(fp_out, "Start DST:%d %d %d\n",
			dstInfo.startWhich,
			dstInfo.startDay,
			dstInfo.startMonth);
	fprintf(fp_out, "End DST:%d %d %d\n",
			dstInfo.endWhich,
			dstInfo.endDay,
			dstInfo.endMonth);
	fprintf(fp_out, "Link Width Selection:%d\n", linkWidthSelection);
	fprintf(fp_out, "Set OPA Node Desc:%s\n", setname?"y":"n");
	fprintf(fp_out, "Link CRC Mode:%d\n", linkCRCModeSelection);

	fclose(fp_out);
	exit(0);
}
