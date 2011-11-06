/**
    Copyright (C) 2001, 2002  Michael Ahlberg, Måns Rullgård

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

#ifndef _TCHASH_H
#define _TCHASH_H

#include <tctypes.h>
#include <tc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tchash_table tchash_table_t;
typedef u_int (*tchash_function_t)(void *key, size_t size);

/* Flags for hash table. */
#define TCHASH_FROZEN 0x01  /* Automatic resizing not allowed */
#define TCHASH_NOCOPY 0x02  /* Don't copy keys */

/* Create a new hash table with specified size and flags. 
 * Return pointer to new table or NULL on failure. */
extern tchash_table_t *tchash_new(int size, int lock, uint32_t flags);

/* Search hash table ht for key. If key is found, return corresponding
 * data in *ret, else add data to table and set *ret to data.
 * Return 0 if key was found, 1 otherwise. */
extern int tchash_search(tchash_table_t *ht, void *key, size_t ks,
			 void *data, void *ret);

/* Search hash table ht for key setting *ret to corresponding data.
 * Return 0 if found, 1 otherwise. */
extern int tchash_find(tchash_table_t *ht, void *key, size_t ks, void *ret);

/* Replace data for key 'key' with 'data'. If not found 'key'
 * is added to the table. Return 0 if 'key' was found, 1 if not. */
extern int tchash_replace(tchash_table_t *ht, void *key, size_t ks,
			  void *data, void *ret);

/* Delete entry 'key'.  Old data is returned in *ret.
 * Returs 0 if 'key' was found, non-zero otherwise. */
extern int tchash_delete(tchash_table_t *ht, void *key, size_t ks, void *ret);

/* Destroy hash table calling hf once for each element. */
extern int tchash_destroy(tchash_table_t *ht, tcfree_fn hf);

/* Resize hash table. */
extern int tchash_rehash(tchash_table_t *ht);

/* Get keys from table.  The number of entries is stored in *entries.
 * If fast == 0 copies of the keys are returned, otherwise the actual
 * keys are returned.  Be aware of the implications of this.  If the
 * table is empty, NULL is returned and 0 stored in *entries. */
extern void **tchash_keys(tchash_table_t *ht, int *entries, int fast);

extern int tchash_sethashfunction(tchash_table_t *ht, tchash_function_t hf);
extern int tchash_setthresholds(tchash_table_t *ht, float low, float high);

extern int tchash_getflags(tchash_table_t *ht);
extern int tchash_setflags(tchash_table_t *ht, int flags);
extern int tchash_setflag(tchash_table_t *ht, int flag);
extern int tchash_clearflag(tchash_table_t *ht, int flag);

#ifdef TCHASH_OLDAPI
#define tchash_new(s, f) tchash_new(s, TC_LOCK_STRICT, f)
#define tchash_search(h, k, d, r) tchash_search(h, k, -1, d, r)
#define tchash_find(h, k, r) tchash_find(h, k, -1, r)
#define tchash_replace(h, k, d) tchash_replace(h, k, -1, d, NULL)
#define tchash_delete(h, k, r) tchash_delete(h, k, -1, r)
#endif

#ifdef __cplusplus
}
#endif

#endif
