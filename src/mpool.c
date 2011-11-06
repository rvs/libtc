/**
    Copyright (C) 2003  Michael Ahlberg, Måns Rullgård

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
#include <unistd.h>
#include <tctypes.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stddef.h>
#include <tcalloc.h>
#include <tcmempool.h>
#include <tc.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

typedef struct tcmempool_page {
    tcmempool_t *pool;
    size_t used, inuse;
    void *free;
    struct tcmempool_page *next, *prev;
    union {
	long l;
	double d;
	void *p;
    } data[1];
} tcmempool_page_t;

struct tcmempool {
    size_t size;
    size_t cpp;
    tcmempool_page_t *pages;
    int locking;
    pthread_mutex_t lock;
};

static long pagesize;

#define align(s,a) (((s)+(a)-1) & ~((a)-1))

static inline void
mp_lock(tcmempool_t *mp)
{
    if(mp->locking)
	pthread_mutex_lock(&mp->lock);
}

static inline void
mp_unlock(tcmempool_t *mp)
{
    if(mp->locking)
	pthread_mutex_unlock(&mp->lock);
}

static void
mp_free(void *p)
{
    tcmempool_t *mp = p;
    pthread_mutex_destroy(&mp->lock);
}

extern tcmempool_t *
tcmempool_new(size_t size, int lock)
{
    tcmempool_t *mp;

    if(!pagesize)
	pagesize = sysconf(_SC_PAGESIZE);

    size = align(size, sizeof(void *));
    if(size > pagesize - offsetof(tcmempool_page_t, data))
	return NULL;

    mp = tcallocdz(sizeof(*mp), NULL, mp_free);
    mp->size = size;
    mp->cpp = (pagesize - offsetof(tcmempool_page_t, data)) / size;
    mp->locking = lock;
    pthread_mutex_init(&mp->lock, NULL);

    return mp;
}

extern void *
tcmempool_get(tcmempool_t *mp)
{
    tcmempool_page_t *mpp;
    void *chunk;

    mp_lock(mp);

    mpp = mp->pages;

    if(!mpp){
	mpp = mmap(NULL, pagesize, PROT_READ | PROT_WRITE,
		   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	mpp->pool = mp;
	mp->pages = mpp;
    }

    if(mpp->free){
	chunk = mpp->free;
	mpp->free = *(void **) chunk;
    } else {
	chunk = (char *) mpp->data + mpp->used * mp->size;
	mpp->used++;
    }

    if(++mpp->inuse == mp->cpp){
	mp->pages = mpp->next;
	mpp->next = NULL;
	mpp->prev = NULL;
    }

    mp_unlock(mp);

    return chunk;
}

extern void
tcmempool_free(void *p)
{
    tcmempool_page_t *mpp = (void *) ((ptrdiff_t) p & ~(pagesize - 1));
    tcmempool_t *mp = mpp->pool;

    mp_lock(mp);

    if(!--mpp->inuse){
	if(mp->pages == mpp)
	    mp->pages = mpp->next;
	if(mpp->next)
	    mpp->next->prev = mpp->prev;
	if(mpp->prev)
	    mpp->prev->next = mpp->next;
	munmap(mpp, pagesize);
    } else {
	*(void **) p = mpp->free;
	mpp->free = p;
	if(!mpp->next && !mpp->prev && mp->pages != mpp){
	    if(mp->pages)
		mp->pages->prev = mpp;
	    mpp->next = mp->pages;
	    mp->pages = mpp;
	}
    }

    mp_unlock(mp);
}
