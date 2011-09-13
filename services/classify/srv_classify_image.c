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
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>

#include "c-icap.h"
#include "service.h"
#include "header.h"
#include "body.h"
#include "simple_api.h"
#include "debug.h"
#include "cfg_param.h"
#include "ci_threads.h"

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

extern char *CLASSIFY_TMP_DIR; // temp directory


extern int ImageScaleDimension; // Scale to this dimension
extern int ImageMaxScale; // Maximum rescale
extern int ImageMinProcess; // don't process images whose minimum dimesnion is under this size
extern int IMAGE_DEBUG_SAVE_ORIG;
extern int IMAGE_DEBUG_SAVE_PARTS;
extern int IMAGE_DEBUG_SAVE_MARKED;
extern int IMAGE_DEBUG_DEMONSTRATE;
extern int IMAGE_DEBUG_DEMONSTRATE_MASKED;
extern int IMAGE_INTERPOLATION;
extern int IMAGE_CATEGORY_COPIES;


ImageCategory *imageCategories = NULL;

/* Locking */
ci_thread_rwlock_t imageclassify_rwlock;

// OpenCV saving types
int png_p[] = {CV_IMWRITE_PNG_COMPRESSION, 9, 0};
int jpeg_p[] = {CV_IMWRITE_JPEG_QUALITY, 100, 0};

