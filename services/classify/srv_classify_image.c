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


// Image Categorization

/* TODO:
	* Add object correction - butts and breats gets confused, etc. sometimes
	* Add escalation acl so that if too many objects, etc. get detected, referrer gets blocked when reloaded (requires db backend and cooperation with srv_classify).
*/

#include "autoconf.h"

// Include header files
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>

#include "c-icap.h"
#include "service.h"
#include "header.h"
#include "body.h"
#include "simple_api.h"
#include "debug.h"
#include "cfg_param.h"
#include "ci_threads.h"
#include "filetype.h"
#include "txt_format.h"

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include "html.h"
#include "srv_classify.h"
#include "srv_classify_image.h"

#ifdef _WIN32
#define F_PERM S_IREAD|S_IWRITE
#else
#define F_PERM S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH
#endif

// What is the minimum debug level to do perfromance eval by timing
static const int PERF_DEBUG_LEVEL = 8;

int IMAGE_SCALE_DIMENSION = 240; // Scale to this dimension
int IMAGE_MAX_SCALE = 4; // Maximum rescale
int IMAGE_MIN_PROCESS = 30; // don't process images whose minimum dimension is under this size
int IMAGE_DEBUG_SAVE_ORIG = 0; // DEBUG: Do we save the original?
int IMAGE_DEBUG_SAVE_PARTS = 0; // DEBUG: Do we save the parts?
int IMAGE_DEBUG_SAVE_MARKED = 0; // DEBUG: Do save the original marked?
int IMAGE_DEBUG_DEMONSTRATE = 0; // DEBUG: Do we demonstrate?
int IMAGE_DEBUG_DEMONSTRATE_MASKED = 0; // DEBUG: we demonstrate with a mask?
int IMAGE_INTERPOLATION = CV_INTER_LINEAR; // Interpolation function to use.
int IMAGE_CATEGORY_COPIES = IMAGE_CATEGORY_COPIES_MIN; // How many copies of each cascade do we load to avoid OpenCV bug and bottle necks

extern external_conversion_t *externalclassifytypes;

ImageCategory *imageCategories = NULL;
uint16_t num_image_categories = 0;

static int IMAGEDETECTED_POOL = -1;
static int IMAGEDETECTEDCOUNT_POOL = -1;

/* Locking */
ci_thread_rwlock_t imageclassify_rwlock;

// OpenCV saving types
int png_p[] = {CV_IMWRITE_PNG_COMPRESSION, 9, 0};
int jpeg_p[] = {CV_IMWRITE_JPEG_QUALITY, 100, 0};

/* Template Functions and Table */
int fmt_srv_classify_image_source(ci_request_t *req, char *buf, int len, const char *param);
int fmt_srv_classify_image_destination_directory(ci_request_t *req, char *buf, int len, const char *param);

struct ci_fmt_entry srv_classify_image_format_table [] = {
    {"%CLIN", "File To Convert", fmt_srv_classify_image_source},
    {"%CLDIROUT", "File To Read Back In For Classification", fmt_srv_classify_image_destination_directory},
    { NULL, NULL, NULL}
};

void classifyImagePrepReload(void);

int postInitImageClassificationService(void)
{
uint16_t category;
	ci_thread_rwlock_init(&imageclassify_rwlock);
	ci_thread_rwlock_wrlock(&imageclassify_rwlock);

	/*Initialize object pools*/
	IMAGEDETECTED_POOL = ci_object_pool_register("image_detected_t", sizeof(image_detected_t) * num_image_categories);

	if(IMAGEDETECTED_POOL < 0) {
		ci_debug_printf(1, " srvclassify_init_service: error registering object_pool image_detected_t\n");
		ci_thread_rwlock_unlock(&imageclassify_rwlock);
		return CI_ERROR;
	}

	IMAGEDETECTEDCOUNT_POOL = ci_object_pool_register("image_detected_count_t", sizeof(image_detected_count_t) * num_image_categories);

	if(IMAGEDETECTEDCOUNT_POOL < 0) {
		ci_debug_printf(1, " srvclassify_init_service: error registering object_pool image_detected_count_t\n");
		ci_object_pool_unregister(IMAGEDETECTED_POOL);
		ci_thread_rwlock_unlock(&imageclassify_rwlock);
		return CI_ERROR;
	}

	if(num_image_categories)
	{
		for(category = 0; category < num_image_categories; category++)
		{
			if(ci_thread_mutex_init(&imageCategories[category].mutex) != 0)
			{
				ci_debug_printf(1, "srv_classify_image: Couldn't init category mutex\n");
			}
			if(ci_thread_cond_init(&imageCategories[category].cond) != 0)
			{
				ci_debug_printf(1, "srv_classify_image: Couldn't init category condition lock variable\n");
			}
		}
	}

	ci_thread_rwlock_unlock(&imageclassify_rwlock);
	return CI_OK;
}

int closeImageClassification(void)
{
	classifyImagePrepReload();
	ci_object_pool_unregister(IMAGEDETECTEDCOUNT_POOL);
	ci_object_pool_unregister(IMAGEDETECTED_POOL);
	return CI_OK;
}

static int unlink_directory(char *directory)
{
DIR *dirp;
struct dirent *dp;
char old_dir[PATH_MAX];

	getcwd(old_dir, PATH_MAX);
	chdir(directory);

	if ((dirp = opendir(directory)) == NULL)
	{
		ci_debug_printf(3, "srv_classify_image: unlink_directory: couldn't open '%s'", directory);
		return -1;
	}

	do {
		errno = 0;
		if ((dp = readdir(dirp)) != NULL)
		{
			if (strcmp(dp->d_name, "..") != 0 && strcmp(dp->d_name, ".") != 0)
			{
				remove(dp->d_name);
			}
		}
	} while (dp != NULL);
	if (errno != 0)
		perror("error reading directory");
	else
		(void) closedir(dirp);

	chdir(old_dir);
	remove(directory);
	return 1;
}

