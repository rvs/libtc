/**
    Copyright (C) 2001  Michael Ahlberg, Måns Rullgård

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

#ifndef _TCC_INTERNAL_H
#define _TCC_INTERNAL_H

#include <tctypes.h>
#include <tclist.h>

#define TCC_INTEGER  1
#define TCC_FLOAT    2
#define TCC_BOOLEAN  3
#define TCC_STRING   4
#define TCC_REF      5
#define TCC_IGNORE   (1<<28)
#define TCC_EXPAND   (1<<29)
#define TCC_LONG     (1<<30)
#define TCC_UNSIGNED (1U<<31)
#define TCC_TYPEMASK (TCC_IGNORE-1)

typedef struct {
    int type;
    union {
	uint64_t integer;
	double floating;
	int boolean;
	char *string;
    } value;
} tcc_value;

typedef struct _conf_section conf_section;
struct _conf_section {
    char *name;
    tclist_t *entries;
    tclist_t *merge;
    conf_section *parent;
};

struct tcconf_section {
    conf_section *sec;
    tcconf_section_t *parent;
};

#define TCC_VALUE    1
#define TCC_SECTION  2
#define TCC_MSECTION 3

typedef struct {
    int type;
    struct {
	char *key;
	tclist_t *values;
    } value;
    conf_section *section;
} tcc_entry;


extern conf_section *conf_new(char *name);
extern int tcc_addint(tcc_entry *, long long);
extern int tcc_addfloat(tcc_entry *, double);
extern int tcc_addstring(tcc_entry *, char *, int);
extern int tcc_addbool(tcc_entry *, int);
extern int tcc_addref(tcc_entry *te, char *ref);
extern conf_section *tcc_lex(void *, tcio_fn, conf_section *);
extern tcc_entry *create_entry(conf_section *sec, char *name, int type);

#endif
