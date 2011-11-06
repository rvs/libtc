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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "tclist.h"

struct tclist_item {
    void *data;
    tclist_item_t *next;
    tclist_item_t *prev;
    int rc, ic;
    tcfree_fn free;
    int deleted;
};

struct tclist {
    tclist_item_t *start;
    tclist_item_t *end;
    unsigned long items, deleted;
    int locking;
    pthread_mutex_t lock;
};

extern tclist_t *
tclist_new(int locking)
{
    tclist_t *l = calloc(1, sizeof(*l));
    l->locking = locking;
    if(locking > TC_LOCK_NONE)
	pthread_mutex_init(&l->lock, NULL);
    return l;
}

extern int
tclist_free(tclist_t *lst)
{
    if(lst->start != NULL)
	return -1;

    if(lst->locking > TC_LOCK_NONE)
	pthread_mutex_destroy(&lst->lock);

    free(lst);
    return 0;
}

static void
list_unlink(tclist_t *lst, tclist_item_t *l)
{
    if(l->prev)
	l->prev->next = l->next;
    if(l->next)
	l->next->prev = l->prev;
    if(l == lst->start)
	lst->start = l->next;
    if(l == lst->end)
	lst->end = l->prev;

    if(l->free)
	l->free(l->data);

    lst->items--;
    if(l->deleted)
	lst->deleted--;
    free(l);
}

static inline void
list_ref(tclist_item_t *l)
{
    l->rc++;
}

static inline void
list_deref(tclist_t *lst, tclist_item_t *l)
{
    if(--l->rc <= 0 && l->ic <= 0)
	list_unlink(lst, l);
}

static inline int
lock_list(tclist_t *lst)
{
    if(lst->locking > TC_LOCK_NONE)
	pthread_mutex_lock(&lst->lock);
    return 0;
}


static inline int
unlock_list(tclist_t *lst)
{
    if(lst->locking > TC_LOCK_NONE)
	pthread_mutex_unlock(&lst->lock);
    return 0;
}

extern int
tclist_unlock(tclist_t *lst, tclist_item_t *l)
{
    lock_list(lst);
    l->ic--;
    list_deref(lst, l);
    unlock_list(lst);
    return 0;
}

extern void
tclist_remove(tclist_t *lst, tclist_item_t *l, tcfree_fn fr)
{
    lock_list(lst);
    l->deleted = 1;
    l->free = fr;
    lst->deleted++;
    list_deref(lst, l);
    unlock_list(lst);
}

extern int
tclist_destroy(tclist_t *lst, tcfree_fn lfree)
{
    lock_list(lst);
    while(lst->start){
	lst->start->free = lfree;
	list_unlink(lst, lst->start);
    }
    unlock_list(lst);
    tclist_free(lst);
    return 0;
}

static tclist_item_t *
new_item(void *p)
{
    tclist_item_t *l = malloc(sizeof(tclist_item_t));
    l->data = p;
    l->rc = 1;
    l->ic = 0;
    l->free = NULL;
    l->deleted = 0;
    return l;
}

extern int
tclist_push(tclist_t *lst, void *p)
{
    tclist_item_t *l = new_item(p);

    lock_list(lst);
    if(lst->start == NULL){
	lst->start = l;
	lst->start->next = NULL;
	lst->start->prev = NULL;
	lst->end = lst->start;
    } else {
	lst->end->next = l;
	l->prev = lst->end;
	l->next = NULL;
	lst->end = l;
    }
    lst->items++;
    unlock_list(lst);
    return 0;
}

extern int
tclist_unshift(tclist_t *lst, void *p)
{
    tclist_item_t *l = new_item(p);

    lock_list(lst);
    if(lst->start == NULL){
	lst->start = l;
	lst->start->next = NULL;
	lst->start->prev = NULL;
	lst->end = lst->start;
    } else {
	lst->start->prev = l;
	l->next = lst->start;
	l->prev = NULL;
	lst->start = l;
    }
    lst->items++;
    unlock_list(lst);
    return 0;
}

extern void *
tclist_shift(tclist_t *lst)
{
    tclist_item_t *li = NULL;
    void *data = NULL;

    tclist_next(lst, &li);
    if(li){
	data = li->data;
	tclist_remove(lst, li, NULL);
	tclist_unlock(lst, li);
    }

    return data;
}

extern void *
tclist_pop(tclist_t *lst)
{
    tclist_item_t *li = NULL;
    void *data = NULL;

    tclist_prev(lst, &li);
    if(li){
	data = li->data;
	tclist_remove(lst, li, NULL);
	tclist_unlock(lst, li);
    }

    return data;
}

