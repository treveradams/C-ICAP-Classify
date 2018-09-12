void hashword2 (const uint32_t *k, /* the key, an array of uint32_t values */
                size_t length, /* the length of the key, in uint32_ts */
                uint32_t *pc,  /* IN: seed OUT: primary hash value */
                uint32_t *pb); /* IN: more seed OUT: secondary hash value */

void hashlittle2 (const void *key,       /* the key to hash */
                  size_t      length,    /* length of the key */
                  uint32_t   *pc,        /* IN: primary initval, OUT: primary hash */
                  uint32_t   *pb);       /* IN: secondary initval, OUT: secondary hash */

void hashbig2 (const void *key,       /* the key to hash */
               size_t      length,    /* length of the key */
               uint32_t   *pc,        /* IN: primary initval, OUT: primary hash */
               uint32_t   *pb);       /* IN: secondary initval, OUT: secondary hash */

#if SIZEOFWCHAR == 4
#define lookup3_hashfunction(array, length, primary_seed, secondary_seed) hashword2(array, length, primary_seed, secondary_seed);
#endif
#if SIZEOFWCHAR == 2
#if defined(__LITTLE_ENDIAN) || defined(LITTLE_ENDIAN)
#define lookup3_hashfunction(array, length, primary_seed, secondary_seed) hashlittle2(array, length * sizeof(wchar_t), primary_seed, secondary_seed);
#else if defined(__BIG_ENDIAN) || defined(BIG_ENDIAN)
#define lookup3_hashfunction(array, length, primary_seed, secondary_seed) hashbig2(array, length * sizeof(wchar_t), primary_seed, secondary_seed);
#endif
#endif
