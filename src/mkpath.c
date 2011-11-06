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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

extern int
tcmkpath(char *path, int mode)
{
    char *p = strdup(path);
    char *e = strrchr(p, '/');
    struct stat st;
    int ret = -1;

    if(e > p){
	*e = 0;
	if(tcmkpath(p, mode))
	    goto end;
    }

    if(stat(path, &st)){
	if(errno != ENOENT)
	    goto end;
	if(mkdir(path, mode))
	    goto end;
    } else if(!S_ISDIR(st.st_mode)){
	goto end;
    }

    ret = 0;
end:
    free(p);
    return ret;
}
