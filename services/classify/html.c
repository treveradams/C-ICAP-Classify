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

#define IN_HTML 1
#include "currency.h"
#include "html.h"
#include "htmlentities.h"
#include "hash.h"

regex_t htmlFinder, superFinder, commentFinder, imageFinder, title1, title2, alt1, alt2;
regex_t metaFinder, metaDescription, metaKeyword, metaContent, currencyFinder, nbspFinder;
regex_t headFinder, charsetFinder;
regex_t entityFinder, numericentityFinder;
regex_t insaneFinder;

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

void initHTML(void)
{
	compileRegexes();
	qsort(htmlentities, sizeof(htmlentities) / sizeof(htmlentities[0]) - 1, sizeof(_htmlentity), &entity_compare );
}

void deinitHTML(void)
{
	freeRegexes();
}

static void compileRegexes(void)
{
wchar_t myRegex[PATH_MAX+1];

//	swprintf(myRegex, PATH_MAX, L"([%ls])([[:space:]]|&nbsp;)*([[:digit:]]+)(\\.[[:digit:]]*)?", CURRENCY);
	swprintf(myRegex, PATH_MAX, L"([%ls])([[:space:]]*)([[:digit:]]+)(\\.[[:digit:]]*)?", CURRENCY); // As is, we don't need to get rid of nbsp as we have done so
	myRegex[PATH_MAX] = L'\0';
	tre_regwcomp(&currencyFinder, myRegex, REG_EXTENDED);

	tre_regwcomp(&nbspFinder, L"(&nbsp;)+", REG_EXTENDED);

	tre_regwcomp(&htmlFinder, L"(<[^>]*>([[:space:]]*))+", REG_EXTENDED);
	tre_regwcomp(&insaneFinder, L"[^[:graph:][:space:]]+", REG_EXTENDED);
//	tre_regwcomp(&insaneFinder, L"[\u200E\u200F\u200B\u202A-\u202E\u2060\u206A-\u206F]+", REG_EXTENDED);
//	tre_regwcomp(&entityFinder, L"&#x?([0-9a-fA-F]+);", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&entityFinder, L"&([^[:space:];&]+);", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&numericentityFinder, L"^#x?([[:xdigit:]]+$)", REG_EXTENDED | REG_ICASE);
	// The following two commentedwere replaced by superFinder
	//    <[[:space:]]*script[^>]*>.*?<[[:space:]]*/script[^>]*>
	//    <[[:space:]]*style[^>]*>.*?<[[:space:]]*/style[^>]*>
	tre_regwcomp(&superFinder, L"<(([[:space:]]*script[^>]*>.*?<[[:space:]]*/script[^>]*)|([[:space:]]*style[^>]*.*?<[[:space:]]*/style[^>]*))>", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&commentFinder, L"<!--.*?-->", REG_EXTENDED);
	tre_regwcomp(&imageFinder, L"<[[:space:]]*img ([^>]*)>", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&title1, L"title=\"([^\"]*)\"", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&title2, L"title=([[:graph:]]*)", REG_EXTENDED | REG_ICASE);
//	tre_regwcomp(&title3, L"title='([^']*)'", REG_EXTENDED | REG_ICASE);
	tre_regwcomp(&alt1, L"alt=\"([^\"]*)\"", REG_EXTENDED | REG_ICASE);
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

	tre_regfree(&nbspFinder);

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

void mkRegexHead(regexHead *head, wchar_t *myData)
{
myRegmatchArray *arrays = calloc(1, sizeof(myRegmatchArray));
myRegmatch_t *data;
	// FIXME FREE FIRST
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
		if(current->data == NULL && startblock == current) // we are only processing internal data here.
		{
			if(current->rm_so <= to_remove->rm_so && current->rm_eo >= to_remove->rm_eo) // we found the start
			{
//				printf("Removing: %.*ls\n", to_remove->rm_eo - to_remove->rm_so, myHead->main_memory + to_remove->rm_so);
				newmatch = getEmptyRegexBlock(myHead);
				newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
				newmatch->rm_eo = current->rm_eo; // the new block ends where the old one used to
				current->rm_eo = to_remove->rm_so; // the old block ends where we started
				myHead->main_memory[to_remove->rm_so] = L'\0';
				newmatch->next = current->next; // save old next
				current->next = newmatch; // insert new match
				if(newmatch->next == NULL) myHead->tail = newmatch;
				myHead->dirty = 1;
				return;
			}
		}
		else if(current->data != NULL && startblock == current) // we process private memory blocks here.
		{
			if(current->rm_so <= to_remove->rm_so && current->rm_eo >= to_remove->rm_eo) // we found the start
			{
//				printf("Private Removing: %.*ls\n", to_remove->rm_eo - to_remove->rm_so, current->data + to_remove->rm_so);
				newmatch = getEmptyRegexBlock(myHead);
				newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
				newmatch->rm_eo = current->rm_eo; // the new block ends where the old one used to
				current->rm_eo = to_remove->rm_so; // the old block ends where we started
				current->data[to_remove->rm_so] = L'\0';
				newmatch->data = current->data;
				newmatch->next = current->next; // save old next
				current->next = newmatch; // insert new match
				if(newmatch->next == NULL) myHead->tail = newmatch;
				myHead->dirty = 1;
//				printf("After Private Remove Next Block (max 10): %.*ls\n", newmatch->rm_eo - newmatch->rm_so > 10 ? 10: newmatch->rm_eo - newmatch->rm_so, newmatch->data + newmatch->rm_so);
				return;
			}
		}
		current = current->next;
	}
}

