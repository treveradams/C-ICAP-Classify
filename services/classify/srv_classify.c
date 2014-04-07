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

#include "autoconf.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <iconv.h>
#include <ctype.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <stdlib.h>

#define __USE_GNU
#define _GNU_SOURCE
#include <string.h>
#include <strings.h>
#include <wchar.h>
#include <wctype.h>

#include "c-icap.h"
#include "service.h"
#include "header.h"
#include "body.h"
#include "simple_api.h"
#include "debug.h"
#include "cfg_param.h"
#include "filetype.h"
#include "commands.h"
#include "txt_format.h"
#define IN_SRV_CLASSIFY
#include "srv_classify.h"
#include "ci_threads.h"

const wchar_t *WCNULL = L"\0";

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "html.h"
#include "bayes.h"
#include "hyperspace.h"
#include "hash.h"

#ifdef _WIN32
#define F_PERM S_IREAD|S_IWRITE
#else
#define F_PERM S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH
#endif

#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
extern int categorize_image(ci_request_t * req);
extern int categorize_external_image(ci_request_t * req);
extern int cfg_AddImageCategory(const char *directive, const char **argv, void *setdata);
extern int cfg_ImageInterpolation(const char *directive, const char **argv, void *setdata);
extern int cfg_coalesceOverlap(const char *directive, const char **argv, void *setdata);
extern int cfg_imageCategoryCopies(const char *directive, const char **argv, void *setdata);
extern void classifyImagePrepReload(void);
extern int postInitImageClassificationService(void);
extern int closeImageClassification(void);
int cfg_ExternalImageConversion(const char *directive, const char **argv, void *setdata); // In this file
int CFG_NUM_CICAP_THREADS;
#endif

int must_classify(int type, classify_req_data_t * data);

void generate_error_page(classify_req_data_t * data, ci_request_t * req);
char *srvclassify_compute_name(ci_request_t * req);
/***********************************************************************************/
/* Module definitions                                                              */

static int ALLOW204 = 0;
static ci_off_t MAX_OBJECT_SIZE = INT_MAX;
static ci_off_t MAX_MEM_CLASS_SIZE = 32768;
static ci_off_t MAX_MEM_CLASS_TOTAL_SIZE = 0;
uint32_t total_mem_class_used = 0;

static struct ci_magics_db *magic_db = NULL;
static int *classifytypes = NULL;
static int *classifygroups = NULL;
external_conversion_t *externalclassifytypes = NULL;

/* TMP DATA DIR */
char *CLASSIFY_TMP_DIR = NULL;

/* Match levels */
static int TEXT_AMBIGUOUS_LEVEL = -2; // Lowest level to be considered ambiguous
static int TEXT_SOLID_LEVEL = 5; // Lowest level to be considered a solid match

/* Window Size */
static int MAX_WINDOW = 4096; // This is for sliding window buffers

/* Locking */
ci_thread_rwlock_t textclassify_rwlock;
ci_thread_mutex_t memmanage_mtx;

/* Image processing information */
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
//#include <opencv/cv.h>
extern int IMAGE_SCALE_DIMENSION; // Scale to this dimension
extern int IMAGE_MAX_SCALE; // Maximum rescale
extern int IMAGE_MIN_PROCESS; // don't process images whose minimum dimesnion is under this size
extern int IMAGE_DEBUG_SAVE_ORIG;
extern int IMAGE_DEBUG_SAVE_PARTS;
extern int IMAGE_DEBUG_SAVE_MARKED;
extern int IMAGE_DEBUG_DEMONSTRATE;
extern int IMAGE_DEBUG_DEMONSTRATE_MASKED;
extern int IMAGE_INTERPOLATION;
extern int IMAGE_CATEGORY_COPIES;
extern ci_thread_rwlock_t imageclassify_rwlock;
#endif

/*srv_classify service extra data ... */
ci_service_xdata_t *srv_classify_xdata = NULL;

static int CLASSIFYREQDATA_POOL = -1;
static int HASHDATA_POOL = -1;

int srvclassify_init_service(ci_service_xdata_t * srv_xdata,
                           struct ci_server_conf *server_conf);
int srvclassify_post_init_service(ci_service_xdata_t * srv_xdata,
                           struct ci_server_conf *server_conf);
void srvclassify_close_service();
int srvclassify_check_preview_handler(char *preview_data, int preview_data_len,
                                    ci_request_t *);
int srvclassify_end_of_data_handler(ci_request_t *);
void *srvclassify_init_request_data(ci_request_t * req);
void srvclassify_release_request_data(void *data);
int srvclassify_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
                 ci_request_t * req);

/*Arguments parse*/
void srvclassify_parse_args(classify_req_data_t * data, char *args);
/*Configuration Functions*/
int cfg_ClassifyFileTypes(const char *directive, const char **argv, void *setdata);
int cfg_DoTextPreload(const char *directive, const char **argv, void *setdata);
int cfg_AddTextCategory(const char *directive, const char **argv, void *setdata);
int cfg_AddTextCategoryDirectoryHS(const char *directive, const char **argv, void *setdata);
int cfg_AddTextCategoryDirectoryNB(const char *directive, const char **argv, void *setdata);
int cfg_TextHashSeeds(const char *directive, const char **argv, void *setdata);
int cfg_OptimizeFNB(const char *directive, const char **argv, void *setdata);
int cfg_ClassifyTmpDir(const char *directive, const char **argv, void *setdata);
int cfg_TmpDir(const char *directive, const char **argv, void *setdata);
int cfg_TextSecondary(const char *directive, const char **argv, void *setdata);
int cfg_ExternalTextConversion(const char *directive, const char **argv, void *setdata);
/*General functions*/
int get_filetype(ci_request_t *req, char *buf, int len);
void set_istag(ci_service_xdata_t *srv_xdata);
int categorize_text(ci_request_t *req);
int categorize_external_text(ci_request_t * req, int classification_type);
char *findCharset(const char *input, int64_t);
int make_wchar(ci_request_t *req);
int make_wchar_from_buf(ci_request_t * req, ci_membuf_t *input);
int classify_uncompress(ci_request_t *req);
static void addTextErrorHeaders(ci_request_t *req, int error, char *extra_info);
/*External functions*/
extern char *strcasestr(const char *haystack, const char *needle);

// PICS label handling
regex_t picslabel;
int make_pics_header(ci_request_t * req);

#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
// Referrer classification handling
#define REFERRERS_SIZE 1000

typedef struct {
	uint32_t hash;
	char *uri;
	HTMLClassification fhs_classification;
	HTMLClassification fnb_classification;
	uint32_t age;
} REFERRERS_T;

ci_thread_rwlock_t referrers_rwlock;
REFERRERS_T *referrers;

static uint32_t classify_requests = 1;
#endif

void insertReferrer(char *uri, HTMLClassification fhs_classification, HTMLClassification fnb_classification);
void getReferrerClassification(const char *uri, HTMLClassification *fhs_classification, HTMLClassification *fnb_classification);
void addReferrerHeaders(ci_request_t *req, HTMLClassification fhs_classification, HTMLClassification fnb_classification);
void createReferrerTable(void);
void freeReferrerTable(void);

/*It is dangerous to pass directly fields of the limits structure in conf_variables,
  because in the feature some of this fields will change type (from int to unsigned int
  or from long to long long etc)
  I must use global variables and use the post_init_service function to fill the
  limits structure.
  But, OK let it go for the time ....
*/

/*Configuration Table .....*/
static struct ci_conf_entry conf_variables[] = {
     {"TmpDir", NULL, cfg_TmpDir, NULL},
     {"TextAmbiguous", &TEXT_AMBIGUOUS_LEVEL, ci_cfg_set_int, NULL},
     {"TextSolidMatch", &TEXT_SOLID_LEVEL, ci_cfg_set_int, NULL},
     {"TextFileTypes", NULL, cfg_ClassifyFileTypes, NULL},
     {"ExternalTextFileType", NULL, cfg_ExternalTextConversion, NULL},
/*     {"ExternalTextMimeType", NULL, cfg_ExternalTextConversion, NULL}, // Mime type handling not yet implemented */
     {"TextPreload", NULL, cfg_DoTextPreload, NULL},
     {"TextCategory", NULL, cfg_AddTextCategory, NULL},
     {"TextCategoryDirectoryHS", NULL, cfg_AddTextCategoryDirectoryHS, NULL},
     {"TextCategoryDirectoryNB", NULL, cfg_AddTextCategoryDirectoryNB, NULL},
     {"TextHashSeeds", NULL, cfg_TextHashSeeds, NULL},
     {"OptimizeFNB", NULL, cfg_OptimizeFNB, NULL},
     {"MaxObjectSize", &MAX_OBJECT_SIZE, ci_cfg_size_off, NULL},
     {"MaxWindowSize", &MAX_WINDOW, ci_cfg_size_off, NULL},
     {"Allow204Responces", &ALLOW204, ci_cfg_onoff, NULL},
     {"TextPrimarySecondary", NULL, cfg_TextSecondary, NULL},
     {"MaxMemClassification", &MAX_MEM_CLASS_SIZE, ci_cfg_size_off, NULL},
     {"MaxTotalMemClassification", &MAX_MEM_CLASS_TOTAL_SIZE, ci_cfg_size_off, NULL},
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     {"ImageFileTypes", NULL, cfg_ClassifyFileTypes, NULL},
     {"ImageScaleDimension", &IMAGE_SCALE_DIMENSION, ci_cfg_set_int, NULL},
     {"ImageInterpolation", NULL, cfg_ImageInterpolation, NULL},
     {"ImageMaxScale", &IMAGE_MAX_SCALE, ci_cfg_set_int, NULL},
     {"ImageMinProcess", &IMAGE_MIN_PROCESS, ci_cfg_set_int, NULL},
     {"ImageCategory", NULL, cfg_AddImageCategory, NULL},
     {"ImageCoalesceOverlap", NULL, cfg_coalesceOverlap, NULL},
     {"ImageCategoryCopies", NULL, cfg_imageCategoryCopies, NULL},
     {"debugSaveOriginal", &IMAGE_DEBUG_SAVE_ORIG, ci_cfg_set_int, NULL},
     {"debugSaveParts", &IMAGE_DEBUG_SAVE_PARTS, ci_cfg_set_int, NULL},
     {"debugSaveMarked", &IMAGE_DEBUG_SAVE_MARKED, ci_cfg_set_int, NULL},
     {"debugDemonstrate", &IMAGE_DEBUG_DEMONSTRATE, ci_cfg_set_int, NULL},
     {"debugDemonstrateMasked", &IMAGE_DEBUG_DEMONSTRATE_MASKED, ci_cfg_set_int, NULL},
     {"ExternalImageFileType", NULL, cfg_ExternalImageConversion, NULL},
/*     {"ExternalImageMimeType", NULL, cfg_ExternalImageConversion, NULL}, // Mime type handling not yet implemented */
#endif
     {NULL, NULL, NULL, NULL}
};

