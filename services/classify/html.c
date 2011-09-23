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

#define IN_HTML 1
#include "currency.h"
#include "html.h"
#include "htmlentities.h"
#include "hash.h"

#if SIZEOFWCHAR == 2
const uint32_t LEAD_OFFSET = 0xD800 - (0x10000 >> 10); // For decimal and hexadecimal entity conversion to wchar_t
#endif

regex_t htmlFinder, superFinder, commentFinder, imageFinder, title1, title2, alt1, alt2;
regex_t metaFinder, metaDescription, metaKeyword, metaContent, currencyFinder;
regex_t headFinder, charsetFinder;
regex_t entityFinder, numericentityFinder;
regex_t insaneFinder;

secondaries_t *secondary_compares = NULL;
int number_secondaries = 0;

static int entity_compare(void const *a, void const *b)
{
_htmlentity *ea, *eb;
int ret;

	ea = (_htmlentity *) a;
	eb = (_htmlentity *) b;
	ret = wcscmp(ea->name, eb->name);
	if(ret < 0)
		return -1;
	if(ret > 0)
		return 1;

return 0; // Equal
}

int HTMLhash_compare(void const *a, void const *b)
{
HTMLFeature *ha, *hb;

	ha = (HTMLFeature *) a;
	hb = (HTMLFeature *) b;
	if(*ha < *hb)
		return -1;
	if(*ha > *hb)
		return 1;

return 0;
}

void makeSortedUniqueHashes(HashList *hashes_list)
{
uint32_t i = 1, j = 0;
	qsort(hashes_list->hashes, hashes_list->used, sizeof(HTMLFeature), &HTMLhash_compare );
//	printf("\nTotal non-unique features: %"PRIu32"\n", hashes_list->used);
	for(i = 1; i < hashes_list->used; i++)
	{
		if(hashes_list->hashes[i] != hashes_list->hashes[j])
		{
			hashes_list->hashes[j+1] = hashes_list->hashes[i];
			j++;
		}
	}
	hashes_list->used = j + 1; // j is last slot actually filled. hashes_list->used is next to be used or count of those used (same number)
//	for(i=0; i<hashes_list->used; i++) printf("Hashed: %"PRIX64"\n", hashes_list->hashes[i]);
//	printf("Total unique features: %"PRIu32"\n", hashes_list->used);
}

void initHTML(void)
{
	compileRegexes();
	qsort(htmlentities, sizeof(htmlentities) / sizeof(htmlentities[0]) - 1, sizeof(_htmlentity), &entity_compare );
}

void deinitHTML(void)
{
	for(int i = 0; i < number_secondaries; i++)
	{
		tre_regfree(&secondary_compares[i].primary_regex);
		tre_regfree(&secondary_compares[i].secondary_regex);
	}
	free(secondary_compares);
	secondary_compares = NULL;
	freeRegexes();
}

static void compileRegexes(void)
{
wchar_t myRegex[PATH_MAX+1];

//	swprintf(myRegex, PATH_MAX, L"([%ls])([[:space:]]|&nbsp;)*([[:digit:]]+)(\\.[[:digit:]]*)?", CURRENCY);
	swprintf(myRegex, PATH_MAX, L"([%ls])([[:space:]]*)([[:digit:]]+)(\\.[[:digit:]]*)?", CURRENCY); // As is, we don't need to get rid of nbsp as we have done so
	myRegex[PATH_MAX] = L'\0';
	tre_regwcomp(&currencyFinder, myRegex, REG_EXTENDED);

//	tre_regwcomp(&htmlFinder, L"(<[^>]*>([[:space:]]*))+", REG_EXTENDED);
	tre_regwcomp(&htmlFinder, L"((/?<P[^>]*>[[:space:]]*)|(</?BR[^>]*>[[:space:]]*))|((<[^=>]*([^=>]*=(('[^']*')|(\"[^\"]*\")))*[^>]*>)((</?P[^>]*>)|(</?BR[^>]*>)|[[:space:]]*))", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&insaneFinder, L"[^[:graph:][:space:]]+", REG_EXTENDED);
	tre_regwcomp(&entityFinder, L"&(#?[[:alnum:]]+);", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&numericentityFinder, L"^#x?([[:xdigit:]]+$)", REG_EXTENDED | REG_ICASE);
	// The following two commented were replaced by superFinder
	//    <[[:space:]]*script[^>]*>.*?<[[:space:]]*/script[^>]*>
	//    <[[:space:]]*style[^>]*>.*?<[[:space:]]*/style[^>]*>
	tre_regwcomp(&superFinder, L"<(([[:space:]]*script[^>]*>.*?<[[:space:]]*/script[^>]*)|([[:space:]]*style[^>]*.*?<[[:space:]]*/style[^>]*))>", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&commentFinder, L"<!--.*?-->", REG_EXTENDED);
//	tre_regwcomp(&imageFinder, L"<[[:space:]]*img ([^>]*)>", REG_EXTENDED | REG_ICASE); // [^>]*(\"(<[^>]*>)*\")*|('(<[^>]*>)*')*|
	tre_regwcomp(&imageFinder, L"<[[:space:]]*img[[:space:]]*([^=>]*([^=>]*=(('[^']*')|(\"[^\"]*\")))*[^>]*>)", REG_EXTENDED | REG_ICASE);
//	tre_regwcomp(&title1, L"title=\"([^\"]*)\"", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&title1, L"title=((\"[^\"]+\")|('[^']+'))", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&title2, L"title=([[:graph:]]*)", REG_EXTENDED | REG_ICASE);
//	tre_regwcomp(&title3, L"title='([^']*)'", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&alt1, L"alt=((\"[^\"]+\")|('[^']+'))", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&alt2, L"alt=([[:graph:]]*)", REG_EXTENDED | REG_ICASE);
//	tre_regwcomp(&alt3, L"alt='([^']*)'", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&metaFinder, L"<[[:space:]]*meta ([^>]*)>", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&metaDescription, L"description\"?(.*)", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&metaKeyword, L"keywords\"?(.*)", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&metaContent, L"content=\"?([^\"]*)\"?", REG_EXTENDED | REG_ICASE);

	// These are used on "plain text" files. Don't do wchar_t.
	tre_regcomp(&headFinder, "<head>(.*?)</head>", REG_EXTENDED | REG_ICASE);
	// HTML 5 <meta charset="..." />
	// OLD HTML <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1">
	tre_regcomp(&charsetFinder, "<meta [^>]*?charset=\"?([^\">]*?)\"", REG_EXTENDED | REG_ICASE);
}