static void regexReplace(regexHead *myHead, myRegmatch_t *startblock, regmatch_t *to_remove, wchar_t *replaceMe, int len, int pad)
{
myRegmatch_t *current = myHead->head, *newmatch, *newdata;
uint32_t myLen = 0;

	while(current != NULL)
	{
		if(current->data == NULL && startblock == current) // we are only processing internal data here.
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
//					myHead->main_memory[current->rm_eo] = L'\0';
					current->next = newmatch; // insert new match
//					printf("Inserted: \"%.*ls\"\n", myLen, myHead->main_memory + to_remove->rm_so);
				}
				else {
					newdata = getEmptyRegexBlock(myHead);
					newdata->data = malloc((len + 3) * sizeof(wchar_t));
					newdata->rm_eo = swprintf(newdata->data, len+3, L"%ls%.*ls%ls", pad ? L" " : L"", len, replaceMe, pad ? L" " : L"");
					newdata->rm_so = 0;
					newdata->owns_memory = 1;
					current->rm_eo = to_remove->rm_so; // the old block ends where we started
//					myHead->main_memory[current->rm_eo] = L'\0';
					current->next = newdata; // insert new data
					newdata->next = newmatch; // insert new match
//					printf("Inserted: %.*ls\n", newdata->rm_eo, newdata->data);
				}
				newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
				if(newmatch->next == NULL) myHead->tail = newmatch;
				myHead->dirty = 1;
				current = NULL;
			}
		}
		else if(current->data != NULL && startblock == current) // we process private memory blocks here.
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
//					current->data[current->rm_eo] = L'\0';
					current->next = newmatch; // insert new match