/* Template Functions and Table */
int fmt_srv_classify_source(ci_request_t *req, char *buf, int len, const char *param);
int fmt_srv_classify_destination(ci_request_t *req, char *buf, int len, const char *param);

struct ci_fmt_entry srv_classify_format_table [] = {
    {"%CLIN", "File To Convert", fmt_srv_classify_source},
    {"%CLOUT", "File To Read Back In For Classification", fmt_srv_classify_destination},
    { NULL, NULL, NULL}
};

CI_DECLARE_MOD_DATA ci_service_module_t service = {
     "srv_classify",              /*Module name */
     "Web Classification service",        /*Module short description */
     ICAP_RESPMOD,        /*Service type responce or request modification */
     srvclassify_init_service,    /*init_service. */
     srvclassify_post_init_service, /*post_init_service. */
     srvclassify_close_service,   /*close_service */
     srvclassify_init_request_data,       /*init_request_data. */
     srvclassify_release_request_data,    /*release request data */
     srvclassify_check_preview_handler,
     srvclassify_end_of_data_handler,
     srvclassify_io,
     conf_variables,
     NULL
};

ci_membuf_t *createMemBody(classify_req_data_t *data, uint32_t size)
{
ci_membuf_t *temp = NULL;
	if(size > MAX_MEM_CLASS_SIZE) return NULL;
	if(MAX_MEM_CLASS_TOTAL_SIZE)
	{
		ci_thread_mutex_lock(&memmanage_mtx);
		if(size + total_mem_class_used > MAX_MEM_CLASS_TOTAL_SIZE)
		{
			ci_thread_mutex_unlock(&memmanage_mtx);
			return NULL;
		}
	}
	temp = ci_membuf_new_sized(size);
	if(temp != NULL) total_mem_class_used += size;
	if(MAX_MEM_CLASS_TOTAL_SIZE)
		ci_thread_mutex_unlock(&memmanage_mtx);
	return temp;
}

int freeMemBody(classify_req_data_t *data)
{
	if(!data) return CI_ERROR;
	if(!data->mem_body) return CI_ERROR;
	else {
		if(MAX_MEM_CLASS_TOTAL_SIZE)
		{
			ci_thread_mutex_lock(&memmanage_mtx);
			total_mem_class_used -= data->mem_body->endpos;
			ci_thread_mutex_unlock(&memmanage_mtx);
		}
		ci_membuf_free(data->mem_body);
		data->mem_body = NULL;
	}
	return 0;
}

void diskBodyToMemBody(ci_request_t *req)
{
classify_req_data_t *data = ci_service_data(req);
ci_membuf_t *tempbody;
int rret;

	if(!data->disk_body) return;

	data->mem_body = ci_membuf_new_sized(ci_simple_file_size(data->disk_body));
	tempbody = data->mem_body;

	lseek(data->disk_body->fd, 0, SEEK_SET);

	while(ci_membuf_size(tempbody) < ci_simple_file_size(data->disk_body))
	{
		rret = read(data->disk_body->fd, tempbody->buf + ci_membuf_size(tempbody), ci_simple_file_size(data->disk_body) - ci_membuf_size(tempbody));
		if(rret > 0) tempbody->endpos += rret;
		else if(rret < 0 && errno == EINTR) continue;
		else {
//			ci_membuf_free(data->mem_body);
//			data->mem_body = NULL;
//			return;
			break;
		}
	}
	// Just let c_icap simple file api do the work:
//	tempbody->endpos = ci_simple_file_read(data->disk_body, tempbody->buf, ci_simple_file_size(data->disk_body));

	if(MAX_MEM_CLASS_TOTAL_SIZE)
	{
		ci_thread_mutex_lock(&memmanage_mtx);
		total_mem_class_used += ci_simple_file_size(data->disk_body);
		ci_thread_mutex_unlock(&memmanage_mtx);
	}

	ci_simple_file_destroy(data->disk_body);
	data->disk_body = NULL;
}

void memBodyToDiskBody(ci_request_t *req)
{
classify_req_data_t *data = ci_service_data(req);
ci_simple_file_t *tempbody;

	if(!data->mem_body) return;

	data->disk_body = ci_simple_file_new(ci_membuf_size(data->mem_body));
	tempbody = data->disk_body;

	ci_simple_file_write(tempbody, data->mem_body->buf, ci_membuf_size(data->mem_body), 0);

	if(MAX_MEM_CLASS_TOTAL_SIZE)
	{
		ci_thread_mutex_lock(&memmanage_mtx);
		total_mem_class_used -= ci_membuf_size(data->mem_body);
		ci_thread_mutex_unlock(&memmanage_mtx);
	}

	ci_membuf_free(data->mem_body);
	data->mem_body = NULL;
}

int srvclassify_init_service(ci_service_xdata_t * srv_xdata,
                           struct ci_server_conf *server_conf)
{
     int i;

     ci_thread_rwlock_init(&textclassify_rwlock);
     ci_thread_rwlock_wrlock(&textclassify_rwlock);
     ci_thread_mutex_init(&memmanage_mtx);

     magic_db = server_conf->MAGIC_DB;
     classifytypes = (int *) malloc(ci_magic_types_num(magic_db) * sizeof(int));
     classifygroups = (int *) malloc(ci_magic_groups_num(magic_db) * sizeof(int));
     externalclassifytypes = calloc(ci_magic_types_num(magic_db), sizeof(external_conversion_t));

     for (i = 0; i < ci_magic_types_num(magic_db); i++)
          classifytypes[i] = NO_CLASSIFY;
     for (i = 0; i < ci_magic_groups_num(magic_db); i++)
          classifygroups[i] = NO_CLASSIFY;

     ci_debug_printf(10, "Going to initialize srv_classify\n");
     srv_classify_xdata = srv_xdata;      /* Needed by db_reload command */

     ci_service_set_preview(srv_xdata, 1024);
     ci_service_enable_204(srv_xdata);
     ci_service_set_transfer_preview(srv_xdata, "*");

     /*Initialize object pools*/
     CLASSIFYREQDATA_POOL = ci_object_pool_register("classify_req_data_t", sizeof(classify_req_data_t));

     if(CLASSIFYREQDATA_POOL < 0) {
	 ci_debug_printf(1, " srvclassify_init_service: error registering object_pool classify_req_data_t\n");
	 ci_thread_rwlock_unlock(&textclassify_rwlock);
	 return CI_ERROR;
     }

     HASHDATA_POOL = ci_object_pool_register("HTMLFeature", (sizeof(HTMLFeature) * HTML_MAX_FEATURE_COUNT));

     if(HASHDATA_POOL < 0) {
         ci_object_pool_unregister(CLASSIFYREQDATA_POOL);
	 ci_debug_printf(1, " srvclassify_init_service: error registering object_pool HTMLFeature\n");
	 ci_thread_rwlock_unlock(&textclassify_rwlock);
	 return CI_ERROR;
     }

     setlocale(LC_ALL, NULL);
     int utf8_mode = (strcmp(nl_langinfo(CODESET), "UTF-8") == 0);
     if(!utf8_mode) setlocale(LC_ALL, "en_US.utf8");
//#ifdef HAVE_TRE
     initBayesClassifier();
     initHyperSpaceClassifier();
     tre_regwcomp(&picslabel, L"<meta http-equiv=\"PICS-Label\" content='\\(PICS-1.1 ([^']*)'.*/?>", REG_EXTENDED | REG_ICASE);
//#endif
     initHTML();
     CFG_NUM_CICAP_THREADS = server_conf->THREADS_PER_CHILD;
     ci_thread_rwlock_unlock(&textclassify_rwlock);

     return 1;
}

int srvclassify_post_init_service(ci_service_xdata_t * srv_xdata,
                           struct ci_server_conf *server_conf)
{
int ret = CI_OK;
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     ret = postInitImageClassificationService();

     ci_thread_rwlock_init(&referrers_rwlock);
     createReferrerTable();
#endif
     set_istag(srv_classify_xdata);

     if(CI_BODY_MAX_MEM > MAX_MEM_CLASS_SIZE) MAX_MEM_CLASS_SIZE = CI_BODY_MAX_MEM - 1;
     if(MAX_OBJECT_SIZE > INT_MAX) MAX_OBJECT_SIZE = INT_MAX;
     return ret;
}

void srvclassify_close_service()
{
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     closeImageClassification();
     freeReferrerTable();
#endif

     ci_object_pool_unregister(HASHDATA_POOL);
     ci_object_pool_unregister(CLASSIFYREQDATA_POOL);

     ci_thread_rwlock_wrlock(&textclassify_rwlock);
     if(CLASSIFY_TMP_DIR) free(CLASSIFY_TMP_DIR);
     if(classifytypes) free(classifytypes);
     classifytypes = NULL;
     if(classifygroups) free(classifygroups);
     classifygroups = NULL;
     if(externalclassifytypes)
     {
         int k;
         for(int i = 0; i < ci_magic_groups_num(magic_db); i++)
         {
             if(externalclassifytypes[i].mime_type) free(externalclassifytypes[i].mime_type);
             if(externalclassifytypes[i].text_program) free(externalclassifytypes[i].text_program);
             if(externalclassifytypes[i].image_program) free(externalclassifytypes[i].image_program);
             k = 0;
             if(externalclassifytypes[i].text_args)
             {
                  while(externalclassifytypes[i].text_args[k] != NULL)
                  {
                       free(externalclassifytypes[i].text_args[k]);
                       k++;
                  }
                  free(externalclassifytypes[i].text_args);
             }
             if(externalclassifytypes[i].image_args)
             {
                  while(externalclassifytypes[i].image_args[k] != NULL)
                  {
                       free(externalclassifytypes[i].image_args[k]);
                       k++;
                  }
                  free(externalclassifytypes[i].image_args);
             }
         }
     }
     free(externalclassifytypes);
     externalclassifytypes = NULL;
//#ifdef HAVE_TRE
     tre_regfree(&picslabel);
     deinitBayesClassifier();
     deinitHyperSpaceClassifier();
//#endif
     deinitHTML();
     ci_thread_rwlock_unlock(&textclassify_rwlock);
}


