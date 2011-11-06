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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tctypes.h>
#include <pthread.h>
#include <tcprioq.h>

struct tcprioq {
    void **qt;
    int size;
    int count;
    tccompare_fn cmp;
    int locking;
    pthread_mutex_t lock;
};

static inline void
tcp_lock(tcprioq_t *pq){
    if(pq->locking){
	pthread_mutex_lock(&pq->lock);
    }
}

static inline void
tcp_unlock(tcprioq_t *pq){
    if(pq->locking){
	pthread_mutex_unlock(&pq->lock);
    }
}

extern tcprioq_t *
tcprioq_new(int size, int lock, tccompare_fn cmp)
{
    tcprioq_t *pq = malloc(sizeof(*pq));

    pq->qt = malloc(size * sizeof(*pq->qt));
    pq->size = size;
    pq->count = 1;
    pq->cmp = cmp;
    pq->locking = lock;
    if(lock)
	pthread_mutex_init(&pq->lock, NULL);

    return pq;
}

extern int
tcprioq_add(tcprioq_t *pq, void *data)
{
    int i;

    tcp_lock(pq);

    if(pq->count == pq->size){
	pq->qt = realloc(pq->qt, (pq->size *= 2) * sizeof(*pq->qt));
    }

    i = pq->count;
    pq->qt[i] = data;

    while(i > 1 && pq->cmp(pq->qt[i/2], pq->qt[i]) > 0){
	void *tmp = pq->qt[i];
	pq->qt[i] = pq->qt[i/2];
	pq->qt[i/2] = tmp;
	i /= 2;
    }

    pq->count++;

    tcp_unlock(pq);
    return 0;
}

extern int
tcprioq_get(tcprioq_t *pq, void **ret)
{
    int rt = -1;

    tcp_lock(pq);

    if(pq->count > 1){
	int i;
	void *r;

	r = pq->qt[1];
	pq->qt[1] = pq->qt[pq->count-1];
	pq->count--;

	i = 1;
	while((2*i < pq->count && pq->cmp(pq->qt[i], pq->qt[2*i]) > 0) ||
	      (2*i < pq->count-1 && pq->cmp(pq->qt[i], pq->qt[2*i+1]) > 0)){
	    void *tmp = pq->qt[i];
	    i = (2*i < pq->count-1 && pq->cmp(pq->qt[2*i], pq->qt[2*i+1]) > 0)?
		 2*i+1: 2*i;
	    pq->qt[i/2] = pq->qt[i];
	    pq->qt[i] = tmp;
	}

	*ret = r;
	rt = 0;
    }

    tcp_unlock(pq);
    return rt;
}

extern int
tcprioq_items(tcprioq_t *pq)
{
    return pq->count - 1;
}

extern void
tprioq_free(tcprioq_t *pq)
{
    free(pq->qt);
    pthread_mutex_destroy(&pq->lock);
    free(pq);
}
