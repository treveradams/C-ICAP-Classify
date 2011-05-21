/*
 *  Copyright (C) 2008-2009 Trever L. Adams
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

#include "hash.c"
#include "bayes.c"
#include "sqlite3.h"

char *learn_in_file;
char *fbc_out_file;

const int SQL_SIZE=4096;

/*int open_database(const char *filename, void **handle, char *category)
{
char sql[SQL_SIZE];
int error;
	switch((error=sqlite3_open(filename, (sqlite3 **) handle)))
	{
		case SQLITE_OK:
			sqlite3_exec(*handle,
				"CREATE TABLE IF NOT EXISTS categories ( cat_key int, category text, " \
				"CONSTRAINT bayes_key PRIMARY KEY (cat_key, category) ON CONFLICT " \
				" REPLACE);", NULL, NULL, NULL);
			sqlite3_exec(*handle,
				"CREATE TABLE IF NOT EXISTS bayes_data ( "\
				"	key1 int," \
				"	cat_key int," \
				"	count int," \
				"	FOREIGN KEY(cat_key) REFERENCES categories(cat_key)" \
				"	CONSTRAINT bayes_key PRIMARY KEY" \
				"		(key1, cat_key) ON CONFLICT REPLACE" \
				"		);", NULL, NULL, NULL);
			snprintf(sql, SQL_SIZE-1, "INSERT OR REPLACE INTO categories (cat_key, category) VALUES (coalesce((select cat_key from categories where category ='%s'),(coalesce((select max(cat_key)+1 from categories), 0))), '%s');", category, category);
			sql[SQL_SIZE-1]='\0';
			sqlite3_exec((sqlite3 *) *handle, sql, NULL, NULL, NULL);
			sqlite3_exec(*handle, "PRAGMA journal_mode=TRUNCATE;", NULL, NULL, NULL);
			sqlite3_exec(*handle, "PRAGMA cache_size=10000;", NULL, NULL, NULL);
			sqlite3_exec(*handle, "PRAGMA auto_vacuum=NONE;", NULL, NULL, NULL);
			sqlite3_exec(*handle, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
			sqlite3_exec(*handle, "PRAGMA count_changes=OFF;", NULL, NULL, NULL);
			sqlite3_exec(*handle, "PRAGMA temp_store=MEMORY;", NULL, NULL, NULL);
			printf("Database opened ok\n");
			return SQLITE_OK;
			break;
		default:
			printf("Database NOT opened ok\n");
			return error;
			break;
	}
}

int close_database(void *handle)
{
	return sqlite3_close((sqlite3 *) handle);
}

int update_count(void *handle, char *category, uint64_t hash1)
{
char sql[SQL_SIZE];
	snprintf(sql, SQL_SIZE-1, "INSERT OR REPLACE INTO bayes_data (key1, cat_key, count) VALUES (%"PRIu64", (select cat_key from categories where category='%s'), coalesce((select count+1 from bayes_data where key1=%"PRIu64" and cat_key=(select cat_key from categories where category='%s')), 1));", hash1, category, hash1, category);
	sql[SQL_SIZE-1]='\0';
	return sqlite3_exec((sqlite3 *) handle, sql, NULL, NULL, NULL);
} */


int readArguments(int argc, char *argv[])
{
int i;
	if(argc < 8)
	{
		printf("Format of arguments is:\n");
		printf("\t-p PRIMARY_HASH_SEED\n");
		printf("\t-s SECONDARY_HASH_SEED\n");
		printf("\t-i INPUT_FILE_TO_LEARN\n");
		printf("\t-o OUTPUT_FBC_FILE\n");
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
			fbc_out_file = malloc(strlen(argv[i+1]) + 1);
			sscanf(argv[i+1], "%s", fbc_out_file);
		}
	}
/*	printf("Primary Seed: %"PRIX32"\n", HASHSEED1);
	printf("Secondary Seed: %"PRIX32"\n", HASHSEED2);
	printf("Learn File: %s\n", learn_in_file);
	printf("FHS Output File: %s\n", fbs_database_file);*/
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

/*int writeSQLFBSHashes(void *handle, char *category, HashList *hashes_list)
{
uint16_t i;
	if(hashes_list->used) // check before we write
	{
		sqlite3_exec(handle, "BEGIN;", NULL, NULL, NULL);
		for(i=0; i < hashes_list->used; i++)
		{
			update_count(handle, category, hashes_list->hashes[i]);
		}
		sqlite3_exec(handle, "COMMIT;", NULL, NULL, NULL);
		return 0;
	}
	return -1;
}*/


int main (int argc, char *argv[])
{
regexHead myRegexHead;
wchar_t *myData;
HashList myHashes;
FBC_HEADERv1 header;
int fbc_file;

	if(readArguments(argc, argv)==-1) exit(-1);
	checkMakeUTF8();
	initHTML();
	initBayesClassifier();
	myData=makeData(learn_in_file);
	mkRegexHead(&myRegexHead, myData);
	removeHTML(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);
	normalizeCurrency(&myRegexHead);
	regexMakeSingleBlock(&myRegexHead);

//	printf("%ls\n", myRegexHead.main_memory);

	myHashes.hashes = malloc(sizeof(FBCFeature) * FBC_MAX_FEATURE_COUNT);
	myHashes.slots = FBC_MAX_FEATURE_COUNT;
	myHashes.used = 0;
	if(NBCategories.used == NBCategories.slots)
	{
		NBCategories.slots += BAYES_CATEGORY_INC;
		NBCategories.categories = realloc(NBCategories.categories, NBCategories.slots * sizeof(FBCTextCategory));
	}
	NBCategories.used++;
	computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);
	fbc_file = openFBC(fbc_out_file, &header, 1);
	learnHashesBayesCategory(0, &myHashes);
	if(writeFBCHashes(fbc_file, &header, &NBJudgeHashList, 0, 0) == -1)
		printf("MAJOR PROBLEM: Input file: %s had no hashed data!\n", learn_in_file);

	free(myHashes.hashes);
	freeRegexHead(&myRegexHead);
	free(learn_in_file);
	free(fbc_out_file);
	deinitHTML();
	return 0;
}