void *srvclassify_init_request_data(ci_request_t * req)
{
     int preview_size;
     classify_req_data_t *data;

     preview_size = ci_req_preview_size(req);

     if (req->args[0] != '\0') {
          ci_debug_printf(5, "service arguments:%s\n", req->args);
     }
     if (ci_req_hasbody(req)) {
          ci_debug_printf(8, "Request type: %d. Preview size:%d\n", req->type,
                          preview_size);
          if (!(data = ci_object_pool_alloc(CLASSIFYREQDATA_POOL))) {
               ci_debug_printf(1,
                               "Error allocation memory for service data!!!!!!!\n");
               return NULL;
          }
          data->disk_body = NULL;
          data->mem_body = NULL;
          data->uncompressedbody = NULL;
          data->external_body = NULL;
          data->must_classify = NO_CLASSIFY;
          if (ALLOW204)
               data->args.enable204 = 1;
          else
               data->args.enable204 = 0;
          data->args.forcescan = 0;
          data->args.sizelimit = 1;
          data->args.mode = 0;

          if (req->args[0] != '\0') {
               ci_debug_printf(5, "service arguments:%s\n", req->args);
               srvclassify_parse_args(data, req->args);
          }
          if (data->args.enable204 && ci_allow204(req))
               data->allow204 = 1;
          else
               data->allow204 = 0;
          data->req = req;
          return data;
     }
     return NULL;
}


void srvclassify_release_request_data(void *data)
{
     if (data) {
          ci_debug_printf(8, "Releasing srv_classify data.....\n");
          if (((classify_req_data_t *) data)->disk_body)
               ci_simple_file_destroy(((classify_req_data_t *) data)->disk_body);

          if (((classify_req_data_t *) data)->mem_body) freeMemBody(data);

          if (((classify_req_data_t *) data)->external_body)
               ci_simple_file_destroy(((classify_req_data_t *) data)->external_body);

          if (((classify_req_data_t *) data)->uncompressedbody)
               ci_membuf_free(((classify_req_data_t *) data)->uncompressedbody);

          ci_object_pool_free(data);
     }
}


int srvclassify_check_preview_handler(char *preview_data, int preview_data_len,
                                    ci_request_t * req)
{
     ci_off_t content_size = 0;
     classify_req_data_t *data = ci_service_data(req);
     char *content_type = NULL;

     ci_debug_printf(9, "OK The preview data size is %d\n", preview_data_len);

     if (!data || !ci_req_hasbody(req)){
	 ci_debug_printf(9, "No body data, allow 204\n");
          return CI_MOD_ALLOW204;
     }

     /*Going to determine the file type, get_filetype can take preview_data as null ....... */
     data->file_type = get_filetype(req, preview_data, preview_data_len);
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     data->type_name = ci_data_type_name(magic_db, data->file_type);
#endif
     if ((data->must_classify = must_classify(data->file_type, data)) == NO_CLASSIFY) {
          ci_debug_printf(8, "srv_classify: Not in \"must classify list\". Allow it...... \n");
          return CI_MOD_ALLOW204;
     }

     content_size = ci_http_content_length(req);

     if((content_type = ci_http_response_get_header(req, "Content-Type")) != NULL) {
          if(strstr(content_type, "application/x-javascript") || strstr(content_type, "application/javascript") ||
             strstr(content_type, "application/ecmascript") || strstr(content_type, "text/ecmascript") ||
             strstr(content_type, "text/javascript") || strstr(content_type, "text/jscript") ||
             strstr(content_type, "text/css"))
          {
               ci_debug_printf(8, "srv_classify: Non-content MIME type (%s). Allow it......\n", content_type);
               data->must_classify = NO_CLASSIFY; // these are not likely to contain data and confuse our classifier
               return CI_MOD_ALLOW204;
          }
     }

     if((content_type = ci_http_response_get_header(req, "Content-Encoding")) != NULL) {
          if(strstr(content_type, "gzip")) data->is_compressed = CI_ENCODE_GZIP;
          else if(strstr(content_type, "deflate")) data->is_compressed = CI_ENCODE_DEFLATE;
          else data->is_compressed = CI_ENCODE_UNKNOWN;
     }
     else data->is_compressed = CI_ENCODE_NONE;

     if (data->args.sizelimit && MAX_OBJECT_SIZE
         && content_size > MAX_OBJECT_SIZE) {
          ci_debug_printf(1,
                          "srv_classify: Object size is %" PRINTF_OFF_T "."
                          " Bigger than max classifiable file size (%"
                          PRINTF_OFF_T "). Allow it.... \n", content_size,
                          MAX_OBJECT_SIZE);
          return CI_MOD_ALLOW204;
     }

     if((data->must_classify == TEXT || data->must_classify == IMAGE) && content_size > 0)
     {
          data->mem_body = createMemBody(data, content_size);
          if(data->mem_body) ci_membuf_lock_all(data->mem_body);
     }
     if(!data->mem_body)
     {
          data->disk_body = ci_simple_file_new(content_size);
          ci_simple_file_lock_all(data->disk_body);
     }

     if (!data->disk_body && !data->mem_body)           /*Memory allocation or something else ..... */
          return CI_ERROR;

     if (preview_data_len) {
          if(data->mem_body)
          {
               if (ci_membuf_write(data->mem_body, preview_data, preview_data_len,
				ci_req_hasalldata(req)) == CI_ERROR)
	          return CI_ERROR;
          }
          else {
               if (ci_simple_file_write(data->disk_body, preview_data, preview_data_len,
				ci_req_hasalldata(req)) == CI_ERROR)
	          return CI_ERROR;
          }
     }
     return CI_MOD_CONTINUE;
}



int srvclassify_read_from_net(char *buf, int len, int iseof, ci_request_t * req)
{
     /* We can put here scanning for jscripts and html and raw data ...... */
     classify_req_data_t *data = ci_service_data(req);
     if (!data)
          return CI_ERROR;

     if (!data->disk_body && !data->mem_body) /*No body data? consume all content*/
	 return len;

     if(data->mem_body)
     {
          /* FIXME the following should just process what is had at the moment... possible? */
          if (ci_membuf_size(data->mem_body) >= MAX_OBJECT_SIZE && MAX_OBJECT_SIZE){
               if(data->args.mode == 1){
                   /* This shouldn't happen... we are in simple mode, cannot send early ICAP responses... */
                   ci_debug_printf(1, "Object does not fit to max object size and early responses are not allowed! \n");
                   return CI_ERROR;
               }
               else { /*Send early response.*/
                    ci_debug_printf(1, "srv_classify: Object size is bigger than max classifiable file size\n");
                    data->must_classify = 0;
                    ci_req_unlock_data(req);      /*Allow ICAP to send data before receives the EOF....... */
                    ci_membuf_unlock_all(data->mem_body);        /*Unlock all body data to continue send them..... */
               }
           }
          /* anything where we are not in over max object size, simply write and exit */
          else if(ci_membuf_size(data->mem_body) + len > data->mem_body->bufsize)
          {
               memBodyToDiskBody(req);
               return ci_simple_file_write(data->disk_body, buf, len, iseof);
          }
          return ci_membuf_write(data->mem_body, buf, len, iseof);
      }
      else {
          /* FIXME the following should just process what is had at the moment... possible? */
          if(ci_simple_file_size(data->disk_body) >= MAX_OBJECT_SIZE && MAX_OBJECT_SIZE) {
               if(data->args.mode == 1){
                   /* This shouldn't happen... we are in simple mode, cannot send early ICAP responses... */
                   ci_debug_printf(1, "Object does not fit to max object size and early responses are not allowed! \n");
                   return CI_ERROR;
               }
               else { /*Send early response.*/
                    ci_debug_printf(1, "srv_classify: Object size is bigger than max classifiable file size\n");
                    data->must_classify = 0;
                    ci_req_unlock_data(req);      /*Allow ICAP to send data before receives the EOF....... */
                    ci_simple_file_unlock_all(data->disk_body);        /*Unlock all body data to continue send them..... */
               }
          }
          /* anything where we are not in over max object size, simply write and exit */
          return ci_simple_file_write(data->disk_body, buf, len, iseof);
     }
}



int srvclassify_write_to_net(char *buf, int len, ci_request_t * req)
{
     int bytes;
     classify_req_data_t *data = ci_service_data(req);
     if (!data)
          return CI_ERROR;

     if(data->mem_body)
	 bytes = ci_membuf_read(data->mem_body, buf, len);
     else if(data->disk_body)
	 bytes = ci_simple_file_read(data->disk_body, buf, len);
     else
	 bytes = 0;
     return bytes;
}

int srvclassify_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
                 ci_request_t * req)
{
     if (rbuf && rlen) {
          *rlen = srvclassify_read_from_net(rbuf, *rlen, iseof, req);
	  if (*rlen == CI_ERROR)
	       return CI_ERROR;
/*          else if (*rlen < 0)
	       ret = CI_OK; */ // Ignore
     }
     else if (iseof) {
	 if (srvclassify_read_from_net(NULL, 0, iseof, req) == CI_ERROR)
	     return CI_ERROR;
     }
     if (wbuf && wlen) {
          *wlen = srvclassify_write_to_net(wbuf, *wlen, req);
     }
     return CI_OK;
}

