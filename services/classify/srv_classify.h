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


#ifndef __SRV_CLASSIFY_H
#define __SRV_CLASSIFY_H

#define myMAX_HEADER 512

#define IMAGE_CATEGORY_COPIES_MIN 10

typedef struct classify_req_data {
     ci_simple_file_t *body;
     ci_membuf_t *uncompressedbody;
     ci_request_t *req;
#ifdef HAVE_OPENCV
     char *type_name;
#endif
     int must_classify;
     int is_compressed;
     int allow204;
     struct{
	  int enable204;
	  int forcescan;
	  int sizelimit;
	  int mode;
     } args;
} classify_req_data_t;

enum {NO_CLASSIFY=0, TEXT, IMAGE};

char *myStrDup(char *string);

#endif