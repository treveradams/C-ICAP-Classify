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


#define FBC_MAX_FEATURE_COUNT 500000
#define MAX_NAIVEBAYES_CATEGORY_NAME 100

#define FBC_FORMAT_VERSION 1
#define UNICODE_BYTE_MARK 0xFEFF

#define	MARKOV_C1	16	/* Markov C1 */
#define MARKIV_C2	1	/* Markov C2 */

typedef struct {
	char *name;
	double probability;
	double probScaled;
} FBCClassification;

typedef struct {
	double naiveBayesNum;
	double naiveBayesDen;
	double naiveBayesResult;
} FBCJudge;

// Fast Hyper Space File Format Version 1 is as follows
// Header
// BYTE 1 2 3 4 5 6 7 8 9
//      ID    Ver UBM Qty
//      F H S
// ID = 3 characters, Ver UINT16_T, UBM is Unicode byte mark se we know if we
// have the correct byte ordering (hash function is not endian neutral)
// Qty is UINT32_T, this is the number of records in the file
// Records
// BYTE 1 2 3 4 5 6 7 8 9 10 .... END
//      H H H H H H   0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// H is a UINT64_T
// END is a 128 bit all zero delimiter to allow for verifying the file
// END is currently not written nor checked

#define FBC_HEADERv1_ID_SIZE 3
#define FBC_HEADERv1_VERSION_SIZE sizeof(uint_least16_t)
#define FBC_HEADERv1_UBM_SIZE sizeof(uint_least32_t)
#define FBC_HEADERv1_RECORDS_QTY_SIZE sizeof(uint_least16_t)
#define FBC_v1_QTY_SIZE sizeof(uint_least16_t)
#define FBC_v1_HASH_SIZE sizeof(uint_least64_t)

#define FBC_HEADERv1_RECORDS_QTY_MAX UINT_LEAST32_MAX
#define FBC_v1_QTY_MAX UINT_LEAST16_MAX

typedef struct {
	char ID[3];
	uint_least16_t version;
	uint_least16_t UBM;
	uint_least32_t records;
} FBC_HEADERv1;

typedef struct {
	char *name;
	uint32_t totalFeatures;
} FBCTextCategory;

typedef struct {
	FBSTextCategory *categories;
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

typedef uint_least64_t FBCFeature;

typedef struct __attribute__ ((__packed__)) {
	FBCFeature hash;
	FBCHashJudgeUsers *users;
	uint_least16_t used;
} FBCFeature;

typedef struct  {
	FBCFeatureEx *hashes;
	uint32_t used;
	uint32_t slots;
} FBCHashListExt;

#ifdef IN_BAYES
void writeFBCHeader(int file, FBC_HEADERv1 *header);
int openFBC(char *filename, FBC_HEADERv1 *header, int forWriting);
void writeFBCHashes(int file, FBC_HEADERv1 *header, HashList *hashes_list);
int writeFBCHashesPreload(int file, FBC_HEADERv1 *header, HashListExt *hashes_list);
void computeHashes(regexHead *myHead, uint32_t primaryseed, uint32_t secondaryseed, HashList *hashes_list);
int preLoadFBC(char *fbc_name);
int loadFBCCategory(char *fbc_name, char *cat_name);
hsClassification doFBCPrepandClassify(HashList *toClassify);
void initBayesClassifier(void);
void deinitBayesClassifier(void);
#else
extern void writeFBCHeader(int file, FBC_HEADERv1 *header);
extern int openFBC(char *filename, FBC_HEADERv1 *header, int forWriting);
extern int writeFBCHashes(int file, FBC_HEADERv1 *header, HashList *hashes_list);
extern int writeFBCHashesPreload(int file, FBC_HEADERv1 *header, HashListExt *hashes_list);
extern int preLoadHyperSpace(char *fbc_name);
extern int loadFBCCategory(char *fbc_name, char *cat_name);
extern FBCClassification doFBCPrepandClassify(HashList *toClassify);
extern void initBayesClassifier(void);
extern void deinitBayesClassifier(void);
#endif

#define BAYES_CATEGORY_INC 10

#ifndef IN_BAYES
extern FBCTextCategoryExt NBCategories;
extern FBCHashListExt NBJudgeHashList;
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
#define ci_debug_printf(i, args...) printf(args);
#endif
#endif