int srvclassify_end_of_data_handler(ci_request_t *req)
{
     classify_req_data_t *data = ci_service_data(req);
     ci_simple_file_t *disk_body;
     ci_membuf_t *mem_body;

     if (!data || (!data->disk_body && !data->mem_body))
          return CI_MOD_DONE;

     disk_body = data->disk_body;
     mem_body = data->mem_body;

     if (data->must_classify == NO_CLASSIFY) {       /*If exceeds the MAX_OBJECT_SIZE for example ......  */
          ci_debug_printf(8, "Not Classifying\n");
          if(mem_body)
               ci_membuf_unlock_all(mem_body);
          else {
               ci_simple_file_unlock_all(disk_body);          /*Unlock all data to continue send them . Not really needed here.... */
//               lseek(disk_body->fd, 0, SEEK_SET); // Not sure why I put this here after all!
          }
          return CI_MOD_DONE;
     }

     if (data->must_classify == TEXT)
     {
          if(data->disk_body) diskBodyToMemBody(req);
          ci_debug_printf(8, "Classifying TEXT from memory\n");
          #ifdef HAVE_ZLIB
          if (data->is_compressed == CI_ENCODE_GZIP || data->is_compressed == CI_ENCODE_DEFLATE)
               classify_uncompress(req);
          #endif
          if(make_wchar(req) == CI_OK) // We should only categorize text if this returns >= 0
          {
               make_pics_header(req);
               categorize_text(req);
          }
     }
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     else if (data->must_classify == IMAGE)
     {
          categorize_image(req);
     }
     // The following may look screwy not having the second being "else if",
     // but it is necessary so that both EXTERNAL IMAGE and EXTERNAL TEXT can be used
     else if (data->must_classify & EXTERNAL_IMAGE || data->must_classify & EXTERNAL_TEXT || data->must_classify & EXTERNAL_TEXT_PIPE)
     {
          if (data->must_classify & EXTERNAL_IMAGE)
          {
               ci_debug_printf(8, "Classifying EXTERNAL IMAGE(S) from file\n");
               categorize_external_image(req);
          }
          if (data->must_classify & EXTERNAL_TEXT || data->must_classify & EXTERNAL_TEXT_PIPE)
          {
               ci_debug_printf(8, "Classifying EXTERNAL TEXT from file\n");
               categorize_external_text(req, data->must_classify);
          }
     }
#else
     else if (data->must_classify & EXTERNAL_TEXT || data->must_classify & EXTERNAL_TEXT_PIPE)
     {
          ci_debug_printf(8, "Classifying EXTERNAL TEXT from file\n");
          categorize_external_text(req, data->must_classify);
     }
#endif
     else if (data->allow204 && !ci_req_sent_data(req)) {
          ci_debug_printf(7, "srv_classify module: Respond with allow 204\n");
          return CI_MOD_ALLOW204;
     }

     if(mem_body) {
          ci_membuf_unlock_all(mem_body);
     }
     else {
          ci_simple_file_unlock_all(disk_body);   /*Unlock all data to continue send them, after all we only modify headers in this module..... */
          ci_debug_printf(7, "file unlocked, flags :%d (unlocked:%" PRINTF_OFF_T ")\n",
                          disk_body->flags, disk_body->unlocked);
     }
     return CI_MOD_DONE;
}

int categorize_text(ci_request_t *req)
{
classify_req_data_t *data = ci_service_data(req);
char reply[2 * CI_MAX_PATH + 1];
char type[20];
regexHead myRegexHead = {.head = NULL, .tail = NULL, .dirty = 0, . main_memory = NULL, .arrays = NULL, .lastarray = NULL};
HashList myHashes;
HTMLClassification HSclassification, NBclassification;

     // sanity check
     if(!data->uncompressedbody)
     {
          ci_debug_printf(3, "Conversion to UTF-32 must have failed...\n");
          addTextErrorHeaders(req, UNKNOWN_ERROR, NULL);
          return CI_ERROR;
     }

     // Obtain Read Lock
     ci_thread_rwlock_rdlock(&textclassify_rwlock);

     mkRegexHead(&myRegexHead, (wchar_t *)data->uncompressedbody->buf, 1);
     removeHTML(&myRegexHead);
     regexMakeSingleBlock(&myRegexHead);
     normalizeCurrency(&myRegexHead);
     regexMakeSingleBlock(&myRegexHead);

     myHashes.hashes = ci_object_pool_alloc(HASHDATA_POOL);
     myHashes.slots = HTML_MAX_FEATURE_COUNT;
     myHashes.used = 0;
     computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);

     HSclassification = doHSPrepandClassify(&myHashes);
     NBclassification = doBayesPrepandClassify(&myHashes);

     ci_object_pool_free(myHashes.hashes);
     freeRegexHead(&myRegexHead);
     data->uncompressedbody->buf = NULL; // This was freed who knows how many times in the classification, avoid double free

     // modify headers
     if (!ci_http_response_headers(req))
          ci_http_response_create(req, 1, 1);
     if(HSclassification.primary_name != NULL)
     {
          if(HSclassification.primary_probScaled >= (float) TEXT_AMBIGUOUS_LEVEL && HSclassification.primary_probScaled < (float) TEXT_SOLID_LEVEL) strcpy(type,"AMBIGUOUS");
          else if(HSclassification.primary_probScaled >= (float) TEXT_SOLID_LEVEL) strcpy(type, "SOLID");
          else strcpy(type,"NEAREST");
          snprintf(reply, CI_MAX_PATH, "X-TEXT-CATEGORY-HS: %s", HSclassification.primary_name);
          reply[CI_MAX_PATH]='\0';
          ci_http_response_add_header(req, reply);
          ci_debug_printf(10, "Added header: %s\n", reply);
          snprintf(reply, CI_MAX_PATH, "X-TEXT-CATEGORY-LEVEL-HS: %f", HSclassification.primary_probScaled);
          reply[CI_MAX_PATH]='\0';
          ci_http_response_add_header(req, reply);
          ci_debug_printf(10, "Added header: %s\n", reply);
          snprintf(reply, CI_MAX_PATH, "X-TEXT-CATEGORY-CONFIDENCE-HS: %s", type);
          reply[CI_MAX_PATH]='\0';
          ci_http_response_add_header(req, reply);
          ci_debug_printf(10, "Added header: %s\n", reply);
          if(HSclassification.secondary_name != NULL)
          {
               if(HSclassification.secondary_probScaled >= (float) TEXT_AMBIGUOUS_LEVEL && HSclassification.secondary_probScaled < (float) TEXT_SOLID_LEVEL) strcpy(type,"AMBIGUOUS");
               else if(HSclassification.secondary_probScaled >= (float) TEXT_SOLID_LEVEL) strcpy(type, "SOLID");
               else strcpy(type,"NEAREST");
               snprintf(reply, CI_MAX_PATH, "X-TEXT-SECONDARY-CATEGORY-HS: %s", HSclassification.secondary_name);
               reply[CI_MAX_PATH]='\0';
               ci_http_response_add_header(req, reply);
               ci_debug_printf(10, "Added header: %s\n", reply);
               snprintf(reply, CI_MAX_PATH, "X-TEXT-SECONDARY-CATEGORY-LEVEL-HS: %f", HSclassification.secondary_probScaled);
               reply[CI_MAX_PATH]='\0';
               ci_http_response_add_header(req, reply);
               ci_debug_printf(10, "Added header: %s\n", reply);
               snprintf(reply, CI_MAX_PATH, "X-TEXT-SECONDARY-CATEGORY-CONFIDENCE-HS: %s", type);
               reply[CI_MAX_PATH]='\0';
               ci_http_response_add_header(req, reply);
               ci_debug_printf(10, "Added header: %s\n", reply);
          }
     }
     if(NBclassification.primary_name != NULL)
     {
          if(NBclassification.primary_probScaled >= (float) TEXT_AMBIGUOUS_LEVEL && NBclassification.primary_probScaled < (float) TEXT_SOLID_LEVEL) strcpy(type,"AMBIGUOUS");
          else if(NBclassification.primary_probScaled >= (float) TEXT_SOLID_LEVEL) strcpy(type, "SOLID");
          else strcpy(type,"NEAREST");
          snprintf(reply, CI_MAX_PATH, "X-TEXT-CATEGORY-NB: %s", NBclassification.primary_name);
          reply[CI_MAX_PATH]='\0';
          ci_http_response_add_header(req, reply);
          ci_debug_printf(10, "Added header: %s\n", reply);
          snprintf(reply, CI_MAX_PATH, "X-TEXT-CATEGORY-LEVEL-NB: %f", NBclassification.primary_probScaled);
          reply[CI_MAX_PATH]='\0';
          ci_http_response_add_header(req, reply);
          ci_debug_printf(10, "Added header: %s\n", reply);
          snprintf(reply, CI_MAX_PATH, "X-TEXT-CATEGORY-CONFIDENCE-NB: %s", type);
          reply[CI_MAX_PATH]='\0';
          ci_http_response_add_header(req, reply);
          ci_debug_printf(10, "Added header: %s\n", reply);
          if(NBclassification.secondary_name != NULL)
          {
               if(NBclassification.secondary_probScaled >= (float) TEXT_AMBIGUOUS_LEVEL && NBclassification.secondary_probScaled < (float) TEXT_SOLID_LEVEL) strcpy(type,"AMBIGUOUS");
               else if(NBclassification.secondary_probScaled >= (float) TEXT_SOLID_LEVEL) strcpy(type, "SOLID");
               else strcpy(type,"NEAREST");
               snprintf(reply, CI_MAX_PATH, "X-TEXT-SECONDARY-CATEGORY-NB: %s", NBclassification.secondary_name);
               reply[CI_MAX_PATH]='\0';
               ci_http_response_add_header(req, reply);
               ci_debug_printf(10, "Added header: %s\n", reply);
               snprintf(reply, CI_MAX_PATH, "X-TEXT-SECONDARY-CATEGORY-LEVEL-NB: %f", NBclassification.secondary_probScaled);
               reply[CI_MAX_PATH]='\0';
               ci_http_response_add_header(req, reply);
               ci_debug_printf(10, "Added header: %s\n", reply);
               snprintf(reply, CI_MAX_PATH, "X-TEXT-SECONDARY-CATEGORY-CONFIDENCE-NB: %s", type);
               reply[CI_MAX_PATH]='\0';
               ci_http_response_add_header(req, reply);
               ci_debug_printf(10, "Added header: %s\n", reply);
          }
     }
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     // Store classification so images and videos can have the data
     char uri[MAX_URI_LENGTH + 1];
     ci_http_request_url(req, uri, MAX_URI_LENGTH);
     insertReferrer(uri, HSclassification, NBclassification);
#endif
     // Release Read Lock
     ci_thread_rwlock_unlock(&textclassify_rwlock);

     return CI_OK;
}

