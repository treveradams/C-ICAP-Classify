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
#include <libgen.h>

#include "hash.c"
#include "hyperspace.c"
#include "findtolearn_util.c"
#include "train_common.h"
#include "train_common_threads.h"

char *judge_dir;
char *fhs_dir;
char *train_as_category;
double threshhold;
int num_threads = 2;

pthread_mutex_t file_mtx, train_mtx;
pthread_cond_t file_avl_cnd, need_file_cnd;
int FILES_PER_NEED = 0; // Treating this like a constant
int file_need_data = 0;
int file_available = 0;
int shutdown = 0;

process_entry *file_names = NULL;
process_entry *busy_file_names = NULL;

HTMLClassification lowest = { .primary_name = NULL, .primary_probability = 0.0, .primary_probScaled = 0.0, .secondary_name = NULL, .secondary_probability = 0.0, .secondary_probScaled = 0.0  };
char lowest_file[PATH_MAX + 1] = "\0";

int readArguments(int argc, char *argv[])
{
int i;
char temp[PATH_MAX];

	if(argc < 13)
	{
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_DIRECTORY_TO_JUDGE\n");
		printf("\t-d CATEGORY_FHS_FILES_DIR\n");
		printf("\t-c TARGET_CATEGORY\n");
		printf("\t-t THRESHHOLD_NOT_TO_EXCEED_TO_REPORT_NEED_TO_TRAIN\n");
		printf("\t-m NUMBER_OF_THREADS_TO_USE (default: 2 and should be <= NUM_CORES_ON_CPU)\n");
		printf("\t-r Related categories in form of \"primary,secondary,bidirectional\". Bidirectional should be 1 for yes, 0 for no. This option should only be supplied once. To include more than one, separate with \"=\".\n");
		printf("Spaces and case matter.\n");
		return -1;
	}
	for(i = 1; i < argc; i += 2)
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
			// This is an optimization to avoid extra checks later
			lowest.primary_name = train_as_category;
		}
		else if(strcmp(argv[i], "-t") == 0)
		{
			sscanf(argv[i+1], "%lf", &threshhold);
			// This is an optimization to avoid extra checks later
			lowest.primary_probScaled = threshhold + 0.1;
		}
		else if(strcmp(argv[i], "-m") == 0)
		{
			sscanf(argv[i+1], "%d", &num_threads);
			FILES_PER_NEED = num_threads * FILES_PER_NEED_MULT;
		}
		else if(strcmp(argv[i], "-r") == 0)
		{
			strncpy(temp, argv[i+1], PATH_MAX-1);
			setupPrimarySecondFromCmdLine(temp);
		}
	}
/*	ci_debug_printf(10, "Primary Seed: %"PRIX32"\n", HASHSEED1);
	ci_debug_printf(10, "Secondary Seed: %"PRIX32"\n", HASHSEED2);
	ci_debug_printf(10, "Learn File: %s\n", judge_file);*/
	return 0;
}

HTMLClassification judgeFile(char *judge_file)
{
regexHead myRegexHead = {.head = NULL, .tail = NULL, .dirty = 0, . main_memory = NULL, .arrays = NULL, .lastarray = NULL};
wchar_t *myData;
HashList myHashes;
HTMLClassification classification;
char *temp, *prehash_file, *dirpath, *filename;
int prehash_data_file = 0;

#ifdef _GNU_SOURCE
	temp = strdup(judge_file);
	dirpath = strdup(dirname(temp));
	free(temp);
	filename = basename(judge_file);
	prehash_file = malloc(strlen(filename) + 12 + strlen(dirpath));
	strcpy(prehash_file, dirpath);
	strcat(prehash_file, "/");
	strcat(prehash_file, ".pre-hash-");
	strcat(prehash_file, filename);
	free(dirpath);
	// If we found the file, load it
	if((prehash_data_file = open(prehash_file, O_RDONLY, S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP)) > 0);
	{
		readPREHASHES(prehash_data_file, &myHashes);
		close(prehash_data_file);
	}
	if(myHashes.used < 5)
	{
		prehash_data_file = 0;
		if(myHashes.hashes) free(myHashes.hashes);
	}
#endif
	if(prehash_data_file <= 0)
	{
		myData = makeData(judge_file);

		mkRegexHead(&myRegexHead, myData, 0);
		removeHTML(&myRegexHead);
		regexMakeSingleBlock(&myRegexHead);
		normalizeCurrency(&myRegexHead);
		regexMakeSingleBlock(&myRegexHead);

	//	ci_debug_printf(10, "%ld: %.*ls\n", myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.head->rm_eo - myRegexHead.head->rm_so, myRegexHead.main_memory);

		myHashes.hashes = malloc(sizeof(HTMLFeature) * HTML_MAX_FEATURE_COUNT);
		myHashes.slots = HTML_MAX_FEATURE_COUNT;
		myHashes.used = 0;
		computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);
		myData = NULL;
	}

	classification = doHSPrepandClassify(&myHashes);

