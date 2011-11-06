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
#include <tctypes.h>
#include <tcstring.h>
#include <tclist.h>
#include <stdarg.h>
#include <pthread.h>
#include <tcalloc.h>
#include <ctype.h>
#include <fnmatch.h>
#include <tcconf.h>
#include "tcc-internal.h"

static int tcc_writeentry(tcc_entry *, void *file, int lv, tcio_fn);
static int tcc_writesection(conf_section *ts, void *file, int lv,
			    char *d, tcio_fn);
static int tcc_writelist(tclist_t *l, void *file, int lv, tcio_fn);

pthread_mutex_t flex_mut = PTHREAD_MUTEX_INITIALIZER;

#define min(a, b) ((a)<(b)?(a):(b))

static void
dumppath(tcconf_section_t *ts)
{
    if(ts->parent)
	dumppath(ts->parent);
    fprintf(stderr, "%s/", ts->sec->name);
}

extern void
tcconf_dumppath(tcconf_section_t *ts)
{
    if(ts){
	dumppath(ts);
	fprintf(stderr, "\n");
    } else {
	fprintf(stderr, "path = NULL\n");
    }
}

static void
tcconf_free(void *p)
{
    tcconf_section_t *s = p;
    tcfree(s->sec);
    if(s->parent)
	tcfree(s->parent);
}

/* Load a configuration file */
extern tcconf_section_t *
tcconf_load(tcconf_section_t *ts, void *data, tcio_fn rfn)
{
    conf_section *sec = ts? ts->sec: NULL;
    pthread_mutex_lock(&flex_mut);
    sec = tcc_lex(data, rfn, sec);
    pthread_mutex_unlock(&flex_mut);

    if(!ts && sec)
	ts = tcallocdz(sizeof(*ts), NULL, tcconf_free);
    if(ts && sec)
	ts->sec = sec;

    return sec? ts: NULL;
}

extern tcconf_section_t *
tcconf_load_file(tcconf_section_t *sec, char *file)
{
    FILE *f;
    tcconf_section_t *s;
    if((f = fopen(file, "r")) == NULL)
	return sec;
    s = tcconf_load(sec, f, (tcio_fn) fread);
    fclose(f);
    return s;
}

struct string_read {
    const char *d;
    int s;
    int p;
};

static size_t
read_string(void *p, size_t s, size_t c, void *d)
{
    struct string_read *sr = d;
    int b = s * c;

    b = min(b, sr->s - sr->p);
    memcpy(p, sr->d + sr->p, b);
    sr->p += b;

    return b / s;
}

extern tcconf_section_t *
tcconf_load_string(tcconf_section_t *sec, const char *conf, int size)
{
    struct string_read sr;

    if(size < 0)
	size = strlen(conf);
    sr.d = conf;
    sr.s = size;
    sr.p = 0;

    return tcconf_load(sec, &sr, read_string);
}

/* Write a configuration file */
extern int
tcconf_write(tcconf_section_t *tcc, void *file, tcio_fn ofn)
{
    tcc_writelist(tcc->sec->entries, file, 0, ofn);
    return 0;
}

static int
cmp_str_sec(const void *p1, const void *p2)
{
    const char *n = p1;
    const tcc_entry *te = p2;
    switch(te->type){
    case TCC_SECTION:
    case TCC_MSECTION:
    	return strcmp(n, te->section->name);
    }
    return -17;
}