static void freeRegexes(void)
{
	tre_regfree(&currencyFinder);

	tre_regfree(&htmlFinder);
	tre_regfree(&insaneFinder);
	tre_regfree(&entityFinder);
	tre_regfree(&numericentityFinder);
	tre_regfree(&superFinder);
	tre_regfree(&commentFinder);
	tre_regfree(&imageFinder);
	tre_regfree(&title1);
	tre_regfree(&title2);
	tre_regfree(&alt1);
	tre_regfree(&alt2);
	tre_regfree(&metaFinder);
	tre_regfree(&metaDescription);
	tre_regfree(&metaKeyword);
	tre_regfree(&metaContent);

	tre_regfree(&headFinder);
	tre_regfree(&charsetFinder);
}

static myRegmatch_t *getEmptyRegexBlock(regexHead *myHead)
{
myRegmatch_t *myRet;
	if(myHead->lastarray->used < regexEDITS)
		myRet = &myHead->lastarray->matches[myHead->lastarray->used];
	else {
//		printf("\n\nMaking new EmptyREgexBlock\n");
		myHead->lastarray->next = calloc(1, sizeof(myRegmatchArray));
		myHead->lastarray = myHead->lastarray->next;
		myRet = &myHead->lastarray->matches[myHead->lastarray->used];
	}
	myRet->owns_memory = 0;
	myRet->next = NULL;
	myRet->data = NULL;
	myHead->lastarray->used++;
	return myRet;
}

// And empty head passed here must have main_memory, arrays and head set to NULL!
// Old heads will have appropriate elements freed before setting up new data.
void mkRegexHead(regexHead *head, wchar_t *myData)
{
myRegmatchArray *arrays = calloc(1, sizeof(myRegmatchArray));
myRegmatch_t *data;
	if(head->arrays || head->main_memory || head->head) freeRegexHead(head);
	head->dirty = 0;
	head->main_memory = myData;
	head->arrays = arrays;
	head->lastarray = arrays;
	data = getEmptyRegexBlock(head);
	data->rm_so = 0;
	data->rm_eo = wcslen(myData);
	head->head = data; // assign data in
	head->tail = data;
}

static void regexRemove(regexHead *myHead, myRegmatch_t *startblock, regmatch_t *to_remove)
{
myRegmatch_t *current = myHead->head, *newmatch;
	while(current != NULL)
	{
		if(startblock == current) // Is this the block we are looking for?
		{
			if(current->data == NULL) // we are only processing internal data here.
			{
				if(current->rm_so <= to_remove->rm_so && current->rm_eo >= to_remove->rm_eo) // we found the start
				{
//					printf("Head Removing: %.*ls\n", to_remove->rm_eo - to_remove->rm_so, myHead->main_memory + to_remove->rm_so);
					newmatch = getEmptyRegexBlock(myHead);
					newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
					newmatch->rm_eo = current->rm_eo; // the new block ends where the old one used to
					current->rm_eo = to_remove->rm_so; // the old block ends where we started
					newmatch->next = current->next; // save old next
					current->next = newmatch; // insert new match
					if(newmatch->next == NULL) myHead->tail = newmatch;
					myHead->dirty = 1;
//					printf("After Head Remove Next Block (max 10): %.*ls\n", newmatch->rm_eo - newmatch->rm_so > 20 ? 20: newmatch->rm_eo - newmatch->rm_so, &myHead->main_memory[newmatch->rm_so]);
					return;
				}
				else printf("regexRemove: (To remove: %d - %d, Current: %d - %d,\n", to_remove->rm_so, to_remove->rm_eo, current->rm_so, current->rm_eo);
			}
			else if(current->data != NULL) // we process private memory blocks here.
			{
				if(current->rm_so <= to_remove->rm_so && current->rm_eo >= to_remove->rm_eo) // we found the start
				{
//					printf("Private Removing: %.*ls\n", to_remove->rm_eo - to_remove->rm_so, current->data + to_remove->rm_so);
					newmatch = getEmptyRegexBlock(myHead);
					newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
					newmatch->rm_eo = current->rm_eo; // the new block ends where the old one used to
					current->rm_eo = to_remove->rm_so; // the old block ends where we started
					newmatch->data = current->data;
					newmatch->next = current->next; // save old next
					current->next = newmatch; // insert new match
					if(newmatch->next == NULL) myHead->tail = newmatch;
					myHead->dirty = 1;
//					printf("After Private Remove Next Block (max 10): %.*ls\n", newmatch->rm_eo - newmatch->rm_so > 10 ? 10 : newmatch->rm_eo - newmatch->rm_so, newmatch->data + newmatch->rm_so);
					return;
				}
			}
		}
		current = current->next;
	}
	printf("regexRemove not handled. Ooops. (%s: %.*ls)\n", startblock->data ? "Private" : "Head", to_remove->rm_eo - to_remove->rm_so, startblock->data ? startblock->data + to_remove->rm_so : myHead->main_memory + to_remove->rm_so);
	if(to_remove->rm_eo - to_remove->rm_so == 1) printf("Character in unhandled regexRemove %"PRIX32"\n", *(myHead->main_memory + to_remove->rm_so));
}

