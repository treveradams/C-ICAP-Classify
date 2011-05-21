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


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
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
#include <langinfo.h>
#include <wchar.h>
#include <wctype.h>
#include <float.h>
#include <math.h>

#include "html.h"

#define IN_BAYES 1
#include "bayes.h"


FBCTextCategoryExt NBCategories;
FBCHashList NBJudgeHashList = { .FBC_LOCKED = 0 };

void initBayesClassifier(void)
{
	NBJudgeHashList.slots = 0;
	NBJudgeHashList.hashes = NULL;
	NBJudgeHashList.used = 0;
	NBCategories.slots = BAYES_CATEGORY_INC;
	NBCategories.categories = calloc(NBCategories.slots, sizeof(FBCTextCategory));
	NBCategories.used = 0;
}

void deinitBayesClassifier(void)
{
uint32_t i=0;
	for(i=0; i < NBCategories.used; i++)
	{
		free(NBCategories.categories[i].name);
	}
	free(NBCategories.categories);

	for(i=0; i < NBJudgeHashList.used; i++)
	{
		free(NBJudgeHashList.hashes[i].users);
	}
	free(NBJudgeHashList.hashes);
}

int optimizeFBC(FBCHashList *hashes)
{
uint64_t total;
double den;

	if(hashes->FBC_LOCKED) return -1;

	// Do precomputing
	for(uint32_t i=0; i < hashes->used; i++)
	{
		total = 0;
		den = 0;
		for(uint_least16_t j=0; j < hashes->hashes[i].used; j++)
		{
			total += hashes->hashes[i].users[j].data.count;
		}
		den = MARKOV_C1 * (total + MARKOV_C2) * 256;
		for(uint_least16_t j=0; j < hashes->hashes[i].used; j++)
		{
			hashes->hashes[i].users[j].data.probability = 0.5 + hashes->hashes[i].users[j].data.count / den;
		}
	}
	hashes->FBC_LOCKED = 1;
        return 0;
}

static int FBCjudgeHash_compare(void const *a, void const *b)
{
FBCFeatureExt *ha, *hb;

	ha = (FBCFeatureExt *) a;
	hb = (FBCFeatureExt *) b;
	if(ha->hash < hb->hash)
		return -1;
	if(ha->hash > hb->hash)
		return 1;

return 0;
}

static int verifyFBC(int fbc_file, FBC_HEADERv1 *header)
{
	lseek64(fbc_file, 0, SEEK_SET);
	read(fbc_file, &header->ID, 3);
	if(memcmp(header->ID, "FNB", FBC_HEADERv1_ID_SIZE) != 0)
	{
//		printf("Not a FastBayesClassifer file\n");
		return -1;
	}
	read(fbc_file, &header->version, FBC_HEADERv1_VERSION_SIZE);
	if(header->version != FBC_FORMAT_VERSION)
	{
//		printf("Wrong version of FastBayesClassifier file\n");
		return -2;
	}
	read(fbc_file, &header->UBM, FBC_HEADERv1_UBM_SIZE);
	if(header->UBM != UNICODE_BYTE_MARK)
	if(header->version != FBC_FORMAT_VERSION)
	{
//		printf("FastBayesClassifier file of incompatible endianness\n");
		return -3;
	}
	if(read(fbc_file, &header->records, FBC_HEADERv1_RECORDS_QTY_SIZE) != 4)
	{
//		printf("FastBayesClassifier file has invalid header: no records count\n");
		return -4;
	}
	return 0;
}

void writeFBCHeader(int file, FBC_HEADERv1 *header)
{
int i;
	memcpy(&header->ID, "FNB", 3);
	header->version = FBC_FORMAT_VERSION;
	header->UBM = UNICODE_BYTE_MARK;
	header->records = 0;
	i = ftruncate(file, 0);
	lseek64(file, 0, SEEK_SET);
        do {
		i = write(file, "FNB", FBC_HEADERv1_ID_SIZE);
		if(i < FBC_HEADERv1_ID_SIZE) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < FBC_HEADERv1_ID_SIZE);

        do {
		i = write(file, &header->version, FBC_HEADERv1_VERSION_SIZE);
		if(i < FBC_HEADERv1_VERSION_SIZE) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < FBC_HEADERv1_VERSION_SIZE);

        do {
		i = write(file, &header->UBM, FBC_HEADERv1_UBM_SIZE);
		if(i < FBC_HEADERv1_UBM_SIZE) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < FBC_HEADERv1_UBM_SIZE);

        do {
		i = write(file, &header->records, FBC_HEADERv1_RECORDS_QTY_SIZE);
		if(i < FBC_HEADERv1_RECORDS_QTY_SIZE) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < FBC_HEADERv1_RECORDS_QTY_SIZE);
}