static conf_section *
getsection(tcconf_section_t **ts, conf_section *sec, char *name)
{
    tcconf_section_t *path = NULL;
    char *tmp = strdup(name);
    char *tn = tmp;
    char *s;

    if(ts)
	path = *ts;

/*     fprintf(stderr, "enter getsection\n"); */
/*     fprintf(stderr, "finding '%s' in ", name); */
/*     if(path) */
/* 	tcconf_dumppath(path); */
/*     else */
/* 	fprintf(stderr, "%s\n", sec->name); */

    while((s = strsep(&tmp, "/")) != NULL){
	if(*s == 0)
	    continue;

	if(!strcmp(s, "..")){
	    if(path && path->parent){
		tcconf_section_t *p = path;
		path = tcref(path->parent);
		sec = path->sec;
		tcfree(p);
	    } else if(sec->parent){
		sec = sec->parent;
	    }
	} else {
	    tcconf_section_t *np;
	    tcc_entry *te;

	    if(tclist_find(sec->entries, s, &te, cmp_str_sec)){
		tclist_t *mlist = sec->merge;
		tclist_item_t *li = NULL;
		char *m;

		sec = NULL;

		while((m = tclist_prev(mlist, &li))){
		    conf_section *ps, *ms;
		    if(path)
			ps = path->parent? path->parent->sec: path->sec;
		    else
			ps = sec->parent? sec->parent: sec;
		    ms = getsection(NULL, ps, m);
		    if(ms && (sec = getsection(NULL, ms, s)))
			break;
		}
		if(li)
		    tclist_unlock(sec->merge, li);
	    } else {
		sec = te->section;
	    }

	    if(!sec)
		break;

	    if(path){
		np = tcallocdz(sizeof(*np), NULL, tcconf_free);
		np->sec = tcref(sec);
		np->parent = path;
		path = np;
	    }
	}
/* 	tcconf_dumppath(path); */
    }

    if(ts)
	*ts = path;
/*     fprintf(stderr, "leave getsection\n"); */
    free(tn);
    return sec;
}

extern tcconf_section_t *
tcconf_getsection(tcconf_section_t *ts, char *name)
{
    tcref(ts);
    if(!name || getsection(&ts, ts->sec, name))
	return ts;
    tcfree(ts);
    return NULL;
}

static int
cmp_str_val(const void *p1, const void *p2)
{
    const char *n = p1;
    const tcc_entry *te = p2;

    if(te->type == TCC_VALUE)
	return strcmp(n, te->value.key);
    return -17;
}

static int
cmp_glob_val(const void *p1, const void *p2)
{
    const char *n = p1;
    const tcc_entry *te = p2;

    if(te->type == TCC_VALUE)
	return fnmatch(n, te->value.key, 0);
    return -1;
}

static tcc_entry *
getvalue(conf_section *sec, char *name, tcconf_section_t **ts)
{
    tcconf_section_t *path = NULL;
    char *tmp = strdup(name);
    char *v;
    tcc_entry *te = NULL;

/*     fprintf(stderr, "enter getvalue\n"); */
/*     fprintf(stderr, "getting '%s' in ", name); */
/*     if(ts) */
/* 	tcconf_dumppath(*ts); */
/*     else */
/* 	fprintf(stderr, "%s\n", sec->name); */

    if((v = strrchr(tmp, '/')) != NULL){
	*v++ = 0;
	if(!(sec = getsection(ts, sec, tmp)))
	    goto end;
    } else {
	v = tmp;
    }

    if(tclist_find(sec->entries, v, &te, cmp_str_val)){
	tclist_item_t *li = NULL;
	char *m;
	while((m = tclist_prev(sec->merge, &li))){
	    conf_section *ps, *ms;
	    if(path)
		ps = path->parent? path->parent->sec: path->sec;
	    else
		ps = sec->parent? sec->parent: sec;
	    ms = getsection(NULL, ps, m);
	    if(ms && (te = getvalue(ms, v, NULL)))
		break;
	}
	if(li)
	    tclist_unlock(sec->merge, li);
    }

/*     fprintf(stderr, "leave getvalue\n"); */
end:
    free(tmp);
    return te;
}

static int cp_val(tcconf_section_t *sec, tcc_value *tv, int type, void *dst);

