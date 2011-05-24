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

#define IN_HYPERSPACE 1
#include "hyperspace.h"


FHSTextCategoryExt HSCategories;
HashListExt HSJudgeHashList;

void initHyperSpaceClassifier(void)
{
	HSJudgeHashList.slots = 0;
	HSJudgeHashList.hashes = NULL;
	HSJudgeHashList.used = 0;
	HSCategories.slots = HYPERSPACE_CATEGORY_INC;
	HSCategories.categories = calloc(HSCategories.slots, sizeof(FHSTextCategory));
	HSCategories.used = 0;
}

void deinitHyperSpaceClassifier(void)
{
uint32_t i=0;
	for(i=0; i < HSCategories.used; i++)
	{
		free(HSCategories.categories[i].name);
		free(HSCategories.categories[i].documentKnownHashes);
	}
	free(HSCategories.categories);

	for(i=0; i < HSJudgeHashList.used; i++)
	{
		free(HSJudgeHashList.hashes[i].users);
	}
	free(HSJudgeHashList.hashes);
}

static int judgeHash_compare(void const *a, void const *b)
{
hyperspaceFeatureExt *ha, *hb;

	ha = (hyperspaceFeatureExt *) a;
	hb = (hyperspaceFeatureExt *) b;
	if(ha->hash < hb->hash)
		return -1;
	if(ha->hash > hb->hash)
		return 1;

return 0; // Equal
}


static int verifyFHS(int fhs_file, FHS_HEADERv1 *header)
{
int offsetFixup;
	lseek64(fhs_file, 0, SEEK_SET);
	do {
		offsetFixup = read(fhs_file, &header->ID, 3);
		if(offsetFixup < 3) lseek64(fhs_file, -offsetFixup, SEEK_CUR);
	} while(offsetFixup > 0 && offsetFixup < 3);
	if(offsetFixup > 0) // Less than or equal to 0 is an empty file
	{
		if(memcmp(header->ID, "FHS", FHS_HEADERv1_ID_SIZE) != 0)
		{
//			printf("Not a FastHyperSpace file\n");
			return -1;
		}
		do {
			offsetFixup = read(fhs_file, &header->version, FHS_HEADERv1_VERSION_SIZE);
			if(offsetFixup < FHS_HEADERv1_VERSION_SIZE) lseek64(fhs_file, -offsetFixup, SEEK_CUR);
		} while(offsetFixup > 0 && offsetFixup < FHS_HEADERv1_VERSION_SIZE);
		if(header->version != HYPERSPACE_FORMAT_VERSION)
		{
//			printf("Wrong version of FastHyperSpace file\n");
			return -2;
		}
		do {
			offsetFixup = read(fhs_file, &header->UBM, FHS_HEADERv1_UBM_SIZE);
			if(offsetFixup < FHS_HEADERv1_UBM_SIZE) lseek64(fhs_file, -offsetFixup, SEEK_CUR);
		} while(offsetFixup > 0 && offsetFixup < FHS_HEADERv1_UBM_SIZE);
		if(header->UBM != UNICODE_BYTE_MARK)
		{
//			printf("FastHyperSpace file of incompatible endianness\n");
			return -3;
		}
		if(read(fhs_file, &header->records, FHS_HEADERv1_RECORDS_QTY_SIZE) != 2)
		{
//			printf("FastHyperSpace file has invalid header: no records count\n");
			return -4;
		}
		return 0;
	}
	else return -5; // Empty FHS file
}

void writeFHSHeader(int file, FHS_HEADERv1 *header)
{
int i;
	memcpy(&header->ID, "FHS", 3);
	header->version = HYPERSPACE_FORMAT_VERSION;
	header->UBM = UNICODE_BYTE_MARK;
	header->records=0;
	i = ftruncate(file, 0);
	lseek64(file, 0, SEEK_SET);
        do {
		i = write(file, "FHS", 3);
		if(i < 3) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < 3);

        do {
		i = write(file, &header->version, FHS_HEADERv1_VERSION_SIZE);
		if(i < FHS_HEADERv1_VERSION_SIZE) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < FHS_HEADERv1_VERSION_SIZE);

        do {
		i = write(file, &header->UBM, FHS_HEADERv1_UBM_SIZE);
		if(i < FHS_HEADERv1_UBM_SIZE) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < FHS_HEADERv1_UBM_SIZE);

        do {
		i = write(file, &header->records, FHS_HEADERv1_RECORDS_QTY_SIZE);
		if(i < FHS_HEADERv1_RECORDS_QTY_SIZE) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < FHS_HEADERv1_RECORDS_QTY_SIZE);
}

