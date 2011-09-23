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
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "html.h"

#define IN_BAYES 1
#include "bayes.h"

// Push high values over 1.00 so that we conserve more bits
#define MAGIC_CONSERVE_OFFSET 0.4
// Magic minimum should be 4-6 significant decimal places. It is used in place of ZERO, or
// other too low of numbers to conserve bits.
#define MAGIC_MINIMUM (0.0001 + MAGIC_CONSERVE_OFFSET)
// How many keys can we process before we must rescale to conserve bits
// If this turns into a configuration parameter, it must be bounded at 100 and 500
#define KEYS_PROCESS_BEFORE_RESCALE 200

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
	if(NBCategories.used) free(NBCategories.categories);

	for(i=0; i < NBJudgeHashList.used; i++)
	{
		free(NBJudgeHashList.hashes[i].users);
	}
	if(NBJudgeHashList.used) free(NBJudgeHashList.hashes);
}

int optimizeFBC(FBCHashList *hashes)
{
uint64_t total;
uint64_t count;

	if(hashes->FBC_LOCKED) return -1;

	// Do precomputing
	for(uint32_t i = 0; i < hashes->used; i++)
	{
		total = MARKOV_C2 + 1;
		for(uint_least16_t j = 0; j < hashes->hashes[i].used; j++)
		{
			total += hashes->hashes[i].users[j].data.count;
		}

		for(uint_least16_t j = 0; j < hashes->hashes[i].used; j++)
		{
			count = hashes->hashes[i].users[j].data.count; // necessary since count and probability are a union
			hashes->hashes[i].users[j].data.probability = ((double) count / (double) (total)); // compute P(w|C)
			hashes->hashes[i].users[j].data.probability /= ((double) (total - count) / (double) (total)); // compute and divide by P(w|not C)
			if(hashes->hashes[i].users[j].data.probability < MAGIC_MINIMUM) hashes->hashes[i].users[j].data.probability = MAGIC_MINIMUM;
			else if(hashes->hashes[i].users[j].data.probability > 1) hashes->hashes[i].users[j].data.probability = 1;
			hashes->hashes[i].users[j].data.probability += MAGIC_CONSERVE_OFFSET; // Not strictly mathematically accurate, but it conserves bits
//			printf("Probability %G\n", hashes->hashes[i].users[j].data.probability);
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
int offsetFixup;
	if(fbc_file < 0) return -999;
	lseek64(fbc_file, 0, SEEK_SET);
	do {
		offsetFixup = read(fbc_file, &header->ID, 3);
		if(offsetFixup < 3) lseek64(fbc_file, -offsetFixup, SEEK_CUR);
	} while(offsetFixup > 0 && offsetFixup < 3);
	if(offsetFixup > 0) // Less than or equal to 0 is an empty file
	{
		if(memcmp(header->ID, "FNB", FBC_HEADERv1_ID_SIZE) != 0)
		{
			ci_debug_printf(10, "Not a FastNaiveBayes file\n");
			return -1;
		}
		do {
			offsetFixup = read(fbc_file, &header->version, FBC_HEADERv1_VERSION_SIZE);
			if(offsetFixup < FBC_HEADERv1_VERSION_SIZE) lseek64(fbc_file, -offsetFixup, SEEK_CUR);
		} while(offsetFixup > 0 && offsetFixup < FBC_HEADERv1_VERSION_SIZE);
		if(header->version != FBC_FORMAT_VERSION && header->version != OLD_FBC_FORMAT_VERSION )
		{
			ci_debug_printf(10, "Wrong version of FastNaiveBayes file\n");
			return -2;
		}
		do {
			offsetFixup = read(fbc_file, &header->UBM, FBC_HEADERv1_UBM_SIZE);
			if(offsetFixup < FBC_HEADERv1_UBM_SIZE) lseek64(fbc_file, -offsetFixup, SEEK_CUR);
		} while(offsetFixup > 0 && offsetFixup < FBC_HEADERv1_UBM_SIZE);
		if(header->UBM != UNICODE_BYTE_MARK)
		{
			ci_debug_printf(10, "FastNaiveBayes file of incompatible endianness\n");
			return -3;
		}

		if(header->version >= 2) // Make sure we have a proper WCS
		{
			do {
				offsetFixup = read(fbc_file, &header->WCS, FBC_HEADERv2_WCS_SIZE);
				if(offsetFixup < FBC_HEADERv2_WCS_SIZE) lseek64(fbc_file, -offsetFixup, SEEK_CUR);
			} while(offsetFixup > 0 && offsetFixup < FBC_HEADERv2_WCS_SIZE);
			if(header->WCS != sizeof(wchar_t))
			{
				ci_debug_printf(10, "FastNaiveBayes file of incompatible wchar_t format\n");
				return -6;
			}
		}
		else ci_debug_printf(5, "Loading old FastNaiveBayes file\n");

		if(read(fbc_file, &header->records, FBC_HEADERv1_RECORDS_QTY_SIZE) != FBC_HEADERv1_RECORDS_QTY_SIZE)
		{
			ci_debug_printf(10, "FastNaiveBayes file has invalid header: no records count\n");
			return -4;
		}
		return 0;
	}
	else return -5; // Empty FNB file
}

void writeFBCHeader(int file, FBC_HEADERv1 *header)
{
int i;
	memcpy(&header->ID, "FNB", 3);
	header->version = FBC_FORMAT_VERSION;
	header->UBM = UNICODE_BYTE_MARK;
	header->WCS = sizeof(wchar_t);
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
		i = write(file, &header->WCS, FBC_HEADERv2_WCS_SIZE);
		if(i < FBC_HEADERv2_WCS_SIZE) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < FBC_HEADERv2_WCS_SIZE);

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

int isBayes(char *filename)
{
FBC_HEADERv1 header;
int file;

	file = openFBC(filename, &header, 0);
	if(file > 0)
	{
		close(file);
		return 1;
	}
return 0;
}

int writeFBCHashes(int file, FBC_HEADERv1 *header, FBCHashList *hashes_list, uint16_t category, int zero_point)
{
uint32_t i;
uint_least32_t qty = 0;
uint16_t j;
int writecheck;

        if(hashes_list->FBC_LOCKED) return -1; // We cannot write when FBC_LOCKED is set, as we are in optimized and not raw count mode
	if(header->WCS != sizeof(wchar_t) || header->version != FBC_FORMAT_VERSION)
	{
		ci_debug_printf(1, "writeFBCHashes cannot write to a different version file or to a file with a different WCS!\n");
		return -2;
	}
	if(hashes_list->used) // check before we write
	{
		for(i = 0; i < hashes_list->used; i++)
		{
			for(j = 0; j < hashes_list->hashes[i].used; j++)
			{
				// Make sure that this is the right category and that we have enough counts to write (not <= zero_point)
				if(hashes_list->hashes[i].users[j].category == category && hashes_list->hashes[i].users[j].data.count >= zero_point)
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
		lseek64(file, 9, SEEK_SET);
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
	i = ftruncate64(file, 14);
	lseek64(file, 13, SEEK_SET);
	if(hashes_list->used) // check before we write
	{
		for(i = 0; i < hashes_list->used; i++)
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
		lseek64(file, 9, SEEK_SET);
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

static uint32_t featuresInCategory(int fbc_file, FBC_HEADERv1 *header)
{
	return header->records;
}

int loadBayesCategory(char *fbc_name, char *cat_name)
{
int fbc_file;
uint32_t i, z, shortcut=0, offsetPos=3;
int64_t BSRet=-1;
int64_t offsets[3];
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
	offsets[2] = NBJudgeHashList.used;

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
					}
					else ci_debug_printf(10, "PROBLEM IN THE CITY\n");
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

		offsets[2] = NBJudgeHashList.used;
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
uint32_t i, z, shortcut = 0, offsetPos = 3, handled = 0;
uint16_t used;
int64_t BSRet = -1;
int64_t offsets[3];
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
	offsets[2] = NBJudgeHashList.used;
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

		offsets[2] = NBJudgeHashList.used;
	}

	if(startHashes != NBJudgeHashList.used) qsort(NBJudgeHashList.hashes, NBJudgeHashList.used, sizeof(FBCFeatureExt), &FBCjudgeHash_compare);
//	ci_debug_printf(10, "Categories: %"PRIu32" Hashes Used: %"PRIu32"\n", NBCategories.used, NBJudgeHashList.used);

	// Fixup memory usage
/*	if(NBJudgeHashList.slots > NBJudgeHashList.used && NBJudgeHashList.used > 1)
	{
		NBJudgeHashList.slots = NBJudgeHashList.used;
		NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
	}*/ // Not needed because this is a trainer

	return 1;
}

static int preload_hash_compare(const uint_least64_t a, const uint_least64_t b)
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
uint32_t i;
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

//		ci_debug_printf(10, "Loading keys: %"PRIX64" in Category: %s Document:%"PRIu16"\n", docHashes->hashes[j], cat_name, i);
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
	// Fixup memory usage
	if(NBJudgeHashList.slots > NBJudgeHashList.used && NBJudgeHashList.used > 1)
	{
		NBJudgeHashList.slots = NBJudgeHashList.used;
		NBJudgeHashList.hashes = realloc(NBJudgeHashList.hashes, NBJudgeHashList.slots * sizeof(FBCFeatureExt));
	}

	close(fbc_file);
	return 0;
}

int loadMassBayesCategories(char *fbc_dir)
{
DIR *dirp;
struct dirent *dp;
char old_dir[PATH_MAX];
int name_len;
char *cat_name;

	getcwd(old_dir, PATH_MAX);
	chdir(fbc_dir);
	preLoadBayes("preload.fnb");
	chdir(old_dir);

	if ((dirp = opendir(fbc_dir)) == NULL)
	{
		ci_debug_printf(3, "couldn't open '%s'", fbc_dir);
		return -1;
	}

	chdir(fbc_dir);
	do {
		errno = 0;
		if ((dp = readdir(dirp)) != NULL)
		{
			if (strcmp(dp->d_name, "preload.fnb") != 0 && strstr(dp->d_name, ".fnb") != NULL)
			{
				name_len = strstr(dp->d_name, ".fnb") - dp->d_name;
				cat_name = malloc(name_len + 1);
				strncpy(cat_name, dp->d_name, name_len);
				cat_name[name_len] = '\0';
				loadBayesCategory(dp->d_name, cat_name);
				free(cat_name);
			}
		}
	} while (dp != NULL);
	if (errno != 0)
		perror("error reading directory");
	else
		(void) closedir(dirp);

	chdir(old_dir);
	return 1;
}

static HTMLClassification doBayesClassify(FBCJudge *categories, HashList *unknown)
{
double total_probability = DBL_MIN;
double remainder = DBL_MIN;

uint32_t cls;

uint32_t bestseen = 0, secondbest = 1;
HTMLClassification myReply = { .primary_name = NULL, .primary_probability = 0.0, .primary_probScaled = 0.0, .secondary_name = NULL, .secondary_probability = 0.0, .secondary_probScaled = 0.0  };

	// Re-normalize Result to probability
	do {
		if(total_probability > DBL_MAX) // reset total_probability so that we are not overflowing
		{
			total_probability = DBL_MIN;
			for (cls = 0; cls < NBCategories.used; cls++)
			{
				categories[cls].naiveBayesResult /= 100;
			}
		}

		for (cls = 0; cls < NBCategories.used; cls++)
		{
			// Avoid divide by zero and keep value small, for log10(remainder) below
			if (categories[cls].naiveBayesResult > DBL_MAX)
				categories[cls].naiveBayesResult = DBL_MAX;
			else if (categories[cls].naiveBayesResult < DBL_MIN)
				categories[cls].naiveBayesResult = DBL_MIN;
			total_probability += categories[cls].naiveBayesResult;
		}
	} while (total_probability > DBL_MAX); // Do until we are a valid double number

	// Find the best and second best categories
	for (cls = 0; cls < NBCategories.used; cls++) // Order of instructions in this loop matters!
	{
		categories[cls].naiveBayesResult = categories[cls].naiveBayesResult / total_probability; // fix-up probability
		if (categories[cls].naiveBayesResult > categories[bestseen].naiveBayesResult)
		{
			secondbest = bestseen;
			bestseen = cls; // are we the best
		}
		else if (cls != bestseen && categories[cls].naiveBayesResult > categories[secondbest].naiveBayesResult) secondbest = cls;
		remainder += categories[cls].naiveBayesResult; // add up remainder
	}

	remainder -= categories[bestseen].naiveBayesResult; // fix-up remainder
	if(remainder < DBL_MIN) remainder = DBL_MIN;

	// Setup data for second best category, if applicable
	for(int i = 0; i < number_secondaries; i++)
	{
		if(tre_regexec(&secondary_compares[i].primary_regex, NBCategories.categories[bestseen].name, 0, NULL, 0) != REG_NOMATCH && tre_regexec(&secondary_compares[i].secondary_regex, NBCategories.categories[secondbest].name, 0, NULL, 0) != REG_NOMATCH)
		{
			remainder -= categories[secondbest].naiveBayesResult;
			if(remainder < DBL_MIN) remainder = DBL_MIN;
			myReply.secondary_probability = categories[secondbest].naiveBayesResult;
			myReply.secondary_probScaled = 10 * (log10(categories[secondbest].naiveBayesResult) - log10(remainder));
			myReply.secondary_name = NBCategories.categories[secondbest].name;
			i = number_secondaries;
		}
		else if(secondary_compares[i].bidirectional == 1)
		{
			if(tre_regexec(&secondary_compares[i].primary_regex, NBCategories.categories[secondbest].name, 0, NULL, 0) != REG_NOMATCH && tre_regexec(&secondary_compares[i].secondary_regex, NBCategories.categories[bestseen].name, 0, NULL, 0) != REG_NOMATCH)
			{
				remainder -= categories[secondbest].naiveBayesResult;
				if(remainder < DBL_MIN) remainder = DBL_MIN;
				myReply.secondary_probability = categories[secondbest].naiveBayesResult;
				myReply.secondary_probScaled = 10 * (log10(categories[secondbest].naiveBayesResult) - log10(remainder));
				myReply.secondary_name = NBCategories.categories[secondbest].name;
				i = number_secondaries;
			}
		}
	}

/*	for (cls = 0; cls < NBCategories.used; cls++)
	{
		ci_debug_printf(10, "Category %s Result %G\n", NBCategories.categories[cls].name, categories[cls].naiveBayesResult);
	}*/

	myReply.primary_probability = categories[bestseen].naiveBayesResult;
	myReply.primary_probScaled = 10 * (log10(categories[bestseen].naiveBayesResult) - log10(remainder));
	myReply.primary_name = NBCategories.categories[bestseen].name;

return myReply;
}

HTMLClassification doBayesPrepandClassify(HashList *toClassify)
{
uint32_t i, j, processed = 0;
uint16_t missing, nextReal;
FBCJudge *categories = malloc(NBCategories.used * sizeof(FBCJudge));
int64_t BSRet = -1;
HTMLClassification data;
uint64_t total;
double local_probability;
// Variables for scaling
uint32_t cls, bestseen = 0;
double scale = 0;
const double scale_numerator = DBL_MAX / NBCategories.used;

	if(NBCategories.used < 2) return data; // We must have at least two categories loaded or it is pointless to run

	// Set result to 1 so we don't have 0's as all answers
	for(i = 0; i < NBCategories.used; i++)
	{
		categories[i].naiveBayesResult = DBL_MAX / 20000; // Conserve bits toward a maximum, but try to avoid overflow
	}

	// do bayes multiplication
	for(i = 0; i < toClassify->used; i++)
	{
		if((BSRet=FBCBinarySearch(&NBJudgeHashList, 0, NBJudgeHashList.used-1, toClassify->hashes[i])) >= 0)
		{
//			ci_debug_printf(10, "Found %"PRIX64"\n", toClassify->hashes[i]);
			if(NBJudgeHashList.FBC_LOCKED)
			{
				for(j = 0; j < NBJudgeHashList.hashes[BSRet].used; j++)
				{
					/*BAYES*/
					if(j == 0) // Catch missing at the beginning
					{
						nextReal = NBJudgeHashList.hashes[BSRet].users[j].category;
						for(missing = 0; missing < nextReal; missing++)
						{
//							ci_debug_printf(10, "Last: (empty) Next: %"PRIu16" Missing: %"PRIu16"\n", nextReal, missing);
							categories[missing].naiveBayesResult *= MAGIC_MINIMUM;
						}
					}

//					ci_debug_printf(10, "Category: %"PRIu16" out of %"PRIu16"\n", NBJudgeHashList.hashes[BSRet].users[j].category, NBCategories.used - 1);

					categories[NBJudgeHashList.hashes[BSRet].users[j].category].naiveBayesResult *= NBJudgeHashList.hashes[BSRet].users[j].data.probability;

					// Catch missing at the end or in between
					if(j + 1 < NBJudgeHashList.hashes[BSRet].used)
					{
						nextReal = NBJudgeHashList.hashes[BSRet].users[j + 1].category;
					}
					else nextReal = NBCategories.used;
					for(missing = NBJudgeHashList.hashes[BSRet].users[j].category + 1; missing < nextReal; missing++)
					{
//						ci_debug_printf(10, "Last: %"PRIu16" Next: %"PRIu16" Missing: %"PRIu16"\n", NBJudgeHashList.hashes[BSRet].users[j].category, nextReal, missing);
						categories[missing].naiveBayesResult *= MAGIC_MINIMUM;
					}
				}
			}
			else
			{
				/*BAYES*/
				total = MARKOV_C2 + 1;
				for(uint_least16_t k = 0; k < NBJudgeHashList.hashes[BSRet].used; k++)
				{
					total += NBJudgeHashList.hashes[BSRet].users[k].data.count;
				}

				for(uint_least16_t j = 0; j < NBJudgeHashList.hashes[BSRet].used; j++)
				{
					if(j == 0) // Catch missing at the beginning
					{
						nextReal = NBJudgeHashList.hashes[BSRet].users[j].category;
						for(missing = 0; missing < nextReal; missing++)
						{
//							ci_debug_printf(10, "Last: (empty) Next: %"PRIu16" Missing: %"PRIu16"\n", nextReal, missing);
							categories[missing].naiveBayesResult *= MAGIC_MINIMUM;
						}
					}

//					ci_debug_printf(10, "Category: %"PRIu16" out of %"PRIu16"\n", NBJudgeHashList.hashes[BSRet].users[j].category, NBCategories.used);

					local_probability =  ((double) NBJudgeHashList.hashes[BSRet].users[j].data.count / (double) (total)); // compute P(w|C)
					local_probability /= ((double) (total - NBJudgeHashList.hashes[BSRet].users[j].data.count) / (double) (total)); // compute and divide by P(w|not C)
					if(local_probability < MAGIC_MINIMUM) local_probability = MAGIC_MINIMUM;
					else if(local_probability > 1) local_probability = 1;
					local_probability += MAGIC_CONSERVE_OFFSET; // Not strictly mathematically accurate, but it conserves bits

					categories[NBJudgeHashList.hashes[BSRet].users[j].category].naiveBayesResult *= local_probability;

					// Catch missing at the end or in between
					if(j + 1 < NBJudgeHashList.hashes[BSRet].used)
					{
						nextReal = NBJudgeHashList.hashes[BSRet].users[j + 1].category;
					}
					else nextReal = NBCategories.used;
					for(missing = NBJudgeHashList.hashes[BSRet].users[j].category + 1; missing < nextReal; missing++)
					{
//						ci_debug_printf(10, "Last: %"PRIu16" Next: %"PRIu16" Missing: %"PRIu16"\n", NBJudgeHashList.hashes[BSRet].users[j].category, nextReal, missing);
						categories[missing].naiveBayesResult *= MAGIC_MINIMUM;
					}
				}
			}
			processed++;
			// Do bit conservation by occasionally maximizing values
			if(processed == KEYS_PROCESS_BEFORE_RESCALE)
			// This used to be if(processed % KEYS_PROCESS_BEFORE_RESCALE == 0)
			// It has been changed to do a simple compare instead of a division and compare.
			// The only downside is it cannot now be used accurately to make sure we processed
			// more than x hashes for a trusted result.
			{
				// Find class with highest naiveBayesResult
				for (cls = 0; cls < NBCategories.used; cls++)
				{
					// If we are over DBL_MAX reset to DBL_MAX.
					// Doing this only every so many processed keys
					// makes this not be perfectly accurate, but it is worth
					// the saved cycles.
					// If this turns out not to be enough, we only need to check
					// this above when we actually find a key in the category in question.
					// Missing values will always shrink the value, not grow it.
					if (categories[cls].naiveBayesResult > DBL_MAX)
					{
						categories[cls].naiveBayesResult = DBL_MAX;
						bestseen = cls;
					}
					// If we are the bestseen, record it.
					else if (categories[cls].naiveBayesResult > categories[bestseen].naiveBayesResult)
					{
						bestseen = cls; // are we the best
					}
				}
				// Compute scale
				scale = scale_numerator / categories[bestseen].naiveBayesResult;
				if(scale > DBL_MAX) scale = scale_numerator;

				// Maximize values for bit conservation
				for (cls = 0; cls < NBCategories.used; cls++)
				{
					categories[cls].naiveBayesResult *= scale;
				}
				processed = 0;
			}
		}
	}
//	ci_debug_printf(10, "Found %"PRIu16" out of %"PRIu16" items\n", z, toClassify->used);

/*	for(i = 0; i < NBCategories.used; i++)
	{
		ci_debug_printf(10, "Here Category %s Result %G\n", NBCategories.categories[i].name, categories[i].naiveBayesResult);
	} */

	data = doBayesClassify(categories, toClassify);

	// cleanup
	free(categories);
	return data;
}