static char *
vtostr(char *name, void *_ts)
{
    tcconf_section_t *ts = _ts;
    tclist_item_t *li = NULL;
    tcc_value *tv;
    tcc_entry *te;
    int l = 0;
    char *s = NULL;
    char *p = s;
    int sp = 0;

#define ext(n) do {				\
    int o = p - s;				\
    s = realloc(s, l += n);			\
    p = s + o;					\
} while(0)

    tcref(ts);

    if(!(te = getvalue(ts->sec, name, &ts)))
	return NULL;

    while((tv = tclist_next(te->value.values, &li))){
	union {
	    uint64_t i;
	    double d;
	    char *s;
	} v;
	int sl;

	cp_val(ts, tv, tv->type | TCC_LONG, &v);

	if(sp)
	    *p++ = ' ';

	switch(tv->type & TCC_TYPEMASK){
	case TCC_INTEGER:
	    ext(22);
	    p += snprintf(p, 22, "%lli", v.i);
	    break;
	case TCC_FLOAT:
	    ext(40);
	    p += snprintf(p, 40, "%lf", v.d);
	    break;
	case TCC_REF:
	    v.s = vtostr(tv->value.string, ts);
	case TCC_STRING:
	    sl = strlen(v.s);
	    ext(sl + 1);
	    strcpy(p, v.s);
	    free(v.s);
	    p += sl;
	    break;
	}
	sp = 1;
    }

    *p = 0;

#undef ext
    tcfree(ts);
    return s;
}

static int
cp_val(tcconf_section_t *ts, tcc_value *tv, int type, void *dst)
{
    switch(tv->type & TCC_TYPEMASK){
    case TCC_STRING:
	if(tv->type & TCC_EXPAND)
	    *(char **) dst = tcstrexp(tv->value.string, "(", ")", ':',
				      vtostr, ts,
				      TCSTREXP_ESCAPE | TCSTREXP_FREE);
	else
	    *(char **) dst = strdup(tv->value.string);
	break;

    case TCC_INTEGER:
	switch(type & ~TCC_TYPEMASK){
	case 0:
	    *(int32_t *) dst = (int32_t) tv->value.integer;
	    break;
	case TCC_UNSIGNED:
	    *(uint32_t *) dst = (uint32_t) tv->value.integer;
	    break;
	case TCC_LONG:
	    *(int64_t *) dst = (int64_t) tv->value.integer;
	    break;
	case TCC_LONG|TCC_UNSIGNED:
	    *(uint64_t *) dst = (uint64_t) tv->value.integer;
	    break;
	}
	break;

    case TCC_FLOAT:
	switch(type & ~TCC_TYPEMASK){
	case 0:
	    *(float *) dst = (float) tv->value.floating;
	    break;
	case TCC_LONG:
	    *(double *) dst = (double) tv->value.floating;
	    break;
	default:
	    return -1;
	}
	break;

    case TCC_REF:
	if(type & TCC_IGNORE)
	    break;

	switch(type & ~TCC_UNSIGNED){
	case TCC_INTEGER:
	    *(uint32_t *) dst = 0;
	    break;
	case TCC_INTEGER|TCC_LONG:
	    *(int64_t *) dst = 0;
	    break;
	case TCC_FLOAT:
	    *(float *) dst = 0;
	    break;
	case TCC_FLOAT|TCC_LONG:
	    *(double *) dst = 0;
	    break;
	case TCC_STRING:
	    *(char **) dst = NULL;
	    break;
	}	    
	break;

    default:
	fprintf(stderr, "BUG: bad type %x\n", type);
	break;
    }
    return 0;
}

