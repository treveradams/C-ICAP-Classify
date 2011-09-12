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

#include "autoconf.h"

#include <sys/types.h>
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

#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
extern int categorize_image(ci_request_t * req);
extern int cfg_AddImageCategory(char *directive, char **argv, void *setdata);
extern int cfg_ImageInterpolation(char *directive, char **argv, void *setdata);
extern int cfg_coalesceOverlap(char *directive, char **argv, void *setdata);
extern int cfg_imageCategoryCopies(char *directive, char **argv, void *setdata);
extern void classifyImagePrepReload(void);
#endif

int must_classify(int type, classify_req_data_t * data);

void generate_error_page(classify_req_data_t * data, ci_request_t * req);
char *srvclassify_compute_name(ci_request_t * req);
/***********************************************************************************/
/* Module definitions                                                              */

static int ALLOW204 = 0;
static ci_off_t MAX_OBJECT_SIZE = 0;

static struct ci_magics_db *magic_db = NULL;
static int *classifytypes = NULL;
static int *classifygroups = NULL;

/* TMP DATA DIR */
char *CLASSIFY_TMP_DIR = NULL;

/* Match levels */
static int Ambiguous = -2; // Lowest level to be considered ambiguous
static int SolidMatch = 5; // Lowest level to be considered a solid match

/* Window Size */
static int MAX_WINDOW = 4096; // This is for sliding window buffers

/* Locking */
ci_thread_rwlock_t textclassify_rwlock;

/* Image processing information */
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
#include <opencv/cv.h>
int ImageScaleDimension = 240; // Scale to this dimension
int ImageMaxScale = 4; // Maximum rescale
int ImageMinProcess = 30; // don't process images whose minimum dimension is under this size
int IMAGE_DEBUG_SAVE_ORIG = 0; // DEBUG: Do we save the orignal?
int IMAGE_DEBUG_SAVE_PARTS = 0; // DEBUG: Do we save the parts?
int IMAGE_DEBUG_SAVE_MARKED = 0; // DEBUG: Do save the original marked?
int IMAGE_DEBUG_DEMONSTRATE = 0; // DEBUG: Do we domonstrate?
int IMAGE_DEBUG_DEMONSTRATE_MASKED = 0; // DEBUG: we demonstrate with a mask?
int IMAGE_INTERPOLATION=CV_INTER_LINEAR; // Interpolation function to use.
int IMAGE_CATEGORY_COPIES = IMAGE_CATEGORY_COPIES_MIN; // How many copies of each cascade do we load to avoid openCV bug and bottle necks
extern ci_thread_rwlock_t imageclassify_rwlock;
#endif

/*srv_classify service extra data ... */
ci_service_xdata_t *srv_classify_xdata = NULL;


