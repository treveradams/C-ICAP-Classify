/*
 *  Copyright (C) 2008-2010 Trever L. Adams
 *
 *  This file is part of srv_classify c-icap module and accompanying tools.
 *
 *  srv_classify is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of
 *  the License, or (at your option) any later version.
 *
 *  srv_classify is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */


#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE

#define NOT_CICAP

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <langinfo.h>
#include <wchar.h>
#include <wctype.h>

#include "hash.c"
#include "hyperspace.c"


/* To find out how many collisions we have, run with computeOSBHashes debug printfs on, sort -u the captured output then run:
   awk '{print $4, $5, $6, $7, $2, $3}' /tmp/hashtest2.txt | uniq -D -s 3 | wc -l
divide the number by 2 */

char *fhs_out_file;
char *learn_in_file;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 9)
	{
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_FILE_TO_LEARN\n");
		printf("\t-o OUTPUT_FHS_FILE\n");
		printf("Spaces and case matter.\n");
		return -1;
	}
	for(i=1; i<8; i+=2)
	{
		if(strcmp(argv[i], "-p") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED1);
		else if(strcmp(argv[i], "-s") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED2);
		else if(strcmp(argv[i], "-i") == 0)
		{
			learn_in_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", learn_in_file);
		}
		else if(strcmp(argv[i], "-o") == 0)
		{
			fhs_out_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", fhs_out_file);
		}
	}
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", learn_in_file);
	printf("FHS Output File: %s\n", fhs_out_file);*/
	return 0;
}

void checkMakeUTF8(void)
{
	setlocale(LC_ALL, "");
	int utf8_mode = (strcmp(nl_langinfo(CODESET), "UTF-8") == 0);
	if(!utf8_mode) setlocale(LC_ALL, "en_US.UTF-8");
}

wchar_t *makeData(char *input_file)
{
int data=0;
struct stat stat_buf;
char *tempData=NULL;
wchar_t *myData=NULL;
int32_t realLen;
	data=open(input_file, O_RDONLY);
	fstat(data, &stat_buf);
	tempData=malloc(stat_buf.st_size+1);
	if(tempData==NULL) exit(-1);
	read(data, tempData, stat_buf.st_size);
	close(data);
	tempData[stat_buf.st_size] = '\0';
	myData=malloc((stat_buf.st_size+1) * sizeof(wchar_t));
	realLen=mbstowcs(myData, tempData, stat_buf.st_size);
	if(realLen!=-1) myData[realLen] = L'\0';
	else printf("Bad character data in %s\n", input_file);
	free(tempData);
	return myData;
}

int main (int argc, char *argv[])
{
regexHead myRegexHead;
wchar_t *myData;
HashList myHashes;
FHS_HEADERv1 header;
int fhs_file;
	if(readArguments(argc, argv)==-1) exit(-1);
	checkMakeUTF8();
	initHTML();
	myData=makeData(learn_in_file);
	mkRegexHead(&myRegexHead, myData);
	removeHTML(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);
	normalizeCurrency(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);

//	printf("%ls\n", myRegexHead.main_memory);

	myHashes.hashes = malloc(sizeof(HTMLFeature) * HYPERSPACE_MAX_FEATURE_COUNT);
	myHashes.slots = HYPERSPACE_MAX_FEATURE_COUNT;
	myHashes.used = 0;
	computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);
	fhs_file = openFHS(fhs_out_file, &header, 1);
	if(writeFHSHashes(fhs_file, &header, &myHashes) == -1)
		printf("MAJOR PROBLEM: Input file: %s had no hashed data!\n", learn_in_file);
	close(fhs_file);

	free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
	free(learn_in_file);
	free(fhs_out_file);
	deinitHTML();
	return 0;
}