static tclist_item_t *
list_find_item(tclist_t *lst, void *p, tccompare_fn cmp)
{
    tclist_item_t *li = NULL;

    for(tclist_next(lst, &li); li != NULL; tclist_next(lst, &li)){
	if(cmp? cmp(p, li->data) == 0: p == li->data){
	    break;
	}
    }
    return li;
}

extern int
tclist_find(tclist_t *lst, void *p, void *ret, tccompare_fn cmp)
{
    tclist_item_t *l;
    void **r = ret;

    if((l = list_find_item(lst, p, cmp)) != NULL){
	if(r != NULL)
	    *r = l->data;
	tclist_unlock(lst, l);
	return 0;
    }

    return 1;
}

extern int
tclist_search(tclist_t *lst, void *p, void *ret, tccompare_fn cmp)
{
    void **r = ret;
    tclist_item_t *l;

    l = list_find_item(lst, p, cmp);
    if(l != NULL){
	if(r != NULL)
	    *r = l->data;
	tclist_unlock(lst, l);
	return 0;
    } else {
	tclist_push(lst, p);
	if(r != NULL)
	    *r = p;
	return 1;
    }
    return -1;
}

extern int
tclist_delete(tclist_t *lst, void *p, tccompare_fn cmp, tcfree_fn fr)
{
    tclist_item_t *li = NULL;

    if((li = list_find_item(lst, p, cmp)) != NULL){
	tclist_remove(lst, li, fr);
	tclist_unlock(lst, li);
	return 0;
    }

    return 1;
}

extern int
tclist_delete_matched(tclist_t *lst, void *p, tccompare_fn cmp, tcfree_fn fr)
{
    int n = 0;
    tclist_item_t *li = NULL;
    
    for(tclist_next(lst, &li); li != NULL; tclist_next(lst, &li)){
	if(cmp(p, li->data) == 0){
	    tclist_remove(lst, li, fr);
	    n++;
	}
    }
    return n;
}

extern void *
tclist_next(tclist_t *lst, tclist_item_t **l)
{
    void *r = NULL;

    if(lst->locking < TC_LOCK_STRICT || *l == NULL)
	lock_list(lst);

    do {
	if(*l == NULL){
	    if(lst->start != NULL)
		list_ref(lst->start);
	    *l = lst->start;
	} else {
	    tclist_item_t *ln;
	    if((*l)->next != NULL){
		list_ref((*l)->next);
	    }
	    ln = (*l)->next;
	    (*l)->ic--;
	    list_deref(lst, *l);
	    *l = ln;
	}

	if(*l != NULL){
	    r = (*l)->data;
	    (*l)->ic++;
	}
    } while(*l && (*l)->deleted);

    if(lst->locking < TC_LOCK_STRICT || *l == NULL)
	unlock_list(lst);
    return *l? r: NULL;
}

extern void *
tclist_next_matched(tclist_t *lst, tclist_item_t **l, void *key,
		    tccompare_fn cmp)
{
    void *r;
    do {
	r = tclist_next(lst, l);
    } while(*l && cmp(key, r));
    return r;
}

extern void *
tclist_prev(tclist_t *lst, tclist_item_t **l)
{
    void *r = NULL;

    if(lst->locking < TC_LOCK_STRICT || *l == NULL)
	lock_list(lst);

    do {
	if(*l == NULL){
	    if(lst->end != NULL)
		list_ref(lst->end);
	    *l = lst->end;
	} else {
	    tclist_item_t *ln;
	    if((*l)->prev != NULL){
		list_ref((*l)->prev);
	    }
	    ln = (*l)->prev;
	    (*l)->ic--;
	    list_deref(lst, *l);
	    *l = ln;
	}

	if(*l != NULL){
	    r = (*l)->data;
	    (*l)->ic++;
	}
    } while(*l && (*l)->deleted);

    if(lst->locking < TC_LOCK_STRICT || *l == NULL)
	unlock_list(lst);
    return *l? r: NULL;
}

extern void *
tclist_prev_matched(tclist_t *lst, tclist_item_t **l, void *key,
		    tccompare_fn cmp)
{
    void *r;
    do {
	r = tclist_prev(lst, l);
    } while(*l && cmp(key, r));
    return r;
}

extern unsigned long
tclist_items(tclist_t *lst)
{
    return lst->items - lst->deleted;
}

extern int
tclist_isfirst(tclist_t *lst, tclist_item_t *li)
{
    return li == lst->start;
}


extern int
tclist_islast(tclist_t *lst, tclist_item_t *li)
{
    return li == lst->end;
}

extern void *
tclist_head(tclist_t *lst)
{
    void *h;
    lock_list(lst);
    h = lst->start->data;
    unlock_list(lst);
    return h;
}

extern void *
tclist_tail(tclist_t *lst)
{
    void *t;
    lock_list(lst);
    t = lst->end->data;
    unlock_list(lst);
    return t;
}