int openFBC(char *filename, FBC_HEADERv1 *header, int forWriting)
{
int file=0;
	file=open(filename, (forWriting ? (O_CREAT | O_RDWR) : O_RDONLY), S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP);
	if(verifyFBC(file, header) < 0)
	{
		if(forWriting == 1)
		{
			writeFBCHeader(file, header);
//			printf("Created FastBayesClassifier file: %s\n", filename);
		}
		else return -1;
	}
	return file;
}

int writeFBCHashes(int file, FBC_HEADERv1 *header, FBCHashList *hashes_list, uint16_t category, int zero_point)
{
uint32_t i;
uint_least32_t qty = 0;
uint16_t j;
int writecheck;

        if(hashes_list->FBC_LOCKED) return -1; // We cannot write when FBC_LOCKED is set, as we are in optimized and not raw count mode
//	lseek64(file, 11, SEEK_SET);
	if(hashes_list->used) // check before we write
	{
		for(i = 0; i < hashes_list->used; i++)
		{
			for(j = 0; j < hashes_list->hashes[i].used; j++)
			{
				// Make sure that this is the right category and that we have enough counts to write (not <= zero_point)
				if(hashes_list->hashes[i].users[j].category == category && hashes_list->hashes[i].users[j].data.count > zero_point)
				{
					qty++;
					do { // write hash
						writecheck = write(file, &hashes_list->hashes[i].hash, FBC_v1_HASH_SIZE);
				                if(writecheck < FBC_v1_HASH_SIZE) lseek64(file, -writecheck, SEEK_CUR);
					} while (writecheck >=0 && writecheck < FBC_v1_HASH_SIZE);
					do { // write use count
						writecheck = write(file, &hashes_list->hashes[i].users[j].data.count, FBC_v1_HASH_USE_COUNT_SIZE);
				                if(writecheck < FBC_v1_HASH_USE_COUNT_SIZE) lseek64(file, -writecheck, SEEK_CUR);
					} while (writecheck >=0 && writecheck < FBC_v1_HASH_USE_COUNT_SIZE);
				}
			}
		}
		/* Ok, have written hashes, now save new count */
//		printf("%"PRIu32" hashes, wrote %"PRIu32" hashes\n", hashes_list->used, qty);
		header->records = qty;
		lseek64(file, 7, SEEK_SET);
		do {
			writecheck = write(file, &header->records, FBC_HEADERv1_RECORDS_QTY_SIZE);
			if(writecheck < FBC_HEADERv1_RECORDS_QTY_SIZE) lseek64(file, -writecheck, SEEK_CUR);
		} while (writecheck >=0 && writecheck < FBC_HEADERv1_RECORDS_QTY_SIZE);
		return 0;
	}
	return -1;
}

int writeFBCHashesPreload(int file, FBC_HEADERv1 *header, FBCHashList *hashes_list)
{
uint32_t i;
const uint_least32_t ZERO_COUNT = 0;
int writecheck;
        if(hashes_list->FBC_LOCKED) return -1; // We cannot write when FBC_LOCKED is set, as we are in optimized and not raw count mode
	lseek64(file, 11, SEEK_SET);
	if(hashes_list->used) // check before we write
	{
		for(i=0; i < hashes_list->used; i++)
		{
			do { // write hash
				writecheck = write(file, &hashes_list->hashes[i].hash, FBC_v1_HASH_SIZE);
		                if(writecheck < FBC_v1_HASH_SIZE) lseek64(file, -writecheck, SEEK_CUR);
			} while (writecheck >=0 && writecheck < FBC_v1_HASH_SIZE);
			do { // write use count
				writecheck = write(file, &ZERO_COUNT, FBC_v1_HASH_USE_COUNT_SIZE);
		                if(writecheck < FBC_v1_HASH_USE_COUNT_SIZE) lseek64(file, -writecheck, SEEK_CUR);
			} while (writecheck >=0 && writecheck < FBC_v1_HASH_USE_COUNT_SIZE);
		}
		/* Ok, have written hashes, now save new count */
		header->records = hashes_list->used;
		lseek64(file, 7, SEEK_SET);
		do {
			writecheck = write(file, &header->records, FBC_HEADERv1_RECORDS_QTY_SIZE);
			if(writecheck < FBC_HEADERv1_RECORDS_QTY_SIZE) lseek64(file, -writecheck, SEEK_CUR);
		} while (writecheck >=0 && writecheck < FBC_HEADERv1_RECORDS_QTY_SIZE);
		return 0;
	}
	return -1;
}

