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


#define HYPERSPACE_MAX_FEATURE_COUNT 500000
#define MAX_HYPSERSPACE_CATEGORY_NAME 100

#define HYPERSPACE_FORMAT_VERSION 1
#define UNICODE_BYTE_MARK 0xFEFF

typedef struct {
	char *name;
	double probability;
	double probScaled;
} bcClassification;

// Fast Hyper Space File Format Version 1 is as follows
// Header
// BYTE 1 2 3 4 5 6 7 8 9
//      ID    Ver UBM Qty
//      F H S
// ID = 3 characters, Ver UINT16_T, UBM is Unicode byte mark se we know if we
// have the correct byte ordering (hash function is not endian neutral)
// Qty is UINT16_T, this is the number of records in the file
// Records
// BYTE 1 2 3 4 5 6 7 8 9 10 .... END
//      Qty H HQTY H HQTY H HQTY H   0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Qty is the number of 64-bit hashes in the record w/ associated HQTY. 8 bytes per Hash
// HQTY is 32bit unsigned integer of how many of the hash were trained positive (negatives
// are the positives of all other categories).
// END is a 128 bit all zero delimiter to allow for verifying the file
// END is currently not written nor checked

#define FBC_HEADERv1_ID_SIZE 3
#define FBC_HEADERv1_VERSION_SIZE sizeof(uint_least16_t)
#define FBC_HEADERv1_UBM_SIZE sizeof(uint_least16_t)
#define FBC_HEADERv1_RECORDS_QTY_SIZE sizeof(uint_least16_t)
#define FBC_v1_QTY_SIZE sizeof(uint_least16_t)
#define FBC_v1_HASH_SIZE sizeof(uint_least64_t)

#define FBC_HEADERv1_RECORDS_QTY_MAX UINT_LEAST16_MAX
#define FBC_v1_QTY_MAX UINT_LEAST16_MAX

typedef struct {
	char ID[3];
	uint_least16_t version;
	uint_least16_t UBM;
	uint_least16_t records;
} FBC_HEADERv1;

typedef struct {
	char *name;
	uint16_t totalDocuments;
	uint32_t totalFeatures;
	uint16_t *documentKnownHashes; // documents[TextCategory.totalDocuments] with the value being the number of known hashes
} TextCategory;

typedef struct {
	TextCategory *categories;
	uint16_t used;
	uint16_t slots;
} TextCategoryExt;

typedef struct __attribute__ ((__packed__)) {
	double probability;
} bayesHashJudgeUsers;

typedef uint_least64_t bayesFeature;

typedef struct __attribute__ ((__packed__)) {
	bayesFeature hash;
	hashJudgeUsers *users;
	uint_least16_t used;
} bayesFeature;

typedef struct  {
	bayesFeatureEx *hashes;
	uint32_t used;
	uint32_t slots;
} bayesHashListExt;

#ifdef IN_HYPSERSPACE
void writeFBCHeader(int file, FBC_HEADERv1 *header);
int openFBC(char *filename, FBC_HEADERv1 *header, int forWriting);
void writeFBCHashes(int file, FBC_HEADERv1 *header, HashList *hashes_list);
int writeFBCHashesPreload(int file, FBC_HEADERv1 *header, HashListExt *hashes_list);
void computeHashes(regexHead *myHead, uint32_t primaryseed, uint32_t secondaryseed, HashList *hashes_list);
int preLoadHyperSpace(char *fbc_name);
int loadHyperSpaceCategory(char *fbc_name, char *cat_name);
hsClassification doHSPrepandClassify(HashList *toClassify);
void initHyperSpaceClassifier(void);
void deinitHyperSpaceClassifier(void);
#else
extern void writeFBCHeader(int file, FBC_HEADERv1 *header);
extern int openFBC(char *filename, FBC_HEADERv1 *header, int forWriting);
extern int writeFBCHashes(int file, FBC_HEADERv1 *header, HashList *hashes_list);
extern int writeFBCHashesPreload(int file, FBC_HEADERv1 *header, HashListExt *hashes_list);
extern int preLoadHyperSpace(char *fbc_name);
extern int loadHyperSpaceCategory(char *fbc_name, char *cat_name);
extern hsClassification doHSPrepandClassify(HashList *toClassify);
extern void initBayesClassifier(void);
extern void deinitBayesClassifier(void);
#endif

#define HYPERSPACE_CATEGORY_INC 10

#ifndef IN_HYPERSPACE
extern TextCategoryExt HSCategories;
extern HashListExt HSJudgeHashList;
#endif

#ifndef IN_HYPERSPACE
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
