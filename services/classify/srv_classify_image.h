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


#ifndef __SRV_CLASSIFY_IMAGE_H
#define __SRV_CLASSIFY_IMAGE_H

#define CATMAXNAME 100
#define myMAX_HEADER 512

typedef struct _linkedcascade {
    CvHaarClassifierCascade *cascade;
    struct _linkedcascade *next;
} LinkedCascade;

typedef struct _Category {
    char name[CATMAXNAME+1];
    char cascade_location[CI_MAX_PATH+1];
    LinkedCascade *cascade_array; // This is simply an array of the cascades, only here so we can free properly
    LinkedCascade *free_cascade; // don't forget to free me on any reload
    CvScalar Color;
    float coalesceOverlap;
    ci_thread_mutex_t mutex;
    ci_thread_cond_t cond;
    int freeing_category;
} ImageCategory;

typedef struct _Detected {
    ImageCategory *category; // Don't free this. This is a copy.
    CvSeq *detected;
} image_detected_t;

typedef struct _DetectedCount {
    ImageCategory *category; // Don't free this. This is a copy.
    uint16_t count;
} image_detected_count_t;

typedef struct {
    char fname[CI_MAX_PATH+1];
    float scale;
    IplImage *origImage;
    IplImage *rightImage;
    CvMemStorage *dstorage; // For use in detect routines and in using detected objects -- don't forget to free me after each image with cvClearMemStorage
    CvMemStorage *lstorage; // For all other uses -- don't forget to free me after each image with cvClearMemStorage
    image_detected_t *detected;
    int featuresDetected;
    ci_request_t *req;
    HTMLClassification referrer_fhs_classification;
    HTMLClassification referrer_fnb_classification;
} image_session_t;

enum {IMAGE_NO_CATEGORIES=-1, IMAGE_NO_MEMORY=-2, IMAGE_FAILED_LOAD=-3};

int categorize_image(ci_request_t * req);
int cfg_AddImageCategory(const char *directive, const char **argv, void *setdata);
int cfg_ImageInterpolation(const char *directive, const char **argv, void *setdata);
int cfg_coalesceOverlap(const char *directive, const char **argv, void *setdata);
#endif