static void doCoalesce(ImageSession *mySession)
{
CvSeq *newDetected = NULL;
ImageDetected *current = mySession->detected;
CvRect tempRect;
int over_left, over_top;
int right1, right2, over_right;
int bottom1, bottom2, over_bottom;
int over_width, over_height;
int r1Area, r2Area;
int *merged = NULL;
int newI = 0;
int i, j;

	while(current != NULL)
	{
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
								ci_debug_printf(10, "Merging detected %s at X: %d, Y: %d, Height: %d, Width: %d and X2: %d, Y2: %d, Height2: %d, Width2: %d\n",
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
		current=current->next;
	}
}

static void addImageErrorHeaders(ImageSession *mySession, int error)
{
char header[myMAX_HEADER+1];

	// modify headers
	if (!ci_http_response_headers(mySession->req))
		ci_http_response_create(mySession->req, 1, 1);

	switch(error)
	{
		case NO_IMAGE_MEMORY:
			snprintf(header, myMAX_HEADER, "X-IMAGE-ERROR: OUT OF MEMORY");
			break;
		case NO_IMAGE_CATEGORIES:
			snprintf(header, myMAX_HEADER, "X-IMAGE-ERROR: NO CATEGORIES PROVIDED, CAN'T PROCESS");
			break;
		default:
			snprintf(header, myMAX_HEADER, "X-IMAGE-ERROR: UNKNOWN ERROR");
			break;
	}
	// add IMAGE-CATEGORIES header
	header[myMAX_HEADER]='\0';
	ci_http_response_add_header(mySession->req, header);
	ci_debug_printf(3, "Added error header: %s\n", header);
}

static void createImageClassificationHeaders(ImageSession *mySession)
{
ImageDetected *current = mySession->detected;
char header[myMAX_HEADER+1];
const char *rating = "#########+";
int i;

	// modify headers
	if (!ci_http_response_headers(mySession->req))
		ci_http_response_create(mySession->req, 1, 1);
	snprintf(header, myMAX_HEADER, "X-IMAGE-CATEGORIES:");
	// Loop through categories
	while (current != NULL) {
	        // Loop the number of detected objects
	        for(i = 0; i < (current->detected ? current->detected->total : 0); i++ )
		{
			// Reset rectangles to original size
			CvRect* r = (CvRect*)cvGetSeqElem( current->detected, i );
			r->x=(int)(r->x * mySession->scale);
			r->y=(int)(r->y * mySession->scale);
			r->height=(int)(r->height * mySession->scale);
			r->width=(int)(r->width * mySession->scale);
		}
		// add each category to header
		if(current->detected->total)
		{
			char *oldHeader = myStrDup(header);
			snprintf(header, myMAX_HEADER, "%s %s(%.*s)", oldHeader, current->category->name, (current->detected->total > 10 ? 10 : current->detected->total), rating);
			free(oldHeader);
		}
		mySession->featuresDetected += current->detected->total;

		current=current->next;
	}
	// add IMAGE-CATEGORIES header
	header[myMAX_HEADER] = '\0';
	if(mySession->featuresDetected)
	{
		ci_http_response_add_header(mySession->req, header);
		ci_debug_printf(10, "Added header: %s\n", header);
	}
	// TODO: Add other headers
	addReferrerHeaders(mySession->req, mySession->referrer_fhs_classification, mySession->referrer_fnb_classification);
}

static void saveDetectedParts(ImageSession *mySession)
{
ImageDetected *current=mySession->detected;
char fname[CI_MAX_PATH+1];
int i;

	// Loop through categories
	while (current != NULL) {
	        // Loop the number of detected objects
	        for(i = 0; i < (current->detected ? current->detected->total : 0); i++ )
		{
			// Create a new rectangle for saving the object
			CvRect* r = (CvRect*)cvGetSeqElem( current->detected, i );
			snprintf(fname, CI_MAX_PATH, "%s/detected-%s-%s-%dx%dx%dx%d.png", CLASSIFY_TMP_DIR, current->category->name, (rindex(mySession->fname, '/'))+1,
				r->x, r->y, r->height, r->width);
			fname[CI_MAX_PATH] = '\0';
                        ci_debug_printf(8, "Saving detected object to: %s\n", fname);
			cvSetImageROI(mySession->origImage, *r);
			cvSaveImage(fname, mySession->origImage, png_p);
			cvResetImageROI(mySession->origImage);
		}
		current=current->next;
	}
}

// Draw mask
static void drawMask(ImageSession *mySession)
{
CvPoint pt1, pt2;
	pt1.x = 0;
	pt1.y = 0;
	pt2.x = mySession->origImage->width;
	pt2.y = mySession->origImage->height;
	cvRectangle( mySession->origImage, pt1, pt2, CV_RGB(30,30,30), CV_FILLED, 8, 0 );
}


// Function to draw on original image the detected objects
static void drawDetected(ImageSession *mySession)
{
ImageDetected *current=mySession->detected;
int i;

	while(current != NULL)
	{
	        // Loop the number of detected objects
	        for(i = 0; i < (current->detected ? current->detected->total : 0); i++ )
	        {
			// Create a new rectangle for drawing the face
			CvRect* r = (CvRect*)cvGetSeqElem( current->detected, i );

			// Draw the rectangle in the input image
			// Find the dimensions of the face,and scale it if necessary
/*			pt1.x = r->x*myscale;
			pt2.x = (r->x+r->width)*myscale;
			pt1.y = r->y*myscale;
			pt2.y = (r->y+r->height)*myscale;
			cvRectangle( img, pt1, pt2, CV_RGB(255,0,0), 3, 8, 0 );*/

			CvPoint center;
			int radius;
			center.x = cvRound(r->x + r->width*0.5);
			center.y = cvRound(r->y + r->height*0.5);
			radius = cvRound((r->width + r->height)*0.25);
			cvCircle( mySession->origImage, center, radius, current->category->Color, 3, 8, 0 );

			ci_debug_printf(10, "%s Detected at X: %d, Y: %d, Height: %d, Width: %d\n", current->category->name, r->x, r->y, r->height, r->width);
		}
		current=current->next;
	}
}

static void saveOriginal(ImageSession *mySession)
{
char fname[CI_MAX_PATH+1];
	snprintf(fname, CI_MAX_PATH, "%s/original-%s.png", CLASSIFY_TMP_DIR, (rindex(mySession->fname, '/'))+1);
	fname[CI_MAX_PATH] = '\0';
	ci_debug_printf(10, "Saving Original Image To: %s\n", fname);
	cvSaveImage(fname, mySession->origImage, png_p);
}

static void saveMarked(ImageSession *mySession)
{
char fname[CI_MAX_PATH+1];
	snprintf(fname, CI_MAX_PATH, "%s/marked-%s.png", CLASSIFY_TMP_DIR, (rindex(mySession->fname, '/'))+1);
	fname[CI_MAX_PATH] = '\0';
	ci_debug_printf(10, "Saving Marked Image To: %s\n", fname);
	cvSaveImage(fname, mySession->origImage, png_p);
}

static void demonstrateDetection(ImageSession *mySession)
{
struct stat stat_buf;
classify_req_data_t *data = ci_service_data(mySession->req);
char imageFILENAME[CI_MAX_PATH+1];

        ci_http_response_remove_header(mySession->req, "Content-Length");
	data->body->readpos = 0;
	close(data->body->fd);
	unlink(data->body->filename);
	snprintf(imageFILENAME, CI_MAX_PATH, "%s.%s", data->body->filename, data->type_name);
	imageFILENAME[CI_MAX_PATH] = '\0';
        if(strstr(imageFILENAME, "jpg"))
		cvSaveImage(imageFILENAME, mySession->origImage, jpeg_p);
        else
		cvSaveImage(imageFILENAME, mySession->origImage, png_p);
	snprintf(data->body->filename, CI_FILENAME_LEN + 1, "%s", imageFILENAME);
	data->body->filename[CI_FILENAME_LEN] = '\0';
	data->body->fd = open(data->body->filename, O_RDWR | O_EXCL, F_PERM);
	fstat(data->body->fd, &stat_buf);
	data->body->endpos = stat_buf.st_size;
	// Make new content length header
	snprintf(imageFILENAME, CI_MAX_PATH, "Content-Length: %ld", (long) data->body->endpos);
	imageFILENAME[CI_MAX_PATH] = '\0';
        ci_http_response_add_header(mySession->req, imageFILENAME);
}

static void getRightSize(ImageSession *mySession)
{
double t;
int maxDim = mySession->origImage->width > mySession->origImage->height ? mySession->origImage->width : mySession->origImage->height;

	// Create a new image based on the input image
	t = (double)cvGetTickCount();
	if(maxDim > ImageScaleDimension) mySession->scale = maxDim / (float) ImageScaleDimension;
	if(mySession->scale > ImageMaxScale) mySession->scale = ImageMaxScale;
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
	//cvEqualizeHist( small_img, small_img ); // Removed as it does weird things and drops accuracy

	if(mySession->scale != 1.0f) ci_debug_printf(9, "Image Resized - X: %d, Y: %d (shrunk by a factor of %f)\n", mySession->rightImage->width, mySession->rightImage->height, mySession->scale);
	t = cvGetTickCount() - t;
	t = t / ((double)cvGetTickFrequency() * 1000.);
	ci_debug_printf(8, "Scaling and Color Prep took: %gms.\n", t);
}

LinkedCascade *getFreeCascade(ImageCategory *category)
{
struct timespec sleep_time = { 0, 100000};
LinkedCascade *item = NULL;
int attempts = 0;
	if(category->freeing_category) return NULL;
	getfreecascade_retry:
	if(category->free_cascade)
	{
		ci_thread_mutex_lock(&category->mutex);
		if(!category->free_cascade) goto getfreecascade_retry_setup;
		else
		{
			// Remove free cascade item from free list
			item = category->free_cascade;
			category->free_cascade = item->next;
			// Add item to busy list
			item->next = category->busy_cascade;
			category->busy_cascade = item;
			if(attempts >= 4) ci_debug_printf(3, "Had to wait on cascade %s, consider increasing ImageCategoryCopies in configuration. Currently set to %d. Retried %d times.\n", category->name, IMAGE_CATEGORY_COPIES, attempts);

		}
	}
	else {
		getfreecascade_retry_setup:
		ci_thread_mutex_unlock(&category->mutex);
		attempts++;
		nanosleep(&sleep_time, NULL);
		goto getfreecascade_retry;
	}
	ci_thread_mutex_unlock(&category->mutex);
return item;
}

void unBusyCascade(ImageCategory *category, LinkedCascade *item)
{
LinkedCascade *current = category->busy_cascade;
	if(item == NULL || current == NULL) return;
	ci_thread_mutex_lock(&category->mutex);
	while(current != NULL)
	{
		if(current->next == item || current == item)
		{
			// Remove item from busy list
			if(category->busy_cascade == item) category->busy_cascade = item->next;
			else if(current->next) current->next = current->next->next;
			else current->next = NULL;
			// Add item back to free list
			item->next = category->free_cascade;
			category->free_cascade = item;
			// Clean up and return
			ci_thread_mutex_unlock(&category->mutex);
			return;
		}
		current = current->next;
	}
	ci_thread_mutex_unlock(&category->mutex);
}

// Function to detect objects
static void detect(ImageSession *mySession)
{
double t = 0;
ImageDetected *current = mySession->detected;
LinkedCascade *cascade;

	while(current != NULL)
	{
		t = cvGetTickCount();
		// Find whether the cascade is loaded, to find the objects. If yes, then:
		if(current->category->free_cascade || current->category->busy_cascade)
		{
			cascade = getFreeCascade(current->category);
			// There can be more than one object in an image. So create a growable sequence of detected objects.
			// Detect the objects and store them in the sequence
#ifdef HAVE_OPENCV
			current->detected = cvHaarDetectObjects( mySession->rightImage, cascade->cascade, mySession->dstorage,
								1.1, 1, 0, cvSize(0, 0) );
#endif
#ifdef HAVE_OPENCV_22X
			current->detected = cvHaarDetectObjects( mySession->rightImage, cascade->cascade, mySession->dstorage,
								1.1, 1, 0, cvSize(0, 0), cvSize(mySession->rightImage->width, mySession->rightImage->height));
#endif
			unBusyCascade(current->category, cascade);
		}
		t = cvGetTickCount() - t;
		t = t / ((double)cvGetTickFrequency() * 1000.);
		ci_debug_printf(8, "File: %s Object: %s (%d) Detection took: %gms.\n", (rindex(mySession->fname, '/'))+1, current->category->name, current->detected->total, t);

		current=current->next;
	}
}

ImageCategory *initImageCategory(ImageCategory *top, const char *name, const char *cascade_loc, const CvScalar Color)
{
ImageCategory *current = top, *recover = NULL;
LinkedCascade *new_cascade;
int copies;

	ci_thread_rwlock_wrlock(&imageclassify_rwlock);

	if(current != NULL)
	{
		// scan for place to insert
		while (current->next != NULL) current=current->next;
		// Save our recovery point
		recover = current;
		// Malloc and setup
		current->next = malloc(sizeof(ImageCategory));
		current = current->next;
		current->next = NULL;
	}
	else
	{
		current = malloc(sizeof(ImageCategory));
		top = current;
		current->next = NULL;
	}

	// do insertion
	strncpy(current->name, name, CATMAXNAME);
	current->name[CATMAXNAME] = '\0';
	strncpy(current->cascade_location, cascade_loc, CATMAXNAME);
	current->cascade_location[CATMAXNAME] = '\0';
	current->Color = Color;
	current->coalesceOverlap = 1.0f;
	current->free_cascade = NULL;
	current->busy_cascade = NULL;
	current->freeing_category = 0;

	ci_thread_mutex_init(&current->mutex);
	ci_thread_mutex_lock(&current->mutex);

	for(copies = 0; copies < IMAGE_CATEGORY_COPIES; copies++)
	{
		new_cascade = calloc(1, sizeof(LinkedCascade));
		new_cascade->cascade = (CvHaarClassifierCascade*) cvLoad(current->cascade_location, NULL, NULL, NULL );
		if( !new_cascade->cascade )
		{
			ci_debug_printf(3, "Failed to load cascade for %s\n", current->name);
			free(new_cascade);
			copies = IMAGE_CATEGORY_COPIES;
			if(copies == 0)
			{
				top = NULL;
				if(recover)
				{
					free(recover->next);
					recover->next = NULL;
				}
			}
		}
		else
		{
			ci_debug_printf(10, "Successfully loaded cascade for %s\n", current->name);
			new_cascade->next = current->free_cascade;
			current->free_cascade = new_cascade;
		}
	}
	ci_thread_mutex_unlock(&current->mutex);
	ci_thread_rwlock_unlock(&imageclassify_rwlock);
	return top;
}

static void freeImageCategories(ImageCategory *top)
{
struct timespec sleep_time = { 0, 100000000};
LinkedCascade *next;

	if(top == NULL) return;

	if(top->next != NULL)
        {
		freeImageCategories(top->next);
		top->next = NULL;
	}
	top->freeing_category = 1;
	while(top->busy_cascade) nanosleep(&sleep_time, NULL);

	ci_thread_mutex_lock(&top->mutex);
	while(top->free_cascade)
	{
		next = top->free_cascade->next;;
		cvReleaseHaarClassifierCascade(&top->free_cascade->cascade);
		free(top->free_cascade);
		top->free_cascade = next;
	}
	ci_thread_mutex_unlock(&top->mutex);
	ci_thread_mutex_destroy(&top->mutex);
	free(top);
        top = NULL;
}

void classifyImagePrepReload(void)
{
	ci_thread_rwlock_wrlock(&imageclassify_rwlock);
	freeImageCategories(imageCategories);
	imageCategories = NULL;
	ci_thread_rwlock_unlock(&imageclassify_rwlock);
}

static int initImageSession(ImageSession *mySession, ci_request_t *req, ImageCategory *categories, const char *filename)
{
ImageCategory *current = categories;
ImageDetected *nDetected = NULL, *cDetected = NULL;

	// Obtain Write Lock
	ci_thread_rwlock_rdlock(&imageclassify_rwlock);

	// If there are no categories, we cannot detect anything
	if(current == NULL)
	{
		ci_debug_printf(3, "No categories present. I cannot initiate session.\n");
		return NO_CATEGORIES;
	}

	mySession->scale = 1.0f;
	mySession->detected = NULL;
	mySession->rightImage = NULL;
	mySession->featuresDetected = 0;
	strncpy(mySession->fname, filename, CI_MAX_PATH);
	mySession->fname[CI_MAX_PATH]='\0';

	// Allocate the memory storage
	if((mySession->dstorage = cvCreateMemStorage(0)) == NULL)
            return NO_MEMORY;
	if((mySession->lstorage = cvCreateMemStorage(0)) == NULL)
            return NO_MEMORY;
	cvClearMemStorage( mySession->dstorage );
	cvClearMemStorage( mySession->lstorage );
	while(current != NULL)
	{
		if((nDetected = malloc(sizeof(ImageDetected))) == NULL)
			return NO_MEMORY;
		nDetected->category = current;
		nDetected->detected = NULL;
		nDetected->next = NULL;
		if(mySession->detected == NULL)
		{
			mySession->detected = nDetected;
			cDetected = nDetected;
		}
		else
		{
			cDetected->next = nDetected;
			cDetected = cDetected->next;
		}
		current = current->next;
	}
	// Remember c_icap request
	mySession->req = req;

	// Release Write Lock
	ci_thread_rwlock_unlock(&imageclassify_rwlock);
	return 0;
}

static void freeImageDetected(ImageDetected *detected)
{
	if(detected==NULL) return;
	if(detected->next!=NULL)
        {
		cvClearSeq( detected->detected );
		freeImageDetected(detected->next);
		detected->next = NULL;
	}
	free(detected);
}

static void deinitImageSession(ImageSession *mySession)
{
	// FIXME: Deallocate detected areas
        freeImageDetected(mySession->detected);
	// Release the image memory
	if(mySession->origImage) cvReleaseImage(&mySession->origImage);
	if(mySession->rightImage) cvReleaseImage(&mySession->rightImage);
	// OpenCV memory management cleanup
	cvClearMemStorage( mySession->dstorage );
	cvReleaseMemStorage( &mySession->dstorage );
	cvClearMemStorage( mySession->lstorage );
	cvReleaseMemStorage( &mySession->lstorage );
}

int categorize_image(ci_request_t * req)
{
classify_req_data_t *data = ci_service_data(req);
ImageSession mySession;
int minDim=0, ret=0;

    // Load the image from that filename
    mySession.origImage = cvLoadImage( data->body->filename, CV_LOAD_IMAGE_COLOR );

    // If Image is loaded succesfully, then:
    if( mySession.origImage )
    {
        // test to see if we should bypass classification
        minDim = mySession.origImage->width < mySession.origImage->height ? mySession.origImage->width : mySession.origImage->height;
        if(ImageMinProcess >= minDim || minDim < 5)
        {
            ci_debug_printf(10, "Image too small for classification per configuration and/or sanity, letting pass.\n");
            cvReleaseImage(&mySession.origImage);
            return 0; // Image is too small, let it pass
        }

	// Setup Session
	ret = initImageSession(&mySession, req, imageCategories, data->body->filename);
	if(ret < 0)
        {
		addImageErrorHeaders(&mySession, ret);
		deinitImageSession(&mySession);
		return -1;
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
        ci_debug_printf(8, "Could not load image (%s) for classification.\n", data->body->filename);
        return -1;
    }

    // return 0 to indicate successfull execution of the program
    return 0;
}

/****************************************************************************************/
/*Configuration Functions                                                               */
int cfg_AddImageCategory(char *directive, char **argv, void *setdata)
{
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
    imageCategories = initImageCategory(imageCategories, argv[0], argv[1], CV_RGB(RED, GREEN, BLUE));
    ci_debug_printf(1, "Setting parameter :%s (Name: %s Cascade File: %s Red: 0x%x Green: 0x%x Blue: 0x%x)\n", directive, argv[0], argv[1], RED, GREEN, BLUE);
    return 1;
}

int cfg_ImageInterpolation(char *directive, char **argv, void *setdata)
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

int cfg_coalesceOverlap(char *directive, char **argv, void *setdata)
{
ImageCategory *current=imageCategories;
     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          return 0;
     }
     while(current!=NULL)
     {
          if(strcasecmp(argv[0], current->name) == 0)
          {
              sscanf(argv[1], "%f", &current->coalesceOverlap);
              ci_debug_printf(1, "Setting parameter :%s=(%s,%s)\n", directive, argv[0], argv[1]);
              return 1;
          }
          current = current->next;
     }
     ci_debug_printf(1, "Category in directive %s not found. Add category before setting coalesce overlap.\n", directive);
     return 0;
}

int cfg_imageCategoryCopies(char *directive, char **argv, void *setdata)
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
     if(IMAGE_CATEGORY_COPIES < IMAGE_CATEGORY_COPIES_MIN) IMAGE_CATEGORY_COPIES = IMAGE_CATEGORY_COPIES_MIN;
     ci_debug_printf(1, "Setting parameter :%s=(%s,%s)\n", directive, argv[0], argv[1]);
     return 1;
}