int openFHS(char *filename, FHS_HEADERv1 *header, int forWriting)
{
int file=0;
	file = open(filename, (forWriting ? (O_CREAT | O_RDWR) : O_RDONLY), S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP);
	if(verifyFHS(file, header) < 0)
	{
		if(forWriting == 1)
		{
			writeFHSHeader(file, header);
//			printf("Created FastHyperSpace file: %s\n", filename);
		}
		else return -1;
	}
	return file;
}

int isHyperSpace(char *filename)
{
FHS_HEADERv1 header;
int file;

	file = openFHS(filename, &header, 0);
	if(file > 0)
	{
		close(file);
		return 1;
	}
return -1;
}

int writeFHSHashes(int file, FHS_HEADERv1 *header, HashList *hashes_list)
{
uint16_t i;
int writecheck;
	lseek64(file, 0, SEEK_END);
	if(hashes_list->used) // check before we write
	{
		do {
			writecheck = write(file, &hashes_list->used, FHS_v1_QTY_SIZE);
			if(writecheck < FHS_v1_QTY_SIZE) lseek64(file, -writecheck, SEEK_CUR);
		} while (writecheck >= 0 && writecheck < FHS_v1_QTY_SIZE);
		for(i = 0; i < hashes_list->used; i++)
		{
			do {
				writecheck = write(file, &hashes_list->hashes[i], FHS_v1_HASH_SIZE);
                                if(writecheck < FHS_v1_HASH_SIZE) lseek64(file, -writecheck, SEEK_CUR);
			} while (writecheck >= 0 && writecheck < FHS_v1_HASH_SIZE);
		}
		/* Ok, have written hashes, now save new count */
		header->records = header->records+1;
		lseek64(file, 7, SEEK_SET);
		do {
			writecheck = write(file, &header->records, FHS_HEADERv1_RECORDS_QTY_SIZE);
			if(writecheck < FHS_HEADERv1_RECORDS_QTY_SIZE) lseek64(file, -writecheck, SEEK_CUR);
		} while (writecheck >=0 && writecheck < FHS_HEADERv1_RECORDS_QTY_SIZE);
		return 0;
	}
	return -1;
}

int writeFHSHashesPreload(int file, FHS_HEADERv1 *header, HashListExt *hashes_list)
{
uint16_t i;
int writecheck;
	lseek64(file, 0, SEEK_END);
	if(hashes_list->used) // check before we write
	{
		do {
			writecheck = write(file, &hashes_list->used, FHS_v1_QTY_SIZE);
			if(writecheck < FHS_v1_QTY_SIZE) lseek64(file, -writecheck, SEEK_CUR);
		} while (writecheck >=0 && writecheck < FHS_v1_QTY_SIZE);
		for(i=0; i < hashes_list->used; i++)
		{
			do {
				writecheck = write(file, &hashes_list->hashes[i], FHS_v1_HASH_SIZE);
                                if(writecheck < FHS_v1_HASH_SIZE) lseek64(file, -writecheck, SEEK_CUR);
			} while (writecheck >=0 && writecheck < FHS_v1_HASH_SIZE);
		}
		/* Ok, have written hashes, now save new count */
		header->records = header->records+1;
		lseek64(file, 7, SEEK_SET);
		do {
			writecheck = write(file, &header->records, FHS_HEADERv1_RECORDS_QTY_SIZE);
			if(writecheck < FHS_HEADERv1_RECORDS_QTY_SIZE) lseek64(file, -writecheck, SEEK_CUR);
		} while (writecheck >=0 && writecheck < FHS_HEADERv1_RECORDS_QTY_SIZE);
		return 0;
	}
	return -1;
}

