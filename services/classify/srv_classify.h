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


#ifndef __SRV_CLASSIFY_H
#define __SRV_CLASSIFY_H

#define MAX_URI_LENGTH 2083

#define myMAX_HEADER 512

#define IMAGE_CATEGORY_COPIES_MIN 10

typedef struct classify_req_data {
    ci_simple_file_t *disk_body;
    ci_membuf_t *mem_body;
    ci_request_t *req;
    ci_simple_file_t *external_body;
    ci_membuf_t *uncompressedbody;
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
    const char *type_name;
#endif
    int file_type;
    int must_classify;
    int encoded;
    int allow204;
    struct {
        int enable204;
        int forcescan;
        int sizelimit;
        int mode;
    } args;
} classify_req_data_t;

enum {NO_CLASSIFY=0, TEXT = 1, IMAGE = 2, EXTERNAL_TEXT = 4, EXTERNAL_TEXT_PIPE = 8, EXTERNAL_IMAGE = 16};

typedef struct {
    int magic_type_num;
    char *mime_type;
    char *text_program;
    char *image_program;
    int data_type; // Is this a read from stdout of program type or from a named file
    char **text_args;
    char **image_args;
} external_conversion_t;

char *myStrDup(const char *string);

#endif

#ifndef IN_SRV_CLASSIFY
extern void getReferrerClassification(const char *uri, HTMLClassification *fhs_classification, HTMLClassification *fnb_classification);
extern void addReferrerHeaders(ci_request_t *req, HTMLClassification fhs_classification, HTMLClassification fnb_classification);
extern void memBodyToDiskBody(ci_request_t *req);
extern char *CLASSIFY_TMP_DIR;
extern int CFG_NUM_CICAP_THREADS;
#endif
