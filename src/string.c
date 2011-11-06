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
#include <ctype.h>
#include <limits.h>

#define hex(x) (((x)<0x3a)? ((x)-'0'): (((x)<0x60)? ((x)-0x37): ((x)-0x57)))

static const char *
escape(char *d, const char *s)
{
    int i;

#define bsub(c,e) case c: *d = e; s++; break

    switch(*s){
	bsub('t', '\t');
	bsub('n', '\n');
	bsub('r', '\r');
	bsub('f', '\f');
	bsub('b', '\b');
	bsub('a', '\a');
	bsub('e', 0x1b);
    case '0':
    case '1':
    case '2':
    case '3':
	*d = 0;
	for(i = 0; i < 3 && isdigit(*s); i++){
	    *d *= 8;
	    *d += *s++ - '0';
	}
	break;
    case 'x':
	if(s[1] && s[2] && isxdigit(s[1]) && isxdigit(s[2])){
	    *d++ = hex(s[1]) * 16 + hex(s[2]);
	    s += 3;
	} else {
	    *d = *s++;
	}
	break;
    case 'c':
	if(s[1]){
	    *d = (toupper(s[1]) - 0x40) & 0x7f;
	    s += 2;
	} else {
	    *d = *s++;
	}
	break;
    default:
	*d = *s++;
	break;
    }

    return s;
}

extern char *
tcstrexp(const char *s, const char *sd, const char *ed, char fs,
	 char *(*lookup)(char *, void *), void *ld, int flags)
{
    int l = strlen(s) + 1;
    char *exp = malloc(l);
    char *p = exp;
    char *d, *f;

#define ext(n) do {				\
    int o = p - exp;				\
    exp = realloc(exp, l += n);			\
    p = exp + o;				\
} while(0)

    while(*s){
	switch(*s){
	case '\\':
	    if(flags & TCSTREXP_ESCAPE){
		s = escape(p++, ++s);
	    } else {
		*p++ = *s++;
	    }
	    break;

	case '$':
	    d = strchr(sd, *++s);
	    if(d){
		const char *e = ++s;
		char ec = ed[d - sd];
		int n = 0;

		/* Find the matching closing paren */
		while(*e){
		    if(*e == *d && *(e - 1) == '$'){
			n++;
		    } else if(*e == ec){
			if(!n)
			    break;
			n--;
		    }
		    e++;
		}
		    
		if(*e){
		    int vl = e - s;
		    char *vn = malloc(vl + 1);
		    char *v;
		    char *def = NULL, *alt = NULL;
		    int upcase = 0, downcase = 0;
		    int sss = 0, ssl = INT_MAX;
		    char *rx = NULL, *rsub = NULL, rd;

		    strncpy(vn, s, vl);
		    vn[vl] = 0;
		    if(fs && (f = strchr(vn, fs))){
			int fl = 1;
			*f++ = 0;
			while(fl && *f){
			    switch(*f++){
			    case '-':
				def = f;
				fl = 0;
				break;
			    case '+':
				alt = f;
				fl = 0;
				break;
			    case 'u':
				upcase = 1;
				break;
			    case 'l':
				downcase = 1;
				break;
			    case 's':
				f++;
			    case '0':
			    case '1':
			    case '2':
			    case '3':
			    case '4':
			    case '5':
			    case '6':
			    case '7':
			    case '8':
			    case '9':
				sss = strtol(f-1, &f, 0);
				if(*f == ':'){
				    f++;
				    ssl = strtol(f, &f, 0);
				}
				break;
			    case '/':
				f--;
			    case 'r':
				rd = *f;
				rx = ++f;
				if((rsub = strchr(rx, rd))){
				    char *re;
				    *rsub++ = 0;
				    if((re = strchr(rsub, rd))){
					*re = 0;
					f = re + 1;
				    } else {
					fl = 0;
				    }
				} else {
				    rx = NULL;
				}
				break;
			    }
			}
		    }
		    if((v = lookup(vn, ld))){
			char *ov = v;
			if(alt)
			    v = tcstrexp(alt, sd, ed, fs, lookup, ld, flags);
			else
			    v = strdup(v);
			if(flags & TCSTREXP_FREE)
			    free(ov);
		    } else if(def){
			v = tcstrexp(def, sd, ed, fs, lookup, ld, flags);
		    }
		    if(v){
			int sl = strlen(v);
			char *vo = v;

			if(sss < 0){
			    if(-sss < sl){
				v += sl + sss;
				sl = -sss;
			    }
			} else if(sss <= sl){
			    v += sss;
			    sl -= sss;
			} else {
			    v += sl;
			    sl = 0;
			}

			if(ssl < 0){
			    if(-ssl < sl){
				v[sl + ssl] = 0;
				sl += ssl;
			    } else {
				sl = 0;
			    }
			} else if(ssl < sl){
			    v[ssl] = 0;
			    sl = ssl;
			}

			if(rx){
			    char *rs = tcregsub(v, rx, rsub, 0);
			    if(rs){
				free(vo);
				vo = v = rs;
				sl = strlen(rs);
			    }
			}

			if(sl){
			    ext(sl + 1);

			    if(upcase){
				char *c = v;
				while(*c)
				    *p++ = toupper(*c++);
			    } else if(downcase){
				char *c = v;
				while(*c)
				    *p++ = tolower(*c++);
			    } else {
				strcpy(p, v);
				p += sl;
			    }
			}
			free(vo);
		    } else if(flags & TCSTREXP_KEEPUNDEF){
			int n = e - s + 3;
			ext(n);
			memcpy(p, s - 2, n);
			p += n;
		    }
		    s = e + 1;
		    free(vn);
		}
	    } else {
		*p++ = '$';
	    }
	    break;
	default:
	    *p++ = *s++;
	    break;
	}
    }

    *p = 0;

#undef ext
    return exp;
}

extern int
tcstresc(char *dst, const char *src)
{
    char *d = dst;

    while(*src){
	if(*src == '\\')
	    src = escape(d++, ++src);
	else
	    *d++ = *src++;
    }

    *d = 0;

    return d - dst;
}