static int64_t FBCBinarySearch(FBCHashList *hashes_list, int64_t start, int64_t end, uint64_t key)
{
int64_t mid=0;
	if(start > end)
		return -1;
	mid = start + ((end - start) / 2);
//	printf("Start %"PRId64" end %"PRId64" mid %"PRId64"\n", start, end, mid);
//	printf("Keys @ mid: %"PRIX64" looking for %"PRIX64"\n", hashes_list->hashes[mid].hash, key);
	if(hashes_list->hashes[mid].hash > key)
		return FBCBinarySearch(hashes_list, start, mid-1, key);
	else if(hashes_list->hashes[mid].hash < key)
		return FBCBinarySearch(hashes_list, mid+1, end, key);
	else return mid;

	return -1; // This should never be reached
}

uint32_t featuresInCategory(int fbc_file, FBC_HEADERv1 *header)
{
struct stat stat_buf;
	fstat(fbc_file, &stat_buf);
        return (stat_buf.st_size - (FBC_HEADERv1_ID_SIZE + FBC_HEADERv1_VERSION_SIZE + FBC_HEADERv1_UBM_SIZE +
		FBC_HEADERv1_RECORDS_QTY_SIZE)) / (FBC_v1_HASH_SIZE) + 1;
}

// The following number is based on a lot of experimentation. It should be between 35-70. The larger the data set, the higher the number should be.
// 95 is ideal for my training set.
#define NBC_OFFSET_MAX 95
int loadBayesCategory(char *fbc_name, char *cat_name)
{
int fbc_file;
uint32_t i, z, shortcut=0, offsetPos=2;
int64_t BSRet=-1;
int64_t offsets[NBC_OFFSET_MAX+1];
FBC_HEADERv1 header;
uint32_t startHashes = NBJudgeHashList.used;
HTMLFeature hash;
uint_least32_t count;
int status;

        if(NBJudgeHashList.FBC_LOCKED) return -1; // We cannot load if we are optimized
	offsets[0] = 0;
	if((fbc_file = openFBC(fbc_name, &header, 0)) < 0) return fbc_file;

	if(NBCategories.used == NBCategories.slots)
	{
		NBCategories.slots += BAYES_CATEGORY_INC;
		NBCategories.categories = realloc(NBCategories.categories, NBCategories.slots * sizeof(FBCTextCategory));
	}
	NBCategories.categories[NBCategories.used].name = strndup(cat_name, MAX_BAYES_CATEGORY_NAME);
	NBCategories.categories[NBCategories.used].totalFeatures = header.records;

	if(NBJudgeHashList.used + featuresInCategory(fbc_file, &header) >= NBJudgeHashList.slots)
	{
		NBJudgeHashList.slots += featuresInCategory(fbc_file, &header);
		NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
	}

//	printf("Going to read %"PRIu32" records from %s\n", header.records, cat_name);
	offsets[1] = NBJudgeHashList.used;

	for(i = 0; i < header.records; i++)
	{
		do { // read hash
			status = read(fbc_file, &hash, FBC_v1_HASH_SIZE);
			if(status < FBC_v1_HASH_SIZE) lseek64(fbc_file, -status, SEEK_CUR);
		} while (status >=0 && status < FBC_v1_HASH_SIZE);
		do { // read use count
			status = read(fbc_file, &count, FBC_v1_HASH_USE_COUNT_SIZE);
			if(status < FBC_v1_HASH_USE_COUNT_SIZE) lseek64(fbc_file, -status, SEEK_CUR);
		} while (status >=0 && status < FBC_v1_HASH_USE_COUNT_SIZE);

//		printf("Loading key: %"PRIX64" in Category: %s\n", hash, cat_name);
		if(i > 0 || offsets[0] != offsets[1])
		{
			shortcut = 0;
			for(z = 1; z < offsetPos; z++)
			{
				if((BSRet=FBCBinarySearch(&NBJudgeHashList, offsets[z-1], offsets[z] - 1, hash)) >= 0)
				{
					if (shortcut == 0)
					{
						NBJudgeHashList.hashes[BSRet].users = realloc(NBJudgeHashList.hashes[BSRet].users, (NBJudgeHashList.hashes[BSRet].used+1) * sizeof(FBCHashJudgeUsers));
//						ci_debug_printf(10, "Found keys: %"PRIX64" already in table (offset %"PRIu16"), updating\n", hash, z);
						NBJudgeHashList.hashes[BSRet].users[NBJudgeHashList.hashes[BSRet].used].category = NBCategories.used;
						NBJudgeHashList.hashes[BSRet].users[NBJudgeHashList.hashes[BSRet].used].data.count = count;
						NBJudgeHashList.hashes[BSRet].used++;
						break;
					}
//						else ci_debug_printf(10, "PROBLEM IN THE CITY\n");
					shortcut++;
				}
			}
			if(!shortcut) goto STORE_NEW;
		}
		else {
			STORE_NEW:
//			ci_debug_printf(10, "Didn't find keys: %"PRIX64" in table\n", hashes[i].feature);
			NBJudgeHashList.hashes[NBJudgeHashList.used].hash = hash;
			NBJudgeHashList.hashes[NBJudgeHashList.used].used = 0;
			NBJudgeHashList.hashes[NBJudgeHashList.used].users = calloc(1, sizeof(FBCHashJudgeUsers));
			NBJudgeHashList.hashes[NBJudgeHashList.used].users[NBJudgeHashList.hashes[NBJudgeHashList.used].used].category = NBCategories.used;
			NBJudgeHashList.hashes[NBJudgeHashList.used].users[NBJudgeHashList.hashes[NBJudgeHashList.used].used].data.count = count;
			NBJudgeHashList.hashes[NBJudgeHashList.used].used++;
			NBJudgeHashList.used++;
		}

		if(offsetPos > NBC_OFFSET_MAX)
		{
			qsort(NBJudgeHashList.hashes, NBJudgeHashList.used, sizeof(FBCFeatureExt), &FBCjudgeHash_compare);
			offsetPos=2;
		}
		offsets[offsetPos] = NBJudgeHashList.used;
		if(offsets[offsetPos-1] != offsets[offsetPos]) offsetPos++;
	}

	if(startHashes != NBJudgeHashList.used) qsort(NBJudgeHashList.hashes, NBJudgeHashList.used, sizeof(FBCFeatureExt), &FBCjudgeHash_compare);
//	ci_debug_printf(10, "Categories: %"PRIu32" Hashes Used: %"PRIu32"\n", NBCategories.used, NBJudgeHashList.used);
	NBCategories.used++;

	// Fixup memory usage
	if(NBJudgeHashList.slots > NBJudgeHashList.used && NBJudgeHashList.used > 1)
	{
		NBJudgeHashList.slots = NBJudgeHashList.used;
		NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
	}

	close(fbc_file);
	return 1;
}

