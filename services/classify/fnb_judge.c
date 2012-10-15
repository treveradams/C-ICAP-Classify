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
#include <time.h>
#include <dirent.h>

#include "hash.c"
#include "bayes.c"
#include "train_common.h"

char *judge_file;
char *fbc_dir;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 9)
	{
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_FILE_TO_JUDGE\n");
		printf("\t-d CATEGORY_FNB_FILES_DIR\n");
		printf("Spaces and case matter.\n");
		return -1;
	}
	for(i=1; i<9; i+=2)
	{
		if(strcmp(argv[i], "-p") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED1);
		else if(strcmp(argv[i], "-s") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED2);
		else if(strcmp(argv[i], "-i") == 0)
		{
			judge_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", judge_file);
		}
		else if(strcmp(argv[i], "-d") == 0)
		{
			int len = strlen(argv[i+1]);
			fbc_dir = malloc(len + 1);
			sscanf(argv[i+1], "%s", fbc_dir);
			if(fbc_dir[len-1] == '/') fbc_dir[len-1] = '\0';
		}
	}
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", judge_file);*/
	return 0;
}

int main (int argc, char *argv[])
{
regexHead myRegexHead = {.head = NULL, .tail = NULL, .dirty = 0, . main_memory = NULL, .arrays = NULL, .lastarray = NULL};
wchar_t *myData;
HashList myHashes;
clock_t start, end;
HTMLClassification classification;
	checkMakeUTF8();
	initHTML();
	initBayesClassifier();
	if(readArguments(argc, argv)==-1) exit(-1);
	myData=makeData(judge_file);

	printf("Loading hashes -- be patient!\n");
	loadMassBayesCategories(fbc_dir);
	optimizeFBC(&NBJudgeHashList);

	printf("Classifying\n");
	start=clock();
	mkRegexHead(&myRegexHead, myData, 0);
	removeHTML(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);
	normalizeCurrency(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);

//	printf("%ld: %.*ls\n", myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.main_memory);

	myHashes.hashes = malloc(sizeof(HTMLFeature) * HTML_MAX_FEATURE_COUNT);
	myHashes.slots = HTML_MAX_FEATURE_COUNT;
	myHashes.used = 0;
	computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);

	classification=doBayesPrepandClassify(&myHashes);
	end=clock();
	printf("Classification took %lf milliseconds\n", (double)((end-start)/(CLOCKS_PER_SEC/1000)));
	printf("Best match: %s prob: %lf pR: %lf\n", classification.primary_name, classification.primary_probability, classification.primary_probScaled);
	if(classification.secondary_name != NULL)
		printf("Best match: %s prob: %lf pR: %lf\n", classification.secondary_name, classification.secondary_probability, classification.secondary_probScaled);

	free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
	free(judge_file);
	free(fbc_dir);
	deinitBayesClassifier();
	deinitHTML();
	return 0;
}
