/**
    Copyright (C) 2001, 2002, 2003  Michael Ahlberg, Måns Rullgård

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
**/

#include <stdlib.h>
#include <string.h>
#include <tctypes.h>
#include <pthread.h>
#include <tchash.h>
#include <tcmempool.h>
#include <tcalloc.h>
#include <tc.h>

/* Structure for each entry in table. */
typedef struct hash_entry {
    void *key;
    size_t key_size;
    void *data;
    struct hash_entry *next;
} hash_entry;

struct tchash_table {
    tchash_function_t hash_func;
    size_t size;           /* Number of buckets. */
    size_t entries;        /* Number of entries in table. */
    hash_entry **buckets;
    uint32_t flags;
    int locking;
    pthread_mutex_t lock;
    float high_mark, low_mark;
    tcmempool_t *mp;
};

static u_int
hash_size(u_int s) 
{ 
    u_int i = 1;
    while(i < s)
	i <<= 1;
    return i;
} 

/* Default hash function. This is Bob Jenkins' well-known hash
   function.  See comments below for details. */
/*
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bits set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a 
  structure that could supported 2x parallelism, like so:
      a -= b; 
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage 
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c)				\
{						\
  a -= b; a -= c; a ^= (c>>13);			\
  b -= c; b -= a; b ^= (a<<8);			\
  c -= a; c -= b; c ^= (b>>13);			\
  a -= b; a -= c; a ^= (c>>12);			\
  b -= c; b -= a; b ^= (a<<16);			\
  c -= a; c -= b; c ^= (b>>5);			\
  a -= b; a -= c; a ^= (c>>3);			\
  b -= c; b -= a; b ^= (a<<10);			\
  c -= a; c -= b; c ^= (b>>15);			\
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  len     : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 6*len+35 instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

typedef uint32_t ub4;

static u_int
hash_func(void *key, size_t length)
{
    register ub4 a,b,c,len;
    char *k = key;

    /* Set up the internal state */
    len = length;
    a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
    c = 0;               /* the previous hash value */

    /*---------------------------------------- handle most of the key */
    while(len >= 12){
	a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
	b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
	c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
	mix(a,b,c);
	k += 12; len -= 12;
    }

    /*------------------------------------- handle the last 11 bytes */
    c += length;
    switch(len){              /* all the case statements fall through */
    case 11: c+=((ub4)k[10]<<24);
    case 10: c+=((ub4)k[9]<<16);
    case 9 : c+=((ub4)k[8]<<8);
	/* the first byte of c is reserved for the length */
    case 8 : b+=((ub4)k[7]<<24);
    case 7 : b+=((ub4)k[6]<<16);
    case 6 : b+=((ub4)k[5]<<8);
    case 5 : b+=k[4];
    case 4 : a+=((ub4)k[3]<<24);
    case 3 : a+=((ub4)k[2]<<16);
    case 2 : a+=((ub4)k[1]<<8);
    case 1 : a+=k[0];
	/* case 0: nothing left to add */
    }
    mix(a,b,c);
    /*-------------------------------------------- report the result */
    return c;
}

/* End of code from Jenkins */

static inline u_int
hash_value(tchash_table_t *ht, void *key, size_t ks, size_t size)
{
    u_int hv = ht->hash_func(key, ks);
    return hv & (size - 1);
}

/* Function to create a new hash table. */
extern tchash_table_t *
tchash_new(int size, int lock, uint32_t flags)
{
    tchash_table_t *ht;

    size = hash_size(size);
    ht = malloc(sizeof(*ht));
    ht->size = size;
    ht->entries = 0;
    ht->flags = flags;
    ht->buckets = calloc(size, sizeof(*ht->buckets));
    ht->locking = lock;
    pthread_mutex_init(&ht->lock, NULL);
    ht->high_mark = 0.7;
    ht->low_mark = 0.3;
    ht->hash_func = hash_func;
    ht->mp = tcmempool_new(sizeof(hash_entry), 0);

    return ht;
}

