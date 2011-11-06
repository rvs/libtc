/**
    Copyright (C) 2002, 2003  Michael Ahlberg, Måns Rullgård

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

#include <tcalloc.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef struct tcattri {
    char *name;
    void *value;
    tcattr_ref_t ref;
    tcfree_fn free;
    struct tcattri *next;
} tcattri_t;

typedef struct tcalloc {
    long rc;
    tc_ref_fn ref;
    tcfree_fn free;
    tcattri_t *attr;
    union {
	long l;
	double d;
	void *p;
    } data[1];
} tcalloc_t;

static void tcattr_free(tcattri_t *a);

extern void *
tcallocd(size_t size, tc_ref_fn r, tcfree_fn f)
{
    tcalloc_t *tca = malloc(size + sizeof(tcalloc_t) - sizeof(tca->data));
    tca->rc = 1;
    tca->ref = r;
    tca->free = f;
    tca->attr = NULL;
    return tca->data;
}

extern void *
tcallocdz(size_t size, tc_ref_fn r, tcfree_fn f)
{
    void *p = tcallocd(size, r, f);
    if(p)
	memset(p, 0, size);
    return p;
}

extern void *
tcalloc(size_t size)
{
    return tcallocd(size, NULL, NULL);
}

extern void *
tcallocz(size_t size)
{
    return tcallocdz(size, NULL, NULL);
}

extern void *
tcref(void *ptr)
{
    tcalloc_t *tca = (tcalloc_t *)((char *) ptr - offsetof(tcalloc_t, data));
    tca->rc++;
    if(tca->ref)
	tca->ref(tca->data);
    return ptr;
}

extern void
tcfree(void *ptr)
{
    tcalloc_t *tca;

    if(!ptr)
	return;

    tca = (tcalloc_t *)((char *) ptr - offsetof(tcalloc_t, data));
    tca->rc--;
    if(!tca->rc){
	tcattri_t *a;

	if(tca->free)
	    tca->free(tca->data);
	for(a = tca->attr; a;){
	    tcattri_t *n = a->next;
	    tcattr_free(a);
	    a = n;
	}
	free(tca);
    }
}

static void
tcattr_free(tcattri_t *a)
{
    free(a->name);
    if(a->free)
	a->free(a->value);
    free(a);
}

extern int
tcattr_set(void *ptr, char *name, void *val, tcattr_ref_t r, tcfree_fn f)
{
    tcalloc_t *tca = (tcalloc_t *)((char *) ptr - offsetof(tcalloc_t, data));
    tcattri_t *a, *n, *p = NULL;

    for(a = tca->attr; a && a->next && strcmp(name, a->name); a = a->next)
	p = a;

    n = calloc(1, sizeof(*n));
    n->name = strdup(name);
    n->value = val;
    n->ref = r;
    n->free = f;

    if(!a){
	tca->attr = n;
    } else if(strcmp(name, a->name)){
	a->next = n;
    } else {
	n->next = a->next;
	if(p)
	    p->next = n;
	else
	    tca->attr = n;
	tcattr_free(a);
    }

    return 0;
}

extern void *
tcattr_get(void *p, char *name)
{
    tcalloc_t *tca = (tcalloc_t *)((char *) p - offsetof(tcalloc_t, data));
    tcattri_t *a;
    void *v = NULL;

    for(a = tca->attr; a && strcmp(name, a->name); a = a->next);

    if(a)
	v = a->ref? a->ref(a->value): a->value;

    return v;
}

extern int
tcattr_getall(void *p, int n, tcattr_t *attr)
{
    tcalloc_t *tca = (tcalloc_t *)((char *) p - offsetof(tcalloc_t, data));
    tcattri_t *a;
    int i;

    for(i = 0, a = tca->attr; a && i < n; a = a->next, i++){
	attr[i].name = a->name;
	attr[i].value = a->ref? a->ref(a->value): a->value;
    }

    return i;
}

extern int
tcattr_del(void *ptr, char *name)
{
    tcalloc_t *tca = (tcalloc_t *)((char *) ptr - offsetof(tcalloc_t, data));
    tcattri_t *a, *p = NULL;

    for(a = tca->attr; a && strcmp(name, a->name); a = a->next)
	p = a;

    if(a){
	if(p)
	    p->next = a->next;
	else
	    tca->attr = a->next;
	tcattr_free(a);
    }

    return 0;
}