static void doCoalesce(image_session_t *mySession)
{
CvSeq *newDetected = NULL;
image_detected_t *current;
CvRect tempRect;
int over_left, over_top;
int right1, right2, over_right;
int bottom1, bottom2, over_bottom;
int over_width, over_height;
int r1Area, r2Area;
int *merged = NULL;
int newI = 0;
int i, j;
uint16_t current_category;

	for(current_category = 0; current_category < num_image_categories; current_category++) {
		current = &mySession->detected[current_category];
		if(current->category->coalesceOverlap != 1.0f && current->detected)
		{
			// Loop the number of detected objects
			newDetected = cvCreateSeq( 0, sizeof(CvSeq), sizeof(CvRect), mySession->lstorage );
			merged = malloc(sizeof(int) *  current->detected->total);
			newI = 0;
			for(i = 0; i < current->detected->total; i++ ) merged[i] = -1; // quickly setup merged variable
			for(i = 0; i < current->detected->total; i++ )
			{
				CvRect* r = (CvRect*)cvGetSeqElem( current->detected, i );

				if(merged[i] == -1) {
					cvSeqPush( newDetected, r );
					merged[i] = newI;
					newI++;
				}

				if(current->detected->total - i > 0)
				{
					r1Area = r->width * r->height;

					for(j = i + 1; j < current->detected->total; j++ )
					{
						// Reset rectangles to original size
						CvRect* r2 = (CvRect*)cvGetSeqElem( current->detected, j );
						r2Area = r2->width * r2->height;

						right1 = r->x + r->width;
						right2 = r2->x + r2->width;
						bottom1 = r->y + r->height;
						bottom2 = r2->y + r2->height;

						if (!(bottom1 < r2->y) && !(r->y > bottom2) && !(right1 < r2->x) && !(r->x > right2))
						{

							// Ok, compute the rectangle of overlap:
							if (bottom1 > bottom2) over_bottom = bottom2;
							else over_bottom = bottom1;

							if (r->y < r2->y) over_top = r2->y;
							else over_top = r->y;

							if (right1 > right2) over_right = right2;
							else over_right = right1;

							if (r->x < r2->x) over_left = r2->x;
							else over_left = r->x;

							over_width=over_right-over_left;
							over_height=over_bottom-over_top;

							if((r1Area * current->category->coalesceOverlap <= over_width*over_height) || (r2Area * current->category->coalesceOverlap <= over_width*over_height))
							{
								ci_debug_printf(10, "srv_classify_image: Merging detected %s at X: %d, Y: %d, Height: %d, Width: %d and X2: %d, Y2: %d, Height2: %d, Width2: %d\n",
										current->category->name, r->x, r->y, r->height, r->width, r2->x, r2->y, r2->height, r2->width);
								tempRect = cvMaxRect( (CvRect*)cvGetSeqElem( newDetected, merged[i] ), r2);
								cvSeqRemove( newDetected, merged[i] );
								cvSeqInsert( newDetected, merged[i], &tempRect );
								merged[j] = merged[i];
							}
						}
					}
				}
			}
			cvClearSeq( current->detected );
			current->detected = newDetected;
			free(merged);
		}
	}
}

static void addImageErrorHeaders(ci_request_t *req, int error)
{
char header[myMAX_HEADER + 1];

	// modify headers
	if (!ci_http_response_headers(req))
		ci_http_response_create(req, 1, 1);

	switch (error)
	{
		case IMAGE_NO_MEMORY:
			snprintf(header, myMAX_HEADER, "X-IMAGE-ERROR: OUT OF MEMORY");
			break;
		case IMAGE_NO_CATEGORIES:
			snprintf(header, myMAX_HEADER, "X-IMAGE-ERROR: NO CATEGORIES PROVIDED, CAN'T PROCESS");
			break;
		case IMAGE_FAILED_LOAD:
			snprintf(header, myMAX_HEADER, "X-IMAGE-ERROR: FAILED TO LOAD IMAGE (Corrupted or Unknown Format)");
			break;
		default:
			snprintf(header, myMAX_HEADER, "X-IMAGE-ERROR: UNKNOWN ERROR");
			break;
	}
	// add IMAGE-CATEGORIES header
	header[myMAX_HEADER] = '\0';
	ci_http_response_add_header(req, header);
	ci_debug_printf(3, "Added error header: %s\n", header);
}

static void createImageClassificationHeaders(image_session_t *mySession)
{
image_detected_t *current;
char header[myMAX_HEADER + 1];
const char *rating = "#########+";
int i;
uint16_t current_category;

	// modify headers
	if (!ci_http_response_headers(mySession->req))
		ci_http_response_create(mySession->req, 1, 1);
	snprintf(header, myMAX_HEADER, "X-IMAGE-CATEGORIES:");
	// Loop through categories
	for(current_category = 0; current_category < num_image_categories; current_category++) {
		current = &mySession->detected[current_category];
		if(current->detected)
		{
			// Loop the number of detected objects
			for(i = 0; i < current->detected->total; i++ )
			{
				// Reset rectangles to original size
				CvRect* r = (CvRect*)cvGetSeqElem( current->detected, i );
				r->x = (int)(r->x * mySession->scale);
				r->y = (int)(r->y * mySession->scale);
				r->height = (int)(r->height * mySession->scale);
				r->width = (int)(r->width * mySession->scale);
			}
			// add each category to header
			if(current->detected->total)
			{
				char *oldHeader = myStrDup(header);
				snprintf(header, myMAX_HEADER, "%s %s(%.*s)", oldHeader, current->category->name, (current->detected->total > 10 ? 10 : current->detected->total), rating);
				free(oldHeader);
			}
			mySession->featuresDetected += current->detected->total;
		}
	}
	// add IMAGE-CATEGORIES header
	header[myMAX_HEADER] = '\0';
	if(mySession->featuresDetected)
	{
		ci_http_response_add_header(mySession->req, header);
		ci_debug_printf(10, "Added header: %s\n", header);

		// Add other headers
		addReferrerHeaders(mySession->req, mySession->referrer_fhs_classification, mySession->referrer_fnb_classification);
	}
}

