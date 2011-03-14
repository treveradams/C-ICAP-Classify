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


TextCategoryExt HSCategories;
HashListExt HSJudgeHashList;

void initBayesClassifier(void)
{
	HSJudgeHashList.slots = 0;
	HSJudgeHashList.hashes = NULL;
	HSJudgeHashList.used = 0;
	HSCategories.slots = Bayes_CATEGORY_INC;
	HSCategories.categories = calloc(HSCategories.slots, sizeof(TextCategory));
	HSCategories.used = 0;
}

void deinitBayesClassifier(void)
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

static int hash_compare(void const *a, void const *b)
{
BayesFeature *ha, *hb;

	ha = (BayesFeature *) a;
	hb = (BayesFeature *) b;
	if(*ha < *hb)
		return -1;
	if(*ha > *hb)
		return 1;


return 0;
}

static int judgeHash_compare(void const *a, void const *b)
{
BayesFeatureExt *ha, *hb;

	ha = (BayesFeatureExt *) a;
	hb = (BayesFeatureExt *) b;
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
	if(memcmp(header->ID, "FBC", FBC_HEADERv1_ID_SIZE)!=0)
	{
//		printf("Not a FastBayesClassifer file\n");
		return -1;
	}
	read(fbc_file, &header->version, FBC_HEADERv1_VERSION_SIZE);
	if(header->version != BAYES_FORMAT_VERSION)
	{
//		printf("Wrong version of FastBayesClassifier file\n");
		return -2;
	}
	read(fbc_file, &header->UBM, FBC_HEADERv1_UBM_SIZE);
	if(header->UBM != UNICODE_BYTE_MARK)
	if(header->version != BAYES_FORMAT_VERSION)
	{
//		printf("FastBayesClassifier file of incompatible endianness\n");
		return -3;
	}
	if(read(fbc_file, &header->records, FBC_HEADERv1_RECORDS_QTY_SIZE)!=2)
	{
//		printf("FastBayesClassifier file has invalid header: no records count\n");
		return -4;
	}
	return 0;
}

void writeFBCHeader(int file, FBC_HEADERv1 *header)
{
int i;
	memcpy(&header->ID, "FBC", 3);
	header->version = BAYES_FORMAT_VERSION;
	header->UBM = UNICODE_BYTE_MARK;
	header->records=0;
	i = ftruncate(file, 0);
	lseek64(file, 0, SEEK_SET);
        do {
		i = write(file, "FBC", 3);
		if(i < 3) lseek64(file, -i, SEEK_CUR);
        } while (i >= 0 && i < 3);

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
		if(forWriting==1)
		{
			writeFBCHeader(file, header);
//			printf("Created FastBayesClassifier file: %s\n", filename);
		}
		else return -1;
	}
	return file;
}

int writeFBCHashes(int file, FBC_HEADERv1 *header, HashList *hashes_list)
{
uint16_t i;
int writecheck;
	lseek64(file, 0, SEEK_END);
	if(hashes_list->used) // check before we write
	{
		write(file, &hashes_list->used, sizeof(uint_least16_t));
		for(i=0; i < hashes_list->used; i++)
		{
			do {
				writecheck = write(file, &hashes_list->hashes[i], FBC_v1_HASH_SIZE);
                                if(writecheck < FBC_v1_HASH_SIZE) lseek64(file, -writecheck, SEEK_CUR);
			} while (writecheck >=0 && writecheck < FBC_v1_HASH_SIZE);
		}
		/* Ok, have written hashes, now save new count */
		header->records = header->records+1;
		lseek64(file, 7, SEEK_SET);
		write(file, &header->records, FBC_HEADERv1_RECORDS_QTY_SIZE);
		return 0;
	}
	return -1;
}

