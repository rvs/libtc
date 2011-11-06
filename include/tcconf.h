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

#ifndef _TCCONF_H
#define _TCCONF_H

#include <stdio.h>
#include <tctypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tcconf_section tcconf_section_t;

extern tcconf_section_t *tcconf_load(tcconf_section_t *, void *, tcio_fn);
extern tcconf_section_t *tcconf_load_file(tcconf_section_t *, char *file);
extern tcconf_section_t *tcconf_load_string(tcconf_section_t *sec,
					    const char *conf, int size);
extern int tcconf_write(tcconf_section_t *tcc, void *file, tcio_fn);
extern tcconf_section_t *tcconf_getsection(tcconf_section_t *sec, char *name);
extern int tcconf_getvalue(tcconf_section_t *sec, char *name, char *fmt, ...);
extern int tcconf_setvalue(tcconf_section_t *sec, char *name, char *fmt, ...);
extern tcconf_section_t *tcconf_new(char *name);
extern int tcconf_clearvalue(tcconf_section_t *sec, char *name);
extern int tcconf_nextvalue(tcconf_section_t *sec, char *name, void **state,
			    char *fmt, ...);
extern int tcconf_nextvalue_g(tcconf_section_t *sec, char *glob, void **state,
			      char **name, char *fmt, ...);
extern tcconf_section_t *tcconf_nextsection(tcconf_section_t *sec, char *name,
					    void **state);
extern tcconf_section_t *tcconf_merge(tcconf_section_t *dst,
				      tcconf_section_t *src);

#ifdef __cplusplus
}
#endif

#endif
