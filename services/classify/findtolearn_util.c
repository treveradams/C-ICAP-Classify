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


#define PREHASH_COUNT_SIZE sizeof(uint_least32_t)
#define PREHASH_SIZE sizeof(uint_least64_t)

int writePREHASHES(int file, HashList *hashes_list)
{
uint32_t i;
uint_least32_t qty = 0;
#ifndef _POSIX_MAPPED_FILES
int writecheck;
#else
struct stat st;
int64_t mmap_offset = 0, size;
char *address;
#endif

	if(hashes_list->used) // check before we write
	{
		qty = hashes_list->used;

#ifdef _POSIX_MAPPED_FILES
		// Truncate file and setup mmap
		i = ftruncate64(file, (hashes_list->used * PREHASH_SIZE) +  PREHASH_COUNT_SIZE);
		fstat(file, &st);
		size = st.st_size;
		address = mmap(0, size, PROT_WRITE, MAP_SHARED, file, 0);
		if(address == MAP_FAILED)
		{
			ci_debug_printf(3, "writePREHASHES: Failed to mmap with error: %s\n", strerror(errno));
			return -1; // If we can't mmap, we can't function
		}
		// Write out prehash count
		binary2char((char *) &qty, address, PREHASH_COUNT_SIZE);
		mmap_offset += PREHASH_COUNT_SIZE;
#else
		do {
			writecheck = write(file, &qty, PREHASH_COUNT_SIZE);
			if(writecheck < PREHASH_COUNT_SIZE) lseek64(file, -writecheck, SEEK_CUR);
		} while (writecheck >=0 && writecheck < PREHASH_COUNT_SIZE);
#endif

		for(i = 0; i < hashes_list->used; i++)
		{
#ifdef _POSIX_MAPPED_FILES
			// write hash
			binary2char((char *) &hashes_list->hashes[i], address + mmap_offset, PREHASH_SIZE);
			mmap_offset += PREHASH_SIZE;
#else
			do { // write hash
				writecheck = write(file, &hashes_list->hashes[i], PREHASH_SIZE);
		                if(writecheck < PREHASH_SIZE) lseek64(file, -writecheck, SEEK_CUR);
			} while (writecheck >=0 && writecheck < PREHASH_SIZE);
#endif
		}
#ifdef _POSIX_MAPPED_FILES
		// wrap up mmap
/*		if(msync(address, size, MS_SYNC) < 0) {
			ci_debug_printf(3, "writeFBCHashes: msync failed with error: %s\n", strerror(errno));
			strerror(errno);
		} */
		if(munmap(address, size) < 0) {
			ci_debug_printf(3, "writePREHASHES: munmap failed with error: %s\n", strerror(errno));
		}
		i = ftruncate64(file, (qty * PREHASH_SIZE) + PREHASH_COUNT_SIZE);
#else
		ftruncate(file, lseek64(file, 0, SEEK_CUR));
#endif
		/* Ok, have written hashes, now save new count */
//		ci_debug_printf(10, "%"PRIu32" hashes, wrote %"PRIu32" hashes\n", hashes_list->used, qty);
		return 0;
	}
	return -1;
}

int readPREHASHES(int file, HashList *hashes_list)
{
uint_least32_t qty = 0;
int readcheck;
struct stat st;

	fstat(file, &st);
	if(file < 0 || st.st_size < PREHASH_COUNT_SIZE)
	{
		hashes_list->used = 0;
		hashes_list->hashes = NULL;
		hashes_list->slots = 0;
		return -999;
	}
	do {
		readcheck = read(file, &qty, PREHASH_COUNT_SIZE);
		if(readcheck < PREHASH_COUNT_SIZE) lseek64(file, -readcheck, SEEK_CUR);
	} while (readcheck > 0 && readcheck < PREHASH_COUNT_SIZE);


	if(st.st_size < PREHASH_COUNT_SIZE + (PREHASH_SIZE * qty))
	{
		ci_debug_printf(1, "readPREHASHES: file too small for number of records it says it holds. Aborting prehash read.\n");
		hashes_list->used = 0;
		hashes_list->hashes = NULL;
		hashes_list->slots = 0;
		return -999;
	}
	hashes_list->used = qty;
	hashes_list->hashes = malloc(sizeof(HTMLFeature) * qty);
	hashes_list->slots = qty;

	do { // read hash
		readcheck = read(file, hashes_list->hashes, PREHASH_SIZE * qty);
                if(readcheck < PREHASH_SIZE * qty) lseek64(file, -readcheck, SEEK_CUR);
	} while (readcheck > 0 && readcheck < PREHASH_SIZE * qty);

	return 0;
}