int writeFBCHashesPreload(int file, FBC_HEADERv1 *header, HashListExt *hashes_list)
{
uint16_t i;
int writecheck;
	lseek64(file, 0, SEEK_END);
	if(hashes_list->used) // check before we write
	{
		write(file, &hashes_list->used, sizeof(uint_least16_t));
		for(i=0; i < hashes_list->used; i++)
		{
			do {
				writecheck = write(file, &hashes_list->hashes[i].primary, FBC_v1_HASH_SIZE);
                                if(writecheck < FBC_v1_HASH_SIZE) lseek64(file, -writecheck, SEEK_CUR);
			} while (writecheck >=0 && writecheck < FBC_v1_HASH_SIZE);
		}
		/* Ok, have written hashes, now save new count */
		header->records = header->records+1;
		lseek64(file, 7, SEEK_SET);
		write(file, &header->records, FBC_HEADERv1_RECORDS_QTY_SIZE);
		return 0;
	}
	return -1;
}

void makeSortedUniqueHashes(HashList *hashes_list)
{
uint32_t i = 0, j = 0;
	qsort(hashes_list->hashes, hashes_list->used, sizeof(BayesFeature), &hash_compare );
//	printf("\nTotal non-unique features: %"PRIu32"\n", hashes_list->used);
	while(i < hashes_list->used)
	{
		if(hashes_list->hashes[i].primary != hashes_list->hashes[i+1].primary || hashes_list->hashes[i].secondary != hashes_list->hashes[i+1].secondary)
		{
			hashes_list->hashes[j] = hashes_list->hashes[i];
			j++;
		}
		i++;
	}
	hashes_list->used = j;
//	for(i=0; i<hashes_list->used; i++) printf("Hashed: %"PRIX32" %"PRIX32"\n", hashes_list->hashes[i].primary, hashes_list->hashes[i].secondary);
//	printf("Total unique features: %"PRIu32"\n", hashes_list->used);
}

static int64_t BCBinarySearch(HashListExt *hashes_list, int64_t start, int64_t end, uint32_t p_key, uint32_t s_key)
{
int64_t mid=0;
	if(start > end)
		return -1;
	mid = start + ((end - start) / 2);
//	printf("Start %"PRId64" end %"PRId64" mid %"PRId64"\n", start, end, mid);
//	printf("Keys @ mid: %"PRIX32" %"PRIX32" looking for %"PRIX32"\n", hashes_list->hashes[mid].hash.primary, hashes_list->hashes[mid].hash.secondary, p_key);
	if(hashes_list->hashes[mid].hash.primary > p_key)
		return HSBinarySearch(hashes_list, start, mid-1, p_key, s_key);
	else if(hashes_list->hashes[mid].hash.primary < p_key)
		return HSBinarySearch(hashes_list, mid+1, end, p_key, s_key);
	else
	{
		if(hashes_list->hashes[mid].hash.secondary > s_key)
			return HSBinarySearch(hashes_list, start, mid-1, p_key, s_key);
		if(hashes_list->hashes[mid].hash.secondary < s_key)
			return HSBinarySearch(hashes_list, mid+1, end, p_key, s_key);
		else return mid;
	}
	return -1;
}

uint32_t featuresInCategory(int fbc_file, FBC_HEADERv1 *header)
{
struct stat stat_buf;
	fstat(fbc_file, &stat_buf);
        return (stat_buf.st_size - (FBC_HEADERv1_ID_SIZE + FBC_HEADERv1_VERSION_SIZE + FBC_HEADERv1_UBM_SIZE +
		FBC_HEADERv1_RECORDS_QTY_SIZE) - (header->records * sizeof(FBC_v1_QTY_SIZE))) / (FBC_v1_HASH_SIZE) + 1;
}

bayesFeature *loadDocument(char *fbc_name, char *cat_name, int fbc_file, uint16_t numHashes)
{
bayesFeature *hashes=NULL;
int status = 0;
int bytes = 0;
int to_read = FBC_v1_HASH_SIZE * numHashes * 2;
        hashes = malloc(FBC_v1_HASH_SIZE * numHashes * 2);
	// printf("Going to read %"PRIu16" hashes from record %"PRIu16"\n", numHashes, i);
        do {
		status = read(fbc_file, hashes + bytes, to_read);
		if(status > 0)
		{
			bytes += status;
			to_read -= status;
		}

	} while(status > 0);
        if(bytes < sizeof(uint32_t) * numHashes * 2) printf("Corrupted fbc file: %s for cat_name: %s\n", fbc_name, cat_name);
return hashes;
}

