/*
 *  Copyright (C) 2008-2017 Trever L. Adams
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

#define regexEDITS 375
#define regexAPPENDSIZE 512

#define HTML_MAX_FEATURE_COUNT 500000


// From hash.c
extern uint32_t HASHSEED1;
extern uint32_t HASHSEED2;

enum {UNKNOWN_ERROR=-9999, NO_CATEGORIES=-1, NO_MEMORY=-2, DOCUMENT_TOO_SMALL=-3, NO_CHARSET=-4, DECOMPRESS_FAIL=-5};

enum {CJK_NONE = 0, KATAKANA, HIRAGANA, CJK_BREAK=999};

typedef struct _myRadix_t {
    int64_t start;
    int64_t stop;
} myRadix_t;

typedef struct _myRegmatch_t {
    regoff_t rm_so;
    regoff_t rm_eo;
    wchar_t *data;
    int owns_memory;
    struct _myRegmatch_t *next;
} myRegmatch_t;

typedef struct _myRegmatchArray {
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
    int head_cicap_membuf;
} regexHead;

typedef uint_least64_t HTMLFeature;

typedef struct {
    HTMLFeature *hashes;
    uint32_t used;
    uint32_t slots;
} HashList;

typedef struct {
    char *primary_name;
    double primary_probability;
    double primary_probScaled;
    char *secondary_name;
    double secondary_probability;
    double secondary_probScaled;
} HTMLClassification;

typedef struct {
    regex_t primary_regex;
    regex_t secondary_regex;
    int bidirectional;
} secondaries_t;

#ifdef IN_HTML
void normalizeCurrency(regexHead *myHead);
void removeHTML(regexHead *myHead);
void mkRegexHead(regexHead *head, wchar_t *myData, int is_cicap_membuf);
void regexMakeSingleBlock(regexHead *myHead);
void freeRegexHead(regexHead *myHead);
static void compileRegexes(void);
static void freeRegexes(void);

typedef uint64_t PTKey;
typedef uint64_t PTItem;

typedef struct _PTnode {
    char bit;
    PTKey item;
    struct _PTnode *l, *r;
} PTnode;

typedef PTnode* PTlink;

typedef struct {
    PTlink head;

    PTnode **nodes;
    int32_t number_of_node_heads;
    int32_t number_of_nodes;
    int32_t last_used_node;
    char zero_found;
    HashList *hashes_list;
} PTsession;

#else
extern void normalizeCurrency(regexHead *myHead);
extern void removeHTML(regexHead *myHead);
extern void mkRegexHead(regexHead *head, wchar_t *myData, int is_cicap_membuf);
extern void regexMakeSingleBlock(regexHead *myHead);
extern void freeRegexHead(regexHead *myHead);
extern regex_t headFinder, charsetFinder;
extern void initHTML(void);
extern void deinitHTML(void);
extern void computeOSBHashes(regexHead *myHead, uint32_t primaryseed, uint32_t secondaryseed, HashList *hashes_list);
extern int HTMLhash_compare(void const *a, void const *b);
extern void makeSortedUniqueHashes(HashList *hashes_list);

extern secondaries_t *secondary_compares;
extern int number_secondaries;
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
#define ci_debug_printf(i, args...) fprintf(stderr, args);
#endif

#if ! defined(MARKUP_SIZEOFWCHAR)
#if __SIZEOF_WCHAR_T__ == 4 || __WCHAR_MAX__ > 0x10000
#define SIZEOFWCHAR 4
#else
#define SIZEOFWCHAR 2
#endif
#endif

// The following is ONLY to be used for allocating buffers for character
// conversion to wchar_t whether or not wchar_t is UTF-16 or UTF-32 we need to
// allocate 4 bytes per input character just in case.
// DO NOT USE THIS FOR ANYTHING ELSE
#define UTF32_CHAR_SIZE 4
