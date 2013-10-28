/*
 *  Copyright (C) 2008-2013 Trever L. Adams
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

#define MAX_BAYES_CATEGORY_NAME 100

#define OLD_FBC_FORMAT_VERSION 1
#define FBC_FORMAT_VERSION 2
#define UNICODE_BYTE_MARK 0xFEFF

#define	MARKOV_C1	16	/* Markov C1 */
#define MARKOV_C2	1	/* Markov C2 */

typedef struct {
	double naiveBayesResult;
} FBCJudge;

// Fast Naive Bayes File Format Version 1 is as follows
// Header
// BYTE 1 2 3 4 5 6 7 8 9 10 11 12 13
//      ID    Ver UBM WCS  Qty
//      F N B
// ID = 3 characters, Ver UINT16_T, UBM is Unicode byte mark se we know if we
// have the correct byte ordering (hash function is not endian neutral)
// WCS is UINT16_T sizeof(wchar_t) -- Version 1 does not have this!
// Qty is UINT32_T, this is the number of records in the file
// Records
// BYTE 1 2 3 4 5 6 7 8 9 10 .... END
//      H COUNT H COUNT H COUNT H COUNT H COUNT H COUNT  0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// H is a UINT64_T
// COUNT is UINT32_T
// END is a 128 bit all zero delimiter to allow for verifying the file
// END is currently not written nor checked

#define FBC_HEADERv1_ID_SIZE 3
#define FBC_HEADERv1_VERSION_SIZE sizeof(uint_least16_t)
#define FBC_HEADERv1_UBM_SIZE sizeof(uint_least16_t)
#define FBC_HEADERv2_WCS_SIZE sizeof(uint_least16_t)
#define FBC_HEADERv1_RECORDS_QTY_SIZE sizeof(uint_least32_t)
typedef uint_least32_t FBC_v1_HASH_COUNT;
#define FBC_v1_HASH_SIZE sizeof(HTMLFeature)
#define FBC_v1_HASH_USE_COUNT_SIZE sizeof(FBC_v1_HASH_COUNT)

#define FBC_HEADERv1_RECORDS_QTY_MAX UINT_LEAST32_MAX
#define FBC_v1_QTY_MAX UINT_LEAST16_MAX

typedef struct {
	char ID[3];
	uint_least16_t version;
	uint_least16_t UBM;
	uint_least16_t WCS;
	uint_least32_t records;
} FBC_HEADERv1;

typedef struct {
	char *name;
	uint32_t totalFeatures;
} FBCTextCategory;

typedef struct {
	FBCTextCategory *categories;
	uint16_t used;
	uint16_t slots;
} FBCTextCategoryExt;

typedef struct __attribute__ ((__packed__)) {
	uint_least16_t category;
        union {
		float probability;
		uint32_t count;
	} data;
} FBCHashJudgeUsers;

typedef struct __attribute__ ((__packed__)) {
	HTMLFeature hash;
	FBCHashJudgeUsers *users;
	uint_least16_t used;
} FBCFeatureExt;

typedef struct  {
	FBCFeatureExt *hashes;
	uint32_t used;
	uint32_t slots;
	int FBC_LOCKED; // FBC_LOCKED is used to keep from loading more data or writing data if we are in an optimized state.
} FBCHashList;

#ifdef IN_BAYES
void writeFBCHeader(int file, FBC_HEADERv1 *header);
int openFBC(const char *filename, FBC_HEADERv1 *header, int forWriting);
int writeFBCHashes(int file, FBC_HEADERv1 *header, FBCHashList *hashes_list, uint16_t category, uint32_t zero_point);
int writeFBCHashesPreload(int file, FBC_HEADERv1 *header, FBCHashList *hashes_list);
void computeHashes(regexHead *myHead, uint32_t primaryseed, uint32_t secondaryseed, HashList *hashes_list);
int preLoadBayes(const char *fbc_name);
int loadBayesCategory(const char *fbc_name, const char *cat_name);
int learnHashesBayesCategory(uint16_t cat_num, HashList *docHashes);
HTMLClassification doBayesPrepandClassify(HashList *toClassify);
void initBayesClassifier(void);
void deinitBayesClassifier(void);
int isBayes(const char *filename);
int loadMassBayesCategories(const char *fbc_dir);
int optimizeFBC(FBCHashList *hashes);
#else
extern void writeFBCHeader(int file, FBC_HEADERv1 *header);
extern int openFBC(const char *filename, FBC_HEADERv1 *header, int forWriting);
int writeFBCHashes(int file, FBC_HEADERv1 *header, FBCHashList *hashes_list, uint16_t category, uint32_t zero_point);
extern int writeFBCHashesPreload(int file, FBC_HEADERv1 *header, HashList *hashes_list);
extern int preLoadBayes(const char *fbc_name);
extern int loadBayesCategory(const char *fbc_name, const char *cat_name);
extern int learnHashesBayesCategory(uint16_t cat_num, HashList *docHashes);
extern HTMLClassification doBayesPrepandClassify(HashList *toClassify);
extern void initBayesClassifier(void);
extern void deinitBayesClassifier(void);
extern int isBayes(const char *filename);
extern int loadMassBayesCategories(const char *fbc_dir);
extern int optimizeFBC(FBCHashList *hashes);
#endif

#define BAYES_CATEGORY_INC 10

#ifndef IN_BAYES
extern FBCTextCategoryExt NBCategories;
extern FBCHashList NBJudgeHashList;
#endif

#ifndef IN_BAYES
extern uint32_t HASHSEED1;
extern uint32_t HASHSEED2;
#endif

#ifndef NOT_CICAP
#include <c_icap/c-icap.h>
#include <c_icap/debug.h>
#endif

#ifndef __DEBUG_H
#define __DEBUG_H
#ifdef _MSC_VER
CI_DECLARE_DATA extern void (*__vlog_error)(void *req, const char *format, va_list ap);
CI_DECLARE_FUNC(void) __ldebug_printf(int i,const char *format, ...);
#define ci_debug_printf __ldebug_printf

#else
 extern void (*__log_error)(void *req, const char *format,... );
#define ci_debug_printf(i, args...) fprintf(stderr, args);
#endif
#endif