static void createImageGroupClassificationHeaders(image_session_t *mySession, image_detected_count_t *count)
{
image_detected_count_t *current;
char header[myMAX_HEADER + 1];
const char *rating = "#########+";
uint16_t current_category;

	// modify headers
	if (!ci_http_response_headers(mySession->req))
		ci_http_response_create(mySession->req, 1, 1);
	snprintf(header, myMAX_HEADER, "X-IMAGE-GROUP-CATEGORIES:");
	// Loop through categories
	for(current_category = 0; current_category < num_image_categories; current_category++) {
		current = &count[current_category];
		// add each category to header
		if(current->count)
		{
			char *oldHeader = myStrDup(header);
			snprintf(header, myMAX_HEADER, "%s %s(%.*s)", oldHeader, current->category->name, (current->count > 10 ? 10 : current->count), rating);
			free(oldHeader);
		}
		mySession->featuresDetected += current->count;
	}
	// add IMAGE-CATEGORIES header
	header[myMAX_HEADER] = '\0';
	if(mySession->featuresDetected)
	{
		ci_http_response_add_header(mySession->req, header);
		ci_debug_printf(10, "Added header: %s\n", header);

		// Add other headers
		addReferrerHeaders(mySession->req, mySession->referrer_fhs_classification, mySession->referrer_fnb_classification);
	}
}

static void saveDetectedParts(image_session_t *mySession)
{
image_detected_t *current;
char fname[CI_MAX_PATH + 1];
int i;
uint16_t current_category;

	// Loop through categories
	for(current_category = 0; current_category < num_image_categories; current_category++)
	{
		current = &mySession->detected[current_category];
	        // Loop the number of detected objects	        // Loop the number of detected objects
	        for(i = 0; i < (current->detected ? current->detected->total : 0); i++ )
		{
			// Create a new rectangle for saving the object
			CvRect* r = (CvRect*)cvGetSeqElem( current->detected, i );
			snprintf(fname, CI_MAX_PATH, "%s/detected-%s-%s-%dx%dx%dx%d.png", CLASSIFY_TMP_DIR, current->category->name, (rindex(mySession->fname, '/'))+1,
				r->x, r->y, r->height, r->width);
			fname[CI_MAX_PATH] = '\0';
                        ci_debug_printf(8, "srv_classify_image: Saving detected object to: %s\n", fname);
			cvSetImageROI(mySession->origImage, *r);
			cvSaveImage(fname, mySession->origImage, png_p);
			cvResetImageROI(mySession->origImage);
		}
	}
}

// Draw mask
static void drawMask(image_session_t *mySession)
{
CvPoint pt1, pt2;
	pt1.x = 0;
	pt1.y = 0;
	pt2.x = mySession->origImage->width;
	pt2.y = mySession->origImage->height;
	cvRectangle( mySession->origImage, pt1, pt2, CV_RGB(30,30,30), CV_FILLED, 8, 0 );
}

// Function to draw on original image the detected objects
static void drawDetected(image_session_t *mySession)
{
image_detected_t *current;
int i;
uint16_t current_category;

	for(current_category = 0; current_category < num_image_categories; current_category++)
	{
		current = &mySession->detected[current_category];
	        // Loop the number of detected objects
	        for(i = 0; i < (current->detected ? current->detected->total : 0); i++ )
	        {
			// Create a new rectangle for drawing the object
			CvRect* r = (CvRect*)cvGetSeqElem( current->detected, i );

			// Draw the rectangle in the input image
			// Find the dimensions of the object, and scale it if necessary
			CvPoint center;
			int radius;
			center.x = cvRound(r->x + r->width*0.5);
			center.y = cvRound(r->y + r->height*0.5);
			radius = cvRound((r->width + r->height)*0.25);
			cvCircle( mySession->origImage, center, radius, current->category->Color, 3, 8, 0 );

			ci_debug_printf(10, "srv_classify_image: %s Detected at X: %d, Y: %d, Height: %d, Width: %d\n", current->category->name, r->x, r->y, r->height, r->width);
		}
	}
}

static void saveOriginal(image_session_t *mySession)
{
char fname[CI_MAX_PATH + 1];
	snprintf(fname, CI_MAX_PATH, "%s/original-%s.png", CLASSIFY_TMP_DIR, (rindex(mySession->fname, '/'))+1);
	fname[CI_MAX_PATH] = '\0';
	ci_debug_printf(10, "srv_classify_image: Saving Original Image To: %s\n", fname);
	cvSaveImage(fname, mySession->origImage, png_p);
}

static void saveMarked(image_session_t *mySession)
{
char fname[CI_MAX_PATH + 1];
	snprintf(fname, CI_MAX_PATH, "%s/marked-%s.png", CLASSIFY_TMP_DIR, (rindex(mySession->fname, '/'))+1);
	fname[CI_MAX_PATH] = '\0';
	ci_debug_printf(10, "srv_classify_image: Saving Marked Image To: %s\n", fname);
	cvSaveImage(fname, mySession->origImage, png_p);
}

