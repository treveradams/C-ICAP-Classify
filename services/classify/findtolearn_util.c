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
