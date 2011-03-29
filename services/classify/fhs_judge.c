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
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include "hash.c"
#include "hyperspace.c"

char *judge_file;
char *fhs_dir;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 9)
	{
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_FILE_TO_JUDGE\n");
		printf("\t-d CATEGORY_FHS_FILES_DIR\n");
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
			fhs_dir = malloc(len + 1);
			sscanf(argv[i+1], "%s", fhs_dir);
			if(fhs_dir[len-1] == '/') fhs_dir[len-1] = '\0';
		}
	}
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", judge_file);*/
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

void loadMassCategories(void)
{
DIR *dirp;
struct dirent *dp;
char old_dir[PATH_MAX];
int name_len;
char *cat_name;

	getcwd(old_dir, PATH_MAX);
	chdir(fhs_dir);
	preLoadHyperSpace("preload.fhs");
	chdir(old_dir);

	if ((dirp = opendir(fhs_dir)) == NULL)
	{
		printf("couldn't open '%s'", fhs_dir);
		return;
	}

	chdir(fhs_dir);
	do {
		errno = 0;
		if ((dp = readdir(dirp)) != NULL)
		{
			if (strcmp(dp->d_name, "preload.fhs") != 0 && strstr(dp->d_name, ".fhs") != NULL)
			{
				name_len = strstr(dp->d_name, ".fhs") - dp->d_name;
				cat_name = malloc(name_len + 1);
				strncpy(cat_name, dp->d_name, name_len);
				cat_name[name_len] = '\0';
				loadHyperSpaceCategory(dp->d_name, cat_name);
				free(cat_name);
			}
		}
	} while (dp != NULL);
	if (errno != 0)
		perror("error reading directory");
	else
		(void) closedir(dirp);

	chdir(old_dir);
}

int main (int argc, char *argv[])
{
regexHead myRegexHead;
wchar_t *myData;
HashList myHashes;
clock_t start, end;
hsClassification classification;
	checkMakeUTF8();
	initHTML();
	initHyperSpaceClassifier();
	if(readArguments(argc, argv)==-1) exit(-1);
	myData=makeData(judge_file);

	printf("Loading hashes -- be patient!\n");
	loadMassCategories();

	printf("Classifying\n");
	start=clock();
	mkRegexHead(&myRegexHead, myData);
	removeHTML(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);
	normalizeCurrency(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);

//	printf("%ld: %.*ls\n", myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.main_memory);

	myHashes.hashes = malloc(sizeof(hyperspaceFeature) * HYPERSPACE_MAX_FEATURE_COUNT);
	myHashes.slots = HYPERSPACE_MAX_FEATURE_COUNT;
	myHashes.used = 0;
	computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);

	classification=doHSPrepandClassify(&myHashes);
	end=clock();
	printf("Classification took %lf milliseconds\n", (double)((end-start)/(CLOCKS_PER_SEC/1000)));
	printf("Best match: %s prob: %lf pR: %lf\n", classification.name, classification.probability, classification.probScaled);

	free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
	free(judge_file);
	deinitHyperSpaceClassifier();
	deinitHTML();
	return 0;
}