static void demonstrateDetection(image_session_t *mySession)
{
struct stat stat_buf;
classify_req_data_t *data = ci_service_data(mySession->req);
char imageFILENAME[CI_MAX_PATH + 1];

	// Go back to a file so we can save a modified version
	memBodyToDiskBody(mySession->req);

	ci_http_response_remove_header(mySession->req, "Content-Length");
	data->disk_body->readpos = 0;
	close(data->disk_body->fd);
	unlink(data->disk_body->filename);
	if(strstr(ci_http_response_get_header(mySession->req, "Content-Type"), "jpeg"))
	{
		snprintf(imageFILENAME, CI_MAX_PATH, "%s.jpg", data->disk_body->filename);
		imageFILENAME[CI_MAX_PATH] = '\0';
		cvSaveImage(imageFILENAME, mySession->origImage, jpeg_p);
	}
	else
	{
		snprintf(imageFILENAME, CI_MAX_PATH, "%s.png", data->disk_body->filename);
		imageFILENAME[CI_MAX_PATH] = '\0';
		cvSaveImage(imageFILENAME, mySession->origImage, png_p);
		ci_http_response_remove_header(mySession->req, "Content-Type");
		ci_http_response_add_header(mySession->req, "Content-Type: image/png");
	}
	snprintf(data->disk_body->filename, CI_FILENAME_LEN + 1, "%s", imageFILENAME);
	data->disk_body->filename[CI_FILENAME_LEN] = '\0';
	data->disk_body->fd = open(data->disk_body->filename, O_RDWR | O_EXCL, F_PERM);
	if(fstat(data->disk_body->fd, &stat_buf) == 0)
		data->disk_body->bytes_in = data->disk_body->endpos = stat_buf.st_size;
	else
		data->disk_body->bytes_in = data->disk_body->endpos = 0;
	// Make new content length header
	ci_http_response_remove_header(mySession->req, "Content-Length");
	snprintf(imageFILENAME, CI_MAX_PATH, "Content-Length: %ld", (long) data->disk_body->endpos);
	imageFILENAME[CI_MAX_PATH] = '\0';
	ci_http_response_add_header(mySession->req, imageFILENAME);
}

static void getRightSize(image_session_t *mySession)
{
double t;
int maxDim = mySession->origImage->width > mySession->origImage->height ? mySession->origImage->width : mySession->origImage->height;

	// Create a new image based on the input image
	if(CI_DEBUG_LEVEL >= PERF_DEBUG_LEVEL) t = (double)cvGetTickCount();
	if(maxDim > IMAGE_SCALE_DIMENSION) mySession->scale = maxDim / (float) IMAGE_SCALE_DIMENSION;
	if(mySession->scale > IMAGE_MAX_SCALE) mySession->scale = IMAGE_MAX_SCALE;
	if(mySession->scale < 1.005f) mySession->scale = 1.0f; // We shouldn't waste time with such small rescales
	mySession->rightImage = cvCreateImage( cvSize(cvRound(mySession->origImage->width / mySession->scale), cvRound(mySession->origImage->height / mySession->scale)),
					IPL_DEPTH_8U, 1 );

	if(mySession->scale > 1.0f)
	{
		IplImage *gray = cvCreateImage( cvSize(mySession->origImage->width, mySession->origImage->height), IPL_DEPTH_8U, 1 );
		cvCvtColor( mySession->origImage, gray, CV_BGR2GRAY );
		cvResize( gray, mySession->rightImage, IMAGE_INTERPOLATION );
		cvReleaseImage( &gray );
	}
	else cvCvtColor( mySession->origImage, mySession->rightImage, CV_BGR2GRAY );

	if(mySession->scale != 1.0f) ci_debug_printf(9, "srv_classify_image: Image Resized - X: %d, Y: %d (shrunk by a factor of %f)\n", mySession->rightImage->width, mySession->rightImage->height, mySession->scale);
	if(CI_DEBUG_LEVEL >= PERF_DEBUG_LEVEL)
	{
		t = cvGetTickCount() - t;
		t = t / ((double)cvGetTickFrequency() * 1000.);
		ci_debug_printf(8, "srv_classify_image: Scaling and Color Prep took: %gms.\n", t);
	}
}

LinkedCascade *getFreeCascade(ImageCategory *category)
{
LinkedCascade *item = NULL;
	if(category->freeing_category) return NULL;

	ci_thread_mutex_lock(&category->mutex);
	while(!category->free_cascade)
	{
		ci_thread_cond_wait(&category->cond, &category->mutex);
	}

	// Ok, we should have a free cascade:
	// Remove free cascade item from free list
	item = category->free_cascade;
	category->free_cascade = item->next;

	ci_thread_mutex_unlock(&category->mutex);
return item; // Tell caller the cascade
}

void unBusyCascade(ImageCategory *category, LinkedCascade *item)
{
int need_signal = 0;

	if(item == NULL || category->freeing_category) return;

	if(ci_thread_mutex_lock(&category->mutex) != 0)
	{
		ci_debug_printf(1, "unBusyCascade: failed to lock\n");
	}

        if(!category->free_cascade) need_signal = 1;

	// Add item back to free list
	item->next = category->free_cascade;
	category->free_cascade = item;

	// Clean up and return
	if(need_signal) ci_thread_cond_signal(&category->cond);

	ci_thread_mutex_unlock(&category->mutex);
}

// Function to detect objects
static void detect(image_session_t *mySession)
{
double t = 0;
uint16_t current_category;
LinkedCascade *cascade;

	for(current_category = 0; current_category < num_image_categories; current_category++)
	{
		if(CI_DEBUG_LEVEL >= PERF_DEBUG_LEVEL) t = cvGetTickCount();
		// Find whether the cascade is loaded, to find the objects. If yes, then:
		if(mySession->detected[current_category].category->cascade_array)
		{
			cascade = getFreeCascade(mySession->detected[current_category].category);
			// There can be more than one object in an image. So create a growable sequence of detected objects.
			// Detect the objects and store them in the sequence
#ifdef HAVE_OPENCV
			mySession->detected[current_category].detected = cvHaarDetectObjects( mySession->rightImage, cascade->cascade, mySession->dstorage,
								1.1, 1, 0, cvSize(0, 0) );
#endif
#ifdef HAVE_OPENCV_22X
			mySession->detected[current_category].detected = cvHaarDetectObjects( mySession->rightImage, cascade->cascade, mySession->dstorage,
								1.1, 1, 0, cvSize(0, 0), cvSize(mySession->rightImage->width, mySession->rightImage->height));
#endif
			unBusyCascade(mySession->detected[current_category].category, cascade);
		}
		if(CI_DEBUG_LEVEL >= PERF_DEBUG_LEVEL)
		{
			t = cvGetTickCount() - t;
			t = t / ((double)cvGetTickFrequency() * 1000.);
			ci_debug_printf(8, "srv_classify_image: File: %s Object: %s (%d) Detection took: %gms.\n", (rindex(mySession->fname, '/'))+1, mySession->detected[current_category].category->name, mySession->detected[current_category].detected->total, t);
		}
	}
}