int learnHashesBayesCategory(uint16_t cat_num, HashList *docHashes)
{
uint32_t i, z, shortcut = 0, offsetPos = 2, handled = 0;
uint16_t used;
int64_t BSRet = -1;
int64_t offsets[NBC_OFFSET_MAX+1];
FBC_HEADERv1 header;
uint32_t startHashes = NBJudgeHashList.used;

        if(NBJudgeHashList.FBC_LOCKED) return -1; // We cannot load if we are optimized
	offsets[0] = 0;
	if(NBCategories.used == NBCategories.slots)
	{
		NBCategories.slots += BAYES_CATEGORY_INC;
		NBCategories.categories = realloc(NBCategories.categories, NBCategories.slots * sizeof(FBCTextCategory));
	}
	NBCategories.categories[NBCategories.used].totalFeatures = header.records;

	if(NBJudgeHashList.used + docHashes->used >= NBJudgeHashList.slots)
	{
		NBJudgeHashList.slots += docHashes->used;
		NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
	}

//	printf("Going to learn %"PRIu32" hashes from input file\n", docHashes->used);
	offsets[1] = NBJudgeHashList.used;
	for(i = 0; i < docHashes->used; i++)
	{
//		printf("Learning key: %"PRIX64" from input\n", docHashes->hashes[i]);
		if(i > 0 || offsets[0] != offsets[1])
		{
			shortcut = 0;
			handled = 0;
			for(z = 1; z < offsetPos; z++)
			{
				if((BSRet=FBCBinarySearch(&NBJudgeHashList, offsets[z-1], offsets[z] - 1, docHashes->hashes[i])) >= 0)
				{
					if (shortcut == 0)
					{
						for(used = 0; used < NBJudgeHashList.hashes[BSRet].used; used++)
						{
							if(NBJudgeHashList.hashes[BSRet].users[used].category == cat_num)
							{
								NBJudgeHashList.hashes[BSRet].users[used].data.count++;
//								ci_debug_printf(10, "Found keys: %"PRIX64" already in table (offset %"PRIu32") with category %"PRIu16", incrementing count to %"PRIu32"\n", docHashes->hashes[i], z, cat_num, NBJudgeHashList.hashes[BSRet].users[used].data.count);
								handled++;
								break;
							}
						}
						if(handled == 0)
						{
//							ci_debug_printf(10, "Found keys: %"PRIX64" already in table (offset %"PRIu32") but not our category (%"PRIu16" != %"PRIu16"), updating\n", docHashes->hashes[i], z,NBJudgeHashList.hashes[BSRet].users[NBJudgeHashList.hashes[BSRet].used].category, cat_num);
							NBJudgeHashList.hashes[BSRet].users = realloc(NBJudgeHashList.hashes[BSRet].users, (NBJudgeHashList.hashes[BSRet].used+1) * sizeof(FBCHashJudgeUsers));
							NBJudgeHashList.hashes[BSRet].users[NBJudgeHashList.hashes[BSRet].used].category = cat_num;
							NBJudgeHashList.hashes[BSRet].users[NBJudgeHashList.hashes[BSRet].used].data.count = 1;
							NBJudgeHashList.hashes[BSRet].used++;
						}
					}
//					else ci_debug_printf(10, "PROBLEM IN THE CITY\n");
					shortcut++;
				}
			}
			if(!shortcut) goto STORE_NEW;
		}
		else {
			STORE_NEW:
//			ci_debug_printf(10, "Didn't find keys: %"PRIX64" in table\n", docHashes->hashes[i]);
			NBJudgeHashList.hashes[NBJudgeHashList.used].hash = docHashes->hashes[i];
			NBJudgeHashList.hashes[NBJudgeHashList.used].used = 0;
			NBJudgeHashList.hashes[NBJudgeHashList.used].users = calloc(1, sizeof(FBCHashJudgeUsers));
			NBJudgeHashList.hashes[NBJudgeHashList.used].users[NBJudgeHashList.hashes[NBJudgeHashList.used].used].category = cat_num;
			NBJudgeHashList.hashes[NBJudgeHashList.used].users[NBJudgeHashList.hashes[NBJudgeHashList.used].used].data.count = 1;
			NBJudgeHashList.hashes[NBJudgeHashList.used].used++;
			NBJudgeHashList.used++;
		}

		if(offsetPos > NBC_OFFSET_MAX)
		{
			qsort(NBJudgeHashList.hashes, NBJudgeHashList.used, sizeof(FBCFeatureExt), &FBCjudgeHash_compare);
			offsetPos = 2;
		}
		offsets[offsetPos] = NBJudgeHashList.used;
		if(offsets[offsetPos - 1] != offsets[offsetPos]) offsetPos++;
	}

	if(startHashes != NBJudgeHashList.used) qsort(NBJudgeHashList.hashes, NBJudgeHashList.used, sizeof(FBCFeatureExt), &FBCjudgeHash_compare);
	ci_debug_printf(10, "Categories: %"PRIu32" Hashes Used: %"PRIu32"\n", NBCategories.used, NBJudgeHashList.used);

	// Fixup memory usage
	if(NBJudgeHashList.slots > NBJudgeHashList.used && NBJudgeHashList.used > 1)
	{
		NBJudgeHashList.slots = NBJudgeHashList.used;
		NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
	}

	return 1;
}