static int64_t HSBinarySearch(HashListExt *hashes_list, int64_t start, int64_t end, uint64_t key)
{
int64_t mid=0;
	if(start > end)
		return -1;
	mid = start + ((end - start) / 2);
//	printf("Start %"PRId64" end %"PRId64" mid %"PRId64"\n", start, end, mid);
//	printf("Keys @ mid: %"PRIX64" looking for %"PRIX64"\n", hashes_list->hashes[mid].hash, key);
	if(hashes_list->hashes[mid].hash > key)
		return HSBinarySearch(hashes_list, start, mid-1, key);
	else if(hashes_list->hashes[mid].hash < key)
		return HSBinarySearch(hashes_list, mid+1, end, key);
	else return mid;

	return -1; // This should never be reached
}

static uint32_t featuresInCategory(int fhs_file, FHS_HEADERv1 *header)
{
struct stat stat_buf;
	fstat(fhs_file, &stat_buf);
	//(stat_buf.st_size - FHS_HEADERv1_TOTAL_SIZE - (header->records * sizeof(FHS_v1_QTY_SIZE))) / FHS_v1_HASH_SIZE + 10); -- for some reason this isn't enough!
        return stat_buf.st_size / FHS_v1_HASH_SIZE;
}

HTMLFeature *loadDocument(char *fhs_name, char *cat_name, int fhs_file, uint16_t numHashes)
{
HTMLFeature *hashes=NULL;
int status = 0;
int bytes = 0;
int to_read = FHS_v1_HASH_SIZE * numHashes;
        hashes = malloc(FHS_v1_HASH_SIZE * numHashes);
	// printf("Going to read %"PRIu16" hashes from record %"PRIu16"\n", numHashes, i);
        do {
		status = read(fhs_file, hashes + bytes, to_read);
		if(status > 0)
		{
			bytes += status;
			to_read -= status;
		}

	} while(status > 0);
        if(bytes < FHS_v1_HASH_SIZE * numHashes) printf("Corrupted fhs file: %s for cat_name: %s\n", fhs_name, cat_name);
return hashes;
}

void closeDocument(uint64_t *hashes)
{
	free(hashes);
}

// The following number is based on a lot of experimentation. It should be between 35-70. The larger the data set, the higher the number should be.
// 95 is ideal for my training set.
#define HS_OFFSET_MAX 95
int loadHyperSpaceCategory(char *fhs_name, char *cat_name)
{
int fhs_file;
uint16_t i, j, z, shortcut=0, offsetPos=2;
int64_t BSRet=-1;
int64_t offsets[HS_OFFSET_MAX+1];
HTMLFeature *docHashes;
FHS_HEADERv1 header;
uint16_t numHashes=0;
uint32_t startHashes = HSJudgeHashList.used;
	offsets[0] = 0;
	if((fhs_file = openFHS(fhs_name, &header, 0)) < 0) return fhs_file;
	if(HSCategories.used == HSCategories.slots)
	{
		HSCategories.slots += HYPERSPACE_CATEGORY_INC;
		HSCategories.categories = realloc(HSCategories.categories, HSCategories.slots * sizeof(FHSTextCategory));
	}
	HSCategories.categories[HSCategories.used].name = strndup(cat_name, MAX_HYPSERSPACE_CATEGORY_NAME);
	HSCategories.categories[HSCategories.used].totalDocuments = header.records;
	HSCategories.categories[HSCategories.used].totalFeatures = 0;
	HSCategories.categories[HSCategories.used].documentKnownHashes = malloc(header.records * sizeof(uint16_t));

	if(HSJudgeHashList.used + featuresInCategory(fhs_file, &header) >= HSJudgeHashList.slots)
	{
		HSJudgeHashList.slots += featuresInCategory(fhs_file, &header);
		HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(hyperspaceFeatureExt));
	}

