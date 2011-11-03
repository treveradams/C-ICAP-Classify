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
#include <string.h>

#include "hash.c"
#include "hyperspace.c"

char *judge_dir;
char *fhs_dir;
char *train_as_category;
double threshhold;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 13)
	{
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_DIRECTORY_TO_JUDGE\n");
		printf("\t-d CATEGORY_FHS_FILES_DIR\n");
		printf("\t-c CATEGORY_INPUT_DIRECTORY_FILES_SHOULD_BE\n");
		printf("\t-t THRESHHOLD_NOT_TO_EXCEED_TO_REPORT_NEED_TO_TRAIN\n");
		printf("Spaces and case matter.\n");
		return -1;
	}
	for(i=1; i<13; i+=2)
	{
		if(strcmp(argv[i], "-p") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED1);
		else if(strcmp(argv[i], "-s") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED2);
		else if(strcmp(argv[i], "-i") == 0)
		{
			judge_dir = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", judge_dir);
		}
		else if(strcmp(argv[i], "-d") == 0)
		{
			int len = strlen(argv[i+1]);
			fhs_dir = malloc(len + 1);
			sscanf(argv[i+1], "%s", fhs_dir);
			if(fhs_dir[len-1] == '/') fhs_dir[len-1] = '\0';
		}
		else if(strcmp(argv[i], "-c") == 0)
		{
			int len = strlen(argv[i+1]);
			train_as_category = malloc(len + 1);
			sscanf(argv[i+1], "%s", train_as_category);
		}
		else if(strcmp(argv[i], "-t") == 0)
		{
			sscanf(argv[i+1], "%lf", &threshhold);
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
	myData=malloc((stat_buf.st_size+1) * UTF32_CHAR_SIZE);
	realLen=mbstowcs(myData, tempData, stat_buf.st_size);
	if(realLen!=-1) myData[realLen] = L'\0';
	else ci_debug_printf(10, "Bad character data in %s\n", input_file);
	free(tempData);
	return myData;
}

HTMLClassification judgeFile(char *judge_file)
{
regexHead myRegexHead = {.head = NULL, .tail = NULL, .dirty = 0, . main_memory = NULL, .arrays = NULL, .lastarray = NULL};
wchar_t *myData;
HashList myHashes;
HTMLClassification classification;

	myData = makeData(judge_file);

	mkRegexHead(&myRegexHead, myData);
	removeHTML(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);
	normalizeCurrency(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);

//	printf("%ld: %.*ls\n", myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.main_memory);

	myHashes.hashes = malloc(sizeof(HTMLFeature) * HTML_MAX_FEATURE_COUNT);
	myHashes.slots = HTML_MAX_FEATURE_COUNT;
	myHashes.used = 0;
	computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);

	classification = doHSPrepandClassify(&myHashes);

	free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
	return classification;
}

int judgeDirectory(char *directory)
{
char full_path[PATH_MAX];
char lowest_file[PATH_MAX];
HTMLClassification lowest = { .primary_name = NULL, .primary_probability = 0.0, .primary_probScaled = 0.0, .secondary_name = NULL, .secondary_probability = 0.0, .secondary_probScaled = 0.0  } , this;

DIR *dirp;
struct dirent *dp;
struct stat info;

	if ((dirp = opendir(directory)) == NULL)
	{
		printf("couldn't open '%s'", directory);
		return -1;
	}

	do {
		errno = 0;
		if ((dp = readdir(dirp)) != NULL)
		{
			snprintf(full_path, PATH_MAX, "%s/%s", directory, dp->d_name);
			stat(full_path, &info);
			if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0 && S_ISREG(info.st_mode))
			{
				this = judgeFile(full_path);
				if(lowest.primary_name == NULL)
				{
					strcpy(lowest_file, dp->d_name);
					lowest = this;
				}
				else if(strcmp(lowest.primary_name, train_as_category) == 0) // lowest is the correct category
				{
					if(strcmp(this.primary_name, train_as_category) != 0) // this is NOT the correct category
					{
						strcpy(lowest_file, dp->d_name);
						lowest = this;
					}
					else if(lowest.primary_probability > this.primary_probability) // this is the correct category, but a lower score
					{
						strcpy(lowest_file, dp->d_name);
						lowest = this;
					}
				}
				else { // lowest is NOT the correct category
					if(strcmp(this.primary_name, train_as_category) != 0 && lowest.primary_probability < this.primary_probability) // this is NOT the correct category and has a higher score (which is further from the right category)
					{
						strcpy(lowest_file, dp->d_name);
						lowest = this;
					}
				}
			}
		}
	} while (dp != NULL);
	if (errno != 0)
		perror("error reading directory");
	else
		(void) closedir(dirp);

	if(strcmp(lowest.primary_name, train_as_category) != 0 || (strcmp(lowest.primary_name, train_as_category) == 0 && lowest.primary_probability < threshhold))
	{
		printf("%s", lowest_file);
		return 0;
	}
	return -1;
}

void setupPrimarySecondary(char *primary, char *secondary, int bidirectional)
{

	if(number_secondaries == 0 || secondary_compares == NULL)
	{
		secondary_compares = malloc(sizeof(secondaries_t));
	}
	else
	{
		secondary_compares = realloc(secondary_compares, sizeof(secondaries_t) * (number_secondaries + 1));
	}

	if(tre_regcomp(&secondary_compares[number_secondaries].primary_regex, primary, REG_EXTENDED | REG_ICASE) != 0 ||
		tre_regcomp(&secondary_compares[number_secondaries].secondary_regex, secondary, REG_EXTENDED | REG_ICASE) != 0)
	{
		tre_regfree(&secondary_compares[number_secondaries].primary_regex);
		tre_regfree(&secondary_compares[number_secondaries].secondary_regex);
		number_secondaries--;
		ci_debug_printf(1, "Invalid REGEX In Setting parameter (PRIMARY_CATEGORY_REGEX: %s SECONDARY_CATEGORY_REGEX: %s BIDIRECTIONAL: %s)\n", primary, secondary, bidirectional ? "TRUE" : "FALSE" );
		exit(-1);
	}
	secondary_compares[number_secondaries].bidirectional = bidirectional;

	number_secondaries++;
}

int main (int argc, char *argv[])
{
int ret;
	checkMakeUTF8();
	initHTML();
	initHyperSpaceClassifier();
	if(readArguments(argc, argv)==-1) exit(-1);

	loadMassHSCategories(fhs_dir);
	setupPrimarySecondary("adult.affairs", "social.dating", 1);
	setupPrimarySecondary("adult.*", "adult.*", 0);
	setupPrimarySecondary("clothing.*","clothing.*", 0);
	setupPrimarySecondary("information.culinary","commerce.food", 1);

	ret = judgeDirectory(judge_dir);

	free(judge_dir);
	free(fhs_dir);
	free(train_as_category);
	deinitHyperSpaceClassifier();
	deinitHTML();
	return ret;
}
