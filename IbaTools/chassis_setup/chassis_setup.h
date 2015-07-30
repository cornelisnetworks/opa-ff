/* BEGIN_ICS_COPYRIGHT3 ****************************************

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

** END_ICS_COPYRIGHT3   ****************************************/

/* [ICS VERSION STRING: unknown] */

#ifndef CHASSISSETUP_H_INCLUDED
#define CHASSISSETUP_H_INCLUDED

/* chassisSetup.h */

#define TIMEZONEOFFSET	(timezone / (60*60))

typedef struct dst_struct
{
	int startWhich;		/* 1 = 1st, 2 = 2nd, 3 = 3rd, 4 = 4th, 5 = 5th */
	int startDay;		/* 1 = Sun, 2 = Mon, 3 = Tue, 4 = Wed, 5 = Thu, 6 = Fri, 7 = Sat */
	int startMonth;		/* 3 = Mar, 4 = Apr, 5 = May, 6 = Jun, 7 = Jul, 8 = Aug, 9 = Sep, 10 = Oct, 11 = Nov */
	int endWhich;		/* 1 = 1st, 2 = 2nd, 3 = 3rd, 4 = 4th, 5 = 5th */
	int endDay;			/* 1 = Sun, 2 = Mon, 3 = Tue, 4 = Wed, 5 = Thu, 6 = Fri, 7 = Sat */
	int endMonth;		/* 3 = Mar, 4 = Apr, 5 = May, 6 = Jun, 7 = Jul, 8 = Aug, 9 = Sep, 10 = Oct, 11 = Nov */
} dstInfo_t;

char *DaysOfWeek[] =
{
	"Zero",
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

char *MonthsOfYear[] =
{
	"Zero",
	"January", 	
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

char *Ordinals[] =
{
	"zero",
	"first",
	"second",
	"third",
	"fourth",
	"last"
};

typedef enum
{
	TZ_NA_ATLA = 1,
	TZ_NA_EAST,
	TZ_NA_CENT,
	TZ_NA_MOUN,
	TZ_NA_PACI,
	TZ_NA_ASKA,
	TZ_NA_HAWI,
	TZ_EU_UTC,
	TZ_EU_CENT,
	TZ_EU_EAST,
	TZ_EU_MOSC,
	TZ_AS_CHIN,
	TZ_AS_JAPN,
	TZ_AU_SYDN,
	TZ_NZ_AUCK,
} tzCode_t;

/* canned DST settings */
dstInfo_t NA_DSTInfo		=		{2, 1, 3, 1, 1, 11};		/* 2nd Sun in Mar to 1st Sun in Nov */
dstInfo_t EUR_DSTInfo		=		{5, 1, 3, 5, 1, 10};		/* last Sun in Mar to last Sun in Oct */
dstInfo_t AUS_DSTInfo		=		{1, 1, 10, 1, 1, 4};		/* 1st Sun in Oct to 1st Sun in Apr */
dstInfo_t NZ_DSTInfo		=		{5, 1, 9, 1, 1, 4};			/* last Sun in Sep to 1st Sun in Apr */
dstInfo_t None_DSTInfo		=		{0, 0, 0, 0, 0, 0};			/* No DST in effect */


#define	NO_IP_ADDR	"0.0.0.0"

#define NOIPADDR(IpAddr)	(strcpy(IpAddr, NO_IP_ADDR))

typedef struct chassisInfo_st
{
	char chassisIpAddr[20];
	char chassisSMKey[50];
} chassisInfo_t;

#define CHASSISINFOBLOCK 10

#endif /* CHASSISSETUP_H_INCLUDED */