//					printf("Inserted: \"%.*ls\"\n", myLen, current->data + to_remove->rm_so);
				}
				else {
					newdata = getEmptyRegexBlock(myHead);
					newdata->data = malloc((len + 3) * sizeof(wchar_t));
					newdata->rm_eo = swprintf(newdata->data, len+3, L"%ls%.*ls%ls", pad ? L" " : L"", len, replaceMe, pad ? L" " : L"");
					newdata->rm_so = 0;
					newdata->owns_memory = 1;
					current->rm_eo = to_remove->rm_so; // the old block ends where we started
//					current->data[current->rm_eo] = L'\0';
					current->next = newdata; // insert new data
					newdata->next = newmatch; // insert new match
//					printf("Inserted: \"%.*ls\"\n", newdata->rm_eo, newdata->data);
				}
				newmatch->rm_so = to_remove->rm_eo; // the new block starts right after the found regex
				if(newmatch->next == NULL) myHead->tail = newmatch;
				myHead->dirty = 1;
				current = NULL;
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

	if(myHead->tail->data && (myHead->tail->rm_eo + len + 2) < regexAPPENDSIZE)
	{
		offset = myHead->tail->rm_eo;
		newdata = myHead->tail;
		newdata->data[offset] = L' ';
		memcpy(newdata->data + offset + 1, appendMe, len * sizeof(wchar_t));
		newdata->rm_eo = newdata->rm_eo + len + 1;
		newdata->data[newdata->rm_eo] = L'\0';
//		printf("Old Appending: %.*ls now: %.*ls\n", len, appendMe, newdata->rm_eo, newdata->data);
	}
	else {
		newdata = getEmptyRegexBlock(myHead);
		// the following 3 lines were removed for the subsequent 5 lines doing the same thing
		//	newdata->rm_eo=swprintf(myAPPEND, PATH_MAX, L" %.*ls", len, appendMe);
		//	myAPPEND[PATH_MAX]='\0';
		//	newdata->data=wcsdup(myAPPEND);
		newdata->rm_eo = len + 1;
		if(len + 2 < regexAPPENDSIZE) newdata->data = malloc(regexAPPENDSIZE * sizeof(wchar_t));
		else newdata->data = malloc((len + 2) * sizeof(wchar_t));
		newdata->data[0] = L' ';
		//	wcsncpy(newdata->data+1, appendMe, len);
		memcpy(newdata->data + 1, appendMe, len * sizeof(wchar_t));
		newdata->data[len + 1] = L'\0';
//		printf("New Appending: %.*ls\n", len, appendMe);
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
	myHead->main_memory[total] = L'\0';
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
	freeRegmatchArrays(myHead->arrays);
	free(myHead->main_memory);
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
		while(tre_regwnexec(&currencyFinder, myData + currentOffset, current->rm_eo - currentOffset, 5, currencyMatch, 0) != REG_NOMATCH)
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
			replace[100] = L'\0';
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
regmatch_t singleMatch[3], doubleMatch[2], tripleMatch[2];
myRegmatch_t *current = myHead->head;
wchar_t unicode_entity;
int32_t entity;
wchar_t *unicode_end;
int metacount;
int xi = 0;

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
		while(current->rm_eo>currentOffset && tre_regwnexec(&metaFinder, myData + currentOffset, current->rm_eo - currentOffset, 2, singleMatch, 0) != REG_NOMATCH)
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
					for(metacount = 0; metacount < tripleMatch[1].rm_eo-tripleMatch[1].rm_so; metacount++)
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
		while(current->rm_eo>currentOffset && tre_regwnexec(&imageFinder, myData + currentOffset, current->rm_eo - currentOffset, 2, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
			singleMatch[1].rm_so += currentOffset;
			singleMatch[1].rm_eo += currentOffset;
			if (tre_regwnexec(&title1, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so, doubleMatch[1].rm_eo - doubleMatch[1].rm_so);
			}
			else if (tre_regwnexec(&title2, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so, doubleMatch[1].rm_eo - doubleMatch[1].rm_so);
			}
			if (tre_regwnexec(&alt1, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so, doubleMatch[1].rm_eo - doubleMatch[1].rm_so);
			}
			else if	(tre_regwnexec(&alt2, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so, 2, doubleMatch, 0) != REG_NOMATCH)
			{
				doubleMatch[0].rm_so += singleMatch[1].rm_so;
				doubleMatch[0].rm_eo += singleMatch[1].rm_so;
				doubleMatch[1].rm_so += singleMatch[1].rm_so;
				doubleMatch[1].rm_eo += singleMatch[1].rm_so;
				regexAppend(myHead, myData + doubleMatch[1].rm_so, doubleMatch[1].rm_eo - doubleMatch[1].rm_so);
			}
			regexRemove(myHead, current, &singleMatch[0]);
//			printf("Killing Image Tag: %.*ls\n", singleMatch[0].rm_eo-singleMatch[0].rm_so, myData+singleMatch[0].rm_so);
			currentOffset = singleMatch[0].rm_eo;
		}
		current=current->next;
	}

	current = myHead->head;
	while(current != NULL) // kill all unused tags
	{
		myData = (wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(current->rm_eo>currentOffset && tre_regwnexec(&htmlFinder, myData + currentOffset, current->rm_eo-currentOffset, 3, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
//			printf("Killing Uncared Tag: %.*ls\n", singleMatch[0].rm_eo - singleMatch[0].rm_so, myData + singleMatch[0].rm_so);
			if(singleMatch[2].rm_so != -1) regexReplace(myHead, current, &singleMatch[0], L" ", 1, 0);
			else regexRemove(myHead, current, &singleMatch[0]);
			currentOffset = singleMatch[0].rm_eo;
		}
		current = current->next;
	}

	current = myHead->head;
	while(current != NULL) // nbsps, must be second to last
	{
		myData = (wchar_t *)(current->data == NULL ? myHead->main_memory : current->data);
		currentOffset = current->rm_so;
		while(tre_regwnexec(&nbspFinder, myData + currentOffset, current->rm_eo - currentOffset, 2, singleMatch, 0) != REG_NOMATCH)
		{
			singleMatch[0].rm_so += currentOffset;
			singleMatch[0].rm_eo += currentOffset;
//			printf("Killing nbsp: %.*ls\n", singleMatch[0].rm_eo - singleMatch[0].rm_so, myData + singleMatch[0].rm_so);
			regexReplace(myHead, current, &singleMatch[0], L" ", 1, 0);
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
		while(current->rm_eo > currentOffset && tre_regwnexec(&entityFinder, myData+currentOffset, current->rm_eo-currentOffset, 2, singleMatch, 0) != REG_NOMATCH)
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
				if(myData[doubleMatch[0].rm_so + 2] == L'X' || myData[doubleMatch[0].rm_so + 2] == L'x') { // Check for hex
					unicode_entity = wcstoul(myData+doubleMatch[1].rm_so, &unicode_end, 16);
					ci_debug_printf(10,"Converting Hexadecimal HTML Entity: %.*ls to %lc\n", doubleMatch[1].rm_eo - doubleMatch[1].rm_so, myData + doubleMatch[1].rm_so, unicode_entity);
				}
				else {
					unicode_entity = wcstoul(myData + doubleMatch[1].rm_so, &unicode_end, 10);
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", doubleMatch[1].rm_eo - doubleMatch[1].rm_so, myData + doubleMatch[1].rm_so, unicode_entity);
				}
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

/*			else if(myData[singleMatch[1].rm_so] == L'a' || myData[singleMatch[1].rm_so] == L'A')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Aacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C1';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"aacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E1';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"acute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00B4';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"AElig", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C6';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"aelig", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E6';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Acirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C2';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"acirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E2';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Agrave", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C0';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"agrave", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E0';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Alpha", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0391';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"alpha", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03B1';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"AMP", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'&';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"amp", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'&';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"And", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2A53';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"and", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2227';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Aring", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C5';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"aring", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E5';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Atilde", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C3';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"atilde", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E3';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Auml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C4';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"auml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E4';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"apos", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = 0x00027;
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'b' || myData[singleMatch[1].rm_so] == L'B')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Beta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0392';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"beta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03B2';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"bdquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u201E';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"bull", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2022';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'c' || myData[singleMatch[1].rm_so] == L'C')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Ccedil", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C7';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ccedil", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E7';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"cent", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00A2';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Chi", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03A7';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"chi", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03C7';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"clubs", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2663';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"clubsuit", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2663';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"COPY", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00A9';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"copy", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00A9';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"curren", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00A4';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'd' || myData[singleMatch[1].rm_so] == L'D')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Dagger", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2021';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"dagger", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2020';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"darr", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2193';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"dArr", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u21D3';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Darr", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u21A1';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"deg", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00B0';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Delta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0394';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"delta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03B4';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"diamondsuit", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2666';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"diams", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2666';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'e' || myData[singleMatch[1].rm_so] == L'E')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Eacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C9';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"eacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E9';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Ecirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00CA';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ecirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00EA';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Egrave", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00C8';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"egrave", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00E8';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ensp", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L' '; // A space is a space is a space - should be L'\u2002';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Epsilon", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0395';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"epsilon", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03B5';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"equiv", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2261';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Eta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0397';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"eta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03B7';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Euml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00CB';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"euml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00EB';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"euro", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u20AC';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'f' || myData[singleMatch[1].rm_so] == L'F')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"frac12", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00BD';
	//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac13", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2153';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac14", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00BC';
	//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac15", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2155';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac16", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2159';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac18", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u215B';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac23", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2154';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac25", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2156';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac34", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00BE';
	//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac35", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2157';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac38", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u215C';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac45", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2158';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac56", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u215A';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac58", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u215D';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"frac78", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u215E';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"Gamma", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u0393';
				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"gamma", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u03B3';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"gt", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'>';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"hearts", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u2665';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"heartsuit", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u2665';
				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"hellip", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u2026';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(myData[singleMatch[1].rm_so] == L'i' || myData[singleMatch[1].rm_so] == L'I')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Iacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00CD';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"iacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00ED';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Icirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00CE';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"icirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00EE';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"iexcl", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00A1';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Igrave", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00CC';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"igrave", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00EC';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Iota", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0399';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"iota", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03B9';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"iquest", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00BF';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Iuml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00CF';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"iuml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00EF';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"Kappa", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u039A';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"kappa", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u03BA';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(myData[singleMatch[1].rm_so] == L'l' || myData[singleMatch[1].rm_so] == L'L')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Lambda", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u039B';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"lambda", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03BB';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"laquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00AB';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ldquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u201C';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"loz", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u25CA';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"lsquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2018';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"lsaquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2039';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"lrm", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u200E';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"lt", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'<';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'm' || myData[singleMatch[1].rm_so] == L'M')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"mdash", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2014';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"micro", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00B5';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"middot", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00B7';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"minus", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2212';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Mu", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u039C';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"mu", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03BC';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'n' || myData[singleMatch[1].rm_so] == L'N')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"nabla", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2207';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ndash", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2013';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"not", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00AC';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Ntilde", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D1';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ntilde", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00F1';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Nu", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u039D';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"nu", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03BD';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so + 10, myData+singleMatch[1].rm_so -5);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'o' || myData[singleMatch[1].rm_so] == L'O')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Oacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D3';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"oacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00F3';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Ocirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D4';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ocirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00F4';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Ograve", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D2';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ograve", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00F2';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Omega", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03A9';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"omega", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03C9';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Omicron", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u039F';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"omicron", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03BF';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ordf", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00AA';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ordm", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00BA';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Oslash", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D8';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"oslash", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00F8';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Otilde", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D5';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"otilde", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00F5';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Ouml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D6';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ouml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00F6';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'p' || myData[singleMatch[1].rm_so] == L'P')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Pi", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03A0';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"pi", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03C0';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"plusmn", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00B1';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"pound", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00A3';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"quot", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'"';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(myData[singleMatch[1].rm_so] == L'r' || myData[singleMatch[1].rm_so] == L'R')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"raquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00BB';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"rarr", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2192';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"rArr", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u21D2';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Rarr", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u21A0';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"rdquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u201D';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"REG", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00AE';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"reg", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00AE';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Rho", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03A1';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"rho", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03C1';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"rlm", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u200F';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"rsaquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u203A';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"rsquo", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2019';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L's' || myData[singleMatch[1].rm_so] == L'S')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Scaron", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0160';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"scaron", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0161';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"sdot", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u22C5';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"shy", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00AD';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Sigma", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03A3';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"sigma", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03C3';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"spades", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2660';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"spadesuit", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2660';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"sup1", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00B9';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"sup2", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00B2';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"sup3", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00B3';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"szlig", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00DF';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L't' || myData[singleMatch[1].rm_so] == L'T')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Tau", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03A4';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"tau", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03C4';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Theta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u0398';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"theta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03B8';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"times", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D7';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"TRADE", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2122';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"trade", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u2122';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(myData[singleMatch[1].rm_so] == L'u' || myData[singleMatch[1].rm_so] == L'U')
			{
				if(wcsncmp(myData + singleMatch[1].rm_so, L"Uacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00DA';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"uacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00FA';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Ucirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00DB';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ucirc", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00FB';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Ugrave", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00D9';
					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"ugrave", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00F9';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Upsilon", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03A5';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"upsilon", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u03C5';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"Uuml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00DC';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else if(wcsncmp(myData + singleMatch[1].rm_so, L"uuml", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
				{
					unicode_entity = L'\u00FC';
//					ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
					regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
				}

				else
				{
					ci_debug_printf(3, "Found Unhandled HTML Entity: %.*ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData+singleMatch[1].rm_so);
					currentOffset++;
				}
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"Xi", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u039E';
				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"xi", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u03BE';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"Yacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u00DD';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"yacute", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u00FD';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"yen", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u00A5';
				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"Zeta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u03A6';
				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			}

			else if(wcsncmp(myData + singleMatch[1].rm_so, L"zeta", singleMatch[1].rm_eo - singleMatch[1].rm_so) == 0)
			{
				unicode_entity = L'\u03B6';
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %lc\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, unicode_entity);
				regexReplace(myHead, current, &singleMatch[0], &unicode_entity, 1, 0); // We are replacing characters, do not add padding!
			} */

			else if((entity=findEntityBS(0, sizeof( htmlentities ) / sizeof( htmlentities[0] ) - 1, myData + singleMatch[1].rm_so, singleMatch[1].rm_eo - singleMatch[1].rm_so)) >= 0)
			{
//				ci_debug_printf(10, "Converting HTML Entity: %.*ls to %ls\n", singleMatch[1].rm_eo - singleMatch[1].rm_so, myData + singleMatch[1].rm_so, htmlentities[entity].value);
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
		while(tre_regwnexec(&insaneFinder, myData + currentOffset, current->rm_eo - currentOffset, 2, singleMatch, 0) != REG_NOMATCH)
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
}



void computeOSBHashes(regexHead *myHead, uint32_t primaryseed, uint32_t secondaryseed, HashList *hashes_list)
{
wchar_t *myData = NULL;
regoff_t currentOffset = 0, morematches = 0;
myRegmatch_t *current = myHead->head;
regmatch_t matches[5];
uint32_t i, j, pos;
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
		while (!iswgraph(myData[currentOffset])
			&& currentOffset < current->rm_eo)
			currentOffset++;
		matches[i].rm_so = currentOffset;
		foundCJK=0;
		while (iswgraph(myData[currentOffset]) && currentOffset < current->rm_eo && foundCJK == 0)
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
	hashword2((uint32_t *) myData+matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so, &prime1, &prime2);
	pos = 0;
	do {
		for(i = 1; i < 5; i++)
		{
			finalA=prime1;
			finalB=prime2;
			if(i > 1) hashword2((uint32_t *) placeHolder, i-1, &finalA, &finalB);
			hashword2((uint32_t *) myData+matches[(pos+i)%5].rm_so, matches[(pos+i)%5].rm_eo - matches[(pos+i)%5].rm_so, &finalA, &finalB);
			hashes_list->hashes[hashes_list->used] = (uint_least64_t) finalA << 32;
			hashes_list->hashes[hashes_list->used] |= (uint_least64_t) (finalB & 0xFFFFFFFF);
			hashes_list->used++;
/*			printf("Hashed: %"PRIX64" (%.*ls %.*ls %.*ls)\n", hashes_list->hashes[hashes_list->used],
				matches[pos].rm_eo - matches[pos].rm_so, myData+matches[pos].rm_so,
				(i>1 ? i-1 : 0), placeHolder,
				matches[(pos+i)%5].rm_eo - matches[(pos+i)%5].rm_so, myData+matches[(pos+i)%5].rm_so); */
		}
		// skip non-graphical characters ([[:graph:]]+)
		while (!iswgraph(myData[currentOffset]) && currentOffset < current->rm_eo)
			currentOffset++;
		matches[pos].rm_so = currentOffset;
		foundCJK = 0;
		while (iswgraph(myData [currentOffset]) && currentOffset < current->rm_eo && foundCJK == 0)
		while (iswgraph(myData[currentOffset]) && currentOffset < current->rm_eo && foundCJK == 0)
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
			if(hashes_list->used + 4 == hashes_list->slots)
			{
				printf("This file creates too many hashes\n");
				return;
			}
			hashword2((uint32_t *) myData+matches[pos].rm_so, matches[pos].rm_eo - matches[pos].rm_so, &prime1, &prime2);
		}
	} while(morematches > 0);
	// compute remaining hashes
	for(j=4; j>0; j--)
	{
		pos++;
		if(pos > 4) pos=0;
		if(hashes_list->used == hashes_list->slots)
		{
			makeSortedUniqueHashes(hashes_list);
			if(hashes_list->used == hashes_list->slots) return;
		}
		for(i = 1; i < j; i++)
		{
			finalA = prime1;
			finalB = prime2;
			if(i > 1) hashword2((uint32_t *) placeHolder, i-1, &finalA, &finalB);
			hashword2((uint32_t *) myData+matches[(pos+i)%5].rm_so, matches[(pos+i)%5].rm_eo - matches[(pos+i)%5].rm_so, &finalA, &finalB);
			hashes_list->hashes[hashes_list->used] = (uint_least64_t) finalA << 32;
			hashes_list->hashes[hashes_list->used] |= (uint_least64_t) (finalB & 0xFFFFFFFF);
			hashes_list->used++;
/*			printf("Hashed: %"PRIX64" (%.*ls %.*ls %.*ls)\n", hashes_list->hashes[hashes_list->used],
				matches[pos].rm_eo - matches[pos].rm_so, myData+matches[pos].rm_so,
				(i>1 ? i-1 : 0), placeHolder,
				matches[(pos+i)%5].rm_eo - matches[(pos+i)%5].rm_so, myData+matches[(pos+i)%5].rm_so); */
		}
	}
	makeSortedUniqueHashes(hashes_list);
}