//	printf("Going to read %"PRIu16" records from %s\n", header.records, cat_name);
	offsets[1] = HSJudgeHashList.used;
	for(i = 0; i < header.records; i++)
	{
		if(read(fhs_file, &numHashes, FHS_v1_QTY_SIZE) < FHS_v1_QTY_SIZE) ; // ERRORFIXME;
		docHashes = loadDocument(fhs_name, cat_name, fhs_file, numHashes);

		HSCategories.categories[HSCategories.used].documentKnownHashes[i] = numHashes;
		HSCategories.categories[HSCategories.used].totalFeatures += numHashes;
		if(HSJudgeHashList.used + numHashes > HSJudgeHashList.slots)
		{
			if(HSJudgeHashList.slots != 0) printf("Ooops, we shouldn't be allocating more memory here. (%s)\n", fhs_name);
			HSJudgeHashList.slots += numHashes;
			HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(hyperspaceFeatureExt));
		}

		for(j = 0; j < numHashes; j++)
		{
//			printf("Loading keys: %"PRIX64" in Category: %s Document:%"PRIu16"\n", docHashes[j], cat_name, i);
			if(i > 0 || offsets[0] != offsets[1])
			{
				shortcut = 0;
				for(z = 1; z < offsetPos; z++)
				{
					if((BSRet=HSBinarySearch(&HSJudgeHashList, offsets[z-1], offsets[z] - 1, docHashes[j])) >= 0)
					{
						if (shortcut == 0)
						{
							HSJudgeHashList.hashes[BSRet].users = realloc(HSJudgeHashList.hashes[BSRet].users, (HSJudgeHashList.hashes[BSRet].used+1) * sizeof(FHSHashJudgeUsers));
//							ci_debug_printf(10, "Found keys: %"PRIX64" already in table (offset %"PRIu16"), updating\n", docHashes[j], z);
							HSJudgeHashList.hashes[BSRet].users[HSJudgeHashList.hashes[BSRet].used].category = HSCategories.used;
							HSJudgeHashList.hashes[BSRet].users[HSJudgeHashList.hashes[BSRet].used].document = i;
							HSJudgeHashList.hashes[BSRet].used++;
						}
//						else ci_debug_printf(10, "PROBLEM IN THE CITY\n");
						shortcut++;
					}
				}
				if(!shortcut) goto STORE_NEW;
			}
			else {
				STORE_NEW:
//				ci_debug_printf(10, "Didn't find keys: %"PRIX64" in table\n", docHashes[j]);
				HSJudgeHashList.hashes[HSJudgeHashList.used].hash = docHashes[j];
				HSJudgeHashList.hashes[HSJudgeHashList.used].used = 0;
				HSJudgeHashList.hashes[HSJudgeHashList.used].users = calloc(1, sizeof(FHSHashJudgeUsers));
				HSJudgeHashList.hashes[HSJudgeHashList.used].users[HSJudgeHashList.hashes[HSJudgeHashList.used].used].category = HSCategories.used;
				HSJudgeHashList.hashes[HSJudgeHashList.used].users[HSJudgeHashList.hashes[HSJudgeHashList.used].used].document = i;
				HSJudgeHashList.hashes[HSJudgeHashList.used].used++;
				HSJudgeHashList.used++;
			}
		}
		closeDocument(docHashes);
		if(offsetPos > HS_OFFSET_MAX)
		{
			qsort(HSJudgeHashList.hashes, HSJudgeHashList.used, sizeof(hyperspaceFeatureExt), &judgeHash_compare );
			offsetPos = 2;
		}
		offsets[offsetPos] = HSJudgeHashList.used;
		if(offsets[offsetPos-1] != offsets[offsetPos]) offsetPos++;
	}
	if(startHashes != HSJudgeHashList.used) qsort(HSJudgeHashList.hashes, HSJudgeHashList.used, sizeof(hyperspaceFeatureExt), &judgeHash_compare);
//	ci_debug_printf(10, "Categories: %"PRIu32" Hashes Used: %"PRIu32"\n", HSCategories.used, HSJudgeHashList.used);
	HSCategories.used++;
	// Fixup memory usage
	if(HSJudgeHashList.slots > HSJudgeHashList.used && HSJudgeHashList.used > 1)
	{
		HSJudgeHashList.slots = HSJudgeHashList.used;
		HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(hyperspaceFeatureExt));
	}
	close(fhs_file);
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

// This function just used to be a slimmed down version of loadHyperSpaceCategory.
// It was allowed to be called even after some categories were loaded. This is
// no longer the case. It must be called first. This was done to make it run faster,
// and be simpler / more obviously correct.
int preLoadHyperSpace(char *fhs_name)
{
int fhs_file;
uint16_t i, j;
uint_least64_t *docHashes;
FHS_HEADERv1 header;
uint16_t numHashes=0;

	if(HSJudgeHashList.used > 0)
	{
		ci_debug_printf(1, "TextPreload / preLoadHyperSpace called with some hashes already loaded. ABORTING PRELOAD!\n");
		return -1;
	}
	if((fhs_file = openFHS(fhs_name, &header, 0)) < 0) return fhs_file;

	if(featuresInCategory(fhs_file, &header) >= HSJudgeHashList.slots)
	{
		HSJudgeHashList.slots += featuresInCategory(fhs_file, &header);
		HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(hyperspaceFeatureExt));
	}

