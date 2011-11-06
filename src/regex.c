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
#include <tcstring.h>
#include <tctypes.h>
#include <regex.h>

typedef struct regsub {
    regex_t rx;
    regmatch_t *m;
    const char *s;
} regsub_t;

static char *
rs_lookup(char *n, void *d)
{
    regsub_t *rs = d;
    char *t;
    int ml;
    u_int m = strtol(n, &t, 0);

    if(*t)
	return NULL;
    if(m > rs->rx.re_nsub)
	return NULL;
    if(rs->m[m].rm_so < 0)
	return strdup("");

    ml = rs->m[m].rm_eo - rs->m[m].rm_so;
    t = malloc(ml + 1);
    strncpy(t, rs->s + rs->m[m].rm_so, ml);
    t[ml] = 0;

    return t;
}

extern char *
tcregsub(const char *str, const char *pat, const char *sub, int flags)
{
    regsub_t rs;
    const char *s;
    char *ss, *p;
    int r, l;

    (void) flags;		/* unused, kill warning */

    if((r = regcomp(&rs.rx, pat, REG_EXTENDED))){
	char buf[256];
	regerror(r, &rs.rx, buf, sizeof(buf));
	fprintf(stderr, "tcregsub: %s\n", buf);
	return NULL;
    }

    rs.m = calloc(rs.rx.re_nsub + 1, sizeof(*rs.m));
    l = strlen(str);
    ss = malloc(l + 1);
    p = ss;
    s = str;

    while(*s){
	int ml;
	if(regexec(&rs.rx, s, rs.rx.re_nsub + 1, rs.m, 0))
	    break;

	strncpy(p, s, rs.m[0].rm_so);
	p += rs.m[0].rm_so;
	ml = rs.m[0].rm_eo - rs.m[0].rm_so;
	if(ml > 0){
	    char *ms = malloc(ml + 1);
	    char *rp;
	    int sl, o;

	    rs.s = s;
	    rp = tcstrexp(sub, "{", "}", 0, rs_lookup, &rs,
			  TCSTREXP_KEEPUNDEF | TCSTREXP_FREE);
	    sl = strlen(rp);
	    o = p - ss;
	    ss = realloc(ss, l += sl);
	    p = ss + o;

	    strcpy(p, rp);
	    p += sl;
	    free(rp);
	    free(ms);
	}
	s += rs.m[0].rm_eo;
    }

    strcpy(p, s);
    free(rs.m);
    regfree(&rs.rx);
    return ss;
}
