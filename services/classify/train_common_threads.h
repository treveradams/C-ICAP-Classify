/*
 *  Copyright (C) 2008-2012 Trever L. Adams
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


#include <pthread.h>

struct thread_info {    /* Used as argument to thread_start() */
	pthread_t thread_id;        /* ID returned by pthread_create() */
	int       thread_num;       /* Application-defined thread # */
};

typedef struct _process_entry {
	char *full_path;
	char *file_name;
	struct _process_entry *next;
} process_entry;


#define handle_error_en(en, msg) \
	do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)

#ifdef _TRAIN_COMMON_THREADS
extern pthread_mutex_t file_mtx, train_mtx;
extern pthread_cond_t file_avl_cnd, need_file_cnd;
extern int file_need_data;
extern int file_available;
extern int shutdown;

extern process_entry *file_names;
extern process_entry *busy_file_names;
// The following is how many files names get added to the list
// per request from a worker thread saying that more names are needed.
// This should be at least 1. It should be small enough that the stat()
// calls don't stall things out too badly.
const int FILES_PER_NEED_MULT = 2;
#endif

#ifndef _TRAIN_COMMON_THREADS
extern process_entry getNextFile(void);
extern void addFile(char *full_path, char *file_name);
extern int start_threads(struct thread_info *tinfo, int num_threads, void *thread_start(void *arg));
extern void destroy_threads(struct thread_info *tinfo);
extern const int FILES_PER_NEED_MULT;
#endif