static int preload_hash_compare(const uint_least32_t a, const uint_least32_t b)
{
	if(a < b)
		return -1;
	if(a > b)
		return 1;

return 0;
}

// This function just used to be a slimmed down version of loadBayesCategory.
// It was allowed to be called even after some categories were loaded. This is
// no longer the case. It must be called first. This was done to make it run faster,
// and be simpler / more obviously correct.
int preLoadBayes(char *fbc_name)
{
int fbc_file;
uint32_t i, j;
FBC_HEADERv1 header;
HTMLFeature hash;
uint_least32_t count;
int status;

	if(NBJudgeHashList.used > 0)
	{
		ci_debug_printf(1, "TextPreload / preLoadBayes called with some hashes already loaded. ABORTING PRELOAD!\n");
		return -1;
	}
	if((fbc_file = openFBC(fbc_name, &header, 0)) < 0) return fbc_file;

	if(featuresInCategory(fbc_file, &header) >= NBJudgeHashList.slots)
	{
		NBJudgeHashList.slots += featuresInCategory(fbc_file, &header);
		NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
	}

//	printf("Going to read %"PRIu32" records from %s\n", header.records, cat_name);
	for(i = 0; i < header.records; i++)
	{
		do { // read hash
			status = read(fbc_file, &hash, FBC_v1_HASH_SIZE);
                        if(status < FBC_v1_HASH_SIZE) lseek64(fbc_file, -status, SEEK_CUR);
		} while (status >=0 && status < FBC_v1_HASH_SIZE);
		do { // read use count
			status = read(fbc_file, &count, FBC_v1_HASH_USE_COUNT_SIZE);
                        if(status < FBC_v1_HASH_USE_COUNT_SIZE) lseek64(fbc_file, -status, SEEK_CUR);
		} while (status >=0 && status < FBC_v1_HASH_USE_COUNT_SIZE);

		if(NBJudgeHashList.used + header.records > NBJudgeHashList.slots)
		{
			if(NBJudgeHashList.slots != 0) ci_debug_printf(10, "Ooops, we shouldn't be allocating more memory here. (%s)\n", fbc_name);
			NBJudgeHashList.slots += header.records;
			NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
		}

		for(j = 0; j < header.records; j++)
		{
//			ci_debug_printf(10, "Loading keys: %"PRIX64" in Category: %s Document:%"PRIu16"\n", docHashes->hashes[j], cat_name, i);
			if(NBJudgeHashList.used == 0) goto ADD_HASH;
			switch(preload_hash_compare(NBJudgeHashList.hashes[NBJudgeHashList.used-1].hash, hash))
			{
				case -1:
					ADD_HASH:
					NBJudgeHashList.hashes[NBJudgeHashList.used].hash = hash;
					NBJudgeHashList.hashes[NBJudgeHashList.used].used = 0;
					NBJudgeHashList.hashes[NBJudgeHashList.used].users = NULL;
					NBJudgeHashList.used++;
					break;
				case 0:
					ci_debug_printf(1, "Key: %"PRIX64" already loaded. Preload file %s corrupted?!?!\n", hash, fbc_name);
					break;
				case 1:
					ci_debug_printf(1, "Key: %"PRIX64" out of order. Preload file %s is corrupted!!!\n" \
								"Aborting preload as is.\n", hash, fbc_name);
					return -1;
					break;
			}
		}
	}
	// Fixup memory usage
	if(NBJudgeHashList.slots > NBJudgeHashList.used && NBJudgeHashList.used > 1)
	{
		NBJudgeHashList.slots = NBJudgeHashList.used;
		NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
	}

	close(fbc_file);
	return 0;
}

