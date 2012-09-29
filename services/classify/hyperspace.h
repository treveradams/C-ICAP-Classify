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

#define MAX_HYPSERSPACE_CATEGORY_NAME 100

#define OLD_HYPERSPACE_FORMAT_VERSION 1
#define HYPERSPACE_FORMAT_VERSION 2
#define UNICODE_BYTE_MARK 0xFEFF

// Fast Hyper Space File Format Version 1 is as follows
// Header
// BYTE 1 2 3 4 5 6 7 8 9 10 11
//      ID    Ver UBM WCS Qty
//      F H S
// ID = 3 characters, Ver UINT16_T, UBM is Unicode byte mark se we know if we
// have the correct byte ordering (hash function is not endian neutral)
// WCS is UINT16_T sizeof(wchar_t) -- Version 1 does not have this!
// Qty is UINT16_T, this is the number of records in the file
// Records
// BYTE 1 2 3 4 5 6 7 8 9 10 .... END
//      Qty H H H H H H H H   0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Qty is the number of 64-bit hashes in the record. 8 bytes per Hash
// END is a 128 bit all zero delimiter to allow for verifying the file
// END is currently not written nor checked

#define FHS_HEADERv1_ID_SIZE 3
#define FHS_HEADERv1_VERSION_SIZE sizeof(uint_least16_t)
#define FHS_HEADERv1_UBM_SIZE sizeof(uint_least16_t)
#define FHS_HEADERv2_WCS_SIZE sizeof(uint_least16_t)
#define FHS_HEADERv1_RECORDS_QTY_SIZE sizeof(uint_least16_t)
#define FHS_HEADERv1_TOTAL_SIZE (FHS_HEADERv1_ID_SIZE + FHS_HEADERv1_VERSION_SIZE + FHS_HEADERv1_UBM_SIZE + FHS_HEADERv1_RECORDS_QTY_SIZE)
#define FHS_v1_QTY_SIZE sizeof(uint_least16_t)
#define FHS_v1_HASH_SIZE sizeof(uint_least64_t)

#define FHS_HEADERv1_RECORDS_QTY_MAX UINT_LEAST16_MAX
#define FHS_v1_QTY_MAX UINT_LEAST16_MAX

typedef struct {
	char ID[3];
	uint_least16_t version;
	uint_least16_t UBM;
	uint_least16_t WCS;
	uint_least16_t records;
} FHS_HEADERv1;

typedef struct {
	char *name;
	uint16_t totalDocuments;
	uint32_t totalFeatures;
	uint16_t *documentKnownHashes; // documents[TextCategory.totalDocuments] with the value being the number of known hashes
} FHSTextCategory;

typedef struct {
	FHSTextCategory *categories;
	uint16_t used;
	uint16_t slots;
} FHSTextCategoryExt;

typedef struct __attribute__ ((__packed__)) {
//	uint16_t category;
//	uint16_t document;
	uint_least16_t category;
	uint_least16_t document;
} FHSHashJudgeUsers;

typedef struct __attribute__ ((__packed__)) {
	HTMLFeature hash;
	FHSHashJudgeUsers *users;
//	uint16_t used;
	uint_least16_t used;
} hyperspaceFeatureExt;

typedef struct  {
	hyperspaceFeatureExt *hashes;
	uint32_t used;
	uint32_t slots;
} HashListExt;

#ifdef IN_HYPSERSPACE
void writeFHSHeader(int file, FHS_HEADERv1 *header);
int openFHS(const char *filename, FHS_HEADERv1 *header, int forWriting);
void writeFHSHashes(int file, FHS_HEADERv1 *header, HashList *hashes_list);
int writeFHSHashesPreload(int file, FHS_HEADERv1 *header, HashListExt *hashes_list);
int preLoadHyperSpace(const char *fhs_name);
int loadHyperSpaceCategory(const char *fhs_name, char *cat_name);
HTMLClassification doHSPrepandClassify(HashList *toClassify);
void initHyperSpaceClassifier(void);
void deinitHyperSpaceClassifier(void);
int isHyperSpace(const char *filename);
int loadMassHSCategories(const char *fhs_dir);
#else
extern void writeFHSHeader(int file, FHS_HEADERv1 *header);
extern int openFHS(const char *filename, FHS_HEADERv1 *header, int forWriting);
extern int writeFHSHashes(int file, FHS_HEADERv1 *header, HashList *hashes_list);
extern int writeFHSHashesPreload(int file, FHS_HEADERv1 *header, HashListExt *hashes_list);
extern int preLoadHyperSpace(const char *fhs_name);
extern int loadHyperSpaceCategory(const char *fhs_name, char *cat_name);
extern HTMLClassification doHSPrepandClassify(HashList *toClassify);
extern void initHyperSpaceClassifier(void);
extern void deinitHyperSpaceClassifier(void);
extern int isHyperSpace(const char *filename);
extern int loadMassHSCategories(const char *fhs_dir);
#endif

#define HYPERSPACE_CATEGORY_INC 10

#ifndef IN_HYPERSPACE
extern FHSTextCategoryExt HSCategories;
extern HashListExt HSJudgeHashList;
#endif

extern uint32_t HASHSEED1;
extern uint32_t HASHSEED2;

#ifndef NOT_CICAP
#include <c_icap/c-icap.h>
#include <c_icap/debug.h>
#endif

#ifndef __DEBUG_H
#define __DEBUG_H
#endif

#if defined(_MSC_VER) && defined(NOT_CICAP)
CI_DECLARE_DATA extern void (*__vlog_error)(void *req, const char *format, va_list ap);
CI_DECLARE_FUNC(void) __ldebug_printf(int i,const char *format, ...);
#define ci_debug_printf __ldebug_printf
#endif

#if !defined(_MSC_VER) && defined(NOT_CICAP)
 extern void (*__log_error)(void *req, const char *format,... );
#define ci_debug_printf(i, args...) fprintf(stderr, args);
#endif