int categorize_external_text(ci_request_t *req, int classification_type)
{
FILE *conversion_in;
char buff[512];
char CALL_OUT[CI_MAX_PATH + 1];
ci_membuf_t *tempbody;
classify_req_data_t *data;
int i = 0;
int ret, maxwrite;
pid_t child_pid;
int wait_status;

	data = ci_service_data(req);

	tempbody = ci_membuf_new();

	if(classification_type == EXTERNAL_TEXT_PIPE)
	{
		maxwrite = CI_MAX_PATH;
		strncpy(CALL_OUT, externalclassifytypes[data->file_type].text_program, maxwrite);
		CALL_OUT[CI_MAX_PATH] = '\0';
		maxwrite -= strlen(CALL_OUT);
		while(externalclassifytypes[data->file_type].text_args[i] != NULL)
		{
			ret = ci_format_text(req, externalclassifytypes[data->file_type].text_args[i], buff, 511, srv_classify_format_table);
			buff[511] = '\0'; // Terminate for safety!
			strncat(CALL_OUT, " ", maxwrite);
			CALL_OUT[CI_MAX_PATH] = '\0';
			strncat(CALL_OUT, buff, maxwrite);
			CALL_OUT[CI_MAX_PATH] = '\0';
			maxwrite -= ret;
			i++;
		}

		CALL_OUT[CI_MAX_PATH] = '\0';
		if (!(conversion_in = popen(CALL_OUT, "r"))) {
			// Do error handling
			ci_debug_printf(3, "categorize_external_text: failed to popen\n");
			ci_membuf_free(tempbody);
			return -1;
		}

		/* read the output of of conversion, one line at a time */
		while (fgets(buff, sizeof(buff), conversion_in) != NULL ) {
			ci_membuf_write(tempbody, buff, strlen(buff), 0);
		}

		/* close the pipe */
		pclose(conversion_in);
	}
	else if (classification_type == EXTERNAL_TEXT)
	{
		char **localargs;
		struct stat stat_buf;

		data->external_body = ci_simple_file_new(0);
		close(data->external_body->fd); // Close early, as it will have exclusive permissions

		child_pid = fork();
		if(child_pid == 0) // We are the child
		{
			i = 0;
			while(externalclassifytypes[data->file_type].text_args[i] != NULL)
				i++;
			localargs = malloc(sizeof(char *) * (i + 2));
			i = 0;
			while(externalclassifytypes[data->file_type].text_args[i] != NULL)
			{
				ret = ci_format_text(req, externalclassifytypes[data->file_type].text_args[i], buff, 511, srv_classify_format_table);
				buff[511] = '\0'; // Terminate for safety!
				localargs[i + 1] = myStrDup(buff);
				i++;
			}
			localargs[i + 1] = NULL;
			localargs[0] = myStrDup(externalclassifytypes[data->file_type].text_program);
			ret = execv(externalclassifytypes[data->file_type].text_program, localargs);
			free(localargs);
		}
		else if(child_pid < 0)
		{
			// Error condition
			ci_debug_printf(3, "categorize_external_text: failed to fork\n");
		}
		else { // We are the original
			waitpid(child_pid, &wait_status, 0);
			if((data->external_body->fd = open(data->external_body->filename, O_RDWR | O_EXCL, F_PERM)))
			{
				if(fstat(data->external_body->fd, &stat_buf) == 0)
				{
					data->external_body->endpos = stat_buf.st_size;
					while((ret = read(data->external_body->fd, buff, 512)) > 0)
					{
						ci_membuf_write(tempbody, buff, ret, 0);
					}
				}
			}
			ci_simple_file_destroy(data->external_body);
		}
	}

	// do classification
	make_wchar_from_buf(req, tempbody);
	return categorize_text(req);
}

int make_pics_header(ci_request_t * req)
{
char *orig_header;
char header[1501];
regmatch_t singleMatch[2];
classify_req_data_t *data = ci_service_data(req);

	// modify headers
	if (!ci_http_response_headers(req))
		ci_http_response_create(req, 1, 1);
	orig_header = ci_http_response_get_header(req, "PICS-Label");
	if(orig_header != NULL)
	{
		strncpy(header, orig_header, 1500);
		header[1500] = '\0';
		header[strlen(header) - 1] = '\0';
	}
	else snprintf(header, 1500, "PICS-Label: (PICS-1.1");

	if(tre_regwexec(&picslabel, (wchar_t *) data->uncompressedbody->buf, 2, singleMatch, 0) != REG_NOMATCH)
	{
		snprintf(header + strlen(header), 1500 - strlen(header), " %.*ls", singleMatch[1].rm_eo - singleMatch[1].rm_so, (wchar_t *) data->uncompressedbody->buf + singleMatch[1].rm_so);
	        ci_http_response_add_header(req, header);
		return 0;
	}
	return 1;
}

int make_wchar(ci_request_t *req)
{
char *charSet;
char *buffer, *tempbuffer = NULL;
ci_membuf_t *tempbody, *input;
char *outputBuffer;
size_t inBytes = 0, outBytes = MAX_WINDOW, status = 0;
classify_req_data_t *data;
iconv_t convert;
ci_off_t content_size = 0;
int i;

     data = ci_service_data(req);

     if(data->uncompressedbody) input = data->uncompressedbody;
     else input = data->mem_body;

     // Fetch document character set
     charSet = findCharset(input->buf, input->endpos);
     if(charSet == NULL)
     {
          if((charSet = ci_http_response_get_header(req, "Content-Type")) != NULL)
          {
               i = strcspn(charSet, "\r\n\0");
               tempbuffer = malloc(i + 1);
               strncpy(tempbuffer, charSet, i);
               tempbuffer[i] = '\0';
               charSet = findCharset(tempbuffer, i);
               free(tempbuffer);
          }
     }
     if(charSet == NULL) charSet = myStrDup("UTF-8");
     for(i = 0; i < strlen(charSet); i++) charSet[i] = toupper(charSet[i]);
     // Some websites send "Windows-1250" instead of "CP1250"
     if(strncmp("WINDOWS-", charSet, 8) == 0)
     {
          charSet[0] = 'C';
          charSet[1] = 'P';
          strcpy(&charSet[2], &charSet[8]);
     }
     // Some websites are criminally broken and return things like "8859-1" instead of "ISO-8859-1"
     else if(strncmp("2022", charSet, 4) == 0 || strncmp("8859", charSet, 4) ==0)
     {
          tempbuffer = malloc(strlen(charSet) + 5);
          strcpy(tempbuffer, "ISO-");
          strcpy(tempbuffer + 4, charSet);
          free(charSet);
          charSet = tempbuffer;
     }

     // Setup and do iconv charset conversion to WCHAR_T
     convert = iconv_open("WCHAR_T", charSet); // UTF-32//TRANSLIT
     if(convert == (iconv_t)-1)
     {
          ci_debug_printf(2, "No conversion from |%s| to WCHAR_T.\n", charSet);
          addTextErrorHeaders(req, NO_CHARSET, charSet);
          free(charSet);
          return CI_ERROR; // no charset conversion available, so bail with error
     }
     content_size = input->endpos;
     buffer = input->buf;
     tempbody = ci_membuf_new_sized((content_size + 33) * UTF32_CHAR_SIZE); // 33 = 1 for termination, 32 for silly fudge-factor
     outputBuffer = (char *) tempbody->buf;
     outBytes = (content_size + 32) * UTF32_CHAR_SIZE; // 32 is the 32 above
     inBytes = content_size;
     ci_debug_printf(10, "Begin conversion from |%s| to WCHAR_T\n", charSet);
     while(inBytes)
     {
          status=iconv(convert, &buffer, &inBytes, &outputBuffer, &outBytes);
          if(status==-1)
          {
               switch(errno) {
                   case EILSEQ: // Invalid character, keep going
                         buffer++;
                         inBytes--;
                         ci_debug_printf(5, "Bad sequence in conversion from %s to WCHAR_T.\n", charSet);
                         break;
                   case EINVAL:
                         // Treat it the same as E2BIG since this is a windowed buffer.
                   case E2BIG:
                         ci_debug_printf(1, "Rewindowing conversion needs to be implemented.\n");
                         inBytes = 0;
                         break;
                   default:
                         ci_debug_printf(2, "Oh, crap, iconv gave us an error, which isn't documented, which we couldn't handle in srv_classify.c: make_wchar.\n");
                         status = -10;
                         break;
               }
          }
     }

     // flush iconv
     iconv(convert, NULL, NULL, &outputBuffer, &outBytes);
     iconv_close(convert);

     // save our data
     tempbody->endpos = ((content_size + 32) * sizeof(wchar_t)) - outBytes; // This formula MUST be the same as setting outBytes above the conversion loop - outBytes
									    // Except that it uses wchar_t now because it is safe to do so!
     // Append a Wide Character NULL (WCNULL) here as the classifier needs the NULL
     ci_membuf_write(tempbody, (char *) WCNULL, sizeof(wchar_t), 1);

     if(data->uncompressedbody) ci_membuf_free(data->uncompressedbody);
     data->uncompressedbody = tempbody;
     tempbody = NULL;

     //cleanup
     ci_debug_printf(7, "Conversion from |%s| to WCHAR_T complete.\n", charSet);
     free(charSet);
     return CI_OK;
}

int make_wchar_from_buf(ci_request_t *req, ci_membuf_t *input)
{
char *charSet = "UTF-8";
char *buffer;
ci_membuf_t *tempbody;
char *outputBuffer;
size_t inBytes = 0, outBytes = MAX_WINDOW, status = 0;
classify_req_data_t *data;
iconv_t convert;
ci_off_t content_size = 0;

     data = ci_service_data(req);

     // Setup and do iconv charset conversion to WCHAR_T from UTF-8
     convert = iconv_open("WCHAR_T", charSet); // UTF-32//TRANSLIT
     if(convert == (iconv_t)-1)
     {
          ci_debug_printf(2, "No conversion from |%s| to WCHAR_T.\n", charSet);
          addTextErrorHeaders(req, NO_CHARSET, charSet);
          return CI_ERROR; // no charset conversion available, so bail with error
     }
     content_size = input->endpos;
     buffer = input->buf;
     tempbody = ci_membuf_new_sized((content_size + 33) * UTF32_CHAR_SIZE); // 33 = 1 for termination, 32 for silly fudge-factor
     outputBuffer = (char *) tempbody->buf;
     outBytes = (content_size + 32) * sizeof(wchar_t); // 32 is the 32 above
     inBytes = content_size;
     ci_debug_printf(10, "Begin conversion from |%s| to WCHAR_T\n", charSet);
     while(inBytes)
     {
          status=iconv(convert, &buffer, &inBytes, &outputBuffer, &outBytes);
          if(status==-1)
          {
               switch(errno) {
                   case EILSEQ: // Invalid character, keep going
                         buffer++;
                         inBytes--;
                         ci_debug_printf(5, "Bad sequence in conversion from %s to WCHAR_T.\n", charSet);
                         break;
                   case EINVAL:
                         // Treat it the same as E2BIG since this is a windowed buffer.
                   case E2BIG:
                         ci_debug_printf(1, "Rewindowing conversion needs to be implemented.\n");
                         inBytes = 0;
                         break;
                   default:
                         ci_debug_printf(2, "Oh, crap, iconv gave us an error, which isn't documented, which we couldn't handle in srv_classify.c: make_wchar_from_buf.\n");
                         status = -10;
                         break;
               }
          }
     }

     // flush iconv
     iconv(convert, NULL, NULL, &outputBuffer, &outBytes);
     iconv_close(convert);

     // save our data
     tempbody->endpos = ((content_size + 32) * sizeof(wchar_t)) - outBytes; // This formula MUST be the same as setting outBytes above the conversion loop - outBytes
									    // Except that it uses wchar_t now because it is safe to do so!
     // Append a Wide Character NULL (WCNULL) here as the classifier needs the NULL
     ci_membuf_write(tempbody, (char *) WCNULL, sizeof(wchar_t), 1);
//     ci_membuf_free(input); -- not needed anymore... here for a short time more only
     data->uncompressedbody = tempbody;
     tempbody = NULL;

     //cleanup
     ci_debug_printf(7, "Conversion from |%s| to WCHAR_T complete.\n", charSet);
     return CI_OK;
}

