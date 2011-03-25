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


#include <tre/tre.h>

#define regexEDITS 250
#define regexAPPENDSIZE 512


// From hash.c
extern uint32_t HASHSEED1;
extern uint32_t HASHSEED2;

enum {UNKNOWN_ERROR=-9999, NO_CATEGORIES=-1, NO_MEMORY=-2, DOCUMENT_TOO_SMALL=-3, NO_CHARSET=-4, ZLIB_FAIL=-5};

typedef struct _myRegmatch_t {
	regoff_t rm_so;
	regoff_t rm_eo;
	wchar_t *data;
	int owns_memory;
	struct _myRegmatch_t *next;
} myRegmatch_t;

typedef struct _myRegmatchArray{
	myRegmatch_t matches[regexEDITS];
	int used;
	struct _myRegmatchArray *next;
} myRegmatchArray;

typedef struct {
	myRegmatch_t *head;
	myRegmatch_t *tail;
	int dirty;
	wchar_t *main_memory;
	myRegmatchArray *arrays;
	myRegmatchArray *lastarray;
} regexHead;

typedef struct {
	uint_least64_t *hashes;
	uint32_t used;
	uint32_t slots;
} HashList;

#ifdef IN_HTML
void normalizeCurrency(regexHead *myHead);
void removeHTML(regexHead *myHead);
void mkRegexHead(regexHead *head, wchar_t *myData);
void regexMakeSingleBlock(regexHead *myHead);
void freeRegexHead(regexHead *myHead);
static void compileRegexes(void);
static void freeRegexes(void);
#else
extern void normalizeCurrency(regexHead *myHead);
extern void removeHTML(regexHead *myHead);
extern void mkRegexHead(regexHead *head, wchar_t *myData);
extern void regexMakeSingleBlock(regexHead *myHead);
extern void freeRegexHead(regexHead *myHead);
extern regex_t headFinder, charsetFinder;
extern void initHTML(void);
extern void deinitHTML(void);
extern void computeOSBHashes(regexHead *myHead, uint32_t primaryseed, uint32_t secondaryseed, HashList *hashes_list);
#endif

extern void makeSortedUniqueHashes(HashList *hashes_list);


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
#define ci_debug_printf(i, args...) printf(args);
#endif
