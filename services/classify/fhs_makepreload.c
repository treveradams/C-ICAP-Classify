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
#include <sys/types.h>
#include <dirent.h>

#include "hash.c"
#include "hyperspace.c"

char *fhs_out_file;
char *fhs_dir;

int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 9)
	{
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-d FHS DIRECTORY TO READ\n");
		printf("\t-o OUTPUT_FHS_PRELOAD_FILE, must be a file name only, will be saved to FHS DIRECTORY\n");
		printf("Spaces and case matter.\n");
		return -1;
	}
	for(i=1; i<9; i+=2)
	{
		if(strcmp(argv[i], "-p") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED1);
		else if(strcmp(argv[i], "-s") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED2);
		else if(strcmp(argv[i], "-o") == 0)
		{
			fhs_out_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", fhs_out_file);
		}
		else if(strcmp(argv[i], "-d") == 0)
		{
			fhs_dir = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", fhs_dir);
		}
	}
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", judge_file);*/
	return 0;
}

void loadMassCategories(void)
{
struct dirent *current_file = NULL;
DIR *directory;
	chdir(fhs_dir);

	directory=opendir(".");
	if(directory==NULL)
	{
		printf("Unable to open FHS Directory provided!\n");
		exit(-1);
	}
	do {
		current_file = readdir(directory);
		if (current_file!=NULL && strcmp(current_file->d_name, ".") != 0 && strcmp(current_file->d_name, "..") != 0 && strcmp(current_file->d_name, fhs_out_file) != 0 && strcmp(&current_file->d_name[strlen(current_file->d_name)-4], ".fhs") == 0)
		{
			printf("Loading FHS file: %s\n", current_file->d_name);
			loadHyperSpaceCategory(current_file->d_name, current_file->d_name);
		}
	} while (current_file != NULL);
	closedir(directory);
}

int main(int argc, char *argv[])
{
int fhs_file;
FHS_HEADERv1 header;
clock_t start, end;
uint32_t realHashesUsed = 0, i = 0;
uint_least16_t docsWritten = 0;
hyperspaceFeatureExt *realHashes;
	initHTML();
	initHyperSpaceClassifier();
	if(readArguments(argc, argv) == -1) exit(-1);

	printf("Loading hashes -- be patient!\n");
	start=clock();
	loadMassCategories();
	printf("\nWriting out preload file: %s\n", fhs_out_file);

	fhs_file = openFHS(fhs_out_file, &header, 1);

	realHashesUsed = HSJudgeHashList.used;
	realHashes = HSJudgeHashList.hashes;
	do {
		// Make sure we only write out FHS_v1_QTY_MAX records per document
		// Do this by screwing with HSJudgeHashList.used, save old, modify
		if(HSJudgeHashList.used > FHS_v1_QTY_MAX)
		{
			HSJudgeHashList.used = FHS_v1_QTY_MAX - 1;
		}
		// Also, change HSJudgeHashList.hashes to offset for previously used ones
		HSJudgeHashList.hashes = &HSJudgeHashList.hashes[i];
		writeFHSHashesPreload(fhs_file, &header, &HSJudgeHashList);
		docsWritten++;

		// Restore HSJudgeHashList.hashes
		HSJudgeHashList.hashes = realHashes;

		// Make sure that we never write out more than FHS_HEADERv1_RECORDS_QTY_MAX times as this is our maximum document count
		if(docsWritten > FHS_HEADERv1_RECORDS_QTY_MAX)
		{
			printf("PROBLEM: We have more hashes than allowed!!\n");
		}

		// Restore HSJudgeHashList.used
		i += HSJudgeHashList.used;
		HSJudgeHashList.used = realHashesUsed - i;
	} while (i < realHashesUsed);
	close(fhs_file);


	end=clock();
	printf("Wrote out: %"PRIu32" hashes as %"PRIu16" documents.\n", realHashesUsed, docsWritten);
	printf("Preload making took %lf seconds\n", (double)((end-start)/(CLOCKS_PER_SEC)));

	deinitHyperSpaceClassifier();
	deinitHTML();
	return 0;
}