#ifdef _GNU_SOURCE
	if(prehash_data_file <= 0 && myHashes.used > 0)
	{
		// write out preload file
		prehash_data_file = open(prehash_file, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP);
		writePREHASHES(prehash_data_file, &myHashes);
		close(prehash_data_file);
	}
	free(prehash_file);
#endif
	if(myHashes.hashes) free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
	return classification;
}

int judgeDirectory(char *directory, struct thread_info *tinfo)
{
char full_path[PATH_MAX];
DIR *dirp;
struct dirent *dp;
struct stat info;
int tnum;
struct timespec delay = { 0, 10000000L};
#ifdef _BSD_SOURCE
unsigned char d_type;
#else
enum DTYPE {DT_UNKNOWN, DT_REG};
DTYPE d_type;
#endif

	if ((dirp = opendir(directory)) == NULL)
	{
		ci_debug_printf(10, "couldn't open '%s'", directory);
		return -1;
	}

	do {
		pthread_mutex_lock(&file_mtx);
		while(!file_need_data)
		{
			pthread_cond_wait(&need_file_cnd, &file_mtx);
			file_need_data += FILES_PER_NEED;
		}

		do {
			errno = 0;
			if ((dp = readdir(dirp)) != NULL)
			{
				snprintf(full_path, PATH_MAX, "%s/%s", directory, dp->d_name);
#ifdef _BSD_SOURCE
				if(dp->d_type == DT_UNKNOWN)
				{
					stat(full_path, &info);
					if(S_ISREG(info.st_mode)) d_type = DT_REG;
					else d_type = DT_UNKNOWN;
				}
				else d_type = dp->d_type;
#else
				stat(full_path, &info);
				if(S_ISREG(info.st_mode)) d_type = DT_REG;
				else d_type == DT_UNKNOWN;
#endif
				if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0 && strncmp(dp->d_name, ".pre-hash-", 10) != 0 && d_type == DT_REG)
				{
					addFile(full_path, dp->d_name);
				}
			}
		} while (dp != NULL && file_need_data);
		pthread_mutex_unlock(&file_mtx);
	} while (dp != NULL);
	if (errno != 0)
		perror("error reading directory");
	else
		(void) closedir(dirp);

	/* Tell threads to shutdown */
	pthread_mutex_lock(&file_mtx);
	shutdown = 1;
	pthread_cond_broadcast(&file_avl_cnd);
	pthread_mutex_unlock(&file_mtx);
	// Wait for everyone to be ready
	nanosleep(&delay, NULL);
	/* Wait for threads to terminate */
	for (tnum = 0; tnum < num_threads; tnum++) {
		pthread_mutex_lock(&file_mtx);
		pthread_cond_broadcast(&file_avl_cnd);
		pthread_mutex_unlock(&file_mtx);
		nanosleep(&delay, NULL);
		pthread_join(tinfo[tnum].thread_id, NULL);
	}

	if(strcmp(lowest.primary_name, train_as_category) != 0 || (strcmp(lowest.primary_name, train_as_category) == 0 && lowest.primary_probScaled < threshhold))
	{
		printf("%s", lowest_file);
		return 0;
	}
	return -1;
}