char *findCharset(const char *input, int64_t length)
{
char *token=NULL;
regmatch_t headMatch[2];
regmatch_t charsetMatch[2];
size_t len=0;

     if((tre_regnexec(&headFinder, input, length, 2, headMatch, 0)) != REG_NOMATCH)
     {
          if((tre_regnexec(&charsetFinder, input + headMatch[1].rm_so, headMatch[1].rm_eo - headMatch[1].rm_so, 2, charsetMatch, 0)) != REG_NOMATCH)
          {
               len = charsetMatch[1].rm_eo - charsetMatch[1].rm_so;
               token = malloc(len + 1);
               memcpy(token, input + headMatch[1].rm_so + charsetMatch[1].rm_so, len);
               token[len] = '\0';
               ci_debug_printf(7, "Charset found: |%s|\n", token);
          }
     }
     return token;
}


#ifdef HAVE_ZLIB
/*
  uncompress function is based on code from the gzip module.
*/

#define ZIP_HEAD_CRC     0x02   /* bit 1 set: header CRC present */
#define ZIP_EXTRA_FIELD  0x04   /* bit 2 set: extra field present */
#define ZIP_ORIG_NAME    0x08   /* bit 3 set: original file name present */
#define ZIP_COMMENT      0x10   /* bit 4 set: file comment present */

int classify_uncompress(ci_request_t * req)
{
     int ret, errors=0;
     char *outputBuffer;
     z_stream strm;
     strm.zalloc = Z_NULL;
     strm.zfree = Z_NULL;
     strm.opaque = Z_NULL;
     classify_req_data_t *data;

     data = ci_service_data(req);

     strm.avail_in = data->mem_body->endpos;
     strm.next_in = (Bytef *) data->mem_body->buf;

     outputBuffer = malloc(MAX_WINDOW);
     strm.avail_out = MAX_WINDOW;
     strm.next_out = (Bytef *) outputBuffer;

     if(data->is_compressed == CI_ENCODE_GZIP)
         ret = inflateInit2(&strm, 32 + 15);
     else
         ret = inflateInit(&strm);
     if (ret != Z_OK)
     {
          ci_debug_printf(1, "Error initializing zlib (inflateInit return: %d)\n", ret);
          addTextErrorHeaders(req, ZLIB_FAIL, NULL);
          return CI_ERROR;
     }
     ci_debug_printf(7, "Decompressing data.\n");

     data->uncompressedbody = ci_membuf_new_sized(strm.avail_in);
     do {
         strm.avail_out = MAX_WINDOW;
         strm.next_out = (Bytef *) outputBuffer;

         ret = inflate(&strm, Z_NO_FLUSH);
         switch (ret) {
              case Z_DATA_ERROR:
                    ci_debug_printf(7, "zlib: Z_DATA_ERROR, attempting to resync.\n");
                    ret=inflateSync(&strm);
                    errors++;
                    break;
              case Z_STREAM_ERROR:
                    ci_debug_printf(7, "zlib: Z_STREAM_ERROR.\n");
                    ret = Z_STREAM_END;
                    break;
              case Z_BUF_ERROR:
                    ci_debug_printf(7, "zlib: Z_BUFF_ERROR.\n");
                    errors++;
                    break;
              case Z_MEM_ERROR:
                    ci_debug_printf(7, "zlib: Z_MEM_ERROR.\n");
                    ret = Z_STREAM_END;
                    break;
              case Z_NEED_DICT:
                    ci_debug_printf(7, "zlib: Z_NEED_DICT.\n");
                    ret = Z_STREAM_END;
                    break;
              case Z_OK:
                    errors=0;
                    break;
         }
         if(errors > 10) ret = Z_STREAM_END;
         ci_membuf_write(data->uncompressedbody, outputBuffer, MAX_WINDOW - strm.avail_out, 0);
         /* done when inflate() says it's done */
     } while (ret != Z_STREAM_END);
     strm.avail_out = MAX_WINDOW;
     strm.next_out = (Bytef *) outputBuffer;
     ret = inflate(&strm, Z_FINISH);
     ci_membuf_write(data->uncompressedbody, outputBuffer, MAX_WINDOW - strm.avail_out, 0);
    // DO NOT APPEND NULL HERE AS make_wchar (iconv) SEEMS TO CAUSE CORRUPTION / CRASHES WITH A NULL

     // cleanup everything
     inflateEnd(&strm);
     free(outputBuffer);
     ci_debug_printf(7, "Finished decompressing data.\n");
     return CI_OK;
}
#endif

void set_istag(ci_service_xdata_t * srv_xdata)
{
     char istag[SERVICE_ISTAG_SIZE + 1];
     char *text_cat_ver="text_categorize1.0";
     char *image_cat_ver="image_categorize1.0";

     /*cfg_version maybe must set by user when he is changing
        the srv_clamav configuration.... */
     snprintf(istag, SERVICE_ISTAG_SIZE, "-T:%s-I:%s",
              text_cat_ver, image_cat_ver);
     istag[SERVICE_ISTAG_SIZE] = '\0';
     ci_service_set_istag(srv_xdata, istag);
}

int get_filetype(ci_request_t * req, char *buf, int len)
{
     int iscompressed, filetype;
     classify_req_data_t *data;

     data = ci_service_data(req);
     filetype = ci_extend_filetype(magic_db, req, buf, len, &iscompressed);
     data->is_compressed=iscompressed;

     return filetype;
}

int must_classify(int file_type, classify_req_data_t * data)
{
     int type, i = 0;
     int *file_groups;

     ci_thread_rwlock_rdlock(&textclassify_rwlock);

     file_groups = ci_data_type_groups(magic_db, file_type);
     type = NO_CLASSIFY;

     if (file_groups) {
         while (file_groups[i] >= 0 && i < MAX_GROUPS) {
	     if ((type = classifygroups[file_groups[i]]) > 0)
	       break;
	     i++;
	 }
     }

     if (type == NO_CLASSIFY)
          type = externalclassifytypes[file_type].data_type;
     if (type == NO_CLASSIFY)
          type = classifytypes[file_type];

     ci_thread_rwlock_unlock(&textclassify_rwlock);

     return type;
}

static void addTextErrorHeaders(ci_request_t *req, int error, char *extra_info)
{
char header[myMAX_HEADER + 1];

	// modify headers
	if (!ci_http_response_headers(req))
		ci_http_response_create(req, 1, 1);

	switch(error)
	{
		case NO_MEMORY:
			snprintf(header, myMAX_HEADER, "X-TEXT-ERROR: OUT OF MEMORY");
			break;
		case NO_CATEGORIES:
			snprintf(header, myMAX_HEADER, "X-TEXT-ERROR: NO CATEGORIES PROVIDED, CAN'T PROCESS");
			break;
		case ZLIB_FAIL:
			snprintf(header, myMAX_HEADER, "X-TEXT-ERROR: ZLIB FAILURE");
			break;
		case NO_CHARSET:
			snprintf(header, myMAX_HEADER, "X-TEXT-ERROR: CANNOT CONVERT %s TO WCHAR_T", (extra_info ? extra_info : "UNKNOWN"));
			break;
		default:
			snprintf(header, myMAX_HEADER, "X-TEXT-ERROR: UNKNOWN ERROR");
			break;
	}
	// add IMAGE-CATEGORIES header
	header[myMAX_HEADER]='\0';
	ci_http_response_add_header(req, header);
	ci_debug_printf(3, "Added error header: %s\n", header);
}

char *myStrDup(const char *string)
{
char *temp;
	if(string == NULL) return NULL;

	temp=malloc(strlen(string)+1);
	strcpy(temp, string);
	return temp;
}

#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
/* All of this code below uses a very simple table.
   It might be better to re-implement this using a tree of some kind. */
void insertReferrer(char *uri, HTMLClassification fhs_classification, HTMLClassification fnb_classification)
{
uint32_t primary = 0, secondary = 0;
int oldest = 0, i;
//HTMLClassification emptyClassification =  { .primary_name = NULL, .primary_probability = 0.0, .primary_probScaled = 0.0, .secondary_name = NULL, .secondary_probability = 0.0, .secondary_probScaled = 0.0  };
	// Compute hash outside of lock
	hashword2((uint32_t *) uri, strlen(uri)/4, &primary, &secondary);

/*	Not sure if this is the right thing to do, but I do not know how to make it work with memcache any other way
	// It is the classification only if it is a solid match
	if(fhs_classification.primary_probScaled < (float) TEXT_SOLID_LEVEL)
		fhs_classification = emptyClassification;
	if(fnb_classification.primary_probScaled < (float) TEXT_SOLID_LEVEL)
		fnb_classification = emptyClassification;
*/

	ci_thread_rwlock_wrlock(&referrers_rwlock);

	// If we already exist, update age and bail
	// While we are at it, find the oldest
	for(i = 0; i < REFERRERS_SIZE; i++)
	{
		if(referrers[i].hash == primary)
		{
			if(strcmp(uri, referrers[i].uri) == 0)
			{
				// Avoid fake outs by resetting the classification
				referrers[i].fhs_classification = fhs_classification;
				referrers[i].fnb_classification = fnb_classification;
				// Reset the age as well
				referrers[i].age = classify_requests;
				ci_thread_rwlock_unlock(&referrers_rwlock);
				return;
			}
		}
		// Find the oldest here to save time
		// We don't do an else because the hash may match while URI doesn't
		if(referrers[i].age < referrers[oldest].age)
		{
			oldest = i;
		}
	}

	// Avoid wrap around problems
	if(classify_requests == 0 && referrers[oldest].age > 0)
	{
		int largest = 0;
		int32_t adjust;

		// We already found the oldest in the previous for loop

		// Adjust downward
		adjust = referrers[oldest].age;

		for(i = 0; i < REFERRERS_SIZE; i++)
		{
			referrers[i].age = referrers[i].age - adjust;
			if(referrers[i].age > referrers[largest].age) // Find largest
				largest = i;
		}
		classify_requests = largest + 1; // Reset our count
	}

	// Replace oldest
	referrers[oldest].hash = primary;
	if(referrers[oldest].uri) free(referrers[oldest].uri);
	referrers[oldest].uri = myStrDup(uri);
	referrers[oldest].fhs_classification = fhs_classification;
	referrers[oldest].fnb_classification = fnb_classification;
	referrers[oldest].age = classify_requests;
	classify_requests++;

	ci_thread_rwlock_unlock(&referrers_rwlock);
}