static int
getentry(tcconf_section_t *ts, tcc_entry *te, char *fmt,
	 va_list args, char **tail)
{
    char *f = fmt, *p;
    int n = 0;
    tclist_item_t *li = NULL;
    tcc_value *tv;
    va_list ac;

    while((p = strchr(f, '%')) != NULL){
	void *dest;
	int type = 0;

	if((tv = tclist_next(te->value.values, &li)) == NULL)
	    break;

	f = p;

	if(tv->type == TCC_REF && strcmp(tv->value.string, "NULL")){
	    tcconf_section_t *rs = tcref(ts);
	    tcc_entry *re;
	    int r;

	    if(!(re = getvalue(rs->sec, tv->value.string, &rs))){
		n = -n;
		break;
	    }
#ifdef __va_copy
	    __va_copy(ac, args);
#else
	    ac = args;
#endif
	    r = getentry(ts, re, f, ac, &f);
	    tcfree(rs);
	    if(r < 0){
		n += -r;
		break;
	    }
	    n += r;
	    while(r--)
		va_arg(args, void *);
	    continue;
	}

	dest = va_arg(args, void *);
	f++;

	while(!(type & TCC_TYPEMASK) && *f){
	    switch(*f){
	    case 's':
		type |= TCC_STRING;
		break;

	    case 'd':
	    case 'i':
		type |= TCC_INTEGER;
		break;

	    case 'f':
		type |= TCC_FLOAT;
		break;

	    case 'l':
		type |= TCC_LONG;
		break;

	    case 'u':
		type |= TCC_UNSIGNED;
		break;

	    case 'z':
		type |= TCC_IGNORE;
		break;
	    }
	    f++;
	}

	if(cp_val(ts, tv, type, dest) < 0){
	    fprintf(stderr, "Type mismatch in '%s'.\n", te->value.key);
	    n = -n;
	    break;
	} else {
	    n++;
	}
    }

    if(tail)
	*tail = f;

    if(li)
	tclist_unlock(te->value.values, li);

    return n;
}

extern int
tcconf_getvalue(tcconf_section_t *sec, char *name, char *fmt, ...)
{
    va_list args;
    tcc_entry *te;
    int n;

    tcref(sec);
    if(!(te = getvalue(sec->sec, name, &sec))){
	tcfree(sec);
	return -1;
    }

    va_start(args, fmt);
    n = getentry(sec, te, fmt, args, NULL);
    va_end(args);

    tcfree(sec);
    return n < 0? -n: n;
}

typedef struct {
    tclist_item_t *li, *ml;
    char *n;
    tcconf_section_t *sec, *ts;
    void *vsr;
} v_state;

static tcconf_section_t *
next_merge(tcconf_section_t *sec, v_state *vs)
{
    tcconf_section_t *s = NULL;
    char *m;

    while((m = tclist_next(sec->sec->merge, &vs->ml))){
	tcconf_section_t *ms = tcconf_getsection(sec->parent, m);
	if(ms){
	    s = ms;
	    break;
	}
    }

    return s;
}

static v_state *
vstate(tcconf_section_t *sec, char *name, void **state)
{
    v_state *vs = *state;

    if(!vs){
	char *tmp = strdup(name);
	char *v = strrchr(tmp, '/');
	tcconf_section_t *ms;

	if(v){
	    *v++ = 0;
	    if(!(sec = tcconf_getsection(sec, tmp))){
		free(tmp);
		return NULL;
	    }
	} else {
	    v = tmp;
	    tcref(sec);
	}

	vs = *state = calloc(1, sizeof(v_state));
	vs->n = strdup(v);
	vs->ts = sec;
	ms = next_merge(sec, vs);
	if(ms)
	    sec = ms;
	vs->sec = sec;
	free(tmp);
    }

    return vs;
}

static void
vsfree(v_state *vs)
{
    free(vs->n);
    tcfree(vs->ts);
    free(vs);
}

static int
tcconf_nextvalue_vc(tcconf_section_t *ts, char *name, void **state,
		    char **rname, char *fmt, va_list args, tccompare_fn cmp)
{
    v_state *vs;
    tcc_entry *te;
    int n = 0;

    if(!(vs = vstate(ts, name, state)))
	return -1;

    while(vs->ml){
	n = tcconf_nextvalue_vc(vs->sec, vs->n, &vs->vsr, rname,
				fmt, args, cmp);
	if(!vs->vsr){
	    tcconf_section_t *ms = next_merge(vs->ts, vs);
	    tcfree(vs->sec);
	    if(ms){
		vs->sec = ms;
	    } else {
		vs->sec = vs->ts;
	    }
	}
	if(n > 0){
	    return n;
	}
    }

    te = tclist_prev_matched(vs->sec->sec->entries, &vs->li, vs->n, cmp);
    if(te){
	n = getentry(vs->sec, te, fmt, args, NULL);
	if(rname)
	    *rname = te->value.key;
    } else {
	*state = NULL;
	vsfree(vs);
    }

    return n < 0? -n: n;
}

