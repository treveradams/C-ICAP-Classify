/*
 *  Copyright (C) 2008-2014 Trever L. Adams
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


#define _GNU_SOURCE

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#if (_FILE_OFFSET_BITS != 64)
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <langinfo.h>

#define _TRAIN_COMMON
#include "html.h"
#include "train_common.h"

void checkMakeUTF8(void)
{
    setlocale(LC_ALL, "");
    int utf8_mode = (strcmp(nl_langinfo(CODESET), "UTF-8") == 0);
    if (!utf8_mode) setlocale(LC_ALL, "en_US.UTF-8");
}

wchar_t *makeData(char *input_file)
{
    int data=0;
    struct stat stat_buf;
    char *tempData = NULL;
    wchar_t *myData = NULL;
    int32_t realLen;
    int32_t status;
    if ((data = open(input_file, O_RDONLY)) >= 0) {
        if (fstat(data, &stat_buf) == 0) {
            tempData = malloc(stat_buf.st_size + 1);
            if (tempData == NULL) exit(-1);
            do {
                status = read(data, tempData, stat_buf.st_size);
                if (status < stat_buf.st_size) lseek64(data, -status, SEEK_CUR);
            } while (status >=0 && status < stat_buf.st_size);
            close(data);
            tempData[stat_buf.st_size] = '\0';
            myData = malloc((stat_buf.st_size + 1) * UTF32_CHAR_SIZE);
            realLen = mbstowcs(myData, tempData, stat_buf.st_size);
            if (realLen!=-1) myData[realLen] = L'\0';
            else {
                ci_debug_printf(1, "*** Bad character data in %s, ignoring file\n", input_file);
                myData[0] = L'\0';
            }
            free(tempData);
        }
    }
    return myData;
}

void setupPrimarySecondary(char *primary, char *secondary, int bidirectional)
{

    if (number_secondaries == 0 || secondary_compares == NULL) {
        secondary_compares = malloc(sizeof(secondaries_t));
    } else {
        secondary_compares = realloc(secondary_compares, sizeof(secondaries_t) * (number_secondaries + 1));
    }

    if (tre_regcomp(&secondary_compares[number_secondaries].primary_regex, primary, REG_EXTENDED | REG_ICASE) != 0 ||
            tre_regcomp(&secondary_compares[number_secondaries].secondary_regex, secondary, REG_EXTENDED | REG_ICASE) != 0) {
        tre_regfree(&secondary_compares[number_secondaries].primary_regex);
        tre_regfree(&secondary_compares[number_secondaries].secondary_regex);
        number_secondaries--;
        ci_debug_printf(1, "Invalid REGEX In Setting parameter (PRIMARY_CATEGORY_REGEX: %s SECONDARY_CATEGORY_REGEX: %s BIDIRECTIONAL: %s)\n", primary, secondary, bidirectional ? "TRUE" : "FALSE" );
        exit(-1);
    }
    secondary_compares[number_secondaries].bidirectional = bidirectional;

    number_secondaries++;
}

void setupPrimarySecondFromCmdLine(char *cmdline)
{
    char *end, *start;
    char pri_name[513], sec_name[513];
    int bidir;
    start = cmdline;
    if (strstr(cmdline, "null,") != NULL) return;
//  fprintf(stderr, "Got: %s\n", start);
    while ((end=strchr(start, '=')) != NULL) {
        *end = '\0';
        sscanf(start, "%512[^,], %512[^,], %d", pri_name, sec_name, &bidir);
//      fprintf(stderr, "Parts: \"%s\", \"%s\", \"%d\"\n", pri_name, sec_name, bidir);
//      fflush(stderr);
        setupPrimarySecondary(pri_name, sec_name, bidir);
        start = end + 1;
    }
    sscanf(start, "%512[^,], %512[^,], %d", pri_name, sec_name, &bidir);
//  fprintf(stderr, "Parts: \"%s\", \"%s\", \"%d\"\n", pri_name, sec_name, bidir);
//  fflush(stderr);
    setupPrimarySecondary(pri_name, sec_name, bidir);
}
