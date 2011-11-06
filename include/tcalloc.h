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

#ifndef _TCALLOC_H
#define _TCALLOC_H

#include <tctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tc_ref_fn)(void *);
typedef void *(*tcattr_ref_t)(void *);

typedef struct tcattr {
    char *name;
    void *value;
} tcattr_t;

extern void *tcalloc(size_t);
extern void *tcallocd(size_t, tc_ref_fn, tcfree_fn);
extern void *tcallocz(size_t);
extern void *tcallocdz(size_t, tc_ref_fn, tcfree_fn);
extern void *tcref(void *);
extern void tcfree(void *);

extern int tcattr_set(void *p, char *name, void *val,
		      tcattr_ref_t r, tcfree_fn f);
extern void *tcattr_get(void *p, char *name);
extern int tcattr_getall(void *ptr, int n, tcattr_t *attr);
extern int tcattr_del(void *p, char *name);

#ifdef __cplusplus
}
#endif

#endif
