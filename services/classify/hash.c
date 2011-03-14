#include <inttypes.h>

uint32_t HASHSEED1=0xDEADBADL;
uint32_t HASHSEED2=0xBADDEADL;

/*
-------------------------------------------------------------------------------
from lookup3.c, by Bob Jenkins, May 2006, Public Domain.

You can use this free for any purpose. It's in the public domain.
It has no warranty.
-------------------------------------------------------------------------------
*/
#include <time.h> /* defines time_t for timings in the test */
#include <sys/param.h> /* attempt to define endianness */
#ifdef linux
# include <endian.h> /* attempt to define endianness */
#endif

/*
 * My best guess at if you are big-endian or little-endian. This may
 * need adjustment.
 */
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
 __BYTE_ORDER == __LITTLE_ENDIAN) || \
 (defined(i386) || defined(__i386__) || defined(__i486__) || \
 defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL))
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
 __BYTE_ORDER == __BIG_ENDIAN) || \
 (defined(sparc) || defined(POWERPC) || defined(mc68000) || defined(sel))
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 0
#endif

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
 of top bits of (a,b,c), or in any combination of bottom bits of
 (a,b,c).
* "differ" is defined as +, -, ^, or ~^. For + and -, I transformed
 the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
 is commonly produced by subtraction) look like a single 1-bit
 difference.
* the base values were pseudorandom, all zero but one bit set, or
 all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
 4 6 8 16 19 4
 9 15 3 18 27 15
 14 9 3 7 17 3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta. I
used http://burtleburtle.net/bob/hash/avalanche.html to choose
the operations, constants, and arrangements of the variables.

This does not achieve avalanche. There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a. The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism. Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism. I did what I could. Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
 a -= c; a ^= rot(c, 4); c += b; \
 b -= a; b ^= rot(a, 6); a += c; \
 c -= b; c ^= rot(b, 8); b += a; \
 a -= c; a ^= rot(c,16); c += b; \
 b -= a; b ^= rot(a,19); a += c; \
 c -= b; c ^= rot(b, 4); b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different. This was tested for
* pairs that differed by one bit, by two bits, in any combination
 of top bits of (a,b,c), or in any combination of bottom bits of
 (a,b,c).
* "differ" is defined as +, -, ^, or ~^. For + and -, I transformed
 the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
 is commonly produced by subtraction) look like a single 1-bit
 difference.
* the base values were pseudorandom, all zero but one bit set, or
 all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
 4 8 15 26 3 22 24
 10 8 15 26 3 22 24
 11 8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
 c ^= b; c -= rot(b,14); \
 a ^= c; a -= rot(c,11); \
 b ^= a; b -= rot(a,25); \
 c ^= b; c -= rot(b,16); \
 a ^= c; a -= rot(c,4); \
 b ^= a; b -= rot(a,14); \
 c ^= b; c -= rot(b,24); \
}

/*
--------------------------------------------------------------------
hashword2() -- same as hashword(), but take two seeds and return two
32-bit values. pc and pb must both be nonnull, and *pc and *pb must
both be initialized with seeds. If you pass in (*pb)==0, the output
(*pc) will be the same as the return value from hashword().

By Bob Jenkins, 2006. bob_jenkins@burtleburtle.net. You may use this
code any way you wish, private, educational, or commercial. It's free.
--------------------------------------------------------------------
*/
void hashword2 (
const uint32_t *k, /* the key, an array of uint32_t values */
size_t length, /* the length of the key, in uint32_ts */
uint32_t *pc, /* IN: seed OUT: primary hash value */
uint32_t *pb) /* IN: more seed OUT: secondary hash value */
{
 uint32_t a,b,c;

 /* Set up the internal state */
 a = b = c = 0xdeadbeef + ((uint32_t)(length<<2)) + *pc;
 c += *pb;

 /*------------------------------------------------- handle most of the key */
 while (length > 3)
 {
 a += k[0];
 b += k[1];
 c += k[2];
 mix(a,b,c);
 length -= 3;
 k += 3;
 }

 /*------------------------------------------- handle the last 3 uint32_t's */
 switch(length) /* all the case statements fall through */
 {
 case 3 : c+=k[2];
 case 2 : b+=k[1];
 case 1 : a+=k[0];
 final(a,b,c);
 case 0: /* case 0: nothing left to add */
 break;
 }
 /*------------------------------------------------------ report the result */
 *pc=c; *pb=b;
}
