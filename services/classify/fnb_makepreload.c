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
#include "bayes.c"

char *fbc_out_file;
char *fbc_dir;

/* Verified that -p and -s are not needed, but they have been left in as I may be using them in the future */
int readArguments(int argc, char *argv[])
{
    int i;
    if (argc < 9) {
        printf("Format of arguments is:\n");
        printf("\t-p PRIMARY_HASH_SEED\n");
        printf("\t-s SECONDARY_HASH_SEED\n");
        printf("\t-d FNB_DIRECTORY\n");
        printf("\t-o OUTPUT_FNB_PRELOAD_FILE, must be a file name only, will be saved to FNB DIRECTORY\n");
        printf("Spaces and case matter.\n");
        return -1;
    }
    for (i=1; i<9; i+=2) {
        if (strcmp(argv[i], "-p") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED1);
        else if (strcmp(argv[i], "-s") == 0) sscanf(argv[i+1], "%"PRIx32, &HASHSEED2);
        else if (strcmp(argv[i], "-o") == 0) {
            fbc_out_file = malloc(strlen(argv[i+1]) + 1);
            sscanf(argv[i+1], "%s", fbc_out_file);
        } else if (strcmp(argv[i], "-d") == 0) {
            fbc_dir = malloc(strlen(argv[i+1]) + 1);
            sscanf(argv[i+1], "%s", fbc_dir);
        }
    }
    // Use old preload, if it exists, to speed up things -- at this time the following two lines are commented as they don't seem to make a difference
    preLoadBayes(fbc_out_file);
    /*  printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
        printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
        printf("Learn File: %s\n", judge_file);*/
    return 0;
}

void loadMassCategories(void)
{
    struct dirent *current_file = NULL;
    DIR *directory;
    if (chdir(fbc_dir) == -1) {
        ci_debug_printf(1, "Unable to change directory in loadMassCategories because %s. Dying.", strerror(errno));
        exit(-1);
    }

    directory = opendir(".");
    if (directory == NULL) {
        printf("Unable to open FNB Directory provided!\n");
        exit(-1);
    }
    do {
        current_file = readdir(directory);
        if (current_file != NULL && strcmp(current_file->d_name, ".") != 0 && strcmp(current_file->d_name, "..") != 0 && strcmp(current_file->d_name, fbc_out_file) != 0 && strcmp(&current_file->d_name[strlen(current_file->d_name)-4], ".fnb") == 0) {
            printf("Loading FNB file: %s\n", current_file->d_name);
            loadBayesCategory(current_file->d_name, current_file->d_name);
        }
    } while (current_file != NULL);
    closedir(directory);
}

int main(int argc, char *argv[])
{
    int fbc_file;
    FBC_HEADERv1 header;
    clock_t start, end;
    uint32_t realHashesUsed = 0;
    initHTML();
    initBayesClassifier();
    if (readArguments(argc, argv) == -1) exit(-1);

    printf("Loading hashes -- be patient!\n");
    start = clock();
    loadMassCategories();
    printf("\nWriting out preload file: %s\n", fbc_out_file);

    fbc_file = openFBC(fbc_out_file, &header, 1);

    realHashesUsed = NBJudgeHashList.used;

    writeFBCHashesPreload(fbc_file, &header, &NBJudgeHashList);

    close(fbc_file);

    end = clock();
    printf("Wrote out: %"PRIu32" hashes.\n", realHashesUsed);
    printf("Preload making took %lf seconds\n", (double)((end-start)/(CLOCKS_PER_SEC)));

    deinitBayesClassifier();
    deinitHTML();
    return 0;
}