//	ci_debug_printf(10, "Going to read %"PRIu16" records from %s\n", header.records, fhs_name);
	for(i = 0; i < header.records; i++)
	{
		if(read(fhs_file, &numHashes, 2)<2) ; // ERRORFIXME;
		docHashes = loadDocument(fhs_name, fhs_name, fhs_file, numHashes);

		if(HSJudgeHashList.used + numHashes > HSJudgeHashList.slots)
		{
			if(HSJudgeHashList.slots != 0) ci_debug_printf(10, "Ooops, we shouldn't be allocating more memory here. (%s)\n", fhs_name);
			HSJudgeHashList.slots += numHashes;
			HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(hyperspaceFeatureExt));
		}

		for(j = 0; j < numHashes; j++)
		{
//			ci_debug_printf(10, "Loading keys: %"PRIX64", in Category: %s Document:%"PRIu16"\n", docHashes[j], cat_name, i);
			if(HSJudgeHashList.used == 0) goto ADD_HASH;
			switch(preload_hash_compare(HSJudgeHashList.hashes[HSJudgeHashList.used-1].hash, docHashes[j]))
			{
				case -1:
					ADD_HASH:
					HSJudgeHashList.hashes[HSJudgeHashList.used].hash = docHashes[j];
					HSJudgeHashList.hashes[HSJudgeHashList.used].used = 0;
					HSJudgeHashList.hashes[HSJudgeHashList.used].users = NULL;
					HSJudgeHashList.used++;
					break;
				case 0:
					ci_debug_printf(1, "Key: %"PRIX64" already loaded. Preload file %s corrupted?!?!\n", docHashes[j], fhs_name);
					break;
				case 1:
					ci_debug_printf(1, "Key: %"PRIX64" out of order. Preload file %s is corrupted!!!\n" \
								"Aborting preload as is.\n", docHashes[j], fhs_name);
					closeDocument(docHashes);
					return -1;
					break;
			}
		}
		closeDocument(docHashes);
	}
	// Fixup memory usage
	if(HSJudgeHashList.slots > HSJudgeHashList.used && HSJudgeHashList.used > 1)
	{
		HSJudgeHashList.slots = HSJudgeHashList.used;
		HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(hyperspaceFeatureExt));
	}
	close(fhs_file);
	return 0;
}

int loadMassHSCategories(char *fhs_dir)
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
		return -1;
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
	return 0;
}

