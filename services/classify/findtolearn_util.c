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


#define PREHASH_COUNT_SIZE sizeof(uint_least32_t)
#define PREHASH_SIZE sizeof(uint_least64_t)

int writePREHASHES(int file, HashList *hashes_list)
{
uint32_t i;
uint_least32_t qty = 0;
int writecheck;

	if(hashes_list->used) // check before we write
	{
		qty = hashes_list->used;
		do {
			writecheck = write(file, &qty, PREHASH_COUNT_SIZE);
			if(writecheck < PREHASH_COUNT_SIZE) lseek64(file, -writecheck, SEEK_CUR);
		} while (writecheck >=0 && writecheck < PREHASH_COUNT_SIZE);

		for(i = 0; i < hashes_list->used; i++)
		{
			do { // write hash
				writecheck = write(file, &hashes_list->hashes[i], PREHASH_SIZE);
		                if(writecheck < PREHASH_SIZE) lseek64(file, -writecheck, SEEK_CUR);
			} while (writecheck >=0 && writecheck < PREHASH_SIZE);
		}
		ftruncate(file, lseek64(file, 0, SEEK_CUR));
		/* Ok, have written hashes, now save new count */
//		printf("%"PRIu32" hashes, wrote %"PRIu32" hashes\n", hashes_list->used, qty);
		return 0;
	}
	return -1;
}

int readPREHASHES(int file, HashList *hashes_list)
{
uint_least32_t qty = 0;
int readcheck;

	do {
		readcheck = read(file, &qty, PREHASH_COUNT_SIZE);
		if(readcheck < PREHASH_COUNT_SIZE) lseek64(file, -readcheck, SEEK_CUR);
	} while (readcheck >0 && readcheck < PREHASH_COUNT_SIZE);

	hashes_list->used = qty;
	hashes_list->hashes = malloc(sizeof(HTMLFeature) * qty);
	hashes_list->slots = qty;

	do { // read hash
		readcheck = read(file, hashes_list->hashes, PREHASH_SIZE * qty);
                if(readcheck < PREHASH_SIZE * qty) lseek64(file, -readcheck, SEEK_CUR);
	} while (readcheck > 0 && readcheck < PREHASH_SIZE * qty);

	return 0;
}