int initImageCategory(const char *name, const char *cascade_loc, const CvScalar Color)
{
ImageCategory *new;
int copies;
int ret = 1;

	if(IMAGE_CATEGORY_COPIES == 0) return 0;
	ci_thread_rwlock_wrlock(&imageclassify_rwlock);

	new = realloc(imageCategories, (num_image_categories + 1) * sizeof(ImageCategory));
	if(new) imageCategories = new;
	else {
		ci_debug_printf(1, "initImageCategory: Couldn't allocate more memory for new categories\n");
		ci_thread_rwlock_unlock(&imageclassify_rwlock);
		return 0;
	}

	// do insertion
	strncpy(imageCategories[num_image_categories].name, name, CATMAXNAME);
	imageCategories[num_image_categories].name[CATMAXNAME] = '\0';
	strncpy(imageCategories[num_image_categories].cascade_location, cascade_loc, CATMAXNAME);
	imageCategories[num_image_categories].cascade_location[CATMAXNAME] = '\0';
	imageCategories[num_image_categories].Color = Color;
	imageCategories[num_image_categories].coalesceOverlap = 1.0f;
	imageCategories[num_image_categories].cascade_array = NULL;
	imageCategories[num_image_categories].free_cascade = NULL;
	imageCategories[num_image_categories].freeing_category = 0;

	// We really treat this as a linked list
	// The structure is arranged as such
	// We allocate it as an array to minimize cache misses
	if((imageCategories[num_image_categories].cascade_array = calloc(IMAGE_CATEGORY_COPIES, sizeof(LinkedCascade))) != NULL)
	{
		for(copies = 0; copies < IMAGE_CATEGORY_COPIES - 1; copies++)
		{
			imageCategories[num_image_categories].cascade_array[copies].cascade = (CvHaarClassifierCascade*) cvLoad(imageCategories[num_image_categories].cascade_location, NULL, NULL, NULL );
			if( !imageCategories[num_image_categories].cascade_array[copies].cascade )
			{
				ci_debug_printf(3, "srv_classify_image: Failed to load cascade for %s\n", imageCategories[num_image_categories].name);
				imageCategories = realloc(imageCategories, sizeof(ImageCategory) * (num_image_categories + 1));
				ret = 0;
			}
			else imageCategories[num_image_categories].cascade_array[copies].next = &imageCategories[num_image_categories].cascade_array[copies + 1];
		}
		imageCategories[num_image_categories].free_cascade = imageCategories[num_image_categories].cascade_array;
	}
	else {
		ci_debug_printf(3, "srv_classify_image: Failed to allocate memory for cascade array for category %s\n", imageCategories[num_image_categories].name);
		imageCategories = realloc(imageCategories, sizeof(ImageCategory) * num_image_categories);
		ret = 0;
	}

	if(ret) num_image_categories++;
	ci_thread_rwlock_unlock(&imageclassify_rwlock);
	return ret;
}

static void freeImageCategories(void)
{
uint16_t current_category, copies;

	if(num_image_categories == 0) return;

	for(current_category = 0; current_category < num_image_categories; current_category++)
	{
		imageCategories[current_category].freeing_category = 1;

		ci_thread_mutex_lock(&imageCategories[current_category].mutex);

		// Free cascades
		for(copies = 0; copies < IMAGE_CATEGORY_COPIES; copies++)
			cvReleaseHaarClassifierCascade(&imageCategories[current_category].cascade_array[copies].cascade);
		free(imageCategories[current_category].cascade_array);

		ci_thread_cond_destroy(&imageCategories[current_category].cond);
		ci_thread_mutex_unlock(&imageCategories[current_category].mutex);
		ci_thread_mutex_destroy(&imageCategories[current_category].mutex);
	}
	free(imageCategories);
	imageCategories = NULL;
	num_image_categories = 0;
}

void classifyImagePrepReload(void)
{
	ci_thread_rwlock_wrlock(&imageclassify_rwlock);
	freeImageCategories();
	imageCategories = NULL;
	ci_thread_rwlock_unlock(&imageclassify_rwlock);
}

static int initImageSession(image_session_t *mySession, ci_request_t *req, ImageCategory *categories, char *filename)
{
uint16_t current_category;

	// Obtain Write Lock
	ci_thread_rwlock_rdlock(&imageclassify_rwlock);

	// Create a sane enivronment, even if we aren't going to do a complete setup
	mySession->scale = 1.0f;
	mySession->detected = NULL;
	mySession->rightImage = NULL;
	mySession->featuresDetected = 0;
	mySession->lstorage = NULL;
	mySession->dstorage = NULL;

	// If there are no categories, we cannot detect anything
	if(num_image_categories == 0)
	{
		ci_debug_printf(3, "srv_classify_image: No categories present. I cannot initiate session.\n");
		ci_thread_rwlock_unlock(&imageclassify_rwlock);
		return IMAGE_NO_CATEGORIES;
	}

	if(filename == NULL)
	{
		filename=tempnam(CLASSIFY_TMP_DIR,"MEM-");
		strncpy(mySession->fname, filename, CI_MAX_PATH);
		mySession->fname[CI_MAX_PATH]='\0';
		free(filename);
	}
	else {
		strncpy(mySession->fname, filename, CI_MAX_PATH);
		mySession->fname[CI_MAX_PATH]='\0';
	}

	// Allocate the memory storage
	if((mySession->dstorage = cvCreateMemStorage(0)) == NULL)
	{
            ci_thread_rwlock_unlock(&imageclassify_rwlock);
            return IMAGE_NO_MEMORY;
	}
	if((mySession->lstorage = cvCreateMemStorage(0)) == NULL)
	{
            ci_thread_rwlock_unlock(&imageclassify_rwlock);
            return IMAGE_NO_MEMORY;
	}
	cvClearMemStorage( mySession->dstorage );
	cvClearMemStorage( mySession->lstorage );

	if (!(mySession->detected = ci_object_pool_alloc(IMAGEDETECTED_POOL))) {
		ci_debug_printf(1,
                       "Error allocation memory for service data!!!!!!!\n");
		ci_thread_rwlock_unlock(&imageclassify_rwlock);
		return IMAGE_NO_MEMORY;
	}
	for(current_category = 0; current_category < num_image_categories; current_category++)
	{
		mySession->detected[current_category].category = &imageCategories[current_category];
		mySession->detected[current_category].detected = NULL;
	}
	// Remember c_icap request
	mySession->req = req;

	// Release Write Lock
	ci_thread_rwlock_unlock(&imageclassify_rwlock);
	return 0;
}

