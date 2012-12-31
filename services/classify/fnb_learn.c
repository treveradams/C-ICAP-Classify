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

#ifndef NOT_AUTOCONF
#include "autoconf.h"
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
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

#include "hash.c"
#include "bayes.c"
#include "train_common.h"
#include "train_common_threads.h"

char *learn_in_file = NULL;
char *fbc_out_file = NULL;
int do_directory_learn = -1;
int learning = 0;
uint32_t zero_point = 0;
int num_threads = 2;

pthread_mutex_t file_mtx, train_mtx;
pthread_cond_t file_avl_cnd, need_file_cnd;
int file_need_data = 0;
int file_available = 0;
int shutdown = 0;

process_entry *file_names = NULL;
process_entry *busy_file_names = NULL;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 9 && argc != 5)
	{
		HELP:
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_FILE_TO_LEARN -- cannot be used with -d\n");
		printf("\t-d INPUT_DIRECTORY_TO_LEARN -- cannot be used with -i\n");
		printf("\t-o OUTPUT_FNB_FILE\n");
		printf("\t-z ZERO (REMOVE) HASH IF COUNT IS LESS THAN THIS NUMBER (OPTIONAL)\n");
		printf("\tOR\n");
		printf("\t-o OUTPUT_FNB_FILE\n");
		printf("\t-z ZERO (REMOVE) HASH IF COUNT IS LESS THAN THIS NUMBER\n");
		printf("\t-m NUMBER_OF_THREADS_TO_USE (default: 2 and should be <= NUM_CORES_ON_CPU)\n");
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
			learning = 1;
		}
		else if(strcmp(argv[i], "-d") == 0)
		{
			if(do_directory_learn == 0) goto HELP;
			learn_in_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", learn_in_file);
			do_directory_learn = 1;
			learning = 1;
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
		else if(strcmp(argv[i], "-m") == 0)
		{
			num_threads = atoi(argv[i+1]);
		}
	}
	if(fbc_out_file == NULL) goto HELP;
	if(argc == 5 && (zero_point < 1 || fbc_out_file == NULL))
	{
		goto HELP;
	}
	else if(argc !=5 && do_directory_learn < 0) goto HELP;
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", learn_in_file);
	printf("FNB Output File: %s\n", fbc_out_file);*/
	return 0;
}

void doLearn(char *data_file)
{
regexHead myRegexHead = {.head = NULL, .tail = NULL, .dirty = 0, . main_memory = NULL, .arrays = NULL, .lastarray = NULL};
wchar_t *myData;
HashList myHashes;
	myData=makeData(data_file);
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

	pthread_mutex_lock(&train_mtx);
	learnHashesBayesCategory(0, &myHashes);
	pthread_mutex_unlock(&train_mtx);

	free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
}

void learnDirectory(char *directory, struct thread_info *tinfo)
{
char full_path[PATH_MAX];
struct stat info;
int tnum;
struct timespec delay = { 0, 10000000L};

#ifndef _SVID_SOURCE
DIR *dirp;
struct dirent *dp;

	if ((dirp = opendir(directory)) == NULL)
	{
		printf("couldn't open '%s'", directory);
		return;
	}

	do {
		errno = 0;
		if ((dp = readdir(dirp)) != NULL)
		{
			snprintf(full_path, PATH_MAX, "%s/%s", directory, dp->d_name);
			stat(full_path, &info);
			if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0 && S_ISREG(info.st_mode))
			{
				printf("Learning %s\n", dp->d_name);
				fprintf(stderr, "Learning %s\n", dp->d_name);

				pthread_mutex_lock(&file_mtx);
				while(!file_need_data)
				{
					pthread_cond_wait(&need_file_cnd, &file_mtx);
				}

				addFile(full_path, dp->d_name);
				pthread_mutex_unlock(&file_mtx);
			}
		}
	} while (dp != NULL);
	if (errno != 0)
		perror("error reading directory");
	else
		(void) closedir(dirp);
#else
struct dirent **namelist;
uint32_t n;

	n = scandir(directory, &namelist, 0, alphasort);
	if (n < 0)
	{
		printf("couldn't open '%s', or it is empty", directory);
		return;
	}
	else {
		n--;
		for(uint32_t i = 0; i <= n; i++)
		{
			snprintf(full_path, PATH_MAX, "%s/%s", directory, namelist[i]->d_name);
			stat(full_path, &info);
			if (strcmp(namelist[i]->d_name, ".") != 0 && strcmp(namelist[i]->d_name, "..") != 0 && S_ISREG(info.st_mode))
			{
				printf("Learning %s (%"PRIu32" / %"PRIu32")\n", namelist[i]->d_name, i, n);
				fprintf(stderr, "Learning %s (%"PRIu32" / %"PRIu32")\n", namelist[i]->d_name, i, n);
				pthread_mutex_lock(&file_mtx);
				while(!file_need_data)
				{
					pthread_cond_wait(&need_file_cnd, &file_mtx);
				}

				addFile(full_path, namelist[i]->d_name);
				pthread_mutex_unlock(&file_mtx);
			}
			free(namelist[i]);
		}
		free(namelist);
	}
#endif

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
}

/* Thread start function: display address near top of our stack,
and return upper-cased copy of argv_string */

static void *thread_start(void *arg)
{
struct thread_info *tinfo = (struct thread_info *) arg;
process_entry entry;

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

		doLearn(entry.full_path);


		free(entry.full_path);
		free(entry.file_name);
	}

	return "NORMAL_SHUTDOWN";
}

int main (int argc, char *argv[])
{
FBC_HEADERv1 header;
int fbc_file;
struct thread_info *tinfo;
int i;
process_entry *temp;

	if(readArguments(argc, argv)==-1) exit(-1);
	checkMakeUTF8();
	initHTML();
	initBayesClassifier();

        if(loadBayesCategory(fbc_out_file, "LEARNING") == -1)
		printf("Unable to open %s\n", fbc_out_file);

	if(learning)
	{
		if(do_directory_learn == 0) doLearn(learn_in_file);
		else {
			/* Allocate memory for pthread_create() arguments */

			tinfo = calloc(num_threads, sizeof(struct thread_info));
			if (tinfo == NULL)
				handle_error("calloc");

			for(i = 0; i < num_threads; i++)
			{
				temp = calloc(1, sizeof(process_entry));
				temp->next = file_names;
				file_names = temp;
			}

			start_threads(tinfo, num_threads, thread_start);
			learnDirectory(learn_in_file, tinfo);
			destroy_threads(tinfo);

			do {
				temp = file_names;
				file_names = file_names->next;
				free(temp);
			} while (file_names);
		}
	}

	fbc_file = openFBC(fbc_out_file, &header, 1);
	if(writeFBCHashes(fbc_file, &header, &NBJudgeHashList, 0, zero_point) == -1)
		printf("MAJOR PROBLEM: Input file: %s had no hashed data!\n", learn_in_file);
	close(fbc_file);

	if(learn_in_file) free(learn_in_file);
	if(fbc_out_file) free(fbc_out_file);
	deinitBayesClassifier();
	deinitHTML();
	return 0;
}