void closeDocument(uint32_t *hashes)
{
	free(hashes);
}

// The following number is based on a lot of experimentation. It should be between 35-70. The larger the data set, the higher the number should be.
// 95 is ideal for my training set.
#define HS_OFFSET_MAX 95
int loadBayesCategory(char *fbc_name, char *cat_name)
{
int fbc_file;
uint16_t i, j, z, shortcut=0, offsetPos=2;
int64_t BSRet=-1;
int64_t offsets[HS_OFFSET_MAX+1];
bayesFeature *docHashes;
FBC_HEADERv1 header;
uint16_t numHashes=0;
uint32_t startHashes = HSJudgeHashList.used;
	offsets[0] = 0;
	if((fbc_file = openFBC(fbc_name, &header, 0)) < 0) return FBC_file;
	if(HSCategories.used == HSCategories.slots)
	{
		HSCategories.slots += BAYES_CATEGORY_INC;
		HSCategories.categories = realloc(HSCategories.categories, HSCategories.slots * sizeof(TextCategory));
	}
	HSCategories.categories[HSCategories.used].name = strndup(cat_name, MAX_HYPSERSPACE_CATEGORY_NAME);
	HSCategories.categories[HSCategories.used].totalDocuments = header.records;
	HSCategories.categories[HSCategories.used].totalFeatures = 0;
	HSCategories.categories[HSCategories.used].documentKnownHashes = malloc(header.records * sizeof(uint16_t));

	if(HSJudgeHashList.used + featuresInCategory(fbc_file, &header) >= HSJudgeHashList.slots)
	{
		HSJudgeHashList.slots += featuresInCategory(fbc_file, &header);
		HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(BayesFeatureExt));
	}