extern int
tcconf_nextvalue_v(tcconf_section_t *ts, char *name, void **state,
		   char *fmt, va_list args)
{
    return tcconf_nextvalue_vc(ts, name, state, NULL, fmt, args, cmp_str_val);
}

extern int
tcconf_nextvalue(tcconf_section_t *sec, char *name, void **state,
		 char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = tcconf_nextvalue_vc(sec, name, state, NULL, fmt, args, cmp_str_val);
    va_end(args);
    return n;
}

extern int
tcconf_nextvalue_g(tcconf_section_t *sec, char *glob, void **state,
		   char **name, char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = tcconf_nextvalue_vc(sec, glob, state, name, fmt, args, cmp_glob_val);
    va_end(args);
    return n;
}

extern tcconf_section_t *
tcconf_nextsection(tcconf_section_t *ts, char *name, void **state)
{
    tcconf_section_t *ns;
    v_state *vs;
    conf_section *sec = NULL;

    if(!(vs = vstate(ts, name, state)))
	return NULL;

    while(vs->ml){
	tcconf_section_t *s = tcconf_nextsection(vs->sec, vs->n, &vs->vsr);
	if(!vs->vsr){
	    tcconf_section_t *ms = next_merge(vs->ts, vs);
	    tcfree(vs->sec);
	    if(ms){
		vs->sec = ms;
	    } else {
		vs->sec = vs->ts;
	    }
	}
	if(s){
	    sec = s->sec;
	    tcfree(s);
	    break;
	}
    }

    if(!sec){
	tcc_entry *te = tclist_prev_matched(vs->sec->sec->entries, &vs->li,
					  vs->n, cmp_str_sec);
	if(!te){
	    *state = NULL;
	    vsfree(vs);
	    return NULL;
	}
	sec = te->section;
    }

    ns = tcallocd(sizeof(*ns), NULL, tcconf_free);
    ns->sec = tcref(sec);
    ns->parent = tcref(ts);
    return ns;
}

extern tcconf_section_t *
tcconf_merge(tcconf_section_t *sec, tcconf_section_t *s2)
{
    tclist_item_t *li = NULL;
    tcc_entry *te;
    char *m;

    if(!sec){
	sec = tcconf_new(NULL);
	if(s2->parent)
	    sec->parent = tcref(s2->parent);
/* 	if(s2->sec->parent) */
/* 	    sec->sec->parent = tcref(s2->sec->parent); */
	sec->sec->parent = s2->sec->parent;
    }

    while((te = tclist_next(s2->sec->entries, &li)))
	tclist_push(sec->sec->entries, tcref(te));

    while((m = tclist_next(s2->sec->merge, &li)))
	tclist_push(sec->sec->merge, strdup(m));

    return sec;
}

static void
free_value(void *p)
{
    tcc_value *tv = p;
    switch(tv->type & TCC_TYPEMASK){
    case TCC_STRING:
    case TCC_REF:
	free(tv->value.string);
    }
    free(tv);
}

static void
free_entry(void *p)
{
    tcc_entry *te = p;

    switch(te->type){
    case TCC_VALUE: 
	tclist_destroy(te->value.values, free_value);
	free(te->value.key);
	break;

    case TCC_SECTION:
    case TCC_MSECTION:
	tcfree(te->section);
	break;
    }
}

static void
conf_free(void *p)
{
    conf_section *sec = p;
    tclist_destroy(sec->entries, tcfree);
    tclist_destroy(sec->merge, free);
    if(sec->name)
	free(sec->name);
}

extern conf_section *
conf_new(char *name)
{
    conf_section *sec;
    sec = tcallocdz(sizeof(*sec), NULL, conf_free);
    sec->name = name? strdup(name): NULL;
    sec->entries = tclist_new(TC_LOCK_SLOPPY);
    sec->merge = tclist_new(TC_LOCK_SLOPPY);
    return sec;
}