static void clearImageDetected(image_detected_t *detected)
{
	if(detected == NULL) return;
	for(uint16_t current_category = 0; current_category < num_image_categories; current_category++)
        {
		if(detected[current_category].detected) cvClearSeq( detected[current_category].detected );
	}
}

static void freeImageDetected(image_detected_t *detected)
{
	if(detected == NULL) return;
	for(uint16_t current_category = 0; current_category < num_image_categories; current_category++)
        {
		if(detected[current_category].detected) cvClearSeq( detected[current_category].detected );
	}

	ci_object_pool_free(detected);
}

static void freeImageDetectedCount(image_detected_count_t *detected)
{
	if(detected == NULL) return;

	ci_object_pool_free(detected);
}

static void deinitImageSession(image_session_t *mySession)
{
	if(!mySession) return;

	// Deallocate detected areas
        if(mySession->detected) freeImageDetected(mySession->detected);
	// Release the image memory
	if(mySession->origImage) cvReleaseImage(&mySession->origImage);
	if(mySession->rightImage) cvReleaseImage(&mySession->rightImage);
	// OpenCV memory management cleanup
	if(mySession->dstorage)
	{
		cvClearMemStorage( mySession->dstorage );
		cvReleaseMemStorage( &mySession->dstorage );
	}
	if(mySession->lstorage)
	{
		cvClearMemStorage( mySession->lstorage );
		cvReleaseMemStorage( &mySession->lstorage );
	}
}

int categorize_image(ci_request_t *req)
{
classify_req_data_t *data = ci_service_data(req);
image_session_t mySession;
int minDim = 0, ret = 0;
CvMat myCvMat;

    // Don't bother to do anything if there is nothing to be done
    if(num_image_categories == 0)
    {
        ci_debug_printf(3, "srv_classify_image: No categories present. Ignoring. You are seeing this because you have no image categories set, but have a srv_classify.ImageFileTypes option set.\n");
        addImageErrorHeaders(req, IMAGE_NO_CATEGORIES);
        return CI_OK;
    }

    // Load the image from that filename
    if(data->mem_body)
    {
        myCvMat = cvMat(1, data->mem_body->endpos, CV_8UC3, data->mem_body->buf);
        mySession.origImage = cvDecodeImage(&myCvMat, CV_LOAD_IMAGE_COLOR);
        ci_debug_printf(8, "Classifying IMAGE from memory (size=%d)\n", data->mem_body->endpos);
    }
    else {
	mySession.origImage = cvLoadImage(data->disk_body->filename, CV_LOAD_IMAGE_COLOR);
        ci_debug_printf(8, "Classifying IMAGE from file (size=%ld)\n", data->disk_body->endpos);
    }

    // If Image is loaded succesfully, then:
    if( mySession.origImage )
    {
        // test to see if we should bypass classification
        minDim = mySession.origImage->width < mySession.origImage->height ? mySession.origImage->width : mySession.origImage->height;
        if(IMAGE_MIN_PROCESS >= minDim || minDim < 5)
        {
            ci_debug_printf(10, "srv_classify_image: Image too small for classification per configuration and/or sanity, letting pass.\n");
            cvReleaseImage(&mySession.origImage);
            return CI_OK; // Image is too small, let it pass
        }

	// Setup Session
	ret = initImageSession(&mySession, req, imageCategories, data->mem_body ? NULL : data->disk_body->filename);
	if(ret < 0)
        {
		addImageErrorHeaders(req, ret);
		deinitImageSession(&mySession);
		return CI_ERROR;
        }
	getRightSize(&mySession);

	// Obtain Read Lock
	ci_thread_rwlock_rdlock(&imageclassify_rwlock);

	// Grab Referrer Classification before it is too late
	getReferrerClassification(ci_http_request_get_header((ci_request_t *)req, "Referer"), &mySession.referrer_fhs_classification, &mySession.referrer_fnb_classification);

        // Detect and add headers
	detect(&mySession);
	doCoalesce(&mySession);
	createImageClassificationHeaders(&mySession);

	// Debug stuff - should all have configuration options
	if(mySession.featuresDetected)
	{
		if(IMAGE_DEBUG_SAVE_ORIG) saveOriginal(&mySession);
	        if(IMAGE_DEBUG_SAVE_PARTS) saveDetectedParts(&mySession);
		if(IMAGE_DEBUG_SAVE_MARKED)
		{
			drawDetected(&mySession);
			saveMarked(&mySession);
		}
		if(IMAGE_DEBUG_DEMONSTRATE || IMAGE_DEBUG_DEMONSTRATE_MASKED)
		{
			if(IMAGE_DEBUG_DEMONSTRATE_MASKED)
				drawMask(&mySession);
			drawDetected(&mySession);
			demonstrateDetection(&mySession);
		}
	}

	// Deinit Session
	deinitImageSession(&mySession);

        // Release Read Lock
        ci_thread_rwlock_unlock(&imageclassify_rwlock);
    }
    else {
	if(data->mem_body)
	{
                ci_debug_printf(8, "srv_classify_image: Could not load image from memory for classification.\n");
	}
	else
	{
                ci_debug_printf(8, "srv_classify_image: Could not load image (%s) for classification.\n", data->disk_body->filename);
	}
	addImageErrorHeaders(req, IMAGE_FAILED_LOAD);
        return CI_ERROR;
    }

    return CI_OK;
}