extern int
tchash_sethashfunction(tchash_table_t *ht, tchash_function_t hf)
{
    if(ht->entries)
	return -1;
    ht->hash_func = hf? hf: hash_func;
    return 0;
}

static inline void
lock_hash(tchash_table_t *ht)
{
    if(ht->locking)
	pthread_mutex_lock(&ht->lock);
}

static inline void
unlock_hash(tchash_table_t *ht)
{
    if(ht->locking)
	pthread_mutex_unlock(&ht->lock);
}

static inline int
hash_cmp(void *key, size_t ks, hash_entry *he)
{
    if(ks != he->key_size)
	return 1;
    if(key == he->key)
	return 0;
    return memcmp(key, he->key, ks);
}

static inline void *
hash_kdup(void *key, size_t ks)
{
    void *nk = malloc(ks);
    memcpy(nk, key, ks);
    return nk;
}

static inline hash_entry *
hash_addentry(tchash_table_t *ht, u_int hv, void *key, size_t ks, void *data)
{
    hash_entry *he = tcmempool_get(ht->mp);
    if(ht->flags & TCHASH_NOCOPY)
	he->key = key;
    else
	he->key = hash_kdup(key, ks);
    he->key_size = ks;
    he->data = data;
    he->next = ht->buckets[hv];
    ht->buckets[hv] = he;
    ht->entries++;

    return he;
}

/* Find or add to table. */
extern int
tchash_search(tchash_table_t *ht, void *key, size_t ks, void *data, void *r)
{
    u_int hv;
    hash_entry *hr;
    void **ret = r;
    int hf = 0;

    if(ks == (size_t) -1)
	ks = strlen(key) + 1;

    lock_hash(ht);

    /* Compute the hash value. */
    hv = hash_value(ht, key, ks, ht->size);

    for(hr = ht->buckets[hv]; hr; hr = hr->next)
	if(!hash_cmp(key, ks, hr))
	    break;

    if(!hr){
	hr = hash_addentry(ht, hv, key, ks, data);
	hf = 1;
    }

    if(ret != NULL)
	*ret = hr->data;

    unlock_hash(ht);
    if(hf && !(ht->flags & TCHASH_FROZEN)){
	if((float) ht->entries / ht->size > ht->high_mark)
	    tchash_rehash(ht);
    }
    return hf;
}


/* Find in table. */
extern int
tchash_find(tchash_table_t *ht, void *key, size_t ks, void *r)
{
    u_int hv;
    hash_entry *hr = NULL;
    void **ret = r;

    if(ks == (size_t) -1)
	ks = strlen(key) + 1;

    lock_hash(ht);
    hv = hash_value(ht, key, ks, ht->size);

    for(hr = ht->buckets[hv]; hr; hr = hr->next)
	if(!hash_cmp(key, ks, hr))
	    break;

    if(hr && ret)
	*ret = hr->data;

    unlock_hash(ht);
    return !hr;
}


extern int
tchash_replace(tchash_table_t *ht, void *key, size_t ks, void *data, void *r)
{
    u_int hv;
    hash_entry *hr;
    int ret = 0;
    void **rt = r;

    if(ks == (size_t) -1)
	ks = strlen(key) + 1;

    lock_hash(ht);
    hv = hash_value(ht, key, ks, ht->size);

    for(hr = ht->buckets[hv]; hr; hr = hr->next)
	if(!hash_cmp(key, ks, hr))
	    break;

    if(hr){
	if(rt)
	    *rt = hr->data;
	hr->data = data;
    } else {
	hash_addentry(ht, hv, key, ks, data);
	ret = 1;
    }

    unlock_hash(ht);
    if(ret && !(ht->flags & TCHASH_FROZEN)){
	if((float) ht->entries / ht->size > ht->high_mark)
	    tchash_rehash(ht);
    }
    return ret;
}