int srvclassify_init_service(ci_service_xdata_t * srv_xdata,
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
int cfg_ClassifyFileTypes(char *directive, char **argv, void *setdata);
int cfg_DoTextPreload(char *directive, char **argv, void *setdata);
int cfg_AddTextCategory(char *directive, char **argv, void *setdata);
int cfg_AddTextCategoryDirectoryHS(char *directive, char **argv, void *setdata);
int cfg_AddTextCategoryDirectoryNB(char *directive, char **argv, void *setdata);
int cfg_TextHashSeeds(char *directive, char **argv, void *setdata);
int cfg_OptimizeFNB(char *directive, char **argv, void *setdata);
int cfg_ClassifyTmpDir(char *directive, char **argv, void *setdata);
int cfg_TmpDir(char *directive, char **argv, void *setdata);
int cfg_TextSecondary(char *directive, char **argv, void *setdata);
/*General functions*/
int get_filetype(ci_request_t *req, char *buf, int len);
void set_istag(ci_service_xdata_t *srv_xdata);
int categorize_text(ci_request_t *req);
char *findCharset(const char *input, int64_t);
int make_utf32(ci_request_t *req);
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

static uint32_t classify_requests = 0;
#endif

void insertReferrer(char *uri, HTMLClassification fhs_classification, HTMLClassification fnb_classification);
void getReferrerClassification(char *uri, HTMLClassification *fhs_classification, HTMLClassification *fnb_classification);
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
     {"TextAmbiguous", &Ambiguous, ci_cfg_set_int, NULL},
     {"TextSolidMatch", &SolidMatch, ci_cfg_set_int, NULL},
     {"TextFileTypes", NULL, cfg_ClassifyFileTypes, NULL},
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
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     {"ImageFileTypes", NULL, cfg_ClassifyFileTypes, NULL},
     {"ImageScaleDimension", &ImageScaleDimension, ci_cfg_set_int, NULL},
     {"ImageInterpolation", NULL, cfg_ImageInterpolation, NULL},
     {"ImageMaxScale", &ImageMaxScale, ci_cfg_set_int, NULL},
     {"ImageMinProcess", &ImageMinProcess, ci_cfg_set_int, NULL},
     {"ImageCategory", NULL, cfg_AddImageCategory, NULL},
     {"ImageCoalesceOverlap", NULL, cfg_coalesceOverlap, NULL},
     {"ImageCategoryCopies", NULL, cfg_imageCategoryCopies, NULL},
     {"debugSaveOriginal", &IMAGE_DEBUG_SAVE_ORIG, ci_cfg_set_int, NULL},
     {"debugSaveParts", &IMAGE_DEBUG_SAVE_PARTS, ci_cfg_set_int, NULL},
     {"debugSaveMarked", &IMAGE_DEBUG_SAVE_MARKED, ci_cfg_set_int, NULL},
     {"debugDemonstrate", &IMAGE_DEBUG_DEMONSTRATE, ci_cfg_set_int, NULL},
     {"debugDemonstrateMasked", &IMAGE_DEBUG_DEMONSTRATE_MASKED, ci_cfg_set_int, NULL},
#endif
     {NULL, NULL, NULL, NULL}
};


CI_DECLARE_MOD_DATA ci_service_module_t service = {
     "srv_classify",              /*Module name */
     "Web Classification service",        /*Module short description */
     ICAP_RESPMOD,        /*Service type responce or request modification */
     srvclassify_init_service,    /*init_service. */
     NULL,                      /*post_init_service. */
     srvclassify_close_service,   /*close_service */
     srvclassify_init_request_data,       /*init_request_data. */
     srvclassify_release_request_data,    /*release request data */
     srvclassify_check_preview_handler,
     srvclassify_end_of_data_handler,
     srvclassify_io,
     conf_variables,
     NULL
};


int srvclassify_init_service(ci_service_xdata_t * srv_xdata,
                           struct ci_server_conf *server_conf)
{
     int i;

     ci_thread_rwlock_init(&textclassify_rwlock);
     ci_thread_rwlock_wrlock(&textclassify_rwlock);
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     ci_thread_rwlock_init(&imageclassify_rwlock);
     ci_thread_rwlock_wrlock(&imageclassify_rwlock);

     ci_thread_rwlock_init(&referrers_rwlock);
     ci_thread_rwlock_wrlock(&referrers_rwlock);
     createReferrerTable();
#endif

     magic_db = server_conf->MAGIC_DB;
     classifytypes = (int *) malloc(ci_magic_types_num(magic_db) * sizeof(int));
     classifygroups = (int *) malloc(ci_magic_groups_num(magic_db) * sizeof(int));

     for (i = 0; i < ci_magic_types_num(magic_db); i++)
          classifytypes[i] = 0;
     for (i = 0; i < ci_magic_groups_num(magic_db); i++)
          classifygroups[i] = 0;


     ci_debug_printf(10, "Going to initialize srvclassify\n");
     srv_classify_xdata = srv_xdata;      /* Needed by db_reload command */
     set_istag(srv_classify_xdata);
     ci_service_set_preview(srv_xdata, 1024);
     ci_service_enable_204(srv_xdata);
     ci_service_set_transfer_preview(srv_xdata, "*");