int categorize_external_image(ci_request_t * req)
{
classify_req_data_t *data = ci_service_data(req);
image_session_t mySession;
int minDim = 0, ret = 0;
char buff[512];
char CALL_OUT[CI_MAX_PATH + 1];
int i = 0;
pid_t child_pid;
int wait_status;
DIR *dirp;
struct dirent *dp;
char old_dir[PATH_MAX];
char **localargs;
image_detected_count_t *count = NULL;
uint16_t current_category;

	// Don't bother to do anything if there is nothing to be done
	if(num_image_categories == 0)
	{
		ci_debug_printf(3, "srv_classify_image: No categories present. Ignoring. You are seeing this because you have no image categories set, but have a srv_classify.ImageFileTypes option set.\n");
		addImageErrorHeaders(req, IMAGE_NO_CATEGORIES);
		return CI_OK;
	}

	mySession.origImage = NULL;

	snprintf(CALL_OUT, CI_MAX_PATH, "%s/EXT_IMAGE-XXXXXX", CLASSIFY_TMP_DIR);
	data->external_body = malloc(sizeof(ci_simple_file_t));
	strncpy(data->external_body->filename, mkdtemp(CALL_OUT), CI_MAX_PATH);

	child_pid = fork();
	if(child_pid == 0) // We are the child
	{
		i = 0;
		while(externalclassifytypes[data->file_type].image_args[i] != NULL)
			i++;
		localargs = malloc(sizeof(char *) * (i + 2));
		i = 0;
		while(externalclassifytypes[data->file_type].image_args[i] != NULL)
		{
			ret = ci_format_text(req, externalclassifytypes[data->file_type].image_args[i], buff, 511, srv_classify_image_format_table);
			buff[511] = '\0'; // Terminate for safety!
			localargs[i + 1] = myStrDup(buff);
			i++;
		}
		localargs[i + 1] = NULL;
		localargs[0] = myStrDup(externalclassifytypes[data->file_type].image_program);
		ret = execv(externalclassifytypes[data->file_type].image_program, localargs);
		free(localargs);
	}
	else if(child_pid < 0)
	{
		// Error condition
		ci_debug_printf(3, "categorize_external_image: categorize_external_image: failed to fork\n");
	}
	else { // We are the original
		waitpid(child_pid, &wait_status, 0);

		getcwd(old_dir, PATH_MAX);
		chdir(data->external_body->filename);

		if ((dirp = opendir(data->external_body->filename)) == NULL)
		{
			ci_debug_printf(3, "srv_classify_image: categorize_external_image: couldn't open '%s'", data->external_body->filename);
			addImageErrorHeaders(req, IMAGE_FAILED_LOAD);
			return CI_ERROR;
		}

		// Setup Session
		ret = initImageSession(&mySession, req, imageCategories, "/categorize_external_image");
		if(ret < 0)
		{
			addImageErrorHeaders(req, ret);
			deinitImageSession(&mySession);
			unlink_directory(data->external_body->filename);
			free(data->external_body);
			closedir(dirp);
			return CI_ERROR;
		}

		// Grab Referrer Classification before it is too late
		getReferrerClassification(ci_http_request_get_header((ci_request_t *)req, "Referer"), &mySession.referrer_fhs_classification, &mySession.referrer_fnb_classification);

		// Create detected count structure
		if((count = ci_object_pool_alloc(IMAGEDETECTEDCOUNT_POOL)) == NULL)
		{
			ci_debug_printf(1, "srv_classify_image: categorize_external_image: couldn't allocate memory");
			closedir(dirp);
			return IMAGE_NO_MEMORY;
		}		
		for(current_category = 0; current_category < num_image_categories; current_category++)
		{
			count[current_category].category = &imageCategories[current_category];
			count[current_category].count = 0;
		}

		// Obtain Read Lock - Write lock not grabbed in normal operations,
		// so it is best to hold read lock for a long time, rather than grab/release/grab/release
		ci_thread_rwlock_rdlock(&imageclassify_rwlock);
		do {
			errno = 0;
			if ((dp = readdir(dirp)) != NULL)
			{
				if (strstr(dp->d_name, ".png") != NULL || strstr(dp->d_name, ".jpg") != NULL || strstr(dp->d_name, ".ppm") != NULL)
				{
					// Load the image from that filename
					mySession.origImage = cvLoadImage( dp->d_name, CV_LOAD_IMAGE_COLOR );
					// If Image is loaded succesfully, then:
					if( mySession.origImage )
					{
						// test to see if we should bypass classification
						minDim = mySession.origImage->width < mySession.origImage->height ? mySession.origImage->width : mySession.origImage->height;
						if(IMAGE_MIN_PROCESS >= minDim || minDim < 5)
						{
							ci_debug_printf(10, "categorize_external_image: Image too small for classification per configuration and/or sanity, letting pass.\n");
							cvReleaseImage(&mySession.origImage);
							mySession.origImage = NULL;
						}
						else {
							getRightSize(&mySession);

							// Detect
							detect(&mySession);
							doCoalesce(&mySession);

							// Loop through categories
							for(current_category = 0; current_category < num_image_categories; current_category++)
							{
								// Loop the number of detected objects
								if(mySession.detected[current_category].detected->total > count->count)
									count->count = mySession.detected[current_category].detected->total;
							}
							// We won't use the detected again, clear it
							clearImageDetected(mySession.detected);

							// We are finished with this image
							if(mySession.origImage) cvReleaseImage(&mySession.origImage);
							if(mySession.rightImage) cvReleaseImage(&mySession.rightImage);
							mySession.origImage = NULL;
							mySession.rightImage = NULL;
						}
					}
					else {
						ci_debug_printf(8, "categorize_external_image: Could not load image (%s) for classification.\n", data->disk_body->filename);
					}
				}
			}
		} while (dp != NULL);
		if (errno != 0)
			perror("categorize_external_image: error reading directory");
		else
			(void) closedir(dirp);

		chdir(old_dir);
		// ADD HEADERS
		createImageGroupClassificationHeaders(&mySession, count);

		// Clean up
		// Free Detected Count Structure
		freeImageDetectedCount(count);

		// Deinit Session
		deinitImageSession(&mySession);

		// Release Read Lock
		ci_thread_rwlock_unlock(&imageclassify_rwlock);

		// Cleanup directory and get rid of external body
		unlink_directory(data->external_body->filename);
		free(data->external_body);
		data->external_body = NULL;

		return CI_OK;
	}
	return CI_ERROR;
}