static FBCClassification doBayesClassify(FBCJudge *categories, HashList *unknown)
{
double total_probability = 0.0;
double remainder = 1000 * DBL_MIN;

uint32_t cls;

uint32_t bestseen = 0;
FBCClassification myReply;

	// Compute base probability
	for (cls = 0; cls < NBCategories.used; cls++)
	{
		categories[cls].naiveBayesResult = categories[cls].naiveBayesNum / ( categories[cls].naiveBayesNum + categories[cls].naiveBayesDen );
	}


	// Renormalize radiance to probability
	for (cls = 0; cls < NBCategories.used; cls++)
	{
		// Avoid divide by zero and keep value small, for log10(remainder) below
		if (categories[cls].naiveBayesResult < DBL_MIN * 100)
			categories[cls].naiveBayesResult = DBL_MIN * 100;
		total_probability += categories[cls].naiveBayesResult;
	}

	for (cls = 0; cls < NBCategories.used; cls++) // Order of instructions in this loop matters!
	{
		categories[cls].naiveBayesResult = categories[cls].naiveBayesResult / total_probability; // fix-up probability
		if (categories[cls].naiveBayesResult > categories[bestseen].naiveBayesResult) bestseen = cls; // are we the best
		remainder += categories[cls].naiveBayesResult; // add up remainder
	}
	remainder -= categories[bestseen].naiveBayesResult; // fix-up remainder

	myReply.probability = categories[bestseen].naiveBayesResult;
	myReply.probScaled = 10 * (log10(categories[bestseen].naiveBayesResult) - log10(remainder));
	myReply.name = NBCategories.categories[bestseen].name;

return myReply;
}