     setlocale(LC_ALL, "");
     int utf8_mode = (strcmp(nl_langinfo(CODESET), "UTF-8") == 0);
     if(!utf8_mode) setlocale(LC_ALL, "en_US.utf8");
//#ifdef HAVE_TRE
     initBayesClassifier();
     initHyperSpaceClassifier();
     tre_regwcomp(&picslabel, L"<meta http-equiv=\"PICS-Label\" content='\\(PICS-1.1 ([^']*)'.*/?>", REG_EXTENDED | REG_ICASE);
//#endif
     initHTML();
     ci_thread_rwlock_unlock(&textclassify_rwlock);
#if defined HAVE_OPENCV
     ci_thread_rwlock_unlock(&imageclassify_rwlock);
#endif

     return 1;
}


void srvclassify_close_service()
{
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     freeReferrerTable();
     classifyImagePrepReload();
#endif

     ci_thread_rwlock_wrlock(&textclassify_rwlock);
     if(CLASSIFY_TMP_DIR) free(CLASSIFY_TMP_DIR);
     if(classifytypes) free(classifytypes);
     classifytypes = NULL;
     if(classifygroups) free(classifygroups);
     classifygroups = NULL;
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

     if (req->args) {
          ci_debug_printf(5, "service arguments:%s\n", req->args);
     }
     if (ci_req_hasbody(req)) {
          ci_debug_printf(8, "Request type: %d. Preview size:%d\n", req->type,
                          preview_size);
          if (!(data = malloc(sizeof(classify_req_data_t)))) {
               ci_debug_printf(1,
                               "Error allocation memory for service data!!!!!!!");
               return NULL;
          }
          data->body = NULL;
          data->uncompressedbody = NULL;
          data->must_classify = NO_CLASSIFY;
          if (ALLOW204)
               data->args.enable204 = 1;
          else
               data->args.enable204 = 0;
          data->args.forcescan = 0;
          data->args.sizelimit = 1;
          data->args.mode = 0;

          if (req->args) {
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
          if (((classify_req_data_t *) data)->body)
               ci_simple_file_destroy(((classify_req_data_t *) data)->body);

          if (((classify_req_data_t *) data)->uncompressedbody)
               ci_membuf_free(((classify_req_data_t *) data)->uncompressedbody);

          free(data);
     }
}


int srvclassify_check_preview_handler(char *preview_data, int preview_data_len,
                                    ci_request_t * req)
{
     ci_off_t content_size = 0;
     int file_type;
     classify_req_data_t *data = ci_service_data(req);
     char *content_type = NULL;

     ci_debug_printf(9, "OK The preview data size is %d\n", preview_data_len);

     if (!data || !ci_req_hasbody(req)){
	 ci_debug_printf(9, "No body data, allow 204\n");
          return CI_MOD_ALLOW204;
     }

     /*Going to determine the file type, get_filetype can take preview_data as null ....... */
     file_type = get_filetype(req, preview_data, preview_data_len);
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     data->type_name = ci_data_type_name(magic_db, file_type);
#endif
     if ((data->must_classify = must_classify(file_type, data)) == 0) {
          ci_debug_printf(8, "Not in \"must classify list\". Allow it...... \n");
          return CI_MOD_ALLOW204;
     }

     content_size = ci_http_content_length(req);

     if((content_type = ci_http_response_get_header(req, "Content-Type")) != NULL) {
          if(strstr(content_type, "application/x-javascript") || strstr(content_type, "application/javascript") ||
             strstr(content_type, "application/ecmascript") || strstr(content_type, "text/ecmascript") ||
             strstr(content_type, "text/javascript") || strstr(content_type, "text/jscript") ||
             strstr(content_type, "text/css"))
          {
               ci_debug_printf(8, "Non-content MIME type (%s). Allow it......\n", content_type);
               data->must_classify = NO_CLASSIFY; // these are not likely to contain data and confuse our classifier
               return CI_MOD_ALLOW204;
          }
     }

     if((content_type = ci_http_response_get_header(req, "Content-Encoding")) != NULL) {
          if(strstr(content_type, "gzip")) data->is_compressed =  CI_ENCODE_GZIP;
          else if(strstr(content_type, "deflate")) data->is_compressed = CI_ENCODE_DEFLATE;
          else data->is_compressed = CI_ENCODE_UNKNOWN;
     }
     else data->is_compressed = CI_ENCODE_NONE;

     if (data->args.sizelimit && MAX_OBJECT_SIZE
         && content_size > MAX_OBJECT_SIZE) {
          ci_debug_printf(1,
                          "Object size is %" PRINTF_OFF_T "."
                          " Bigger than max scannable file size (%"
                          PRINTF_OFF_T "). Allow it.... \n", content_size,
                          MAX_OBJECT_SIZE);
          return CI_MOD_ALLOW204;
     }

     data->body = ci_simple_file_new(content_size);
     ci_simple_file_lock_all(data->body);

     if (!data->body)           /*Memory allocation or something else ..... */
          return CI_ERROR;

     if (preview_data_len) {
          ci_simple_file_write(data->body, preview_data, preview_data_len,
                               ci_req_hasalldata(req));
     }
     return CI_MOD_CONTINUE;
}



int srvclassify_read_from_net(char *buf, int len, int iseof, ci_request_t * req)
{
     /*We can put here scanning hor jscripts and html and raw data ...... */
     classify_req_data_t *data = ci_service_data(req);
     if (!data)
          return CI_ERROR;

     /* FIXME the following if should just process what is had at the moment... possible? */
     if (ci_simple_file_size(data->body) >= MAX_OBJECT_SIZE && MAX_OBJECT_SIZE) {
          ci_debug_printf(1, "Object size is bigger than max scannable file size\n");
          data->must_classify = 0;
          ci_req_unlock_data(req);      /*Allow ICAP to send data before receives the EOF....... */
          ci_simple_file_unlock_all(data->body);        /*Unlock all body data to continue send them..... */
     }
     /* anything where we are not in over max object size, simly write and exit */
     return ci_simple_file_write(data->body, buf, len, iseof);
}



int srvclassify_write_to_net(char *buf, int len, ci_request_t * req)
{
     int bytes;
     classify_req_data_t *data = ci_service_data(req);
     if (!data)
          return CI_ERROR;

//     if (data->error_page)
//          return ci_membuf_read(data->error_page, buf, len);

     bytes = ci_simple_file_read(data->body, buf, len);
     return bytes;
}

int srvclassify_io(char *wbuf, int *wlen, char *rbuf, int *rlen, int iseof,
                 ci_request_t * req)
{
     int ret = CI_OK;
     if (rbuf && rlen) {
          *rlen = srvclassify_read_from_net(rbuf, *rlen, iseof, req);
          if (*rlen < 0)
               ret = CI_OK;
     }
     else if (iseof)
          srvclassify_read_from_net(NULL, 0, iseof, req);
     if (wbuf && wlen) {
          *wlen = srvclassify_write_to_net(wbuf, *wlen, req);
     }
     return CI_OK;
}

int srvclassify_end_of_data_handler(ci_request_t * req)
{
     classify_req_data_t *data = ci_service_data(req);
     ci_simple_file_t *body;

     if (!data || !data->body)
          return CI_MOD_DONE;

     body = data->body;

     if (data->must_classify == NO_CLASSIFY) {       /*If exceeds the MAX_OBJECT_SIZE for example ......  */
          ci_debug_printf(8, "Not Classifying\n");
          ci_simple_file_unlock_all(body);          /*Unlock all data to continue send them . Not really needed here.... */
          return CI_MOD_DONE;
     }

     lseek(body->fd, 0, SEEK_SET);

     if (data->must_classify == TEXT)
     {
          ci_debug_printf(8, "Classifying TEXT from file\n");
          #ifdef HAVE_ZLIB
          if (data->is_compressed == CI_ENCODE_GZIP || data->is_compressed == CI_ENCODE_DEFLATE)
               classify_uncompress(req);
          #endif
          if(make_utf32(req) == CI_OK) // We should only categorize text if this reteurns >= 0
          {
               make_pics_header(req);
               categorize_text(req);
          }
     }
#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
     else if (data->must_classify == IMAGE)
     {
          ci_debug_printf(8, "Classifying IMAGE from file\n");
          categorize_image(req);
     }
#endif
     else if (data->allow204 && !ci_req_sent_data(req)) {
          ci_debug_printf(7, "srvClassify module: Respond with allow 204\n");
          return CI_MOD_ALLOW204;
     }

     ci_simple_file_unlock_all(body);   /*Unlock all data to continue send them, after all we only modify headers in this module..... */
     ci_debug_printf(7, "file unlocked, flags :%d (unlocked:%" PRINTF_OFF_T ")\n",
                     body->flags, body->unlocked);
     return CI_MOD_DONE;
}

int categorize_text(ci_request_t * req)
{
classify_req_data_t *data = ci_service_data(req);
char reply[2 * CI_MAX_PATH + 1];
char type[20];
regexHead myRegexHead;
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

     mkRegexHead(&myRegexHead, (wchar_t *) data->uncompressedbody->buf);
     removeHTML(&myRegexHead);
     regexMakeSingleBlock(&myRegexHead);
     normalizeCurrency(&myRegexHead);
     regexMakeSingleBlock(&myRegexHead);

     myHashes.hashes = malloc(sizeof(HTMLFeature) * HTML_MAX_FEATURE_COUNT);
     myHashes.slots = HTML_MAX_FEATURE_COUNT;
     myHashes.used = 0;
     computeOSBHashes(&myRegexHead, HASHSEED1, HASHSEED2, &myHashes);

     HSclassification = doHSPrepandClassify(&myHashes);
     NBclassification = doBayesPrepandClassify(&myHashes);

     free(myHashes.hashes);
     freeRegexHead(&myRegexHead);
     data->uncompressedbody->buf = NULL; // This was freed who knows how many times in the classification, avoid double free

     // modify headers
     if (!ci_http_response_headers(req))
          ci_http_response_create(req, 1, 1);
     if(HSclassification.primary_name != NULL)
     {
          if(HSclassification.primary_probScaled >= (float) Ambiguous && HSclassification.primary_probScaled < (float) SolidMatch) strcpy(type,"AMBIGUOUS");
          else if(HSclassification.primary_probScaled >= (float) SolidMatch) strcpy(type, "SOLID");
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
               if(HSclassification.secondary_probScaled >= (float) Ambiguous && HSclassification.secondary_probScaled < (float) SolidMatch) strcpy(type,"AMBIGUOUS");
               else if(HSclassification.secondary_probScaled >= (float) SolidMatch) strcpy(type, "SOLID");
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
          if(NBclassification.primary_probScaled >= (float) Ambiguous && NBclassification.primary_probScaled < (float) SolidMatch) strcpy(type,"AMBIGUOUS");
          else if(NBclassification.primary_probScaled >= (float) SolidMatch) strcpy(type, "SOLID");
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
               if(NBclassification.secondary_probScaled >= (float) Ambiguous && NBclassification.secondary_probScaled < (float) SolidMatch) strcpy(type,"AMBIGUOUS");
               else if(NBclassification.secondary_probScaled >= (float) SolidMatch) strcpy(type, "SOLID");
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

int make_utf32(ci_request_t * req)
{
char *charSet;
char *buffer, *tempbuffer;
ci_membuf_t *tempbody;
char *outputBuffer;
int i=0;
size_t inBytes=0, outBytes=MAX_WINDOW, status=0;
classify_req_data_t *data;
iconv_t convert;
ci_off_t content_size = 0;

     data = ci_service_data(req);

     if(data->uncompressedbody) // Was the document compressed and did we get the result?
     {
          content_size = data->uncompressedbody->endpos;
     }
     else { // Document wasn't compressed, copy original document!
          content_size = data->body->endpos;
          data->uncompressedbody = ci_membuf_new_sized(content_size + 1);
          if (!data->uncompressedbody)
          {
               ci_debug_printf(1, "make_utf32: Unable to allocate memory for conversion to UTF-32 for Text Classification (%s)!\n", strerror(errno));
               addTextErrorHeaders(req, NO_MEMORY, NULL);
               return CI_ERROR;
           }
          lseek(data->body->fd, 0, SEEK_SET);
          data->uncompressedbody->endpos = 0;
          do {
               inBytes = read(data->body->fd, data->uncompressedbody->buf + data->uncompressedbody->endpos, content_size);
               if(inBytes)
               {
                    data->uncompressedbody->endpos += inBytes;
                    content_size -= data->uncompressedbody->endpos;
               }
          } while (inBytes > 0 && content_size > data->uncompressedbody->endpos);
          // DO NOT APPEND NULL HERE AS iconv SEEMS TO CAUSE CORRUPTION / CRASHES WITH A NULL
          content_size = data->uncompressedbody->endpos;
     }

     // Fetch document character set
     charSet = findCharset(data->uncompressedbody->buf, data->uncompressedbody->endpos);
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
          ci_debug_printf(2, "No conversion from |%s| to UTF-32.\n", charSet);
          addTextErrorHeaders(req, NO_CHARSET, charSet);
          free(charSet);
          return CI_ERROR; // no charset conversion available, so bail with error
     }
     buffer = data->uncompressedbody->buf;
     tempbody = ci_membuf_new_sized((content_size + 33) * sizeof(wchar_t)); // 33 = 1 for termination, 32 for silly fudgefactor
     outputBuffer = (char *) tempbody->buf;
     outBytes = (content_size + 32) * sizeof(wchar_t); // 32 is the 32 above
     inBytes = content_size;
     ci_debug_printf(10, "Begin conversion from |%s| to UTF-32\n", charSet);
     while(inBytes)
     {
          status=iconv(convert, &buffer, &inBytes, &outputBuffer, &outBytes);
          if(status==-1)
          {
               switch(errno) {
                   case EILSEQ: // Invalid character, keep going
                         buffer++;
                         inBytes--;
                         ci_debug_printf(5, "Bad sequence in conversion from %s to UTF-32.\n", charSet);
                         break;
                   case EINVAL:
                         // Treat it the same as E2BIG since this is a windowed buffer.
                   case E2BIG:
                         ci_debug_printf(1, "Rewindowing conversion needs to be implemented.\n");
                         inBytes = 0;
                         break;
                   default:
                         ci_debug_printf(2, "Oh, crap, iconv gave us an error, which isn't documented, which we couldn't handle in srv_classify.c: make_utf8.\n");
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
     // Append a Wide Character NULL (WCNULL) here as the classifier needs the NULL
     ci_membuf_write(tempbody, (char *) WCNULL, 4, 1);
     ci_membuf_free(data->uncompressedbody);
     data->uncompressedbody = tempbody;
     tempbody = NULL;

     //cleanup
     ci_debug_printf(7, "Conversion from |%s| to UTF-32 complete.\n", charSet);
     free(charSet);
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
     ci_membuf_t *tempbody;

     data = ci_service_data(req);

     tempbody = ci_membuf_new_sized(data->body->endpos);
     lseek(data->body->fd, 0, SEEK_SET);
     tempbody->endpos = 0;
     while(tempbody->endpos < data->body->endpos)
         tempbody->endpos += read(data->body->fd, tempbody->buf + tempbody->endpos, data->body->endpos - tempbody->endpos);
     strm.avail_in = tempbody->endpos;
     strm.next_in = (Bytef *) tempbody->buf;

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
          ci_membuf_free(tempbody);
          tempbody = NULL;
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
     ret=inflate(&strm, Z_FINISH);
     ci_membuf_write(data->uncompressedbody, outputBuffer, MAX_WINDOW - strm.avail_out, 0);
    // DO NOT APPEND NULL HERE AS make_utf32 (iconv) SEEMS TO CAUSE CORRUPTION / CRASHES WITH A NULL

     // cleanup everything
     inflateEnd(&strm);
     ci_membuf_free(tempbody);
     tempbody=NULL; // used later so we can't wait for garbage collection
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
			snprintf(header, myMAX_HEADER, "X-TEXT-ERROR: CANNOT CONVERT %s TO UTF-32", (extra_info ? extra_info : "UNKNOWN"));
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

/****************************************************************************************/
/*Configuration Functions                                                               */

int cfg_ClassifyFileTypes(char *directive, char **argv, void *setdata)
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
               ci_debug_printf(2, ",%s", ci_data_type_name(magic_db, i));
     }
     for (i = 0; i < ci_magic_groups_num(magic_db); i++) {
          if (classifygroups[i] == type)
               ci_debug_printf(2, ",%s", ci_data_group_name(magic_db, i));
     }
     ci_debug_printf(1, "\n");
     return 1;
}

int cfg_TmpDir(char *directive, char **argv, void *setdata)
{
     int val = 1;
     char fname[CI_MAX_PATH+1];
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

int cfg_DoTextPreload(char *directive, char **argv, void *setdata)
{
     if (argv == NULL || argv[0] == NULL) {
          ci_debug_printf(1, "Missing arguments in directive:%s\n", directive);
          ci_debug_printf(1, "Format: %s LOCATION_OF_FHS_PRELOAD_FILE\n", directive);
          return 0;
     }
     ci_debug_printf(1, "BE PATIENT -- Preloading Text Classification File: %s\n", argv[0]);
     ci_thread_rwlock_wrlock(&textclassify_rwlock);
     if(isHyperSpace(argv[0]))
          preLoadHyperSpace(argv[0]);
     else if(isBayes(argv[0]))
          preLoadBayes(argv[0]);
     ci_thread_rwlock_unlock(&textclassify_rwlock);
     return 1;
}

int cfg_AddTextCategory(char *directive, char **argv, void *setdata)
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

int cfg_AddTextCategoryDirectoryHS(char *directive, char **argv, void *setdata)
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

int cfg_AddTextCategoryDirectoryNB(char *directive, char **argv, void *setdata)
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


int cfg_TextHashSeeds(char *directive, char **argv, void *setdata)
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

int cfg_TextSecondary(char *directive, char **argv, void *setdata)
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
          number_secondaries++;
          ci_debug_printf(1, "Invalid REGEX In Setting parameter :%s (PRIMARY_CATEGORY_REGEX: %s SECONDARY_CATEGORY_REGEX: %s BIDIRECTIONAL: %s)\n", directive, argv[0], argv[1], bidirectional ? "TRUE" : "FALSE" );
          return 0;
     }
     secondary_compares[number_secondaries].bidirectional = bidirectional;

     ci_debug_printf(1, "Setting parameter :%s (PRIMARY_CATEGORY_REGEX: %s SECONDARY_CATEGORY_REGEX: %s BIDIRECTIONAL: %s)\n", directive, argv[0], argv[1], bidirectional ? "TRUE" : "FALSE" );
     number_secondaries++;
     return 1;
}

int cfg_OptimizeFNB(char *directive, char **argv, void *setdata)
{
     optimizeFBC(&NBJudgeHashList);

     ci_debug_printf(1, "Optimizing FBC Data\n");
     return 1;
}

char *myStrDup(char *string)
{
char *temp;

	temp=malloc(strlen(string)+1);
	strcpy(temp, string);
	return temp;
}

#if defined(HAVE_OPENCV) || defined(HAVE_OPENCV_22X)
/* All of this code below uses a very simple table.
   It might be better to reimplement this using a tree of some kind. */
void insertReferrer(char *uri, HTMLClassification fhs_classification, HTMLClassification fnb_classification)
{
uint32_t primary = 0, secondary = 0;
int oldest = 0, i;
	ci_thread_rwlock_wrlock(&referrers_rwlock);

	hashword2((uint32_t *) uri, strlen(uri)/4, &primary, &secondary);

	// If we already exist, update age and bail
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
	}

	classify_requests++;

	// Avoid wrap around problems
	if(classify_requests == 0)
	{
		int largest = 0;
		int32_t adjust;

		// Find smallest -- Also the oldest
		for(i = 0; i < REFERRERS_SIZE; i++)
		{
			if(referrers[i].age < referrers[oldest].age)
				oldest = i;
		}
		// Adjust downward
		adjust = referrers[oldest].age - 1;
		for(i = 0; i < REFERRERS_SIZE; i++)
		{
			referrers[i].age = referrers[i].age - adjust;
			if(referrers[i].age > referrers[largest].age) // Find largest
				largest = i;
		}
		classify_requests = largest + 1; // Reset our count
	}

	// Find oldest
	else for(i = 0; i < REFERRERS_SIZE; i++)
	{
		if(referrers[i].age < referrers[oldest].age || referrers[i].age == 0)
		{
			oldest = i;
		}
	}

	// Replace oldest
	referrers[oldest].hash = primary;
	if(referrers[oldest].uri) free(referrers[oldest].uri);
	referrers[oldest].uri = myStrDup(uri);
	referrers[oldest].fhs_classification = fhs_classification;
	referrers[oldest].fnb_classification = fnb_classification;
	referrers[oldest].age = classify_requests;

	ci_thread_rwlock_unlock(&referrers_rwlock);
}

void getReferrerClassification(char *uri, HTMLClassification *fhs_classification, HTMLClassification *fnb_classification)
{
uint32_t primary = 0, secondary = 0;
HTMLClassification emptyClassification =  { .primary_name = NULL, .primary_probability = 0.0, .primary_probScaled = 0.0, .secondary_name = NULL, .secondary_probability = 0.0, .secondary_probScaled = 0.0  };
int i;
char *realURI, *pos;

	ci_thread_rwlock_rdlock(&referrers_rwlock);
	realURI = myStrDup(uri);
	pos = strstr(realURI, "?");
	if(pos != NULL) *pos = '\0';
	hashword2((uint32_t *) realURI, strlen(realURI)/4, &primary, &secondary);
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
	  if(fhs_classification.primary_probScaled >= (float) Ambiguous && fhs_classification.primary_probScaled < (float) SolidMatch) strcpy(type,"AMBIGUOUS");
	  else if(fhs_classification.primary_probScaled >= (float) SolidMatch) strcpy(type, "SOLID");
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
	       if(fhs_classification.secondary_probScaled >= (float) Ambiguous && fhs_classification.secondary_probScaled < (float) SolidMatch) strcpy(type,"AMBIGUOUS");
	       else if(fhs_classification.secondary_probScaled >= (float) SolidMatch) strcpy(type, "SOLID");
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
	  if(fnb_classification.primary_probScaled >= (float) Ambiguous && fnb_classification.primary_probScaled < (float) SolidMatch) strcpy(type,"AMBIGUOUS");
	  else if(fnb_classification.primary_probScaled >= (float) SolidMatch) strcpy(type, "SOLID");
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
	       if(fnb_classification.secondary_probScaled >= (float) Ambiguous && fnb_classification.secondary_probScaled < (float) SolidMatch) strcpy(type,"AMBIGUOUS");
	       else if(fnb_classification.secondary_probScaled >= (float) SolidMatch) strcpy(type, "SOLID");
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
	ci_thread_rwlock_unlock(&referrers_rwlock);
}
#endif