static void regexReplace(regexHead *myHead, myRegmatch_t *startblock, regmatch_t *to_remove, wchar_t *replaceMe, int len, int pad)
{
myRegmatch_t *current = myHead->head, *newmatch, *newdata;
uint32_t myLen = 0;

	while(current != NULL)
	{
		if(startblock == current) // Is this the block we are looking for?
		{
			if(current->data == NULL) // we are only processing internal data here.
			{
				if(current->rm_so <= to_remove->rm_so && current->rm_eo >= to_remove->rm_eo) // we found the start
				{
					newmatch = getEmptyRegexBlock(myHead);
					newmatch->next = current->next; // save old data
					newmatch->rm_eo = current->rm_eo; // the new block ends where the old one used to
					if(len + 3 * pad < to_remove->rm_eo - to_remove->rm_so)
					{
						// The following line is replaced with several memmoves to avoid overlapping problems
						// myLen = swprintf(myHead->main_memory + to_remove->rm_so, len + 3, L"%ls%.*ls%ls", pad ? L" " : L"", len, replaceMe, pad ? L" " : L"");
						if(pad) {
							memmove(myHead->main_memory + to_remove->rm_so + 1, replaceMe, len * sizeof(wchar_t));
							myHead->main_memory[to_remove->rm_so] = L' ';
							myHead->main_memory[to_remove->rm_so + 1 + len] = L' ';
							myLen = len + 2;
						}
						else {
							memmove(myHead->main_memory + to_remove->rm_so, replaceMe, len * sizeof(wchar_t));
							myLen = len;
						}
						current->rm_eo = to_remove->rm_so + myLen;
						current->next = newmatch; // insert new match
						newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
//						printf("regexReplace Internal/Inserted: \"%.*ls\"\n", myLen, myHead->main_memory + to_remove->rm_so);
					}
					else {
						newdata = getEmptyRegexBlock(myHead);
						newdata->data = malloc((len + 3) * sizeof(wchar_t));
						newdata->rm_eo = swprintf(newdata->data, len+3, L"%ls%.*ls%ls", pad ? L" " : L"", len, replaceMe, pad ? L" " : L"");
						newdata->rm_so = 0;
						newdata->owns_memory = 1;
						current->rm_eo = to_remove->rm_so; // the old block ends where we started
						current->next = newdata; // insert new data
						newdata->next = newmatch; // insert new match
//						printf("regexReplace Internal/Inserted/Allocate: %.*ls\n", newdata->rm_eo, newdata->data);
					}
					newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
					if(newmatch->next == NULL) myHead->tail = newmatch;
					myHead->dirty = 1;
					current = NULL;
				}
			}
			else if(current->data != NULL) // we process private memory blocks here.
			{
				if(current->rm_so <= to_remove->rm_so && current->rm_eo >= to_remove->rm_eo) // we found the start
				{
					newmatch = getEmptyRegexBlock(myHead);
					newmatch->next = current->next; // save old data
					newmatch->rm_eo = current->rm_eo; // the new block ends where the old one used to
					newmatch->data = current->data; // This is a custom data block
					if(len + 3 * pad < to_remove->rm_eo - to_remove->rm_so)
					{
						// The following line is replaced with several memmoves to avoid overlapping problems
						// myLen = swprintf(current->data + to_remove->rm_so, len + 3, L"%ls%.*ls%ls", pad ? L" " : L"", len, replaceMe, pad ? L" " : L"");
						if(pad) {
							memmove(current->data + to_remove->rm_so + 1, replaceMe, len * sizeof(wchar_t));
							current->data[to_remove->rm_so] = L' ';
							current->data[to_remove->rm_so + 1 + len] = L' ';
							myLen = len + 2;
						}
						else {
							memmove(current->data + to_remove->rm_so, replaceMe, len * sizeof(wchar_t));
							myLen = len;
						}
						current->rm_eo = to_remove->rm_so + myLen;
						current->next = newmatch; // insert new match
//						printf("regexReplace Private/Inserted: \"%.*ls\"\n", myLen, current->data + to_remove->rm_so);
					}
					else {
						newdata = getEmptyRegexBlock(myHead);
						newdata->data = malloc((len + 3) * sizeof(wchar_t));
						newdata->rm_eo = swprintf(newdata->data, len+3, L"%ls%.*ls%ls", pad ? L" " : L"", len, replaceMe, pad ? L" " : L"");
						newdata->rm_so = 0;
						newdata->owns_memory = 1;
						current->rm_eo = to_remove->rm_so; // the old block ends where we started
						current->next = newdata; // insert new data
						newdata->next = newmatch; // insert new match
//						printf("regexReplace Private/Inserted/Allocate: \"%.*ls\"\n", newdata->rm_eo, newdata->data);
					}
					newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
					if(newmatch->next == NULL) myHead->tail = newmatch;
					myHead->dirty = 1;
					current = NULL;
				}
			}
		}
		if(current) current = current->next;
	}
	myHead->dirty = 1;
}

