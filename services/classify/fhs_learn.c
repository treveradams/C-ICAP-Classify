/*
 *  Copyright (C) 2008-2012 Trever L. Adams
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

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#if (_FILE_OFFSET_BITS != 64)
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

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
#include <wchar.h>
#include <wctype.h>

#include "hash.c"
#include "hyperspace.c"
#include "train_common.h"

/* To find out how many collisions we have, run with computeOSBHashes debug printfs on, sort -u the captured output then run:
   awk '{print $4, $5, $6, $7, $2, $3}' /tmp/hashtest2.txt | uniq -D -s 3 | wc -l
divide the number by 2 */

char *fhs_out_file = NULL;
char *learn_in_file = NULL;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 9)
	{
		HELP:
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
	if(learn_in_file == NULL) goto HELP;
	if(fhs_out_file == NULL) goto HELP;
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", learn_in_file);
	printf("FHS Output File: %s\n", fhs_out_file);*/
	return 0;
}

int main (int argc, char *argv[])
{
regexHead myRegexHead = {.head = NULL, .tail = NULL, .dirty = 0, . main_memory = NULL, .arrays = NULL, .lastarray = NULL};
wchar_t *myData;
HashList myHashes;
FHS_HEADERv1 header;
int fhs_file;
	if(readArguments(argc, argv)==-1) exit(-1);
	checkMakeUTF8();
	initHTML();
	myData=makeData(learn_in_file);
	mkRegexHead(&myRegexHead, myData, 0);
	removeHTML(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);
	normalizeCurrency(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);

//	printf("%ls\n", myRegexHead.main_memory);

	myHashes.hashes = malloc(sizeof(HTMLFeature) * HTML_MAX_FEATURE_COUNT);
	myHashes.slots = HTML_MAX_FEATURE_COUNT;
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
