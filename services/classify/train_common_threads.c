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

#include <stdlib.h>
#include <stdio.h>
#include <langinfo.h>
#include <string.h>
#include <errno.h>

#define _TRAIN_COMMON_THREADS
#include "train_common_threads.h"

void addFile(char *full_path, char *file_name)
{
process_entry *item;
	if(file_names)
		item = file_names;
	else
		item = calloc(1, sizeof(process_entry));
	file_names = item->next;

	// Add item to busy list
	item->next = busy_file_names;
	busy_file_names = item;

	item->full_path = strdup(full_path);
	item->file_name = strdup(file_name);

	file_available++;
	file_need_data--;
	pthread_cond_signal(&file_avl_cnd);
}

process_entry getNextFile(void)
{
process_entry *item = NULL, ret_entry = {NULL, NULL, NULL};
	if(file_available)
	{
		item = busy_file_names;
		busy_file_names = item->next;

		// Add item back to free list
		item->next = file_names;
		file_names = item;

		ret_entry = *item;
		file_available--;
	}
	return ret_entry;
}

int start_threads(struct thread_info *tinfo, int num_threads, void *thread_start(void *arg))
{
int s, tnum;

	pthread_mutex_init(&file_mtx, NULL);
	pthread_mutex_init(&train_mtx, NULL);
	pthread_cond_init(&file_avl_cnd, NULL);
	pthread_cond_init(&need_file_cnd, NULL);

	/* Create one thread for each command-line argument */

	for (tnum = 0; tnum < num_threads; tnum++) {
		tinfo[tnum].thread_num = tnum + 1;

		/* The pthread_create() call stores the thread ID into
		corresponding element of tinfo[] */

		s = pthread_create(&tinfo[tnum].thread_id, NULL,
		                  thread_start, &tinfo[tnum]);
		if (s != 0)
			handle_error_en(s, "pthread_create");
	}
	return 0;
}

void destroy_threads(struct thread_info *tinfo)
{
	free(tinfo);

	pthread_mutex_destroy(&file_mtx);
	pthread_mutex_destroy(&train_mtx);
	pthread_cond_destroy(&file_avl_cnd);
	pthread_cond_destroy(&need_file_cnd);
}