void getReferrerClassification(const char *uri, HTMLClassification *fhs_classification, HTMLClassification *fnb_classification)
{
uint32_t primary = 0, secondary = 0;
HTMLClassification emptyClassification =  { .primary_name = NULL, .primary_probability = 0.0, .primary_probScaled = 0.0, .secondary_name = NULL, .secondary_probability = 0.0, .secondary_probScaled = 0.0  };
int i;
char *realURI, *pos;
	if(uri == NULL)
	{
		*fhs_classification = emptyClassification;
		*fnb_classification = emptyClassification;
		return;
	}
	// Process the URI to hash and remove cgi parts before lock
	realURI = myStrDup(uri);
	pos = strstr(realURI, "?");
	if(pos != NULL) *pos = '\0';
	hashword2((uint32_t *) realURI, strlen(realURI)/4, &primary, &secondary);

	// lock and find
	ci_thread_rwlock_rdlock(&referrers_rwlock);
	for(i = 0; i < REFERRERS_SIZE; i++)
	{
		if(referrers[i].hash == primary)
		{
			if(strcmp(realURI, referrers[i].uri) == 0)
			{
				*fhs_classification = referrers[i].fhs_classification;
				*fnb_classification = referrers[i].fnb_classification;
				referrers[i].age = classify_requests;
				free(realURI);
				ci_thread_rwlock_unlock(&referrers_rwlock);
				return;
			}
		}
	}
	// We failed to find the URI, return empty classification
	*fhs_classification = emptyClassification;
	*fnb_classification = emptyClassification;
	free(realURI);
	ci_thread_rwlock_unlock(&referrers_rwlock);
}

void addReferrerHeaders(ci_request_t *req, HTMLClassification fhs_classification, HTMLClassification fnb_classification)
{
char reply[2 * CI_MAX_PATH + 1];
char type[20];

     if(fhs_classification.primary_name != NULL)
     {
	  if(fhs_classification.primary_probScaled >= (float) TEXT_AMBIGUOUS_LEVEL && fhs_classification.primary_probScaled < (float) TEXT_SOLID_LEVEL) strcpy(type,"AMBIGUOUS");
	  else if(fhs_classification.primary_probScaled >= (float) TEXT_SOLID_LEVEL) strcpy(type, "SOLID");
	  else strcpy(type,"NEAREST");
	  snprintf(reply, CI_MAX_PATH, "X-REFERRER-TEXT-CATEGORY-HS: %s", fhs_classification.primary_name);
	  reply[CI_MAX_PATH]='\0';
	  ci_http_response_add_header(req, reply);
	  ci_debug_printf(10, "Added header: %s\n", reply);
	  snprintf(reply, CI_MAX_PATH, "X-REFERRER-TEXT-CATEGORY-CONFIDENCE-HS: %s", type);
	  reply[CI_MAX_PATH]='\0';
	  ci_http_response_add_header(req, reply);
	  ci_debug_printf(10, "Added header: %s\n", reply);
	  if(fhs_classification.secondary_name != NULL)
	  {
	       if(fhs_classification.secondary_probScaled >= (float) TEXT_AMBIGUOUS_LEVEL && fhs_classification.secondary_probScaled < (float) TEXT_SOLID_LEVEL) strcpy(type,"AMBIGUOUS");
	       else if(fhs_classification.secondary_probScaled >= (float) TEXT_SOLID_LEVEL) strcpy(type, "SOLID");
	       else strcpy(type,"NEAREST");
	       snprintf(reply, CI_MAX_PATH, "X-REFERRER-TEXT-SECONDARY-CATEGORY-HS: %s", fhs_classification.secondary_name);
	       reply[CI_MAX_PATH]='\0';
	       ci_http_response_add_header(req, reply);
	       ci_debug_printf(10, "Added header: %s\n", reply);
	       snprintf(reply, CI_MAX_PATH, "X-REFERRER-TEXT-SECONDARY-CATEGORY-CONFIDENCE-HS: %s", type);
	       reply[CI_MAX_PATH]='\0';
	       ci_http_response_add_header(req, reply);
	       ci_debug_printf(10, "Added header: %s\n", reply);
	  }
     }
     if(fnb_classification.primary_name != NULL)
     {
	  if(fnb_classification.primary_probScaled >= (float) TEXT_AMBIGUOUS_LEVEL && fnb_classification.primary_probScaled < (float) TEXT_SOLID_LEVEL) strcpy(type,"AMBIGUOUS");
	  else if(fnb_classification.primary_probScaled >= (float) TEXT_SOLID_LEVEL) strcpy(type, "SOLID");
	  else strcpy(type,"NEAREST");
	  snprintf(reply, CI_MAX_PATH, "X-REFERRER-TEXT-CATEGORY-NB: %s", fnb_classification.primary_name);
	  reply[CI_MAX_PATH]='\0';
	  ci_http_response_add_header(req, reply);
	  ci_debug_printf(10, "Added header: %s\n", reply);
	  snprintf(reply, CI_MAX_PATH, "X-REFERRER-TEXT-CATEGORY-CONFIDENCE-NB: %s", type);
	  reply[CI_MAX_PATH]='\0';
	  ci_http_response_add_header(req, reply);
	  ci_debug_printf(10, "Added header: %s\n", reply);
	  if(fnb_classification.secondary_name != NULL)
	  {
	       if(fnb_classification.secondary_probScaled >= (float) TEXT_AMBIGUOUS_LEVEL && fnb_classification.secondary_probScaled < (float) TEXT_SOLID_LEVEL) strcpy(type,"AMBIGUOUS");
	       else if(fnb_classification.secondary_probScaled >= (float) TEXT_SOLID_LEVEL) strcpy(type, "SOLID");
	       else strcpy(type,"NEAREST");
	       snprintf(reply, CI_MAX_PATH, "X-REFERRER-TEXT-SECONDARY-CATEGORY-NB: %s", fnb_classification.secondary_name);
	       reply[CI_MAX_PATH]='\0';
	       ci_http_response_add_header(req, reply);
	       ci_debug_printf(10, "Added header: %s\n", reply);
	       snprintf(reply, CI_MAX_PATH, "X-REFERRER-TEXT-SECONDARY-CATEGORY-CONFIDENCE-NB: %s", type);
	       reply[CI_MAX_PATH]='\0';
	       ci_http_response_add_header(req, reply);
	       ci_debug_printf(10, "Added header: %s\n", reply);
	  }
     }
}

void createReferrerTable(void)
{
	ci_thread_rwlock_wrlock(&referrers_rwlock);
	referrers = calloc(sizeof(REFERRERS_T), REFERRERS_SIZE);
	ci_thread_rwlock_unlock(&referrers_rwlock);
}

void freeReferrerTable(void)
{
int i;
	ci_thread_rwlock_wrlock(&referrers_rwlock);
	for(i = 0; i < REFERRERS_SIZE; i++)
	{
		free(referrers[i].uri);
	}
	free(referrers);
	referrers = NULL;
	ci_thread_rwlock_unlock(&referrers_rwlock);
}
#endif

/***************************************************************************************/
/* Parse arguments function -
   Current arguments: allow204=on|off, force=on, sizelimit=off
*/
void srvclassify_parse_args(classify_req_data_t * data, char *args)
{
     char *str;
     if ((str = strstr(args, "allow204="))) {
          if (strncmp(str + 9, "on", 2) == 0)
               data->args.enable204 = 1;
          else if (strncmp(str + 9, "off", 3) == 0)
               data->args.enable204 = 0;
     }
     if ((str = strstr(args, "force="))) {
          if (strncmp(str + 6, "on", 2) == 0)
               data->args.forcescan = 1;
     }
     if ((str = strstr(args, "sizelimit="))) {
          if (strncmp(str + 10, "off", 3) == 0)
               data->args.sizelimit = 0;
     }
}

/*************************************************************************************/
/* Template Functions                                                                */
int fmt_srv_classify_source(ci_request_t *req, char *buf, int len, const char *param)
{
    classify_req_data_t *data = ci_service_data(req);
    if (! data->disk_body || strlen(data->disk_body->filename) <= 0)
        return 0;

    return snprintf(buf, len, "%s", data->disk_body->filename);
}

int fmt_srv_classify_destination(ci_request_t *req, char *buf, int len, const char *param)
{
    classify_req_data_t *data = ci_service_data(req);
    if (! data->external_body || strlen(data->external_body->filename) <= 0)
        return 0;

    return snprintf(buf, len, "%s", data->external_body->filename);
}

/*************************************************************************************/
/* Configuration Functions                                                           */

int cfg_ClassifyFileTypes(const char *directive, const char **argv, void *setdata)
{
     int i, id;
     int type = NO_CLASSIFY;
     if (strcmp(directive, "ImageFileTypes") == 0)
          type = IMAGE;
     else if (strcmp(directive, "TextFileTypes") == 0)
          type = TEXT;
     else
          return 0;

     for (i = 0; argv[i] != NULL; i++) {
          if ((id = ci_get_data_type_id(magic_db, argv[i])) >= 0)
               classifytypes[id] = type;
          else if ((id = ci_get_data_group_id(magic_db, argv[i])) >= 0)
               classifygroups[id] = type;
          else
               ci_debug_printf(1, "Unknown data type %s \n", argv[i]);

     }

     ci_debug_printf(1, "I am going to classify data for %s scanning of type:",
                     (type == 1 ? "TEXT" : "IMAGE"));
     for (i = 0; i < ci_magic_types_num(magic_db); i++) {
          if (classifytypes[i] == type)
               ci_debug_printf(1, ",%s", ci_data_type_name(magic_db, i));
     }
     for (i = 0; i < ci_magic_groups_num(magic_db); i++) {
          if (classifygroups[i] == type)
               ci_debug_printf(1, ",%s", ci_data_group_name(magic_db, i));
     }
     ci_debug_printf(1, "\n");
     return 1;
}