extern tcconf_section_t *
tcconf_new(char *name)
{
    tcconf_section_t *ts = tcallocdz(sizeof(*ts), NULL, tcconf_free);
    ts->sec = conf_new(name);
    return ts;
}

static tcc_entry *
alloc_entry(conf_section *sec, char *name, int type)
{
    tcc_entry *te = NULL;
    te = tcallocd(sizeof(*te), NULL, free_entry);
    switch((te->type = type)){
    case TCC_SECTION:
    case TCC_MSECTION:
	te->section = conf_new(name);
	te->section->parent = sec;
	break;
    case TCC_VALUE:
	te->value.key = strdup(name);
	te->value.values = tclist_new(TC_LOCK_SLOPPY);
	break;
    }
    return te;
}

extern tcc_entry *
create_entry(conf_section *sec, char *name, int type)
{
    char *tmp = strdup(name);
    char *tmpf = tmp;
    char *s, *v;
    tcc_entry *te;
    int crt;
    tccompare_fn cmp = NULL;

    if((v = strrchr(tmp, '/'))){
	*v++ = 0;
    } else {
	v = name;
	*tmp = 0;
    }

    while((s = strsep(&tmp, "/")) != NULL){
	if(*s == 0)
	    continue;

	if(tclist_find(sec->entries, s, &te, cmp_str_sec) != 0){
	    te = alloc_entry(sec, s, TCC_SECTION);
	    tclist_unshift(sec->entries, te);
	}
	sec = te->section;
    }

    switch(type){
    case TCC_VALUE:
	crt = 1;
	cmp = cmp_str_val;
	break;

    case TCC_SECTION:
	crt = 0;
	cmp = cmp_str_sec;
	break;

    case TCC_MSECTION:
	crt = 1;
	cmp = cmp_str_sec;
	break;
    }

    if(crt || tclist_find(sec->entries, v, &te, cmp) != 0){
	te = alloc_entry(sec, v, type);
	tclist_unshift(sec->entries, te);
    }

    free(tmpf);
    return te;
}

extern int
tcconf_setvalue(tcconf_section_t *ts, char *name, char *fmt, ...)
{
    conf_section *sec = ts->sec;
    tcc_entry *te;
    va_list args;
    char *f = fmt;

    te = create_entry(sec, name, TCC_VALUE);

    va_start(args, fmt);
    while((f = strchr(f, '%')) != NULL){
	int lng = 0;

	if(*++f == 'l'){
	    lng = 1;
	    f++;
	}

	switch(*f){
	case 'd':
	case 'i':
	    tcc_addint(te, lng? va_arg(args, long): va_arg(args, int));
	    break;

	case 'f':
	    tcc_addfloat(te, va_arg(args, double));
	    break;

	case 's':
	    tcc_addstring(te, strdup(va_arg(args, char *)), 0);
	    break;
	}
    }

    return 0;
}

extern int
tcconf_clearvalue(tcconf_section_t *ts, char *name)
{
    conf_section *sec = NULL;
    char *n = strdup(name);
    char *p = strrchr(n, '/');
    tcconf_section_t *s = NULL;
    int c = 0;

    if(p){
	*p++ = 0;
	if((s = tcconf_getsection(ts, n)))
	sec = s->sec;
    } else {
	p = n;
	sec = ts->sec;
    }

    if(sec)
	c = tclist_delete_matched(sec->entries, p, cmp_str_sec, tcfree);
    if(s)
	tcfree(s);

    free(n);
    return c;
}


/* Internal functions */

extern int
tcc_addint(tcc_entry *te, long long n)
{
    if(te->type == TCC_VALUE){
	tcc_value *tv = malloc(sizeof(tcc_value));
	tv->type = TCC_INTEGER;
	tv->value.integer = n;
	tclist_push(te->value.values, tv);
    }
    return 0;
}

extern int
tcc_addfloat(tcc_entry *te, double f)
{
    if(te->type == TCC_VALUE){
	tcc_value *tv = malloc(sizeof(tcc_value));
	tv->type = TCC_FLOAT;
	tv->value.floating = f;
	tclist_push(te->value.values, tv);
    }
    return 0;
}