extern int
tchash_delete(tchash_table_t *ht, void *key, size_t ks, void *r)
{
    int hv;
    hash_entry *hr = NULL, *hp = NULL;
    void **ret = r;

    if(ks == (size_t) -1)
	ks = strlen(key) + 1;

    lock_hash(ht);
    hv = hash_value(ht, key, ks, ht->size);

    for(hr = ht->buckets[hv]; hr; hr = hr->next){
	if(!hash_cmp(key, ks, hr))
	    break;
	hp = hr;
    }

    if(hr){
	if(ret)
	    *ret = hr->data;
	if(hp)
	    hp->next = hr->next;
	else
	    ht->buckets[hv] = hr->next;
	ht->entries--;
	if(!(ht->flags & TCHASH_NOCOPY))
	    free(hr->key);
	tcmempool_free(hr);
    }

    unlock_hash(ht);
    if(!hr && !(ht->flags & TCHASH_FROZEN)){
	if((float) ht->entries / ht->size < ht->low_mark)
	    tchash_rehash(ht);
    }
    return !hr;
}

extern int
tchash_destroy(tchash_table_t *ht, tcfree_fn hf)
{
    size_t i;
    for(i = 0; i < ht->size; i++){
	if(ht->buckets[i] != NULL){
	    hash_entry *he = ht->buckets[i];
	    while(he){
		hash_entry *hn = he->next;
		if(hf)
		    hf(he->data);
		if(!(ht->flags & TCHASH_NOCOPY))
		    free(he->key);
		tcmempool_free(he);
		he = hn;
	    }
	}
    }

    free(ht->buckets);
    pthread_mutex_destroy(&ht->lock);
    tcfree(ht->mp);
    free(ht);

    return 0;
}

extern int
tchash_rehash(tchash_table_t *ht)
{
    hash_entry **nb = NULL;
    size_t ns, i;

    lock_hash(ht);

    ns = hash_size(ht->entries * 2 / (ht->high_mark + ht->low_mark));
    if(ns == ht->size)
	goto end;
    nb = calloc(ns, sizeof(*nb));

    for(i = 0; i < ht->size; i++){
	hash_entry *he = ht->buckets[i];
	while(he){
	    hash_entry *hn = he->next;
	    int hv = hash_value(ht, he->key, he->key_size, ns);
	    he->next = nb[hv];
	    nb[hv] = he;
	    he = hn;
	}
    }

    ht->size = ns;
    free(ht->buckets);
    ht->buckets = nb;

end:
    unlock_hash(ht);
    return 0;
}

extern void **
tchash_keys(tchash_table_t *ht, int *n, int fast)
{
    void **keys;
    size_t i, j;

    if(ht->entries == 0){
	*n = 0;
	return NULL;
    }

    lock_hash(ht);

    keys = malloc(ht->entries * sizeof(*keys));

    for(i = 0, j = 0; i < ht->size; i++){
	hash_entry *he;
	for(he = ht->buckets[i]; he; he = he->next)
	    keys[j++] = fast? he->key: hash_kdup(he->key, he->key_size);
    }

    *n = ht->entries;

    unlock_hash(ht);

    return keys;
}

extern int
tchash_setthresholds(tchash_table_t *ht, float low, float high)
{
    float h = high < 0? ht->high_mark: high;
    float l = low < 0? ht->low_mark: low;

    if(h < l)
	return -1;

    ht->high_mark = h;
    ht->low_mark = l;

    return 0;
}

extern int
tchash_getflags(tchash_table_t *ht)
{
    return ht->flags;
}

extern int
tchash_setflags(tchash_table_t *ht, int flags)
{
    lock_hash(ht);
    ht->flags = flags;
    unlock_hash(ht);
    return ht->flags;
}

extern int
tchash_setflag(tchash_table_t *ht, int flag)
{
    lock_hash(ht);
    ht->flags |= flag;
    unlock_hash(ht);
    return ht->flags;
}

extern int
tchash_clearflag(tchash_table_t *ht, int flag)
{
    lock_hash(ht);
    ht->flags &= ~flag;
    unlock_hash(ht);
    return ht->flags;
}