//	printf("Going to read %"PRIu16" records from %s\n", header.records, cat_name);
	offsets[1] = HSJudgeHashList.used;
	for(i = 0; i < header.records; i++)
	{
		if(read(fbc_file, &numHashes, 2)<2) ; // ERRORFIXME;
		docHashes = loadDocument(fbc_name, cat_name, fbc_file, numHashes);

		HSCategories.categories[HSCategories.used].documentKnownHashes[i] = numHashes;
		HSCategories.categories[HSCategories.used].totalFeatures += numHashes;
		if(HSJudgeHashList.used + numHashes > HSJudgeHashList.slots)
		{
			if(HSJudgeHashList.slots != 0) printf("Ooops, we shouldn't be allocating more memory here. (%s)\n", fbc_name);
			HSJudgeHashList.slots += numHashes;
			HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(BayesFeatureExt));
		}

		for(j = 0; j < numHashes; j++)
		{
//			printf("Loading keys: %"PRIX32", %"PRIX32" in Category: %s Document:%"PRIu16"\n", docHashes[j], docHashes[j*2+1], cat_name, i);
			if(i > 0 || offsets[0] != offsets[1])
			{
				shortcut = 0;
				for(z = 1; z < offsetPos; z++)
				{
					if((BSRet=HSBinarySearch(&HSJudgeHashList, offsets[z-1], offsets[z] - 1, docHashes[j], docHashes[j*2+1])) >= 0)
					{
						if (shortcut == 0)
						{
							HSJudgeHashList.hashes[BSRet].users = realloc(HSJudgeHashList.hashes[BSRet].users, (HSJudgeHashList.hashes[BSRet].used+1) * sizeof(hashJudgeUsers));
//							ci_debug_printf(10, "Found keys: %"PRIX32", %"PRIX32" already in table (offset %"PRIu16"), updating\n", docHashes[j], docHashes[j*2+1], z);
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
//				ci_debug_printf(10, "Didn't find keys: %"PRIX32", %"PRIX32" in table\n", docHashes[j], docHashes[j*2+1]);
				HSJudgeHashList.hashes[HSJudgeHashList.used].hash.primary = docHashes[j];
				HSJudgeHashList.hashes[HSJudgeHashList.used].hash.secondary = docHashes[j*2+1];
				HSJudgeHashList.hashes[HSJudgeHashList.used].used = 0;
				HSJudgeHashList.hashes[HSJudgeHashList.used].users = calloc(1, sizeof(hashJudgeUsers));
				HSJudgeHashList.hashes[HSJudgeHashList.used].users[HSJudgeHashList.hashes[HSJudgeHashList.used].used].category = HSCategories.used;
				HSJudgeHashList.hashes[HSJudgeHashList.used].users[HSJudgeHashList.hashes[HSJudgeHashList.used].used].document = i;
				HSJudgeHashList.hashes[HSJudgeHashList.used].used++;
				HSJudgeHashList.used++;
			}
		}
		closeDocument(docHashes);
		if(offsetPos > HS_OFFSET_MAX)
		{
			qsort(HSJudgeHashList.hashes, HSJudgeHashList.used, sizeof(BayesFeatureExt), &judgeHash_compare );
			offsetPos=2;
		}
		offsets[offsetPos] = HSJudgeHashList.used;
		if(offsets[offsetPos-1] != offsets[offsetPos]) offsetPos++;
	}
	if(startHashes != HSJudgeHashList.used) qsort(HSJudgeHashList.hashes, HSJudgeHashList.used, sizeof(BayesFeatureExt), &judgeHash_compare);
//	ci_debug_printf(10, "Categories: %"PRIu32" Hashes Used: %"PRIu32"\n", HSCategories.used, HSJudgeHashList.used);
	HSCategories.used++;
	// Fixup memory usage
	if(HSJudgeHashList.slots > HSJudgeHashList.used && HSJudgeHashList.used > 1)
	{
		HSJudgeHashList.slots = HSJudgeHashList.used;
		HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(BayesFeatureExt));
	}
	close(fbc_file);
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
uint16_t i, j;
uint_least32_t *docHashes;
FBC_HEADERv1 header;
uint16_t numHashes=0;

	if(HSJudgeHashList.used > 0)
	{
		ci_debug_printf(1, "TextPreload / preLoadBayes called with some hashes already loaded. ABORTING PRELOAD!\n");
		return -1;
	}
	if((fbc_file = openFBC(fbc_name, &header, 0)) < 0) return fbc_file;

	if(featuresInCategory(fbc_file, &header) >= HSJudgeHashList.slots)
	{
		HSJudgeHashList.slots += featuresInCategory(fbc_file, &header);
		HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(BayesFeatureExt));
	}

//	ci_debug_printf(10, "Going to read %"PRIu16" records from %s\n", header.records, fbc_name);
	for(i = 0; i < header.records; i++)
	{
		if(read(fbc_file, &numHashes, 2)<2) ; // ERRORFIXME;
		docHashes = loadDocument(fbc_name, fbc_name, fbc_file, numHashes);

		if(HSJudgeHashList.used + numHashes > HSJudgeHashList.slots)
		{
			if(HSJudgeHashList.slots != 0) ci_debug_printf(10, "Ooops, we shouldn't be allocating more memory here. (%s)\n", fbc_name);
			HSJudgeHashList.slots += numHashes;
			HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(BayesFeatureExt));
		}

		for(j = 0; j < numHashes; j++)
		{
//			ci_debug_printf(10, "Loading keys: %"PRIX32", %"PRIX32" in Category: %s Document:%"PRIu16"\n", docHashes[j], docHashes[j*2+1], cat_name, i);
			if(HSJudgeHashList.used == 0) goto ADD_HASH;
			switch(preload_hash_compare(HSJudgeHashList.hashes[HSJudgeHashList.used-1].hash.primary, HSJudgeHashList.hashes[HSJudgeHashList.used-1].hash.secondary,
							docHashes[j], docHashes[j*2+1]))
			{
				case -1:
					ADD_HASH:
					HSJudgeHashList.hashes[HSJudgeHashList.used].hash.primary = docHashes[j];
					HSJudgeHashList.hashes[HSJudgeHashList.used].hash.secondary = docHashes[j*2+1];
					HSJudgeHashList.hashes[HSJudgeHashList.used].used = 0;
					HSJudgeHashList.hashes[HSJudgeHashList.used].users = NULL;
					HSJudgeHashList.used++;
					break;
				case 0:
					ci_debug_printf(1, "Keys: %"PRIX32", %"PRIX32" already loaded. Preload file %s corrupted?!?!\n", docHashes[j], docHashes[j*2+1], fbc_name);
					break;
				case 1:
					ci_debug_printf(1, "Keys: %"PRIX32", %"PRIX32" out of order. Preload file %s is corrupted!!!\n" \
								"Aborting preload as is.\n", docHashes[j], docHashes[j*2+1], fbc_name);
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
		HSJudgeHashList.hashes = realloc(HSJudgeHashList.hashes, HSJudgeHashList.slots * sizeof(BayesFeatureExt));
	}
	close(fbc_file);
	return 0;
}