/*************************************************************************************/
/* Template Functions                                                                */
int fmt_srv_classify_image_source(ci_request_t *req, char *buf, int len, const char *param)
{
    classify_req_data_t *data = ci_service_data(req);
    if (! data->disk_body || strlen(data->disk_body->filename) <= 0)
        return 0;

    return snprintf(buf, len, "%s", data->disk_body->filename);
}

int fmt_srv_classify_image_destination_directory(ci_request_t *req, char *buf, int len, const char *param)
{
    classify_req_data_t *data = ci_service_data(req);
    if (! data->external_body || strlen(data->external_body->filename) <= 0)
        return 0;

    return snprintf(buf, len, "%s/", data->external_body->filename);
}

/****************************************************************************************/
/*Configuration Functions                                                               */
int cfg_AddImageCategory(const char *directive, const char **argv, void *setdata)
{
     int ret;
     unsigned int RED=0xFF, GREEN=0XFF, BLUE=0XFF;

     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "Format: %s NAME LOCATION_OF_CASCADE_XML OPTIONAL_RED_OUTLINE_HEX OPTIONAL_GREEN_OUTLINE_HEX OPTIONAL_BLUE_OUTLINE_HEX\n", directive);
          return 0;
     }
     if(argv[2] != NULL)
     {
           sscanf(argv[2], "%x", &RED);
           if(argv[3] != NULL)
           {
                sscanf(argv[3], "%x", &GREEN);
                if(argv[4] != NULL) sscanf(argv[4], "%x", &BLUE);
           }
    }
    ret = initImageCategory(argv[0], argv[1], CV_RGB(RED, GREEN, BLUE));
    ci_debug_printf(1, "Setting parameter :%s (Name: %s Cascade File: %s Red: 0x%x Green: 0x%x Blue: 0x%x)\n", directive, argv[0], argv[1], RED, GREEN, BLUE);
    return ret;
}

int cfg_ImageInterpolation(const char *directive, const char **argv, void *setdata)
{
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "ImageScaleFunction should be set to one of the following:\n" \
				"NN - nearest-neigbor interpolation (fastest),\n" \
				"LINEAR - bilinear interpolation (default),\n" \
				"AREA - resampling using pixel area relation,\n" \
				"CUBIC - bicubic interpolation (slowest).\n");
          return 0;
     }
     if(strcasecmp(argv[0], "NN") == 0) IMAGE_INTERPOLATION=CV_INTER_NN;
     else if(strcasecmp(argv[0], "LINEAR") == 0) IMAGE_INTERPOLATION=CV_INTER_LINEAR;
     else if(strcasecmp(argv[0], "AREA") == 0) IMAGE_INTERPOLATION=CV_INTER_AREA;
     else if(strcasecmp(argv[0], "CUBIC") == 0) IMAGE_INTERPOLATION=CV_INTER_CUBIC;
     else {
          ci_debug_printf(1, "Unknown option in directive: %s\n", directive);
	return 0;
     }
     ci_debug_printf(1, "Setting parameter :%s=%s\n", directive, argv[0]);
     return 1;
}

int cfg_coalesceOverlap(const char *directive, const char **argv, void *setdata)
{
uint16_t current_category = 0;
     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          return 0;
     }
     for(current_category = 0; current_category < num_image_categories; current_category++)
     {
          if(strcasecmp(argv[0], imageCategories[current_category].name) == 0)
          {
              sscanf(argv[1], "%f", &imageCategories[current_category].coalesceOverlap);
              ci_debug_printf(1, "Setting parameter :%s=(%s,%s)\n", directive, argv[0], argv[1]);
              return 1;
          }
     }
     ci_debug_printf(1, "Category in directive %s not found. Add category before setting coalesce overlap.\n", directive);
     return 0;
}

int cfg_imageCategoryCopies(const char *directive, const char **argv, void *setdata)
{
     if(imageCategories != NULL)
     {
          ci_debug_printf(1, "%s must be in the configuration file before ANY image categories are added!\n", directive);
          return 0;
     }
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          return 0;
     }
     sscanf(argv[0], "%d", &IMAGE_CATEGORY_COPIES);
     if(IMAGE_CATEGORY_COPIES == -1) IMAGE_CATEGORY_COPIES = CFG_NUM_CICAP_THREADS;
     else if(IMAGE_CATEGORY_COPIES < IMAGE_CATEGORY_COPIES_MIN)
          IMAGE_CATEGORY_COPIES = IMAGE_CATEGORY_COPIES_MIN;
     if(IMAGE_CATEGORY_COPIES > CFG_NUM_CICAP_THREADS) IMAGE_CATEGORY_COPIES = CFG_NUM_CICAP_THREADS;

     ci_debug_printf(1, "Setting parameter :%s=(%d)\n", directive, IMAGE_CATEGORY_COPIES);
     return 1;
}
