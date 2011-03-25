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
	LinkedCascade *free_cascade; // don't forget to free me on any reload
	LinkedCascade *busy_cascade; // don't forget to free me on any reload, wait for this to be null and all free_cascade to be released
	CvScalar Color;
	float coalesceOverlap;
	ci_thread_mutex_t mutex;
	int freeing_category;
	struct _Category *next;
} ImageCategory;

typedef struct _Detected {
	ImageCategory *category; // Don't free this. This is a copy.
	CvSeq *detected;
	struct _Detected *next;
} ImageDetected;

typedef struct {
	char fname[CI_MAX_PATH+1];
	float scale;
	IplImage *origImage;
	IplImage *rightImage;
	CvMemStorage *dstorage; // For use in detect routines and in using detected objects -- don't forget to free me after each image with cvClearMemStorage
	CvMemStorage *lstorage; // For all other uses -- don't forget to free me after each image with cvClearMemStorage
	ImageDetected *detected;
	int featuresDetected;
	ci_request_t * req;
} ImageSession;

enum {NO_CATEGORIES=-1, NO_MEMORY=-2};

int categorize_image(ci_request_t * req);
int cfg_AddImageCategory(char *directive, char **argv, void *setdata);
int cfg_ImageInterpolation(char *directive, char **argv, void *setdata);
int cfg_coalesceOverlap(char *directive, char **argv, void *setdata);
#endif