FBCClassification doHSPrepandClassify(HashList *toClassify)
{
uint32_t i, j;
uint16_t missing;
FBCJudge *categories = malloc(NBCategories.used * sizeof(FBCJudge));
int64_t BSRet=-1;
FBCClassification data;
uint64_t total;
double den;

	// Set numerator and denominator to 1 so we don't have 0's as all answers
	for(i=0; i < toClassify->used; i++)
	{
		categories[i].naiveBayesNum = 1;
		categories[i].naiveBayesDen = 1;
	}

	// do bayes multiplication
	for(i=0; i < toClassify->used; i++)
	{
		if((BSRet=FBCBinarySearch(&NBJudgeHashList, 0, NBJudgeHashList.used-1, toClassify->hashes[i]))>=0)
		{
//			printf("Found %"PRIX64"\n", toClassify->hashes[i]);
			for(j = 0; j < NBJudgeHashList.hashes[BSRet].used; j++)
			{
				/*BAYES*/
				if(NBJudgeHashList.FBC_LOCKED)
				{
					categories[NBJudgeHashList.hashes[BSRet].users[j].category].naiveBayesNum *= NBJudgeHashList.hashes[BSRet].users[j].data.probability;
					categories[NBJudgeHashList.hashes[BSRet].users[j].category].naiveBayesDen *= (1 - NBJudgeHashList.hashes[BSRet].users[j].data.probability);
					for(missing = NBJudgeHashList.hashes[BSRet].users[j].category + 1; missing < NBJudgeHashList.hashes[BSRet].users[j + 1].category; missing++)
					{
						categories[missing].naiveBayesNum *= .5;
						categories[missing].naiveBayesDen *= .5;
					}
				}
				else
				{
					total = 0;
					den = 0;
					for(uint_least16_t k=0; k < NBJudgeHashList.hashes[BSRet].used; k++)
					{
						total += NBJudgeHashList.hashes[BSRet].users[k].data.count;
					}
					den = MARKOV_C1 * (total + MARKOV_C2) * 256;
					for(uint_least16_t k=0; k < NBJudgeHashList.hashes[BSRet].used; k++)
					{
						categories[NBJudgeHashList.hashes[BSRet].users[k].category].naiveBayesNum *= (0.5 + NBJudgeHashList.hashes[BSRet].users[k].data.count / den);
						categories[NBJudgeHashList.hashes[BSRet].users[k].category].naiveBayesDen *= (1 - categories[NBJudgeHashList.hashes[BSRet].users[k].category].naiveBayesNum);
						for(missing = NBJudgeHashList.hashes[BSRet].users[k].category + 1; missing < NBJudgeHashList.hashes[BSRet].users[k + 1].category; missing++)
						{
							categories[missing].naiveBayesNum *= .5;
							categories[missing].naiveBayesDen *= .5;
						}
					}
				}
			}
		}
	}
//	printf("Found %"PRIu16" out of %"PRIu16" items\n", z, toClassify->used);

	data=doBayesClassify(categories, toClassify);

	// cleanup
	free(categories);
	return data;
}
