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

#ifndef _TCLIST_H
#define _TCLIST_H

#include <stdlib.h>
#include <tctypes.h>
#include <tc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tclist_item tclist_item_t;
typedef struct tclist tclist_t;


/* Create new empty tclist_t */
extern tclist_t *tclist_new(int locking);

/* Free resources used by list. */
extern int tclist_free(tclist_t *);

/* Destroy list calling lfree with data in each item. */
extern int tclist_destroy(tclist_t *lst, tcfree_fn lfree);

/* Remove item from tclist_t */
extern void tclist_remove(tclist_t *lst, tclist_item_t *l, tcfree_fn fr);

/* Add to end of tclist_t */
extern int tclist_push(tclist_t *lst, void *p);

/* Add to start of tclist_t */
extern int tclist_unshift(tclist_t *lst, void *p);

/* Remove and return first element in tclist_t */
extern void *tclist_shift(tclist_t *lst);

/* Remove and return last element in tclist_t */
extern void *tclist_pop(tclist_t *lst);

/* Find element in list equal to p as determined by comparison
 * function cmp. Return 0 if found. */
extern int tclist_find(tclist_t *lst, void *p, void *ret, tccompare_fn cmp);

/* Find element in list equal to p as determined by comparison
 * function cmp. Add to end of list if not found. */
extern int tclist_search(tclist_t *lst, void *p, void *ret, tccompare_fn cmp);

/* Remove element matching p from list. Return deleted element. */
extern int tclist_delete(tclist_t *lst, void *p, tccompare_fn cmp, tcfree_fn);

/* Remove all elements matching p from list. Return # of elements removed. */
extern int tclist_delete_matched(tclist_t *lst, void *p, tccompare_fn cmp,
			       tcfree_fn);

extern void *tclist_next(tclist_t *lst, tclist_item_t **l);
extern void *tclist_next_matched(tclist_t *lst, tclist_item_t **l,
			       void *key, tccompare_fn cmp);

extern void *tclist_prev(tclist_t *lst, tclist_item_t **l);
extern void *tclist_prev_matched(tclist_t *lst, tclist_item_t **l,
			       void *key, tccompare_fn cmp);

/* Unlock list item */
extern int tclist_unlock(tclist_t *lst, tclist_item_t *l);

/* Returns item count. */
extern unsigned long tclist_items(tclist_t *lst);

extern int tclist_isfirst(tclist_t *lst, tclist_item_t *li);

extern int tclist_islast(tclist_t *lst, tclist_item_t *li);

extern void *tclist_head(tclist_t *lst);
extern void *tclist_tail(tclist_t *lst);

#ifdef __cplusplus
}
#endif

#endif