int cfg_TmpDir(const char *directive, const char **argv, void *setdata)
{
     int val = 1;
     char fname[CI_MAX_PATH + 1];
     FILE *test;
     struct stat stat_buf;
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          return 0;
     }
     if (stat(argv[0], &stat_buf) != 0 || !S_ISDIR(stat_buf.st_mode)) {
          ci_debug_printf(1,
                          "The directory %s (%s=%s) does not exist or is not a directory !!!\n",
                          argv[0], directive, argv[0]);
          return 0;
     }

     // Try to write to the directory to see if it is writable .......
     snprintf(fname, CI_MAX_PATH, "%s/test.txt", argv[0]);
     test = fopen(fname, "w+");
     if (test == NULL)
     {
          ci_debug_printf(1,
                          "The directory %s is not writeable!!!\n", argv[0]);
          val = -1;
          return -1;
     }
     else fclose(test);
     unlink(fname);

     // All is well, set the parameter
     if(CLASSIFY_TMP_DIR)
     {
          free(CLASSIFY_TMP_DIR);
          CLASSIFY_TMP_DIR=NULL;
     }
     CLASSIFY_TMP_DIR = myStrDup(argv[0]);
     return val;
}

int cfg_DoTextPreload(const char *directive, const char **argv, void *setdata)
{
int ret = 0;
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "Format: %s LOCATION_OF_FHS_PRELOAD_FILE\n", directive);
          return 0;
     }
     ci_debug_printf(1, "BE PATIENT -- Preloading Text Classification File: %s\n", argv[0]);
     ci_thread_rwlock_wrlock(&textclassify_rwlock);
     if(isHyperSpace(argv[0]))
          ret = preLoadHyperSpace(argv[0]);
     else if(isBayes(argv[0]))
          ret = preLoadBayes(argv[0]);
     ci_thread_rwlock_unlock(&textclassify_rwlock);
     return ret;
}

int cfg_AddTextCategory(const char *directive, const char **argv, void *setdata)
{
int val = 0;
     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "Format: %s NAME LOCATION_OF_FHS_FILE\n", directive);
          return val;
     }
     ci_debug_printf(1, "BE PATIENT -- Loading and optimizing Text Category: %s from File: %s\n", argv[0], argv[1]);
     ci_thread_rwlock_wrlock(&textclassify_rwlock);
     if(isHyperSpace(argv[1]))
          val = loadHyperSpaceCategory(argv[1], argv[0]);
     else if(isBayes(argv[1]))
          val = loadBayesCategory(argv[1], argv[0]);
     ci_thread_rwlock_unlock(&textclassify_rwlock);
     return val;
}

int cfg_AddTextCategoryDirectoryHS(const char *directive, const char **argv, void *setdata)
{
int val = 0;
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "Format: %s LOCATION_OF_FHS_FILES\n", directive);
          return val;
     }
     ci_debug_printf(1, "BE PATIENT -- Mass Loading and optimizing Text Categories from directory: %s\n", argv[0]);
     ci_thread_rwlock_wrlock(&textclassify_rwlock);
     val = loadMassHSCategories(argv[0]);
     ci_thread_rwlock_unlock(&textclassify_rwlock);
     return val;
}

int cfg_AddTextCategoryDirectoryNB(const char *directive, const char **argv, void *setdata)
{
int val = 0;
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "Format: %s LOCATION_OF_FNB_FILES\n", directive);
          return val;
     }
     ci_debug_printf(1, "BE PATIENT -- Mass Loading and optimizing Text Categories from directory: %s\n", argv[0]);
     ci_thread_rwlock_wrlock(&textclassify_rwlock);
     val = loadMassBayesCategories(argv[0]);
     ci_thread_rwlock_unlock(&textclassify_rwlock);
     return val;
}

int cfg_TextHashSeeds(const char *directive, const char **argv, void *setdata)
{
     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "Format: %s 32BIT_HASHSEED1 32BIT_HASHSEED2\n", directive);
          return 0;
     }
     sscanf(argv[0], "%x", &HASHSEED1);
     sscanf(argv[1], "%x", &HASHSEED2);

     ci_debug_printf(1, "Setting parameter :%s (HASHSEED1: 0x%x HASHSEED2: 0x%x)\n", directive, HASHSEED1, HASHSEED2);
     return 1;
}

int cfg_TextSecondary(const char *directive, const char **argv, void *setdata)
{
     unsigned int bidirectional = 0;

     if (argv == NULL || argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "Format: %s PRIMARY_CATEGORY_REGEX SECONDARY_CATEGORY_REGEX OPTIONALY_BIDIRECTIONAL_BINARY_TRUE_FALSE\n", directive);
          return 0;
     }
     if(argv[2] != NULL)
     {
          errno = 0;
          bidirectional = strtoll(argv[2], NULL, 10);
          if (errno != 0)
              return 0;
     }

     if(number_secondaries == 0 || secondary_compares == NULL)
     {
          secondary_compares = malloc(sizeof(secondaries_t));
     }
     else
     {
          secondary_compares = realloc(secondary_compares, sizeof(secondaries_t) * (number_secondaries + 1));
     }

     if(tre_regcomp(&secondary_compares[number_secondaries].primary_regex, argv[0], REG_EXTENDED | REG_ICASE) != 0 ||
          tre_regcomp(&secondary_compares[number_secondaries].secondary_regex, argv[1], REG_EXTENDED | REG_ICASE) != 0)
     {
          number_secondaries--;
          secondary_compares = realloc(secondary_compares, sizeof(secondaries_t) * (number_secondaries + 1));
          ci_debug_printf(1, "Invalid REGEX In Setting parameter :%s (PRIMARY_CATEGORY_REGEX: %s SECONDARY_CATEGORY_REGEX: %s BIDIRECTIONAL: %s)\n", directive, argv[0], argv[1], bidirectional ? "TRUE" : "FALSE" );
          return 0;
     }
     secondary_compares[number_secondaries].bidirectional = bidirectional;

     ci_debug_printf(1, "Setting parameter :%s (PRIMARY_CATEGORY_REGEX: %s SECONDARY_CATEGORY_REGEX: %s BIDIRECTIONAL: %s)\n", directive, argv[0], argv[1], bidirectional ? "TRUE" : "FALSE" );
     number_secondaries++;
     return 1;
}

int cfg_OptimizeFNB(const char *directive, const char **argv, void *setdata)
{
     optimizeFBC(&NBJudgeHashList);

     ci_debug_printf(1, "Optimizing FBC Data\n");
     return 1;
}

int cfg_ExternalTextConversion(const char *directive, const char **argv, void *setdata)
{
     int i, id = -1, k;
     int type = NO_CLASSIFY;
     if(argv[0] == NULL || argv[1] == NULL || argv[2] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          if(strstr(directive, "Text") != NULL) {
               ci_debug_printf(1, "Format: %s (STDOUT|FILE) FILE_TYPE PROGRAM ARG1 ARG2 ARG3 ...\n", directive);
          }
          return 0;
     }
     if (strcmp(directive, "ExternalTextFileType") == 0)
     {
          if(strcmp(argv[0], "STDOUT") == 0)
               type |= EXTERNAL_TEXT_PIPE;
          else if(strcmp(argv[0], "FILE") == 0)
               type |= EXTERNAL_TEXT;
          else {
               ci_debug_printf(1, "Incorrect second argument in directive:%s\n", directive);
               ci_debug_printf(1, "Format: %s (STDOUT|FILE) FILE_TYPE PROGRAM ARG1 ARG2 ARG3 ...\n", directive);
               return 0;
          }
     }
     else
          return 0;

     if(strstr(directive, "FileType") != NULL)
     {
          if ((id = ci_get_data_type_id(magic_db, argv[1])) >= 0)
          {
               if(externalclassifytypes[id].data_type & type)
               {
                    ci_debug_printf(1, "%s: already configured to handle %s\n", directive, argv[1]);
                    return 0;
               }
               else externalclassifytypes[id].data_type |= type;
          }
          else
               ci_debug_printf(1, "Unknown data type %s \n", argv[1]);
     }
     // FIXME: Handle Mime Types here

     if(id >= 0)
     {
          // Handle Program name
          externalclassifytypes[id].text_program = myStrDup(argv[2]);

          // Process argument list
          i = 0;
          while(argv[3 + i] != NULL)
               i++;
          externalclassifytypes[id].text_args = malloc(sizeof(char *) * (i + 1));
          for(k = 0; k < i; k++)
          {
              externalclassifytypes[id].text_args[k] = myStrDup(argv[3 + k]);
          }
          externalclassifytypes[id].text_args[k] = NULL;
     }

     ci_debug_printf(1, "Setting parameter :%s (Using program: %s [arguments hidden] to convert data for type %s, receiving via: %s)\n", directive, argv[2], argv[1], argv[0]);
     return 1;
}

#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
int cfg_ExternalImageConversion(const char *directive, const char **argv, void *setdata)
{
     int i, id = -1, k;
     int type = NO_CLASSIFY;
     if(argv[0] == NULL || argv[1] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          if(strstr(directive, "Text") != NULL) {
               ci_debug_printf(1, "Format: %s FILE_TYPE PROGRAM ARG1 ARG2 ARG3 ...\n", directive);
          }
          return 0;
     }
     type = EXTERNAL_IMAGE;

     if(strstr(directive, "FileType") != NULL)
     {
          if ((id = ci_get_data_type_id(magic_db, argv[0])) >= 0)
          {
               if(externalclassifytypes[id].data_type & type)
               {
                    ci_debug_printf(1, "%s: already configured to handle %s\n", directive, argv[0]);
                    return 0;
               }
               else externalclassifytypes[id].data_type |= type;
          }
          else
               ci_debug_printf(1, "Unknown data type %s \n", argv[0]);
     }
     // FIXME: Handle Mime Types here

     if(id >= 0)
     {
          // Handle Program name
          externalclassifytypes[id].image_program = myStrDup(argv[1]);

          // Process argument list
          i = 0;
          while(argv[2 + i] != NULL)
               i++;
          externalclassifytypes[id].image_args = malloc(sizeof(char *) * (i + 1));
          for(k = 0; k < i; k++)
          {
              externalclassifytypes[id].image_args[k] = myStrDup(argv[2 + k]);
          }
          externalclassifytypes[id].image_args[k] = NULL;
     }

     ci_debug_printf(1, "Setting parameter :%s (Using program: %s [arguments hidden] to convert data for type %s)\n", directive, argv[1], argv[0]);
     return 1;
}
#endif