/* Thread start function: display address near top of our stack,
and return upper-cased copy of argv_string */

static void *thread_start(void *arg)
{
//struct thread_info *tinfo = (struct thread_info *) arg;
HTMLClassification this;
process_entry entry;
int new_category_correct, lowest_category_correct;

	while(!shutdown || file_available)
	{
		// check to see if file is available

		pthread_mutex_lock(&file_mtx);
		while(!file_available)
		{
			file_need_data++;
			pthread_cond_signal(&need_file_cnd);
			pthread_cond_wait(&file_avl_cnd, &file_mtx);
			if(shutdown && !file_available)
			{
				pthread_mutex_unlock(&file_mtx);
				return "SHUTDOWN_IN_RUN";
			}
		}

		entry = getNextFile();
		pthread_mutex_unlock(&file_mtx);

		this = judgeFile(entry.full_path);

		new_category_correct = (strcmp(this.primary_name, train_as_category) == 0 ? 1 : 0);

		// Avoid taking the mutex if possible. Things may change on us, so we redo all important checks inside the mutex.
		// This is fast and loose but should not cause problems as we recheck in the mutex.
		if(!new_category_correct || (new_category_correct && lowest.primary_probScaled > this.primary_probScaled))
		{
			pthread_mutex_lock(&train_mtx);

			lowest_category_correct = (strcmp(lowest.primary_name, train_as_category) == 0 ? 1 : 0);

			if(lowest_category_correct)
			{
				// this is NOT the correct category
				if(!new_category_correct)
				{
					strncpy(lowest_file, entry.file_name, PATH_MAX + 1);
					lowest = this;
//					ci_debug_printf(10, "*** Thread: %d / To Train now %s @ %f\n", tinfo->thread_num, entry.file_name, lowest.primary_probScaled);
				}
				// this is the correct category, but a lower score
				else if(lowest.primary_probScaled > this.primary_probScaled)
				{
					strncpy(lowest_file, entry.file_name, PATH_MAX + 1);
					lowest = this;
//					ci_debug_printf(10, "*** Thread: %d / To Train now %s @ %f\n", tinfo->thread_num, entry.file_name, lowest.primary_probScaled);
				}
			}
			else { // lowest is NOT the correct category
				// this is NOT the correct category and has a higher score (which is further from the right category)
				if(!new_category_correct && lowest.primary_probScaled < this.primary_probScaled)
				{
					strncpy(lowest_file, entry.file_name, PATH_MAX + 1);
					lowest = this;
//					ci_debug_printf(10, "*** Thread: %d / To Train now %s @ %f\n", tinfo->thread_num, entry.file_name, lowest.primary_probScaled);
				}
			}
			pthread_mutex_unlock(&train_mtx);
		}
		free(entry.full_path);
		free(entry.file_name);
	}

	return "NORMAL_SHUTDOWN";
}

int main(int argc, char *argv[])
{
int ret;
struct thread_info *tinfo;
int i;
process_entry *temp;

	checkMakeUTF8();
	initHTML();
	initHyperSpaceClassifier();
	if(readArguments(argc, argv)==-1) exit(-1);

	/* Allocate memory for pthread_create() arguments */

	tinfo = calloc(num_threads, sizeof(struct thread_info));
	if (tinfo == NULL)
		handle_error("calloc");

	loadMassHSCategories(fhs_dir);

	for(i = 0; i < FILES_PER_NEED; i++)
	{
		temp = calloc(1, sizeof(process_entry));
		temp->next = file_names;
		file_names = temp;
	}

	start_threads(tinfo, num_threads, thread_start);

	ret = judgeDirectory(judge_dir, tinfo);

	destroy_threads(tinfo);

	do {
		temp = file_names;
		file_names = file_names->next;
		free(temp);
	} while (file_names);

	free(judge_dir);
	free(fhs_dir);
	free(train_as_category);
	deinitHyperSpaceClassifier();
	deinitHTML();
	return ret;
}