static void regexAppend(regexHead *myHead, wchar_t *appendMe, int len)
{
myRegmatch_t *newdata;
uint32_t offset;

	if(myHead->tail->data && (myHead->tail->rm_eo + len + 1) < regexAPPENDSIZE)
	{
		offset = myHead->tail->rm_eo;
		newdata = myHead->tail;
		newdata->data[offset] = L' ';
		memcpy(newdata->data + offset + 1, appendMe, len * sizeof(wchar_t));
		newdata->rm_eo = newdata->rm_eo + len + 1; // offset was newdata->rm_eo
//		printf("regexAppend: Old Appending: %.*ls now: %.*ls\n", len, appendMe, newdata->rm_eo + 1, newdata->data);
	}
	else {
		newdata = getEmptyRegexBlock(myHead);
		// the following 3 lines were removed for the subsequent 5 lines doing the same thing
		//	newdata->rm_eo=swprintf(myAPPEND, PATH_MAX, L" %.*ls", len, appendMe);
		//	myAPPEND[PATH_MAX]='\0';
		//	newdata->data=wcsdup(myAPPEND);
		newdata->rm_eo = len + 1;
		if(len + 1 < regexAPPENDSIZE) newdata->data = calloc(1, regexAPPENDSIZE * sizeof(wchar_t));
		else newdata->data = malloc((len + 1) * sizeof(wchar_t));
		newdata->data[0] = L' ';
		//	wcsncpy(newdata->data+1, appendMe, len);
		memcpy(newdata->data + 1, appendMe, len * sizeof(wchar_t));
//		printf("regexAppend: New Appending: %.*ls\n", len, appendMe);
		newdata->rm_so = 0;
		newdata->owns_memory = 1;
		myHead->tail->next = newdata;
		myHead->tail = newdata;
	}
	myHead->dirty = 1;
}

static void freeRegmatchArrays(myRegmatchArray *to_free)
{
	if(to_free == NULL) return;
	freeRegmatchArrays(to_free->next);
	free(to_free);
}

void regexMakeSingleBlock(regexHead *myHead)
{
myRegmatch_t *current = myHead->head;
wchar_t *old_main = myHead->main_memory;
unsigned long offset = 0;
unsigned long total = 0;

	if(!myHead->dirty) return;
	while(current != NULL)
	{
		total += current->rm_eo - current->rm_so;
		current = current->next;
	}

	myHead->main_memory = malloc((total + 1) * sizeof(wchar_t));
	current = myHead->head;
	while(current != NULL) // Copy memory - Free memory next
	{
		memcpy(myHead->main_memory + offset, (current->data == NULL ? old_main : current->data) + current->rm_so, (current->rm_eo - current->rm_so) * sizeof(wchar_t));
		offset += current->rm_eo - current->rm_so;
		current = current->next;
	}
	free(old_main);

	current = myHead->head;
	while(current != NULL) // Free memory - Must be done separately as we now have multiple users of each private area
	{
		if(current->data && current->owns_memory) free(current->data);
		current = current->next;
	}

	freeRegmatchArrays(myHead->arrays);

	myHead->arrays = calloc(1, sizeof(myRegmatchArray));
	myHead->lastarray = myHead->arrays;
	myHead->head = getEmptyRegexBlock(myHead);
	myHead->head->rm_eo = offset;
	myHead->head->rm_so = 0;
	myHead->dirty = 0;
	myHead->tail = myHead->head;
}

void freeRegexHead(regexHead *myHead)
{
myRegmatch_t *current = myHead->head;

	while(current != NULL)
	{
		if(current->data && current->owns_memory) free(current->data);
		current = current->next;
	}
	if(myHead->arrays) freeRegmatchArrays(myHead->arrays);
	if(myHead->main_memory) free(myHead->main_memory);
}