static hsClassification doBayesClassify(uint32_t **categories, HashList *unknown)
{
double total_probability = 0.0;
double remainder = 1000 * DBL_MIN;
double *class_probability = malloc(HSCategories.used * sizeof(double));

uint32_t bestseen=0;
hsClassification myReply;

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
float *class_radiance = malloc(HSCategories.used * sizeof(float));

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
				radiance = 1.0 / (distance + .FLT_MIN);
				radiance = radiance * k_intersect_u * k_intersect_u;

				class_radiance[cls] += radiance;
			}
		}
	}

	// Renormalize radiance to probability
	for (cls = 0; cls < HSCategories.used; cls++)
	{
		// Avoid divide by zero and keep value small, for log10(remainder) below
		class_probability[cls] = class_radiance[cls];
		if (class_probability[cls] < DBL_MIN * 100)
			class_probability[cls] = DBL_MIN * 100;
		total_probability += class_probability[cls];
	}

	for (cls = 0; cls < HSCategories.used; cls++) // Order of instructions in this loop matters!
	{
		class_probability[cls] = class_probability[cls] / total_probability; // fix-up probability
		if (class_probability[cls] > class_probability[bestseen]) bestseen = cls; // are we the best
		remainder += class_probability[cls]; // add up remainder
	}
	remainder -= class_probability[bestseen]; // fix-up remainder

	myReply.probability = class_probability[bestseen];
	myReply.probScaled = 10 * (log10(class_probability[bestseen]) - log10(remainder));
	myReply.name = HSCategories.categories[bestseen].name;

	// cleanup time!
	free(class_probability);
	free(class_radiance);

return myReply;
}

bayesClassification doHSPrepandClassify(HashList *toClassify)
{
uint32_t i, j;
uint32_t **categories = malloc(HSCategories.used * sizeof(uint32_t *));
int64_t BSRet=-1;
bayesClassification data;

	// alloc data for document hash match stats
	for(i=0; i < HSCategories.used; i++)
	{
		categories[i]=calloc(HSCategories.categories[i].totalDocuments, sizeof(uint32_t));
	}

	// set the hash as having been seen on each category/document pair
	for(i=0; i < toClassify->used; i++)
	{
		if((BSRet=HSBinarySearch(&HSJudgeHashList, 0, HSJudgeHashList.used-1, toClassify->hashes[i].primary, toClassify->hashes[i].secondary))>=0)
		{
//			printf("Found %"PRIX32" %"PRIX32"\n", toClassify->hashes[i].primary, toClassify->hashes[i].secondary);
			for(j=0; j < HSJudgeHashList.hashes[BSRet].used; j++)
			{
				categories[HSJudgeHashList.hashes[BSRet].users[j].category][HSJudgeHashList.hashes[BSRet].users[j].document]++;
			}
		}
	}
//	printf("Found %"PRIu16" out of %"PRIu16" items\n", z, toClassify->used);

	data=doBayesClassify(categories, toClassify);

	// cleanup
	for(i=0; i < HSCategories.used; i++)
	{
		free(categories[i]);
	}
	free(categories);
	return data;
}
