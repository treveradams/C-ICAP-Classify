/*
 *  Copyright (C) 2008-2011 Trever L. Adams
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
#include <dirent.h>
#include <errno.h>

#include "hash.c"
#include "bayes.c"
#include "sqlite3.h"

char *learn_in_file;
char *fbc_out_file;
int do_directory_learn = -1;
uint32_t zero_point = 0;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 9 || (argc -1) % 2)
	{
		HELP:
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_FILE_TO_LEARN -- cannot be used with -d\n");
		printf("\t-d INPUT_DIRECTORY_TO_LEARN -- cannot be used with -i\n");
		printf("\t-o OUTPUT_FNB_FILE\n");
		printf("\t-z ZERO (REMOVE) HASH IF COUNT IS LESS THAN THIS NUMBER\n");
		printf("Spaces and case matter.\n");
		return -1;
	}
	for(i = 1; i < argc; i += 2)
	{
		if(strcmp(argv[i], "-p") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED1);
		else if(strcmp(argv[i], "-s") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED2);
		else if(strcmp(argv[i], "-i") == 0)
		{
			if(do_directory_learn == 1) goto HELP;
			learn_in_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", learn_in_file);
			do_directory_learn = 0;
		}
		else if(strcmp(argv[i], "-d") == 0)
		{
			if(do_directory_learn == 0) goto HELP;
			learn_in_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", learn_in_file);
			do_directory_learn = 1;
		}
		else if(strcmp(argv[i], "-o") == 0)
		{
			fbc_out_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", fbc_out_file);
		}
		else if(strcmp(argv[i], "-z") == 0)
		{
			zero_point = strtol(argv[i+1], NULL, 10);
		}
	}
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", learn_in_file);
	printf("FNV Output File: %s\n", fbc_out_file);*/
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

void doLearn(char *data_file)
{
regexHead myRegexHead;
wchar_t *myData;
HashList myHashes;
	myData=makeData(data_file);
	mkRegexHead(&myRegexHead, myData);
	removeHTML(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);
	normalizeCurrency(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);

//	printf("%ls\n", myRegexHead.main_memory);

	myHashes.hashes = malloc(sizeof(HTMLFeature) * FBC_MAX_FEATURE_COUNT);
	myHashes.slots = FBC_MAX_FEATURE_COUNT;
	myHashes.used = 0;

	computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);

	learnHashesBayesCategory(0, &myHashes);

	free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
}

void learnDirectory(char *directory)
{
DIR *dirp;
struct dirent *dp;
char full_path[PATH_MAX];

	if ((dirp = opendir(directory)) == NULL)
	{
		printf("couldn't open '%s'", directory);
		return;
	}

	do {
		errno = 0;
		if ((dp = readdir(dirp)) != NULL)
		{
			if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
			{
				snprintf(full_path, PATH_MAX, "%s/%s", directory, dp->d_name);
				printf("Learning %s\n", dp->d_name);
				fprintf(stderr, "Learning %s\n", dp->d_name);
				doLearn(full_path);
			}
		}
	} while (dp != NULL);
	if (errno != 0)
		perror("error reading directory");
	else
		(void) closedir(dirp);
}

int main (int argc, char *argv[])
{
FBC_HEADERv1 header;
int fbc_file;
	if(readArguments(argc, argv)==-1) exit(-1);
	checkMakeUTF8();
	initHTML();
	initBayesClassifier();

        if(loadBayesCategory(fbc_out_file, "LEARNING") == -1)
		printf("Unable to open %s\n", fbc_out_file);

	if(do_directory_learn == 0) doLearn(learn_in_file);
	else learnDirectory(learn_in_file);

	fbc_file = openFBC(fbc_out_file, &header, 1);
	if(writeFBCHashes(fbc_file, &header, &NBJudgeHashList, 0, zero_point) == -1)
		printf("MAJOR PROBLEM: Input file: %s had no hashed data!\n", learn_in_file);
	close(fbc_file);

	free(learn_in_file);
	free(fbc_out_file);
	deinitBayesClassifier();
	deinitHTML();
	return 0;
}