void normalizeCurrency(regexHead *myHead)
{
wchar_t *XS = L"XXXXXXXXXXXXXXXXXXXX";
regoff_t currentOffset = 0;
wchar_t replace[101];
wchar_t *myData = NULL;
regmatch_t currencyMatch[5];
myRegmatch_t *current = myHead->head;
int len;

	current = myHead->head;
	while(current != NULL)
	{
		myData = (wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(current->rm_eo > currentOffset && tre_regwnexec(&currencyFinder, myData + currentOffset, current->rm_eo - currentOffset, 5, currencyMatch, 0) != REG_NOMATCH)
		{
			currencyMatch[0].rm_so += currentOffset;
			currencyMatch[0].rm_eo += currentOffset;
			currencyMatch[1].rm_so += currentOffset;
			currencyMatch[1].rm_eo += currentOffset;
			currencyMatch[3].rm_so += currentOffset;
			currencyMatch[3].rm_eo += currentOffset;
			currencyMatch[4].rm_so += currentOffset;
			currencyMatch[4].rm_eo += currentOffset;
			len = swprintf(replace, 101, L"$ %.*ls%ls%.*ls", currencyMatch[3].rm_eo - currencyMatch[3].rm_so, XS, (currencyMatch[4].rm_eo - currencyMatch[4].rm_so > 0 ? L"." : L""),
				(currencyMatch[4].rm_eo - currencyMatch[4].rm_so > 0 ? (currencyMatch[4].rm_eo - currencyMatch[4].rm_so) - 1 : 0), XS);
//			printf("Currency %.*ls to %.*ls\n", currencyMatch[0].rm_eo - currencyMatch[0].rm_so, myData + currencyMatch[0].rm_so, len, replace);
			regexReplace(myHead, current, &currencyMatch[0], replace, len, 0);
			currentOffset = currencyMatch[0].rm_eo;
		}
		current = current->next;
	}
}

int32_t findEntityBS(int32_t start, int32_t end, wchar_t *key, int len)
{
int32_t mid=0;
int ret;
	if(start > end)
		return -1;
	mid = start + ((end - start) / 2);
//	printf("Start %"PRId64" end %"PRId64" mid %"PRId64"\n", start, end, mid);
//	printf("Keys @ mid: %ls looking for %.*ls\n", htmlentities[mid].name, len, key);
	ret = wcsncmp(htmlentities[mid].name, key, len);
	if(ret == 0 && wcslen(htmlentities[mid].name) > len) ret = 1;
	if(ret > 0) return findEntityBS(start, mid-1, key, len);
	if(ret < 0) return findEntityBS(mid+1, end, key, len);
	if(ret == 0) return mid;

	return -1; // This should never be reached
}


void removeHTML(regexHead *myHead)
{
wchar_t *myData = NULL;
regoff_t currentOffset = 0;
regmatch_t singleMatch[11], doubleMatch[4], tripleMatch[2];
myRegmatch_t *current = myHead->head;
wchar_t unicode_entity[4];
int32_t entity;
wchar_t *unicode_end;
int metacount;
int xi = 0;
#if SIZEOFWCHAR < 4
uint32_t tempUTF32CHAR;
#endif

	current = myHead->head;
	while(current != NULL) // scripts, styles -- each used to be a block identical to this with their own regex and a slightly different printf statement
	{
		myData= (wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(current->rm_eo > currentOffset && tre_regwnexec(&superFinder, myData + currentOffset, current->rm_eo - currentOffset, 1, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
//			printf("Killing Script/Style Tag: %.*ls\n", singleMatch[0].rm_eo-singleMatch[0].rm_so, myData+singleMatch[0].rm_so);
			regexRemove(myHead, current, &singleMatch[0]);
			currentOffset = singleMatch[0].rm_eo;
		}
		current=current->next;
	}

	current = myHead->head;
	while(current != NULL) // scripts, styles -- each used to be a block identical to this with their own regex and a slightly different printf statement
	{
		myData = (wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(current->rm_eo > currentOffset && tre_regwnexec(&commentFinder, myData + currentOffset, current->rm_eo - currentOffset, 1, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
//			printf("Killing Comment Tag: %.*ls\n", singleMatch[0].rm_eo-singleMatch[0].rm_so, myData+singleMatch[0].rm_so);
			regexRemove(myHead, current, &singleMatch[0]);
			currentOffset = singleMatch[0].rm_eo;
		}
		current = current->next;
	}

	current = myHead->head;
	while(current != NULL) // metas
	{
		myData = (wchar_t *)(current->data==NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(current->rm_eo > currentOffset && tre_regwnexec(&metaFinder, myData + currentOffset, current->rm_eo - currentOffset, 2, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
			singleMatch[1].rm_so += currentOffset;
			singleMatch[1].rm_eo += currentOffset;
//			printf("Meta found: %.*ls\n", singleMatch[0].rm_eo - singleMatch[0].rm_so, myData+singleMatch[0].rm_so);
			if (tre_regwnexec(&metaDescription, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				if (tre_regwnexec(&metaContent, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, tripleMatch, 0) != REG_NOMATCH)
				{
					tripleMatch[0].rm_so += singleMatch[1].rm_so;
					tripleMatch[0].rm_eo += singleMatch[1].rm_so;
					tripleMatch[1].rm_so += singleMatch[1].rm_so;
					tripleMatch[1].rm_eo += singleMatch[1].rm_so;
//					printf("Saving Meta Description: %.*ls\n", tripleMatch[1].rm_eo-tripleMatch[1].rm_so, myData+tripleMatch[1].rm_so);
					regexReplace(myHead, current, &singleMatch[0], myData + tripleMatch[1].rm_so, tripleMatch[1].rm_eo - tripleMatch[1].rm_so, 1);
				}
			}
			else if (tre_regwnexec(&metaKeyword, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				if (tre_regwnexec(&metaContent, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, tripleMatch, 0) != REG_NOMATCH)
				{
					tripleMatch[0].rm_so += singleMatch[1].rm_so;
					tripleMatch[0].rm_eo += singleMatch[1].rm_so;
					tripleMatch[1].rm_so += singleMatch[1].rm_so;
					tripleMatch[1].rm_eo += singleMatch[1].rm_so;
					for(metacount = 0; metacount < tripleMatch[1].rm_eo - tripleMatch[1].rm_so; metacount++)
						if(*(myData + tripleMatch[1].rm_so + metacount) == L',') *(myData + tripleMatch[1].rm_so + metacount) = L' ';
//					printf("Saving Meta Keywords: %.*ls\n", tripleMatch[1].rm_eo-tripleMatch[1].rm_so, myData+tripleMatch[1].rm_so);
					regexReplace(myHead, current, &singleMatch[0], myData + tripleMatch[1].rm_so, tripleMatch[1].rm_eo - tripleMatch[1].rm_so, 1);
				}
			}
			else {
//				printf("Killing Meta Tag: %.*ls\n", singleMatch[0].rm_eo-singleMatch[0].rm_so, myData+singleMatch[0].rm_so);
				regexRemove(myHead, current, &singleMatch[0]);
			}
			currentOffset=singleMatch[0].rm_eo;
		}
		current = current->next;
	}

	current = myHead->head;
	while(current != NULL) // images
	{
		myData = (wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(current->rm_eo > currentOffset && tre_regwnexec(&imageFinder, myData + currentOffset, current->rm_eo - currentOffset, 2, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
			singleMatch[1].rm_so += currentOffset;
			singleMatch[1].rm_eo += currentOffset;
			if (tre_regwnexec(&title1, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 4, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so + 1, doubleMatch[1].rm_eo - doubleMatch[1].rm_so - 2);
//				printf("Title found: %.*ls\n", doubleMatch[1].rm_eo - doubleMatch[1].rm_so - 2, myData + doubleMatch[1].rm_so + 1);
			}
			else if (tre_regwnexec(&title2, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so, doubleMatch[1].rm_eo - doubleMatch[1].rm_so);
			}
			if (tre_regwnexec(&alt1, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 3, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so + 1, doubleMatch[1].rm_eo - doubleMatch[1].rm_so - 2);
//				printf("Alt found: %.*ls\n", doubleMatch[1].rm_eo - doubleMatch[1].rm_so, myData + doubleMatch[1].rm_so);
			}
			else if	(tre_regwnexec(&alt2, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so, doubleMatch[1].rm_eo - doubleMatch[1].rm_so);
			}
//			printf("Killing Image Tag: %.*ls\n", singleMatch[0].rm_eo - singleMatch[0].rm_so, myData + singleMatch[0].rm_so);
//			printf("Image Data: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so);
			regexRemove(myHead, current, &singleMatch[0]);
			currentOffset = singleMatch[0].rm_eo;
		}
		current=current->next;
	}

	current = myHead->head;
	while(current != NULL) // kill all unused tags
	{
		myData = (wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(current->rm_eo > currentOffset && tre_regwnexec(&htmlFinder, myData + currentOffset, current->rm_eo-currentOffset, 11, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
//			printf("Killing Uncared Tag: %.*ls\n", singleMatch[0].rm_eo - singleMatch[0].rm_so, myData + singleMatch[0].rm_so);
			if(tre_regwnexec(&title1, myData + singleMatch[0].rm_so, singleMatch[0].rm_eo - singleMatch[0].rm_so, 4, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[0].rm_so;
				doubleMatch[0].rm_eo += singleMatch[0].rm_so;
				doubleMatch[1].rm_so += singleMatch[0].rm_so;
				doubleMatch[1].rm_eo += singleMatch[0].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so + 1, doubleMatch[1].rm_eo - doubleMatch[1].rm_so - 2);
//				printf("Title found: %.*ls\n", doubleMatch[1].rm_eo - doubleMatch[1].rm_so - 2, myData + doubleMatch[1].rm_so + 1);
			}
			if(singleMatch[1].rm_so != -1 && singleMatch[1].rm_eo - singleMatch[1].rm_so > 0)
			{
				singleMatch[1].rm_so += currentOffset;
				singleMatch[1].rm_eo += currentOffset;
//				printf("Space was: '%.*ls'\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so);
				regexReplace(myHead, current, &singleMatch[0], L" ", 1, 0);
			}
			else if(singleMatch[10].rm_so != -1 && singleMatch[10].rm_eo - singleMatch[10].rm_so > 0)
			{
				singleMatch[10].rm_so += currentOffset;
				singleMatch[10].rm_eo += currentOffset;
//				printf("Space was: '%.*ls'\n", singleMatch[10].rm_eo - singleMatch[10].rm_so, myData + singleMatch[10].rm_so);
				regexReplace(myHead, current, &singleMatch[0], L" ", 1, 0);
			}
			else regexRemove(myHead, current, &singleMatch[0]);
			currentOffset = singleMatch[0].rm_eo;
		}
		current = current->next;
	}

	current = myHead->head;
	// http://www.w3.org/TR/html5/named-character-references.html
	// Only add those that are found in training sets, not the wild
	while(current != NULL) // HTML Entity removals -- MUST BE LAST
	{
		myData=(wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset=current->rm_so;
		while(current->rm_eo > currentOffset && tre_regwnexec(&entityFinder, myData + currentOffset, current->rm_eo - currentOffset, 2, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
			singleMatch[1].rm_so += currentOffset;
			singleMatch[1].rm_eo += currentOffset;

			if (tre_regwnexec(&numericentityFinder, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				unicode_end = myData + doubleMatch[1].rm_eo;
				if(myData[doubleMatch[0].rm_so + 1] == L'X' || myData[doubleMatch[0].rm_so + 1] == L'x') { // Check for hex
#if SIZEOFWCHAR == 4
					unicode_entity[0] = wcstoul(myData+doubleMatch[1].rm_so, &unicode_end, 16);
	                                unicode_entity[1] = L'\0';
#else
					// This algorithm is from http://unicode.org/faq/utf_bom.html#utf16-4 adjusted for endianness
					tempUTF32CHAR = wcstoul(myData + doubleMatch[1].rm_so, &unicode_end, 16);
					if(tempUTF32CHAR < 0xD7FF || (tempUTF32CHAR > 0xE000 && tempUTF32CHAR < 0xFFFF)) // Single UTF-16 character
					{
						unicode_entity[0] = tempUTF32CHAR;
						unicode_entity[1] = L'\0'
					}
					else {
						unicode_entity[0] = LEAD_OFFSET + (tempUTF32CHAR >> 10);
						unicode_entity[1] = 0xDC00 + (tempUTF32CHAR & 0x3FF);
						unicode_entity[2] = L'\0'
					}
#endif
//					ci_debug_printf(10,"Converting Hexadecimal HTML Entity: %.*ls to %ls\n", doubleMatch[1].rm_eo - doubleMatch[1].rm_so, myData + doubleMatch[1].rm_so, unicode_entity);
				}
				else {
#if SIZEOFWCHAR == 4
					unicode_entity[0] = wcstoul(myData + doubleMatch[1].rm_so, &unicode_end, 10);
	                                unicode_entity[1] = L'\0';
#else
					tempUTF32CHAR = wcstoul(myData + doubleMatch[1].rm_so, &unicode_end, 10);
					if(tempUTF32CHAR < 0xD7FF || (tempUTF32CHAR > 0xE000 && tempUTF32CHAR < 0xFFFF)) // Single UTF-16 character
					{
						unicode_entity[0] = tempUTF32CHAR;
						unicode_entity[1] = L'\0'
					}
					else {
						unicode_entity[0] = LEAD_OFFSET + (tempUTF32CHAR >> 10);
						unicode_entity[1] = 0xDC00 + (tempUTF32CHAR & 0x3FF);
						unicode_entity[2] = L'\0'
					}
#endif
//					ci_debug_printf(10, "Converting Decimal HTML Entity: %.*ls to %ls\n", doubleMatch[1].rm_eo - doubleMatch[1].rm_so, myData + doubleMatch[1].rm_so, unicode_entity);
				}
				regexReplace(myHead, current, &singleMatch[0], unicode_entity, wcslen(unicode_entity), 0); // We are replacing characters, do not add padding!
			}

			else if((entity=findEntityBS(0, sizeof( htmlentities ) / sizeof( htmlentities[0] ) - 1, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so)) >= 0)
			{
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %ls with length %d\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, htmlentities[entity].value, wcslen(htmlentities[entity].value));
				regexReplace(myHead, current, &singleMatch[0], htmlentities[entity].value, wcslen(htmlentities[entity].value), 0); // We are replacing characters, do not add padding!
			}

			else
			{
				ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
				currentOffset++;
			}
		}
		current=current->next;
	}

	current = myHead->head;
	while(current != NULL) // Remove hidden characters since they may cause problems with classification
	{
		myData=(wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(currentOffset < current->rm_eo && tre_regwnexec(&insaneFinder, myData + currentOffset, current->rm_eo - currentOffset, 1, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
			for(xi = 0; xi < singleMatch[0].rm_eo - singleMatch[0].rm_so; xi++)
			{
				ci_debug_printf(10, "Insane character: %"PRIX32"\n", myData[singleMatch[0].rm_so + xi]);
			}
			regexRemove(myHead, current, &singleMatch[0]);
			currentOffset = singleMatch[0].rm_eo;
		}
		current=current->next;
	}

	current = myHead->head;
	while(current != NULL) // Make everything lower case -- increase accuracy and lower key count
	{
		myData=(wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		for(; currentOffset < current->rm_eo; currentOffset++)
		{
/*			if(iswupper(myData[currentOffset]))
			{
				printf("Changing %lc", myData[currentOffset]);*/
			myData[currentOffset] = towlower(myData[currentOffset]);
/*				printf(" to %lc\n", myData[currentOffset]);
			}*/
		}
		current=current->next;
	}
}



void computeOSBHashes(regexHead *myHead, uint32_t primaryseed, uint32_t secondaryseed, HashList *hashes_list)
{
wchar_t *myData = NULL;
regoff_t currentOffset = 0, morematches = 0;
myRegmatch_t *current = myHead->head;
regmatch_t matches[5];
uint32_t i, j, pos, modPos;
wchar_t *placeHolder = L"***";
uint32_t prime1, prime2;
uint32_t finalA, finalB;
int foundCJK = 0;

	current = myHead->head;
	myData = (wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
	currentOffset = current->rm_so;
	for(i = 0; i < 5 && currentOffset < current->rm_eo; i++)
	{
		// find words, skip non-graphical characthers (similar to regex([[:graph:]]+)
		while (currentOffset < current->rm_eo && !iswgraph(myData[currentOffset]))
			currentOffset++;
		matches[i].rm_so = currentOffset;
		foundCJK = 0;
		while (currentOffset < current->rm_eo && iswgraph(myData[currentOffset]) && foundCJK == 0)
		{
			if((myData[currentOffset] >= 0x00002E80 && myData[currentOffset] <= 0x00002EFF) ||
			(myData[currentOffset] >= 0x00002FF0 && myData[currentOffset] <= 0x00002FFF) ||
			(myData[currentOffset] >= 0x00003001 && myData[currentOffset] <= 0x0000303F) || // 0x00003000 is a space character, don't mask it out.
			(myData[currentOffset] >= 0x000031C0 && myData[currentOffset] <= 0x000031EF) ||
			(myData[currentOffset] >= 0x00003400 && myData[currentOffset] <= 0x00004DBF) ||
			(myData[currentOffset] >= 0x00004E00 && myData[currentOffset] <= 0x00009FFF) ||
//			(myData[currentOffset] >= 0x0000AC00 && myData[currentOffset] <= 0x0000D7AF) || // These are syllables, they still are separated with spaces
			(myData[currentOffset] >= 0x0000F900 && myData[currentOffset] <= 0x0000FAFF) ||
			(myData[currentOffset] >= 0x00020000 && myData[currentOffset] <= 0x0002A6DF))
			{
//				printf("Found CJK %"PRIX32" @ %"PRIu32"\n", myData[currentOffset], currentOffset);
				if(matches[i].rm_so == currentOffset)
					currentOffset++;
				foundCJK++;
			}
			else currentOffset++;
		}
		matches[i].rm_eo = currentOffset;
		if(matches[i].rm_so == matches[i].rm_eo) currentOffset++;
//		printf("New Word: %.*ls @ %"PRIu32" with length %"PRIu32"\n", matches[i].rm_eo - matches[i].rm_so, myData+matches[i].rm_so, matches[i].rm_so, matches[i].rm_eo - matches[i].rm_so);
	}
	if(i < 5) return;
	prime1 = HASHSEED1;
	prime2 = HASHSEED2;
	lookup3_hashfunction((uint32_t *) myData+matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so, &prime1, &prime2);
	pos = 0;
	do {
		for(i = 1; i < 5; i++)
		{
			finalA = prime1;
			finalB = prime2;
			if(i > 1) lookup3_hashfunction((uint32_t *) placeHolder, i - 1, &finalA, &finalB);
			modPos = (pos + i) % 5;
			lookup3_hashfunction((uint32_t *) myData+matches[modPos].rm_so, matches[modPos].rm_eo - matches[modPos].rm_so, &finalA, &finalB);
			hashes_list->hashes[hashes_list->used] = (uint_least64_t) finalA << 32;
			hashes_list->hashes[hashes_list->used] |= (uint_least64_t) (finalB & 0xFFFFFFFF);
/*			printf("Hashed: %"PRIX64" (%.*ls %.*ls %.*ls)\n", hashes_list->hashes[hashes_list->used],
				matches[pos].rm_eo - matches[pos].rm_so, myData+matches[pos].rm_so,
				(i>1 ? i-1 : 0), placeHolder,
				matches[modPos].rm_eo - matches[modPos].rm_so, myData+matches[modPos].rm_so);*/
			hashes_list->used++;
		}
		// skip non-graphical characters ([[:graph:]]+)
		while (currentOffset < current->rm_eo && !iswgraph(myData[currentOffset]))
			currentOffset++;
		matches[pos].rm_so = currentOffset;
		foundCJK = 0;
		while (currentOffset < current->rm_eo && iswgraph(myData[currentOffset]) && foundCJK == 0)
		{
			if((myData[currentOffset] >= 0x00002E80 && myData[currentOffset] <= 0x00002EFF) ||
			(myData[currentOffset] >= 0x00002FF0 && myData[currentOffset] <= 0x00002FFF) ||
			(myData[currentOffset] >= 0x00003001 && myData[currentOffset] <= 0x0000303F) || // 0x00003000 is a space
			(myData[currentOffset] >= 0x000031C0 && myData[currentOffset] <= 0x000031EF) ||
			(myData[currentOffset] >= 0x00003400 && myData[currentOffset] <= 0x00004DBF) ||
			(myData[currentOffset] >= 0x00004E00 && myData[currentOffset] <= 0x00009FFF) ||
//			(myData[currentOffset] >= 0x0000AC00 && myData[currentOffset] <= 0x0000D7AF) || // These are syllables, they still are separated with spaces
			(myData[currentOffset] >= 0x0000F900 && myData[currentOffset] <= 0x0000FAFF) ||
			(myData[currentOffset] >= 0x00020000 && myData[currentOffset] <= 0x0002A6DF))
			{
//				printf("Found CJK %"PRIX32" @ %"PRIu32"\n", myData[currentOffset], currentOffset);
				if(matches[pos].rm_so == currentOffset)
					currentOffset++;
				foundCJK++;
			}
			else currentOffset++;
		}
		matches[pos].rm_eo = currentOffset;
		morematches = matches[pos].rm_eo - matches[pos].rm_so;
		if(morematches > 0)
		{
//			printf("New Word: %.*ls @ %"PRIu32" with length %"PRIu32"\n", matches[pos].rm_eo - matches[pos].rm_so, myData+matches[pos].rm_so, matches[pos].rm_so, matches[pos].rm_eo - matches[pos].rm_so);
			prime1 = HASHSEED1;
			prime2 = HASHSEED2;
			pos++;
			if(pos>4) pos=0;
			if(hashes_list->used + 4 >= hashes_list->slots)
			{
				makeSortedUniqueHashes(hashes_list); // Attempt to make more room by removing duplicates
				if(hashes_list->used + 4 >= hashes_list->slots) // If this is still the condition, we cannot handle more hashes
				{
					printf("This file creates too many hashes\n");
					return;
				}
			}
			lookup3_hashfunction((uint32_t *) myData+matches[pos].rm_so, matches[pos].rm_eo - matches[pos].rm_so, &prime1, &prime2);
		}
	} while(morematches > 0);
	// compute remaining hashes
	for(j = 4; j > 0; j--)
	{
		pos++;
		if(pos > 4) pos = 0;
		if(hashes_list->used == hashes_list->slots)
		{
			makeSortedUniqueHashes(hashes_list);
			if(hashes_list->used == hashes_list->slots) return;
		}
		for(i = 1; i < j; i++)
		{
			finalA = prime1;
			finalB = prime2;
			if(i > 1) lookup3_hashfunction((uint32_t *) placeHolder, i - 1, &finalA, &finalB);
			modPos = (pos + i) % 5;
			lookup3_hashfunction((uint32_t *) myData+matches[modPos].rm_so, matches[modPos].rm_eo - matches[modPos].rm_so, &finalA, &finalB);
			hashes_list->hashes[hashes_list->used] = (uint_least64_t) finalA << 32;
			hashes_list->hashes[hashes_list->used] |= (uint_least64_t) (finalB & 0xFFFFFFFF);
/*			printf("Hashed: %"PRIX64" (%.*ls %.*ls %.*ls)\n", hashes_list->hashes[hashes_list->used],
				matches[pos].rm_eo - matches[pos].rm_so, myData+matches[pos].rm_so,
				(i>1 ? i-1 : 0), placeHolder,
				matches[modPos].rm_eo - matches[modPos].rm_so, myData+matches[modPos].rm_so);*/
			hashes_list->used++;
		}
	}
	makeSortedUniqueHashes(hashes_list);
}