static HTMLClassification doHyperSpaceClassify(uint32_t **categories, HashList *unknown)
{
double total_radiance = DBL_MIN;
double remainder = DBL_MIN;
double *class_radiance = malloc(HSCategories.used * sizeof(double));

uint32_t bestseen=0;
HTMLClassification myReply;

uint32_t cls, doc; // class and document counters
uint32_t nfeats;   // total features
uint32_t ufeats;   // features in this unknown
uint32_t kfeats;   // features in the known

// Basic match parameters
float k_disjoint_u;   // features in known doc, not in unknown
float u_disjoint_k;   // features in unknown doc, not in known
float k_intersect_u;  // feature in both known and unknown

// Distance is pseudo-pythagorean distance (sqrt(b+c) not
// sqrt(b^2 + c^2)).
// This is SQRT of count of complement K in U and U in K
float distance;

// Radiance - all linear waves fall off in accordance with
// the inverse square law (1/d^2). The power (light) in this
// case is the square of shared features between known and
// unknown. Hence the code ammounts to 1/d^2 * shared^2.
// Since distance and light are per document, we sum them up
// and do the inverse square law and then sum the results in
// a running total. The final value is per class.
float radiance;

	ufeats = unknown->used;

	for (cls = 0; cls < HSCategories.used; cls++)
		class_radiance[cls]=0.0;

	// Class-level loop
	for (cls = 0; cls < HSCategories.used; cls++)
	{
		// Document-level loop
		for(doc = 0 ; doc < HSCategories.categories[cls].totalDocuments; doc++)
		{
			kfeats = HSCategories.categories[cls].documentKnownHashes[doc];
			k_intersect_u = categories[cls][doc];
			u_disjoint_k = ufeats - (uint32_t) k_intersect_u;
			k_disjoint_u = HSCategories.categories[cls].documentKnownHashes[doc] - (uint32_t) k_intersect_u;
			nfeats = kfeats + ufeats - (uint32_t) k_intersect_u;

			if (nfeats > 10)
			{
				// This is not proper Pythagorean (Euclidean) distance.
				// Proper would be sqrtf(u_disjoint_k^2 + k_disjoint_u^2).
				// Proper distance is not used because it would tend to
				// "repulse" things from the right class. Using something
				// that weakens radiance for differences, but doesn't
				// "repulse."
				// We don't actually take a square root here, because our only
				// use is in the radience formula (inverse square law), where
				// we just square it again.
				distance = u_disjoint_k + k_disjoint_u;

				// This formula was the best found in the MIT `SC 2006 paper.
				// It works well because by doing k_intersect_u^2, we get a "pulling"
				// effect which helps "pull" things into the right class.
				// The first line is inverse square law, the .000001 is to avoid
				// divide by zero.
				// We don't bother squaring the distance as it is effectively
				// squared already.
				if(distance > 0) // If there is no distance, ignore it
				{
					radiance = 1.0 / distance;
					radiance = radiance * k_intersect_u * k_intersect_u;
				}
				else radiance = k_intersect_u * k_intersect_u;

				class_radiance[cls] += radiance;
			}
		}
	}

	// Renormalize radiance to probability
	for (cls = 0; cls < HSCategories.used; cls++)
	{
		// Avoid divide by zero and keep value small, for log10(remainder) below
		if (class_radiance[cls] < DBL_MIN)
			class_radiance[cls] = DBL_MIN;
		total_radiance += class_radiance[cls];
	}

	for (cls = 0; cls < HSCategories.used; cls++) // Order of instructions in this loop matters!
	{
		class_radiance[cls] = class_radiance[cls] / total_radiance; // fix-up probability
		if (class_radiance[cls] > class_radiance[bestseen]) bestseen = cls; // are we the best
		remainder += class_radiance[cls]; // add up remainder
	}
	remainder -= class_radiance[bestseen]; // fix-up remainder

	myReply.probability = class_radiance[bestseen];
	myReply.probScaled = 10 * (log10(class_radiance[bestseen]) - log10(remainder));
	myReply.name = HSCategories.categories[bestseen].name;

	// cleanup time!
	free(class_radiance);

return myReply;
}

HTMLClassification doHSPrepandClassify(HashList *toClassify)
{
uint32_t i, j;
uint32_t **categories = malloc(HSCategories.used * sizeof(uint32_t *));
int64_t BSRet = -1;
HTMLClassification data = { .name = NULL, .probability = 0.0, .probScaled = 0.0 };

	if(HSCategories.used < 1) return data;

	// alloc data for document hash match stats
	for(i = 0; i < HSCategories.used; i++)
	{
		categories[i] = calloc(HSCategories.categories[i].totalDocuments, sizeof(uint32_t));
	}

	// set the hash as having been seen on each category/document pair
	for(i=0; i < toClassify->used; i++)
	{
		if((BSRet = HSBinarySearch(&HSJudgeHashList, 0, HSJudgeHashList.used-1, toClassify->hashes[i]))>=0)
		{
//			printf("Found %"PRIX64"\n", toClassify->hashes[i]);
			for(j = 0; j < HSJudgeHashList.hashes[BSRet].used; j++)
			{
				categories[HSJudgeHashList.hashes[BSRet].users[j].category][HSJudgeHashList.hashes[BSRet].users[j].document]++;
			}
		}
	}
//	printf("Found %"PRIu16" out of %"PRIu16" items\n", z, toClassify->used);

	data = doHyperSpaceClassify(categories, toClassify);

	// cleanup
	for(i = 0; i < HSCategories.used; i++)
	{
		free(categories[i]);
	}
	free(categories);
	return data;
}
