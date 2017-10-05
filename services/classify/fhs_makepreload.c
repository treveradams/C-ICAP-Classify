/*
 *  Copyright (C) 2008-2017 Trever L. Adams
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

#ifndef NOT_CICAP
#define NOT_CICAP
#endif

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
	if (chdir(fhs_dir) == -1) {
		ci_debug_printf(1, "Unable to change directory in loadMassCategories because %s. Dying.", strerror(errno));
		exit(-1);
	}

	directory = opendir(".");
	if(directory == NULL)
	{
		printf("Unable to open FHS Directory provided!\n");
		exit(-1);
	}
	do {
		current_file = readdir(directory);
		if (current_file != NULL && strcmp(current_file->d_name, ".") != 0 && strcmp(current_file->d_name, "..") != 0 && strcmp(current_file->d_name, fhs_out_file) != 0 && strcmp(&current_file->d_name[strlen(current_file->d_name)-4], ".fhs") == 0)
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
uint_least16_t docsWritten = 0;
	initHTML();
	initHyperSpaceClassifier();
	if(readArguments(argc, argv) == -1) exit(-1);

	printf("Loading hashes -- be patient!\n");
	start = clock();
	loadMassCategories();
	printf("\nWriting out preload file: %s\n", fhs_out_file);

	fhs_file = openFHS(fhs_out_file, &header, 1);

	docsWritten = writeFHSHashesPreload(fhs_file, &header, &HSJudgeHashList);

	close(fhs_file);

	end = clock();
	printf("Wrote out: %"PRIu32" hashes as %"PRIu16" documents.\n", HSJudgeHashList.used, docsWritten);
	printf("Preload making took %lf seconds\n", (double)((end-start)/(CLOCKS_PER_SEC)));

	deinitHyperSpaceClassifier();
	deinitHTML();
	return 0;
}