extern int
tcc_addstring(tcc_entry *te, char *s, int exp)
{
    if(te->type == TCC_VALUE){
	tcc_value *tv = malloc(sizeof(tcc_value));
	tv->type = TCC_STRING;
	if(exp)
	    tv->type |= TCC_EXPAND;
	tv->value.string = s;
	tclist_push(te->value.values, tv);
    }
    return 0;
}

extern int
tcc_addbool(tcc_entry *te, int n)
{
    if(te->type == TCC_VALUE){
	tcc_value *tv = malloc(sizeof(tcc_value));
	tv->type = TCC_BOOLEAN;
	tv->value.boolean = n;
	tclist_push(te->value.values, tv);
    }
    return 0;
}

extern int
tcc_addref(tcc_entry *te, char *ref)
{
    if(te->type == TCC_VALUE){
	tcc_value *tv = malloc(sizeof(*tv));
	tv->type = TCC_REF;
	tv->value.string = ref;
	tclist_push(te->value.values, tv);
    }
    return 0;
}

static int
tcc_writelist(tclist_t *l, void *file, int lv, tcio_fn ofn)
{
    tclist_item_t *li = NULL;
    tcc_entry *te;
    while((te = tclist_prev(l, &li)) != NULL){
	tcc_writeentry(te, file, lv, ofn);
    }
    return 0;
}

static int
do_printf(void *file, tcio_fn ofn, char *fmt, ...)
{
    char buf[512];
    va_list args;
    int l, r;

    va_start(args, fmt);
    l = vsnprintf(buf, sizeof(buf), fmt, args);
    if(l == sizeof(buf))
	fprintf(stderr, "tcconf: truncated output\n");
    r = ofn(buf, 1, l, file);
    va_end(args);

    return r;
}

static int
tcc_writesection(conf_section *ts, void *file, int lv, char *d, tcio_fn ofn)
{
    int l = lv * 8;
    tclist_item_t *li = NULL;
    char *m;

    do_printf(file, ofn, "%*s%s", l, "", ts->name);

    while((m = tclist_next(ts->merge, &li)))
	fprintf(file, " : %s", m);
    do_printf(file, ofn, " %c\n", d[0]);
    tcc_writelist(ts->entries, file, lv + 1, ofn);
    do_printf(file, ofn, "%*s%c\n", l, "", d[1]);
    return 0;
}

static int
tcc_writeentry(tcc_entry *te, void *file, int lv, tcio_fn ofn)
{
    tclist_item_t *li = NULL;
    tcc_value *tv;
    int l = lv * 8;

    switch(te->type){
    case TCC_VALUE:
	do_printf(file, ofn, "%*s%s", l, "", te->value.key);
	while((tv = tclist_next(te->value.values, &li)) != NULL){
	    char sd;
	    switch(tv->type & TCC_TYPEMASK){
	    case TCC_INTEGER:
		do_printf(file, ofn, " %ld", tv->value.integer);
		break;
	    case TCC_FLOAT:
		do_printf(file, ofn, " %.16g", tv->value.floating);
		break;
	    case TCC_BOOLEAN:
		do_printf(file, ofn, " %s",
			  tv->value.boolean? "true": "false");
		break;
	    case TCC_STRING:
		sd = tv->type & TCC_EXPAND? '"': '\'';
		do_printf(file, ofn, " %c%s%c", sd, tv->value.string, sd);
		break;
	    case TCC_REF:
		do_printf(file, ofn, " %s", tv->value.string);
		break;
	    }
	}
	do_printf(file, ofn, "\n");
	break;

    case TCC_SECTION:
	tcc_writesection(te->section, file, lv, "{}", ofn);
	break;

    case TCC_MSECTION:
	tcc_writesection(te->section, file, lv, "[]", ofn);
	break;

    default:
	fprintf(stderr, "Internal error.\n");
	return -1;
    }

    return 0;
}
